// timewarp.h — Virtual time acceleration
// Any view can request time to run fast (e.g. spin the sun around a full day).
// The warp accumulates a virtual offset that gets added to wall clock time
// for solar/calendar calculations.

#ifndef TIMEWARP_H
#define TIMEWARP_H

#include "tween.h"

typedef struct {
    bool      active;
    double    speed;            // target multiplier (1.0 = realtime, 360.0 = 6min/sec)
    double    virtual_offset;   // accumulated offset in seconds
    double    duration;         // total warp duration in real seconds (0 = indefinite)
    double    elapsed;          // real seconds since warp started

    // Speed ramp: smooth acceleration/deceleration
    EaseCurve ramp_curve;
    double    ramp_in;          // seconds to ramp up from 1x to full speed
    double    ramp_out;         // seconds to ramp down before end
} TimeWarp;

static inline void timewarp_init(TimeWarp *tw) {
    memset(tw, 0, sizeof(TimeWarp));
}

static inline void timewarp_start(TimeWarp *tw, double speed, double duration,
                                  EaseCurve ramp_curve, double ramp_in, double ramp_out) {
    tw->active = true;
    tw->speed = speed;
    tw->virtual_offset = 0;
    tw->duration = duration;
    tw->elapsed = 0;
    tw->ramp_curve = ramp_curve;
    tw->ramp_in = ramp_in;
    tw->ramp_out = ramp_out;
}

static inline void timewarp_stop(TimeWarp *tw) {
    tw->active = false;
    tw->virtual_offset = 0;
    tw->elapsed = 0;
}

static inline void timewarp_update(TimeWarp *tw, double dt) {
    if (!tw->active) return;

    tw->elapsed += dt;

    // Check if warp is done
    if (tw->duration > 0 && tw->elapsed >= tw->duration) {
        tw->active = false;
        tw->virtual_offset = 0;
        return;
    }

    // Compute effective speed with ramp envelope
    double effective = tw->speed;

    // Ramp in
    if (tw->ramp_in > 0 && tw->elapsed < tw->ramp_in) {
        double t = tw->elapsed / tw->ramp_in;
        effective = 1.0 + (tw->speed - 1.0) * ease_eval(tw->ramp_curve, t);
    }

    // Ramp out
    if (tw->duration > 0 && tw->ramp_out > 0) {
        double time_left = tw->duration - tw->elapsed;
        if (time_left < tw->ramp_out) {
            double t = time_left / tw->ramp_out;
            effective = 1.0 + (effective - 1.0) * ease_eval(tw->ramp_curve, t);
        }
    }

    tw->virtual_offset += effective * dt;
}

// Get the total virtual offset in seconds (add to wall clock)
static inline double timewarp_offset(const TimeWarp *tw) {
    return tw->active ? tw->virtual_offset : 0.0;
}

// Compute effective hours/minutes from wall clock + warp offset
static inline void timewarp_effective_time(const TimeWarp *tw,
                                           int base_hours, int base_mins, int base_secs,
                                           int *out_hours, int *out_mins) {
    double offset = timewarp_offset(tw);
    int total_secs = base_hours * 3600 + base_mins * 60 + base_secs + (int)offset;
    total_secs = ((total_secs % 86400) + 86400) % 86400; // wrap positive
    *out_hours = total_secs / 3600;
    *out_mins = (total_secs % 3600) / 60;
}

#endif // TIMEWARP_H
