// view_orrery.h — The earth instrument, at every worldview.
//
// This view is the SINGLE renderer for the globe and everything attached
// to it, across the whole geocentric <-> heliocentric morph (including
// both pure endpoints). The solar view computes data (SPA, caches) but
// draws nothing — single ownership means draw order is decided once,
// here, and handoff artifacts are structurally impossible.
//
// Element groups:
//   dial group  (alpha 1-m): dial plate, outer ring, daylight arc —
//                            anchored at the dial position, engraved panel
//   helio group (alpha m):   sun, orbit radial, axis pin, 24h bezel
//   shared      (alpha 1):   globe, sun marker + hand line, city marker,
//                            sky-dome — continuous across the morph by
//                            construction (their geo and helio forms
//                            coincide at m = 0)

#ifndef VIEW_ORRERY_H
#define VIEW_ORRERY_H

#include "../view.h"
#include "../../assets/coastlines.h"

struct OrreryViewState {
    TimeView tv;  // must be first field
    double opacity;
    double zoom;        // mirrored from the calendar wheel's zoom
    double blend;       // mirrored scene helio_blend (morph parameter)
    double geo_azimuth; // live sun az/zen from the solar view, for the
    double geo_zenith;  // geocentric endpoint of the morph
    const SolarViewState *solar;  // solar data + sun-path caches

    // Published by render for hit-testing: the sun bead is draggable in
    // helio mode (rotating the sun's direction = choosing the date)
    float bead_x, bead_y;   // bead position, world coords
    float bead_hit;         // hit radius
    float glob_x, glob_y;   // globe center, world coords
    float glob_r;           // globe radius (wheel drags exclude the globe)
    bool  dragging;

    // Coastline unit vectors (earth frame), precomputed at init
    float coast_vec[COAST_NUM_PTS][3];
};

#endif // VIEW_ORRERY_H

// ---- Function implementations (need complete Scene type) ----
#if defined(SCENE_DEFINED) && !defined(VIEW_ORRERY_IMPL)
#define VIEW_ORRERY_IMPL

static void orrery_init(void *buf, const Tempus *t, const RenderStyle *s) {
    OrreryViewState *st = (OrreryViewState *)buf;
    st->opacity = 1.0;
    for (int i = 0; i < COAST_NUM_PTS; i++) {
        double lat = coast_pts[i][0] * 0.01 * M_PI / 180.0;
        double lon = coast_pts[i][1] * 0.01 * M_PI / 180.0;
        st->coast_vec[i][0] = (float)(cos(lat) * cos(lon));
        st->coast_vec[i][1] = (float)(cos(lat) * sin(lon));
        st->coast_vec[i][2] = (float)(sin(lat));
    }
    (void)t; (void)s;
}

static void orrery_enter(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void orrery_exit(void *buf, const Tempus *t, Scene *sc) {
    (void)buf; (void)t; (void)sc;
}

static void orrery_update(void *buf, const Tempus *t, double dt, Scene *sc) {
    OrreryViewState *st = (OrreryViewState *)buf;
    (void)t; (void)dt;
    st->zoom = sc->calendar_state.zoom;   // ride the wheel's zoom
    st->blend = sc->helio_blend;
    st->geo_azimuth = sc->solar_state.azimuth;
    st->geo_zenith = sc->solar_state.zenith;
    st->solar = &sc->solar_state;
}

static void orrery_render(const void *buf, DrawCtx *d, const Tempus *t,
                          const RenderStyle *s) {
    const OrreryViewState *st = (const OrreryViewState *)buf;
    const TimeView *tv = &st->tv;
    float base_alpha = d->alpha;

    float m = (float)st->blend;
    if (m < 0) m = 0;
    if (m > 1) m = 1;
    float geo_a = 1.0f - m;
    float helio_a = m;

    float dial_y = s->sunrise_dial_offset;
    float dial_r = s->sunrise_dial_radius;

    // Wheel geometry (zoom rides the same tween as the morph)
    float z = (float)st->zoom;
    float wheel_r = s->calendar_base_radius + z * s->zoom_in_radius;
    double phi = tempus_year_pct(t) * 2.0 * M_PI;
    float sphi = sinf((float)phi), cphi = cosf((float)phi);
    float off_r = wheel_r - s->calendar_base_radius;
    float sun_x = -sphi * off_r, sun_y = cphi * off_r;

    // ---- Morph state: position/size lerp to the FINAL helio arrangement
    // (zoom = 1: centered, full size — the zoom tween lands together with
    // m, so the endpoints match), camera slerp, and the sun direction
    // carried in the EARTH frame so the terminator stays glued.
    double clock_hours = tv->percent_of_day * 24.0;
    float helio_rot[16], helio_light[3];
    globe_orrery(phi, clock_hours, t->config.timezone, helio_rot, helio_light);

    float rot[16], light[3];
    float ex, ey, earth_r;
    if (m >= 0.999f) {
        memcpy(rot, helio_rot, sizeof(rot));
        memcpy(light, helio_light, sizeof(light));
        ex = sphi * s->calendar_base_radius * (1.0f - z);
        ey = -cphi * s->calendar_base_radius * (1.0f - z);
        earth_r = 42.0f + z * 198.0f;   // full-zoom helio size: 240
    } else {
        float geo_rot[16], geo_light[3];
        globe_rotation(t->config.latitude, t->config.longitude, geo_rot);
        globe_sun_dir(st->geo_azimuth, st->geo_zenith, geo_light);

        ex = 0.0f;
        ey = dial_y * (1 - m);
        earth_r = dial_r * (1 - m) + 240.0f * m;

        globe_rot_slerp(geo_rot, helio_rot, m, rot);
        float le_g[3], le_h[3], le[3];
        globe_mat_tmul_vec(geo_rot, geo_light, le_g);
        globe_mat_tmul_vec(helio_rot, helio_light, le_h);
        globe_vec_nlerp(le_g, le_h, m, le);
        globe_mat_mul_vec(rot, le, light);
    }

    // ================= UNDER THE GLOBE =================

    // Helio underlay: sun at wheel center + orbit radial to the globe
    if (helio_a > 0.001f) {
        d->alpha = base_alpha * helio_a;
        draw_set_color(d, dc_u8(196, 126, 16));
        draw_circle_filled(d, sun_x, sun_y, 22.0f);
        draw_set_color(d, dca(0.77f, 0.49f, 0.06f, 0.35f));
        draw_circle_stroked(d, sun_x, sun_y, 28.0f, 1.0f);
        draw_set_color(d, dca(0.5f, 0.5f, 0.5f, 0.18f));
        draw_line_thin(d, sun_x, sun_y, ex, ey);
    }

    // Dial plate: the engraved panel behind the geocentric globe
    if (geo_a > 0.001f) {
        d->alpha = base_alpha * geo_a;
        draw_set_color(d, s->clear);
        draw_circle_filled(d, 0, dial_y, dial_r + 14.0f);
    }

    // ================= THE GLOBE =================

    d->alpha = base_alpha;
    GlobeCmd *g = draw_globe_slot(d, ex, ey, earth_r);
    if (g) {
        memcpy(g->rot, rot, sizeof(rot));
        memcpy(g->light, light, sizeof(light));
        // Shader overlays (chrono/daylight/envelope) are frame-independent;
        // sun-paths renders as the sky-dome below.
        g->overlay = (s->globe_overlay == GLOBE_OVERLAY_SUNPATHS)
                   ? GLOBE_OVERLAY_NONE : s->globe_overlay;
        g->declination = (float)(-cos(phi) * GLOBE_OBLIQUITY_DEG);
        g->obs_lat = (float)t->config.latitude;
        // With coastlines carrying the geography, the plain graticule is
        // noise at orrery scale — fade it out as the morph completes
        g->grid_boost = 1.0f - m;
    }

    // Coastlines: Natural Earth outlines on the front hemisphere only,
    // rotating with the planet's spin. Subtle at dial size, present at
    // orrery scale.
    {
        float coast_a = 0.16f + 0.34f * m;
        draw_set_color(d, dca(0.63f, 0.58f, 0.50f, coast_a));
        for (int li = 0; li < COAST_NUM_LINES; li++) {
            int start = coast_lines[li][0], count = coast_lines[li][1];
            float prev[3];
            bool has_prev = false;
            for (int i = start; i < start + count; i++) {
                float v[3];
                globe_mat_mul_vec(rot, st->coast_vec[i], v);
                if (has_prev && v[2] > 0.02f && prev[2] > 0.02f)
                    draw_line(d, ex + prev[0] * earth_r, ey + prev[1] * earth_r,
                              ex + v[0] * earth_r, ey + v[1] * earth_r, 1.0f);
                prev[0] = v[0]; prev[1] = v[1]; prev[2] = v[2];
                has_prev = true;
            }
        }
    }

    // Limb outline: bounds the map at the silhouette so coastline vectors
    // meet a rim instead of floating unconnected against the sky
    draw_set_color(d, dca(0.63f, 0.58f, 0.50f, 0.10f + 0.35f * m));
    draw_circle_stroked(d, ex, ey, earth_r, 1.0f);

    // ================= OVER THE GLOBE =================

    // Dial furniture: outer ring, anchored at the dial. (The old daylight
    // arc is retired — the observer-latitude ring on the globe now shows
    // day length directly, thick in daylight and hairline in night.)
    if (geo_a > 0.001f) {
        d->alpha = base_alpha * geo_a;
        draw_set_color(d, dc_scale(s->sunrise_lit, 0.8f));
        draw_circle_stroked(d, 0, dial_y, dial_r + 12.0f, 1.0f);

        // Moon aperture rim at 6 o'clock
        draw_set_color(d, dca(0.45f, 0.44f, 0.42f, 0.5f));
        draw_circle_stroked(d, 0, -dial_y, 62.0f, 1.0f);
    }

    // Helio furniture: axis pin + 24h bezel, riding the globe
    if (helio_a > 0.001f) {
        d->alpha = base_alpha * helio_a;

        // The axis as a physical pin: the visible-pole segment runs from
        // space down to the pole's projected point ON the disc; the
        // far-pole segment terminates at the perimeter, occluded by the
        // planet. Uses the live rotation, so it tracks the morph.
        float axv0 = rot[8], axv1 = rot[9], axv2 = rot[10];
        float an = sqrtf(axv0 * axv0 + axv1 * axv1);
        if (an > 1e-4f) {
            float sgn = (axv2 >= 0) ? 1.0f : -1.0f;   // toward visible pole
            float vx = sgn * axv0 / an, vy = sgn * axv1 / an;
            float ext = earth_r * 0.17f + 4.0f;
            draw_set_color(d, dca(0.75f, 0.72f, 0.65f, 0.8f));
            draw_line(d, ex + vx * (earth_r + ext), ey + vy * (earth_r + ext),
                      ex + sgn * axv0 * earth_r, ey + sgn * axv1 * earth_r, 1.2f);
            draw_line(d, ex - vx * (earth_r + ext), ey - vy * (earth_r + ext),
                      ex - vx * earth_r, ey - vy * earth_r, 1.2f);
        }

        // (The uniform 24h bezel is gone — the surface clock projects its
        // hour marks outward past the limb at their true bearings.)

        // Moon orbit ring (the moon itself is a shared element below)
        draw_set_color(d, dca(0.6f, 0.58f, 0.54f, 0.20f));
        draw_circle_stroked(d, ex, ey, earth_r * 1.55f, 1.0f);
    }

    // ---- Shared elements: continuous across the morph, never fade ----
    d->alpha = base_alpha;

    // Sun marker + hand line: at m=0 the light-based position equals the
    // dial marker exactly (both are the subsolar orthographic projection).
    // When the subsolar point is behind the planet, clamp to the sunward
    // limb, dimmed — "the sun is beyond this horizon".
    {
        float lx = light[0], ly = light[1], lz = light[2];
        DrawColor sun_c = s->sunrise_handle;
        if (lz < 0) {
            float n = sqrtf(lx * lx + ly * ly);
            if (n > 1e-4f) { lx /= n; ly /= n; }
            sun_c = dc_scale(sun_c, 0.5f);
        }
        // In helio the real sun sits at the wheel center, so the marker
        // lifts well off the globe toward it — a bead on the sun line.
        // Its tether starts at the PERIMETER, not the globe center.
        float lift = 1.0f + 0.9f * m;
        float px = ex + lx * earth_r * lift, py = ey + ly * earth_r * lift;
        float mag = sqrtf(lx * lx + ly * ly);
        if (mag * lift > 1.02f && mag > 1e-4f) {
            float sx0 = ex + (lx / mag) * earth_r;
            float sy0 = ey + (ly / mag) * earth_r;
            draw_set_color(d, dca(0.75f, 0.75f, 0.75f, 0.35f));
            draw_line(d, sx0, sy0, px, py, 1.0f);
        }
        draw_set_color(d, sun_c);
        draw_circle_filled(d, px, py, s->sun_size * earth_r / dial_r);

        // Publish for hit-testing (render-side cache; see scene_pointer)
        OrreryViewState *wst = (OrreryViewState *)(uintptr_t)buf;
        wst->bead_x = px;
        wst->bead_y = py;
        wst->bead_hit = s->sun_size * earth_r / dial_r + 16.0f;
        wst->glob_x = ex;
        wst->glob_y = ey;
        wst->glob_r = earth_r;
    }

    // ---- The moon: ONE object across both worldviews ----
    // Geo endpoint: the 6 o'clock aperture, lit in the phase frame (what
    // you see from here). Helio endpoint: on its orbit at the true
    // elongation, lit by the same sun as Earth (why you see it). The
    // morph tweens position, size, and the light vector itself.
    {
        double ph = globe_moon_phase(tv->jd_current
                                     + tv->percent_of_day - 0.5);
        float b = (float)(ph * 2.0 * M_PI);

        // Geo endpoint
        float gx2 = 0.0f, gy2 = -dial_y, gr2 = 52.0f;
        float gl2[3] = { sinf(b), 0.0f, -cosf(b) };

        // Helio endpoint: elongation from the sun direction
        float hx2 = ex, hy2 = ey, hr2 = 22.0f;
        float hl2[3] = { light[0], light[1], light[2] };
        float edx = 0, edy = -1;   // direction moon -> earth, screen
        float lm = sqrtf(light[0] * light[0] + light[1] * light[1]);
        if (lm > 1e-4f) {
            float lx2 = light[0] / lm, ly2 = light[1] / lm;
            float mdx = lx2 * cosf(b) - ly2 * sinf(b);
            float mdy = lx2 * sinf(b) + ly2 * cosf(b);
            float morb = earth_r * 1.55f;
            hx2 = ex + mdx * morb;
            hy2 = ey + mdy * morb;
            edx = -mdx;
            edy = -mdy;
        }

        float mmx = gx2 * (1 - m) + hx2 * m;
        float mmy = gy2 * (1 - m) + hy2 * m;
        float mmr = gr2 * (1 - m) + hr2 * m;
        // Light blend WITHOUT the shortest-path sign flip: the two frames
        // (phase-view vs orbital sun) can be arbitrarily far apart, and
        // the flip hands the moon a negated sun for half of every orbit.
        float ml[3];
        float mn = 0;
        for (int i = 0; i < 3; i++) {
            ml[i] = gl2[i] * (1 - m) + hl2[i] * m;
            mn += ml[i] * ml[i];
        }
        mn = sqrtf(mn);
        if (mn > 1e-3f) {
            for (int i = 0; i < 3; i++) ml[i] /= mn;
        } else {
            memcpy(ml, m > 0.5f ? hl2 : gl2, sizeof(ml));
        }

        GlobeCmd *gm = draw_globe_slot(d, mmx, mmy, mmr);
        if (gm) {
            // Tidal locking: geo shows the near side (lon 0 centered,
            // north up); helio looks down the lunar pole with the
            // near-side meridian aimed at Earth, turning as it orbits.
            float rot_geo[16], rot_helio[16];
            globe_rotation(0, 0, rot_geo);
            memset(rot_helio, 0, sizeof(rot_helio));
            // rows: (edx,edy,0), (edy,-edx,0), (0,0,1) — det -1 display
            rot_helio[0] = edx;  rot_helio[4] = edy;
            rot_helio[1] = edy;  rot_helio[5] = -edx;
            rot_helio[10] = 1.0f;
            rot_helio[15] = 1.0f;
            globe_rot_slerp(rot_geo, rot_helio, m, gm->rot);
            memcpy(gm->light, ml, sizeof(ml));
            gm->land = true;      // sample the lunar albedo
            gm->tex_id = 1;
            gm->grid_boost = 0.0f;
            gm->obs_lat = 999.0f;
            gm->day_col[0] = 0.58f; gm->day_col[1] = 0.55f;
            gm->day_col[2] = 0.49f; gm->day_col[3] = 1.0f;
            gm->night_col[0] = 0.075f; gm->night_col[1] = 0.07f;
            gm->night_col[2] = 0.09f;  gm->night_col[3] = 1.0f;
        }
    }

    // City marker ("you are here") + sky-dome. At m=0 the city projects
    // to the globe center — identical to the old dial ownship ring — and
    // the dome flattens into the dial's sun-path rendering.
    {
        double latr = t->config.latitude * M_PI / 180.0;
        double lonr = t->config.longitude * M_PI / 180.0;
        float p[3] = {
            (float)(cos(latr) * cos(lonr)),
            (float)(cos(latr) * sin(lonr)),
            (float)(sin(latr)),
        };
        float v[3];
        globe_mat_mul_vec(rot, p, v);
        if (v[2] > 0.05f) {
            draw_set_color(d, dca(0.85f, 0.85f, 0.85f, 0.9f));
            draw_circle_stroked(d, ex + v[0] * earth_r, ey + v[1] * earth_r,
                                earth_r * 0.025f + 2.0f, 1.0f);

            const SolarViewState *sol = st->solar;
            if (s->globe_overlay == GLOBE_OVERLAY_SUNPATHS && sol
                && sol->paths_jd == sol->tv.jd_current) {
                float e_east[3] = {
                    (float)-sin(lonr), (float)cos(lonr), 0.0f };
                float e_north[3] = {
                    (float)(-sin(latr) * cos(lonr)),
                    (float)(-sin(latr) * sin(lonr)),
                    (float)cos(latr) };
                float ev[3], nv2[3];
                globe_mat_mul_vec(rot, e_east, ev);
                globe_mat_mul_vec(rot, e_north, nv2);

                struct { const SunPathPt *pts; int n; bool close; DrawColor c; } sets[4] = {
                    { sol->path_jun, SOLAR_PATH_N, false, dca(0.55f, 0.55f, 0.55f, 0.45f) },
                    { sol->path_dec, SOLAR_PATH_N, false, dca(0.55f, 0.55f, 0.55f, 0.45f) },
                    { sol->path_today, SOLAR_PATH_N, false, dca(0.85f, 0.85f, 0.85f, 0.75f) },
                    { sol->analemma, SOLAR_ANA_N, true, dc_scale(s->sunrise_handle, 0.8f) },
                };
                for (int si = 0; si < 4; si++) {
                    draw_set_color(d, sets[si].c);
                    int last = sets[si].close ? sets[si].n : sets[si].n - 1;
                    for (int i = 0; i < last; i++) {
                        const SunPathPt *a = &sets[si].pts[i];
                        const SunPathPt *b = &sets[si].pts[(i + 1) % sets[si].n];
                        if (!a->up || !b->up) continue;
                        // Dome reconstruction: altitude = sqrt(1-x^2-y^2);
                        // pure view-z at m=0, so the flat dial is unchanged
                        float ua = sqrtf(fmaxf(0.0f, 1.0f - a->x * a->x - a->y * a->y));
                        float ub = sqrtf(fmaxf(0.0f, 1.0f - b->x * b->x - b->y * b->y));
                        float ax = v[0] * (1 + ua) + ev[0] * a->x - nv2[0] * a->y;
                        float ay = v[1] * (1 + ua) + ev[1] * a->x - nv2[1] * a->y;
                        float az = v[2] * (1 + ua) + ev[2] * a->x - nv2[2] * a->y;
                        float bx = v[0] * (1 + ub) + ev[0] * b->x - nv2[0] * b->y;
                        float by = v[1] * (1 + ub) + ev[1] * b->x - nv2[1] * b->y;
                        float bz = v[2] * (1 + ub) + ev[2] * b->x - nv2[2] * b->y;
                        // Sphere occlusion
                        if (az < 0 && ax * ax + ay * ay < 1.0f) continue;
                        if (bz < 0 && bx * bx + by * by < 1.0f) continue;
                        draw_line(d, ex + ax * earth_r, ey + ay * earth_r,
                                  ex + bx * earth_r, ey + by * earth_r, 1.0f);
                    }
                }
            }
        }
    }

    // ---- Surface clock face: the latitude ring as a 24-hour dial ----
    // Tick positions are sun-anchored (noon is always the ring's sunward
    // crossing), so the city marker sweeps past them as the hour hand —
    // a clock face lying on the surface of the planet.
    if (m > 0.02f) {
        double latr2 = t->config.latitude * M_PI / 180.0;
        float cl = (float)cos(latr2), sl = (float)sin(latr2);
        float le[3];
        globe_mat_tmul_vec(rot, light, le);   // subsolar point, earth frame
        float lam_ss = atan2f(le[1], le[0]);
        float tick_len = earth_r * 0.035f;
        float num_size = _font_compat[FONT_event].size * 0.42f;
        int w_id = _font_compat[FONT_event].weight;

        for (int h = 0; h < 24; h++) {
            float lam = lam_ss + (float)(h - 12) * 15.0f * (float)M_PI / 180.0f;
            float p[3]  = { cl * cosf(lam), cl * sinf(lam), sl };
            float tn[3] = { -sl * cosf(lam), -sl * sinf(lam), cl };  // poleward
            float pv[3], tw3[3];
            globe_mat_mul_vec(rot, p, pv);
            globe_mat_mul_vec(rot, tn, tw3);
            if (pv[2] < 0.03f) continue;   // front hemisphere only

            bool major = (h % 3) == 0;
            (void)tw3; (void)tick_len;

            // Bezel tick aimed at the DIAL's center — the ring's own
            // projected center (where the axis pierces the ring plane) —
            // not the globe center. Cast a ray from there through the
            // hour point and find where it exits the bezel radius.
            float cvx = rot[8] * sl, cvy = rot[9] * sl;   // ring center, view
            float ux = pv[0] - cvx, uy = pv[1] - cvy;
            float un = sqrtf(ux * ux + uy * uy);
            if (un < 0.03f) continue;
            ux /= un; uy /= un;

            float t0 = earth_r * 0.06f + 1.5f;
            float t1 = earth_r * (major ? 0.16f : 0.11f) + (major ? 3.5f : 2.5f);

            // Ray-circle intersection: |c + s*u| = k (earth-radius units)
            float k0 = 1.0f + t0 / earth_r;
            float cu = cvx * ux + cvy * uy;
            float cc = cvx * cvx + cvy * cvy;
            float disc = cu * cu + k0 * k0 - cc;
            if (disc <= 0) continue;
            float s0 = -cu + sqrtf(disc);
            float bx = ex + (cvx + s0 * ux) * earth_r;
            float by = ey + (cvy + s0 * uy) * earth_r;

            draw_set_color(d, major ? s->clock_lines_strong : s->clock_lines);
            d->alpha = base_alpha * m;
            draw_line(d, bx, by, bx + ux * (t1 - t0), by + uy * (t1 - t0), 1.0f);

            if (major) {
                char hb[3];
                if (h >= 10) { hb[0] = (char)('0' + h / 10); hb[1] = (char)('0' + h % 10); hb[2] = 0; }
                else         { hb[0] = (char)('0' + h); hb[1] = 0; }
                float nd = (t1 - t0) + num_size * 0.85f;
                float nx = bx + ux * nd, ny = by + uy * nd;
                float tw = sdf_measure_width(w_id, hb) * num_size;
                draw_set_color(d, dca(0.8f, 0.78f, 0.72f, 0.65f));
                draw_text_ex(d, w_id, num_size,
                             nx - tw * 0.5f, ny - num_size * 0.5f, hb);
            }
            d->alpha = base_alpha;
        }
    }

    d->alpha = base_alpha;
}

static const ViewVtable orrery_vtable = {
    .init   = orrery_init,
    .enter  = orrery_enter,
    .exit   = orrery_exit,
    .update = orrery_update,
    .render = orrery_render,
};

#endif // SCENE_DEFINED && !VIEW_ORRERY_IMPL
