// tempus.glsl.h — Inline shader source for the 2D batch renderer
// SDF text rendering with fwidth-based antialiasing.
// Same body for desktop GL 3.3 core and GLES3/WebGL2; only the
// version/precision preamble differs.

#ifndef TEMPUS_GLSL_H
#define TEMPUS_GLSL_H

#if defined(SOKOL_GLES3)
#define TEMPUS_GLSL_VS_PREAMBLE "#version 300 es\n"
#define TEMPUS_GLSL_FS_PREAMBLE "#version 300 es\nprecision mediump float;\n"
#else
#define TEMPUS_GLSL_VS_PREAMBLE "#version 330 core\n"
#define TEMPUS_GLSL_FS_PREAMBLE "#version 330 core\n"
#endif

static const char *vs_src =
    TEMPUS_GLSL_VS_PREAMBLE
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_color;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "    v_color = a_color;\n"
    "}\n";

// ---- Globe shaders ----
// Orthographic earth: lambert day/night from the true sun vector, 30°
// graticule and terminator hairline drawn analytically with fwidth AA.

static const char *globe_vs_src =
    TEMPUS_GLSL_VS_PREAMBLE
    "layout(location=0) in vec3 a_pos;\n"
    "uniform mat4 u_rot;\n"       // earth -> view rotation
    "uniform vec4 u_place;\n"     // cx, cy (world), radius (world), unused
    "uniform vec4 u_screen;\n"    // w/2, h/2, unused, unused
    "out vec3 v_en;\n"            // earth-frame normal (graticule)
    "out vec3 v_vn;\n"            // view-frame normal (lighting, silhouette)
    "void main() {\n"
    "    vec3 vp = mat3(u_rot) * a_pos;\n"
    "    v_en = a_pos;\n"
    "    v_vn = vp;\n"
    "    vec2 world = u_place.xy + vp.xy * u_place.z;\n"
    "    float depth = 0.5 - vp.z * 0.4;\n"  // front of sphere nearer
    "    gl_Position = vec4(world.x / u_screen.x, -world.y / u_screen.y, depth, 1.0);\n"
    "}\n";

static const char *globe_fs_src =
    TEMPUS_GLSL_FS_PREAMBLE
    "in vec3 v_en;\n"
    "in vec3 v_vn;\n"
    "uniform vec4 u_light;\n"     // xyz = sun dir (view frame)
    "uniform vec4 u_day;\n"
    "uniform vec4 u_night;\n"
    "uniform vec4 u_grid;\n"      // rgb + line alpha
    "uniform vec4 u_term;\n"      // rgb + line alpha
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec3 ne = normalize(v_en);\n"
    "    vec3 nv = normalize(v_vn);\n"
    "    vec3 L = normalize(u_light.xyz);\n"
    "    float d = dot(nv, L);\n"
    "    // Day/night with a twilight band (~ +/-5 deg)\n"
    "    float day = smoothstep(-0.09, 0.09, d);\n"
    "    vec3 col = mix(u_night.rgb, u_day.rgb, day);\n"
    "    // Limb shading for sphericity\n"
    "    col *= 0.68 + 0.32 * max(nv.z, 0.0);\n"
    "    // Terminator hairline (fade where surface turns edge-on, else\n"
    "    // fwidth explodes at the limb and the line blobs)\n"
    "    float fwd = fwidth(d);\n"
    "    float term = 1.0 - smoothstep(fwd * 0.8, fwd * 2.2, abs(d));\n"
    "    term *= smoothstep(0.02, 0.3, nv.z);\n"
    "    col = mix(col, u_term.rgb, term * u_term.a);\n"
    "    // 30-degree graticule\n"
    "    float lat = degrees(asin(clamp(ne.z, -1.0, 1.0)));\n"
    "    float lon = degrees(atan(ne.y, ne.x));\n"
    "    vec2 g = vec2(lon, lat) / 30.0;\n"
    "    vec2 fw = fwidth(g);\n"
    "    vec2 dist = abs(fract(g + 0.5) - 0.5);\n"
    "    vec2 line = vec2(1.0) - smoothstep(vec2(0.0), fw * 1.4, dist);\n"
    "    line.x *= step(fw.x, 1.0);\n"                 // kill seam artifact
    "    line.x *= smoothstep(90.0, 82.0, abs(lat));\n" // fade meridians at poles
    "    float grid = max(line.x, line.y);\n"
    "    // Equator: bold reference line, anchors the axis\n"
    "    float eq = 1.0 - smoothstep(0.0, fwidth(lat) * 2.2, abs(lat));\n"
    "    grid = max(grid, eq * 1.8);\n"
    "    // Foreshorten: lines on a front surface dim as it turns away.\n"
    "    // This is the depth cue that keeps the wireframe from Necker-\n"
    "    // flipping into a view of the back half.\n"
    "    grid *= mix(0.15, 1.0, nv.z * nv.z);\n"
    "    col = mix(col, u_grid.rgb, clamp(grid, 0.0, 1.0) * u_grid.a);\n"
    "    // Silhouette antialias via view-normal z\n"
    "    float alpha = smoothstep(0.0, fwidth(nv.z) * 1.5, nv.z);\n"
    "    frag_color = vec4(col, alpha);\n"
    "}\n";

static const char *fs_src =
    TEMPUS_GLSL_FS_PREAMBLE
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    float d = texture(u_tex, v_uv).r;\n"
    "    // SDF threshold: edge at 128/255, AA via fwidth\n"
    "    float edge = 0.502;\n"  // 128/255
    "    float fw = fwidth(d);\n"
    "    float w = clamp(fw * 0.35, 0.001, 0.1);\n"  // tighter smoothing
    "    float alpha = smoothstep(edge - w, edge + w, d);\n"
    "    frag_color = v_color * vec4(1.0, 1.0, 1.0, alpha);\n"
    "}\n";

#endif
