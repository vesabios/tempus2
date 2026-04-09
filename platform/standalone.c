// standalone.c — Sokol standalone entry point for Tempus

#define _POSIX_C_SOURCE 199309L
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "../lib/sokol_app.h"
#include "../lib/sokol_gfx.h"
#include "../lib/sokol_glue.h"
#include "../lib/sokol_log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../lib/stb_image.h"

#include "../core/scene.h"
#include "../shaders/tempus.glsl.h"

// ---- Globals ----

static Tempus          g_tempus;
static Scene           g_scene;
static DrawCtx         g_draw;

static sg_pipeline     g_pip;
static sg_bindings     g_bind;
static sg_buffer       g_vbuf;
static sg_buffer       g_ibuf;
static sg_image        g_atlas_img;
static sg_view         g_atlas_view;
static sg_sampler      g_sampler;
static sg_pass_action  g_pass_action;

// ---- Load font atlas ----

static void load_atlas(void) {
    int w, h, n;
    unsigned char *data = stbi_load("assets/font_atlas.png", &w, &h, &n, 1);
    if (!data)
        data = stbi_load("../assets/font_atlas.png", &w, &h, &n, 1);
    if (!data) {
        fprintf(stderr, "Failed to load font_atlas.png\n");
        exit(1);
    }

    // Set pixel (0,0) to white for untextured draws
    data[0] = 255;

    g_atlas_img = sg_make_image(&(sg_image_desc){
        .width = w,
        .height = h,
        .pixel_format = SG_PIXELFORMAT_R8,
        .data.mip_levels[0] = { .ptr = data, .size = (size_t)(w * h) },
    });
    stbi_image_free(data);

    g_atlas_view = sg_make_view(&(sg_view_desc){
        .texture.image = g_atlas_img,
    });
}

// ---- Callbacks ----

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    // Initialize Tempus core state
    TempusConfig cfg = tempus_default_config();
    tempus_init(&g_tempus, cfg);

    // Initialize scene
    scene_init(&g_scene, &g_tempus);

    // Register views
    scene_register_view(&g_scene, VIEW_CLOCK,    &clock_vtable);
    scene_register_view(&g_scene, VIEW_CALENDAR,  &calendar_vtable);
    scene_register_view(&g_scene, VIEW_SOLAR,     &solar_vtable);
    scene_init_views(&g_scene, &g_tempus);

    // Set up screensaver cycle
    g_scene.cycle[0] = VIEW_CALENDAR;
    g_scene.cycle[1] = VIEW_SOLAR;
    g_scene.cycle[2] = VIEW_CALENDAR;
    g_scene.cycle_len = 3;
    g_scene.dwell_duration = 10.0;

    // Start with clock (persistent foreground) + calendar (background)
    scene_add_layer(&g_scene, VIEW_CALENDAR);
    g_scene.views[VIEW_CALENDAR].opacity = 1.0;
    calendar_enter(g_scene.views[VIEW_CALENDAR].state, &g_tempus, &g_scene);

    scene_add_layer(&g_scene, VIEW_SOLAR);
    g_scene.views[VIEW_SOLAR].opacity = 1.0;

    scene_add_layer(&g_scene, VIEW_CLOCK);
    g_scene.views[VIEW_CLOCK].opacity = 1.0;

    // Init draw context
    draw_init(&g_draw);

    // Load atlas
    load_atlas();
    g_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    // Dynamic buffers
    g_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(DrawVertex) * DRAW_MAX_VERTS,
        .usage.vertex_buffer = true,
        .usage.stream_update = true,
    });
    g_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(uint16_t) * DRAW_MAX_INDICES,
        .usage.index_buffer = true,
        .usage.stream_update = true,
    });

    // Shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source = vs_src,
        .fragment_func.source = fs_src,
        .attrs = {
            [0] = { .glsl_name = "a_pos" },
            [1] = { .glsl_name = "a_uv" },
            [2] = { .glsl_name = "a_color" },
        },
        .views[0] = {
            .texture = {
                .stage = SG_SHADERSTAGE_FRAGMENT,
                .image_type = SG_IMAGETYPE_2D,
                .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
            },
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .sampler_type = SG_SAMPLERTYPE_FILTERING,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .view_slot = 0,
            .sampler_slot = 0,
            .glsl_name = "u_tex",
        },
    });

    // Pipeline
    g_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout.attrs = {
            [0] = { .format = SG_VERTEXFORMAT_FLOAT2, .offset = offsetof(DrawVertex, x) },
            [1] = { .format = SG_VERTEXFORMAT_FLOAT2, .offset = offsetof(DrawVertex, u) },
            [2] = { .format = SG_VERTEXFORMAT_FLOAT4, .offset = offsetof(DrawVertex, r) },
        },
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
        .depth.compare = SG_COMPAREFUNC_ALWAYS,
    });

    // Bindings
    g_bind = (sg_bindings){
        .vertex_buffers[0] = g_vbuf,
        .index_buffer = g_ibuf,
        .views[0] = g_atlas_view,
        .samplers[0] = g_sampler,
    };

    // Pass action
    g_pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };
}

static void frame(void) {
    float w = sapp_widthf();
    float h = sapp_heightf();
    double dt = sapp_frame_duration();
    double t = sapp_frame_count() * dt;

    // Update core time state
    tempus_update(&g_tempus, t);

    // Update scene (tweens, transitions, view updates)
    scene_update(&g_scene, &g_tempus, dt);

    // Build draw commands
    float scale = h / 1280.0f;
    draw_begin(&g_draw, w, h);
    g_draw.sx = scale;
    g_draw.sy = scale;

    scene_render(&g_scene, &g_draw, &g_tempus);

    // Upload and draw
    if (g_draw.num_verts > 0 && g_draw.num_indices > 0) {
        sg_update_buffer(g_vbuf, &(sg_range){
            .ptr = g_draw.verts,
            .size = (size_t)g_draw.num_verts * sizeof(DrawVertex),
        });
        sg_update_buffer(g_ibuf, &(sg_range){
            .ptr = g_draw.indices,
            .size = (size_t)g_draw.num_indices * sizeof(uint16_t),
        });
    }

    sg_begin_pass(&(sg_pass){
        .action = g_pass_action,
        .swapchain = sglue_swapchain(),
    });
    if (g_draw.num_indices > 0) {
        sg_apply_pipeline(g_pip);
        sg_apply_bindings(&g_bind);
        sg_draw(0, g_draw.num_indices, 1);
    }
    sg_end_pass();
    sg_commit();
}

static void event(const sapp_event *e) {
    if (e->type == SAPP_EVENTTYPE_KEY_DOWN) {
        switch (e->key_code) {
            case SAPP_KEYCODE_ESCAPE:
            case SAPP_KEYCODE_Q:
                sapp_request_quit();
                break;
            case SAPP_KEYCODE_A:
                g_tempus.config.use_alternate_names = !g_tempus.config.use_alternate_names;
                break;
            case SAPP_KEYCODE_S:
                // Manually trigger solar warp
                timewarp_start(&g_scene.warp, 360.0, 10.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
                break;
            case SAPP_KEYCODE_SPACE:
                // Skip to next in cycle
                g_scene.dwell_timer = g_scene.dwell_duration;
                break;
            default:
                break;
        }
    }
}

static void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = event,
        .cleanup_cb = cleanup,
        .width = 1280,
        .height = 800,
        .window_title = "T E M P V S",
        .logger.func = slog_func,
    };
}
