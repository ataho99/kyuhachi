/* gensei_host.cpp -- GENSEI mini-emulator host, stage 2b (r007 test build)
 *
 * Replaces sdlmain.cpp for the PC-98 subset link: Config creation, embedded profile, DOSBOX_SetupConfigSections,
 * minimal device init, direct boot (no DOS kernel / shell), SDL window sink (RENDER_*), log, headless test hooks.
 * Everything not defined here comes from the extracted DOSBox-X objects or from auto stubs (xor eax,eax; ret).
 */
#define SDL_MAIN_HANDLED 1
#define NOMINMAX 1
#include <algorithm>
#include "dosbox.h"
#include "control.h"
#include "setup.h"
#include "cross.h"
#include "regs.h"
#include "callback.h"
#include "cpu.h"
#include "mem.h"
#include "bios.h"
#include "bios_disk.h"
#include "dos_inc.h"
#include "ide.h"
#include "inout.h"
#include "dma.h"
#include "keyboard.h"
#include "mapper.h"
#include "render.h"
#include "vga.h"
#include "logging.h"
#include "menu.h"
#include "video.h"
#include "pic.h"
#include <SDL.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>
#if defined(WIN32)
# include <windows.h>
#endif
#include "gensei_input.h"
#include "pc98_savestate.h"
#include "pc98_gdc.h"

/* ------------------------------------------------------------------ declarations (copied from sdlmain.cpp) */
void MSG_Init(); void DOSBOX_RealInit(); void DOSBOX_InitTickLoop(); void DOSBOX_SetupConfigSections(void);
void PAGING_Init(); void IO_Init(); void Init_VGABIOS(); void Init_AddressLimitAndGateMask(); void Init_RAM();
void Init_MemHandles(); void Init_MemoryAccessArray(); void Init_PS2_Port_92h(); void Init_A20_Gate();
void HARDWARE_Init(); void ROMBIOS_Init(); void CALLBACK_Init(); void Init_DMA(); void Init_PIC(); void PCIBUS_Init();
void PROGRAMS_Init(); void TIMER_Init(); void CMOS_Init(); void VGA_Init(); void CPU_Init(); void Weitek_Init();
void CPU_PreInit(); void ISAPNP_Cfg_Init(); void FPU_Init(); void KEYBOARD_Init(); void MIXER_Init(); void MIDI_Init();
void MPU401_Init(); void SBLASTER_Init(); void GUS_Init(); void IMFC_Init(); void INNOVA_Init(); void WSS_Init();
void PCSPEAKER_Init(); void TANDYSOUND_Init(); void DISNEY_Init(); void PS1SOUND_Init(); void BIOS_Init();
void INT10_Init(); void JOYSTICK_Init(); void SERIAL_Init(); void PARALLEL_Init(); void DONGLE_Init(); void IDE_Init();
void NE2K_Init(); void FDC_Primary_Init(); void AllocCallback2(); void MEM_A20_Enable(bool enabled);
bool FDC_AssignINT13Disk(unsigned char drv);
void PC98_show_cursor(bool show);
bool VM_PowerOn_gensei();
extern Bitu BIOS_bootfail_code_offset;

/* stub call counters (asm, generated per link round) */
extern "C" unsigned int gensei_stub_counts[];
extern "C" const char * const gensei_stub_names[];
extern "C" const int gensei_stub_n;

/* ------------------------------------------------------------------ globals that must be real (not zero blobs) */
DOS_Block dos;
bool dos_kernel_disabled = true;
int bootdrive = -1;
bool bootguest = false;
class DOS_Shell;
DOS_Shell * first_shell = NULL;
bool guest_machine_power_on = false;
DOS_Drive * Drives[DOS_DRIVES];
Render_t render;
ScalerLineHandler_t RENDER_DrawLine = NULL;
uint32_t GFX_Rmask = 0x00FF0000, GFX_Gmask = 0x0000FF00, GFX_Bmask = 0x000000FF, GFX_Amask = 0xFF000000;
unsigned char GFX_Rshift = 16, GFX_Gshift = 8, GFX_Bshift = 0, GFX_bpp = 32;

/* ------------------------------------------------------------------ log */
static FILE * g_log = NULL;
static unsigned long long g_log_bytes = 0;
static unsigned int g_log_t0 = 0;
static void gh_vlog(const char * pfx, const char * fmt, va_list ap) {
    if (!g_log) return;
    if (g_log_bytes > (48ull << 20)) return;   /* cap 48 MB */
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    buf[n] = 0;
    unsigned int t = SDL_GetTicks() - g_log_t0;
    g_log_bytes += fprintf(g_log, "%7u %s%s%s", t, pfx, buf, (n > 0 && buf[n - 1] == '\n') ? "" : "\n");
}
static void gh_log(const char * fmt, ...) { va_list ap; va_start(ap, fmt); gh_vlog("HOST ", fmt, ap); va_end(ap); fflush(g_log); }

void DEBUG_ShowMsg(char const * format, ...) { va_list ap; va_start(ap, format); gh_vlog("MSG  ", format, ap); va_end(ap); }
void LOG::operator() (char const * format, ...) {
    const char * sv = "";
    switch (d_severity) { case LOG_DEBUG: sv = "D "; break; case LOG_NORMAL: sv = "N "; break; case LOG_WARN: sv = "W "; break; case LOG_ERROR: sv = "E "; break; case LOG_FATAL: sv = "F "; break; default: break; }
    va_list ap; va_start(ap, format); gh_vlog(sv, format, ap); va_end(ap);
}

/* ------------------------------------------------------------------ profile */
struct GKey { unsigned int at_ms; KBD_KEYS key; };
struct GProfile {
    const char * id;
    const char * disk[4];        /* index 0=A 1=B 2=C 3=D (bios_disk index) */
    char bootletter;             /* 'A' or 'C' */
    std::vector<GKey> keys;      /* ms after guest boot */
};
static const char * const g_conf_common =
    "[dosbox]\n"
    "machine=pc98\n"
    "captures=capture\n"
    "[cpu]\n"
    "core=normal\n"
    "cycles=fixed 3000\n"
    "[render]\n"
    "scaler=none\n"
    "aspect=false\n"
    "[mixer]\n"
    "rate=44100\n"
    "blocksize=512\n"
    "prebuffer=20\n"
    "[pc98]\n"
    "pc-98 anex86 font=gensei_np2kai_font.bmp\n";

static bool gh_profile(const std::string & id, GProfile & p) {
    p = GProfile();
    p.id = "";
    if (id == "hukyou" || id == "kaitou") {
        p.id = id == "hukyou" ? "hukyou" : "kaitou";
        p.disk[0] = id == "hukyou" ? "hukyou_kr.fdi" : "kaitou_kr.fdi";
        p.bootletter = 'A';
        p.keys = { {15000, KBD_enter}, {17000, KBD_enter}, {19000, KBD_enter} };
        return true;
    }
    if (id == "kitan") {
        p.id = "kitan"; p.disk[0] = "kitan-system.fdi"; p.disk[1] = "kitan-data.fdi"; p.bootletter = 'A';
        p.keys = { {15000, KBD_space}, {17000, KBD_space}, {19000, KBD_enter} };
        return true;
    }
    if (id == "torimono") {
        p.id = "torimono"; p.disk[2] = "torimono_kr.hdi"; p.bootletter = 'C';
        p.keys = { {15000, KBD_enter}, {17000, KBD_enter}, {19000, KBD_enter} };
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ run options */
static std::string g_imgdir = ".", g_outdir = ".";
static bool g_headless = false;
static unsigned int g_run_ms = 0;
static std::vector<unsigned int> g_shots_ms;
static GProfile g_prof;
/* r009 세이브스테이트 시험: --save SEC / --load SEC (게임 부팅 후 벽시계 초) --state FILE, 이벤트 뒤 0.1s·1s·3s 캡처 */
static unsigned int g_save_ms = 0, g_load_ms = 0;
static std::string g_state_path;
static bool g_save_done = false, g_load_done = false;
static unsigned int g_ev_tick = 0;        /* SDL ticks at save/load event */
static std::vector<Pc98StateDisk> g_disks;
static unsigned int g_boot_tick = 0;      /* SDL ticks at guest boot, 0 = not yet */
static bool g_quit = false;

/* ------------------------------------------------------------------ video sink */
static SDL_Window * g_win = NULL;
static SDL_Renderer * g_ren = NULL;
static SDL_Texture * g_tex = NULL;
static std::vector<uint32_t> g_fb;
static int g_fbw = 0, g_fbh = 0, g_cury = 0;
static unsigned long long g_frames = 0, g_starts = 0, g_aborts = 0; static Bitu g_pic0 = 0;
static double g_fps = 0;

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
    if (g_cury >= 0 && g_cury < g_fbh && g_fbw > 0) memcpy(&g_fb[(size_t)g_cury * g_fbw], src, (size_t)g_fbw * 4);
    g_cury++;
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
enum { FIT_43 = 0, FIT_1610 = 1, FIT_INT = 2 };
enum { SH_NEAREST = 0, SH_SHARP = 1, SH_CRT = 2, SH_N = 3 };
static volatile int g_fit = FIT_43, g_shader = SH_SHARP;
static volatile float g_curve = 0.0f;
static bool g_windowed = false, g_use_gl = false;
static int g_winw = 1280, g_winh = 800;
static std::string g_shaderdir;
static SDL_GLContext g_glctx = NULL;
static SDL_Thread * g_glthr = NULL;
static volatile bool g_gl_stop = false;
static SDL_mutex * g_fb_mtx = NULL;
static std::vector<uint32_t> g_pub, g_rbuf;
static int g_pub_w = 0, g_pub_h = 0;
static bool g_pub_new = false;
static volatile int g_shot_req = 0; static char g_shot_path[1024];
static bool g_vsync = false;
static unsigned int g_glmx_ms = 0; static bool g_glmx_done = false;
struct GProg { unsigned int p; int uTex, uSrc, uOut, uCurve; bool ok, linear; };
static GProg g_prog[SH_N];
static unsigned int g_vbo = 0, g_tex_gl = 0;
static const char * g_nearest_src =
    "varying vec2 vUV;\n#ifdef VERTEX\nattribute vec2 aPos;\nattribute vec2 aUV;\nvoid main(){ gl_Position = vec4(aPos, 0.0, 1.0); vUV = aUV; }\n#endif\n"
    "#ifdef FRAGMENT\nuniform sampler2D uTex;\nvoid main(){ gl_FragColor = vec4(texture2D(uTex, vUV).rgb, 1.0); }\n#endif\n";
static bool gl_build(const std::string & src, GProg & g, bool linear, const char * name) {
    g = GProg(); g.linear = linear;
    unsigned int sh[2] = { 0, 0 };
    for (int i = 0; i < 2; i++) {
        sh[i] = F.CreateShader(i == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        const char * parts[3] = { "#version 120\n", i == 0 ? "#define VERTEX\n" : "#define FRAGMENT\n", src.c_str() };
        F.ShaderSource(sh[i], 3, parts, NULL); F.CompileShader(sh[i]);
        int ok = 0; F.GetShaderiv(sh[i], GL_COMPILE_STATUS, &ok);
        if (!ok) { char lg[1024]; int n = 0; F.GetShaderInfoLog(sh[i], 1023, &n, lg); lg[n < 0 ? 0 : n] = 0; gh_log("GL: shader %s %s compile FAIL: %s", name, i == 0 ? "vertex" : "fragment", lg); return false; }
    }
    g.p = F.CreateProgram(); F.AttachShader(g.p, sh[0]); F.AttachShader(g.p, sh[1]);
    F.BindAttribLocation(g.p, 0, "aPos"); F.BindAttribLocation(g.p, 1, "aUV"); F.LinkProgram(g.p);
    int ok = 0; F.GetProgramiv(g.p, GL_LINK_STATUS, &ok);
    if (!ok) { char lg[1024]; int n = 0; F.GetProgramInfoLog(g.p, 1023, &n, lg); lg[n < 0 ? 0 : n] = 0; gh_log("GL: shader %s link FAIL: %s", name, lg); return false; }
    F.DeleteShader(sh[0]); F.DeleteShader(sh[1]);
    g.uTex = F.GetUniformLocation(g.p, "uTex"); g.uSrc = F.GetUniformLocation(g.p, "uSrcSize"); g.uOut = F.GetUniformLocation(g.p, "uOutSize"); g.uCurve = F.GetUniformLocation(g.p, "uCurve");
    g.ok = true; gh_log("GL: shader %s OK (prog %u)", name, g.p);
    return true;
}
static bool gl_readfile(const std::string & path, std::string & out) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
    char b[4096]; size_t n; out.clear(); while ((n = fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
    fclose(f); return true;
}
static bool gl_init_all(void) {   /* 메인 스레드, 컨텍스트 current 상태에서 */
    if (!gl_load()) return false;
    gh_log("GL: vendor='%s' renderer='%s' version='%s'", (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
    gl_build(g_nearest_src, g_prog[SH_NEAREST], false, "builtin-nearest");
    if (!g_prog[SH_NEAREST].ok) return false;
    const char * names[SH_N] = { "", "pc98_sharp.glsl", "pc98_crt.glsl" };
    for (int i = SH_SHARP; i < SH_N; i++) {
        std::string src, path = g_shaderdir + "\\" + names[i];
        if (!gl_readfile(path, src)) { gh_log("GL: shader file not found: %s -> nearest", path.c_str()); g_prog[i] = g_prog[SH_NEAREST]; continue; }
        if (!gl_build(src, g_prog[i], true, names[i])) g_prog[i] = g_prog[SH_NEAREST];
    }
    static const float quad[16] = { -1, -1, 0, 1,   1, -1, 1, 1,   -1, 1, 0, 0,   1, 1, 1, 0 };
    F.GenBuffers(1, &g_vbo); F.BindBuffer(GL_ARRAY_BUFFER, g_vbo); F.BufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenTextures(1, &g_tex_gl); glBindTexture(GL_TEXTURE_2D, g_tex_gl);
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
    SDL_GL_MakeCurrent(g_win, g_glctx);
    int tw = 0, th = 0, cw = 0, ch = 0;
    while (!g_gl_stop) {
        int dw = 0, dh = 0; SDL_GL_GetDrawableSize(g_win, &dw, &dh);
        bool nf = false;
        SDL_LockMutex(g_fb_mtx);
        if (g_pub_new) { std::swap(g_pub, g_rbuf); cw = g_pub_w; ch = g_pub_h; g_pub_new = false; nf = true; }
        SDL_UnlockMutex(g_fb_mtx);
        glViewport(0, 0, dw, dh); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        if (cw > 0 && ch > 0 && g_rbuf.size() >= (size_t)cw * ch) {
            glBindTexture(GL_TEXTURE_2D, g_tex_gl);
            if (cw != tw || ch != th) { glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cw, ch, 0, GL_BGRA, GL_UNSIGNED_BYTE, g_rbuf.data()); tw = cw; th = ch; }
            else if (nf) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch, GL_BGRA, GL_UNSIGNED_BYTE, g_rbuf.data());
            int fit = g_fit, sh = g_shader;
            if (fit == FIT_INT) sh = SH_NEAREST;
            const GProg & P = g_prog[sh];
            int x, y, rw, rh; gl_rect(dw, dh, cw, ch, fit, x, y, rw, rh);
            glViewport(x, y, rw, rh);
            GLint filt = P.linear ? GL_LINEAR : GL_NEAREST;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
            F.UseProgram(P.p);
            if (P.uTex >= 0) F.Uniform1i(P.uTex, 0);
            if (P.uSrc >= 0) F.Uniform2f(P.uSrc, (float)cw, (float)ch);
            if (P.uOut >= 0) F.Uniform2f(P.uOut, (float)rw, (float)rh);
            if (P.uCurve >= 0) F.Uniform1f(P.uCurve, g_curve);
            F.BindBuffer(GL_ARRAY_BUFFER, g_vbo);
            F.EnableVertexAttribArray(0); F.EnableVertexAttribArray(1);
            F.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (const void *)0); F.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void *)8);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        if (g_shot_req) {
            std::vector<uint32_t> px((size_t)dw * dh), fl((size_t)dw * dh);
            glReadPixels(0, 0, dw, dh, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
            for (int yy = 0; yy < dh; yy++) memcpy(&fl[(size_t)yy * dw], &px[(size_t)(dh - 1 - yy) * dw], (size_t)dw * 4);
            gh_png(g_shot_path, fl.data(), dw, dh);
            g_shot_req = 0;
        }
        SDL_GL_SwapWindow(g_win);
        if (!g_vsync) SDL_Delay(8);
    }
    SDL_GL_MakeCurrent(g_win, NULL);
    return 0;
}

bool TempLineAlloc(unsigned int w); void TempLineFree(void);  /* vga_draw.cpp: 진짜 RENDER_SetSize(render.cpp:1055)가 하던 몫 */
void RENDER_SetSize(Bitu width, Bitu height, Bitu bpp, float fps, double scrn_ratio) {
    TempLineFree(); TempLineAlloc((unsigned)width);
    gh_log("RENDER_SetSize %ux%u bpp=%u fps=%.3f ratio=%.4f", (unsigned)width, (unsigned)height, (unsigned)bpp, fps, scrn_ratio);
    g_fps = fps;
    if (bpp != 32) gh_log("WARN: bpp %u not supported by host sink", (unsigned)bpp);
    g_fbw = (int)width; g_fbh = (int)height; g_fb.assign((size_t)g_fbw * g_fbh, 0);
    RENDER_DrawLine = gh_drawline;
    if (!g_headless && g_use_gl) {
        /* GL 경로: 창 크기는 사용자/전체화면이 정한다(프레임 크기 변경은 텍스처만 다시 만듦) */
        SDL_LockMutex(g_fb_mtx);
        g_pub.assign((size_t)g_fbw * g_fbh, 0); g_pub_w = g_fbw; g_pub_h = g_fbh; g_pub_new = true;
        SDL_UnlockMutex(g_fb_mtx);
    } else if (!g_headless && g_ren) {
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, g_fbw, g_fbh);
        int sc = 1; while ((g_fbw * (sc + 1) <= 1600) && (g_fbh * (sc + 1) <= 1000)) sc++;
        SDL_SetWindowSize(g_win, g_fbw * sc, g_fbh * sc);
        SDL_RenderSetLogicalSize(g_ren, g_fbw, g_fbh);
    }
}
bool RENDER_StartUpdate(void) { g_cury = 0; g_starts++; return true; }
void RENDER_EndUpdate(bool abort) {
    if (abort) { g_aborts++; return; }
    g_frames++;
    if (!g_headless && g_use_gl && !g_fb.empty()) {
        SDL_LockMutex(g_fb_mtx);
        if (g_pub.size() != g_fb.size()) g_pub.resize(g_fb.size());
        memcpy(g_pub.data(), g_fb.data(), g_fb.size() * 4); g_pub_w = g_fbw; g_pub_h = g_fbh; g_pub_new = true;
        SDL_UnlockMutex(g_fb_mtx);
    } else if (!g_headless && g_ren && g_tex && !g_fb.empty()) {
        SDL_UpdateTexture(g_tex, NULL, g_fb.data(), g_fbw * 4);
        SDL_RenderClear(g_ren);
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        SDL_RenderPresent(g_ren);
    }
}
void RENDER_SetPal(uint8_t, uint8_t, uint8_t, uint8_t) {}
bool RENDER_IsScalerCompatibleWithDoublescan(void) { return false; }
SDL_Window * GFX_GetSDLWindow(void) { return g_win; }

/* ------------------------------------------------------------------ GameLink RAM (C_GAMELINK=1 이라 memory.cpp 가 호출; 서브셋에 없어 NULL 스텁이 되던 것) */
namespace GameLink {
    uint8_t * AllocRAM(const uint32_t size) { void * p = calloc(1, size); return (uint8_t *)p; }
    void FreeRAM(void * membase) { free(membase); }
}

/* ------------------------------------------------------------------ sdlmain / dos_keyboard_layout 몫 (스텁이면 std::string 반환이 쓰레기라 MSG_Init 이 죽음) */
std::string GetDOSBoxXPath(bool) { return std::string(""); }
uint16_t GetDefaultCP(void) { return 932; }

/* ------------------------------------------------------------------ 디버거 훅 — 진짜 debug.cpp 는 서브셋 밖. 스텁이면 lock inc 로 억 단위 비용(r007j 실측)이라 호스트가 상수로 정의 */
bool DEBUG_HeavyIsBreakpoint(void) { return false; }
bool IsDebuggerActive(void) { return false; }
bool DEBUG_ExitLoop(void) { return false; }
bool DEBUG_IntBreakpoint(uint8_t) { return false; }
bool DOSBox_Paused(void) { return false; }

/* ------------------------------------------------------------------ mapper dummy item */

void MAPPER_AddHandler(MAPPER_Handler *, MapKeys, Bitu, char const * const eventname, char const * const buttonname, DOSBoxMenu::item ** ret_menuitem) {
    if (ret_menuitem) *ret_menuitem = NULL;
    const std::string nm = std::string("mapper_") + eventname;
    DOSBoxMenu::item * it;
    if (mainMenu.item_exists(nm)) it = &mainMenu.get_item(nm);
    else { it = &mainMenu.alloc_item(DOSBoxMenu::item_type_id, nm); it->set_text(buttonname); }
    if (ret_menuitem) *ret_menuitem = it;
}

/* ------------------------------------------------------------------ images + direct boot */
static FILE * gh_open_rw(const std::string & path, uint64_t * size) {
    FILE * f = fopen(path.c_str(), "rb+");
    if (!f) f = fopen(path.c_str(), "rb");
    if (!f) return NULL;
    fseeko64(f, 0, SEEK_END); *size = (uint64_t)ftello64(f); fseeko64(f, 0, SEEK_SET);
    return f;
}
static bool gh_attach(int idx, const char * name) {
    std::string path = g_imgdir + "\\" + name;
    uint64_t sz = 0;
    FILE * f = gh_open_rw(path, &sz);
    if (!f) { gh_log("ERROR: cannot open image %s", path.c_str()); return false; }
    imageDisk * img = new imageDisk(f, name, sz, sz > 2880ull * 1024ull);
    uint32_t h, c, s, ss; img->Get_Geometry(&h, &c, &s, &ss);
    gh_log("image[%d] %s: size=%llu CHS=%u/%u/%u ss=%u", idx, name, (unsigned long long)sz, c, h, s, ss);
    { /* 스테이트 헤더용 디스크 정보: 부팅 직전(수정 전) 내용 CRC32 */
        Pc98StateDisk d; d.name = name; d.size = sz; d.crc32 = 0xFFFFFFFFu;
        static uint32_t tab[256]; static bool ti = false;
        if (!ti) { for (uint32_t i = 0; i < 256; i++) { uint32_t cc = i; for (int k = 0; k < 8; k++) cc = (cc & 1u) ? (0xEDB88320u ^ (cc >> 1)) : (cc >> 1); tab[i] = cc; } ti = true; }
        std::vector<uint8_t> buf(1 << 20);
        fseeko64(f, 0, SEEK_SET);
        for (;;) { size_t n = fread(&buf[0], 1, buf.size(), f); if (!n) break; for (size_t i = 0; i < n; i++) d.crc32 = tab[(d.crc32 ^ buf[i]) & 0xFFu] ^ (d.crc32 >> 8); }
        d.crc32 ^= 0xFFFFFFFFu; fseeko64(f, 0, SEEK_SET);
        g_disks.push_back(d);
        gh_log("image[%d] crc32=%08x", idx, d.crc32);
    }
    imageDiskList[idx] = img; imageDiskChange[idx] = true; img->Addref();
    if (idx <= 1) FDC_AssignINT13Disk((unsigned char)idx);
    else {
        signed char ide_index = -1; bool slave = false;
        IDE_Auto(ide_index, slave);
        IDE_Hard_Disk_Attach(ide_index, slave, (unsigned char)idx);
        updateDPT();
    }
    return true;
}

/* BOOT (PC-98 branch) re-done without the DOS kernel; called from cb_bios_boot__func */
bool VM_Boot_DOSBox_Kernel() {
    int drive = g_prof.bootletter;
    int di = drive - 'A';
    gh_log("direct boot: drive %c", drive);
    if (di < 0 || di >= MAX_DISK_IMAGES || !imageDiskList[di]) { gh_log("ERROR: no image on boot drive"); g_quit = true; return true; }
    bootSector bootarea;
    memset(&bootarea, 0, sizeof(bootarea));
    imageDisk * img = imageDiskList[di];
    unsigned int bootsize = img->getSectSize();
    bool has_read = false, sect128 = false;
    imageDiskChange[di] = false;
    if (drive < 'C') {
        if (img->Read_Sector(0, 0, 1, (uint8_t *)&bootarea, 128) == Int13Status::NoError && img->Read_Sector(0, 0, 2, (uint8_t *)&bootarea + 128, 128) == Int13Status::NoError &&
            img->Read_Sector(0, 0, 3, (uint8_t *)&bootarea + 256, 128) == Int13Status::NoError && img->Read_Sector(0, 0, 4, (uint8_t *)&bootarea + 384, 128) == Int13Status::NoError) {
            gh_log("boot: 128 byte/sector track0"); has_read = true; bootsize = 512; sect128 = true;
        }
        if (!has_read && img->Read_Sector(0, 0, 1, (uint8_t *)&bootarea, 256) == Int13Status::NoError && img->Read_Sector(0, 0, 2, (uint8_t *)&bootarea + 256, 256) == Int13Status::NoError &&
            img->Read_Sector(0, 0, 3, (uint8_t *)&bootarea + 512, 256) == Int13Status::NoError && img->Read_Sector(0, 0, 4, (uint8_t *)&bootarea + 768, 256) == Int13Status::NoError) {
            gh_log("boot: 256 byte/sector track0"); has_read = true; bootsize = 1024; sect128 = true;
        }
    }
    unsigned int max_seg = std::min((unsigned int)(MEM_ConventionalPages() * (4096u / 16u)), 0xC000u);
    unsigned int load_seg = std::min(max_seg, 0x2000u) - (bootsize / 16u);
    Bitu stack_seg = 0x0030;
    if ((stack_seg + 0x20) > max_seg) stack_seg = max_seg - 0x20;
    if (!has_read) {
        if (img->Read_Sector(0, 0, 1, (uint8_t *)&bootarea) != Int13Status::NoError) { gh_log("ERROR: boot sector read failed"); g_quit = true; return true; }
    }
    if (bootarea.rawdata[0] == 0 && bootarea.rawdata[1] == 0) { gh_log("ERROR: boot sector empty"); g_quit = true; return true; }

    if (drive == 'A' || drive == 'B') FDC_AssignINT13Disk((unsigned char)(drive - 'A'));
    GetDMAChannel(2)->tcount = true;
    GetDMAChannel(3)->tcount = true;

    MEM_BlockWrite((PhysPt)(load_seg << 4u), bootarea.rawdata, bootsize);
    gh_log("boot: loaded %u bytes at %04x:0000 (stack_seg=%04x)", bootsize, load_seg, (unsigned)stack_seg);
    DispatchVMEvent(VM_EVENT_GUEST_OS_BOOT);

    SegSet16(cs, (uint16_t)load_seg);
    SegSet16(ds, 0x0000);
    SegSet16(es, (uint16_t)load_seg);
    reg_ip = 0; reg_ebx = 0x200; reg_esp = 0xD8;
    SegSet16(ss, (uint16_t)stack_seg);
    reg_esi = (uint32_t)load_seg; reg_ecx = 0x200; reg_ebp = 0; reg_eax = 0x30; reg_edx = 0x1;

    reg_eax = 0x4200; reg_ecx = 0xC000; CALLBACK_RunRealInt(0x18);   /* 640x400 graphics */
    reg_eax = 0x4100; CALLBACK_RunRealInt(0x18);                    /* hide graphics */
    CPU_Push16((uint16_t)(BIOS_bootfail_code_offset >> 4));
    CPU_Push16(BIOS_bootfail_code_offset & 0xF);
    for (unsigned int i = 0; i < 80 * 25 * 2; i += 2) { mem_writew((PhysPt)(0xA0000 + i), 0x0000); mem_writew((PhysPt)(0xA2000 + i), 0x00E1); }
    PC98_show_cursor(false);

    uint32_t heads, cyls, sects, ssize;
    img->Get_Geometry(&heads, &cyls, &sects, &ssize);
    uint8_t F2HD_MODE = 0; uint16_t disk_equip = 0, disk_equip_144 = 0; uint8_t scsi_equip = 0;
    for (int i = 0; i < 2; i++) if (imageDiskList[i]) { disk_equip |= (0x0111u << i); disk_equip_144 |= (1u << i); F2HD_MODE |= (0x11u << i); }
    for (int i = 0; i < 2; i++) if (imageDiskList[i + 2]) {
        scsi_equip |= (1u << i);
        uint16_t m = 0x460u + ((uint16_t)i * 4u);
        mem_writeb(m + 0u, sects); mem_writeb(m + 1u, heads);
        mem_writew(m + 2u, (cyls & 0xFFFu) + (ssize == 512u ? 0x1000u : 0u) + (ssize == 1024u ? 0x2000u : 0) + 0x8000u);
    }
    mem_writew(0x55C, disk_equip); mem_writew(0x5AE, disk_equip_144); mem_writeb(0x482, scsi_equip);
    mem_writeb(0x488, 0); mem_writeb(0x493, F2HD_MODE); mem_writeb(0x5CA, 0);
    if (drive >= 'C') mem_writeb(0x584, 0xA0 + (drive - 'C'));
    else if ((ssize == 1024 && heads == 2 && cyls == 77 && sects == 8) || sect128) mem_writeb(0x584, 0x90 + (drive - 65));
    else if (ssize == 512 && heads == 2 && cyls == 80 && sects == 18) mem_writeb(0x584, 0x30 + (drive - 65));
    else { gh_log("boot: unknown floppy type, guessing"); mem_writeb(0x584, (ssize < 1024 ? 0x30 : 0x90) + (drive - 65)); }

    bootguest = false;
    bootdrive = drive - 65;
    g_boot_tick = SDL_GetTicks();
    if (!g_boot_tick) g_boot_tick = 1;
    g_pic0 = PIC_Ticks;
    gh_log("boot: CS:IP=%04x:%04x SS:SP=%04x:%04x", SegValue(cs), (unsigned)reg_ip, SegValue(ss), (unsigned)reg_sp);
    return true;
}

/* ------------------------------------------------------------------ events (called every emulated 1 ms tick) */
static void gh_dump_shot(unsigned int ms) {
    char p[1024];
    snprintf(p, sizeof(p), "%s\\shot_%s_%05u.png", g_outdir.c_str(), g_prof.id, ms);
    if (!g_fb.empty()) gh_png(p, g_fb.data(), g_fbw, g_fbh); else gh_log("shot at %u ms: no frame yet", ms);
}
static unsigned int g_next_key = 0, g_key_release_at = 0;
static KBD_KEYS g_key_down = KBD_NONE;
static uint32_t gh_crc(const uint8_t * p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); }
    return c ^ 0xFFFFFFFFu;
}
/* 저장 직전·로드 직후 상태 지문(설계서 §5 자동화): VRAM·GDC·본체 RAM·CPU */
static void gh_state_diag(const char * tag) {
    uint32_t c_vram = vga.mem.linear ? gh_crc(vga.mem.linear, (size_t)vga.mem.memsize) : 0;
    uint32_t c_gdc = gh_crc((const uint8_t *)&pc98_gdc[0], sizeof(pc98_gdc));
    std::vector<uint8_t> ram(0xA0000);
    for (unsigned i = 0; i < 0xA0000; i++) ram[i] = mem_readb((PhysPt)i);
    uint32_t c_ram = gh_crc(&ram[0], ram.size());
    gh_log("STATE-DIAG %s: vram_crc=%08x gdc_crc=%08x ram640_crc=%08x ticks=%u CS:IP=%04x:%04x AX=%04x SP=%04x FLAGS=%04x", tag, c_vram, c_gdc, c_ram, (unsigned)PIC_Ticks, SegValue(cs), (unsigned)reg_ip, (unsigned)reg_ax, (unsigned)reg_sp, (unsigned)reg_flags);
}
static void gh_dump_named(const char * tag, unsigned int ms) {
    char p[1024];
    snprintf(p, sizeof(p), "%s\\%s_%s_%05u.png", g_outdir.c_str(), tag, g_prof.id, ms);
    if (!g_fb.empty()) gh_png(p, g_fb.data(), g_fbw, g_fbh); else gh_log("shot %s: no frame yet", tag);
}
void GFX_Events(void) {
    static unsigned int last_ev = 0;
    unsigned int now = SDL_GetTicks();
    gensei_input_tick();
    if (now != last_ev) {
        last_ev = now;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYDOWN && (e.key.keysym.mod & KMOD_ALT) && !g_headless && g_win) {   /* r010 화면 단축키(Alt+…): PC-98 에는 Alt 가 없어 게임 키와 겹치지 않음 */
                SDL_Keycode k = e.key.keysym.sym; bool hit = true;
                if (k == SDLK_RETURN) { bool fs = (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP; SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); gh_log("SCREEN fullscreen=%d", fs ? 0 : 1); }
                else if (k == SDLK_1) { g_fit = FIT_43; gh_log("SCREEN fit=4:3"); }
                else if (k == SDLK_2) { g_fit = FIT_1610; gh_log("SCREEN fit=16:10"); }
                else if (k == SDLK_3) { g_fit = FIT_INT; gh_log("SCREEN fit=integer"); }
                else if (k == SDLK_s) { g_shader = (g_shader + 1) % SH_N; gh_log("SCREEN shader=%d", (int)g_shader); }
                else if (k == SDLK_UP) { float c = g_curve + 0.01f; g_curve = c > 0.10f ? 0.10f : c; gh_log("SCREEN curve=%.2f", (double)g_curve); }
                else if (k == SDLK_DOWN) { float c = g_curve - 0.01f; g_curve = c < 0.0f ? 0.0f : c; gh_log("SCREEN curve=%.2f", (double)g_curve); }
                else hit = false;
                if (hit) continue;
            }
            if (gensei_input_event(e)) continue;
            if (e.type == SDL_QUIT) { gh_log("SDL_QUIT"); g_quit = true; }
        }
        if (g_boot_tick) {
            unsigned int since = now - g_boot_tick;
            if (g_key_down != KBD_NONE && now >= g_key_release_at) { KEYBOARD_AddKey(g_key_down, false); g_key_down = KBD_NONE; }
            if (g_next_key < g_prof.keys.size() && since >= g_prof.keys[g_next_key].at_ms && g_key_down == KBD_NONE) {
                g_key_down = g_prof.keys[g_next_key].key; g_key_release_at = now + 80;
                KEYBOARD_AddKey(g_key_down, true);
                gh_log("autokey #%u at %u ms", g_next_key, since);
                g_next_key++;
            }
            if (g_save_ms && !g_save_done && since >= g_save_ms) {
                g_save_done = true;
                gh_dump_named("pre_save", since);
                gh_state_diag("before-save");
                std::string err;
                bool ok = PC98_SaveState_ToFile(g_state_path.c_str(), g_disks, err);
                gh_log("SAVE %s -> %s %s", g_state_path.c_str(), ok ? "OK" : "FAIL", err.c_str());
                gh_state_diag("after-save");
                g_ev_tick = now ? now : 1;
            }
            if (g_load_ms && !g_load_done && since >= g_load_ms) {
                g_load_done = true;
                gh_dump_named("pre_load", since);
                gh_state_diag("before-load");
                std::string err;
                bool ok = PC98_SaveState_FromFile(g_state_path.c_str(), g_disks, err);
                gh_log("LOAD %s <- %s %s", g_state_path.c_str(), ok ? "OK" : "FAIL", err.c_str());
                gh_state_diag("after-load");
                g_next_key = (unsigned)g_prof.keys.size();   /* 자동 키 중단: 저장 시점 이후는 상태 그대로 */
                g_pic0 = PIC_Ticks;
                g_ev_tick = now ? now : 1;
            }
            if (g_ev_tick) {
                static const unsigned offs[3] = { 100, 1000, 3000 };
                static unsigned next_post = 0;
                if (next_post < 3 && now - g_ev_tick >= offs[next_post]) { gh_dump_named(g_save_done && !g_load_done ? "post_save" : "post_load", offs[next_post]); next_post++; }
            }
            if (g_glmx_ms && !g_glmx_done && g_use_gl && since >= g_glmx_ms) {   /* r010 시험: 화면 설정 조합별 렌더 결과를 GL 에서 읽어 PNG 로 */
                g_glmx_done = true;
                struct C { int fit, sh; float cv; const char * tag; };
                static const C cs[] = { {FIT_43, SH_SHARP, 0.f, "fit43_sharp"}, {FIT_43, SH_CRT, 0.f, "fit43_crt_flat"}, {FIT_43, SH_CRT, 0.04f, "fit43_crt_curve004"},
                                        {FIT_43, SH_NEAREST, 0.f, "fit43_nearest"}, {FIT_1610, SH_SHARP, 0.f, "fit1610_sharp"}, {FIT_INT, SH_NEAREST, 0.f, "int_nearest"} };
                int w = 0, h = 0; SDL_GL_GetDrawableSize(g_win, &w, &h);
                gh_log("GLMATRIX start drawable=%dx%d frame=%dx%d vsync=%d", w, h, g_fbw, g_fbh, (int)g_vsync);
                for (unsigned i = 0; i < sizeof(cs) / sizeof(cs[0]); i++) {
                    g_fit = cs[i].fit; g_shader = cs[i].sh; g_curve = cs[i].cv; SDL_Delay(250);
                    snprintf(g_shot_path, sizeof(g_shot_path), "%s\\gl_%s_%s.png", g_outdir.c_str(), g_prof.id, cs[i].tag);
                    g_shot_req = 1; for (int t = 0; t < 400 && g_shot_req; t++) SDL_Delay(5);
                    gh_log("GLMATRIX %s done=%d", cs[i].tag, g_shot_req ? 0 : 1);
                }
                g_fit = FIT_43; g_shader = SH_SHARP; g_curve = 0;
            }
            static unsigned int next_shot = 0;
            if (next_shot < g_shots_ms.size() && since >= g_shots_ms[next_shot]) { gh_dump_shot(g_shots_ms[next_shot]); next_shot++; }
            static unsigned int last_stat = 0;
            if (since / 2000 != last_stat) { last_stat = since / 2000; gh_log("stat: wall=%u ms game=%u ms (ratio %.3f) frames=%llu starts=%llu aborts=%llu cycles=%d auto=%d pmode=%d pct=%d CS:IP=%04x:%04x", since, (unsigned)(PIC_Ticks - g_pic0), since ? (double)(PIC_Ticks - g_pic0) / (double)since : 0.0, g_frames, g_starts, g_aborts, (int)CPU_CycleMax, (int)CPU_CycleAutoAdjust, (int)cpu.pmode, (int)CPU_CyclePercUsed, SegValue(cs), (unsigned)reg_ip); }
            if (g_run_ms && since >= g_run_ms) { gh_log("run time reached"); g_quit = true; }
        }
        if (g_quit) throw (int)0;
    }
}

/* ------------------------------------------------------------------ misc */
#if defined(WIN32)
#include <dbghelp.h>
static void gh_dump_stubs(void);
static LONG WINAPI gh_seh(EXCEPTION_POINTERS * ep) {
    static volatile LONG busy = 0;
    if (InterlockedExchange(&busy, 1)) TerminateProcess(GetCurrentProcess(), 0xC5);
    EXCEPTION_RECORD * er = ep->ExceptionRecord;
    gh_log("CRASH code=%08lx addr=%p", (unsigned long)er->ExceptionCode, er->ExceptionAddress);
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        gh_log("  access violation: %s address %p", er->ExceptionInformation[0] == 1 ? "write" : (er->ExceptionInformation[0] == 8 ? "exec" : "read"), (void *)er->ExceptionInformation[1]);
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf; memset(&sf, 0, sizeof(sf));
    sf.AddrPC.Offset = ctx.Rip; sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Rsp; sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 40; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
        if (!sf.AddrPC.Offset) break;
        char sbuf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO * sym = (SYMBOL_INFO *)sbuf; memset(sym, 0, sizeof(SYMBOL_INFO));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
        DWORD64 d64 = 0; DWORD d32 = 0; IMAGEHLP_LINE64 ln; memset(&ln, 0, sizeof(ln)); ln.SizeOfStruct = sizeof(ln);
        bool hs = SymFromAddr(proc, sf.AddrPC.Offset, &d64, sym) != 0;
        bool hl = SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &d32, &ln) != 0;
        gh_log("  #%02d %p %s+0x%llx %s:%u", i, (void *)sf.AddrPC.Offset, hs ? sym->Name : "?", (unsigned long long)d64, hl ? ln.FileName : "", hl ? (unsigned)ln.LineNumber : 0u);
    }
    gh_dump_stubs();
    if (g_log) { fflush(g_log); }
    TerminateProcess(GetCurrentProcess(), 0xC5);
    return EXCEPTION_EXECUTE_HANDLER;
}
static DWORD WINAPI gh_boxkiller(LPVOID) {
    for (;;) {
        Sleep(200);
        HWND h = FindWindowA(NULL, "E_Exit");
        if (h) { if (g_log) { fprintf(g_log, "HOST E_Exit message box closed by watchdog\n"); fflush(g_log); } PostMessage(h, WM_CLOSE, 0, 0); PostMessage(h, WM_COMMAND, IDOK, 0); }
    }
    return 0;
}
#endif

static void gh_dump_stubs(void) {
    FILE * f = NULL;
    std::string p = g_outdir + "\\stub_calls.txt";
    f = fopen(p.c_str(), "w");
    if (!f) return;
    int n = gensei_stub_n, called = 0;
    for (int i = 0; i < n; i++) if (gensei_stub_counts[i]) { fprintf(f, "%u\t%s\n", gensei_stub_counts[i], gensei_stub_names[i]); called++; }
    fprintf(f, "# stubs total=%d called=%d\n", n, called);
    fclose(f);
}

static void Gh_Null_Init(Section *) {}

/* dump + hard exit: the extracted modules are not built to be torn down (skip static/Config destructors) */
static void gh_finish(int rc) {
    gh_log("exit rc=%d frames=%llu", rc, g_frames);
    gh_dump_stubs();
    if (g_log) { fflush(g_log); fclose(g_log); g_log = NULL; }
    fflush(NULL);
    _exit(rc);
}

int main(int argc, char * argv[]) {
    SDL_SetMainReady();
    std::string id;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--imgdir" && i + 1 < argc) g_imgdir = argv[++i];
        else if (a == "--out" && i + 1 < argc) g_outdir = argv[++i];
        else if (a == "--headless") g_headless = true;
        else if (a == "--save" && i + 1 < argc) g_save_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--load" && i + 1 < argc) g_load_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--state" && i + 1 < argc) g_state_path = argv[++i];
        else if (a == "--sec" && i + 1 < argc) g_run_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--shot" && i + 1 < argc) { char * s = argv[++i]; while (*s) { g_shots_ms.push_back((unsigned)(atof(s) * 1000.0)); while (*s && *s != ',') s++; if (*s == ',') s++; } }
        else if (a == "--windowed") g_windowed = true;
        else if (a == "--winsize" && i + 1 < argc) { int w = 0, h = 0; if (sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 64 && h > 64) { g_winw = w; g_winh = h; } }
        else if (a == "--fit" && i + 1 < argc) { std::string v = argv[++i]; g_fit = v == "1610" ? FIT_1610 : (v == "int" ? FIT_INT : FIT_43); }
        else if (a == "--shader" && i + 1 < argc) { std::string v = argv[++i]; g_shader = v == "crt" ? SH_CRT : (v == "nearest" ? SH_NEAREST : SH_SHARP); }
        else if (a == "--curve" && i + 1 < argc) { float c = (float)atof(argv[++i]); g_curve = c < 0 ? 0 : (c > 0.10f ? 0.10f : c); }
        else if (a == "--shaderdir" && i + 1 < argc) g_shaderdir = argv[++i];
        else if (a == "--glmatrix" && i + 1 < argc) g_glmx_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a[0] != '-') id = a;
    }
    {
        std::string lp = g_outdir + "\\gensei_host_log.txt";
        g_log = fopen(lp.c_str(), "w");
    }
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | (g_headless ? 0 : SDL_INIT_VIDEO)) < 0)
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    g_log_t0 = SDL_GetTicks();
    gh_log("gensei_host r010 start, profile='%s' headless=%d stubs=%d", id.c_str(), (int)g_headless, gensei_stub_n);
    if (!gh_profile(id, g_prof)) { fprintf(stderr, "usage: gensei_host <hukyou|kaitou|kitan|torimono> [--imgdir D] [--out D] [--headless] [--sec N] [--shot a,b]\n"); return 1; }
#if defined(WIN32)
    SetUnhandledExceptionFilter(gh_seh);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CreateThread(NULL, 0, gh_boxkiller, NULL, 0, NULL);
#endif
    gensei_input_init();
    gensei_lock = true;
    if (!g_headless) {
        if (g_shaderdir.empty()) {
            char mp[MAX_PATH]; DWORD n = GetModuleFileNameA(NULL, mp, MAX_PATH); std::string s(mp, n);
            size_t sl = s.find_last_of("\\/"); g_shaderdir = (sl == std::string::npos ? std::string(".") : s.substr(0, sl)) + "\\shaders";
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        Uint32 wf = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (g_windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        g_win = SDL_CreateWindow("GENSEI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_winw, g_winh, wf);
        if (g_win) {
            g_glctx = SDL_GL_CreateContext(g_win);
            if (g_glctx) {
                g_fb_mtx = SDL_CreateMutex();
                g_use_gl = gl_init_all();
                if (g_use_gl) {
                    g_vsync = SDL_GL_SetSwapInterval(1) == 0;
                    SDL_GL_MakeCurrent(g_win, NULL);
                    g_glthr = SDL_CreateThread(gl_thread, "gensei_gl", NULL);
                    if (!g_glthr) { gh_log("GL: thread create FAIL"); g_use_gl = false; SDL_GL_MakeCurrent(g_win, g_glctx); }
                }
                if (!g_use_gl) { SDL_GL_DeleteContext(g_glctx); g_glctx = NULL; }
            } else gh_log("GL: context create FAIL: %s", SDL_GetError());
            if (!g_use_gl) {   /* 폴백: 소프트웨어 렌더러(nearest, 비율 유지 논리 크기) */
                SDL_DestroyWindow(g_win);
                g_win = SDL_CreateWindow("GENSEI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_winw, g_winh, SDL_WINDOW_RESIZABLE | (g_windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP));
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                if (g_win) { g_ren = SDL_CreateRenderer(g_win, -1, 0); }
            }
        }
        gh_log("window=%p gl=%d vsync=%d renderer=%p fullscreen=%d fit=%d shader=%d curve=%.3f shaderdir=%s", (void *)g_win, (int)g_use_gl, (int)g_vsync, (void *)g_ren, g_windowed ? 0 : 1, (int)g_fit, (int)g_shader, (double)g_curve, g_shaderdir.c_str());
    }

    try {
        CommandLine com_line(argc, argv);
        Config myconf(&com_line);
        control = &myconf;

        /* the one section that lives in sdlmain.cpp: only what the subset reads */
        { Section_prop * s = control->AddSection_prop("sdl", &Gh_Null_Init); s->Add_string("output", Property::Changeable::Always, "surface"); }
        DOSBOX_SetupConfigSections();
        control->ParseConfigString("gensei:common", g_conf_common);
        gh_log("config parsed");

        DOSBOX_InitTickLoop();
        DOSBOX_RealInit();
        gh_log("RealInit done, machine=%d", (int)machine);

#define GH_INIT(f) do { gh_log("init " #f); f; } while (0)
        { /* sdlmain.cpp 가 만들던 메뉴항목(get_item 으로 찾기만 하는 것)을 미리 할당 — 목록은 빌드 때 소스에서 뽑는다 */
            static const char * const pre_names[] = {
#include "gensei_menu_names.inc"
                0 };
            for (int i = 0; pre_names[i]; i++) if (!mainMenu.item_exists(pre_names[i])) mainMenu.alloc_item(DOSBoxMenu::item_type_id, pre_names[i]);
        }
        GH_INIT(IO_Init());
        GH_INIT(HARDWARE_Init());
        GH_INIT(CPU_PreInit());
        GH_INIT(Init_AddressLimitAndGateMask());
        GH_INIT(Init_MemoryAccessArray());
        GH_INIT(Init_A20_Gate());
        GH_INIT(Init_PS2_Port_92h());
        GH_INIT(Init_RAM());
        GH_INIT(Init_DMA());
        GH_INIT(Init_PIC());
        GH_INIT(TIMER_Init());
        GH_INIT(PCIBUS_Init());
        GH_INIT(PAGING_Init());
        GH_INIT(CMOS_Init());
        GH_INIT(ROMBIOS_Init());
        GH_INIT(CALLBACK_Init());
        GH_INIT(Init_VGABIOS());
        GH_INIT(PROGRAMS_Init());
        GH_INIT(PCSPEAKER_Init());
        GH_INIT(MIXER_Init());
        GH_INIT(CPU_Init());
        GH_INIT(Weitek_Init());
        GH_INIT(FPU_Init());
        GH_INIT(VGA_Init());
        GH_INIT(ISAPNP_Cfg_Init());
        GH_INIT(FDC_Primary_Init());
        GH_INIT(KEYBOARD_Init());
        GH_INIT(PS1SOUND_Init());
        GH_INIT(DISNEY_Init());
        GH_INIT(GUS_Init());
        GH_INIT(IDE_Init());
        GH_INIT(IMFC_Init());
        GH_INIT(INNOVA_Init());
        GH_INIT(WSS_Init());
        GH_INIT(BIOS_Init());
        GH_INIT(INT10_Init());
        GH_INIT(SERIAL_Init());
        GH_INIT(DONGLE_Init());
        GH_INIT(PARALLEL_Init());
        GH_INIT(NE2K_Init());
        GH_INIT(MEM_A20_Enable(true));
        GH_INIT(Init_MemHandles());
        GH_INIT(AllocCallback2());
        GH_INIT(MSG_Init());
        gh_log("devices initialised");

        for (int i = 0; i < 4; i++) if (g_prof.disk[i] && !gh_attach(i, g_prof.disk[i])) { gh_finish(2); }

        if (!VM_PowerOn_gensei()) { gh_log("ERROR: power on failed"); gh_finish(3); }
        gh_log("powered on, running machine");
        try {
            DOSBOX_RunMachine();
        } catch (int x) {
            gh_log("RunMachine exit signal %d", x);
        }
        gh_finish(0);
    } catch (...) {
        gh_log("EXCEPTION escaped main");
        gh_finish(4);
    }
    return 0;
}

bool VM_PowerOn_gensei() {
    if (!guest_machine_power_on) {
        guest_machine_power_on = true;
        DispatchVMEvent(VM_EVENT_POWERON);
        DispatchVMEvent(VM_EVENT_RESET);
        DispatchVMEvent(VM_EVENT_RESET_END);
    }
    return true;
}
