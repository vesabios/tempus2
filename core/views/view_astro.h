// view_astro.h — ASTROLABIVM: the planispheric astrolabe.
//
// The medieval computer: the sky stereographically flattened onto the
// equatorial plane (projected from the south celestial pole, so the
// whole visible heaven fits inside the tropic of Capricorn's limb).
// The TYMPAN is the fixed plate engraved for YOUR latitude — horizon,
// almucantars, twilight line. The RETE is the pierced star map that
// turns over it with the sidereal day, carrying the ecliptic ring and
// the named stars' pointers. The RULE lies across the sun's place,
// telling the hour. Every brass astrolabe froze one latitude per
// plate; this one regenerates the tympan live when you choose a new
// home at ORBIS — the plate no smith could file.
//
// Station-private projection (docs/TRANSITIONS.md): evaluate-only,
// never tweened between. The view is self-contained chrome in v1;
// the luminaries' member integration can follow.

#ifndef VIEW_ASTRO_H
#define VIEW_ASTRO_H

#include "../view.h"
#include "../planets.h"

struct AstroViewState {
    TimeView tv;  // must be first field
    double opacity;
    double blend;     // mirrored scene astro_blend
};

#endif // VIEW_ASTRO_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_ASTRO_IMPL)
#define VIEW_ASTRO_IMPL

// The limb: the tropic of Capricorn bounds the plate
#define ASTRO_R_CAP 400.0f
#define ASTRO_OBL   23.436f

// Equatorial radius from the limb: r(dec) = Req * tan((90 - dec)/2)
static inline float astro__req(void) {
    return ASTRO_R_CAP
         / tanf((90.0f + ASTRO_OBL) * 0.5f * (float)M_PI / 180.0f);
}

// Project (dec, hour angle H): the meridian stands vertical, the
// upper culmination (H = 0) toward the TOP of the plate — the
// classical face, south up in the northern signs.
static inline bool astro__project(float dec_deg, float ha_deg,
                                  float *x, float *y) {
    float r = astro__req()
            * tanf((90.0f - dec_deg) * 0.5f * (float)M_PI / 180.0f);
    if (r > ASTRO_R_CAP * 1.35f) return false;   // deep south: off plate
    float h = ha_deg * (float)M_PI / 180.0f;
    *x = r * sinf(h);
    *y = -r * cosf(h);
    return true;
}

// (altitude, azimuth) at latitude phi -> (dec, H), then project.
// The almucantar sampler: the tympan's whole geometry comes through
// here, so the plate is correct at ANY latitude by construction.
static inline bool astro__project_altaz(float alt_deg, float az_deg,
                                        float lat_deg,
                                        float *x, float *y) {
    float d2r = (float)M_PI / 180.0f;
    float sa = sinf(alt_deg * d2r), ca = cosf(alt_deg * d2r);
    float sp = sinf(lat_deg * d2r), cp = cosf(lat_deg * d2r);
    float cA = cosf(az_deg * d2r), sA = sinf(az_deg * d2r);
    float sd = sp * sa + cp * ca * cA;
    if (sd > 1) sd = 1;
    if (sd < -1) sd = -1;
    float dec = asinf(sd) / d2r;
    // hour angle: sin H = -sin A cos a / cos dec; cos H from the alt
    float cd = cosf(asinf(sd));
    float H;
    if (cd < 1.0e-5f) H = 0;
    else H = atan2f(-sA * ca / cd, (sa - sp * sd) / (cp * cd)) / d2r;
    return astro__project(dec, H, x, y);
}

// The named stars of the rete — the classical pointer set (J2000)
typedef struct { const char *name; float ra, dec; } AstroStar;
static const AstroStar astro__stars[] = {
    { "SIRIVS",     101.287f, -16.716f },
    { "PROCYON",    114.825f,   5.225f },
    { "REGVLVS",    152.093f,  11.967f },
    { "SPICA",      201.298f, -11.161f },
    { "ARCTVRVS",   213.915f,  19.182f },
    { "ANTARES",    247.352f, -26.432f },
    { "VEGA",       279.235f,  38.784f },
    { "ALTAIR",     297.696f,   8.868f },
    { "DENEB",      310.358f,  45.280f },
    { "FOMALHAVT",  344.413f, -29.622f },
    { "ALDEBARAN",   68.980f,  16.509f },
    { "RIGEL",       78.634f,  -8.202f },
    { "CAPELLA",     79.172f,  45.998f },
    { "BETELGEVSE",  88.793f,   7.407f },
    { "POLLVX",     116.329f,  28.026f },
};
#define ASTRO_NSTARS (int)(sizeof(astro__stars) / sizeof(astro__stars[0]))

static void astro_init(void *buf, const Tempus *t, const RenderStyle *s) {
    AstroViewState *st = (AstroViewState *)buf;
    st->opacity = 1.0;
    (void)t; (void)s;
}

static void astro_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void astro_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void astro_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    AstroViewState *st = (AstroViewState *)buf;
    (void)t; (void)dt;
    st->blend = sc->astro_blend;
}

static void astro_render(const void *buf, DrawCtx *d, const Tempus *t,
                         const RenderStyle *s) {
    const AstroViewState *st = (const AstroViewState *)buf;
    const TimeView *tv = &st->tv;
    float base_alpha = d->alpha;
    (void)s;
    if (base_alpha < 0.004f) return;

    float d2r = (float)M_PI / 180.0f;
    float lat = (float)t->config.latitude;
    float sp = sinf(lat * d2r), cp = cosf(lat * d2r);
    float Req = astro__req();
    // Local sidereal time, degrees — the rete's whole rotation
    double jd_ut = tv->jd_current + tv->percent_of_day - 0.5
                 - t->config.timezone / 24.0;
    float lst = (float)fmod(planets__gmst(jd_ut) + t->config.longitude,
                            360.0);

    // The sun first — its altitude keys the whole plate's light
    float sun_dec, sun_ha, sun_alt, sx, sy;
    bool sun_on;
    {
        float eps = ASTRO_OBL * d2r;
        double slon, slat;
        planets__body_lonlat(BODY_SUN, jd_ut, &slon, &slat);
        float lam = (float)slon * d2r;
        sun_dec = asinf(sinf(eps) * sinf(lam)) / d2r;
        float ra = atan2f(cosf(eps) * sinf(lam), cosf(lam)) / d2r;
        sun_ha = lst - ra;
        sun_alt = asinf(sp * sinf(sun_dec * d2r)
                        + cp * cosf(sun_dec * d2r)
                          * cosf(sun_ha * d2r)) / d2r;
        sun_on = astro__project(sun_dec, sun_ha, &sx, &sy);
    }
    // Day / twilight / night, from the sun's true altitude
    float dayk = (float)tempus_smoothstep(-12.0, 6.0, sun_alt);

    // ---- The sky itself: the region above YOUR horizon ----
    // The one shape that says what the instrument is: everything
    // inside this wash is UP right now; everything outside is under
    // the earth. Filled as a fan about the zenith, the horizon
    // clamped to the limb where it runs off the plate (exactly the
    // limb arc — the clamp radius IS the limb).
    {
        float zx, zy;
        astro__project(lat, 0, &zx, &zy);
        DrawColor sky = dca(0.045f + 0.075f * dayk,
                            0.055f + 0.085f * dayk,
                            0.085f + 0.095f * dayk, 1.0f);
        draw_set_color(d, sky);
        d->alpha = base_alpha * 0.9f;
        float px = 0, py = 0;
        for (int i = 0; i <= 144; i++) {
            float az = (float)i / 144.0f * 360.0f;
            float x, y;
            astro__project_altaz(0.0f, az, lat, &x, &y);
            float r = sqrtf(x * x + y * y);
            if (r > ASTRO_R_CAP) {
                x *= ASTRO_R_CAP / r;
                y *= ASTRO_R_CAP / r;
            }
            if (i > 0) {
                int vb = d->num_verts;
                draw__push_vert(d, zx, zy, d->white_u, d->white_v);
                draw__push_vert(d, px, py, d->white_u, d->white_v);
                draw__push_vert(d, x, y, d->white_u, d->white_v);
                draw__tri(d, vb, vb + 1, vb + 2);
            }
            px = x; py = y;
        }
        d->alpha = base_alpha;
    }

    // ---- The plate: limb, equator, tropics ----
    draw_set_color(d, dca(0.50f, 0.48f, 0.44f, 0.75f));
    d->alpha = base_alpha * 0.9f;
    draw_circle_stroked(d, 0, 0, ASTRO_R_CAP, 1.6f);
    d->alpha = base_alpha * 0.20f;
    draw_circle_stroked(d, 0, 0, Req, 1.0f);
    draw_circle_stroked(d, 0, 0,
        Req * tanf((90.0f - ASTRO_OBL) * 0.5f * d2r), 1.0f);
    for (int i = 0; i < 72; i++) {
        float a = (float)i * 5.0f * d2r;
        float ux = sinf(a), uy = -cosf(a);
        bool major = (i % 3) == 0;
        d->alpha = base_alpha * (major ? 0.50f : 0.22f);
        draw_line(d, ux * ASTRO_R_CAP, uy * ASTRO_R_CAP,
                  ux * (ASTRO_R_CAP - (major ? 12.0f : 6.0f)),
                  uy * (ASTRO_R_CAP - (major ? 12.0f : 6.0f)), 1.0f);
    }

    // ---- The tympan: your latitude, engraved live ----
    // The horizon is the instrument's one loud line — the boundary of
    // the observable. Almucantars recede; the twilights are dashed
    // whispers under the night side.
    {
        float alts[11] = { 10, 20, 30, 40, 50, 60, 70, 80, -18, -6, 0 };
        for (int L = 0; L < 11; L++) {
            float alt = alts[L];
            bool horizon = (alt == 0);
            bool twil = (alt < 0);
            float la = horizon ? 0.85f
                     : (twil ? 0.26f * (1.0f - dayk) + 0.06f : 0.15f);
            draw_set_color(d, twil
                ? dca(0.35f, 0.42f, 0.52f, 0.8f)
                : (horizon ? dca(0.66f, 0.63f, 0.55f, 0.9f)
                           : dca(0.55f, 0.53f, 0.49f, 0.8f)));
            float px = 0, py = 0;
            bool pv = false;
            for (int i = 0; i <= 144; i++) {
                float az = (float)i / 144.0f * 360.0f;
                float x, y;
                bool v = astro__project_altaz(alt, az, lat, &x, &y)
                      && x * x + y * y
                         <= ASTRO_R_CAP * ASTRO_R_CAP;
                if (v && pv && !(twil && (i & 2))) {
                    d->alpha = base_alpha * la;
                    draw_line(d, px, py, x, y, horizon ? 1.6f : 1.0f);
                }
                px = x; py = y; pv = v;
            }
        }
        // Zenith: the point straight up, small and certain
        float zx, zy;
        if (astro__project(lat, 0, &zx, &zy)) {
            d->alpha = base_alpha * 0.85f;
            draw_set_color(d, dca(0.66f, 0.63f, 0.57f, 0.9f));
            draw_circle_stroked(d, zx, zy, 3.0f, 1.2f);
        }
        // Cardinals ON the horizon: where east and west truly lie on
        // the plate (east LEFT — the sky seen from beneath), and the
        // north point. They teach the orientation in three words.
        int cw = _font_compat[FONT_date].weight;
        const struct { float az; const char *name; } card[3] = {
            { 90.0f, "ORIENS" }, { 270.0f, "OCCIDENS" },
            { 0.0f, "SEPTENTRIO" },
        };
        for (int i = 0; i < 3; i++) {
            float x, y;
            if (!astro__project_altaz(0.0f, card[i].az, lat, &x, &y))
                continue;
            if (x * x + y * y > ASTRO_R_CAP * ASTRO_R_CAP) continue;
            float tw = sdf_measure_width(cw, card[i].name) * 11.0f;
            d->alpha = base_alpha * 0.5f;
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.8f));
            draw_text_ex(d, cw, 11.0f, x - tw * 0.5f, y + 8.0f,
                         card[i].name);
        }
    }

    // ---- The rete: the zodiac ring and the star pointers ----
    // Hour angle LST - RA is the turning; no matrix anywhere. The
    // ring wears its twelve sigils — it IS the wheel's zodiac, bent
    // onto the sky and turning with it.
    {
        float eps = ASTRO_OBL * d2r;
        draw_set_color(d, dca(0.70f, 0.54f, 0.24f, 0.85f));
        float px = 0, py = 0;
        bool pv = false;
        for (int i = 0; i <= 180; i++) {
            float lam = (float)i / 180.0f * 360.0f * d2r;
            float dec = asinf(sinf(eps) * sinf(lam));
            float ra = atan2f(cosf(eps) * sinf(lam), cosf(lam)) / d2r;
            float x, y;
            bool v = astro__project(dec / d2r, lst - ra, &x, &y);
            if (v && pv) {
                d->alpha = base_alpha * 0.50f;
                draw_line(d, px, py, x, y, 1.3f);
            }
            px = x; py = y; pv = v;
        }
        // The sigils, every 30 degrees of the ecliptic, turning with
        // the rete; feet toward the plate center
        for (int sign = 0; sign < 12; sign++) {
            float lam = ((float)sign * 30.0f + 15.0f) * d2r;
            float dec = asinf(sinf(eps) * sinf(lam));
            float ra = atan2f(cosf(eps) * sinf(lam), cosf(lam)) / d2r;
            float x, y;
            if (!astro__project(dec / d2r, lst - ra, &x, &y)) continue;
            if (x * x + y * y > ASTRO_R_CAP * ASTRO_R_CAP) continue;
            float rn = sqrtf(x * x + y * y);
            if (rn < 1) continue;
            float ux = x / rn, uy = y / rn;
            d->alpha = base_alpha * 0.55f;
            draw_set_color(d, dca(0.70f, 0.54f, 0.24f, 0.85f));
            orr__zodiac_glyph(d, sign, x + ux * 13.0f, y + uy * 13.0f,
                              ux, uy, 13.0f);
        }
        // Star pointers: risen stars burn with their names; set stars
        // are ghosts — the plate always says WHICH sky you own now
        int fw = _font_compat[FONT_date].weight;
        for (int i = 0; i < ASTRO_NSTARS; i++) {
            float ha = lst - astro__stars[i].ra;
            float sd = sinf(astro__stars[i].dec * d2r);
            float cd = cosf(astro__stars[i].dec * d2r);
            float alt = asinf(sp * sd + cp * cd * cosf(ha * d2r)) / d2r;
            float x, y;
            if (!astro__project(astro__stars[i].dec, ha, &x, &y))
                continue;
            if (x * x + y * y > ASTRO_R_CAP * ASTRO_R_CAP) continue;
            bool up = alt > 0.0f;
            float rn = sqrtf(x * x + y * y);
            float ux = rn > 1 ? x / rn : 0, uy = rn > 1 ? y / rn : -1;
            if (up) {
                d->alpha = base_alpha;
                draw_set_color(d, dca(0.86f, 0.82f, 0.70f, 1.0f));
                draw_circle_filled(d, x, y, 2.6f);
                d->alpha = base_alpha * 0.6f;
                draw_line(d, x, y, x - ux * 16.0f, y - uy * 16.0f, 1.1f);
                d->alpha = base_alpha * 0.55f;
                draw_set_color(d, dca(0.66f, 0.63f, 0.57f, 0.85f));
                draw_text_ex(d, fw, 11.5f, x + 6.0f, y + 4.0f,
                             astro__stars[i].name);
            } else {
                d->alpha = base_alpha * 0.28f;
                draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.7f));
                draw_circle_stroked(d, x, y, 2.2f, 1.0f);
            }
        }
    }

    // ---- The rule and the sun: the observable state, stated ----
    // A full diameter through the sun's place, as on the brass. Risen,
    // the sun is gold and rayed and its altitude is written at the
    // limb; set, it is a hollow ember under the earth and the rule
    // waits with it.
    if (sun_on) {
        bool up = sun_alt > 0.0f;
        float rn = sqrtf(sx * sx + sy * sy);
        float ux = rn > 1 ? sx / rn : 0, uy = rn > 1 ? sy / rn : -1;
        d->alpha = base_alpha * (up ? 0.70f : 0.30f);
        draw_set_color(d, dca(0.72f, 0.55f, 0.25f, 0.9f));
        draw_line(d, ux * ASTRO_R_CAP, uy * ASTRO_R_CAP,
                  -ux * ASTRO_R_CAP, -uy * ASTRO_R_CAP, 1.2f);
        d->alpha = base_alpha * (up ? 1.0f : 0.45f);
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 1.0f));
        if (up) {
            draw_circle_filled(d, sx, sy, 8.0f);
            for (int i = 0; i < 8; i++) {
                float a = ((float)i / 8.0f) * 2.0f * (float)M_PI;
                float rx2 = sinf(a), ry2 = -cosf(a);
                d->alpha = base_alpha * 0.7f;
                draw_line(d, sx + rx2 * 11.0f, sy + ry2 * 11.0f,
                          sx + rx2 * (11.0f + ((i & 1) ? 4.0f : 7.0f)),
                          sy + ry2 * (11.0f + ((i & 1) ? 4.0f : 7.0f)),
                          1.1f);
            }
        } else {
            draw_circle_stroked(d, sx, sy, 8.0f, 1.5f);
        }
        // The altitude, written where the rule meets the limb
        {
            char alt_s[24];
            snprintf(alt_s, sizeof alt_s, "SOL %+d\xc2\xb0",
                     (int)lroundf(sun_alt));
            int fw = _font_compat[FONT_date].weight;
            float tw = sdf_measure_width(fw, alt_s) * 12.0f;
            d->alpha = base_alpha * (up ? 0.75f : 0.4f);
            draw_set_color(d, dca(0.72f, 0.55f, 0.25f, 0.95f));
            draw_text_ex(d, fw, 12.0f,
                         ux * (ASTRO_R_CAP + 20.0f) - tw * 0.5f,
                         uy * (ASTRO_R_CAP + 20.0f) - 5.0f, alt_s);
        }
    }
    // The moon, phase-true in brightness terms: an open ring, brighter
    // when risen
    {
        float eps = ASTRO_OBL * d2r;
        double mlon, mlat2;
        planets__body_lonlat(BODY_MOON, jd_ut, &mlon, &mlat2);
        float lm = (float)mlon * d2r;
        float bm = (float)mlat2 * d2r;
        float sdm = sinf(bm) * cosf(eps)
                  + cosf(bm) * sinf(eps) * sinf(lm);
        float decm = asinf(sdm) / d2r;
        float ram = atan2f(sinf(lm) * cosf(eps)
                           - tanf(bm) * sinf(eps), cosf(lm)) / d2r;
        float ham = lst - ram;
        float altm = asinf(sp * sinf(decm * d2r)
                           + cp * cosf(decm * d2r)
                             * cosf(ham * d2r)) / d2r;
        float mx, my;
        if (astro__project(decm, ham, &mx, &my)
            && mx * mx + my * my <= ASTRO_R_CAP * ASTRO_R_CAP) {
            d->alpha = base_alpha * (altm > 0 ? 0.95f : 0.35f);
            draw_set_color(d, dca(0.84f, 0.83f, 0.80f, 0.95f));
            draw_circle_stroked(d, mx, my, 5.5f, 1.5f);
            if (altm > 0) {
                int fw = _font_compat[FONT_date].weight;
                d->alpha = base_alpha * 0.5f;
                draw_set_color(d, dca(0.66f, 0.63f, 0.57f, 0.85f));
                draw_text_ex(d, fw, 11.0f, mx + 8.0f, my + 4.0f,
                             "LVNA");
            }
        }
    }

    // ---- The name ----
    d->alpha = base_alpha * 0.5f;
    draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.8f));
    draw_text_centered(d, FONT_date, 0, ASTRO_R_CAP + 44.0f,
                       "ASTROLABIVM");
    d->alpha = base_alpha;
}

static const ViewVtable astro_vtable = {
    .init   = astro_init,
    .enter  = astro_enter,
    .exit   = astro_exit,
    .update = astro_update,
    .render = astro_render,
};

#endif // VIEW_ASTRO_IMPL
