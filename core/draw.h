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

// ---- Text rendering (using font_atlas.h data) ----

// Draw a string at (x,y) — top-left origin
static inline float draw_text(DrawCtx *d, int font_id, float x, float y, const char *s) {
    float cursor = x;
    float ascent = font_ascent[font_id];
    for (; *s; s++) {
        int ci = *s - FONT_FIRST_CHAR;
        if (ci < 0 || ci >= FONT_NUM_CHARS) {
            cursor += font_glyphs[font_id][0].advance; // space width for unknown
            continue;
        }
        const FontGlyph *g = &font_glyphs[font_id][ci];
        if (g->w > 0 && g->h > 0) {
            draw_quad_textured(d,
                cursor + g->xoff, y + ascent + g->yoff,
                g->w, g->h,
                g->x0, g->y0, g->x1, g->y1);
        }
        cursor += g->advance;
    }
    return cursor - x;
}

// Draw a string centered at (cx,cy)
static inline void draw_text_centered(DrawCtx *d, int font_id, float cx, float cy, const char *s) {
    float w = font_measure_width(font_id, s);
    float h = font_sizes[font_id];
    draw_text(d, font_id, cx - w * 0.5f, cy - h * 0.5f, s);
}

// Draw a single glyph at (x,y), return advance
static inline float draw_glyph(DrawCtx *d, int font_id, float x, float y, char ch) {
    int ci = ch - FONT_FIRST_CHAR;
    if (ci < 0 || ci >= FONT_NUM_CHARS) return 0;
    const FontGlyph *g = &font_glyphs[font_id][ci];
    float ascent = font_ascent[font_id];
    if (g->w > 0 && g->h > 0) {
        draw_quad_textured(d,
            x + g->xoff, y + ascent + g->yoff,
            g->w, g->h,
            g->x0, g->y0, g->x1, g->y1);
    }
    return g->advance;
}

// Draw curved text along the calendar wheel
static inline void draw_text_curved(DrawCtx *d, int font_id,
                                    float cx, float cy, float radius,
                                    float center_angle, // radians, 0=top
                                    const char *s, float spacing) {
    // Measure total width
    int len = 0;
    float total_w = 0;
    for (const char *p = s; *p; p++) {
        int ci = *p - FONT_FIRST_CHAR;
        if (ci >= 0 && ci < FONT_NUM_CHARS)
            total_w += font_glyphs[font_id][ci].advance * spacing;
        len++;
    }

    // Convert pixel width to arc angle
    float circumference = 2.0f * (float)M_PI * radius;
    float total_angle = total_w / circumference * 2.0f * (float)M_PI;

    // Should we flip text? (bottom half of wheel)
    float norm_angle = fmodf(center_angle, 2.0f * (float)M_PI);
    if (norm_angle < 0) norm_angle += 2.0f * (float)M_PI;
    bool flip = (norm_angle > M_PI * 0.5f && norm_angle < M_PI * 1.5f);

    float angle;
    if (flip)
        angle = center_angle + total_angle * 0.5f;
    else
        angle = center_angle - total_angle * 0.5f;

    float ascent = font_ascent[font_id];
    float font_h = font_sizes[font_id];

    for (const char *p = s; *p; p++) {
        int ci = *p - FONT_FIRST_CHAR;
        if (ci < 0 || ci >= FONT_NUM_CHARS) continue;
        const FontGlyph *g = &font_glyphs[font_id][ci];
        float adv = g->advance * spacing;
        float char_angle = adv / circumference * 2.0f * (float)M_PI;

        float a;
        if (flip)
            a = angle - char_angle * 0.5f;
        else
            a = angle + char_angle * 0.5f;

        // Position on circle
        float px = cx + sinf(a) * radius;
        float py = cy - cosf(a) * radius;

        if (g->w > 0 && g->h > 0) {
            // For each glyph, we draw a rotated quad
            // We need 4 corners of the glyph rect, rotated by the angle
            float gx = -g->w * 0.5f;
            float gy;
            if (flip)
                gy = -font_h + ascent + g->yoff;
            else
                gy = ascent + g->yoff;

            float rot = flip ? (a + (float)M_PI) : a;
            float cs = cosf(rot), sn = sinf(rot);

            float corners[4][2] = {
                { gx,          gy },
                { gx + g->w,   gy },
                { gx + g->w,   gy + g->h },
                { gx,          gy + g->h },
            };

            int base = d->num_verts;
            float uvs[4][2] = {
                { g->x0, g->y0 },
                { g->x1, g->y0 },
                { g->x1, g->y1 },
                { g->x0, g->y1 },
            };

            for (int i = 0; i < 4; i++) {
                float rx = corners[i][0] * cs - corners[i][1] * sn;
                float ry = corners[i][0] * sn + corners[i][1] * cs;
                draw__push_vert(d, px + rx, py + ry, uvs[i][0], uvs[i][1]);
            }
            draw__tri(d, base, base+1, base+2);
            draw__tri(d, base, base+2, base+3);
        }

        if (flip)
            angle -= char_angle;
        else
            angle += char_angle;
    }
}

#endif // DRAW_H
