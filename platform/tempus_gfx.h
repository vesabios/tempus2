// tempus_gfx.h — the instrument's GPU layer, shared by every host.
//
// The app (sokol_app) and the macOS screensaver (ScreenSaverView on an
// NSOpenGLContext) build the same textures, shaders, and pipelines and
// submit the same vertex stream; only the window and the event loop
// differ. Hosts provide tempus_asset_path() — the app resolves against
// the working directory, the saver against its bundle's Resources.

#ifndef TEMPUS_GFX_H
#define TEMPUS_GFX_H

// Provided by the host.
static const char *tempus_asset_path(const char *name);

static sg_pipeline     g_pip;
static sg_bindings     g_bind;
static sg_buffer       g_vbuf;
static sg_buffer       g_ibuf;
static sg_image        g_atlas_img;
static sg_view         g_atlas_view;
static sg_sampler      g_sampler;
static sg_pass_action  g_pass_action;

// Globe (3D earth instrument)
static sg_pipeline     g_globe_pip;
static sg_buffer       g_globe_vbuf;
static sg_buffer       g_globe_ibuf;
static sg_image        g_land_img;
static sg_view         g_land_view;
static sg_image        g_moon_img;
static sg_view         g_moon_view;
static sg_sampler      g_land_sampler;

typedef struct {
    float rot[16];      // earth -> view rotation (column-major mat4)
    float place[4];     // cx, cy, radius (world), pad
    float screen[4];    // w/2, h/2, pad, pad
} GlobeVsUniforms;

typedef struct {
    float light[4];
    float day[4];
    float night[4];
    float grid[4];
    float mode[4];      // x = overlay, y = declination (radians)
    float axis[4];      // earth rotation axis, view frame
    float sunj[4];      // sun re-declined to june solstice
    float sund[4];      // sun re-declined to december solstice
    float params[4];    // x = land mask on, y = whole-sphere alpha
} GlobeFsUniforms;

static void tempus__load_atlas(void) {
    int w, h, n;
    unsigned char *data = stbi_load(tempus_asset_path("font_atlas.png"), &w, &h, &n, 1);
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

// Build every GPU object the instrument draws with.
static void tempus_gfx_init(void) {
    tempus__load_atlas();

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
        .size = sizeof(uint32_t) * DRAW_MAX_INDICES,
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
        .index_type = SG_INDEXTYPE_UINT32,
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

    // Globe mesh + pipeline
    {
        // Landmass mask (equirectangular, R8)
        int lw, lh, ln;
        unsigned char *ldata = stbi_load(tempus_asset_path("land_mask.png"), &lw, &lh, &ln, 1);
        if (ldata) {
            g_land_img = sg_make_image(&(sg_image_desc){
                .width = lw,
                .height = lh,
                .pixel_format = SG_PIXELFORMAT_R8,
                .data.mip_levels[0] = { .ptr = ldata, .size = (size_t)(lw * lh) },
            });
            stbi_image_free(ldata);
            g_land_view = sg_make_view(&(sg_view_desc){
                .texture.image = g_land_img,
            });
        }
        // Moon albedo (LROC WAC mosaic, NASA public domain)
        int mw, mh, mn;
        unsigned char *mdata = stbi_load(tempus_asset_path("moon_mask.png"), &mw, &mh, &mn, 1);
        if (mdata) {
            g_moon_img = sg_make_image(&(sg_image_desc){
                .width = mw,
                .height = mh,
                .pixel_format = SG_PIXELFORMAT_R8,
                .data.mip_levels[0] = { .ptr = mdata, .size = (size_t)(mw * mh) },
            });
            stbi_image_free(mdata);
            g_moon_view = sg_make_view(&(sg_view_desc){
                .texture.image = g_moon_img,
            });
        }

        g_land_sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_REPEAT,            // longitude seam
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        });

        static GlobeMesh mesh;
        globe_mesh_build(&mesh);

        g_globe_vbuf = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(mesh.verts),
            .usage.vertex_buffer = true,
        });
        g_globe_ibuf = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(mesh.indices),
            .usage.index_buffer = true,
        });

        sg_shader globe_shd = sg_make_shader(&(sg_shader_desc){
            .vertex_func.source = globe_vs_src,
            .fragment_func.source = globe_fs_src,
            .attrs = {
                [0] = { .glsl_name = "a_pos" },
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
                .glsl_name = "u_land",
            },
            .uniform_blocks = {
                [0] = {
                    .stage = SG_SHADERSTAGE_VERTEX,
                    .size = sizeof(GlobeVsUniforms),
                    .glsl_uniforms = {
                        [0] = { .type = SG_UNIFORMTYPE_MAT4,   .glsl_name = "u_rot" },
                        [1] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_place" },
                        [2] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_screen" },
                    },
                },
                [1] = {
                    .stage = SG_SHADERSTAGE_FRAGMENT,
                    .size = sizeof(GlobeFsUniforms),
                    .glsl_uniforms = {
                        [0] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_light" },
                        [1] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_day" },
                        [2] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_night" },
                        [3] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_grid" },
                        [4] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_mode" },
                        [5] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_axis" },
                        [6] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_sunj" },
                        [7] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_sund" },
                        [8] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "u_params" },
                    },
                },
            },
        });

        g_globe_pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = globe_shd,
            .index_type = SG_INDEXTYPE_UINT16,
            .layout.attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT3 },
            },
            .depth = {
                .compare = SG_COMPAREFUNC_LESS,
                .write_enabled = true,
            },
            .colors[0].blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        });
    }
}

// Submit one frame's vertex stream. A render pass must be active; the
// host owns the pass so it can add its own chrome (the app's debug GUI).
static void tempus_gfx_draw(const DrawCtx *d, float w, float h) {
    if (d->num_indices > 0) {
        int drawn = 0;
        for (int gi = 0; gi <= d->num_globes; gi++) {
            int upto = (gi < d->num_globes)
                ? d->globes[gi].split_index : d->num_indices;
            if (upto > drawn) {
                sg_apply_pipeline(g_pip);
                sg_apply_bindings(&g_bind);
                sg_draw(drawn, upto - drawn, 1);
                drawn = upto;
            }
            if (gi >= d->num_globes) break;

            const GlobeCmd *gc = &d->globes[gi];
            GlobeVsUniforms vsu;
            GlobeFsUniforms fsu;
            memcpy(vsu.rot, gc->rot, sizeof(vsu.rot));
            vsu.place[0] = gc->cx;  vsu.place[1] = gc->cy;
            vsu.place[2] = gc->radius;
            vsu.place[3] = 0.85f - 0.3f * (float)gi;   // depth band center
            vsu.screen[0] = w * 0.5f; vsu.screen[1] = h * 0.5f;
            vsu.screen[2] = 0; vsu.screen[3] = 0;

            memcpy(fsu.light, gc->light, sizeof(float) * 3);
            fsu.light[3] = 0;
            const RenderStyle *st = &g_scene.style;
            if (gc->day_col[3] > 0) {   // per-body palette (e.g. the moon)
                memcpy(fsu.day,   gc->day_col,   sizeof(float) * 4);
                memcpy(fsu.night, gc->night_col, sizeof(float) * 4);
            } else {
                memcpy(fsu.day,   &st->globe_light,  sizeof(float) * 4);
                memcpy(fsu.night, &st->globe_shadow, sizeof(float) * 4);
            }
            memcpy(fsu.grid,  &st->globe_grid,       sizeof(float) * 4);
            fsu.params[0] = gc->land ? gc->land_mix : 0.0f;
            fsu.params[1] = gc->alpha;
            fsu.params[2] = (float)gc->tex_id;
            fsu.params[3] = 0;
            fsu.mode[0] = (float)gc->overlay;
            fsu.mode[1] = gc->declination * (float)M_PI / 180.0f;
            fsu.mode[2] = gc->obs_lat;   // degrees
            fsu.mode[3] = gc->grid_boost;   // orrery always sets this
            // Earth axis in view frame = rotation matrix z column
            fsu.axis[0] = gc->rot[8];
            fsu.axis[1] = gc->rot[9];
            fsu.axis[2] = gc->rot[10];
            fsu.axis[3] = 0;
            globe_sun_with_decl(gc->rot, fsu.light, 23.44, fsu.sunj);
            globe_sun_with_decl(gc->rot, fsu.light, -23.44, fsu.sund);
            fsu.sunj[3] = fsu.sund[3] = 0;
            if (gc->aux_dir[3] > 0)   // observer-visibility dir (the moon)
                memcpy(fsu.sunj, gc->aux_dir, sizeof(float) * 4);

            sg_apply_pipeline(g_globe_pip);
            sg_apply_bindings(&(sg_bindings){
                .vertex_buffers[0] = g_globe_vbuf,
                .index_buffer = g_globe_ibuf,
                .views[0] = (gc->tex_id == 1 && g_moon_view.id)
                          ? g_moon_view : g_land_view,
                .samplers[0] = g_land_sampler,
            });
            sg_apply_uniforms(0, &SG_RANGE(vsu));
            sg_apply_uniforms(1, &SG_RANGE(fsu));
            sg_draw(0, GLOBE_NUM_INDICES, 1);
        }
    }
}

#endif // TEMPUS_GFX_H
