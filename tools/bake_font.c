// bake_font.c — Build-time tool to bake Avenir font variants into a texture atlas
// Outputs: font_atlas.png + font_atlas.h (glyph metrics)
//
// Usage: ./bake_font <output_dir>
//
// Bakes a single atlas with multiple font sizes/weights.
// Each "font slot" is a weight+size pair.

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

// Characters to bake (ASCII printable range)
#define FIRST_CHAR  32
#define LAST_CHAR   126
#define NUM_CHARS   (LAST_CHAR - FIRST_CHAR + 1)

#define ATLAS_W     1024
#define ATLAS_H     1024
#define MAX_FONTS   8

typedef struct {
    const char *filename;
    const char *name;       // C identifier prefix
    float       size_px;
} FontSpec;

// Font slots matching the original Tempus usage
static FontSpec fonts[] = {
    { "assets/fonts/Avenir-Black.otf",  "logo",     18.0f },
    { "assets/fonts/Avenir-Light.otf",  "event",    48.0f },
    { "assets/fonts/Avenir-Medium.otf", "month",    24.0f },
    { "assets/fonts/Avenir-Heavy.otf",  "seconds",  14.0f },
    { "assets/fonts/Avenir-Light.otf",  "minutes",  48.0f },
    { "assets/fonts/Avenir-Medium.otf", "clock",    38.0f },
    { "assets/fonts/Avenir-Medium.otf", "date",     18.0f },
};
#define NUM_FONTS (sizeof(fonts) / sizeof(fonts[0]))

typedef struct {
    float x0, y0, x1, y1;  // texcoords in atlas (0..1)
    float w, h;             // pixel size
    float xoff, yoff;       // offset from cursor
    float advance;          // horizontal advance
} GlyphInfo;

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

    // Rect packer
    stbrp_context pack_ctx;
    stbrp_node nodes[ATLAS_W];
    stbrp_init_target(&pack_ctx, ATLAS_W, ATLAS_H, nodes, ATLAS_W);

    // We'll collect all rects first, then pack, then render
    int total_glyphs = NUM_FONTS * NUM_CHARS;
    stbrp_rect *rects = calloc(total_glyphs, sizeof(stbrp_rect));
    GlyphInfo *glyphs = calloc(total_glyphs, sizeof(GlyphInfo));

    // Load fonts and measure glyphs
    stbtt_fontinfo fontinfos[MAX_FONTS];
    unsigned char *fontbufs[MAX_FONTS];
    float scales[MAX_FONTS];

    for (int fi = 0; fi < (int)NUM_FONTS; fi++) {
        int fsize;
        fontbufs[fi] = load_file(fonts[fi].filename, &fsize);
        if (!fontbufs[fi]) { fprintf(stderr, "Failed to load %s\n", fonts[fi].filename); return 1; }

        stbtt_InitFont(&fontinfos[fi], fontbufs[fi],
                       stbtt_GetFontOffsetForIndex(fontbufs[fi], 0));
        scales[fi] = stbtt_ScaleForPixelHeight(&fontinfos[fi], fonts[fi].size_px);

        for (int ci = 0; ci < NUM_CHARS; ci++) {
            int idx = fi * NUM_CHARS + ci;
            int ch = FIRST_CHAR + ci;
            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&fontinfos[fi], ch, scales[fi], scales[fi],
                                        &x0, &y0, &x1, &y1);
            int gw = x1 - x0 + 2; // +2 for padding
            int gh = y1 - y0 + 2;
            if (gw < 1) gw = 1;
            if (gh < 1) gh = 1;

            rects[idx].w = gw;
            rects[idx].h = gh;
            rects[idx].id = idx;

            glyphs[idx].w = (float)(x1 - x0);
            glyphs[idx].h = (float)(y1 - y0);
            glyphs[idx].xoff = (float)x0;
            glyphs[idx].yoff = (float)y0;

            int advw, lsb;
            stbtt_GetCodepointHMetrics(&fontinfos[fi], ch, &advw, &lsb);
            glyphs[idx].advance = advw * scales[fi];
        }
    }

    // Pack
    if (!stbrp_pack_rects(&pack_ctx, rects, total_glyphs)) {
        fprintf(stderr, "Atlas packing failed! Need larger atlas.\n");
        return 1;
    }

    // Render glyphs into atlas
    for (int fi = 0; fi < (int)NUM_FONTS; fi++) {
        for (int ci = 0; ci < NUM_CHARS; ci++) {
            int idx = fi * NUM_CHARS + ci;
            int ch = FIRST_CHAR + ci;

            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&fontinfos[fi], ch, scales[fi], scales[fi],
                                        &x0, &y0, &x1, &y1);
            int gw = x1 - x0;
            int gh = y1 - y0;
            if (gw <= 0 || gh <= 0) continue;

            int px = rects[idx].x + 1; // +1 for padding
            int py = rects[idx].y + 1;

            stbtt_MakeCodepointBitmap(&fontinfos[fi],
                                      atlas + py * ATLAS_W + px,
                                      gw, gh, ATLAS_W,
                                      scales[fi], scales[fi], ch);

            glyphs[idx].x0 = (float)px / ATLAS_W;
            glyphs[idx].y0 = (float)py / ATLAS_H;
            glyphs[idx].x1 = (float)(px + gw) / ATLAS_W;
            glyphs[idx].y1 = (float)(py + gh) / ATLAS_H;
        }
    }

    // Write atlas PNG
    char path[512];
    snprintf(path, sizeof(path), "%s/font_atlas.png", outdir);
    stbi_write_png(path, ATLAS_W, ATLAS_H, 1, atlas, ATLAS_W);
    printf("Wrote %s (%dx%d)\n", path, ATLAS_W, ATLAS_H);

    // Write header with glyph data
    snprintf(path, sizeof(path), "%s/font_atlas.h", outdir);
    FILE *hf = fopen(path, "w");
    if (!hf) { fprintf(stderr, "Cannot write %s\n", path); return 1; }

    fprintf(hf, "// font_atlas.h — AUTO-GENERATED by bake_font\n");
    fprintf(hf, "// Atlas: %dx%d, %d fonts, %d glyphs each (ASCII %d-%d)\n\n",
            ATLAS_W, ATLAS_H, (int)NUM_FONTS, NUM_CHARS, FIRST_CHAR, LAST_CHAR);
    fprintf(hf, "#ifndef FONT_ATLAS_DATA_H\n#define FONT_ATLAS_DATA_H\n\n");

    fprintf(hf, "#define FONT_ATLAS_W %d\n", ATLAS_W);
    fprintf(hf, "#define FONT_ATLAS_HT %d\n", ATLAS_H);
    fprintf(hf, "#define FONT_FIRST_CHAR %d\n", FIRST_CHAR);
    fprintf(hf, "#define FONT_LAST_CHAR %d\n", LAST_CHAR);
    fprintf(hf, "#define FONT_NUM_CHARS %d\n", NUM_CHARS);
    fprintf(hf, "#define FONT_NUM_FONTS %d\n\n", (int)NUM_FONTS);

    // Font slot enum
    fprintf(hf, "enum {\n");
    for (int fi = 0; fi < (int)NUM_FONTS; fi++)
        fprintf(hf, "    FONT_%s = %d,\n", fonts[fi].name, fi);
    fprintf(hf, "};\n\n");

    // Font sizes (for measurement)
    fprintf(hf, "static const float font_sizes[FONT_NUM_FONTS] = {\n");
    for (int fi = 0; fi < (int)NUM_FONTS; fi++)
        fprintf(hf, "    %.1ff, // %s\n", fonts[fi].size_px, fonts[fi].name);
    fprintf(hf, "};\n\n");

    // Ascent/descent per font
    fprintf(hf, "static const float font_ascent[FONT_NUM_FONTS] = {\n");
    for (int fi = 0; fi < (int)NUM_FONTS; fi++) {
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&fontinfos[fi], &asc, &desc, &gap);
        fprintf(hf, "    %.2ff,\n", asc * scales[fi]);
    }
    fprintf(hf, "};\n\n");

    // Glyph table
    fprintf(hf, "typedef struct {\n");
    fprintf(hf, "    float x0, y0, x1, y1; // atlas texcoords\n");
    fprintf(hf, "    float w, h;           // pixel size\n");
    fprintf(hf, "    float xoff, yoff;     // offset from cursor\n");
    fprintf(hf, "    float advance;        // horizontal advance\n");
    fprintf(hf, "} FontGlyph;\n\n");

    fprintf(hf, "static const FontGlyph font_glyphs[FONT_NUM_FONTS][FONT_NUM_CHARS] = {\n");
    for (int fi = 0; fi < (int)NUM_FONTS; fi++) {
        fprintf(hf, "    { // %s (%.0fpx)\n", fonts[fi].name, fonts[fi].size_px);
        for (int ci = 0; ci < NUM_CHARS; ci++) {
            int idx = fi * NUM_CHARS + ci;
            GlyphInfo *g = &glyphs[idx];
            fprintf(hf, "        {%.6ff,%.6ff,%.6ff,%.6ff, %.1ff,%.1ff, %.1ff,%.1ff, %.2ff}, // '%c'\n",
                    g->x0, g->y0, g->x1, g->y1,
                    g->w, g->h, g->xoff, g->yoff, g->advance,
                    (FIRST_CHAR + ci) >= 32 ? (char)(FIRST_CHAR + ci) : '?');
        }
        fprintf(hf, "    },\n");
    }
    fprintf(hf, "};\n\n");

    // Convenience: measure string width
    fprintf(hf, "static inline float font_measure_width(int font_id, const char *s) {\n");
    fprintf(hf, "    float w = 0;\n");
    fprintf(hf, "    for (; *s; s++) {\n");
    fprintf(hf, "        int ci = *s - FONT_FIRST_CHAR;\n");
    fprintf(hf, "        if (ci >= 0 && ci < FONT_NUM_CHARS)\n");
    fprintf(hf, "            w += font_glyphs[font_id][ci].advance;\n");
    fprintf(hf, "    }\n");
    fprintf(hf, "    return w;\n");
    fprintf(hf, "}\n\n");

    fprintf(hf, "#endif // FONT_ATLAS_DATA_H\n");
    fclose(hf);
    printf("Wrote %s\n", path);

    // Cleanup
    for (int fi = 0; fi < (int)NUM_FONTS; fi++) free(fontbufs[fi]);
    free(atlas);
    free(rects);
    free(glyphs);

    return 0;
}
