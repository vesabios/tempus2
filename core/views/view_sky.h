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
#define SKY_ANA_N      74       // analemma samples (~5-day steps)

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
    double astb;      // mirrored scene astro_blend (the shared sky)

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

    // CAELVM'S LAYER TOGGLES. The chart carries a great deal now, so
    // each family can be put away. Read by the shell's toggle list
    // (lower right) and by the draws below; pl_show is PUBLISHED
    // because the planets themselves are VIEW_LVMEN's, not this
    // view's — only their paths and labels are drawn here.
    bool  show_planets, show_figures, show_stars;
    bool  show_cage, show_analemma;
    float pl_show;          // 1/0, mirrored for the orrery's compose

    // THE CHART'S FLYWHEEL: let go mid-turn and the sky keeps
    // swinging, decaying like the band's and the globe's. Held in
    // SCREEN units/sec so the coast runs back through sky_view_pan and
    // inherits its loupe scaling live — store degrees and a coast
    // begun at one zoom would carry the wrong rate into another.
    float pan_ax, pan_ay;   // drag delta since the last update tick
    float pan_vx, pan_vy;   // units/sec; nonzero = coasting

    // The LOUPE: 1 = whole sphere, SKY_LOUPE_MAX = horizon circle on
    // the calendar wheel. Station-local; see sky__loupe.
    float loupe;

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
    // THE ANALEMMA: where the sun stands at THIS clock time on every
    // day of the year — the figure of eight. Held in az/alt, this
    // view's own frame; ORBIS keeps its own copy in dial coords and
    // the two cannot be shared.
    float ana_az[SKY_ANA_N], ana_alt[SKY_ANA_N];

    float ecl_az[SKY_ECL_N], ecl_alt[SKY_ECL_N];
    float sign_az[12], sign_alt[12];         // cusp positions (0 Aries...)
    float sign_maz[12], sign_malt[12];       // sign MIDDLES (the sigils)
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

// THE LOUPE. A pure multiplier on the projection's radius — not a
// magnifier: the mapping stays azimuthal-equidistant with the same
// distortion law, so this only chooses how much of the sphere the
// disc spends itself on. Expanding SPACE, not the image (Seren) —
// body radii are deliberately untouched.
//   1.00  the whole sphere, nadir at the rim (SKY_R)
//   2.18  the horizon circle out on the calendar wheel; the visible
//         hemisphere alone, the unseen half bled off the frame
//   6.00  a ~34 degree field — constellations at reading size, the
//         horizon itself off the frame. Past the 2.18 landmark the
//         wheel stops being a boundary and becomes what it is: a
//         frame the sky passes under.
static float sky__loupe = 1.0f;

#define SKY_LOUPE_MAX 6.0f

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
    // EAST LEFT (Seren): this is a sky seen looking UP facing north,
    // not a map looked down upon — the right-hand basis is negated,
    // matching the astrolabe's plate and every planisphere
    sky__vr[0] = (sky__vd[1] * sky__vc[2] - sky__vd[2] * sky__vc[1]);
    sky__vr[1] = (sky__vd[2] * sky__vc[0] - sky__vd[0] * sky__vc[2]);
    sky__vr[2] = (sky__vd[0] * sky__vc[1] - sky__vd[1] * sky__vc[0]);
}

// The azimuth the projection should actually use. It EASES BACK TO
// NORTH as the astrolabe takes the shared wash over (Seren): the wash
// blends per-vertex between this bowl and the plate, and the plate's
// frame is fixed north-up, so a swung bowl twists against it and the
// sky deforms mid-flight. Weighted by the wash's own mix (wc), so a
// departure to any OTHER station — where nothing blends against the
// bowl's geometry — leaves the azimuth alone and does not spin the
// chart on the way out.
static inline float sky__az(const SkyViewState *st) {
    float fam = (float)st->blend + (float)st->astb;
    float wc = fam > 1.0e-6f ? (float)st->blend / fam : 1.0f;
    // AZIMUTH IS CIRCULAR: fold to the signed shortest arc before
    // scaling, or the ease home is a plain scalar lerp toward zero and
    // takes the LONG way round — a chart swung to 250 walks down
    // through 180 instead of up through 360 (Seren). Folding to
    // [-180, 180) makes 250 read as -110 and the wc scaling then
    // shortens the near side. The result may be negative; the basis
    // builder takes any angle.
    float a = fmodf(st->view_az + 180.0f, 360.0f);
    if (a < 0.0f) a += 360.0f;
    a -= 180.0f;
    return a * wc;
}

// Both update and render arm the projection through here. The loupe
// RELAXES TO 1 as the station is left, so every flight — and the
// bowl the astrolabe's wash blends against — sees the unscaled sphere.
static inline void sky__arm(const SkyViewState *st) {
    sky__set_center(sky__az(st), st->view_alt);
    sky__loupe = 1.0f + (st->loupe - 1.0f) * (float)st->blend;
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
    float rr = acosf(cosc) / (float)M_PI * SKY_R * sky__loupe;
    float px = v[0]*sky__vr[0] + v[1]*sky__vr[1] + v[2]*sky__vr[2];
    float py = v[0]*sky__vd[0] + v[1]*sky__vd[1] + v[2]*sky__vd[2];
    float len = sqrtf(px * px + py * py);
    if (len < 1.0e-5f) {
        *x = 0.0f;
        *y = cosc > 0.0f ? 0.0f : SKY_R * sky__loupe;
        return;
    }
    *x = px / len * rr;
    *y = py / len * rr;
}

// Pan the look. Vertical tips the PITCH between the zenith and the
// faced horizon; horizontal swings the AZIMUTH around.
//
// The azimuth rate is FLAT — a fixed degrees-per-pixel, the same
// whether the grab is at the rim or on the zenith (Seren). The
// tempting reading of a horizontal drag is a torque about the
// center, but that rate goes as 1/radius and a grab near the middle
// spins the whole sky at absurd speed. A flat rate has no such
// singularity and needs no dead zone around the center.
//
// The pitch keeps its clamp (no digging below 5 degrees); the azimuth
// wraps freely — at the zenith it simply turns the chart, which is
// well-defined, so there is still no pole to mishandle.
static inline void sky_view_pan(SkyViewState *st, float dx, float dy) {
    // DIVIDED BY THE LOUPE (Seren): the sky is magnified L times, so
    // a pixel of drag must buy 1/L of the angle or the chart runs away
    // from the finger — it was ~6x too fast at full zoom, which is
    // exactly where the fine control is wanted. Uses the EFFECTIVE
    // loupe (the same relaxation sky__arm applies), so a drag caught
    // mid-flight scales by what is actually on screen.
    float lp = 1.0f + (st->loupe - 1.0f) * (float)st->blend;
    if (lp < 1.0f) lp = 1.0f;
    float k = 180.0f / (SKY_R * lp);
    // NEGATED against dx: the chart handedness flip mirrored screen-x
    // (see sky__set_center), so the drag had to mirror with it or the
    // sky swings away from the finger.
    float az = st->view_az - dx * k;
    az = fmodf(az, 360.0f);
    if (az < 0.0f) az += 360.0f;
    st->view_az = az;

    float alt = st->view_alt + dy * k;
    if (alt > 90.0f) alt = 90.0f;
    if (alt < 5.0f) alt = 5.0f;     // the pitch clamp: no digging
    st->view_alt = alt;
}

// Scroll the loupe. Multiplicative so each notch feels the same at
// both ends of the range, clamped to the two honest states: the whole
// sphere, and the horizon circle out on the calendar wheel.
static inline void sky_view_loupe(SkyViewState *st, float dy) {
    if (st->blend < 0.5) return;      // CAELVM's gesture alone
    float l = st->loupe * (1.0f + dy * 0.06f);
    if (l < 1.0f) l = 1.0f;
    if (l > SKY_LOUPE_MAX) l = SKY_LOUPE_MAX;
    st->loupe = l;
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
    st->show_planets = true;
    st->show_figures = true;
    st->show_stars = true;
    st->show_cage = true;
    st->show_analemma = true;
    st->pl_show = 1.0f;
    st->view_alt = 45.0f;   // default LOOK: halfway to the north
                            // horizon (Seren) — the sky as a scene,
                            // not a map; drag restores the zenith
    // Dev: TEMPUS_SKYZOOM pins the loupe until it has a gesture
    st->loupe = 1.0f;
    {
        const char *z = getenv("TEMPUS_SKYZOOM");
        if (z) {
            float v = (float)atof(z);
            if (v < 1.0f) v = 1.0f;
            if (v > SKY_LOUPE_MAX) v = SKY_LOUPE_MAX;
            st->loupe = v;
        }
    }
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
    st->pl_show = st->show_planets ? 1.0f : 0.0f;
    (void)dt;
    st->blend = sc->sky_blend;
    st->orbw = sc->orbis_wheel;
    st->astb = sc->astro_blend;
    if (st->blend < 0.001) return;
    sky__arm(st);
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

    // The analemma at the CURRENT clock time, so the figure passes
    // through the live sun. Minute-cached with everything else here —
    // it is 74 SPA solves, too heavy for a frame.
    {
        const TempusConfig *cfg = &t->config;
        int ay = st->tv.year;
        int adays = cal_days_in_year(ay);
        double apct = st->tv.percent_of_day;
        for (int k = 0; k < SKY_ANA_N; k++) {
            int doy = (int)((double)k / SKY_ANA_N * adays);
            int am, ad;
            solar__doy_to_date(ay, doy, &am, &ad);
            spa_data aspa;
            solar__spa_at(cfg, ay, am, ad, apct, cfg->timezone, &aspa);
            st->ana_az[k] = (float)aspa.azimuth;
            st->ana_alt[k] = (float)(90.0 - aspa.zenith);
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
        planets_sky_azalt(i * 30.0 + 15.0, 0.0, jd_ut, lat, lon,
                          &az, &alt);
        st->sign_maz[i] = (float)az;
        st->sign_malt[i] = (float)alt;
    }

    // Targets from the fresh ephemeris — always AFTER the fill, so
    // the very first update never publishes zeroed az/alt (the sun
    // and moon were landing pinned to the north horizon)
    sky__lum_targets(st);
}

// Alias kept for the morph call sites: the full-sphere projection
// already accepts any altitude
// SHOULD THIS SEGMENT BE DRAWN? The antipode of the look maps to the
// WHOLE RIM, so any polyline passing near it leaves one edge and
// re-enters at another — and joining those two samples draws a chord
// clean across the chart (Seren caught the ecliptic doing it). The
// projection is equidistant, so honest motion between two samples is
// bounded by their true angular separation; only the antipode's
// blow-up beats that bound. Segments that do are dropped, leaving a
// small gap at the rim instead of a false line through the middle.
//
// Applies to EVERY polyline through this projection — ecliptic,
// diurnal paths, graticule, analemma, constellation figures.
static inline bool sky__seg_ok(float az0, float alt0,
                               float az1, float alt1,
                               float x0, float y0, float x1, float y1) {
    float v0[3], v1[3];
    sky__vec(az0, alt0, v0);
    sky__vec(az1, alt1, v1);
    float dp = v0[0]*v1[0] + v0[1]*v1[1] + v0[2]*v1[2];
    if (dp > 1.0f) dp = 1.0f;
    if (dp < -1.0f) dp = -1.0f;
    float sep = acosf(dp) * 180.0f / (float)M_PI;
    float R = SKY_R * sky__loupe;
    float allow = (sep / 180.0f) * R * 2.5f + 8.0f;
    float dx = x1 - x0, dy = y1 - y0;
    return (dx * dx + dy * dy) <= allow * allow;
}

// ADAPTIVE ARC. Draws a curve segment between two sky directions,
// subdividing until each piece is short ON SCREEN. The projection
// stretches without bound toward the antipode, so a fixed angular
// step gives long faceted chords out at the rim and wastes samples
// near the centre where everything is compressed (Seren). Splitting
// on the SCREEN gap spends samples exactly where the projection is
// stretching them apart.
//
// The midpoint is found by SLERP in direction space, not by averaging
// angles — exact for great circles (the ecliptic, the hour circles),
// and negligibly off for the small circles at these step sizes.
// Wrapping segments bail: the antipode is a genuine discontinuity and
// no amount of subdivision converges across it.
// WHICH LINE PRIMITIVE THE ARCS END IN. The chart's sparse furniture —
// the graticule, the constellation figures — has nothing nearby to
// alias against, so it draws as plain quads at a third the geometry
// (see draw_line_flat). Set around those blocks and cleared after;
// everything else keeps the antialiased ribbon.
static bool sky__flat = false;
static inline void sky__stroke(DrawCtx *d, float x0, float y0,
                               float x1, float y1, float w) {
    if (sky__flat) draw_line_flat(d, x0, y0, x1, y1, w);
    else           draw_line(d, x0, y0, x1, y1, w);
}

static void sky__arc(DrawCtx *d, float az0, float alt0,
                     float az1, float alt1, float w, int depth) {
    float x0, y0, x1, y1;
    sky__project(az0, alt0, &x0, &y0);
    sky__project(az1, alt1, &x1, &y1);
    // TEMPUS_NOWRAPCUT=1 keeps the wrapping segments, to see what the
    // subdivision alone does with them (Seren). Off by default: the
    // antipode is a real discontinuity and no depth converges across
    // it, so the chord survives however finely it is split.
    static int wrapcut = -1;
    if (wrapcut < 0) wrapcut = getenv("TEMPUS_NOWRAPCUT") ? 0 : 1;
    if (wrapcut && !sky__seg_ok(az0, alt0, az1, alt1, x0, y0, x1, y1))
        return;
    // OFF-SCREEN SEGMENTS COST NOTHING. At full loupe the sphere maps
    // to a radius five times the frame's half-height, so most of any
    // curve is outside it — and subdividing that was more than half
    // the whole index budget (Seren hit the cap and layers began
    // dropping). A bounding-box reject is conservative and exact: a
    // segment whose box misses the frame cannot cross it.
    {
        float sx = d->sx != 0.0f ? d->sx : 1.0f;
        float sy = d->sy != 0.0f ? d->sy : 1.0f;
        float xlo = (-d->screen_w * 0.5f - d->tx) / sx - 40.0f;
        float xhi = ( d->screen_w * 0.5f - d->tx) / sx + 40.0f;
        float ylo = (-d->screen_h * 0.5f - d->ty) / sy - 40.0f;
        float yhi = ( d->screen_h * 0.5f - d->ty) / sy + 40.0f;
        if ((x0 < xlo && x1 < xlo) || (x0 > xhi && x1 > xhi)
         || (y0 < ylo && y1 < ylo) || (y0 > yhi && y1 > yhi)) return;
    }
    // SPLIT ON CURVATURE, NOT ON LENGTH. This used to subdivide any
    // chord longer than 14px, which chops a line that is ALREADY
    // STRAIGHT on screen into fourteen-pixel crumbs — the graticule
    // alone was spending 50k indices that way, and at the machina
    // morph (both worlds drawn at once) the sum of the two saturated
    // the index buffer exactly, so whole layers were silently dropped
    // and came back one at a time as the budget freed (Seren: "they
    // literally disappear"). The honest question is not how long the
    // chord is but how far the curve departs from it: project the true
    // midpoint and measure its sagitta against the chord's midpoint.
    // Under half a pixel, one line IS the curve, at any length.
    //
    // WHY THE OLD LENGTH RULE EXISTED, and what has to change if it is
    // ever wanted back: the density was buying a crisp edge at the
    // PLATE'S LIMB (Seren). That limb is not a clip but a radial CLAMP
    // (view_astro.h: x *= clip/pr2) which slides stray vertices down
    // onto the boundary circle — so the edge reads clean only when
    // enough vertices land near it to trace the circle, and chopping
    // every chord to 14px was how they got there. With the clamp
    // currently disabled (clip = 1.0e6f, ASTROLABIVM out of the menu)
    // that spend buys nothing at all. Should the limb return, DO NOT
    // reach for density again: intersect the segment with the circle
    // and emit the true crossing point. That is crisp by construction,
    // costs nothing, and also fixes the deformation the clamp causes
    // by sliding crossing vertices along the radius instead of cutting.
    if (depth > 0) {
        float v0[3], v1[3];
        sky__vec(az0, alt0, v0);
        sky__vec(az1, alt1, v1);
        float mx = v0[0] + v1[0], my = v0[1] + v1[1], mz = v0[2] + v1[2];
        float n = sqrtf(mx * mx + my * my + mz * mz);
        if (n > 1.0e-5f) {
            mx /= n; my /= n; mz /= n;
            float altm = asinf(mz) * 180.0f / (float)M_PI;
            float azm = atan2f(mx, my) * 180.0f / (float)M_PI;
            float xm, ym;
            sky__project(azm, altm, &xm, &ym);
            float ex = xm - 0.5f * (x0 + x1);
            float ey = ym - 0.5f * (y0 + y1);
            static float tol = -1.0f;
            if (tol < 0.0f) { const char *e = getenv("TEMPUS_ARCTOL");
                              tol = e ? (float)atof(e) : 0.45f; }
            if (ex * ex + ey * ey > tol * tol) {
                sky__arc(d, az0, alt0, azm, altm, w, depth - 1);
                sky__arc(d, azm, altm, az1, alt1, w, depth - 1);
                return;
            }
        }
    }
    sky__stroke(d, x0, y0, x1, y1, w);
}

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
    sky__arm(st);
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
    // ms IS A PRESENCE, NOT A SECOND EASING. It answers "is there a
    // machine counterpart to fly from at all" — parked at MACHINA yes,
    // parked anywhere else no. Multiplying the raw value into the
    // weight made mw the PRODUCT OF TWO EASED QUANTITIES, which is the
    // one thing this instrument's continuity law forbids: flying
    // CAELVM -> MACHINA both ms and (1-mb) travel 0->1 over the same
    // flight, so their product double-eases. Measured, the rings stood
    // 21% of the way to the machine when the flight was 58% done, then
    // rushed the rest — Seren saw exactly that as "fading out and then
    // snapping in at the very end". Saturating it early leaves the
    // weight riding the sky's OWN departure, one ease, monotone.
    float have = (float)tempus_smoothstep(0.0, 0.25, ms);
    float mw = have * (1.0f - mb);
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
        (void)R_live;
        // (The ground disc and the dome are drawn ONCE by the
        // calendar view — the shared sky circle and its dark earth.)

        // (The dome is drawn ONCE by the calendar view — the shared
        // sky circle across the whole chart family.)
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
            for (int i = 0; i < 24; i++) {
                float a0 = (float)i / 24.0f * 360.0f;
                float a1 = (float)(i + 1) / 24.0f * 360.0f;
                sky__arc(d, a0, circ_alt[ci], a1, circ_alt[ci], 1.0f, 5);
            }
        }
        // ---- The graticule: RIGHT ASCENSION + DECLINATION ----
        // Centred on the CELESTIAL POLE, because that is the point the
        // sky turns about: scrub the 24-hour ring and the whole lattice
        // spins in place around its own centre rather than wobbling
        // (Seren's test, and the thing that settles which frame this
        // is). An ecliptic grid centres on the ecliptic pole, which
        // itself circles the celestial one — so it drifts under
        // exactly that gesture.
        //
        // Present ONLY under the loupe: nothing at rest, full ink at
        // maximum zoom, where the horizon and the cardinals have left
        // the frame and this is all that says where you are looking.
        {
            float grat = st->show_cage
                       ? (float)tempus_smoothstep(1.15, SKY_LOUPE_MAX,
                                                  sky__loupe) : 0.0f;
            if (grat > 0.004f) {
                const TimeView *gtv = &st->tv;
                double jd_g = gtv->jd_current + gtv->percent_of_day
                            - 0.5 - t->config.timezone / 24.0;
                double glat = t->config.latitude;
                float lst_g = (float)fmod(planets__gmst(jd_g)
                                          + t->config.longitude, 360.0);
                d->alpha = base_alpha * fb * grat;
                // Hour circles every 30 deg (2h of RA), pole to pole.
                for (int m = 0; m < 12; m++) {
                    float ra = m * 30.0f;
                    bool major = (m % 3) == 0;
                    draw_set_color(d, dca(0.55f, 0.58f, 0.62f,
                                          major ? 0.22f : 0.13f));
                    float paz = 0, palt = 0;
                    for (int k = 0; k <= 12; k++) {
                        float dec = -90.0f + (float)k / 12.0f * 180.0f;
                        float az, alt;
                        planets_star_azalt(ra, dec, lst_g, (float)glat,
                                           &az, &alt);
                        if (k > 0)
                            sky__arc(d, paz, palt, az, alt, 1.0f, 5);
                        paz = az; palt = alt;
                    }
                }
                // Declination rings every 15 deg; the equator leads.
                for (int dd = -75; dd <= 75; dd += 15) {
                    bool eq = (dd == 0);
                    draw_set_color(d, dca(0.55f, 0.58f, 0.62f,
                                          eq ? 0.30f
                                             : ((dd % 45) == 0 ? 0.18f
                                                               : 0.11f)));
                    float paz = 0, palt = 0;
                    for (int k = 0; k <= 18; k++) {
                        float ra = (float)k / 18.0f * 360.0f;
                        float az, alt;
                        planets_star_azalt(ra, (float)dd, lst_g,
                                           (float)glat, &az, &alt);
                        if (k > 0)
                            sky__arc(d, paz, palt, az, alt, 1.0f, 5);
                        paz = az; palt = alt;
                    }
                }
                d->alpha = base_alpha * fb;
            }
        }

        // ---- The ANALEMMA ----
        // The sun's place at this same clock time on every day of the
        // year: the figure of eight, whose fatness is the equation of
        // time and whose height is the seasons. Closed, and it passes
        // through the live sun by construction — it is computed at the
        // current hour. Drawn in the sun's own gold; the stretch that
        // falls below the horizon is dimmed, not hidden, on the same
        // terms as the diurnal arcs.
        if (st->show_analemma) {
            draw_set_color(d, dca(196 / 255.0f, 126 / 255.0f,
                                  16 / 255.0f, 1.0f));
            for (int k = 1; k <= SKY_ANA_N; k++) {
                int kk = k % SKY_ANA_N, pk = k - 1;
                bool up = st->ana_alt[pk] > 0.0f
                       && st->ana_alt[kk] > 0.0f;
                d->alpha = base_alpha * fb * (up ? 0.55f : 0.30f);
                sky__arc(d, st->ana_az[pk], st->ana_alt[pk],
                         st->ana_az[kk], st->ana_alt[kk], 1.0f, 5);
            }
            d->alpha = base_alpha * fb;
        }

        // ---- The NAMED STARS ----
        // The astrolabe's rete carried these and the bowl did not;
        // they belong to both (Seren). Same table, planets_stars —
        // one catalogue, so a star cannot sit in two places. Risen
        // ones burn with their names, set ones are ghosts, exactly
        // the plate's own grammar.
        if (st->show_stars) {
            const TimeView *stv = &st->tv;
            double jd_s2 = stv->jd_current + stv->percent_of_day - 0.5
                         - t->config.timezone / 24.0;
            float lst_s = (float)fmod(planets__gmst(jd_s2)
                                      + t->config.longitude, 360.0);
            int sfw = _font_compat[FONT_date].weight;
            for (int i = 0; i < PLANETS_NSTARS; i++) {
                float saz, salt;
                planets_star_azalt(planets_stars[i].ra,
                                   planets_stars[i].dec, lst_s,
                                   (float)t->config.latitude,
                                   &saz, &salt);
                float sx2, sy2;
                sky__project(saz, salt, &sx2, &sy2);
                bool up = salt > 0.0f;
                d->alpha = base_alpha * fb * (up ? 1.0f : 0.32f);
                draw_set_color(d, dca(0.92f, 0.88f, 0.76f, 1.0f));
                if (up) draw_circle_filled(d, sx2, sy2, 2.6f);
                else    draw_circle_stroked(d, sx2, sy2, 2.0f, 1.0f);
                d->alpha = base_alpha * fb * (up ? 0.85f : 0.28f);
                draw_text_ex(d, sfw, 12.0f, sx2 + 6.0f, sy2 + 4.0f,
                             planets_stars[i].name);
            }
            d->alpha = base_alpha * fb;
        }

        // The chart's edge: the point opposite the look, stretched
        // into the outermost rim
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.20f));
        // The antipode of the look sits at SKY_R * loupe by the
        // projection's own law — a literal SKY_R here left the rim
        // behind as the sky grew past it.
        draw_circle_stroked(d, 0, 0, SKY_R * sky__loupe, 1.0f);
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

    // (The ecliptic and its sigils are SHARED elements — the one
    // drawer renders them from every station's published seats.)

    // ---- Orbit rings unfurling into diurnal arcs ----
    // Each body's ring is sampled as an arc of its orbit centered on
    // where the body is now; sample i lerps to the matching moment of
    // its day across the bowl. The year becomes the day. The sun has
    // no ring — its path unfolds out of the sun itself.
    for (int b = 0; st->show_planets && b < BODY_COUNT; b++) {
        float pa = (b == BODY_SUN) ? 0.30f
                 : (b == BODY_MOON) ? 0.25f : 0.18f;
        // MACHINE-END ALPHA MUST MATCH THE ORRERY'S OWN RINGS. The
        // orrery stops drawing them the instant sky_owns goes true
        // (skyb > 0.001) and this view picks them up in the same
        // frame — so any difference here is a visible step at the
        // handoff, in both directions. It read as the rings fading to
        // nothing on the way out and popping in hard on the way back
        // (Seren). 0.44 / 0.62 are the orrery's ring inks; drifting
        // them apart again will reopen exactly this seam.
        float ra = (b == BODY_SUN) ? 0.0f
                 : (b == BODY_MOON) ? 0.30f : 0.44f;
        const uint8_t *c = sky__body_col[b];
        // WHICH WAY DOES THIS BODY'S DIURNAL PATH WIND ON SCREEN? It
        // depends on the LOOK — swing the azimuth to 180 and it
        // reverses — so the ring's sweep cannot be fixed at compile
        // time (a constant sign fixed az 0 and broke az 180, Seren).
        // Measure the signed area of the projected path and sweep the
        // machine ring the same way, so the two parametrisations agree
        // whatever the view is doing. Stable within a flight: the
        // azimuth does not move during one, and while the chart owns
        // the paths outright the machine term is zero anyway.
        float wind = 0.0f;
        {
            float wx0 = 0, wy0 = 0;
            for (int i = 0; i < SKY_PATH_N; i++) {
                float sx, sy;
                sky__project(st->path[b][i][0], st->path[b][i][1],
                             &sx, &sy);
                if (i > 0) wind += wx0 * sy - sx * wy0;
                wx0 = sx; wy0 = sy;
            }
        }
        float wsgn = (wind < 0.0f) ? -1.0f : 1.0f;
        for (int i = 0; i + 1 < SKY_PATH_N; i++) {
            float mx0, my0, mx1, my1;
            // The SKY-side endpoints are kept alongside the blended
            // ones: the wrap test below must judge the projection, not
            // the machine (see the call site).
            float skx0 = 0, sky0 = 0, skx1 = 0, sky1 = 0;
            for (int k = 0; k < 2; k++) {
                int ii = i + k;
                float *out_x = k ? &mx1 : &mx0;
                float *out_y = k ? &my1 : &my0;
                float sx, sy;
                sky__project_clamped(st->path[b][ii][0],
                                     st->path[b][ii][1], &sx, &sy);
                if (k) { skx1 = sx; sky1 = sy; }
                else   { skx0 = sx; sky0 = sy; }
                // Machine endpoint for this sample
                float rx = 0, ry = 0;
                if (b == BODY_SUN) {
                    rx = 0; ry = 0;
                } else if (b == BODY_MOON) {
                    // Its little machina orbit, sampled at the moon's
                    // real angular rate (~13.2 deg/day), SEATED ON THE
                    // MACHINE'S OWN EARTH.
                    //
                    // This used to rebuild Earth's seat from the
                    // calendar wheel — sin/cos of the year fraction
                    // times wheel_R — which is only where Earth is at
                    // tempo scale with no recentre. Measured: at tempo
                    // the two agree to 0.00, which is exactly why it
                    // went unseen; at true scale the machine has
                    // recentred Earth onto the origin and the ring
                    // still began 360px away, so the moon's orbit flew
                    // in from empty space (Seren: "the orbital ring of
                    // the moon starts from the wrong location").
                    //
                    // earth_mx/earth_my is the seat the orrery publishes
                    // after both the scale morph and the recentre —
                    // the same discipline the planets' rings above
                    // already follow through g_ox/g_oy. NOT
                    // pl_mx[PL_EARTH], which carries the same number
                    // but only on frames where the machine layout runs:
                    // reading that one made three CAELVM->MACHINA
                    // frames move, because flying INTO the machine the
                    // layout has not run yet and the seat is stale.
                    // THE BEARING IS THE ORRERY'S OWN. It used to be
                    // orr__ecl_dir(geo_lon), the ecliptic-longitude
                    // direction — which the orrery computes and then
                    // THROWS AWAY, overwriting it with the physical
                    // celestial-frame bearing (the moon frame law).
                    // Sweeping the ring from the abandoned convention
                    // put its centre sample nearly opposite the moon:
                    // measured gap 19.77 against a ring radius of
                    // 10.38. Sweep the published bearing instead and
                    // the ring's centre IS the moon, by construction.
                    float gx, gy;
                    {
                        float b0 = st->orr
                                 ? atan2f(st->orr->moon_mdy, st->orr->moon_mdx)
                                 : 0.0f;
                        float sweep = wsgn * (ii - SKY_PATH_N / 2) * 0.275f
                                    * (float)M_PI / 180.0f;
                        gx = cosf(b0 + sweep);
                        gy = sinf(b0 + sweep);
                    }
                    float edx, edy;
                    if (st->orr) {
                        edx = st->orr->earth_mx;
                        edy = st->orr->earth_my;
                    } else {
                        double yp = tempus_year_pct(t);
                        edx = sinf((float)(yp * 2.0 * M_PI)) * wheel_R;
                        edy = -cosf((float)(yp * 2.0 * M_PI)) * wheel_R;
                    }
                    // AND ITS RADIUS IS earth_r * 1.55, the orrery's own
                    // law for this ring — not a constant. It was written
                    // 42.0f * 1.55f, which is that law evaluated at one
                    // machine state and frozen; Earth's globe grows and
                    // shrinks through the morph and the scale, so the
                    // ring only landed correctly where earth_r happened
                    // to be 42 (Seren guessed this exactly: "using the
                    // unscaled orbital distance from earth"). Seating it
                    // right while sizing it wrong still starts the ring
                    // in the wrong place.
                    float emr = st->orr ? st->orr->earth_mr : 42.0f;
                    rx = edx + gx * emr * 1.55f;
                    ry = edy + gy * emr * 1.55f;
                } else {
                    int pl = planets_body_pl(b);
                    float gx, gy;
                    // A slice of the orbit ring, centered on the body.
                    // The radius is the RING's at this sample's own
                    // longitude (orr__ring_r_blend), not the planet's
                    // own distance — they differ once the orbits are
                    // ellipses — and the slice is seated on the
                    // machine's live recentre, without which the rings
                    // unfurl from heliocentric positions the dial has
                    // already left.
                    // SWEPT BACKWARDS so the two ends WIND THE SAME
                    // WAY (Seren). Sample ii runs ecliptic longitude on
                    // the machine and TIME on the chart, and those turn
                    // opposite ways on screen — measured, sky winds
                    // negative and the ring positive. With the
                    // parametrisations opposed, every sample lerped
                    // clean across the ring and the path folded through
                    // itself mid-morph. Negating the offset costs
                    // nothing: an arc centred on the body is the same
                    // arc swept either way.
                    double lon_s = st->now.helio_lon[pl]
                                 + wsgn * (ii - SKY_PATH_N / 2)
                                   * (360.0f / SKY_PATH_N);
                    orr__ecl_dir(lon_s, &gx, &gy);
                    double jd_s = st->cache_jd;
                    float orbr = orr__ring_r_blend(pl, jd_s, lon_s,
                                                   wheel_R);
                    rx = (st->orr ? st->orr->g_ox : 0.0f) + gx * orbr;
                    ry = (st->orr ? st->orr->g_oy : 0.0f) + gy * orbr;
                }
                *out_x = rx * mw + sx * (1 - mw);
                *out_y = ry * mw + sy * (1 - mw);
            }
            if (getenv("TEMPUS_WINDTEST") && i == 0) {
                // signed area contribution of this segment, machine
                // end vs sky end, about each end's own centre
                float sxa, sya, sxb, syb;
                sky__project(st->path[b][i][0], st->path[b][i][1],
                             &sxa, &sya);
                sky__project(st->path[b][i+1][0], st->path[b][i+1][1],
                             &sxb, &syb);
                int pl2 = planets_body_pl(b);
                float g0x, g0y, g1x, g1y;
                orr__ecl_dir(st->now.helio_lon[pl2]
                             + wsgn * (i - SKY_PATH_N/2) * (360.0f/SKY_PATH_N),
                             &g0x, &g0y);
                orr__ecl_dir(st->now.helio_lon[pl2]
                             + wsgn * (i + 1 - SKY_PATH_N/2) * (360.0f/SKY_PATH_N),
                             &g1x, &g1y);
                static float acc_s = 0, acc_m = 0; static int nn = 0;
                acc_s += sxa * syb - sxb * sya;
                acc_m += g0x * g1y - g1x * g0y;
                (void)acc_s; (void)acc_m; (void)nn;
                fprintf(stderr, "WIND %-8s wind=%+12.1f  sweep=%+.0f\n",
                        b < 2 ? (b ? "MOON" : "SUN")
                              : orr__planet[planets_body_pl(b)].name,
                        wind, wsgn);
            }
            bool vis = st->path[b][i][1] > 0.0f
                    && st->path[b][i + 1][1] > 0.0f;
            // The below-horizon stretch is DIMMER, not hidden (Seren):
            // it was 0.45x, which put a planet's night arc at alpha
            // 0.08 and read as absent. The whole diurnal circle is
            // worth seeing — where a body will be is as much the
            // chart's business as where it is.
            float a = ra * mw + (vis ? pa : pa * 0.78f) * sw;
            // A WRAP BELONGS TO THE SKY, SO IT DIES WITH THE SKY.
            // Whether these two samples straddle the antipode is a
            // fact about THIS PROJECTION and nothing else: on the
            // machine's orbit ring there is no antipode and no
            // discontinuity, so the same segment is perfectly real
            // there. Dropping it outright carried a caelum artifact
            // all the way into mundi — at the end of the transition,
            // with the ring fully machine-side, pieces stayed missing
            // because the sky had objected to them (Seren). So the
            // objection is WEIGHTED instead of obeyed: a wrapped
            // segment keeps only its machine share. At full machine it
            // draws in full, at full sky it is gone, and in between it
            // fades exactly as fast as the false chord fades in — no
            // threshold, which is the continuity law.
            if (!sky__seg_ok(st->path[b][i][0], st->path[b][i][1],
                             st->path[b][i+1][0], st->path[b][i+1][1],
                             skx0, sky0, skx1, sky1))
                a *= mw;
            if (a < 0.004f) continue;
            draw_set_color(d, dca(c[0] / 255.0f, c[1] / 255.0f,
                                  c[2] / 255.0f, a));
            // (The wrap is weighted above, not rejected here: the
            // endpoints it judges are the SKY's, because that is the
            // only projection an antipode exists in — feeding it the
            // blended points made it discard 60% of the machine's own
            // ring geometry.)
            draw_line(d, mx0, my0, mx1, my1, 1.0f);
        }
    }

    // (The horizon rim is stroked by the shared sky drawer — the
    // same boundary the circle itself renders, no drift possible.)

    // ---- The constellations: the figures of the fixed sky ----
    // The classical stick figures, engraved faint: risen edges read,
    // set edges whisper, each name at the center of its risen stars.
    // They bow out under the astrolabe's plate (whose rete carries
    // pointer stars instead — the period instrument's own idiom).
    if (fb > 0.001f && st->show_figures) {
        float csup = 1.0f - (float)tempus_smoothstep(0.0, 0.15,
                                                     st->astb);
        if (csup > 0.004f) {
            float lst2 = (float)fmod(
                planets__gmst(st->tv.jd_current
                              + st->tv.percent_of_day - 0.5
                              - t->config.timezone / 24.0)
                + t->config.longitude, 360.0);
            float latf = (float)t->config.latitude;
            int fw2 = _font_compat[FONT_date].weight;
            for (int c = 0; c < PLANETS_NCON; c++) {
                const Constellation *cn = &planets_constellations[c];
                float cx2[12], cy2[12], calt[12];
                for (int k = 0; k < cn->nstars; k++) {
                    float saz, salt2;
                    planets_star_azalt(cn->stars[k].ra,
                                       cn->stars[k].dec, lst2,
                                       latf, &saz, &salt2);
                    calt[k] = salt2;
                    sky__project(saz, salt2, &cx2[k], &cy2[k]);
                }
                // Below the horizon the figures wear the deep of
                // night — dark violet, plainly visible, so the
                // projection's outer-annulus warp can be SEEN
                // stretching them toward the rim (Seren)
                for (int e = 0; e < cn->nedges; e++) {
                    int a2 = cn->edges[e * 2];
                    int b2 = cn->edges[e * 2 + 1];
                    bool up2 = calt[a2] > 0 && calt[b2] > 0;
                    if (up2)
                        draw_set_color(d, dca(0.62f, 0.60f, 0.55f,
                                              0.8f));
                    else
                        draw_set_color(d, dca(0.38f, 0.33f, 0.62f,
                                              0.8f));
                    d->alpha = base_alpha * fb * csup
                             * (up2 ? 0.30f : 0.30f);
                    draw_line(d, cx2[a2], cy2[a2],
                              cx2[b2], cy2[b2], 1.0f);
                }
                float nx = 0, ny = 0, ax3 = 0, ay3 = 0;
                int nup = 0;
                for (int k = 0; k < cn->nstars; k++) {
                    bool up2 = calt[k] > 0;
                    d->alpha = base_alpha * fb * csup
                             * (up2 ? 0.60f : 0.45f);
                    draw_set_color(d, up2
                        ? dca(0.80f, 0.77f, 0.68f, 0.9f)
                        : dca(0.48f, 0.42f, 0.75f, 0.9f));
                    draw_circle_filled(d, cx2[k], cy2[k], 1.8f);
                    ax3 += cx2[k]; ay3 += cy2[k];
                    if (up2) { nx += cx2[k]; ny += cy2[k]; nup++; }
                }
                {
                    char nm[20];
                    snprintf(nm, sizeof(nm), "%s", cn->name);
                    float tw2 = sdf_measure_width(fw2, nm) * 11.0f;
                    float lx2, ly2;
                    bool up3 = nup >= 2;
                    if (up3) { lx2 = nx / nup; ly2 = ny / nup; }
                    else { lx2 = ax3 / cn->nstars;
                           ly2 = ay3 / cn->nstars; }
                    d->alpha = base_alpha * fb * csup
                             * (up3 ? 0.35f : 0.28f);
                    draw_set_color(d, up3
                        ? dca(0.62f, 0.60f, 0.55f, 0.8f)
                        : dca(0.44f, 0.39f, 0.68f, 0.8f));
                    draw_text_ex(d, fw2, 11.0f, lx2 - tw2 * 0.5f,
                                 ly2 + 4.0f, nm);
                }
            }
            d->alpha = base_alpha;
        }
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
        // Minor bearing ticks only — the CARDINALS are shared objects
        // now, drawn by the one sky drawer and riding the rim between
        // stations
        float zpx, zpy;
        sky__project(0.0f, 90.0f, &zpx, &zpy);
        for (int i = 0; i < 36; i++) {
            if ((i % 9) == 0) continue;
            float az = (float)i * 10.0f;
            float bx, by;
            sky__project(az, 0.0f, &bx, &by);
            float dx = bx - zpx, dy = by - zpy;
            float dn = sqrtf(dx * dx + dy * dy);
            if (dn < 1.0e-3f) { dx = 0; dy = 1; dn = 1; }
            dx /= dn; dy /= dn;
            draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.25f));
            draw_line(d, bx, by, bx + dx * 7.0f, by + dy * 7.0f,
                      1.0f);
        }

        // (The horizon calendar is a SHARED element now — the one
        // drawer rides its sightlines on the blended rim.)

    }

    // (The 24-hour ring is ONE OBJECT now — the calendar view
    // draws it at the station-blended radius for every station
    // that declares one.)

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
