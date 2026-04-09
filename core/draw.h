// draw.h — Batched 2D drawing primitives for Tempus
// Collects vertices into batches, flushed per-frame via sokol_gfx.
// All coordinates are in world space (centered at 0,0).

#ifndef DRAW_H
#define DRAW_H

#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "../assets/font_atlas.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- Color ----

typedef struct { float r, g, b, a; } DrawColor;

static inline DrawColor dc(float r, float g, float b) {
    return (DrawColor){r, g, b, 1.0f};
}

static inline DrawColor dca(float r, float g, float b, float a) {
    return (DrawColor){r, g, b, a};
}

static inline DrawColor dc_scale(DrawColor c, float s) {
    return (DrawColor){c.r * s, c.g * s, c.b * s, c.a};
}

static inline DrawColor dc_u8(int r, int g, int b) {
    return (DrawColor){r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
}

// ---- Vertex ----

typedef struct {
    float x, y;         // position
    float u, v;         // texcoord
    float r, g, b, a;   // color
} DrawVertex;

// ---- Batch limits ----

#define DRAW_MAX_VERTS   (64 * 1024)
#define DRAW_MAX_INDICES (128 * 1024)
#define DRAW_CIRCLE_SEGS 48

// ---- Draw state ----

typedef struct {
    DrawVertex  verts[DRAW_MAX_VERTS];
    uint16_t    indices[DRAW_MAX_INDICES];
    int         num_verts;
    int         num_indices;

    DrawColor   color;

    // Transform stack (simple: translate + scale, no rotation needed beyond manual)
    float       tx, ty;     // translation
    float       sx, sy;     // scale

    // Screen info
    float       screen_w, screen_h;
    float       display_scalar;     // screen_h / 1280.0

    // White pixel UV (for untextured draws)
    float       white_u, white_v;
} DrawCtx;

static inline void draw_init(DrawCtx *d) {
    memset(d, 0, sizeof(DrawCtx));
    d->color = dc(1, 1, 1);
    d->sx = 1.0f;
    d->sy = 1.0f;
    // White pixel at atlas (0,0) — use half-pixel center for clean sampling
    d->white_u = 0.5f / FONT_ATLAS_W;
    d->white_v = 0.5f / FONT_ATLAS_HT;
}

static inline void draw_begin(DrawCtx *d, float screen_w, float screen_h) {
    d->num_verts = 0;
    d->num_indices = 0;
    d->screen_w = screen_w;
    d->screen_h = screen_h;
    d->display_scalar = screen_h / 1280.0f;
    d->tx = 0;
    d->ty = 0;
    d->sx = 1.0f;
    d->sy = 1.0f;
    d->color = dc(1, 1, 1);
}

static inline void draw_set_color(DrawCtx *d, DrawColor c) {
    d->color = c;
}

static inline void draw_translate(DrawCtx *d, float x, float y) {
    d->tx += x * d->sx;
    d->ty += y * d->sy;
}

static inline void draw_set_translate(DrawCtx *d, float x, float y) {
    d->tx = x;
    d->ty = y;
}

static inline void draw_set_scale(DrawCtx *d, float sx, float sy) {
    d->sx = sx;
    d->sy = sy;
}

// ---- Low-level vertex/index push ----

static inline int draw__push_vert(DrawCtx *d, float x, float y, float u, float v) {
    if (d->num_verts >= DRAW_MAX_VERTS) return d->num_verts - 1;
    float wx = d->tx + x * d->sx;
    float wy = d->ty + y * d->sy;
    // Convert to NDC: center of screen = 0,0
    float nx = wx / (d->screen_w * 0.5f);
    float ny = -wy / (d->screen_h * 0.5f); // flip Y for GL
    d->verts[d->num_verts] = (DrawVertex){
        nx, ny, u, v,
        d->color.r, d->color.g, d->color.b, d->color.a
    };
    return d->num_verts++;
}

static inline void draw__push_idx(DrawCtx *d, uint16_t i) {
    if (d->num_indices < DRAW_MAX_INDICES)
        d->indices[d->num_indices++] = i;
}

static inline void draw__tri(DrawCtx *d, int a, int b, int c) {
    draw__push_idx(d, (uint16_t)a);
    draw__push_idx(d, (uint16_t)b);
    draw__push_idx(d, (uint16_t)c);
}

// ---- Primitives (untextured, using white_u/white_v) ----

static inline void draw_line(DrawCtx *d, float x0, float y0, float x1, float y1, float width) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float hw = width * 0.5f;
    float nx = -dy / len * hw;
    float ny = dx / len * hw;

    float u = d->white_u, v = d->white_v;
    int base = d->num_verts;
    draw__push_vert(d, x0 + nx, y0 + ny, u, v);
    draw__push_vert(d, x0 - nx, y0 - ny, u, v);
    draw__push_vert(d, x1 - nx, y1 - ny, u, v);
    draw__push_vert(d, x1 + nx, y1 + ny, u, v);
    draw__tri(d, base, base+1, base+2);
    draw__tri(d, base, base+2, base+3);
}

// Thin line (1px at display scale)
static inline void draw_line_thin(DrawCtx *d, float x0, float y0, float x1, float y1) {
    draw_line(d, x0, y0, x1, y1, 1.0f);
}

static inline void draw_rect_filled(DrawCtx *d, float x0, float y0, float x1, float y1) {
    float u = d->white_u, v = d->white_v;
    int base = d->num_verts;
    draw__push_vert(d, x0, y0, u, v);
    draw__push_vert(d, x1, y0, u, v);
    draw__push_vert(d, x1, y1, u, v);
    draw__push_vert(d, x0, y1, u, v);
    draw__tri(d, base, base+1, base+2);
    draw__tri(d, base, base+2, base+3);
}

static inline void draw_circle_filled(DrawCtx *d, float cx, float cy, float radius) {
    float u = d->white_u, v = d->white_v;
    int center = draw__push_vert(d, cx, cy, u, v);
    int first = -1;
    for (int i = 0; i <= DRAW_CIRCLE_SEGS; i++) {
        float a = (float)i / DRAW_CIRCLE_SEGS * (float)(M_PI * 2.0);
        int vi = draw__push_vert(d, cx + cosf(a) * radius, cy + sinf(a) * radius, u, v);
        if (i > 0)
            draw__tri(d, center, vi - 1, vi);
        else
            first = vi;
    }
    (void)first;
}

static inline void draw_circle_stroked(DrawCtx *d, float cx, float cy, float radius, float width) {
    float u = d->white_u, v = d->white_v;
    float hw = width * 0.5f;
    float ri = radius - hw;
    float ro = radius + hw;
    int base = d->num_verts;
    for (int i = 0; i <= DRAW_CIRCLE_SEGS; i++) {
        float a = (float)i / DRAW_CIRCLE_SEGS * (float)(M_PI * 2.0);
        float cs = cosf(a), sn = sinf(a);
        draw__push_vert(d, cx + cs * ri, cy + sn * ri, u, v);
        draw__push_vert(d, cx + cs * ro, cy + sn * ro, u, v);
        if (i > 0) {
            int j = base + (i - 1) * 2;
            int k = base + i * 2;
            draw__tri(d, j, j+1, k+1);
            draw__tri(d, j, k+1, k);
        }
    }
}

// Arc: filled ring segment from angle a0 to a1 (radians), inner/outer radius
static inline void draw_arc_filled(DrawCtx *d, float cx, float cy,
                                   float r_inner, float r_outer,
                                   float a0, float a1, int segments) {
    float u = d->white_u, v = d->white_v;
    int base = d->num_verts;
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float a = a0 + (a1 - a0) * t;
        float cs = cosf(a), sn = sinf(a);
        draw__push_vert(d, cx + cs * r_inner, cy + sn * r_inner, u, v);
        draw__push_vert(d, cx + cs * r_outer, cy + sn * r_outer, u, v);
        if (i > 0) {
            int j = base + (i - 1) * 2;
            int k = base + i * 2;
            draw__tri(d, j, j+1, k+1);
            draw__tri(d, j, k+1, k);
        }
    }
}

// ---- Textured quad (for font glyphs) ----

static inline void draw_quad_textured(DrawCtx *d,
                                      float x, float y, float w, float h,
                                      float u0, float v0, float u1, float v1) {
    int base = d->num_verts;
    draw__push_vert(d, x,     y,     u0, v0);
    draw__push_vert(d, x + w, y,     u1, v0);
    draw__push_vert(d, x + w, y + h, u1, v1);
    draw__push_vert(d, x,     y + h, u0, v1);
    draw__tri(d, base, base+1, base+2);
    draw__tri(d, base, base+2, base+3);
}

// ---- Text rendering (SDF glyph data, normalized metrics) ----
// All glyph metrics are normalized (divide by bake_size). Multiply by desired
// pixel size at render time. This lets one atlas work at any display size.

// Draw a string at (x,y) with given pixel size. weight = SDF_WEIGHT_*.
static inline float draw_text_ex(DrawCtx *d, int weight, float size,
                                 float x, float y, const char *s) {
    float cursor = x;
    float ascent = sdf_nascent[weight] * size;
    for (; *s; s++) {
        int ci = *s - SDF_FIRST_CHAR;
        if (ci < 0 || ci >= SDF_NUM_CHARS) {
            cursor += sdf_glyphs[weight][0].nadvance * size;
            continue;
        }
        const SdfGlyph *g = &sdf_glyphs[weight][ci];
        if (g->nw > 0 && g->nh > 0) {
            draw_quad_textured(d,
                cursor + g->nxoff * size, y + ascent + g->nyoff * size,
                g->nw * size, g->nh * size,
                g->u0, g->v0, g->u1, g->v1);
        }
        cursor += g->nadvance * size;
    }
    return cursor - x;
}

// Compat wrapper: draw_text(d, FONT_clock, x, y, s)
// FONT_clock etc. expand to integer font_id — but now these map to old-style
// indexes (0-6). We map them to SDF weights + sizes.
static const struct { int weight; float size; } _font_compat[] = {
    { SDF_WEIGHT_BLACK,  18.0f }, // 0 = logo
    { SDF_WEIGHT_LIGHT,  48.0f }, // 1 = event
    { SDF_WEIGHT_MEDIUM, 24.0f }, // 2 = month
    { SDF_WEIGHT_HEAVY,  14.0f }, // 3 = seconds
    { SDF_WEIGHT_LIGHT,  48.0f }, // 4 = minutes
    { SDF_WEIGHT_MEDIUM, 38.0f }, // 5 = clock
    { SDF_WEIGHT_MEDIUM, 18.0f }, // 6 = date
};

static inline float draw_text(DrawCtx *d, int font_id, float x, float y, const char *s) {
    return draw_text_ex(d, _font_compat[font_id].weight, _font_compat[font_id].size, x, y, s);
}

static inline void draw_text_centered(DrawCtx *d, int font_id, float cx, float cy, const char *s) {
    int w_id = _font_compat[font_id].weight;
    float sz = _font_compat[font_id].size;
    float tw = sdf_measure_width(w_id, s) * sz;
    draw_text_ex(d, w_id, sz, cx - tw * 0.5f, cy - sz * 0.5f, s);
}

// Curved text along an arc
static inline void draw_text_curved(DrawCtx *d, int font_id,
                                    float cx, float cy, float radius,
                                    float center_angle,
                                    const char *s, float spacing) {
    int w_id = _font_compat[font_id].weight;
    float sz = _font_compat[font_id].size;

    // Measure total advance in pixels
    float total_w = 0;
    for (const char *p = s; *p; p++) {
        int ci = *p - SDF_FIRST_CHAR;
        if (ci >= 0 && ci < SDF_NUM_CHARS)
            total_w += sdf_glyphs[w_id][ci].nadvance * sz * spacing;
    }

    float circumference = 2.0f * (float)M_PI * radius;
    float total_angle = total_w / circumference * 2.0f * (float)M_PI;

    float norm_angle = fmodf(center_angle, 2.0f * (float)M_PI);
    if (norm_angle < 0) norm_angle += 2.0f * (float)M_PI;
    bool flip = (norm_angle > M_PI * 0.5f && norm_angle < M_PI * 1.5f);

    float angle = flip
        ? center_angle + total_angle * 0.5f
        : center_angle - total_angle * 0.5f;

    float ascent = sdf_nascent[w_id] * sz;

    for (const char *p = s; *p; p++) {
        int ci = *p - SDF_FIRST_CHAR;
        if (ci < 0 || ci >= SDF_NUM_CHARS) continue;
        const SdfGlyph *g = &sdf_glyphs[w_id][ci];
        float adv = g->nadvance * sz * spacing;
        float char_angle = adv / circumference * 2.0f * (float)M_PI;

        float a = flip
            ? angle - char_angle * 0.5f
            : angle + char_angle * 0.5f;

        float px = cx + sinf(a) * radius;
        float py = cy - cosf(a) * radius;

        if (g->nw > 0 && g->nh > 0) {
            float gw = g->nw * sz;
            float gh = g->nh * sz;
            float gx = -gw * 0.5f;
            float gy = flip
                ? -sz + ascent + g->nyoff * sz
                : ascent + g->nyoff * sz;

            float rot = flip ? (a + (float)M_PI) : a;
            float cs = cosf(rot), sn = sinf(rot);

            float corners[4][2] = {
                { gx,      gy },
                { gx + gw, gy },
                { gx + gw, gy + gh },
                { gx,      gy + gh },
            };
            float uvs[4][2] = {
                { g->u0, g->v0 }, { g->u1, g->v0 },
                { g->u1, g->v1 }, { g->u0, g->v1 },
            };

            int base = d->num_verts;
            for (int i = 0; i < 4; i++) {
                float rx = corners[i][0] * cs - corners[i][1] * sn;
                float ry = corners[i][0] * sn + corners[i][1] * cs;
                draw__push_vert(d, px + rx, py + ry, uvs[i][0], uvs[i][1]);
            }
            draw__tri(d, base, base+1, base+2);
            draw__tri(d, base, base+2, base+3);
        }

        if (flip) angle -= char_angle;
        else      angle += char_angle;
    }
}

#endif // DRAW_H
