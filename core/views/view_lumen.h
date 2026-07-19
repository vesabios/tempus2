// view_lumen.h — LVMEN: the sun and the moon, drawn once.
//
// The ONE OBJECT law, made structural. The orrery composes the
// luminaries' parameters across every station — dial marker, TELLVS
// bead, MACHINA throne, ORBIS hang, and the CAELVM chart targets the
// sky view publishes — as one continuous blend per body. This view is
// their single renderer, layered above both the machine and the sky,
// so there is no ownership handoff, no second copy, and nothing to
// crossfade, ever. If a discontinuity exists it is a bug in the
// composition, not a seam between renderers — seams no longer exist.

#ifndef VIEW_LUMEN_H
#define VIEW_LUMEN_H

#include "../view.h"

struct LumenViewState {
    TimeView tv;  // must be first field
    double opacity;
    const OrreryViewState *orr;   // the composer's published parameters
};

#endif // VIEW_LUMEN_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_LUMEN_IMPL)
#define VIEW_LUMEN_IMPL

static void lumen_init(void *buf, const Tempus *t, const RenderStyle *s) {
    LumenViewState *st = (LumenViewState *)buf;
    st->opacity = 1.0;
    (void)t; (void)s;
}

static void lumen_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void lumen_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void lumen_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    LumenViewState *st = (LumenViewState *)buf;
    (void)t; (void)dt;
    st->orr = &sc->orrery_state;
}

static void lumen_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const LumenViewState *st = (const LumenViewState *)buf;
    const OrreryViewState *o = st->orr;
    float base_alpha = d->alpha;
    (void)t; (void)s;
    if (!o) return;

    // ---- The planets (Stage 3: one renderer for every body) ----
    // Composed member rows from the orrery — machine ring seat x sky
    // chart target. Drawn under the luminaries, above every surface.
    // Rows carry their ABSOLUTE alpha (each side's own fade law is
    // composed in) — this view's opacity is the luminaries' law, not
    // the planets'.
    for (int p = 0; p < PL_COUNT; p++) {
        if (o->pl_ca[p] <= 0.003f) continue;
        d->alpha = o->pl_ca[p];
        draw_set_color(d, dca(o->pl_col[p][0], o->pl_col[p][1],
                              o->pl_col[p][2], o->pl_col[p][3]));
        if (o->pl_stroke[p])
            draw_circle_stroked(d, o->pl_cx[p], o->pl_cy[p],
                                o->pl_cr[p], 1.2f);
        else
            draw_circle_filled(d, o->pl_cx[p], o->pl_cy[p],
                               o->pl_cr[p]);
        // Saturn's engraved ring is machine regalia — it rides the
        // machine side of the row and dissolves toward the chart
        if (o->pl_ring_a[p] > 0.003f) {
            d->alpha = o->pl_ring_a[p];
            draw_set_color(d, dca(o->pl_col[p][0], o->pl_col[p][1],
                                  o->pl_col[p][2], 0.45f));
            draw_circle_stroked(d, o->pl_cx[p], o->pl_cy[p],
                                o->pl_cr[p] * 1.75f, 1.0f);
        }
    }
    d->alpha = base_alpha;

    // ---- The sun ----
    {
        float px = o->lum_sun_x, py = o->lum_sun_y;
        float sz = o->lum_sun_r;
        draw_set_color(d, dca(o->lum_sun_col[0], o->lum_sun_col[1],
                              o->lum_sun_col[2], 1.0f));
        draw_circle_filled(d, px, py, sz);

        // The corona ring and the sun in splendour — the historical
        // triangular rays, alternating long and short
        if (o->lum_sun_ray > 0.001f) {
            d->alpha = base_alpha * o->lum_sun_ray;
            draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.35f));
            draw_circle_stroked(d, px, py, sz * 28.0f / 22.0f, 1.0f);

            draw_set_color(d, dca(0.72f, 0.46f, 0.08f, 0.75f));
            for (int i = 0; i < 12; i++) {
                float a = (float)i / 12.0f * 2.0f * (float)M_PI;
                float ux = sinf(a), uy = -cosf(a);
                float qx = cosf(a), qy = sinf(a);   // base tangent
                float rb = sz * 1.32f;
                float rt = sz * ((i & 1) ? 1.52f : 1.78f);
                float half = sz * 0.13f;
                int vb = d->num_verts;
                draw__push_vert(d, px + ux * rt, py + uy * rt,
                                d->white_u, d->white_v);
                draw__push_vert(d, px + ux * rb - qx * half,
                                py + uy * rb - qy * half,
                                d->white_u, d->white_v);
                draw__push_vert(d, px + ux * rb + qx * half,
                                py + uy * rb + qy * half,
                                d->white_u, d->white_v);
                draw__tri(d, vb, vb + 1, vb + 2);
            }
            d->alpha = base_alpha;
        }
    }

    // ---- The moon ----
    {
        d->alpha = base_alpha * o->lum_moon_a;
        GlobeCmd *gm = draw_globe_slot(d, o->lum_moon_x, o->lum_moon_y,
                                       o->lum_moon_r);
        d->alpha = base_alpha;
        if (gm) {
            memcpy(gm->rot, o->lum_moon_rot, sizeof(gm->rot));
            memcpy(gm->light, o->lum_moon_light, sizeof(gm->light));
            memcpy(gm->aux_dir, o->lum_moon_aux, sizeof(gm->aux_dir));
            gm->land = true;      // sample the lunar albedo
            gm->tex_id = 1;
            gm->grid_boost = 0.0f;
            gm->obs_lat = 999.0f;
            gm->day_col[0] = 0.58f; gm->day_col[1] = 0.55f;
            gm->day_col[2] = 0.49f; gm->day_col[3] = 1.0f;
            // Dark indigo night side — the full disc stays legible, and
            // the albedo ghosting through reads as earthshine. (A
            // phase-dependent earthshine lift was tried here and
            // reverted: it brightened every station's moon, which were
            // already tuned — DRACO alone carries the ashen light.)
            gm->night_col[0] = 0.10f; gm->night_col[1] = 0.105f;
            gm->night_col[2] = 0.23f; gm->night_col[3] = 1.0f;
        }
    }

    d->alpha = base_alpha;
}

static const ViewVtable lumen_vtable = {
    .init   = lumen_init,
    .enter  = lumen_enter,
    .exit   = lumen_exit,
    .update = lumen_update,
    .render = lumen_render,
};

#endif // SCENE_DEFINED && !VIEW_LUMEN_IMPL
