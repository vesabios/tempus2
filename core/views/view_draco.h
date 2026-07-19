// view_draco.h — DRACO: the eclipse dragon.
//
// The medieval answer to why the lights go out: a dragon coiled
// through the zodiac, devouring the sun or moon when they stray into
// its jaws. Its head and tail are real machinery — CAPVT and CAVDA
// DRACONIS, the lunar nodes, where the moon's tilted orbit crosses
// the ecliptic. The dragon's BODY here is that orbit's latitude wave
// drawn honestly: a sinusoid weaving inside and outside the ecliptic
// circle, crossing it exactly at the nodes, regressing westward once
// around in 18.6 years. The sun rides the ecliptic; the moon rides
// the wave. When both stand at a crossing together, the dragon eats —
// and that is an eclipse, the only time the geometry permits one.
// Scrub the years and hunt eclipse seasons: they arrive twice a year,
// wherever the head and tail have wandered.

#ifndef VIEW_DRACO_H
#define VIEW_DRACO_H

#include <stdio.h>
#include "../view.h"

struct DracoViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;       // mirrored scene draco_blend

    // Ephemeris cache (minute-gated like the other stations)
    double cache_jd;
    double sun_lon, moon_lon;   // geocentric ecliptic longitudes, deg
    double moon_lat;            // geocentric ecliptic latitude, deg
    double node_lon;            // mean ascending node (CAPVT), deg
};

#endif // VIEW_DRACO_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_DRACO_IMPL)
#define VIEW_DRACO_IMPL

#define DRACO_R    370.0f   // the ecliptic circle
#define DRACO_AMP   26.0f   // the wave's reach (5.145 deg, amplified)

// The nodes' sigils: CAPVT (horseshoe opening down) and CAVDA
// (opening up), each with its two feet — stroke tables in the
// instrument's engraved idiom
static const float draco__sg_caput[] = {
    7, -0.20f,-0.14f, -0.24f,0.02f, -0.16f,0.18f, 0,0.26f,
       0.16f,0.18f, 0.24f,0.02f, 0.20f,-0.14f,
    9, -0.11f,-0.23f, -0.136f,-0.166f, -0.20f,-0.14f, -0.264f,-0.166f,
       -0.29f,-0.23f, -0.264f,-0.294f, -0.20f,-0.32f, -0.136f,-0.294f,
       -0.11f,-0.23f,
    9, 0.29f,-0.23f, 0.264f,-0.166f, 0.20f,-0.14f, 0.136f,-0.166f,
       0.11f,-0.23f, 0.136f,-0.294f, 0.20f,-0.32f, 0.264f,-0.294f,
       0.29f,-0.23f, 0 };
static const float draco__sg_cauda[] = {
    7, -0.20f,0.14f, -0.24f,-0.02f, -0.16f,-0.18f, 0,-0.26f,
       0.16f,-0.18f, 0.24f,-0.02f, 0.20f,0.14f,
    9, -0.11f,0.23f, -0.136f,0.166f, -0.20f,0.14f, -0.264f,0.166f,
       -0.29f,0.23f, -0.264f,0.294f, -0.20f,0.32f, -0.136f,0.294f,
       -0.11f,0.23f,
    9, 0.29f,0.23f, 0.264f,0.166f, 0.20f,0.14f, 0.136f,0.166f,
       0.11f,0.23f, 0.136f,0.294f, 0.20f,0.32f, 0.264f,0.294f,
       0.29f,0.23f, 0 };

// Sign names in the ablative — "the head IN Leo" — dial order
static const char *draco__sign_abl[12] = {
    "ARIETE", "TAVRO", "GEMINIS", "CANCRO", "LEONE", "VIRGINE",
    "LIBRA", "SCORPIONE", "SAGITTARIO", "CAPRICORNO", "AQVARIO",
    "PISCIBVS",
};

static void draco_init(void *buf, const Tempus *t, const RenderStyle *s) {
    DracoViewState *st = (DracoViewState *)buf;
    st->opacity = 1.0;
    st->cache_jd = -1.0e9;
    (void)t; (void)s;
}

static void draco_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void draco_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void draco_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    DracoViewState *st = (DracoViewState *)buf;
    (void)dt;
    st->blend = sc->draco_blend;
    if (st->blend < 0.001) return;

    double jd_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                 - t->config.timezone / 24.0;
    if (fabs(jd_ut - st->cache_jd) <= 1.0 / 1440.0) return;
    st->cache_jd = jd_ut;

    double lons[BODY_COUNT];
    planets__geo_lons(jd_ut, lons);
    st->sun_lon = lons[BODY_SUN];
    st->moon_lon = lons[BODY_MOON];
    st->moon_lat = planets_moon_lat(jd_ut);
    // Mean ascending node, regressing 18.6-year cycle (of-date)
    st->node_lon = planets__wrap360(
        125.04452 - 0.05295377 * (jd_ut - 2451545.0));
}

static void draco_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const DracoViewState *st = (const DracoViewState *)buf;
    float base_alpha = d->alpha;
    float d2r = (float)M_PI / 180.0f;
    (void)t;

    // ---- The alignment, measured first: how close the dragon's jaws
    // stand to a feeding. Eclipse season = sun within ~18 deg of a
    // node; the meal itself needs the moon at syzygy too.
    float dh = (float)fabs(planets_lon_diff(st->sun_lon, st->node_lon));
    float dt2 = (float)fabs(planets_lon_diff(st->sun_lon,
                                             st->node_lon + 180.0));
    bool head_near = dh <= dt2;
    float dmin = head_near ? dh : dt2;
    float season = 1.0f - (float)tempus_smoothstep(10.0, 18.0, dmin);
    float el = (float)fabs(planets_lon_diff(st->moon_lon, st->sun_lon));
    float syz_new  = 1.0f - (float)tempus_smoothstep(0.0, 10.0, el);
    float syz_full = 1.0f - (float)tempus_smoothstep(0.0, 10.0,
                                                     180.0 - el);
    float syz = syz_new > syz_full ? syz_new : syz_full;
    float glow = season * syz;

    // ---- The ecliptic: the sun's road, and the zodiac around it ----
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.30f));
    draw_circle_stroked(d, 0, 0, DRACO_R, 1.0f);
    for (int i = 0; i < 12; i++) {
        float dx, dy;
        orr__ecl_dir(i * 30.0, &dx, &dy);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
        draw_line(d, dx * (DRACO_R + 26.0f), dy * (DRACO_R + 26.0f),
                  dx * (DRACO_R + 38.0f), dy * (DRACO_R + 38.0f), 1.0f);
        float mx, my;
        orr__ecl_dir(i * 30.0 + 15.0, &mx, &my);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.50f));
        orr__zodiac_glyph(d, i, mx * (DRACO_R + 32.0f),
                          my * (DRACO_R + 32.0f), mx, my, 22.0f);
    }

    // ---- The dragon's body: the moon's orbit as a latitude wave ----
    // r = R + k*sin(lon - node): inside the ecliptic half the way
    // round, outside the other half, crossing AT the nodes. Two edges
    // give the serpent a body; it thins toward the crossings where
    // the head and tail take over.
    {
        const int N = 288;
        for (int e = 0; e < 2; e++) {
            float side = e ? -1.0f : 1.0f;
            float lx = 0, ly = 0;
            for (int i = 0; i <= N; i++) {
                float lam = (float)i / N * 360.0f;
                float ph = (lam - (float)st->node_lon) * d2r;
                float body = 2.6f * fabsf(sinf(ph));   // girth
                float rr = DRACO_R + DRACO_AMP * sinf(ph) + side * body;
                float dx, dy;
                orr__ecl_dir(lam, &dx, &dy);
                float px = dx * rr, py = dy * rr;
                if (i) {
                    draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.50f));
                    draw_line(d, lx, ly, px, py, 1.2f);
                }
                lx = px; ly = py;
            }
        }
    }

    // ---- Head and tail at the crossings ----
    {
        float hx, hy, tx, ty;
        orr__ecl_dir(st->node_lon, &hx, &hy);
        orr__ecl_dir(st->node_lon + 180.0, &tx, &ty);
        float ha = 0.55f + (head_near ? 0.40f * season : 0.0f);
        float ta = 0.55f + (!head_near ? 0.40f * season : 0.0f);
        // The feeding: a soft gold halo swells at the near jaw as the
        // moon closes on syzygy in season
        if (glow > 0.01f) {
            float gx = head_near ? hx : tx, gy = head_near ? hy : ty;
            DrawColor g = dc_scale(s->sunrise_handle, 1.0f);
            g.a = 0.30f * glow;
            draw_set_color(d, g);
            draw_circle_filled(d, gx * DRACO_R, gy * DRACO_R,
                               20.0f + 10.0f * glow);
        }
        draw_set_color(d, dca(0.72f, 0.70f, 0.64f, ha));
        orr__strokes(d, draco__sg_caput, hx * DRACO_R, hy * DRACO_R,
                     hx, hy, 34.0f, 1.3f);
        draw_set_color(d, dca(0.72f, 0.70f, 0.64f, ta));
        orr__strokes(d, draco__sg_cauda, tx * DRACO_R, ty * DRACO_R,
                     tx, ty, 34.0f, 1.3f);
    }

    // ---- The travelers: sun on the road, moon on the wave ----
    // Full presence, equal size — when they meet at a jaw, the
    // occlusion IS the eclipse. The moon wears the clock face's own
    // dress: the phase-lit globe, its bright limb aimed at the sun.
    {
        float sx, sy;
        orr__ecl_dir(st->sun_lon, &sx, &sy);
        float spx = sx * DRACO_R, spy = sy * DRACO_R;
        draw_set_color(d, dca(0.85f, 0.62f, 0.18f, 0.95f));
        draw_circle_filled(d, spx, spy, 22.0f);
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.35f));
        draw_circle_stroked(d, spx, spy, 27.0f, 1.0f);

        float mx, my;
        orr__ecl_dir(st->moon_lon, &mx, &my);
        // The moon sits at its TRUE latitude, scaled by the wave's
        // own exaggeration — it rides the dragon's back exactly
        float mr = DRACO_R + DRACO_AMP / 5.145f * (float)st->moon_lat;
        float mpx = mx * mr, mpy = my * mr;
        GlobeCmd *gm = draw_globe_slot(d, mpx, mpy, 22.0f);
        if (gm) {
            // The clock aperture's own dress: PHASE-FRAME light, the
            // canonical pure lune (bright limb to screen-right), not
            // a 3D reading aimed at the sun
            double phb = globe_moon_phase(st->cache_jd);
            float bb = (float)(phb * 2.0 * M_PI);
            globe_rotation(0, 0, gm->rot);
            gm->light[0] = sinf(bb);
            gm->light[1] = 0.0f;
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
    }

    // ---- The reading ----
    // Degree within the sign rides along (archaic additive numerals),
    // so the node's slow regression READS as motion — without it, a
    // node near a cusp flips signs under the scrub as if at random
    static const char *draco__deg[30] = {
        "", "I", "II", "III", "IIII", "V", "VI", "VII", "VIII",
        "VIIII", "X", "XI", "XII", "XIII", "XIIII", "XV", "XVI",
        "XVII", "XVIII", "XVIIII", "XX", "XXI", "XXII", "XXIII",
        "XXIIII", "XXV", "XXVI", "XXVII", "XXVIII", "XXVIIII",
    };
    {
        int hs = (int)(st->node_lon / 30.0) % 12;
        int ts = (hs + 6) % 12;
        int dg = (int)fmod(st->node_lon, 30.0);
        char line[48];
        draw_set_color(d, dca(0.80f, 0.77f, 0.70f, 0.95f));
        snprintf(line, sizeof(line), "CAPVT IN %s %s",
                 draco__sign_abl[hs], draco__deg[dg]);
        draw_text_centered(d, FONT_month, 0, -18.0f, line);
        draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.55f));
        snprintf(line, sizeof(line), "CAVDA IN %s %s",
                 draco__sign_abl[ts], draco__deg[dg]);
        draw_text_centered(d, FONT_date, 0, 18.0f, line);
        if (season > 0.15f) {
            DrawColor g = dc_scale(s->sunrise_handle, 1.0f);
            g.a = 0.45f + 0.50f * season;
            draw_set_color(d, g);
            draw_text_centered(d, FONT_date, 0, 48.0f,
                               "TEMPVS ECLIPSIVM");
        }
    }

    d->alpha = base_alpha;
}

static const ViewVtable draco_vtable = {
    .init   = draco_init,
    .enter  = draco_enter,
    .exit   = draco_exit,
    .update = draco_update,
    .render = draco_render,
};

#endif // SCENE_DEFINED && !VIEW_DRACO_IMPL
