// scene.h — Scene manager
// Owns all views, their state, transitions, and the screensaver cycle.

#ifndef SCENE_H
#define SCENE_H

#include "view.h"

// ---- Include ONLY the state struct definitions (no functions yet) ----
// The view headers guard their function implementations behind SCENE_DEFINED.

#include "views/view_clock.h"
#include "views/view_calendar.h"
#include "views/view_solar.h"
#include "views/view_orrery.h"

// ---- Transitions ----

typedef enum {
    TRANS_NONE,
    TRANS_CROSSFADE,
    TRANS_ZOOM,
} TransitionKind;

// ---- Pacing ----
// The scene reports the frame rate it currently needs; shells map this onto
// their presentation mechanism (swap interval, timer period, animation
// interval). Idle clock ≠ GPU fan.

typedef struct {
    double animate_fps;   // transitions, tweens, time warps
    double sweep_fps;     // sweeping second hand, otherwise idle
    double tick_fps;      // ticking second hand, otherwise idle
    double ambient_fps;   // slowest views only (calendar/solar idle)
} PacePolicy;

static inline PacePolicy pace_default(void) {
    return (PacePolicy){
        .animate_fps = 60.0,
        .sweep_fps   = 20.0,
        .tick_fps    = 4.0,
        .ambient_fps = 1.0,
    };
}

// ---- Scene ----

#define SCENE_MAX_LAYERS  4
#define SCENE_MAX_CYCLE  16

struct Scene {
    // Views
    View        views[VIEW_COUNT];

    // Properly-typed state storage
    ClockViewState    clock_state;
    CalendarViewState calendar_state;
    SolarViewState    solar_state;
    OrreryViewState   orrery_state;

    // Active layer stack (back to front)
    ViewId      layers[SCENE_MAX_LAYERS];
    int         num_layers;

    // Transition
    TransitionKind trans_kind;
    ViewId      trans_from, trans_to;
    double      trans_progress;
    uint16_t    trans_tween_id;
    bool        transitioning;

    // Auto-cycle
    ViewId      cycle[SCENE_MAX_CYCLE];
    int         cycle_len;
    int         cycle_idx;
    double      dwell_timer;
    double      dwell_duration;

    // Shared systems
    TweenPool   tweens;
    RenderStyle style;
    PacePolicy  pace;

    // Geocentric <-> heliocentric morph (0 = clock/dial, 1 = orrery).
    // Tweened; the orrery view morphs the globe, other views crossfade.
    double      helio_blend;
};

// Now Scene is defined — include view function implementations
#define SCENE_DEFINED
#include "views/view_clock.h"
#include "views/view_calendar.h"
#include "views/view_solar.h"
#include "views/view_orrery.h"

// ---- Layer management ----

static inline bool scene_has_layer(const Scene *sc, ViewId id) {
    for (int i = 0; i < sc->num_layers; i++)
        if (sc->layers[i] == id) return true;
    return false;
}

static inline void scene_add_layer(Scene *sc, ViewId id) {
    if (sc->num_layers < SCENE_MAX_LAYERS && !scene_has_layer(sc, id))
        sc->layers[sc->num_layers++] = id;
}

static inline void scene_remove_layer(Scene *sc, ViewId id) {
    for (int i = 0; i < sc->num_layers; i++) {
        if (sc->layers[i] == id) {
            for (int j = i; j < sc->num_layers - 1; j++)
                sc->layers[j] = sc->layers[j + 1];
            sc->num_layers--;
            return;
        }
    }
}

// ---- Transitions ----

static inline void scene_transition_to(Scene *sc, ViewId to,
                                        TransitionKind kind, double duration) {
    if (sc->transitioning) return;
    if (sc->num_layers == 0) {
        scene_add_layer(sc, to);
        View *v = &sc->views[to];
        v->opacity = 1.0;
        if (v->vt && v->vt->enter)
            v->vt->enter(v->state, NULL, sc);
        return;
    }

    sc->trans_kind = kind;
    sc->trans_from = sc->layers[0];
    sc->trans_to = to;
    sc->trans_progress = 0.0;
    sc->transitioning = true;

    View *incoming = &sc->views[to];
    incoming->opacity = 0.0;
    scene_add_layer(sc, to);
    if (incoming->vt && incoming->vt->enter)
        incoming->vt->enter(incoming->state, NULL, sc);

    View *outgoing = &sc->views[sc->trans_from];
    if (outgoing->vt && outgoing->vt->exit)
        outgoing->vt->exit(outgoing->state, NULL, sc);

    sc->trans_tween_id = tween_start(&sc->tweens, &sc->trans_progress,
                                     0.0, 1.0, duration, EASE_IN_OUT_CUBIC);
}

// ---- Init ----

static inline void scene_init(Scene *sc, const Tempus *t) {
    memset(sc, 0, sizeof(Scene));
    tween_pool_init(&sc->tweens);
    sc->style = style_default();
    sc->pace = pace_default();
    sc->dwell_duration = 10.0;

    sc->views[VIEW_CLOCK].state    = &sc->clock_state;
    sc->views[VIEW_CALENDAR].state = &sc->calendar_state;
    sc->views[VIEW_SOLAR].state    = &sc->solar_state;
    sc->views[VIEW_ORRERY].state   = &sc->orrery_state;
}

static inline void scene_register_view(Scene *sc, ViewId id, const ViewVtable *vt) {
    sc->views[id].id = id;
    sc->views[id].vt = vt;
    sc->views[id].opacity = 1.0;
}

static inline void scene_init_views(Scene *sc, const Tempus *t) {
    for (int i = 0; i < VIEW_COUNT; i++) {
        View *v = &sc->views[i];
        if (v->vt && v->vt->init)
            v->vt->init(v->state, t, &sc->style);
        // Views are born synced to the master clock — otherwise the first
        // update derives from a zeroed TimeView, and at ambient pacing the
        // correcting sync can be a full second away.
        if (v->state) {
            TimeView *tv = (TimeView *)v->state;
            tv->synced = true;
            timeview_sync(tv, t);
        }
    }
}

// ---- Update ----

static inline void scene__advance_override_days(Tempus *t, double dv);

static inline void scene_update(Scene *sc, Tempus *t, double dt) {
    tween_update_all(&sc->tweens, dt);

    // Time-scrub flywheel: while dragging, estimate velocity from the
    // accumulated motion; after release, coast with exponential decay —
    // a flicked wheel keeps spinning and slows like a physical machine.
    {
        CalendarViewState *c = &sc->calendar_state;
        bool grabbing = c->wheel_dragging;   // only the wheel has inertia
        if (grabbing && dt > 1e-4) {
            double inst = c->drag_accum / dt;
            c->drag_accum = 0;
            c->fling_vel = c->fling_vel * 0.65 + inst * 0.35;
            if (c->fling_vel > 60.0) c->fling_vel = 60.0;
            if (c->fling_vel < -60.0) c->fling_vel = -60.0;
        } else if (c->fling_vel != 0.0 && t->time_override) {
            scene__advance_override_days(t, c->fling_vel * dt);
            c->fling_vel *= exp2(-dt / 0.7);   // half-life ~0.7s
            if (fabs(c->fling_vel) < 0.05) c->fling_vel = 0.0;
        } else {
            c->fling_vel = 0.0;
        }
    }

    // Sync/update per-view TimeViews
    for (int i = 0; i < VIEW_COUNT; i++) {
        View *v = &sc->views[i];
        if (!v->state) continue;
        TimeView *tv = (TimeView *)v->state; // TimeView is first field
        if (tv->synced) {
            timeview_sync(tv, t);
        } else {
            timeview_update(tv, dt);
        }
    }

    if (sc->transitioning) {
        View *from = &sc->views[sc->trans_from];
        View *to   = &sc->views[sc->trans_to];

        switch (sc->trans_kind) {
            case TRANS_CROSSFADE:
                from->opacity = 1.0 - sc->trans_progress;
                to->opacity = sc->trans_progress;
                break;
            case TRANS_ZOOM:
                from->opacity = 1.0;
                to->opacity = 1.0;
                break;
            default: break;
        }

        if (sc->trans_progress >= 1.0) {
            from->opacity = 0.0;
            to->opacity = 1.0;
            scene_remove_layer(sc, sc->trans_from);
            sc->transitioning = false;
        }
    }

    for (int i = 0; i < sc->num_layers; i++) {
        View *v = &sc->views[sc->layers[i]];
        if (v->vt && v->vt->update)
            v->vt->update(v->state, t, dt, sc);
    }

    if (!sc->transitioning && sc->cycle_len > 0) {
        sc->dwell_timer += dt;
        if (sc->dwell_timer >= sc->dwell_duration) {
            sc->dwell_timer = 0;
            sc->cycle_idx = (sc->cycle_idx + 1) % sc->cycle_len;
            ViewId next = sc->cycle[sc->cycle_idx];
            if (!scene_has_layer(sc, next) || next != sc->layers[0])
                scene_transition_to(sc, next, TRANS_CROSSFADE, 2.0);
        }
    }
}

// ---- Render ----

static inline void scene_render(const Scene *sc, DrawCtx *d, const Tempus *t) {
    for (int i = 0; i < sc->num_layers; i++) {
        const View *v = &sc->views[sc->layers[i]];
        if (v->opacity <= 0.001) continue;
        d->alpha = (float)v->opacity;
        if (v->vt && v->vt->render)
            v->vt->render(v->state, d, t, &sc->style);
    }
    d->alpha = 1.0f;
}

static inline void scene_event(Scene *sc, const Tempus *t, int key) {
    (void)sc; (void)t; (void)key;
}

// ---- Pointer interaction ----
// phase: 0 = down, 1 = move, 2 = up. Coordinates in world units.
// In helio mode the sun bead is draggable: rotating the sun's direction
// around the planet IS choosing the orbit position, i.e. the date. Drags
// engage the manual time override (year component), preserving the time
// of day; the override stays active afterward for inspection.

// Advance the manual override by a signed number of days (fractional —
// hours scroll continuously), wrapping within the override year
static inline void scene__advance_override_days(Tempus *t, double dv) {
    double diy = (double)cal_days_in_year(t->override_year);
    double v = floor(t->override_year_pct * diy) + t->override_day_pct + dv;
    v = fmod(v, diy);
    if (v < 0) v += diy;
    t->override_year_pct = v / diy;
    t->override_day_pct = v - floor(v);
    t->solar_dirty = true;
}

// Engage the manual override at the current moment (shared by drags)
static inline void scene__begin_override(Tempus *t) {
    if (t->time_override) return;
    t->time_override = true;
    t->override_year = t->year;
    t->override_day_pct = t->percent_of_day;
    int doy = 0;
    for (int mm = 1; mm < t->month; mm++)
        doy += cal_days_in_month(mm, t->year);
    doy += t->day - 1;
    t->override_year_pct = (double)doy / cal_days_in_year(t->year);
}

static inline void scene_pointer(Scene *sc, Tempus *t, int phase,
                                 float wx, float wy) {
    OrreryViewState *o = &sc->orrery_state;
    CalendarViewState *c = &sc->calendar_state;

    if (phase == 0) {
        float dx = wx - o->bead_x, dy = wy - o->bead_y;
        if (sc->helio_blend > 0.5
            && dx * dx + dy * dy < o->bead_hit * o->bead_hit) {
            o->dragging = true;
            c->fling_vel = 0;    // grabbing stops the flywheel
            c->drag_accum = 0;
            scene__begin_override(t);
            return;
        }

        // Calendar wheel: film-strip drag anywhere on the band (numerals
        // through month text), excluding the globe's interior
        {
            float R = sc->style.calendar_base_radius
                    + (float)c->zoom * sc->style.zoom_in_radius;
            double ypct = tempus_year_pct(t);
            float ox = sinf((float)(ypct * 2.0 * M_PI))
                     * (R - sc->style.calendar_base_radius);
            float oy = -cosf((float)(ypct * 2.0 * M_PI))
                     * (R - sc->style.calendar_base_radius);
            float px = wx + ox, py = wy + oy;   // undo the wheel pan
            float rp = sqrtf(px * px + py * py);
            float gdx = wx - o->glob_x, gdy = wy - o->glob_y;
            bool on_globe = sc->helio_blend > 0.5
                && gdx * gdx + gdy * gdy < (o->glob_r + 20) * (o->glob_r + 20);
            if (!on_globe && rp > R - 80.0f && rp < R + 150.0f) {
                c->wheel_dragging = true;
                c->last_wx = wx;
                c->last_wy = wy;
                c->fling_vel = 0;    // grabbing stops the flywheel
                c->drag_accum = 0;
                scene__begin_override(t);
            }
        }
    } else if (phase == 1 && c->wheel_dragging) {
        // Incremental: project the finger delta onto the band tangent and
        // move time so the strip follows the finger. The pan rate is
        // 2*pi*(R - base) per year, which is what the finger fights.
        float R = sc->style.calendar_base_radius
                + (float)c->zoom * sc->style.zoom_in_radius;
        double ypct = tempus_year_pct(t);
        float th = (float)(ypct * 2.0 * M_PI);
        float ox = sinf(th) * (R - sc->style.calendar_base_radius);
        float oy = -cosf(th) * (R - sc->style.calendar_base_radius);
        float px = wx + ox, py = wy + oy;
        float rp = sqrtf(px * px + py * py);
        if (rp > 1.0f) {
            // Band tangent at the pointer (direction of increasing date)
            float tx = -py / rp, ty = px / rp;
            float dxf = wx - c->last_wx, dyf = wy - c->last_wy;
            float along = dxf * tx + dyf * ty;
            float denom = R - sc->style.calendar_base_radius;
            if (denom < 120.0f) denom = 120.0f;   // low-zoom coarse rate
            double dv = -(double)along / (2.0 * M_PI * (double)denom)
                      * t->total_days;
            scene__advance_override_days(t, dv);
            c->drag_accum += dv;
        }
        c->last_wx = wx;
        c->last_wy = wy;
    } else if (phase == 1 && o->dragging) {
        float dx = wx - o->glob_x, dy = wy - o->glob_y;
        if (dx * dx + dy * dy > 100.0f) {
            // Bead direction -> sun direction -> wheel angle (yule-based)
            double phi = atan2((double)-dx, (double)dy);
            double pct_yule = phi / (2.0 * M_PI);
            pct_yule -= floor(pct_yule);
            // Convert wheel position to calendar-year fraction
            double doy = pct_yule * t->total_days
                       - (t->jd_months[0] - t->jd_newyear);
            double diy = (double)cal_days_in_year(t->override_year);
            double pct_cal = doy / diy;
            pct_cal -= floor(pct_cal);
            // (No flywheel feed: the sun is a direct positional control —
            // it goes where you put it and stays there on release.)
            t->override_year_pct = pct_cal;
            t->solar_dirty = true;
        }
    } else if (phase == 2) {
        o->dragging = false;
        c->wheel_dragging = false;
    }
}

// ---- Pacing query ----
// The frame rate the scene needs right now. Shells are free to render
// faster (e.g. vsync-locked), but only need to *update* at this rate.

static inline double scene_desired_fps(const Scene *sc) {
    if (sc->transitioning || tween_any_active(&sc->tweens)
        || sc->calendar_state.fling_vel != 0.0)
        return sc->pace.animate_fps;

    double fps = sc->pace.ambient_fps;
    for (int i = 0; i < sc->num_layers; i++) {
        const View *v = &sc->views[sc->layers[i]];
        if (!v->state) continue;

        // Warps animate even when the owning view is an invisible data
        // provider (the solar view drives the orrery's sun)
        const TimeView *tv = (const TimeView *)v->state; // first field
        if (!tv->synced || tv->warp.active)
            return sc->pace.animate_fps;

        if (v->opacity <= 0.001) continue;
        if (v->id == VIEW_CLOCK) {
            double f = sc->style.sweep_seconds ? sc->pace.sweep_fps
                                               : sc->pace.tick_fps;
            if (f > fps) fps = f;
        }
    }
    return fps;
}

#endif // SCENE_H
