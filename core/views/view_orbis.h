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

struct OrbisViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene orbis_blend

    bool  dragging;   // turning the globe under the reticle
    float last_wx, last_wy;
};

#endif // VIEW_ORBIS_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_ORBIS_IMPL)
#define VIEW_ORBIS_IMPL

static void orbis_init(void *buf, const Tempus *t, const RenderStyle *s) {
    OrbisViewState *st = (OrbisViewState *)buf;
    st->opacity = 1.0;
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
    (void)st;

    // ---- The reticle: you are here, always, at center ----
    // The globe underneath is oriented observer-face-on, so the point
    // under these crosshairs IS the configured location.
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
