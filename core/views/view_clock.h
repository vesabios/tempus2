// view_clock.h — Analog clock face view

#ifndef VIEW_CLOCK_H
#define VIEW_CLOCK_H

#include "../view.h"

struct ClockViewState {
    TimeView tv;  // must be first field
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

// The face furniture — logo, numerals, ticks — renders in its own
// pass (VIEW_CLOCKBACK) so it layers BENEATH the orrery's globes:
// the growing ORBIS planet covers the markings, while the hands
// (VIEW_CLOCK, below) stay above everything.
static void clockback_render(const void *buf, DrawCtx *d, const Tempus *t, const RenderStyle *s) {
    (void)buf; (void)t;

    // Logo
    draw_set_color(d, s->logo_text);
    draw_text_centered(d, FONT_logo, 0, 80.0f, "T E M P V S");

    // The globe occupies the 12 position: numeral 12 is not drawn, and
    // ticks are clipped against the globe's margin disc so they stop at
    // its edge instead of running underneath.
    float mcy = s->sunrise_dial_offset;
    float mrad = s->sunrise_dial_radius + 16.0f;

    // Moonphase aperture at 6 — diametrically opposite the earth
    float moon_y = -s->sunrise_dial_offset;
    float moon_r = 52.0f;

    // Hour numbers (12 and 6 skipped — the planet is the 12, the moon
    // is the 6)
    draw_set_color(d, s->clock_lines_strong);
    for (int i = 1; i < 12; i++) {
        if (i == 6) continue;
        float a = (float)i / 12.0f;
        float px, py;
        clock__fc(a, 225.0f, &px, &py);
        char num[4];
        snprintf(num, sizeof(num), "%d", i);
        draw_text_centered(d, FONT_clock, px, py, num);
    }

    // Tick marks, clipped at both bodies' margins
    float mask_cy[2] = { mcy, moon_y };
    float mask_r[2]  = { mrad, moon_r + 14.0f };
    for (int i = 0; i < 60; i++) {
        float a = (float)i / 60.0f;
        float outer = 300.0f;
        float inner = (i % 5 == 0) ? 250.0f : 270.0f;
        float ux = sinf(a * 2.0f * (float)M_PI);
        float uy = -cosf(a * 2.0f * (float)M_PI);
        bool covered = false;
        for (int k = 0; k < 2 && !covered; k++) {
            // Ray s*u vs disc centered (0, cy): clip the inner end outward
            float uc = uy * mask_cy[k];
            float disc = uc * uc - (mask_cy[k] * mask_cy[k] - mask_r[k] * mask_r[k]);
            if (disc > 0) {
                float sb = uc + sqrtf(disc);
                if (sb > inner) inner = sb;
                if (inner >= outer) covered = true;
            }
        }
        if (covered) continue;
        draw_set_color(d, (i % 5 == 0) ? s->clock_lines_strong : s->clock_lines);
        draw_line_thin(d, ux * outer, uy * outer, ux * inner, uy * inner);
    }

    // (The moon itself is rendered by the orrery view — one object that
    // morphs between the 6 o'clock aperture and its heliocentric orbit.
    // This view only reserves the aperture space: numeral 6 skipped and
    // ticks clipped above.)

    // The dial's hairlines — the outer ring around the 12-o'clock
    // globe and the moon aperture rim — live down here with the rest
    // of the furniture, UNDER the globes (they used to ride over the
    // limbs from the orrery's furniture pass)
    draw_set_color(d, dc_scale(s->sunrise_lit, 0.8f));
    draw_circle_stroked(d, 0, s->sunrise_dial_offset,
                        s->sunrise_dial_radius + 12.0f, 1.0f);
    draw_set_color(d, dca(0.45f, 0.44f, 0.42f, 0.5f));
    draw_circle_stroked(d, 0, -s->sunrise_dial_offset, 62.0f, 1.0f);
}

static void clock_render(const void *buf, DrawCtx *d, const Tempus *t, const RenderStyle *s) {
    const ClockViewState *st = (const ClockViewState *)buf;
    (void)t;
    const TimeView *tv = &st->tv;
    double real_secs = s->sweep_seconds
        ? ((double)tv->secs + tv->frac_secs) / 60.0
        : (double)tv->secs / 60.0;

    // Hour hand
    {
        float angle = (float)(tv->percent_of_day * 2.0 * M_PI * 2.0);
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
        float m = ((float)tv->mins + (float)real_secs) / 60.0f;
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

// The under-layer: render only (state, lifecycle, and update belong
// to VIEW_CLOCK; this shares its state buffer)
static const ViewVtable clockback_vtable = {
    .render = clockback_render,
};

#endif // SCENE_DEFINED && !VIEW_CLOCK_IMPL
