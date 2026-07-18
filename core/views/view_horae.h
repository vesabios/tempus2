// view_horae.h — HORAE: the planetary hours, as gearing.
//
// The oldest fusion of planets and timekeeping, and the reason the days
// of the week are named what they are. Each hour is ruled by a planet
// in the Chaldean order (Saturn, Jupiter, Mars, Sun, Venus, Mercury,
// Moon — slowest to fastest), one unbroken chain with no daily reset.
// The chain's true period is not the day but the WEEK: 168 hours, and
// 168/24 = 7, so the system IS a 7:1 internal gear — a 24-hour pinion
// rolling inside a 168-cell annulus, turning seven times per lap. The
// skip-of-three that orders the weekdays is just what that pinion does.
//
// So the dial is built as that gear: the week ring stands FIXED
// (Sunday's midnight at the top, time clockwise), carrying the whole
// 168-tooth chain in the rulers' colors, and the day clock ORBITS
// around its outside — touching it always, never spinning. The clock
// stays upright and readable (midnight top, its own hand, the red
// seconds pulse); its cells carry NO ruler colors, because the hour's
// ruler was never a property of the clock — it is PRODUCED at the
// point of contact, where the plain tooth presses the colored chain.
// Where they touch is the current planetary day and hour.
// Both wheels cut their teeth from the real sunrise and sunset — the
// hours are TEMPORAL, daylight split into twelve and night into
// twelve, so summer day-teeth run wide. Night grounds grade through
// the twilights; dusk is a place, not a line (Prague's lesson).

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

// Gear geometry (world units; the calendar wheel rides outside at
// 450, and the orbiting clock must clear it: RING1 + 2*WHEEL_R < 434)
#define HORAE_RING0   150.0f   // week annulus inner
#define HORAE_RING1   210.0f   // week annulus outer
#define HORAE_WHEEL_R 108.0f   // day clock radius (touches the ring)
#define HORAE_WHEEL_W 36.0f    // day clock tooth band depth

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

// Weekday initials in dial order Sunday..Saturday (dies Solis..Saturni)
static const char *horae__dies_init[7] = {
    "S", "L", "M", "M", "I", "V", "S",
};

static const char *horae__roman[12] = {
    "I", "II", "III", "IV", "V", "VI",
    "VII", "VIII", "IX", "X", "XI", "XII",
};

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

// Wrap into [-half, half)
static inline float horae__wrap(float v, float period) {
    v = fmodf(v + period * 0.5f, period);
    if (v < 0) v += period;
    return v - period * 0.5f;
}

// Arc band cell on any center, in wheel-pct coordinates [p0, p1),
// splitting cells that wrap the top
static void horae__cell(DrawCtx *d, float cx, float cy, float r0,
                        float r1, float p0, float p1) {
    p0 -= floorf(p0);
    p1 -= floorf(p1);
    if (p1 <= p0) {
        if (1.0f - p0 > 1e-4f)
            draw_arc_filled(d, cx, cy, r0, r1,
                            p0 * 2.0f * (float)M_PI - (float)M_PI * 0.5f,
                            1.0f * 2.0f * (float)M_PI - (float)M_PI * 0.5f,
                            6);
        if (p1 > 1e-4f)
            draw_arc_filled(d, cx, cy, r0, r1,
                            -(float)M_PI * 0.5f,
                            p1 * 2.0f * (float)M_PI - (float)M_PI * 0.5f,
                            6);
    } else {
        draw_arc_filled(d, cx, cy, r0, r1,
                        p0 * 2.0f * (float)M_PI - (float)M_PI * 0.5f,
                        p1 * 2.0f * (float)M_PI - (float)M_PI * 0.5f, 6);
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
    float dl = set - rise;
    float dh = dl / 12.0f;              // one day-tooth
    float nh = (1.0f - dl) / 12.0f;     // one night-tooth

    // Tooth boundaries within one PLANETARY day (sunrise to sunrise),
    // in day units: u[0]=0 at sunrise .. u[24]=1 at the next sunrise
    float u[25];
    for (int h = 0; h <= 24; h++)
        u[h] = (h <= 12) ? h * dh : 12.0f * dh + (h - 12) * nh;

    // Where we are: civil weekday, week-time, planetary day and hour
    int w = (int)(((long)floor(tv->jd_current + 1.5)) % 7);
    float now = (float)tv->percent_of_day;
    float m_now = w + now;                       // week-time, days
    float u_now = now - rise;                    // planetary-day fraction
    if (u_now < 0) u_now += 1.0f;
    int pd = (now >= rise) ? w : (w + 6) % 7;    // planetary day
    int hcur = 0;
    while (hcur < 23 && u[hcur + 1] <= u_now) hcur++;
    int ridx = (horae__day_ruler[pd] + hcur) % 7;

    // Sun-altitude sampler for twilight grading (civil fraction f of the
    // planetary day pd may exceed 1 = past midnight; jd handles it)
    double jd0 = tv->jd_current - 0.5 - t->config.timezone / 24.0
               + (pd - w);   // start of the planetary day's civil date

    // ---- The week annulus: 168 teeth, FIXED — the week as a dial ----
    // Sunday's midnight at the top, time clockwise. The chain runs
    // unbroken around the loop; day teeth lit, night teeth sunk — the
    // temporal valley laid out seven times, once per day.
    for (int dd = 0; dd < 7; dd++) {
        int dri = horae__day_ruler[dd];
        for (int h = 0; h < 24; h++) {
            float m0 = dd + rise + u[h];
            float m1 = dd + rise + u[h + 1];

            int rr = (dri + h) % 7;
            const uint8_t *c = orr__body_col[horae__chaldean_body[rr]];
            bool is_day = h < 12;
            bool cur = (dd == pd && h == hcur);
            float al = cur ? 0.95f : (is_day ? 0.60f : 0.22f);
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, al));
            horae__cell(d, 0, 0, HORAE_RING0, HORAE_RING1,
                        m0 / 7.0f, m1 / 7.0f);
        }
    }
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
    draw_circle_stroked(d, 0, 0, HORAE_RING0, 1.0f);
    draw_circle_stroked(d, 0, 0, HORAE_RING1, 1.0f);

    // Civil midnight spokes + day initials riding the strip
    {
        int iw = _font_compat[FONT_seconds].weight;
        for (int i = 0; i < 7; i++) {
            float pm = (float)i / 7.0f;
            float am = pm * 2.0f * (float)M_PI;
            float sx = sinf(am), sy = -cosf(am);
            draw_set_color(d, dca(0.10f, 0.10f, 0.10f, 0.9f));
            draw_line(d, sx * (HORAE_RING0 - 6.0f),
                      sy * (HORAE_RING0 - 6.0f),
                      sx * (HORAE_RING1 + 4.0f),
                      sy * (HORAE_RING1 + 4.0f), 2.0f);

            float pc = (i + 0.5f) / 7.0f;
            float ac = pc * 2.0f * (float)M_PI;
            float cx = sinf(ac) * (HORAE_RING0 - 18.0f);
            float cy = -cosf(ac) * (HORAE_RING0 - 18.0f);
            float tw2 = sdf_measure_width(iw, horae__dies_init[i]) * 15.0f;
            draw_set_color(d, i == w
                ? dca(0.78f, 0.75f, 0.68f, 0.9f)
                : dca(0.50f, 0.49f, 0.46f, 0.40f));
            draw_text_ex(d, iw, 15.0f, cx - tw2 * 0.5f, cy - 7.5f,
                         horae__dies_init[i]);
        }
    }

    // ---- The day clock, orbiting the ring without spinning ----
    // Its center rides the contact angle (now, on the week dial) but
    // its face stays upright — midnight top, a readable clock. The
    // touch point against the colored chain IS the reading.
    float theta_c = m_now / 7.0f;                    // contact, ring pct
    float ca = theta_c * 2.0f * (float)M_PI;
    float wcx = sinf(ca) * (HORAE_RING1 + HORAE_WHEEL_R);
    float wcy = -cosf(ca) * (HORAE_RING1 + HORAE_WHEEL_R);
    {
        int nw2 = _font_compat[FONT_seconds].weight;
        for (int h = 0; h < 24; h++) {
            // Fixed orientation: a tooth sits at its TIME OF DAY on the
            // face (midnight top), no ruler colors — the day clock is
            // neutral; the chain it presses against is not
            float f0 = rise + u[h];
            float f1 = rise + u[h + 1];

            bool is_day = h < 12;
            bool cur = h == hcur;

            if (is_day) {
                draw_set_color(d, dca(0.32f, 0.25f, 0.12f,
                                      cur ? 0.55f : 0.28f));
            } else {
                double jm = jd0 + rise + (u[h] + u[h + 1]) * 0.5;
                double slon, slat;
                planets__body_lonlat(BODY_SUN, jm, &slon, &slat);
                float sa = (float)planets_sky_altitude(
                    slon, slat, jm, t->config.latitude,
                    t->config.longitude);
                float twf = -sa / 18.0f;
                if (twf < 0) twf = 0;
                if (twf > 1) twf = 1;
                draw_set_color(d, dca(0.42f + (0.055f - 0.42f) * twf,
                                      0.24f + (0.060f - 0.24f) * twf,
                                      0.10f + (0.150f - 0.10f) * twf,
                                      cur ? 0.60f : 0.32f));
            }
            horae__cell(d, wcx, wcy, HORAE_WHEEL_R - HORAE_WHEEL_W,
                        HORAE_WHEEL_R, f0, f1);

            // Tooth boundary
            {
                float ab = (f0 - floorf(f0)) * 2.0f * (float)M_PI;
                float sx = sinf(ab), sy = -cosf(ab);
                draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.25f));
                draw_line(d, wcx + sx * (HORAE_WHEEL_R - HORAE_WHEEL_W),
                          wcy + sy * (HORAE_WHEEL_R - HORAE_WHEEL_W),
                          wcx + sx * HORAE_WHEEL_R,
                          wcy + sy * HORAE_WHEEL_R, 1.0f);
            }

            // Numeral, inside the band
            {
                float fm = (f0 + f1) * 0.5f;
                float am = fm * 2.0f * (float)M_PI;
                float rn = HORAE_WHEEL_R - HORAE_WHEEL_W - 13.0f;
                float rx = wcx + sinf(am) * rn;
                float ry = wcy - cosf(am) * rn;
                float tw2 = sdf_measure_width(nw2, horae__roman[h % 12])
                          * 10.0f;
                draw_set_color(d, dca(0.62f, 0.60f, 0.55f,
                                      cur ? 0.95f : 0.35f));
                draw_text_ex(d, nw2, 10.0f, rx - tw2 * 0.5f, ry - 5.0f,
                             horae__roman[h % 12]);
            }
        }

        // Rims + the gold thresholds, fixed on the face: ORTVS at
        // sunrise, OCCASVS at sunset
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.45f));
        draw_circle_stroked(d, wcx, wcy, HORAE_WHEEL_R, 1.0f);
        draw_circle_stroked(d, wcx, wcy, HORAE_WHEEL_R - HORAE_WHEEL_W,
                            1.0f);
        for (int e = 0; e < 2; e++) {
            float fe = e ? set : rise;
            float ae = fe * 2.0f * (float)M_PI;
            float sx = sinf(ae), sy = -cosf(ae);
            draw_set_color(d, dc_scale(s->sunrise_handle, 0.95f));
            draw_line(d, wcx + sx * (HORAE_WHEEL_R - HORAE_WHEEL_W - 6.0f),
                      wcy + sy * (HORAE_WHEEL_R - HORAE_WHEEL_W - 6.0f),
                      wcx + sx * (HORAE_WHEEL_R + 6.0f),
                      wcy + sy * (HORAE_WHEEL_R + 6.0f), 1.6f);
        }

        // The day hand (hour-hand dress, one rev per day) + the red
        // seconds pulse — this face is a CLOCK, upright and readable
        {
            float angle = now * 2.0f * (float)M_PI;
            float dx = sinf(angle), dy = -cosf(angle);
            float px2 = -dy, py2 = dx;
            float hw = 4.0f;
            float h0 = 12.0f, h1 = HORAE_WHEEL_R - HORAE_WHEEL_W - 26.0f;
            draw_set_color(d, s->hours_color);
            int vb = d->num_verts;
            draw__push_vert(d, wcx + dx * h0 - px2 * hw,
                            wcy + dy * h0 - py2 * hw,
                            d->white_u, d->white_v);
            draw__push_vert(d, wcx + dx * h0 + px2 * hw,
                            wcy + dy * h0 + py2 * hw,
                            d->white_u, d->white_v);
            draw__push_vert(d, wcx + dx * h1 + px2 * hw,
                            wcy + dy * h1 + py2 * hw,
                            d->white_u, d->white_v);
            draw__push_vert(d, wcx + dx * h1 - px2 * hw,
                            wcy + dy * h1 - py2 * hw,
                            d->white_u, d->white_v);
            draw__tri(d, vb, vb + 1, vb + 2);
            draw__tri(d, vb, vb + 2, vb + 3);
        }
        double real_secs = s->sweep_seconds
            ? ((double)tv->secs + tv->frac_secs) / 60.0
            : (double)tv->secs / 60.0;
        float sa2 = (float)(real_secs * 2.0 * M_PI);
        draw_set_color(d, s->seconds_color);
        draw_line(d, wcx, wcy,
                  wcx + sinf(sa2) * (HORAE_WHEEL_R - HORAE_WHEEL_W - 8.0f),
                  wcy - cosf(sa2) * (HORAE_WHEEL_R - HORAE_WHEEL_W - 8.0f),
                  1.5f);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.6f));
        draw_circle_filled(d, wcx, wcy, 2.5f);
    }

    // ---- Readout at the heart of the week ----
    {
        const uint8_t *c = orr__body_col[horae__chaldean_body[ridx]];
        draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                              c[2] / 255.0f, 0.95f));
        draw_circle_filled(d, 0, -62.0f, 9.0f);

        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, -28.0f, "HORA");
        draw_set_color(d, dca(0.78f, 0.75f, 0.68f, 0.95f));
        draw_text_centered(d, FONT_month, 0, 0.0f,
                          horae__genitive[ridx]);

        char hb[24];
        snprintf(hb, sizeof(hb), "%s %s", horae__roman[hcur % 12],
                 hcur < 12 ? "DIEI" : "NOCTIS");
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, 28.0f, hb);
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
