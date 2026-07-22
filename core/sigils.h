// sigils.h — engraved figures, and the engine that strokes them.
//
// A sigil is not a picture, it is a set of pen strokes: run lengths
// followed by unit-square points, so a figure can be set at any size
// and any orientation on the instrument without a glyph atlas. The
// zodiac signs, the labors of the months, and the dragon's two nodes
// are all drawn this way.
//
// THIS LIVES HERE BECAUSE MORE THAN ONE VIEW NEEDS IT. The stroke
// engine grew up inside the orrery and was already reaching across to
// OFFICIVM and HORAE; when the calendar wheel also came to want the
// node figures, the shared thing had to stop belonging to one view.
// Same law as the luminaries: one object, one owner — and the owner of
// a figure used everywhere is nobody in particular.

#ifndef SIGILS_H
#define SIGILS_H

#include "draw.h"

// Draw a stroke table at (cx,cy): (ux,uy) is glyph-up in screen space
// — pointed radially outward, every figure stands feet-to-center like
// a clock face's engravings. Screen y runs down, so right = (-uy, ux)
// keeps the figure unmirrored.
static void sigil_strokes(DrawCtx *d, const float *g, float cx, float cy,
                          float ux, float uy, float sz, float w) {
    float rx = -uy, ry = ux;
    int i = 0;
    while (g[i] > 0.5f) {
        int n = (int)g[i++];
        float lx = 0, ly = 0;
        for (int k = 0; k < n; k++) {
            float gx = g[i++], gy = g[i++];
            float sx = cx + (rx * gx + ux * gy) * sz;
            float sy = cy + (ry * gx + uy * gy) * sz;
            if (k) draw_line(d, lx, ly, sx, sy, w);
            lx = sx; ly = sy;
        }
    }
}

// ---- The dragon's nodes ----
// CAPVT DRACONIS, the head: a horseshoe opening downward, with its two
// feet. CAVDA, the tail: the same figure inverted. They mark where the
// moon's tilted orbit crosses the ecliptic — the two points where an
// eclipse is possible at all. DRACO sets them on its dial; the
// calendar wheel sets them at the middle of each eclipse season, which
// is the moment the sun actually arrives at that crossing.
static const float sigil_caput[] = {
    7, -0.20f,-0.14f, -0.24f,0.02f, -0.16f,0.18f, 0,0.26f,
       0.16f,0.18f, 0.24f,0.02f, 0.20f,-0.14f,
    9, -0.11f,-0.23f, -0.136f,-0.166f, -0.20f,-0.14f, -0.264f,-0.166f,
       -0.29f,-0.23f, -0.264f,-0.294f, -0.20f,-0.32f, -0.136f,-0.294f,
       -0.11f,-0.23f,
    9, 0.29f,-0.23f, 0.264f,-0.166f, 0.20f,-0.14f, 0.136f,-0.166f,
       0.11f,-0.23f, 0.136f,-0.294f, 0.20f,-0.32f, 0.264f,-0.294f,
       0.29f,-0.23f, 0 };
static const float sigil_cauda[] = {
    7, -0.20f,0.14f, -0.24f,-0.02f, -0.16f,-0.18f, 0,-0.26f,
       0.16f,-0.18f, 0.24f,-0.02f, 0.20f,0.14f,
    9, -0.11f,0.23f, -0.136f,0.166f, -0.20f,0.14f, -0.264f,0.166f,
       -0.29f,0.23f, -0.264f,0.294f, -0.20f,0.32f, -0.136f,0.294f,
       -0.11f,0.23f,
    9, 0.29f,0.23f, 0.264f,0.166f, 0.20f,0.14f, 0.136f,0.166f,
       0.11f,0.23f, 0.136f,0.294f, 0.20f,0.32f, 0.264f,0.294f,
       0.29f,0.23f, 0 };

#endif // SIGILS_H
