// view.h — View abstraction for Tempus
// A view is a self-contained visualization with lifecycle, state, and rendering.

#ifndef VIEW_H
#define VIEW_H

#include "tempus.h"
#include "draw.h"
#include "tween.h"
#include "timewarp.h"

// ---- Forward decl ----
typedef struct Scene Scene;

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
} RenderStyle;

static inline RenderStyle style_default(void) {
    RenderStyle s = {0};
    s.calendar_base_radius = 450.0f;
    s.zoom_in_radius = 2400.0f;
    s.sunrise_dial_offset = -195.0f;
    s.sunrise_dial_radius = 80.0f;
    s.month_arc_radius_a = 49.0f;  s.month_arc_width_a = -30.0f;
    s.month_arc_radius_b = 52.0f;  s.month_arc_width_b = -49.0f;
    s.month_text_radius_a = 118.0f; s.month_text_radius_b = 130.0f;
    s.wheel_pointer_offset_a = 9.0f; s.wheel_pointer_offset_b = 9.0f;
    s.glyph_start_offset = 50.0f;
    s.glyph_end_offset = -120.0f;
    s.glyph_line_start = -10.0f;
    s.glyph_line_end = 19.0f;
    s.year_stroke_start = -9.0f;
    s.year_stroke_length = 20.0f;
    s.seconds_start = 60.0f;  s.seconds_end = 300.0f;
    s.minutes_width = 12.0f;  s.minutes_start = 60.0f;  s.minutes_end = 250.0f;
    s.hours_width = 20.0f;    s.hours_start = 60.0f;    s.hours_end = 200.0f;
    s.sun_size = 7.0f;
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
    s.globe_light     = dc_u8(59, 13, 22);
    s.globe_shadow    = dc_u8(22, 0, 17);
    return s;
}

// ---- View lifecycle ----

typedef enum {
    VIEW_CLOCK,
    VIEW_CALENDAR,
    VIEW_SOLAR,
    VIEW_COUNT,
} ViewId;

// Forward declarations of concrete view state types (defined in views/*.h)
typedef struct ClockViewState ClockViewState;
typedef struct SolarViewState SolarViewState;
typedef struct CalendarViewState CalendarViewState;

// View state is a tagged union — properly typed, no byte buffer casts.
typedef union {
    ClockViewState    *clock;
    SolarViewState    *solar;
    CalendarViewState *calendar;
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
