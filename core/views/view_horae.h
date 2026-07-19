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

    // Sky-band colors from the shared scattering atmosphere, cached
    // per date (the band is a map of the whole day — it changes with
    // the season, not the minute)
    float  band_col[193][3];
    double band_jd;
    float  band_rise, band_lat;

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

// The seven rulers' sigils, stroke tables in the instrument's engraved
// idiom (point count then x,y pairs, unit box y-up, 0 ends) — Chaldean
// order to match horae__metal_col: Saturn, Jupiter, Mars, Sun, Venus,
// Mercury, Moon
static const float horae__sg_saturn[] = {
    2, -0.34f,0.24f, -0.10f,0.24f,
    8, -0.22f,0.38f, -0.22f,0, -0.16f,0.10f, -0.04f,0.14f,
       0.08f,0.08f, 0.12f,-0.06f, 0.08f,-0.24f, 0.16f,-0.36f, 0 };
static const float horae__sg_jupiter[] = {
    6, -0.30f,0.16f, -0.24f,0.30f, -0.10f,0.34f, 0.02f,0.26f,
       0.02f,0.10f, -0.26f,-0.14f,
    2, -0.32f,-0.14f, 0.28f,-0.14f,
    2, 0.12f,0.02f, 0.12f,-0.36f, 0 };
static const float horae__sg_mars[] = {
    9, 0.09f,-0.12f, 0.04f,0, -0.08f,0.05f, -0.20f,0, -0.25f,-0.12f,
       -0.20f,-0.24f, -0.08f,-0.29f, 0.04f,-0.24f, 0.09f,-0.12f,
    2, 0.04f,0, 0.26f,0.24f,
    2, 0.26f,0.24f, 0.08f,0.22f,
    2, 0.26f,0.24f, 0.24f,0.06f, 0 };
static const float horae__sg_sun[] = {
    13, 0.24f,0, 0.208f,0.12f, 0.12f,0.208f, 0,0.24f, -0.12f,0.208f,
        -0.208f,0.12f, -0.24f,0, -0.208f,-0.12f, -0.12f,-0.208f,
        0,-0.24f, 0.12f,-0.208f, 0.208f,-0.12f, 0.24f,0,
    2, -0.03f,0, 0.03f,0, 0 };
static const float horae__sg_venus[] = {
    9, 0.17f,0.14f, 0.12f,0.26f, 0,0.31f, -0.12f,0.26f, -0.17f,0.14f,
       -0.12f,0.02f, 0,-0.03f, 0.12f,0.02f, 0.17f,0.14f,
    2, 0,-0.03f, 0,-0.36f,
    2, -0.12f,-0.22f, 0.12f,-0.22f, 0 };
static const float horae__sg_mercury[] = {
    9, 0.15f,0.04f, 0.106f,0.146f, 0,0.19f, -0.106f,0.146f,
       -0.15f,0.04f, -0.106f,-0.066f, 0,-0.11f, 0.106f,-0.066f,
       0.15f,0.04f,
    5, -0.14f,0.34f, -0.07f,0.24f, 0,0.21f, 0.07f,0.24f, 0.14f,0.34f,
    2, 0,-0.11f, 0,-0.36f,
    2, -0.11f,-0.25f, 0.11f,-0.25f, 0 };
static const float horae__sg_moon[] = {
    11, 0.10f,0.30f, -0.06f,0.26f, -0.17f,0.12f, -0.20f,0,
        -0.17f,-0.12f, -0.06f,-0.26f, 0.10f,-0.30f, -0.02f,-0.20f,
        -0.08f,0, -0.02f,0.20f, 0.10f,0.30f, 0 };
static const float *horae__sigil[7] = {
    horae__sg_saturn, horae__sg_jupiter, horae__sg_mars, horae__sg_sun,
    horae__sg_venus, horae__sg_mercury, horae__sg_moon,
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
    // The temporal dial's rotation: its NOW cell rides the weekly
    // hand's direction while the face turns through its day
    float rot = m_now / 7.0f - now;
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
        // One continuous gradient around the band, from the SAME
        // single-scattering atmosphere that lights CAELVM's dome:
        // each hour's color is the sky seen low toward the sun's
        // azimuth at that hour — where the color drama lives. Cached
        // per date; the band is a map of the day, not of the minute.
        {
            const int N = 192;
            HoraeViewState *stw = (HoraeViewState *)(uintptr_t)buf;
            if (fabs(stw->band_jd - tv->jd_current) > 1.0e-6
                || fabsf(stw->band_rise - rise) > 1.0e-4f
                || fabsf(stw->band_lat
                         - (float)t->config.latitude) > 1.0e-3f) {
                stw->band_jd = tv->jd_current;
                stw->band_rise = rise;
                stw->band_lat = (float)t->config.latitude;
                for (int i = 0; i <= N; i++) {
                    float fp = (float)i / N;
                    float H = (fp - fnoon) * 2.0f * (float)M_PI;
                    float salt = sphi * sdec + cphi * cdec * cosf(H);
                    if (salt > 1.0f) salt = 1.0f;
                    if (salt < -1.0f) salt = -1.0f;
                    float sun_alt = asinf(salt) / d2r;
                    // ONE ray per hour, aimed AT THE SUN'S PLACE in
                    // that hour's sky (Seren's fix — no averaging
                    // fixed-altitude rays across the day): noon looks
                    // high into the blue around the sun, sunset looks
                    // along the horizon into the fire, night's ray
                    // grazes a sky the sun no longer reaches. The ray
                    // stays just above the horizon once the sun sets —
                    // you can't look below it — which is exactly what
                    // makes twilight decay instead of snapping dark.
                    float sd2[3], ra[3], ca[3];
                    atmo_dir(0.0f, sun_alt, sd2);
                    float ray_alt = sun_alt < 1.5f ? 1.5f : sun_alt;
                    atmo_dir(0.0f, ray_alt, ra);
                    atmo_scatter(ra, sd2, ca);
                    static const float band_base[3] = { 0.030f, 0.036f,
                                                        0.075f };
                    atmo_tone(ca, 1.0f, band_base,
                              stw->band_col[i]);
                }
            }
            int pi = -1, po = -1;
            for (int i = 0; i <= N; i++) {
                float fp = (float)i / N;
                DrawColor cc = dca(st->band_col[i][0],
                                   st->band_col[i][1],
                                   st->band_col[i][2], 0.92f);
                draw_set_color(d, cc);

                float a = (fp + rot) * 2.0f * (float)M_PI;
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
            float f0 = rise + u[h] + rot;
            float ab = (f0 - floorf(f0)) * 2.0f * (float)M_PI;
            float sx = sinf(ab), sy = -cosf(ab);
            draw_set_color(d, dca(0.95f, 0.94f, 0.90f, 0.45f));
            draw_line(d, sx * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                      sy * (HORAE_CLOCK_R - HORAE_CLOCK_W),
                      sx * HORAE_CLOCK_R, sy * HORAE_CLOCK_R, 1.0f);
        }

        // (The current cell used to wear bright rims here — retired:
        // the boxed highlight fought the sky band's own colors. The
        // hand at the contact already names the hour.)

        // Full archaic: no civil ticks, no arabic — the TEMPORAL cells
        // carry roman numerals, I through XII twice, ENGRAVED ALONG
        // the turning face like dial lettering (waist-centered, flip-
        // compensated). Day hours speak up; night hours murmur.
        {
            float fsz = _font_compat[FONT_clock].size;
            for (int h = 0; h < 24; h++) {
                float pm = rise + (u[h] + u[h + 1]) * 0.5f + rot;
                float ang = pm * 2.0f * (float)M_PI;
                float na2 = fmodf(ang, 2.0f * (float)M_PI);
                if (na2 < 0) na2 += 2.0f * (float)M_PI;
                bool lflip = (na2 > (float)M_PI * 0.5f
                              && na2 < (float)M_PI * 1.5f);
                float nsz2 = h < 12 ? 15.0f : 12.5f;
                float mid = HORAE_CLOCK_R - HORAE_CLOCK_W - 16.0f;
                float lr = mid - nsz2 * 0.5f
                         + nsz2 * (lflip ? 0.51f : 0.37f);
                draw_set_color(d, h < 12
                    ? dca(0.72f, 0.70f, 0.64f, 0.85f)
                    : dca(0.55f, 0.53f, 0.49f, 0.55f));
                draw_text_curved(d, FONT_clock, 0, 0, lr, ang,
                                 horae__roman[h % 12], 0.25f,
                                 nsz2 / fsz);
            }
        }

        // Rims + gold thresholds: ORTVS at sunrise, OCCASVS at sunset
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.45f));
        draw_circle_stroked(d, 0, 0, HORAE_CLOCK_R, 1.0f);
        draw_circle_stroked(d, 0, 0, HORAE_CLOCK_R - HORAE_CLOCK_W, 1.0f);
        for (int e = 0; e < 2; e++) {
            float fe = e ? set : rise;
            float ae = (fe + rot) * 2.0f * (float)M_PI;
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
    // THE PLANETARY TRAIN, final form: the hand makes ONE revolution
    // per WEEK. The week ring is a FIXED dial (the 168 hours laid out
    // once around it) whose center ORBITS the clock — once a week,
    // tracking the hand, tangent always at the contact. The inner
    // temporal dial still spins, counterclockwise through its day, so
    // the current cell meets the current tooth at the moving touch.
    float ah = (m_now / 7.0f) * 2.0f * (float)M_PI;   // the WEEK hand
    float hdx = sinf(ah), hdy = -cosf(ah);
    float rcx = -hdx * HORAE_ECC;                // ring center
    float rcy = -hdy * HORAE_ECC;
    {
        // Seven day regions + engraved names
        float lsz = _font_compat[FONT_date].size;
        for (int i = 0; i < 7; i++) {
            float q0 = (float)i / 7.0f;
            const uint8_t *c = horae__metal_col[horae__day_ruler[i]];
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
                float q0 = m0 / 7.0f;
                float q1 = m1 / 7.0f;

                bool is_day = h < 12;
                float r0 = HORAE_RING_IN + 2.0f;
                float r1 = HORAE_RING_IN + (is_day ? 12.0f : 8.0f);
                // (No gold override on the current tooth — the sky
                // wheel's marker points at it; the band stays pure.)
                // The whole band lays down in the light; dark hours
                // are filled ON it at FULL angular width, inset only
                // radially. Cells share their boundaries exactly, so
                // the black boxes read their true width (the old
                // per-side light outline visibly shrank them).
                draw_set_color(d, dca(0.64f, 0.62f, 0.56f, 0.70f));
                horae__cell(d, rcx, rcy, r0, r1, q0, q1);
                if (((dd * 24 + h) & 1) != 0) {
                    draw_set_color(d, dca(0.03f, 0.03f, 0.03f, 0.95f));
                    horae__cell(d, rcx, rcy, r0 + 1.1f, r1 - 1.1f,
                                q0, q1);
                }
            }
        }

        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
        draw_circle_stroked(d, rcx, rcy, HORAE_RING_IN, 1.0f);
        draw_circle_stroked(d, rcx, rcy, HORAE_RING_OUT, 1.0f);
    }

    // ---- The rulers' pinion ----
    // The tangent idea carried inward: the seven Chaldean rulers as a
    // small wheel meshing the INSIDE of the sky wheel at the same
    // contact the week ring touches outside. The radius is the honest
    // gear ratio — 7 cells against the dial's 24 gives rp = Rn*7/24,
    // equal tooth pitch at the mesh — so it rolls 24/7 turns a day,
    // the ruler of the hour always pressed to the touch, wearing its
    // alchemical metal and sigil.
    {
        float Rn = HORAE_CLOCK_R - HORAE_CLOCK_W;
        float rp = Rn * 7.0f / 24.0f;
        // Held 2 off the band's inner edge — the same breath of
        // gutter the railroad keeps, uniform between all the wheels
        float pcx2 = hdx * (Rn - rp - 2.0f);
        float pcy2 = hdy * (Rn - rp - 2.0f);
        float hfrac = (u_now - u[hcur])
                    / (u[hcur + 1] - u[hcur] + 1.0e-6f);
        float phi = m_now / 7.0f - ((float)ridx + hfrac) / 7.0f;
        // A dark solid plate, its seven sigils, and nothing else: the
        // lozenge points at the mesh and the ruling sigil stands
        // bright — the wheel needs no box or band to say what the
        // marks already say. The plate occludes the numerals it
        // rolls over.
        draw_set_color(d, dca(0.02f, 0.02f, 0.02f, 1.0f));
        draw_circle_filled(d, pcx2, pcy2, rp);
        for (int c = 0; c < 7; c++) {
            bool curp = (c == ridx);
            float sa = ((c + 0.5f) / 7.0f + phi) * 2.0f * (float)M_PI;
            float sux = sinf(sa), suy = -cosf(sa);
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f,
                                  curp ? 0.95f : 0.30f));
            orr__strokes(d, horae__sigil[c], pcx2 + sux * 42.0f,
                         pcy2 + suy * 42.0f, sux, suy, 24.0f, 1.0f);
        }
        // The meshing edge, so the dark wheel holds its shape
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
        draw_circle_stroked(d, pcx2, pcy2, rp, 1.0f);
    }

    // ---- The marker: a double-pointed lozenge on the sky wheel ----
    // No hand. A rhombus centered in the sky band at the contact
    // bearing, its points aiming outward at the week ring's tooth and
    // inward at the pinion's mesh simultaneously — the whole gear
    // train named by one shape that lives ON the wheel it reads.
    // Drawn LAST among the wheels: the marker rides over all of them.
    {
        float px2 = -hdy, py2 = hdx;
        float rmid = HORAE_CLOCK_R - HORAE_CLOCK_W * 0.5f;
        float rout = HORAE_CLOCK_R + 6.5f;
        float rin  = HORAE_CLOCK_R - HORAE_CLOCK_W - 6.5f;
        float half = 7.0f;
        draw_set_color(d, dca(0.82f, 0.79f, 0.71f, 0.95f));
        int vb = d->num_verts;
        draw__push_vert(d, hdx * rout, hdy * rout,
                        d->white_u, d->white_v);
        draw__push_vert(d, hdx * rmid + px2 * half,
                        hdy * rmid + py2 * half,
                        d->white_u, d->white_v);
        draw__push_vert(d, hdx * rin, hdy * rin,
                        d->white_u, d->white_v);
        draw__push_vert(d, hdx * rmid - px2 * half,
                        hdy * rmid - py2 * half,
                        d->white_u, d->white_v);
        draw__tri(d, vb, vb + 1, vb + 2);
        draw__tri(d, vb, vb + 2, vb + 3);
    }

    // ---- The reading, written outside the ring at the contact ----
    // Just the DAY, in full classical dress — DIES LVNAE — steady for
    // a whole revolution of the inner dial. (The hour-ruler genitive
    // and HORA line flickered through their changes at every scrub;
    // the pinion's sigil and the gold tooth already carry the hour.)
    {
        float na = fmodf(ah, 2.0f * (float)M_PI);
        if (na < 0) na += 2.0f * (float)M_PI;
        bool lflip = (na > (float)M_PI * 0.5f && na < (float)M_PI * 1.5f);

        char db[24];
        snprintf(db, sizeof(db), "DIES %s", horae__dies[w]);
        float nsz = _font_compat[FONT_month].size;
        // Baseline arc CONCENTRIC WITH THE WEEK DISC, not the clock:
        // the words ride the wheel they name. Same contact point —
        // the ring center sits ECC behind the origin along the hand,
        // so the radius about it grows by ECC at the same bearing.
        float rn = (346.0f + HORAE_ECC) - nsz * 0.5f
                 + nsz * (lflip ? 0.51f : 0.37f);
        draw_set_color(d, dca(0.80f, 0.77f, 0.70f, 0.95f));
        draw_text_curved(d, FONT_month, rcx, rcy, rn, ah, db,
                         1.2f, 1.0f);

        // (The ruler's pip is retired — the pinion's metal cell and
        // sigil at the mesh already name the ruler; the outer rim
        // belongs to the day's words alone.)
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
