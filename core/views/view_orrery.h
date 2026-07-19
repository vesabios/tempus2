// view_orrery.h — The earth instrument, at every worldview.
//
// This view is the SINGLE renderer for the globe and everything attached
// to it, across the whole geocentric <-> heliocentric morph (including
// both pure endpoints). The solar view computes data (SPA, caches) but
// draws nothing — single ownership means draw order is decided once,
// here, and handoff artifacts are structurally impossible.
//
// Element groups:
//   dial group  (alpha 1-m): dial plate, outer ring, daylight arc —
//                            anchored at the dial position, engraved panel
//   helio group (alpha m):   sun, orbit radial, axis pin, 24h bezel
//   shared      (alpha 1):   globe, sun marker + hand line, city marker,
//                            sky-dome — continuous across the morph by
//                            construction (their geo and helio forms
//                            coincide at m = 0)

#ifndef VIEW_ORRERY_H
#define VIEW_ORRERY_H

#include "../view.h"
#include "../planets.h"
#include "../../assets/coastlines.h"

struct OrreryViewState {
    TimeView tv;  // must be first field
    double opacity;
    double zoom;        // mirrored from the calendar wheel's zoom
    double blend;       // mirrored scene helio_blend (morph parameter)
    double sys;         // mirrored scene system_blend (full-system stage)
    double skyb;        // mirrored scene sky_blend (CAELVM handoff)
    double orbisb;      // mirrored scene orbis_blend (globe closeup)
    double geo_azimuth; // live sun az/zen from the solar view, for the
    double geo_zenith;  // geocentric endpoint of the morph
    const SolarViewState *solar;  // solar data + sun-path caches

    // Published by render for hit-testing: the sun bead is draggable in
    // helio mode (rotating the sun's direction = choosing the date)
    float bead_x, bead_y;   // bead position, world coords
    float bead_hit;         // hit radius
    float bead_r;           // drawn size (CAELVM movers seam to it live)
    float moon_x, moon_y;   // live moon center + size, same purpose
    float moon_r;
    float glob_x, glob_y;   // globe center, world coords
    float glob_r;           // globe radius (wheel drags exclude the globe)
    float glob_rot[16];     // live earth->view rotation (ORBIS city pips)

    // Luminary parameters, published for VIEW_LVMEN — the single
    // renderer of the sun and moon, drawn above machine and sky alike.
    // Composed HERE (this view knows every station's forms and blends
    // toward the sky's published chart targets), drawn THERE, once.
    float lum_sun_x, lum_sun_y, lum_sun_r;
    float lum_sun_col[3];
    float lum_sun_ray;        // corona/ray strength
    float lum_moon_x, lum_moon_y, lum_moon_r;
    float lum_moon_rot[16];
    float lum_moon_light[3];
    float lum_moon_aux[4];
    float lum_moon_a;
    float lum_pq[4];
    bool  lum_pq_valid;
    const SkyViewState *skyv;
    const DracoViewState *drav;
    const AstroViewState *astv;
    double drab;        // mirrored scene draco_blend (DRACO handoff)
    double astb;        // mirrored scene astro_blend (chart handoff)
    double w_dial;      // station-weight sum of the dial family
    double w_globe;     // station-weight sum of the globe family

    // Stage 3: the planets' composed member rows (PL_* index; Earth
    // unused), published for VIEW_LVMEN — the body renderer — and
    // for the sky's name labels. pl_mx/my is the LIVE machine ring
    // seat, stashed by the machine layout each frame it runs.
    float pl_mx[PL_COUNT], pl_my[PL_COUNT];
    float pl_cx[PL_COUNT], pl_cy[PL_COUNT];   // composed position
    float pl_cr[PL_COUNT];                    // composed radius
    float pl_ca[PL_COUNT];                    // composed alpha
    float pl_col[PL_COUNT][4];                // composed rgba
    float pl_ring_a[PL_COUNT];                // Saturn's machine ring
    uint8_t pl_stroke[PL_COUNT];              // chart: stroked ring
    bool  dragging;
    bool  drag_earth;       // system view: dragging Earth around its orbit

    // Orientation-morph continuity state (see globe_rot_slerp_cont)
    float earth_pq[4];
    bool  earth_pq_valid;
    float moon_pq[4];
    bool  moon_pq_valid;
    // Analemma on the surface: the subsolar point at this same clock
    // time across the year (73 five-day steps)
    float  ana_vec[74][3];
    double ana_jd;

    // Full-system ephemeris (recomputed when the clock minute moves)
    PlanetsNow planets;
    PlanetsSky sky;
    Aspect     aspects[PLANETS_MAX_ASPECTS];
    int        num_aspects;
    double     planets_jd;

    // Coastline unit vectors (earth frame), precomputed at init
    float coast_vec[COAST_NUM_PTS][3];
};

#endif // VIEW_ORRERY_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_ORRERY_IMPL)
#define VIEW_ORRERY_IMPL

// ---- Full-system layout ----
// Not distance-scale — TEMPO-scale. This machine is a clock, so its
// rings are laid out by time: a planet's radius is linear in the LOG
// OF ITS ORBITAL PERIOD, the slow wanderers ringed farther out by how
// much slower they run. (By Kepler's third law this is log-distance
// too, up to a constant — but the period is what the instrument
// actually shows.) Earth's orbit IS the calendar wheel (1.0), which
// splits the rule into two registers with their own step-per-decade:
//   inner: Mercury (88d) anchors at 0.36 of the wheel (clear of the
//          moon's ring), Venus (225d) falls at 0.782 by the rule;
//   outer: Mars (1.9y) anchors at wheel+150 (the month text needs
//          ~130 units whatever the wheel does), Pluto (248y) at
//          wheel+300 (the moat edge), and Jupiter through Neptune
//          fall at +207/+235/+267/+287 by the rule — the rings
//          crowding toward Pluto as each decade of period compresses.
// The gap from Pluto's ring to the zodiac dial stays deliberate
// negative space — the moat between the heliocentric machine and the
// geocentric dial, crossed only by the sight-lines.
static const float orr__inner_frac[2] = { 0.36f, 0.782f };

static inline float orr__orbit_r(int p, float wheel_R) {
    static const float outer_off[6] = {
        150.0f, 207.0f, 235.0f, 267.0f, 287.0f, 300.0f };
    if (p < PL_EARTH) return wheel_R * orr__inner_frac[p];
    if (p == PL_EARTH) return wheel_R;
    return wheel_R + outer_off[p - PL_MARS];
}

#define ORBIS_GLOBE_R   355.0f   // the location-picker closeup size

#define ORR_WEB_R       930.0f   // aspect chords + geocentric markers
#define ORR_ZODIAC_IN   946.0f
#define ORR_ZODIAC_OUT  980.0f
#define ORR_ZODIAC_TEXT 999.0f

// Beads: ordered by real size but nowhere near to scale
static const struct { const char *name; float size; uint8_t r, g, b; }
orr__planet[PL_COUNT] = {
    { "MERCURY",  5.0f, 152, 146, 138 },
    { "VENUS",    8.0f, 208, 183, 130 },
    { 0,          0.0f,   0,   0,   0 },   // Earth is the globe itself
    { "MARS",     6.5f, 188,  92,  58 },
    { "JUPITER", 15.0f, 196, 168, 126 },
    { "SATURN",  12.5f, 205, 190, 146 },
    { "URANUS",   9.5f, 126, 172, 178 },
    { "NEPTUNE",  9.0f,  98, 128, 188 },
    { "PLUTO",    3.5f, 138, 128, 132 },
};

// Geocentric marker colors, BODY_* order (luminaries + the planets)
static const uint8_t orr__body_col[BODY_COUNT][3] = {
    { 196, 126,  16 },   // Sun — the instrument's gold
    { 205, 202, 192 },   // Moon
    { 152, 146, 138 }, { 208, 183, 130 }, { 188,  92,  58 },
    { 196, 168, 126 }, { 205, 190, 146 }, { 126, 172, 178 },
    {  98, 128, 188 }, { 138, 128, 132 },
};

// Aspect chord colors: hard aspects in oxidized red, soft in the
// instrument's teal, the quincunx off on its own uneasy violet
// Saturated, deliberately APART from the planet palette (the muted
// body colors ride the sight-line curves; the aspect web speaks in
// pure hues): hard aspects burn crimson, soft aspects vivid emerald,
// the conjunction white-gold, the quincunx electric violet.
static const uint8_t orr__aspect_col[ASPECT_TYPE_COUNT][3] = {
    [ASPECT_CONJUNCTION]    = { 255, 228, 160 },
    [ASPECT_OPPOSITION]     = { 235,  40,  50 },
    [ASPECT_TRINE]          = {   0, 210, 140 },
    [ASPECT_SQUARE]         = { 235,  40,  50 },
    [ASPECT_SEXTILE]        = {  40, 205, 175 },
    [ASPECT_SEMISEXTILE]    = {  50, 160, 135 },
    [ASPECT_SEMISQUARE]     = { 200,  60,  55 },
    [ASPECT_SESQUIQUADRATE] = { 200,  60,  55 },
    [ASPECT_QUINCUNX]       = { 175,  90, 235 },
};

// ---- Zodiac sigils ----
// The traditional glyphs, engraved as polyline strokes like the rest
// of the instrument (no raster text). Unit box: x right, y up,
// extents ~±0.46. Each table is a run of polylines — point count,
// then that many x,y pairs — terminated by 0.
static const float orr__gl_aries[] = {
    7, 0,-0.50f, -0.05f,-0.20f, -0.13f,0.05f, -0.22f,0.26f,
       -0.33f,0.38f, -0.42f,0.33f, -0.46f,0.18f,
    7, 0,-0.50f, 0.05f,-0.20f, 0.13f,0.05f, 0.22f,0.26f,
       0.33f,0.38f, 0.42f,0.33f, 0.46f,0.18f, 0 };
static const float orr__gl_taurus[] = {
    13, 0.27f,-0.16f, 0.23f,-0.03f, 0.14f,0.07f, 0,0.11f,
        -0.14f,0.07f, -0.23f,-0.03f, -0.27f,-0.16f, -0.23f,-0.30f,
        -0.14f,-0.39f, 0,-0.43f, 0.14f,-0.39f, 0.23f,-0.30f,
        0.27f,-0.16f,
    7, -0.38f,0.40f, -0.30f,0.26f, -0.17f,0.15f, 0,0.11f,
       0.17f,0.15f, 0.30f,0.26f, 0.38f,0.40f, 0 };
static const float orr__gl_gemini[] = {
    2, -0.17f,-0.34f, -0.17f,0.34f,
    2, 0.17f,-0.34f, 0.17f,0.34f,
    5, -0.36f,0.44f, -0.18f,0.36f, 0,0.34f, 0.18f,0.36f, 0.36f,0.44f,
    5, -0.36f,-0.44f, -0.18f,-0.36f, 0,-0.34f, 0.18f,-0.36f,
       0.36f,-0.44f, 0 };
static const float orr__gl_cancer[] = {
    9, -0.11f,0.16f, -0.15f,0.25f, -0.24f,0.29f, -0.33f,0.25f,
       -0.37f,0.16f, -0.33f,0.07f, -0.24f,0.03f, -0.15f,0.07f,
       -0.11f,0.16f,
    4, -0.24f,0.29f, -0.02f,0.34f, 0.20f,0.29f, 0.36f,0.16f,
    9, 0.11f,-0.16f, 0.15f,-0.25f, 0.24f,-0.29f, 0.33f,-0.25f,
       0.37f,-0.16f, 0.33f,-0.07f, 0.24f,-0.03f, 0.15f,-0.07f,
       0.11f,-0.16f,
    4, 0.24f,-0.29f, 0.02f,-0.34f, -0.20f,-0.29f, -0.36f,-0.16f, 0 };
static const float orr__gl_leo[] = {
    9, -0.15f,-0.24f, -0.19f,-0.16f, -0.27f,-0.12f, -0.35f,-0.16f,
       -0.39f,-0.24f, -0.35f,-0.33f, -0.27f,-0.36f, -0.19f,-0.33f,
       -0.15f,-0.24f,
    12, -0.27f,-0.12f, -0.26f,0.08f, -0.16f,0.28f, 0,0.38f,
        0.16f,0.34f, 0.25f,0.20f, 0.26f,0.02f, 0.17f,-0.16f,
        0.13f,-0.30f, 0.19f,-0.42f, 0.32f,-0.44f, 0.42f,-0.36f, 0 };
static const float orr__gl_virgo[] = {
    2, -0.44f,0.30f, -0.44f,-0.38f,
    5, -0.44f,0.16f, -0.38f,0.30f, -0.28f,0.30f, -0.22f,0.16f,
       -0.22f,-0.38f,
    5, -0.22f,0.16f, -0.16f,0.30f, -0.06f,0.30f, 0,0.16f, 0,-0.38f,
    5, 0,0.16f, 0.06f,0.30f, 0.16f,0.30f, 0.22f,0.16f, 0.22f,-0.10f,
    6, 0.22f,-0.10f, 0.30f,-0.26f, 0.40f,-0.34f, 0.36f,-0.44f,
       0.22f,-0.44f, 0.10f,-0.36f, 0 };
static const float orr__gl_libra[] = {
    2, -0.42f,-0.34f, 0.42f,-0.34f,
    9, -0.42f,-0.12f, -0.20f,-0.12f, -0.18f,0.06f, -0.10f,0.18f,
       0,0.22f, 0.10f,0.18f, 0.18f,0.06f, 0.20f,-0.12f,
       0.42f,-0.12f, 0 };
static const float orr__gl_scorpio[] = {
    2, -0.46f,0.30f, -0.46f,-0.38f,
    5, -0.46f,0.16f, -0.40f,0.30f, -0.31f,0.30f, -0.25f,0.16f,
       -0.25f,-0.38f,
    5, -0.25f,0.16f, -0.19f,0.30f, -0.10f,0.30f, -0.04f,0.16f,
       -0.04f,-0.24f,
    5, -0.04f,-0.24f, 0.02f,-0.38f, 0.16f,-0.42f, 0.30f,-0.36f,
       0.38f,-0.24f,
    2, 0.38f,-0.24f, 0.24f,-0.26f,
    2, 0.38f,-0.24f, 0.36f,-0.40f, 0 };
static const float orr__gl_sagittarius[] = {
    2, -0.34f,-0.36f, 0.40f,0.38f,
    2, 0.40f,0.38f, 0.12f,0.34f,
    2, 0.40f,0.38f, 0.36f,0.10f,
    2, -0.26f,0.02f, 0,-0.24f, 0 };
static const float orr__gl_capricorn[] = {
    3, -0.46f,0.34f, -0.32f,-0.08f, -0.20f,0.30f,
    10, -0.20f,0.30f, -0.12f,0.02f, -0.06f,-0.20f, 0.04f,-0.38f,
        0.20f,-0.44f, 0.34f,-0.38f, 0.38f,-0.24f, 0.30f,-0.10f,
        0.16f,-0.08f, 0.06f,-0.16f, 0 };
static const float orr__gl_aquarius[] = {
    7, -0.42f,0.10f, -0.28f,0.30f, -0.14f,0.10f, 0,0.30f,
       0.14f,0.10f, 0.28f,0.30f, 0.42f,0.10f,
    7, -0.42f,-0.32f, -0.28f,-0.12f, -0.14f,-0.32f, 0,-0.12f,
       0.14f,-0.32f, 0.28f,-0.12f, 0.42f,-0.32f, 0 };
static const float orr__gl_pisces[] = {
    7, -0.10f,0.44f, -0.24f,0.36f, -0.32f,0.18f, -0.34f,0,
       -0.32f,-0.18f, -0.24f,-0.36f, -0.10f,-0.44f,
    7, 0.10f,0.44f, 0.24f,0.36f, 0.32f,0.18f, 0.34f,0,
       0.32f,-0.18f, 0.24f,-0.36f, 0.10f,-0.44f,
    2, -0.30f,0, 0.30f,0, 0 };
static const float *orr__glyphs[12] = {
    orr__gl_aries, orr__gl_taurus, orr__gl_gemini, orr__gl_cancer,
    orr__gl_leo, orr__gl_virgo, orr__gl_libra, orr__gl_scorpio,
    orr__gl_sagittarius, orr__gl_capricorn, orr__gl_aquarius,
    orr__gl_pisces,
};

// Draw a stroke table at (cx,cy): (ux,uy) is glyph-up in screen
// space — pointed radially outward, every glyph stands feet-to-center
// like a clock face's engravings. Screen y runs down, so
// right = (-uy, ux) keeps the figure unmirrored. Shared by the zodiac
// sigils here and the labors of the months at OFFICIVM.
static void orr__strokes(DrawCtx *d, const float *g, float cx, float cy,
                         float ux, float uy, float sz, float w) {
    float rx = -uy, ry = ux;
    int i = 0;
    while (g[i] > 0.5f) {
        int n = (int)g[i++];
        float lx = 0, ly = 0;
        for (int k = 0; k < n; k++) {
            float gx = g[i++], gy = g[i++];
            float sx = cx + (rx * gx + ux * gy) * sz;
            float sy = cy + (ry * gx + uy * gy) * sz;
            if (k) draw_line(d, lx, ly, sx, sy, w);
            lx = sx; ly = sy;
        }
    }
}

static void orr__zodiac_glyph(DrawCtx *d, int sign, float cx, float cy,
                              float ux, float uy, float sz) {
    orr__strokes(d, orr__glyphs[sign], cx, cy, ux, uy, sz, 1.1f);
}

// Pip radius from apparent magnitude: how bright a body looks in the
// real sky is how big its dial marker draws. Clamped so the sun and
// moon don't flood the ring and Pluto stays a defiant speck.
static inline float orr__pip_r(double mag) {
    float r = 5.2f - 0.55f * (float)mag;
    if (r < 1.4f) r = 1.4f;
    if (r > 8.5f) r = 8.5f;
    return r;
}

// Dashed straight line — the aspect chords wear this so they read
// distinct from the solid curved sight-lines at a glance
static inline void orr__dashed_line(DrawCtx *d, float x0, float y0,
                                    float x1, float y1, float w,
                                    float dash, float gap) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0e-3f) return;
    float ux = dx / len, uy = dy / len;
    float t = 0.0f;
    while (t < len) {
        float e = t + dash;
        if (e > len) e = len;
        draw_line(d, x0 + ux * t, y0 + uy * t,
                  x0 + ux * e, y0 + uy * e, w);
        t = e + gap;
    }
}

// Screen direction of an ecliptic-of-date longitude. The wheel anchors
// yule (December solstice) at screen-top, where the sun's geocentric
// longitude is exactly 270 and the sun (at wheel center) is seen from
// Earth looking screen-down — so lambda 270 maps to down, and longitude
// increases clockwise with the year. Wheel angle = (0.75 + lam/360) * 2pi.
static inline void orr__ecl_dir(double lon_deg, float *dx, float *dy) {
    double a = (0.75 + lon_deg / 360.0) * 2.0 * M_PI;
    *dx = (float)sin(a);
    *dy = (float)-cos(a);
}

static void orrery_init(void *buf, const Tempus *t, const RenderStyle *s) {
    OrreryViewState *st = (OrreryViewState *)buf;
    st->opacity = 1.0;
    st->planets_jd = -1.0e9;
    for (int i = 0; i < COAST_NUM_PTS; i++) {
        double lat = coast_pts[i][0] * 0.01 * M_PI / 180.0;
        double lon = coast_pts[i][1] * 0.01 * M_PI / 180.0;
        st->coast_vec[i][0] = (float)(cos(lat) * cos(lon));
        st->coast_vec[i][1] = (float)(cos(lat) * sin(lon));
        st->coast_vec[i][2] = (float)(sin(lat));
    }
    (void)t; (void)s;
}

static void orrery_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void orrery_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void orrery_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    OrreryViewState *st = (OrreryViewState *)buf;
    (void)t; (void)dt;
    st->zoom = sc->calendar_state.zoom;   // ride the wheel's zoom
    st->blend = sc->helio_blend;
    st->sys = sc->system_blend;
    st->skyb = sc->sky_blend;
    st->orbisb = sc->orbis_blend;
    st->skyv = &sc->sky_state;
    st->drav = &sc->draco_state;
    st->drab = sc->draco_blend;
    st->astv = &sc->astro_state;
    st->astb = sc->astro_blend;
    // The station weight vector, folded into the two families the
    // machine's own seats know: dial furniture vs globe attachment
    st->w_dial  = sc->stw[ST_HOROLOGIVM] + sc->stw[ST_HORAE]
                + sc->stw[ST_ROTAE] + sc->stw[ST_SAECVLVM]
                + sc->stw[ST_OFFICIVM];
    st->w_globe = sc->stw[ST_TELLVS] + sc->stw[ST_MACHINA]
                + sc->stw[ST_ORBIS];
    st->geo_azimuth = sc->solar_state.azimuth;
    st->geo_zenith = sc->solar_state.zenith;
    st->solar = &sc->solar_state;

    // Full-system ephemeris: clock time is local, the sky runs on UT.
    // Minute granularity — the fastest body (the Moon) moves 0.009
    // deg/minute, far below a hairline at dial scale. Computed at any
    // heliocentric station, not just the full system: the moon's
    // orbital direction uses the zodiac frame everywhere (one object,
    // one frame — no flip between TELLVS and MACHINA).
    if (st->sys > 0.001 || st->blend > 0.001) {
        double jd_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                     - t->config.timezone / 24.0;
        if (fabs(jd_ut - st->planets_jd) > 1.0 / 1440.0) {
            planets_compute(&st->planets, jd_ut);
            // The horizon layer (moat shading, ascendant, observability)
            // is pinned to the coming SOLAR MIDNIGHT of the displayed
            // date, matching CAELVM: "tonight's sky half", stable per
            // date and creeping with the season — not the fast daily
            // sweep, which mostly reports daytime, when none of this is
            // viewable anyway. Positions and aspects stay live.
            {
                double jd_mid = st->tv.jd_current + 0.5
                              - t->config.longitude / (15.0 * 24.0);
                PlanetsNow mid;
                planets_compute(&mid, jd_mid);
                planets_sky_compute(&st->sky, &mid,
                                    t->config.latitude,
                                    t->config.longitude);
            }
            st->num_aspects = planets_find_aspects(st->planets.geo_lon,
                                                   st->aspects,
                                                   PLANETS_MAX_ASPECTS);
            st->planets_jd = jd_ut;
        }
    }
}

static void orrery_render(const void *buf, DrawCtx *d, const Tempus *t,
                          const RenderStyle *s) {
    const OrreryViewState *st = (const OrreryViewState *)buf;
    // Writable alias for render-side caches (hit-test publishing and
    // orientation-morph continuity)
    OrreryViewState *stw = (OrreryViewState *)(uintptr_t)buf;
    const TimeView *tv = &st->tv;
    float base_alpha = d->alpha;
    // At CAELVM the machine is invisible (opacity floored just above
    // zero so this render still runs to compose the luminaries) —
    // skip the vertex-heavy passes outright.
    bool machine_vis = base_alpha > 0.004f;

    float m = (float)st->blend;
    if (m < 0) m = 0;
    if (m > 1) m = 1;
    float geo_a = 1.0f - m;
    float helio_a = m;

    float dial_y = s->sunrise_dial_offset;
    float dial_r = s->sunrise_dial_radius;

    // Wheel geometry (zoom rides the same tween as the morph; the wheel
    // cedes radius to the system stage)
    float z = (float)st->zoom;
    float base_w = s->calendar_base_radius
                 * (float)tempus_wheel_scale(st->sys);
    float wheel_r = base_w + z * s->zoom_in_radius;
    double phi = tempus_year_pct(t) * 2.0 * M_PI;
    float sphi = sinf((float)phi), cphi = cosf((float)phi);
    float off_r = wheel_r - base_w;
    float sun_x = -sphi * off_r, sun_y = cphi * off_r;

    // ---- Morph state: position/size lerp to the FINAL helio arrangement
    // (zoom = 1: centered, full size — the zoom tween lands together with
    // m, so the endpoints match), camera slerp, and the sun direction
    // carried in the EARTH frame so the terminator stays glued.
    double clock_hours = tv->percent_of_day * 24.0;
    float helio_rot[16], helio_light[3];
    globe_orrery(phi, clock_hours, t->config.timezone, helio_rot, helio_light);

    float rot[16], light[3];
    float ex, ey, earth_r;
    if (m >= 0.999f) {
        memcpy(rot, helio_rot, sizeof(rot));
        ex = sphi * base_w * (1.0f - z);
        ey = -cphi * base_w * (1.0f - z);
        earth_r = 42.0f + z * 198.0f;   // full-zoom helio size: 240
    } else {
        float geo_rot[16];
        globe_rotation(t->config.latitude, t->config.longitude, geo_rot);

        // Mid-morph target: the helio arrangement AT THE CURRENT ZOOM
        // (centered planet at z = 1, bead on the wheel at z = 0), so the
        // flight lands wherever the zoom currently is — geo -> system
        // flies directly onto the wheel without a detour through center.
        float hx = sphi * base_w * (1.0f - z);
        float hy = -cphi * base_w * (1.0f - z);
        float hr = 42.0f + z * 198.0f;
        ex = hx * m;
        ey = dial_y * (1 - m) + hy * m;
        earth_r = dial_r * (1 - m) + hr * m;

        globe_rot_slerp_cont(geo_rot, helio_rot, m,
                             stw->earth_pq, &stw->earth_pq_valid, rot);
    }

    // The light is the SUN IN THE EARTH FRAME (declination, RA minus
    // sidereal time) rotated by the LIVE view — glued to the planet
    // through every morph. The old per-station frame lights nlerped
    // against a slerping rotation and cut the corner: the sun slid
    // cartesian across the view while the globe turned polar beneath
    // it. Rotate WITH the planet and the sun holds its relative seat.
    {
        double slon = (t->solar.alpha - t->solar.nu) * M_PI / 180.0;
        double sla = t->solar.delta * M_PI / 180.0;
        float sun_earth[3] = {
            (float)(cos(sla) * cos(slon)),
            (float)(cos(sla) * sin(slon)),
            (float)(sin(sla)),
        };
        globe_mat_mul_vec(rot, sun_earth, light);
    }

    // ---- ORBIS: the location picker is THIS globe, up close ----
    // The same object flies to center and grows to picking size; the geo
    // orientation is already observer-face-on, so whatever lands under
    // the fixed reticle IS the configured location. The dial and helio
    // furniture around it bow out; the earth remains.
    float ob = (float)st->orbisb;
    if (ob < 0) ob = 0;
    if (ob > 1) ob = 1;
    float obq1 = 1.0f - ob;
    float obf = 1.0f - obq1 * obq1 * obq1 * obq1;
    // Display instant in UT (same formula as the ephemeris cache) —
    // keys the analemma cache
    double orbis_jd = tv->jd_current + tv->percent_of_day - 0.5
                    - t->config.timezone / 24.0;
    if (ob > 0.001f) {
        // Slow-release closeup: composed with a station flight, a
        // linear release lets the closeup collapse faster than the base
        // morph grows the destination globe (the base radius stays
        // dial-small through the middle of the flight), so the earth
        // dips under its final size and settles back up — the sun's
        // overshoot problem in another coat. A quartic ease-out holds
        // the closeup while the flight catches up, with BOUNDED
        // endpoint derivatives (unlike sqrt) so the composed path
        // parks together with the tween. Monotone from every origin.
        //
        // Position and radius ride the SAME curve: the sphere inflates
        // and travels as one motion. From the dial, that pins the
        // bottom edge near its seat and grows the globe upward into
        // place — radius on its own clock left a full-size sphere
        // parked at the dial, bulging out of frame before it slid up.
        ex *= (1.0f - obf);
        ey *= (1.0f - obf);
        earth_r += (ORBIS_GLOBE_R - earth_r) * obf;
        geo_a   *= (1.0f - ob);
        helio_a *= (1.0f - ob);
    }

    // ================= UNDER THE GLOBE =================

    // ---- The system: every planet, the zodiac dial, the aspect web ----
    // Staggered arrival as the camera pulls back: orbit rings first, then
    // the planets, the zodiac, the sight-lines, and finally the web — the
    // astrology arrives after the astronomy that explains it.
    float ss = (float)st->sys;
    // Declutter for the system stage: zoomed out, the emphasis is
    // celestial geometry — the globe keeps only its lit-ness (terminator).
    // Everything painted ON it (continents, latitude ring, city marker,
    // sky-dome, surface clock, axis pin, marker tether) fades with sysf.
    float sysf = ink_out(INK_SYS_DIAL, ss);
    // ONE OBJECT, ONE OWNER: the moment the CAELVM morph begins, the
    // sky view owns every morphing body (planets, sun, moon, orbit
    // rings) as single lerped objects starting from these exact
    // positions — so this view stops drawing them entirely. No
    // crossfade: ownership transfers at a seam where the two forms
    // coincide.
    bool sky_owns = st->skyb > 0.001;
    float skw = (float)st->skyb;
    if (skw < 0) skw = 0;
    if (skw > 1) skw = 1;
    if (ss > 0.001f && machine_vis) {
        const PlanetsNow *pn = &st->planets;
        float a_ring   = ink_in(INK_RING, ss);
        float a_planet = ink_in(INK_PLANET, ss);
        float a_zod    = ink_in(INK_ZODIAC, ss);
        float a_sight  = ink_in(INK_SIGHT, ss);
        float a_web    = ink_in(INK_WEB, ss);
        float wheel_R  = base_w;   // Earth's orbit, system-stage size

        // Bead positions: true heliocentric longitudes on stylized rings.
        // Earth rides the live morph position (glued to the wheel).
        float ppx[PL_COUNT], ppy[PL_COUNT];
        for (int p = 0; p < PL_COUNT; p++) {
            float dx, dy;
            orr__ecl_dir(pn->helio_lon[p], &dx, &dy);
            ppx[p] = dx * orr__orbit_r(p, wheel_R);
            ppy[p] = dy * orr__orbit_r(p, wheel_R);
        }
        ppx[PL_EARTH] = ex;
        ppy[PL_EARTH] = ey;
        {
            // Stash the LIVE machine seats for the member-row compose
            OrreryViewState *wpl = (OrreryViewState *)(uintptr_t)buf;
            memcpy(wpl->pl_mx, ppx, sizeof(ppx));
            memcpy(wpl->pl_my, ppy, sizeof(ppy));
        }

        // Geocentric markers: where each body APPEARS in the zodiac. The
        // dial is a true longitude scale around the sun, so aspect chords
        // keep their sacred shapes (a grand trine is equilateral).
        float mpx[BODY_COUNT], mpy[BODY_COUNT];
        for (int b = 0; b < BODY_COUNT; b++) {
            float dx, dy;
            orr__ecl_dir(pn->geo_lon[b], &dx, &dy);
            mpx[b] = dx * ORR_WEB_R;
            mpy[b] = dy * ORR_WEB_R;
        }

        // Orbit rings (Earth's is the calendar wheel itself); the sky
        // view owns them during the CAELVM morph
        if (a_ring > 0.001f && !sky_owns) {
            d->alpha = base_alpha * a_ring;
            // Earth's ring draws too now — the calendar wheel has
            // moved out to the moat and no longer marks the orbit.
            // The HOME orbit reads a clear step above the others: this
            // is the ring the whole instrument turns on.
            for (int p = 0; p < PL_COUNT; p++) {
                bool home = (p == PL_EARTH);
                draw_set_color(d, dca(0.55f, 0.53f, 0.49f,
                                      home ? 0.34f : 0.22f));
                draw_circle_stroked(d, 0, 0,
                                    orr__orbit_r(p, wheel_R),
                                    home ? 1.3f : 1.0f);
            }
        }

        // Zodiac dial: 30-degree signs, 10-degree ticks, engraved names
        if (a_zod > 0.001f) {
            d->alpha = base_alpha * a_zod;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.22f));
            draw_circle_stroked(d, 0, 0, ORR_ZODIAC_IN, 1.0f);
            draw_circle_stroked(d, 0, 0, ORR_ZODIAC_OUT, 1.0f);
            for (int i = 0; i < 36; i++) {
                float dx, dy;
                orr__ecl_dir(i * 10.0, &dx, &dy);
                bool cusp = (i % 3) == 0;
                float r1 = cusp ? ORR_ZODIAC_OUT : ORR_ZODIAC_IN + 12.0f;
                draw_set_color(d, cusp ? dca(0.55f, 0.53f, 0.49f, 0.50f)
                                       : dca(0.55f, 0.53f, 0.49f, 0.25f));
                draw_line(d, dx * ORR_ZODIAC_IN, dy * ORR_ZODIAC_IN,
                          dx * r1, dy * r1, 1.0f);
            }
            // Sigils on the band's waist, feet toward the center all
            // the way around — engraved glyphs, not letters, so no
            // readability flip on the lower half
            draw_set_color(d, dc_scale(s->medium_grey, 0.72f));
            for (int i = 0; i < 12; i++) {
                float dx, dy;
                orr__ecl_dir(i * 30.0 + 15.0, &dx, &dy);
                float gr = 0.5f * (ORR_ZODIAC_IN + ORR_ZODIAC_OUT);
                orr__zodiac_glyph(d, i, dx * gr, dy * gr, dx, dy,
                                  28.0f);
            }

            // ---- The local horizon, cut through the zodiac ----
            // The horizon great circle crosses the ecliptic at two
            // opposite points (the chart's ascendant axis). The arc of
            // the zodiac below the horizon is shaded in the moat —
            // "under the earth" — and wheels around once per day.
            // Answers: which of these bodies is over MY horizon now.
            {
                // Ascendant: degrees past it have yet to rise, so
                // altitude falls through zero as longitude increases.
                // Descendant: the opposite crossing.
                double lam_asc = -1.0, lam_dsc = -1.0;
                for (int i = 0; i < 72; i++) {
                    float a0 = st->sky.ecl_alt[i];
                    float a1 = st->sky.ecl_alt[(i + 1) % 72];
                    if (a0 > 0 && a1 <= 0)
                        lam_asc = (i + a0 / (a0 - a1)) * 5.0;
                    else if (a0 <= 0 && a1 > 0)
                        lam_dsc = (i + a0 / (a0 - a1)) * 5.0;
                }
                if (lam_asc >= 0 && lam_dsc >= 0) {
                    double l0 = lam_asc;             // below-horizon arc:
                    double l1 = lam_dsc;             // ASC -> DSC
                    if (l1 < l0) l1 += 360.0;
                    float ba0 = (float)((0.75 + l0 / 360.0) * 2.0 * M_PI
                                        - M_PI * 0.5);
                    float ba1 = (float)((0.75 + l1 / 360.0) * 2.0 * M_PI
                                        - M_PI * 0.5);
                    draw_set_color(d, dca(0.25f, 0.10f, 0.07f, 0.30f));
                    draw_arc_filled(d, 0, 0, ORR_WEB_R - 78.0f,
                                    ORR_WEB_R - 8.0f, ba0, ba1, 64);

                    // Ascendant axis: ticks at both crossings, the
                    // rising end named
                    for (int k = 0; k < 2; k++) {
                        double lam = k ? lam_asc : lam_dsc;
                        float dx, dy;
                        orr__ecl_dir(lam, &dx, &dy);
                        draw_set_color(d, dca(0.60f, 0.55f, 0.48f, 0.55f));
                        draw_line(d, dx * (ORR_WEB_R - 82.0f),
                                  dy * (ORR_WEB_R - 82.0f),
                                  dx * (ORR_WEB_R - 4.0f),
                                  dy * (ORR_WEB_R - 4.0f), 1.0f);
                    }
                    {
                        float dx, dy;
                        orr__ecl_dir(lam_asc, &dx, &dy);
                        int aw = _font_compat[FONT_seconds].weight;
                        float tw = sdf_measure_width(aw, "ASC") * 13.0f;
                        draw_set_color(d, dca(0.60f, 0.55f, 0.48f, 0.65f));
                        draw_text_ex(d, aw, 13.0f,
                                     dx * (ORR_WEB_R - 100.0f) - tw * 0.5f,
                                     dy * (ORR_WEB_R - 100.0f) - 6.5f,
                                     "ASC");
                    }
                }
            }
        }

        // Sight-lines: from Earth, through the body, to where it lands in
        // the zodiac. Quadratic curves — the bend is the honest cost of a
        // not-to-scale layout (the endpoints and the bead are all true).
        if (a_sight > 0.001f) {
            d->alpha = base_alpha * a_sight;
            // Planets only: the sun's line would just trace the orbit
            // radial, and the moon hangs right next to Earth — both
            // projections are obvious without ink
            for (int b = BODY_MERCURY; b < BODY_COUNT; b++) {
                int p = planets_body_pl(b);
                float bx2 = ppx[p], by2 = ppy[p];
                // Control point puts the curve through the bead at its
                // TRUE fractional position along Earth->bead->marker.
                // Forcing t=0.5 slings the control point past Earth
                // whenever the bead hangs close to us (Venus on the
                // tempo rings) — the line left on the wrong side and
                // hairpinned back. Clamped off the ends: a bead at the
                // extremes would launch the control point instead.
                float d0 = sqrtf((bx2 - ex) * (bx2 - ex)
                               + (by2 - ey) * (by2 - ey));
                float d1 = sqrtf((mpx[b] - bx2) * (mpx[b] - bx2)
                               + (mpy[b] - by2) * (mpy[b] - by2));
                float ts = d0 / (d0 + d1 + 1.0e-6f);
                if (ts < 0.12f) ts = 0.12f;
                if (ts > 0.88f) ts = 0.88f;
                float tw = 2.0f * ts * (1.0f - ts);
                float p1x = (bx2 - (1.0f - ts) * (1.0f - ts) * ex
                                 - ts * ts * mpx[b]) / tw;
                float p1y = (by2 - (1.0f - ts) * (1.0f - ts) * ey
                                 - ts * ts * mpy[b]) / tw;
                draw_set_color(d, dca(orr__body_col[b][0] / 255.0f,
                                      orr__body_col[b][1] / 255.0f,
                                      orr__body_col[b][2] / 255.0f, 0.62f));
                float lx0 = ex, ly0 = ey;
                const int SEG = 20;
                for (int k = 1; k <= SEG; k++) {
                    float tt = (float)k / SEG, omt = 1.0f - tt;
                    float qx = omt * omt * ex + 2.0f * omt * tt * p1x
                             + tt * tt * mpx[b];
                    float qy = omt * omt * ey + 2.0f * omt * tt * p1y
                             + tt * tt * mpy[b];
                    draw_line(d, lx0, ly0, qx, qy, 1.0f);
                    lx0 = qx; ly0 = qy;
                }
            }
        }

        // The aspect web: chords between zodiac positions whose separation
        // matches a division of the circle. Ptolemaic majors only — the
        // minors are computed but stay off the dial (they were most of
        // the noise for the least meaning). Brightness is a hard cube of
        // orb tightness: a loose aspect whispers, an exact one burns —
        // the web flares when the sky does something geometric instead
        // of droning as a constant lattice. Conjunctions tie with a
        // bracket outside the chord radius (a zero-length chord says
        // nothing).
        if (a_web > 0.001f) {
            for (int i = 0; i < st->num_aspects; i++) {
                const Aspect *A = &st->aspects[i];
                const AspectDef *def = &planets_aspect_defs[A->kind];
                if (!def->major) continue;
                // Quiet ink (Seren): the web is structure, not
                // spectacle — uniform dark grey dots, meaning carried
                // by orb tightness alone (sharp aspects draw darker
                // and wider, loose ones barely whisper)
                float sharp = A->strength * A->strength * A->strength;
                if (A->kind == ASPECT_CONJUNCTION) {
                    double la = pn->geo_lon[A->a];
                    double dlt = planets_lon_diff(pn->geo_lon[A->b], la);
                    double l0 = (dlt < 0) ? la + dlt : la;
                    double l1 = (dlt < 0) ? la : la + dlt;
                    float aa0 = (float)((0.75 + (l0 - 1.5) / 360.0)
                                        * 2.0 * M_PI - M_PI * 0.5);
                    float aa1 = (float)((0.75 + (l1 + 1.5) / 360.0)
                                        * 2.0 * M_PI - M_PI * 0.5);
                    d->alpha = base_alpha * a_web
                             * (0.12f + 0.78f * sharp);
                    draw_set_color(d, dca(0.42f, 0.42f, 0.42f, 0.8f));
                    draw_arc_filled(d, 0, 0, ORR_WEB_R + 10.0f,
                                    ORR_WEB_R + 11.5f, aa0, aa1, 10);
                } else {
                    float wdt = 0.6f + 1.8f * sharp;
                    d->alpha = base_alpha * a_web
                             * (0.05f + 0.60f * sharp);
                    draw_set_color(d, dca(0.42f, 0.42f, 0.42f, 1.0f));
                    orr__dashed_line(d, mpx[A->a], mpy[A->a],
                                     mpx[A->b], mpy[A->b], wdt,
                                     4.0f, 10.0f);
                }
            }
            d->alpha = base_alpha;
        }

        // Geocentric markers on the dial + the classic retrograde R.
        // Marker state reads SEASONAL visibility: filled = reaches dark
        // sky from the observer's location tonight, hollow = out of view
        // for now (lost behind the sun, or never clearing the horizon in
        // darkness at this latitude). The combust wedge below shows why
        // most hollow markers are hollow.
        if (a_zod > 0.001f) {
            // Solar glare sector: bodies whose markers fall inside it
            // rise and set with the sun — invisible for weeks around
            // conjunction. The classic "combust" zone, made geometric.
            {
                double ls = pn->geo_lon[BODY_SUN];
                float ga0 = (float)((0.75 + (ls - 16.0) / 360.0)
                                    * 2.0 * M_PI - M_PI * 0.5);
                float ga1 = (float)((0.75 + (ls + 16.0) / 360.0)
                                    * 2.0 * M_PI - M_PI * 0.5);
                d->alpha = base_alpha * a_zod;
                draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.17f));
                draw_arc_filled(d, 0, 0, ORR_WEB_R - 28.0f,
                                ORR_WEB_R + 16.0f, ga0, ga1, 24);
            }
            int rw = _font_compat[FONT_seconds].weight;
            for (int b = 0; b < BODY_COUNT; b++) {
                // Pip size = apparent magnitude; filled vs outlined at
                // full strength carries the seasonal verdict. Venus
                // blooms, Mars breathes with its oppositions, Pluto
                // stays a speck no matter how close its aspect.
                float pr = orr__pip_r(pn->mag[b]);
                draw_set_color(d, dca(orr__body_col[b][0] / 255.0f,
                                      orr__body_col[b][1] / 255.0f,
                                      orr__body_col[b][2] / 255.0f, 0.9f));
                if (st->sky.observable[b])
                    draw_circle_filled(d, mpx[b], mpy[b], pr);
                else
                    draw_circle_stroked(d, mpx[b], mpy[b], pr, 1.2f);
                if (pn->retro[b]) {
                    float dx, dy;
                    orr__ecl_dir(pn->geo_lon[b], &dx, &dy);
                    float rx2 = dx * (ORR_WEB_R - 22.0f);
                    float ry2 = dy * (ORR_WEB_R - 22.0f);
                    float tw = sdf_measure_width(rw, "R") * 15.0f;
                    draw_text_ex(d, rw, 15.0f, rx2 - tw * 0.5f,
                                 ry2 - 7.5f, "R");
                }
            }
        }

        // Planet beads + engraved radial labels; the sky view owns
        // the BEADS during the CAELVM morph (it flies live copies),
        // but it has no counterpart for the ring names — so the names
        // keep their own weight, easing in from black over the tail
        // of the sky's departure instead of popping at the handoff
        float a_name = a_planet
                     * (st->skyb > 0.25 ? 0.0f
                                        : 1.0f - (float)st->skyb / 0.25f);
        if (a_planet > 0.001f && (!sky_owns || a_name > 0.001f)) {
            for (int p = 0; p < PL_COUNT; p++) {
                if (p == PL_EARTH) continue;
                // Beads themselves are VIEW_LVMEN's now (Stage 3) —
                // this view only engraves the ring names.
                if (a_name < 0.001f) continue;
                float orbr = orr__orbit_r(p, wheel_R);
                float th = (float)((0.75 + pn->helio_lon[p] / 360.0)
                                   * 2.0 * M_PI);
                // Name engraved along the ring like the month text —
                // planet and label share the arc, set clockwise of the
                // bead (ahead of it), hugging just outside the ring line
                float lscale = 1.2f;
                float ltrack = 0.5f;
                int lw = _font_compat[FONT_date].weight;
                float lsz = _font_compat[FONT_date].size * lscale;
                float tw = (sdf_measure_width(lw, orr__planet[p].name)
                            + ltrack * (float)strlen(orr__planet[p].name))
                         * lsz;
                float ang_off = (orr__planet[p].size + 14.0f + tw * 0.5f)
                              / orbr;
                float lth = th + ang_off;   // clockwise of the bead
                // The dial flip renders glyphs at a different radial
                // depth relative to the anchor radius — compensate each
                // branch separately so the letterform band CENTERS on
                // the orbit ring: text mid-height and planet center
                // share the same arc on both halves
                float na = fmodf(lth, 2.0f * (float)M_PI);
                if (na < 0) na += 2.0f * (float)M_PI;
                bool lflip = (na > (float)M_PI * 0.5f
                              && na < (float)M_PI * 1.5f);
                float lr = orbr + lsz * (lflip ? 0.51f : 0.37f);
                d->alpha = base_alpha * a_name * 0.85f;
                draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.85f));
                draw_text_curved(d, FONT_date, 0, 0, lr, lth,
                                 orr__planet[p].name, ltrack, lscale);
            }
            d->alpha = base_alpha;
        }
    }

    // Helio underlay: orbit radial to the globe. (The sun itself is
    // ONE object across all worldviews, drawn with the shared marker
    // below — at TELLVS it rides near the globe, and on the flight to
    // MACHINA it extends out along its own line to land at the wheel's
    // center.)
    if (helio_a > 0.001f) {
        draw_set_color(d, dca(0.5f, 0.5f, 0.5f, 0.18f));
        // Orbit radial fades at system scale (the sun sight-line covers it)
        d->alpha = base_alpha * helio_a * sysf;
        draw_line_thin(d, sun_x, sun_y, ex, ey);
        d->alpha = base_alpha * helio_a;
    }

    // Dial plate: the engraved panel behind the geocentric globe
    if (geo_a > 0.001f) {
        d->alpha = base_alpha * geo_a;
        draw_set_color(d, s->clear);
        draw_circle_filled(d, 0, dial_y, dial_r + 14.0f);
    }

    // ================= THE GLOBE =================

    d->alpha = base_alpha;
    GlobeCmd *g = machine_vis ? draw_globe_slot(d, ex, ey, earth_r)
                              : NULL;
    if (g) {
        memcpy(g->rot, rot, sizeof(rot));
        memcpy(g->light, light, sizeof(light));
        // Shader overlays (chrono/daylight/envelope) are frame-independent;
        // sun-paths renders as the sky-dome below.
        g->overlay = (s->globe_overlay == GLOBE_OVERLAY_SUNPATHS)
                   ? GLOBE_OVERLAY_NONE : s->globe_overlay;
        g->declination = (float)(-cos(phi) * GLOBE_OBLIQUITY_DEG);
        // The day-length ring slides poleward off the globe as the system
        // arrives; the continents fade in the shader. Only the terminator
        // survives the zoom-out.
        g->obs_lat = (float)(t->config.latitude
                     + (91.0 - t->config.latitude) * (1.0 - sysf));
        g->land_mix = sysf;
        // With coastlines carrying the geography, the plain graticule is
        // noise at orrery scale — fade it out as the morph completes
        g->grid_boost = 1.0f - m;
    }

    // Coastlines: Natural Earth outlines on the front hemisphere only,
    // rotating with the planet's spin. Subtle at dial size, present at
    // orrery scale.
    if (machine_vis) {
        float coast_a = (0.16f + 0.34f * m) * sysf;
        coast_a += (0.55f - coast_a) * ob;
        draw_set_color(d, dca(0.63f, 0.58f, 0.50f, coast_a));
        for (int li = 0; li < COAST_NUM_LINES; li++) {
            int start = coast_lines[li][0], count = coast_lines[li][1];
            float prev[3];
            bool has_prev = false;
            for (int i = start; i < start + count; i++) {
                float v[3];
                globe_mat_mul_vec(rot, st->coast_vec[i], v);
                if (has_prev && v[2] > 0.02f && prev[2] > 0.02f)
                    draw_line(d, ex + prev[0] * earth_r, ey + prev[1] * earth_r,
                              ex + v[0] * earth_r, ey + v[1] * earth_r, 1.0f);
                prev[0] = v[0]; prev[1] = v[1]; prev[2] = v[2];
                has_prev = true;
            }
        }
    }

    // ---- The analemma, engraved on the surface ----
    // The subsolar point at this same clock time across the year: the
    // figure-eight of the seasons, drawn around wherever the sun
    // stands today. Whole-day steps keep the clock time fixed; the
    // width is the equation of time, the height the declination.
    if (obf > 0.01f && machine_vis) {
        if (fabs(orbis_jd - stw->ana_jd) > 1.0 / 144.0) {
            stw->ana_jd = orbis_jd;
            for (int k = 0; k < 74; k++) {
                double jdk = orbis_jd + (double)((k % 73) * 5 - 180);
                double lam = (planets_helio_lon(PL_EARTH, jdk, NULL)
                              + 180.0) * M_PI / 180.0;
                double T = (jdk - 2451545.0) / 36525.0;
                double eps = (23.439291 - 0.0130042 * T) * M_PI / 180.0;
                double ra = atan2(sin(lam) * cos(eps), cos(lam));
                double dec = asin(sin(eps) * sin(lam));
                double slo = fmod(ra * 180.0 / M_PI
                                  - planets__gmst(jdk), 360.0)
                           * M_PI / 180.0;
                stw->ana_vec[k][0] = (float)(cos(dec) * cos(slo));
                stw->ana_vec[k][1] = (float)(cos(dec) * sin(slo));
                stw->ana_vec[k][2] = (float)(sin(dec));
            }
        }
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.30f * obf));
        float pv[3];
        bool has = false;
        for (int k = 0; k < 74; k++) {
            float v[3];
            globe_mat_mul_vec(rot, st->ana_vec[k], v);
            if (has && v[2] > 0.02f && pv[2] > 0.02f)
                draw_line(d, ex + pv[0] * earth_r, ey + pv[1] * earth_r,
                          ex + v[0] * earth_r, ey + v[1] * earth_r, 1.0f);
            pv[0] = v[0]; pv[1] = v[1]; pv[2] = v[2];
            has = true;
        }
    }

    // Limb outline: bounds the map at the silhouette so coastline vectors
    // meet a rim instead of floating unconnected against the sky
    draw_set_color(d, dca(0.63f, 0.58f, 0.50f,
                          0.10f + 0.35f * m + (0.35f - 0.35f * m) * ob));
    draw_circle_stroked(d, ex, ey, earth_r, 1.0f);

    // ================= OVER THE GLOBE =================

    // (The dial's outer ring and the moon aperture rim moved DOWN to
    // VIEW_CLOCKBACK — dial furniture belongs under the globes, not
    // painted over their limbs.)

    // Helio furniture: axis pin + 24h bezel, riding the globe
    if (helio_a > 0.001f) {
        d->alpha = base_alpha * helio_a;

        // The axis as a physical pin: the visible-pole segment runs from
        // space down to the pole's projected point ON the disc; the
        // far-pole segment terminates at the perimeter, occluded by the
        // planet. Uses the live rotation, so it tracks the morph.
        float axv0 = rot[8], axv1 = rot[9], axv2 = rot[10];
        float an = sqrtf(axv0 * axv0 + axv1 * axv1);
        d->alpha = base_alpha * helio_a * sysf;   // pin is globe detail
        if (an > 1e-4f && sysf > 0.001f) {
            float sgn = (axv2 >= 0) ? 1.0f : -1.0f;   // toward visible pole
            float vx = sgn * axv0 / an, vy = sgn * axv1 / an;
            float ext = earth_r * 0.17f + 4.0f;
            draw_set_color(d, dca(0.75f, 0.72f, 0.65f, 0.8f));
            draw_line(d, ex + vx * (earth_r + ext), ey + vy * (earth_r + ext),
                      ex + sgn * axv0 * earth_r, ey + sgn * axv1 * earth_r, 1.2f);
            draw_line(d, ex - vx * (earth_r + ext), ey - vy * (earth_r + ext),
                      ex - vx * earth_r, ey - vy * earth_r, 1.2f);
        }

        // (The uniform 24h bezel is gone — the surface clock projects its
        // hour marks outward past the limb at their true bearings.)

    }

    // Moon orbit ring: helio furniture that also rides the ORBIS
    // closeup — and there it goes THREE-DIMENSIONAL: the moon's true
    // orbital path (one sidereal month of geocentric track, inclined
    // ~5 deg to the ecliptic) in the current earth frame, rotated by
    // the live view. It projects as a correctly-oriented ellipse
    // through the moon itself; the far side dims, and the stretch
    // that passes BEHIND the planet is hidden by it. At the flat
    // stations each sample collapses to its screen bearing on the
    // classic circle, so the ring tilts into 3D as the closeup rises.
    {
        float ring_a = helio_a + (1.0f - helio_a) * obf;
        if (ring_a > 0.001f) {
            float ring_fac2 = 1.55f + (1.22f - 1.55f) * obf;
            enum { ORB_N = 96 };
            float ox[ORB_N + 1], oy[ORB_N + 1], oz[ORB_N + 1];
            double gmst_now = planets__gmst(orbis_jd);
            for (int k = 0; k <= ORB_N; k++) {
                double jdk = orbis_jd
                           + (double)(k % ORB_N) / ORB_N * 27.321662;
                double lo2, la2;
                planets__body_lonlat(BODY_MOON, jdk, &lo2, &la2);
                double Tk = (jdk - 2451545.0) / 36525.0;
                double ek = (23.439291 - 0.0130042 * Tk) * M_PI / 180.0;
                double lamk = lo2 * M_PI / 180.0;
                double betk = la2 * M_PI / 180.0;
                double sdk = sin(betk) * cos(ek)
                           + cos(betk) * sin(ek) * sin(lamk);
                double deck = asin(sdk);
                double rak = atan2(sin(lamk) * cos(ek)
                                   - tan(betk) * sin(ek), cos(lamk));
                double lek = (rak * 180.0 / M_PI - gmst_now)
                           * M_PI / 180.0;
                float v[3] = { (float)(cos(deck) * cos(lek)),
                               (float)(cos(deck) * sin(lek)),
                               (float)(sin(deck)) };
                float pv[3];
                globe_mat_mul_vec(rot, v, pv);
                // Flat-station form: the same sample squashed onto its
                // screen bearing on the classic circle
                float fl = sqrtf(pv[0] * pv[0] + pv[1] * pv[1]);
                float fx = fl > 1.0e-4f ? pv[0] / fl : 1.0f;
                float fy = fl > 1.0e-4f ? pv[1] / fl : 0.0f;
                ox[k] = ex + (fx * (1.0f - obf) + pv[0] * obf)
                             * earth_r * ring_fac2;
                oy[k] = ey + (fy * (1.0f - obf) + pv[1] * obf)
                             * earth_r * ring_fac2;
                oz[k] = pv[2];
            }
            // Brighter at the closeup: the 3D path must read against
            // both the lit disc and the black beyond
            // Assertive at the closeup, quiet at the stations
            draw_set_color(d, dca(0.64f, 0.62f, 0.58f,
                                  0.20f + 0.35f * obf));
            for (int k = 0; k < ORB_N; k++) {
                float mzk = (oz[k] + oz[k + 1]) * 0.5f;
                float fseg = 1.0f;
                if (mzk < 0.0f) {
                    float cxk = (ox[k] + ox[k + 1]) * 0.5f - ex;
                    float cyk = (oy[k] + oy[k + 1]) * 0.5f - ey;
                    bool occl = cxk * cxk + cyk * cyk
                              < earth_r * earth_r;
                    fseg = occl ? (1.0f - obf) : (1.0f - 0.5f * obf);
                }
                if (fseg < 0.01f) continue;
                d->alpha = base_alpha * ring_a * (1.0f - skw) * fseg;
                draw_line(d, ox[k], oy[k], ox[k + 1], oy[k + 1], 1.0f);
            }
            d->alpha = base_alpha;
        }
    }

    // ---- The armillary cage: TELLVS becomes an armillary sphere ----
    // The classical ring cage in the globe's LIVE frame. Equator and
    // tropics are circles of latitude (spin-invariant, so the daily
    // rotation never carries them); the two colures stand CELESTIALLY
    // — built each frame from the live axis and the equinox line
    // (equator plane meets the orrery plane), so they hold their
    // stations while the surface spins beneath. The ecliptic band
    // lies flat in the orrery plane — the one circle the cage shares
    // with the machine — with a small sun bead riding it at the true
    // bearing. Segments passing behind the globe go nearly dark; the
    // back of the cage reads at half ink, engraved not neon.
    {
        float arm = ink_in(INK_ARMILLARY, m) * sysf * (1.0f - obf);
        if (arm > 0.004f && machine_vis) {
            float d2r_ = (float)M_PI / 180.0f;
            float Rc = earth_r * 1.14f;
            d->alpha = base_alpha * arm;

            // Axis and celestial frame, live
            float zax[3] = { 0, 0, 1 }, n3[3];
            globe_mat_mul_vec(rot, zax, n3);
            float en = sqrtf(n3[0]*n3[0] + n3[1]*n3[1]);
            float e3[3] = { 1, 0, 0 }, s3[3];
            if (en > 1.0e-4f) {          // equinox line: n x z_view
                e3[0] = n3[1] / en; e3[1] = -n3[0] / en; e3[2] = 0;
            }
            s3[0] = n3[1]*e3[2] - n3[2]*e3[1];   // solstitial dir
            s3[1] = n3[2]*e3[0] - n3[0]*e3[2];
            s3[2] = n3[0]*e3[1] - n3[1]*e3[0];

            // One engraved ring: p(t) = u cos t + v sin t, segments
            // shaded by depth and the globe's silhouette
            #define ORR__CAGE_RING(U, V, A, W) do {                    \
                float px_ = 0, py_ = 0, pz_ = 0;                       \
                for (int i_ = 0; i_ <= 96; i_++) {                     \
                    float t_ = (float)i_ / 96.0f * 2.0f * (float)M_PI; \
                    float ct_ = cosf(t_), st_ = sinf(t_);              \
                    float qx_ = (U)[0]*ct_ + (V)[0]*st_;               \
                    float qy_ = (U)[1]*ct_ + (V)[1]*st_;               \
                    float qz_ = (U)[2]*ct_ + (V)[2]*st_;               \
                    float sx_ = ex + qx_ * Rc, sy_ = ey + qy_ * Rc;    \
                    if (i_ > 0) {                                      \
                        float mz_ = (pz_ + qz_) * 0.5f;                \
                        float mr_ = Rc * sqrtf(((px_+qx_)*0.5f)        \
                                    *((px_+qx_)*0.5f)                  \
                                    + ((py_+qy_)*0.5f)                 \
                                    *((py_+qy_)*0.5f));                \
                        float fa_ = 1.0f;                              \
                        if (mz_ < 0)                                   \
                            fa_ = mr_ < earth_r ? 0.10f : 0.45f;       \
                        d->alpha = base_alpha * arm * (A) * fa_;       \
                        draw_line(d, ex + px_ * Rc, ey + py_ * Rc,     \
                                  sx_, sy_, (W));                      \
                    }                                                  \
                    px_ = qx_; py_ = qy_; pz_ = qz_;                   \
                }                                                      \
            } while (0)

            draw_set_color(d, dca(0.58f, 0.56f, 0.51f, 0.85f));
            // Equator + tropics: u/v span the equatorial plane, the
            // circle of latitude phi rides the axis
            float lats[3] = { 0.0f, 23.436f, -23.436f };
            float lalpha[3] = { 0.60f, 0.28f, 0.28f };
            for (int L = 0; L < 3; L++) {
                float cl = cosf(lats[L] * d2r_);
                float sl = sinf(lats[L] * d2r_);
                float u3[3] = { e3[0]*cl + n3[0]*sl,
                                e3[1]*cl + n3[1]*sl,
                                e3[2]*cl + n3[2]*sl };
                float v3[3] = { s3[0]*cl + n3[0]*sl,
                                s3[1]*cl + n3[1]*sl,
                                s3[2]*cl + n3[2]*sl };
                // circles of latitude: center offset along the axis
                float off = sl;
                float ec3[3] = { e3[0]*cl, e3[1]*cl, e3[2]*cl };
                float sc3[3] = { s3[0]*cl, s3[1]*cl, s3[2]*cl };
                (void)u3; (void)v3;
                float px2 = 0, py2 = 0, pz2 = 0;
                for (int i = 0; i <= 96; i++) {
                    float t = (float)i / 96.0f * 2.0f * (float)M_PI;
                    float qx = ec3[0]*cosf(t) + sc3[0]*sinf(t)
                             + n3[0]*off;
                    float qy = ec3[1]*cosf(t) + sc3[1]*sinf(t)
                             + n3[1]*off;
                    float qz = ec3[2]*cosf(t) + sc3[2]*sinf(t)
                             + n3[2]*off;
                    if (i > 0) {
                        float mz = (pz2 + qz) * 0.5f;
                        float mr = Rc * sqrtf(((px2+qx)*0.5f)
                                   *((px2+qx)*0.5f)
                                   + ((py2+qy)*0.5f)*((py2+qy)*0.5f));
                        float fa = 1.0f;
                        if (mz < 0)
                            fa = mr < earth_r ? 0.10f : 0.45f;
                        d->alpha = base_alpha * arm * lalpha[L] * fa;
                        draw_line(d, ex + px2 * Rc, ey + py2 * Rc,
                                  ex + qx * Rc, ey + qy * Rc,
                                  L == 0 ? 1.4f : 1.0f);
                    }
                    px2 = qx; py2 = qy; pz2 = qz;
                }
            }
            // The colures: great circles through the poles, one
            // through the equinoxes, one through the solstices
            ORR__CAGE_RING(e3, n3, 0.30f, 1.0f);
            ORR__CAGE_RING(s3, n3, 0.30f, 1.0f);
            #undef ORR__CAGE_RING

            // The ecliptic band: flat in the orrery plane, pushed
            // OUTSIDE the moon's ring reach (1.55) so the two never
            // fight — the cage's one gilded ring, outermost as on
            // the brass instruments
            float Re = earth_r * 1.72f;
            draw_set_color(d, dca(0.70f, 0.54f, 0.24f, 0.60f));
            d->alpha = base_alpha * arm * 0.75f;
            draw_circle_stroked(d, ex, ey, Re, 1.2f);
            d->alpha = base_alpha * arm * 0.35f;
            draw_circle_stroked(d, ex, ey, Re + 3.0f, 1.0f);
            // The sun rides the band at its true bearing
            float lm = sqrtf(light[0]*light[0] + light[1]*light[1]);
            if (lm > 1.0e-4f) {
                d->alpha = base_alpha * arm * 0.9f;
                draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.9f));
                draw_circle_filled(d, ex + light[0]/lm * (Re + 1.5f),
                                   ey + light[1]/lm * (Re + 1.5f),
                                   3.5f);
            }
            d->alpha = base_alpha;
        }
    }

    // ---- Stage 3: the planets' member rows -> VIEW_LVMEN ----
    // One renderer for all nine bodies. Machine side: the live ring
    // seats with the machine's own stagger (VIEW_LVMEN's opacity
    // supplies the view fade — the beads dim exactly as the old
    // in-pass draw did). Chart side: the sky's published members on
    // the mover's parity weights (docs/TRANSITIONS.md): position
    // pw = sys*(1-skw), alpha ba*(sys + (1-sys)*fin) — born in place
    // when the machine has no system stage to fly from.
    {
        OrreryViewState *wpl = (OrreryViewState *)(uintptr_t)buf;
        const PlanetsNow *pn3 = &st->planets;
        // The astrolabe is PERIOD ACCURATE: no planets VISIBLE on the
        // plate — but they still TRAVEL true (Seren: like the stars,
        // move them correctly and fade them out). POSITION rides the
        // whole chart family (sky seat -> plate seat); PRESENCE rides
        // CAELVM's share alone, so they dissolve en route and the
        // plate stands period-pure at rest.
        float abp = (float)st->astb;
        float wS3 = (st->skyv && skw > 0.001f) ? skw : 0.0f;
        float wA3 = (st->astv && abp > 0.001f) ? abp : 0.0f;
        float chf = wS3 + wA3;
        if (chf > 1.0f) chf = 1.0f;
        bool chart3 = chf > 0.0f && st->skyv;
        float fin3 = ink_in(INK_BORN, skw);
        float a_pl3 = ink_in(INK_PLANET, ss);
        for (int p = 0; p < PL_COUNT; p++) {
            wpl->pl_ring_a[p] = 0;
            wpl->pl_stroke[p] = 0;
            if (p == PL_EARTH) { wpl->pl_ca[p] = 0; continue; }
            wpl->pl_col[p][0] = orr__planet[p].r / 255.0f;
            wpl->pl_col[p][1] = orr__planet[p].g / 255.0f;
            wpl->pl_col[p][2] = orr__planet[p].b / 255.0f;
            if (!chart3) {
                wpl->pl_cx[p] = wpl->pl_mx[p];
                wpl->pl_cy[p] = wpl->pl_my[p];
                wpl->pl_cr[p] = orr__planet[p].size;
                // The machine's own view fade rides IN the row (the
                // renderer draws at absolute alpha): a DRACO flight
                // holds VIEW_LVMEN's opacity high for the luminaries,
                // and the beads must not borrow that strength.
                wpl->pl_ca[p] = base_alpha * a_pl3;
                wpl->pl_col[p][3] = 1.0f;
                if (p == PL_SATURN)
                    wpl->pl_ring_a[p] = wpl->pl_ca[p];
            } else {
                const SkyViewState *sv = st->skyv;
                const AstroViewState *av = st->astv;
                int b = (p < PL_EARTH) ? BODY_MERCURY + p
                                       : BODY_MERCURY + p - 1;
                float ct = wS3 + wA3;
                float cx3 = (wS3 > 0 ? sv->pl_x[b] * wS3 : 0)
                          + (wA3 > 0 && av ? av->pl_x[b] * wA3 : 0);
                float cy3 = (wS3 > 0 ? sv->pl_y[b] * wS3 : 0)
                          + (wA3 > 0 && av ? av->pl_y[b] * wA3 : 0);
                cx3 /= ct > 0 ? ct : 1.0f;
                cy3 /= ct > 0 ? ct : 1.0f;
                // The machine seat is the LIVE morphing ring (base_w
                // flies with the station), and its claim is the
                // station weight x the machine's own bead presence
                // (a_pl3). sys*(1-skw) pinned beads to the chart for
                // most of a geo-parked flight to MVNDI and raced them
                // home at the tail (Seren: HOROLOGIVM -> CAELVM ->
                // MVNDI skipped into place); the bare station weight
                // dragged them toward invisible dial-scale rings on
                // flights where the machine shows no beads at all —
                // a_pl3 keeps the born-in-place law for those, and
                // saturates by sys 0.6 so real flights glide early.
                float dx3, dy3;
                orr__ecl_dir(pn3->helio_lon[p], &dx3, &dy3);
                float orbr3 = orr__orbit_r(p, base_w);
                float pw3 = (float)tempus_smoothstep(0.0, 1.0,
                                                     1.0 - chf)
                          * ink_in(INK_BEAD_CLAIM, ss);
                wpl->pl_cx[p] = dx3 * orbr3 * pw3
                              + cx3 * (1.0f - pw3);
                wpl->pl_cy[p] = dy3 * orbr3 * pw3
                              + cy3 * (1.0f - pw3);
                wpl->pl_cr[p] = orr__planet[p].size * pw3
                              + sv->pl_r[b] * (1.0f - pw3);
                wpl->pl_ca[p] = sv->pl_ba[b]
                              * (ss + (1.0f - ss) * fin3);
                wpl->pl_col[p][3] = 0.95f;
                wpl->pl_stroke[p] = sv->pl_stroke[b];
            }
        }
    }

    // ---- Shared elements: continuous across the morph, never fade ----
    d->alpha = base_alpha;

    // THE SUN: one object across every worldview. At m=0 its position
    // equals the dial marker exactly (the subsolar orthographic
    // projection); at TELLVS it is the bead lifted off the globe along
    // the sun line; and as the system arrives it EXTENDS OUT along
    // that same line to land at the wheel's center, growing into the
    // disc that anchors MACHINA MVNDI. When the subsolar point is
    // behind the planet (geo only), clamp to the sunward limb, dimmed
    // — "the sun is beyond this horizon".
    {
        float lx = light[0], ly = light[1], lz = light[2];
        DrawColor sun_c = s->sunrise_handle;
        if (lz < 0) {
            float n = sqrtf(lx * lx + ly * ly);
            if (n > 1e-4f) { lx /= n; ly /= n; }
            // Fake translucency belongs to the SMALL clock-face
            // globe only — at TELLVS the sun stands free (Seren)
            sun_c = dc_scale(sun_c, 1.0f - 0.5f * sysf * (1.0f - m));
        }
        // In helio the real sun sits at the wheel center, so the marker
        // lifts well off the globe toward it — a bead on the sun line.
        // Its tether starts at the PERIMETER, not the globe center.
        // The ORBIS closeup collapses the lift on its own clock: the
        // marker pins to the surface as the subsolar point, instead of
        // wobbling out-and-in while the lift (station clock) and the
        // radius hold (orbis clock) race each other.
        // At the closeup the sun floats a little off the surface —
        // the TELLVS gesture at a smaller reach — tethered straight
        // down to the subsolar point.
        float lift = 1.0f + 0.9f * m * (1.0f - ob) + 0.22f * obf;
        float mx0 = ex + lx * earth_r * lift, my0 = ey + ly * earth_r * lift;
        float mag = sqrtf(lx * lx + ly * ly);

        // The departure: bead position -> the wheel's center. A plain
        // lerp fights the globe's own outbound flight (the anchor
        // swings out on the same eased clock the lerp pulls in on,
        // bowing the path and overshooting the center) — so instead
        // the whole vector retracts along a COSINE matched to the
        // wheel zoom's curve (z = 1 - ss on station flights), with the
        // globe-follow term additionally damped so it cannot inject
        // the sideways swing.
        float retr = cosf((float)M_PI * 0.5f * ss);
        float px = (ex * (1.0f - ss) + lx * earth_r * lift) * retr;
        float py = (ey * (1.0f - ss) + ly * earth_r * lift) * retr;
        float msz = s->sun_size * earth_r / dial_r;
        msz *= 1.0f - 0.55f * ob;   // the closeup reads it as the
                                    // subsolar point, not a body
        float sz = msz + (32.0f - msz) * ss;
        sun_c.r = sun_c.r + (196.0f / 255.0f - sun_c.r) * ss;
        sun_c.g = sun_c.g + (126.0f / 255.0f - sun_c.g) * ss;
        sun_c.b = sun_c.b + (16.0f / 255.0f - sun_c.b) * ss;

        // Tether is globe furniture — gone as the sun departs, and
        // faded with the rising sky (its endpoint is the machine form,
        // not the flying luminary)
        d->alpha = base_alpha * sysf * (1.0f - obf) * (1.0f - skw);
        if (mag * lift > 1.02f && mag > 1e-4f) {
            float sx0 = ex + (lx / mag) * earth_r;
            float sy0 = ey + (ly / mag) * earth_r;
            draw_set_color(d, dca(0.75f, 0.75f, 0.75f, 0.35f));
            draw_line(d, sx0, sy0, mx0, my0, 1.0f);
        }
        // ORBIS tether: from the subsolar point on the surface up to
        // the floating sun — the line says "the sun stands over HERE"
        if (obf > 0.01f && skw < 0.999f) {
            d->alpha = base_alpha * obf * (1.0f - skw);
            draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.45f));
            draw_line(d, ex + lx * earth_r, ey + ly * earth_r,
                      ex + lx * earth_r * lift, ey + ly * earth_r * lift,
                      1.0f);
        }
        d->alpha = base_alpha;

        // The chart stations (CAELVM, DRACO) are alternatives to the
        // MACHINE's seat, not to each other — BARYCENTRIC weights, so
        // a sky<->draco flight travels chart-to-chart while the
        // machine's weight pins to zero (sequential lerps let the
        // aperture leak back in mid-flight: the sun dipped inward
        // and back out).
        float dw = (float)st->drab;
        float ab = (float)st->astb;
        float wS = (st->skyv && skw > 0.001f) ? skw : 0.0f;
        float wD = (st->drav && dw > 0.001f) ? dw : 0.0f;
        float wA = (st->astv && ab > 0.001f) ? ab : 0.0f;
        if (wS > 0.0f || wD > 0.0f || wA > 0.0f) {
            // MEMBER ROWS (Stage 2): seat 0 is the machine's own
            // composed seat (its internal morph over helio/system/
            // orbis); seats 1..3 are the chart stations' published
            // members — CAELVM, DRACO, and the astrolabe's plate.
            // Cross-frame outputs, so seat_mix_pos (rule 3).
            float wB = 1.0f - wS - wD - wA;
            if (wB < 0.0f) wB = 0.0f;
            double wT = wB + wS + wD + wA;
            double w[4] = { wB / wT, wS / wT, wD / wT, wA / wT };
            float mx[4] = { px, wS > 0 ? st->skyv->lum_sun_x : 0,
                            wD > 0 ? st->drav->lum_sun_x : 0,
                            wA > 0 ? st->astv->lum_sun_x : 0 };
            float my[4] = { py, wS > 0 ? st->skyv->lum_sun_y : 0,
                            wD > 0 ? st->drav->lum_sun_y : 0,
                            wA > 0 ? st->astv->lum_sun_y : 0 };
            seat_mix_pos(mx, my, w, 4, &px, &py);
            float mr[4] = { sz, wS > 0 ? st->skyv->lum_sun_r : 0,
                            28.0f,
                            wA > 0 ? st->astv->lum_sun_r : 0 };
            sz = mr[0] * (float)w[0] + mr[1] * (float)w[1]
               + mr[2] * (float)w[2] + mr[3] * (float)w[3];
            // the dragon's gold rides its share of the mix
            float gd = (float)w[2];
            sun_c.r += (0.85f - sun_c.r) * gd;
            sun_c.g += (0.62f - sun_c.g) * gd;
            sun_c.b += (0.18f - sun_c.b) * gd;
        }

        // Publish for VIEW_LVMEN and for hit-testing
        OrreryViewState *wst = (OrreryViewState *)(uintptr_t)buf;
        wst->lum_sun_x = px;
        wst->lum_sun_y = py;
        wst->lum_sun_r = sz;
        wst->lum_sun_col[0] = sun_c.r;
        wst->lum_sun_col[1] = sun_c.g;
        wst->lum_sun_col[2] = sun_c.b;
        wst->lum_sun_ray = ss * (1.0f - skw) * (1.0f - dw)
                         * (1.0f - ab);
        wst->bead_x = px;
        wst->bead_y = py;
        wst->bead_r = sz;
        wst->bead_hit = s->sun_size * earth_r / dial_r + 16.0f;
        wst->glob_x = ex;
        wst->glob_y = ey;
        wst->glob_r = earth_r;
        memcpy(wst->glob_rot, rot, sizeof(rot));
    }

    // ---- The moon: ONE object across both worldviews ----
    // Two STATIC world-space endpoint transforms — the geo aperture from
    // geo values, the helio orbit slot from PURE helio values (never
    // mid-morph intermediates) — and a single pos/scale lerp + slerp
    // between them. The endpoints evolve only with real time, so the
    // transition is one clean tween toward a stable target.
    {
        double ph = globe_moon_phase(tv->jd_current
                                     + tv->percent_of_day - 0.5);
        float b = (float)(ph * 2.0 * M_PI);

        // Geo endpoint: the 6 o'clock aperture, phase-frame light
        float gx2 = 0.0f, gy2 = -dial_y, gr2 = 52.0f;
        float gl2[3] = { sinf(b), 0.0f, -cosf(b) };

        // THE MOON'S FRAME LAW (Seren): shown WITH the globe — at
        // EVERY station, MACHINA included — the moon is PHYSICALLY
        // placed: its true direction from the earth, seen in the
        // globe's own view frame. The one exception is the 12-hour
        // clock's 6-o'clock aperture, which is dial furniture. (The
        // moon has no sight-line to a zodiac marker, so nothing in
        // the wheel-world needs the mirrored direction.)
        float mdx, mdy;
        orr__ecl_dir(st->planets.geo_lon[BODY_MOON], &mdx, &mdy);
        float moon_vz = 1.0f;   // view-frame z of the physical direction
        float mpx, mpy;         // un-normalized 3D projection: the
                                // moon's true place on its tilted path
        {
            double mlon2, mlat2;
            planets__body_lonlat(BODY_MOON, orbis_jd, &mlon2, &mlat2);
            double T2 = (orbis_jd - 2451545.0) / 36525.0;
            double ep2 = (23.439291 - 0.0130042 * T2) * M_PI / 180.0;
            double lam2 = mlon2 * M_PI / 180.0;
            double bet2 = mlat2 * M_PI / 180.0;
            double sd2 = sin(bet2) * cos(ep2)
                       + cos(bet2) * sin(ep2) * sin(lam2);
            double dec2 = asin(sd2);
            double ra2 = atan2(sin(lam2) * cos(ep2)
                               - tan(bet2) * sin(ep2), cos(lam2));
            double sl2 = fmod(ra2 * 180.0 / M_PI
                              - planets__gmst(orbis_jd), 360.0)
                       * M_PI / 180.0;
            float me[3] = { (float)(cos(dec2) * cos(sl2)),
                            (float)(cos(dec2) * sin(sl2)),
                            (float)(sin(dec2)) };
            float mv[3];
            globe_mat_mul_vec(rot, me, mv);
            float mm2 = sqrtf(mv[0] * mv[0] + mv[1] * mv[1]);
            if (mm2 > 1.0e-4f) {
                moon_vz = mv[2];
                mdx = mv[0] / mm2;
                mdy = mv[1] / mm2;
                mpx = mv[0];
                mpy = mv[1];
            } else {
                mpx = mdx;
                mpy = mdy;
            }
        }
        // At system scale the moon shrinks toward a bead beside its planet
        float hr2 = 22.0f * (1.0f - 0.55f
                             * ink_in(INK_MOON_SHRINK, ss));
        float hl2[3] = { helio_light[0], helio_light[1], helio_light[2] };
        float edx = -mdx, edy = -mdy;   // direction moon -> earth, screen

        // The moon's SEAT is polar about the LIVE globe: physical
        // bearing, ring-reach radius (1.55 at the stations, easing to
        // the 1.22 hang at the closeup), riding the planet's live
        // center and size — between TELLVS, MACHINA, and ORBIS the
        // relative seat never moves. The 12-hour clock's aperture is
        // the only other form, and its claim follows the CLOCK'S own
        // choreography: the hands leave the face in the first quarter
        // of any departure, and so does the moon — a plain point mix
        // over that brief window (never an arc about a globe 600
        // units away), zero for the rest of every flight, so neither
        // clock->MUNDI spirals nor MUNDI->ORBIS ever feels its pull.
        float ring_fac = 1.55f + (1.22f - 1.55f) * obf;
        // THE APERTURE'S CLAIM rides the station weight vector: dial
        // family against globe family, normalized within the machine's
        // own seat (the charts blend downstream). The claim spans the
        // WHOLE flight — the moon GLIDES from dial furniture into
        // globe attachment instead of welding on at the first quarter
        // (Seren: the hard handoff into MVNDI). The parked closeup
        // still silences it, so overlay departures fade in place.
        double wdg = st->w_dial + st->w_globe;
        float w_ap = wdg > 1.0e-6
                   ? (float)(st->w_dial / wdg) * (1.0f - obf) : 0.0f;
        float morb = 1.0f - w_ap;    // the orbital form's claim
        float sdx = mdx * (1.0f - obf) + mpx * obf;
        float sdy = mdy * (1.0f - obf) + mpy * obf;
        float seat_x = ex + sdx * earth_r * ring_fac;
        float seat_y = ey + sdy * earth_r * ring_fac;
        float mmx = seat_x * morb + gx2 * w_ap;
        float mmy = seat_y * morb + gy2 * w_ap;
        float mmr = gr2 * w_ap
                  + (hr2 * (1.0f - obf) + 24.0f * obf) * morb;

        // Light blend WITHOUT the shortest-path sign flip: aperture
        // phase-frame vs orbital sun, then eased to the shared
        // earth-frame light as the closeup rises
        float ml[3];
        float mn = 0;
        for (int i = 0; i < 3; i++) {
            ml[i] = gl2[i] * w_ap + hl2[i] * morb;
            mn += ml[i] * ml[i];
        }
        mn = sqrtf(mn);
        if (mn > 1e-3f) {
            for (int i = 0; i < 3; i++) ml[i] /= mn;
        } else {
            memcpy(ml, morb > 0.5f ? hl2 : gl2, sizeof(ml));
        }
        float moon_dim = 1.0f;
        if (moon_vz < 0.0f) {
            moon_dim = 1.0f - 0.45f * obf * (1.0f - skw);
            float odx = mmx - ex, ody = mmy - ey;
            if (odx * odx + ody * ody < earth_r * earth_r)
                moon_dim *= 1.0f - 0.85f * obf * (1.0f - skw);
        }
        if (obf > 0.001f) {
            // Tether: from the surface point directly beneath the
            // moon's 3D place, fading with the rising sky
            if (skw < 0.999f) {
                d->alpha = base_alpha * obf * moon_dim * (1.0f - skw);
                draw_set_color(d, dca(0.72f, 0.72f, 0.70f, 0.35f));
                draw_line(d, ex + mpx * earth_r,
                          ey + mpy * earth_r, mmx, mmy, 1.0f);
                d->alpha = base_alpha;
            }
            float mn2 = 0;
            for (int i = 0; i < 3; i++) {
                ml[i] = ml[i] * (1.0f - obf) + light[i] * obf;
                mn2 += ml[i] * ml[i];
            }
            mn2 = sqrtf(mn2);
            if (mn2 > 1.0e-3f)
                for (int i = 0; i < 3; i++) ml[i] /= mn2;
        }

        // Chart stations mix BARYCENTRICALLY (see the sun): the
        // machine's aperture seat must not leak into a sky<->draco
        // flight. Position, size, and phase light all share weights.
        float dw2 = (float)st->drab;
        float ab2 = (float)st->astb;
        {
            float wS = (st->skyv && skw > 0.001f) ? skw : 0.0f;
            float wD = (st->drav && dw2 > 0.001f) ? dw2 : 0.0f;
            float wA = (st->astv && ab2 > 0.001f) ? ab2 : 0.0f;
            if (wS > 0.0f || wD > 0.0f || wA > 0.0f) {
                // MEMBER ROWS (Stage 2): seat 0 = the machine's composed
                // moon; seats 1..3 = the chart stations' published
                // members (CAELVM, DRACO, the astrolabe's plate).
                float wB = 1.0f - wS - wD - wA;
                if (wB < 0.0f) wB = 0.0f;
                double wT = wB + wS + wD + wA;
                double w[4] = { wB / wT, wS / wT, wD / wT, wA / wT };
                float mx[4] = { mmx, wS > 0 ? st->skyv->lum_moon_x : 0,
                                wD > 0 ? st->drav->lum_moon_x : 0,
                                wA > 0 ? st->astv->lum_moon_x : 0 };
                float my[4] = { mmy, wS > 0 ? st->skyv->lum_moon_y : 0,
                                wD > 0 ? st->drav->lum_moon_y : 0,
                                wA > 0 ? st->astv->lum_moon_y : 0 };
                seat_mix_pos(mx, my, w, 4, &mmx, &mmy);
                float mr[4] = { mmr, wS > 0 ? st->skyv->lum_moon_r : 0,
                                28.0f,
                                wA > 0 ? st->astv->lum_moon_r : 0 };
                mmr = mr[0] * (float)w[0] + mr[1] * (float)w[1]
                    + mr[2] * (float)w[2] + mr[3] * (float)w[3];
                float lv[4][3];
                memcpy(lv[0], ml, sizeof(lv[0]));
                for (int i = 0; i < 3; i++) {
                    lv[1][i] = wS > 0 ? st->skyv->lum_moon_light[i] : 0;
                    lv[2][i] = wD > 0 ? st->drav->lum_light[i] : 0;
                    lv[3][i] = wA > 0 ? st->astv->lum_moon_light[i] : 0;
                }
                seat_mix_dir3((const float (*)[3])lv, w, 4, ml);
            }
        }
        // Publish for VIEW_LVMEN (and the pointer code's exclusions)
        stw->moon_x = mmx;
        stw->moon_y = mmy;
        stw->moon_r = mmr;
        stw->lum_moon_x = mmx;
        stw->lum_moon_y = mmy;
        stw->lum_moon_r = mmr;
        stw->lum_moon_a = moon_dim;
        memcpy(stw->lum_moon_light, ml, sizeof(ml));
        {
            // Tidal locking: geo shows the near side (lon 0 centered,
            // north up); helio looks down the lunar pole with the
            // near-side meridian aimed at Earth; the sky chart returns
            // to the near side. Two continuity-slerped stages.
            float rot_geo[16], rot_helio[16], mrot0[16];
            globe_rotation(0, 0, rot_geo);
            memset(rot_helio, 0, sizeof(rot_helio));
            // rows: (edx,edy,0), (edy,-edx,0), (0,0,1) — det -1 display
            rot_helio[0] = edx;  rot_helio[4] = edy;
            rot_helio[1] = edy;  rot_helio[5] = -edx;
            rot_helio[10] = 1.0f;
            rot_helio[15] = 1.0f;
            globe_rot_slerp_cont(rot_geo, rot_helio, morb,
                                 stw->moon_pq, &stw->moon_pq_valid,
                                 mrot0);
            // Every CHART returns the moon to the near side — the
            // claim is the chart family's combined weight
            double chw = skw + ab2;
            if (chw > 1.0) chw = 1.0;
            globe_rot_slerp_cont(mrot0, rot_geo, chw,
                                 stw->lum_pq, &stw->lum_pq_valid,
                                 stw->lum_moon_rot);
        }
        stw->lum_moon_aux[0] = edx * morb * (1.0f - skw) * (1.0f - dw2)
                             * (1.0f - ab2);
        stw->lum_moon_aux[1] = edy * morb * (1.0f - skw) * (1.0f - dw2)
                             * (1.0f - ab2);
        stw->lum_moon_aux[2] = 1.0f - morb * (1.0f - skw)
                                    * (1.0f - dw2) * (1.0f - ab2);
        stw->lum_moon_aux[3] = 1.0f;
    }

    // City marker ("you are here") + sky-dome. At m=0 the city projects
    // to the globe center — identical to the old dial ownship ring — and
    // the dome flattens into the dial's sun-path rendering. Both are
    // globe detail: gone at system scale.
    if (sysf > 0.001f) {
        d->alpha = base_alpha * sysf;
        double latr = t->config.latitude * M_PI / 180.0;
        double lonr = t->config.longitude * M_PI / 180.0;
        float p[3] = {
            (float)(cos(latr) * cos(lonr)),
            (float)(cos(latr) * sin(lonr)),
            (float)(sin(latr)),
        };
        float v[3];
        globe_mat_mul_vec(rot, p, v);
        if (v[2] > 0.05f) {
            draw_set_color(d, dca(0.85f, 0.85f, 0.85f, 0.9f));
            draw_circle_stroked(d, ex + v[0] * earth_r, ey + v[1] * earth_r,
                                earth_r * 0.025f + 2.0f, 1.0f);

            const SolarViewState *sol = st->solar;
            if (s->globe_overlay == GLOBE_OVERLAY_SUNPATHS && sol
                && sol->paths_jd == sol->tv.jd_current) {
                float e_east[3] = {
                    (float)-sin(lonr), (float)cos(lonr), 0.0f };
                float e_north[3] = {
                    (float)(-sin(latr) * cos(lonr)),
                    (float)(-sin(latr) * sin(lonr)),
                    (float)cos(latr) };
                float ev[3], nv2[3];
                globe_mat_mul_vec(rot, e_east, ev);
                globe_mat_mul_vec(rot, e_north, nv2);

                struct { const SunPathPt *pts; int n; bool close; DrawColor c; } sets[4] = {
                    { sol->path_jun, SOLAR_PATH_N, false, dca(0.55f, 0.55f, 0.55f, 0.45f) },
                    { sol->path_dec, SOLAR_PATH_N, false, dca(0.55f, 0.55f, 0.55f, 0.45f) },
                    { sol->path_today, SOLAR_PATH_N, false, dca(0.85f, 0.85f, 0.85f, 0.75f) },
                    { sol->analemma, SOLAR_ANA_N, true, dc_scale(s->sunrise_handle, 0.8f) },
                };
                for (int si = 0; si < 4; si++) {
                    draw_set_color(d, sets[si].c);
                    int last = sets[si].close ? sets[si].n : sets[si].n - 1;
                    for (int i = 0; i < last; i++) {
                        const SunPathPt *a = &sets[si].pts[i];
                        const SunPathPt *b = &sets[si].pts[(i + 1) % sets[si].n];
                        if (!a->up || !b->up) continue;
                        // Dome reconstruction: altitude = sqrt(1-x^2-y^2);
                        // pure view-z at m=0, so the flat dial is unchanged
                        float ua = sqrtf(fmaxf(0.0f, 1.0f - a->x * a->x - a->y * a->y));
                        float ub = sqrtf(fmaxf(0.0f, 1.0f - b->x * b->x - b->y * b->y));
                        float ax = v[0] * (1 + ua) + ev[0] * a->x - nv2[0] * a->y;
                        float ay = v[1] * (1 + ua) + ev[1] * a->x - nv2[1] * a->y;
                        float az = v[2] * (1 + ua) + ev[2] * a->x - nv2[2] * a->y;
                        float bx = v[0] * (1 + ub) + ev[0] * b->x - nv2[0] * b->y;
                        float by = v[1] * (1 + ub) + ev[1] * b->x - nv2[1] * b->y;
                        float bz = v[2] * (1 + ub) + ev[2] * b->x - nv2[2] * b->y;
                        // Sphere occlusion
                        if (az < 0 && ax * ax + ay * ay < 1.0f) continue;
                        if (bz < 0 && bx * bx + by * by < 1.0f) continue;
                        draw_line(d, ex + ax * earth_r, ey + ay * earth_r,
                                  ex + bx * earth_r, ey + by * earth_r, 1.0f);
                    }
                }
            }
        }
    }

    // ---- Surface clock face: the latitude ring as a 24-hour dial ----
    // Tick positions are sun-anchored (noon is always the ring's sunward
    // crossing), so the city marker sweeps past them as the hour hand —
    // a clock face lying on the surface of the planet.
    // At full-system scale the globe is a bead — the surface clock goes
    // out entirely with the rest of the globe detail.
    float clk_a = m * sysf;
    if (clk_a > 0.02f) {
        double latr2 = t->config.latitude * M_PI / 180.0;
        float cl = (float)cos(latr2), sl = (float)sin(latr2);
        float le[3];
        globe_mat_tmul_vec(rot, light, le);   // subsolar point, earth frame
        float lam_ss = atan2f(le[1], le[0]);
        float tick_len = earth_r * 0.035f;
        float num_size = _font_compat[FONT_event].size * 0.42f;
        int w_id = _font_compat[FONT_event].weight;

        for (int h = 0; h < 24; h++) {
            float lam = lam_ss + (float)(h - 12) * 15.0f * (float)M_PI / 180.0f;
            float p[3]  = { cl * cosf(lam), cl * sinf(lam), sl };
            float tn[3] = { -sl * cosf(lam), -sl * sinf(lam), cl };  // poleward
            float pv[3], tw3[3];
            globe_mat_mul_vec(rot, p, pv);
            globe_mat_mul_vec(rot, tn, tw3);
            if (pv[2] < 0.03f) continue;   // front hemisphere only

            bool major = (h % 3) == 0;
            (void)tw3; (void)tick_len;

            // Bezel tick aimed at the DIAL's center — the ring's own
            // projected center (where the axis pierces the ring plane) —
            // not the globe center. Cast a ray from there through the
            // hour point and find where it exits the bezel radius.
            float cvx = rot[8] * sl, cvy = rot[9] * sl;   // ring center, view
            float ux = pv[0] - cvx, uy = pv[1] - cvy;
            float un = sqrtf(ux * ux + uy * uy);
            if (un < 0.03f) continue;
            ux /= un; uy /= un;

            float t0 = earth_r * 0.06f + 1.5f;
            float t1 = earth_r * (major ? 0.16f : 0.11f) + (major ? 3.5f : 2.5f);

            // Ray-circle intersection: |c + s*u| = k (earth-radius units)
            float k0 = 1.0f + t0 / earth_r;
            float cu = cvx * ux + cvy * uy;
            float cc = cvx * cvx + cvy * cvy;
            float disc = cu * cu + k0 * k0 - cc;
            if (disc <= 0) continue;
            float s0 = -cu + sqrtf(disc);
            float bx = ex + (cvx + s0 * ux) * earth_r;
            float by = ey + (cvy + s0 * uy) * earth_r;

            draw_set_color(d, major ? s->clock_lines_strong : s->clock_lines);
            d->alpha = base_alpha * clk_a;
            draw_line(d, bx, by, bx + ux * (t1 - t0), by + uy * (t1 - t0), 1.0f);

            if (major) {
                char hb[3];
                if (h >= 10) { hb[0] = (char)('0' + h / 10); hb[1] = (char)('0' + h % 10); hb[2] = 0; }
                else         { hb[0] = (char)('0' + h); hb[1] = 0; }
                float nd = (t1 - t0) + num_size * 0.85f;
                float nx = bx + ux * nd, ny = by + uy * nd;
                float tw = sdf_measure_width(w_id, hb) * num_size;
                draw_set_color(d, dca(0.8f, 0.78f, 0.72f, 0.65f));
                draw_text_ex(d, w_id, num_size,
                             nx - tw * 0.5f, ny - num_size * 0.5f, hb);
            }
            d->alpha = base_alpha;
        }
    }

    d->alpha = base_alpha;
}

static const ViewVtable orrery_vtable = {
    .init   = orrery_init,
    .enter  = orrery_enter,
    .exit   = orrery_exit,
    .update = orrery_update,
    .render = orrery_render,
};

#endif // SCENE_DEFINED && !VIEW_ORRERY_IMPL
