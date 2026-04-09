// test_glyphs.c — Render a glyph sheet from the SDF atlas to verify metrics
// Outputs: test_glyphs.png
// Usage: ./test_glyphs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../lib/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../lib/stb_image_write.h"

#include "../assets/font_atlas.h"

#define OUT_W 1200
#define OUT_H 800

static unsigned char atlas_data[FONT_ATLAS_W * FONT_ATLAS_HT];
static unsigned char output[OUT_W * OUT_H];

// Sample the SDF atlas with bilinear filtering
static float sample_sdf(float u, float v) {
    float fx = u * FONT_ATLAS_W - 0.5f;
    float fy = v * FONT_ATLAS_HT - 0.5f;
    int ix = (int)floorf(fx);
    int iy = (int)floorf(fy);
    float dx = fx - ix;
    float dy = fy - iy;

    int x0 = ix < 0 ? 0 : (ix >= FONT_ATLAS_W ? FONT_ATLAS_W-1 : ix);
    int y0 = iy < 0 ? 0 : (iy >= FONT_ATLAS_HT ? FONT_ATLAS_HT-1 : iy);
    int x1 = x0+1 >= FONT_ATLAS_W ? x0 : x0+1;
    int y1 = y0+1 >= FONT_ATLAS_HT ? y0 : y0+1;

    float s00 = atlas_data[y0 * FONT_ATLAS_W + x0] / 255.0f;
    float s10 = atlas_data[y0 * FONT_ATLAS_W + x1] / 255.0f;
    float s01 = atlas_data[y1 * FONT_ATLAS_W + x0] / 255.0f;
    float s11 = atlas_data[y1 * FONT_ATLAS_W + x1] / 255.0f;

    float top = s00 + (s10 - s00) * dx;
    float bot = s01 + (s11 - s01) * dx;
    return top + (bot - top) * dy;
}

// Render a string into the output buffer at (ox, oy) with given size
static void render_string(const char *s, int weight, float size,
                          int ox, int oy) {
    float ascent = sdf_nascent[weight] * size;
    float cursor = 0;

    for (; *s; s++) {
        int ci = *s - SDF_FIRST_CHAR;
        if (ci < 0 || ci >= SDF_NUM_CHARS) {
            cursor += sdf_glyphs[weight][0].nadvance * size;
            continue;
        }
        const SdfGlyph *g = &sdf_glyphs[weight][ci];
        if (g->nw <= 0 || g->nh <= 0) {
            cursor += g->nadvance * size;
            continue;
        }

        float gx = cursor + g->nxoff * size;
        float gy = ascent + g->nyoff * size;
        float gw = g->nw * size;
        float gh = g->nh * size;

        // Rasterize this glyph quad
        int px0 = (int)(ox + gx);
        int py0 = (int)(oy + gy);
        int px1 = (int)(ox + gx + gw + 1);
        int py1 = (int)(oy + gy + gh + 1);

        for (int py = py0; py <= py1; py++) {
            for (int px = px0; px <= px1; px++) {
                if (px < 0 || px >= OUT_W || py < 0 || py >= OUT_H) continue;

                // Map pixel back to UV
                float t = (float)(px - ox - gx) / gw;
                float u_val = g->u0 + (g->u1 - g->u0) * t;
                float s_val = (float)(py - oy - gy) / gh;
                float v_val = g->v0 + (g->v1 - g->v0) * s_val;

                float d = sample_sdf(u_val, v_val);

                // SDF threshold
                float alpha = (d - 0.48f) / 0.04f; // sharp edge
                if (alpha < 0) alpha = 0;
                if (alpha > 1) alpha = 1;

                unsigned char existing = output[py * OUT_W + px];
                unsigned char val = (unsigned char)(alpha * 255);
                if (val > existing)
                    output[py * OUT_W + px] = val;
            }
        }

        cursor += g->nadvance * size;
    }
}

// Draw a horizontal baseline/guide line
static void hline(int y, int x0, int x1, unsigned char val) {
    if (y < 0 || y >= OUT_H) return;
    for (int x = x0; x < x1 && x < OUT_W; x++)
        if (x >= 0) output[y * OUT_W + x] = val;
}

int main(void) {
    // Load SDF atlas
    int w, h, n;
    unsigned char *data = stbi_load("assets/font_atlas.png", &w, &h, &n, 1);
    if (!data) {
        fprintf(stderr, "Failed to load font_atlas.png\n");
        return 1;
    }
    memcpy(atlas_data, data, FONT_ATLAS_W * FONT_ATLAS_HT);
    stbi_image_free(data);

    memset(output, 0, sizeof(output));

    const char *test_strings[] = {
        "0123456789",
        "ABCDEFGHIJKLM",
        "NOPQRSTUVWXYZ",
        "T E M P V S",
        "JANUARY FEBRUARY",
        "abcdefghijklmnop",
    };
    int num_strings = sizeof(test_strings) / sizeof(test_strings[0]);

    // Row 1-3: Medium weight at different sizes
    int y = 10;
    float sizes[] = { 48.0f, 38.0f, 24.0f, 18.0f };
    const char *size_labels[] = { "48px", "38px", "24px", "18px" };

    for (int si = 0; si < 4; si++) {
        float sz = sizes[si];
        // Guide line at baseline
        hline(y + (int)(sdf_nascent[SDF_WEIGHT_MEDIUM] * sz), 0, OUT_W, 30);
        render_string(test_strings[0], SDF_WEIGHT_MEDIUM, sz, 10, y);
        render_string(size_labels[si], SDF_WEIGHT_MEDIUM, 14, OUT_W - 60, y);
        y += (int)(sz * 1.4f);
    }

    y += 20;

    // Row 4+: All weights at 38px
    const char *weight_names[] = { "Black", "Heavy", "Medium", "Light" };
    for (int wi = 0; wi < SDF_NUM_FONTS; wi++) {
        hline(y + (int)(sdf_nascent[wi] * 38), 0, OUT_W, 30);
        render_string("0123456789 TEMPVS", wi, 38.0f, 10, y);
        render_string(weight_names[wi], SDF_WEIGHT_MEDIUM, 14, OUT_W - 80, y);
        y += 54;
    }

    y += 20;

    // Large text
    render_string("12", SDF_WEIGHT_MEDIUM, 80.0f, 10, y);
    render_string("OSTARA", SDF_WEIGHT_LIGHT, 60.0f, 200, y);

    stbi_write_png("test_glyphs.png", OUT_W, OUT_H, 1, output, OUT_W);
    printf("Wrote test_glyphs.png (%dx%d)\n", OUT_W, OUT_H);
    return 0;
}
