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
    float Req = astro__req();
    // Local sidereal time, degrees — the rete's whole rotation
    double jd_ut = tv->jd_current + tv->percent_of_day - 0.5
                 - t->config.timezone / 24.0;
    float lst = (float)fmod(planets__gmst(jd_ut) + t->config.longitude,
                            360.0);

    // ---- The plate: limb, equator, tropics ----
    draw_set_color(d, dca(0.50f, 0.48f, 0.44f, 0.75f));
    d->alpha = base_alpha * 0.9f;
    draw_circle_stroked(d, 0, 0, ASTRO_R_CAP, 1.6f);
    d->alpha = base_alpha * 0.35f;
    draw_circle_stroked(d, 0, 0, Req, 1.0f);
    draw_circle_stroked(d, 0, 0,
        Req * tanf((90.0f - ASTRO_OBL) * 0.5f * d2r), 1.0f);
    // Degree ticks on the limb, every 5, majors at 15
    for (int i = 0; i < 72; i++) {
        float a = (float)i * 5.0f * d2r;
        float ux = sinf(a), uy = -cosf(a);
        bool major = (i % 3) == 0;
        d->alpha = base_alpha * (major ? 0.55f : 0.28f);
        draw_line(d, ux * ASTRO_R_CAP, uy * ASTRO_R_CAP,
                  ux * (ASTRO_R_CAP - (major ? 12.0f : 6.0f)),
                  uy * (ASTRO_R_CAP - (major ? 12.0f : 6.0f)), 1.0f);
    }

    // ---- The tympan: your latitude, engraved live ----
    // Horizon (bright), almucantars every 10 degrees (faint), the
    // astronomical twilight line (dashed by segment skipping), and
    // the zenith point. Sampled through the projection — correct at
    // any latitude, including the ones brass never served.
    {
        float alts[11] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, -18, -6 };
        for (int L = 0; L < 11; L++) {
            float alt = alts[L];
            bool horizon = (L == 0);
            bool twil = (alt < 0);
            float la = horizon ? 0.70f : (twil ? 0.30f : 0.22f);
            draw_set_color(d, twil
                ? dca(0.35f, 0.42f, 0.52f, 0.8f)
                : dca(0.55f, 0.53f, 0.49f, 0.8f));
            float px = 0, py = 0;
            bool pv = false;
            for (int i = 0; i <= 144; i++) {
                float az = (float)i / 144.0f * 360.0f;
                float x, y;
                bool v = astro__project_altaz(alt, az, lat, &x, &y)
                      && x * x + y * y
                         <= ASTRO_R_CAP * ASTRO_R_CAP * 1.0f;
                if (v && pv && !(twil && (i & 2))) {
                    d->alpha = base_alpha * la;
                    draw_line(d, px, py, x, y, horizon ? 1.4f : 1.0f);
                }
                px = x; py = y; pv = v;
            }
        }
        float zx, zy;
        if (astro__project(lat, 0, &zx, &zy)) {
            d->alpha = base_alpha * 0.8f;
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.9f));
            draw_circle_stroked(d, zx, zy, 3.0f, 1.0f);
        }
    }

    // ---- The rete: the ecliptic ring and the star pointers ----
    // No rotation matrix anywhere: each point's hour angle LST - RA
    // IS the turning of the rete.
    {
        draw_set_color(d, dca(0.70f, 0.54f, 0.24f, 0.85f));
        float eps = ASTRO_OBL * d2r;
        float px = 0, py = 0;
        bool pv = false;
        for (int i = 0; i <= 180; i++) {
            float lam = (float)i / 180.0f * 360.0f * d2r;
            float dec = asinf(sinf(eps) * sinf(lam));
            float ra = atan2f(cosf(eps) * sinf(lam), cosf(lam)) / d2r;
            float x, y;
            bool v = astro__project(dec / d2r, lst - ra, &x, &y);
            if (v && pv) {
                d->alpha = base_alpha * 0.55f;
                draw_line(d, px, py, x, y, 1.3f);
            }
            px = x; py = y; pv = v;
        }
        // Star pointers: a dot and a short flame aimed at the pole
        int fw = _font_compat[FONT_date].weight;
        for (int i = 0; i < ASTRO_NSTARS; i++) {
            float x, y;
            if (!astro__project(astro__stars[i].dec,
                                lst - astro__stars[i].ra, &x, &y))
                continue;
            if (x * x + y * y > ASTRO_R_CAP * ASTRO_R_CAP) continue;
            float rn = sqrtf(x * x + y * y);
            float ux = rn > 1 ? x / rn : 0, uy = rn > 1 ? y / rn : -1;
            d->alpha = base_alpha * 0.9f;
            draw_set_color(d, dca(0.82f, 0.78f, 0.66f, 0.95f));
            draw_circle_filled(d, x, y, 2.2f);
            d->alpha = base_alpha * 0.5f;
            draw_line(d, x, y, x - ux * 14.0f, y - uy * 14.0f, 1.0f);
            d->alpha = base_alpha * 0.40f;
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.8f));
            draw_text_ex(d, fw, 11.0f, x + 5.0f, y + 4.0f,
                         astro__stars[i].name);
        }
    }

    // ---- The rule and the sun ----
    // The sun's ecliptic place, put on the rete's ring; the rule laid
    // radially through it — the hour, told the old way.
    {
        float eps = ASTRO_OBL * d2r;
        double slon, slat;
        planets__body_lonlat(BODY_SUN, jd_ut, &slon, &slat);
        float lam = (float)slon * d2r;
        float dec = asinf(sinf(eps) * sinf(lam));
        float ra = atan2f(cosf(eps) * sinf(lam), cosf(lam)) / d2r;
        float x, y;
        if (astro__project(dec / d2r, lst - ra, &x, &y)) {
            float rn = sqrtf(x * x + y * y);
            if (rn > 1.0f) {
                float ux = x / rn, uy = y / rn;
                d->alpha = base_alpha * 0.65f;
                draw_set_color(d, dca(0.72f, 0.55f, 0.25f, 0.9f));
                draw_line(d, 0, 0, ux * ASTRO_R_CAP, uy * ASTRO_R_CAP,
                          1.2f);
            }
            d->alpha = base_alpha;
            draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 1.0f));
            draw_circle_filled(d, x, y, 7.0f);
        }
        // The moon, from the live ephemeris, with its own small mark
        double mlon, mlat2;
        planets__body_lonlat(BODY_MOON, jd_ut, &mlon, &mlat2);
        float lm = (float)mlon * d2r;
        float bm = (float)mlat2 * d2r;
        float sdm = sinf(bm) * cosf(eps)
                  + cosf(bm) * sinf(eps) * sinf(lm);
        float decm = asinf(sdm);
        float ram = atan2f(sinf(lm) * cosf(eps)
                           - tanf(bm) * sinf(eps), cosf(lm)) / d2r;
        float mx, my;
        if (astro__project(decm / d2r, lst - ram, &mx, &my)
            && mx * mx + my * my
               <= ASTRO_R_CAP * ASTRO_R_CAP) {
            d->alpha = base_alpha * 0.95f;
            draw_set_color(d, dca(0.84f, 0.83f, 0.80f, 0.95f));
            draw_circle_stroked(d, mx, my, 5.0f, 1.4f);
        }
    }

    // ---- The name ----
    d->alpha = base_alpha * 0.5f;
    draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.8f));
    draw_text_centered(d, FONT_date, 0, ASTRO_R_CAP + 26.0f,
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
