// view_solar.h — Sunrise/solar dial view

#ifndef VIEW_SOLAR_H
#define VIEW_SOLAR_H

#include "../view.h"

struct SolarViewState {
    TimeView tv;  // must be first field
    double  opacity;
    bool    warping;
    int     last_mins;
    double  zenith;
    double  azimuth;
    double  sunrise_hr;
    double  sunset_hr;
};

#endif // VIEW_SOLAR_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_SOLAR_IMPL)
#define VIEW_SOLAR_IMPL

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
    timeview_start_day_warp(&st->tv, 8640.0, 10.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
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
    (void)dt; (void)sc;
    const TimeView *tv = &st->tv;

    int combined = tv->mins | (tv->hours << 8);
    if (combined != st->last_mins) {
        spa_data solar;
        timeview_fill_spa(&solar, tv, &t->config, SPA_ALL);
        spa_calculate(&solar);

        st->zenith = solar.zenith;
        st->azimuth = solar.azimuth;
        st->sunrise_hr = solar.sunrise;
        st->sunset_hr = solar.sunset;
        st->last_mins = combined;
    }
}

static void solar_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const SolarViewState *st = (const SolarViewState *)buf;
    float offy = s->sunrise_dial_offset;
    float r = s->sunrise_dial_radius;

    // Background
    draw_set_color(d, s->clear);
    draw_circle_filled(d, 0, offy, r + 14.0f);

    // Earth globe: orthographic view from above the observer, lit by the
    // real sun. Everything drawn after this composites on top.
    draw_globe(d, 0, offy, r,
               (float)t->config.latitude, (float)t->config.longitude,
               (float)st->azimuth, (float)st->zenith);

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

static const ViewVtable solar_vtable = {
    .init   = solar_init,
    .enter  = solar_enter,
    .exit   = solar_exit,
    .update = solar_update,
    .render = solar_render,
};

#endif // SCENE_DEFINED && !VIEW_SOLAR_IMPL
