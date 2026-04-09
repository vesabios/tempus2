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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- Configuration ----

typedef struct {
    double latitude;
    double longitude;
    double timezone;        // hours from GMT (e.g. -8 for PST)
    double elevation;       // meters
    bool   use_alternate_names;
} TempusConfig;

static inline TempusConfig tempus_default_config(void) {
    return (TempusConfig){
        .latitude  = 37.7,
        .longitude = -122.4,
        .timezone  = -8.0,
        .elevation = 1830.14,
        .use_alternate_names = true,
    };
}

// ---- Display modes ----

enum {
    TMODE_ZOOM_OUT = 0,     // calendar zooms out from detail
    TMODE_SIMULATOR = 1,    // solar position animation
    TMODE_ZOOM_IN = 2,      // calendar zooms into detail
    TMODE_DETAIL = 3,       // hold on zoomed-in view
    TMODE_COUNT = 4,
};

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

    // Solar position simulator
    bool   simulator_running;
    int    simulator_minutes;
    double simulator_length;    // 24*60

    // Display mode
    int    mode;
    double mode_timer;
    double mode_length;
    double rotation_arc;
    double rotation_timer;

    // Tracking for recalculation
    int    last_mins;
    int    last_hours;

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
    t->simulator_length = 24.0 * 60.0;
    t->last_mins = -1;
    t->last_hours = -1;

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

    // Mode
    t->mode = 0;
    t->mode_timer = 0;
    t->mode_length = 10.0;
    t->rotation_arc = 0;
}

// ---- Update (called each frame) ----

static inline void tempus_update(Tempus *t, double now_secs) {
    double delta = now_secs - t->last_elapsed;
    t->last_elapsed = now_secs;
    if (delta < 0 || delta > 1.0) delta = 0; // guard against first frame / jumps

    t->elapsed = now_secs;
    t->mode_timer += delta;
    t->rotation_timer += delta * t->rotation_arc;
    if (t->rotation_arc < 1.0)
        t->rotation_arc += 0.0015;

    if (t->time_override) {
        // Derive date from year + normalized year position (0..1)
        t->year = t->override_year;
        int days_total = cal_days_in_year(t->year);
        double ypct = t->override_year_pct;
        if (ypct < 0) ypct = 0;
        if (ypct > 0.9999) ypct = 0.9999;
        int day_of_year = (int)(ypct * days_total); // 0-based
        // Walk months to find month/day
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

        // Recalculate calendar data if year changed
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

            // Recalc sunrise/sunset for new date/location
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

    // Recalculate solar when hour changes
    if (t->last_hours != t->hours) {
        t->solar_dirty = true;
        t->last_hours = t->hours;
    }

    // Simulator
    if (t->simulator_running) {
        t->simulator_minutes += 6;
        if (t->simulator_minutes >= (int)t->simulator_length) {
            t->simulator_running = false;
            t->simulator_minutes = 0;
        }
    }

    double sim_pct = (double)t->simulator_minutes / t->simulator_length;
    int smooth_mins = (int)(tempus_ease_in_out_quad(sim_pct) * t->simulator_length);

    int new_mins = t->mins + smooth_mins;
    int new_hours = t->hours + new_mins / 60;
    new_hours %= 24;
    new_mins %= 60;

    // Recalculate solar when minute changes
    if (t->last_mins != new_mins) {
        tempus__fill_spa(&t->solar, t, new_hours, new_mins, SPA_ALL);
        spa_calculate(&t->solar);
        t->zenith = t->solar.zenith;
        t->last_mins = new_mins;
    }

    // Mode cycling
    if (t->mode_timer > t->mode_length) {
        t->mode_timer = 0;
        t->mode = (t->mode + 1) % TMODE_COUNT;
        if (t->mode == TMODE_SIMULATOR) {
            t->simulator_running = true;
            t->simulator_minutes = 0;
        }
        t->mode_length = 10.0;
    }
}

// ---- Derived values for rendering ----

// blend_val: 0 = zoomed out (full wheel), 1 = zoomed in (detail)
static inline double tempus_blend_val(const Tempus *t) {
    switch (t->mode) {
        case TMODE_ZOOM_OUT:
            return 1.0 - tempus_ease_in_out_quint(
                tempus_clamp(t->mode_timer / 10.0, 0.0, 1.0));
        case TMODE_SIMULATOR:
            return 0.0;
        case TMODE_ZOOM_IN:
            return tempus_ease_in_out_quint(
                tempus_clamp(t->mode_timer / 10.0, 0.0, 1.0));
        case TMODE_DETAIL:
            return 1.0;
        default:
            return 0.0;
    }
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
