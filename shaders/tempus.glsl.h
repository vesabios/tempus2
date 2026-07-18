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
    "    // Per-globe depth band (u_place.w = band center): each sphere\n"
    "    // self-occludes within its own +/-0.1 slice, and bands are\n"
    "    // disjoint so spheres never depth-fight each other — slot order\n"
    "    // is painter's order\n"
    "    float depth = u_place.w - vp.z * 0.1;\n"
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
    "uniform vec4 u_mode;\n"      // x = overlay mode, y = declination (rad)
    "uniform vec4 u_axis;\n"      // earth rotation axis, view frame
    "uniform vec4 u_sunj;\n"      // sun if it were june solstice
    "uniform vec4 u_sund;\n"      // sun if it were december solstice
    "uniform vec4 u_params;\n"    // x = land mask on, y = whole-sphere alpha
    "uniform sampler2D u_land;\n" // equirectangular landmass mask
    "out vec4 frag_color;\n"
    "float hairline(float d, float scale) {\n"
    "    float fw = fwidth(d) + 1e-5;\n"
    "    return 1.0 - smoothstep(fw * 0.6 * scale, fw * 2.0 * scale, abs(d));\n"
    "}\n"
    "void main() {\n"
    "    vec3 ne = normalize(v_en);\n"
    "    vec3 nv = normalize(v_vn);\n"
    "    vec3 L = normalize(u_light.xyz);\n"
    "    float d = dot(nv, L);\n"
    "    int mode = int(u_mode.x + 0.5);\n"
    "    float lat_r = asin(clamp(ne.z, -1.0, 1.0));\n"
    "    vec3 col;\n"
    "    if (mode == 1) {\n"
    "        // Chronophotograph: the day's terminators multi-exposed.\n"
    "        // Dense + faint converges on the daylight field (mode 2).\n"
    "        col = u_night.rgb;\n"
    "        vec3 ax = normalize(u_axis.xyz);\n"
    "        vec3 Lp = L - ax * dot(ax, L);\n"      // sun ⊥ axis component
    "        vec3 Lq = cross(ax, Lp);\n"
    "        vec3 La = ax * dot(ax, L);\n"
    "        for (int k = 0; k < 24; k++) {\n"
    "            float a = 6.2831853 * (float(k) / 24.0);\n"
    "            vec3 Lk = La + Lp * cos(a) + Lq * sin(a);\n"
    "            float dk = dot(nv, Lk);\n"
    "            col += u_day.rgb * hairline(dk, 1.2) * 0.16;\n"
    "        }\n"
    "    } else if (mode == 2) {\n"
    "        // Daylight-hours field: fraction of today each point is lit\n"
    "        float x = -tan(lat_r) * tan(u_mode.y);\n"
    "        float f = (x <= -1.0) ? 1.0 : (x >= 1.0) ? 0.0\n"
    "                : acos(clamp(x, -1.0, 1.0)) / 3.14159265;\n"
    "        col = mix(u_night.rgb, u_day.rgb, f);\n"
    "    } else {\n"
    "        // Live day/night with a twilight band (~ +/-5 deg)\n"
    "        float day = smoothstep(-0.09, 0.09, d);\n"
    "        col = mix(u_night.rgb, u_day.rgb, day);\n"
    "    }\n"
    "    // Landmass fill: a red-leaning lift over the ocean tone, part\n"
    "    // of the surface albedo so day/night shading applies naturally\n"
    "    float lon_r = atan(ne.y, ne.x);\n"
    "    vec2 luv = vec2(lon_r / 6.2831853 + 0.5, 0.5 - lat_r / 3.14159265);\n"
    "    float land = texture(u_land, luv).r * u_params.x;\n"
    "    if (u_params.z > 0.5) {\n"
    "        // Albedo body (the moon): continuous brightness modulation\n"
    "        col *= 0.45 + 0.85 * land;\n"
    "        // Phase legibility: only the lit surface VISIBLE FROM the\n"
    "        // observer (u_sunj = earth direction here) stays bright —\n"
    "        // the bright lune IS the phase as seen from Earth. In the\n"
    "        // geo aperture the observer is the viewer, so no change.\n"
    "        float evis = smoothstep(-0.06, 0.06, dot(nv, normalize(u_sunj.xyz)));\n"
    "        col *= 0.40 + 0.60 * evis;\n"
    "    } else {\n"
    "        // Earth: landmass lift over the ocean tone\n"
    "        col *= vec3(1.0) + land * vec3(0.40, 0.14, 0.12);\n"
    "    }\n"
    "    // Limb shading for sphericity\n"
    "    col *= 0.68 + 0.32 * max(nv.z, 0.0);\n"
    "    float limbfade = smoothstep(0.02, 0.3, nv.z);\n"
    "    if (mode == 3) {\n"
    "        // Solstice envelope: engraved extreme terminators + dashed\n"
    "        // tropics/polar circles; the live terminator swings between.\n"
    "        float dj = dot(nv, normalize(u_sunj.xyz));\n"
    "        float dd = dot(nv, normalize(u_sund.xyz));\n"
    "        float env = max(hairline(dj, 1.0), hairline(dd, 1.0));\n"
    "        col = mix(col, u_grid.rgb, env * limbfade * 0.85);\n"
    "        float latd = degrees(lat_r);\n"
    "        float lon_d = degrees(atan(ne.y, ne.x));\n"
    "        float ref = min(min(abs(latd - 23.44), abs(latd + 23.44)),\n"
    "                        min(abs(latd - 66.56), abs(latd + 66.56)));\n"
    "        float dash = step(0.5, fract(lon_d / 15.0));\n"
    "        float refl = (1.0 - smoothstep(0.0, fwidth(latd) * 1.6, ref)) * dash;\n"
    "        col = mix(col, u_grid.rgb, refl * limbfade * 0.6);\n"
    "    }\n"
    "    // (No terminator hairline: the twilight gradient in the lambert\n"
    "    // shading IS the terminator, and reads more naturally alone.)\n"
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
    "    col = mix(col, u_grid.rgb, clamp(grid, 0.0, 1.0) * u_grid.a * u_mode.w);\n"
    "    // Observer-latitude ring: day length made visible. Thick and\n"
    "    // bright where the parallel is in daylight, hairline in night —\n"
    "    // the thick/thin ratio IS today's day/night split at this place.\n"
    "    float dobs = abs(lat - u_mode.z);\n"
    "    float dayness = smoothstep(-0.04, 0.06, d);\n"
    "    float ringw = mix(1.0, 3.0, dayness);\n"
    "    float oring = 1.0 - smoothstep(0.0, fwidth(lat) * ringw, dobs);\n"
    "    col = mix(col, vec3(0.88, 0.86, 0.80),\n"
    "              oring * limbfade * mix(0.25, 0.55, dayness));\n"
    "    // Silhouette antialias via view-normal z\n"
    "    float alpha = smoothstep(0.0, fwidth(nv.z) * 1.5, nv.z);\n"
    "    frag_color = vec4(col, alpha * u_params.y);\n"
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
