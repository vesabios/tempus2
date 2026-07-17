// timeview.h — Per-view time state
// Each view maintains its own time coordinates for visualization.
// When synced, mirrors the master clock. When unsynced, views can
// animate time independently (e.g. solar day warp, calendar year scrub).

#ifndef TIMEVIEW_H
#define TIMEVIEW_H

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "calendar.h"
#include "timewarp.h"

typedef struct {
    // Canonical time coordinates
    int    year;
    double year_pct;      // 0..1 position within year -> month/day
    double day_pct;       // 0..1 position within day -> hours/mins/secs

    // Sync mode: when true, mirrors master clock each frame
    bool   synced;

    // Per-view time warp
    TimeWarp warp;
    double base_day_pct;  // snapshot when warp started
    double base_year_pct;

    // Derived fields (computed by timeview_derive)
    int    month, day;
    int    hours, mins, secs;
    double frac_secs;
    double percent_of_day;
    double jd_current;
} TimeView;

static inline void timeview_init(TimeView *tv) {
    memset(tv, 0, sizeof(TimeView));
    tv->synced = true;
    timewarp_init(&tv->warp);
}

// Derive month/day/hours/mins/secs from year + year_pct + day_pct
static inline void timeview_derive(TimeView *tv) {
    // Year position -> month/day
    int days_total = cal_days_in_year(tv->year);
    double ypct = tv->year_pct;
    if (ypct < 0) ypct = 0;
    if (ypct > 0.9999) ypct = 0.9999;
    int day_of_year = (int)(ypct * days_total);

    int accum = 0;
    tv->month = 1;
    tv->day = 1;
    for (int m = 1; m <= 12; m++) {
        int dm = cal_days_in_month(m, tv->year);
        if (accum + dm > day_of_year) {
            tv->month = m;
            tv->day = day_of_year - accum + 1;
            break;
        }
        accum += dm;
    }

    // Day position -> hours/mins/secs
    double dpct = tv->day_pct;
    if (dpct < 0) dpct = 0;
    if (dpct > 0.9999) dpct = 0.9999;
    tv->percent_of_day = dpct;

    double day_secs = dpct * 86400.0;
    int total_secs = (int)day_secs;
    tv->hours = total_secs / 3600;
    tv->mins  = (total_secs % 3600) / 60;
    tv->secs  = total_secs % 60;
    tv->frac_secs = day_secs - total_secs;

    tv->jd_current = cal_jd_noon(tv->year, tv->month, tv->day);
}

// Warp through exactly `days` of virtual time over `duration` real
// seconds. Solves for the speed multiplier including ramp losses: a
// symmetric ease averages 1/2 over a ramp, so covered time is
//   S*(duration - (ramp_in+ramp_out)/2) + (ramp_in+ramp_out)/2.
static inline void timeview_start_day_warp(TimeView *tv, double days,
                                           double duration, EaseCurve curve,
                                           double ramp_in, double ramp_out) {
    double half_ramps = (ramp_in + ramp_out) * 0.5;
    double target = days * 86400.0;
    double speed = (target - half_ramps) / (duration - half_ramps);
    tv->synced = false;
    tv->base_day_pct = tv->day_pct;
    tv->base_year_pct = tv->year_pct;
    timewarp_start(&tv->warp, speed, duration, curve, ramp_in, ramp_out);
}

// Stop warp and re-sync to master
static inline void timeview_stop_warp(TimeView *tv) {
    timewarp_stop(&tv->warp);
    tv->synced = true;
}

// Update when not synced (call each frame for all views)
static inline void timeview_update(TimeView *tv, double dt) {
    if (tv->synced) return;

    timewarp_update(&tv->warp, dt);

    if (tv->warp.active) {
        double day_offset = tv->warp.virtual_offset / 86400.0;
        tv->day_pct = fmod(tv->base_day_pct + day_offset, 1.0);
        if (tv->day_pct < 0) tv->day_pct += 1.0;
    } else {
        // Warp finished — re-sync to master
        tv->synced = true;
    }

    timeview_derive(tv);
}

#endif // TIMEVIEW_H
