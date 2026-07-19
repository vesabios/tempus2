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
    double  orbb;         // mirrored scene orbis_blend (wheel breathes out)

    // The 24-hour ring, ONE OBJECT across the stations that declare
    // it (station_table hour_r/hour_w): presence and radius blended
    // over the station weight vector, so a CAELVM -> ASTROLABIVM
    // flight GLIDES the ring between its two seats
    float   hr_a, hr_r, hr_w;

    // THE SKY CIRCLE — one shape, one drawer, every station. Seat
    // and radius blend between CAELVM's bowl (centered, growing off
    // the bezel) and the astrolabe's horizon circle; the plate's
    // limb clips it; colors are the astro view's minute-cached
    // atmosphere. alpha = the chart family's combined weight.
    float   sky_a, sky_wc;
    double  skyb_l, astb_l;   // raw blends (bezel growth, clip)
    const AstroViewState *astv;

    // Wheel dragging (see scene_pointer): film-strip time scrubbing,
    // incremental (finger deltas projected onto the band tangent), with
    // flywheel inertia — release mid-motion and the machine keeps
    // spinning, decaying like a physical wheel
    bool    wheel_dragging;
    float   last_wx, last_wy;   // pointer position at last event
    double  drag_accum;         // days moved since last update tick
    double  fling_vel;          // days/second; nonzero = coasting
    bool    fling_week;         // HORAE band: the date steps in whole
                                // WEEK clicks (the station's quantum)
    double  week_accum;         // silent accumulator toward the click
    bool    fling_keep_time;    // the fling's source: day-only (HORAE
                                // band) vs fractional (ring, elsewhere)

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

        // Hairline: a 2-wide bar ALONG THE RADIAL (the original's
        // rotated-frame rect; an axis-aligned rect here reads as a
        // skewed box at most angles)
        draw_set_color(d, s->holiday_stroke);
        draw_line(d, px + cs * s->glyph_line_start,
                  py + sn * s->glyph_line_start,
                  px + cs * s->glyph_line_end,
                  py + sn * s->glyph_line_end, 2.0f);

        if (i % 2 == 1) {
            draw_set_color(d, s->glyph_color);
            float gx = px + cs * ypos;
            float gy = py + sn * ypos;

            if (i == 7)
                draw_circle_stroked(d, gx, gy, 15.0f, 1.5f);
            else if (i == 3)
                draw_circle_filled(d, gx, gy, 15.0f);
            else if (i == 1 || i == 5) {
                // Equinox: the half-disc — a semicircle with its flat
                // edge tangential, the visible half toward the center
                // (the original covered the outer half with a rotated
                // black rect; we just draw the surviving half)
                float ta = atan2f(sn, cs);
                draw_arc_filled(d, gx, gy, 0.0f, 15.0f,
                                ta + (float)M_PI * 0.5f,
                                ta + (float)M_PI * 1.5f, 24);
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
    st->orbb = sc->orbis_wheel;
    st->astv = &sc->astro_state;
    st->skyb_l = sc->sky_blend;
    st->astb_l = sc->astro_blend;
    {
        double fam = sc->sky_blend + sc->astro_blend;
        st->sky_a = (float)(fam > 1.0 ? 1.0 : fam);
        st->sky_wc = fam > 1.0e-6
                   ? (float)(sc->sky_blend / fam) : 0.0f;
    }
    {
        double a = 0, r = 0, w = 0;
        for (int i = 0; i < ST_COUNT; i++) {
            if (station_table[i].hour_r <= 0) continue;
            a += sc->stw[i];
            r += sc->stw[i] * station_table[i].hour_r;
            w += sc->stw[i] * station_table[i].hour_w;
        }
        st->hr_a = (float)a;
        st->hr_r = a > 1.0e-6 ? (float)(r / a) : 0.0f;
        st->hr_w = a > 1.0e-6 ? (float)(w / a) : 0.0f;
    }
    if (st->cached_day != st->tv.day || st->cached_year != st->tv.year)
        cal__rebuild_ticks(st, t, &sc->style);
}

static void cal__hour_ring(const CalendarViewState *st, DrawCtx *d,
                           const RenderStyle *s);
static void cal__sky_circle(const CalendarViewState *st, DrawCtx *d,
                            const RenderStyle *s);

static void calendar_render(const void *buf, DrawCtx *d, const Tempus *t,
                            const RenderStyle *s) {
    const CalendarViewState *st = (const CalendarViewState *)buf;
    const TimeView *tv = &st->tv;
    double blend = st->zoom;
    double year_pct = (tv->jd_current - t->jd_newyear) / t->total_days
                    + tv->percent_of_day / t->total_days;

    float base_r = (float)tempus_wheel_radius(s->calendar_base_radius,
                                              st->sys, st->skyb, st->orbb);
    float radius = base_r + (float)(blend * s->zoom_in_radius);

    // ORBIS asks for a finer wheel: the ring keeps its radius but the
    // FURNITURE shrinks — a global scale with the radius compensated,
    // so marks, text, band, pointer, and glyphs all fine down together
    float cal_s = 1.0f - 0.15f * (float)st->orbb;
    float save_sx = d->sx, save_sy = d->sy;
    d->sx *= cal_s;
    d->sy *= cal_s;
    base_r /= cal_s;
    radius /= cal_s;

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

    // Current-month arc: OUTSIDE the tick ring entirely (the original's
    // negative width builds outward: offset 49, width -30 => the band
    // spans +49..+79, clear of the ticks, the pointer nesting between)
    {
        int cm = tv->month - 1;
        float arc_r = radius + (float)tempus_mix(s->month_arc_radius_a, s->month_arc_radius_b, blend);
        float arc_w = (float)tempus_mix(s->month_arc_width_a, s->month_arc_width_b, blend);
        float m0 = (float)((t->jd_months[cm] - t->jd_newyear) / t->total_days);
        float m1 = (float)((t->jd_months[cm + 1] - t->jd_newyear) / t->total_days);
        float a0 = m0 * 2.0f * (float)M_PI - (float)(M_PI * 0.5);
        float a1 = m1 * 2.0f * (float)M_PI - (float)(M_PI * 0.5);
        draw_set_color(d, dc_scale(s->month_color, 0.5f));
        draw_arc_filled(d, 0, 0, arc_r, arc_r - arc_w, a0, a1, 48);
    }

    // Day tick marks (from cache), the original's grammar: ordinary
    // days are HAIRLINES, month boundaries (and the leap tick) are
    // 4-wide BARS, and the span holds radius..radius+20 at every
    // zoom — the date numerals move inside instead.
    float tick_in = radius;
    float tick_out = radius + 20.0f;
    for (int i = 0; i < st->num_ticks; i++) {
        bool bar = true;
        switch (st->ticks[i].kind) {
            case TICK_LEAP:           draw_set_color(d, s->leap_year); break;
            case TICK_MONTH_BOUNDARY: draw_set_color(d, s->month_color); break;
            case TICK_TODAY_MONTH:    draw_set_color(d, s->month_text_color); break;
            case TICK_TODAY:
                draw_set_color(d, s->month_color);
                bar = false;
                break;
            default:
                draw_set_color(d, s->day_marks);
                bar = false;
                break;
        }
        float ix, iy, ox, oy;
        cal__fc(st->ticks[i].angle, tick_in, &ix, &iy);
        cal__fc(st->ticks[i].angle, tick_out, &ox, &oy);
        if (bar)
            draw_line(d, ix, iy, ox, oy, 4.0f);
        else
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
                    float nr = nflip ? (radius - 12.0f) : (radius - 12.0f - gh);
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
            // Waist-centered on the nominal radius: without the flip
            // compensation, top-half names ride farther out than
            // bottom-half ones (the original's renderer centered the
            // letterform band)
            float na = fmodf(angle, 2.0f * (float)M_PI);
            if (na < 0) na += 2.0f * (float)M_PI;
            bool mflip = (na > (float)M_PI * 0.5f
                          && na < (float)M_PI * 1.5f);
            float msz = _font_compat[FONT_month].size;
            float mr = text_r - msz * 0.5f
                     + msz * (mflip ? 0.51f : 0.37f);
            draw_text_curved(d, FONT_month, 0, 0, mr,
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

        // The original's wheel pointer: a HOLLOW triangle, 30 wide by
        // 28 tall, apex aimed radially INWARD and capping the day
        // tick's outer end (offset 9 + texture inset 11 = +20), flat
        // base outward at +48 — drawn as three strokes, month teal
        draw_set_color(d, s->month_text_color);
        float angle = (float)(pos * M_PI * 2.0);
        float cs = sinf(angle), sn = -cosf(angle);   // radial out
        float tx2 = -sn, ty2 = cs;                   // tangent
        float ax = px - offx + cs * 11.0f;
        float ay = py - offy + sn * 11.0f;
        float b1x = px - offx + cs * 39.0f + tx2 * 15.0f;
        float b1y = py - offy + sn * 39.0f + ty2 * 15.0f;
        float b2x = px - offx + cs * 39.0f - tx2 * 15.0f;
        float b2y = py - offy + sn * 39.0f - ty2 * 15.0f;
        draw_line(d, ax, ay, b1x, b1y, 2.2f);
        draw_line(d, b1x, b1y, b2x, b2y, 2.2f);
        draw_line(d, b2x, b2y, ax, ay, 2.2f);
    }

    d->sx = save_sx;
    d->sy = save_sy;

    cal__sky_circle(st, d, s);
    cal__hour_ring(st, d, s);
}

// ---- THE SKY CIRCLE: one shape, every station ----
static void cal__sky_circle(const CalendarViewState *st, DrawCtx *d,
                            const RenderStyle *s) {
    float fam = st->sky_a;
    if (fam < 0.004f || !st->astv) return;
    const AstroViewState *av = st->astv;
    float base_alpha = d->alpha;
    float wc = st->sky_wc;
    // CAELVM's seat: centered, radius growing off the live bezel as
    // the chart forms (the old bowl's own law)
    float bez = (float)tempus_wheel_radius(s->calendar_base_radius,
                                           st->sys, st->skyb_l,
                                           st->orbb);
    float r_cael = bez + (280.0f - bez)
                 * (float)tempus_smoothstep(0.0, 1.0, st->skyb_l);
    float cy = av->sky_hyc * (1.0f - wc);
    float cr = av->sky_hr + (r_cael - av->sky_hr) * wc;
    // The plate's limb clips the circle; at CAELVM the circle never
    // reaches it, so one clip serves every station
    float clip = 400.0f;
    // The dark earth under CAELVM's whole chart, beneath the circle
    if (wc > 0.004f) {
        d->alpha = base_alpha * fam * wc;
        draw_set_color(d, dca(0.055f, 0.038f, 0.030f, 1.0f));
        draw_circle_filled(d, 0, 0, 560.0f);
    }
    int prev[48 + 1], curv[48 + 1];
    d->alpha = base_alpha * 0.94f * fam;
    draw_set_color(d, dca(av->sky_cols[0][0], av->sky_cols[0][1],
                          av->sky_cols[0][2], 1.0f));
    int cvi = draw__push_vert(d, 0, cy, d->white_u, d->white_v);
    static const float alts[8] = { 75, 60, 45, 30, 18, 8, 3, 0 };
    for (int ri = 0; ri < 8; ri++) {
        float rr2 = cr * (90.0f - alts[ri]) / 90.0f;
        for (int si = 0; si <= 48; si++) {
            const float *c = av->sky_cols[1 + ri * 49 + si];
            draw_set_color(d, dca(c[0], c[1], c[2], 1.0f));
            float az = (float)si / 48.0f * 2.0f * (float)M_PI;
            float x = -sinf(az) * rr2;
            float y = cy + cosf(az) * rr2;
            float pr2 = sqrtf(x * x + y * y);
            if (pr2 > clip) {
                x *= clip / pr2;
                y *= clip / pr2;
            }
            int vi = draw__push_vert(d, x, y, d->white_u,
                                     d->white_v);
            curv[si] = vi;
            if (si > 0) {
                if (ri == 0) {
                    draw__tri(d, cvi, curv[si - 1], vi);
                } else {
                    draw__tri(d, prev[si - 1], curv[si - 1], vi);
                    draw__tri(d, prev[si - 1], vi, prev[si]);
                }
            }
        }
        memcpy(prev, curv, sizeof(prev));
    }
    d->alpha = base_alpha;
}

// ---- The 24-hour ring: the rendering-time control, one object ----
// CAELVM's dial and the astrolabe's share one body: hairlines, 24
// ticks, the gold mark at the rendering instant. Radius and width
// ride the station weight vector; presence stages in late (the ink
// law), so the ring forms as its station arrives.
static void cal__hour_ring(const CalendarViewState *st, DrawCtx *d,
                           const RenderStyle *s) {
    float a = ink_in(INK_CHART_LATE, st->hr_a);
    if (a < 0.004f || st->hr_r <= 0) return;
    float base_alpha = d->alpha;
    float r0 = st->hr_r, r1 = st->hr_r + st->hr_w;
    d->alpha = base_alpha * a;
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.22f));
    draw_circle_stroked(d, 0, 0, r0, 1.0f);
    draw_circle_stroked(d, 0, 0, r1, 1.0f);
    for (int h = 0; h < 24; h++) {
        float an = (float)h / 24.0f * 2.0f * (float)M_PI;
        float sx = sinf(an), sy = -cosf(an);
        bool major = (h % 6) == 0;
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f,
                              major ? 0.55f : 0.28f));
        float t0 = major ? r0 : r0 + st->hr_w * 0.25f;
        draw_line(d, sx * t0, sy * t0, sx * r1, sy * r1, 1.0f);
    }
    float an = (float)st->tv.percent_of_day * 2.0f * (float)M_PI;
    float sx = sinf(an), sy = -cosf(an);
    draw_set_color(d, dc_scale(s->sunrise_handle, 1.05f));
    draw_line(d, sx * (r0 - 3.0f), sy * (r0 - 3.0f),
              sx * (r1 + 3.0f), sy * (r1 + 3.0f), 1.8f);
    draw_circle_filled(d, sx * (r0 + st->hr_w * 0.5f),
                       sy * (r0 + st->hr_w * 0.5f), 4.5f);
    d->alpha = base_alpha;
}

static const ViewVtable calendar_vtable = {
    .init   = calendar_init,
    .enter  = calendar_enter,
    .exit   = calendar_exit,
    .update = calendar_update,
    .render = calendar_render,
};

#endif // SCENE_DEFINED && !VIEW_CALENDAR_IMPL
