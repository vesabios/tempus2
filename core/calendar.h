// calendar.h — Calendar year, seasonal events, Julian date utilities
// Ported from CalendarYear.cpp

#ifndef CALENDAR_H
#define CALENDAR_H

#include <math.h>
#include <stdbool.h>

// --- Julian Date ---

static inline double cal_jd(int y, int m, int d) {
    if (m <= 2) { y -= 1; m += 12; }
    int A = (int)floor(y / 100.0);
    int B = 2 - A + (int)floor(A / 4.0);
    return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

// JD with half-day offset (as used in the original for wheel positioning)
static inline double cal_jd_noon(int y, int m, int d) {
    return cal_jd(y, m, d) + 0.5;
}

// --- Leap year / days ---

static inline bool cal_is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static inline int cal_days_in_year(int year) {
    return cal_is_leap_year(year) ? 366 : 365;
}

static inline int cal_days_in_month(int month, int year) {
    // month is 1-based
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    if (month == 2) return cal_is_leap_year(year) ? 29 : 28;
    return 31;
}

// --- Wheel of the Year: solstices, equinoxes, cross-quarter days ---
// Uses Meeus polynomial approximation for equinoxes/solstices

typedef struct {
    int    year;
    int    total_days;

    // Astronomical events (Julian dates)
    double jd_yule;     // winter solstice — year boundary
    double jd_imbolc;   // cross-quarter (midpoint yule→ostara)
    double jd_ostara;   // spring equinox
    double jd_beltane;  // cross-quarter (ostara→litha)
    double jd_litha;    // summer solstice
    double jd_lammas;   // cross-quarter (litha→mabon)
    double jd_mabon;    // autumn equinox
    double jd_samhain;  // cross-quarter (mabon→yule)
} CalendarYear;

static inline double cal__yule_jd(int year) {
    double m = ((double)year - 2000.0) / 1000.0;
    return 2451900.05952 + 365242.74049 * m - 0.06223 * m*m
         - 0.00823 * m*m*m + 0.00032 * m*m*m*m;
}

static inline void cal_year_init(CalendarYear *cy, int year) {
    cy->year = year;
    cy->total_days = cal_days_in_year(year);

    double m = ((double)year - 2000.0) / 1000.0;

    cy->jd_ostara = 2451623.80984 + 365242.37404 * m + 0.05169 * m*m
                  - 0.00411 * m*m*m - 0.00057 * m*m*m*m;
    cy->jd_litha  = 2451716.56767 + 365241.62603 * m + 0.00325 * m*m
                  + 0.00888 * m*m*m - 0.00030 * m*m*m*m;
    cy->jd_mabon  = 2451810.21715 + 365242.01767 * m - 0.11575 * m*m
                  + 0.00337 * m*m*m + 0.00078 * m*m*m*m;
    cy->jd_yule   = cal__yule_jd(year);

    // Cross-quarter days are midpoints between the cardinal events
    double last_yule = cal__yule_jd(year - 1);
    cy->jd_imbolc  = (last_yule + cy->jd_ostara) / 2.0;
    cy->jd_beltane = (cy->jd_ostara + cy->jd_litha) / 2.0;
    cy->jd_lammas  = (cy->jd_litha + cy->jd_mabon) / 2.0;
    cy->jd_samhain = (cy->jd_mabon + cy->jd_yule) / 2.0;
}

// --- JD ↔ Gregorian conversion ---

#define CAL_LASTJULJDN 2299160L

static inline void cal_jd_to_date(double jd, int *year, int *month, int *day,
                                  int *hour, int *minute) {
    jd += 0.5;
    long jdn = (long)floor(jd);
    double ut = jd - jdn;
    bool julian = (jdn <= CAL_LASTJULJDN);

    long x = jdn + 68569L;
    long daysPer400 = julian ? 146100L : 146097L;
    long fudged4000 = julian ? (1461000L + 1) : (1460970L + 31);

    if (julian) x += 38;

    long z = 4 * x / daysPer400;
    x = x - (daysPer400 * z + 3) / 4;
    long y = 4000 * (x + 1) / fudged4000;
    x = x - 1461 * y / 4 + 31;
    long m = 80 * x / 2447;
    long d = x - 2447 * m / 80;
    x = m / 11;
    m = m + 2 - 12 * x;
    y = 100 * (z - 49) + y + x;

    *year = (int)y;
    *month = (int)m;
    *day = (int)d;
    if (*year <= 0) (*year)--;
    if (hour) *hour = (int)(ut * 24);
    if (minute) *minute = (int)((ut * 24 - (int)(ut * 24)) * 60);
}

// --- Event names ---

static const char *cal_event_names[8] = {
    "IMBOLC", "SPRING EQUINOX", "BELTANE", "SUMMER SOLSTICE",
    "LUGHNASADH", "AUTUMN EQUINOX", "SAMHAIN", "WINTER SOLSTICE"
};

static const char *cal_alternate_names[8] = {
    "CANDLEMAS", "OSTARA", "MAY EVE", "LITHA",
    "LAMMAS", "MABON", "SAMHAIN", "YULE"
};

static const char *cal_month_names[12] = {
    "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};

static const char *cal_roman_month_names[12] = {
    "IANUARIUS", "FEBRUARIUS", "MARTIUS", "APRILIS", "MAIUS", "IUNIUS",
    "IULIUS", "AUGUSTUS", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};

static const char *cal_month_abbrev[12] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

#endif // CALENDAR_H
