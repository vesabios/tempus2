// tempus.h — Core state and logic for Tempus
// Pure C, no rendering or platform dependencies.

#ifndef TEMPUS_H
#define TEMPUS_H

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "calendar.h"
#include "sunset.h"
#include "spa.h"
#include "timeview.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- Configuration ----

typedef struct {
    double latitude;
    double longitude;
    double timezone;        // hours from GMT (e.g. -8 for PST)
    bool   timezone_auto;   // derive timezone from the system clock (w/ DST)
    double elevation;       // meters
    bool   use_alternate_names;
    // SAECVLVM: with a birth date the century ring becomes the ninety
    // years of a life (0 = unset, show the current century)
    int    birth_year;
    int    birth_month;
    int    birth_day;
} TempusConfig;

static inline TempusConfig tempus_default_config(void) {
    return (TempusConfig){
        .latitude  = 52.52,      // Berlin, DE
        .longitude = 13.405,
        .timezone  = 1.0,        // fallback when timezone_auto is off
        .timezone_auto = true,
        .elevation = 34.0,       // meters
        .use_alternate_names = true,
    };
}

// Current UTC offset of the system clock in hours, DST included.
// Portable: reinterpret the UTC field breakdown as local time and diff.
static inline double tempus_system_tz_offset(void) {
    time_t now = time(NULL);
    struct tm utc_tm = *gmtime(&now);
    utc_tm.tm_isdst = -1;
    time_t utc_as_local = mktime(&utc_tm);
    return difftime(now, utc_as_local) / 3600.0;
}

// ---- Main state ----

typedef struct {
    TempusConfig config;

    // Time
    int    year, month, day;
    int    hours, mins, secs;
    double frac_secs;           // fractional seconds (ms precision)
    double percent_of_day;      // 0..1

    // Debug override: when true, derive date/time from normalized sliders
    bool   time_override;
    int    override_year;
    double override_year_pct;   // 0..1 position within year → month/day
    double override_day_pct;    // 0..1 position within day → hour/min/sec

    // Calendar years (previous, current, next — for yule boundaries)
    CalendarYear last_year;
    CalendarYear this_year;
    CalendarYear next_year;

    // Julian dates
    double jd_current;
    double jd_newyear;          // last year's yule (= year boundary)
    double jd_nextyear;         // this year's yule
    double jd_months[13];       // JD of 1st of each month + next jan
    double jd_events[8];        // the 8 sabbats
    int    days_in_month[12];
    double total_days;          // jd_nextyear - jd_newyear

    // Sunrise/sunset
    SunsetCalc sunset_calc;
    double     sunrise_mins;    // minutes from midnight
    double     sunset_mins;

    // Solar position (current)
    spa_data solar;
    double   zenith;
    bool     solar_dirty;

    // Tracking for recalculation
    int    last_mins;

    // Elapsed time
    double elapsed;
    double last_elapsed;

} Tempus;

// ---- Geometry helpers ----

static inline void tempus_fract_circle(double pct, double *x, double *y) {
    *x = sin(pct * M_PI * 2.0);
    *y = -cos(pct * M_PI * 2.0);
}

static inline double tempus_jd_to_wheel_pct(const Tempus *t, double jd) {
    return (jd - t->jd_newyear) / t->total_days;
}

// ---- Easing ----

static inline double tempus_ease_in_out_quad(double t) {
    return t < 0.5 ? 2.0 * t * t : 1.0 - pow(-2.0 * t + 2.0, 2.0) / 2.0;
}

static inline double tempus_ease_in_out_quint(double t) {
    return t < 0.5 ? 16.0 * t*t*t*t*t : 1.0 - pow(-2.0 * t + 2.0, 5.0) / 2.0;
}

static inline double tempus_smoothstep(double edge0, double edge1, double x) {
    double z = (x - edge0) / (edge1 - edge0);
    if (z < 0.0) z = 0.0;
    if (z > 1.0) z = 1.0;
    return z * z * (3.0 - 2.0 * z);
}

static inline double tempus_clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline double tempus_mix(double a, double b, double t) {
    return a + (b - a) * t;
}

// ---- SPA helper ----

static inline void tempus__fill_spa(spa_data *spa, const Tempus *t,
                                    int hour, int minute, int func) {
    memset(spa, 0, sizeof(spa_data));
    spa->year          = t->year;
    spa->month         = t->month;
    spa->day           = t->day;
    spa->hour          = hour;
    spa->minute        = minute;
    spa->second        = 0;
    spa->timezone      = t->config.timezone;
    spa->delta_ut1     = 0;
    spa->delta_t       = 67;
    spa->longitude     = t->config.longitude;
    spa->latitude      = t->config.latitude;
    spa->elevation     = t->config.elevation;
    spa->pressure      = 820;
    spa->temperature   = 11;
    spa->slope         = 0;
    spa->azm_rotation  = 0;
    spa->atmos_refract = 0.5667;
    spa->function      = func;
}

static inline spa_data tempus_get_solar_at(const Tempus *t, double day_pct) {
    int h = (int)(day_pct * 24.0);
    int m = ((int)(day_pct * 24.0 * 60.0)) % 60;
    spa_data spa;
    tempus__fill_spa(&spa, t, h, m, SPA_ZA);
    spa_calculate(&spa);
    return spa;
}

// ---- Init ----

static inline void tempus_recalc_day(Tempus *t) {
    t->total_days = t->jd_nextyear - t->jd_newyear;

    for (int i = 0; i < 12; i++)
        t->days_in_month[i] = cal_days_in_month(i + 1, t->year);

    t->solar_dirty = true;
    t->last_mins = -1;
}

static inline void tempus_init(Tempus *t, TempusConfig cfg) {
    memset(t, 0, sizeof(Tempus));
    t->config = cfg;
    if (t->config.timezone_auto)
        t->config.timezone = tempus_system_tz_offset();
    t->last_mins = -1;

    // Get current time
    time_t now_t = time(NULL);
    struct tm *tm = localtime(&now_t);
    t->year  = 1900 + tm->tm_year;
    t->month = 1 + tm->tm_mon;
    t->day   = tm->tm_mday;
    t->hours = tm->tm_hour;
    t->mins  = tm->tm_min;
    t->secs  = tm->tm_sec;

    // Calendar years
    cal_year_init(&t->last_year, t->year - 1);
    cal_year_init(&t->this_year, t->year);
    cal_year_init(&t->next_year, t->year + 1);

    // Sunrise/sunset
    sunset_init(&t->sunset_calc, cfg.latitude, cfg.longitude, (int)cfg.timezone);
    sunset_set_date(&t->sunset_calc, t->year, t->month, t->day);
    t->sunrise_mins = sunset_calc_sunrise(&t->sunset_calc);
    t->sunset_mins  = sunset_calc_sunset(&t->sunset_calc);

    // Julian dates
    t->jd_current  = cal_jd_noon(t->year, t->month, t->day);
    t->jd_newyear  = t->last_year.jd_yule;
    t->jd_nextyear = t->this_year.jd_yule;

    for (int i = 0; i < 12; i++)
        t->jd_months[i] = cal_jd_noon(t->year, i + 1, 1);
    t->jd_months[12] = cal_jd_noon(t->year + 1, 1, 1);

    // Events
    t->jd_events[0] = t->this_year.jd_imbolc;
    t->jd_events[1] = t->this_year.jd_ostara;
    t->jd_events[2] = t->this_year.jd_beltane;
    t->jd_events[3] = t->this_year.jd_litha;
    t->jd_events[4] = t->this_year.jd_lammas;
    t->jd_events[5] = t->this_year.jd_mabon;
    t->jd_events[6] = t->this_year.jd_samhain;
    t->jd_events[7] = t->this_year.jd_yule;

    tempus_recalc_day(t);

    // Initial solar position
    tempus__fill_spa(&t->solar, t, t->hours, t->mins, SPA_ALL);
    spa_calculate(&t->solar);
    t->zenith = t->solar.zenith;

}

// Change observer location at runtime: re-seed sunrise/sunset and force
// solar recalculation on the next update.
static inline void tempus_set_location(Tempus *t, double lat, double lon) {
    t->config.latitude = lat;
    t->config.longitude = lon;
    sunset_init(&t->sunset_calc, lat, lon, (int)t->config.timezone);
    sunset_set_date(&t->sunset_calc, t->year, t->month, t->day);
    t->sunrise_mins = sunset_calc_sunrise(&t->sunset_calc);
    t->sunset_mins  = sunset_calc_sunset(&t->sunset_calc);
    t->solar_dirty = true;
}

// ---- Update (called each frame) ----

static inline void tempus_update(Tempus *t, double now_secs) {
    double delta = now_secs - t->last_elapsed;
    t->last_elapsed = now_secs;
    if (delta < 0 || delta > 1.0) delta = 0;

    t->elapsed = now_secs;

    if (t->time_override) {
        // Derive date from year + normalized year position (0..1)
        t->year = t->override_year;
        int days_total = cal_days_in_year(t->year);
        double ypct = t->override_year_pct;
        if (ypct < 0) ypct = 0;
        if (ypct > 0.9999) ypct = 0.9999;
        int day_of_year = (int)(ypct * days_total);
        int accum = 0;
        t->month = 1;
        t->day = 1;
        for (int m = 1; m <= 12; m++) {
            int dm = cal_days_in_month(m, t->year);
            if (accum + dm > day_of_year) {
                t->month = m;
                t->day = day_of_year - accum + 1;
                break;
            }
            accum += dm;
        }

        // Derive time from normalized day position (0..1)
        double dpct = t->override_day_pct;
        if (dpct < 0) dpct = 0;
        if (dpct > 0.9999) dpct = 0.9999;
        int total_secs = (int)(dpct * 86400.0);
        t->hours = total_secs / 3600;
        t->mins  = (total_secs % 3600) / 60;
        t->secs  = total_secs % 60;
        t->frac_secs = 0;

        // Recalculate calendar data if date changed
        double new_jd = cal_jd_noon(t->year, t->month, t->day);
        if (new_jd != t->jd_current) {
            t->jd_current = new_jd;
            cal_year_init(&t->last_year, t->year - 1);
            cal_year_init(&t->this_year, t->year);
            cal_year_init(&t->next_year, t->year + 1);
            t->jd_newyear  = t->last_year.jd_yule;
            t->jd_nextyear = t->this_year.jd_yule;
            for (int i = 0; i < 12; i++)
                t->jd_months[i] = cal_jd_noon(t->year, i + 1, 1);
            t->jd_months[12] = cal_jd_noon(t->year + 1, 1, 1);
            t->jd_events[0] = t->this_year.jd_imbolc;
            t->jd_events[1] = t->this_year.jd_ostara;
            t->jd_events[2] = t->this_year.jd_beltane;
            t->jd_events[3] = t->this_year.jd_litha;
            t->jd_events[4] = t->this_year.jd_lammas;
            t->jd_events[5] = t->this_year.jd_mabon;
            t->jd_events[6] = t->this_year.jd_samhain;
            t->jd_events[7] = t->this_year.jd_yule;
            tempus_recalc_day(t);

            sunset_set_date(&t->sunset_calc, t->year, t->month, t->day);
            t->sunrise_mins = sunset_calc_sunrise(&t->sunset_calc);
            t->sunset_mins  = sunset_calc_sunset(&t->sunset_calc);
        }
    } else {
        // Read wall clock with sub-second precision (C11 timespec_get)
        struct timespec ts;
        timespec_get(&ts, TIME_UTC);
        struct tm *tm = localtime(&ts.tv_sec);
        t->hours = tm->tm_hour;
        t->mins  = tm->tm_min;
        t->secs  = tm->tm_sec;
        t->frac_secs = ts.tv_nsec / 1000000000.0;
    }

    // Percent of day
    t->percent_of_day = ((double)t->hours * 3600.0 + (double)t->mins * 60.0
                        + (double)t->secs + t->frac_secs) / 86400.0;

    // Recalculate solar when minute changes
    if (t->last_mins != t->mins || t->solar_dirty) {
        if (t->config.timezone_auto)
            // While scrubbing manual time, interpret the clock as local
            // mean solar time at the configured longitude — so "midnight"
            // on the slider means the sun is opposite this location, even
            // when the machine's timezone is somewhere else entirely.
            t->config.timezone = t->time_override
                ? t->config.longitude / 15.0
                : tempus_system_tz_offset();
        tempus__fill_spa(&t->solar, t, t->hours, t->mins, SPA_ALL);
        spa_calculate(&t->solar);
        t->zenith = t->solar.zenith;
        t->last_mins = t->mins;
        t->solar_dirty = false;
    }
}

// ---- Derived values for rendering ----

// Sync a TimeView from the master clock
static inline void timeview_sync(TimeView *tv, const Tempus *t) {
    tv->year = t->year;
    tv->month = t->month;
    tv->day = t->day;
    tv->hours = t->hours;
    tv->mins = t->mins;
    tv->secs = t->secs;
    tv->frac_secs = t->frac_secs;
    tv->percent_of_day = t->percent_of_day;
    tv->day_pct = t->percent_of_day;
    tv->jd_current = t->jd_current;

    // Compute year_pct from day of year
    int doy = 0;
    for (int m = 1; m < t->month; m++)
        doy += cal_days_in_month(m, t->year);
    doy += t->day - 1;
    tv->year_pct = (double)doy / cal_days_in_year(t->year);
}

// Fill SPA from a TimeView + config (for per-view solar calculations)
static inline void timeview_fill_spa(spa_data *spa, const TimeView *tv,
                                     const TempusConfig *cfg, int func) {
    memset(spa, 0, sizeof(spa_data));
    spa->year          = tv->year;
    spa->month         = tv->month;
    spa->day           = tv->day;
    spa->hour          = tv->hours;
    spa->minute        = tv->mins;
    spa->second        = tv->secs;
    spa->timezone      = cfg->timezone;
    spa->delta_ut1     = 0;
    spa->delta_t       = 67;
    spa->longitude     = cfg->longitude;
    spa->latitude      = cfg->latitude;
    spa->elevation     = cfg->elevation;
    spa->pressure      = 820;
    spa->temperature   = 11;
    spa->slope         = 0;
    spa->azm_rotation  = 0;
    spa->atmos_refract = 0.5667;
    spa->function      = func;
}

static inline double tempus_year_pct(const Tempus *t) {
    double pct = (t->jd_current - t->jd_newyear) / t->total_days;
    pct += t->percent_of_day / t->total_days;
    return pct;
}

// Get the event name for the i-th sabbat (0-7)
static inline const char *tempus_event_name(const Tempus *t, int i) {
    if (i < 0 || i >= 8) return "";
    return t->config.use_alternate_names
        ? cal_alternate_names[i]
        : cal_event_names[i];
}

// Get the month name (0-based)
static inline const char *tempus_month_name(const Tempus *t, int i) {
    if (i < 0 || i >= 12) return "";
    return t->config.use_alternate_names
        ? cal_month_names[i]
        : cal_roman_month_names[i];
}

#endif // TEMPUS_H
