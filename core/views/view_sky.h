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
    float      bowl_sun_alt;   // sun altitude at the DATE's solar
                               // midnight — the bowl's tint anchor

    // The orrery's state: its published live sun/moon positions are the
    // machine-side endpoints of the morph, read at render time — every
    // flight starts from where the bodies actually are, whatever
    // station the machine is at or between
    const OrreryViewState *orr;

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
static inline void sky__project(float az_deg, float alt_deg,
                                float *x, float *y) {
    if (alt_deg > 90.0f) alt_deg = 90.0f;
    if (alt_deg < -90.0f) alt_deg = -90.0f;
    float rr = (90.0f - alt_deg) / 180.0f * SKY_R;
    float a = az_deg * (float)M_PI / 180.0f;
    *x = -sinf(a) * rr;
    *y = -cosf(a) * rr;
}

#define SKY_HOR (SKY_R * 0.5f)   // the horizon circle

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

static void sky_init(void *buf, const Tempus *t, const RenderStyle *s) {
    SkyViewState *st = (SkyViewState *)buf;
    st->opacity = 1.0;
    st->cache_jd = -1.0e9;
    (void)t; (void)s;
}

static void sky_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void sky_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void sky_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    SkyViewState *st = (SkyViewState *)buf;
    st->orr = &sc->orrery_state;
    (void)dt;
    st->blend = sc->sky_blend;
    if (st->blend < 0.001) return;

    // CAELVM runs on the TRUE display instant — the wheel scrubs hours
    // and the bodies wheel across the bowl accordingly. The BOWL's
    // tint, though, is anchored to the date's local mean solar
    // midnight: the sky stays a readable instrument night, no
    // day/night strobing under a fast scrub — the sun simply crosses
    // the horizon ring as a body when its hour comes.
    double jd_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                 - t->config.timezone / 24.0;
    if (fabs(jd_ut - st->cache_jd) <= 1.0 / 1440.0) return;
    st->cache_jd = jd_ut;

    double lat = t->config.latitude, lon = t->config.longitude;
    planets_compute(&st->now, jd_ut);
    planets_sky_compute(&st->sky, &st->now, lat, lon);

    // Tint anchor: sun altitude at the date's solar midnight
    {
        double jd_mid = st->tv.jd_current + 0.5
                      - t->config.longitude / (15.0 * 24.0);
        double blon, blat, az, alt;
        planets__body_lonlat(BODY_SUN, jd_mid, &blon, &blat);
        planets_sky_azalt(blon, blat, jd_mid, lat, lon, &az, &alt);
        st->bowl_sun_alt = (float)alt;
    }

    for (int b = 0; b < BODY_COUNT; b++) {
        double blon, blat, az, alt;
        planets__body_lonlat(b, jd_ut, &blon, &blat);
        planets_sky_azalt(blon, blat, jd_ut, lat, lon, &az, &alt);
        st->body_az[b] = (float)az;
        st->body_alt[b] = (float)alt;

        // Diurnal arc: the body's whole day across the bowl
        for (int i = 0; i < SKY_PATH_N; i++) {
            double jd = jd_ut + (i - SKY_PATH_N / 2) * 0.5 / 24.0;
            planets__body_lonlat(b, jd, &blon, &blat);
            planets_sky_azalt(blon, blat, jd, lat, lon, &az, &alt);
            st->path[b][i][0] = (float)az;
            st->path[b][i][1] = (float)alt;
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
    float fb = (float)tempus_smoothstep(0.25, 0.95, st->blend);
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
    float fin = (float)tempus_smoothstep(0.10, 0.75, st->blend);
    float mw = ms * (1.0f - mb);
    float sw = ms * mb + (1.0f - ms) * fin;
    float wheel_R = s->calendar_base_radius
                  * (float)tempus_wheel_scale(1.0);   // MACHINA station

    // ---- The bowl: sky tint riding the sun's altitude ----
    // Day -> civil -> nautical -> astronomical -> night, as one smooth
    // ramp. An instrument's sky, not a photograph's: always dark enough
    // to read engravings against. Arrives as a deepening veil over the
    // dissolving machine.
    {
        float sa = st->bowl_sun_alt;
        float day = (float)tempus_smoothstep(-18.0, 8.0, sa);
        d->alpha = base_alpha * mb;
        // Under the earth: the outer annulus, a dark warm ground
        draw_set_color(d, dca(0.055f, 0.038f, 0.030f, 1.0f));
        draw_circle_filled(d, 0, 0, SKY_R);
        // The visible sky: the inner disc, breathing with the sun
        draw_set_color(d, dca(0.020f + 0.055f * day,
                              0.022f + 0.075f * day,
                              0.035f + 0.130f * day, 1.0f));
        draw_circle_filled(d, 0, 0,
                           wheel_R + (SKY_HOR - wheel_R) * mb);
        d->alpha = base_alpha;
    }

    // Altitude circles at 30 and 60 degrees + the zenith (furniture)
    if (fb > 0.001f) {
        d->alpha = base_alpha * fb;
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.10f));
        draw_circle_stroked(d, 0, 0, SKY_R * (30.0f / 180.0f), 1.0f);
        draw_circle_stroked(d, 0, 0, SKY_R * (60.0f / 180.0f), 1.0f);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.06f));
        draw_circle_stroked(d, 0, 0, SKY_R * (120.0f / 180.0f), 1.0f);
        draw_circle_stroked(d, 0, 0, SKY_R * (150.0f / 180.0f), 1.0f);
        // The nadir, stretched into the outermost rim
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.20f));
        draw_circle_stroked(d, 0, 0, SKY_R, 1.0f);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
        draw_line(d, -6.0f, 0, 6.0f, 0, 1.0f);
        draw_line(d, 0, -6.0f, 0, 6.0f, 1.0f);
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
    // The calendar wheel settles into the horizon circle at half
    // radius; the unseen sky lies beyond it, out to the nadir rim.
    {
        float rim_r = wheel_R + (SKY_HOR - wheel_R) * mb;
        d->alpha = base_alpha * (float)tempus_smoothstep(0.05, 0.5,
                                                         st->blend);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.55f));
        draw_circle_stroked(d, 0, 0, rim_r, 1.0f);
        d->alpha = base_alpha;
    }

    // ---- The bodies themselves ----
    // Beads leave their orbit rings and fly to where they truly hang in
    // your sky; anything below the horizon exits through the rim and
    // fades — which is what setting is.
    for (int b = BODY_COUNT - 1; b >= 0; b--) {   // sun and moon last
        float sx, sy;
        sky__project_clamped(st->body_az[b], st->body_alt[b], &sx, &sy);

        // Machine endpoint + size. Sun and moon come from the orrery's
        // LIVE published positions — the mover starts from wherever the
        // machine actually draws them, so direct flights from ANY
        // station (TELLVS included) are continuous without routing
        // through MACHINA first.
        float rx = 0, ry = 0, rsz;
        if (b == BODY_SUN) {
            rx = st->orr ? st->orr->bead_x : 0;
            ry = st->orr ? st->orr->bead_y : 0;
            rsz = st->orr ? st->orr->bead_r : 22.0f * 1.45f;
        } else if (b == BODY_MOON) {
            rx = st->orr ? st->orr->moon_x : 0;
            ry = st->orr ? st->orr->moon_y : 0;
            rsz = st->orr ? st->orr->moon_r : 9.9f;
        } else {
            int pl = planets_body_pl(b);
            float gx, gy;
            orr__ecl_dir(st->now.helio_lon[pl], &gx, &gy);
            float orbr = orr__orbit_r(pl, wheel_R);
            rx = gx * orbr;
            ry = gy * orbr;
            rsz = orr__planet[pl].size;
        }

        bool handoff = (b == BODY_SUN || b == BODY_MOON);
        float pw = handoff ? (1 - mb) : mw;
        float x = rx * pw + sx * (1 - pw);
        float y = ry * pw + sy * (1 - pw);
        // Below the horizon is a place on this chart, not an exit:
        // bodies in the earth annulus just read slightly subdued
        float ba = st->body_alt[b] < 0.0f ? 0.78f : 1.0f;
        if (!handoff)
            ba *= ms + (1.0f - ms) * fin;   // born in place, fading in
        float pr = rsz * pw + orr__pip_r(st->now.mag[b]) * (1 - pw);

        if (b == BODY_MOON) {
            // The moon as a phase-lit globe, its bright limb aimed at
            // the sun's place in (or under) the sky
            d->alpha = base_alpha * ba;
            GlobeCmd *gm = draw_globe_slot(d, x, y,
                rsz * pw + (orr__pip_r(st->now.mag[b]) + 12.0f) * (1 - pw));
            d->alpha = base_alpha;
            if (gm) {
                double phb = globe_moon_phase(st->cache_jd);
                float bb = (float)(phb * 2.0 * M_PI);
                float ssx, ssy;
                sky__project(st->body_az[BODY_SUN],
                             st->body_alt[BODY_SUN], &ssx, &ssy);
                float ux = ssx - x, uy = ssy - y;
                float un = sqrtf(ux * ux + uy * uy);
                if (un > 1e-4f) { ux /= un; uy /= un; }
                globe_rotation(0, 0, gm->rot);
                gm->light[0] = ux * sinf(bb);
                gm->light[1] = uy * sinf(bb);
                gm->light[2] = -cosf(bb);
                gm->land = true;
                gm->tex_id = 1;
                gm->grid_boost = 0.0f;
                gm->obs_lat = 999.0f;
                gm->day_col[0] = 0.58f; gm->day_col[1] = 0.55f;
                gm->day_col[2] = 0.49f; gm->day_col[3] = 1.0f;
                gm->night_col[0] = 0.10f; gm->night_col[1] = 0.105f;
                gm->night_col[2] = 0.23f; gm->night_col[3] = 1.0f;
            }
            continue;
        }

        d->alpha = base_alpha * ba;
        draw_set_color(d, dca(sky__body_col[b][0] / 255.0f,
                              sky__body_col[b][1] / 255.0f,
                              sky__body_col[b][2] / 255.0f, 0.95f));
        if (st->sky.observable[b] || b == BODY_SUN)
            draw_circle_filled(d, x, y, pr);
        else
            draw_circle_stroked(d, x, y, pr, 1.2f);

        // The same sun that anchored MACHINA: its corona and rays ride
        // along, dissolving as it becomes a body in the sky
        if (b == BODY_SUN && mb < 0.999f) {
            float ra2 = 1.0f - mb;
            d->alpha = base_alpha * ra2;
            draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.35f));
            draw_circle_stroked(d, x, y, pr * 28.0f / 22.0f, 1.0f);
            draw_set_color(d, dca(0.72f, 0.46f, 0.08f, 0.75f));
            for (int i = 0; i < 12; i++) {
                float a = (float)i / 12.0f * 2.0f * (float)M_PI;
                float ux = sinf(a), uy = -cosf(a);
                float qx = cosf(a), qy = sinf(a);
                float rb = pr * 1.32f;
                float rt = pr * ((i & 1) ? 1.52f : 1.78f);
                float half = pr * 0.13f;
                int vb = d->num_verts;
                draw__push_vert(d, x + ux * rt, y + uy * rt,
                                d->white_u, d->white_v);
                draw__push_vert(d, x + ux * rb - qx * half,
                                y + uy * rb - qy * half,
                                d->white_u, d->white_v);
                draw__push_vert(d, x + ux * rb + qx * half,
                                y + uy * rb + qy * half,
                                d->white_u, d->white_v);
                draw__tri(d, vb, vb + 1, vb + 2);
            }
            d->alpha = base_alpha;
        }

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

    // ---- Compass furniture (the rim itself morphs in above) ----
    if (fb > 0.001f) {
        d->alpha = base_alpha * fb;
        int cw = _font_compat[FONT_month].weight;
        // az 0/90/180/270 = N/E/S/W; the chart mirror puts E on the left
        static const char *card[4] = { "N", "E", "S", "W" };
        for (int i = 0; i < 36; i++) {
            float a = (float)i * 10.0f * (float)M_PI / 180.0f;
            // Chart mirror: az 90 (east) renders left
            float dx = -sinf(a), dy = -cosf(a);
            bool major = (i % 9) == 0;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f,
                                  major ? 0.6f : 0.25f));
            draw_line(d, dx * SKY_HOR, dy * SKY_HOR,
                      dx * (SKY_HOR + (major ? 14.0f : 7.0f)),
                      dy * (SKY_HOR + (major ? 14.0f : 7.0f)), 1.0f);
            if (major) {
                float sz = _font_compat[FONT_month].size;
                float tw2 = sdf_measure_width(cw, card[i / 9]) * sz;
                draw_set_color(d, dca(0.66f, 0.63f, 0.57f, 0.75f));
                draw_text_ex(d, cw, sz,
                             dx * (SKY_HOR + 34.0f) - tw2 * 0.5f,
                             dy * (SKY_HOR + 34.0f) - sz * 0.5f,
                             card[i / 9]);
            }
        }
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
