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

// ---- Transitions ----

typedef enum {
    TRANS_NONE,
    TRANS_CROSSFADE,
    TRANS_ZOOM,
} TransitionKind;

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
    TimeWarp    warp;
    RenderStyle style;
};

// Now Scene is defined — include view function implementations
#define SCENE_DEFINED
#include "views/view_clock.h"
#include "views/view_calendar.h"
#include "views/view_solar.h"

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
    timewarp_init(&sc->warp);
    sc->style = style_default();
    sc->dwell_duration = 10.0;

    sc->views[VIEW_CLOCK].state    = &sc->clock_state;
    sc->views[VIEW_CALENDAR].state = &sc->calendar_state;
    sc->views[VIEW_SOLAR].state    = &sc->solar_state;
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
    }
}

// ---- Update ----

static inline void scene_update(Scene *sc, const Tempus *t, double dt) {
    tween_update_all(&sc->tweens, dt);
    timewarp_update(&sc->warp, dt);

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
        if (v->vt && v->vt->render)
            v->vt->render(v->state, d, t, &sc->style);
    }
}

static inline void scene_event(Scene *sc, const Tempus *t, int key) {
    (void)sc; (void)t; (void)key;
}

#endif // SCENE_H
