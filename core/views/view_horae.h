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
// So the dial is built as that gear, spirograph-fashion: the day
// clock holds the CENTER, fixed and upright (midnight top, its hands,
// the red seconds pulse), and the 168-tooth week ring rides around it
// ECCENTRICALLY — always tangent at exactly one point: where the day
// hand points. The hand aims at the mesh; the chain tooth pressed
// there is the ruling planet of this hour. The clock's cells carry NO
// ruler colors, because the hour's ruler was never a property of the
// clock — it is PRODUCED at the contact. Once per day the ring wobbles
// around the face; once per week its chain precesses home.
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

// Gear geometry (world units; the eccentric ring's farthest sweep,
// HORAE_ECC + HORAE_RING_OUT + labels, must clear the calendar wheel
// at 434)
#define HORAE_CLOCK_R   210.0f   // the fixed day clock, center stage
#define HORAE_CLOCK_W    46.0f   // its tooth band depth
#define HORAE_RING_IN   270.0f   // week ring inner (touches the clock)
#define HORAE_RING_OUT  330.0f   // week ring outer
#define HORAE_ECC (HORAE_RING_IN - HORAE_CLOCK_R)   // eccentricity

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

// Weekday names in dial order Sunday..Saturday (dies Solis..Saturni)
static const char *horae__dies[7] = {
    "SOLIS", "LVNAE", "MARTIS", "MERCVRII",
    "IOVIS", "VENERIS", "SATVRNI",
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

    // ---- The day clock, fixed at center stage ----
    // A readable 24-hour temporal clock: midnight top, cells at their
    // time of day, neutral of any planet. The hand points at now —
    // which is exactly where the week ring touches.
    {
        int nw2 = _font_compat[FONT_seconds].weight;
        for (int h = 0; h < 24; h++) {
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
            horae__cell(d, 0, 0, HORAE_CLOCK_R - HORAE_CLOCK_W,
                        HORAE_CLOCK_R, f0, f1);

            // Tooth boundary
            {
                float ab = (f0 - floorf(f0)) * 2.0f * (float)M_PI;
                float sx = sinf(ab), sy = -cosf(ab);
                draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.25f));
                draw_line(d, sx * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                          sy * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                          sx * HORAE_CLOCK_R, sy * HORAE_CLOCK_R, 1.0f);
            }

            // Numeral, inside the band
            {
                float fm = (f0 + f1) * 0.5f;
                float am = fm * 2.0f * (float)M_PI;
                float rn = HORAE_CLOCK_R - HORAE_CLOCK_W - 13.0f;
                float rx = sinf(am) * rn;
                float ry = -cosf(am) * rn;
                float tw2 = sdf_measure_width(nw2, horae__roman[h % 12])
                          * 11.0f;
                draw_set_color(d, dca(0.62f, 0.60f, 0.55f,
                                      cur ? 0.95f : 0.35f));
                draw_text_ex(d, nw2, 11.0f, rx - tw2 * 0.5f, ry - 5.5f,
                             horae__roman[h % 12]);
            }
        }

        // Rims + gold thresholds: ORTVS at sunrise, OCCASVS at sunset
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.45f));
        draw_circle_stroked(d, 0, 0, HORAE_CLOCK_R, 1.0f);
        draw_circle_stroked(d, 0, 0, HORAE_CLOCK_R - HORAE_CLOCK_W, 1.0f);
        for (int e = 0; e < 2; e++) {
            float fe = e ? set : rise;
            float ae = fe * 2.0f * (float)M_PI;
            float sx = sinf(ae), sy = -cosf(ae);
            draw_set_color(d, dc_scale(s->sunrise_handle, 0.95f));
            draw_line(d, sx * (HORAE_CLOCK_R - HORAE_CLOCK_W - 6.0f),
                      sy * (HORAE_CLOCK_R - HORAE_CLOCK_W - 6.0f),
                      sx * (HORAE_CLOCK_R + 6.0f),
                      sy * (HORAE_CLOCK_R + 6.0f), 1.6f);
        }
    }

    // ---- The week ring, riding eccentrically — the spirograph ----
    // Tangent to the clock at the hand's direction: ring center sits
    // opposite the contact by the eccentricity, oriented so the
    // CURRENT hour's tick is the one being pressed. Seven named day
    // regions washed in their rulers' colors; the 168-hour chain drawn
    // as fine ruler-colored ticks on the meshing edge — accents on the
    // structure, not the structure itself.
    float ah = now * 2.0f * (float)M_PI;         // the hand = the contact
    float hdx = sinf(ah), hdy = -cosf(ah);
    float rcx = -hdx * HORAE_ECC;                // ring center
    float rcy = -hdy * HORAE_ECC;
    {
        // Seven day regions + engraved names
        float lsz = _font_compat[FONT_date].size;
        for (int i = 0; i < 7; i++) {
            float q0 = now + horae__wrap((float)i - m_now, 7.0f) / 7.0f;
            const uint8_t *c =
                orr__body_col[horae__chaldean_body[horae__day_ruler[i]]];
            bool today = i == w;
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, today ? 0.16f : 0.06f));
            horae__cell(d, rcx, rcy, HORAE_RING_IN, HORAE_RING_OUT,
                        q0, q0 + 1.0f / 7.0f);

            // Midnight boundary spoke
            {
                float ab = (q0 - floorf(q0)) * 2.0f * (float)M_PI;
                float sx = sinf(ab), sy = -cosf(ab);
                draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.30f));
                draw_line(d, rcx + sx * (HORAE_RING_IN - 4.0f),
                          rcy + sy * (HORAE_RING_IN - 4.0f),
                          rcx + sx * (HORAE_RING_OUT + 4.0f),
                          rcy + sy * (HORAE_RING_OUT + 4.0f), 1.0f);
            }

            // Name along the band, waist on the band's center line
            {
                float qc = q0 + 0.5f / 7.0f;
                float ang = qc * 2.0f * (float)M_PI;
                float na = fmodf(ang, 2.0f * (float)M_PI);
                if (na < 0) na += 2.0f * (float)M_PI;
                bool lflip = (na > (float)M_PI * 0.5f
                              && na < (float)M_PI * 1.5f);
                float mid = (HORAE_RING_IN + HORAE_RING_OUT) * 0.5f;
                float lr = mid - lsz * 0.5f
                         + lsz * (lflip ? 0.51f : 0.37f);
                draw_set_color(d, today
                    ? dca(0.80f, 0.77f, 0.70f, 0.95f)
                    : dca(0.55f, 0.53f, 0.49f, 0.45f));
                draw_text_curved(d, FONT_date, rcx, rcy, lr, ang,
                                 horae__dies[i], 0.8f, 1.0f);
            }
        }

        // The 168-hour chain as ticks on the meshing edge
        for (int dd = 0; dd < 7; dd++) {
            int dri = horae__day_ruler[dd];
            for (int h = 0; h < 24; h++) {
                float mc = dd + rise + (u[h] + u[h + 1]) * 0.5f;
                float q = now + horae__wrap(mc - m_now, 7.0f) / 7.0f;
                float aq = q * 2.0f * (float)M_PI;
                float sx = sinf(aq), sy = -cosf(aq);

                int rr = (dri + h) % 7;
                const uint8_t *c =
                    orr__body_col[horae__chaldean_body[rr]];
                bool is_day = h < 12;
                bool cur = (dd == pd && h == hcur);
                float len = cur ? 22.0f : (is_day ? 13.0f : 8.0f);
                float al = cur ? 1.0f : (is_day ? 0.75f : 0.40f);
                draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                      c[2] / 255.0f, al));
                draw_line(d, rcx + sx * (HORAE_RING_IN + 2.0f),
                          rcy + sy * (HORAE_RING_IN + 2.0f),
                          rcx + sx * (HORAE_RING_IN + 2.0f + len),
                          rcy + sy * (HORAE_RING_IN + 2.0f + len),
                          cur ? 2.2f : 1.4f);
            }
        }

        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
        draw_circle_stroked(d, rcx, rcy, HORAE_RING_IN, 1.0f);
        draw_circle_stroked(d, rcx, rcy, HORAE_RING_OUT, 1.0f);
    }

    // ---- The hands, over everything: this is a clock ----
    {
        // Day hand: hour-hand dress, one revolution per day, aimed at
        // the contact — it points AT the ruling planet's tooth
        float px2 = -hdy, py2 = hdx;
        float hw = 5.0f;
        float h0 = 16.0f, h1 = HORAE_CLOCK_R - 10.0f;
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

        double real_secs = s->sweep_seconds
            ? ((double)tv->secs + tv->frac_secs) / 60.0
            : (double)tv->secs / 60.0;
        float sa2 = (float)(real_secs * 2.0 * M_PI);
        draw_set_color(d, s->seconds_color);
        draw_line(d, 0, 0,
                  sinf(sa2) * (HORAE_CLOCK_R - HORAE_CLOCK_W - 10.0f),
                  -cosf(sa2) * (HORAE_CLOCK_R - HORAE_CLOCK_W - 10.0f),
                  1.5f);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.6f));
        draw_circle_filled(d, 0, 0, 3.0f);
    }

    // ---- The reading, written outside the ring at the contact ----
    // The hand points through the mesh to the ruling planet; the words
    // follow the touch around the dial.
    {
        const uint8_t *c = orr__body_col[horae__chaldean_body[ridx]];
        float tr = HORAE_RING_OUT - HORAE_ECC + 46.0f;
        float tx = hdx * tr, ty = hdy * tr;

        draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                              c[2] / 255.0f, 0.95f));
        draw_circle_filled(d, tx, ty - 34.0f, 6.5f);
        draw_set_color(d, dca(0.80f, 0.77f, 0.70f, 0.95f));
        draw_text_centered(d, FONT_month, tx, ty, horae__genitive[ridx]);

        char hb[28];
        snprintf(hb, sizeof(hb), "HORA %s %s", horae__roman[hcur % 12],
                 hcur < 12 ? "DIEI" : "NOCTIS");
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.60f));
        draw_text_centered(d, FONT_date, tx, ty + 26.0f, hb);
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
