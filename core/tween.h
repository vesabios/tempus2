// tween.h — Tween pool and easing curves
// Animates double* targets with nonlinear interpolation, chaining, delays.
// Static pool, no allocations.

#ifndef TWEEN_H
#define TWEEN_H

#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// ---- Easing curves ----

typedef enum {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_QUINT,
    EASE_OUT_QUINT,
    EASE_IN_OUT_QUINT,
    EASE_SMOOTHSTEP,
    EASE_COUNT,
} EaseCurve;

static inline double ease_eval(EaseCurve curve, double t) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    switch (curve) {
        case EASE_LINEAR:       return t;
        case EASE_IN_QUAD:      return t * t;
        case EASE_OUT_QUAD:     return 1.0 - (1.0 - t) * (1.0 - t);
        case EASE_IN_OUT_QUAD:  return t < 0.5 ? 2.0*t*t : 1.0 - pow(-2.0*t + 2.0, 2.0) / 2.0;
        case EASE_IN_CUBIC:     return t * t * t;
        case EASE_OUT_CUBIC:    { double u = 1.0 - t; return 1.0 - u*u*u; }
        case EASE_IN_OUT_CUBIC: return t < 0.5 ? 4.0*t*t*t : 1.0 - pow(-2.0*t + 2.0, 3.0) / 2.0;
        case EASE_IN_QUINT:     return t*t*t*t*t;
        case EASE_OUT_QUINT:    { double u = 1.0 - t; return 1.0 - u*u*u*u*u; }
        case EASE_IN_OUT_QUINT: return t < 0.5 ? 16.0*t*t*t*t*t : 1.0 - pow(-2.0*t + 2.0, 5.0) / 2.0;
        case EASE_SMOOTHSTEP:   return t * t * (3.0 - 2.0 * t);
        default:                return t;
    }
}

// ---- Tween ----

enum {
    TWEEN_FREE = 0,
    TWEEN_WAITING,      // delay not yet elapsed
    TWEEN_RUNNING,
    TWEEN_DONE,
};

typedef struct {
    uint16_t  id;           // nonzero handle, 0 = free slot
    uint16_t  chain_next;   // id of tween to activate when this one completes
    double   *target;       // pointer to animated value
    double    from, to;
    double    delay;        // seconds before animation starts
    double    duration;     // seconds of active animation
    double    elapsed;      // total time since creation
    EaseCurve curve;
    uint8_t   state;
    bool      ping_pong;    // reverse on completion instead of stopping
} Tween;

// ---- Pool ----

#define TWEEN_POOL_SIZE 64

typedef struct {
    Tween    pool[TWEEN_POOL_SIZE];
    uint16_t next_id;       // monotonic, wraps, skips 0
} TweenPool;

static inline void tween_pool_init(TweenPool *tp) {
    memset(tp, 0, sizeof(TweenPool));
    tp->next_id = 1;
}

static inline uint16_t tween__alloc_id(TweenPool *tp) {
    uint16_t id = tp->next_id++;
    if (tp->next_id == 0) tp->next_id = 1;
    return id;
}

static inline Tween *tween__find(TweenPool *tp, uint16_t id) {
    if (id == 0) return NULL;
    for (int i = 0; i < TWEEN_POOL_SIZE; i++)
        if (tp->pool[i].id == id) return &tp->pool[i];
    return NULL;
}

static inline Tween *tween__alloc_slot(TweenPool *tp) {
    // Prefer free slots, then reuse done slots
    for (int i = 0; i < TWEEN_POOL_SIZE; i++)
        if (tp->pool[i].state == TWEEN_FREE) return &tp->pool[i];
    for (int i = 0; i < TWEEN_POOL_SIZE; i++)
        if (tp->pool[i].state == TWEEN_DONE) return &tp->pool[i];
    return NULL; // pool exhausted
}

// ---- API ----

// Start a tween. Returns handle (nonzero) or 0 on pool exhaustion.
static inline uint16_t tween_start(TweenPool *tp, double *target,
                                   double from, double to,
                                   double duration, EaseCurve curve) {
    Tween *tw = tween__alloc_slot(tp);
    if (!tw) return 0;
    tw->id = tween__alloc_id(tp);
    tw->chain_next = 0;
    tw->target = target;
    tw->from = from;
    tw->to = to;
    tw->delay = 0;
    tw->duration = duration;
    tw->elapsed = 0;
    tw->curve = curve;
    tw->state = TWEEN_RUNNING;
    tw->ping_pong = false;
    if (target) *target = from;
    return tw->id;
}

// Start a tween with a delay before it begins.
static inline uint16_t tween_start_delayed(TweenPool *tp, double *target,
                                           double from, double to,
                                           double delay, double duration,
                                           EaseCurve curve) {
    uint16_t id = tween_start(tp, target, from, to, duration, curve);
    Tween *tw = tween__find(tp, id);
    if (tw) {
        tw->delay = delay;
        tw->state = TWEEN_WAITING;
    }
    return id;
}

// Chain: start tween B when tween A completes.
// B is created in WAITING state with infinite delay; A's completion triggers it.
static inline uint16_t tween_after(TweenPool *tp, uint16_t after_id,
                                   double *target, double from, double to,
                                   double duration, EaseCurve curve) {
    Tween *prev = tween__find(tp, after_id);
    if (!prev) return 0;

    Tween *tw = tween__alloc_slot(tp);
    if (!tw) return 0;
    tw->id = tween__alloc_id(tp);
    tw->chain_next = 0;
    tw->target = target;
    tw->from = from;
    tw->to = to;
    tw->delay = 1e30; // effectively infinite — activated by chain
    tw->duration = duration;
    tw->elapsed = 0;
    tw->curve = curve;
    tw->state = TWEEN_WAITING;
    tw->ping_pong = false;

    prev->chain_next = tw->id;
    return tw->id;
}

static inline void tween_cancel(TweenPool *tp, uint16_t id) {
    Tween *tw = tween__find(tp, id);
    if (tw) { tw->state = TWEEN_FREE; tw->id = 0; }
}

// Cancel all tweens targeting a specific variable
static inline void tween_cancel_target(TweenPool *tp, double *target) {
    for (int i = 0; i < TWEEN_POOL_SIZE; i++) {
        if (tp->pool[i].target == target && tp->pool[i].state != TWEEN_FREE) {
            tp->pool[i].state = TWEEN_FREE;
            tp->pool[i].id = 0;
        }
    }
}

static inline bool tween_active(const TweenPool *tp, uint16_t id) {
    for (int i = 0; i < TWEEN_POOL_SIZE; i++)
        if (tp->pool[i].id == id)
            return tp->pool[i].state == TWEEN_RUNNING || tp->pool[i].state == TWEEN_WAITING;
    return false;
}

// Returns true if any tween is running or waiting
static inline bool tween_any_active(const TweenPool *tp) {
    for (int i = 0; i < TWEEN_POOL_SIZE; i++)
        if (tp->pool[i].state == TWEEN_RUNNING || tp->pool[i].state == TWEEN_WAITING)
            return true;
    return false;
}

// Advance all tweens by dt seconds
static inline void tween_update_all(TweenPool *tp, double dt) {
    for (int i = 0; i < TWEEN_POOL_SIZE; i++) {
        Tween *tw = &tp->pool[i];
        if (tw->state == TWEEN_FREE || tw->state == TWEEN_DONE) continue;

        tw->elapsed += dt;

        if (tw->state == TWEEN_WAITING) {
            if (tw->elapsed >= tw->delay) {
                tw->state = TWEEN_RUNNING;
                tw->elapsed -= tw->delay;
                tw->delay = 0;
                if (tw->target) *tw->target = tw->from;
            }
            continue;
        }

        // TWEEN_RUNNING
        double t = (tw->duration > 0) ? (tw->elapsed / tw->duration) : 1.0;
        if (t >= 1.0) {
            t = 1.0;
            if (tw->target) *tw->target = tw->to;

            if (tw->ping_pong) {
                // Reverse direction
                double tmp = tw->from;
                tw->from = tw->to;
                tw->to = tmp;
                tw->elapsed = 0;
            } else {
                tw->state = TWEEN_DONE;
                // Activate chained tween
                if (tw->chain_next) {
                    Tween *next = tween__find(tp, tw->chain_next);
                    if (next && next->state == TWEEN_WAITING) {
                        next->delay = 0;
                        next->elapsed = 0;
                        next->state = TWEEN_RUNNING;
                        if (next->target) *next->target = next->from;
                    }
                }
            }
        } else {
            double v = ease_eval(tw->curve, t);
            if (tw->target) *tw->target = tw->from + (tw->to - tw->from) * v;
        }
    }
}

#endif // TWEEN_H
