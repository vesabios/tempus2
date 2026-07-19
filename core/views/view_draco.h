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

    // ONE OBJECT chart targets, published for the orrery's luminaire
    // composition (VIEW_LVMEN renders; this view takes over only at
    // exact coincidence when the flight completes)
    float lum_sun_x, lum_sun_y;
    float lum_moon_x, lum_moon_y;
    float lum_light[3];         // the canonical lune's phase light
};

#endif // VIEW_DRACO_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_DRACO_IMPL)
#define VIEW_DRACO_IMPL

#define DRACO_R    330.0f   // the ecliptic circle
#define DRACO_AMP   70.0f   // the wave's reach (5.145 deg, amplified)

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

// The feeding fire: a radial gradient fan, furnace-white at the core
// falling through gold and ember to nothing — per-vertex color over
// three shells. k scales the whole flame.
static void draco__glow(DrawCtx *d, float cx, float cy, float r,
                        float k, bool ember) {
    const int SEG = 48;
    #define DRACO_NSH 5
    // TIGHT falloff — alphas ride ~(1-r)^2.5, so the fire hugs the
    // silhouette in a hot ring and dies fast into a faint bloom
    // (the photographic look, not a wide wash)
    static const float shell_r[DRACO_NSH] = {
        0.0f, 0.14f, 0.30f, 0.55f, 1.0f };
    // Gold furnace for the sun's meal; for the moon's, the umbral
    // blood-copper — Earth's shadow is lit only by sunlight refracted
    // through our atmosphere, every sunset at once
    static const float shell_sun[DRACO_NSH][4] = {
        { 1.00f, 0.98f, 0.92f, 0.98f },
        { 1.00f, 0.88f, 0.55f, 0.72f },
        { 1.00f, 0.66f, 0.22f, 0.34f },
        { 0.85f, 0.40f, 0.08f, 0.10f },
        { 0.70f, 0.28f, 0.03f, 0.00f },
    };
    static const float shell_moon[DRACO_NSH][4] = {
        { 0.95f, 0.55f, 0.38f, 0.85f },
        { 0.85f, 0.32f, 0.14f, 0.55f },
        { 0.62f, 0.16f, 0.06f, 0.25f },
        { 0.40f, 0.08f, 0.03f, 0.08f },
        { 0.26f, 0.04f, 0.02f, 0.00f },
    };
    const float (*shell_c)[4] = ember ? shell_moon : shell_sun;
    int base[DRACO_NSH];
    for (int sh = 0; sh < DRACO_NSH; sh++) {
        draw_set_color(d, dca(shell_c[sh][0], shell_c[sh][1],
                              shell_c[sh][2], shell_c[sh][3] * k));
        if (sh == 0) {
            base[0] = draw__push_vert(d, cx, cy,
                                      d->white_u, d->white_v);
        } else {
            base[sh] = d->num_verts;
            for (int i = 0; i < SEG; i++) {
                float a = (float)i / SEG * 2.0f * (float)M_PI;
                draw__push_vert(d, cx + cosf(a) * r * shell_r[sh],
                                cy + sinf(a) * r * shell_r[sh],
                                d->white_u, d->white_v);
            }
        }
    }
    for (int i = 0; i < SEG; i++) {
        int j = (i + 1) % SEG;
        draw__tri(d, base[0], base[1] + i, base[1] + j);
        for (int sh = 1; sh < DRACO_NSH - 1; sh++) {
            draw__tri(d, base[sh] + i, base[sh + 1] + i,
                      base[sh + 1] + j);
            draw__tri(d, base[sh] + i, base[sh + 1] + j, base[sh] + j);
        }
    }
    #undef DRACO_NSH
}

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
    if (st->blend < 0.0001) return;

    double jd_ut = st->tv.jd_current + st->tv.percent_of_day - 0.5
                 - t->config.timezone / 24.0;
    if (fabs(jd_ut - st->cache_jd) > 1.0 / 1440.0) {
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

    // Chart targets for the luminaire composition, every update the
    // station is anywhere in play — the movers need live endpoints
    {
        float sx, sy, mx, my;
        orr__ecl_dir(st->sun_lon, &sx, &sy);
        st->lum_sun_x = sx * DRACO_R;
        st->lum_sun_y = sy * DRACO_R;
        orr__ecl_dir(st->moon_lon, &mx, &my);
        float mr = DRACO_R + DRACO_AMP / 5.145f * (float)st->moon_lat;
        st->lum_moon_x = mx * mr;
        st->lum_moon_y = my * mr;
        float bb = (float)(globe_moon_phase(st->cache_jd) * 2.0 * M_PI);
        st->lum_light[0] = sinf(bb);
        st->lum_light[1] = 0.0f;
        st->lum_light[2] = -cosf(bb);
    }
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
    // Two different meals: SOLAR = new moon at the sun's own jaw
    // (the lune slides over the bead), LVNAR = full moon devoured at
    // the OPPOSITE jaw while the sun stands at this one
    float glow_sol = season * syz_new;
    float glow_lun = season * syz_full;
    bool  solar = glow_sol >= glow_lun;
    float glow = solar ? glow_sol : glow_lun;

    // ---- The ecliptic: the sun's road, and the zodiac around it ----
    // Deliberately fainter than the dragon — the road is reference,
    // the serpent is the actor; two identical hairlines read as "two
    // tracks"
    draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.20f));
    draw_circle_stroked(d, 0, 0, DRACO_R, 1.0f);
    for (int i = 0; i < 12; i++) {
        float dx, dy;
        orr__ecl_dir(i * 30.0, &dx, &dy);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.35f));
        draw_line(d, dx * 410.0f, dy * 410.0f,
                  dx * 422.0f, dy * 422.0f, 1.0f);
        float mx, my;
        orr__ecl_dir(i * 30.0 + 15.0, &mx, &my);
        draw_set_color(d, dca(0.55f, 0.53f, 0.49f, 0.50f));
        orr__zodiac_glyph(d, i, mx * 428.0f,
                          my * 428.0f, mx, my, 24.0f);
    }

    // ---- The dragon's body: the moon's orbit as a latitude wave ----
    // r = R + k*sin(lon - node): inside the ecliptic half the way
    // round, outside the other half, crossing AT the nodes. Two edges
    // give the serpent a body; it thins toward the crossings where
    // the head and tail take over.
    // A FILLED ribbon between two edges, in warmer ink than the road:
    // a body, not a second track.
    {
        const int N = 288;
        float pox = 0, poy = 0, pix = 0, piy = 0;
        for (int i = 0; i <= N; i++) {
            float lam = (float)i / N * 360.0f;
            float ph = (lam - (float)st->node_lon) * d2r;
            float body = 4.0f * fabsf(sinf(ph));   // girth
            float rbase = DRACO_R + DRACO_AMP * sinf(ph);
            float dx, dy;
            orr__ecl_dir(lam, &dx, &dy);
            float ox = dx * (rbase + body), oy = dy * (rbase + body);
            float ix = dx * (rbase - body), iy = dy * (rbase - body);
            if (i) {
                draw_set_color(d, dca(0.58f, 0.52f, 0.40f, 0.12f));
                int vb = d->num_verts;
                draw__push_vert(d, pox, poy, d->white_u, d->white_v);
                draw__push_vert(d, ox, oy, d->white_u, d->white_v);
                draw__push_vert(d, ix, iy, d->white_u, d->white_v);
                draw__push_vert(d, pix, piy, d->white_u, d->white_v);
                draw__tri(d, vb, vb + 1, vb + 2);
                draw__tri(d, vb, vb + 2, vb + 3);
                draw_set_color(d, dca(0.68f, 0.63f, 0.50f, 0.55f));
                draw_line(d, pox, poy, ox, oy, 1.2f);
                draw_line(d, pix, piy, ix, iy, 1.2f);
            }
            pox = ox; poy = oy; pix = ix; piy = iy;
        }
    }

    // ---- Head and tail at the crossings ----
    {
        float hx, hy, tx, ty;
        orr__ecl_dir(st->node_lon, &hx, &hy);
        orr__ecl_dir(st->node_lon + 180.0, &tx, &ty);
        float ha = 0.55f + (head_near ? 0.40f * season : 0.0f);
        float ta = 0.55f + (!head_near ? 0.40f * season : 0.0f);
        draw_set_color(d, dca(0.72f, 0.70f, 0.64f, ha));
        orr__strokes(d, draco__sg_caput, hx * DRACO_R, hy * DRACO_R,
                     hx, hy, 48.0f, 1.3f);
        draw_set_color(d, dca(0.72f, 0.70f, 0.64f, ta));
        orr__strokes(d, draco__sg_cauda, tx * DRACO_R, ty * DRACO_R,
                     tx, ty, 48.0f, 1.3f);
    }

    // ---- The travelers: sun on the road, moon on the wave ----
    // Full presence, equal size — when they meet at a jaw, the
    // occlusion IS the eclipse. The moon wears the clock face's own
    // dress: the phase-lit globe, its bright limb aimed at the sun.
    {
        float sx, sy;
        orr__ecl_dir(st->sun_lon, &sx, &sy);
        float spx = sx * DRACO_R, spy = sy * DRACO_R;
        float mx, my;
        orr__ecl_dir(st->moon_lon, &mx, &my);
        // The moon sits at its TRUE latitude, scaled by the wave's
        // own exaggeration — it rides the dragon's back exactly
        float mr = DRACO_R + DRACO_AMP / 5.145f * (float)st->moon_lat;
        float mpx = mx * mr, mpy = my * mr;

        // The feeding fire rides the MEAL, not the jaw sigil — an
        // eclipse can strike up to ~18 degrees from the crossing
        // (the ecliptic limit), so the furnace wraps the body being
        // eaten: the sun at a solar eclipse, the moon at a lunar.
        // Under the beads, blazing white at the heart.
        if (glow > 0.01f)
            draco__glow(d, solar ? spx : mpx, solar ? spy : mpy,
                        70.0f + 50.0f * glow, glow, !solar);

        // ONE OBJECT: during flights the luminaries are VIEW_LVMEN's
        // (the orrery composes toward this view's published targets);
        // this view draws its own only at EXACT coincidence — full
        // blend, same position, size, and lune light — so the handoff
        // is invisible and the furnace/umbra sandwich stays intact.
        bool own = st->blend > 0.999;

        // The sun itself heats only at ITS OWN meal — gold to
        // near-white as the solar eclipse closes (a lunar eclipse
        // dims the moon; the sun just stands there, opposite)
        if (own) {
        draw_set_color(d, dca(0.85f + 0.15f * glow_sol,
                              0.62f + 0.33f * glow_sol,
                              0.18f + 0.60f * glow_sol, 0.95f));
        draw_circle_filled(d, spx, spy, 28.0f);
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.35f));
        draw_circle_stroked(d, spx, spy, 34.0f, 1.0f);
        }

        GlobeCmd *gm = own ? draw_globe_slot(d, mpx, mpy, 28.0f) : NULL;
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
            // Observer direction for the shader's phase-legibility
            // dim: straight at the viewer, the geo aperture's "no
            // change" setting. Left unset, the shader falls back to
            // the EARTH solstice sun — a phantom second terminator
            // (Seren caught the double shadow).
            gm->aux_dir[0] = 0.0f;
            gm->aux_dir[1] = 0.0f;
            gm->aux_dir[2] = 1.0f;
            gm->aux_dir[3] = 1.0f;
            gm->land = true;
            gm->tex_id = 1;
            gm->grid_boost = 0.0f;
            gm->obs_lat = 999.0f;
            // EARTHSHINE, this station only (the other moons' fixed
            // indigo is already tuned; DRACO's big lune on bare black
            // needed its shadow side lifted), then the blood moon on
            // top: entering Earth's shadow, the disc reddens to the
            // umbral copper of refracted sunset light
            float es = 0.5f * (1.0f + cosf(bb));
            float nr = 0.10f + 0.17f * es;
            float ng = 0.105f + 0.175f * es;
            float nb2 = 0.23f + 0.12f * es;
            float bl = glow_lun;
            gm->day_col[0] = 0.58f - 0.08f * bl;
            gm->day_col[1] = 0.55f - 0.33f * bl;
            gm->day_col[2] = 0.49f - 0.38f * bl;
            gm->day_col[3] = 1.0f;
            gm->night_col[0] = nr + (0.42f - nr) * bl;
            gm->night_col[1] = ng + (0.14f - ng) * bl;
            gm->night_col[2] = nb2 + (0.07f - nb2) * bl;
            gm->night_col[3] = 1.0f;
        }

        // ---- The umbra itself, crossing the moon ----
        // Earth's shadow hangs at the exact anti-solar point on the
        // road, ~2.7 moon-widths wide — and it is INVISIBLE, a dark
        // disc on a dark sky, until the full moon sails into it. Then
        // the bite crosses the disc: entry, totality, exit, a few
        // hours by the true clock. Soft penumbral rim; drawn over
        // the lune (the globe pass renders beneath later pushes).
        if (glow_lun > 0.005f) {
            float ax, ay;
            orr__ecl_dir(st->sun_lon + 180.0, &ax, &ay);
            float ucx = ax * DRACO_R, ucy = ay * DRACO_R;
            const int SEG = 40;
            float r_umb = 74.0f;
            float uf = glow_lun * 6.0f;
            if (uf > 1.0f) uf = 1.0f;
            // Dark but not opaque: a totally eclipsed moon stays
            // VISIBLE as the blood disc through the shadow — the
            // umbra darkens what it covers, it doesn't erase it
            static const float ush_r[3] = { 0.0f, 0.70f, 1.0f };
            static const float ush_a[3] = { 0.62f, 0.55f, 0.0f };
            int base[3];
            for (int sh = 0; sh < 3; sh++) {
                draw_set_color(d, dca(0.05f, 0.012f, 0.008f,
                                      ush_a[sh] * uf));
                if (sh == 0) {
                    base[0] = draw__push_vert(d, ucx, ucy,
                                              d->white_u, d->white_v);
                } else {
                    base[sh] = d->num_verts;
                    for (int i = 0; i < SEG; i++) {
                        float a = (float)i / SEG * 2.0f * (float)M_PI;
                        draw__push_vert(d,
                            ucx + cosf(a) * r_umb * ush_r[sh],
                            ucy + sinf(a) * r_umb * ush_r[sh],
                            d->white_u, d->white_v);
                    }
                }
            }
            for (int i = 0; i < SEG; i++) {
                int j = (i + 1) % SEG;
                draw__tri(d, base[0], base[1] + i, base[1] + j);
                draw__tri(d, base[1] + i, base[2] + i, base[2] + j);
                draw__tri(d, base[1] + i, base[2] + j, base[1] + j);
            }
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
            // The season names its meal once a syzygy closes in:
            // solis (the sun eaten at its jaw) or lunae (the full
            // moon at the opposite one); before that, just "a time
            // of eclipses"
            draw_text_centered(d, FONT_date, 0, 48.0f,
                               glow > 0.10f
                                   ? (solar ? "ECLIPSIS SOLIS"
                                            : "ECLIPSIS LVNAE")
                                   : "TEMPVS ECLIPSIVM");
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
