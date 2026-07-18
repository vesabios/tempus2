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

// Geocentric ecliptic latitude of the Moon (deg) — companion series
static inline double planets_moon_lat(double jd_ut) {
    double T = (jd_ut - 2451545.0) / 36525.0;
    double d2r = M_PI / 180.0;
    return 5.13 * sin((93.3 + 483202.03 * T) * d2r)
         + 0.28 * sin((228.2 + 960400.87 * T) * d2r)
         - 0.28 * sin((318.3 + 6003.18 * T) * d2r)
         - 0.17 * sin((217.6 - 407332.20 * T) * d2r);
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
    double mag[BODY_COUNT];       // apparent visual magnitude
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

    // Apparent magnitudes (Harris/Meeus): absolute term + distance
    // dimming + phase darkening. r = sun distance, D = earth distance,
    // i = phase angle at the planet. Saturn's ring-tilt term is skipped
    // (up to ~0.5 mag) — this feeds pip sizes, not photometry.
    {
        double e[3];
        planets_helio_xyz(PL_EARTH, jd_ut, e);
        double R = pn->helio_r[PL_EARTH];
        // {m0, phase c1, c2, c3} per PL_* planet
        static const double ph[PL_COUNT][4] = {
            { -0.42, 3.80e-2, -2.73e-4, 2.00e-6 },   // Mercury
            { -4.40, 9.00e-4,  2.39e-4, -6.5e-7 },   // Venus
            {  0.0,  0.0,      0.0,      0.0    },   // (Earth)
            { -1.52, 1.60e-2,  0.0,      0.0    },   // Mars
            { -9.40, 5.00e-3,  0.0,      0.0    },   // Jupiter
            { -8.88, 4.40e-2,  0.0,      0.0    },   // Saturn (no rings)
            { -7.19, 0.0,      0.0,      0.0    },   // Uranus
            { -6.87, 0.0,      0.0,      0.0    },   // Neptune
            { -1.01, 0.0,      0.0,      0.0    },   // Pluto
        };
        pn->mag[BODY_SUN] = -26.74;
        // Moon: full is -12.7; phase angle ~ 180 - elongation
        {
            double el = fabs(planets_lon_diff(pn->geo_lon[BODY_MOON],
                                              pn->geo_lon[BODY_SUN]));
            double a = 180.0 - el;
            pn->mag[BODY_MOON] = -12.73 + 0.026 * a
                               + 4.0e-9 * a * a * a * a;
        }
        for (int b = BODY_MERCURY; b < BODY_COUNT; b++) {
            int pl = planets_body_pl(b);
            double p[3];
            planets_helio_xyz(pl, jd_ut, p);
            double dx = p[0] - e[0], dy = p[1] - e[1], dz = p[2] - e[2];
            double D = sqrt(dx * dx + dy * dy + dz * dz);
            double r = pn->helio_r[pl];
            double ci = (r * r + D * D - R * R) / (2.0 * r * D);
            if (ci > 1.0) ci = 1.0;
            if (ci < -1.0) ci = -1.0;
            double i = acos(ci) * 180.0 / M_PI;
            pn->mag[b] = ph[pl][0] + 5.0 * log10(r * D)
                       + ph[pl][1] * i + ph[pl][2] * i * i
                       + ph[pl][3] * i * i * i;
        }
    }
}

// ---- The local sky: who is above the horizon ----
// Geocentric ecliptic -> equatorial -> horizon, via Greenwich sidereal
// time. Answers "which of these bodies is up right now" for an observer
// at (lat, lon). Moon parallax (<1 deg) is ignored — this feeds a dial,
// not a telescope.

// Greenwich mean sidereal time, degrees
static inline double planets__gmst(double jd_ut) {
    double D = jd_ut - 2451545.0;
    double T = D / 36525.0;
    return planets__wrap360(280.46061837 + 360.98564736629 * D
                            + 0.000387933 * T * T);
}

// Azimuth (deg from north, eastward) and altitude (deg) of a sky
// position given in ecliptic-of-date lon/lat
static inline void planets_sky_azalt(double lon_deg, double lat_deg,
                                     double jd_ut,
                                     double obs_lat_deg,
                                     double obs_lon_deg,
                                     double *az_deg, double *alt_deg) {
    double d2r = M_PI / 180.0;
    double T = (jd_ut - 2451545.0) / 36525.0;
    double eps = (23.439291 - 0.0130042 * T) * d2r;
    double lam = lon_deg * d2r, bet = lat_deg * d2r;

    double sdec = sin(bet) * cos(eps) + cos(bet) * sin(eps) * sin(lam);
    double dec = asin(sdec);
    double ra = atan2(sin(lam) * cos(eps) - tan(bet) * sin(eps), cos(lam));

    double lst = (planets__gmst(jd_ut) + obs_lon_deg) * d2r;
    double H = lst - ra;   // hour angle, positive west
    double phi = obs_lat_deg * d2r;

    // Horizontal frame: north, east, up
    double xN = -cos(dec) * cos(H) * sin(phi) + sdec * cos(phi);
    double xE = -cos(dec) * sin(H);
    double xU = sin(phi) * sdec + cos(phi) * cos(dec) * cos(H);
    if (xU > 1.0) xU = 1.0;
    if (xU < -1.0) xU = -1.0;
    *alt_deg = asin(xU) / d2r;
    *az_deg = planets__wrap360(atan2(xE, xN) / d2r);
}

static inline double planets_sky_altitude(double lon_deg, double lat_deg,
                                          double jd_ut,
                                          double obs_lat_deg,
                                          double obs_lon_deg) {
    double az, alt;
    planets_sky_azalt(lon_deg, lat_deg, jd_ut, obs_lat_deg, obs_lon_deg,
                      &az, &alt);
    return alt;
}

// Geocentric ecliptic lon/lat of one body (deg) — the Moon moves 13
// deg/day, so observability sweeps recompute this per sample
static inline void planets__body_lonlat(int b, double jd_ut,
                                        double *lon, double *lat) {
    if (b == BODY_MOON) {
        *lon = planets_moon_lon(jd_ut);
        *lat = planets_moon_lat(jd_ut);
        return;
    }
    double e[3];
    planets_helio_xyz(PL_EARTH, jd_ut, e);
    double prec = planets__precession(jd_ut);
    if (b == BODY_SUN) {
        *lon = planets__wrap360(atan2(-e[1], -e[0]) * 180.0 / M_PI + prec);
        *lat = 0.0;
        return;
    }
    double p[3];
    planets_helio_xyz(planets_body_pl(b), jd_ut, p);
    double dx = p[0] - e[0], dy = p[1] - e[1], dz = p[2] - e[2];
    double dr = sqrt(dx * dx + dy * dy + dz * dz);
    *lon = planets__wrap360(atan2(dy, dx) * 180.0 / M_PI + prec);
    *lat = (dr > 1e-9) ? asin(dz / dr) * 180.0 / M_PI : 0.0;
}

// A body is "observable" when it clears the horizon while the sky is
// dark enough FOR THAT BODY. One threshold cannot serve all: the moon
// shines through any twilight, Venus and Mercury are twilight objects
// by definition (Mercury never sees a fully dark high-latitude sky),
// and the dim outer planets need real darkness. Planets near solar
// conjunction fail their test for weeks to months — the season-scale
// answer to "can I see it from here", not the hour-scale one.
typedef struct { float sun_below; float min_alt; } PlanetsObsReq;

static inline PlanetsObsReq planets__obs_req(int b) {
    switch (b) {
        case BODY_MOON:    return (PlanetsObsReq){  0.0f, 2.0f };
        case BODY_MERCURY: return (PlanetsObsReq){ -5.0f, 4.0f };
        case BODY_VENUS:   return (PlanetsObsReq){ -4.0f, 3.0f };
        default:           return (PlanetsObsReq){ -8.0f, 5.0f };
    }
}

typedef struct {
    double alt[BODY_COUNT];    // altitude right now, deg
    // Altitude of the ecliptic itself, sampled every 5 deg of longitude
    // (72 samples): the horizon's cut through the zodiac dial
    float  ecl_alt[72];
    // Season-scale visibility: best altitude reached during dark sky
    // over the coming 24h, and the verdict
    float  best_dark_alt[BODY_COUNT];
    bool   observable[BODY_COUNT];
} PlanetsSky;

static inline void planets_sky_compute(PlanetsSky *sky, const PlanetsNow *pn,
                                       double obs_lat_deg,
                                       double obs_lon_deg) {
    double jd_ut = pn->jd_ut;

    for (int b = 0; b < BODY_COUNT; b++) {
        double lon, lat;
        planets__body_lonlat(b, jd_ut, &lon, &lat);
        sky->alt[b] = planets_sky_altitude(lon, lat, jd_ut,
                                           obs_lat_deg, obs_lon_deg);
    }
    for (int i = 0; i < 72; i++)
        sky->ecl_alt[i] = (float)planets_sky_altitude(i * 5.0, 0.0, jd_ut,
                                                      obs_lat_deg,
                                                      obs_lon_deg);

    // Observability sweep: the coming 24h in 15-minute steps
    for (int b = 0; b < BODY_COUNT; b++)
        sky->best_dark_alt[b] = -90.0f;
    for (int s = 0; s < 96; s++) {
        double jd = jd_ut + s / 96.0;
        double slon, slat;
        planets__body_lonlat(BODY_SUN, jd, &slon, &slat);
        double sun_alt = planets_sky_altitude(slon, slat, jd,
                                              obs_lat_deg, obs_lon_deg);
        for (int b = BODY_MOON; b < BODY_COUNT; b++) {
            if (sun_alt > planets__obs_req(b).sun_below) continue;
            double lon, lat;
            planets__body_lonlat(b, jd, &lon, &lat);
            double alt = planets_sky_altitude(lon, lat, jd,
                                              obs_lat_deg, obs_lon_deg);
            if (alt > sky->best_dark_alt[b])
                sky->best_dark_alt[b] = (float)alt;
        }
    }
    sky->observable[BODY_SUN] = true;   // trivially: it is the daytime
    for (int b = BODY_MOON; b < BODY_COUNT; b++)
        sky->observable[b] =
            sky->best_dark_alt[b] > planets__obs_req(b).min_alt;
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
