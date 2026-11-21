/* The two filters, drawn over the frame instead of the plain blit. One fullscreen quad, GLSL 1.20 because the
 * port's context is OpenGL 2.1: Off, CRT and VHS. The source's row 0 is the frame's top, and the quad's texture
 * coordinates already sample it the way the blit does, so nothing is flipped here. */
#include "filter.h"
#include "../rt/rt.h"

static GLuint prog;
static GLint u_src = -1, u_mode = -1, u_time = -1, u_size = -1;
static int state;  /* 0 = not tried, 1 = ready, -1 = failed */

static const char *vs_src =
    "#version 120\n"
    "attribute vec4 aPos;\n"
    "varying vec2 vUv;\n"
    "void main() {\n"
    "    vUv = aPos.zw;\n"
    "    gl_Position = vec4(aPos.xy, 0.0, 1.0);\n"
    "}\n";

static const char *fs_src =
    "#version 120\n"
    "varying vec2 vUv;\n"
    "uniform sampler2D src;\n"
    "uniform int mode;\n"
    "uniform float seconds;\n"
    "uniform vec4 size;\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }\n"
    "void main() {\n"
    "    vec2 d = vUv - 0.5;\n"
    "    vec3 c;\n"
    "    float scan = 0.80 + 0.20 * sin(vUv.y * size.y * 3.14159265);\n"
    "    if (mode == 1) {\n"
    "        /* CRT: scanlines, a warm tube, and the corners darker. */\n"
    "        c = texture2D(src, vUv).rgb;\n"
    "        c *= scan * (1.0 - 0.38 * dot(d, d)) * vec3(1.05, 1.0, 0.90);\n"
    "    } else {\n"
    "        /* VHS: the colour is a hair out of line, and the tape has grain and a slow roll. */\n"
    "        float off = 0.0011 * sin(vUv.y * 52.0 + seconds * 2.0);\n"
    "        c.r = texture2D(src, vUv + vec2(off, 0.0)).r;\n"
    "        c.g = texture2D(src, vUv).g;\n"
    "        c.b = texture2D(src, vUv - vec2(off, 0.0)).b;\n"
    "        c += (hash(vUv * size.xy + seconds) - 0.5) * 0.07;\n"
    "        c *= scan * (1.0 - 0.30 * dot(d, d));\n"
    "        c *= 0.96 + 0.04 * sin(vUv.y * 9.0 - seconds * 3.0);\n"
    "    }\n"
    "    gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
    "}\n";

static GLuint compile(GLenum type, const char *src, const char *what) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[800];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        rt_log("filter: %s shader failed: %s", what, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

int filter_init(void) {
    if (state) return state > 0;
    state = -1;
    GLuint vs = compile(GL_VERTEX_SHADER, vs_src, "vertex"), fs = compile(GL_FRAGMENT_SHADER, fs_src, "fragment");
    if (!vs || !fs) return 0;
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "aPos");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[800];
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        rt_log("filter: link failed: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    u_src = glGetUniformLocation(prog, "src");
    u_mode = glGetUniformLocation(prog, "mode");
    u_time = glGetUniformLocation(prog, "seconds");
    u_size = glGetUniformLocation(prog, "size");
    state = 1;
    rt_log("filter: ready");
    return 1;
}

void filter_draw(GLuint src, int mode, float seconds) {
    if (!filter_init() || !src || mode < 1) return;
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] <= 0 || vp[3] <= 0) return;
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src);
    glUniform1i(u_src, 0);
    glUniform1i(u_mode, mode);
    glUniform1f(u_time, seconds);
    glUniform4f(u_size, (float)vp[2], (float)vp[3], 0.0f, 0.0f);
    static const float quad[] = {-1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1};
    for (int r = 1; r < 16; r++) glDisableVertexAttribArray((GLuint)r);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glUseProgram(0);
}
