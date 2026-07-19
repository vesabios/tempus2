// atmo.h — Single-scattering atmosphere (Nishita-style raymarch).
// Rayleigh + Mie along a view ray through a two-layer atmosphere,
// with a secondary march toward the sun for attenuation. Shared by
// CAELVM's dome and HORAE's sky band: one physics, every sky on the
// instrument.

#ifndef ATMO_H
#define ATMO_H

#include <math.h>
#include <stdbool.h>

static inline bool atmo__rsi(const float ro[3], const float rd[3],
                             float sr, float *t0, float *t1) {
    float b = ro[0]*rd[0] + ro[1]*rd[1] + ro[2]*rd[2];
    float c = ro[0]*ro[0] + ro[1]*ro[1] + ro[2]*ro[2] - sr*sr;
    float h = b*b - c;
    if (h < 0.0f) return false;
    h = sqrtf(h);
    *t0 = -b - h;
    *t1 = -b + h;
    return true;
}

static inline void atmo_scatter(const float rd[3], const float sd[3],
                                float out[3]) {
    const float Re = 6371e3f, Ra = 6471e3f;
    const float kR[3] = { 5.5e-6f, 13.0e-6f, 22.4e-6f };
    const float kM = 21e-6f;
    const float shR = 8000.0f, shM = 1200.0f;
    const float g = 0.758f;
    const float iSun = 22.0f;
    const int ISTEPS = 12, JSTEPS = 6;

    float ro[3] = { 0.0f, Re + 1200.0f, 0.0f };
    float t0, t1;
    out[0] = out[1] = out[2] = 0.0f;
    if (!atmo__rsi(ro, rd, Ra, &t0, &t1) || t1 < 0.0f) return;
    float tA = t0 > 0.0f ? t0 : 0.0f;
    float ds = (t1 - tA) / ISTEPS;
    float tI = tA;

    float mu = rd[0]*sd[0] + rd[1]*sd[1] + rd[2]*sd[2];
    float mumu = mu * mu, gg = g * g;
    float pR = 3.0f / (16.0f * (float)M_PI) * (1.0f + mumu);
    float pM = 3.0f / (8.0f * (float)M_PI)
             * ((1.0f - gg) * (1.0f + mumu))
             / ((2.0f + gg) * powf(1.0f + gg - 2.0f * g * mu, 1.5f));

    float totR[3] = { 0, 0, 0 }, totM[3] = { 0, 0, 0 };
    float iOdR = 0.0f, iOdM = 0.0f;
    for (int i = 0; i < ISTEPS; i++) {
        float px = ro[0] + rd[0] * (tI + ds * 0.5f);
        float py = ro[1] + rd[1] * (tI + ds * 0.5f);
        float pz = ro[2] + rd[2] * (tI + ds * 0.5f);
        float h = sqrtf(px*px + py*py + pz*pz) - Re;
        float odR = expf(-h / shR) * ds;
        float odM = expf(-h / shM) * ds;
        iOdR += odR;
        iOdM += odM;

        float pp[3] = { px, py, pz };
        float jt0, jt1;
        atmo__rsi(pp, sd, Ra, &jt0, &jt1);
        float jds = jt1 / JSTEPS;
        float jt = 0.0f;
        float jOdR = 0.0f, jOdM = 0.0f;
        for (int j = 0; j < JSTEPS; j++) {
            float qx = px + sd[0] * (jt + jds * 0.5f);
            float qy = py + sd[1] * (jt + jds * 0.5f);
            float qz = pz + sd[2] * (jt + jds * 0.5f);
            float jh = sqrtf(qx*qx + qy*qy + qz*qz) - Re;
            jOdR += expf(-jh / shR) * jds;
            jOdM += expf(-jh / shM) * jds;
            jt += jds;
        }
        for (int cch = 0; cch < 3; cch++) {
            float attn = expf(-(kM * (iOdM + jOdM)
                                + kR[cch] * (iOdR + jOdR)));
            totR[cch] += odR * attn;
            totM[cch] += odM * attn;
        }
        tI += ds;
    }
    for (int cch = 0; cch < 3; cch++)
        out[cch] = iSun * (pR * kR[cch] * totR[cch]
                           + pM * kM * totM[cch]);
}

// az/alt (degrees) to the scattering frame's unit vector (y up)
static inline void atmo_dir(float az_deg, float alt_deg, float v[3]) {
    float a = az_deg * (float)M_PI / 180.0f;
    float l = alt_deg * (float)M_PI / 180.0f;
    v[0] = sinf(a) * cosf(l);
    v[1] = sinf(l);
    v[2] = cosf(a) * cosf(l);
}

#endif // ATMO_H
