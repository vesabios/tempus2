// view_calendar.h — Calendar wheel view

#ifndef VIEW_CALENDAR_H
#define VIEW_CALENDAR_H

#include "../view.h"

// ---- Cached tick data (rebuilt only on day/year change) ----

#define CAL_MAX_TICKS 400

enum {
    TICK_DAY = 0,
    TICK_MONTH_BOUNDARY,
    TICK_TODAY,
    TICK_TODAY_MONTH,
    TICK_LEAP,
};

typedef struct {
    float   angle;          // fraction of circle (0..1)
    uint8_t kind;
} CachedTick;              // 8 bytes

struct CalendarViewState {
    TimeView tv;  // must be first field
    double  zoom;
    double  target_zoom;
    double  sys;          // mirrored scene system_blend (wheel cedes 20%)
    double  skyb;         // mirrored scene sky_blend (wheel -> sky bezel)

    // Wheel dragging (see scene_pointer): film-strip time scrubbing,
    // incremental (finger deltas projected onto the band tangent), with
    // flywheel inertia — release mid-motion and the machine keeps
    // spinning, decaying like a physical wheel
    bool    wheel_dragging;
    float   last_wx, last_wy;   // pointer position at last event
    double  drag_accum;         // days moved since last update tick
    double  fling_vel;          // days/second; nonzero = coasting

    CachedTick ticks[CAL_MAX_TICKS];
    int        num_ticks;
    int        cached_day;
    int        cached_year;
};

#endif // VIEW_CALENDAR_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_CALENDAR_IMPL)
#define VIEW_CALENDAR_IMPL

static inline void cal__fc(double pct, float r, float *x, float *y) {
    *x = (float)(sin(pct * M_PI * 2.0) * r);
    *y = (float)(-cos(pct * M_PI * 2.0) * r);
}

static void cal__rebuild_ticks(CalendarViewState *st, const Tempus *t,
                               const RenderStyle *s) {
    (void)s;
    const TimeView *tv = &st->tv;
    int total = (int)floor(t->total_days);
    if (cal_is_leap_year(tv->year)) total++;
    if (total > CAL_MAX_TICKS) total = CAL_MAX_TICKS;

    float theta_inc = 1.0f / (float)t->total_days;
    float start_theta = (float)((t->jd_months[0] - t->jd_newyear) / t->total_days);

    int day = 0, month = 0;
    st->num_ticks = 0;

    for (int x = 0; x < total && st->num_ticks < CAL_MAX_TICKS; x++) {
        float theta = start_theta + theta_inc * x;
        bool is_today = (month == tv->month - 1 && day == tv->day - 1);

        uint8_t kind;
        if (month == 1 && day == 28)
            kind = TICK_LEAP;
        else if (day == 0)
            kind = is_today ? TICK_TODAY_MONTH : TICK_MONTH_BOUNDARY;
        else
            kind = is_today ? TICK_TODAY : TICK_DAY;

        st->ticks[st->num_ticks++] = (CachedTick){ .angle = theta, .kind = kind };

        day++;
        if (day >= t->days_in_month[month]) { month++; day = 0; }
    }

    st->cached_day = tv->day;
    st->cached_year = tv->year;
}

static void cal__render_glyphs(DrawCtx *d, const Tempus *t, const RenderStyle *s,
                               float radius, float blend) {
    for (int i = 0; i < 8; i++) {
        float f = (float)tempus_jd_to_wheel_pct(t, t->jd_events[i]);
        float px, py;
        cal__fc(f, radius + 10.0f, &px, &py);

        float ypos = (float)tempus_mix(s->glyph_start_offset, s->glyph_end_offset, blend);
        float angle = f * (float)(M_PI * 2.0);
        float cs = sinf(angle), sn = -cosf(angle);

        draw_set_color(d, s->holiday_stroke);
        float lx0 = px + cs * s->glyph_line_start;
        float ly0 = py + sn * s->glyph_line_start;
        float lx1 = px + cs * s->glyph_line_end;
        float ly1 = py + sn * s->glyph_line_end;
        draw_rect_filled(d, lx0 - 1, ly0, lx1 + 1, ly1);

        if (i % 2 == 1) {
            draw_set_color(d, s->glyph_color);
            float gx = px + cs * ypos;
            float gy = py + sn * ypos;

            if (i == 7)
                draw_circle_stroked(d, gx, gy, 15.0f, 1.5f);
            else if (i == 3)
                draw_circle_filled(d, gx, gy, 15.0f);
            else if (i == 1 || i == 5) {
                draw_circle_filled(d, gx, gy, 15.0f);
                draw_set_color(d, s->clear);
                draw_rect_filled(d, gx - 20, gy, gx + 20, gy + 20);
            }
        }
    }
}

static void calendar_init(void *buf, const Tempus *t, const RenderStyle *s) {
    CalendarViewState *st = (CalendarViewState *)buf;
    st->zoom = 0.0;
    st->target_zoom = 0.0;
    st->cached_day = -1;
    st->cached_year = -1;
    cal__rebuild_ticks(st, t, s);
}

static void calendar_enter(void *buf, const Tempus *t, Scene *sc) {
    CalendarViewState *st = (CalendarViewState *)buf;
    tween_cancel_target(&sc->tweens, &st->zoom);
    double target = (st->zoom < 0.5) ? 1.0 : 0.0;
    st->target_zoom = target;
    tween_start(&sc->tweens, &st->zoom, st->zoom, target, 3.0, EASE_IN_OUT_QUINT);
    (void)t;
}

static void calendar_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void calendar_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    CalendarViewState *st = (CalendarViewState *)buf;
    (void)dt;
    st->sys = sc->system_blend;
    st->skyb = sc->sky_blend;
    if (st->cached_day != st->tv.day || st->cached_year != st->tv.year)
        cal__rebuild_ticks(st, t, &sc->style);
}

static void calendar_render(const void *buf, DrawCtx *d, const Tempus *t,
                            const RenderStyle *s) {
    const CalendarViewState *st = (const CalendarViewState *)buf;
    const TimeView *tv = &st->tv;
    double blend = st->zoom;
    double year_pct = (tv->jd_current - t->jd_newyear) / t->total_days
                    + tv->percent_of_day / t->total_days;

    float base_r = (float)tempus_wheel_radius(s->calendar_base_radius,
                                              st->sys, st->skyb);
    float radius = base_r + (float)(blend * s->zoom_in_radius);

    float offx, offy;
    cal__fc(year_pct, radius - base_r, &offx, &offy);

    float save_tx = d->tx, save_ty = d->ty;
    draw_translate(d, -offx, -offy);

    // Event names
    float label_vis = (float)tempus_smoothstep(600, 1200, radius);
    if (label_vis > 0.01f) {
        float event_vis = (float)tempus_smoothstep(1000, 1500, radius) * 0.7f;
        draw_set_color(d, dc(event_vis, event_vis, event_vis));
        for (int i = 0; i < 8; i++) {
            float ef = (float)tempus_jd_to_wheel_pct(t, t->jd_events[i]);
            float angle = ef * 2.0f * (float)M_PI;
            draw_text_curved(d, FONT_event, 0, 0, radius - 58.0f,
                           angle, tempus_event_name(t, i), 1.8f, 1.0f);
        }
    }

    // Current-month arc: a background band drawn UNDER the day ticks so
    // it never occludes them
    {
        int cm = tv->month - 1;
        float arc_r = radius + (float)tempus_mix(s->month_arc_radius_a, s->month_arc_radius_b, blend);
        float arc_w = (float)tempus_mix(s->month_arc_width_a, s->month_arc_width_b, blend);
        float m0 = (float)((t->jd_months[cm] - t->jd_newyear) / t->total_days);
        float m1 = (float)((t->jd_months[cm + 1] - t->jd_newyear) / t->total_days);
        float a0 = m0 * 2.0f * (float)M_PI - (float)(M_PI * 0.5);
        float a1 = m1 * 2.0f * (float)M_PI - (float)(M_PI * 0.5);
        draw_set_color(d, dc_scale(s->month_color, 0.5f));
        draw_arc_filled(d, 0, 0, arc_r + arc_w, arc_r, a0, a1, 48);
    }

    // Day tick marks (from cache). Zoomed in, they pull inside the ring
    // so the month arc band never crowds them.
    float tick_in = radius - 16.0f * (float)blend;
    float tick_out = radius + 20.0f * (1.0f - (float)blend) - 2.0f * (float)blend;
    for (int i = 0; i < st->num_ticks; i++) {
        switch (st->ticks[i].kind) {
            case TICK_LEAP:           draw_set_color(d, s->leap_year); break;
            case TICK_MONTH_BOUNDARY: draw_set_color(d, s->month_color); break;
            case TICK_TODAY_MONTH:    draw_set_color(d, s->month_text_color); break;
            case TICK_TODAY:          draw_set_color(d, s->month_color); break;
            default:                 draw_set_color(d, s->day_marks); break;
        }
        float ix, iy, ox, oy;
        cal__fc(st->ticks[i].angle, tick_in, &ix, &iy);
        cal__fc(st->ticks[i].angle, tick_out, &ox, &oy);
        draw_line_thin(d, ix, iy, ox, oy);
    }

    // Day-of-month numbers, radially set beside their ticks — fade in
    // once the zoom gives them room to breathe
    {
        float num_vis = (float)tempus_smoothstep(2200, 3800, radius);
        if (num_vis > 0.01f) {
            for (int mi = 0; mi < 12; mi++) {
                int dim = t->days_in_month[mi];
                for (int day = 1; day <= dim; day++) {
                    double jd = t->jd_months[mi] + (day - 1);
                    float pct = (float)((jd - t->jd_newyear) / t->total_days);
                    float theta = pct * 2.0f * (float)M_PI;
                    char buf[3];
                    if (day >= 10) {
                        buf[0] = (char)('0' + day / 10);
                        buf[1] = (char)('0' + day % 10);
                        buf[2] = 0;
                    } else {
                        buf[0] = (char)('0' + day);
                        buf[1] = 0;
                    }
                    bool is_today = (mi + 1 == tv->month && day == tv->day);
                    DrawColor c = is_today ? s->month_text_color
                                           : dc_scale(s->medium_grey, 0.55f);
                    c.a = num_vis;
                    draw_set_color(d, c);
                    // Tangential, half-size; curved text keeps the glyphs
                    // upright (baseline toward the wheel on the bottom
                    // half, away from it on top). The two branches place
                    // glyphs on opposite sides of the baseline circle, so
                    // compensate the radius to keep the numeral band at a
                    // constant depth inside the rim.
                    float na = fmodf(theta, 2.0f * (float)M_PI);
                    if (na < 0) na += 2.0f * (float)M_PI;
                    bool nflip = (na > (float)M_PI * 0.5f && na < (float)M_PI * 1.5f);
                    float gh = _font_compat[FONT_event].size * 0.5f * 0.8f;
                    float nr = nflip ? (tick_in - 12.0f) : (tick_in - 12.0f - gh);
                    draw_text_curved(d, FONT_event, 0, 0, nr,
                                     theta, buf, 0.05f, 0.5f);
                }
            }
        }
    }

    // Year boundary
    float start_theta = (float)((t->jd_months[0] - t->jd_newyear) / t->total_days);
    draw_set_color(d, s->year_stroke);
    {
        float ix, iy, ox, oy;
        cal__fc(start_theta, radius + s->year_stroke_start, &ix, &iy);
        cal__fc(start_theta, radius + s->year_stroke_start + s->year_stroke_length, &ox, &oy);
        draw_line(d, ix, iy, ox, oy, 2.0f);
    }

    cal__render_glyphs(d, t, s, radius, (float)blend);

    d->tx = save_tx; d->ty = save_ty;

    int cur_month = tv->month - 1;

    // Month names
    {
        float text_r = radius + (float)tempus_mix(s->month_text_radius_a, s->month_text_radius_b, blend);
        // additive tracking (em): letterspacing widens as the ring zooms
        float text_mix = (float)tempus_mix(0.6, 2.4, blend);
        float outer_mix = (float)tempus_mix(0.1, 0.6, blend);

        float stx = d->tx, sty = d->ty;
        draw_translate(d, -offx, -offy);

        for (int i = 0; i < 12; i++) {
            draw_set_color(d, (i == cur_month)
                ? s->month_text_color : dc_scale(s->medium_grey, outer_mix));
            double mid_jd = (t->jd_months[i] + t->jd_months[i + 1]) / 2.0;
            float mf = (float)tempus_jd_to_wheel_pct(t, mid_jd);
            float angle = mf * 2.0f * (float)M_PI;
            draw_text_curved(d, FONT_month, 0, 0, text_r,
                           angle, tempus_month_name(t, i), text_mix, 1.0f);
        }
        d->tx = stx; d->ty = sty;
    }

    // Wheel pointer
    {
        float pointer_r = radius + (float)tempus_mix(
            s->wheel_pointer_offset_a, s->wheel_pointer_offset_b, blend);
        double pos = tempus_jd_to_wheel_pct(t, tv->jd_current + tv->percent_of_day);
        float px, py;
        cal__fc(pos, pointer_r, &px, &py);

        draw_set_color(d, s->month_text_color);
        float angle = (float)(pos * M_PI * 2.0);
        float cs = sinf(angle), sn = -cosf(angle);
        float size = 8.0f;
        float tip_x = px - offx - cs * size;
        float tip_y = py - offy - sn * size;
        float base_x = px - offx + cs * size;
        float base_y = py - offy + sn * size;
        float perp_x = -sn * size * 0.6f;
        float perp_y = cs * size * 0.6f;

        int base = d->num_verts;
        draw__push_vert(d, tip_x, tip_y, d->white_u, d->white_v);
        draw__push_vert(d, base_x + perp_x, base_y + perp_y, d->white_u, d->white_v);
        draw__push_vert(d, base_x - perp_x, base_y - perp_y, d->white_u, d->white_v);
        draw__tri(d, base, base+1, base+2);
    }
}

static const ViewVtable calendar_vtable = {
    .init   = calendar_init,
    .enter  = calendar_enter,
    .exit   = calendar_exit,
    .update = calendar_update,
    .render = calendar_render,
};

#endif // SCENE_DEFINED && !VIEW_CALENDAR_IMPL
