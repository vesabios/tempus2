// view_offic.h — OFFICIVM: the book of hours.
//
// The day as the monastery kept it: eight offices strung around the
// sun's circuit, each anchored to the TEMPORAL hours the church
// inherited from Rome — Prime at sunrise, Terce at the third hour,
// Sext at the sixth (whose slow drift toward midday named "noon"
// after None, the ninth), Vespers at sunset, Compline at nightfall,
// Matins in the deep of night, Lauds at first light. The dial is a
// day wheel, noon at the bottom like every day dial on the
// instrument; the offices ride the season — as the days lengthen,
// the little hours spread apart and the night offices huddle.
//
// Time as something HEARD: each office wears its bell.

#ifndef VIEW_OFFIC_H
#define VIEW_OFFIC_H

#include <stdio.h>
#include "../view.h"

struct OfficViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene offic_blend
};

#endif // VIEW_OFFIC_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_OFFIC_IMPL)
#define VIEW_OFFIC_IMPL

#define OFFIC_R   368.0f   // the day wheel's outer radius
#define OFFIC_W   34.0f    // band width

// The eight offices: name, position rule handled in render, and
// whether it is a MAJOR office (Matins, Lauds, Vespers — the great
// hours of the old cursus) or one of the little hours
static const struct { const char *name; bool major; }
offic__hours[8] = {
    { "MATVTINVM",    true  },   // deep night
    { "LAVDES",       true  },   // first light
    { "PRIMA",        false },   // sunrise
    { "TERTIA",       false },   // third hour
    { "SEXTA",        false },   // sixth hour — midday
    { "NONA",         false },   // ninth hour
    { "VESPERAE",     true  },   // sunset
    { "COMPLETORIVM", false },   // nightfall
};

// Liturgical weekday names: Sunday is the Lord's day, Saturday keeps
// the sabbath, the rest are numbered ferias
static const char *offic__feria[7] = {
    "DOMINICA", "FERIA SECVNDA", "FERIA TERTIA", "FERIA QVARTA",
    "FERIA QVINTA", "FERIA SEXTA", "SABBATO",
};

// ---- The labors of the months ----
// The canonical northern cycle every book of hours engraves over its
// calendar (Tres Riches Heures, Amiens, Chartres): feast, fire, plow,
// bloom, hunt, hay, harvest, threshing, vintage, sowing, acorns,
// slaughter. Emblems in the same stroke-table idiom as the zodiac
// sigils — point count then x,y pairs, unit box y-up, 0 ends.
static const float offic__lab_jan[] = {   // the feast: a goblet
    6, -0.20f,0.30f, -0.16f,0.10f, -0.06f,0, 0.06f,0, 0.16f,0.10f,
       0.20f,0.30f,
    2, -0.20f,0.30f, 0.20f,0.30f,
    2, 0,0, 0,-0.34f,
    2, -0.16f,-0.34f, 0.16f,-0.34f, 0 };
static const float offic__lab_feb[] = {   // warming: flame on hearth
    9, -0.16f,-0.38f, -0.22f,-0.08f, -0.10f,0.10f, -0.14f,0.30f,
       0,0.44f, 0.10f,0.24f, 0.20f,0.08f, 0.16f,-0.14f, 0.12f,-0.38f,
    2, -0.28f,-0.38f, 0.28f,-0.38f, 0 };
static const float offic__lab_mar[] = {   // plowing: the ard
    3, -0.42f,0.10f, 0.10f,0.02f, 0.30f,0.18f,
    2, 0.30f,0.18f, 0.44f,0.34f,
    3, 0.10f,0.02f, 0.02f,-0.24f, -0.10f,-0.36f,
    3, -0.10f,-0.36f, 0.10f,-0.30f, 0.02f,-0.24f,
    7, -0.20f,-0.24f, -0.24f,-0.14f, -0.34f,-0.14f, -0.40f,-0.24f,
       -0.34f,-0.34f, -0.24f,-0.34f, -0.20f,-0.24f, 0 };
static const float offic__lab_apr[] = {   // greening: a bloom
    3, 0,-0.42f, 0.02f,-0.10f, 0,0.10f,
    9, 0,0.10f, -0.14f,0.16f, -0.16f,0.34f, -0.04f,0.26f, 0.02f,0.40f,
       0.10f,0.26f, 0.18f,0.32f, 0.14f,0.14f, 0,0.10f,
    2, 0.01f,-0.14f, 0.20f,-0.26f,
    2, -0.01f,-0.22f, -0.18f,-0.34f, 0 };
static const float offic__lab_may[] = {   // the hunt: a horn
    5, -0.34f,0.10f, -0.24f,-0.14f, -0.04f,-0.28f, 0.18f,-0.24f,
       0.34f,-0.10f,
    5, -0.30f,0.06f, -0.20f,-0.08f, -0.02f,-0.20f, 0.16f,-0.16f,
       0.28f,-0.06f,
    2, -0.34f,0.10f, -0.30f,0.06f,
    2, 0.34f,-0.10f, 0.28f,-0.06f,
    2, -0.34f,0.10f, -0.42f,0.22f, 0 };
static const float offic__lab_jun[] = {   // haymaking: the scythe
    4, -0.30f,-0.44f, -0.10f,0, 0.02f,0.30f, 0,0.44f,
    2, -0.10f,0, -0.24f,0.06f,
    4, 0,0.44f, 0.16f,0.38f, 0.30f,0.24f, 0.38f,0.06f, 0 };
static const float offic__lab_jul[] = {   // harvest: the sickle
    5, -0.05f,0.05f, 0.08f,0.18f, 0.26f,0.20f, 0.40f,0.08f,
       0.44f,-0.08f,
    2, -0.05f,0.05f, -0.16f,-0.10f,
    2, -0.34f,-0.44f, -0.30f,-0.06f,
    2, -0.30f,-0.06f, -0.38f,0.06f,
    2, -0.30f,-0.06f, -0.24f,0.08f,
    2, -0.31f,-0.16f, -0.39f,-0.06f, 0 };
static const float offic__lab_aug[] = {   // threshing: the flail
    2, -0.24f,-0.44f, -0.06f,0.06f,
    2, -0.06f,0.06f, 0.02f,0.14f,
    2, 0.02f,0.14f, 0.32f,0.32f, 0 };
static const float offic__lab_sep[] = {   // vintage: the cluster
    7, 0,0.02f, -0.06f,0.10f, -0.14f,0.06f, -0.14f,-0.02f, -0.06f,-0.06f,
       0.02f,-0.02f, 0,0.02f,
    7, 0.12f,0.06f, 0.06f,0.14f, -0.02f,0.12f, 0,0.04f, 0.08f,0,
       0.14f,0.02f, 0.12f,0.06f,
    7, -0.02f,-0.10f, -0.08f,-0.02f, -0.16f,-0.06f, -0.16f,-0.14f,
       -0.08f,-0.18f, -0.02f,-0.14f, -0.02f,-0.10f,
    7, 0.10f,-0.08f, 0.04f,-0.02f, -0.02f,-0.08f, 0,-0.16f, 0.08f,-0.18f,
       0.12f,-0.14f, 0.10f,-0.08f,
    7, 0.04f,-0.22f, -0.02f,-0.18f, -0.08f,-0.24f, -0.04f,-0.32f,
       0.04f,-0.32f, 0.08f,-0.26f, 0.04f,-0.22f,
    2, 0,0.30f, 0,0.10f,
    4, 0,0.24f, 0.14f,0.32f, 0.10f,0.20f, 0,0.24f, 0 };
static const float offic__lab_oct[] = {   // sowing: pouch and seed
    6, -0.30f,0.10f, -0.26f,-0.12f, -0.12f,-0.22f, 0.02f,-0.14f,
       0.06f,0.06f, -0.30f,0.10f,
    3, -0.28f,0.08f, -0.10f,0.30f, 0.04f,0.06f,
    2, 0.20f,-0.02f, 0.23f,-0.02f,
    2, 0.30f,-0.14f, 0.33f,-0.14f,
    2, 0.22f,-0.26f, 0.25f,-0.26f,
    2, 0.34f,-0.32f, 0.37f,-0.32f,
    2, 0.14f,-0.38f, 0.17f,-0.38f, 0 };
static const float offic__lab_nov[] = {   // pannage: the acorn
    3, -0.16f,0.16f, -0.02f,0.24f, 0.12f,0.16f,
    2, -0.16f,0.16f, 0.12f,0.16f,
    5, -0.14f,0.16f, -0.14f,-0.06f, -0.02f,-0.20f, 0.10f,-0.06f,
       0.10f,0.16f,
    2, -0.02f,0.24f, -0.02f,0.34f,
    2, 0.26f,-0.30f, 0.29f,-0.30f, 0 };
static const float offic__lab_dec[] = {   // slaughter: the axe
    2, -0.30f,-0.44f, 0.14f,0.20f,
    5, 0.06f,0.10f, 0.30f,0.28f, 0.44f,0.06f, 0.20f,-0.06f,
       0.06f,0.10f, 0 };
static const float *offic__labors[12] = {
    offic__lab_jan, offic__lab_feb, offic__lab_mar, offic__lab_apr,
    offic__lab_may, offic__lab_jun, offic__lab_jul, offic__lab_aug,
    offic__lab_sep, offic__lab_oct, offic__lab_nov, offic__lab_dec,
};

static void offic_init(void *buf, const Tempus *t, const RenderStyle *s) {
    OfficViewState *st = (OfficViewState *)buf;
    st->opacity = 1.0;
    (void)t; (void)s;
}

static void offic_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void offic_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void offic_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    OfficViewState *st = (OfficViewState *)buf;
    (void)t; (void)dt;
    st->blend = sc->offic_blend;
}

static void offic_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const OfficViewState *st = (const OfficViewState *)buf;
    const TimeView *tv = &st->tv;
    float base_alpha = d->alpha;

    // Sunrise/sunset as day fractions, guarded for polar edge cases
    float rise = (float)(t->sunrise_mins / 1440.0);
    float set  = (float)(t->sunset_mins / 1440.0);
    if (!(set > rise) || set - rise < 0.02f || set - rise > 0.98f) {
        rise = 0.25f;
        set = 0.75f;
    }
    float dh = (set - rise) / 12.0f;          // temporal day hour
    float nh = (1.0f - (set - rise)) / 12.0f; // temporal night hour
    float fnoon = (rise + set) * 0.5f;
    float fmid = fnoon - 0.5f;
    if (fmid < 0) fmid += 1.0f;
    float now = (float)tv->percent_of_day;

    // The offices' places on the day, in day fractions
    float op[8];
    op[0] = fmid;               // Matins: solar midnight
    op[1] = rise - nh;          // Lauds: the last night hour
    op[2] = rise;               // Prime
    op[3] = rise + 3.0f * dh;   // Terce
    op[4] = rise + 6.0f * dh;   // Sext (= solar noon)
    op[5] = rise + 9.0f * dh;   // None
    op[6] = set;                // Vespers
    op[7] = set + nh;           // Compline: nightfall
    for (int i = 0; i < 8; i++) {
        if (op[i] < 0) op[i] += 1.0f;
        if (op[i] >= 1.0f) op[i] -= 1.0f;
    }

    // Which office governs now: the most recent one begun
    int cur = 0;
    {
        float best = -2.0f;
        for (int i = 0; i < 8; i++) {
            float since = now - op[i];
            if (since < 0) since += 1.0f;
            float score = 1.0f - since;   // most recent = highest
            if (score > best) { best = score; cur = i; }
        }
    }

    // ---- The day wheel ----
    {
        // Night: the plate itself. Day: a parchment-warm wash between
        // the ORTVS and OCCASVS cuts, twilight softened at the edges.
        {
            const int N = 160;
            int pi = -1, po = -1;
            for (int i = 0; i <= N; i++) {
                float fp = (float)i / N;
                // Warmth from proximity inside the daylight span
                float dd = fp - rise;
                if (dd < 0) dd += 1.0f;
                float span = set - rise;
                float wgt = 0.0f;
                if (dd < span) {
                    float e0 = dd / 0.035f;
                    float e1 = (span - dd) / 0.035f;
                    wgt = e0 < 1.0f ? e0 : (e1 < 1.0f ? e1 : 1.0f);
                    if (wgt < 0) wgt = 0;
                }
                DrawColor cc = dca(0.32f + 0.30f * wgt,
                                   0.28f + 0.24f * wgt,
                                   0.20f + 0.16f * wgt,
                                   0.10f + 0.16f * wgt);
                draw_set_color(d, cc);
                float a = fp * 2.0f * (float)M_PI;
                float sx = sinf(a), sy = -cosf(a);
                int vi = draw__push_vert(d, sx * (OFFIC_R - OFFIC_W),
                                         sy * (OFFIC_R - OFFIC_W),
                                         d->white_u, d->white_v);
                int vo = draw__push_vert(d, sx * OFFIC_R, sy * OFFIC_R,
                                         d->white_u, d->white_v);
                if (i > 0) {
                    draw__tri(d, pi, po, vi);
                    draw__tri(d, po, vo, vi);
                }
                pi = vi;
                po = vo;
            }
        }

        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.40f));
        draw_circle_stroked(d, 0, 0, OFFIC_R, 1.0f);
        draw_circle_stroked(d, 0, 0, OFFIC_R - OFFIC_W, 1.0f);

        // The current office's span, washed gold to the next bell
        {
            float a0 = op[cur];
            float a1 = op[(cur + 1) % 8];
            if (a1 <= a0) a1 += 1.0f;
            DrawColor g = dc_scale(s->sunrise_handle, 0.9f);
            g.a = 0.16f;
            draw_set_color(d, g);
            draw_arc_filled(d, 0, 0, OFFIC_R - OFFIC_W + 1.0f,
                            OFFIC_R - 1.0f,
                            a0 * 2.0f * (float)M_PI - (float)M_PI * 0.5f,
                            a1 * 2.0f * (float)M_PI - (float)M_PI * 0.5f,
                            10);
        }

        // The offices: bell-pips on the band, names outside
        for (int i = 0; i < 8; i++) {
            float a = op[i] * 2.0f * (float)M_PI;
            float sx = sinf(a), sy = -cosf(a);
            bool major = offic__hours[i].major;
            bool active = i == cur;

            // Bell: a small trapezoid hanging on the band, its
            // clapper the pip — drawn from primitives, always upright
            {
                float bx = sx * (OFFIC_R - OFFIC_W * 0.5f);
                float by = sy * (OFFIC_R - OFFIC_W * 0.5f);
                float bs = major ? 10.5f : 8.0f;
                DrawColor c = active
                    ? dc_scale(s->sunrise_handle, 1.1f)
                    : dca(0.62f, 0.60f, 0.55f, 0.75f);
                draw_set_color(d, c);
                int vb = d->num_verts;
                draw__push_vert(d, bx - bs * 0.55f, by + bs * 0.5f,
                                d->white_u, d->white_v);
                draw__push_vert(d, bx + bs * 0.55f, by + bs * 0.5f,
                                d->white_u, d->white_v);
                draw__push_vert(d, bx + bs * 0.32f, by - bs * 0.6f,
                                d->white_u, d->white_v);
                draw__push_vert(d, bx - bs * 0.32f, by - bs * 0.6f,
                                d->white_u, d->white_v);
                draw__tri(d, vb, vb + 1, vb + 2);
                draw__tri(d, vb, vb + 2, vb + 3);
                draw_circle_filled(d, bx, by + bs * 0.72f,
                                   bs * 0.22f);
            }

            // Tick through the band
            draw_set_color(d, active
                ? dc_scale(s->sunrise_handle, 1.0f)
                : dca(0.55f, 0.53f, 0.49f, 0.5f));
            draw_line(d, sx * (OFFIC_R - OFFIC_W - 5.0f),
                      sy * (OFFIC_R - OFFIC_W - 5.0f),
                      sx * (OFFIC_R - OFFIC_W), sy * (OFFIC_R - OFFIC_W),
                      active ? 1.6f : 1.0f);

            // Name on its arc outside the band, waist-centered.
            // TWO ROWS: major hours inner, little hours outer — in
            // high summer Lauds crowds Prime and Compline crowds
            // Vespers (the night hours truly are that short), so the
            // rows keep the names legible while the geometry tells
            // the seasonal truth.
            {
                float na = fmodf(a, 2.0f * (float)M_PI);
                if (na < 0) na += 2.0f * (float)M_PI;
                bool lflip = (na > (float)M_PI * 0.5f
                              && na < (float)M_PI * 1.5f);
                float lsz = _font_compat[FONT_date].size
                          * (major ? 1.0f : 0.88f);
                float mid = OFFIC_R + (major ? 18.0f : 42.0f);
                float lr = mid - lsz * 0.5f
                         + lsz * (lflip ? 0.51f : 0.37f);
                draw_set_color(d, active
                    ? dca(0.85f, 0.81f, 0.72f, 0.95f)
                    : dca(0.58f, 0.56f, 0.51f, major ? 0.70f : 0.50f));
                draw_text_curved(d, FONT_date, 0, 0, lr, a,
                                 offic__hours[i].name, 0.9f,
                                 major ? 1.0f : 0.88f);
            }
        }

        // The day hand, hour-hand dress, one revolution per day
        {
            float a = now * 2.0f * (float)M_PI;
            float hdx = sinf(a), hdy = -cosf(a);
            float px2 = -hdy, py2 = hdx;
            float hw = 5.0f;
            float h0 = 46.0f, h1 = OFFIC_R - OFFIC_W - 14.0f;
            draw_set_color(d, s->hours_color);
            int vb = d->num_verts;
            draw__push_vert(d, hdx * h0 - px2 * hw, hdy * h0 - py2 * hw,
                            d->white_u, d->white_v);
            draw__push_vert(d, hdx * h0 + px2 * hw, hdy * h0 + py2 * hw,
                            d->white_u, d->white_v);
            draw__push_vert(d, hdx * h1 + px2 * hw, hdy * h1 + py2 * hw,
                            d->white_u, d->white_v);
            draw__push_vert(d, hdx * h1 - px2 * hw, hdy * h1 - py2 * hw,
                            d->white_u, d->white_v);
            draw__tri(d, vb, vb + 1, vb + 2);
            draw__tri(d, vb, vb + 2, vb + 3);
        }
    }

    // ---- The labors of the months ----
    // Twelve emblems riding just outside the calendar wheel at each
    // month's middle — the book of hours' arch over its calendar. The
    // current month's labor stands forward; the rest wait their turn.
    {
        float lr2 = 450.0f + 55.0f;   // the slot between the wheel's
                                      // tick band and the month
                                      // lettering (~555 out)
        for (int m = 0; m < 12; m++) {
            double midjd = (t->jd_months[m] + t->jd_months[m + 1]) * 0.5;
            float pct = (float)tempus_jd_to_wheel_pct(t, midjd);
            float a = pct * 2.0f * (float)M_PI;
            float ux = sinf(a), uy = -cosf(a);
            bool cur_m = (m == tv->month - 1);
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f,
                                  cur_m ? 0.85f : 0.32f));
            orr__strokes(d, offic__labors[m], ux * lr2, uy * lr2,
                         ux, uy, cur_m ? 36.0f : 30.0f, 1.1f);
        }
    }

    // ---- Center: the reading ----
    {
        draw_set_color(d, dca(0.80f, 0.77f, 0.70f, 0.95f));
        draw_text_centered(d, FONT_month, 0, -14.0f,
                           offic__hours[cur].name);

        int wd = (int)(((long)floor(tv->jd_current + 1.5)) % 7);
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, 22.0f, offic__feria[wd]);
    }

    d->alpha = base_alpha;
}

static const ViewVtable offic_vtable = {
    .init   = offic_init,
    .enter  = offic_enter,
    .exit   = offic_exit,
    .update = offic_update,
    .render = offic_render,
};

#endif // SCENE_DEFINED && !VIEW_OFFIC_IMPL
