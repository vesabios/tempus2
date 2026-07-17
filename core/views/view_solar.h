// view_solar.h — Sunrise/solar dial view

#ifndef VIEW_SOLAR_H
#define VIEW_SOLAR_H

#include "../view.h"

#define SOLAR_PATH_N 49   // samples per sun path (30-min steps)
#define SOLAR_ANA_N  74   // analemma samples (~5-day steps)

typedef struct {
    float x, y;   // dial unit coords (multiply by dial radius)
    bool  up;     // sun above horizon
} SunPathPt;

struct SolarViewState {
    TimeView tv;  // must be first field
    double  opacity;
    bool    warping;
    int     last_mins;
    double  zenith;
    double  azimuth;
    double  sunrise_hr;
    double  sunset_hr;
    double  last_jd;      // date component of the solar cache key
    double  helio;        // mirrored scene helio_blend (orrery owns globe)

    // Cached static-encoding geometry (GLOBE_OVERLAY_SUNPATHS). Paths are
    // per-date (computed in local mean solar time — the curves are pure
    // geometry, so the time frame doesn't matter). The analemma is drawn
    // at the CURRENT hour in the clock's own time frame (so it passes
    // through the live sun marker), cached on 10-minute steps.
    double    paths_jd;
    double    ana_pct;    // quantized day_pct the analemma was computed at
    SunPathPt path_today[SOLAR_PATH_N];
    SunPathPt path_jun[SOLAR_PATH_N];
    SunPathPt path_dec[SOLAR_PATH_N];
    SunPathPt analemma[SOLAR_ANA_N];
};

#endif // VIEW_SOLAR_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_SOLAR_IMPL)
#define VIEW_SOLAR_IMPL

// SPA at an explicit date + fraction-of-day in the given timezone
static void solar__spa_at(const TempusConfig *cfg, int year, int month,
                          int day, double day_pct, double tz, spa_data *spa) {
    memset(spa, 0, sizeof(*spa));
    int total_secs = (int)(day_pct * 86399.0);
    spa->year = year;
    spa->month = month;
    spa->day = day;
    spa->hour = total_secs / 3600;
    spa->minute = (total_secs % 3600) / 60;
    spa->second = total_secs % 60;
    spa->timezone = tz;
    spa->delta_t = 67;
    spa->longitude = cfg->longitude;
    spa->latitude = cfg->latitude;
    spa->elevation = cfg->elevation;
    spa->pressure = 820;
    spa->temperature = 11;
    spa->atmos_refract = 0.5667;
    spa->function = SPA_ZA;
    spa_calculate(spa);
}

static inline SunPathPt solar__dial_pt(const spa_data *spa) {
    double az = spa->azimuth * M_PI / 180.0;
    double rf = sin(spa->zenith * M_PI / 180.0);
    return (SunPathPt){
        .x = (float)(sin(az) * rf),
        .y = (float)(-cos(az) * rf),
        .up = spa->zenith < 90.0,
    };
}

static void solar__compute_path(SunPathPt *pts, int n, const TempusConfig *cfg,
                                int year, int month, int day) {
    for (int i = 0; i < n; i++) {
        spa_data spa;
        solar__spa_at(cfg, year, month, day, (double)i / (n - 1),
                      cfg->longitude / 15.0, &spa);
        pts[i] = solar__dial_pt(&spa);
    }
}

static void solar__doy_to_date(int year, int doy, int *m, int *d) {
    int mm = 1;
    while (mm <= 12 && doy >= cal_days_in_month(mm, year)) {
        doy -= cal_days_in_month(mm, year);
        mm++;
    }
    *m = mm > 12 ? 12 : mm;
    *d = doy + 1;
}

static void solar__compute_paths(SolarViewState *st, const Tempus *t) {
    const TempusConfig *cfg = &t->config;
    int year = st->tv.year;

    solar__compute_path(st->path_today, SOLAR_PATH_N, cfg,
                        st->tv.year, st->tv.month, st->tv.day);
    solar__compute_path(st->path_jun, SOLAR_PATH_N, cfg, year, 6, 21);
    solar__compute_path(st->path_dec, SOLAR_PATH_N, cfg, year, 12, 21);

    st->paths_jd = st->tv.jd_current;
}

// Analemma at the current hour: the sun's position at this clock time on
// every day of the year. Sampled in the clock's own time frame so the
// curve passes through the live sun marker.
static void solar__compute_analemma(SolarViewState *st, const Tempus *t,
                                    double day_pct) {
    const TempusConfig *cfg = &t->config;
    int year = st->tv.year;
    int days = cal_days_in_year(year);
    for (int k = 0; k < SOLAR_ANA_N; k++) {
        int doy = (int)((double)k / SOLAR_ANA_N * days);
        int m, d;
        solar__doy_to_date(year, doy, &m, &d);
        spa_data spa;
        solar__spa_at(cfg, year, m, d, day_pct, cfg->timezone, &spa);
        st->analemma[k] = solar__dial_pt(&spa);
    }
    st->ana_pct = day_pct;
}

// Draw a polyline of above-horizon segments in dial coords
static void solar__draw_path(DrawCtx *d, const SunPathPt *pts, int n,
                             float r, float offy, bool close) {
    int last = close ? n : n - 1;
    for (int i = 0; i < last; i++) {
        const SunPathPt *a = &pts[i];
        const SunPathPt *b = &pts[(i + 1) % n];
        if (!a->up || !b->up) continue;
        draw_line(d, a->x * r, offy + a->y * r, b->x * r, offy + b->y * r, 1.0f);
    }
}

static void solar_init(void *buf, const Tempus *t, const RenderStyle *s) {
    SolarViewState *st = (SolarViewState *)buf;
    st->opacity = 1.0;
    st->last_mins = -1;
    st->zenith = t->zenith;
    st->azimuth = t->solar.azimuth;
    st->sunrise_hr = t->solar.sunrise;
    st->sunset_hr = t->solar.sunset;
    (void)s;
}

static void solar_enter(void *buf, const Tempus *t, Scene *sc) {
    SolarViewState *st = (SolarViewState *)buf;
    st->warping = true;
    st->last_mins = -1;
    timeview_start_day_warp(&st->tv, 1.0, 10.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
    (void)t; (void)sc;
}

static void solar_exit(void *buf, const Tempus *t, Scene *sc) {
    SolarViewState *st = (SolarViewState *)buf;
    st->warping = false;
    timeview_stop_warp(&st->tv);
    (void)t; (void)sc;
}

static void solar_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    SolarViewState *st = (SolarViewState *)buf;
    (void)dt;
    const TimeView *tv = &st->tv;
    st->helio = sc->helio_blend;

    if (sc->style.globe_overlay == GLOBE_OVERLAY_SUNPATHS) {
        bool date_changed = st->paths_jd != tv->jd_current;
        if (date_changed)
            solar__compute_paths(st, t);
        double q = floor(tv->day_pct * 144.0) / 144.0;   // 10-min steps
        if (st->ana_pct != q || date_changed)
            solar__compute_analemma(st, t, q);
    }

    int combined = tv->mins | (tv->hours << 8);
    if (combined != st->last_mins || tv->jd_current != st->last_jd) {
        spa_data solar;
        timeview_fill_spa(&solar, tv, &t->config, SPA_ALL);
        spa_calculate(&solar);

        st->zenith = solar.zenith;
        st->azimuth = solar.azimuth;
        st->sunrise_hr = solar.sunrise;
        st->sunset_hr = solar.sunset;
        st->last_mins = combined;
        st->last_jd = tv->jd_current;
    }
}

static void solar_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    // The earth instrument — globe, dial furniture, markers, sky-dome —
    // is rendered by the orrery view at EVERY morph state, including pure
    // geocentric. Single owner, single draw order: no handoff artifacts.
    // This view only computes solar data (see solar_update); it draws
    // nothing. The shell keeps its opacity at 0.
    (void)buf; (void)d; (void)t; (void)s;
}

#if 0  // retired dial rendering — superseded by view_orrery.h
static void solar_render_retired(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const SolarViewState *st = (const SolarViewState *)buf;
    float offy = s->sunrise_dial_offset;
    float r = s->sunrise_dial_radius;

    // Background
    draw_set_color(d, s->clear);
    draw_circle_filled(d, 0, offy, r + 14.0f);

    // Earth globe: orthographic view from above the observer, lit by the
    // real sun. Everything drawn after this composites on top. During the
    // geo<->helio morph the orrery view owns (and morphs) the globe.
    // Threshold matches the scene's render-skip cutoff exactly — the
    // frame the orrery starts rendering is the frame this stops.
    if (st->helio <= 0.001) {
        GlobeCmd *g = draw_globe_slot(d, 0, offy, r);
        if (g) {
            globe_rotation(t->config.latitude, t->config.longitude, g->rot);
            globe_sun_dir(st->azimuth, st->zenith, g->light);
            g->overlay = s->globe_overlay;
            g->declination = (float)t->solar.delta;
        }
    }

    // Static sun-path encodings (drawn over the globe): solstice arcs
    // bracket today's arc; the analemma loops through solar noon.
    if (s->globe_overlay == GLOBE_OVERLAY_SUNPATHS
        && st->paths_jd == st->tv.jd_current) {
        draw_set_color(d, dca(0.55f, 0.55f, 0.55f, 0.45f));
        solar__draw_path(d, st->path_jun, SOLAR_PATH_N, r, offy, false);
        solar__draw_path(d, st->path_dec, SOLAR_PATH_N, r, offy, false);
        draw_set_color(d, dca(0.85f, 0.85f, 0.85f, 0.75f));
        solar__draw_path(d, st->path_today, SOLAR_PATH_N, r, offy, false);
        draw_set_color(d, dc_scale(s->sunrise_handle, 0.8f));
        solar__draw_path(d, st->analemma, SOLAR_ANA_N, r, offy, true);
    }

    // Center reference: "you are here" ownship marker
    draw_set_color(d, dca(0.85f, 0.85f, 0.85f, 0.9f));
    draw_circle_stroked(d, 0, offy, 3.0f, 1.0f);

    // Outer ring
    draw_set_color(d, dc_scale(s->sunrise_lit, 0.8f));
    draw_circle_stroked(d, 0, offy, r + 12.0f, 1.0f);

    // Daylight arc: annulus between globe and ring (sunrise to sunset).
    // 24h time dial in the from-space frame: noon at the bottom (where a
    // northern-hemisphere sun culminates: due south), midnight at top.
    if (st->sunrise_hr > 0 && st->sunset_hr > 0) {
        double rise_pct = st->sunrise_hr / 24.0;
        double set_pct = st->sunset_hr / 24.0;
        float a0 = (float)(rise_pct * M_PI * 2.0 - M_PI * 0.5);
        float a1 = (float)(set_pct * M_PI * 2.0 - M_PI * 0.5);
        draw_set_color(d, s->sunrise_lit);
        draw_arc_filled(d, 0, offy, r + 4.0f, r + 10.0f, a0, a1, 48);
    }

    // Sun position: orthographic projection of the subsolar point — lands
    // exactly on the globe's lit pole of the terminator geometry.
    // From-space frame: azimuth 0 (north) is screen-up, east is right.
    {
        float az_angle = (float)(st->azimuth / 360.0 * M_PI * 2.0);
        float hand_len = (float)(sin(st->zenith * M_PI / 180.0) * r);
        float sx = sinf(az_angle) * hand_len;
        float sy = -cosf(az_angle) * hand_len;

        draw_set_color(d, dca(0.75f, 0.75f, 0.75f, 0.35f));
        draw_line(d, 0, offy, sx, offy + sy, 1.0f);

        DrawColor sun_c = (st->zenith > 90.0)
            ? dc_scale(s->sunrise_handle, 0.5f) : s->sunrise_handle;
        draw_set_color(d, sun_c);
        draw_circle_filled(d, sx, offy + sy, s->sun_size);
    }
}
#endif  // retired dial rendering

static const ViewVtable solar_vtable = {
    .init   = solar_init,
    .enter  = solar_enter,
    .exit   = solar_exit,
    .update = solar_update,
    .render = solar_render,
};

#endif // SCENE_DEFINED && !VIEW_SOLAR_IMPL
