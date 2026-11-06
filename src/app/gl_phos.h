/* app/gl_phos.h -- t662 (#635): phosphor passes. Persistence = one feedback texture (ping-pong), glow = low-res bright/blur textures.
   Included from video_sink.h (after gl_build). Everything here runs on the GL render thread only. Missing FBO support -> phos_gl = false
   (the host then ignores persistence/glow; beam width and breathing are plain uniforms of the crt shader). */
#include <math.h>
#define GLP_FRAMEBUFFER 0x8D40
#define GLP_COLOR_ATT0 0x8CE0
#define GLP_FB_COMPLETE 0x8CD5
#define GLP_TEXTURE0 0x84C0
typedef void (APIENTRY * pf_GenFb)(int, unsigned int *);
typedef void (APIENTRY * pf_BindFb)(unsigned int, unsigned int);
typedef void (APIENTRY * pf_FbTex)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef unsigned int (APIENTRY * pf_CheckFb)(unsigned int);
typedef void (APIENTRY * pf_ActTex)(unsigned int);
static struct GLP { pf_GenFb GenFb; pf_BindFb BindFb; pf_FbTex FbTex; pf_CheckFb CheckFb; pf_ActTex ActTex; } FP;
static struct GPhos {
    bool tried, ok, pvalid; int pw, ph, gw, gh, gdiv, ping; unsigned long long last;
    unsigned pt[2], pf[2], gt[2], gf[2]; GProg pers, bright, blur; int uPrev, uDecay, uBTexel, uKTexel, uDir;
} PH;
static const char * g_phos_src =
    "varying vec2 vUV;\n"
    "#ifdef VERTEX\nattribute vec2 aPos;\nattribute vec2 aUV;\nvoid main(){ gl_Position = vec4(aPos, 0.0, 1.0); vUV = aUV; }\n#endif\n"
    "#ifdef FRAGMENT\n"
    "uniform sampler2D uTex;\nuniform sampler2D uPrev;\nuniform vec2 uTexel;\nuniform vec2 uDir;\nuniform float uDecay;\n"
    "const vec3 LW = vec3(0.30, 0.59, 0.11);\n"
    "void main(){\n"
    "  vec2 t = vec2(vUV.x, 1.0 - vUV.y);\n"
    "#ifdef PASS_PERS\n"
    "  vec3 c = texture2D(uTex, t).rgb;\n  vec3 p = texture2D(uPrev, t).rgb;\n"
    "  vec3 d = sqrt(max(p * p * uDecay, 0.0)) - vec3(0.0039216);\n"
    "  gl_FragColor = vec4(max(c, d), 1.0);\n"
    "#endif\n"
    "#ifdef PASS_BRIGHT\n"
    "  vec3 s = vec3(0.0);\n"
    "  for (int i = 0; i < 4; i++) {\n"
    "    vec2 o = vec2((i == 0 || i == 2) ? -1.0 : 1.0, (i < 2) ? -1.0 : 1.0) * uTexel;\n"
    "    vec3 c = texture2D(uTex, t + o).rgb; float lg = dot(c, LW); c = c * c;\n"
    "    s += c * smoothstep(0.6, 0.9, lg);\n"
    "  }\n"
    "  gl_FragColor = vec4(sqrt(s * 0.25), 1.0);\n"
    "#endif\n"
    "#ifdef PASS_BLUR\n"
    "  vec3 a0 = texture2D(uTex, t).rgb; vec3 s = 0.2270270 * (a0 * a0);\n"
    "  for (int i = 1; i <= 4; i++) {\n"
    "    float w = (i == 1) ? 0.1945946 : ((i == 2) ? 0.1216216 : ((i == 3) ? 0.0540541 : 0.0162162));\n"
    "    vec2 o = uDir * uTexel * (0.6 * float(i));\n"
    "    vec3 a = texture2D(uTex, t + o).rgb; vec3 b = texture2D(uTex, t - o).rgb;\n"
    "    s += w * (a * a + b * b);\n"
    "  }\n"
    "  gl_FragColor = vec4(sqrt(s), 1.0);\n"
    "#endif\n"
    "}\n#endif\n";
static bool glp_pass_build(const char * def, GProg & g, const char * name) { return gl_build(std::string(def) + g_phos_src, g, false,
    name); }
static bool glp_init(void) {
    if (PH.tried) return PH.ok;
    PH.tried = true; PH.gdiv = 2; { const char * e = getenv("KH_GLOWDIV"); if (e && atoi(e) >= 1 && atoi(e) <= 16) PH.gdiv = atoi(e); }
#define LP(m, a, b) do { FP.m = (decltype(FP.m))SDL_GL_GetProcAddress(a); if (!FP.m) FP.m = (decltype(FP.m))SDL_GL_GetProcAddress(b); \
        if (!FP.m) { gh_log("GL: phos: missing %s", a); return false; } } while (0)
    LP(GenFb, "glGenFramebuffers", "glGenFramebuffersEXT"); LP(BindFb, "glBindFramebuffer", "glBindFramebufferEXT");
    LP(FbTex, "glFramebufferTexture2D", "glFramebufferTexture2DEXT"); LP(CheckFb, "glCheckFramebufferStatus",
        "glCheckFramebufferStatusEXT");
    LP(ActTex, "glActiveTexture", "glActiveTextureARB");
#undef LP
    if (!glp_pass_build("#define PASS_PERS 1\n", PH.pers, "phos-pers") || !glp_pass_build("#define PASS_BRIGHT 1\n", PH.bright,
        "phos-bright")
        || !glp_pass_build("#define PASS_BLUR 1\n", PH.blur, "phos-blur")) return false;
    PH.uPrev = F.GetUniformLocation(PH.pers.p, "uPrev"); PH.uDecay = F.GetUniformLocation(PH.pers.p, "uDecay");
    PH.uBTexel = F.GetUniformLocation(PH.bright.p, "uTexel"); PH.uKTexel = F.GetUniformLocation(PH.blur.p, "uTexel");
    PH.uDir = F.GetUniformLocation(PH.blur.p, "uDir");
    PH.ok = true; gh_log("GL: phos passes OK (glow 1/%d)", PH.gdiv); return true;
}
/* create or resize one render-target texture (RGBA8, linear, cleared to black) */
static bool glp_target(unsigned & tex, unsigned & fb, int w, int h) {
    if (!tex) { glGenTextures(1, &tex); FP.GenFb(1, &fb); }
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GH_INTFMT, w, h, 0, GH_PIXFMT, GL_UNSIGNED_BYTE, NULL);
    FP.BindFb(GLP_FRAMEBUFFER, fb); FP.FbTex(GLP_FRAMEBUFFER, GLP_COLOR_ATT0, GL_TEXTURE_2D, tex, 0);
    const bool ok = FP.CheckFb(GLP_FRAMEBUFFER) == GLP_FB_COMPLETE;
    glViewport(0, 0, w, h); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    return ok;
}
static void glp_quad(void) {
    F.BindBuffer(GL_ARRAY_BUFFER, gApp.vbo); F.EnableVertexAttribArray(0); F.EnableVertexAttribArray(1);
    F.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (const void *)0); F.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void *)8);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
/* One frame of preparation. Returns the texture the crt shader reads as its source (raw or the persistence texture);
   *gtex = glow texture (0 = none). The caller restores the viewport and the program afterwards. */
static unsigned glp_prepare(unsigned raw, int cw, int ch, int persms, bool glow, unsigned * gtex) {
    *gtex = 0;
    if (persms <= 0) { PH.pvalid = false; PH.last = 0; }
    if (persms <= 0 && !glow) return raw;
    if (!glp_init()) { gApp.phos_gl = false; return raw; }
    const int gw = cw / PH.gdiv < 1 ? 1 : cw / PH.gdiv, gh = ch / PH.gdiv < 1 ? 1 : ch / PH.gdiv;
    bool ok = true;
    if (PH.pw != cw || PH.ph != ch) {
        ok = glp_target(PH.pt[0], PH.pf[0], cw, ch) && glp_target(PH.pt[1], PH.pf[1], cw, ch) && ok; PH.pw = cw; PH.ph = ch;
            PH.pvalid = false;
        gh_log("GL: phos persistence target %dx%d %s", cw, ch, ok ? "ok" : "FAIL");
    }
    if (PH.gw != gw || PH.gh != gh) {
        ok = glp_target(PH.gt[0], PH.gf[0], gw, gh) && glp_target(PH.gt[1], PH.gf[1], gw, gh) && ok; PH.gw = gw; PH.gh = gh;
        gh_log("GL: phos glow target %dx%d %s", gw, gh, ok ? "ok" : "FAIL");
    }
    if (!ok) { FP.BindFb(GLP_FRAMEBUFFER, 0); gApp.phos_gl = false; return raw; }
    unsigned src = raw;
    glDisable(GL_BLEND);
    if (persms > 0) {
        const Uint64 now = SDL_GetPerformanceCounter();
            double dt = PH.last ? (double)(now - PH.last) / (double)SDL_GetPerformanceFrequency() : 0.0;
        PH.last = now; if (dt > 0.1) dt = 0.1;
        const float k = PH.pvalid ? (float)exp(-dt * 1000.0 / (double)persms) : 0.0f;   /* real elapsed time, not frame count */
        const int s0 = PH.ping, d0 = PH.ping ^ 1;
        FP.BindFb(GLP_FRAMEBUFFER, PH.pf[d0]); glViewport(0, 0, cw, ch);
        F.UseProgram(PH.pers.p); if (PH.pers.uTex >= 0) F.Uniform1i(PH.pers.uTex, 0); if (PH.uPrev >= 0) F.Uniform1i(PH.uPrev, 1);
        if (PH.uDecay >= 0) F.Uniform1f(PH.uDecay, k);
        FP.ActTex(GLP_TEXTURE0 + 1); glBindTexture(GL_TEXTURE_2D, PH.pt[s0]); FP.ActTex(GLP_TEXTURE0); glBindTexture(GL_TEXTURE_2D, raw);
        glp_quad(); PH.ping = d0; PH.pvalid = true; src = PH.pt[d0];
    }
    if (glow) {
        glBindTexture(GL_TEXTURE_2D, src);
        FP.BindFb(GLP_FRAMEBUFFER, PH.gf[0]); glViewport(0, 0, gw, gh); F.UseProgram(PH.bright.p);
        if (PH.bright.uTex >= 0) F.Uniform1i(PH.bright.uTex, 0); if (PH.uBTexel >= 0) F.Uniform2f(PH.uBTexel, 1.0f / (float)cw,
            1.0f / (float)ch);
        glp_quad();
        FP.BindFb(GLP_FRAMEBUFFER, PH.gf[1]); F.UseProgram(PH.blur.p); glBindTexture(GL_TEXTURE_2D, PH.gt[0]);
        if (PH.blur.uTex >= 0) F.Uniform1i(PH.blur.uTex, 0); if (PH.uKTexel >= 0) F.Uniform2f(PH.uKTexel, 1.0f / (float)gw,
            1.0f / (float)gh);
        if (PH.uDir >= 0) F.Uniform2f(PH.uDir, 1.0f, 0.0f);
        glp_quad();
        FP.BindFb(GLP_FRAMEBUFFER, PH.gf[0]); glBindTexture(GL_TEXTURE_2D, PH.gt[1]); if (PH.uDir >= 0) F.Uniform2f(PH.uDir, 0.0f, 1.0f);
        glp_quad(); *gtex = PH.gt[0];
    }
    FP.BindFb(GLP_FRAMEBUFFER, 0);
    return src;
}
