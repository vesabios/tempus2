// view_sky.h — CAELVM: the local sky, seen from where you stand.
//
// The instrument's only first-person station. An all-sky chart: zenith
// at center, horizon at the rim, north at top — and, because you are
// looking UP at this circle rather than down at a map, east on the
// LEFT (the planisphere convention). Every body the ephemeris knows is
// placed at its true azimuth/altitude with its magnitude for a size,
// and each draws its diurnal arc across the bowl: where it rose, where
// it will culminate, where it will set. The background breathes with
// the sun's own altitude — day, the twilights, night.
//
// The other stations explain the sky; this one is the sky.

#ifndef VIEW_SKY_H
#define VIEW_SKY_H

#include "../view.h"
#include "../planets.h"

#define SKY_R          560.0f   // horizon rim, world units
#define SKY_PATH_N     49       // diurnal path samples (30-min steps)
#define SKY_ECL_N      90       // ecliptic samples (4-deg steps)

struct SkyViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;             // mirrored scene sky_blend

    PlanetsNow now;
    PlanetsSky sky;
    double     cache_jd;

    // Sunrise/sunset hours (config-tz clock), mirrored from the solar
    // view
    float rise_hr, set_hr;
    double orbw;      // mirrored scene orbis_wheel (bezel radius)

    // The hour ring (between the chart and the calendar bezel) is the
    // rendering-time control: drag it and the bodies wheel along
    // their arcs
    bool  hour_dragging;
    float last_wx, last_wy;

    // The LOOK: the chart's projection center. Zenith by default;
    // drag the chart to pitch toward any horizon and look around.
    // view_alt clamps at 10 degrees — you may graze the ground with
    // your gaze but not bury it.
    float view_az, view_alt;
    bool  chart_dragging;

    // The dome's vertex colors, computed by true single-scattering in
    // update (minute-gated): zenith vertex + rings x sectors
    float dome[1 + 9 * 97][3];

    // Luminary chart targets, published for the orrery's composition:
    // the sun and moon are SINGLE objects — the orrery composes their
    // parameters across every station (this chart included, using
    // these targets) and VIEW_LVMEN draws them once, above everything.
    float lum_sun_x, lum_sun_y, lum_sun_r;
    float lum_moon_x, lum_moon_y, lum_moon_r;
    float lum_moon_light[3];

    // The orrery's state: its published live sun/moon positions are the
    // machine-side endpoints of the morph, read at render time — every
    // flight starts from where the bodies actually are, whatever
    // station the machine is at or between
    const OrreryViewState *orr;

    // Stage 3: published chart members for the planets (BODY_*
    // index; sun/moon slots unused — they are VIEW_LVMEN's already).
    // The orrery composes member rows from these; LVMEN renders.
    float pl_x[BODY_COUNT], pl_y[BODY_COUNT], pl_r[BODY_COUNT];
    float pl_ba[BODY_COUNT];        // subdued-below-horizon policy
    uint8_t pl_stroke[BODY_COUNT];  // not observable: stroked ring

    // Everything projected, refreshed when the clock minute moves
    float body_az[BODY_COUNT], body_alt[BODY_COUNT];
    float path[BODY_COUNT][SKY_PATH_N][2];   // az, alt over +/-12h
    float ecl_az[SKY_ECL_N], ecl_alt[SKY_ECL_N];
    float sign_az[12], sign_alt[12];         // cusp positions (0 Aries...)
};

#endif // VIEW_SKY_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_SKY_IMPL)
#define VIEW_SKY_IMPL

// Project (az, alt) onto the chart. Equidistant azimuthal pushed to
// the ANTIPODE (the "Welt um Nauen" projection, inverted for the sky):
// zenith at center, the horizon a circle at HALF radius, the unseen
// sky filling the outer annulus out to the nadir stretched into the
// rim. Nothing is ever off the chart — setting is just crossing the
// horizon circle. North top; east LEFT (the view looking up).
// The projection center — file-scope so the pure projection helpers
// keep their signatures. Set from the view state at the top of both
// update and render.
static float sky__vc[3], sky__vr[3], sky__vd[3];

static void sky__set_center(float az0_deg, float alt0_deg) {
    float a = az0_deg * (float)M_PI / 180.0f;
    float l = alt0_deg * (float)M_PI / 180.0f;
    // center; screen +y (down) = toward the az0 horizon; +x = their
    // cross — reduces to north-bottom/east-right at the zenith
    sky__vc[0] = sinf(a) * cosf(l);
    sky__vc[1] = cosf(a) * cosf(l);
    sky__vc[2] = sinf(l);
    sky__vd[0] = sinf(a) * sinf(l);
    sky__vd[1] = cosf(a) * sinf(l);
    sky__vd[2] = -cosf(l);
    sky__vr[0] = sky__vd[1] * sky__vc[2] - sky__vd[2] * sky__vc[1];
    sky__vr[1] = sky__vd[2] * sky__vc[0] - sky__vd[0] * sky__vc[2];
    sky__vr[2] = sky__vd[0] * sky__vc[1] - sky__vd[1] * sky__vc[0];
}

// Azimuthal-equidistant about the LIVE view center: angular distance
// maps to radius, bearing to screen direction. At the zenith this is
// exactly the old chart (north bottom, east right); pitched toward a
// horizon, that horizon draws near while the far sky recedes toward
// the rim — and nothing is ever off the chart.
static inline void sky__project(float az_deg, float alt_deg,
                                float *x, float *y) {
    if (alt_deg > 90.0f) alt_deg = 90.0f;
    if (alt_deg < -90.0f) alt_deg = -90.0f;
    float a = az_deg * (float)M_PI / 180.0f;
    float l = alt_deg * (float)M_PI / 180.0f;
    float v[3] = { sinf(a) * cosf(l), cosf(a) * cosf(l), sinf(l) };
    float cosc = v[0]*sky__vc[0] + v[1]*sky__vc[1] + v[2]*sky__vc[2];
    if (cosc > 1.0f) cosc = 1.0f;
    if (cosc < -1.0f) cosc = -1.0f;
    float rr = acosf(cosc) / (float)M_PI * SKY_R;
    float px = v[0]*sky__vr[0] + v[1]*sky__vr[1] + v[2]*sky__vr[2];
    float py = v[0]*sky__vd[0] + v[1]*sky__vd[1] + v[2]*sky__vd[2];
    float len = sqrtf(px * px + py * py);
    if (len < 1.0e-5f) {
        *x = 0.0f;
        *y = cosc > 0.0f ? 0.0f : SKY_R;
        return;
    }
    *x = px / len * rr;
    *y = py / len * rr;
}

// Pan the look: PITCH ONLY — the azimuth is fixed, so the drag's
// vertical component tips the view between the zenith and the faced
// horizon and nothing else moves. Drag down = look up (the content
// follows the finger). With the yaw frozen there is no pole to
// mishandle: alt runs the full clamp to 90.
static inline void sky_view_pan(SkyViewState *st, float dx, float dy) {
    (void)dx;
    float alt = st->view_alt + dy / SKY_R * 180.0f;
    if (alt > 90.0f) alt = 90.0f;
    if (alt < 5.0f) alt = 5.0f;     // the pitch clamp: no digging
    st->view_alt = alt;
}

#define SKY_HOR (SKY_R * 0.5f)   // the horizon circle
#define SKY_WIN_N 360             // visible-window samples

// Frame vector (E, N, U) of an az/alt direction — az from north,
// eastward, matching planets_sky_azalt
static inline void sky__vec(float az_deg, float alt_deg, float v[3]) {
    float a = az_deg * (float)M_PI / 180.0f;
    float l = alt_deg * (float)M_PI / 180.0f;
    v[0] = sinf(a) * cosf(l);
    v[1] = cosf(a) * cosf(l);
    v[2] = sinf(l);
}

// Is this az/alt visible at some point of the night — above the
// horizon at midnight, or inside either of the dusk/dawn-side
// windows? z0/z1 are those windows' zenith vectors.
static inline bool sky__in_night(float az_deg, float alt_deg,
                                 const float z0[3], const float z1[3]) {
    float v[3];
    sky__vec(az_deg, alt_deg, v);
    if (v[2] > 0.0f) return true;
    if (v[0] * z0[0] + v[1] * z0[1] + v[2] * z0[2] > 0.0f) return true;
    return v[0] * z1[0] + v[1] * z1[1] + v[2] * z1[2] > 0.0f;
}

static inline void sky__vec_project(const float v[3], float *x, float *y) {
    float u = v[2];
    if (u > 1.0f) u = 1.0f;
    if (u < -1.0f) u = -1.0f;
    float az = atan2f(v[0], v[1]) * 180.0f / (float)M_PI;
    float alt = asinf(u) * 180.0f / (float)M_PI;
    sky__project(az, alt, x, y);
}

static const uint8_t sky__body_col[BODY_COUNT][3] = {
    { 196, 126,  16 },   // Sun — the instrument's gold, same object
    { 215, 212, 202 },   // Moon
    { 152, 146, 138 }, { 208, 183, 130 }, { 188,  92,  58 },
    { 196, 168, 126 }, { 205, 190, 146 }, { 126, 172, 178 },
    {  98, 128, 188 }, { 138, 128, 132 },
};

static const char *sky__body_name[BODY_COUNT] = {
    0, 0,   // the sun and moon need no caption
    "MERCURY", "VENUS", "MARS", "JUPITER", "SATURN",
    "URANUS", "NEPTUNE", "PLUTO",
};

// Dome sampling grid (shared by the update-side scattering pass and
// the render-side mesh)
#define SKY_DOME_SEC 96
#define SKY_DOME_RINGS 9
static const float sky__dome_alts[SKY_DOME_RINGS] = {
    75.0f, 60.0f, 47.0f, 35.0f, 24.0f, 15.0f, 8.0f, 3.0f, 0.0f };

static void sky_init(void *buf, const Tempus *t, const RenderStyle *s) {
    SkyViewState *st = (SkyViewState *)buf;
    st->opacity = 1.0;
    st->cache_jd = -1.0e9;
    st->view_az = 0.0f;
    st->view_alt = 90.0f;
    (void)t; (void)s;
}

static void sky_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void sky_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

// Luminary chart targets for VIEW_LVMEN: positions on this chart
// through the LIVE look, one shared legible size (the sun and moon
// subtend the same half degree of real sky), and the moon's phase
// light aimed at the sun's chart position. Requires body_az/body_alt
// already filled — call only after the ephemeris cache is valid.
static void sky__lum_targets(SkyViewState *st) {
    float sx, sy, mx2, my2;
    sky__project(st->body_az[BODY_SUN], st->body_alt[BODY_SUN],
                 &sx, &sy);
    sky__project(st->body_az[BODY_MOON], st->body_alt[BODY_MOON],
                 &mx2, &my2);
    st->lum_sun_x = sx;
    st->lum_sun_y = sy;
    st->lum_sun_r = 14.0f;
    st->lum_moon_x = mx2;
    st->lum_moon_y = my2;
    st->lum_moon_r = 14.0f;
    double phb = globe_moon_phase(st->cache_jd);
    float bb = (float)(phb * 2.0 * M_PI);
    float ux = sx - mx2, uy = sy - my2;
    float un = sqrtf(ux * ux + uy * uy);
    if (un > 1.0e-4f) { ux /= un; uy /= un; }
    st->lum_moon_light[0] = ux * sinf(bb);
    st->lum_moon_light[1] = uy * sinf(bb);
    st->lum_moon_light[2] = -cosf(bb);

    // The planets' chart members, published on the same terms
    for (int b = BODY_MERCURY; b < BODY_COUNT; b++) {
        float bx, by;
        sky__project(st->body_az[b], st->body_alt[b], &bx, &by);
        st->pl_x[b] = bx;
        st->pl_y[b] = by;
        st->pl_r[b] = orr__pip_r(st->now.mag[b]);
        st->pl_ba[b] = st->body_alt[b] > 0.0f ? 1.0f : 0.78f;
        st->pl_stroke[b] = !st->sky.observable[b];
    }
}

static void sky_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    SkyViewState *st = (SkyViewState *)buf;
    st->orr = &sc->orrery_state;
    (void)dt;
    st->blend = sc->sky_blend;
    st->orbw = sc->orbis_wheel;
    if (st->blend < 0.001) return;
    sky__set_center(st->view_az, st->view_alt);
    st->rise_hr = (float)sc->solar_state.sunrise_hr;
    st->set_hr = (float)sc->solar_state.sunset_hr;

    // CAELVM shows the sky AT THE RENDERING INSTANT: the horizon is
    // the fixed circle, and the hour ring scrubs the display time —
    // bodies wheel along their diurnal arcs across the chart, and the
    // bowl wears the sky's own color for that instant.
    double jd_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                 - t->config.timezone / 24.0;
    if (fabs(jd_ut - st->cache_jd) <= 1.0 / 1440.0) {
        // Ephemeris cache holds — but the LOOK moves between minute
        // ticks, so the luminary chart targets reproject every update
        sky__lum_targets(st);
        return;
    }
    st->cache_jd = jd_ut;

    double lat = t->config.latitude, lon = t->config.longitude;
    planets_compute(&st->now, jd_ut);
    planets_sky_compute(&st->sky, &st->now, lat, lon);

    for (int b = 0; b < BODY_COUNT; b++) {
        double blon, blat, az, alt;
        planets__body_lonlat(b, jd_ut, &blon, &blat);
        planets_sky_azalt(blon, blat, jd_ut, lat, lon, &az, &alt);
        st->body_az[b] = (float)az;
        st->body_alt[b] = (float)alt;

        // Diurnal arc: the body's whole day across the bowl, centered
        // on the true instant — a day-track mostly re-traces itself,
        // so the arc glides with the scrub instead of lurching at the
        // date tick, always passing through the body
        for (int i = 0; i < SKY_PATH_N; i++) {
            double jd = jd_ut + (i - SKY_PATH_N / 2) * 0.5 / 24.0;
            planets__body_lonlat(b, jd, &blon, &blat);
            planets_sky_azalt(blon, blat, jd, lat, lon, &az, &alt);
            st->path[b][i][0] = (float)az;
            st->path[b][i][1] = (float)alt;
        }
    }

    // The dome: true scattering per vertex, cached on the minute.
    // Night floor = the instrument's night tint, so the chart never
    // goes pit-black under the stars.
    {
        float sd[3];
        atmo_dir(st->body_az[BODY_SUN], st->body_alt[BODY_SUN],
                      sd);
        for (int vi2 = 0;
             vi2 < 1 + SKY_DOME_RINGS * (SKY_DOME_SEC + 1); vi2++) {
            float az, altv;
            if (vi2 == 0) {
                az = 0.0f;
                altv = 90.0f;
            } else {
                int ri = (vi2 - 1) / (SKY_DOME_SEC + 1);
                int si = (vi2 - 1) % (SKY_DOME_SEC + 1);
                az = (float)si / SKY_DOME_SEC * 360.0f;
                altv = sky__dome_alts[ri];
            }
            float rd[3], col[3];
            static const float dome_base[3] = { 0.020f, 0.022f,
                                                0.035f };
            atmo_dir(az, altv, rd);
            atmo_scatter(rd, sd, col);
            atmo_tone(col, 0.95f, dome_base, st->dome[vi2]);
        }
    }

    // The ecliptic's current lie across the sky + the sign cusps
    for (int i = 0; i < SKY_ECL_N; i++) {
        double az, alt;
        planets_sky_azalt(i * 360.0 / SKY_ECL_N, 0.0, jd_ut, lat, lon,
                          &az, &alt);
        st->ecl_az[i] = (float)az;
        st->ecl_alt[i] = (float)alt;
    }
    for (int i = 0; i < 12; i++) {
        double az, alt;
        planets_sky_azalt(i * 30.0, 0.0, jd_ut, lat, lon, &az, &alt);
        st->sign_az[i] = (float)az;
        st->sign_alt[i] = (float)alt;
    }

    // Targets from the fresh ephemeris — always AFTER the fill, so
    // the very first update never publishes zeroed az/alt (the sun
    // and moon were landing pinned to the north horizon)
    sky__lum_targets(st);
}

// Alias kept for the morph call sites: the full-sphere projection
// already accepts any altitude
static inline void sky__project_clamped(float az, float alt,
                                        float *x, float *y) {
    sky__project(az, alt, x, y);
}

static void sky_render(const void *buf, DrawCtx *d, const Tempus *t,
                       const RenderStyle *s) {
    const SkyViewState *st = (const SkyViewState *)buf;
    float base_alpha = d->alpha;

    // The morph from MACHINA MVNDI: every sky element interpolates from
    // its counterpart on the machine — beads from their orbit rings to
    // their true az/alt, orbit rings unfurling into diurnal arcs, the
    // zodiac dial bending into the ecliptic's lie across the bowl, and
    // the calendar wheel (Earth's orbit) growing into the horizon rim:
    // the year becomes the ground. Chart furniture without a machine
    // counterpart fades in late.
    float mb = (float)st->blend;               // morph position
    float fb = ink_in(INK_CHART_LATE, st->blend);
    sky__set_center(st->view_az, st->view_alt);
    // Machine-counterpart weight: only MACHINA has zodiac/rings/beads
    // to hand off. Parked there (system stage 1) every element takes
    // the full ownership flight from its machine slot; parked anywhere
    // else there is NOTHING to fly from, so the sky's planets and
    // chart lines are born AT their sky positions and simply fade in
    // place (and fade out the same way leaving). mw scales the
    // machine-side position/alpha, sw is the sky-side alpha ramp.
    float ms = st->orr ? (float)st->orr->sys : 1.0f;
    if (ms < 0) ms = 0;
    if (ms > 1) ms = 1;
    float fin = ink_in(INK_BORN, st->blend);
    float mw = ms * (1.0f - mb);
    float sw = ms * mb + (1.0f - ms) * fin;
    float wheel_R = s->calendar_base_radius
                  * (float)tempus_wheel_scale(1.0);   // Earth-orbit const
                                                      // (machine endpoints)
    // The BEZEL is the visible calendar wheel — the horizon circle
    // unfurls from wherever it actually is
    float bez_R = (float)tempus_wheel_radius(
        s->calendar_base_radius, st->orr ? st->orr->sys : 1.0,
        st->blend, st->orbw);

    // ---- The bowl: the true sky at the rendering instant ----
    // Single-scattering atmosphere, evaluated per dome vertex in
    // update (minute-cached): real Rayleigh blues, real Mie-forward
    // sunset fire climbing from wherever the sun actually stands.
    {
        float R_live = bez_R + (SKY_HOR - bez_R) * mb;
        d->alpha = base_alpha * mb;
        // Under the earth: the whole chart, a dark warm ground
        draw_set_color(d, dca(0.055f, 0.038f, 0.030f, 1.0f));
        draw_circle_filled(d, 0, 0, SKY_R);

        enum { SEC = SKY_DOME_SEC };
        float mk = R_live / SKY_HOR;   // morph scale about center
        int prev[SEC + 1], curv[SEC + 1];
        draw_set_color(d, dca(st->dome[0][0], st->dome[0][1],
                              st->dome[0][2], 1.0f));
        float zx0, zy0;
        sky__project(0.0f, 90.0f, &zx0, &zy0);
        int cvi = draw__push_vert(d, zx0 * mk, zy0 * mk,
                                  d->white_u, d->white_v);
        for (int ri = 0; ri < SKY_DOME_RINGS; ri++) {
            float alt = sky__dome_alts[ri];
            for (int si = 0; si <= SEC; si++) {
                const float *dc2 =
                    st->dome[1 + ri * (SEC + 1) + si];
                draw_set_color(d, dca(dc2[0], dc2[1], dc2[2], 1.0f));
                float az = (float)si / SEC * 360.0f;
                float vx2, vy2;
                sky__project(az, alt, &vx2, &vy2);
                int vi = draw__push_vert(d, vx2 * mk, vy2 * mk,
                                         d->white_u, d->white_v);
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

    // Altitude circles at +/-30 and +/-60 degrees + the zenith cross —
    // all sampled through the live projection so they follow the look
    if (fb > 0.001f) {
        d->alpha = base_alpha * fb;
        static const float circ_alt[4] = { 60.0f, 30.0f, -30.0f,
                                           -60.0f };
        for (int ci = 0; ci < 4; ci++) {
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f,
                                  ci < 2 ? 0.10f : 0.06f));
            float px2 = 0, py2 = 0;
            for (int i = 0; i <= 96; i++) {
                float az = (float)i / 96.0f * 360.0f;
                float cx2, cy2;
                sky__project(az, circ_alt[ci], &cx2, &cy2);
                if (i > 0)
                    draw_line(d, px2, py2, cx2, cy2, 1.0f);
                px2 = cx2; py2 = cy2;
            }
        }
        // The chart's edge: the point opposite the look, stretched
        // into the outermost rim
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.20f));
        draw_circle_stroked(d, 0, 0, SKY_R, 1.0f);
        // The zenith cross rides the look
        {
            float zx2, zy2;
            sky__project(0.0f, 90.0f, &zx2, &zy2);
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
            draw_line(d, zx2 - 6.0f, zy2, zx2 + 6.0f, zy2, 1.0f);
            draw_line(d, zx2, zy2 - 6.0f, zx2, zy2 + 6.0f, 1.0f);
        }
        d->alpha = base_alpha;
    }

    // ---- The zodiac dial bending into the ecliptic ----
    // Matched point-by-point in ecliptic longitude: the ring's lambda
    // position lerps to the sky's lambda position. Below-horizon
    // stretches fade out as they arrive — half the zodiac is always
    // under the earth, and now you can watch which half.
    {
        for (int i = 0; i < SKY_ECL_N; i++) {
            int j = (i + 1) % SKY_ECL_N;
            float l0 = i * 360.0f / SKY_ECL_N;
            float l1 = j * 360.0f / SKY_ECL_N;
            float rx0, ry0, rx1, ry1, sx0, sy0, sx1, sy1;
            orr__ecl_dir(l0, &rx0, &ry0);
            orr__ecl_dir(l1, &rx1, &ry1);
            sky__project_clamped(st->ecl_az[i], st->ecl_alt[i], &sx0, &sy0);
            sky__project_clamped(st->ecl_az[j], st->ecl_alt[j], &sx1, &sy1);
            float x0 = rx0 * ORR_WEB_R * mw + sx0 * (1 - mw);
            float y0 = ry0 * ORR_WEB_R * mw + sy0 * (1 - mw);
            float x1 = rx1 * ORR_WEB_R * mw + sx1 * (1 - mw);
            float y1 = ry1 * ORR_WEB_R * mw + sy1 * (1 - mw);
            bool vis = st->ecl_alt[i] > 0.0f && st->ecl_alt[j] > 0.0f;
            float a = 0.22f * mw + (vis ? 0.30f : 0.13f) * sw;
            draw_set_color(d, dca(0.65f, 0.52f, 0.25f, a));
            draw_line(d, x0, y0, x1, y1, 1.0f);
        }
        // Sign cusps ride the same lerp
        for (int i = 0; i < 12; i++) {
            float rx, ry, sx, sy;
            orr__ecl_dir(i * 30.0f, &rx, &ry);
            sky__project_clamped(st->sign_az[i], st->sign_alt[i], &sx, &sy);
            float x = rx * ORR_WEB_R * mw + sx * (1 - mw);
            float y = ry * ORR_WEB_R * mw + sy * (1 - mw);
            bool vis = st->sign_alt[i] > 0.0f;
            float a = 0.30f * mw + (vis ? 0.45f : 0.22f) * sw;
            draw_set_color(d, dca(0.65f, 0.52f, 0.25f, a));
            draw_line(d, x - 4.0f, y - 4.0f, x + 4.0f, y + 4.0f, 1.0f);
            draw_line(d, x - 4.0f, y + 4.0f, x + 4.0f, y - 4.0f, 1.0f);
        }
    }

    // ---- Orbit rings unfurling into diurnal arcs ----
    // Each body's ring is sampled as an arc of its orbit centered on
    // where the body is now; sample i lerps to the matching moment of
    // its day across the bowl. The year becomes the day. The sun has
    // no ring — its path unfolds out of the sun itself.
    for (int b = 0; b < BODY_COUNT; b++) {
        float pa = (b == BODY_SUN) ? 0.30f
                 : (b == BODY_MOON) ? 0.25f : 0.18f;
        float ra = (b == BODY_SUN) ? 0.0f
                 : (b == BODY_MOON) ? 0.20f : 0.13f;
        const uint8_t *c = sky__body_col[b];
        for (int i = 0; i + 1 < SKY_PATH_N; i++) {
            float mx0, my0, mx1, my1;
            for (int k = 0; k < 2; k++) {
                int ii = i + k;
                float *out_x = k ? &mx1 : &mx0;
                float *out_y = k ? &my1 : &my0;
                float sx, sy;
                sky__project_clamped(st->path[b][ii][0],
                                     st->path[b][ii][1], &sx, &sy);
                // Machine endpoint for this sample
                float rx = 0, ry = 0;
                if (b == BODY_SUN) {
                    rx = 0; ry = 0;
                } else if (b == BODY_MOON) {
                    // Its little machina orbit, sampled at the moon's
                    // real angular rate (~13.2 deg/day)
                    float gx, gy;
                    orr__ecl_dir(st->now.geo_lon[BODY_MOON]
                                 + (ii - SKY_PATH_N / 2) * 0.275f,
                                 &gx, &gy);
                    double yp = tempus_year_pct(t);
                    float edx = sinf((float)(yp * 2.0 * M_PI)) * wheel_R;
                    float edy = -cosf((float)(yp * 2.0 * M_PI)) * wheel_R;
                    rx = edx + gx * 42.0f * 1.55f;
                    ry = edy + gy * 42.0f * 1.55f;
                } else {
                    int pl = planets_body_pl(b);
                    float gx, gy;
                    // A slice of the orbit ring, centered on the body
                    orr__ecl_dir(st->now.helio_lon[pl]
                                 + (ii - SKY_PATH_N / 2) * (360.0f / SKY_PATH_N),
                                 &gx, &gy);
                    float orbr = orr__orbit_r(pl, wheel_R);
                    rx = gx * orbr;
                    ry = gy * orbr;
                }
                *out_x = rx * mw + sx * (1 - mw);
                *out_y = ry * mw + sy * (1 - mw);
            }
            bool vis = st->path[b][i][1] > 0.0f
                    && st->path[b][i + 1][1] > 0.0f;
            float a = ra * mw + (vis ? pa : pa * 0.45f) * sw;
            if (a < 0.004f) continue;
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, a));
            draw_line(d, mx0, my0, mx1, my1, 1.0f);
        }
    }

    // ---- The horizon: Earth's orbit becomes Earth's ground ----
    // Sampled through the projection: pitched toward a horizon, the
    // near rim draws close while the far side recedes.
    {
        float rim_r = bez_R + (SKY_HOR - bez_R) * mb;
        float mk = rim_r / SKY_HOR;
        d->alpha = base_alpha * ink_in(INK_HORIZON, st->blend);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.55f));
        float px2 = 0, py2 = 0;
        for (int i = 0; i <= 144; i++) {
            float az = (float)i / 144.0f * 360.0f;
            float hx2, hy2;
            sky__project(az, 0.0f, &hx2, &hy2);
            hx2 *= mk; hy2 *= mk;
            if (i > 0)
                draw_line(d, px2, py2, hx2, hy2, 1.0f);
            px2 = hx2; py2 = hy2;
        }
        d->alpha = base_alpha;
    }

    // ---- The bodies themselves ----
    // Beads leave their orbit rings and fly to where they truly hang in
    // your sky; anything below the horizon exits through the rim and
    // fades — which is what setting is.
    for (int b = BODY_COUNT - 1; b >= 0; b--) {
        // The beads are VIEW_LVMEN's now (Stage 3): every body is
        // composed by the orrery from these published chart members
        // and drawn once by the body renderer. This chart keeps only
        // its NAME labels, set at the composed (live) positions.
        if (b == BODY_SUN || b == BODY_MOON) continue;
        int pl = planets_body_pl(b);
        if (pl < 0 || !st->orr) continue;
        float x = st->orr->pl_cx[pl], y = st->orr->pl_cy[pl];
        float pr = st->orr->pl_cr[pl];
        float ba = st->orr->pl_ca[pl];
        if (sky__body_name[b] && fb > 0.01f) {
            int lw = _font_compat[FONT_date].weight;
            float lsz = 15.0f;
            float tw = sdf_measure_width(lw, sky__body_name[b]) * lsz;
            d->alpha = base_alpha * ba * fb * 0.55f;
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.85f));
            draw_text_ex(d, lw, lsz, x - tw * 0.5f, y + pr + 5.0f,
                         sky__body_name[b]);
        }
        d->alpha = base_alpha;
    }

    // ---- Compass furniture (on the fixed midnight circle) ----
    if (fb > 0.001f) {
        d->alpha = base_alpha * fb;
        int cw = _font_compat[FONT_month].weight;
        // az 0/90/180/270 = N/E/S/W — pinned to the LIVE horizon,
        // ticks pointing away from the zenith's projection
        static const char *card[4] = { "N", "E", "S", "W" };
        float zpx, zpy;
        sky__project(0.0f, 90.0f, &zpx, &zpy);
        for (int i = 0; i < 36; i++) {
            float az = (float)i * 10.0f;
            float bx, by;
            sky__project(az, 0.0f, &bx, &by);
            float dx = bx - zpx, dy = by - zpy;
            float dn = sqrtf(dx * dx + dy * dy);
            if (dn < 1.0e-3f) { dx = 0; dy = 1; dn = 1; }
            dx /= dn; dy /= dn;
            bool major = (i % 9) == 0;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f,
                                  major ? 0.6f : 0.25f));
            draw_line(d, bx, by,
                      bx + dx * (major ? 14.0f : 7.0f),
                      by + dy * (major ? 14.0f : 7.0f), 1.0f);
            if (major) {
                float sz = _font_compat[FONT_month].size;
                float tw2 = sdf_measure_width(cw, card[i / 9]) * sz;
                draw_set_color(d, dca(0.66f, 0.63f, 0.57f, 0.75f));
                draw_text_ex(d, cw, sz,
                             bx + dx * 34.0f - tw2 * 0.5f,
                             by + dy * 34.0f - sz * 0.5f,
                             card[i / 9]);
            }
        }

        // ---- The horizon calendar: the eight days as sightlines ----
        // Before brass, the year was kept exactly here: where on YOUR
        // horizon the sun rises and sets on each of the eight days —
        // Stonehenge's diagram, at your latitude. Gold sightline
        // ticks pointing inward from the horizon at each rise/set
        // bearing; the solstice pair emphasized (they bound the fan),
        // the season underway brightest.
        {
            int cur = 0;
            double bd = 1.0e9;
            for (int i = 0; i < 8; i++) {
                double dd = fabs(st->tv.jd_current - t->jd_events[i]);
                if (dd < bd) { bd = dd; cur = i; }
            }
            for (int ev = 0; ev < 8; ev++) {
                double dec = sunset__sun_declination(
                    sunset__time_julian_cent(t->jd_events[ev]));
                float azr;
                if (!tempus_rise_azimuth(dec, t->config.latitude, &azr))
                    continue;   // no crossing at this latitude
                bool sol = (ev == 3 || ev == 7);   // Litha, Yule
                float em = (ev == cur) ? 0.95f : (sol ? 0.62f : 0.38f);
                float ln = sol ? 24.0f : 15.0f;
                for (int side = 0; side < 2; side++) {
                    float az = side ? 360.0f - azr : azr;
                    float bx, by;
                    sky__project(az, 0.0f, &bx, &by);
                    float dx = bx - zpx, dy = by - zpy;
                    float dn = sqrtf(dx * dx + dy * dy);
                    if (dn < 1.0e-3f) continue;
                    dx /= dn; dy /= dn;
                    d->alpha = base_alpha * fb * em;
                    draw_set_color(d, dca(0.72f, 0.55f, 0.25f, 0.85f));
                    // The sightline straddles the horizon, reaching
                    // OUTWARD into the dark (inward it drowns in the
                    // daylit bowl) — a gnomon seen from above
                    draw_line(d, bx - dx * 4.0f, by - dy * 4.0f,
                              bx + dx * ln, by + dy * ln,
                              sol ? 1.5f : 1.0f);
                }
            }
            d->alpha = base_alpha * fb;
        }
    }

    // ---- The hour ring: the rendering-time control ----
    // A 24-hour dial between the chart and the calendar bezel,
    // midnight at the top and noon at the bottom like every day dial
    // on the instrument. Drag it and the chart's rendering instant
    // turns — the bodies wheel along their arcs while the horizon,
    // compass, and bowl hold perfectly still.
    if (fb > 0.001f) {
        d->alpha = base_alpha * fb;
        float r0 = 564.0f, r1 = 584.0f;
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.22f));
        draw_circle_stroked(d, 0, 0, r0, 1.0f);
        draw_circle_stroked(d, 0, 0, r1, 1.0f);
        for (int h = 0; h < 24; h++) {
            float a = (float)h / 24.0f * 2.0f * (float)M_PI;
            float sx = sinf(a), sy = -cosf(a);
            bool major = (h % 6) == 0;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f,
                                  major ? 0.55f : 0.28f));
            float t0 = major ? r0 : r0 + 5.0f;
            draw_line(d, sx * t0, sy * t0, sx * r1, sy * r1, 1.0f);
        }
        // The rendering instant, gold on the ring
        {
            float a = (float)st->tv.percent_of_day * 2.0f
                    * (float)M_PI;
            float sx = sinf(a), sy = -cosf(a);
            draw_set_color(d, dc_scale(s->sunrise_handle, 1.05f));
            draw_line(d, sx * (r0 - 3.0f), sy * (r0 - 3.0f),
                      sx * (r1 + 3.0f), sy * (r1 + 3.0f), 1.8f);
            draw_circle_filled(d, sx * (r0 + (r1 - r0) * 0.5f),
                               sy * (r0 + (r1 - r0) * 0.5f), 4.5f);
        }
        d->alpha = base_alpha;
    }

    d->alpha = base_alpha;
}

static const ViewVtable sky_vtable = {
    .init   = sky_init,
    .enter  = sky_enter,
    .exit   = sky_exit,
    .update = sky_update,
    .render = sky_render,
};

#endif // SCENE_DEFINED && !VIEW_SKY_IMPL
