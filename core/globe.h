// globe.h — Earth globe instrument: mesh + orientation math.
// Pure C, no rendering. The globe is Earth seen orthographically from
// directly above the observer's lat/lon, lit by the true sun direction,
// so the terminator sweeps the disc as the planet turns.
//
// Frames:
//   Earth frame:  unit sphere, +z = north pole, lon 0 at +x.
//   View frame:   matches the 2D draw space — +x screen right, +y screen
//                 down, +z toward the viewer. This is a true from-space
//                 view: the observer's location maps to disc center
//                 (0,0,1), local north maps to screen-up, local east to
//                 screen-right. For a northern-hemisphere observer the
//                 north pole is visible toward the top of the disc.

#ifndef GLOBE_H
#define GLOBE_H

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- Mesh (unit sphere; position doubles as normal) ----

#define GLOBE_STACKS 48
#define GLOBE_SLICES 64
#define GLOBE_NUM_VERTS   ((GLOBE_STACKS + 1) * (GLOBE_SLICES + 1))
#define GLOBE_NUM_INDICES (GLOBE_STACKS * GLOBE_SLICES * 6)

typedef struct { float x, y, z; } GlobeVert;

typedef struct {
    GlobeVert verts[GLOBE_NUM_VERTS];
    uint16_t  indices[GLOBE_NUM_INDICES];
} GlobeMesh;

static inline void globe_mesh_build(GlobeMesh *m) {
    int vi = 0;
    for (int s = 0; s <= GLOBE_STACKS; s++) {
        double lat = -M_PI * 0.5 + M_PI * (double)s / GLOBE_STACKS;
        double cl = cos(lat), sl = sin(lat);
        for (int i = 0; i <= GLOBE_SLICES; i++) {
            double lon = -M_PI + 2.0 * M_PI * (double)i / GLOBE_SLICES;
            m->verts[vi++] = (GlobeVert){
                (float)(cl * cos(lon)),
                (float)(cl * sin(lon)),
                (float)sl,
            };
        }
    }
    int ii = 0;
    for (int s = 0; s < GLOBE_STACKS; s++) {
        for (int i = 0; i < GLOBE_SLICES; i++) {
            uint16_t a = (uint16_t)(s * (GLOBE_SLICES + 1) + i);
            uint16_t b = (uint16_t)(a + GLOBE_SLICES + 1);
            m->indices[ii++] = a;   m->indices[ii++] = b;     m->indices[ii++] = a + 1;
            m->indices[ii++] = a + 1; m->indices[ii++] = b;   m->indices[ii++] = b + 1;
        }
    }
}

// ---- Orientation ----
// Rotation earth→view for an observer at (lat, lon) degrees, written as a
// column-major mat4 ready for GL. Rows of the 3x3 are the view axes
// expressed in earth coordinates: x = -east, y = north tangent, z = up.

static inline void globe_rotation(double lat_deg, double lon_deg, float out_mat4[16]) {
    double phi = lat_deg * M_PI / 180.0;
    double lam = lon_deg * M_PI / 180.0;
    double sp = sin(phi), cp = cos(phi);
    double sl = sin(lam), cl = cos(lam);

    // Row-major 3x3. World +y is screen-down, so the y row is the
    // *south* tangent (north renders up).
    double R[3][3] = {
        { -sl,        cl,        0  },   // east
        {  sp * cl,   sp * sl,  -cp },   // south tangent (-north)
        {  cp * cl,   cp * sl,   sp },   // up (observer position)
    };

    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            out_mat4[c * 4 + r] = (c < 3 && r < 3) ? (float)R[r][c]
                                : (c == 3 && r == 3) ? 1.0f : 0.0f;
}

// Sun direction in the view frame from SPA azimuth/zenith (degrees).
// From-space frame: east lights the globe from screen-right, north from
// screen-top. Solar noon (az 180, sun due south) lights it from
// screen-bottom — matching the dial marker, which culminates at the
// bottom of the dial at noon.
static inline void globe_sun_dir(double azimuth_deg, double zenith_deg, float out[3]) {
    double az = azimuth_deg * M_PI / 180.0;
    double ze = zenith_deg * M_PI / 180.0;
    double e = sin(ze) * sin(az);   // east component
    double n = sin(ze) * cos(az);   // north component
    double u = cos(ze);             // up component
    out[0] = (float)(e);
    out[1] = (float)(-n);           // world +y is screen-down
    out[2] = (float)(u);
}

#endif // GLOBE_H
