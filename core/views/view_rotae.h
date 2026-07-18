// view_rotae.h — ROTAE: the wheels.
//
// A clock's gear train exploded into one readable diagram: every nested
// cycle of civil time as a concentric ring turning at its true rate.
// Seconds innermost — the only ring you can watch move — then minutes,
// hours, the week, the month; the calendar wheel outside is the year,
// completing the train. Each ring is 60, 60, 24, 7, N teeth of the next
// one in. Midnight/zero at the top, clockwise, one gold bead per ring:
// at a glance you see the second blur, the hour creep, and the month
// stand nearly still — the ratios of time made visible.

#ifndef VIEW_ROTAE_H
#define VIEW_ROTAE_H

#include "../view.h"

struct RotaeViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene rotae_blend
};

#endif // VIEW_ROTAE_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_ROTAE_IMPL)
#define VIEW_ROTAE_IMPL

static void rotae_init(void *buf, const Tempus *t, const RenderStyle *s) {
    RotaeViewState *st = (RotaeViewState *)buf;
    st->opacity = 1.0;
    (void)t; (void)s;
}

static void rotae_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void rotae_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void rotae_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    RotaeViewState *st = (RotaeViewState *)buf;
    (void)t; (void)dt;
    st->blend = sc->rotae_blend;
}

// One ring: graduations, label, and the bead at its current phase
static void rotae__ring(DrawCtx *d, const RenderStyle *s, float r,
                        int divisions, int major_every, float phase,
                        const char *label) {
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.30f));
    draw_circle_stroked(d, 0, 0, r, 1.0f);

    for (int i = 0; i < divisions; i++) {
        float a = (float)i / divisions * 2.0f * (float)M_PI;
        float sx = sinf(a), sy = -cosf(a);
        bool major = major_every > 0 && (i % major_every) == 0;
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, major ? 0.55f : 0.26f));
        float t0 = r - (major ? 9.0f : 5.0f);
        draw_line(d, sx * t0, sy * t0, sx * r, sy * r, 1.0f);
    }

    // The bead of now + a short hand to catch the eye at speed
    {
        float a = phase * 2.0f * (float)M_PI;
        float sx = sinf(a), sy = -cosf(a);
        draw_set_color(d, dc_scale(s->sunrise_handle, 0.85f));
        draw_line(d, sx * (r - 20.0f), sy * (r - 20.0f),
                  sx * (r - 5.0f), sy * (r - 5.0f), 1.3f);
        draw_circle_filled(d, sx * r, sy * r, 4.5f);
    }

    // Label engraved just outside the ring at the bottom
    draw_set_color(d, dca(0.58f, 0.56f, 0.51f, 0.60f));
    draw_text_curved(d, FONT_date, 0, 0, r + 6.0f, (float)M_PI,
                     label, 0.8f, 0.85f);
}

static void rotae_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const RotaeViewState *st = (const RotaeViewState *)buf;
    const TimeView *tv = &st->tv;
    float base_alpha = d->alpha;

    // Phases of the nested cycles (0 at the top of each)
    float sec_ph  = (float)((tv->secs + tv->frac_secs) / 60.0);
    float min_ph  = (float)((tv->mins + (tv->secs + tv->frac_secs) / 60.0)
                            / 60.0);
    float hour_ph = (float)tv->percent_of_day;
    int w = (int)(((long)floor(tv->jd_current + 1.5)) % 7);
    float week_ph = (float)((w + tv->percent_of_day) / 7.0);
    int dim = t->days_in_month[tv->month - 1];
    float mon_ph  = (float)((tv->day - 1 + tv->percent_of_day) / dim);

    // Minuta secunda, minuta prima: the medieval names are the true
    // ones — the "second" is the second small division of the hour
    rotae__ring(d, s, 100.0f, 60, 5, sec_ph,  "SECVNDA");
    rotae__ring(d, s, 165.0f, 60, 5, min_ph,  "PRIMA");
    rotae__ring(d, s, 230.0f, 24, 6, hour_ph, "HORA");
    rotae__ring(d, s, 295.0f, 7,  1, week_ph, "HEBDOMAS");
    rotae__ring(d, s, 360.0f, dim, 5, mon_ph, "MENSIS");
    // (the calendar wheel outside is ANNVS, completing the train)

    // Hour numerals on the 24 ring
    {
        int hw = _font_compat[FONT_seconds].weight;
        static const char *hn[4] = { "0", "6", "12", "18" };
        for (int i = 0; i < 4; i++) {
            float a = (float)i / 4.0f * 2.0f * (float)M_PI;
            float sx = sinf(a), sy = -cosf(a);
            float tw2 = sdf_measure_width(hw, hn[i]) * 13.0f;
            draw_set_color(d, dca(0.60f, 0.58f, 0.53f, 0.60f));
            draw_text_ex(d, hw, 13.0f, sx * 214.0f - tw2 * 0.5f,
                         sy * 214.0f - 6.5f, hn[i]);
        }
    }

    // Weekday initials on the week ring (dies Solis..Saturni), today lit
    {
        static const char *wi[7] = { "S", "L", "M", "M", "I", "V", "S" };
        int ww = _font_compat[FONT_seconds].weight;
        for (int i = 0; i < 7; i++) {
            float a = (float)(i + 0.5f) / 7.0f * 2.0f * (float)M_PI;
            float sx = sinf(a), sy = -cosf(a);
            float tw2 = sdf_measure_width(ww, wi[i]) * 14.0f;
            draw_set_color(d, i == w
                ? dca(0.75f, 0.72f, 0.65f, 0.9f)
                : dca(0.50f, 0.49f, 0.46f, 0.35f));
            draw_text_ex(d, ww, 14.0f, sx * 277.0f - tw2 * 0.5f,
                         sy * 277.0f - 7.0f, wi[i]);
        }
    }

    // Day-of-month numerals at the fives
    {
        int mw = _font_compat[FONT_seconds].weight;
        for (int day = 5; day <= dim; day += 5) {
            float a = (float)(day - 1) / dim * 2.0f * (float)M_PI;
            float sx = sinf(a), sy = -cosf(a);
            char db[3];
            db[0] = (char)('0' + day / 10);
            db[1] = (char)('0' + day % 10);
            db[2] = 0;
            float tw2 = sdf_measure_width(mw, db) * 12.0f;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.45f));
            draw_text_ex(d, mw, 12.0f, sx * 342.0f - tw2 * 0.5f,
                         sy * 342.0f - 6.0f, db);
        }
    }

    // The still center the train turns around
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.5f));
    draw_circle_filled(d, 0, 0, 2.5f);

    d->alpha = base_alpha;
}

static const ViewVtable rotae_vtable = {
    .init   = rotae_init,
    .enter  = rotae_enter,
    .exit   = rotae_exit,
    .update = rotae_update,
    .render = rotae_render,
};

#endif // SCENE_DEFINED && !VIEW_ROTAE_IMPL
