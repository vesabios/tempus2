// bake_font.c — Build-time SDF font atlas generator
// Outputs: font_atlas.png (SDF atlas) + font_atlas.h (normalized glyph metrics)
//
// Usage: ./bake_font <output_dir>
//
// Uses stb_truetype SDF rasterization. One slot per font weight — size is a
// runtime parameter since SDF scales cleanly to any size.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include "../lib/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../lib/stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../lib/stb_image_write.h"
#include "sdf_from_bitmap.h"

// ASCII printable range
#define FIRST_CHAR  32
#define LAST_CHAR   126
#define NUM_CHARS   (LAST_CHAR - FIRST_CHAR + 1)

#define ATLAS_W     1024
#define ATLAS_H     1024
#define MAX_FONTS   8

// SDF parameters
// We render at HIRES_SCALE * SDF_BAKE_SIZE, compute SDF, then downsample.
// This produces clean distance fields for CFF/cubic fonts where stbtt's
// native GetCodepointSDF fails.
#define SDF_BAKE_SIZE   48.0f   // logical pixel height
#define SDF_PADDING     6       // padding in atlas pixels (after downsample)
#define SDF_ONEDGE      128     // 0-255 value at the glyph edge
#define SDF_SPREAD      6       // max distance in atlas pixels encoded in SDF
#define HIRES_SCALE     4       // render at 4x, then downsample

typedef struct {
    const char *filename;
    const char *name;       // C identifier
} FontWeight;

// One slot per weight — size is runtime
static FontWeight weights[] = {
    { "assets/fonts/Avenir-Black.otf",  "BLACK"  },
    { "assets/fonts/Avenir-Heavy.otf",  "HEAVY"  },
    { "assets/fonts/Avenir-Medium.otf", "MEDIUM" },
    { "assets/fonts/Avenir-Light.otf",  "LIGHT"  },
};
#define NUM_WEIGHTS (sizeof(weights) / sizeof(weights[0]))

typedef struct {
    float u0, v0, u1, v1;  // atlas texcoords
    float nw, nh;           // normalized glyph size (pixels / bake_size)
    float nxoff, nyoff;     // normalized offset from cursor
    float nadvance;         // normalized horizontal advance
} SdfGlyphInfo;

static unsigned char *load_file(const char *path, int *size) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(*size);
    fread(buf, 1, *size, f);
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    const char *outdir = argc > 1 ? argv[1] : ".";

    unsigned char *atlas = calloc(ATLAS_W * ATLAS_H, 1);

    stbrp_context pack_ctx;
    stbrp_node nodes[ATLAS_W];
    stbrp_init_target(&pack_ctx, ATLAS_W, ATLAS_H, nodes, ATLAS_W);

    int total_glyphs = NUM_WEIGHTS * NUM_CHARS;
    stbrp_rect *rects = calloc(total_glyphs, sizeof(stbrp_rect));
    SdfGlyphInfo *glyphs = calloc(total_glyphs, sizeof(SdfGlyphInfo));

    stbtt_fontinfo fontinfos[MAX_FONTS];
    unsigned char *fontbufs[MAX_FONTS];
    float scales[MAX_FONTS];

    // Load fonts
    for (int fi = 0; fi < (int)NUM_WEIGHTS; fi++) {
        int fsize;
        fontbufs[fi] = load_file(weights[fi].filename, &fsize);
        if (!fontbufs[fi]) { fprintf(stderr, "Failed to load %s\n", weights[fi].filename); return 1; }

        stbtt_InitFont(&fontinfos[fi], fontbufs[fi],
                       stbtt_GetFontOffsetForIndex(fontbufs[fi], 0));
        scales[fi] = stbtt_ScaleForPixelHeight(&fontinfos[fi], SDF_BAKE_SIZE);
    }

    // Measure glyphs — get bitmap box, add SDF padding
    float hires_scale_f = (float)HIRES_SCALE;
    for (int fi = 0; fi < (int)NUM_WEIGHTS; fi++) {
        for (int ci = 0; ci < NUM_CHARS; ci++) {
            int idx = fi * NUM_CHARS + ci;
            int ch = FIRST_CHAR + ci;

            // Get bitmap bounds at bake size
            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&fontinfos[fi], ch, scales[fi], scales[fi],
                                        &x0, &y0, &x1, &y1);
            int bw = x1 - x0;
            int bh = y1 - y0;

            // Skip empty glyphs (space, etc.) — no SDF needed
            if (bw <= 0 || bh <= 0) {
                rects[idx].w = 1;
                rects[idx].h = 1;
                rects[idx].id = idx;
                glyphs[idx].nw = 0;
                glyphs[idx].nh = 0;
                int advw, lsb;
                stbtt_GetCodepointHMetrics(&fontinfos[fi], ch, &advw, &lsb);
                glyphs[idx].nadvance = advw * scales[fi] / SDF_BAKE_SIZE;
                continue;
            }

            // Final atlas glyph size = bitmap size + SDF_PADDING on each side
            int gw = bw + SDF_PADDING * 2;
            int gh = bh + SDF_PADDING * 2;

            rects[idx].w = gw + 2;
            rects[idx].h = gh + 2;
            rects[idx].id = idx;

            float inv = 1.0f / SDF_BAKE_SIZE;
            glyphs[idx].nw    = (float)gw * inv;
            glyphs[idx].nh    = (float)gh * inv;
            glyphs[idx].nxoff = (float)(x0 - SDF_PADDING) * inv;
            glyphs[idx].nyoff = (float)(y0 - SDF_PADDING) * inv;

            int advw, lsb;
            stbtt_GetCodepointHMetrics(&fontinfos[fi], ch, &advw, &lsb);
            glyphs[idx].nadvance = advw * scales[fi] * inv;
        }
    }

    // Pack
    if (!stbrp_pack_rects(&pack_ctx, rects, total_glyphs)) {
        fprintf(stderr, "Atlas packing failed! Need larger atlas.\n");
        return 1;
    }

    // Render: bitmap at HIRES_SCALE, compute SDF, downsample to atlas
    for (int fi = 0; fi < (int)NUM_WEIGHTS; fi++) {
        float hscale = scales[fi] * hires_scale_f;
        for (int ci = 0; ci < NUM_CHARS; ci++) {
            int idx = fi * NUM_CHARS + ci;
            int ch = FIRST_CHAR + ci;

            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&fontinfos[fi], ch, scales[fi], scales[fi],
                                        &x0, &y0, &x1, &y1);
            int bw = x1 - x0;
            int bh = y1 - y0;
            if (bw <= 0 || bh <= 0) continue;

            // Target atlas glyph size
            int gw = bw + SDF_PADDING * 2;
            int gh = bh + SDF_PADDING * 2;

            // High-res bitmap size
            int hw = gw * HIRES_SCALE;
            int hh = gh * HIRES_SCALE;
            int hpad = SDF_PADDING * HIRES_SCALE;

            // Render bitmap at high res
            unsigned char *hires_bmp = calloc(hw * hh, 1);
            stbtt_MakeCodepointBitmap(&fontinfos[fi],
                hires_bmp + hpad * hw + hpad,
                bw * HIRES_SCALE, bh * HIRES_SCALE, hw,
                hscale, hscale, ch);

            // Compute SDF from high-res bitmap
            int spread_hires = SDF_SPREAD * HIRES_SCALE;
            unsigned char *sdf_hires = sdf_from_bitmap(hires_bmp, hw, hh,
                                                        spread_hires, SDF_ONEDGE);
            free(hires_bmp);

            // Downsample SDF to atlas resolution
            unsigned char *sdf = sdf_downsample(sdf_hires, hw, hh, gw, gh);
            free(sdf_hires);

            int px = rects[idx].x + 1;
            int py = rects[idx].y + 1;

            // Copy SDF data into atlas
            for (int row = 0; row < gh; row++) {
                memcpy(atlas + (py + row) * ATLAS_W + px,
                       sdf + row * gw, gw);
            }

            glyphs[idx].u0 = (float)px / ATLAS_W;
            glyphs[idx].v0 = (float)py / ATLAS_H;
            glyphs[idx].u1 = (float)(px + gw) / ATLAS_W;
            glyphs[idx].v1 = (float)(py + gh) / ATLAS_H;

            free(sdf);
        }
    }

    // Write atlas PNG
    char path[512];
    snprintf(path, sizeof(path), "%s/font_atlas.png", outdir);
    stbi_write_png(path, ATLAS_W, ATLAS_H, 1, atlas, ATLAS_W);
    printf("Wrote %s (%dx%d, SDF padding=%d onedge=%d)\n",
           path, ATLAS_W, ATLAS_H, SDF_PADDING, SDF_ONEDGE);

    // Write header
    snprintf(path, sizeof(path), "%s/font_atlas.h", outdir);
    FILE *hf = fopen(path, "w");
    if (!hf) { fprintf(stderr, "Cannot write %s\n", path); return 1; }

    fprintf(hf, "// font_atlas.h — AUTO-GENERATED by bake_font (SDF)\n");
    fprintf(hf, "// Atlas: %dx%d, %d weights, %d glyphs each, bake_size=%.0f, padding=%d\n\n",
            ATLAS_W, ATLAS_H, (int)NUM_WEIGHTS, NUM_CHARS, SDF_BAKE_SIZE, SDF_PADDING);
    fprintf(hf, "#ifndef FONT_ATLAS_DATA_H\n#define FONT_ATLAS_DATA_H\n\n");

    fprintf(hf, "#define FONT_ATLAS_W %d\n", ATLAS_W);
    fprintf(hf, "#define FONT_ATLAS_HT %d\n", ATLAS_H);
    fprintf(hf, "#define SDF_BAKE_SIZE %.1ff\n", SDF_BAKE_SIZE);
    fprintf(hf, "#define SDF_PADDING %d\n", SDF_PADDING);
    fprintf(hf, "#define SDF_ONEDGE %d\n", SDF_ONEDGE);
    fprintf(hf, "#define SDF_NUM_FONTS %d\n", (int)NUM_WEIGHTS);
    fprintf(hf, "#define SDF_FIRST_CHAR %d\n", FIRST_CHAR);
    fprintf(hf, "#define SDF_LAST_CHAR %d\n", LAST_CHAR);
    fprintf(hf, "#define SDF_NUM_CHARS %d\n\n", NUM_CHARS);

    // Weight enum
    fprintf(hf, "enum {\n");
    for (int fi = 0; fi < (int)NUM_WEIGHTS; fi++)
        fprintf(hf, "    SDF_WEIGHT_%s = %d,\n", weights[fi].name, fi);
    fprintf(hf, "};\n\n");

    // FontStyle: weight + size (size is runtime)
    fprintf(hf, "typedef struct { int weight; float size; } FontStyle;\n\n");

    // Backward-compat integer font IDs (index into _font_compat table in draw.h)
    fprintf(hf, "enum {\n");
    fprintf(hf, "    FONT_logo = 0, FONT_event = 1, FONT_month = 2,\n");
    fprintf(hf, "    FONT_seconds = 3, FONT_minutes = 4, FONT_clock = 5, FONT_date = 6,\n");
    fprintf(hf, "};\n\n");

    // Normalized ascent per weight
    fprintf(hf, "static const float sdf_nascent[SDF_NUM_FONTS] = {\n");
    for (int fi = 0; fi < (int)NUM_WEIGHTS; fi++) {
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&fontinfos[fi], &asc, &desc, &gap);
        fprintf(hf, "    %.6ff, // %s\n", asc * scales[fi] / SDF_BAKE_SIZE, weights[fi].name);
    }
    fprintf(hf, "};\n\n");

    // SDF glyph struct
    fprintf(hf, "typedef struct {\n");
    fprintf(hf, "    float u0, v0, u1, v1; // atlas texcoords\n");
    fprintf(hf, "    float nw, nh;         // normalized glyph size\n");
    fprintf(hf, "    float nxoff, nyoff;   // normalized offset from cursor\n");
    fprintf(hf, "    float nadvance;       // normalized horizontal advance\n");
    fprintf(hf, "} SdfGlyph;\n\n");

    // Glyph table
    fprintf(hf, "static const SdfGlyph sdf_glyphs[SDF_NUM_FONTS][SDF_NUM_CHARS] = {\n");
    for (int fi = 0; fi < (int)NUM_WEIGHTS; fi++) {
        fprintf(hf, "    { // %s\n", weights[fi].name);
        for (int ci = 0; ci < NUM_CHARS; ci++) {
            int idx = fi * NUM_CHARS + ci;
            SdfGlyphInfo *g = &glyphs[idx];
            fprintf(hf, "        {%.6ff,%.6ff,%.6ff,%.6ff, %.6ff,%.6ff, %.6ff,%.6ff, %.6ff}, // '%c'\n",
                    g->u0, g->v0, g->u1, g->v1,
                    g->nw, g->nh, g->nxoff, g->nyoff, g->nadvance,
                    (FIRST_CHAR + ci) >= 32 ? (char)(FIRST_CHAR + ci) : '?');
        }
        fprintf(hf, "    },\n");
    }
    fprintf(hf, "};\n\n");

    // Convenience: measure string width (normalized — multiply by size for pixels)
    fprintf(hf, "static inline float sdf_measure_width(int weight, const char *s) {\n");
    fprintf(hf, "    float w = 0;\n");
    fprintf(hf, "    for (; *s; s++) {\n");
    fprintf(hf, "        int ci = *s - SDF_FIRST_CHAR;\n");
    fprintf(hf, "        if (ci >= 0 && ci < SDF_NUM_CHARS)\n");
    fprintf(hf, "            w += sdf_glyphs[weight][ci].nadvance;\n");
    fprintf(hf, "    }\n");
    fprintf(hf, "    return w;\n");
    fprintf(hf, "}\n\n");

    // Backward compat shims so existing draw.h code still compiles
    // (these will be removed when text.h takes over)
    fprintf(hf, "// Backward-compat shims (temporary)\n");
    fprintf(hf, "#define FONT_NUM_FONTS SDF_NUM_FONTS\n");
    fprintf(hf, "#define FONT_FIRST_CHAR SDF_FIRST_CHAR\n");
    fprintf(hf, "#define FONT_NUM_CHARS SDF_NUM_CHARS\n");
    fprintf(hf, "typedef SdfGlyph FontGlyph;\n");
    fprintf(hf, "#define font_glyphs sdf_glyphs\n");
    fprintf(hf, "#define font_ascent sdf_nascent\n");
    fprintf(hf, "#define font_sizes ((const float[]){18,48,24,14,48,38,18})\n");
    fprintf(hf, "static inline float font_measure_width(int fid, const char *s) {\n");
    fprintf(hf, "    return sdf_measure_width(fid, s);\n");
    fprintf(hf, "}\n\n");

    fprintf(hf, "#endif // FONT_ATLAS_DATA_H\n");
    fclose(hf);
    printf("Wrote %s\n", path);

    for (int fi = 0; fi < (int)NUM_WEIGHTS; fi++) free(fontbufs[fi]);
    free(atlas);
    free(rects);
    free(glyphs);

    return 0;
}
