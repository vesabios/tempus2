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

    // The week ring is itself draggable: one revolution is 7/6 of a
    // day — the instrument's fine scrub, feeding the same flywheel
    bool  ring_dragging;
    float last_wx, last_wy;
};

#endif // VIEW_HORAE_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_HORAE_IMPL)
#define VIEW_HORAE_IMPL

// Gear geometry (world units; the eccentric ring's farthest sweep,
// HORAE_ECC + HORAE_RING_OUT + labels, must clear the calendar wheel
// at 434)
#define HORAE_CLOCK_R   230.0f   // the fixed day clock, center stage
#define HORAE_CLOCK_W    28.0f   // its sky band depth
#define HORAE_RING_IN   296.0f   // week ring inner (touches the clock)
#define HORAE_RING_OUT  364.0f   // week ring outer
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

// The seven rulers in their ALCHEMICAL colors — the planet-metal
// scheme of the old plates (the Azoth's seven rays): lead-black
// Saturn, tin-violet Jupiter, iron-red Mars, the yellow ray of Sol,
// Venus in copper's verdigris, quicksilver-tawny Mercury, silver
// Luna. This is the hours' own tradition — the astronomical
// appearance palette stays with the physical views. Chaldean order;
// lead lightens to graphite so its wash reads on a black plate.
static const uint8_t horae__metal_col[7][3] = {
    { 105, 102, 112 },   // SATVRNVS — plumbum
    { 148, 105, 176 },   // IVPPITER — stannum
    { 198,  74,  56 },   // MARS — ferrum
    { 216, 170,  44 },   // SOL — aurum
    {  82, 158, 110 },   // VENVS — cuprum
    { 208, 132,  58 },   // MERCVRIVS — argentum vivum
    { 224, 221, 212 },   // LVNA — argentum
};

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

// The sky's own colors, keyed to the sun's altitude — the Rayleigh
// story told twice over: azure day, the gold and fire of the
// crossings, then the BLUE HOUR (the deep saturated blue after
// sunset), sinking through nautical blue to near-black night. The
// purple the ramp passes between fire and blue hour is real too —
// twilight's "purple light". Rich on purpose: this band is the one
// place the instrument wears the sky at full voice.
// Keyframes interpolate in OKLab, not RGB — a straight RGB lerp
// between saturated hues sags through grey-brown mud; the perceptual
// space keeps every intermediate luminous, which is how the actual sky
// manages its own gradients.
static inline float horae__cbrtf(float x) {
    return x < 0 ? -powf(-x, 1.0f / 3.0f) : powf(x, 1.0f / 3.0f);
}

static void horae__rgb2oklab(const float rgb[3], float lab[3]) {
    float r = powf(rgb[0], 2.2f);
    float g = powf(rgb[1], 2.2f);
    float b = powf(rgb[2], 2.2f);
    float l = horae__cbrtf(0.4122214708f * r + 0.5363325363f * g
                           + 0.0514459929f * b);
    float m = horae__cbrtf(0.2119034982f * r + 0.6806995451f * g
                           + 0.1073969566f * b);
    float sc = horae__cbrtf(0.0883024619f * r + 0.2817188376f * g
                            + 0.6299787005f * b);
    lab[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * sc;
    lab[1] = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * sc;
    lab[2] = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * sc;
}

static DrawColor horae__oklab2rgb(const float lab[3]) {
    float l = lab[0] + 0.3963377774f * lab[1] + 0.2158037573f * lab[2];
    float m = lab[0] - 0.1055613458f * lab[1] - 0.0638541728f * lab[2];
    float sc = lab[0] - 0.0894841775f * lab[1] - 1.2914855480f * lab[2];
    l = l * l * l; m = m * m * m; sc = sc * sc * sc;
    float r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * sc;
    float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * sc;
    float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * sc;
    r = r < 0 ? 0 : powf(r, 1.0f / 2.2f);
    g = g < 0 ? 0 : powf(g, 1.0f / 2.2f);
    b = b < 0 ? 0 : powf(b, 1.0f / 2.2f);
    return dc(fminf(r, 1.0f), fminf(g, 1.0f), fminf(b, 1.0f));
}

static DrawColor horae__sky(float alt) {
    static const float k[8][4] = {
        {  25.0f, 0.30f, 0.55f, 0.95f },   // azure day
        {   6.0f, 0.62f, 0.72f, 0.92f },   // pale horizon blue
        {   1.0f, 1.00f, 0.58f, 0.18f },   // golden hour
        {  -2.0f, 0.95f, 0.30f, 0.14f },   // the crossing: fire
        {  -6.0f, 0.22f, 0.24f, 0.60f },   // the blue hour
        { -11.0f, 0.08f, 0.13f, 0.38f },   // nautical blue
        { -17.0f, 0.03f, 0.05f, 0.18f },   // astronomical
        { -28.0f, 0.015f, 0.02f, 0.075f }, // night
    };
    if (alt >= k[0][0]) return dc(k[0][1], k[0][2], k[0][3]);
    for (int i = 0; i < 7; i++) {
        if (alt >= k[i + 1][0]) {
            float f = (alt - k[i + 1][0]) / (k[i][0] - k[i + 1][0]);
            float la[3], lb[3], lm[3];
            horae__rgb2oklab(&k[i][1], la);
            horae__rgb2oklab(&k[i + 1][1], lb);
            for (int c = 0; c < 3; c++)
                lm[c] = lb[c] + (la[c] - lb[c]) * f;
            return horae__oklab2rgb(lm);
        }
    }
    return dc(k[7][1], k[7][2], k[7][3]);
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

    // Sun-altitude curve for the sky band, from hour angle about the
    // DIAL'S OWN noon (the rise/set midpoint) and today's declination —
    // no timezone anywhere, so it agrees with the gold thresholds by
    // construction: altitude zero exactly where ORTVS and OCCASVS draw.
    float fnoon = (rise + set) * 0.5f;
    float d2r = (float)M_PI / 180.0f;
    float sphi = sinf((float)t->config.latitude * d2r);
    float cphi = cosf((float)t->config.latitude * d2r);
    float sdec = sinf((float)t->solar.delta * d2r);
    float cdec = cosf((float)t->solar.delta * d2r);

    // ---- The day clock, fixed at center stage ----
    // A thin band of the sky itself — every temporal cell colored by
    // the sun's true altitude at that hour, dawn to dusk to deep night
    // — with the 12-hour face's own furniture inside it: the same tick
    // language, the same numeral voice, because that is what this is —
    // a 24-hour clock.
    {
        // One continuous gradient around the band: per-vertex color,
        // sampled from the altitude curve, OKLab between keys — the
        // sky as the sky actually blends
        {
            const int N = 192;
            int pi = -1, po = -1;
            for (int i = 0; i <= N; i++) {
                float fp = (float)i / N;
                float H = (fp - fnoon) * 2.0f * (float)M_PI;
                float salt = sphi * sdec + cphi * cdec * cosf(H);
                if (salt > 1.0f) salt = 1.0f;
                if (salt < -1.0f) salt = -1.0f;
                DrawColor cc = horae__sky(asinf(salt) / d2r);
                cc.a = 0.92f;
                draw_set_color(d, cc);

                float a = fp * 2.0f * (float)M_PI;
                float sx = sinf(a), sy = -cosf(a);
                int vi = draw__push_vert(d,
                    sx * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                    sy * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                    d->white_u, d->white_v);
                int vo = draw__push_vert(d, sx * HORAE_CLOCK_R,
                                         sy * HORAE_CLOCK_R,
                                         d->white_u, d->white_v);
                if (i > 0) {
                    draw__tri(d, pi, po, vi);
                    draw__tri(d, po, vo, vi);
                }
                pi = vi;
                po = vo;
            }
        }

        // Temporal hour boundaries: light segmentations over the flow
        for (int h = 0; h < 24; h++) {
            float f0 = rise + u[h];
            float ab = (f0 - floorf(f0)) * 2.0f * (float)M_PI;
            float sx = sinf(ab), sy = -cosf(ab);
            draw_set_color(d, dca(0.95f, 0.94f, 0.90f, 0.45f));
            draw_line(d, sx * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                      sy * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                      sx * HORAE_CLOCK_R, sy * HORAE_CLOCK_R, 1.0f);
        }

        // The current cell wears bright rims
        {
            float p0 = rise + u[hcur], p1 = rise + u[hcur + 1];
            draw_set_color(d, dca(0.92f, 0.89f, 0.80f, 0.9f));
            horae__cell(d, 0, 0, HORAE_CLOCK_R - 2.0f, HORAE_CLOCK_R,
                        p0, p1);
            horae__cell(d, 0, 0, HORAE_CLOCK_R - HORAE_CLOCK_W,
                        HORAE_CLOCK_R - HORAE_CLOCK_W + 2.0f, p0, p1);
        }

        // Civil hour ticks in the clock's own voice, inside the band
        for (int h = 0; h < 24; h++) {
            float a = (float)h / 24.0f * 2.0f * (float)M_PI;
            float sx = sinf(a), sy = -cosf(a);
            bool major = (h % 3) == 0;
            float outer = HORAE_CLOCK_R - HORAE_CLOCK_W - 4.0f;
            float inner = outer - (major ? 22.0f : 12.0f);
            draw_set_color(d, major ? s->clock_lines_strong
                                    : s->clock_lines);
            draw_line_thin(d, sx * outer, sy * outer,
                           sx * inner, sy * inner);
        }

        // Numerals at the even hours, the clock face's numeral voice
        {
            int cw2 = _font_compat[FONT_clock].weight;
            for (int h = 0; h < 24; h += 2) {
                float a = (float)h / 24.0f * 2.0f * (float)M_PI;
                float rn = HORAE_CLOCK_R - HORAE_CLOCK_W - 48.0f;
                float rx = sinf(a) * rn, ry = -cosf(a) * rn;
                char nb[3];
                snprintf(nb, sizeof(nb), "%d", h);
                float nsz2 = 20.0f;
                float tw2 = sdf_measure_width(cw2, nb) * nsz2;
                draw_set_color(d, s->clock_lines_strong);
                draw_text_ex(d, cw2, nsz2, rx - tw2 * 0.5f,
                             ry - nsz2 * 0.5f, nb);
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
            float q0 = now - horae__wrap((float)i - m_now, 7.0f) / 7.0f;
            const uint8_t *c = horae__metal_col[horae__day_ruler[i]];
            bool today = i == w;
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, today ? 0.16f : 0.06f));
            horae__cell(d, rcx, rcy, HORAE_RING_IN, HORAE_RING_OUT,
                        q0 - 1.0f / 7.0f, q0);

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
                float qc = q0 - 0.5f / 7.0f;
                float ang = qc * 2.0f * (float)M_PI;
                float na = fmodf(ang, 2.0f * (float)M_PI);
                if (na < 0) na += 2.0f * (float)M_PI;
                bool lflip = (na > (float)M_PI * 0.5f
                              && na < (float)M_PI * 1.5f);
                float mid = (HORAE_RING_IN + HORAE_RING_OUT) * 0.5f
                          + 12.0f;   // clear of the meshing ticks
                float lr = mid - lsz * 0.5f
                         + lsz * (lflip ? 0.51f : 0.37f);
                draw_set_color(d, today
                    ? dca(0.80f, 0.77f, 0.70f, 0.95f)
                    : dca(0.55f, 0.53f, 0.49f, 0.45f));
                draw_text_curved(d, FONT_date, rcx, rcy, lr, ang,
                                 horae__dies[i], 0.8f, 1.0f);
            }
        }

        // The 168 hours as an alternating light/dark chapter band on
        // the meshing edge — the chemin de fer of the week (168 is
        // even, so the alternation closes seamlessly around the loop).
        // Day hours stand taller than night hours; the current one
        // wears gold at the touch.
        for (int dd = 0; dd < 7; dd++) {
            for (int h = 0; h < 24; h++) {
                float m0 = dd + rise + u[h];
                float m1 = dd + rise + u[h + 1];
                float q0 = now - horae__wrap(m0 - m_now, 7.0f) / 7.0f;
                float q1 = now - horae__wrap(m1 - m_now, 7.0f) / 7.0f;
                if (q0 < q1) continue;   // the seam cell opposite now

                bool is_day = h < 12;
                bool cur = (dd == pd && h == hcur);
                float r0 = HORAE_RING_IN + 2.0f;
                float r1 = HORAE_RING_IN + (is_day ? 12.0f : 8.0f);
                if (cur) {
                    draw_set_color(d, dc_scale(s->sunrise_handle, 1.15f));
                    horae__cell(d, rcx, rcy, r0, HORAE_RING_IN + 15.0f,
                                q1, q0);
                } else if (((dd * 24 + h) & 1) == 0) {
                    draw_set_color(d, dca(0.64f, 0.62f, 0.56f, 0.70f));
                    horae__cell(d, rcx, rcy, r0, r1, q1, q0);
                } else {
                    // Black cells outline in the light so they read
                    // against the black ground: light underlay, dark
                    // fill inset a hairline
                    const float eps = 0.0007f;   // ~1.3 units at r 300
                    draw_set_color(d, dca(0.64f, 0.62f, 0.56f, 0.70f));
                    horae__cell(d, rcx, rcy, r0, r1, q1, q0);
                    draw_set_color(d, dca(0.03f, 0.03f, 0.03f, 0.95f));
                    horae__cell(d, rcx, rcy, r0 + 1.1f, r1 - 1.1f,
                                q1 + eps, q0 - eps);
                }
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
        // The hands hold off the center, as on the 12-hour face — the
        // hub is negative space
        float h0 = 46.0f, h1 = HORAE_CLOCK_R - 10.0f;
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
        draw_line(d, sinf(sa2) * 46.0f, -cosf(sa2) * 46.0f,
                  sinf(sa2) * (HORAE_CLOCK_R - HORAE_CLOCK_W - 10.0f),
                  -cosf(sa2) * (HORAE_CLOCK_R - HORAE_CLOCK_W - 10.0f),
                  1.5f);
    }

    // ---- The reading, written outside the ring at the contact ----
    // The hand points through the mesh to the ruling planet; the words
    // follow the touch around the dial.
    {
        const uint8_t *c = horae__metal_col[ridx];
        float na = fmodf(ah, 2.0f * (float)M_PI);
        if (na < 0) na += 2.0f * (float)M_PI;
        bool lflip = (na > (float)M_PI * 0.5f && na < (float)M_PI * 1.5f);

        // Planet name on its own arc, waist-centered
        float nsz = _font_compat[FONT_month].size;
        float rn = 346.0f - nsz * 0.5f + nsz * (lflip ? 0.51f : 0.37f);
        draw_set_color(d, dca(0.80f, 0.77f, 0.70f, 0.95f));
        draw_text_curved(d, FONT_month, 0, 0, rn, ah,
                         horae__genitive[ridx], 1.2f, 1.0f);

        // The ruler's pip sits directly ON the week ring's outer rim
        // at the contact, a bead threaded on the line — hand, pip,
        // name, hour: one radial procession
        {
            float pr = HORAE_RING_OUT - HORAE_ECC;
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, 0.95f));
            draw_circle_filled(d, hdx * pr, hdy * pr, 6.5f);
        }

        // The hour line on the arc beyond
        char hb[28];
        snprintf(hb, sizeof(hb), "HORA %s %s", horae__roman[hcur % 12],
                 hcur < 12 ? "DIEI" : "NOCTIS");
        float isz = _font_compat[FONT_date].size * 0.9f;
        float ri = 386.0f - isz * 0.5f + isz * (lflip ? 0.51f : 0.37f);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.60f));
        draw_text_curved(d, FONT_date, 0, 0, ri, ah, hb, 0.8f, 0.9f);
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
