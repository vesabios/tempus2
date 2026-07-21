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

// 32-bit indices: the machina<->caelum morph renders both worlds at
// once and blew straight through the old 64k/uint16 budget — the
// silent drop ate whatever drew last (the menu text, famously)
#define DRAW_MAX_VERTS   (128 * 1024)
#define DRAW_MAX_INDICES (256 * 1024)
#define DRAW_CIRCLE_SEGS 48

// ---- Draw state ----

// ---- Globe commands ----
// A view can request a 3D earth globe be composited into the batch at
// the current point: 2D indices before split_index draw first, then the
// globe, then the rest. Views compute the orientation (earth->view
// rotation) and sun direction themselves (see globe.h); the shell owns
// the GPU pipeline. Up to DRAW_MAX_GLOBES per frame (geocentric dial +
// heliocentric orrery can coexist during transitions).

#define DRAW_MAX_GLOBES 3

typedef struct {
    float cx, cy, radius;       // world coords (transform already applied)
    float rot[16];              // earth -> view rotation, column-major
    float light[3];             // sun direction, view frame
    int   overlay;              // GlobeOverlay mode
    float declination;          // current solar declination, degrees
    float obs_lat;              // observer latitude, degrees (latitude ring)
    float grid_boost;           // graticule alpha multiplier
    float alpha;                // whole-sphere fade (defaults to view alpha)
    bool  land;                 // sample the surface texture
    float land_mix;             // surface-texture strength (1 = full; the
                                // system view fades the continents out)
    int   tex_id;               // 0 = Earth land mask, 1 = Moon albedo
    float aux_dir[4];           // observer direction (w > 0 = enabled):
                                // lit surface facing away from it is dimmed
    float day_col[4];           // per-body palette; w > 0 = use these
    float night_col[4];         // (otherwise the style's Earth colors)
    int   split_index;
} GlobeCmd;

typedef struct {
    DrawVertex  verts[DRAW_MAX_VERTS];
    uint32_t    indices[DRAW_MAX_INDICES];
    int         num_verts;
    int         num_indices;

    GlobeCmd    globes[DRAW_MAX_GLOBES];
    int         num_globes;

    DrawColor   color;
    float       alpha;      // global multiplier (view opacity crossfades)

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
    d->alpha = 1.0f;
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
    d->alpha = 1.0f;
    d->num_globes = 0;
}

// Claim a globe slot at (cx, cy) with the given radius (local coords, the
// current transform is applied). Later 2D geometry draws on top of it.
// Caller fills rot/light (see globe.h) and overlay fields. NULL if full.
static inline GlobeCmd *draw_globe_slot(DrawCtx *d, float cx, float cy,
                                        float radius) {
    if (d->num_globes >= DRAW_MAX_GLOBES) return NULL;
    GlobeCmd *g = &d->globes[d->num_globes++];
    memset(g, 0, sizeof(*g));
    g->cx = d->tx + cx * d->sx;
    g->cy = d->ty + cy * d->sy;
    g->radius = radius * d->sx;
    g->alpha = d->alpha;    // globes inherit the view's opacity
    g->land = true;
    g->land_mix = 1.0f;
    g->split_index = d->num_indices;
    return g;
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
        d->color.r, d->color.g, d->color.b, d->color.a * d->alpha
    };
    return d->num_verts++;
}

static inline void draw__push_idx(DrawCtx *d, uint32_t i) {
    if (d->num_indices < DRAW_MAX_INDICES)
        d->indices[d->num_indices++] = i;
}

static inline void draw__tri(DrawCtx *d, int a, int b, int c) {
    draw__push_idx(d, (uint32_t)a);
    draw__push_idx(d, (uint32_t)b);
    draw__push_idx(d, (uint32_t)c);
}

// ---- Primitives (untextured, using white_u/white_v) ----

// One screen pixel expressed in local (pre-transform) units.
static inline float draw__px(const DrawCtx *d) {
    float s = d->sx != 0.0f ? d->sx : 1.0f;
    return 1.0f / fabsf(s);
}

// Push a vertex with an explicit alpha override (keeps rgb from d->color).
static inline int draw__push_vert_a(DrawCtx *d, float x, float y,
                                    float u, float v, float alpha) {
    DrawColor saved = d->color;
    d->color.a *= alpha;
    int idx = draw__push_vert(d, x, y, u, v);
    d->color = saved;
    return idx;
}

// Feathered line: a 1-screen-pixel alpha ramp on each edge kills aliasing
// and moire in dense tick rings. Lines thinner than a pixel render as
// proportionally dimmer full-pixel lines (energy conservation) instead of
// snapping in and out of the pixel grid.
static inline void draw_line(DrawCtx *d, float x0, float y0, float x1, float y1, float width) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float px = draw__px(d);
    float alpha = 1.0f;
    if (width < px) {
        alpha = width / px;
        width = px;
    }
    float hw = width * 0.5f;
    float core = hw - px * 0.5f;   // full-alpha half-width
    if (core < 0) core = 0;
    float edge = hw + px * 0.5f;   // zero-alpha half-width

    float ux = -dy / len, uy = dx / len;
    float u = d->white_u, v = d->white_v;

    // Four vertex rows: 0-alpha edge, core, core, 0-alpha edge
    float offs[4]  = { -edge, -core, core, edge };
    float alphas[4] = { 0.0f, alpha, alpha, 0.0f };
    int base = d->num_verts;
    for (int r = 0; r < 4; r++) {
        float nx = ux * offs[r], ny = uy * offs[r];
        draw__push_vert_a(d, x0 + nx, y0 + ny, u, v, alphas[r]);
        draw__push_vert_a(d, x1 + nx, y1 + ny, u, v, alphas[r]);
    }
    for (int r = 0; r < 3; r++) {
        int j = base + r * 2;
        draw__tri(d, j, j+1, j+3);
        draw__tri(d, j, j+3, j+2);
    }
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
    float px = draw__px(d);
    float alpha = 1.0f;
    if (width < px) {
        alpha = width / px;
        width = px;
    }
    float hw = width * 0.5f;
    float core = hw - px * 0.5f;
    if (core < 0) core = 0;
    float edge = hw + px * 0.5f;

    float radii[4]  = { radius - edge, radius - core, radius + core, radius + edge };
    float alphas[4] = { 0.0f, alpha, alpha, 0.0f };
    int base = d->num_verts;
    for (int i = 0; i <= DRAW_CIRCLE_SEGS; i++) {
        float a = (float)i / DRAW_CIRCLE_SEGS * (float)(M_PI * 2.0);
        float cs = cosf(a), sn = sinf(a);
        for (int r = 0; r < 4; r++)
            draw__push_vert_a(d, cx + cs * radii[r], cy + sn * radii[r], u, v, alphas[r]);
        if (i > 0) {
            int j = base + (i - 1) * 4;
            int k = base + i * 4;
            for (int r = 0; r < 3; r++) {
                draw__tri(d, j+r, j+r+1, k+r+1);
                draw__tri(d, j+r, k+r+1, k+r);
            }
        }
    }
}

// Arc: filled ring segment from angle a0 to a1 (radians), inner/outer radius.
// Radial edges are feathered by one screen pixel.
static inline void draw_arc_filled(DrawCtx *d, float cx, float cy,
                                   float r_inner, float r_outer,
                                   float a0, float a1, int segments) {
    float u = d->white_u, v = d->white_v;
    float px = draw__px(d);

    float radii[4]  = { r_inner - px, r_inner, r_outer, r_outer + px };
    float alphas[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    if (radii[0] < 0) radii[0] = 0;

    int base = d->num_verts;
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float a = a0 + (a1 - a0) * t;
        float cs = cosf(a), sn = sinf(a);
        for (int r = 0; r < 4; r++)
            draw__push_vert_a(d, cx + cs * radii[r], cy + sn * radii[r], u, v, alphas[r]);
        if (i > 0) {
            int j = base + (i - 1) * 4;
            int k = base + i * 4;
            for (int r = 0; r < 3; r++) {
                draw__tri(d, j+r, j+r+1, k+r+1);
                draw__tri(d, j+r, k+r+1, k+r);
            }
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
// Curved text along a circle. `tracking_em` is ADDITIVE letterspacing in
// em added to every advance — constant optical gap regardless of glyph
// width. (A multiplier here makes wide letters carry wide gaps, which
// reads as uneven spacing once display text is tracked out.)
// Radial (spoke-aligned) text: the baseline runs along the radius at
// wheel angle theta (0 = top, matching cal__fc). `anchor_r` is the OUTER
// end of the text; it extends inward from there. On the left half of the
// wheel glyphs rotate 180 degrees for legibility (classic dial flip).
static inline void draw_text_radial(DrawCtx *d, int font_id,
                                    float cx, float cy, float anchor_r,
                                    float theta, const char *s, float scale) {
    int w_id = _font_compat[font_id].weight;
    float sz = _font_compat[font_id].size * scale;

    float total_w = 0;
    for (const char *p = s; *p; p++) {
        int ci = *p - SDF_FIRST_CHAR;
        if (ci >= 0 && ci < SDF_NUM_CHARS)
            total_w += sdf_glyphs[w_id][ci].nadvance * sz;
    }

    float rx = sinf(theta), ry = -cosf(theta);   // outward radial unit
    bool flip = rx < 0;                          // left half of the wheel
    float rot = atan2f(ry, rx) + (flip ? (float)M_PI : 0.0f);
    float cs = cosf(rot), sn = sinf(rot);
    float ascent = sdf_nascent[w_id] * sz;

    // Pen advances along local +x: outward normally; inward when flipped
    float pen = flip ? anchor_r : anchor_r - total_w;
    float sign = flip ? -1.0f : 1.0f;

    for (const char *p = s; *p; p++) {
        int ci = *p - SDF_FIRST_CHAR;
        if (ci < 0 || ci >= SDF_NUM_CHARS) continue;
        const SdfGlyph *g = &sdf_glyphs[w_id][ci];
        float adv = g->nadvance * sz;
        float mid = pen + sign * adv * 0.5f;

        if (g->nw > 0 && g->nh > 0) {
            float gw = g->nw * sz;
            float gh = g->nh * sz;
            float gx = -gw * 0.5f;
            // Center the em box on the spoke line
            float gy = ascent - sz * 0.5f + g->nyoff * sz;
            float px = cx + rx * mid, py = cy + ry * mid;
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
                float qx = corners[i][0] * cs - corners[i][1] * sn;
                float qy = corners[i][0] * sn + corners[i][1] * cs;
                draw__push_vert(d, px + qx, py + qy, uvs[i][0], uvs[i][1]);
            }
            draw__tri(d, base, base+1, base+2);
            draw__tri(d, base, base+2, base+3);
        }
        pen += sign * adv;
    }
}

// The RADIAL SHIFT between draw_text_curved's two branches. The flip
// branch drops gy by exactly one em AND rotates the frame 180, so the
// glyph lands on the other side of the baseline circle. A caller that
// wants curved text to sit at a CONSTANT DEPTH all the way round must
// move the unflipped baseline inward by this much:
//
//   non-flip glyph spans inward [gy, gy+gh]
//   flip     glyph spans inward [sz-gy-gh, sz-gy]
//   equal depth  =>  shift = sz - 2*gy - gh
//
// Derived from the same metrics the drawer uses, so it tracks the font
// rather than being a fitted constant. (Callers used to guess: the
// per-day numerals used 0.8*size, which is ~4x too much and threw the
// numerals 30 units inward every time they crossed 3 or 9 o'clock.)
static inline float draw_text_curved_flip_shift(int font_id, const char *s,
                                                float scale) {
    int w_id = _font_compat[font_id].weight;
    float sz = _font_compat[font_id].size * scale;
    float ascent = sdf_nascent[w_id] * sz;
    float gy = 0, gh = 0;
    bool any = false;
    for (const char *p = s; *p; p++) {
        int ci = *p - SDF_FIRST_CHAR;
        if (ci < 0 || ci >= SDF_NUM_CHARS) continue;
        const SdfGlyph *g = &sdf_glyphs[w_id][ci];
        if (g->nh <= 0) continue;
        float h = g->nh * sz;
        if (!any || h > gh) { gh = h; gy = ascent + g->nyoff * sz; any = true; }
    }
    return any ? (sz - 2.0f * gy - gh) : 0.0f;
}

static inline void draw_text_curved(DrawCtx *d, int font_id,
                                    float cx, float cy, float radius,
                                    float center_angle,
                                    const char *s, float tracking_em,
                                    float scale) {
    int w_id = _font_compat[font_id].weight;
    float sz = _font_compat[font_id].size * scale;

    // Measure total advance in pixels
    float total_w = 0;
    for (const char *p = s; *p; p++) {
        int ci = *p - SDF_FIRST_CHAR;
        if (ci >= 0 && ci < SDF_NUM_CHARS)
            total_w += (sdf_glyphs[w_id][ci].nadvance + tracking_em) * sz;
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
        float adv = (g->nadvance + tracking_em) * sz;
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
