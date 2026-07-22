// test_eclipse.c — validation harness for core/eclipse.h
//
// An eclipse predictor is only worth anything if it agrees with the
// sky. Three checks, in order of how hard they are to fake:
//
//   1. THE CANON. Every solar and lunar eclipse published for
//      2023-2027 — 21 of them — must be found, on the right date, of
//      the right kind. And nothing else may be reported: a predictor
//      that flags every syzygy would score perfectly on recall and be
//      useless. Both directions are checked.
//   2. SEPARATION. The margin between the worst real eclipse and the
//      nearest ordinary syzygy, measured rather than assumed. This is
//      the number that says whether the threshold is safe or lucky —
//      if it ever goes negative the classification is guesswork.
//   3. LONG-RUN RATE. Swept over centuries the count must land on the
//      real statistics: ~2.4 solar and ~2.2 lunar a year, never fewer
//      than 4 in a year, never more than 7. This catches a threshold
//      that is quietly wrong far from the window we validated in.
//
// Build & run: make test_eclipse

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/eclipse.h"

static int g_fail = 0;

static void check(bool ok, const char *what) {
    if (!ok) g_fail++;
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
}

static double jdate(int y, int m, int d) {
    if (m <= 2) { y -= 1; m += 12; }
    int A = y / 100, B = 2 - A + A / 4;
    return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1))
         + d + B - 1524.5 + 0.5;
}

static void caldate(double jd, int *y, int *m, int *d) {
    double z = floor(jd + 0.5), f = jd + 0.5 - z, A = z;
    if (z >= 2299161) {
        double al = floor((z - 1867216.25) / 36524.25);
        A = z + 1 + al - floor(al / 4);
    }
    double B = A + 1524, C = floor((B - 122.1) / 365.25);
    double D = floor(365.25 * C), E = floor((B - D) / 30.6001);
    *d = (int)(B - D - floor(30.6001 * E) + f);
    *m = (int)(E < 14 ? E - 1 : E - 13);
    *y = (int)(*m > 2 ? C - 4716 : C - 4715);
}

// Date of greatest eclipse (UT) for every eclipse 2023-2027, from the
// published canon. 's' = solar, 'l' = lunar (umbral and penumbral).
static const struct { int y, m, d; char kind; } CANON[] = {
    { 2023,  4, 20, 's' }, { 2023,  5,  5, 'l' },
    { 2023, 10, 14, 's' }, { 2023, 10, 28, 'l' },
    { 2024,  3, 25, 'l' }, { 2024,  4,  8, 's' },
    { 2024,  9, 18, 'l' }, { 2024, 10,  2, 's' },
    { 2025,  3, 14, 'l' }, { 2025,  3, 29, 's' },
    { 2025,  9,  7, 'l' }, { 2025,  9, 21, 's' },
    { 2026,  2, 17, 's' }, { 2026,  3,  3, 'l' },
    { 2026,  8, 12, 's' }, { 2026,  8, 28, 'l' },
    { 2027,  2,  6, 's' }, { 2027,  2, 20, 'l' },
    { 2027,  7, 18, 'l' }, { 2027,  8,  2, 's' },
    { 2027,  8, 17, 'l' },
};
#define NCANON (int)(sizeof(CANON) / sizeof(CANON[0]))

int main(void) {
    printf("\n=== eclipse.h validation ===\n\n");

    // ---- 1. The canon, both directions ----
    printf("The canon, 2023-2027 (%d published eclipses):\n", NCANON);
    static EclipseTable tb;
    eclipse_find(&tb, jdate(2023, 1, 1), jdate(2028, 1, 1));

    int matched[NCANON];
    memset(matched, 0, sizeof(matched));
    int spurious = 0;

    for (int i = 0; i < tb.n; i++) {
        int y, m, d;
        caldate(tb.e[i].jd, &y, &m, &d);
        char kind = tb.e[i].solar ? 's' : 'l';
        int hit = -1;
        for (int c = 0; c < NCANON; c++) {
            if (CANON[c].kind != kind) continue;
            // +-1 day: greatest eclipse and syzygy can straddle midnight
            if (fabs(jdate(CANON[c].y, CANON[c].m, CANON[c].d)
                     - jdate(y, m, d)) <= 1.0) { hit = c; break; }
        }
        if (hit >= 0) matched[hit] = 1;
        else {
            spurious++;
            printf("      SPURIOUS  %04d-%02d-%02d %s  |b|=%.3f\n",
                   y, m, d, tb.e[i].solar ? "SOL" : "LVN", tb.e[i].beta);
        }
    }
    int found = 0;
    for (int c = 0; c < NCANON; c++) {
        if (matched[c]) { found++; continue; }
        printf("      MISSED    %04d-%02d-%02d %s\n",
               CANON[c].y, CANON[c].m, CANON[c].d,
               CANON[c].kind == 's' ? "SOL" : "LVN");
    }
    printf("    found %d/%d, %d spurious, %d reported\n",
           found, NCANON, spurious, tb.n);
    check(found == NCANON, "every published eclipse is found");
    check(spurious == 0,   "no ordinary syzygy is called an eclipse");
    check(tb.dropped == 0, "the table had room for all of them");

    // ---- 2. Separation margin ----
    printf("Separation of eclipses from ordinary syzygies:\n");
    double worst_real = 0.0, best_false = 99.0;
    for (int pass = 0; pass < 2; pass++) {
        double target = pass ? 180.0 : 0.0;
        char kind = pass ? 'l' : 's';
        double prev = ecl__resid(jdate(2023, 1, 1), target);
        for (double jd = jdate(2023, 1, 1) + 1.0;
             jd < jdate(2028, 1, 1); jd += 1.0) {
            double cur = ecl__resid(jd, target);
            if (!(prev < 0.0 && cur >= 0.0 && fabs(cur - prev) < 30.0)) {
                prev = cur; continue;
            }
            double a = jd - 1.0, b = jd;
            for (int i = 0; i < 40; i++) {
                double m = 0.5 * (a + b);
                if (ecl__resid(m, target) < 0.0) a = m; else b = m;
            }
            double t = 0.5 * (a + b);
            int y, mo, dd; caldate(t, &y, &mo, &dd);
            bool real = false;
            for (int c = 0; c < NCANON; c++)
                if (CANON[c].kind == kind
                    && fabs(jdate(CANON[c].y, CANON[c].m, CANON[c].d)
                            - jdate(y, mo, dd)) <= 1.0) { real = true; break; }
            double bt = fabs(planets_moon_lat(t));
            if (real) { if (bt > worst_real) worst_real = bt; }
            else      { if (bt < best_false) best_false = bt; }
            prev = cur;
        }
    }
    printf("    worst real |b| = %.3f, nearest false |b| = %.3f,"
           " margin %.3f (cut at %.2f)\n",
           worst_real, best_false, best_false - worst_real, ECL_LIMIT);
    check(worst_real < ECL_LIMIT && best_false > ECL_LIMIT,
          "the cut lies strictly between them");
    check(best_false - worst_real > 0.05, "margin is not a hairline");

    // ---- 3. Long-run rate, far outside the validated window ----
    printf("Long-run rate (truth: ~2.4 solar + ~2.2 lunar, 4..7 a year):\n");
    static const int ERA[][2] = {
        { 1900, 1920 }, { 1950, 1970 }, { 2000, 2020 },
        { 2020, 2040 }, { 2060, 2080 }, { 2150, 2170 },
    };
    bool rate_ok = true, bounds_ok = true;
    for (int e = 0; e < 6; e++) {
        int y0 = ERA[e][0], y1 = ERA[e][1], sol = 0, lun = 0;
        int lo = 99, hi = 0;
        for (int y = y0; y < y1; y++) {
            static EclipseTable yt;
            eclipse_find(&yt, jdate(y, 1, 1), jdate(y + 1, 1, 1));
            int ns = 0;
            for (int i = 0; i < yt.n; i++) if (yt.e[i].solar) ns++;
            sol += ns; lun += yt.n - ns;
            if (yt.n < lo) lo = yt.n;
            if (yt.n > hi) hi = yt.n;
        }
        double yrs = y1 - y0;
        double rs = sol / yrs, rl = lun / yrs;
        printf("    %d-%d  solar %.2f/yr  lunar %.2f/yr  total %.2f/yr"
               "  (min %d, max %d)\n", y0, y1, rs, rl, rs + rl, lo, hi);
        if (rs < 1.9 || rs > 2.7 || rl < 1.8 || rl > 2.7) rate_ok = false;
        if (lo < 4 || hi > 7) bounds_ok = false;
    }
    check(rate_ok,   "solar and lunar rates match the real statistics");
    check(bounds_ok, "yearly count never leaves 4..7");

    // ---- 4. Seasons contain the eclipses, and regress ----
    // Two independent claims. First: every eclipse must fall inside a
    // season window — that is the definition, and if the node
    // expression and the syzygy scan ever disagree the marks would sit
    // outside their own bands on the wheel. Second: a node passage
    // recurs on the ECLIPSE year of 346.62 days, not the calendar's
    // 365.25, so the seasons walk backwards ~18.6 days a year. That
    // drift is what the wheel shows when the years are scrubbed.
    printf("Seasons:\n");
    {
        static EclipseSeason ss[64];
        int ns = eclipse_seasons(jdate(2023, 1, 1) - ECL_SEASON_HALF,
                                 jdate(2028, 1, 1) + ECL_SEASON_HALF,
                                 ss, 64);
        int outside = 0;
        for (int i = 0; i < tb.n; i++) {
            bool in = false;
            for (int j = 0; j < ns; j++)
                if (fabs(tb.e[i].jd - ss[j].mid) <= ECL_SEASON_HALF) {
                    in = true; break;
                }
            if (!in) {
                outside++;
                int y, m, dd; caldate(tb.e[i].jd, &y, &m, &dd);
                printf("      OUTSIDE  %04d-%02d-%02d\n", y, m, dd);
            }
        }
        printf("    %d seasons over 5 years, %d eclipses outside them\n",
               ns, outside);
        check(outside == 0, "every eclipse falls inside a season");

        // Successive passages of the SAME node
        double worst = 0.0;
        int pairs = 0;
        for (int i = 0; i < ns; i++)
            for (int j = i + 1; j < ns; j++) {
                if (ss[i].head != ss[j].head) continue;
                double gap = ss[j].mid - ss[i].mid;
                if (gap < 300.0 || gap > 400.0) continue;
                double err = fabs(gap - 346.62);
                if (err > worst) worst = err;
                pairs++;
            }
        printf("    %d same-node intervals, worst error vs the"
               " 346.62d eclipse year: %.2f d\n", pairs, worst);
        check(pairs >= 6 && worst < 2.0,
              "seasons recur on the eclipse year (so they regress)");
    }

    printf("\n%s (%d failures)\n\n", g_fail ? "FAILED" : "ALL PASS", g_fail);
    return g_fail ? 1 : 0;
}
