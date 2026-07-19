// view.h — View abstraction for Tempus
// A view is a self-contained visualization with lifecycle, state, and rendering.

#ifndef VIEW_H
#define VIEW_H

#include "tempus.h"
#include "atmo.h"
#include "draw.h"
#include "globe.h"
#include "tween.h"
#include "timewarp.h"

// ---- Forward decl ----
typedef struct Scene Scene;

// ---- Globe overlays: static encodings of solar motion ----

typedef enum {
    GLOBE_OVERLAY_NONE = 0,
    GLOBE_OVERLAY_CHRONO,     // terminator multi-exposure (dense -> heatmap)
    GLOBE_OVERLAY_DAYLIGHT,   // analytic daylight-hours field (chrono's limit)
    GLOBE_OVERLAY_ENVELOPE,   // solstice terminators + tropics/polar circles
    GLOBE_OVERLAY_SUNPATHS,   // dial-side solstice/today sun paths + analemma
    GLOBE_OVERLAY_COUNT
} GlobeOverlay;

// ---- Style (shared across views) ----

typedef struct {
    // Calendar wheel
    float calendar_base_radius;
    float zoom_in_radius;

    // Sunrise dial
    float sunrise_dial_offset;
    float sunrise_dial_radius;

    // Month arcs
    float month_arc_radius_a, month_arc_width_a;
    float month_arc_radius_b, month_arc_width_b;
    float month_text_radius_a, month_text_radius_b;
    float wheel_pointer_offset_a, wheel_pointer_offset_b;

    // Glyphs (sabbat markers)
    float glyph_start_offset, glyph_end_offset;
    float glyph_line_start, glyph_line_end;

    // Year stroke
    float year_stroke_start, year_stroke_length;

    // Clock hands
    float seconds_start, seconds_end;
    float minutes_width, minutes_start, minutes_end;
    float hours_width, hours_start, hours_end;

    // Sun
    float sun_size;

    // Second hand: continuous sweep (needs high frame rate) or per-second tick
    bool sweep_seconds;

    // Colors
    DrawColor clear;
    DrawColor logo_text;
    DrawColor dark_blue;
    DrawColor dark_grey;
    DrawColor medium_grey;
    DrawColor month_color;
    DrawColor month_text_color;
    DrawColor month_lines;
    DrawColor glyph_color;
    DrawColor holiday_stroke;
    DrawColor leap_year;
    DrawColor year_stroke;
    DrawColor day_marks;
    DrawColor seconds_color;
    DrawColor minutes_color;
    DrawColor hours_color;
    DrawColor clock_lines_strong;
    DrawColor clock_lines;
    DrawColor sunrise_handle;
    DrawColor sunrise_lit;
    DrawColor globe_light;
    DrawColor globe_shadow;
    DrawColor globe_grid;        // graticule lines (alpha = line strength)
    DrawColor globe_terminator;  // day/night hairline (alpha = line strength)

    int globe_overlay;           // GlobeOverlay mode (debug-gated for now)
} RenderStyle;

static inline RenderStyle style_default(void) {
    RenderStyle s = {0};
    s.calendar_base_radius = 450.0f;
    s.zoom_in_radius = 7200.0f;
    s.sunrise_dial_offset = -195.0f;
    s.sunrise_dial_radius = 80.0f;
    s.month_arc_radius_a = 51.0f;  s.month_arc_width_a = -39.0f;
    s.month_arc_radius_b = 54.0f;  s.month_arc_width_b = -49.0f;
    s.month_text_radius_a = 82.0f; s.month_text_radius_b = 94.0f;
    s.wheel_pointer_offset_a = 9.0f; s.wheel_pointer_offset_b = 9.0f;
    s.glyph_start_offset = 108.0f;
    s.glyph_end_offset = -120.0f;
    s.glyph_line_start = -10.0f;
    s.glyph_line_end = 19.0f;
    s.year_stroke_start = -9.0f;
    s.year_stroke_length = 20.0f;
    s.seconds_start = 60.0f;  s.seconds_end = 300.0f;
    s.minutes_width = 12.0f;  s.minutes_start = 60.0f;  s.minutes_end = 250.0f;
    s.hours_width = 20.0f;    s.hours_start = 60.0f;    s.hours_end = 200.0f;
    s.sun_size = 7.0f;
    s.sweep_seconds = true;
    s.clear           = dc(0, 0, 0);
    s.logo_text       = dc(0.3f, 0.3f, 0.3f);
    s.dark_blue       = dc(0, 0.1f, 0.2f);
    s.dark_grey       = dc(0.25f, 0.25f, 0.25f);
    s.medium_grey     = dc(0.5f, 0.5f, 0.5f);
    s.month_color     = dc_u8(33, 73, 70);
    s.month_text_color = dc_u8(0, 135, 118);
    s.month_lines     = dc_u8(0, 45, 96);
    s.glyph_color     = dc(0.3f, 0.3f, 0.3f);
    s.holiday_stroke  = dc_u8(80, 80, 80);
    s.leap_year       = dc(0.5f, 0.5f, 0);
    s.year_stroke     = dc_u8(221, 240, 0);
    s.day_marks       = dc(0.2f, 0.2f, 0.2f);
    s.seconds_color   = dc(1, 0, 0);
    s.minutes_color   = dc(1, 1, 1);
    s.hours_color     = dc(1, 1, 1);
    s.clock_lines_strong = dc(0.5f, 0.5f, 0.5f);
    s.clock_lines     = dc(0.2f, 0.2f, 0.2f);
    s.sunrise_handle  = dc_u8(168, 95, 0);
    s.sunrise_lit     = dc_u8(64, 26, 18);
    s.globe_light     = dc_u8(92, 26, 30);
    s.globe_shadow    = dc_u8(16, 0, 13);
    s.globe_grid      = dca(0.42f, 0.40f, 0.38f, 0.30f);
    s.globe_terminator = dca(0.65f, 0.42f, 0.20f, 0.55f);
    return s;
}

// The calendar wheel gives up 20% of its radius at the full-system
// stage — Mercury and Venus are its only tenants there, and the space
// buys a tighter planetary band and a wider moat. Keyed off
// system_blend, so the geo and helio stages are untouched.
static inline double tempus_wheel_scale(double system_blend) {
    return 1.0 - 0.20 * system_blend;
}

// At CAELVM the wheel peels off the forming horizon (both born from the
// same circle) and glides outside the sky chart to become its bezel —
// the time control survives every worldview. Radius shared by the
// calendar renderer and the pointer hit-tests.
#define TEMPUS_SKY_WHEEL_R 610.0
// At the full system the wheel leaves the planet rings entirely and
// glides out to the MOAT — the band of negative space between Pluto's
// ring and the zodiac dial — becoming the machine's outer time bezel.
// Earth's ORBIT stays at its own radius (the orrery keys its geometry
// to base * tempus_wheel_scale, not to this bezel).
#define TEMPUS_SYS_WHEEL_R 762.0
// At ORBIS the wheel breathes outward a little — clearance between
// the globe closeup (r 355, sun and moon ring floating above it) and
// the band.
#define TEMPUS_ORBIS_WHEEL_R 515.0
static inline double tempus_wheel_radius(double base, double system_blend,
                                         double sky_blend,
                                         double orbis_blend) {
    double r = base * tempus_wheel_scale(system_blend);
    r += (TEMPUS_SYS_WHEEL_R - r) * system_blend;
    r += (TEMPUS_ORBIS_WHEEL_R - r) * orbis_blend;
    return r + (TEMPUS_SKY_WHEEL_R - r) * sky_blend;
}

// ---- View lifecycle ----

typedef enum {
    VIEW_CLOCK,
    VIEW_CALENDAR,
    VIEW_SOLAR,
    VIEW_ORRERY,
    VIEW_SKY,
    VIEW_HORAE,
    VIEW_ROTAE,
    VIEW_SAEC,
    VIEW_ORBIS,
    VIEW_LVMEN,
    VIEW_OFFIC,
    VIEW_DRACO,
    VIEW_COUNT,
} ViewId;

// Forward declarations of concrete view state types (defined in views/*.h)
typedef struct ClockViewState ClockViewState;
typedef struct SolarViewState SolarViewState;
typedef struct CalendarViewState CalendarViewState;
typedef struct OrreryViewState OrreryViewState;
typedef struct SkyViewState SkyViewState;
typedef struct HoraeViewState HoraeViewState;
typedef struct RotaeViewState RotaeViewState;
typedef struct SaecViewState SaecViewState;
typedef struct OrbisViewState OrbisViewState;
typedef struct LumenViewState LumenViewState;
typedef struct OfficViewState OfficViewState;
typedef struct DracoViewState DracoViewState;

// View state is a tagged union — properly typed, no byte buffer casts.
typedef union {
    ClockViewState    *clock;
    SolarViewState    *solar;
    CalendarViewState *calendar;
    OrreryViewState   *orrery;
    SkyViewState      *sky;
    HoraeViewState    *horae;
    RotaeViewState    *rotae;
    SaecViewState     *saec;
    OrbisViewState    *orbis;
    LumenViewState    *lumen;
    OfficViewState    *offic;
} ViewStatePtr;

typedef struct {
    void (*init)(void *state, const Tempus *t, const RenderStyle *s);
    void (*enter)(void *state, const Tempus *t, Scene *sc);
    void (*exit)(void *state, const Tempus *t, Scene *sc);
    void (*update)(void *state, const Tempus *t, double dt, Scene *sc);
    void (*render)(const void *state, DrawCtx *d, const Tempus *t, const RenderStyle *s);
} ViewVtable;

typedef struct {
    ViewId           id;
    const ViewVtable *vt;
    double           opacity;
    void            *state;         // points into Scene's storage
} View;

#endif // VIEW_H
