// view_solar.h — Sunrise/solar dial view

#ifndef VIEW_SOLAR_H
#define VIEW_SOLAR_H

#include "../view.h"

struct SolarViewState {
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
    timewarp_start(&sc->warp, 360.0, 10.0, EASE_IN_OUT_QUAD, 1.5, 1.5);
    (void)t;
}

static void solar_exit(void *buf, const Tempus *t, Scene *sc) {
    SolarViewState *st = (SolarViewState *)buf;
    st->warping = false;
    timewarp_stop(&sc->warp);
    (void)t;
}

static void solar_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    SolarViewState *st = (SolarViewState *)buf;
    (void)dt;

    int eff_hours, eff_mins;
    timewarp_effective_time(&sc->warp, t->hours, t->mins, t->secs,
                           &eff_hours, &eff_mins);

    int combined = eff_mins | (eff_hours << 8);
    if (combined != st->last_mins) {
        spa_data solar;
        tempus__fill_spa(&solar, t, eff_hours, eff_mins, SPA_ALL);
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
    (void)t;
    float offy = s->sunrise_dial_offset;
    float r = s->sunrise_dial_radius;

    // Background
    draw_set_color(d, s->clear);
    draw_circle_filled(d, 0, offy, r + 14.0f);

    // Outer ring
    draw_set_color(d, dc_scale(s->sunrise_lit, 0.8f));
    draw_circle_stroked(d, 0, offy, r + 12.0f, 1.0f);

    // Sunrise arc
    if (st->sunrise_hr > 0 && st->sunset_hr > 0) {
        double rise_pct = st->sunrise_hr / 24.0;
        double set_pct = st->sunset_hr / 24.0;
        float a0 = (float)(rise_pct * M_PI * 2.0 - M_PI * 0.5);
        float a1 = (float)(set_pct * M_PI * 2.0 - M_PI * 0.5);
        draw_set_color(d, s->sunrise_lit);
        draw_arc_filled(d, 0, offy, 0, r, a0, a1, 32);
    }

    // Sun position
    if (st->azimuth > 0) {
        float az_angle = (float)((180.0 + st->azimuth) / 360.0 * M_PI * 2.0);
        double z = st->zenith / 90.0;
        if (z > 1.0) z = 2.0 - z;
        z = -cos((st->zenith + 90.0) * M_PI / 180.0);
        float hand_len = (float)(z * r);
        float sx = sinf(az_angle) * hand_len;
        float sy = -cosf(az_angle) * hand_len;

        draw_set_color(d, dc(0.2f, 0.2f, 0.2f));
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
