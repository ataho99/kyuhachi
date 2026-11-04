/* app/video_sink.h -- S9a: video sink, PNG writer, GL output, window/WM glue, RENDER_SetSize. Moved unchanged from gensei_host.cpp lines 634-1008 (as of t54s). Included in place; single translation unit. */
/* ------------------------------------------------------------------ video sink */

static void gh_png(const char * path, const uint32_t * fb, int w, int h) {
    FILE * f = fopen(path, "wb");
    if (!f) { gh_log("png: cannot open %s", path); return; }
    png_structp pp = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop pi = png_create_info_struct(pp);
    if (setjmp(png_jmpbuf(pp))) { fclose(f); gh_log("png: error %s", path); return; }
    png_init_io(pp, f);
    png_set_IHDR(pp, pi, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(pp, pi);
    std::vector<unsigned char> row(w * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) { uint32_t v = fb[y * w + x]; row[x * 3] = (v >> 16) & 255; row[x * 3 + 1] = (v >> 8) & 255; row[x * 3 + 2] = v & 255; }
        png_write_row(pp, row.data());
    }
    png_write_end(pp, pi);
    png_destroy_write_struct(&pp, &pi);
    fclose(f);
    gh_log("png: wrote %s (%dx%d)", path, w, h);
}

static void gh_drawline(const void * src) {
    if (gApp.cury >= 0 && gApp.cury < gApp.fbh && gApp.fbw > 0) memcpy(&gApp.fb[(size_t)gApp.cury * gApp.fbw], src, (size_t)gApp.fbw * 4);
    gApp.cury++;
}

/* ------------------------------------------------------------------ r010 화면 출력: GL 렌더 스레드(에뮬 스레드는 프레임 복사만) */
#include <SDL_opengl.h>
#ifndef GL_FRAGMENT_SHADER
# define GL_FRAGMENT_SHADER 0x8B30
# define GL_VERTEX_SHADER 0x8B31
# define GL_COMPILE_STATUS 0x8B81
# define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_ARRAY_BUFFER
# define GL_ARRAY_BUFFER 0x8892
# define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_BGRA
# define GL_BGRA 0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif
#include "app/gl_es.h"
typedef unsigned int (APIENTRY * pf_CreateShader)(unsigned int);
typedef void (APIENTRY * pf_ShaderSource)(unsigned int, int, const char * const *, const int *);
typedef void (APIENTRY * pf_CompileShader)(unsigned int);
typedef void (APIENTRY * pf_GetShaderiv)(unsigned int, unsigned int, int *);
typedef void (APIENTRY * pf_GetInfoLog)(unsigned int, int, int *, char *);
typedef unsigned int (APIENTRY * pf_CreateProgram)(void);
typedef void (APIENTRY * pf_AttachShader)(unsigned int, unsigned int);
typedef void (APIENTRY * pf_BindAttribLocation)(unsigned int, unsigned int, const char *);
typedef void (APIENTRY * pf_LinkProgram)(unsigned int);
typedef void (APIENTRY * pf_GetProgramiv)(unsigned int, unsigned int, int *);
typedef void (APIENTRY * pf_UseProgram)(unsigned int);
typedef int (APIENTRY * pf_GetUniformLocation)(unsigned int, const char *);
typedef void (APIENTRY * pf_Uniform1i)(int, int);
typedef void (APIENTRY * pf_Uniform1f)(int, float);
typedef void (APIENTRY * pf_Uniform2f)(int, float, float);
typedef void (APIENTRY * pf_EnableVAA)(unsigned int);
typedef void (APIENTRY * pf_VAP)(unsigned int, int, unsigned int, unsigned char, int, const void *);
typedef void (APIENTRY * pf_GenBuffers)(int, unsigned int *);
typedef void (APIENTRY * pf_BindBuffer)(unsigned int, unsigned int);
typedef void (APIENTRY * pf_BufferData)(unsigned int, ptrdiff_t, const void *, unsigned int);
typedef void (APIENTRY * pf_Delete)(unsigned int);
static struct GLF {
    pf_CreateShader CreateShader; pf_ShaderSource ShaderSource; pf_CompileShader CompileShader; pf_GetShaderiv GetShaderiv; pf_GetInfoLog GetShaderInfoLog;
    pf_CreateProgram CreateProgram; pf_AttachShader AttachShader; pf_BindAttribLocation BindAttribLocation; pf_LinkProgram LinkProgram; pf_GetProgramiv GetProgramiv;
    pf_GetInfoLog GetProgramInfoLog; pf_UseProgram UseProgram; pf_GetUniformLocation GetUniformLocation; pf_Uniform1i Uniform1i; pf_Uniform1f Uniform1f; pf_Uniform2f Uniform2f;
    pf_EnableVAA EnableVertexAttribArray; pf_VAP VertexAttribPointer; pf_GenBuffers GenBuffers; pf_BindBuffer BindBuffer; pf_BufferData BufferData; pf_Delete DeleteShader; pf_Delete DeleteProgram;
} F;
static bool gl_load(void) {
#define LG(m, n) do { F.m = (decltype(F.m))SDL_GL_GetProcAddress(n); if (!F.m) { gh_log("GL: missing %s", n); return false; } } while (0)
    LG(CreateShader, "glCreateShader"); LG(ShaderSource, "glShaderSource"); LG(CompileShader, "glCompileShader"); LG(GetShaderiv, "glGetShaderiv"); LG(GetShaderInfoLog, "glGetShaderInfoLog");
    LG(CreateProgram, "glCreateProgram"); LG(AttachShader, "glAttachShader"); LG(BindAttribLocation, "glBindAttribLocation"); LG(LinkProgram, "glLinkProgram"); LG(GetProgramiv, "glGetProgramiv");
    LG(GetProgramInfoLog, "glGetProgramInfoLog"); LG(UseProgram, "glUseProgram"); LG(GetUniformLocation, "glGetUniformLocation"); LG(Uniform1i, "glUniform1i"); LG(Uniform1f, "glUniform1f"); LG(Uniform2f, "glUniform2f");
    LG(EnableVertexAttribArray, "glEnableVertexAttribArray"); LG(VertexAttribPointer, "glVertexAttribPointer"); LG(GenBuffers, "glGenBuffers"); LG(BindBuffer, "glBindBuffer"); LG(BufferData, "glBufferData");
    LG(DeleteShader, "glDeleteShader"); LG(DeleteProgram, "glDeleteProgram");
#undef LG
    return true;
}
/* 설정(에뮬 스레드가 쓰고 렌더 스레드가 읽음) */

/* ------------------------------------------------------------------ r017: 창 배율 (640x480 기준 x1/x2/x3…) · 테두리로 끈 크기 = 「사용자 지정」 */
/* 순수 판정(시험 가능): 이 크기 변경 이벤트가 사용자가 테두리를 끈 것인가 */
static bool gh_win_userresize(int w, int h, Uint32 flags, int ew, int eh, unsigned now, unsigned until) {
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) return false;
    if (ew > 0 && w == ew && h == eh && (int)(until - now) >= 0) return false;
    return w >= 160 && h >= 120;
}
static void gh_wm_refresh(void) {   /* 모니터에 들어가는 최대 배율 */
    SDL_Rect r; int top = 0, left = 0, bottom = 0, right = 0; r.x = r.y = 0; r.w = 1920; r.h = 1080;
    if (SDL_GetDisplayUsableBounds(0, &r) != 0) { SDL_DisplayMode dm; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { r.w = dm.w; r.h = dm.h - 40; } }
    if (gApp.win) SDL_GetWindowBordersSize(gApp.win, &top, &left, &bottom, &right);
    int dx = left + right, dy = top + bottom; if (dy == 0) { dy = 48; dx = 16; }
    int m = 0; for (int k = 1; k <= ui::Ui::WMN; k++) if (640 * k + dx <= r.w && 480 * k + dy <= r.h) m = k;
    gApp.ui_.wmax = m < 1 ? 1 : m;
}
static void gh_fs_toggle(void) {   /* t52i (B3): 화면 설정 「전체화면」 줄 · Alt+Enter 와 같은 동작 */
    if (!gApp.win) return;
    bool fs = (SDL_GetWindowFlags(gApp.win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(gApp.win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); SDL_ShowCursor(fs ? SDL_ENABLE : SDL_DISABLE);
    gh_log("SCREEN fullscreen=%d (menu)", fs ? 0 : 1); gh_osd(fs ? phr_("창 모드") : phr_("전체화면"));
}
static bool gh_fs_now(void) { return gApp.win && (SDL_GetWindowFlags(gApp.win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP; }
static void gh_win_apply(int m, bool osd) {   /* m>0: 배율 · m==0: 사용자 지정 크기(g_ui.winw x winh) */
    gh_wm_refresh(); int w, h;
    if (m > 0) { if (m > gApp.ui_.wmax) m = gApp.ui_.wmax; w = 640 * m; h = 480 * m; gApp.ui_.wmult = m; } else { w = gApp.ui_.winw; h = gApp.ui_.winh; gApp.ui_.wmult = 0; }
    if (w < 160 || h < 120) return;
    gApp.winw = w; gApp.winh = h; gApp.wm_ew = w; gApp.wm_eh = h; gApp.wm_expect_until = SDL_GetTicks() + 1500u;
    if (gApp.win) {
        if (SDL_GetWindowFlags(gApp.win) & SDL_WINDOW_MAXIMIZED) SDL_RestoreWindow(gApp.win);
        SDL_SetWindowSize(gApp.win, w, h); SDL_SetWindowPosition(gApp.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    gApp.wm_dirty = true; gApp.wm_last = SDL_GetTicks(); gApp.ui_.dirty = true;
    gh_log("WINDOW size %dx%d mult=%d (max x%d)", w, h, gApp.ui_.wmult, gApp.ui_.wmax);
    if (osd) { char b[64]; if (m > 0) snprintf(b, sizeof b, "창 x%d (%dx%d)", m, w, h); else snprintf(b, sizeof b, "창 사용자 지정 (%dx%d)", w, h); gh_osd(b); }
}
static void gh_titlebar_event(const SDL_Event * e);   /* t52a (#54/#55) */
static int SDLCALL gh_ev_watch(void *, SDL_Event * e) {
    gh_titlebar_event(e);
    kha_ev_watch(e);
    if (e->type != SDL_WINDOWEVENT || (e->window.event != SDL_WINDOWEVENT_SIZE_CHANGED && e->window.event != SDL_WINDOWEVENT_RESIZED) || !gApp.win) return 0;
    int w = e->window.data1, h = e->window.data2; unsigned now = SDL_GetTicks();
    if (!gh_win_userresize(w, h, SDL_GetWindowFlags(gApp.win), gApp.wm_ew, gApp.wm_eh, now, gApp.wm_expect_until)) return 0;
    int m = 0; for (int k = 1; k <= ui::Ui::WMN; k++) if (w == 640 * k && h == 480 * k) m = k;   /* 끌다가 정확히 배율 크기가 되면 배율로 */
    gApp.ui_.wmult = m; gApp.ui_.winw = w; gApp.ui_.winh = h; gApp.winw = w; gApp.winh = h; gApp.wm_dirty = true; gApp.wm_last = now; gApp.ui_.dirty = true;
    return 0;
}
static void gh_wm_poll(void) {   /* 끄는 동안은 기다렸다가(0.6초 조용하면) 한 번만 저장 + 로그 */
    if (gApp.wm_dirty && SDL_GetTicks() - gApp.wm_last > 600u) {
        gApp.wm_dirty = false; ui::settings_save(gApp.ui_, gApp.cfgpath); gh_log("WINDOW saved: mult=%d custom=%dx%d", gApp.ui_.wmult, gApp.ui_.winw, gApp.ui_.winh);
        if (!gApp.ui_.wmult) gh_osd(phr_("창 사용자 지정 (") + std::to_string(gApp.ui_.winw) + "x" + std::to_string(gApp.ui_.winh) + ")");
    }
}
/* Alt+[ / Alt+] : 한 단계 작은/큰 배율 (사용자 지정에서는 그 크기 바로 아래/위 배율) */
static bool gh_wm_hotkey(const SDL_Event & e) {
    if (e.type != SDL_KEYDOWN || !(e.key.keysym.mod & KMOD_ALT) || gApp.headless || !gApp.win) return false;
    SDL_Keycode k = e.key.keysym.sym; if (k != SDLK_LEFTBRACKET && k != SDLK_RIGHTBRACKET) return false;
    gh_wm_refresh(); if (!(SDL_GetWindowFlags(gApp.win) & SDL_WINDOW_FULLSCREEN_DESKTOP) || true) { int m = gApp.ui_.wm_next(k == SDLK_RIGHTBRACKET ? 1 : -1); if (m > 0) gh_win_apply(m, true); else gh_osd(k == SDLK_RIGHTBRACKET ? phr_("더 큰 배율이 모니터에 안 들어갑니다") : phr_("더 작은 배율이 없습니다")); }
    return true;
}
/* t46z (#548): kbshader=0(기본) = 화면 키보드·버튼·배속 ▶▶·OSD 를 g_pub 이 아니라 별도 버퍼에 그려 셰이더 뒤에 평면으로 얹는다. 1 = t46y 와 같은 경로(셰이더를 받음).
   별도 버퍼의 알파는 「검정 바탕 / 흰 바탕」 2회 합성의 차이로 뽑는다: 바탕 d 위에 알파 a 로 합성하면 결과 = c*a + d*(1-a) 이므로 W-B = 255*(1-a), B = premultiplied 색. */
static const char * g_overlay_src =
    "varying vec2 vUV;\n#ifdef VERTEX\nattribute vec2 aPos;\nattribute vec2 aUV;\nvoid main(){ gl_Position = vec4(aPos, 0.0, 1.0); vUV = aUV; }\n#endif\n"
    "#ifdef FRAGMENT\nuniform sampler2D uTex;\nvoid main(){ gl_FragColor = texture2D(uTex, vUV); }\n#endif\n";
static void gh_ovl_draw_all(uint32_t * px, int fw, int fh) { ov::Canvas oc = { px, fw, fh }; kbov::draw(px, fw, fh); gh_boost_osd_draw(oc, gApp.osd, gApp.osd_until && SDL_GetTicks() < gApp.osd_until); }
static bool gh_ovl_build(int fw, int fh) {   /* g_fb_mtx 를 잡은 채로 호출 */
    bool osd_on = gApp.osd_until && SDL_GetTicks() < gApp.osd_until;
    if (fw <= 0 || fh <= 0 || !(kbov::active() || osd_on || (gApp.boost_on && gApp.bosd_on) || !gApp.bk_osd.empty() || gApp.fps_on)) return false;
    size_t n = (size_t)fw * fh; gApp.ovt_b.assign(n, 0xFF000000u); gApp.ovt_w.assign(n, 0xFFFFFFFFu);
    gh_ovl_draw_all(gApp.ovt_b.data(), fw, fh); gh_ovl_draw_all(gApp.ovt_w.data(), fw, fh);
    gApp.ovl.resize(n); bool any = false;
    for (size_t i = 0; i < n; i++) {
        uint32_t b = gApp.ovt_b[i], w = gApp.ovt_w[i];
        int dr = (int)((w >> 16) & 255) - (int)((b >> 16) & 255), dg = (int)((w >> 8) & 255) - (int)((b >> 8) & 255), db = (int)(w & 255) - (int)(b & 255);
        int dm = dr > dg ? dr : dg; if (db > dm) dm = db; if (dm < 0) dm = 0; if (dm > 255) dm = 255;
        int a = 255 - dm; if (a <= 0) { gApp.ovl[i] = 0; continue; }
        int r = (int)((b >> 16) & 255), g = (int)((b >> 8) & 255), bl = (int)(b & 255); if (r > a) r = a; if (g > a) g = a; if (bl > a) bl = a;
        gApp.ovl[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl; any = true;
    }
    return any;
}
static void gh_ovl_off(void) { if (gApp.ovl_has) { gApp.ovl_has = false; gApp.ovl_new = true; } }   /* g_fb_mtx 를 잡은 채로 */
static void gh_ovl_update(int fw, int fh) {   /* g_fb_mtx 를 잡은 채로. 배속 중 프레임이 몰려도 16ms 에 한 번만 다시 만든다 */
    static Uint32 last = 0; static bool first = true; Uint32 now = SDL_GetTicks();
    if (!first && now - last < 16) return; first = false; last = now;
    bool b = gh_ovl_build(fw, fh); gApp.ovl_has = b; gApp.ovl_w = fw; gApp.ovl_h = fh; gApp.ovl_new = true;
}

static const char * g_nearest_src =
    "varying vec2 vUV;\n#ifdef VERTEX\nattribute vec2 aPos;\nattribute vec2 aUV;\nvoid main(){ gl_Position = vec4(aPos, 0.0, 1.0); vUV = aUV; }\n#endif\n"
    "#ifdef FRAGMENT\nuniform sampler2D uTex;\nvoid main(){ gl_FragColor = vec4(texture2D(uTex, vUV).rgb, 1.0); }\n#endif\n";
static bool gl_build(const std::string & src, GProg & g, bool linear, const char * name) {
    g = GProg(); g.linear = linear;
    unsigned int sh[2] = { 0, 0 }; std::string gs = gh_es_src(src);
    for (int i = 0; i < 2; i++) {
        sh[i] = F.CreateShader(i == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        const char * parts[4] = { GH_GLSL_VER, i == 0 ? "#define VERTEX\n" : "#define FRAGMENT\n", i == 0 ? "" : GH_FRAG_PREC, gs.c_str() };
        F.ShaderSource(sh[i], 4, parts, NULL); F.CompileShader(sh[i]);
        int ok = 0; F.GetShaderiv(sh[i], GL_COMPILE_STATUS, &ok);
        if (!ok) { char lg[1024]; int n = 0; F.GetShaderInfoLog(sh[i], 1023, &n, lg); lg[n < 0 ? 0 : n] = 0; gh_log("GL: shader %s %s compile FAIL: %s", name, i == 0 ? "vertex" : "fragment", lg); return false; }
    }
    g.p = F.CreateProgram(); F.AttachShader(g.p, sh[0]); F.AttachShader(g.p, sh[1]);
    F.BindAttribLocation(g.p, 0, "aPos"); F.BindAttribLocation(g.p, 1, "aUV"); F.LinkProgram(g.p);
    int ok = 0; F.GetProgramiv(g.p, GL_LINK_STATUS, &ok);
    if (!ok) { char lg[1024]; int n = 0; F.GetProgramInfoLog(g.p, 1023, &n, lg); lg[n < 0 ? 0 : n] = 0; gh_log("GL: shader %s link FAIL: %s", name, lg); return false; }
    F.DeleteShader(sh[0]); F.DeleteShader(sh[1]);
    g.uTex = F.GetUniformLocation(g.p, "uTex"); g.uSrc = F.GetUniformLocation(g.p, "uSrcSize"); g.uOut = F.GetUniformLocation(g.p, "uOutSize"); g.uCurve = F.GetUniformLocation(g.p, "uCurve"); g.uScan = F.GetUniformLocation(g.p, "uScan"); g.uMask = F.GetUniformLocation(g.p, "uMask"); g.uBleed = F.GetUniformLocation(g.p, "uBleed"); g.uGlow = F.GetUniformLocation(g.p, "uGlow"); g.uNear = F.GetUniformLocation(g.p, "uNear");
    g.ok = true; gh_log("GL: shader %s OK (prog %u)", name, g.p);
    return true;
}
static bool gl_readfile(const std::string & path, std::string & out) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
    char b[4096]; size_t n; out.clear(); while ((n = fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
    fclose(f); return true;
}
/* r012: crt 셰이더의 SCANLINE/CGWG #define 줄을 uniform 으로 바꿔 화면 설정의 막대가 바로 먹게 한다 */
static void gl_patch_crt(std::string & src) {
    struct R { const char * key; const char * rep; };
    static const R rs[] = { { "#define SCANLINE", "uniform float uScan;\n#define SCANLINE uScan" }, { "#define CGWG", "uniform float uMask;\n#define CGWG uMask" }, { "#define BLEED", "uniform float uBleed;\n#define BLEED uBleed" } };
    for (int k = 0; k < 3; k++) {
        size_t p = src.find(rs[k].key); if (p == std::string::npos) { gh_log("GL: crt patch: %s not found", rs[k].key); continue; }
        size_t e = src.find('\n', p); if (e == std::string::npos) e = src.size();
        src.replace(p, e - p, rs[k].rep);
    }
}
static bool gl_init_all(void) {   /* 메인 스레드, 컨텍스트 current 상태에서 */
#if defined(GH_GLES)
    if (!gh_gles_load()) return false;
#endif
    if (!gl_load()) return false;
    gh_log("GL: vendor='%s' renderer='%s' version='%s'", (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
    gl_build(g_nearest_src, gApp.prog[SH_NEAREST], false, "builtin-nearest");
    if (!gApp.prog[SH_NEAREST].ok) return false;
    gl_build(g_overlay_src, gApp.prog_ov, true, "builtin-overlay");
    const char * names[SH_N] = { "", "pc98_sharp.glsl", "pc98_crt.glsl" };
    for (int i = SH_SHARP; i < SH_N; i++) {
        std::string src, path = gApp.shaderdir + "\\" + names[i];
        if (!gl_readfile(path, src)) {   /* r019: 파일이 없으면 내장 셰이더 + 화면 경고 */
            const unsigned char * ep = (i == SH_SHARP) ? emb_sh_sharp : emb_sh_crt; size_t en = (i == SH_SHARP) ? emb_sh_sharp_n : emb_sh_crt_n;
            src.assign((const char *)ep, en); gApp.builtin_res = true; if (gApp.osd.empty()) gh_osd(phr_("셰이더 파일이 없어 내장 셰이더로 실행합니다 (exe 옆 shaders 폴더 확인)"), 12000); gh_log("WARN: GL: shader file not found: %s -> 내장 셰이더 사용", path.c_str());
        }
        if (i == SH_CRT && src.find("PC98_FX_T48C") == std::string::npos) { gh_log("GL: pc98_crt.glsl 에 효과 표지(PC98_FX_T48C)가 없다(옛 판) -> 내장 셰이더 사용"); src.assign((const char *)emb_sh_crt, emb_sh_crt_n); }   /* t48c: 옛 res 폴더의 셰이더 파일이 남아 있어도 새 효과가 먹게 한다 */
        if (!gl_build(src, gApp.prog[i], true, names[i])) gApp.prog[i] = gApp.prog[SH_NEAREST];
    }
    static const float quad[16] = { -1, -1, 0, 1,   1, -1, 1, 1,   -1, 1, 0, 0,   1, 1, 1, 0 };
    F.GenBuffers(1, &gApp.vbo); F.BindBuffer(GL_ARRAY_BUFFER, gApp.vbo); F.BufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenTextures(1, &gApp.tex_gl); glBindTexture(GL_TEXTURE_2D, gApp.tex_gl);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return true;
}
static void gl_rect(int dw, int dh, int w, int h, int fit, int & x, int & y, int & rw, int & rh) {
    if (fit == FIT_INT && w > 0 && h > 0) {
        int s = std::min(dw / w, dh / h);
        if (s >= 1) { rw = w * s; rh = h * s; x = (dw - rw) / 2; y = (dh - rh) / 2; return; }
    }
    double A = fit == FIT_1610 ? 1.6 : (fit == FIT_INT && w > 0 && h > 0 ? (double)w / h : 4.0 / 3.0);
    rw = dw; rh = (int)(rw / A + 0.5);
    if (rh > dh) { rh = dh; rw = (int)(rh * A + 0.5); }
    x = (dw - rw) / 2; y = (dh - rh) / 2;
}
static int SDLCALL gl_thread(void *) {
    SDL_GL_MakeCurrent(gApp.win, gApp.glctx);
    int tw = 0, th = 0, cw = 0, ch = 0; int ovw = 0, ovh = 0, otw = 0, oth = 0; bool ov_has = false, ov_dirty = false; std::vector<uint32_t> sw;
    GLuint vp_tex = 0; std::vector<uint32_t> vp_buf; unsigned vp_mask = ~0u; int vp_w = 0, vp_h = 0, vp_sx = -1, vp_sw = -1;   // virtual pad
    while (!gApp.gl_stop) {
        { unsigned hd = g_kha_hold; if (hd && (hd == 0xFFFFFFFFu || (int)(SDL_GetTicks() - hd) < 0)) { g_kha_glidle = 1; g_kha_was = 1; SDL_GL_MakeCurrent(gApp.win, NULL); SDL_Delay(10); continue; } g_kha_glidle = 0; if (g_kha_was) { g_kha_was = 0; g_kha_rl = 4; } if (g_kha_rl > 0) { g_kha_rl--; int rc0 = SDL_GL_MakeCurrent(gApp.win, gApp.glctx); gh_log("KHA glres rc=%d err=%s cur=%p ctx=%p", rc0, rc0 ? SDL_GetError() : "", (void *)SDL_GL_GetCurrentContext(), (void *)gApp.glctx); } }   /* t146 glres (a127) */
        int dw = 0, dh = 0; SDL_GL_GetDrawableSize(gApp.win, &dw, &dh);
        bool nf = false;
        SDL_LockMutex(gApp.fb_mtx);
        if (gApp.pub_new) { std::swap(gApp.pub, gApp.rbuf); cw = gApp.pub_w; ch = gApp.pub_h; gApp.pub_new = false; nf = true; }
        if (gApp.ovl_new) { std::swap(gApp.ovl, gApp.ovl_gl); ovw = gApp.ovl_w; ovh = gApp.ovl_h; ov_has = gApp.ovl_has; gApp.ovl_new = false; ov_dirty = true; }

        SDL_UnlockMutex(gApp.fb_mtx);
        glViewport(0, 0, dw, dh); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        int sx = 0, sy = 0, srw = 0, srh = 0;
        if (cw > 0 && ch > 0 && gApp.rbuf.size() >= (size_t)cw * ch) {
            glBindTexture(GL_TEXTURE_2D, gApp.tex_gl);
            if (cw != tw || ch != th) { glTexImage2D(GL_TEXTURE_2D, 0, GH_INTFMT, cw, ch, 0, GH_PIXFMT, GL_UNSIGNED_BYTE, gh_up(gApp.rbuf, (size_t)cw * ch, sw)); tw = cw; th = ch; }
            else if (nf) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch, GH_PIXFMT, GL_UNSIGNED_BYTE, gh_up(gApp.rbuf, (size_t)cw * ch, sw));
            int fit = gApp.fit, sh = gApp.shader;
            if (fit == FIT_INT) sh = SH_NEAREST;
            const GProg & P = gApp.prog[sh];
            int x, y, rw, rh; gl_rect(dw, dh, cw, ch, fit, x, y, rw, rh);
            sx = x; sy = y; srw = rw; srh = rh;
            glViewport(x, y, rw, rh);
            GLint filt = P.linear ? GL_LINEAR : GL_NEAREST;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
            Uint64 gt_a = SDL_GetPerformanceCounter();
            F.UseProgram(P.p);
            if (P.uTex >= 0) F.Uniform1i(P.uTex, 0);
            if (P.uSrc >= 0) F.Uniform2f(P.uSrc, (float)cw, (float)ch);
            if (P.uOut >= 0) F.Uniform2f(P.uOut, (float)rw, (float)rh);
            if (P.uCurve >= 0) F.Uniform1f(P.uCurve, gApp.curve);
            if (P.uScan >= 0) F.Uniform1f(P.uScan, gApp.scanv);
            if (P.uMask >= 0) F.Uniform1f(P.uMask, gApp.maskv);
            if (P.uBleed >= 0) F.Uniform1f(P.uBleed, gApp.bleedv);
            if (P.uGlow >= 0) F.Uniform1f(P.uGlow, gApp.glowv);
            if (P.uNear >= 0) F.Uniform1f(P.uNear, gApp.nearv);
            F.BindBuffer(GL_ARRAY_BUFFER, gApp.vbo);
            F.EnableVertexAttribArray(0); F.EnableVertexAttribArray(1);
            F.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (const void *)0); F.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void *)8);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            if (gApp.gltime) {   /* t42: 그리기 시간(glFinish 까지) 3초 평균 */
                static double gt_sum = 0, gt_max = 0; static int gt_n = 0; static Uint64 gt_t0 = 0;
                glFinish(); Uint64 gt_b = SDL_GetPerformanceCounter(); double fq = (double)SDL_GetPerformanceFrequency();
                double ms = (double)(gt_b - gt_a) * 1000.0 / fq; gt_sum += ms; if (ms > gt_max) gt_max = ms; gt_n++;
                if (gt_t0 == 0) gt_t0 = gt_b;
                if ((double)(gt_b - gt_t0) / fq >= 3.0) { gh_log("GLTIME 그리기 평균 %.3f ms (최대 %.3f, %d프레임, glFinish 기준) shader=%d scan=%.1f mask=%.2f bleed=%.2f glow=%.2f near=%.0f curve=%.2f out=%dx%d", gt_sum / gt_n, gt_max, gt_n, sh, (double)gApp.scanv, (double)gApp.maskv, (double)gApp.bleedv, (double)gApp.glowv, (double)gApp.nearv, (double)gApp.curve, rw, rh); gt_sum = 0; gt_max = 0; gt_n = 0; gt_t0 = gt_b; }
            }
        }
        if (ov_has && !gApp.kbshader && !gApp.shot_req && srw > 0 && srh > 0 && ovw > 0 && ovh > 0 && gApp.ovl_gl.size() >= (size_t)ovw * ovh && gApp.prog_ov.ok) {   /* t46z: 셰이더 사각형과 같은 뷰포트에 평면 알파 합성(premultiplied) */
            if (gApp.ov_tex == 0) { glGenTextures(1, &gApp.ov_tex); glBindTexture(GL_TEXTURE_2D, gApp.ov_tex); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); otw = 0; oth = 0; ov_dirty = true; }
            glBindTexture(GL_TEXTURE_2D, gApp.ov_tex);
            if (ov_dirty) { if (ovw != otw || ovh != oth) { glTexImage2D(GL_TEXTURE_2D, 0, GH_INTFMT, ovw, ovh, 0, GH_PIXFMT, GL_UNSIGNED_BYTE, gh_up(gApp.ovl_gl, (size_t)ovw * ovh, sw)); otw = ovw; oth = ovh; } else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ovw, ovh, GH_PIXFMT, GL_UNSIGNED_BYTE, gh_up(gApp.ovl_gl, (size_t)ovw * ovh, sw)); ov_dirty = false; }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glViewport(sx, sy, srw, srh);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            F.UseProgram(gApp.prog_ov.p); if (gApp.prog_ov.uTex >= 0) F.Uniform1i(gApp.prog_ov.uTex, 0);
            F.BindBuffer(GL_ARRAY_BUFFER, gApp.vbo); F.EnableVertexAttribArray(0); F.EnableVertexAttribArray(1);
            F.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (const void *)0); F.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void *)8);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisable(GL_BLEND); glBindTexture(GL_TEXTURE_2D, gApp.tex_gl);
        }
        if (vpad::g_show && srw > 0 && dw > 0 && dh > 0 && gApp.prog_ov.ok) {   // virtual pad in the side bands (window space)
            vpad::Geo vg; vpad::layout(dw, dh, sx, srw, vg);
            if (vg.on) {
                unsigned vm = vpad::g_mask;
                if (vp_tex == 0) {
                    glGenTextures(1, &vp_tex); glBindTexture(GL_TEXTURE_2D, vp_tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                }
                glBindTexture(GL_TEXTURE_2D, vp_tex);
                if (vm != vp_mask || dw != vp_w || dh != vp_h || sx != vp_sx || srw != vp_sw) {
                    vp_buf.assign((size_t)dw * dh, 0); vpad::render(vp_buf.data(), dw, dh, vg, vm);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, dw, dh, 0, GL_BGRA, GL_UNSIGNED_BYTE, vp_buf.data());
                    vp_mask = vm; vp_w = dw; vp_h = dh; vp_sx = sx; vp_sw = srw;
                }
                glViewport(0, 0, dw, dh); glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                F.UseProgram(gApp.prog_ov.p); if (gApp.prog_ov.uTex >= 0) F.Uniform1i(gApp.prog_ov.uTex, 0);
                F.BindBuffer(GL_ARRAY_BUFFER, gApp.vbo); F.EnableVertexAttribArray(0); F.EnableVertexAttribArray(1);
                F.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (const void *)0); F.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void *)8);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glDisable(GL_BLEND); glBindTexture(GL_TEXTURE_2D, gApp.tex_gl);
            }
        }
        if (gApp.shot_req) {
            int r0x = 0, r0y = 0, rwd = dw, rht = dh;

            if (gApp.shot_crop && srw > 0 && srh > 0 && sx >= 0 && sy >= 0 && sx + srw <= dw && sy + srh <= dh) { r0x = sx; r0y = sy; rwd = srw; rht = srh; }
            std::vector<uint32_t> px((size_t)rwd * rht), fl((size_t)rwd * rht);
            glReadPixels(r0x, r0y, rwd, rht, GH_PIXFMT, GL_UNSIGNED_BYTE, px.data()); gh_swz(px);
            for (int yy = 0; yy < rht; yy++) memcpy(&fl[(size_t)yy * rwd], &px[(size_t)(rht - 1 - yy) * rwd], (size_t)rwd * 4);
            gh_png(gApp.shot_path, fl.data(), rwd, rht);
            gApp.shot_w = rwd; gApp.shot_h = rht;
            gApp.shot_req = 0;
        }
        SDL_GL_SwapWindow(gApp.win);
        if (!gApp.vsync) SDL_Delay(8);
    }
    SDL_GL_MakeCurrent(gApp.win, NULL);
    return 0;
}

bool TempLineAlloc(unsigned int w); void TempLineFree(void);  /* vga_draw.cpp: 진짜 RENDER_SetSize(render.cpp:1055)가 하던 몫 */
void RENDER_SetSize(Bitu width, Bitu height, Bitu bpp, float fps, double scrn_ratio) {
    TempLineFree(); TempLineAlloc((unsigned)width);
    gh_log("RENDER_SetSize %ux%u bpp=%u fps=%.3f ratio=%.4f", (unsigned)width, (unsigned)height, (unsigned)bpp, fps, scrn_ratio);
    gApp.fps = fps;
    if (bpp != 32) gh_log("WARN: bpp %u not supported by host sink", (unsigned)bpp);
    gApp.fbw = (int)width; gApp.fbh = (int)height; gApp.fb.assign((size_t)gApp.fbw * gApp.fbh, 0);
    RENDER_DrawLine = gh_drawline;
    if (!gApp.headless && gApp.use_gl) {
        /* GL 경로: 창 크기는 사용자/전체화면이 정한다(프레임 크기 변경은 텍스처만 다시 만듦) */
        SDL_LockMutex(gApp.fb_mtx);
        gApp.pub.assign((size_t)gApp.fbw * gApp.fbh, 0); gApp.pub_w = gApp.fbw; gApp.pub_h = gApp.fbh; gApp.pub_new = true; gh_ovl_off();
        SDL_UnlockMutex(gApp.fb_mtx);
    } else if (!gApp.headless && gApp.ren) {
        if (gApp.tex) SDL_DestroyTexture(gApp.tex);
        gApp.tex = SDL_CreateTexture(gApp.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, gApp.fbw, gApp.fbh);
        int sc = 1; while ((gApp.fbw * (sc + 1) <= 1600) && (gApp.fbh * (sc + 1) <= 1000)) sc++;
        SDL_SetWindowSize(gApp.win, gApp.fbw * sc, gApp.fbh * sc);

#if defined(__ANDROID__)
        SDL_RenderSetLogicalSize(gApp.ren, gApp.fbw, gApp.fbw * 3 / 4);   /* fixed 4:3 outer frame */
#else
        SDL_RenderSetLogicalSize(gApp.ren, gApp.fbw, gApp.fbh);
#endif

    }
}
