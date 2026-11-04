/* app/gl_es.h -- t57a: GLES2 glue (Android); desktop keeps old values. */
#if defined(__ANDROID__)
/* t57a: GLES2 -- gl* entry points come from SDL_GL_GetProcAddress (the link stubs are empty); RGBA uploads, GLSL 100 */
# define GH_GLES 1
# define GH_INTFMT GL_RGBA
# define GH_PIXFMT GL_RGBA
# define GH_GLSL_VER "#version 100\n"
# define GH_FRAG_PREC "#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
static struct GHES {
    const unsigned char * (APIENTRY * GetString)(unsigned int); void (APIENTRY * Viewport)(int, int, int, int);
    void (APIENTRY * ClearColor)(float, float, float, float); void (APIENTRY * Clear)(unsigned int);
    void (APIENTRY * BindTexture)(unsigned int, unsigned int); void (APIENTRY * GenTextures)(int, unsigned int *);
    void (APIENTRY * TexImage2D)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
    void (APIENTRY * TexSubImage2D)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
    void (APIENTRY * TexParameteri)(unsigned int, unsigned int, int); void (APIENTRY * PixelStorei)(unsigned int, int);
    void (APIENTRY * DrawArrays)(unsigned int, int, int); void (APIENTRY * Enable)(unsigned int); void (APIENTRY * Disable)(unsigned int);
    void (APIENTRY * BlendFunc)(unsigned int, unsigned int); void (APIENTRY * Finish)(void);
    void (APIENTRY * ReadPixels)(int, int, int, int, unsigned int, unsigned int, void *);
} GE;
# define glGetString GE.GetString
# define glViewport GE.Viewport
# define glClearColor GE.ClearColor
# define glClear GE.Clear
# define glBindTexture GE.BindTexture
# define glGenTextures GE.GenTextures
# define glTexImage2D GE.TexImage2D
# define glTexSubImage2D GE.TexSubImage2D
# define glTexParameteri GE.TexParameteri
# define glPixelStorei GE.PixelStorei
# define glDrawArrays GE.DrawArrays
# define glEnable GE.Enable
# define glDisable GE.Disable
# define glBlendFunc GE.BlendFunc
# define glFinish GE.Finish
# define glReadPixels GE.ReadPixels
static bool gh_gles_load(void) {
#define LE(m) do { GE.m = (decltype(GE.m))SDL_GL_GetProcAddress("gl" #m);   \
        if (!GE.m) { gh_log("GL: missing gl%s", #m); return false; } } while (0)
    LE(GetString); LE(Viewport); LE(ClearColor); LE(Clear); LE(BindTexture); LE(GenTextures); LE(TexImage2D); LE(TexSubImage2D);
    LE(TexParameteri); LE(PixelStorei); LE(DrawArrays); LE(Enable); LE(Disable); LE(BlendFunc); LE(Finish); LE(ReadPixels);
#undef LE
    return true;
}
static inline uint32_t gh_swz1(uint32_t x) { return (x & 0xFF00FF00u) | ((x >> 16) & 0xFFu) | ((x & 0xFFu) << 16); }
static inline const uint32_t * gh_up(const std::vector<uint32_t> & v, size_t n, std::vector<uint32_t> & t) {
    if (t.size() < n) t.resize(n);
    for (size_t i = 0; i < n; i++) t[i] = gh_swz1(v[i]);
    return t.data();
}
static inline void gh_swz(std::vector<uint32_t> & v) { for (auto & x : v) x = gh_swz1(x); }
static inline std::string gh_es_src(const std::string & s) {   /* same highp in both stages (uniform precision must match) */
    std::string r = s; const std::string k = "#ifdef GL_FRAGMENT_PRECISION_HIGH"; size_t p = 0;
    while ((p = r.find(k, p)) != std::string::npos) { r.replace(p, k.size(), "#if 1"); p += 5; }
    return r;
}
#else
# define GH_INTFMT GL_RGBA8
# define GH_PIXFMT GL_BGRA
# define GH_GLSL_VER "#version 120\n"
# define GH_FRAG_PREC ""
static inline const uint32_t * gh_up(const std::vector<uint32_t> & v, size_t, std::vector<uint32_t> &) { return v.data(); }
static inline void gh_swz(std::vector<uint32_t> &) {}
static inline std::string gh_es_src(const std::string & s) { return s; }
#endif
