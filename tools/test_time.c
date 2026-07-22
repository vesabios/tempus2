// test_time.c — the override clock's arithmetic invariants.
//
// The manual time override stores "where in the year are we" as a
// normalized ratio, doy/days_total, and every consumer divides it back
// out. That round trip is not free: floating point does not guarantee
// (doy/n)*n == doy, and when it falls a hair short a truncation reads
// the PREVIOUS day. Seren found it as "the first click on the 24-hour
// wheel takes me back a day" — first because the override engages once,
// and only on some dates because only some ratios fall short.
//
// Two claims, and the second matters as much as the first:
//   1. Every day of every year shape survives the round trip.
//   2. The fix does not overcorrect. The guard is an epsilon, and an
//      epsilon that is too generous would round a genuine part-way-
//      through-the-day position UP into the next day — trading a
//      visible bug for a subtler one. So every position within every
//      day is checked too.
//
// Build & run: make test_time

#include <stdio.h>
#include <math.h>
#include "../core/tempus.h"

static int g_fail = 0;

static void check(bool ok, const char *what) {
    if (!ok) g_fail++;
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
}

int main(void) {
    printf("\n=== override clock arithmetic ===\n\n");

    // ---- 1. Every day round-trips ----
    printf("Day round trip (store doy/diy, read it back):\n");
    int wrong = 0, first_bad = -1, first_diy = 0;
    for (int diy = 365; diy <= 366; diy++)
        for (int doy = 0; doy < diy; doy++) {
            double pct = (double)doy / (double)diy;
            if (tempus_doy_from_pct(pct, diy) != doy) {
                if (first_bad < 0) { first_bad = doy; first_diy = diy; }
                wrong++;
            }
        }
    if (wrong)
        printf("      first failure: doy %d of %d\n", first_bad, first_diy);
    printf("    %d of 731 days lost in the round trip\n", wrong);
    check(wrong == 0, "every day of a common and a leap year survives");

    // Without the guard, to show the test can actually fail — if this
    // ever reports zero the epsilon has stopped being the thing under
    // test and the check above has become vacuous.
    int naive = 0;
    for (int diy = 365; diy <= 366; diy++)
        for (int doy = 0; doy < diy; doy++) {
            double pct = (double)doy / (double)diy;
            if ((int)(pct * diy) != doy) naive++;
        }
    printf("    (a bare truncation loses %d of them — the bug)\n", naive);
    check(naive > 0, "the naive form really is broken (test is not vacuous)");

    // ---- 2. The guard does not steal from the next day ----
    // Scrubbing to any hour of a day must still read as THAT day.
    printf("Fractional positions stay inside their own day:\n");
    int bled = 0;
    for (int diy = 365; diy <= 366; diy++)
        for (int doy = 0; doy < diy; doy++)
            for (int step = 0; step < 1440; step++) {   // every minute
                double v = doy + (double)step / 1440.0;
                if (tempus_doy_from_pct(v / diy, diy) != doy) bled++;
            }
    printf("    %d of %d minute positions read as the wrong day\n",
           bled, (365 + 366) * 1440);
    check(bled == 0, "no minute of any day rounds into the next");

    // And the epsilon must stay far below the resolution it protects:
    // a full day is 86400 s, so the nudge is well under a second.
    printf("    epsilon = %.1e day = %.4f s\n",
           TEMPUS_DAY_EPS, TEMPUS_DAY_EPS * 86400.0);
    check(TEMPUS_DAY_EPS * 86400.0 < 1.0,
          "the nudge is under a second of wall time");

    printf("\n%s (%d failures)\n\n", g_fail ? "FAILED" : "ALL PASS", g_fail);
    return g_fail ? 1 : 0;
}
