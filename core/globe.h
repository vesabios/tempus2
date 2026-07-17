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
#include <string.h>

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

// ---- Rotation interpolation (camera flight between view frames) ----

// v' = R * v (column-major mat4, 3x3 part)
static inline void globe_mat_mul_vec(const float m[16], const float v[3], float out[3]) {
    for (int r = 0; r < 3; r++)
        out[r] = m[0*4+r] * v[0] + m[1*4+r] * v[1] + m[2*4+r] * v[2];
}

// v' = R^T * v (view -> earth)
static inline void globe_mat_tmul_vec(const float m[16], const float v[3], float out[3]) {
    for (int i = 0; i < 3; i++)
        out[i] = m[i*4+0] * v[0] + m[i*4+1] * v[1] + m[i*4+2] * v[2];
}

static inline void globe_vec_nlerp(const float a[3], const float b[3], float t, float out[3]) {
    float d = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    float sb = (d < 0) ? -1.0f : 1.0f;   // shortest path
    float x = a[0] + (sb * b[0] - a[0]) * t;
    float y = a[1] + (sb * b[1] - a[1]) * t;
    float z = a[2] + (sb * b[2] - a[2]) * t;
    float n = sqrtf(x*x + y*y + z*z);
    if (n < 1e-6f) { out[0] = a[0]; out[1] = a[1]; out[2] = a[2]; return; }
    out[0] = x / n; out[1] = y / n; out[2] = z / n;
}

static inline void globe__mat_to_quat(const float m[16], double q[4]) {
    double m00 = m[0], m01 = m[4], m02 = m[8];
    double m10 = m[1], m11 = m[5], m12 = m[9];
    double m20 = m[2], m21 = m[6], m22 = m[10];
    double tr = m00 + m11 + m22;
    if (tr > 0) {
        double s = sqrt(tr + 1.0) * 2.0;
        q[0] = 0.25 * s;
        q[1] = (m21 - m12) / s;
        q[2] = (m02 - m20) / s;
        q[3] = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        double s = sqrt(1.0 + m00 - m11 - m22) * 2.0;
        q[0] = (m21 - m12) / s;
        q[1] = 0.25 * s;
        q[2] = (m01 + m10) / s;
        q[3] = (m02 + m20) / s;
    } else if (m11 > m22) {
        double s = sqrt(1.0 + m11 - m00 - m22) * 2.0;
        q[0] = (m02 - m20) / s;
        q[1] = (m01 + m10) / s;
        q[2] = 0.25 * s;
        q[3] = (m12 + m21) / s;
    } else {
        double s = sqrt(1.0 + m22 - m00 - m11) * 2.0;
        q[0] = (m10 - m01) / s;
        q[1] = (m02 + m20) / s;
        q[2] = (m12 + m21) / s;
        q[3] = 0.25 * s;
    }
}

static inline void globe__quat_to_mat(const double q[4], float out[16]) {
    double w = q[0], x = q[1], y = q[2], z = q[3];
    double M[3][3] = {
        { 1 - 2*(y*y + z*z), 2*(x*y - w*z),     2*(x*z + w*y)     },
        { 2*(x*y + w*z),     1 - 2*(x*x + z*z), 2*(y*z - w*x)     },
        { 2*(x*z - w*y),     2*(y*z + w*x),     1 - 2*(x*x + y*y) },
    };
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            out[c*4+r] = (c < 3 && r < 3) ? (float)M[r][c]
                       : (c == 3 && r == 3) ? 1.0f : 0.0f;
}

static inline double globe__det3(const float m[16]) {
    double m00 = m[0], m01 = m[4], m02 = m[8];
    double m10 = m[1], m11 = m[5], m12 = m[9];
    double m20 = m[2], m21 = m[6], m22 = m[10];
    return m00 * (m11 * m22 - m12 * m21)
         - m01 * (m10 * m22 - m12 * m20)
         + m02 * (m10 * m21 - m11 * m20);
}

// Spherical interpolation between two view orientations — a smooth
// camera flight from one vantage to the other. Display matrices carry a
// reflection (det -1, left-handed screen basis); quaternions only exist
// for proper rotations, so the common mirror is factored out, the pure
// rotations slerped, and the mirror reapplied.
static inline void globe_rot_slerp(const float A[16], const float B[16],
                                   double t, float out[16]) {
    float Ap[16], Bp[16];
    memcpy(Ap, A, sizeof(Ap));
    memcpy(Bp, B, sizeof(Bp));
    bool improper = globe__det3(A) < 0.0;
    if (improper) {
        // F * M flips the x row (elements m[c*4+0])
        for (int c = 0; c < 3; c++) { Ap[c*4] = -Ap[c*4]; Bp[c*4] = -Bp[c*4]; }
    }
    double qa[4], qb[4];
    globe__mat_to_quat(Ap, qa);
    globe__mat_to_quat(Bp, qb);
    double dot = qa[0]*qb[0] + qa[1]*qb[1] + qa[2]*qb[2] + qa[3]*qb[3];
    if (dot < 0) { for (int i = 0; i < 4; i++) qb[i] = -qb[i]; dot = -dot; }
    if (dot > 1.0) dot = 1.0;
    double q[4];
    if (dot > 0.9995) {
        for (int i = 0; i < 4; i++) q[i] = qa[i] + (qb[i] - qa[i]) * t;
    } else {
        double th = acos(dot);
        double sa = sin((1.0 - t) * th) / sin(th);
        double sb = sin(t * th) / sin(th);
        for (int i = 0; i < 4; i++) q[i] = qa[i] * sa + qb[i] * sb;
    }
    double n = sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    for (int i = 0; i < 4; i++) q[i] /= n;
    globe__quat_to_mat(q, out);
    if (improper)
        for (int c = 0; c < 3; c++) out[c*4] = -out[c*4];
}

// ---- Heliocentric (orrery) orientation ----
// View from the ecliptic north pole; the calendar wheel is the orbit.
// Wheel angle convention: year_pct 0 (yule) at screen-top, so the June
// solstice sits at screen-bottom. Earth's axis is FIXED in space, tilted
// toward screen-up — which makes it lean toward the sun at the June
// position and away at yule. Seasons, as geometry.

#define GLOBE_OBLIQUITY_DEG 23.44

// Build earth->view rotation and sun direction for an orbit position.
//   orbit_angle: year_pct * 2pi (0 = yule, screen-top)
//   clock_hours: local clock time (hours, 0..24)
//   tz_hours:    clock's UTC offset
// Spin is solved so the subsolar meridian matches the mean sun: local
// solar noon at longitude L when the clock says 12 there. The model's
// declination emerges from the tilt: +23.44 at June, -23.44 at yule.
static inline void globe_orrery(double orbit_angle, double clock_hours,
                                double tz_hours,
                                float rot_out[16], float light_out[3]) {
    double eps = GLOBE_OBLIQUITY_DEG * M_PI / 180.0;
    double phi = orbit_angle;

    // Sun direction: from earth (on the wheel) toward the wheel center
    light_out[0] = (float)(-sin(phi));
    light_out[1] = (float)(cos(phi));
    light_out[2] = 0.0f;

    // Subsolar longitude of the mean sun for this clock reading
    double lam_ss = (12.0 - clock_hours + tz_hours) * 15.0 * M_PI / 180.0;

    // Spin about the (tilted, fixed) axis so that earth longitude lam_ss
    // faces the sun. The display basis (screen-right, screen-down,
    // toward-viewer) is left-handed, so like the geocentric matrix this
    // one carries a reflection (det -1): M = F * Rx(eps) * Rz(sigma),
    // F = diag(-1,1,1). Without F the globe renders east-west mirrored.
    double sigma = atan2(cos(phi) * cos(eps), sin(phi)) - lam_ss;

    double c = cos(sigma), s = sin(sigma);
    double ce = cos(eps), se = sin(eps);
    double M[3][3] = {
        { -c,       s,        0   },
        { ce * s,   ce * c,  -se  },
        { se * s,   se * c,   ce  },
    };
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            rot_out[col * 4 + row] = (col < 3 && row < 3) ? (float)M[row][col]
                                   : (col == 3 && row == 3) ? 1.0f : 0.0f;
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

// Earth's rotation axis (north) expressed in the view frame. Independent
// of longitude by construction.
static inline void globe_axis(double lat_deg, float out[3]) {
    double phi = lat_deg * M_PI / 180.0;
    out[0] = 0.0f;
    out[1] = (float)(-cos(phi));   // world +y is screen-down
    out[2] = (float)(sin(phi));
}

// Given the current sun direction (view frame) and the earth->view
// rotation, return the sun direction a sun at declination `decl_deg`
// would have at the same subsolar longitude — i.e. "the sun right now,
// if it were the solstice." Used for the envelope overlay.
static inline void globe_sun_with_decl(const float rot_mat4[16],
                                       const float sun_view[3],
                                       double decl_deg, float out[3]) {
    // Earth frame: e_i = sum_r R[r][i] * v_r  (R^T, col-major storage)
    double se[3];
    for (int i = 0; i < 3; i++)
        se[i] = rot_mat4[i * 4 + 0] * sun_view[0]
              + rot_mat4[i * 4 + 1] * sun_view[1]
              + rot_mat4[i * 4 + 2] * sun_view[2];
    double lam = atan2(se[1], se[0]);
    double d = decl_deg * M_PI / 180.0;
    double ne[3] = { cos(d) * cos(lam), cos(d) * sin(lam), sin(d) };
    // Back to view frame: v_r = sum_c R[r][c] * e_c
    for (int r = 0; r < 3; r++)
        out[r] = (float)(rot_mat4[0 * 4 + r] * ne[0]
                       + rot_mat4[1 * 4 + r] * ne[1]
                       + rot_mat4[2 * 4 + r] * ne[2]);
}

#endif // GLOBE_H
