// tempus.glsl.h — Inline shader source for the 2D batch renderer
// These are GLSL 3.30 core shaders embedded as C strings.

#ifndef TEMPUS_GLSL_H
#define TEMPUS_GLSL_H

static const char *vs_src =
    "#version 330 core\n"
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

static const char *fs_src =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    float alpha = texture(u_tex, v_uv).r;\n"
    "    frag_color = v_color * vec4(1.0, 1.0, 1.0, alpha);\n"
    "}\n";

#endif
