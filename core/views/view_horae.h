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
// So the dial is built as that gear. The week annulus streams past a
// fixed mesh marker at the bottom (the instrument's film-strip rule);
// the day wheel sits tangent there, counter-rotating like a proper
// pinion, and the two touching cells always agree: the current hour.
// Both gears cut their teeth from the real sunrise and sunset — the
// hours are TEMPORAL, daylight split into twelve and night into twelve,
// so summer day-teeth run wide, and the lit valley streams around the
// annulus with the season. Night grounds grade through the twilights;
// dusk is a place, not a line (Prague's lesson).

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

// Gear geometry (world units; the calendar wheel rides outside at 450)
#define HORAE_RING0   332.0f   // week annulus inner
#define HORAE_RING1   414.0f   // week annulus outer
#define HORAE_WHEEL_R 150.0f   // day pinion radius (tangent at the mesh)
#define HORAE_WHEEL_W 44.0f    // pinion tooth band depth

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

    // ---- The week annulus: 168 teeth, one unbroken chain ----
    // Ring position of week-time m: the strip streams past the mesh at
    // the bottom (pct 0.5), later hours arriving from the right.
    for (int dd = 0; dd < 7; dd++) {
        int dri = horae__day_ruler[dd];
        for (int h = 0; h < 24; h++) {
            float m0 = dd + rise + u[h];
            float m1 = dd + rise + u[h + 1];
            float p1 = 0.5f - horae__wrap(m0 - m_now, 7.0f) / 7.0f;
            float p0 = 0.5f - horae__wrap(m1 - m_now, 7.0f) / 7.0f;
            if (p1 - p0 < 0) continue;   // cell spans the far seam; skip
            float a0 = p0 * 2.0f * (float)M_PI - (float)M_PI * 0.5f;
            float a1 = p1 * 2.0f * (float)M_PI - (float)M_PI * 0.5f;

            int rr = (dri + h) % 7;
            const uint8_t *c = orr__body_col[horae__chaldean_body[rr]];
            bool is_day = h < 12;
            bool cur = (dd == pd && h == hcur);
            // Ruler color carries the chain; day teeth lit, night teeth
            // sunk — the temporal valley streams around the loop
            float al = cur ? 0.95f : (is_day ? 0.60f : 0.22f);
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, al));
            draw_arc_filled(d, 0, 0, HORAE_RING0, HORAE_RING1,
                            a0, a1, 6);
        }
    }
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
    draw_circle_stroked(d, 0, 0, HORAE_RING0, 1.0f);
    draw_circle_stroked(d, 0, 0, HORAE_RING1, 1.0f);

    // Civil midnight spokes + day initials riding the strip
    {
        int iw = _font_compat[FONT_seconds].weight;
        for (int i = 0; i < 7; i++) {
            float pm = 0.5f - horae__wrap((float)i - m_now, 7.0f) / 7.0f;
            float am = pm * 2.0f * (float)M_PI;
            float sx = sinf(am), sy = -cosf(am);
            draw_set_color(d, dca(0.10f, 0.10f, 0.10f, 0.9f));
            draw_line(d, sx * HORAE_RING0, sy * HORAE_RING0,
                      sx * (HORAE_RING1 + 6.0f),
                      sy * (HORAE_RING1 + 6.0f), 2.0f);

            float pc = 0.5f - horae__wrap(i + 0.5f - m_now, 7.0f) / 7.0f;
            float ac = pc * 2.0f * (float)M_PI;
            float cx = sinf(ac) * (HORAE_RING1 + 18.0f);
            float cy = -cosf(ac) * (HORAE_RING1 + 18.0f);
            float tw2 = sdf_measure_width(iw, horae__dies_init[i]) * 15.0f;
            draw_set_color(d, i == w
                ? dca(0.78f, 0.75f, 0.68f, 0.9f)
                : dca(0.50f, 0.49f, 0.46f, 0.40f));
            draw_text_ex(d, iw, 15.0f, cx - tw2 * 0.5f, cy - 7.5f,
                         horae__dies_init[i]);
        }
    }

    // ---- The day pinion, tangent at the mesh ----
    // Counter-rotates against the stream, one turn per planetary day;
    // its current tooth touches the ring's current tooth at the mesh.
    float wcy = HORAE_RING0 - HORAE_WHEEL_R;   // wheel center (0, wcy)
    {
        // Tooth grounds + rulers + numerals
        int nw2 = _font_compat[FONT_seconds].weight;
        for (int h = 0; h < 24; h++) {
            // Wheel position: mesh (wheel bottom) = pct 0.5; later teeth
            // to the right — counter-rotation against the ring
            float q1 = 0.5f - horae__wrap(u[h] - u_now, 1.0f);
            float q0 = 0.5f - horae__wrap(u[h + 1] - u_now, 1.0f);
            if (q1 - q0 < 0) continue;   // spans the wheel's far side
            float a0 = q0 * 2.0f * (float)M_PI - (float)M_PI * 0.5f;
            float a1 = q1 * 2.0f * (float)M_PI - (float)M_PI * 0.5f;

            bool is_day = h < 12;
            bool cur = h == hcur;
            int rr = (horae__day_ruler[pd] + h) % 7;
            const uint8_t *c = orr__body_col[horae__chaldean_body[rr]];

            // Ground: warm day / twilight-graded night
            if (is_day) {
                draw_set_color(d, dca(0.32f, 0.25f, 0.12f,
                                      cur ? 0.5f : 0.28f));
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
                                      cur ? 0.55f : 0.30f));
            }
            draw_arc_filled(d, 0, wcy, HORAE_WHEEL_R - HORAE_WHEEL_W,
                            HORAE_WHEEL_R, a0, a1, 6);

            // Ruler wash on the tooth face
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, cur ? 0.45f : 0.16f));
            draw_arc_filled(d, 0, wcy, HORAE_WHEEL_R - HORAE_WHEEL_W,
                            HORAE_WHEEL_R, a0, a1, 6);

            // Tooth boundary
            {
                float qb = 0.5f - horae__wrap(u[h] - u_now, 1.0f);
                float ab = qb * 2.0f * (float)M_PI;
                float sx = sinf(ab), sy = -cosf(ab);
                draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.25f));
                draw_line(d, sx * (HORAE_WHEEL_R - HORAE_WHEEL_W),
                          wcy + -cosf(ab) * (HORAE_WHEEL_R - HORAE_WHEEL_W),
                          sx * HORAE_WHEEL_R,
                          wcy + sy * HORAE_WHEEL_R, 1.0f);
            }

            // Numeral, upright, riding its tooth
            {
                float qm = 0.5f - horae__wrap((u[h] + u[h + 1]) * 0.5f
                                              - u_now, 1.0f);
                float am = qm * 2.0f * (float)M_PI;
                float rx = sinf(am) * (HORAE_WHEEL_R - HORAE_WHEEL_W - 16.0f);
                float ry = wcy - cosf(am) * (HORAE_WHEEL_R - HORAE_WHEEL_W
                                             - 16.0f);
                float tw2 = sdf_measure_width(nw2, horae__roman[h % 12])
                          * 12.0f;
                draw_set_color(d, dca(0.62f, 0.60f, 0.55f,
                                      cur ? 0.95f : 0.35f));
                draw_text_ex(d, nw2, 12.0f, rx - tw2 * 0.5f, ry - 6.0f,
                             horae__roman[h % 12]);
            }
        }

        // Rim + hub + the gold thresholds (sunrise at u = 0, sunset at
        // u = 12 teeth in): ORTVS and OCCASVS ride the wheel
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.45f));
        draw_circle_stroked(d, 0, wcy, HORAE_WHEEL_R, 1.0f);
        draw_circle_stroked(d, 0, wcy, HORAE_WHEEL_R - HORAE_WHEEL_W, 1.0f);
        for (int e = 0; e < 2; e++) {
            float ue = e ? u[12] : 0.0f;
            float qe = 0.5f - horae__wrap(ue - u_now, 1.0f);
            float ae = qe * 2.0f * (float)M_PI;
            float sx = sinf(ae), sy = -cosf(ae);
            draw_set_color(d, dc_scale(s->sunrise_handle, 0.95f));
            draw_line(d, sx * (HORAE_WHEEL_R - HORAE_WHEEL_W - 8.0f),
                      wcy + sy * (HORAE_WHEEL_R - HORAE_WHEEL_W - 8.0f),
                      sx * (HORAE_WHEEL_R + 8.0f),
                      wcy + sy * (HORAE_WHEEL_R + 8.0f), 1.6f);
        }

        // The seconds hairline at the hub — the pulse that says clock
        double real_secs = s->sweep_seconds
            ? ((double)tv->secs + tv->frac_secs) / 60.0
            : (double)tv->secs / 60.0;
        float sa2 = (float)(real_secs * 2.0 * M_PI);
        draw_set_color(d, s->seconds_color);
        draw_line(d, 0, wcy,
                  sinf(sa2) * (HORAE_WHEEL_R - HORAE_WHEEL_W - 6.0f),
                  wcy - cosf(sa2) * (HORAE_WHEEL_R - HORAE_WHEEL_W - 6.0f),
                  1.5f);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.6f));
        draw_circle_filled(d, 0, wcy, 2.5f);
    }

    // ---- The mesh marker: where the gears agree ----
    {
        draw_set_color(d, s->month_text_color);
        int vb = d->num_verts;
        draw__push_vert(d, 0.0f, HORAE_RING1 + 14.0f,
                        d->white_u, d->white_v);
        draw__push_vert(d, -6.0f, HORAE_RING1 + 26.0f,
                        d->white_u, d->white_v);
        draw__push_vert(d, 6.0f, HORAE_RING1 + 26.0f,
                        d->white_u, d->white_v);
        draw__tri(d, vb, vb + 1, vb + 2);
    }

    // ---- Readout, in the clear above the pinion ----
    {
        const uint8_t *c = orr__body_col[horae__chaldean_body[ridx]];
        draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                              c[2] / 255.0f, 0.95f));
        draw_circle_filled(d, 0, -196.0f, 9.0f);

        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, -162.0f, "HORA");
        draw_set_color(d, dca(0.78f, 0.75f, 0.68f, 0.95f));
        draw_text_centered(d, FONT_month, 0, -134.0f,
                          horae__genitive[ridx]);

        char hb[24];
        snprintf(hb, sizeof(hb), "%s %s", horae__roman[hcur % 12],
                 hcur < 12 ? "DIEI" : "NOCTIS");
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        draw_text_centered(d, FONT_date, 0, -106.0f, hb);
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
