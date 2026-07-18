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

// Project (az, alt) onto the chart. Equidistant azimuthal: the zenith
// at center, radius linear in zenith distance. North top; east LEFT —
// the chart is the view looking up, mirrored relative to a map.
static inline void sky__project(float az_deg, float alt_deg,
                                float *x, float *y) {
    float rr = (90.0f - alt_deg) / 90.0f * SKY_R;
    float a = az_deg * (float)M_PI / 180.0f;
    *x = -sinf(a) * rr;
    *y = -cosf(a) * rr;
}

static const uint8_t sky__body_col[BODY_COUNT][3] = {
    { 224, 160,  40 },   // Sun
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
    (void)dt;
    st->blend = sc->sky_blend;
    if (st->blend < 0.001) return;

    double jd_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                 - t->config.timezone / 24.0;
    if (fabs(jd_ut - st->cache_jd) <= 1.0 / 1440.0) return;
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

static void sky_render(const void *buf, DrawCtx *d, const Tempus *t,
                       const RenderStyle *s) {
    const SkyViewState *st = (const SkyViewState *)buf;
    float base_alpha = d->alpha;
    (void)t;

    // ---- The bowl: sky tint riding the sun's altitude ----
    // Day -> civil -> nautical -> astronomical -> night, as one smooth
    // ramp. An instrument's sky, not a photograph's: always dark enough
    // to read engravings against.
    {
        float sa = st->body_alt[BODY_SUN];
        float day = (float)tempus_smoothstep(-18.0, 8.0, sa);
        draw_set_color(d, dca(0.020f + 0.055f * day,
                              0.022f + 0.075f * day,
                              0.035f + 0.130f * day, 1.0f));
        draw_circle_filled(d, 0, 0, SKY_R);
    }

    // Altitude circles at 30 and 60 degrees + the zenith
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.10f));
    draw_circle_stroked(d, 0, 0, SKY_R * (2.0f / 3.0f), 1.0f);
    draw_circle_stroked(d, 0, 0, SKY_R * (1.0f / 3.0f), 1.0f);
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
    draw_line(d, -6.0f, 0, 6.0f, 0, 1.0f);
    draw_line(d, 0, -6.0f, 0, 6.0f, 1.0f);

    // ---- The ecliptic: the zodiac's lie across tonight's sky ----
    // Same gold as the combust wedge family; sign cusps tick it, tying
    // this view back to the MACHINA MVNDI dial.
    {
        draw_set_color(d, dca(0.65f, 0.52f, 0.25f, 0.30f));
        for (int i = 0; i < SKY_ECL_N; i++) {
            int j = (i + 1) % SKY_ECL_N;
            if (st->ecl_alt[i] < 0.5f || st->ecl_alt[j] < 0.5f) continue;
            float x0, y0, x1, y1;
            sky__project(st->ecl_az[i], st->ecl_alt[i], &x0, &y0);
            sky__project(st->ecl_az[j], st->ecl_alt[j], &x1, &y1);
            draw_line(d, x0, y0, x1, y1, 1.0f);
        }
        draw_set_color(d, dca(0.65f, 0.52f, 0.25f, 0.45f));
        for (int i = 0; i < 12; i++) {
            if (st->sign_alt[i] < 1.0f) continue;
            float x, y;
            sky__project(st->sign_az[i], st->sign_alt[i], &x, &y);
            draw_line(d, x - 4.0f, y - 4.0f, x + 4.0f, y + 4.0f, 1.0f);
            draw_line(d, x - 4.0f, y + 4.0f, x + 4.0f, y - 4.0f, 1.0f);
        }
    }

    // ---- Diurnal arcs: each body's whole day across the bowl ----
    for (int b = 0; b < BODY_COUNT; b++) {
        float pa = (b == BODY_SUN) ? 0.30f
                 : (b == BODY_MOON) ? 0.25f : 0.18f;
        draw_set_color(d, dca(sky__body_col[b][0] / 255.0f,
                              sky__body_col[b][1] / 255.0f,
                              sky__body_col[b][2] / 255.0f, pa));
        for (int i = 0; i + 1 < SKY_PATH_N; i++) {
            if (st->path[b][i][1] < 0.3f || st->path[b][i + 1][1] < 0.3f)
                continue;
            float x0, y0, x1, y1;
            sky__project(st->path[b][i][0], st->path[b][i][1], &x0, &y0);
            sky__project(st->path[b][i + 1][0], st->path[b][i + 1][1],
                         &x1, &y1);
            draw_line(d, x0, y0, x1, y1, 1.0f);
        }
    }

    // ---- The bodies themselves, where they hang right now ----
    for (int b = BODY_COUNT - 1; b >= 0; b--) {   // sun and moon last
        if (st->body_alt[b] < -0.5f) continue;
        float x, y;
        sky__project(st->body_az[b], st->body_alt[b], &x, &y);
        float pr = orr__pip_r(st->now.mag[b]);

        if (b == BODY_MOON) {
            // The moon as a phase-lit globe, its bright limb aimed at
            // the sun's place in (or under) the sky
            GlobeCmd *gm = draw_globe_slot(d, x, y, pr + 12.0f);
            if (gm) {
                double phb = globe_moon_phase(st->tv.jd_current
                                              + st->tv.percent_of_day - 0.5);
                float bb = (float)(phb * 2.0 * M_PI);
                float sx, sy;
                sky__project(st->body_az[BODY_SUN],
                             st->body_alt[BODY_SUN], &sx, &sy);
                float ux = sx - x, uy = sy - y;
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

        draw_set_color(d, dca(sky__body_col[b][0] / 255.0f,
                              sky__body_col[b][1] / 255.0f,
                              sky__body_col[b][2] / 255.0f, 0.95f));
        if (st->sky.observable[b] || b == BODY_SUN)
            draw_circle_filled(d, x, y, pr);
        else
            draw_circle_stroked(d, x, y, pr, 1.2f);

        if (sky__body_name[b]) {
            int lw = _font_compat[FONT_date].weight;
            float lsz = 15.0f;
            float tw = sdf_measure_width(lw, sky__body_name[b]) * lsz;
            d->alpha = base_alpha * 0.55f;
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.85f));
            draw_text_ex(d, lw, lsz, x - tw * 0.5f, y + pr + 5.0f,
                         sky__body_name[b]);
            d->alpha = base_alpha;
        }
    }

    // ---- The horizon rim + compass ----
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.55f));
    draw_circle_stroked(d, 0, 0, SKY_R, 1.0f);
    {
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
            draw_line(d, dx * SKY_R, dy * SKY_R,
                      dx * (SKY_R + (major ? 14.0f : 7.0f)),
                      dy * (SKY_R + (major ? 14.0f : 7.0f)), 1.0f);
            if (major) {
                float sz = _font_compat[FONT_month].size;
                float tw2 = sdf_measure_width(cw, card[i / 9]) * sz;
                draw_set_color(d, dca(0.66f, 0.63f, 0.57f, 0.75f));
                draw_text_ex(d, cw, sz,
                             dx * (SKY_R + 34.0f) - tw2 * 0.5f,
                             dy * (SKY_R + 34.0f) - sz * 0.5f,
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
