// glyph_sheet.c — contact sheet of candidate quarter-point glyphs.
//
// The calendar wheel marks the two solstices and two equinoxes with
// icons that are currently a stroked circle, a filled circle and two
// half-discs — which is to say new moon, full moon, and quarter moon
// twice (Seren: "too similar to the moon in its phases"). The two
// equinoxes are also identical to each other, so Ostara and Mabon
// cannot be told apart at all.
//
// These are candidate replacements, drawn from REAL STROKE TABLES in
// the instrument's own format — the same run-length/unit-point tables
// core/sigils.h ships — so whichever family is chosen is already the
// implementation, not a mockup that then has to be redrawn.
//
// Build & run: make glyph_sheet   (writes glyph_sheet.png)

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

#define OUT_W 1500
#define OUT_H 1180

static unsigned char atlas_data[FONT_ATLAS_W * FONT_ATLAS_HT];
static unsigned char output[OUT_W * OUT_H];

static float sample_sdf(float u, float v) {
    float fx = u * FONT_ATLAS_W - 0.5f, fy = v * FONT_ATLAS_HT - 0.5f;
    int ix = (int)floorf(fx), iy = (int)floorf(fy);
    float dx = fx - ix, dy = fy - iy;
    int x0 = ix < 0 ? 0 : (ix >= FONT_ATLAS_W ? FONT_ATLAS_W - 1 : ix);
    int x1 = x0 + 1 >= FONT_ATLAS_W ? FONT_ATLAS_W - 1 : x0 + 1;
    int y0 = iy < 0 ? 0 : (iy >= FONT_ATLAS_HT ? FONT_ATLAS_HT - 1 : iy);
    int y1 = y0 + 1 >= FONT_ATLAS_HT ? FONT_ATLAS_HT - 1 : y0 + 1;
    float s00 = atlas_data[y0 * FONT_ATLAS_W + x0] / 255.0f;
    float s10 = atlas_data[y0 * FONT_ATLAS_W + x1] / 255.0f;
    float s01 = atlas_data[y1 * FONT_ATLAS_W + x0] / 255.0f;
    float s11 = atlas_data[y1 * FONT_ATLAS_W + x1] / 255.0f;
    float top = s00 + (s10 - s00) * dx, bot = s01 + (s11 - s01) * dx;
    return top + (bot - top) * dy;
}

static void text(const char *s, int weight, float size, int ox, int oy,
                 unsigned char maxv) {
    float ascent = sdf_nascent[weight] * size, cursor = 0;
    for (; *s; s++) {
        int ci = *s - SDF_FIRST_CHAR;
        if (ci < 0 || ci >= SDF_NUM_CHARS) { cursor += sdf_glyphs[weight][0].nadvance * size; continue; }
        const SdfGlyph *g = &sdf_glyphs[weight][ci];
        if (g->nw <= 0 || g->nh <= 0) { cursor += g->nadvance * size; continue; }
        float gx = cursor + g->nxoff * size, gy = ascent + g->nyoff * size;
        float gw = g->nw * size, gh = g->nh * size;
        for (int py = (int)(oy + gy); py <= (int)(oy + gy + gh + 1); py++)
            for (int px = (int)(ox + gx); px <= (int)(ox + gx + gw + 1); px++) {
                if (px < 0 || px >= OUT_W || py < 0 || py >= OUT_H) continue;
                float t = (float)(px - ox - gx) / gw;
                float sv = (float)(py - oy - gy) / gh;
                float d = sample_sdf(g->u0 + (g->u1 - g->u0) * t,
                                     g->v0 + (g->v1 - g->v0) * sv);
                float a = (d - 0.48f) / 0.04f;
                if (a < 0) a = 0; if (a > 1) a = 1;
                unsigned char val = (unsigned char)(a * maxv);
                if (val > output[py * OUT_W + px]) output[py * OUT_W + px] = val;
            }
        cursor += g->nadvance * size;
    }
}

// Anti-aliased line, additive-max into the buffer
static void line(float x0, float y0, float x1, float y1, float w,
                 unsigned char v) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    int bx0 = (int)floorf(fminf(x0, x1) - w - 1), bx1 = (int)ceilf(fmaxf(x0, x1) + w + 1);
    int by0 = (int)floorf(fminf(y0, y1) - w - 1), by1 = (int)ceilf(fmaxf(y0, y1) + w + 1);
    for (int py = by0; py <= by1; py++)
        for (int px = bx0; px <= bx1; px++) {
            if (px < 0 || px >= OUT_W || py < 0 || py >= OUT_H) continue;
            float t = ((px - x0) * dx + (py - y0) * dy) / (len * len);
            if (t < 0) t = 0; if (t > 1) t = 1;
            float cx = x0 + dx * t, cy = y0 + dy * t;
            float dist = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
            float a = 1.0f - (dist - w * 0.5f) / 1.2f;
            if (a <= 0) continue; if (a > 1) a = 1;
            unsigned char val = (unsigned char)(a * v);
            if (val > output[py * OUT_W + px]) output[py * OUT_W + px] = val;
        }
}

// Draw a stroke table exactly as sigil_strokes does, upright
static void strokes(const float *g, float cx, float cy, float sz, float w,
                    unsigned char v) {
    int i = 0;
    while (g[i] > 0.5f) {
        int n = (int)g[i++];
        float lx = 0, ly = 0;
        for (int k = 0; k < n; k++) {
            float gx = g[i++], gy = g[i++];
            float sx = cx + gx * sz, sy = cy - gy * sz;   // y up
            if (k) line(lx, ly, sx, sy, w, v);
            lx = sx; ly = sy;
        }
    }
}

// ---------------------------------------------------------------
// The candidates. Unit square, y up, drawn at ~0.35 reach like the
// node sigils in core/sigils.h.
// ---------------------------------------------------------------

// --- A. CARDINAL ZODIAC SIGILS -------------------------------
// The quarter points ARE the cardinal ingresses: the vernal equinox is
// the instant the sun enters Aries. The glyph names the moment.
// VERBATIM from orr__glyphs in view_orrery.h — these are the sigils
// the instrument already engraves on MACHINA's zodiac band, so this
// row shows the real thing rather than an approximation of it. (They
// live behind SCENE_DEFINED, hence the copy; if this family wins they
// move to core/sigils.h beside the node figures and there is one copy
// again.) Note they reach 0.50, not 0.35 — a touch larger than the
// other families here, exactly as they render on the band.
static const float A_capricorn[] = {   // Yule
    3, -0.46f,0.34f, -0.32f,-0.08f, -0.20f,0.30f,
    10, -0.20f,0.30f, -0.12f,0.02f, -0.06f,-0.20f, 0.04f,-0.38f,
        0.20f,-0.44f, 0.34f,-0.38f, 0.38f,-0.24f, 0.30f,-0.10f,
        0.16f,-0.08f, 0.06f,-0.16f, 0 };
static const float A_aries[] = {       // Ostara
    7, 0,-0.50f, -0.05f,-0.20f, -0.13f,0.05f, -0.22f,0.26f,
       -0.33f,0.38f, -0.42f,0.33f, -0.46f,0.18f,
    7, 0,-0.50f, 0.05f,-0.20f, 0.13f,0.05f, 0.22f,0.26f,
       0.33f,0.38f, 0.42f,0.33f, 0.46f,0.18f, 0 };
static const float A_cancer[] = {      // Litha
    9, -0.11f,0.16f, -0.15f,0.25f, -0.24f,0.29f, -0.33f,0.25f,
       -0.37f,0.16f, -0.33f,0.07f, -0.24f,0.03f, -0.15f,0.07f,
       -0.11f,0.16f,
    4, -0.24f,0.29f, -0.02f,0.34f, 0.20f,0.29f, 0.36f,0.16f,
    9, 0.11f,-0.16f, 0.15f,-0.25f, 0.24f,-0.29f, 0.33f,-0.25f,
       0.37f,-0.16f, 0.33f,-0.07f, 0.24f,-0.03f, 0.15f,-0.07f,
       0.11f,-0.16f,
    4, 0.24f,-0.29f, 0.02f,-0.34f, -0.20f,-0.29f, -0.36f,-0.16f, 0 };
static const float A_libra[] = {       // Mabon
    2, -0.42f,-0.34f, 0.42f,-0.34f,
    9, -0.42f,-0.12f, -0.20f,-0.12f, -0.18f,0.06f, -0.10f,0.18f,
       0,0.22f, 0.10f,0.18f, 0.18f,0.06f, 0.20f,-0.12f,
       0.42f,-0.12f, 0 };

// --- B. SUN ARC OVER THE HORIZON -----------------------------
// The sun's actual path that day: how high it climbs. Shallow at
// Yule, high at Litha, half at the equinoxes — with the arc rising
// or falling so the two equinoxes are no longer twins.
static const float B_low[] = {
    2, -0.34f,-0.18f, 0.34f,-0.18f,
    7, -0.28f,-0.18f, -0.20f,-0.09f, -0.10f,-0.04f, 0.0f,-0.02f,
       0.10f,-0.04f, 0.20f,-0.09f, 0.28f,-0.18f, 0 };
static const float B_high[] = {
    2, -0.34f,-0.18f, 0.34f,-0.18f,
    7, -0.28f,-0.18f, -0.22f,0.08f, -0.12f,0.24f, 0.0f,0.30f,
       0.12f,0.24f, 0.22f,0.08f, 0.28f,-0.18f, 0 };
static const float B_rise[] = {
    2, -0.34f,-0.18f, 0.34f,-0.18f,
    7, -0.28f,-0.18f, -0.21f,0.0f, -0.11f,0.10f, 0.0f,0.14f,
       0.11f,0.10f, 0.21f,0.0f, 0.28f,-0.18f,
    3, 0.10f,0.24f, 0.20f,0.30f, 0.16f,0.19f, 0 };
static const float B_fall[] = {
    2, -0.34f,-0.18f, 0.34f,-0.18f,
    7, -0.28f,-0.18f, -0.21f,0.0f, -0.11f,0.10f, 0.0f,0.14f,
       0.11f,0.10f, 0.21f,0.0f, 0.28f,-0.18f,
    3, -0.10f,0.24f, -0.20f,0.30f, -0.16f,0.19f, 0 };

// --- C. THE TURNING AND THE CROSSING -------------------------
// SOLSTICE means the sun stands still and turns: a chevron at the
// bottom of its fall, or the top of its climb. EQUINOX means the
// crossing of the celestial equator: a line crossed by the ecliptic,
// ascending or descending. Pure geometry, nothing round at all.
static const float C_turn_dn[] = {
    3, -0.30f,0.18f, 0.0f,-0.24f, 0.30f,0.18f,
    2, -0.16f,-0.30f, 0.16f,-0.30f, 0 };
static const float C_turn_up[] = {
    3, -0.30f,-0.18f, 0.0f,0.24f, 0.30f,-0.18f,
    2, -0.16f,0.30f, 0.16f,0.30f, 0 };
static const float C_cross_up[] = {
    2, -0.32f,0.0f, 0.32f,0.0f,
    2, -0.26f,-0.24f, 0.26f,0.24f, 0 };
static const float C_cross_dn[] = {
    2, -0.32f,0.0f, 0.32f,0.0f,
    2, -0.26f,0.24f, 0.26f,-0.24f, 0 };

// --- D. DAY AND NIGHT, AS A MEASURE --------------------------
// A bar divided where the day divides: mostly dark at Yule, mostly
// light at Litha, halved at the equinoxes. A reading, not a symbol —
// the notch says where the light stops.
static const float D_yule[] = {
    5, -0.30f,-0.16f, 0.30f,-0.16f, 0.30f,0.16f, -0.30f,0.16f, -0.30f,-0.16f,
    2, -0.14f,-0.24f, -0.14f,0.24f,
    3, -0.30f,-0.08f, -0.22f,0.0f, -0.30f,0.08f, 0 };
static const float D_litha[] = {
    5, -0.30f,-0.16f, 0.30f,-0.16f, 0.30f,0.16f, -0.30f,0.16f, -0.30f,-0.16f,
    2, 0.14f,-0.24f, 0.14f,0.24f,
    3, 0.30f,-0.08f, 0.22f,0.0f, 0.30f,0.08f, 0 };
static const float D_ostara[] = {
    5, -0.30f,-0.16f, 0.30f,-0.16f, 0.30f,0.16f, -0.30f,0.16f, -0.30f,-0.16f,
    2, 0.0f,-0.24f, 0.0f,0.24f,
    3, 0.10f,0.26f, 0.20f,0.31f, 0.16f,0.21f, 0 };
static const float D_mabon[] = {
    5, -0.30f,-0.16f, 0.30f,-0.16f, 0.30f,0.16f, -0.30f,0.16f, -0.30f,-0.16f,
    2, 0.0f,-0.24f, 0.0f,0.24f,
    3, -0.10f,0.26f, -0.20f,0.31f, -0.16f,0.21f, 0 };

// --- E. THE SUN'S DECLINATION ON A SCALE ---------------------
// A vertical scale with the tropics and the equator marked, and a bar
// where the sun stands: bottom at Yule, top at Litha, dead centre at
// the equinoxes, arrowed for which way it is travelling.
static const float E_bot[] = {
    2, 0.0f,-0.30f, 0.0f,0.30f,
    2, -0.10f,0.26f, 0.10f,0.26f,
    2, -0.07f,0.0f, 0.07f,0.0f,
    2, -0.22f,-0.26f, 0.22f,-0.26f, 0 };
static const float E_top[] = {
    2, 0.0f,-0.30f, 0.0f,0.30f,
    2, -0.22f,0.26f, 0.22f,0.26f,
    2, -0.07f,0.0f, 0.07f,0.0f,
    2, -0.10f,-0.26f, 0.10f,-0.26f, 0 };
static const float E_mid_up[] = {
    2, 0.0f,-0.30f, 0.0f,0.30f,
    2, -0.10f,0.26f, 0.10f,0.26f,
    2, -0.22f,0.0f, 0.22f,0.0f,
    2, -0.10f,-0.26f, 0.10f,-0.26f,
    3, 0.10f,0.10f, 0.18f,0.18f, 0.07f,0.17f, 0 };
static const float E_mid_dn[] = {
    2, 0.0f,-0.30f, 0.0f,0.30f,
    2, -0.10f,0.26f, 0.10f,0.26f,
    2, -0.22f,0.0f, 0.22f,0.0f,
    2, -0.10f,-0.26f, 0.10f,-0.26f,
    3, 0.10f,-0.10f, 0.18f,-0.18f, 0.07f,-0.17f, 0 };

// --- F. RAYED SOLAR DISCS ------------------------------------
// Kept in the running because it is the nearest thing to the current
// canon: still a disc, but rayed, so it reads solar. The risk Seren
// named — that a disc reads as a moon — is not fully gone here.
static const float F_yule[] = {
    9, 0.14f,0.0f, 0.10f,0.10f, 0.0f,0.14f, -0.10f,0.10f, -0.14f,0.0f,
       -0.10f,-0.10f, 0.0f,-0.14f, 0.10f,-0.10f, 0.14f,0.0f,
    2, 0.0f,0.20f, 0.0f,0.26f,
    2, 0.0f,-0.20f, 0.0f,-0.26f,
    2, 0.20f,0.0f, 0.26f,0.0f,
    2, -0.20f,0.0f, -0.26f,0.0f, 0 };
static const float F_litha[] = {
    9, 0.14f,0.0f, 0.10f,0.10f, 0.0f,0.14f, -0.10f,0.10f, -0.14f,0.0f,
       -0.10f,-0.10f, 0.0f,-0.14f, 0.10f,-0.10f, 0.14f,0.0f,
    2, 0.0f,0.19f, 0.0f,0.33f,
    2, 0.0f,-0.19f, 0.0f,-0.33f,
    2, 0.19f,0.0f, 0.33f,0.0f,
    2, -0.19f,0.0f, -0.33f,0.0f,
    2, 0.13f,0.13f, 0.23f,0.23f,
    2, -0.13f,-0.13f, -0.23f,-0.23f,
    2, 0.13f,-0.13f, 0.23f,-0.23f,
    2, -0.13f,0.13f, -0.23f,0.23f, 0 };
static const float F_ostara[] = {
    9, 0.14f,0.0f, 0.10f,0.10f, 0.0f,0.14f, -0.10f,0.10f, -0.14f,0.0f,
       -0.10f,-0.10f, 0.0f,-0.14f, 0.10f,-0.10f, 0.14f,0.0f,
    2, 0.20f,0.0f, 0.32f,0.0f,
    2, -0.20f,0.0f, -0.32f,0.0f,
    3, 0.12f,0.20f, 0.20f,0.27f, 0.09f,0.26f, 0 };
static const float F_mabon[] = {
    9, 0.14f,0.0f, 0.10f,0.10f, 0.0f,0.14f, -0.10f,0.10f, -0.14f,0.0f,
       -0.10f,-0.10f, 0.0f,-0.14f, 0.10f,-0.10f, 0.14f,0.0f,
    2, 0.20f,0.0f, 0.32f,0.0f,
    2, -0.20f,0.0f, -0.32f,0.0f,
    3, -0.12f,0.20f, -0.20f,0.27f, -0.09f,0.26f, 0 };

typedef struct { const char *name, *note; const float *g[4]; } Family;

static const Family FAM[] = {
  { "A  CARDINAL SIGILS", "the sun entering Capricorn / Aries / Cancer / Libra",
    { A_capricorn, A_aries, A_cancer, A_libra } },
  { "B  SUN ARC", "the height the sun climbs that day, over the horizon",
    { B_low, B_rise, B_high, B_fall } },
  { "C  TURN AND CROSSING", "solstice = the sun turns; equinox = it crosses the equator",
    { C_turn_dn, C_cross_up, C_turn_up, C_cross_dn } },
  { "D  DAY AND NIGHT", "a bar divided where the light stops",
    { D_yule, D_ostara, D_litha, D_mabon } },
  { "E  DECLINATION SCALE", "tropics and equator, with the sun's standing marked",
    { E_bot, E_mid_up, E_top, E_mid_dn } },
  { "F  RAYED DISCS", "nearest the current canon - still round, so still moon-ish",
    { F_yule, F_ostara, F_litha, F_mabon } },
};
#define NFAM (int)(sizeof(FAM) / sizeof(FAM[0]))

// The CURRENT glyphs, for comparison: stroked circle, half disc,
// filled circle, half disc — new moon, quarter, full, quarter.
static void draw_current(int col, float cx, float cy, float r) {
    if (col == 0) {
        for (int i = 0; i < 48; i++) {
            float a0 = i / 48.0f * 2 * (float)M_PI, a1 = (i + 1) / 48.0f * 2 * (float)M_PI;
            line(cx + cosf(a0) * r, cy + sinf(a0) * r,
                 cx + cosf(a1) * r, cy + sinf(a1) * r, 1.6f, 200);
        }
    } else if (col == 2) {
        for (float rr = 0; rr <= r; rr += 0.5f)
            for (int i = 0; i < 64; i++) {
                float a0 = i / 64.0f * 2 * (float)M_PI, a1 = (i + 1) / 64.0f * 2 * (float)M_PI;
                line(cx + cosf(a0) * rr, cy + sinf(a0) * rr,
                     cx + cosf(a1) * rr, cy + sinf(a1) * rr, 1.6f, 200);
            }
    } else {
        for (float rr = 0; rr <= r; rr += 0.5f)
            for (int i = 0; i < 64; i++) {
                float a0 = (float)M_PI * 0.5f + i / 64.0f * (float)M_PI;
                float a1 = (float)M_PI * 0.5f + (i + 1) / 64.0f * (float)M_PI;
                line(cx + cosf(a0) * rr, cy + sinf(a0) * rr,
                     cx + cosf(a1) * rr, cy + sinf(a1) * rr, 1.6f, 200);
            }
    }
}

int main(void) {
    int w, h, n;
    unsigned char *data = stbi_load("assets/font_atlas.png", &w, &h, &n, 1);
    if (!data) { fprintf(stderr, "need assets/font_atlas.png (run: make atlas)\n"); return 1; }
    memcpy(atlas_data, data, FONT_ATLAS_W * FONT_ATLAS_HT);
    stbi_image_free(data);
    memset(output, 0, sizeof(output));

    static const char *COL[4] = { "YVLE", "OSTARA", "LITHA", "MABON" };
    static const char *SUB[4] = { "winter solstice", "spring equinox",
                                  "summer solstice", "autumn equinox" };
    const int x0 = 470, dx = 250, y0 = 150, dy = 148;

    text("QUARTER-POINT GLYPHS  -  CANDIDATES", 1, 28, 40, 30, 255);
    text("real stroke tables. left of each pair = the wheel's own size; right = 3x",
         0, 15, 40, 66, 115);

    for (int c = 0; c < 4; c++) {
        text(COL[c], 1, 20, x0 + c * dx - 34, y0 - 76, 220);
        text(SUB[c], 0, 13, x0 + c * dx - 44, y0 - 52, 110);
    }

    // Row 0: what is there now
    text("CURRENT", 1, 19, 40, y0 - 12, 200);
    text("moon phases, and the two", 0, 13, 40, y0 + 14, 105);
    text("equinoxes are identical", 0, 13, 40, y0 + 32, 105);
    for (int c = 0; c < 4; c++) draw_current(c, (float)(x0 + c * dx), (float)y0 + 6, 15.0f);
    for (int x = 40; x < OUT_W - 40; x += 7)
        if (x + 3 < OUT_W) { output[(y0 + 62) * OUT_W + x] = 70; output[(y0 + 62) * OUT_W + x + 1] = 70; }

    for (int f = 0; f < NFAM; f++) {
        int yy = y0 + 100 + f * dy;
        text(FAM[f].name, 1, 19, 40, yy - 12, 210);
        text(FAM[f].note, 0, 12, 40, yy + 16, 105);
        for (int c = 0; c < 4; c++) {
            float cx = (float)(x0 + c * dx), cy = (float)yy + 8;
            // wheel size (r15 -> sz 42 covers the 0.35 reach) and 3x
            strokes(FAM[f].g[c], cx - 46, cy, 42.0f, 1.7f, 225);
            strokes(FAM[f].g[c], cx + 34, cy, 120.0f, 2.6f, 235);
        }
    }

    stbi_write_png("glyph_sheet.png", OUT_W, OUT_H, 1, output, OUT_W);
    printf("wrote glyph_sheet.png (%dx%d)\n", OUT_W, OUT_H);
    return 0;
}
