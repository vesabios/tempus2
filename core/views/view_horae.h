// view_horae.h — HORAE: the planetary hours.
//
// The oldest fusion of planets and timekeeping, and the reason the days
// of the week are named what they are. Each hour is ruled by a planet
// in the Chaldean order (Saturn, Jupiter, Mars, Sun, Venus, Mercury,
// Moon — slowest to fastest); the ruler of a day's FIRST hour names the
// day, and stepping 24 hours through the order lands three places on —
// which generates Sun, Moon, Mars, Mercury, Jupiter, Venus, Saturn:
// the week.
//
// The hours are TEMPORAL (horae temporales): daylight divided into
// twelve, night into twelve, so summer day-hours run long and winter
// ones short. The dial wears that inequality openly — the day sectors
// swell and shrink with the seasons. Noon at the bottom, matching the
// solar dial's convention. Pure time: no ephemeris, just sunrise,
// sunset, and arithmetic older than the calendar.

#ifndef VIEW_HORAE_H
#define VIEW_HORAE_H

#include <stdio.h>
#include "../view.h"

struct HoraeViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene horae_blend
};

#endif // VIEW_HORAE_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_HORAE_IMPL)
#define VIEW_HORAE_IMPL

// Dial bands (world units; the calendar wheel rides outside at 450)
#define HORAE_R0     298.0f   // hour ring inner
#define HORAE_R1     404.0f   // hour ring outer
#define HORAE_RPIP   351.0f   // ruler pips
#define HORAE_WK0    225.0f   // week ring inner
#define HORAE_WK1    263.0f   // week ring outer

// Chaldean order, slowest to fastest; colors borrowed from the orrery
static const uint8_t horae__chaldean_body[7] = {
    BODY_SATURN, BODY_JUPITER, BODY_MARS, BODY_SUN,
    BODY_VENUS, BODY_MERCURY, BODY_MOON,
};

// Hour names, genitive: "the hour OF Saturn"
static const char *horae__genitive[7] = {
    "SATVRNI", "IOVIS", "MARTIS", "SOLIS",
    "VENERIS", "MERCVRII", "LVNAE",
};

// Weekday (0 = Sunday) -> Chaldean index of the day's ruler
static const uint8_t horae__day_ruler[7] = { 3, 6, 2, 5, 1, 4, 0 };

// Weekday names in dial order Sunday..Saturday
static const char *horae__dies[7] = {
    "SOLIS", "LVNAE", "MARTIS", "MERCVRII",
    "IOVIS", "VENERIS", "SATVRNI",
};

static const char *horae__roman[12] = {
    "I", "II", "III", "IV", "V", "VI",
    "VII", "VIII", "IX", "X", "XI", "XII",
};

// Day-fraction -> wheel percent. Midnight at the top, noon at the
// bottom — the solar dial's convention (a northern sun culminates low).
// The wheel mapping (sin, -cos from top, clockwise) gives that with the
// fraction used directly.
static inline float horae__pct(float day_frac) {
    return day_frac - floorf(day_frac);
}

static void horae_init(void *buf, const Tempus *t, const RenderStyle *s) {
    HoraeViewState *st = (HoraeViewState *)buf;
    st->opacity = 1.0;
    (void)t; (void)s;
}

static void horae_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void horae_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void horae_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    HoraeViewState *st = (HoraeViewState *)buf;
    (void)t; (void)dt;
    st->blend = sc->horae_blend;
}

// One hour sector: [f0, f1] in day fractions, chaldean ruler, hour
// number within its half (0-11), day or night
static void horae__sector(DrawCtx *d, const Tempus *t, float f0, float f1,
                          int ruler, int idx, bool is_day, bool current) {
    if (f1 <= f0) return;
    float a0 = horae__pct(f0) * 2.0f * (float)M_PI - (float)M_PI * 0.5f;
    float a1 = a0 + (f1 - f0) * 2.0f * (float)M_PI;
    const uint8_t *c = orr__body_col[horae__chaldean_body[ruler]];

    // Ground tint: day sectors warm, night sectors cold — the dial
    // wears the daylight share of the date
    if (is_day)
        draw_set_color(d, dca(0.30f, 0.24f, 0.12f, current ? 0.34f : 0.16f));
    else
        draw_set_color(d, dca(0.10f, 0.11f, 0.20f, current ? 0.40f : 0.18f));
    draw_arc_filled(d, 0, 0, HORAE_R0, HORAE_R1, a0, a1, 12);

    // Ruler wash over the ground
    draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f,
                          current ? 0.20f : 0.07f));
    draw_arc_filled(d, 0, 0, HORAE_R0, HORAE_R1, a0, a1, 12);

    // Leading boundary
    {
        float pct = horae__pct(f0);
        float sx = sinf(pct * 2.0f * (float)M_PI);
        float sy = -cosf(pct * 2.0f * (float)M_PI);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.22f));
        draw_line(d, sx * HORAE_R0, sy * HORAE_R0,
                  sx * HORAE_R1, sy * HORAE_R1, 1.0f);
    }

    // Ruler pip at sector center + hour numeral inside
    {
        float mid = horae__pct((f0 + f1) * 0.5f) * 2.0f * (float)M_PI;
        float sx = sinf(mid), sy = -cosf(mid);
        draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f,
                              current ? 1.0f : 0.55f));
        draw_circle_filled(d, sx * HORAE_RPIP, sy * HORAE_RPIP,
                           current ? 6.5f : 4.0f);

        int rw = _font_compat[FONT_seconds].weight;
        float rsz = 13.0f;
        float tw = sdf_measure_width(rw, horae__roman[idx]) * rsz;
        draw_set_color(d, dca(0.62f, 0.60f, 0.55f, current ? 0.9f : 0.35f));
        draw_text_ex(d, rw, rsz, sx * (HORAE_R0 + 22.0f) - tw * 0.5f,
                     sy * (HORAE_R0 + 22.0f) - rsz * 0.5f,
                     horae__roman[idx]);
    }
}

static void horae_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const HoraeViewState *st = (const HoraeViewState *)buf;
    const TimeView *tv = &st->tv;
    float base_alpha = d->alpha;

    // Sunrise/sunset as day fractions, guarded for polar edge cases
    float rise = (float)(t->sunrise_mins / 1440.0);
    float set  = (float)(t->sunset_mins / 1440.0);
    if (!(set > rise) || set - rise < 0.02f || set - rise > 0.98f) {
        rise = 0.25f;
        set = 0.75f;
    }
    float dl = set - rise;          // daylight length
    float nl = 1.0f - dl;           // night length
    float dh = dl / 12.0f;          // one day-hour
    float nh = nl / 12.0f;          // one night-hour

    // Weekday and the day's ruler (0 = Sunday; JD + 1.5 floors to it)
    int w = (int)(((long)floor(tv->jd_current + 1.5)) % 7);
    int dri = horae__day_ruler[w];

    // The current temporal hour (index 0-23 from today's sunrise;
    // negative pre-dawn indices reach into yesterday's night)
    float now = (float)tv->percent_of_day;
    int cur;
    if (now >= rise && now < set)
        cur = (int)((now - rise) / dh);
    else if (now >= set)
        cur = 12 + (int)((now - set) / nh);
    else
        cur = -1 - (int)((rise - now) / nh);   // -1 = hour ending at rise

    // ---- The 24 temporal hours covering this date ----
    // Day, then tonight's hours until midnight, then the pre-dawn tail
    // of YESTERDAY'S night (its rulers three Chaldean steps back).
    for (int k = 0; k < 12; k++)
        horae__sector(d, t, rise + k * dh, rise + (k + 1) * dh,
                      (dri + k) % 7, k, true, cur == k);
    for (int k = 0; k < 12; k++) {
        float f0 = set + k * nh;
        if (f0 >= 1.0f) break;
        horae__sector(d, t, f0, fminf(f0 + nh, 1.0f),
                      (dri + 12 + k) % 7, k, false, cur == 12 + k);
    }
    for (int k = 0; k < 12; k++) {
        float f1 = rise - k * nh;
        if (f1 <= 0.0f) break;
        horae__sector(d, t, fmaxf(f1 - nh, 0.0f), f1,
                      (dri + 7 + 20 - k) % 7, 11 - k, false,
                      cur == -1 - k);
    }

    // Sunrise and sunset: gold thresholds
    for (int e = 0; e < 2; e++) {
        float pct = horae__pct(e ? set : rise);
        float sx = sinf(pct * 2.0f * (float)M_PI);
        float sy = -cosf(pct * 2.0f * (float)M_PI);
        draw_set_color(d, dc_scale(s->sunrise_handle, 0.9f));
        draw_line(d, sx * (HORAE_R0 - 8.0f), sy * (HORAE_R0 - 8.0f),
                  sx * (HORAE_R1 + 10.0f), sy * (HORAE_R1 + 10.0f), 1.6f);
    }

    // Ring rims
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
    draw_circle_stroked(d, 0, 0, HORAE_R0, 1.0f);
    draw_circle_stroked(d, 0, 0, HORAE_R1, 1.0f);

    // ---- The week ring: a film strip past a fixed pointer ----
    // The ring turns continuously with the day; a small arrow at noon
    // (bottom, the dial's convention) reads today — the same grammar as
    // the calendar wheel's own pointer. The Chaldean skip stays visible:
    // each day's pip matches its first-hour ruler.
    {
        double daypos = w + tv->percent_of_day;   // continuous day-of-week
        for (int i = 0; i < 7; i++) {
            // Sector center rides the strip; today's center meets the
            // pointer exactly at noon
            float ctr = 0.5f + (float)((i + 0.5 - daypos) / 7.0);
            float a0 = (ctr - 0.5f / 7.0f) * 2.0f * (float)M_PI
                     - (float)M_PI * 0.5f;
            float a1 = a0 + (1.0f / 7.0f) * 2.0f * (float)M_PI;
            const uint8_t *c =
                orr__body_col[horae__chaldean_body[horae__day_ruler[i]]];

            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, 0.10f));
            draw_arc_filled(d, 0, 0, HORAE_WK0, HORAE_WK1, a0, a1, 10);

            float mid = ctr * 2.0f * (float)M_PI;
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, 0.55f));
            draw_circle_filled(d, sinf(mid) * (HORAE_WK0 + 11.0f),
                               -cosf(mid) * (HORAE_WK0 + 11.0f), 3.5f);

            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.45f));
            draw_text_curved(d, FONT_date, 0, 0, HORAE_WK0 + 24.0f,
                             mid, horae__dies[i], 0.6f, 0.9f);
        }
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.20f));
        draw_circle_stroked(d, 0, 0, HORAE_WK0, 1.0f);
        draw_circle_stroked(d, 0, 0, HORAE_WK1, 1.0f);

        // The fixed arrow at noon, aimed outward at the strip
        {
            float tipy = HORAE_WK0 - 3.0f;
            float basey = HORAE_WK0 - 13.0f;
            draw_set_color(d, s->month_text_color);
            int vb = d->num_verts;
            draw__push_vert(d, 0.0f, tipy, d->white_u, d->white_v);
            draw__push_vert(d, -5.0f, basey, d->white_u, d->white_v);
            draw__push_vert(d, 5.0f, basey, d->white_u, d->white_v);
            draw__tri(d, vb, vb + 1, vb + 2);
        }
    }

    // ---- The hands: this IS a clock face ----
    // The day hand wears the 12-hour face's hour-hand dress (same width,
    // color, and start radius) but makes one revolution per day; the
    // seconds hand carries over unchanged, the familiar red pulse that
    // says "clock" at a glance.
    {
        float angle = (float)(now * 2.0 * M_PI);
        float dx = sinf(angle), dy = -cosf(angle);
        float px2 = -dy, py2 = dx;
        float hw = s->hours_width * 0.5f;
        float h0 = s->hours_start;
        float h1 = HORAE_R0 - 36.0f;
        draw_set_color(d, s->hours_color);
        float corners[4][2] = {
            { dx * h0 - px2 * hw, dy * h0 - py2 * hw },
            { dx * h0 + px2 * hw, dy * h0 + py2 * hw },
            { dx * h1 + px2 * hw, dy * h1 + py2 * hw },
            { dx * h1 - px2 * hw, dy * h1 - py2 * hw },
        };
        int vbase = d->num_verts;
        for (int j = 0; j < 4; j++)
            draw__push_vert(d, corners[j][0], corners[j][1],
                            d->white_u, d->white_v);
        draw__tri(d, vbase, vbase + 1, vbase + 2);
        draw__tri(d, vbase, vbase + 2, vbase + 3);
    }
    {
        double real_secs = s->sweep_seconds
            ? ((double)tv->secs + tv->frac_secs) / 60.0
            : (double)tv->secs / 60.0;
        float a = (float)(real_secs * 2.0 * M_PI);
        float dx = sinf(a), dy = -cosf(a);
        draw_set_color(d, s->seconds_color);
        draw_line(d, dx * s->seconds_start, dy * s->seconds_start,
                  dx * (HORAE_R0 - 12.0f), dy * (HORAE_R0 - 12.0f), 1.5f);
    }

    // ---- Center readout: whose hour is this ----
    {
        int ridx;
        int hnum;
        bool hday;
        if (cur >= 0 && cur < 12) {
            ridx = (dri + cur) % 7;  hnum = cur;       hday = true;
        } else if (cur >= 12) {
            ridx = (dri + cur) % 7;  hnum = cur - 12;  hday = false;
        } else {
            ridx = (dri + 7 + 20 - (-1 - cur)) % 7;
            hnum = 11 - (-1 - cur);
            hday = false;
        }
        const uint8_t *c = orr__body_col[horae__chaldean_body[ridx]];
        draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                              c[2] / 255.0f, 0.95f));
        draw_circle_filled(d, 0, -34.0f, 9.0f);

        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, 2.0f, "HORA");
        draw_set_color(d, dca(0.78f, 0.75f, 0.68f, 0.95f));
        draw_text_centered(d, FONT_month, 0, 30.0f, horae__genitive[ridx]);

        char hb[24];
        snprintf(hb, sizeof(hb), "%s %s", horae__roman[hnum],
                 hday ? "DIEI" : "NOCTIS");
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, 58.0f, hb);
    }

    d->alpha = base_alpha;
}

static const ViewVtable horae_vtable = {
    .init   = horae_init,
    .enter  = horae_enter,
    .exit   = horae_exit,
    .update = horae_update,
    .render = horae_render,
};

#endif // SCENE_DEFINED && !VIEW_HORAE_IMPL
