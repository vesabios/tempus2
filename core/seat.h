// seat.h — the transition manager's mathematics (Stage 2).
//
// docs/TRANSITIONS.md: stations declare per-object MEMBERS in typed
// frames; one manager interpolates properties barycentrically over
// the station weight vector. This header is the manager's toolbox —
// each function embodies one of the instrument's hard-won laws as
// code instead of discipline:
//
//   seat_mix_polar  — circular mean: never cartesian-lerp across a
//                     polar space (the moon dipped through the globe)
//   seat_mix_scale  — log-lerp: sizes breathe geometrically
//   seat_mix_dir3   — light frames blend by component, live, without
//                     shortest-path flips (never nlerp snapshots)
//   seat_mix_pos    — plain barycentric point mix, for cross-frame
//                     OUTPUT blending only (rule 3)

#ifndef SEAT_H
#define SEAT_H

#include <math.h>
#include <stdbool.h>
#include "station.h"

// Weighted circular mean of angles (radians) with per-seat radii
// about a shared center: the polar-lerp law, N-way. Angles blend on
// the unit circle (no wraparound seams); radii blend linearly.
static inline void seat_mix_polar(const float *ang, const float *rad,
                                  const double *w, int n,
                                  float *out_ang, float *out_rad) {
    float sx = 0, sy = 0, r = 0;
    for (int i = 0; i < n; i++) {
        sx += (float)w[i] * cosf(ang[i]);
        sy += (float)w[i] * sinf(ang[i]);
        r  += (float)w[i] * rad[i];
    }
    *out_ang = (sx * sx + sy * sy > 1e-10f) ? atan2f(sy, sx) : ang[0];
    *out_rad = r;
}

// Log-lerp of strictly positive scales: a 10 -> 40 flight passes
// through 20, not 25 — growth reads as growth.
static inline float seat_mix_scale(const float *s, const double *w,
                                   int n) {
    double acc = 0, tot = 0;
    for (int i = 0; i < n; i++) {
        if (s[i] <= 0) continue;
        acc += w[i] * log((double)s[i]);
        tot += w[i];
    }
    return tot > 1e-9 ? (float)exp(acc / tot) : (n ? s[0] : 1.0f);
}

// Component blend of unit direction vectors, renormalized — no
// shortest-path snapping, no snapshot nlerp: callers pass LIVE frame
// vectors each frame.
static inline void seat_mix_dir3(const float (*v)[3], const double *w,
                                 int n, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 3; k++)
            out[k] += (float)w[i] * v[i][k];
    float m = sqrtf(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
    if (m > 1e-4f) {
        out[0] /= m; out[1] /= m; out[2] /= m;
    } else if (n) {
        out[0] = v[0][0]; out[1] = v[0][1]; out[2] = v[0][2];
    }
}

// Plain barycentric point mix — legitimate ONLY for cross-frame
// output blending (rule 3): both sides already evaluated in their
// own live frames.
static inline void seat_mix_pos(const float *x, const float *y,
                                const double *w, int n,
                                float *ox, float *oy) {
    float ax = 0, ay = 0;
    for (int i = 0; i < n; i++) {
        ax += (float)w[i] * x[i];
        ay += (float)w[i] * y[i];
    }
    *ox = ax; *oy = ay;
}

#endif // SEAT_H
