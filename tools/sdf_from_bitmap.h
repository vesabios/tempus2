// sdf_from_bitmap.h — Compute SDF from a high-res binary bitmap
// Dead-reckoning distance transform (8SSEDT approximation).
// Input: 8-bit bitmap (0 = outside, nonzero = inside)
// Output: 8-bit SDF (onedge_value at boundary, higher = inside, lower = outside)

#ifndef SDF_FROM_BITMAP_H
#define SDF_FROM_BITMAP_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

// Compute SDF from a binary bitmap using the dead-reckoning approach.
// spread: max distance in pixels to encode (maps to 0..onedge and onedge..255)
// Returns malloc'd buffer (caller frees).
static unsigned char *sdf_from_bitmap(const unsigned char *bitmap, int w, int h,
                                      int spread, unsigned char onedge) {
    int size = w * h;
    float *dist = (float *)malloc(size * sizeof(float));
    unsigned char *out = (unsigned char *)malloc(size);

    float maxdist = (float)spread;

    // Initialize: inside = 0, outside = large
    for (int i = 0; i < size; i++)
        dist[i] = (bitmap[i] > 127) ? 0.0f : maxdist * 2.0f;

    // Forward pass (top-left to bottom-right)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            float d = dist[i];
            if (x > 0)             d = fminf(d, dist[i - 1] + 1.0f);
            if (y > 0)             d = fminf(d, dist[(y-1)*w + x] + 1.0f);
            if (x > 0 && y > 0)    d = fminf(d, dist[(y-1)*w + x - 1] + 1.414f);
            if (x < w-1 && y > 0)  d = fminf(d, dist[(y-1)*w + x + 1] + 1.414f);
            dist[i] = d;
        }
    }

    // Backward pass (bottom-right to top-left)
    for (int y = h - 1; y >= 0; y--) {
        for (int x = w - 1; x >= 0; x--) {
            int i = y * w + x;
            float d = dist[i];
            if (x < w-1)           d = fminf(d, dist[i + 1] + 1.0f);
            if (y < h-1)           d = fminf(d, dist[(y+1)*w + x] + 1.0f);
            if (x < w-1 && y < h-1) d = fminf(d, dist[(y+1)*w + x + 1] + 1.414f);
            if (x > 0 && y < h-1)  d = fminf(d, dist[(y+1)*w + x - 1] + 1.414f);
            dist[i] = d;
        }
    }

    // Now compute the OUTSIDE distance (from inverted bitmap)
    float *dist_out = (float *)malloc(size * sizeof(float));
    for (int i = 0; i < size; i++)
        dist_out[i] = (bitmap[i] <= 127) ? 0.0f : maxdist * 2.0f;

    // Forward
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            float d = dist_out[i];
            if (x > 0)             d = fminf(d, dist_out[i - 1] + 1.0f);
            if (y > 0)             d = fminf(d, dist_out[(y-1)*w + x] + 1.0f);
            if (x > 0 && y > 0)    d = fminf(d, dist_out[(y-1)*w + x - 1] + 1.414f);
            if (x < w-1 && y > 0)  d = fminf(d, dist_out[(y-1)*w + x + 1] + 1.414f);
            dist_out[i] = d;
        }
    }

    // Backward
    for (int y = h - 1; y >= 0; y--) {
        for (int x = w - 1; x >= 0; x--) {
            int i = y * w + x;
            float d = dist_out[i];
            if (x < w-1)           d = fminf(d, dist_out[i + 1] + 1.0f);
            if (y < h-1)           d = fminf(d, dist_out[(y+1)*w + x] + 1.0f);
            if (x < w-1 && y < h-1) d = fminf(d, dist_out[(y+1)*w + x + 1] + 1.414f);
            if (x > 0 && y < h-1)  d = fminf(d, dist_out[(y+1)*w + x - 1] + 1.414f);
            dist_out[i] = d;
        }
    }

    // Combine: signed distance = inside_dist - outside_dist
    // Map to 0-255: onedge at boundary, higher = further inside
    for (int i = 0; i < size; i++) {
        float sd = dist_out[i] - dist[i]; // positive inside, negative outside
        float norm = sd / maxdist; // -1..1 range
        float val = onedge + norm * onedge; // 0..2*onedge range
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        out[i] = (unsigned char)(val + 0.5f);
    }

    free(dist);
    free(dist_out);
    return out;
}

// Downsample an SDF from (sw,sh) to (dw,dh) with bilinear filtering
static unsigned char *sdf_downsample(const unsigned char *src, int sw, int sh,
                                     int dw, int dh) {
    unsigned char *dst = (unsigned char *)malloc(dw * dh);
    float sx = (float)sw / dw;
    float sy = (float)sh / dh;

    for (int dy = 0; dy < dh; dy++) {
        for (int dx = 0; dx < dw; dx++) {
            float fx = (dx + 0.5f) * sx - 0.5f;
            float fy = (dy + 0.5f) * sy - 0.5f;
            int ix = (int)floorf(fx);
            int iy = (int)floorf(fy);
            float tx = fx - ix;
            float ty = fy - iy;

            int x0 = ix < 0 ? 0 : (ix >= sw ? sw-1 : ix);
            int y0 = iy < 0 ? 0 : (iy >= sh ? sh-1 : iy);
            int x1 = x0+1 >= sw ? x0 : x0+1;
            int y1 = y0+1 >= sh ? y0 : y0+1;

            float v = (1-tx)*(1-ty)*src[y0*sw+x0] + tx*(1-ty)*src[y0*sw+x1]
                    + (1-tx)*ty*src[y1*sw+x0] + tx*ty*src[y1*sw+x1];
            dst[dy*dw+dx] = (unsigned char)(v + 0.5f);
        }
    }
    return dst;
}

#endif
