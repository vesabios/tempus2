// view_calendar.h — Calendar wheel view

#ifndef VIEW_CALENDAR_H
#define VIEW_CALENDAR_H

#include "../view.h"
#include "../eclipse.h"

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

    // HOROLOGIVM's own weight: the pointer's day numeral belongs to
    // the clock alone (Seren), and rides this so it eases with the
    // station instead of popping on arrival.
    float   horo;

    // DRACO's weight, and the year's feedings. The dragon's dial shows
    // WHY an eclipse can happen; the wheel is the only surface in the
    // instrument that already means "a year of dates", so it is where
    // WHEN belongs — the whole season's worth at a glance instead of
    // scrubbed for one at a time. Marks ride this weight, so they
    // belong to the station and do not litter the other six.
    float   draco;
    const EclipseTable *ecl;   // covers the wheel's year plus a lookahead
    double  ecl_tz;            // hours to add to a UT instant for the wheel
    EclipseSeason seasons[ECL_SEASON_MAX];
    int     num_seasons;
    double  seasons_epoch;     // the wheel year they were computed for

    // THE SKY CIRCLE — one shape, one drawer, every station. Seat
    // and radius blend between CAELVM's bowl (centered, growing off
    // the bezel) and the astrolabe's horizon circle; the plate's
    // limb clips it; colors are the astro view's minute-cached
    // atmosphere. alpha = the chart family's combined weight.
    float   sky_a, sky_wc;
    double  skyb_l, astb_l;   // raw blends (bezel growth, clip)
    const AstroViewState *astv;
    const SkyViewState   *skyv2;  // the LIVE look (pitch deforms)

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
    st->draco = 0.0f;
    st->ecl = NULL;
    st->ecl_tz = 0.0;
    st->num_seasons = 0;
    st->seasons_epoch = -1.0e9;
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
    st->skyv2 = &sc->sky_state;
    st->skyb_l = sc->sky_blend;
    st->astb_l = sc->astro_blend;
    {
        double fam = sc->sky_blend + sc->astro_blend;
        st->sky_a = (float)(fam > 1.0 ? 1.0 : fam);
        st->sky_wc = fam > 1.0e-6
                   ? (float)(sc->sky_blend / fam) : 0.0f;
    }
    st->horo = (float)sc->stw[ST_HOROLOGIVM];

    // The year's eclipses. The wheel counts in LOCAL noon-based JD
    // (jd_current = cal_jd_noon) while the ephemeris answers in UT, so
    // the whole window shifts by the zone and every instant shifts
    // back — otherwise a mark can sit a day off its tick at the far
    // zones. The scan runs a year PAST the wheel's end so the "next
    // feeding" reading still has an answer on the last day of the year.
    st->draco = (float)sc->stw[ST_DRACO];
    st->ecl_tz = t->config.timezone / 24.0;
    if (st->draco > 0.0005f) {
        double w0 = t->jd_newyear - st->ecl_tz;
        st->ecl = eclipse_table(w0, w0 + t->total_days + 400.0);
        if (fabs(st->seasons_epoch - w0) > 0.5) {
            st->num_seasons = eclipse_seasons(w0 - ECL_SEASON_HALF,
                                              w0 + t->total_days
                                                 + ECL_SEASON_HALF,
                                              st->seasons, ECL_SEASON_MAX);
            st->seasons_epoch = w0;
        }
    }
    {
        double a = 0, r = 0, w = 0;
        for (int i = 0; i < ST_COUNT; i++) {
            if (station_table[i].hour_r <= 0) continue;
            double sw = sc->stw[i];
            // THE CHART STATIONS PARK THE MACHINE, so MACHINA keeps
            // weight in the normalised vector and halves theirs: at
            // full CAELVM hr_a came out at exactly 0.500, which drew
            // the ring at half ink AND failed the drag test's
            // hr_a > 0.5 by a hair — dim and dead (Seren). Their own
            // blend is the honest measure of how far up they are; the
            // parked machine is not a station you are partly at.
            if (i == ST_CAELVM)        sw = sc->sky_blend;
            else if (i == ST_ASTROLAB) sw = sc->astro_blend;
            // Where the wheel has two states, the hour ring belongs to
            // the ZOOMED-OUT one (Seren): zoomed out the band clicks
            // whole days and the ring carries the hour; zoomed in the
            // band already scrubs true time and the ring would only
            // say the same thing twice. It fades on the zoom's own
            // tween, so the two states hand the hour back and forth.
            if (station_table[i].band_zoom) sw *= (1.0 - st->zoom);
            if (sw <= 0.0) continue;
            a += sw;
            r += sw * station_table[i].hour_r;
            w += sw * station_table[i].hour_w;
        }
        st->hr_a = (float)(a > 1.0 ? 1.0 : a);
        st->hr_r = a > 1.0e-6 ? (float)(r / a) : 0.0f;
        st->hr_w = a > 1.0e-6 ? (float)(w / a) : 0.0f;
    }
    if (st->cached_day != st->tv.day || st->cached_year != st->tv.year)
        cal__rebuild_ticks(st, t, &sc->style);
}

// ---- The year's feedings, marked on the band ----
//
// DRACO's dial answers "why"; the wheel answers "when". Every eclipse
// of the wheel's year gets a mark on the date it falls, in the free
// band between the tick ring and the teal month block. They cluster in
// pairs a fortnight apart — that clustering IS the eclipse season, and
// scrubbing the years walks the pairs backwards around the wheel as
// the nodes regress. Nothing else in the instrument shows that.
//
// A SOLAR mark is the thing itself: a dark disc with a bright ring of
// light escaping around it. A LVNAR mark is the copper of a moon in
// Earth's umbra — the same blood the dial uses when it feeds. Size and
// ink ride `depth`, so a central eclipse marks harder than a graze,
// and the merely-possible ones (see eclipse.h — near the limit our
// ephemeris cannot be sure) draw faint and hollow rather than
// asserting themselves.
static void cal__eclipse_marks(const CalendarViewState *st, DrawCtx *d,
                               const Tempus *t, float radius) {
    if (st->draco <= 0.0005f || !st->ecl) return;
    float k = st->draco;
    // The table speaks UT; the wheel speaks local. Compare in the
    // table's frame, not the wheel's — mixing them puts "is this one
    // still ahead of us" out by the zone on the day itself.
    double now_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                  - st->ecl_tz;
    int next_i = eclipse_next(st->ecl, now_ut);
    float mark_r = radius + 34.0f;      // clear of ticks (+20) and arc (+51)

    // The seasons first, UNDER the marks: the five-week windows where
    // the sun stands near enough a node for a shadow to land. They are
    // the reason the marks come in pairs, and scrubbing the years
    // walks them backwards round the wheel as the nodes regress.
    for (int i = 0; i < st->num_seasons; i++) {
        double c = st->seasons[i].mid + st->ecl_tz;
        double p0 = tempus_jd_to_wheel_pct(t, c - ECL_SEASON_HALF);
        double p1 = tempus_jd_to_wheel_pct(t, c + ECL_SEASON_HALF);
        if (p1 <= 0.0 || p0 >= 1.0) continue;
        if (p0 < 0.0) p0 = 0.0;
        if (p1 > 1.0) p1 = 1.0;
        float a0 = (float)(p0 * 2.0 * M_PI - M_PI * 0.5);
        float a1 = (float)(p1 * 2.0 * M_PI - M_PI * 0.5);
        // The head's season in the dragon's own warm ink, the tail's
        // a shade cooler — the dial names which jaw is in play, and
        // the wheel should not contradict it
        bool head = st->seasons[i].head;
        DrawColor ink = dca(head ? 0.42f : 0.34f,
                            head ? 0.33f : 0.31f,
                            head ? 0.22f : 0.28f, k * 0.30f);

        // TAPERED, thickest at the crossing and thinning to nothing at
        // the limits. This is not decoration: the sun's distance from
        // the node is what decides whether a shadow lands at all, so
        // the band is genuinely deepest in the middle and marginal at
        // its ends. Drawn as a fan of slices rather than one arc so
        // the width can vary along it. (Seren reads the two bands as
        // dragons on the wheel — which is exactly what they are: the
        // head's season and the tail's, the same two jaws the dial
        // names, so a body that tapers is the honest shape.)
        {
            const int SL = 32;
            for (int sgi = 0; sgi < SL; sgi++) {
                float t0 = (float)sgi / SL, t1 = (float)(sgi + 1) / SL;
                float b0 = a0 + (a1 - a0) * t0;
                float b1 = a0 + (a1 - a0) * t1;
                // Half-width follows a cosine hump across the window
                float m0 = cosf(((float)t0 - 0.5f) * (float)M_PI);
                float m1 = cosf(((float)t1 - 0.5f) * (float)M_PI);
                float w0 = 3.0f + 12.0f * m0, w1 = 3.0f + 12.0f * m1;
                draw_set_color(d, ink);
                int vb = d->num_verts;
                float c0 = cosf(b0), s0 = sinf(b0);
                float c1 = cosf(b1), s1 = sinf(b1);
                draw__push_vert(d, c0 * (mark_r - w0), s0 * (mark_r - w0),
                                d->white_u, d->white_v);
                draw__push_vert(d, c0 * (mark_r + w0), s0 * (mark_r + w0),
                                d->white_u, d->white_v);
                draw__push_vert(d, c1 * (mark_r + w1), s1 * (mark_r + w1),
                                d->white_u, d->white_v);
                draw__push_vert(d, c1 * (mark_r - w1), s1 * (mark_r - w1),
                                d->white_u, d->white_v);
                draw__tri(d, vb, vb + 1, vb + 2);
                draw__tri(d, vb, vb + 2, vb + 3);
            }
        }

        // The jaw itself, set at the middle of its own season — which
        // is the instant the sun actually arrives at that crossing,
        // not a label placed for looks. CAPVT on the ascending node,
        // CAVDA on the descending, the same figures DRACO's dial wears.
        {
            double pm = tempus_jd_to_wheel_pct(t, c);
            if (pm >= 0.0 && pm < 1.0) {
                float gx, gy;
                cal__fc(pm, mark_r, &gx, &gy);
                float ux = gx / mark_r, uy = gy / mark_r;   // glyph-up
                draw_set_color(d, dca(0.84f, 0.80f, 0.72f, k * 0.72f));
                sigil_strokes(d, head ? sigil_caput : sigil_cauda,
                              gx, gy, ux, uy, 34.0f, 1.3f);
            }
        }
    }

    for (int i = 0; i < st->ecl->n; i++) {
        const Eclipse *e = &st->ecl->e[i];
        double wjd = e->jd + st->ecl_tz;             // UT -> the wheel's clock
        double pct = tempus_jd_to_wheel_pct(t, wjd);
        if (pct < 0.0 || pct >= 1.0) continue;       // a lookahead year's

        float mx, my;
        cal__fc(pct, mark_r, &mx, &my);
        float sure = e->certain ? 1.0f : 0.50f;
        float a = k * sure * (0.62f + 0.38f * e->depth);
        float rr = 9.0f + 5.0f * e->depth;

        // The stem: without it the eye cannot tell which tick the mark
        // belongs to, and the whole point is the date
        float ix, iy, ox, oy;
        cal__fc(pct, radius + 20.0f, &ix, &iy);
        cal__fc(pct, mark_r - rr - 2.0f, &ox, &oy);
        draw_set_color(d, dca(0.62f, 0.58f, 0.50f, a * 0.55f));
        draw_line_thin(d, ix, iy, ox, oy);

        if (e->solar) {
            // The ring of light around the black disc
            draw_set_color(d, dca(1.00f, 0.82f, 0.42f, a * 0.85f));
            draw_circle_filled(d, mx, my, rr);
            draw_set_color(d, dca(0.04f, 0.035f, 0.03f, a));
            draw_circle_filled(d, mx, my, rr * 0.58f);
            draw_set_color(d, dca(1.00f, 0.90f, 0.60f, a * 0.30f));
            draw_circle_stroked(d, mx, my, rr + 2.5f, 1.0f);
        } else {
            // The umbral copper, refracted sunset light
            draw_set_color(d, dca(0.72f, 0.26f, 0.13f, a * 0.90f));
            draw_circle_filled(d, mx, my, rr * 0.86f);
            draw_set_color(d, dca(0.90f, 0.45f, 0.28f, a * 0.35f));
            draw_circle_stroked(d, mx, my, rr + 1.0f, 1.0f);
        }

        // THE NEXT ONE wears a halo. Seren's ask was partly "when is
        // the next" — on a wheel of a dozen marks that has to be
        // answerable without counting round from the pointer.
        if (i == next_i) {
            draw_set_color(d, dca(0.95f, 0.93f, 0.86f, k * 0.55f));
            draw_circle_stroked(d, mx, my, rr + 7.0f, 1.2f);
        }
    }
}

static void cal__hour_ring(const CalendarViewState *st, DrawCtx *d,
                           const RenderStyle *s);
static void cal__sky_circle(const CalendarViewState *st, DrawCtx *d,
                            const Tempus *t, const RenderStyle *s);

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

    // Event names. ONE curve decides both presence and ink — there
    // used to be two that disagreed, and the ink's ran 1000..1500 of
    // radius, which is zoom 0.076..0.146: under a tenth of the tween,
    // some 70ms, so the names snapped off instead of fading (Seren).
    // Widened to a quarter of the zoom's travel, settled before the
    // date numerals begin their own reveal at 2200.
    {
        float event_vis = (float)tempus_smoothstep(700, 2600, radius);
        if (event_vis > 0.004f) {
            float ink = event_vis * 0.7f;
            draw_set_color(d, dc(ink, ink, ink));
            // Tracking CLOSES as the wheel shrinks: the letterforms
            // keep their size while the arc they sit on gets shorter,
            // so a fixed em spread swallows more and more of the
            // circle on the way out. Same shape as the month names'.
            float ev_track = (float)tempus_mix(0.4, 1.8, blend);
            for (int i = 0; i < 8; i++) {
                float ef = (float)tempus_jd_to_wheel_pct(t, t->jd_events[i]);
                float angle = ef * 2.0f * (float)M_PI;
                draw_text_curved(d, FONT_event, 0, 0, radius - 58.0f,
                               angle, tempus_event_name(t, i),
                               ev_track, 1.0f);
            }
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
            // MONTH BOUNDARIES ARE ORDINARY DAYS (Seren): identical
            // ink, opacity, width and length — the tick band no longer
            // marks them at all. The teal block above already names
            // the month and shows its extent, so a second mark in the
            // band was restating it. Colour alone was not enough: the
            // 4-wide BAR kept them legible, so the bar goes too. (This
            // departs from the original tempus, where month boundaries
            // are 4-wide bars in month_color.)
            case TICK_MONTH_BOUNDARY:
                draw_set_color(d, s->day_marks);
                bar = false;
                break;
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

    // The dragon's feedings, on the dates they fall
    cal__eclipse_marks(st, d, t, radius);

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

    // The month name — ONE name only, the current month's, riding the
    // pointer. The other eleven are gone: they read as a dim smear at
    // the rim and the pointer already says which month is meant.
    {
        float text_r = radius + (float)tempus_mix(s->month_text_radius_a, s->month_text_radius_b, blend);
        // additive tracking (em): letterspacing widens as the ring zooms
        float text_mix = (float)tempus_mix(0.6, 2.4, blend);

        float stx = d->tx, sty = d->ty;
        draw_translate(d, -offx, -offy);

        draw_set_color(d, s->month_text_color);
        const char *mname = tempus_month_name(t, cur_month);
        double name_jd = tv->jd_current + tv->percent_of_day;
        // No clamp to the band's ends: the name simply rides the day
        // mark, so a month boundary changes the LETTERING and nothing
        // else — the label never jumps position
        float angle = (float)tempus_jd_to_wheel_pct(t, name_jd)
                    * 2.0f * (float)M_PI;

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
                       angle, mname, text_mix, 1.0f);
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

        // THE DAY, in numerals, at the pointer (Seren). Zoomed out the
        // wheel names no days at all — the per-day numerals only fade
        // in from radius 2200 — so the mark says WHICH day it is and
        // the month name outside it says which month. Set ON THE BAND,
        // baseline following the circle, just inside the tick ring; it
        // fades exactly as the per-day numerals arrive, so the two
        // never both name the same date.
        {
            // THE CLOCK'S ALONE (Seren): every other station has its
            // own reading of the date, so the numeral answers to
            // HOROLOGIVM's weight as well as the zoom.
            float dnum = (1.0f - (float)tempus_smoothstep(800, 2200, radius))
                       * st->horo;
            if (dnum > 0.004f) {
                char db[4];
                int dd = tv->day;
                if (dd >= 10) { db[0] = (char)('0' + dd / 10);
                                db[1] = (char)('0' + dd % 10); db[2] = 0; }
                else          { db[0] = (char)('0' + dd); db[1] = 0; }
                DrawColor dc2 = s->month_text_color;
                dc2.a = dnum;
                draw_set_color(d, dc2);
                // Flip compensation, as the per-day numerals use: the
                // two branches put glyphs on opposite sides of the
                // baseline circle, so the radius has to answer or the
                // numerals ride at different depths top and bottom.
                float na = fmodf(angle, 2.0f * (float)M_PI);
                if (na < 0) na += 2.0f * (float)M_PI;
                bool nflip = (na > (float)M_PI * 0.5f
                              && na < (float)M_PI * 1.5f);
                const float dscale = 2.25f;   // 3x the first pass
                // 6 off the tick ring's inner edge: the numerals sit
                // right under the band they mark, not adrift inside it.
                // The FLIP SHIFT comes from the font's own metrics, not
                // a guessed fraction — Seren caught the numeral
                // stepping 30 units inward between Sept 20 and Sept 21,
                // which is exactly the 9 o'clock flip boundary.
                float dsh = draw_text_curved_flip_shift(FONT_date, db,
                                                        dscale);
                // Shared base, so both flip cases move together; dsh
                // alone is the difference between them.
                const float dbase = radius + 1.0f;
                float dr = nflip ? dbase : (dbase - dsh);
                draw_text_curved(d, FONT_date, -offx, -offy, dr,
                                 angle, db, 0.10f, dscale);
            }
        }
    }

    d->sx = save_sx;
    d->sy = save_sy;

    cal__hour_ring(st, d, s);
}

// The SHARED SKY WASH is not instrument furniture — it is the chart's
// own ground, and every planet, star name and ecliptic the sky draws
// must land ON it. So it renders in its own pass (VIEW_CALBACK) at the
// BOTTOM of the stack, while the wheel above it rides at the top.
// Drawn here only because this is where the two chart stations' shared
// geometry is reachable; see cal__sky_circle in view_astro.h.
static void calback_render(const void *buf, DrawCtx *d, const Tempus *t,
                           const RenderStyle *s) {
    const CalendarViewState *st = (const CalendarViewState *)buf;
    cal__sky_circle(st, d, t, s);
}

// (cal__sky_circle is DEFINED in view_astro.h's impl — the one
// place that sees BOTH stations' projections. Forward-declared
// above; the calendar calls it, the chart stations shape it.)

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
    // The ring carries its OWN GROUND — a dark plate cut through
    // whatever it rides over, the rulers' pinion law (HORAE) applied
    // here: the hour marks are faint strokes and the loupe can bring
    // the noon limb up underneath them. Over the black of every other
    // station the plate costs nothing; over the sky it reads as a
    // groove. Margin covers the now-mark's overhang (r0-3 .. r1+3).
    draw_set_color(d, dca(0.03f, 0.03f, 0.035f, 0.55f));
    draw_arc_filled(d, 0, 0, r1 + 4.0f, r0 - 4.0f,
                    0.0f, 2.0f * (float)M_PI, 96);
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
    // The rendering instant: a HOLLOW TRIANGLE in the sun's own gold —
    // the wheel's teal pointer spoken at TWO THIRDS (30x28 -> 20x18.7)
    // and riding ON the ring rather than outside it (Seren). Centered
    // on the band, so it clears the calendar wheel at every station
    // that declares an hour ring, whatever radius the ring takes.
    // Apex inward, flat base outward, three strokes: the same grammar
    // as the day pointer, so the two marks read as one family.
    float an = (float)st->tv.percent_of_day * 2.0f * (float)M_PI;
    float cs = sinf(an), sn = -cosf(an);   // radial out
    float tx = -sn, ty = cs;               // tangent
    const float TRI_H = 28.0f * 2.0f / 3.0f;   // 18.67
    const float TRI_W = 30.0f * 2.0f / 3.0f;   // 20 across
    float mid = (r0 + r1) * 0.5f;
    float rin = mid - TRI_H * 0.5f, rout = mid + TRI_H * 0.5f;
    draw_set_color(d, dc_u8(196, 126, 16));   // the instrument's gold
    float ax = cs * rin, ay = sn * rin;
    float b1x = cs * rout + tx * TRI_W * 0.5f;
    float b1y = sn * rout + ty * TRI_W * 0.5f;
    float b2x = cs * rout - tx * TRI_W * 0.5f;
    float b2y = sn * rout - ty * TRI_W * 0.5f;
    draw_line(d, ax, ay, b1x, b1y, 1.8f);
    draw_line(d, b1x, b1y, b2x, b2y, 1.8f);
    draw_line(d, b2x, b2y, ax, ay, 1.8f);
    d->alpha = base_alpha;
}

static const ViewVtable calendar_vtable = {
    .init   = calendar_init,
    .enter  = calendar_enter,
    .exit   = calendar_exit,
    .update = calendar_update,
    .render = calendar_render,
};

// The under-layer: render only (state, lifecycle and update belong to
// VIEW_CALENDAR; this shares its state buffer)
static const ViewVtable calback_vtable = {
    .render = calback_render,
};

#endif // SCENE_DEFINED && !VIEW_CALENDAR_IMPL
