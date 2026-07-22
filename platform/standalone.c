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
#include <strings.h>   // strncasecmp (the station-name prefix match)
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
#include "../core/stage.h"
#include "../core/chrome.h"
#include "../core/globe.h"
#include "../shaders/tempus.glsl.h"

// ---- Globals ----

static Tempus          g_tempus;
static Scene           g_scene;
static DrawCtx         g_draw;
static bool            g_show_debug = false;   // D toggles

// (The GPU layer — objects, atlas, pipelines, submission — lives
// in platform/tempus_gfx.h, shared with the screensaver host.)

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
// TEMPUS_FILM=<dir> renders the TOUR as a numbered frame sequence at a
// fixed 30fps — the instrument in motion, which no still can hold.
// TEMPUS_FILM_SECS sets its length, TEMPUS_FILM_DWELL the pause at each
// station before the next flight.
static const char *g_film_dir = NULL;
static int    g_film_frame = 0;
static double g_film_secs = 40.0;
static double g_film_dwell = 2.6;
static double g_film_clock = 0.0;
static int    g_film_idx = 0;
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
#define WV_ASTROLAB   ST_ASTROLAB
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

// CAELVM's layer toggles, lower right. Minimal by intent (Seren):
// lit when on, dimmed when off, a click flips it. No boxes, no rule.
#define SKY_TOG_N 5
static float g_tog_btn[SKY_TOG_N][4];
static const char *g_tog_name[SKY_TOG_N] = {
    "PLANETAE", "FIGVRAE", "STELLAE", "CANCELLI", "ANALEMMA",
};
static bool *sky_tog_flag(int i) {
    SkyViewState *k = &g_scene.sky_state;
    switch (i) {
        case 0: return &k->show_planets;
        case 1: return &k->show_figures;
        case 2: return &k->show_stars;
        case 3: return &k->show_cage;
        default: return &k->show_analemma;
    }
}
// Time controls: NVNC (return to the living present) and the two
// fast-forward rates, in minutes of instrument time per real second
static double g_ffwd = 0.0;
static float g_tc_btn[3][4];

// (The camera lives in core/stage.h, shared with the saver.)
static float sys_camera_scale(void) {
    return tempus_camera_scale(&g_scene);
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
    g_scene.astro_blend  = (wv == WV_ASTROLAB) ? 1.0 : 0.0;
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

// A TOUR flight is the instrument showing itself, so it drives every
// control including the wheel's zoom. A USER flight leaves the wheel
// alone at the two-state stations — that zoom is the user's, and
// walking between HOROLOGIVM / TELLVS / OFFICIVM must not reach in
// and change it (Seren).
static bool g_flight_is_tour = false;
static void set_worldview(Worldview wv);

static void set_worldview_tour(Worldview wv) {
    g_flight_is_tour = true;
    set_worldview(wv);
    g_flight_is_tour = false;
}

static void set_worldview(Worldview wv) {
    if (wv == g_worldview) return;
    // Leaving the world chart commits the chosen location to disk
    if (g_worldview == WV_ORBIS && !g_shot_path)
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
        [ST_ASTROLAB] = &g_scene.astro_blend,
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
    // THE WHEEL'S ZOOM IS PERSISTENT USER STATE at the two-state
    // stations (Seren): flying HOROLOGIVM <-> TELLVS <-> OFFICIVM must
    // leave the band exactly as the wheel was left, so the declared
    // fly_zoom applies only where the station has no such state of its
    // own (MACHINA in particular still needs 0 — its wheel is already
    // out at 762 and a zoomed band would run it off the frame).
    if (!station_table[wv].park_machine)
        fly_worldview(station_table[wv].fly_helio,
                      (station_table[wv].band_zoom && !g_flight_is_tour)
                          ? g_scene.calendar_state.zoom
                          : station_table[wv].fly_zoom,
                      station_table[wv].fly_system, 3.5, fly_delay);
}

static void apply_view_mode(void) {
    g_scene.num_layers = 0;
    // The shared sky wash: the chart's ground, under everything
    scene_add_layer(&g_scene, VIEW_CALBACK);
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
    scene_add_layer(&g_scene, VIEW_ASTRO);   // a chart: under the
                                             // luminaries, like the sky
    scene_add_layer(&g_scene, VIEW_LVMEN);
    scene_add_layer(&g_scene, VIEW_CLOCK);
    scene_add_layer(&g_scene, VIEW_HORAE);
    scene_add_layer(&g_scene, VIEW_ROTAE);
    scene_add_layer(&g_scene, VIEW_SAEC);
    scene_add_layer(&g_scene, VIEW_ORBIS);
    scene_add_layer(&g_scene, VIEW_OFFIC);
    // THE WHEEL IS THE INSTRUMENT'S FRAME, so it rides above everything
    // (Seren). It used to be the backmost layer, which only worked
    // because nothing ever reached past it; the sky's loupe scale
    // bleeds the bowl clear off the frame and would bury it.
    scene_add_layer(&g_scene, VIEW_CALENDAR);
}

// (The staging pass lives in core/stage.h, shared with the saver.)
static void set_view_opacities(void) {
    tempus_stage_views(&g_scene, (int)g_worldview);
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


// The app runs from the repo or its build dir: try both.
static const char *tempus_asset_path(const char *name) {
    static char buf[512];
    snprintf(buf, sizeof buf, "assets/%s", name);
    FILE *f = fopen(buf, "rb");
    if (f) { fclose(f); return buf; }
    snprintf(buf, sizeof buf, "../assets/%s", name);
    return buf;
}

#include "tempus_gfx.h"

// ---- Load font atlas ----


// Letterspaced caps for chrome engravings (the logo's voice). Returns
// advance. draw_text_ex has no tracking; per-glyph is cheap at chrome
// sizes.

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
    // Shot runs are measurement instruments: the observer is pinned to
    // defaults — user config is neither read nor written, so baselines
    // can't drift when the live app saves a new location. An explicit
    // TEMPUS_CONFIG is still honored (a deliberately frozen observer).
    if (!getenv("TEMPUS_SHOT") || getenv("TEMPUS_CONFIG"))
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
    scene_register_view(&g_scene, VIEW_ASTRO,     &astro_vtable);
    scene_register_view(&g_scene, VIEW_DRACO,     &draco_vtable);
    scene_register_view(&g_scene, VIEW_CLOCKBACK, &clockback_vtable);
    scene_register_view(&g_scene, VIEW_CALBACK,   &calback_vtable);
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
    tempus_gfx_init();
    if (getenv("TEMPUS_CHARTTEST")) {
        chart__selftest(52.52f); chart__selftest(0.0f);
        chart__selftest(-33.9f); exit(0);
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
    g_film_dir = getenv("TEMPUS_FILM");
    if (g_film_dir) {
        const char *fs = getenv("TEMPUS_FILM_SECS");
        if (fs) g_film_secs = atof(fs);
        const char *fd = getenv("TEMPUS_FILM_DWELL");
        if (fd) g_film_dwell = atof(fd);
        g_show_debug = false;
        g_tour_active = false;
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
    { const char *z = getenv("TEMPUS_MACHINASCALE");
      if (z) g_scene.orrery_state.true_mix = (float)atof(z); }
    { const char *z = getenv("TEMPUS_ORBISZOOM");
      if (z) g_scene.orrery_state.orbis_loupe = (float)atof(z); }
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
    if (g_film_dir)
        dt = 1.0 / 30.0;   // the film's own clock
    g_time += dt;

    // Film: fly the tour on a fixed timestep, dumping every frame
    if (g_film_dir) {
        static const Worldview film_tour[] = {
            WV_HOROLOGIVM, WV_ORBIS, WV_TELLVS, WV_MACHINA,
            WV_ASTROLAB, WV_CAELVM, WV_DRACO, WV_HORAE,
        };
        const int film_n = (int)(sizeof(film_tour) / sizeof(film_tour[0]));
        const double leg = 3.5 + g_film_dwell;
        if (g_film_frame == 0)
            snap_station(film_tour[0]);
        g_film_clock += dt;
        int want = (int)(g_film_clock / leg);
        if (want != g_film_idx && want < film_n) {
            g_film_idx = want;
            set_worldview_tour(film_tour[want % film_n]);
        }
        char path[512];
        snprintf(path, sizeof path, "%s/f%05d.png", g_film_dir,
                 g_film_frame++);
        save_shot(path);
        if (g_film_clock >= g_film_secs) sapp_request_quit();
        return;
    }

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
            set_worldview_tour(g_tour[g_tour_idx]);
        }
    } else if (g_idle_secs >= TOUR_RESUME) {
        g_tour_active = true;
        g_tour_idx = 0;
        g_tour_timer = 0;
        set_worldview_tour(g_tour[0]);
    }

    g_desired_fps = scene_desired_fps(&g_scene);
    if (g_show_debug || g_time < g_boost_until || g_ffwd > 0.0)
        g_desired_fps = g_scene.pace.animate_fps;  // interactive: full rate

    bool do_update = (g_time - g_last_update) >= (1.0 / g_desired_fps) - dt * 0.5;

    if (do_update) {
        double upd_dt = g_time - g_last_update;
        if (upd_dt < 0 || upd_dt > 2.0) upd_dt = dt;
        g_last_update = g_time;
        g_updates_this_sec++;

        // Any hands-on time control stops the fast-forward (Seren)
        if (g_ffwd > 0.0
            && (g_scene.calendar_state.wheel_dragging
                || g_scene.sky_state.hour_dragging
                || g_scene.horae_state.ring_dragging))
            g_ffwd = 0.0;
        // Fast-forward: the override river runs at DC rate
        if (g_ffwd > 0.0) {
            scene__begin_override(&g_tempus);
            scene__advance_override_days(&g_tempus,
                                         g_ffwd * upd_dt / 1440.0,
                                         false);
        }
        tempus_update(&g_tempus, g_time);
        scene_update(&g_scene, &g_tempus, upd_dt);
        set_view_opacities();

        // Build tempus draw commands (system stage pulls the camera back)
        float scale = h / 1280.0f * sys_camera_scale();
        draw_begin(&g_draw, w, h);
        g_draw.sx = scale;
        g_draw.sy = scale;
        scene_render(&g_scene, &g_draw, &g_tempus);
        if (getenv("TEMPUS_VERTTEST"))
            fprintf(stderr, "VERTS %6d / %d   IDX %7d / %d%s\n",
                    g_draw.num_verts, DRAW_MAX_VERTS,
                    g_draw.num_indices, DRAW_MAX_INDICES,
                    (g_draw.num_verts >= DRAW_MAX_VERTS
                     || g_draw.num_indices >= DRAW_MAX_INDICES)
                    ? "   *** CAPPED — DROPPING ***" : "");

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
            // Display order (Seren): the physical instruments first —
            // clock, globe, sun, system, plate, sky, dragon — then a
            // rule, then the cycle dials.
            // ROTAE, SAECVLVM, OFFICIVM and now ASTROLABIVM are
            // BUILT BUT UNLISTED
            // (Seren): the stations live on and still fly, they simply
            // have no door in the annunciator. Reachable by the debug
            // combo and the TEMPUS_* pins.
            static const int wv_order[] = {
                WV_HOROLOGIVM, WV_ORBIS, WV_TELLVS, WV_MACHINA,
                WV_CAELVM, WV_DRACO,
                WV_HORAE,
            };
            const int wv_shown = (int)(sizeof wv_order / sizeof *wv_order);
            // Every rect starts IMPOSSIBLE. g_wv_btn is static, so an
            // unlisted station would otherwise keep an all-zero rect —
            // and chrome space is centred on the screen, so a click at
            // dead centre would satisfy 0 <= x <= 0 and silently fly.
            for (int i = 0; i < WV_COUNT; i++) {
                g_wv_btn[i][0] = 1.0f; g_wv_btn[i][2] = -1.0f;
                g_wv_btn[i][1] = 1.0f; g_wv_btn[i][3] = -1.0f;
            }
            for (int k = 0; k < wv_shown; k++) {
                int i = wv_order[k];
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
                if (k == 5) {   // the rule between worlds and wheels
                    draw_set_color(&g_draw,
                                   dca(0.50f, 0.49f, 0.46f, 0.30f));
                    draw_line(&g_draw, x_r - 96.0f, y + 2.0f,
                              x_r, y + 2.0f, 1.0f);
                    y += 18.0f;
                }
            }

            // ---- CAELVM's layer toggles, lower right ----
            {
                float sky_a = (float)g_scene.sky_blend;
                for (int i = 0; i < SKY_TOG_N; i++) {
                    g_tog_btn[i][0] = 1.0f; g_tog_btn[i][2] = -1.0f;
                    g_tog_btn[i][1] = 1.0f; g_tog_btn[i][3] = -1.0f;
                }
                if (sky_a > 0.02f) {
                    float ty = 640.0f - 40.0f - SKY_TOG_N * 30.0f;
                    for (int i = 0; i < SKY_TOG_N; i++) {
                        bool on = *sky_tog_flag(i);
                        float tw2 = sdf_measure_width(wv_w,
                                        g_tog_name[i]) * 17.0f;
                        draw_set_color(&g_draw, on
                            ? dca(0.92f, 0.90f, 0.86f, 0.95f * sky_a)
                            : dca(0.42f, 0.41f, 0.39f, 0.55f * sky_a));
                        draw_text_ex(&g_draw, wv_w, 17.0f,
                                     x_r - tw2, ty, g_tog_name[i]);
                        g_tog_btn[i][0] = x_r - tw2 - 10.0f;
                        g_tog_btn[i][1] = ty - 5.0f;
                        g_tog_btn[i][2] = x_r + 10.0f;
                        g_tog_btn[i][3] = ty + 22.0f;
                        ty += 30.0f;
                    }
                }
            }
        }

        // The register (core/chrome.h); this host adds its controls
        tempus_chrome_readout(&g_draw, &g_tempus, w, h);
        {
            float half_w = (w / h) * 640.0f;
            float x0 = -half_w + 42.0f;
            // Time controls: NVNC returns to the living present;
            // DC pours the override forward at 600x
            {
                const char *lbl[2] = { "NVNC", "DC" };
                bool lit[2] = {
                    !g_tempus.time_override && g_ffwd == 0.0,
                    g_ffwd == 10.0,
                };
                float bx = x0;
                float by = -444.0f;
                for (int i = 0; i < 2; i++) {
                    draw_set_color(&g_draw, lit[i]
                        ? dca(0.78f, 0.75f, 0.68f, 0.95f)
                        : dca(0.50f, 0.49f, 0.46f, 0.38f));
                    float tw2 = chrome_text_tracked(&g_draw,
                        SDF_WEIGHT_MEDIUM, 16.0f, bx, by, 0.30f,
                        lbl[i]);
                    g_tc_btn[i][0] = bx - 8.0f;
                    g_tc_btn[i][1] = by - 6.0f;
                    g_tc_btn[i][2] = bx + tw2 + 8.0f;
                    g_tc_btn[i][3] = by + 24.0f;
                    bx += tw2 + 34.0f;
                }
            }
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

    tempus_gfx_draw(&g_draw, w, h);

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
        for (int i = 0; i < SKY_TOG_N; i++) {
            if (cx >= g_tog_btn[i][0] && cx <= g_tog_btn[i][2]
                && cy >= g_tog_btn[i][1] && cy <= g_tog_btn[i][3]) {
                bool *f = sky_tog_flag(i);
                *f = !*f;
                return;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (cx >= g_tc_btn[i][0] && cx <= g_tc_btn[i][2]
                && cy >= g_tc_btn[i][1] && cy <= g_tc_btn[i][3]) {
                g_scene.calendar_state.fling_vel = 0;
                if (i == 0) {
                    // NVNC: drop the override, rejoin the present
                    g_ffwd = 0.0;
                    g_tempus.time_override = false;
                } else {
                    g_ffwd = 10.0;
                    scene__begin_override(&g_tempus);
                }
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

    // THE LOUPE: scroll is unclaimed everywhere else in the instrument,
    // so it belongs to the only thing that wants a continuous scalar —
    // CAELVM's sky scale. Inert at every other station.
    if (e->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        // One gesture, whichever station owns the view: ORBIS zooms
        // its globe, CAELVM its sky. Each gate is its own blend, so
        // they can never both claim a notch.
        scene_band_zoom(&g_scene, e->scroll_y);
        machina_view_scale(&g_scene.orrery_state, e->scroll_y);
        orbis_view_loupe(&g_scene.orrery_state, e->scroll_y);
        sky_view_loupe(&g_scene.sky_state, e->scroll_y);
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
