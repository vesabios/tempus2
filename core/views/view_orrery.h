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
    double geo_azimuth; // live sun az/zen from the solar view, for the
    double geo_zenith;  // geocentric endpoint of the morph
    const SolarViewState *solar;  // solar data + sun-path caches

    // Published by render for hit-testing: the sun bead is draggable in
    // helio mode (rotating the sun's direction = choosing the date)
    float bead_x, bead_y;   // bead position, world coords
    float bead_hit;         // hit radius
    float glob_x, glob_y;   // globe center, world coords
    float glob_r;           // globe radius (wheel drags exclude the globe)
    bool  dragging;
    bool  drag_earth;       // system view: dragging Earth around its orbit

    // Orientation-morph continuity state (see globe_rot_slerp_cont)
    float earth_pq[4];
    bool  earth_pq_valid;
    float moon_pq[4];
    bool  moon_pq_valid;

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
// Not to scale: orbit radii in calendar-wheel units, spaced like the
// concentric rings of an astronomical clock face. Earth's orbit IS the
// calendar wheel (1.0); Mercury and Venus sit inside it, Mars through
// Pluto outside, and the zodiac dial is the outermost ring.

// Inner planets scale with the wheel; outer rings sit at ABSOLUTE
// offsets beyond it — the month text needs ~130 units of clearance no
// matter how far the wheel shrinks at system stage. The gap from
// Pluto's ring to the zodiac dial is deliberate negative space — the
// moat between the heliocentric machine and the geocentric dial,
// crossed only by the sight-lines.
static const float orr__inner_frac[2] = { 0.36f, 0.64f };

static inline float orr__orbit_r(int p, float wheel_R) {
    static const float outer_off[6] = {
        150.0f, 180.0f, 210.0f, 240.0f, 270.0f, 300.0f };
    if (p < PL_EARTH) return wheel_R * orr__inner_frac[p];
    if (p == PL_EARTH) return wheel_R;
    return wheel_R + outer_off[p - PL_MARS];
}

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
static const uint8_t orr__aspect_col[ASPECT_TYPE_COUNT][3] = {
    [ASPECT_CONJUNCTION]    = { 220, 205, 175 },
    [ASPECT_OPPOSITION]     = { 190,  70,  55 },
    [ASPECT_TRINE]          = {   0, 150, 130 },
    [ASPECT_SQUARE]         = { 190,  70,  55 },
    [ASPECT_SEXTILE]        = {  45, 150, 122 },
    [ASPECT_SEMISEXTILE]    = {  70, 122, 108 },
    [ASPECT_SEMISQUARE]     = { 148,  84,  72 },
    [ASPECT_SESQUIQUADRATE] = { 148,  84,  72 },
    [ASPECT_QUINCUNX]       = { 138, 112, 160 },
};

static const char *orr__sign_names[12] = {
    "ARIES", "TAURUS", "GEMINI", "CANCER", "LEO", "VIRGO",
    "LIBRA", "SCORPIO", "SAGITTARIUS", "CAPRICORNUS", "AQUARIUS", "PISCES",
};

// Pip radius from apparent magnitude: how bright a body looks in the
// real sky is how big its dial marker draws. Clamped so the sun and
// moon don't flood the ring and Pluto stays a defiant speck.
static inline float orr__pip_r(double mag) {
    float r = 5.2f - 0.55f * (float)mag;
    if (r < 1.4f) r = 1.4f;
    if (r > 8.5f) r = 8.5f;
    return r;
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
    st->geo_azimuth = sc->solar_state.azimuth;
    st->geo_zenith = sc->solar_state.zenith;
    st->solar = &sc->solar_state;

    // Full-system ephemeris: clock time is local, the sky runs on UT.
    // Minute granularity — the fastest body (the Moon) moves 0.009
    // deg/minute, far below a hairline at dial scale.
    if (st->sys > 0.001) {
        double jd_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                     - t->config.timezone / 24.0;
        if (fabs(jd_ut - st->planets_jd) > 1.0 / 1440.0) {
            planets_compute(&st->planets, jd_ut);
            planets_sky_compute(&st->sky, &st->planets,
                                t->config.latitude, t->config.longitude);
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
        memcpy(light, helio_light, sizeof(light));
        ex = sphi * base_w * (1.0f - z);
        ey = -cphi * base_w * (1.0f - z);
        earth_r = 42.0f + z * 198.0f;   // full-zoom helio size: 240
    } else {
        float geo_rot[16], geo_light[3];
        globe_rotation(t->config.latitude, t->config.longitude, geo_rot);
        globe_sun_dir(st->geo_azimuth, st->geo_zenith, geo_light);

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
        float le_g[3], le_h[3], le[3];
        globe_mat_tmul_vec(geo_rot, geo_light, le_g);
        globe_mat_tmul_vec(helio_rot, helio_light, le_h);
        globe_vec_nlerp(le_g, le_h, m, le);
        globe_mat_mul_vec(rot, le, light);
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
    float sysf = 1.0f - (float)tempus_smoothstep(0.15, 0.60, ss);
    if (ss > 0.001f) {
        const PlanetsNow *pn = &st->planets;
        float a_ring   = (float)tempus_smoothstep(0.05, 0.45, ss);
        float a_planet = (float)tempus_smoothstep(0.20, 0.60, ss);
        float a_zod    = (float)tempus_smoothstep(0.40, 0.80, ss);
        float a_sight  = (float)tempus_smoothstep(0.60, 0.95, ss);
        float a_web    = (float)tempus_smoothstep(0.70, 1.00, ss);
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

        // Orbit rings (Earth's is the calendar wheel itself)
        if (a_ring > 0.001f) {
            d->alpha = base_alpha * a_ring;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.13f));
            for (int p = 0; p < PL_COUNT; p++) {
                if (p == PL_EARTH) continue;
                draw_circle_stroked(d, 0, 0, orr__orbit_r(p, wheel_R), 1.0f);
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
            draw_set_color(d, dc_scale(s->medium_grey, 0.60f));
            for (int i = 0; i < 12; i++) {
                float ang = (float)((0.75 + (i * 30.0 + 15.0) / 360.0)
                                    * 2.0 * M_PI);
                draw_text_curved(d, FONT_month, 0, 0, ORR_ZODIAC_TEXT,
                                 ang, orr__sign_names[i], 1.6f, 0.85f);
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
                // Control point puts the curve through the bead at t=0.5
                float p1x = 2.0f * bx2 - 0.5f * (ex + mpx[b]);
                float p1y = 2.0f * by2 - 0.5f * (ey + mpy[b]);
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
                const uint8_t *c = orr__aspect_col[A->kind];
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
                    draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                          c[2] / 255.0f, 0.8f));
                    draw_arc_filled(d, 0, 0, ORR_WEB_R + 10.0f,
                                    ORR_WEB_R + 11.5f, aa0, aa1, 10);
                } else {
                    float wdt = 0.6f + 1.8f * sharp;
                    d->alpha = base_alpha * a_web
                             * (0.05f + 0.60f * sharp);
                    draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                          c[2] / 255.0f, 1.0f));
                    draw_line(d, mpx[A->a], mpy[A->a],
                              mpx[A->b], mpy[A->b], wdt);
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

        // Planet beads + engraved radial labels
        if (a_planet > 0.001f) {
            for (int p = 0; p < PL_COUNT; p++) {
                if (p == PL_EARTH) continue;
                d->alpha = base_alpha * a_planet;
                draw_set_color(d, dc_u8(orr__planet[p].r, orr__planet[p].g,
                                        orr__planet[p].b));
                draw_circle_filled(d, ppx[p], ppy[p], orr__planet[p].size);
                if (p == PL_SATURN) {
                    draw_set_color(d, dca(orr__planet[p].r / 255.0f,
                                          orr__planet[p].g / 255.0f,
                                          orr__planet[p].b / 255.0f, 0.45f));
                    draw_circle_stroked(d, ppx[p], ppy[p],
                                        orr__planet[p].size * 1.75f, 1.0f);
                }
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
                d->alpha = base_alpha * a_planet * 0.85f;
                draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.85f));
                draw_text_curved(d, FONT_date, 0, 0, lr, lth,
                                 orr__planet[p].name, ltrack, lscale);
            }
            d->alpha = base_alpha;
        }
    }

    // Helio underlay: sun at wheel center + orbit radial to the globe
    if (helio_a > 0.001f) {
        // The sun gains a little presence once it anchors the whole system
        float sgrow = 1.0f + 0.45f * (float)tempus_smoothstep(0.2, 0.7, ss);
        d->alpha = base_alpha * helio_a;
        draw_set_color(d, dc_u8(196, 126, 16));
        draw_circle_filled(d, sun_x, sun_y, 22.0f * sgrow);
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.35f));
        draw_circle_stroked(d, sun_x, sun_y, 28.0f * sgrow, 1.0f);
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
    GlobeCmd *g = draw_globe_slot(d, ex, ey, earth_r);
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
    {
        float coast_a = (0.16f + 0.34f * m) * sysf;
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

    // Limb outline: bounds the map at the silhouette so coastline vectors
    // meet a rim instead of floating unconnected against the sky
    draw_set_color(d, dca(0.63f, 0.58f, 0.50f, 0.10f + 0.35f * m));
    draw_circle_stroked(d, ex, ey, earth_r, 1.0f);

    // ================= OVER THE GLOBE =================

    // Dial furniture: outer ring, anchored at the dial. (The old daylight
    // arc is retired — the observer-latitude ring on the globe now shows
    // day length directly, thick in daylight and hairline in night.)
    if (geo_a > 0.001f) {
        d->alpha = base_alpha * geo_a;
        draw_set_color(d, dc_scale(s->sunrise_lit, 0.8f));
        draw_circle_stroked(d, 0, dial_y, dial_r + 12.0f, 1.0f);

        // Moon aperture rim at 6 o'clock
        draw_set_color(d, dca(0.45f, 0.44f, 0.42f, 0.5f));
        draw_circle_stroked(d, 0, -dial_y, 62.0f, 1.0f);
    }

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

        // Moon orbit ring (the moon itself is a shared element below)
        d->alpha = base_alpha * helio_a;
        draw_set_color(d, dca(0.6f, 0.58f, 0.54f, 0.20f));
        draw_circle_stroked(d, ex, ey, earth_r * 1.55f, 1.0f);
    }

    // ---- Shared elements: continuous across the morph, never fade ----
    d->alpha = base_alpha;

    // Sun marker + hand line: at m=0 the light-based position equals the
    // dial marker exactly (both are the subsolar orthographic projection).
    // When the subsolar point is behind the planet, clamp to the sunward
    // limb, dimmed — "the sun is beyond this horizon".
    {
        float lx = light[0], ly = light[1], lz = light[2];
        DrawColor sun_c = s->sunrise_handle;
        if (lz < 0) {
            float n = sqrtf(lx * lx + ly * ly);
            if (n > 1e-4f) { lx /= n; ly /= n; }
            sun_c = dc_scale(sun_c, 0.5f);
        }
        // In helio the real sun sits at the wheel center, so the marker
        // lifts well off the globe toward it — a bead on the sun line.
        // Its tether starts at the PERIMETER, not the globe center.
        float lift = 1.0f + 0.9f * m;
        float px = ex + lx * earth_r * lift, py = ey + ly * earth_r * lift;
        float mag = sqrtf(lx * lx + ly * ly);
        // Marker and tether are globe furniture — gone at system scale
        // (the sun itself and its sight-line carry the direction there)
        d->alpha = base_alpha * sysf;
        if (mag * lift > 1.02f && mag > 1e-4f) {
            float sx0 = ex + (lx / mag) * earth_r;
            float sy0 = ey + (ly / mag) * earth_r;
            draw_set_color(d, dca(0.75f, 0.75f, 0.75f, 0.35f));
            draw_line(d, sx0, sy0, px, py, 1.0f);
        }
        draw_set_color(d, sun_c);
        draw_circle_filled(d, px, py, s->sun_size * earth_r / dial_r);
        d->alpha = base_alpha;

        // Publish for hit-testing (render-side cache; see scene_pointer)
        OrreryViewState *wst = (OrreryViewState *)(uintptr_t)buf;
        wst->bead_x = px;
        wst->bead_y = py;
        wst->bead_hit = s->sun_size * earth_r / dial_r + 16.0f;
        wst->glob_x = ex;
        wst->glob_y = ey;
        wst->glob_r = earth_r;
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
        double b_deg = ph * 360.0;
        if (ss > 0.001f) {
            // In the system view the true geocentric elongation takes
            // over, so the moon hangs at its real zodiac longitude — the
            // sight-line and the aspect web land on the same point
            double elo = planets_lon_diff(st->planets.geo_lon[BODY_MOON],
                                          st->planets.geo_lon[BODY_SUN]);
            b_deg += planets_lon_diff(elo, b_deg)
                   * tempus_smoothstep(0.2, 0.7, ss);
        }
        float b = (float)(b_deg * M_PI / 180.0);

        // Geo endpoint: the 6 o'clock aperture, phase-frame light
        float gx2 = 0.0f, gy2 = -dial_y, gr2 = 52.0f;
        float gl2[3] = { sinf(b), 0.0f, -cosf(b) };

        // Helio endpoint: pure ecliptic sun (helio_light, untouched by
        // the morph) and the helio earth arrangement AT THE CURRENT ZOOM
        // (center/240 when z rides with m; a bead on the wheel when the
        // system flight leaves z at 0) — matching the earth's own target
        float hex = (m >= 0.999f) ? ex : sphi * base_w * (1.0f - z);
        float hey = (m >= 0.999f) ? ey : -cphi * base_w * (1.0f - z);
        float her = (m >= 0.999f) ? earth_r : 42.0f + z * 198.0f;
        float plm = sqrtf(helio_light[0] * helio_light[0]
                        + helio_light[1] * helio_light[1]);
        float slx = (plm > 1e-4f) ? helio_light[0] / plm : 0.0f;
        float sly = (plm > 1e-4f) ? helio_light[1] / plm : -1.0f;
        // Elongation advances CCW on screen — the display frame's
        // physical rotation sense (the surface clock's hour labels run
        // CCW; a first-quarter moon must sit over the 18 tick, waning
        // over the morning side). CW here was the inversion that made
        // geo and helio phases disagree.
        float mdx = slx * cosf(b) + sly * sinf(b);
        float mdy = -slx * sinf(b) + sly * cosf(b);
        if (ss > 0.001f) {
            // The system stage is governed by ecliptic longitude. The
            // spin-sense elongation above lives in the globe's PROPER
            // frame; the wheel maps orbits MIRRORED (the year runs
            // clockwise), and the zodiac dial, ring marker and
            // sight-line live in the wheel's world — so the moon swings
            // to its true sky direction as the system arrives.
            float sw = (float)tempus_smoothstep(0.2, 0.7, ss);
            float tdx, tdy;
            orr__ecl_dir(st->planets.geo_lon[BODY_MOON], &tdx, &tdy);
            float a0 = atan2f(mdy, mdx), a1 = atan2f(tdy, tdx);
            float da = a1 - a0;
            while (da > (float)M_PI) da -= 2.0f * (float)M_PI;
            while (da < -(float)M_PI) da += 2.0f * (float)M_PI;
            float aa = a0 + da * sw;
            mdx = cosf(aa);
            mdy = sinf(aa);
        }
        float hx2 = hex + mdx * her * 1.55f;
        float hy2 = hey + mdy * her * 1.55f;
        // At system scale the moon shrinks toward a bead beside its planet
        float hr2 = 22.0f * (1.0f - 0.55f
                             * (float)tempus_smoothstep(0.2, 0.7, ss));
        float hl2[3] = { helio_light[0], helio_light[1], helio_light[2] };
        float edx = -mdx, edy = -mdy;   // direction moon -> earth, screen

        float mmx = gx2 * (1 - m) + hx2 * m;
        float mmy = gy2 * (1 - m) + hy2 * m;
        float mmr = gr2 * (1 - m) + hr2 * m;
        // Light blend WITHOUT the shortest-path sign flip: the two frames
        // (phase-view vs orbital sun) can be arbitrarily far apart, and
        // the flip hands the moon a negated sun for half of every orbit.
        float ml[3];
        float mn = 0;
        for (int i = 0; i < 3; i++) {
            ml[i] = gl2[i] * (1 - m) + hl2[i] * m;
            mn += ml[i] * ml[i];
        }
        mn = sqrtf(mn);
        if (mn > 1e-3f) {
            for (int i = 0; i < 3; i++) ml[i] /= mn;
        } else {
            memcpy(ml, m > 0.5f ? hl2 : gl2, sizeof(ml));
        }

        GlobeCmd *gm = draw_globe_slot(d, mmx, mmy, mmr);
        if (gm) {
            // Tidal locking: geo shows the near side (lon 0 centered,
            // north up); helio looks down the lunar pole with the
            // near-side meridian aimed at Earth, turning as it orbits.
            float rot_geo[16], rot_helio[16];
            globe_rotation(0, 0, rot_geo);
            memset(rot_helio, 0, sizeof(rot_helio));
            // rows: (edx,edy,0), (edy,-edx,0), (0,0,1) — det -1 display
            rot_helio[0] = edx;  rot_helio[4] = edy;
            rot_helio[1] = edy;  rot_helio[5] = -edx;
            rot_helio[10] = 1.0f;
            rot_helio[15] = 1.0f;
            globe_rot_slerp_cont(rot_geo, rot_helio, m,
                                 stw->moon_pq, &stw->moon_pq_valid, gm->rot);
            memcpy(gm->light, ml, sizeof(ml));
            gm->land = true;      // sample the lunar albedo
            gm->tex_id = 1;
            // Observer direction for phase legibility: the viewer in geo
            // (identity — whole disc counts), Earth in helio; the morph
            // sweeps between them
            gm->aux_dir[0] = edx * m;
            gm->aux_dir[1] = edy * m;
            gm->aux_dir[2] = 1.0f - m;
            gm->aux_dir[3] = 1.0f;
            gm->grid_boost = 0.0f;
            gm->obs_lat = 999.0f;
            gm->day_col[0] = 0.58f; gm->day_col[1] = 0.55f;
            gm->day_col[2] = 0.49f; gm->day_col[3] = 1.0f;
            // Dark indigo night side — the full disc stays legible, and
            // the albedo ghosting through reads as earthshine
            gm->night_col[0] = 0.10f; gm->night_col[1] = 0.105f;
            gm->night_col[2] = 0.23f; gm->night_col[3] = 1.0f;
        }
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
