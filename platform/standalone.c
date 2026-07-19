// standalone.c — Sokol standalone entry point for Tempus

#if !defined(__APPLE__)
#define _POSIX_C_SOURCE 199309L
#endif
#define SOKOL_IMPL
#if defined(__EMSCRIPTEN__)
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
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
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../lib/stb_image_write.h"

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <stdlib.h>

#include "../core/scene.h"
#include "../core/globe.h"
#include "../shaders/tempus.glsl.h"

// ---- Globals ----

static Tempus          g_tempus;
static Scene           g_scene;
static DrawCtx         g_draw;
static bool            g_show_debug = false;   // D toggles

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

// Dev harness: TEMPUS_SHOT=<path> renders N frames, dumps a PNG, exits.
static const char     *g_shot_path = NULL;
static int             g_shot_countdown = 0;

// Flight-matrix harness (Stage 0 of the transition refactor):
// TEMPUS_FLY="FROM,TO" + TEMPUS_FLYT=<sec> + TEMPUS_SHOT=<png> snaps
// to station FROM, fires the real flight to TO, and captures the
// frame FLYT seconds into it — on a fixed 60Hz timestep so the same
// flight always lands on the same pixels. The regression net for
// every transition.
static int    g_fly_from = -1, g_fly_to = -1;
static double g_fly_t = 1.75;
static int    g_fly_settle = 0;
static double g_fly_clock = -1.0;

// View mode: geocentric (calendar + solar dial + clock) vs heliocentric
// (calendar wheel as orbit + orrery earth). The scene's helio_blend
// morphs between them; layer opacities derive from it every frame.
// Worldview stations — mutually exclusive vantages, each a point on the
// (helio_blend, zoom, system_blend) morph axis. Named in the
// instrument's Latin: the timepiece, the earth as a body, the machine
// of the world. CAELVM (the local sky) joins as the fourth.
// The station enum and its declared columns live in core/station.h
// (docs/TRANSITIONS.md). The shell keeps its WV_ names as aliases.
typedef Station Worldview;
#define WV_HOROLOGIVM ST_HOROLOGIVM
#define WV_HORAE      ST_HORAE
#define WV_ROTAE      ST_ROTAE
#define WV_SAECVLVM   ST_SAECVLVM
#define WV_TELLVS     ST_TELLVS
#define WV_MACHINA    ST_MACHINA
#define WV_CAELVM     ST_CAELVM
#define WV_ORBIS      ST_ORBIS
#define WV_OFFICIVM   ST_OFFICIVM
#define WV_DRACO      ST_DRACO
#define WV_COUNT      ST_COUNT
#define g_worldview_names_at(i) (station_table[i].name)

static Worldview g_worldview = WV_HOROLOGIVM;

// ---- The tour: an idle attractor through the stations ----
// Loops until any CLICK (or key) ends it; sixty idle seconds — no
// click, no mouse motion — start it again from the top. Four seconds
// of dwell after each flight lands.
static const Worldview g_tour[] = {
    WV_HOROLOGIVM, WV_HORAE, WV_HOROLOGIVM, WV_ORBIS,
    WV_TELLVS, WV_MACHINA, WV_CAELVM, WV_DRACO,
};
#define TOUR_LEN    ((int)(sizeof(g_tour) / sizeof(g_tour[0])))
#define TOUR_FLIGHT 3.5     /* matches the station tween */
#define TOUR_DWELL  4.0
#define TOUR_RESUME 60.0
static bool   g_tour_active = true;
static int    g_tour_idx = 0;
static double g_tour_timer = 0;
static double g_idle_secs = 0;


// Annunciator hit rects (chrome space: 1280-tall world units, no camera
// zoom), published by the frame that draws them
static float g_wv_btn[WV_COUNT][4];   // x0, y0, x1, y1

#define SYS_CAMERA_ZOOM    0.62f
#define CAELVM_CAMERA_ZOOM 0.85f
static float sys_camera_scale(void) {
    float k = 1.0f - (1.0f - SYS_CAMERA_ZOOM) * (float)g_scene.system_blend;
    // CAELVM: camera eases most of the way home — enough room for the
    // sky bowl AND the calendar wheel riding outside it as the bezel.
    // The zodiac ring still hands off to the horizon rim at near-
    // matching screen size.
    return k + (CAELVM_CAMERA_ZOOM - k) * (float)g_scene.sky_blend;
}

// Fly the instrument to a worldview station: geo (0,0,0), heliocentric
// earth (1,1,0), full system (1,0,1). All three parameters tween together
// so any station reaches any other in one continuous camera move.
static void fly_worldview(double m, double z, double s, double dur,
                          double delay) {
    tween_cancel_target(&g_scene.tweens, &g_scene.helio_blend);
    tween_start_delayed(&g_scene.tweens, &g_scene.helio_blend,
                        g_scene.helio_blend, m, delay, dur,
                        EASE_IN_OUT_CUBIC);
    tween_cancel_target(&g_scene.tweens, &g_scene.calendar_state.zoom);
    g_scene.calendar_state.target_zoom = z;
    tween_start_delayed(&g_scene.tweens, &g_scene.calendar_state.zoom,
                        g_scene.calendar_state.zoom, z, delay, dur,
                        EASE_IN_OUT_CUBIC);
    tween_cancel_target(&g_scene.tweens, &g_scene.system_blend);
    tween_start_delayed(&g_scene.tweens, &g_scene.system_blend,
                        g_scene.system_blend, s, delay, dur,
                        EASE_IN_OUT_CUBIC);
}

static void config_save(const TempusConfig *cfg);

// Instantly BE at a station: every blend snapped to its resting
// value, no tweens. The flight harness's launch pad.
static void snap_station(Worldview wv) {
    g_worldview = wv;
    g_scene.helio_blend  = (wv == WV_TELLVS || wv == WV_MACHINA) ? 1.0 : 0.0;
    g_scene.system_blend = (wv == WV_MACHINA) ? 1.0 : 0.0;
    g_scene.sky_blend    = (wv == WV_CAELVM) ? 1.0 : 0.0;
    g_scene.horae_blend  = (wv == WV_HORAE) ? 1.0 : 0.0;
    g_scene.rotae_blend  = (wv == WV_ROTAE) ? 1.0 : 0.0;
    g_scene.saec_blend   = (wv == WV_SAECVLVM) ? 1.0 : 0.0;
    g_scene.orbis_blend  = (wv == WV_ORBIS) ? 1.0 : 0.0;
    g_scene.orbis_wheel  = g_scene.orbis_blend;
    g_scene.offic_blend  = (wv == WV_OFFICIVM) ? 1.0 : 0.0;
    g_scene.draco_blend  = (wv == WV_DRACO) ? 1.0 : 0.0;
    g_scene.calendar_state.zoom = (wv == WV_TELLVS) ? 1.0 : 0.0;
    g_scene.calendar_state.target_zoom = g_scene.calendar_state.zoom;
}

// Station by case-insensitive prefix ("MACHINA" matches "MACHINA MVNDI")
static int station_by_name(const char *name, size_t len) {
    for (int i = 0; i < WV_COUNT; i++) {
        size_t n = strlen(g_worldview_names_at(i));
        if (len <= n && strncasecmp(name, g_worldview_names_at(i), len) == 0)
            return i;
    }
    return -1;
}

static void set_worldview(Worldview wv) {
    if (wv == g_worldview) return;
    // Leaving the world chart commits the chosen location to disk
    if (g_worldview == WV_ORBIS)
        config_save(&g_tempus.config);
    g_worldview = wv;

    // Sky flights run simultaneously with the machine flight: the sky
    // morph's sun/moon endpoints track the orrery's LIVE published
    // positions, so direct flights from any station are continuous —
    // no routing through MACHINA.
    double fly_delay = 0.0;
    double sky_delay = 0.0;

    // Every station-owned blend flies by the same rule; the table
    // declares the columns, the special cases declare themselves:
    // ORBIS rides the 3.5s geometric clock and PARKS on flights to
    // pure overlays (absent-seat rule — the globe has no seat there,
    // so it holds, fades with the machine, and snaps home hidden).
    static double *const blends[ST_COUNT] = {
        [ST_CAELVM]   = &g_scene.sky_blend,
        [ST_HORAE]    = &g_scene.horae_blend,
        [ST_ROTAE]    = &g_scene.rotae_blend,
        [ST_SAECVLVM] = &g_scene.saec_blend,
        [ST_OFFICIVM] = &g_scene.offic_blend,
        [ST_DRACO]    = &g_scene.draco_blend,
        [ST_ORBIS]    = &g_scene.orbis_blend,
    };
    bool overlay_dst = blends[wv] != NULL
                    && wv != ST_ORBIS && wv != ST_CAELVM;
    for (int i = 0; i < ST_COUNT; i++) {
        if (!blends[i]) continue;
        tween_cancel_target(&g_scene.tweens, blends[i]);
        if (i == ST_ORBIS && overlay_dst && *blends[i] > 0.5)
            continue;   // park
        double dur = (i == ST_ORBIS) ? 3.5 : 3.0;
        double dly = (i == ST_CAELVM) ? sky_delay : fly_delay;
        tween_start_delayed(&g_scene.tweens, blends[i], *blends[i],
                            (int)wv == i ? 1.0 : 0.0, dly, dur,
                            EASE_IN_OUT_CUBIC);
    }
    // The wheel's orbis voice always flies (never parks) — see
    // Scene.orbis_wheel
    tween_cancel_target(&g_scene.tweens, &g_scene.orbis_wheel);
    tween_start_delayed(&g_scene.tweens, &g_scene.orbis_wheel,
                        g_scene.orbis_wheel, wv == WV_ORBIS ? 1.0 : 0.0,
                        fly_delay, 3.5, EASE_IN_OUT_CUBIC);
    // The base machine morph flies to the station's declared targets;
    // CAELVM parks the machine WHEREVER IT WAS (park_machine): the
    // sky movers seam to live published positions, so no machine
    // flight happens under the rising sky.
    if (!station_table[wv].park_machine)
        fly_worldview(station_table[wv].fly_helio,
                      station_table[wv].fly_zoom,
                      station_table[wv].fly_system, 3.5, fly_delay);
}

static void apply_view_mode(void) {
    g_scene.num_layers = 0;
    scene_add_layer(&g_scene, VIEW_CALENDAR);
    // Solar is a pure data provider (renders nothing, opacity stays 0);
    // it precedes the orrery so the instrument reads fresh solar values.
    // The orrery renders the entire earth instrument at every morph
    // state; the clock draws its hands on top.
    scene_add_layer(&g_scene, VIEW_SOLAR);
    // The clock's face furniture sits UNDER the orrery: the globes
    // (and the ORBIS closeup especially) cover the ticks and
    // numerals; only the hands ride above (VIEW_CLOCK, later).
    scene_add_layer(&g_scene, VIEW_CLOCKBACK);
    scene_add_layer(&g_scene, VIEW_ORRERY);
    // The sky renders under the luminaries; the clock's hands render
    // over them (the moon lives in the dial's aperture at HOROLOGIVM).
    // The two never fight: wherever the sky is up, the clock is gone.
    scene_add_layer(&g_scene, VIEW_SKY);
    // DRACO's tracks live UNDER the luminaries: during flights the
    // sun and moon are VIEW_LVMEN's and must ride over the dragon's
    // scenery; at full blend draco draws its own beads late in its
    // own layer, so the sandwich (furnace under, umbra over) holds.
    scene_add_layer(&g_scene, VIEW_DRACO);
    scene_add_layer(&g_scene, VIEW_LVMEN);
    scene_add_layer(&g_scene, VIEW_CLOCK);
    scene_add_layer(&g_scene, VIEW_HORAE);
    scene_add_layer(&g_scene, VIEW_ROTAE);
    scene_add_layer(&g_scene, VIEW_SAEC);
    scene_add_layer(&g_scene, VIEW_ORBIS);
    scene_add_layer(&g_scene, VIEW_OFFIC);
}

static void set_view_opacities(void) {
    double hb = g_scene.helio_blend;
    double sky = g_scene.sky_blend;
    // The machine dissolves EARLY under the sky morph — its beads and
    // rings hand off to the sky view's moving copies, which render at
    // full strength for the whole flight (the sky view alphas its own
    // elements internally).
    double horae = g_scene.horae_blend;
    double rotae = g_scene.rotae_blend;
    double saec = g_scene.saec_blend;
    double orbis = g_scene.orbis_blend;
    double offic = g_scene.offic_blend;
    double draco = g_scene.draco_blend;
    double fade = (1.0 - tempus_smoothstep(0.0, 0.55, sky))
                * (1.0 - tempus_smoothstep(0.0, 0.55, horae))
                * (1.0 - tempus_smoothstep(0.0, 0.55, rotae))
                * (1.0 - tempus_smoothstep(0.0, 0.55, saec))
                * (1.0 - tempus_smoothstep(0.0, 0.55, offic))
                * (1.0 - tempus_smoothstep(0.0, 0.55, draco));
    // ORBIS keeps the orrery: the globe IS the station (it grows to the
    // closeup inside the orrery itself). Only the clock chrome bows out
    // — and it bows out FAST (gone by a fifth of the flight): the dial
    // furniture has no business hanging over the growing planet.
    double orbis_fade = 1.0 - tempus_smoothstep(0.0, 0.20, orbis);
    // The calendar wheel survives into the sky as its bezel — the time
    // control rides along to every worldview
    g_scene.views[VIEW_CALENDAR].opacity = 1.0;
    g_scene.views[VIEW_SOLAR].opacity = 0.0;    // data only, never draws
    // The orrery must keep rendering even fully faded — it is the
    // COMPOSER of the luminaries (its render publishes their
    // parameters for VIEW_LVMEN). Floor just above the render cutoff;
    // its vertex-heavy passes skip themselves below visibility.
    g_scene.views[VIEW_ORRERY].opacity = fade > 0.002 ? fade : 0.002;
    // The parked ORBIS closeup snaps home UNSEEN once the machine is
    // fully hidden at an overlay station (the absent-seat rule's
    // second half: park, fade, then reset while nobody watches).
    // DRACO keeps the LUMINARIES visible through its flight and their
    // base seats read orbis_blend — so there, wait for the chart to
    // own them completely (blend at 1) before snapping, or the sun
    // and moon jump mid-flight (Seren caught it on ORBIS -> DRACO).
    if (g_worldview != WV_ORBIS && g_scene.orbis_blend > 0.001
        && fade < 0.004
        && (draco <= 0.001 || draco >= 0.999)) {
        tween_cancel_target(&g_scene.tweens, &g_scene.orbis_blend);
        g_scene.orbis_blend = 0.0;
    }
    // Clock face and hands exit in the first quarter of the transit (and
    // return in the last quarter coming home) — they're geocentric
    // furniture and have no business lingering over the flight
    double clock_vis = 1.0 - hb * 4.0;
    if (clock_vis < 0) clock_vis = 0.0;
    g_scene.views[VIEW_CLOCK].opacity = clock_vis * fade * orbis_fade;
    g_scene.views[VIEW_CLOCKBACK].opacity =
        g_scene.views[VIEW_CLOCK].opacity;
    g_scene.views[VIEW_SKY].opacity = sky > 0.001 ? 1.0 : 0.0;
    g_scene.views[VIEW_HORAE].opacity = horae;
    g_scene.views[VIEW_ROTAE].opacity = rotae;
    g_scene.views[VIEW_SAEC].opacity = saec;
    // The station's own chrome (reticle, readout, city pips) follows
    // the RELEASED voice, so a parked flight fades it instead of
    // holding it over the next station until the snap
    g_scene.views[VIEW_ORBIS].opacity =
        orbis < g_scene.orbis_wheel ? orbis : g_scene.orbis_wheel;
    g_scene.views[VIEW_OFFIC].opacity = offic;
    g_scene.views[VIEW_DRACO].opacity = draco;
    // The luminaries ride whichever surface is up: the machine's fade
    // or the sky — never both gone unless an overlay dial owns the
    // stage. This is what makes the sun and moon SINGLE objects: one
    // renderer, always on top, parameters composed continuously.
    {
        double lum = 1.0 - tempus_smoothstep(0.0, 0.15, sky);
        double skyvis = tempus_smoothstep(0.0, 0.15, sky);
        lum = fade > skyvis ? fade : skyvis;
        // DRACO flights keep the luminaries with VIEW_LVMEN the whole
        // way (the orrery composes toward draco's published targets);
        // at full blend the station takes over at exact coincidence —
        // its own furnace-under, umbra-over sandwich needs the beads
        // in its own layer
        if (draco < 0.999) {
            double dvis = tempus_smoothstep(0.0, 0.10, draco);
            if (dvis > lum) lum = dvis;
        }
        g_scene.views[VIEW_LVMEN].opacity = lum;
    }
}

// Pacing: vsync drives the frame callback, but update/rebuild work only
// runs at scene_desired_fps(). Skipped frames re-present existing buffers.
static double          g_time = 0;
static double          g_last_update = -1.0e9;
static double          g_boost_until = 0;       // input → brief full rate
static double          g_desired_fps = 60.0;    // for debug display
static bool            g_pace_log = false;      // TEMPUS_PACE_LOG=1
static int             g_updates_this_sec = 0;
static double          g_pace_log_timer = 0;

static void save_shot(const char *path) {
    int w = sapp_width(), h = sapp_height();
    unsigned char *pixels = malloc((size_t)w * h * 4);
    unsigned char *flipped = malloc((size_t)w * h * 4);
    if (!pixels || !flipped) { free(pixels); free(flipped); return; }
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    // Force opaque: the framebuffer alpha carries blending residue that
    // makes the PNG spuriously translucent in viewers
    for (size_t i = 3; i < (size_t)w * h * 4; i += 4) pixels[i] = 255;
    for (int y = 0; y < h; y++)
        memcpy(flipped + (size_t)y * w * 4,
               pixels + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
    stbi_write_png(path, w, h, 4, flipped, w * 4);
    free(pixels);
    free(flipped);
    fprintf(stderr, "wrote %s (%dx%d)\n", path, w, h);
}

// ---- Config persistence (key=value file) ----

// sweep_seconds lives in scene style, which isn't initialized until after
// config load — stash it and apply post scene_init
static int g_cfg_sweep_override = -1;

// Optional IANA timezone (e.g. Europe/Berlin): runs the entire instrument
// — clock hands included — in the configured location's time via the
// system tz database (DST-correct). Empty = machine local time.
static char g_cfg_tz_name[64] = "";

#if !defined(__EMSCRIPTEN__)
static void config_file_path(char *buf, size_t n) {
    const char *override = getenv("TEMPUS_CONFIG");
    if (override) {
        snprintf(buf, n, "%s", override);
        return;
    }
    const char *home = getenv("HOME");
    snprintf(buf, n, "%s/.config/tempus.conf", home ? home : ".");
}

static void config_load(TempusConfig *cfg) {
    char path[512];
    config_file_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        double val;
        if (sscanf(line, "timezone_name = %63s", g_cfg_tz_name) == 1) continue;
        if (sscanf(line, "%63[a-z_] = %lf", key, &val) != 2) continue;
        if      (strcmp(key, "latitude") == 0)  cfg->latitude = val;
        else if (strcmp(key, "longitude") == 0) cfg->longitude = val;
        else if (strcmp(key, "elevation") == 0) cfg->elevation = val;
        else if (strcmp(key, "timezone") == 0)  cfg->timezone = val;
        else if (strcmp(key, "timezone_auto") == 0) cfg->timezone_auto = val != 0;
        else if (strcmp(key, "alternate_names") == 0) cfg->use_alternate_names = val != 0;
        else if (strcmp(key, "sweep_seconds") == 0) g_cfg_sweep_override = (val != 0) ? 1 : 0;
        else if (strcmp(key, "birth_year") == 0)  cfg->birth_year = (int)val;
        else if (strcmp(key, "birth_month") == 0) cfg->birth_month = (int)val;
        else if (strcmp(key, "birth_day") == 0)   cfg->birth_day = (int)val;
    }
    fclose(f);
}

static void config_save(const TempusConfig *cfg) {
    char path[512];
    config_file_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "tempus: could not write %s\n", path);
        return;
    }
    fprintf(f, "latitude = %.6f\n", cfg->latitude);
    fprintf(f, "longitude = %.6f\n", cfg->longitude);
    fprintf(f, "elevation = %.2f\n", cfg->elevation);
    fprintf(f, "timezone = %.2f\n", cfg->timezone);
    fprintf(f, "timezone_auto = %d\n", cfg->timezone_auto ? 1 : 0);
    fprintf(f, "alternate_names = %d\n", cfg->use_alternate_names ? 1 : 0);
    fprintf(f, "sweep_seconds = %d\n", g_scene.style.sweep_seconds ? 1 : 0);
    if (g_cfg_tz_name[0])
        fprintf(f, "timezone_name = %s\n", g_cfg_tz_name);
    if (cfg->birth_year > 0) {
        fprintf(f, "birth_year = %d\n", cfg->birth_year);
        fprintf(f, "birth_month = %d\n", cfg->birth_month);
        fprintf(f, "birth_day = %d\n", cfg->birth_day);
    }
    fclose(f);
}
#else
static void config_load(TempusConfig *cfg) { (void)cfg; }
static void config_save(const TempusConfig *cfg) { (void)cfg; }
#endif

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

// Letterspaced caps for chrome engravings (the logo's voice). Returns
// advance. draw_text_ex has no tracking; per-glyph is cheap at chrome
// sizes.
static float chrome_text_tracked(DrawCtx *d, int weight, float size,
                                 float x, float y, float track_em,
                                 const char *s) {
    char c[2] = { 0, 0 };
    float cur = x;
    for (; *s; s++) {
        c[0] = *s;
        cur += draw_text_ex(d, weight, size, cur, y, c) + track_em * size;
    }
    return cur - track_em * size - x;
}

// ---- Debug GUI ----

static void debug_gui(void) {
    struct nk_context *ctx = snk_new_frame();

    if (g_show_debug) {
        if (nk_begin(ctx, "Tempus Debug", nk_rect(10, 10, 280, 400),
                NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE |
                NK_WINDOW_MINIMIZABLE)) {

            // Time override toggle. Engaging captures the CURRENT moment
            // (stale slider values would make time jump).
            nk_layout_row_dynamic(ctx, 25, 1);
            int override = g_tempus.time_override;
            nk_checkbox_label(ctx, "Manual Time", &override);
            if (override && !g_tempus.time_override)
                scene__begin_override(&g_tempus);
            else if (!override)
                g_tempus.time_override = false;

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

            // Pacing (debug panel itself forces full rate — hide it with D
            // and use TEMPUS_PACE_LOG=1 to observe idle pacing)
            snprintf(buf, sizeof(buf), "Pace: %.0f fps (%.0f when hidden)",
                     g_desired_fps, scene_desired_fps(&g_scene));
            nk_label(ctx, buf, NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            int sweep = g_scene.style.sweep_seconds;
            nk_checkbox_label(ctx, "Sweep second hand", &sweep);
            g_scene.style.sweep_seconds = sweep;

            // Worldview station (tweened camera flight between vantages)
            nk_layout_row_dynamic(ctx, 25, 1);
            static const char *combo_names[ST_COUNT];
            for (int ci = 0; ci < ST_COUNT; ci++)
                combo_names[ci] = station_table[ci].name;
            int wv = nk_combo(ctx, combo_names, WV_COUNT,
                              (int)g_worldview, 22, nk_vec2(240, 140));
            if (wv != (int)g_worldview)
                set_worldview((Worldview)wv);

            // Globe overlay: static encodings of solar motion
            static const char *overlay_names[GLOBE_OVERLAY_COUNT] = {
                "Overlay: none",
                "Chronophotograph",
                "Daylight field",
                "Solstice envelope",
                "Sun paths + analemma",
            };
            nk_layout_row_dynamic(ctx, 25, 1);
            g_scene.style.globe_overlay = nk_combo(ctx, overlay_names,
                GLOBE_OVERLAY_COUNT, g_scene.style.globe_overlay, 22,
                nk_vec2(240, 160));

            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);

            // Calendar zoom
            nk_layout_row_dynamic(ctx, 20, 1);
            char zl[64];
            snprintf(zl, sizeof(zl), "Zoom: %.3f", g_scene.calendar_state.zoom);
            nk_label(ctx, zl, NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 20, 1);
            float zf = (float)g_scene.calendar_state.zoom;
            nk_slider_float(ctx, 0.0f, &zf, 1.0f, 0.001f);
            g_scene.calendar_state.zoom = zf;

            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);

            // Actions
            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Solar Warp"))
                timeview_start_day_warp(&g_scene.solar_state.tv, 1.0, 8.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
            if (nk_button_label(ctx, "Toggle Names"))
                g_tempus.config.use_alternate_names = !g_tempus.config.use_alternate_names;

            // Latitude/Longitude
            nk_layout_row_dynamic(ctx, 25, 1);
            snprintf(buf, sizeof(buf), "Location (UTC%+.1f%s):",
                     g_tempus.config.timezone,
                     g_tempus.config.timezone_auto ? " auto" : "");
            nk_label(ctx, buf, NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 2);
            double lat = g_tempus.config.latitude;
            double lon = g_tempus.config.longitude;
            nk_property_double(ctx, "#Lat", -90, &lat, 90, 0.1, 0.1);
            nk_property_double(ctx, "#Lon", -180, &lon, 180, 0.1, 0.1);
            if (lat != g_tempus.config.latitude || lon != g_tempus.config.longitude)
                tempus_set_location(&g_tempus, lat, lon);

            nk_layout_row_dynamic(ctx, 30, 1);
            if (nk_button_label(ctx, "Save Config"))
                config_save(&g_tempus.config);
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
    config_load(&cfg);
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
    if (g_cfg_tz_name[0]) {
        setenv("TZ", g_cfg_tz_name, 1);
        tzset();
    }
#endif
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
    scene_register_view(&g_scene, VIEW_ORRERY,    &orrery_vtable);
    scene_register_view(&g_scene, VIEW_SKY,       &sky_vtable);
    scene_register_view(&g_scene, VIEW_HORAE,     &horae_vtable);
    scene_register_view(&g_scene, VIEW_ROTAE,     &rotae_vtable);
    scene_register_view(&g_scene, VIEW_SAEC,      &saec_vtable);
    scene_register_view(&g_scene, VIEW_ORBIS,     &orbis_vtable);
    scene_register_view(&g_scene, VIEW_LVMEN,     &lumen_vtable);
    scene_register_view(&g_scene, VIEW_OFFIC,     &offic_vtable);
    scene_register_view(&g_scene, VIEW_DRACO,     &draco_vtable);
    scene_register_view(&g_scene, VIEW_CLOCKBACK, &clockback_vtable);
    scene_init_views(&g_scene, &g_tempus);

    if (g_cfg_sweep_override >= 0)
        g_scene.style.sweep_seconds = g_cfg_sweep_override != 0;

    g_scene.cycle_len = 0;

    if (getenv("TEMPUS_HELIO")) {
        g_worldview = WV_TELLVS;
        g_scene.helio_blend = 1.0;
        g_scene.calendar_state.zoom = 1.0;
        g_scene.calendar_state.target_zoom = 1.0;
    }
    if (getenv("TEMPUS_SYSTEM")) {
        g_worldview = WV_MACHINA;
        g_scene.helio_blend = 1.0;
        g_scene.system_blend = 1.0;
        g_scene.calendar_state.zoom = 0.0;
        g_scene.calendar_state.target_zoom = 0.0;
    }
    if (getenv("TEMPUS_ROTAE")) {
        g_worldview = WV_ROTAE;
        g_scene.rotae_blend = 1.0;
    }
    if (getenv("TEMPUS_SAECVLVM")) {
        g_worldview = WV_SAECVLVM;
        g_scene.saec_blend = 1.0;
    }
    if (getenv("TEMPUS_HORAE")) {
        g_worldview = WV_HORAE;
        g_scene.horae_blend = 1.0;
    }
    if (getenv("TEMPUS_DRACO")) {
        g_worldview = WV_DRACO;
        g_scene.draco_blend = 1.0;
    }
    const char *drblend = getenv("TEMPUS_DRACOBLEND");  // dev: pin morph
    if (drblend) {
        g_worldview = WV_DRACO;
        g_scene.draco_blend = atof(drblend);
    }
    if (getenv("TEMPUS_OFFICIVM")) {
        g_worldview = WV_OFFICIVM;
        g_scene.offic_blend = 1.0;
    }
    if (getenv("TEMPUS_ORBIS")) {
        g_worldview = WV_ORBIS;
        g_scene.orbis_blend = 1.0;
        g_scene.orbis_wheel = 1.0;
    }
    if (getenv("TEMPUS_SKY")) {
        g_worldview = WV_CAELVM;
        g_scene.helio_blend = 1.0;
        g_scene.system_blend = 1.0;
        g_scene.sky_blend = 1.0;
        g_scene.calendar_state.zoom = 0.0;
        g_scene.calendar_state.target_zoom = 0.0;
    }
    const char *blend = getenv("TEMPUS_BLEND");   // dev: pin mid-morph
    if (blend) g_scene.helio_blend = atof(blend);
    const char *sblend = getenv("TEMPUS_SYSBLEND");  // dev: pin system stage
    if (sblend) {
        g_scene.helio_blend = 1.0;
        g_scene.system_blend = atof(sblend);
    }
    const char *lkalt = getenv("TEMPUS_LOOKALT");   // dev: pin the look
    if (lkalt) g_scene.sky_state.view_alt = (float)atof(lkalt);
    const char *lkaz = getenv("TEMPUS_LOOKAZ");
    if (lkaz) g_scene.sky_state.view_az = (float)atof(lkaz);
    const char *oblend = getenv("TEMPUS_ORBISBLEND");  // dev: pin closeup
    if (oblend) {
        g_worldview = WV_ORBIS;
        g_scene.orbis_blend = atof(oblend);
    }
    const char *kblend = getenv("TEMPUS_SKYBLEND");  // dev: pin sky morph
    if (kblend) {
        g_worldview = WV_CAELVM;
        g_scene.helio_blend = 1.0;
        g_scene.system_blend = 1.0;
        g_scene.sky_blend = atof(kblend);
        g_scene.calendar_state.zoom = 0.0;
        g_scene.calendar_state.target_zoom = 0.0;
    }
    apply_view_mode();
    set_view_opacities();

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
        unsigned char *ldata = stbi_load("assets/land_mask.png", &lw, &lh, &ln, 1);
        if (!ldata)
            ldata = stbi_load("../assets/land_mask.png", &lw, &lh, &ln, 1);
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
        unsigned char *mdata = stbi_load("assets/moon_mask.png", &mw, &mh, &mn, 1);
        if (!mdata)
            mdata = stbi_load("../assets/moon_mask.png", &mw, &mh, &mn, 1);
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

    g_shot_path = getenv("TEMPUS_SHOT");
    if (g_shot_path) {
        g_shot_countdown = 30;  // let animations settle
        g_show_debug = false;
    }
    const char *fly = getenv("TEMPUS_FLY");
    if (fly && g_shot_path) {
        const char *comma = strchr(fly, ',');
        if (comma) {
            int a = station_by_name(fly, (size_t)(comma - fly));
            int b = station_by_name(comma + 1, strlen(comma + 1));
            if (a >= 0 && b >= 0 && a != b) {
                g_fly_from = a;
                g_fly_to = b;
                const char *ft = getenv("TEMPUS_FLYT");
                if (ft) g_fly_t = atof(ft);
                snap_station((Worldview)a);
                g_fly_settle = 5;   // frames to settle before takeoff
                fprintf(stderr, "fly: %s -> %s @ %.2fs\n",
                        g_worldview_names_at(a), g_worldview_names_at(b),
                        g_fly_t);
            }
        }
        if (g_fly_from < 0)
            fprintf(stderr, "TEMPUS_FLY: bad spec '%s'\n", fly);
    }
    g_pace_log = getenv("TEMPUS_PACE_LOG") != NULL;
    if (g_pace_log)
        g_show_debug = false;  // the debug panel forces full rate
    if (getenv("TEMPUS_TICK"))
        g_scene.style.sweep_seconds = false;

    // Dev: preselect a globe overlay mode
    const char *overlay = getenv("TEMPUS_OVERLAY");
    if (overlay)
        g_scene.style.globe_overlay = atoi(overlay) % GLOBE_OVERLAY_COUNT;

    // Dev: pin the calendar zoom for screenshots
    const char *zoom = getenv("TEMPUS_ZOOM");
    if (zoom) {
        g_scene.calendar_state.zoom = atof(zoom);
        g_scene.calendar_state.target_zoom = g_scene.calendar_state.zoom;
    }

    // Dev: pin the clock to a fraction of the day/year for screenshots
    // Dev harness: pinned stations and screenshot runs do not tour
    if (g_worldview != WV_HOROLOGIVM || getenv("TEMPUS_SHOT"))
        g_tour_active = false;

    const char *daypct = getenv("TEMPUS_DAYPCT");
    if (daypct) {
        g_tempus.time_override = true;
        g_tempus.override_day_pct = atof(daypct);
        const char *yearpct = getenv("TEMPUS_YEARPCT");
        if (yearpct) g_tempus.override_year_pct = atof(yearpct);
    }
}

static void frame(void) {
    float w = sapp_widthf();
    float h = sapp_heightf();
    double dt = sapp_frame_duration();
    if (g_fly_from >= 0)
        dt = 1.0 / 60.0;   // determinism: same flight, same pixels
    g_time += dt;

    // Flight harness: settle, take off, capture at the appointed second
    if (g_fly_from >= 0) {
        if (g_fly_settle > 0) {
            if (--g_fly_settle == 0) {
                set_worldview((Worldview)g_fly_to);
                g_fly_clock = 0.0;
            }
        } else if (g_fly_clock >= 0.0) {
            g_fly_clock += dt;
            if (g_fly_clock >= g_fly_t) {
                save_shot(g_shot_path);
                sapp_request_quit();
            }
        }
    }

    // Tour clock: advance stations while it runs; wake it after a
    // minute of stillness
    g_idle_secs += dt;
    if (g_tour_active) {
        g_tour_timer += dt;
        if (g_tour_timer >= TOUR_FLIGHT + TOUR_DWELL) {
            g_tour_timer = 0;
            g_tour_idx = (g_tour_idx + 1) % TOUR_LEN;
            set_worldview(g_tour[g_tour_idx]);
        }
    } else if (g_idle_secs >= TOUR_RESUME) {
        g_tour_active = true;
        g_tour_idx = 0;
        g_tour_timer = 0;
        set_worldview(g_tour[0]);
    }

    g_desired_fps = scene_desired_fps(&g_scene);
    if (g_show_debug || g_time < g_boost_until)
        g_desired_fps = g_scene.pace.animate_fps;  // interactive: full rate

    bool do_update = (g_time - g_last_update) >= (1.0 / g_desired_fps) - dt * 0.5;

    if (do_update) {
        double upd_dt = g_time - g_last_update;
        if (upd_dt < 0 || upd_dt > 2.0) upd_dt = dt;
        g_last_update = g_time;
        g_updates_this_sec++;

        tempus_update(&g_tempus, g_time);
        scene_update(&g_scene, &g_tempus, upd_dt);
        set_view_opacities();

        // Build tempus draw commands (system stage pulls the camera back)
        float scale = h / 1280.0f * sys_camera_scale();
        draw_begin(&g_draw, w, h);
        g_draw.sx = scale;
        g_draw.sy = scale;
        scene_render(&g_scene, &g_draw, &g_tempus);

        // Worldview annunciator: the station names engraved upper-right,
        // active one lit. Chrome space — plain 1280-tall units, no
        // camera pull-back — so it holds still while the instrument
        // flies. Hit rects published for the click handler.
        {
            g_draw.sx = g_draw.sy = h / 1280.0f;
            g_draw.tx = g_draw.ty = 0;
            float half_w = (w / h) * 640.0f;
            int wv_w = _font_compat[FONT_date].weight;
            float wv_sz = 21.0f;
            float x_r = half_w - 42.0f;
            float y = -640.0f + 46.0f;
            for (int i = 0; i < WV_COUNT; i++) {
                float tw = sdf_measure_width(wv_w, g_worldview_names_at(i))
                         * wv_sz;
                draw_set_color(&g_draw, (int)g_worldview == i
                    ? dca(0.78f, 0.75f, 0.68f, 0.95f)
                    : dca(0.50f, 0.49f, 0.46f, 0.38f));
                draw_text_ex(&g_draw, wv_w, wv_sz, x_r - tw, y,
                             g_worldview_names_at(i));
                g_wv_btn[i][0] = x_r - tw - 10.0f;
                g_wv_btn[i][1] = y - 6.0f;
                g_wv_btn[i][2] = x_r + 10.0f;
                g_wv_btn[i][3] = y + wv_sz + 6.0f;
                y += 34.0f;
            }
        }

        // Civil-time readout, upper-left: the modern register in chrome
        // space, opposite the annunciator. Location + GMT offset over
        // the date and 12-hour time.
        {
            float half_w = (w / h) * 640.0f;
            float x0 = -half_w + 42.0f;

            // Nearest charted city to the configured home. Rescanned
            // only when the location moves (ORBIS drags it live).
            static double loc_la = 999.0, loc_lo = 999.0;
            static const char *loc_city = NULL;
            static double loc_km = 0.0;
            if (g_tempus.config.latitude != loc_la ||
                g_tempus.config.longitude != loc_lo) {
                loc_la = g_tempus.config.latitude;
                loc_lo = g_tempus.config.longitude;
                double la = loc_la * M_PI / 180.0;
                double lo = loc_lo * M_PI / 180.0;
                double o0 = cos(la) * cos(lo), o1 = cos(la) * sin(lo);
                double o2 = sin(la);
                int best = 0;
                double bd = -2.0;
                for (int i = 0; i < CITY_NUM; i++) {
                    double cla = city_pts[i].lat * 0.01 * M_PI / 180.0;
                    double clo = city_pts[i].lon * 0.01 * M_PI / 180.0;
                    double dd = cos(cla) * cos(clo) * o0
                              + cos(cla) * sin(clo) * o1 + sin(cla) * o2;
                    if (dd > bd) { bd = dd; best = i; }
                }
                if (bd > 1.0) bd = 1.0;
                loc_city = city_pts[best].name;
                loc_km = acos(bd) * 6371.0;
            }

            char tzs[16];
            double tz = g_tempus.config.timezone;
            int tzh = (int)fabs(tz);
            int tzm = (int)((fabs(tz) - tzh) * 60.0 + 0.5);
            if (tzm)
                snprintf(tzs, sizeof(tzs), "GMT%c%d:%02d",
                         tz < 0 ? '-' : '+', tzh, tzm);
            else
                snprintf(tzs, sizeof(tzs), "GMT%c%d",
                         tz < 0 ? '-' : '+', tzh);

            char loc_label[64];
            if (loc_city && loc_km < 150.0)
                snprintf(loc_label, sizeof(loc_label), "%s", loc_city);
            else
                snprintf(loc_label, sizeof(loc_label), "%.2f%s %.2f%s",
                         fabs(loc_la), loc_la >= 0 ? "N" : "S",
                         fabs(loc_lo), loc_lo >= 0 ? "E" : "W");

            char mon[16];
            snprintf(mon, sizeof(mon), "%s",
                     cal_month_names[g_tempus.month - 1]);
            for (char *p = mon + 1; *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
            char dateline[40];
            snprintf(dateline, sizeof(dateline), "%s %d, %d",
                     mon, g_tempus.day, g_tempus.year);

            int hh = g_tempus.hours % 12;
            if (hh == 0) hh = 12;
            char timeline[16];
            snprintf(timeline, sizeof(timeline), "%d:%02d",
                     hh, g_tempus.mins);
            const char *meridiem = g_tempus.hours < 12 ? "AM" : "PM";

            // Engraved caps up top: place tracked wide, offset dimmer.
            // Below, the date in light figures; then the hour as the
            // hero line, meridiem in small caps sharing its baseline.
            float lx = x0;
            draw_set_color(&g_draw, dca(0.62f, 0.60f, 0.55f, 0.62f));
            lx += chrome_text_tracked(&g_draw, SDF_WEIGHT_MEDIUM, 15.0f,
                                      lx, -604.0f, 0.38f, loc_label);
            draw_set_color(&g_draw, dca(0.50f, 0.49f, 0.46f, 0.40f));
            chrome_text_tracked(&g_draw, SDF_WEIGHT_MEDIUM, 15.0f,
                                lx + 22.0f, -604.0f, 0.38f, tzs);

            draw_set_color(&g_draw, dca(0.78f, 0.75f, 0.68f, 0.78f));
            draw_text_ex(&g_draw, SDF_WEIGHT_LIGHT, 40.0f,
                         x0, -578.0f, dateline);

            draw_set_color(&g_draw, dca(0.78f, 0.75f, 0.68f, 0.95f));
            float tw = draw_text_ex(&g_draw, SDF_WEIGHT_LIGHT, 68.0f,
                                    x0, -528.0f, timeline);
            float mer_y = -528.0f + sdf_nascent[SDF_WEIGHT_LIGHT] * 68.0f
                        - sdf_nascent[SDF_WEIGHT_MEDIUM] * 20.0f;
            draw_set_color(&g_draw, dca(0.62f, 0.60f, 0.55f, 0.62f));
            chrome_text_tracked(&g_draw, SDF_WEIGHT_MEDIUM, 20.0f,
                                x0 + tw + 12.0f, mer_y, 0.20f, meridiem);
        }

        // Build nuklear GUI
        if (g_show_debug)
            debug_gui();

        // Upload tempus buffers
        if (g_draw.num_verts > 0 && g_draw.num_indices > 0) {
            sg_update_buffer(g_vbuf, &(sg_range){
                .ptr = g_draw.verts,
                .size = (size_t)g_draw.num_verts * sizeof(DrawVertex),
            });
            sg_update_buffer(g_ibuf, &(sg_range){
                .ptr = g_draw.indices,
                .size = (size_t)g_draw.num_indices * sizeof(uint32_t),
            });
        }
    }

    if (g_pace_log) {
        g_pace_log_timer += dt;
        if (g_pace_log_timer >= 1.0) {
            fprintf(stderr, "pace: desired=%.1ffps updates=%d/s\n",
                    g_desired_fps, g_updates_this_sec);
            g_pace_log_timer = 0;
            g_updates_this_sec = 0;
        }
    }

    // Present every vsync (re-presents previous buffers on skipped frames,
    // which keeps the swapchain valid at negligible GPU cost).
    sg_begin_pass(&(sg_pass){
        .action = g_pass_action,
        .swapchain = sglue_swapchain(),
    });

    if (g_draw.num_indices > 0) {
        int drawn = 0;
        for (int gi = 0; gi <= g_draw.num_globes; gi++) {
            int upto = (gi < g_draw.num_globes)
                ? g_draw.globes[gi].split_index : g_draw.num_indices;
            if (upto > drawn) {
                sg_apply_pipeline(g_pip);
                sg_apply_bindings(&g_bind);
                sg_draw(drawn, upto - drawn, 1);
                drawn = upto;
            }
            if (gi >= g_draw.num_globes) break;

            const GlobeCmd *gc = &g_draw.globes[gi];
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

    // Draw nuklear GUI on top (only built on update frames; debug mode
    // forces full rate so every frame is an update frame)
    if (g_show_debug && do_update)
        snk_render(sapp_width(), sapp_height());

    sg_end_pass();
    sg_commit();

    if (getenv("TEMPUS_DEBUG_GLOBES") && sapp_frame_count() == 10) {
        fprintf(stderr, "num_globes=%d solar.helio=%.3f blend=%.3f op=[%.2f %.2f %.2f %.2f] layers=%d\n",
                g_draw.num_globes, g_scene.solar_state.helio,
                g_scene.helio_blend,
                g_scene.views[VIEW_CALENDAR].opacity,
                g_scene.views[VIEW_SOLAR].opacity,
                g_scene.views[VIEW_CLOCK].opacity,
                g_scene.views[VIEW_ORRERY].opacity,
                g_scene.num_layers);
        for (int i = 0; i < g_draw.num_globes; i++) {
            const GlobeCmd *gc = &g_draw.globes[i];
            fprintf(stderr, "  g%d pos=(%.0f,%.0f) r=%.0f light=(%.2f,%.2f,%.2f) rot0=%.2f\n",
                    i, gc->cx, gc->cy, gc->radius,
                    gc->light[0], gc->light[1], gc->light[2], gc->rot[0]);
        }
    }

    if (g_shot_path && g_fly_from < 0 && --g_shot_countdown == 0) {
        save_shot(g_shot_path);
        sapp_request_quit();
    }
}

static void event(const sapp_event *e) {
    // Shot and flight-harness runs are measurement instruments: they
    // ignore ALL input. (A baseline bake once caught stray desktop
    // clicks and keystrokes — five polluted reference frames.)
    if (g_shot_path)
        return;

    // Any input: briefly return to full rate so interaction feels live
    g_boost_until = g_time + 1.0;

    // The tour: a click (or key) ends it; any motion defers its return
    g_idle_secs = 0;
    if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN
        || e->type == SAPP_EVENTTYPE_KEY_DOWN
        || e->type == SAPP_EVENTTYPE_TOUCHES_BEGAN)
        g_tour_active = false;

    // Pass events to nuklear first
    if (snk_handle_event(e))
        return; // nuklear consumed the event

    // Worldview annunciator clicks (chrome space: no camera zoom)
    if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
        float cs = sapp_heightf() / 1280.0f;
        float cx = (e->mouse_x - sapp_widthf() * 0.5f) / cs;
        float cy = (e->mouse_y - sapp_heightf() * 0.5f) / cs;
        for (int i = 0; i < WV_COUNT; i++) {
            if (cx >= g_wv_btn[i][0] && cx <= g_wv_btn[i][2]
                && cy >= g_wv_btn[i][1] && cy <= g_wv_btn[i][3]) {
                set_worldview((Worldview)i);
                return;
            }
        }
    }

    // Pointer events in world coordinates (the draw space is a virtual
    // 1280-unit-tall frame centered on screen)
    if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN
        || e->type == SAPP_EVENTTYPE_MOUSE_MOVE
        || e->type == SAPP_EVENTTYPE_MOUSE_UP) {
        float scale = sapp_heightf() / 1280.0f * sys_camera_scale();
        float wx = (e->mouse_x - sapp_widthf() * 0.5f) / scale;
        float wy = (e->mouse_y - sapp_heightf() * 0.5f) / scale;
        int phase = (e->type == SAPP_EVENTTYPE_MOUSE_DOWN) ? 0
                  : (e->type == SAPP_EVENTTYPE_MOUSE_MOVE) ? 1 : 2;
        scene_pointer(&g_scene, &g_tempus, phase, wx, wy);
    }

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
                timeview_start_day_warp(&g_scene.solar_state.tv, 1.0, 8.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
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
        .high_dpi = true,
        .window_title = "T E M P V S",
        .logger.func = slog_func,
    };
}
