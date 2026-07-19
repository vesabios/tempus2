// view_orbis.h — ORBIS TERRARVM: the earth up close, and where you are
// on it.
//
// The one station that configures PLACE instead of showing time. The
// globe itself is the picker — the SAME globe object the orrery owns at
// every station, flown to center and grown to picking size, observer
// face-on (see the ORBIS block in view_orrery.h). Orthographic
// magnification puts the landmass you are hunting largest exactly where
// you are picking. Drag the globe and the world turns under the fixed
// reticle; the configured location follows LIVE, so the terminator, the
// day-length ring, the sun paths — the whole instrument — answer
// immediately.
//
// This view draws only the picker chrome: the center reticle and the
// coordinate readout. Deliberateness is the design: location changes
// only here. Nothing at any other station can move your home.

#ifndef VIEW_ORBIS_H
#define VIEW_ORBIS_H

#include <stdio.h>
#include "../view.h"
#include "../../assets/cities.h"

struct OrbisViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene orbis_blend

    bool  dragging;   // turning the globe under the reticle
    float last_wx, last_wy;

    // The orrery's state — its published globe (position, radius, live
    // rotation) is read at render time so the pips ride the same object
    // with zero lag, immune to update pacing
    const OrreryViewState *orr;

    // Nearest city to the configured location (update-side scan)
    int    near_city;
    double near_km;

    // City unit vectors (earth frame), precomputed at init
    float city_vec[CITY_NUM][3];
};

#endif // VIEW_ORBIS_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_ORBIS_IMPL)
#define VIEW_ORBIS_IMPL

static void orbis_init(void *buf, const Tempus *t, const RenderStyle *s) {
    OrbisViewState *st = (OrbisViewState *)buf;
    st->opacity = 1.0;
    st->near_city = -1;
    for (int i = 0; i < CITY_NUM; i++) {
        double la = city_pts[i].lat * 0.01 * M_PI / 180.0;
        double lo = city_pts[i].lon * 0.01 * M_PI / 180.0;
        st->city_vec[i][0] = (float)(cos(la) * cos(lo));
        st->city_vec[i][1] = (float)(cos(la) * sin(lo));
        st->city_vec[i][2] = (float)(sin(la));
    }
    (void)t; (void)s;
}

static void orbis_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void orbis_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void orbis_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    OrbisViewState *st = (OrbisViewState *)buf;
    (void)dt;
    st->blend = sc->orbis_blend;
    st->orr = &sc->orrery_state;

    // Nearest city: max dot product against the observer's unit vector.
    // Ties in identical spots resolve to the larger city (the table is
    // population-descending).
    if (st->blend > 0.001) {
        double la = t->config.latitude * M_PI / 180.0;
        double lo = t->config.longitude * M_PI / 180.0;
        float o0 = (float)(cos(la) * cos(lo));
        float o1 = (float)(cos(la) * sin(lo));
        float o2 = (float)(sin(la));
        int best = 0;
        float bd = -2.0f;
        for (int i = 0; i < CITY_NUM; i++) {
            const float *v = st->city_vec[i];
            float dd = v[0] * o0 + v[1] * o1 + v[2] * o2;
            if (dd > bd) { bd = dd; best = i; }
        }
        if (bd > 1.0f) bd = 1.0f;
        st->near_city = best;
        st->near_km = acos((double)bd) * 6371.0;
    }
}

static void orbis_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const OrbisViewState *st = (const OrbisViewState *)buf;
    float base_alpha = d->alpha;

    // ---- City pips: the aiming marks ----
    // Every charted city on the front hemisphere, riding the live globe;
    // the nearest one is ringed. Aim, then fine-drag it under the reticle.
    if (st->orr && st->orr->glob_r > 1.0f) {
        float gx = st->orr->glob_x, gy = st->orr->glob_y;
        float gr = st->orr->glob_r;
        const float *grot = st->orr->glob_rot;
        draw_set_color(d, dca(0.80f, 0.76f, 0.68f, 0.35f));
        for (int i = 0; i < CITY_NUM; i++) {
            float v[3];
            globe_mat_mul_vec(grot, st->city_vec[i], v);
            if (v[2] < 0.05f) continue;
            float x = gx + v[0] * gr;
            float y = gy + v[1] * gr;
            draw_rect_filled(d, x - 1.2f, y - 1.2f, x + 1.2f, y + 1.2f);
        }
        if (st->near_city >= 0) {
            float v[3];
            globe_mat_mul_vec(grot, st->city_vec[st->near_city], v);
            if (v[2] > 0.05f) {
                draw_set_color(d, dc_scale(s->sunrise_handle, 1.1f));
                draw_circle_stroked(d, gx + v[0] * gr,
                                    gy + v[1] * gr, 6.5f, 1.2f);
            }
        }
    }

    // ---- The reticle: you are here, always, at center ----
    // The globe underneath is oriented observer-face-on, so the point
    // under these crosshairs IS the configured location. It rides the
    // LIVE globe center, so mid-flight it stays on the earth instead
    // of floating at the station's origin waiting for it.
    {
        float rx = 0.0f, ry = 0.0f;
        if (st->orr && st->orr->glob_r > 1.0f) {
            rx = st->orr->glob_x;
            ry = st->orr->glob_y;
        }
        draw_set_color(d, dc_scale(s->sunrise_handle, 1.1f));
        draw_circle_stroked(d, rx, ry, 12.0f, 1.5f);
        for (int i = 0; i < 4; i++) {
            float a = (float)i * 0.5f * (float)M_PI;
            float cx = cosf(a), cy = sinf(a);
            draw_line(d, rx + cx * 16.0f, ry + cy * 16.0f,
                      rx + cx * 26.0f, ry + cy * 26.0f, 1.5f);
        }
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.9f));
        draw_circle_filled(d, rx, ry, 2.0f);

        // ---- The horizon calendar at the home point ----
        // The eight days' sunrise/sunset bearings radiating from
        // where you stand — the neolithic instrument, live: drag the
        // globe north and the solstice fan flings apart, because at
        // 60N midsummer rises nearly northeast and midwinter nearly
        // southeast. The SPREAD of the fan is your latitude. Above
        // the arctic circle the solstice rays vanish — midsummer
        // never rises there, and the instrument says so by silence.
        {
            int cur = 0;
            double bd = 1.0e9;
            for (int i = 0; i < 8; i++) {
                double dd = fabs(st->tv.jd_current - t->jd_events[i]);
                if (dd < bd) { bd = dd; cur = i; }
            }
            for (int ev = 0; ev < 8; ev++) {
                double dec = sunset__sun_declination(
                    sunset__time_julian_cent(t->jd_events[ev]));
                float azr;
                if (!tempus_rise_azimuth(dec, t->config.latitude, &azr))
                    continue;
                bool sol = (ev == 3 || ev == 7);   // Litha, Yule
                float em = (ev == cur) ? 0.9f : (sol ? 0.55f : 0.32f);
                float r1 = sol ? 64.0f : 52.0f;
                for (int side = 0; side < 2; side++) {
                    float az = side ? 360.0f - azr : azr;
                    // Observer-face-on globe: north up, east right
                    float ux = sinf(az * (float)M_PI / 180.0f);
                    float uy = -cosf(az * (float)M_PI / 180.0f);
                    d->alpha = base_alpha * em;
                    draw_set_color(d, dca(0.72f, 0.55f, 0.25f, 0.85f));
                    draw_line(d, rx + ux * 34.0f, ry + uy * 34.0f,
                              rx + ux * r1, ry + uy * r1,
                              sol ? 1.5f : 1.0f);
                }
            }
            d->alpha = base_alpha;
        }
    }

    // ---- Readout: the configured coordinates, live under the drag ----
    {
        char loc[48];
        double la = t->config.latitude, lo = t->config.longitude;
        snprintf(loc, sizeof(loc), "%.2f%s   %.2f%s",
                 fabs(la), la >= 0 ? " N" : " S",
                 fabs(lo), lo >= 0 ? " E" : " W");
        draw_set_color(d, dca(0.78f, 0.75f, 0.68f, 0.95f));
        draw_text_centered(d, FONT_month, 0, 52.0f, loc);

        // The nearest charted city: just the name when you are on it,
        // name and distance when you are aiming from afar
        if (st->near_city >= 0) {
            char cb[64];
            if (st->near_km < 20.0)
                snprintf(cb, sizeof(cb), "%s",
                         city_pts[st->near_city].name);
            else
                snprintf(cb, sizeof(cb), "%s  %d KM",
                         city_pts[st->near_city].name,
                         (int)(st->near_km + 0.5));
            draw_set_color(d, dca(0.68f, 0.65f, 0.58f, 0.85f));
            draw_text_centered(d, FONT_date, 0, 84.0f, cb);
        }
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.45f));
        draw_text_centered(d, FONT_date, 0, 112.0f, "LOCVS OBSERVATORIS");
    }

    d->alpha = base_alpha;
}

static const ViewVtable orbis_vtable = {
    .init   = orbis_init,
    .enter  = orbis_enter,
    .exit   = orbis_exit,
    .update = orbis_update,
    .render = orbis_render,
};

#endif // SCENE_DEFINED && !VIEW_ORBIS_IMPL
