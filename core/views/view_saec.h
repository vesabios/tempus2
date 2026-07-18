// view_saec.h — SAECVLVM: the years of a life.
//
// The one station that points the other way on the time axis: out past
// the year. A saeculum was Rome's word for both a century and a human
// span — the time from a moment until the last person who lived it has
// died — and the dial honors both readings. Without a birth date it is
// the current century: one cell per year, the spent ones engraved
// solid, the current one filling as the calendar wheel turns, the rest
// hairline. With birth_year/month/day configured it becomes the ninety
// years of a life, decades counted in years of age.
//
// Every other station shows a cycle that returns. This one doesn't.
// Tempus fugit; the clocks always said so.

#ifndef VIEW_SAEC_H
#define VIEW_SAEC_H

#include <stdio.h>
#include "../view.h"

struct SaecViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene saec_blend
};

#endif // VIEW_SAEC_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_SAEC_IMPL)
#define VIEW_SAEC_IMPL

#define SAEC_R0 300.0f
#define SAEC_R1 408.0f

// Roman numerals up to 3999 — the only honest way to write a year on
// this instrument
static void saec__roman(int n, char *out, size_t cap) {
    static const int         val[] = { 1000, 900, 500, 400, 100, 90,
                                       50, 40, 10, 9, 5, 4, 1 };
    static const char *const sym[] = { "M", "CM", "D", "CD", "C", "XC",
                                       "L", "XL", "X", "IX", "V", "IV",
                                       "I" };
    size_t p = 0;
    if (n <= 0 || n > 3999) { if (cap) out[0] = 0; return; }
    for (int i = 0; i < 13; i++) {
        while (n >= val[i]) {
            for (const char *c = sym[i]; *c && p + 1 < cap; c++)
                out[p++] = *c;
            n -= val[i];
        }
    }
    out[p] = 0;
}

static void saec_init(void *buf, const Tempus *t, const RenderStyle *s) {
    SaecViewState *st = (SaecViewState *)buf;
    st->opacity = 1.0;
    (void)t; (void)s;
}

static void saec_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void saec_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void saec_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    SaecViewState *st = (SaecViewState *)buf;
    (void)t; (void)dt;
    st->blend = sc->saec_blend;
}

static void saec_render(const void *buf, DrawCtx *d, const Tempus *t,
                        const RenderStyle *s) {
    const SaecViewState *st = (const SaecViewState *)buf;
    const TimeView *tv = &st->tv;
    float base_alpha = d->alpha;

    bool vita = t->config.birth_year > 0;
    int y0 = vita ? t->config.birth_year : tv->year - (tv->year % 100);
    int n  = vita ? 90 : 100;

    float gap = 0.0012f;   // angular breath between year cells

    for (int i = 0; i < n; i++) {
        int y = y0 + i;
        float p0 = (float)i / n + gap;
        float p1 = (float)(i + 1) / n - gap;
        float a0 = p0 * 2.0f * (float)M_PI - (float)M_PI * 0.5f;
        float a1 = p1 * 2.0f * (float)M_PI - (float)M_PI * 0.5f;

        if (y < tv->year) {
            // A spent year: engraved solid
            draw_set_color(d, dca(0.55f, 0.52f, 0.46f, 0.30f));
            draw_arc_filled(d, 0, 0, SAEC_R0, SAEC_R1, a0, a1, 4);
        } else if (y == tv->year) {
            // The current year, filling radially as the wheel turns
            float fill = (float)tv->year_pct;
            draw_set_color(d, dc_scale(s->sunrise_handle, 1.0f));
            draw_arc_filled(d, 0, 0, SAEC_R0,
                            SAEC_R0 + (SAEC_R1 - SAEC_R0) * fill, a0, a1, 4);
            draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.5f));
            draw_arc_filled(d, 0, 0, SAEC_R1 - 1.0f, SAEC_R1, a0, a1, 4);
        } else {
            // A year still owed: hairline
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.05f));
            draw_arc_filled(d, 0, 0, SAEC_R0, SAEC_R1, a0, a1, 4);
        }
    }

    // Ring rims
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.25f));
    draw_circle_stroked(d, 0, 0, SAEC_R0 - 3.0f, 1.0f);
    draw_circle_stroked(d, 0, 0, SAEC_R1 + 3.0f, 1.0f);

    // Decade spokes + labels: calendar decades for the century, years
    // of age for a life
    {
        int lw = _font_compat[FONT_seconds].weight;
        for (int i = 0; i <= n; i += 10) {
            float p = (float)i / n;
            float sx = sinf(p * 2.0f * (float)M_PI);
            float sy = -cosf(p * 2.0f * (float)M_PI);
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.40f));
            draw_line(d, sx * (SAEC_R0 - 10.0f), sy * (SAEC_R0 - 10.0f),
                      sx * (SAEC_R1 + 8.0f), sy * (SAEC_R1 + 8.0f), 1.0f);
            if (i == n) continue;   // the closing spoke needs no label

            char lb[16];
            if (vita)
                saec__roman(i == 0 ? 1 : i, lb, sizeof(lb));
            else
                snprintf(lb, sizeof(lb), "%d", y0 + i);
            float tw = sdf_measure_width(lw, lb) * 13.0f;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.45f));
            draw_text_ex(d, lw, 13.0f, sx * (SAEC_R1 + 22.0f) - tw * 0.5f,
                         sy * (SAEC_R1 + 22.0f) - 6.5f, lb);
        }
    }

    // ---- Center: the year, and where you stand in the span ----
    {
        char yb[16], ab[32];
        saec__roman(tv->year, yb, sizeof(yb));
        draw_set_color(d, dca(0.78f, 0.75f, 0.68f, 0.95f));
        draw_text_centered(d, FONT_month, 0, -14.0f, yb);

        if (vita) {
            // AETATIS: "in the Nth year of age" — the year you are
            // living through, not the birthdays you have banked
            int aet = tv->year - t->config.birth_year + 1;
            char rb[16];
            saec__roman(aet, rb, sizeof(rb));
            snprintf(ab, sizeof(ab), "AETATIS %s", rb);
        } else {
            char rb[8];
            saec__roman(tv->year / 100 + 1, rb, sizeof(rb));
            snprintf(ab, sizeof(ab), "SAECVLVM %s", rb);
        }
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, 22.0f, ab);
    }

    d->alpha = base_alpha;
}

static const ViewVtable saec_vtable = {
    .init   = saec_init,
    .enter  = saec_enter,
    .exit   = saec_exit,
    .update = saec_update,
    .render = saec_render,
};

#endif // SCENE_DEFINED && !VIEW_SAEC_IMPL
