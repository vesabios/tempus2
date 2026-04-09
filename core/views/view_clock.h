// view_clock.h — Analog clock face view

#ifndef VIEW_CLOCK_H
#define VIEW_CLOCK_H

#include "../view.h"

struct ClockViewState {
    double opacity;
};

#endif // VIEW_CLOCK_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_CLOCK_IMPL)
#define VIEW_CLOCK_IMPL

static inline void clock__fc(double pct, float r, float *x, float *y) {
    *x = (float)(sin(pct * M_PI * 2.0) * r);
    *y = (float)(-cos(pct * M_PI * 2.0) * r);
}

static void clock_init(void *buf, const Tempus *t, const RenderStyle *s) {
    ClockViewState *st = (ClockViewState *)buf;
    st->opacity = 1.0;
    (void)t; (void)s;
}

static void clock_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void clock_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void clock_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    (void)buf; (void)t; (void)dt; (void)sc;
}

static void clock_render(const void *buf, DrawCtx *d, const Tempus *t, const RenderStyle *s) {
    (void)buf;
    double real_secs = ((double)t->secs + t->frac_secs) / 60.0;

    // Logo
    draw_set_color(d, s->logo_text);
    draw_text_centered(d, FONT_logo, 0, 80.0f, "T E M P V S");

    // Hour numbers
    draw_set_color(d, s->clock_lines_strong);
    for (int i = 0; i < 12; i++) {
        float a = (float)i / 12.0f;
        float px, py;
        clock__fc(a, 225.0f, &px, &py);
        char num[4];
        if (i == 0) { num[0]='1'; num[1]='2'; num[2]=0; }
        else snprintf(num, sizeof(num), "%d", i);
        draw_text_centered(d, FONT_clock, px, py, num);
    }

    // Tick marks
    for (int i = 0; i < 60; i++) {
        float a = (float)i / 60.0f;
        float outer = 300.0f;
        float inner = (i % 5 == 0) ? 250.0f : 270.0f;
        draw_set_color(d, (i % 5 == 0) ? s->clock_lines_strong : s->clock_lines);
        float x0, y0, x1, y1;
        clock__fc(a, outer, &x0, &y0);
        clock__fc(a, inner, &x1, &y1);
        draw_line_thin(d, x0, y0, x1, y1);
    }

    // Hour hand
    {
        float angle = (float)(t->percent_of_day * 2.0 * M_PI * 2.0);
        float dx = sinf(angle), dy = -cosf(angle);
        float px = -dy, py = dx;
        float hw = s->hours_width * 0.5f;
        draw_set_color(d, s->hours_color);
        float corners[4][2] = {
            {dx*s->hours_start - px*hw, dy*s->hours_start - py*hw},
            {dx*s->hours_start + px*hw, dy*s->hours_start + py*hw},
            {dx*s->hours_end   + px*hw, dy*s->hours_end   + py*hw},
            {dx*s->hours_end   - px*hw, dy*s->hours_end   - py*hw},
        };
        int base = d->num_verts;
        for (int j = 0; j < 4; j++)
            draw__push_vert(d, corners[j][0], corners[j][1], d->white_u, d->white_v);
        draw__tri(d, base, base+1, base+2);
        draw__tri(d, base, base+2, base+3);
    }

    // Minute hand
    {
        float m = ((float)t->mins + (float)real_secs) / 60.0f;
        float angle = (float)(m * M_PI * 2.0);
        float dx = sinf(angle), dy = -cosf(angle);
        float px = -dy, py = dx;
        float hw = s->minutes_width * 0.5f;
        draw_set_color(d, s->minutes_color);
        float corners[4][2] = {
            {dx*s->minutes_start - px*hw, dy*s->minutes_start - py*hw},
            {dx*s->minutes_start + px*hw, dy*s->minutes_start + py*hw},
            {dx*s->minutes_end   + px*hw, dy*s->minutes_end   + py*hw},
            {dx*s->minutes_end   - px*hw, dy*s->minutes_end   - py*hw},
        };
        int base = d->num_verts;
        for (int j = 0; j < 4; j++)
            draw__push_vert(d, corners[j][0], corners[j][1], d->white_u, d->white_v);
        draw__tri(d, base, base+1, base+2);
        draw__tri(d, base, base+2, base+3);
    }

    // Seconds hand
    {
        draw_set_color(d, s->seconds_color);
        float sx0, sy0, sx1, sy1;
        clock__fc(real_secs, s->seconds_start, &sx0, &sy0);
        clock__fc(real_secs, s->seconds_end, &sx1, &sy1);
        draw_line(d, sx0, sy0, sx1, sy1, 1.5f);
    }
}

static const ViewVtable clock_vtable = {
    .init   = clock_init,
    .enter  = clock_enter,
    .exit   = clock_exit,
    .update = clock_update,
    .render = clock_render,
};

#endif // SCENE_DEFINED && !VIEW_CLOCK_IMPL
