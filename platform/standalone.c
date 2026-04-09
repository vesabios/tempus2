// standalone.c — Sokol standalone entry point for Tempus

#define _POSIX_C_SOURCE 199309L
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "../lib/sokol_app.h"
#include "../lib/sokol_gfx.h"
#include "../lib/sokol_glue.h"
#include "../lib/sokol_log.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_IMPLEMENTATION
#include "../lib/nuklear.h"
#define SOKOL_NUKLEAR_IMPL
#include "../lib/sokol_nuklear.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../lib/stb_image.h"

#include "../core/scene.h"
#include "../shaders/tempus.glsl.h"

// ---- Globals ----

static Tempus          g_tempus;
static Scene           g_scene;
static DrawCtx         g_draw;
static bool            g_show_debug = true;

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

// ---- Debug GUI ----

static void debug_gui(void) {
    struct nk_context *ctx = snk_new_frame();

    if (g_show_debug) {
        if (nk_begin(ctx, "Tempus Debug", nk_rect(10, 10, 280, 400),
                NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE |
                NK_WINDOW_MINIMIZABLE)) {

            // Time override toggle
            nk_layout_row_dynamic(ctx, 25, 1);
            int override = g_tempus.time_override;
            nk_checkbox_label(ctx, "Manual Time", &override);
            g_tempus.time_override = override;

            if (g_tempus.time_override) {
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_property_int(ctx, "#Year", 1900, &g_tempus.override_year, 2200, 1, 1);

                // Year slider: 0..1 → month/day
                nk_layout_row_dynamic(ctx, 20, 1);
                char yl[64];
                snprintf(yl, sizeof(yl), "Year: %.4f  (%s %d)",
                         g_tempus.override_year_pct,
                         cal_month_names[g_tempus.month - 1], g_tempus.day);
                nk_label(ctx, yl, NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 20, 1);
                float yf = (float)g_tempus.override_year_pct;
                nk_slider_float(ctx, 0.0f, &yf, 0.9999f, 0.001f);
                g_tempus.override_year_pct = yf;

                // Day slider: 0..1 → hour/min/sec
                nk_layout_row_dynamic(ctx, 20, 1);
                char dl[64];
                snprintf(dl, sizeof(dl), "Day: %.4f  (%02d:%02d)",
                         g_tempus.override_day_pct,
                         g_tempus.hours, g_tempus.mins);
                nk_label(ctx, dl, NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 20, 1);
                float df = (float)g_tempus.override_day_pct;
                nk_slider_float(ctx, 0.0f, &df, 0.9999f, 0.001f);
                g_tempus.override_day_pct = df;
            }

            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);

            // Current state display
            nk_layout_row_dynamic(ctx, 25, 1);
            char buf[128];
            snprintf(buf, sizeof(buf), "Date: %04d-%02d-%02d",
                     g_tempus.year, g_tempus.month, g_tempus.day);
            nk_label(ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d",
                     g_tempus.hours, g_tempus.mins, g_tempus.secs);
            nk_label(ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "Zenith: %.1f  Az: %.1f",
                     g_tempus.zenith, g_tempus.solar.azimuth);
            nk_label(ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "Sunrise: %.0fmin  Sunset: %.0fmin",
                     g_tempus.sunrise_mins, g_tempus.sunset_mins);
            nk_label(ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "Year pct: %.4f", tempus_year_pct(&g_tempus));
            nk_label(ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "Verts: %d  Idx: %d",
                     g_draw.num_verts, g_draw.num_indices);
            nk_label(ctx, buf, NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);

            // Actions
            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Solar Warp"))
                timewarp_start(&g_scene.warp, 8640.0, 8.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
            if (nk_button_label(ctx, "Toggle Names"))
                g_tempus.config.use_alternate_names = !g_tempus.config.use_alternate_names;

            // Latitude/Longitude
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Location:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 2);
            nk_property_double(ctx, "#Lat", -90, &g_tempus.config.latitude, 90, 0.1, 0.1);
            nk_property_double(ctx, "#Lon", -180, &g_tempus.config.longitude, 180, 0.1, 0.1);
        }
        nk_end(ctx);
    }
}

// ---- Callbacks ----

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    // Initialize nuklear
    snk_setup(&(snk_desc_t){
        .logger.func = slog_func,
    });

    // Initialize Tempus core state
    TempusConfig cfg = tempus_default_config();
    tempus_init(&g_tempus, cfg);

    // Set override defaults to current time as normalized values
    g_tempus.override_year = g_tempus.year;
    // Compute day-of-year for year slider
    int doy = 0;
    for (int m = 1; m < g_tempus.month; m++)
        doy += cal_days_in_month(m, g_tempus.year);
    doy += g_tempus.day - 1;
    g_tempus.override_year_pct = (double)doy / cal_days_in_year(g_tempus.year);
    g_tempus.override_day_pct = g_tempus.percent_of_day;

    // Initialize scene
    scene_init(&g_scene, &g_tempus);
    scene_register_view(&g_scene, VIEW_CLOCK,    &clock_vtable);
    scene_register_view(&g_scene, VIEW_CALENDAR,  &calendar_vtable);
    scene_register_view(&g_scene, VIEW_SOLAR,     &solar_vtable);
    scene_init_views(&g_scene, &g_tempus);

    g_scene.cycle_len = 0;

    scene_add_layer(&g_scene, VIEW_CALENDAR);
    g_scene.views[VIEW_CALENDAR].opacity = 1.0;

    scene_add_layer(&g_scene, VIEW_SOLAR);
    g_scene.views[VIEW_SOLAR].opacity = 1.0;

    scene_add_layer(&g_scene, VIEW_CLOCK);
    g_scene.views[VIEW_CLOCK].opacity = 1.0;

    draw_init(&g_draw);
    load_atlas();

    g_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

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

    g_bind = (sg_bindings){
        .vertex_buffers[0] = g_vbuf,
        .index_buffer = g_ibuf,
        .views[0] = g_atlas_view,
        .samplers[0] = g_sampler,
    };

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

    tempus_update(&g_tempus, t);
    scene_update(&g_scene, &g_tempus, dt);

    // Build tempus draw commands
    float scale = h / 1280.0f;
    draw_begin(&g_draw, w, h);
    g_draw.sx = scale;
    g_draw.sy = scale;
    scene_render(&g_scene, &g_draw, &g_tempus);

    // Build nuklear GUI
    debug_gui();

    // Upload and draw tempus
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

    // Draw tempus scene
    if (g_draw.num_indices > 0) {
        sg_apply_pipeline(g_pip);
        sg_apply_bindings(&g_bind);
        sg_draw(0, g_draw.num_indices, 1);
    }

    // Draw nuklear GUI on top
    snk_render(sapp_width(), sapp_height());

    sg_end_pass();
    sg_commit();
}

static void event(const sapp_event *e) {
    // Pass events to nuklear first
    if (snk_handle_event(e))
        return; // nuklear consumed the event

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
                timewarp_start(&g_scene.warp, 8640.0, 8.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
                break;
            case SAPP_KEYCODE_D:
                g_show_debug = !g_show_debug;
                break;
            default:
                break;
        }
    }
}

static void cleanup(void) {
    snk_shutdown();
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
