// test_planets.c — validation harness for core/planets.h
//
// Checks the Kepler pipeline against independent truths:
//   1. Sun geocentric longitude vs NREL SPA (high-precision, independent
//      theory) across 25 years — validates the Earth row + the whole
//      element->longitude pipeline.
//   2. Eclipses: a solar eclipse is an exact Sun-Moon conjunction, a lunar
//      eclipse an exact opposition — validates the lunar series absolutely.
//   3. Elongation bounds: Mercury never strays >28.5 deg from the Sun,
//      Venus never >47.5 — validates inner-planet geometry over a decade.
//   4. Mars retrograde cadence (~every 26 months) — validates apparent
//      motion, i.e. the Earth-relative vector, not just longitudes.
//   5. Current-sky spot checks for the slow movers (their sign placement
//      is public knowledge and moves ~1 deg/yr — a table typo shows up as
//      being in the wrong sign entirely).
//
// Build & run: make test_planets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/planets.h"
#include "../core/spa.h"

static int g_fail = 0;

static void check(bool ok, const char *what, double got, double want, double tol) {
    if (!ok) g_fail++;
    printf("  [%s] %-46s got %10.4f  want %10.4f  (tol %.3f)\n",
           ok ? "ok" : "FAIL", what, got, want, tol);
}

// Julian date from calendar date + UT time (standard algorithm)
static double jd_ut(int y, int m, int d, double hour_ut) {
    if (m <= 2) { y -= 1; m += 12; }
    int A = y / 100;
    int B = 2 - A + A / 4;
    return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1))
         + d + B - 1524.5 + hour_ut / 24.0;
}

// Sun geocentric longitude from SPA for a UT moment
static double spa_sun_lon(int y, int m, int d, double hour_ut) {
    spa_data s;
    memset(&s, 0, sizeof(s));
    s.year = y; s.month = m; s.day = d;
    s.hour = (int)hour_ut;
    s.minute = (int)((hour_ut - s.hour) * 60.0);
    s.second = 0;
    s.timezone = 0;
    s.delta_ut1 = 0; s.delta_t = 67;
    s.longitude = 0; s.latitude = 0; s.elevation = 0;
    s.pressure = 820; s.temperature = 11;
    s.atmos_refract = 0.5667;
    s.function = SPA_ALL;
    spa_calculate(&s);
    return s.lamda;   // apparent sun longitude, deg
}

int main(void) {
    // ---- 1. Sun vs SPA across 25 years ----
    printf("Sun geocentric longitude vs SPA:\n");
    double worst = 0;
    for (int i = 0; i < 100; i++) {
        int y = 2001 + (i * 7) % 25;
        int m = 1 + (i * 5) % 12;
        int d = 1 + (i * 11) % 28;
        double h = (i * 37) % 24;
        double lon_spa = spa_sun_lon(y, m, d, h);
        double geo[BODY_COUNT];
        planets__geo_lons(jd_ut(y, m, d, h), geo);
        double err = fabs(planets_lon_diff(geo[BODY_SUN], lon_spa));
        if (err > worst) worst = err;
    }
    check(worst < 0.05, "max |sun - SPA| over 100 samples", worst, 0.0, 0.05);

    // ---- 2. Eclipses as exact conjunction/opposition ----
    printf("Eclipse geometry:\n");
    struct { int y, m, d; double h; double sep; const char *name; } ecl[] = {
        { 2024,  4,  8, 18.30,   0.0, "solar eclipse 2024-04-08" },
        { 2017,  8, 21, 18.43,   0.0, "solar eclipse 2017-08-21" },
        { 2000,  1,  6, 18.23,   0.0, "new moon 2000-01-06" },
        { 2025,  3, 14,  6.98, 180.0, "lunar eclipse 2025-03-14" },
        { 2019,  1, 21,  5.20, 180.0, "lunar eclipse 2019-01-21" },
    };
    for (size_t i = 0; i < sizeof(ecl) / sizeof(ecl[0]); i++) {
        double geo[BODY_COUNT];
        planets__geo_lons(jd_ut(ecl[i].y, ecl[i].m, ecl[i].d, ecl[i].h), geo);
        double sep = fabs(planets_lon_diff(geo[BODY_MOON], geo[BODY_SUN]));
        double err = fabs(sep - ecl[i].sep);
        check(err < 0.8, ecl[i].name, sep, ecl[i].sep, 0.8);
    }

    // ---- 3. Elongation bounds, daily sweep 2016-2026 ----
    printf("Inner-planet elongation bounds:\n");
    double max_merc = 0, max_venus = 0;
    for (double jd = jd_ut(2016, 1, 1, 0); jd < jd_ut(2026, 1, 1, 0); jd += 1.0) {
        double geo[BODY_COUNT];
        planets__geo_lons(jd, geo);
        double em = fabs(planets_lon_diff(geo[BODY_MERCURY], geo[BODY_SUN]));
        double ev = fabs(planets_lon_diff(geo[BODY_VENUS], geo[BODY_SUN]));
        if (em > max_merc) max_merc = em;
        if (ev > max_venus) max_venus = ev;
    }
    check(max_merc < 28.5 && max_merc > 17.0, "Mercury max elongation", max_merc, 23.0, 5.5);
    check(max_venus < 47.5 && max_venus > 45.0, "Venus max elongation", max_venus, 46.5, 1.0);

    // ---- 4. Mars retrograde cadence 2000-2026 ----
    printf("Mars retrograde cadence:\n");
    int episodes = 0;
    bool was_retro = false;
    for (double jd = jd_ut(2000, 1, 1, 0); jd < jd_ut(2026, 1, 1, 0); jd += 2.0) {
        double b[BODY_COUNT], a[BODY_COUNT];
        planets__geo_lons(jd - 1.0, b);
        planets__geo_lons(jd + 1.0, a);
        bool retro = planets_lon_diff(a[BODY_MARS], b[BODY_MARS]) < 0;
        if (retro && !was_retro) episodes++;
        was_retro = retro;
    }
    check(episodes >= 11 && episodes <= 13, "Mars retrogrades in 26 years", episodes, 12, 1);

    // ---- 5. Slow movers, sky of 2026-07-18 (public sign placements) ----
    printf("Current-sky spot checks (2026-07-18):\n");
    {
        double geo[BODY_COUNT];
        planets__geo_lons(jd_ut(2026, 7, 18, 12.0), geo);
        static const char *names[BODY_COUNT] = {
            "Sun", "Moon", "Mercury", "Venus", "Mars",
            "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto" };
        static const char *signs[12] = {
            "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo", "Libra",
            "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces" };
        for (int b2 = 0; b2 < BODY_COUNT; b2++)
            printf("    %-8s %7.2f  %2d %s\n", names[b2], geo[b2],
                   (int)fmod(geo[b2], 30.0) + 1,
                   signs[((int)(geo[b2] / 30.0)) % 12]);
        // Slow movers with well-known placements (~1 deg/yr drift)
        check(fabs(planets_lon_diff(geo[BODY_PLUTO], 303.0)) < 4.0,
              "Pluto near 3 Aquarius", geo[BODY_PLUTO], 303.0, 4.0);
        check(fabs(planets_lon_diff(geo[BODY_NEPTUNE], 1.5)) < 4.0,
              "Neptune near 1 Aries", geo[BODY_NEPTUNE], 1.5, 4.0);
        check(fabs(planets_lon_diff(geo[BODY_URANUS], 62.0)) < 5.0,
              "Uranus near 2 Gemini", geo[BODY_URANUS], 62.0, 5.0);
        // Sun on 2026-07-18 is ~25-26 Cancer by definition of the calendar
        check(fabs(planets_lon_diff(geo[BODY_SUN], 115.8)) < 1.0,
              "Sun near 26 Cancer", geo[BODY_SUN], 115.8, 1.0);
    }

    // ---- Sun altitude vs SPA (validates the whole horizon pipeline:
    //      equatorial conversion + sidereal time + hour angle) ----
    printf("Sun altitude vs SPA (Berlin):\n");
    {
        double worst_alt = 0;
        for (int i = 0; i < 60; i++) {
            int y = 2004 + (i * 5) % 22;
            int m = 1 + (i * 7) % 12;
            int d = 3 + (i * 13) % 25;
            double hh = (i * 29) % 24;
            spa_data s;
            memset(&s, 0, sizeof(s));
            s.year = y; s.month = m; s.day = d;
            s.hour = (int)hh; s.minute = 0; s.second = 0;
            s.timezone = 0;
            s.delta_ut1 = 0; s.delta_t = 67;
            s.longitude = 13.405; s.latitude = 52.52;
            s.elevation = 0; s.pressure = 0; s.temperature = 11;
            s.atmos_refract = 0;   // geometric, to match our pipeline
            s.function = SPA_ALL;
            spa_calculate(&s);
            double alt_spa = 90.0 - s.zenith;

            PlanetsNow pn;
            PlanetsSky sk;
            planets_compute(&pn, jd_ut(y, m, d, hh));
            planets_sky_compute(&sk, &pn, 52.52, 13.405);
            double err = fabs(sk.alt[BODY_SUN] - alt_spa);
            if (err > worst_alt) worst_alt = err;
        }
        check(worst_alt < 0.3, "max |sun alt - SPA| over 60 samples",
              worst_alt, 0.0, 0.3);
    }

    // ---- Aspect finder smoke test ----
    printf("Aspect finder:\n");
    {
        double lons[BODY_COUNT] = { 10, 130, 100, 190, 40, 250, 11.5, 355, 55, 222 };
        Aspect as[PLANETS_MAX_ASPECTS];
        int n = planets_find_aspects(lons, as, PLANETS_MAX_ASPECTS);
        // Expected pairs include: Sun-Moon trine (120), Sun-Mercury square,
        // Sun-Saturn conjunction (1.5 off), Moon-Venus sextile, Sun-Uranus
        // semisextile (15 off -> none)... just verify a few structurally.
        bool saw_trine = false, saw_conj = false;
        for (int i = 0; i < n; i++) {
            if (as[i].a == BODY_SUN && as[i].b == BODY_MOON
                && as[i].kind == ASPECT_TRINE) saw_trine = true;
            if (as[i].a == BODY_SUN && as[i].b == BODY_SATURN
                && as[i].kind == ASPECT_CONJUNCTION) saw_conj = true;
        }
        check(saw_trine, "Sun-Moon 120 found as trine", saw_trine, 1, 0);
        check(saw_conj, "Sun-Saturn 1.5 found as conjunction", saw_conj, 1, 0);
        printf("    %d aspects in synthetic sky\n", n);
    }

    printf(g_fail ? "\n%d FAILURES\n" : "\nall checks passed\n", g_fail);
    return g_fail ? 1 : 0;
}
