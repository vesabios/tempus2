// view_orbis.h — ORBIS TERRARVM: the circle of the lands.
//
// The one station that configures PLACE instead of showing time: an
// azimuthal-equidistant chart of the whole earth, centered on the
// observer. Your location sits under the fixed reticle at map center by
// construction; the rim is your antipode — like CAELVM's sky bowl,
// nothing is ever off the chart. Drag the map and the world turns under
// the reticle; the configured location follows LIVE, so the terminator,
// the sun paths, the sky — the whole instrument — answer immediately.
//
// This deliberateness is the design: location changes only here.
// Nothing at any other station can move your home.

#ifndef VIEW_ORBIS_H
#define VIEW_ORBIS_H

#include <stdio.h>
#include "../view.h"
#include "../../assets/coastlines.h"

struct OrbisViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene orbis_blend

    bool  dragging;   // panning the world under the reticle
    float last_wx, last_wy;

    // Coastline unit vectors (earth frame), precomputed at init
    float coast_vec[COAST_NUM_PTS][3];
};

#endif // VIEW_ORBIS_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_ORBIS_IMPL)
#define VIEW_ORBIS_IMPL

// Map disc radius: center = the observer, rim = the antipode (180 deg).
// Kept inside the calendar wheel's grab band so both stay draggable.
#define ORBIS_R 385.0f

// Observer basis at the map center: c points at the observer, n along
// local north, e along local east. Projection of any earth-frame unit
// vector v: angular distance from c maps to radius (equidistant), the
// azimuth about c maps to screen bearing, north up.
typedef struct {
    float c[3], n[3], e[3];
} OrbisFrame;

static inline void orbis__frame(double lat_deg, double lon_deg,
                                OrbisFrame *f) {
    double la = lat_deg * M_PI / 180.0, lo = lon_deg * M_PI / 180.0;
    f->c[0] = (float)(cos(la) * cos(lo));
    f->c[1] = (float)(cos(la) * sin(lo));
    f->c[2] = (float)(sin(la));
    f->n[0] = (float)(-sin(la) * cos(lo));
    f->n[1] = (float)(-sin(la) * sin(lo));
    f->n[2] = (float)(cos(la));
    f->e[0] = (float)(-sin(lo));
    f->e[1] = (float)(cos(lo));
    f->e[2] = 0.0f;
}

static inline void orbis__unit(double lat_deg, double lon_deg, float v[3]) {
    double la = lat_deg * M_PI / 180.0, lo = lon_deg * M_PI / 180.0;
    v[0] = (float)(cos(la) * cos(lo));
    v[1] = (float)(cos(la) * sin(lo));
    v[2] = (float)(sin(la));
}

// Project an earth-frame unit vector. Screen y is downward-positive,
// so north renders up as -y.
static inline void orbis__project(const OrbisFrame *f, const float v[3],
                                  float *x, float *y) {
    float d = v[0] * f->c[0] + v[1] * f->c[1] + v[2] * f->c[2];
    if (d > 1.0f) d = 1.0f;
    if (d < -1.0f) d = -1.0f;
    float en = v[0] * f->e[0] + v[1] * f->e[1] + v[2] * f->e[2];
    float nn = v[0] * f->n[0] + v[1] * f->n[1] + v[2] * f->n[2];
    float len = sqrtf(en * en + nn * nn);
    float rr = acosf(d) * (1.0f / (float)M_PI) * ORBIS_R;
    if (len < 1.0e-6f) { *x = 0.0f; *y = -rr; return; }
    *x = en / len * rr;
    *y = -nn / len * rr;
}

// A geodesic step near the antipode can lash across the whole chart as
// its azimuth swings — draw only segments that stay short on screen.
static inline bool orbis__seg_ok(float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0;
    return dx * dx + dy * dy < (0.35f * ORBIS_R) * (0.35f * ORBIS_R);
}

static void orbis_init(void *buf, const Tempus *t, const RenderStyle *s) {
    OrbisViewState *st = (OrbisViewState *)buf;
    st->opacity = 1.0;
    for (int i = 0; i < COAST_NUM_PTS; i++) {
        orbis__unit(coast_pts[i][0] * 0.01, coast_pts[i][1] * 0.01,
                    st->coast_vec[i]);
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
    (void)t; (void)dt;
    st->blend = sc->orbis_blend;
}

static void orbis_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const OrbisViewState *st = (const OrbisViewState *)buf;
    float base_alpha = d->alpha;

    OrbisFrame f;
    orbis__frame(t->config.latitude, t->config.longitude, &f);

    // Subsolar point straight from SPA: the sun is at zenith where its
    // hour angle is zero, so lon = RA - Greenwich sidereal time.
    float sun[3];
    {
        double slon = t->solar.alpha - t->solar.nu;
        while (slon > 180.0) slon -= 360.0;
        while (slon < -180.0) slon += 360.0;
        orbis__unit(t->solar.delta, slon, sun);
    }

    // ---- Graticule: parallels and meridians every 15 deg ----
    // Off-center azimuthal-equidistant bends the grid into the chart's
    // signature curves. Equator and prime meridian read slightly firmer.
    for (int la = -75; la <= 75; la += 15) {
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, la == 0 ? 0.14f : 0.06f));
        float px = 0, py = 0;
        bool has = false;
        for (int lo = -180; lo <= 180; lo += 6) {
            float v[3], x, y;
            orbis__unit(la, lo, v);
            orbis__project(&f, v, &x, &y);
            if (has && orbis__seg_ok(px, py, x, y))
                draw_line(d, px, py, x, y, 1.0f);
            px = x; py = y; has = true;
        }
    }
    for (int lo = -180; lo < 180; lo += 15) {
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, lo == 0 ? 0.12f : 0.06f));
        float px = 0, py = 0;
        bool has = false;
        for (int la = -87; la <= 87; la += 6) {
            float v[3], x, y;
            orbis__unit(la, lo, v);
            orbis__project(&f, v, &x, &y);
            if (has && orbis__seg_ok(px, py, x, y))
                draw_line(d, px, py, x, y, 1.0f);
            px = x; py = y; has = true;
        }
    }

    // ---- Coastlines, lit by the actual day ----
    // Day side in the engraved coast tone, night side sunk toward the
    // plate — the terminator crosses the chart as a change of light.
    for (int li = 0; li < COAST_NUM_LINES; li++) {
        int start = coast_lines[li][0], count = coast_lines[li][1];
        float px = 0, py = 0;
        bool has = false;
        float pdot = 0;
        // Stride-2 decimation: 110m data is denser than the chart can
        // show, and the draw budget is shared with the whole instrument
        for (int k = start; k < start + count + 1; k += 2) {
            int i = k < start + count ? k : start + count - 1;
            const float *v = st->coast_vec[i];
            float x, y;
            orbis__project(&f, v, &x, &y);
            float sd = v[0] * sun[0] + v[1] * sun[1] + v[2] * sun[2];
            if (has && orbis__seg_ok(px, py, x, y)) {
                bool day = (sd + pdot) * 0.5f > 0.0f;
                draw_set_color(d, dca(0.63f, 0.58f, 0.50f,
                                      day ? 0.55f : 0.22f));
                draw_line(d, px, py, x, y, 1.0f);
            }
            px = x; py = y; pdot = sd; has = true;
        }
    }

    // ---- The terminator: the great circle where the sun sets ----
    {
        // Basis of the plane perpendicular to the sun direction
        float u[3], w[3];
        if (fabsf(sun[2]) < 0.95f) {
            u[0] = -sun[1]; u[1] = sun[0]; u[2] = 0.0f;
        } else {
            u[0] = 1.0f; u[1] = 0.0f; u[2] = 0.0f;
        }
        float ul = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        u[0] /= ul; u[1] /= ul; u[2] /= ul;
        w[0] = sun[1] * u[2] - sun[2] * u[1];
        w[1] = sun[2] * u[0] - sun[0] * u[2];
        w[2] = sun[0] * u[1] - sun[1] * u[0];

        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.28f));
        float px = 0, py = 0;
        bool has = false;
        for (int i = 0; i <= 120; i++) {
            float a = (float)i / 120.0f * 2.0f * (float)M_PI;
            float ca = cosf(a), sa = sinf(a);
            float v[3] = { u[0] * ca + w[0] * sa,
                           u[1] * ca + w[1] * sa,
                           u[2] * ca + w[2] * sa };
            float x, y;
            orbis__project(&f, v, &x, &y);
            if (has && orbis__seg_ok(px, py, x, y))
                draw_line(d, px, py, x, y, 1.0f);
            px = x; py = y; has = true;
        }

        // The subsolar point: where it is noon straight overhead
        float sx, sy;
        orbis__project(&f, sun, &sx, &sy);
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.85f));
        draw_circle_filled(d, sx, sy, 4.5f);
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.35f));
        draw_circle_stroked(d, sx, sy, 9.0f, 1.0f);
    }

    // ---- Rim: the antipode, the far edge of the world ----
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.30f));
    draw_circle_stroked(d, 0, 0, ORBIS_R, 1.0f);
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.10f));
    draw_circle_stroked(d, 0, 0, ORBIS_R + 5.0f, 1.0f);

    // ---- The reticle: you are here, always, at center ----
    {
        draw_set_color(d, dc_scale(s->sunrise_handle, 1.1f));
        draw_circle_stroked(d, 0, 0, 12.0f, 1.5f);
        for (int i = 0; i < 4; i++) {
            float a = (float)i * 0.5f * (float)M_PI;
            float cx = cosf(a), cy = sinf(a);
            draw_line(d, cx * 16.0f, cy * 16.0f, cx * 26.0f, cy * 26.0f,
                      1.5f);
        }
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.9f));
        draw_circle_filled(d, 0, 0, 2.0f);
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
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, 84.0f, "LOCVS OBSERVATORIS");
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
