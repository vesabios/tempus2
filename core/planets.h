// planets.h — Solar-system ephemeris + aspect geometry.
// Pure C, no rendering. The system view's data source: where every major
// body is (heliocentric, for the orrery) and where it appears from Earth
// (geocentric ecliptic longitude, for the zodiac dial and the aspect web).
//
// Planetary positions use JPL's approximate Keplerian elements (Standish,
// "Approximate Positions of the Major Planets", Table 1: mean elements +
// centennial rates, valid 1800-2050, arcminute-class accuracy) — the same
// precision-to-weight philosophy as spa.c, two orders of magnitude lighter.
// Pluto's row is from the original memo (JPL dropped it from the current
// page along with its planethood; the elements still work fine).
//
// The Moon uses the Astronomical Almanac's low-precision geocentric series
// (~0.3 deg in longitude — an order of magnitude tighter than any aspect
// orb). The Sun's geocentric longitude is Earth's heliocentric + 180.
//
// All output longitudes are ecliptic-of-date, degrees — the tropical
// frame, where 0 Aries IS the current vernal equinox. This is the frame
// the zodiac (and the whole instrument: yule-at-top = current solstice)
// lives in. The Kepler elements are J2000; general precession in
// longitude (~1.397 deg/century) carries them to the equinox of date.
// Time is UT Julian date.

#ifndef PLANETS_H
#define PLANETS_H

#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- Bodies ----
// Display/aspect bodies in traditional order: luminaries, then outward.

typedef enum {
    BODY_SUN = 0,
    BODY_MOON,
    BODY_MERCURY,
    BODY_VENUS,
    BODY_MARS,
    BODY_JUPITER,
    BODY_SATURN,
    BODY_URANUS,
    BODY_NEPTUNE,
    BODY_PLUTO,
    BODY_COUNT
} PlanetBody;

// Kepler-table planets, sun-orbiting, in distance order (Earth = EMB).
enum {
    PL_MERCURY = 0, PL_VENUS, PL_EARTH, PL_MARS, PL_JUPITER,
    PL_SATURN, PL_URANUS, PL_NEPTUNE, PL_PLUTO, PL_COUNT
};

// ---- Keplerian elements (JPL Table 1, J2000 ecliptic, epoch J2000) ----
// Per planet: a[au]  e  I[deg]  L[deg]  varpi[deg]  Omega[deg], value then
// rate per Julian century.

static const double planets__elem[PL_COUNT][12] = {
    // Mercury
    {  0.38709927,  0.00000037,  0.20563593,  0.00001906,
       7.00497902, -0.00594749, 252.25032350, 149472.67411175,
      77.45779628,  0.16047689,  48.33076593, -0.12534081 },
    // Venus
    {  0.72333566,  0.00000390,  0.00677672, -0.00004107,
       3.39467605, -0.00078890, 181.97909950, 58517.81538729,
     131.60246718,  0.00268329,  76.67984255, -0.27769418 },
    // Earth-Moon barycenter
    {  1.00000261,  0.00000562,  0.01671123, -0.00004392,
      -0.00001531, -0.01294668, 100.46457166, 35999.37244981,
     102.93768193,  0.32327364,   0.0,         0.0 },
    // Mars
    {  1.52371034,  0.00001847,  0.09339410,  0.00007882,
       1.84969142, -0.00813131,  -4.55343205, 19140.30268499,
     -23.94362959,  0.44441088,  49.55953891, -0.29257343 },
    // Jupiter
    {  5.20288700, -0.00011607,  0.04838624, -0.00013253,
       1.30439695, -0.00183714,  34.39644051,  3034.74612775,
      14.72847983,  0.21252668, 100.47390909,  0.20469106 },
    // Saturn
    {  9.53667594, -0.00125060,  0.05386179, -0.00050991,
       2.48599187,  0.00193609,  49.95424423,  1222.49362201,
      92.59887831, -0.41897216, 113.66242448, -0.28867794 },
    // Uranus
    { 19.18916464, -0.00196176,  0.04725744, -0.00004397,
       0.77263783, -0.00242939, 313.23810451,   428.48202785,
     170.95427630,  0.40805281,  74.01692503,  0.04240589 },
    // Neptune
    { 30.06992276,  0.00026291,  0.00859048,  0.00005105,
       1.77004347,  0.00035372, -55.12002969,   218.45945325,
      44.96476227, -0.32241464, 131.78422574, -0.00508664 },
    // Pluto (from the original memo's Table 1)
    { 39.48211675, -0.00031596,  0.24882730,  0.00005170,
      17.14001206,  0.00004818, 238.92903833,   145.20780515,
     224.06891629, -0.04062942, 110.30393684, -0.01183482 },
};

static inline double planets__wrap360(double d) {
    d = fmod(d, 360.0);
    return d < 0 ? d + 360.0 : d;
}

// General precession in ecliptic longitude since J2000, degrees
static inline double planets__precession(double jd_ut) {
    double T = (jd_ut - 2451545.0) / 36525.0;
    return 1.39697 * T;
}

// Signed smallest difference a-b, in (-180, 180]
static inline double planets_lon_diff(double a, double b) {
    double d = fmod(a - b, 360.0);
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    return d;
}

// Kepler's equation E - e sinE = M by Newton iteration (radians)
static inline double planets__kepler(double M, double e) {
    double E = M + e * sin(M);
    for (int i = 0; i < 10; i++) {
        double dE = (M - (E - e * sin(E))) / (1.0 - e * cos(E));
        E += dE;
        if (fabs(dE) < 1e-9) break;
    }
    return E;
}

// Heliocentric J2000-ecliptic rectangular position of a table planet, au
static inline void planets_helio_xyz(int pl, double jd_ut, double out[3]) {
    const double *el = planets__elem[pl];
    double T = (jd_ut - 2451545.0) / 36525.0;

    double a     = el[0] + el[1] * T;
    double e     = el[2] + el[3] * T;
    double I     = (el[4] + el[5] * T) * M_PI / 180.0;
    double L     = el[6] + el[7] * T;
    double varpi = el[8] + el[9] * T;
    double Omega = (el[10] + el[11] * T) * M_PI / 180.0;

    double w = (varpi - (el[10] + el[11] * T)) * M_PI / 180.0; // arg. perihelion
    double M = planets__wrap360(L - varpi) * M_PI / 180.0;
    double E = planets__kepler(M, e);

    // Orbital-plane coordinates (perihelion along +x')
    double xp = a * (cos(E) - e);
    double yp = a * sqrt(1.0 - e * e) * sin(E);

    // Rotate by argument of perihelion, inclination, ascending node
    double cw = cos(w), sw = sin(w);
    double cO = cos(Omega), sO = sin(Omega);
    double cI = cos(I), sI = sin(I);
    out[0] = (cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp;
    out[1] = (cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp;
    out[2] = (sw * sI) * xp + (cw * sI) * yp;
}

// Heliocentric ecliptic-of-date longitude (deg) and distance (au)
static inline double planets_helio_lon(int pl, double jd_ut, double *r_au) {
    double v[3];
    planets_helio_xyz(pl, jd_ut, v);
    if (r_au) *r_au = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    return planets__wrap360(atan2(v[1], v[0]) * 180.0 / M_PI
                            + planets__precession(jd_ut));
}

// Geocentric ecliptic longitude of the Moon (deg) — Astronomical Almanac
// low-precision series, ~0.3 deg. T in Julian centuries from J2000 (UT).
static inline double planets_moon_lon(double jd_ut) {
    double T = (jd_ut - 2451545.0) / 36525.0;
    double d2r = M_PI / 180.0;
    double lam = 218.32 + 481267.881 * T
        + 6.29 * sin((135.0 + 477198.87 * T) * d2r)
        - 1.27 * sin((259.3 - 413335.36 * T) * d2r)
        + 0.66 * sin((235.7 + 890534.22 * T) * d2r)
        + 0.21 * sin((269.9 + 954397.74 * T) * d2r)
        - 0.19 * sin((357.5 + 35999.05 * T) * d2r)
        - 0.11 * sin((186.5 + 966404.03 * T) * d2r);
    return planets__wrap360(lam);
}

// ---- The whole sky at one moment ----

typedef struct {
    double jd_ut;
    // Sun-orbiting bodies, PL_* order: for placing beads on orbit rings
    double helio_lon[PL_COUNT];   // deg
    double helio_r[PL_COUNT];     // au
    // All display bodies, BODY_* order: for the zodiac dial + aspects
    double geo_lon[BODY_COUNT];   // deg
    bool   retro[BODY_COUNT];     // apparent longitude currently decreasing
} PlanetsNow;

// BODY_* index -> PL_* index (-1 for Sun/Moon)
static inline int planets_body_pl(int body) {
    switch (body) {
        case BODY_MERCURY: return PL_MERCURY;
        case BODY_VENUS:   return PL_VENUS;
        case BODY_MARS:    return PL_MARS;
        case BODY_JUPITER: return PL_JUPITER;
        case BODY_SATURN:  return PL_SATURN;
        case BODY_URANUS:  return PL_URANUS;
        case BODY_NEPTUNE: return PL_NEPTUNE;
        case BODY_PLUTO:   return PL_PLUTO;
        default:           return -1;
    }
}

// Geocentric ecliptic longitudes of every body at jd_ut (deg, BODY_* order)
static inline void planets__geo_lons(double jd_ut, double out[BODY_COUNT]) {
    double e[3];
    double prec = planets__precession(jd_ut);   // J2000 -> of-date
    planets_helio_xyz(PL_EARTH, jd_ut, e);
    out[BODY_SUN]  = planets__wrap360(atan2(-e[1], -e[0]) * 180.0 / M_PI + prec);
    out[BODY_MOON] = planets_moon_lon(jd_ut);   // series is already of-date
    for (int b = BODY_MERCURY; b < BODY_COUNT; b++) {
        double p[3];
        planets_helio_xyz(planets_body_pl(b), jd_ut, p);
        out[b] = planets__wrap360(atan2(p[1] - e[1], p[0] - e[0]) * 180.0 / M_PI
                                  + prec);
    }
}

static inline void planets_compute(PlanetsNow *pn, double jd_ut) {
    pn->jd_ut = jd_ut;
    for (int pl = 0; pl < PL_COUNT; pl++)
        pn->helio_lon[pl] = planets_helio_lon(pl, jd_ut, &pn->helio_r[pl]);
    planets__geo_lons(jd_ut, pn->geo_lon);

    // Retrograde: apparent-longitude rate by central difference over a day
    double before[BODY_COUNT], after[BODY_COUNT];
    planets__geo_lons(jd_ut - 0.5, before);
    planets__geo_lons(jd_ut + 0.5, after);
    for (int b = 0; b < BODY_COUNT; b++)
        pn->retro[b] = planets_lon_diff(after[b], before[b]) < 0.0;
}

// ---- Aspects ----
// An aspect is a geocentric angular separation matching one of the sacred
// divisions of the circle, within an orb (tolerance). Orbs are disjoint by
// construction, so a pair matches at most one aspect.

typedef enum {
    ASPECT_CONJUNCTION = 0,   // 0
    ASPECT_OPPOSITION,        // 180
    ASPECT_TRINE,             // 120
    ASPECT_SQUARE,            // 90
    ASPECT_SEXTILE,           // 60
    ASPECT_SEMISEXTILE,       // 30
    ASPECT_SEMISQUARE,        // 45
    ASPECT_SESQUIQUADRATE,    // 135
    ASPECT_QUINCUNX,          // 150
    ASPECT_TYPE_COUNT
} AspectKind;

typedef struct {
    float angle;    // exact separation, deg
    float orb;      // max deviation, deg
    bool  major;    // Ptolemaic five vs minor
} AspectDef;

static const AspectDef planets_aspect_defs[ASPECT_TYPE_COUNT] = {
    [ASPECT_CONJUNCTION]    = {   0.0f, 8.0f, true  },
    [ASPECT_OPPOSITION]     = { 180.0f, 8.0f, true  },
    [ASPECT_TRINE]          = { 120.0f, 7.0f, true  },
    [ASPECT_SQUARE]         = {  90.0f, 7.0f, true  },
    [ASPECT_SEXTILE]        = {  60.0f, 5.0f, true  },
    [ASPECT_SEMISEXTILE]    = {  30.0f, 2.5f, false },
    [ASPECT_SEMISQUARE]     = {  45.0f, 2.5f, false },
    [ASPECT_SESQUIQUADRATE] = { 135.0f, 2.5f, false },
    [ASPECT_QUINCUNX]       = { 150.0f, 3.0f, false },
};

typedef struct {
    uint8_t a, b;      // BODY_* indices, a < b
    uint8_t kind;      // AspectKind
    float   err;       // |separation - exact|, deg
    float   strength;  // 1 at exact, 0 at orb edge
} Aspect;

#define PLANETS_MAX_ASPECTS (BODY_COUNT * (BODY_COUNT - 1) / 2)

static inline int planets_find_aspects(const double geo_lon[BODY_COUNT],
                                       Aspect *out, int max) {
    int n = 0;
    for (int a = 0; a < BODY_COUNT; a++) {
        for (int b = a + 1; b < BODY_COUNT; b++) {
            double sep = fabs(planets_lon_diff(geo_lon[a], geo_lon[b]));
            for (int k = 0; k < ASPECT_TYPE_COUNT; k++) {
                double err = fabs(sep - planets_aspect_defs[k].angle);
                if (err <= planets_aspect_defs[k].orb) {
                    if (n < max) {
                        out[n].a = (uint8_t)a;
                        out[n].b = (uint8_t)b;
                        out[n].kind = (uint8_t)k;
                        out[n].err = (float)err;
                        out[n].strength =
                            1.0f - (float)err / planets_aspect_defs[k].orb;
                        n++;
                    }
                    break;   // orbs are disjoint
                }
            }
        }
    }
    return n;
}

#endif // PLANETS_H
