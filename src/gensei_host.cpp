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
#include "mixer.h"
#include "pic.h"
#include <SDL.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <map>
#include <sstream>
#include <typeinfo>
#include <vector>
#include <deque>
#if defined(WIN32)
# include <windows.h>
#endif
#include "gensei_input12.h"
#include "gensei_padnames_t48e.h"   /* t48e (#646): 키 이름 표 확장 */
#include "gensei_ui.h"
#include "settings/gensei_prof.h"
#include "pc98_savestate.h"
#include "pc98_gdc.h"
#include "imagedisk_d88.h"
#include "imagedisk_nfd.h"
#include "gensei_boostaud.h"
#include "settings/gensei_nkdb.h"
#include "settings/gensei_addwork.h"
#include "platform/plat.h" /* S3: platform layer */
#include "gensei_uilist.h"
#include "gensei_embed.h"   /* r019: 글꼴·셰이더 내장본 (빌드 스크립트가 생성) */

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
#ifndef GH_DOSKERNEL
DOS_Block dos;
#endif
bool dos_kernel_disabled = true;
#ifdef GH_DOSKERNEL
extern int bootdrive; extern bool bootguest;   /* t37cd: dos_programs.obj 가 정의 — 한쪽 정의만 (LNK4006 정리) */
#else
int bootdrive = -1;
bool bootguest = false;
#endif
class DOS_Shell;
#ifndef GH_DOSKERNEL
DOS_Shell * first_shell = NULL;
#endif
bool guest_machine_power_on = false;
#ifndef GH_DOSKERNEL
DOS_Drive * Drives[DOS_DRIVES];
#endif
#ifdef GH_DOSKERNEL
/* t36cd: 0 으로 채운 std::string/std::map 은 유효하지 않다 — DOS::DOS 가 prefix_overlay.substr(3) 을 호출하므로 실물로 정의 */
std::string prefix_overlay = ".DBOVERLAY~1";
std::map<int,int> lowboxdrawmap;
std::map<int,int> pc98boxdrawmap;   /* t37cd(#285): 0 으로 채운 std::map 은 유효하지 않다 — 실물 정의 */
std::vector<unsigned char> boot_code_image;
/* 값으로 std::string/vector/map 을 돌려주는 함수는 자동 스텁(rax=0)이면 쓰레기 객체가 나온다 — 실물 정의 */
std::string configfile, savefilename, working_dir;
std::string mapper_event_keybind_string(const std::string &) { return "F11"; }
std::string get_mapper_shortcut(const char *) { return std::string(); }
std::string GetDefaultOutput() { return "surface"; }
std::string LoadGLShader(Section_prop *) { return std::string(); }
std::vector<std::string> MAPPER_GetEventNames(const std::string &) { return std::vector<std::string>(); }
std::map<std::string,std::string> get_event_map() { return std::map<std::string,std::string>(); }
#endif
Render_t render;
ScalerLineHandler_t RENDER_DrawLine = NULL;
uint32_t GFX_Rmask = 0x00FF0000, GFX_Gmask = 0x0000FF00, GFX_Bmask = 0x000000FF, GFX_Amask = 0xFF000000;
unsigned char GFX_Rshift = 16, GFX_Gshift = 8, GFX_Bshift = 0, GFX_bpp = 32;

/* ------------------------------------------------------------------ log */
static FILE * g_log = NULL;
static unsigned long long g_log_bytes = 0;
static unsigned int g_log_t0 = 0;
/* r019: 로그는 UTF-8. 한글 문장(소스는 UTF-8)과 시스템 경로(ACP)가 한 줄에 섞이므로 3바이트 한글은 그대로, 나머지 8비트 구간은 ACP 로 보고 UTF-8 로 바꾼다 */
static std::string gh_mixed_u8(const char * s, int n) {
    std::string o; int i = 0;
    auto cont = [&](int k) { return k < n && (((unsigned char)s[k]) & 0xC0) == 0x80; };
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { o += (char)c; i++; continue; }
        if ((c & 0xF0) == 0xE0 && cont(i + 1) && cont(i + 2)) { o.append(s + i, 3); i += 3; continue; }
        if (c == 0xC2 && i + 1 < n && (unsigned char)s[i + 1] == 0xB7 && (i == 0 || s[i - 1] == ' ') && (i + 2 >= n || s[i + 2] == ' ')) { o.append(s + i, 2); i += 2; continue; }
        int j = i; while (j < n && (unsigned char)s[j] >= 0x80) { if ((((unsigned char)s[j]) & 0xF0) == 0xE0 && cont(j + 1) && cont(j + 2) && j > i) break; j++; }
        { std::string u; if (plat::acp_bytes_to_u8(s + i, j - i, u)) o += u; }
        i = j;
    }
    return o;
}
static void gh_vlog(const char * pfx, const char * fmt, va_list ap) {
    if (!g_log) return;
    if (g_log_bytes > (48ull << 20)) return;   /* cap 48 MB */
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    buf[n] = 0;
    unsigned int t = SDL_GetTicks() - g_log_t0;
    std::string u8 = gh_mixed_u8(buf, n);
    g_log_bytes += fprintf(g_log, "%7u %s%s%s", t, pfx, u8.c_str(), (n > 0 && buf[n - 1] == '\n') ? "" : "\n");
}
static void gh_log(const char * fmt, ...) { va_list ap; va_start(ap, fmt); gh_vlog("HOST ", fmt, ap); va_end(ap); fflush(g_log); }
static std::string g_errmsg;   /* t48a (#614 ④): 사용자 화면에 보일 사람 말 오류 문구(비면 일반 문구) */
static void gh_msgbox(const char * utf8text);
static void gh_human(const std::string & msg) { g_errmsg = msg; gh_log("안내(사용자 화면): %s", msg.c_str()); }

void DEBUG_ShowMsg(char const * format, ...) { va_list ap; va_start(ap, format); gh_vlog("MSG  ", format, ap); va_end(ap); }
void LOG::operator() (char const * format, ...) {
    const char * sv = "";
    switch (d_severity) { case LOG_DEBUG: sv = "D "; break; case LOG_NORMAL: sv = "N "; break; case LOG_WARN: sv = "W "; break; case LOG_ERROR: sv = "E "; break; case LOG_FATAL: sv = "F "; break; default: break; }
    va_list ap; va_start(ap, format); gh_vlog(sv, format, ap); va_end(ap);
}

/* ------------------------------------------------------------------ profile */
struct GKey { unsigned int at_ms; KBD_KEYS key; int step = 0; std::string lab; };   /* t47a: step = 편집 화면 단계 번호(1 부터), lab = OSD 에 보일 이름 */
struct GProfile {
    const char * id = "";
    const char * disk[4] = { NULL, NULL, NULL, NULL };   /* index 0=A 1=B 2=C 3=D (bios_disk index) */
    char bootletter = 'A';       /* 'A'..'D' */
    std::vector<GKey> keys;      /* ms after guest boot */
    std::vector<GKey> bootkeys;  /* t36cd: 부팅 후 키·명령 주입(--autokeys 없이도 동작). bootkeys=3000:type=dir~,9000:enter */
    std::string cd, autoexec;    /* t36cd: CD 슬롯(이미지 경로 또는 이름) · 내장 DOS 의 autoexec 줄(| 로 구분) */
    std::string title, engine, src, reason, imgdir, cycles, font, extra, never_map;
    std::map<std::string, std::string> ov, pad, kv;
    std::vector<std::string> swap; bool install_required = false;
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
    "pc-98 anex86 font=@FONT@\n";


/* ------------------------------------------------------------------ run options */
static std::string g_imgdir = ".", g_outdir = "."; static bool g_imgdir_given = false;
static bool g_headless = false;
static bool g_gltime = false;   /* t42(#313): --gltime — 렌더 스레드 그리기 시간(glFinish 기준) 3초 평균을 로그 한 줄로 */
static bool g_hidewin = false;   /* t41b (#308): 시험 전용 — 보이지 않는 GL 창(SDL_WINDOW_HIDDEN)으로 실행, 슨상 화면에 창이 뜨지 않음 */
static unsigned int g_run_ms = 0;
static std::vector<unsigned int> g_shots_ms;
static bool g_vol_forced = false; extern double gensei_out_sumsq; extern unsigned long long gensei_out_n;
static int g_setvol_b = -1, g_setvol_s = -1; static unsigned int g_rms_from = 0; static bool g_rms_started = false;   /* t35 시험용 */
static GProfile g_prof;
/* r009 세이브스테이트 시험: --save SEC / --load SEC (게임 부팅 후 벽시계 초) --state FILE, 이벤트 뒤 0.1s·1s·3s 캡처 */
static unsigned int g_save_ms = 0, g_load_ms = 0;
static std::string g_state_path;
static std::string g_note; static uint32_t g_note_until = 0;   /* t43h: 메뉴 안내 한 줄(3초) */
static std::string g_savedir = "saves";   /* r011: 슬롯 저장 폴더(저장/<게임>/slot01.g98s) */
static bool g_autokeys = false;           /* r011: 시험용 자동 키 입력(--autokeys). 실기 실행에서는 꺼짐 */
static size_t g_next_bk = 0;              /* t36cd: bootkeys 다음 순번(--autokeys 와 무관하게 항상 동작) */
/* t47a (#559): 자동 입력 전용 시계 g_bk_t — 일시정지(Alt+P)·게임 메뉴가 열린 시간은 흐르지 않는다. 멈춘 적이 없으면 부팅 후 경과(since)와 같다. */
static bool g_bk_osd_on = false;          /* --bkosd 1 (시험 실행 △ 이 붙임): 단계 OSD 줄 표시 */
static bool g_bk_paused = false, g_bk_started = false;
static unsigned g_bk_t = 0, g_bk_last = 0, g_bk_done_at = 0;
static unsigned g_bkp_from = 0, g_bkp_to = 0; static bool g_bkp_a = false, g_bkp_b = false;   /* --bkpause 시작ms,끝ms (시험용) */
static std::string g_bk_osd;              /* 비면 줄 없음. 주 스레드가 쓰고 오버레이 합성(같은 스레드)이 읽는다 */

static bool g_dosfd = false;              /* t43: FD 가 스스로 부팅 못 함(DOS 시스템 없음) → 내장 DOS 에 A:/B: 장착 */
static bool g_dosmode = false;            /* t36cd: CD 슬롯 → 내장 DOS 로 부팅(디스크가 없고 cd= 가 있을 때) */
static std::string g_entry_cli;           /* t36cd: --entry <이름> */
struct GMouse { unsigned at_ms; float dx, dy; int act; };   /* act: 0 이동만, 1 좌 누름, 2 좌 뗌, 3 우 누름, 4 우 뗌 */
static bool g_dumpcode = false;
static bool g_boot400 = false;
static bool g_bda = false;
static bool g_mirq12 = false;   /* --mouseirq12: old r020 (MOUSE_IRQ stayed at the AT default 12) */
static std::string g_flaglog; static unsigned g_keyhold = 80; static bool g_keytrace = false;   /* t29 (#266): --keyhold <ms> 자동키 누름 길이 · --keytrace BDA 키 버퍼(0x502~)·PIC 마스크 로그 */
static bool g_int18trace = false;   /* t31 (#267): --int18trace  INT 18h 콜백을 감싸 AX(AH=기능,AL=그룹)·호출 위치를 2초마다 요약 */
static CallBack_Handler g_orig18 = NULL; static std::map<unsigned, unsigned> g_i18_ax, g_i18_caller; static unsigned g_i18_last = 0, g_i18_n = 0, g_i18_non04 = 0;
static Bitu gh_int18_wrap(void) {
    unsigned ax = (unsigned)reg_ax; g_i18_ax[ax]++; g_i18_n++;
    { PhysPt sp = SegPhys(ss) + (cpu.stack.big ? reg_esp : (reg_sp & 0xffff)); unsigned ip = mem_readw(sp), cs_ = mem_readw(sp + 2); g_i18_caller[(cs_ << 16) | ip]++; }
    if ((ax >> 8) != 0x04 && g_i18_non04 < 60) { g_i18_non04++; gh_log("int18: AX=%04x BX=%04x CX=%04x DX=%04x (AH=%02x 기능, 04 이외)", ax, (unsigned)reg_bx, (unsigned)reg_cx, (unsigned)reg_dx, ax >> 8); }
    unsigned now = SDL_GetTicks();
    if (now - g_i18_last >= 2000) { g_i18_last = now;
        std::vector<std::pair<unsigned, unsigned>> v; for (auto & kv : g_i18_ax) v.push_back({ kv.second, kv.first }); std::sort(v.begin(), v.end(), [](const std::pair<unsigned, unsigned> & a, const std::pair<unsigned, unsigned> & b) { return a.first > b.first; });
        std::string s; for (size_t i = 0; i < v.size() && i < 8; i++) { char b[40]; snprintf(b, sizeof b, " %04x:%u", v[i].second, v[i].first); s += b; }
        std::vector<std::pair<unsigned, unsigned>> c; for (auto & kv : g_i18_caller) c.push_back({ kv.second, kv.first }); std::sort(c.begin(), c.end(), [](const std::pair<unsigned, unsigned> & a, const std::pair<unsigned, unsigned> & b) { return a.first > b.first; });
        std::string t; for (size_t i = 0; i < c.size() && i < 5; i++) { char b[48]; snprintf(b, sizeof b, " %04x:%04x=%u", c[i].second >> 16, c[i].second & 0xffff, c[i].first); t += b; }
        gh_log("int18sum: 누적 %u회 | AX별 상위:%s | 호출 위치 상위:%s", g_i18_n, s.c_str(), t.c_str()); g_i18_ax.clear(); g_i18_caller.clear(); }
    return g_orig18 ? g_orig18() : 0;
}
static void gh_install_int18trace(void) {
    unsigned off = mem_readw(0x18 * 4), seg = mem_readw(0x18 * 4 + 2); PhysPt p = (PhysPt)(seg * 16 + off);
    for (int i = 0; i < 8; i++) if (mem_readb(p + i) == 0xFE && mem_readb(p + i + 1) == 0x38) { unsigned nr = mem_readw(p + i + 2); g_orig18 = CallBack_Handlers[nr]; Bitu n2 = CALLBACK_Allocate(); CallBack_Handlers[n2] = gh_int18_wrap; phys_writew(p + i + 2, (uint16_t)n2); gh_log("int18trace: IVT18=%04x:%04x 콜백 %u -> %u 로 감쌈 (쓰기 뒤 읽어 보기: %u, 핸들러 교체 %s)", seg, off, nr, (unsigned)n2, (unsigned)mem_readw(p + i + 2), CallBack_Handlers[n2] == gh_int18_wrap ? "ok" : "NO"); return; }
    gh_log("int18trace: IVT18=%04x:%04x 에서 FE 38 콜백 명령을 못 찾음", seg, off);
}
static unsigned g_segdump_ms = 0, g_hist_from = 0, g_hist_to = 0;   /* t30 (#267): --segdump <ms> CS/DS/ES/SS 64KB 덤프 · --hist <from>,<to> CS:EIP 표본 히스토그램 */
static bool g_di0 = true;   /* DI=0 (old r020); FFFE hypothesis rejected in t26 */
static bool g_ext14 = false;   /* --ext14: keep r020 behaviour (BDA 0401h = extended memory 14MB at boot) */
static bool g_ext0 = false;
static bool g_trace21 = false; static int g_watch_off = -1;   /* r021 진단: --trace21 (IDT/IVT 핸들러 덤프·PIC 마스크 변화·마우스 레지스터 변화 로그), --watch <16진 오프셋> (DS:off 바이트 변화 로그) */
static void gh_tr21_dump_vectors(const char * tag) {
    gh_log("tr21 vectors [%s] pmode=%d", tag, (int)cpu.pmode);
    for (int v = 0; v < 0x20; v++) {
        unsigned sel = 0, off = 0; PhysPt hp = 0; bool ok = true;
        if (cpu.pmode) {
            PhysPt g = cpu.idt.GetBase() + (PhysPt)v * 8u; unsigned type = (unsigned)mem_readb(g + 5) & 0x1fu;
            off = (unsigned)mem_readw(g); if (type & 8u) off |= ((unsigned)mem_readw(g + 6) << 16);
            sel = (unsigned)mem_readw(g + 2);
            if ((sel & 4u) || (sel & ~7u) == 0) ok = false;
            else { PhysPt d = cpu.gdt.GetBase() + (PhysPt)(sel & ~7u); hp = (PhysPt)(((unsigned)mem_readw(d + 2)) | (((unsigned)mem_readb(d + 4)) << 16) | (((unsigned)mem_readb(d + 7)) << 24)) + off; }
        } else { off = (unsigned)mem_readw((PhysPt)v * 4u); sel = (unsigned)mem_readw((PhysPt)v * 4u + 2u); hp = ((PhysPt)sel << 4) + off; }
        if (!ok) { gh_log("tr21 vec %02x: sel=%04x off=%08x (LDT/널 — 생략)", v, sel, off); continue; }
        char hx[128]; int n = 0; for (int i = 0; i < 24; i++) n += snprintf(hx + n, sizeof hx - n, "%02x ", (unsigned)mem_readb(hp + i));
        char wr[96] = ""; if (g_watch_off >= 0) {   /* 핸들러 앞 192바이트에서 DS:off 에 쓰는 명령(c6/c7/fe/ff 06 lo hi, 88/89 .. 06 lo hi) 찾기 */
            unsigned lo = (unsigned)g_watch_off & 0xff, hi = ((unsigned)g_watch_off >> 8) & 0xff; int m = 0;
            for (int i = 0; i < 192 && m < 3; i++) { unsigned b0 = mem_readb(hp + i), b1 = mem_readb(hp + i + 1), b2 = mem_readb(hp + i + 2), b3 = mem_readb(hp + i + 3);
                if ((b0 == 0xc6 || b0 == 0xc7 || b0 == 0xfe || b0 == 0xff || b0 == 0x88 || b0 == 0x89 || b0 == 0xa2 || b0 == 0xa3) && ((b1 == 0x06 && b2 == lo && b3 == hi) || ((b0 == 0xa2 || b0 == 0xa3) && b1 == lo && b2 == hi)))
                { char t[24]; snprintf(t, sizeof t, " WR@+%d(op%02x)", i, b0); if (strlen(wr) + strlen(t) < sizeof wr) strcat(wr, t); m++; } } }
        gh_log("tr21 vec %02x: %04x:%08x phys=%08x | %s%s", v, sel, off, (unsigned)hp, hx, wr);
    }
}
static std::vector<GMouse> g_amouse; static size_t g_next_mouse = 0; static bool g_mouse_game = false, g_relmouse = false, g_mouse_free = false;   /* r020: 시험용 자동 마우스(--automouse) · 이 작품이 마우스를 쓰는가 · SDL 상대 마우스 모드 상태 */
static bool g_mouseok_cli = false; static int g_ems_req = 1;   /* t38cd: EMS 기본 켬(t37cd 시험에서 마도456 60초 정상) · --noems 로 끄기 */
static bool gh_parse_amouse(const std::string & s) {   /* "ms:dx,dy,act;..." act = - | ld | lu | rd | ru */
    size_t p = 0; while (p < s.size()) { size_t e = s.find(';', p); if (e == std::string::npos) e = s.size(); std::string it = s.substr(p, e - p); p = e + 1; if (it.empty()) continue;
        GMouse m; m.at_ms = (unsigned)atoi(it.c_str()); m.dx = m.dy = 0; m.act = 0; size_t c = it.find(':'); if (c == std::string::npos) continue; std::string r = it.substr(c + 1);
        size_t c1 = r.find(','); if (c1 == std::string::npos) continue; m.dx = (float)atof(r.c_str()); size_t c2 = r.find(',', c1 + 1); m.dy = (float)atof(r.c_str() + c1 + 1);
        if (c2 != std::string::npos) { std::string a = r.substr(c2 + 1); m.act = a == "ld" ? 1 : a == "lu" ? 2 : a == "rd" ? 3 : a == "ru" ? 4 : 0; } g_amouse.push_back(m); }
    return !g_amouse.empty();
}
/* ---- t48e (#648/#651/#652): --inject <파일> — 시험 전용 입력 주입. 슨상의 실제 OS 마우스·키는 건드리지 않는다(SetCursorPos/mouse_event/SendInput 금지):
 *   ① SDL_PushEvent(컨트롤러 버튼·축 · 마우스 이동 xrel/yrel · 키)  ② SDL 가상 조이스틱(SDL_JoystickAttachVirtual)  ③ 플레이어 안 스크립트(uikeys)
 * 파일 한 줄 = "<ms> <명령> [인자...]" ('#' 줄·빈 줄 무시). ms 는 게임 시작(since) 기준.
 *   cbtn <표준버튼이름> <0|1>      SDL_CONTROLLERBUTTON 이벤트를 직접 밀어 넣음 (a b x y back start leftshoulder rightshoulder leftstick dpup dpdown dpleft dpright)
 *   tap <표준버튼이름>             누름 → 120ms 뒤 뗌
 *   caxis <lefty|leftx|triggerright|…> <값>   SDL_CONTROLLERAXISMOTION
 *   vpad <xinput|dinput|unknown> [kind]       가상 조이스틱 붙이기. xinput = Xbox 360 GUID(030000005e0400008e02000000000000)의 DB 매핑 · dinput = 일반 DInput(Logitech Dual Action 030000006d04000016c2000000000000)의 DB 매핑 · unknown = 매핑 없음(폴백)
 *   vstd <표준버튼이름> <0|1>      (vpad 로 붙인 장치) 그 매핑이 가리키는 물리 버튼/십자를 가상 조이스틱에 세팅 — SDL 매핑을 실제로 거친다
 *   vraw <버튼번호> <0|1> · vhat <값>   가상 조이스틱 원시 번호
 *   vdetach                        가상 조이스틱 떼기
 *   kind <0|1|2>                   표시용 패드 종류 강제(0 그 밖 1 Xbox 2 PlayStation)
 *   dbcheck <GUID 32자>            그 GUID 가 gamecontrollerdb 로 매핑이 잡히는지 로그
 *   mm <dx> <dy> · mb <l|r> <0|1>  SDL_MOUSEMOTION(xrel,yrel) / 버튼 이벤트
 *   key <SDL키이름> <0|1>          SDL_KEYDOWN/UP (창 포커스 없이)
 *   log <글>                       로그에 한 줄 */
struct GInj { unsigned at; std::vector<std::string> a; };
static std::vector<GInj> g_inj; static size_t g_next_inj = 0; static std::string g_inject_file;
static SDL_Joystick * g_vj = NULL; static int g_vj_idx = -1; static std::string g_vj_map;
static bool gh_parse_inject(const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        std::string s = ln; while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back(); if (s.empty() || s[0] == '#') continue;
        GInj g; size_t p = 0; while (p < s.size()) { while (p < s.size() && s[p] == ' ') p++; size_t e = s.find(' ', p); if (e == std::string::npos) e = s.size(); if (e > p) g.a.push_back(s.substr(p, e - p)); p = e; }
        if (g.a.size() < 2) continue; g.at = (unsigned)atoi(g.a[0].c_str()); g.a.erase(g.a.begin());
        if (g.a[0] == "tap" && g.a.size() >= 2) { GInj d = g; d.a = { "cbtn", g.a[1], "1" }; GInj u = g; u.at = g.at + 120; u.a = { "cbtn", g.a[1], "0" }; g_inj.push_back(d); g_inj.push_back(u); continue; }
        if (g.a[0] == "vtap" && g.a.size() >= 2) { GInj d = g; d.a = { "vstd", g.a[1], "1" }; GInj u = g; u.at = g.at + 120; u.a = { "vstd", g.a[1], "0" }; g_inj.push_back(d); g_inj.push_back(u); continue; }
        g_inj.push_back(g);
    }
    fclose(f); std::stable_sort(g_inj.begin(), g_inj.end(), [](const GInj & x, const GInj & y) { return x.at < y.at; });
    return !g_inj.empty();
}
static std::string gh_mapfield(const std::string & map, const std::string & nm) {   /* "…,nm:xx,…" 의 xx */
    std::string k = "," + nm + ":"; size_t p = map.find(k); if (p == std::string::npos) return std::string(); p += k.size(); size_t e = map.find(',', p); return map.substr(p, e == std::string::npos ? std::string::npos : e - p);
}
static void gh_inj_run(const GInj & g, unsigned since) {
    const std::vector<std::string> & a = g.a; const std::string & c = a[0];
    gh_log("INJECT t=%u %s%s%s%s%s%s", since, c.c_str(), a.size() > 1 ? " " : "", a.size() > 1 ? a[1].c_str() : "", a.size() > 2 ? " " : "", a.size() > 2 ? a[2].c_str() : "", a.size() > 3 ? " ..." : "");
    if (c == "cbtn" && a.size() >= 3) { SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(a[1].c_str()); if (b == SDL_CONTROLLER_BUTTON_INVALID) { gh_log("INJECT cbtn 알 수 없는 이름 %s", a[1].c_str()); return; }
        bool d = a[2] != "0"; SDL_Event e; SDL_zero(e); e.type = d ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP; e.cbutton.which = g_vj ? SDL_JoystickInstanceID(g_vj) : 0; e.cbutton.button = (Uint8)b; e.cbutton.state = d ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&e); return; }
    if (c == "caxis" && a.size() >= 3) { SDL_GameControllerAxis x = SDL_GameControllerGetAxisFromString(a[1].c_str()); if (x == SDL_CONTROLLER_AXIS_INVALID) return; SDL_Event e; SDL_zero(e); e.type = SDL_CONTROLLERAXISMOTION; e.caxis.axis = (Uint8)x; e.caxis.value = (Sint16)atoi(a[2].c_str()); SDL_PushEvent(&e); return; }
    if (c == "kind" && a.size() >= 2) { gensei_padkind_v = atoi(a[1].c_str()); gh_log("INJECT padkind -> %d", gensei_padkind_v); return; }
    if (c == "log") { std::string t; for (size_t i = 1; i < a.size(); i++) t += (i > 1 ? " " : "") + a[i]; gh_log("INJECT-LOG %s", t.c_str()); return; }
    if (c == "mm" && a.size() >= 3) { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEMOTION; e.motion.xrel = atoi(a[1].c_str()); e.motion.yrel = atoi(a[2].c_str()); SDL_PushEvent(&e); return; }
    if (c == "mb" && a.size() >= 3) { SDL_Event e; SDL_zero(e); bool d = a[2] != "0"; e.type = d ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; e.button.button = a[1] == "r" ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT; e.button.state = d ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&e); return; }
    if (c == "key" && a.size() >= 3) { SDL_Keycode k = SDL_GetKeyFromName(a[1].c_str()); if (k == SDLK_UNKNOWN) return; SDL_Event e; SDL_zero(e); bool d = a[2] != "0"; e.type = d ? SDL_KEYDOWN : SDL_KEYUP; e.key.keysym.sym = k; e.key.keysym.scancode = SDL_GetScancodeFromKey(k); if (a.size() >= 4 && a[3] == "ctrl") e.key.keysym.mod = KMOD_LCTRL; e.key.state = d ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&e); return; }
    if (c == "dbcheck" && a.size() >= 2) { SDL_JoystickGUID gu = SDL_JoystickGetGUIDFromString(a[1].c_str()); char * m = SDL_GameControllerMappingForGUID(gu);
        gh_log("DBCHECK guid=%s found=%d map=%.90s", a[1].c_str(), m ? 1 : 0, m ? m : "-"); GPLOG("DBCHECK guid=%s found=%d", a[1].c_str(), m ? 1 : 0); if (m) SDL_free(m); return; }
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (c == "vpad" && a.size() >= 2) {
        { SDL_version lv; SDL_GetVersion(&lv); gh_log("INJECT SDL linked %d.%d.%d", (int)lv.major, (int)lv.minor, (int)lv.patch); }
        int idx = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 15, 1); if (idx < 0) { gh_log("INJECT vpad attach FAIL: %s", SDL_GetError()); return; }
        g_vj_idx = idx; g_vj = SDL_JoystickOpen(idx); char gs[64] = "?"; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), gs, sizeof gs);
        gh_log("INJECT vpad(%s) attached idx=%d guid=%s open=%d", a[1].c_str(), idx, gs, g_vj ? 1 : 0); g_vj_map.clear();
        if (a[1] != "unknown") {
            const char * dbg = a[1] == "xinput" ? "030000005e0400008e02000000000000" : "030000006d04000016c2000000000000";
            char * m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(dbg));
            gh_log("INJECT DB lookup %s guid=%s found=%d map=%.100s", a[1].c_str(), dbg, m ? 1 : 0, m ? m : "-"); GPLOG("DBCHECK %s guid=%s found=%d", a[1].c_str(), dbg, m ? 1 : 0);
            if (m && strlen(m) > 33) { std::string nm = std::string(gs) + std::string(m + 32); g_vj_map = nm; int r = SDL_GameControllerAddMapping(nm.c_str()); gh_log("INJECT AddMapping(virtual guid + DB mapping) -> %d is_controller=%d", r, (int)SDL_IsGameController(idx)); }
            if (m) SDL_free(m);
            gensei_open_pad(idx);
        } else { gh_log("INJECT unknown guid=%s mapping_for_guid=%d is_controller=%d (0 이어야 폴백)", gs, SDL_GameControllerMappingForGUID(SDL_JoystickGetDeviceGUID(idx)) ? 1 : 0, (int)SDL_IsGameController(idx)); }
        if (a.size() >= 3) gensei_padkind_v = atoi(a[2].c_str());
        return; }
    if (c == "vstd" && a.size() >= 3 && g_vj) {
        std::string f = gh_mapfield(g_vj_map, a[1]); if (f.empty()) { gh_log("INJECT vstd %s: 매핑에 없음", a[1].c_str()); return; } bool d = a[2] != "0";
        if (f[0] == 'b') SDL_JoystickSetVirtualButton(g_vj, atoi(f.c_str() + 1), d ? 1 : 0);
        else if (f[0] == 'h') { int hv = 0; const char * dot = strchr(f.c_str(), '.'); if (dot) hv = atoi(dot + 1); SDL_JoystickSetVirtualHat(g_vj, 0, d ? (Uint8)hv : (Uint8)0); }
        else gh_log("INJECT vstd %s: 지원 안 하는 매핑 %s", a[1].c_str(), f.c_str());
        return; }
    if (c == "vraw" && a.size() >= 3 && g_vj) { SDL_JoystickSetVirtualButton(g_vj, atoi(a[1].c_str()), a[2] != "0" ? 1 : 0); return; }
    if (c == "vhat" && a.size() >= 2 && g_vj) { SDL_JoystickSetVirtualHat(g_vj, 0, (Uint8)atoi(a[1].c_str())); return; }
    if (c == "vaxis" && a.size() >= 3 && g_vj) { SDL_JoystickSetVirtualAxis(g_vj, atoi(a[1].c_str()), (Sint16)atoi(a[2].c_str())); return; }
    if (c == "vdetach") { if (g_vj) { SDL_JoystickClose(g_vj); g_vj = NULL; } if (g_vj_idx >= 0) { SDL_JoystickDetachVirtual(g_vj_idx); g_vj_idx = -1; } return; }
#else
    if (c.compare(0, 1, "v") == 0) { gh_log("INJECT %s: SDL < 2.0.14 — 가상 조이스틱 없음", c.c_str()); return; }
#endif
    gh_log("INJECT 알 수 없는 명령 %s", c.c_str());
}
static std::string g_audiodev;            /* r011: --audiodev 이름(부분 일치) */
static bool g_listaudio = false;
/* r011: 믹서 계수(mixer.cpp 사본 패치에서 정의) */
extern unsigned gensei_mix_cb, gensei_mix_under, gensei_mix_drop;
extern int gensei_audio_obt_freq, gensei_audio_obt_samples;
extern const char * gensei_audio_dev;
static bool g_save_done = false, g_load_done = false;
static unsigned int g_ev_tick = 0;        /* SDL ticks at save/load event */
static std::vector<Pc98StateDisk> g_disks;
static unsigned int g_boot_tick = 0;      /* SDL ticks at guest boot, 0 = not yet */
static bool g_quit = false;
static bool g_winsize_user = false;
static std::string g_uitest_dir; static std::string g_logoshot;   /* t52d: --logoshot <png> 로고 한 장만 그리고 끝 */
static unsigned g_swap_ms = 0; static int g_swap_drv = 0, g_swap_idx = 0; static bool g_swap_done = false, g_axistest = false;   /* 시험: --swaptest 초,드라이브,번호 / --axistest */
static unsigned g_bt_at = 0, g_bt_len = 0; static int g_bt_x = 0; static unsigned g_caps_at = 0, g_caps_len = 0;

/* ------------------------------------------------------------------ r017 공유 상태 */
extern int gensei_nosplash;                                   /* bios.cpp 사본: 1 = DOSBox-X BIOS 스플래시 생략 */
extern int gensei_ba_mode, gensei_ba_xq, gensei_ba_gain;      /* mixer.cpp 사본: 배속 소리 (모드 · 배율x100 · 이득/256) */
extern unsigned gensei_ba_calls, gensei_ba_under, gensei_ba_drops; extern double gensei_ba_rho, gensei_ba_sumsq; extern unsigned long long gensei_ba_n;
static bool g_loading = false, g_noload = false;              /* 「Now Loading...」 덮개: 켠 때부터 첫 게임 화면까지 */
static unsigned g_pon_tick = 0; static Bitu g_pon_pic = 0;    /* 전원 투입 시각(SDL ms · PIC ms) */
static unsigned g_dbx_raw_frames = 0, g_frames_seen = 0, g_load_end_wall = 0, g_load_end_game = 0;
static std::string g_ft_dir; static unsigned g_ft_ms = 2000;  /* --frametest DIR : 처음 2초 프레임별 기록 */
static bool g_ft_done = false;
static int g_bsound_override = -1;                            /* --bsound N 시험용 */
static std::string g_nkdb_path, g_gamefolder_acp;             /* NK DB(gamedb_nk.tsv) · 게임 폴더(시스템 문자열) */
static nk::Db g_nk; static aw::ShaCache g_shacache;
static std::string g_addwork_cli, g_gamefolder_cli, g_report_dir; static bool g_scanonly = false;

/* ------------------------------------------------------------------ r012: 공유 상태(OSD·UI·배속) */
extern uint32_t emulator_speed;       /* dosbox.cpp: 시계 배율(%) */
void MENU_mute(bool enabled);         /* mixer.cpp(믹서 사본) */
static ui::Ui g_ui;
static int g_cur_game = 0;            /* g_ui.games 인덱스 */
static std::string g_osd; static unsigned int g_osd_until = 0;
static void gh_osd(const std::string & s, unsigned int ms = 1500) { g_osd = s; g_osd_until = SDL_GetTicks() + ms; }
static bool g_builtin_res = false;   /* r019: 셰이더를 내장본으로 대신했는가 */
static bool g_builtin_res_font = false;
/* r019 (#246-3): 외부 글꼴·셰이더가 없어 내장본을 쓰는 중이면 화면 맨 아래에 경고 한 줄 */
static std::string gh_builtin_warn(void) {
    if (g_builtin_res_font && g_builtin_res) return phr_("글꼴·셰이더 파일을 찾지 못해 내장본으로 실행 중 (run\\res 폴더 확인)");
    if (g_builtin_res_font) return phr_("글꼴 파일을 찾지 못해 내장 글꼴로 실행 중 (run\\res\\fonts 에 kyuhachi_font*.fnt)");
    if (g_builtin_res) return phr_("셰이더 파일을 찾지 못해 내장 셰이더로 실행 중 (run\\res\\shaders 폴더)");
    return std::string();
}
/* r019: UI 글꼴 — 외부 파일(exe 옆) 우선, 없으면 내장 글꼴 (빈 화면 금지) */
static bool gh_font_init(const std::string & exedir) {
    bool fl_ = ov::font_load(exedir); gh_log("T52H 일본어 글꼴: 동봉 FONTX2 = %s", ov::g_jp_fx.ok ? ov::g_jp_src.c_str() : "없음");   /* t52h (#134) */
    if (fl_) { gh_log("T52G UI 글꼴 .fnt 읽음: %s", ov::g_font_src.c_str()); return true; }
    gh_log("WARN: UI 글꼴 파일을 %s 에서 찾지 못함 -> 내장 글꼴 사용", exedir.c_str());
    ov::font_load_mem(emb_font_sb, emb_font_sb_n, emb_font_db, emb_font_db_n); g_builtin_res_font = true;
    gh_osd(phr_("글꼴 파일이 없어 내장 글꼴로 실행합니다 (run\\res\\fonts · run\\res\\shaders 폴더를 확인하세요)"), 12000); return ov::g_font_ok;
}
static bool g_boost_on = false, g_bosd_on = true; static int g_boost_xv = 2;   /* 배속 상태 · 「배속 표시」 설정 · 표시 배율 */
static unsigned g_play_ms = 0, g_play_last = 0;   /* 플레이 시간(메뉴 시간 제외) */
static void gh_boost_osd_draw(ov::Canvas & oc, const std::string & other_osd, bool other_on) {   /* 오른쪽 위: 배속 중 계속 표시 + 그 밑에 다른 안내 */
    int y = 8; if (g_boost_on && g_bosd_on) { ov::osd_draw(oc, "▶▶ x" + std::to_string(g_boost_xv), true, 8); y = 38; }
    if (other_on) { ov::osd_draw(oc, other_osd, true, y); y += 30; } if (!g_bk_osd.empty()) ov::osd_draw(oc, g_bk_osd, true, y);   /* t47a: 자동 입력 단계 줄 */
}
/* ---- t46x: 화면 키보드 오버레이 (경로 B) — 키 표(플랫폼 무관) + 상태·그리기. SDL 입력층은 gh_kbd_event 한 함수(아래 글루). */
#include "gensei_kbtable.h"
#include "gensei_kbov.h"
static volatile float g_scanv = 7.0f, g_maskv = 0.15f, g_bleedv = 0.0f, g_glowv = 0.0f, g_nearv = 0.0f;   /* t48c: 빛 번짐 · 밑 그림 nearest(1) */   /* crt 셰이더 uniform(렌더 스레드가 읽음) */
static std::string g_exedir, g_cfgpath;
/* ------------------------------------------------------------------ r020 (#247정정·#248): 공용 자원 run\res\fonts · run\res\rhythm */
static bool gh_exists(const std::string & p) { return plat::exists(p.c_str()); }
static std::string gh_res_font_path(const std::string & name) {   /* 글꼴 이름 → 파일 (res\fonts 우선, 옛 exe 옆도 허용). 없으면 res 쪽 경로를 돌려준다 */
    std::string a = g_exedir + "\\res\\fonts\\" + name + ".bmp", b = g_exedir + "\\" + name + ".bmp";
    if (gh_exists(a)) return a; if (gh_exists(b)) return b; return a;
}
/* t52b (#127): anex86 형식 글꼴 BMP(524,350 B)가 하나도 없으면 PC-98 글꼴 로딩 뒤 널 읽기(c0000005)로 죽었다.
 * → 작품을 시작하기 전에 막고 안내한다. 일부만 있으면 있는 것으로 대신한다(글자 모양은 달라도 죽지 않음). */
static bool gh_font_bmp_ok(const std::string & p) { bool ok = false; if (!plat::find_a(p, [&](const plat::DirEnt & e) { ok = !e.is_dir && e.size_hi == 0 && e.size_lo == 524350u; return false; })) return false; return ok; }
static bool gh_any_font_bmp(std::string * which) {
    std::string dir = g_exedir + "\\res\\fonts\\"; bool found = false;
    plat::find_a(dir + "*.bmp", [&](const plat::DirEnt & e) { if (!e.is_dir && e.size_hi == 0 && e.size_lo == 524350u) { if (which) *which = dir + e.name; found = true; return false; } return true; });
    if (found) return true;
    { static const char * const nm[2] = { "\\kyuhachi_font.bmp", "\\pc98player_font.bmp" };   /* t52g: 새 이름 먼저, 옛 이름도 읽는다 */
      for (int q = 0; q < 2; q++) { std::string b = g_exedir + nm[q]; if (gh_font_bmp_ok(b)) { if (which) *which = b; return true; } } }
    return false;
}
static std::string gh_default_font_path(void) { std::string p = gh_res_font_path("kyuhachi_font"); if (gh_font_bmp_ok(p)) return p; std::string p0 = gh_res_font_path("pc98player_font"); if (gh_font_bmp_ok(p0)) return p0; std::string f; if (gh_any_font_bmp(&f)) return f; return p; }   /* t52g: 새 이름 → 옛 이름 → 아무 글꼴 */
static bool gh_font_guard(void) { if (gh_any_font_bmp(NULL)) return true; g_note = phr_("res/fonts 에 anex86 형식 글꼴 BMP 를 넣어 주세요"); g_note_until = SDL_GetTicks() + 5000u; return false; }
static std::string gh_sha1_buf(const std::vector<uint8_t> & b) { prof::Sha1 s; if (!b.empty()) s.add(&b[0], b.size()); return s.hex(); }
static bool gh_read_all(const std::string & p, std::vector<uint8_t> & out) {
    FILE * f = fopen(p.c_str(), "rb"); if (!f) return false; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); if (n < 0) { fclose(f); return false; }
    out.resize((size_t)n); size_t r = n ? fread(&out[0], 1, (size_t)n, f) : 0; fclose(f); return r == (size_t)n;
}
static bool gh_write_all(const std::string & p, const std::vector<uint8_t> & b) { FILE * f = fopen(p.c_str(), "wb"); if (!f) return false; if (!b.empty()) fwrite(&b[0], 1, b.size(), f); fclose(f); return true; }
/* res\fonts\fonts.ini: 이름=sha1 (한 줄에 하나, ; 주석) — 가져올 때 해시가 맞으면 그 이름으로 등록 */
static std::string gh_font_name_for(const std::string & sha) {
    std::vector<uint8_t> b; if (!gh_read_all(g_exedir + "\\res\\fonts\\fonts.ini", b)) return std::string();
    std::string s((const char *)(b.empty() ? (const uint8_t *)"" : &b[0]), b.size()); size_t p = 0;
    while (p < s.size()) { size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size(); std::string ln = s.substr(p, e - p); p = e + 1;
        while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ')) ln.pop_back(); if (ln.empty() || ln[0] == ';' || ln[0] == '#') continue;
        size_t q = ln.find('='); if (q == std::string::npos) continue; if (aw::lower(ln.substr(q + 1)) == sha) return ln.substr(0, q); }
    return std::string();
}
/* 작품 추가 때 한 번: 게임 폴더(또는 그 안 zip)에 font.bmp·anex86.bmp·2608_*.wav 가 보이면 해시를 확인해 res\ 로 복사. 같은 해시가 이미 있으면 건너뜀. 원본은 건드리지 않는다 */
static void gh_res_put(const std::string & from, const std::string & bn, const std::vector<uint8_t> & buf) {
    std::string lb = aw::lower(bn), root = g_exedir + "\\res";
    if (lb.size() > 4 && lb.compare(lb.size() - 4, 4, ".wav") == 0) {
        if (lb.compare(0, 5, "2608_") != 0) return;
        aw::make_dir(root); std::string rd = root + "\\rhythm"; aw::make_dir(rd); std::string dst = rd + "\\" + lb; std::vector<uint8_t> old;
        if (gh_read_all(dst, old) && gh_sha1_buf(old) == gh_sha1_buf(buf)) { gh_log("자원 가져오기: 리듬 %s — 같은 해시가 이미 있음, 건너뜀", lb.c_str()); return; }
        if (gh_exists(dst)) { gh_log("자원 가져오기: 리듬 %s — 다른 파일이 이미 있어 덮어쓰지 않음 (%s)", lb.c_str(), from.c_str()); return; }
        if (gh_write_all(dst, buf)) gh_log("자원 가져오기: 리듬 %s → res\\rhythm\\%s (출처 %s)", lb.c_str(), lb.c_str(), from.c_str());
        return;
    }
    if (lb == "font.bmp" || lb == "anex86.bmp") {
        std::string sha = gh_sha1_buf(buf); aw::make_dir(root); std::string fd = root + "\\fonts"; aw::make_dir(fd);
        std::vector<std::string> fl; aw::list_dir(fd, fl, NULL);
        for (auto & f : fl) { std::string l = aw::lower(f); if (l.size() < 5 || l.compare(l.size() - 4, 4, ".bmp") != 0) continue; std::vector<uint8_t> o; if (gh_read_all(fd + "\\" + f, o) && o.size() == buf.size() && gh_sha1_buf(o) == sha) { gh_log("자원 가져오기: 글꼴 %s — 같은 해시가 이미 있음(%s), 건너뜀", lb.c_str(), f.c_str()); return; } }
        std::string nm = gh_font_name_for(sha); if (nm.empty()) nm = "imp_" + sha.substr(0, 8);
        std::string dst = fd + "\\" + nm + ".bmp"; if (gh_exists(dst)) { gh_log("자원 가져오기: 글꼴 %s — 이름 %s 이(가) 이미 다른 파일로 있어 건너뜀", lb.c_str(), nm.c_str()); return; }
        if (gh_write_all(dst, buf)) gh_log("자원 가져오기: 글꼴 %s → res\\fonts\\%s.bmp (sha1 %s, 출처 %s)", lb.c_str(), nm.c_str(), sha.c_str(), from.c_str());
    }
}
static void gh_res_import_dir(const std::string & dir) {
    static std::set<std::string> done; if (dir.empty() || !done.insert(aw::lower(dir)).second || !aw::dir_exists(dir)) return;
    std::vector<std::string> fl; aw::list_dir(dir, fl, NULL);
    for (auto & f : fl) {
        std::string l = aw::lower(f);
        if (l == "font.bmp" || l == "anex86.bmp" || (l.size() > 9 && l.compare(0, 5, "2608_") == 0 && l.compare(l.size() - 4, 4, ".wav") == 0)) { std::vector<uint8_t> b; if (gh_read_all(aw::join(dir, f), b)) gh_res_put(aw::join(dir, f), f, b); continue; }
        if (!aw::is_zip(f)) continue; std::string zp_ = aw::join(dir, f), err; zp::Zip z; if (!zp::open(zp_, z, &err)) continue;
        for (auto & e : z.e) { if (e.isdir || e.usize == 0 || e.usize > 8u * 1024 * 1024) continue; std::string bn = aw::base_of(e.name), lb = aw::lower(bn);
            bool want = lb == "font.bmp" || lb == "anex86.bmp" || (lb.size() > 9 && lb.compare(0, 5, "2608_") == 0 && lb.compare(lb.size() - 4, 4, ".wav") == 0); if (!want) continue;
            std::vector<uint8_t> b; std::string er; if (zp::extract(z, e, b, er)) gh_res_put(zp_ + "!" + e.name, bn, b); }
    }
}
static void gh_res_on_work(const std::string & dir) { gh_res_import_dir(dir); }
/* ------------------------------------------------------------------ 작품 프로필 (#230/#237): game.ini > 게임 DB(sha1) > 자동(원판 파일 구조) */
static std::string g_pst_id; static std::deque<std::string> g_pst_pool;   /* 프로필마다 문자열을 새로 보관(포인터가 다음 호출에 덮이지 않게) */
static std::vector<prof::DbGame> g_db; static bool g_db_loaded = false;
static KBD_KEYS gh_keyname(std::string n) {
    for (auto & c : n) c = (char)tolower((unsigned char)c);
    if (n.size() == 1) { if (n[0] >= 'a' && n[0] <= 'z') { static const KBD_KEYS L[26] = { KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g, KBD_h, KBD_i, KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o, KBD_p, KBD_q, KBD_r, KBD_s, KBD_t, KBD_u, KBD_v, KBD_w, KBD_x, KBD_y, KBD_z }; return L[n[0] - 'a']; }
        if (n[0] >= '0' && n[0] <= '9') { static const KBD_KEYS N[10] = { KBD_0, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7, KBD_8, KBD_9 }; return N[n[0] - '0']; } }
    if (n == "enter" || n == "return") return KBD_enter; if (n == "space") return KBD_space; if (n == "esc" || n == "escape") return KBD_esc; if (n == "tab") return KBD_tab;
    if (n == "bs" || n == "backspace") return KBD_backspace; if (n == "del" || n == "delete") return KBD_delete; if (n == "up") return KBD_up; if (n == "down") return KBD_down;
    if (n == "left") return KBD_left; if (n == "right") return KBD_right; if (n == "caps" || n == "capslock") return KBD_capslock; if (n == "ctrl") return KBD_leftctrl;
    if (n == "shift") return KBD_leftshift; if (n == "kana") return KBD_kana; if (n == "pageup" || n == "rollup") return KBD_pageup; if (n == "pagedown" || n == "rolldown") return KBD_pagedown;
    if (n.size() == 2 && n[0] == 'f' && n[1] >= '1' && n[1] <= '9') { static const KBD_KEYS F[9] = { KBD_f1, KBD_f2, KBD_f3, KBD_f4, KBD_f5, KBD_f6, KBD_f7, KBD_f8, KBD_f9 }; return F[n[1] - '1']; }
    if (n == "f10") return KBD_f10;
    return KBD_NONE;
}
static bool gh_parse_keys(const std::string & s, std::vector<GKey> & out) {
    for (auto & it : prof::split_list(s)) { size_t c = it.find(':'); if (c == std::string::npos) continue; KBD_KEYS k = gh_keyname(it.substr(c + 1)); if (k == KBD_NONE) continue; GKey g; g.at_ms = (unsigned)atoi(it.substr(0, c).c_str()); g.key = k; out.push_back(g); }
    return !out.empty();
}
/* t36cd: bootkeys=<ms>:<키이름>,<ms>:type=<글자들> — type= 은 글자마다 220ms 간격으로 눌러 쓴다. ~ = Enter, 지원 글자: a-z 0-9 공백 . - / \ (대문자는 소문자로) */
static KBD_KEYS gh_bk_char(char ch, bool & ok) {
    ok = true; char c = (char)tolower((unsigned char)ch);
    if (c >= 'a' && c <= 'z') return gh_keyname(std::string(1, c)); if (c >= '0' && c <= '9') return gh_keyname(std::string(1, c));
    if (c == ' ') return KBD_space; if (c == '~') return KBD_enter; if (c == '.') return KBD_period; if (c == '-') return KBD_minus; if (c == '/') return KBD_slash; if (c == '\\') return KBD_backslash;
    ok = false; return KBD_NONE;
}
static bool gh_parse_bootkeys(const std::string & s, std::vector<GKey> & out) { int stepn = 0; for (auto & it : prof::split_list(s)) { ++stepn;
        size_t c = it.find(':'); if (c == std::string::npos) continue; unsigned at = (unsigned)atoi(it.substr(0, c).c_str()); std::string rest = it.substr(c + 1);
        if (rest.compare(0, 5, "type=") == 0) { std::string tx = rest.substr(5); unsigned t = at; for (char ch : tx) { bool ok; KBD_KEYS k = gh_bk_char(ch, ok); if (!ok) continue; GKey g; g.at_ms = t; g.key = k; g.step = stepn; g.lab = tx; out.push_back(g); t += 220; } continue; }
        KBD_KEYS k = gh_keyname(rest); if (k == KBD_NONE) continue; GKey g; g.at_ms = at; g.key = k; g.step = stepn; g.lab = rest; out.push_back(g);
    }
    std::stable_sort(out.begin(), out.end(), [](const GKey & a, const GKey & b) { return a.at_ms < b.at_ms; });
    return !out.empty();
}
static void gh_list_dir(const std::string & dir, std::vector<std::string> & files, std::vector<std::string> * subdirs) {
    aw::list_dir(dir, files, subdirs);   /* r018: 와이드(UTF-16) API 로 열거 — 한글 폴더·파일 이름 */
}
static void gh_nk_load_once(void);
/* t43c (#330 C1): 설치 대상 호스트 폴더 훑기 — 깊이 2 까지 EXE/COM/BAT 중 설치기·압축 도구 이름을 뺀 것. 반환 = 개수, run/dirp = 하나 고를 때의 파일·폴더(루트면 빈칸) */
static int gh_scan_hdd(const std::string & root, std::string & run, std::string & dirp) {
    struct F { std::string d, n; uint64_t sz; }; std::vector<F> v;
    std::function<void(const std::string &, const std::string &, int)> rec = [&](const std::string & ph, const std::string & rel, int dep) {
        plat::find_a(ph + "\\*", [&](const plat::DirEnt & fd) -> bool {
        std::string n = fd.name; if (n == "." || n == "..") return true;
            if (fd.is_dir) { if (dep < 2) rec(ph + "\\" + n, rel.empty() ? n : rel + "\\" + n, dep + 1); return true; }
            std::string u; for (char c : n) u += (char)toupper((unsigned char)c); size_t d = u.find_last_of('.'); if (d == std::string::npos) return true; std::string x = u.substr(d + 1), st = u.substr(0, d);
            if (x != "EXE" && x != "COM" && x != "BAT") return true; if (prof::inst_rank(n) || prof::is_tool_stem(st) || st == "COMMAND" || st == "LHA" || st == "LHARC" || st == "UNLHA" || st == "LHX" || st == "DELETE" || st == "UNINST") return true;
            v.push_back({ rel, u, ((uint64_t)fd.size_hi << 32) | fd.size_lo }); return true; }); };
    rec(root, "", 0);
    if (v.size() > 1) { std::vector<F> b; for (auto & f : v) if (f.n.size() > 4 && f.n.substr(f.n.size() - 4) == ".BAT") b.push_back(f); if (b.size() == 1) { run = b[0].n; dirp = b[0].d; return 1; } }
    if (v.size() > 1) { std::vector<prof::RunEnt> ents; for (auto & f : v) if (f.d.empty()) { prof::RunEnt e; e.name = f.n; e.size = (uint32_t)f.sz; ents.push_back(e); }   /* t43r: 풀린(설치된) 폴더 루트에 실행 파일이 여럿이면 선택 규칙(dos_pick)으로 하나 고른다 */
        if (ents.size() > 1) { std::string pk = prof::dos_pick(ents, ""); if (!pk.empty()) { run = pk; dirp = ""; return 1; } } }
    if (v.size() >= 1) { run = v[0].n; dirp = v[0].d; } return (int)v.size();
}
static void gh_db_load_once(void) {
    if (g_db_loaded) return; g_db_loaded = true; gh_nk_load_once();
    if (prof::db_load(g_exedir + "\\gamedb.ini", g_db)) gh_log("gamedb.ini: %d games", (int)g_db.size()); else gh_log("gamedb.ini: none");
}
/* id = 작품 폴더 이름(games\<id>) 또는 폴더 경로. 채워지는 것: p(디스크·부팅·자동키·제목·cycles·font·extra·덮어쓰기·패드), g_imgdir */
/* r019 (#247-6): 작품 폴더의 보조 zip(RetroArch.zip 등) 안에서 글꼴·사운드 롬·리듬 WAV 를 찾아 games\_zip\_aux\<키>\ 에 풀어 둔다.
   want = 찾을 파일 이름(대소문자 무시) · 빈칸이면 font.bmp/sound.rom/bios.rom/*.wav 전부. 돌려주는 값 = want 가 들어 있던 풀린 폴더(없으면 빈칸) · got 에 푼 파일 목록 */
static std::string gh_aux_extract(const std::vector<std::string> & dirs, const std::string & want, std::vector<std::string> * got) {
    std::string wl = aw::lower(aw::base_of(want)), found; std::set<std::string> seen;
    auto wanted = [&](const std::string & bn) { std::string b = aw::lower(bn); if (!wl.empty()) return b == wl; size_t d = b.find_last_of('.'); std::string x = d == std::string::npos ? "" : b.substr(d); return b == "font.bmp" || b == "sound.rom" || b == "bios.rom" || x == ".wav"; };
    for (auto & dir : dirs) {
        if (dir.empty() || !seen.insert(aw::lower(dir)).second || !aw::dir_exists(dir)) continue;
        std::vector<std::string> fl; aw::list_dir(dir, fl, NULL);
        for (auto & f : fl) {
            if (!aw::is_zip(f)) continue; std::string zp_ = aw::join(dir, f), err; zp::Zip z; if (!zp::open(zp_, z, &err)) continue;
            uint64_t zs = 0, zm = 0; aw::file_stat(zp_, zs, zm); std::string out = aw::join(aw::join(aw::join(g_exedir, "games"), "_zip"), "_aux"); aw::make_dir(aw::join(g_exedir, "games")); aw::make_dir(aw::join(aw::join(g_exedir, "games"), "_zip")); aw::make_dir(out);
            out = aw::join(out, aw::hex64(aw::fnv(aw::lower(zp_) + ":" + std::to_string(zs))).substr(0, 12)); bool made = false;
            for (auto & e : z.e) {
                if (e.isdir || !wanted(aw::base_of(e.name))) continue;
                if (!made) { aw::make_dir(out); made = true; }
                std::string bn = aw::sys_from_ini(aw::base_of(e.name)), dst = aw::join(out, bn); uint64_t sz = 0, mt = 0;
                if (!(aw::file_stat(dst, sz, mt) && sz == e.usize)) { std::vector<uint8_t> buf; std::string er; if (!zp::extract(z, e, buf, er)) { gh_log("WARN: 보조 zip %s: %s: %s", zp_.c_str(), e.name.c_str(), er.c_str()); continue; }
                    FILE * fo = fopen(dst.c_str(), "wb"); if (!fo) continue; if (!buf.empty()) fwrite(buf.data(), 1, buf.size(), fo); fclose(fo); }
                if (got) got->push_back(dst); if (!wl.empty() && aw::lower(bn) == wl && found.empty()) found = out;
            }
            if (!found.empty()) return found;
        }
    }
    return found;
}
static bool gh_profile(const std::string & id, GProfile & p, bool quick = false) {
    p = GProfile();
    gh_db_load_once();
    bool path = id.find('\\') != std::string::npos || id.find('/') != std::string::npos || id.find(':') != std::string::npos;
    std::string dir = path ? id : g_exedir + "\\games\\" + id;
    std::vector<std::string> files, subs; gh_list_dir(dir, files, &subs);
    prof::Ini gi; bool have_ini = prof::ini_load(dir + "\\game.ini", gi) && gi.count("game");
    std::string imgdir = dir;
    if (have_ini && gi["game"].count("imgdir")) { std::string r = aw::sys_from_ini(gi["game"]["imgdir"]); imgdir =(r.size() > 1 && r[1] == ':') ? r : g_exedir + "\\" + r; files.clear(); gh_list_dir(imgdir, files, NULL); }
    if (!have_ini && files.empty()) { gh_log("profile '%s': 폴더 없음/비어 있음 (%s)", id.c_str(), dir.c_str()); return false; }
    std::vector<prof::ImgInfo> imgs;
    for (auto & f : files) {
        std::string lf = f; for (auto & c : lf) c = (char)tolower((unsigned char)c);
        if (lf == "game.ini" || (lf.size() > 4 && (lf.substr(lf.size() - 4) == ".bmp" || lf.substr(lf.size() - 4) == ".txt" || lf.substr(lf.size() - 4) == ".png"))) continue;
        prof::ImgInfo i = prof::probe(imgdir + "\\" + f);
        if (i.fmt == prof::F_UNKNOWN || i.fmt == prof::F_NOTIMG) continue;
        if (!quick && i.size < (96ull << 20)) i.sha1 = g_shacache.sha1(i.path);
        prof::probe_boot(i); imgs.push_back(i);
    }
    prof::Profile q = prof::resolve(dir, imgs, g_db, id);
    for (auto & dk : q.disk) dk = aw::sys_from_ini(dk);          /* game.ini 는 UTF-8 — 시스템 문자열로 */
    for (auto & sw : q.swap) sw = aw::sys_from_ini(sw);
    if (q.kv.count("srcdir")) q.kv["srcdir"] = aw::sys_from_ini(q.kv["srcdir"]);
    if (!q.dbhit_title.empty() && !q.kv.count("dbhit")) q.kv["dbhit"] = q.dbhit_title;
    g_pst_id = path ? id.substr(id.find_last_of("\\/") + 1) : id; if (!q.id.empty() && have_ini) g_pst_id = q.id;
    if (!q.id.empty() && !have_ini && q.src == "DB") g_pst_id = path ? g_pst_id : id;   /* 슬롯·로그 폴더 이름은 폴더 이름을 유지(저장 호환) */
    g_pst_pool.push_back(g_pst_id); p.id = g_pst_pool.back().c_str(); p.title = q.title.empty() ? g_pst_id : q.title; p.engine = q.engine; p.src = q.src; p.reason = q.reason;
    for (int i = 0; i < 4; i++) { if (q.disk[i].empty()) { p.disk[i] = NULL; } else { g_pst_pool.push_back(q.disk[i]); p.disk[i] = g_pst_pool.back().c_str(); } }
    p.bootletter = q.bootletter; p.swap = q.swap; p.install_required = q.install_required; p.imgdir = imgdir;
    p.ov = q.ov; p.pad = q.pad; p.kv = q.kv;
    if (q.kv.count("cycles")) p.cycles = q.kv["cycles"]; if (q.kv.count("font")) p.font = q.kv["font"]; if (q.kv.count("extra")) p.extra = q.kv["extra"];
    if (q.kv.count("never_map")) p.never_map = q.kv["never_map"];
    if (q.kv.count("keys")) gh_parse_keys(q.kv["keys"], p.keys);
    if (q.kv.count("bootkeys")) gh_parse_bootkeys(q.kv["bootkeys"], p.bootkeys);
    if (q.kv.count("cd")) { p.cd = aw::sys_from_ini(q.kv["cd"]); if (!p.cd.empty() && !(p.cd.size() > 1 && p.cd[1] == ':') && p.cd[0] != '\\') p.cd = imgdir + "\\" + p.cd; }
    if (q.kv.count("autoexec")) p.autoexec = aw::sys_from_ini(q.kv["autoexec"]);
    if (!quick) gh_log("profile '%s' [%s] 제목='%s' 부팅=%c A=%s B=%s C=%s D=%s swap=%d keys=%d cycles=%s font=%s cd=%s bootkeys=%d | %s", id.c_str(), p.src.c_str(), p.title.c_str(), p.bootletter, p.disk[0] ? p.disk[0] : "-", p.disk[1] ? p.disk[1] : "-", p.disk[2] ? p.disk[2] : "-", p.disk[3] ? p.disk[3] : "-", (int)p.swap.size(), (int)p.keys.size(), p.cycles.c_str(), p.font.c_str(), p.cd.empty() ? "-" : p.cd.c_str(), (int)p.bootkeys.size(), p.reason.c_str());
    return true;
}

static bool g_noboot = false, g_noB = false, g_demo = false;
/* t51a (#878 묶음 2): 창 안 전환 — 일시정지 메뉴 「다음 단계」·「메인 메뉴로」 가 창·프로세스를 닫지 않고 같은 창에서 디스크를 바꿔 리셋한다. 실패하면 현행 재실행 경로(폴백, 설계_창안전환_874.md §9) */
static int g_inwin_req = 0;                  /* 0 없음 · 1 다음 단계 · 2 메인 메뉴로 */
static std::string g_inwin_target;           /* 1 일 때 다음 단계 항목 이름 */
static bool g_inwin_enabled = true, g_inwin_timeout = false, g_inwin_wait = false, g_inw_menu_pick = false;
static unsigned g_inwin_t0 = 0, g_inj_base = 0, g_inj_stamp = 0; static int g_inwin_n = 0, g_boot_n = 1, g_inw_kind = 0;
static std::string g_inwin_testfail;         /* 시험용 --inwintest attach|noboot|throw (제품 동작 무관) */
static std::string g_pid;                    /* 지금 작품 id(프로필 다시 읽기용) */
static std::string g_inw_sel_id;             /* 메뉴에서 고른 작품 id(폴백 재실행용) */
static bool g_menu_open = false, g_menu_req = false;
static void gh_apply_settings(const ui::Settings & s, bool vol);
static bool g_menu43 = false;   /* t52f (#132 2항): 큐하치 자체 화면(시작 메뉴)이 열려 있는 동안 화면비는 항상 4:3. 작품별 화면비는 게임이 시작될 때만 적용 */
static int g_resume_slot = 0;         /* 1..8 = 게임 시작 뒤 이 슬롯을 불러옴 */


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
static bool g_windowed = true, g_use_gl = false;
static int g_winw = 1280, g_winh = 800;

/* ------------------------------------------------------------------ r017: 창 배율 (640x480 기준 x1/x2/x3…) · 테두리로 끈 크기 = 「사용자 지정」 */
static unsigned g_wm_expect_until = 0; static int g_wm_ew = 0, g_wm_eh = 0;   /* 우리가 정한 크기가 되돌아오는 이벤트 무시용 */
static bool g_wm_dirty = false; static unsigned g_wm_last = 0;
/* 순수 판정(시험 가능): 이 크기 변경 이벤트가 사용자가 테두리를 끈 것인가 */
static bool gh_win_userresize(int w, int h, Uint32 flags, int ew, int eh, unsigned now, unsigned until) {
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) return false;
    if (ew > 0 && w == ew && h == eh && (int)(until - now) >= 0) return false;
    return w >= 160 && h >= 120;
}
static void gh_wm_refresh(void) {   /* 모니터에 들어가는 최대 배율 */
    SDL_Rect r; int top = 0, left = 0, bottom = 0, right = 0; r.x = r.y = 0; r.w = 1920; r.h = 1080;
    if (SDL_GetDisplayUsableBounds(0, &r) != 0) { SDL_DisplayMode dm; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { r.w = dm.w; r.h = dm.h - 40; } }
    if (g_win) SDL_GetWindowBordersSize(g_win, &top, &left, &bottom, &right);
    int dx = left + right, dy = top + bottom; if (dy == 0) { dy = 48; dx = 16; }
    int m = 0; for (int k = 1; k <= ui::Ui::WMN; k++) if (640 * k + dx <= r.w && 480 * k + dy <= r.h) m = k;
    g_ui.wmax = m < 1 ? 1 : m;
}
static void gh_fs_toggle(void) {   /* t52i (B3): 화면 설정 「전체화면」 줄 · Alt+Enter 와 같은 동작 */
    if (!g_win) return;
    bool fs = (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); SDL_ShowCursor(fs ? SDL_ENABLE : SDL_DISABLE);
    gh_log("SCREEN fullscreen=%d (menu)", fs ? 0 : 1); gh_osd(fs ? phr_("창 모드") : phr_("전체화면"));
}
static bool gh_fs_now(void) { return g_win && (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP; }
static void gh_win_apply(int m, bool osd) {   /* m>0: 배율 · m==0: 사용자 지정 크기(g_ui.winw x winh) */
    gh_wm_refresh(); int w, h;
    if (m > 0) { if (m > g_ui.wmax) m = g_ui.wmax; w = 640 * m; h = 480 * m; g_ui.wmult = m; } else { w = g_ui.winw; h = g_ui.winh; g_ui.wmult = 0; }
    if (w < 160 || h < 120) return;
    g_winw = w; g_winh = h; g_wm_ew = w; g_wm_eh = h; g_wm_expect_until = SDL_GetTicks() + 1500u;
    if (g_win) {
        if (SDL_GetWindowFlags(g_win) & SDL_WINDOW_MAXIMIZED) SDL_RestoreWindow(g_win);
        SDL_SetWindowSize(g_win, w, h); SDL_SetWindowPosition(g_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    g_wm_dirty = true; g_wm_last = SDL_GetTicks(); g_ui.dirty = true;
    gh_log("WINDOW size %dx%d mult=%d (max x%d)", w, h, g_ui.wmult, g_ui.wmax);
    if (osd) { char b[64]; if (m > 0) snprintf(b, sizeof b, "창 x%d (%dx%d)", m, w, h); else snprintf(b, sizeof b, "창 사용자 지정 (%dx%d)", w, h); gh_osd(b); }
}
static void gh_titlebar_event(const SDL_Event * e);   /* t52a (#54/#55) */
static int SDLCALL gh_ev_watch(void *, SDL_Event * e) {
    gh_titlebar_event(e);
    if (e->type != SDL_WINDOWEVENT || (e->window.event != SDL_WINDOWEVENT_SIZE_CHANGED && e->window.event != SDL_WINDOWEVENT_RESIZED) || !g_win) return 0;
    int w = e->window.data1, h = e->window.data2; unsigned now = SDL_GetTicks();
    if (!gh_win_userresize(w, h, SDL_GetWindowFlags(g_win), g_wm_ew, g_wm_eh, now, g_wm_expect_until)) return 0;
    int m = 0; for (int k = 1; k <= ui::Ui::WMN; k++) if (w == 640 * k && h == 480 * k) m = k;   /* 끌다가 정확히 배율 크기가 되면 배율로 */
    g_ui.wmult = m; g_ui.winw = w; g_ui.winh = h; g_winw = w; g_winh = h; g_wm_dirty = true; g_wm_last = now; g_ui.dirty = true;
    return 0;
}
static void gh_wm_poll(void) {   /* 끄는 동안은 기다렸다가(0.6초 조용하면) 한 번만 저장 + 로그 */
    if (g_wm_dirty && SDL_GetTicks() - g_wm_last > 600u) {
        g_wm_dirty = false; ui::settings_save(g_ui, g_cfgpath); gh_log("WINDOW saved: mult=%d custom=%dx%d", g_ui.wmult, g_ui.winw, g_ui.winh);
        if (!g_ui.wmult) gh_osd(phr_("창 사용자 지정 (") + std::to_string(g_ui.winw) + "x" + std::to_string(g_ui.winh) + ")");
    }
}
/* Alt+[ / Alt+] : 한 단계 작은/큰 배율 (사용자 지정에서는 그 크기 바로 아래/위 배율) */
static bool gh_wm_hotkey(const SDL_Event & e) {
    if (e.type != SDL_KEYDOWN || !(e.key.keysym.mod & KMOD_ALT) || g_headless || !g_win) return false;
    SDL_Keycode k = e.key.keysym.sym; if (k != SDLK_LEFTBRACKET && k != SDLK_RIGHTBRACKET) return false;
    gh_wm_refresh(); if (!(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) || true) { int m = g_ui.wm_next(k == SDLK_RIGHTBRACKET ? 1 : -1); if (m > 0) gh_win_apply(m, true); else gh_osd(k == SDLK_RIGHTBRACKET ? phr_("더 큰 배율이 모니터에 안 들어갑니다") : phr_("더 작은 배율이 없습니다")); }
    return true;
}
static std::string g_shaderdir;
static SDL_GLContext g_glctx = NULL;
static SDL_Thread * g_glthr = NULL;
static volatile bool g_gl_stop = false;
static SDL_mutex * g_fb_mtx = NULL;
static std::vector<uint32_t> g_pub, g_rbuf;
static int g_pub_w = 0, g_pub_h = 0;
static bool g_pub_new = false;
/* t46z (#548): kbshader=0(기본) = 화면 키보드·버튼·배속 ▶▶·OSD 를 g_pub 이 아니라 별도 버퍼에 그려 셰이더 뒤에 평면으로 얹는다. 1 = t46y 와 같은 경로(셰이더를 받음).
   별도 버퍼의 알파는 「검정 바탕 / 흰 바탕」 2회 합성의 차이로 뽑는다: 바탕 d 위에 알파 a 로 합성하면 결과 = c*a + d*(1-a) 이므로 W-B = 255*(1-a), B = premultiplied 색. */
static volatile int g_kbshader = 0; static int g_cli_kbsh = -1;   /* --kbshader 0|1 이 설정보다 우선(시험용) */
static std::vector<uint32_t> g_ovl, g_ovl_gl, g_ovt_b, g_ovt_w;
static int g_ovl_w = 0, g_ovl_h = 0; static bool g_ovl_has = false, g_ovl_new = false;
static const char * g_overlay_src =
    "varying vec2 vUV;\n#ifdef VERTEX\nattribute vec2 aPos;\nattribute vec2 aUV;\nvoid main(){ gl_Position = vec4(aPos, 0.0, 1.0); vUV = aUV; }\n#endif\n"
    "#ifdef FRAGMENT\nuniform sampler2D uTex;\nvoid main(){ gl_FragColor = texture2D(uTex, vUV); }\n#endif\n";
static void gh_ovl_draw_all(uint32_t * px, int fw, int fh) { ov::Canvas oc = { px, fw, fh }; kbov::draw(px, fw, fh); gh_boost_osd_draw(oc, g_osd, g_osd_until && SDL_GetTicks() < g_osd_until); }
static bool gh_ovl_build(int fw, int fh) {   /* g_fb_mtx 를 잡은 채로 호출 */
    bool osd_on = g_osd_until && SDL_GetTicks() < g_osd_until;
    if (fw <= 0 || fh <= 0 || !(kbov::active() || osd_on || (g_boost_on && g_bosd_on) || !g_bk_osd.empty())) return false;
    size_t n = (size_t)fw * fh; g_ovt_b.assign(n, 0xFF000000u); g_ovt_w.assign(n, 0xFFFFFFFFu);
    gh_ovl_draw_all(g_ovt_b.data(), fw, fh); gh_ovl_draw_all(g_ovt_w.data(), fw, fh);
    g_ovl.resize(n); bool any = false;
    for (size_t i = 0; i < n; i++) {
        uint32_t b = g_ovt_b[i], w = g_ovt_w[i];
        int dr = (int)((w >> 16) & 255) - (int)((b >> 16) & 255), dg = (int)((w >> 8) & 255) - (int)((b >> 8) & 255), db = (int)(w & 255) - (int)(b & 255);
        int dm = dr > dg ? dr : dg; if (db > dm) dm = db; if (dm < 0) dm = 0; if (dm > 255) dm = 255;
        int a = 255 - dm; if (a <= 0) { g_ovl[i] = 0; continue; }
        int r = (int)((b >> 16) & 255), g = (int)((b >> 8) & 255), bl = (int)(b & 255); if (r > a) r = a; if (g > a) g = a; if (bl > a) bl = a;
        g_ovl[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl; any = true;
    }
    return any;
}
static void gh_ovl_off(void) { if (g_ovl_has) { g_ovl_has = false; g_ovl_new = true; } }   /* g_fb_mtx 를 잡은 채로 */
static void gh_ovl_update(int fw, int fh) {   /* g_fb_mtx 를 잡은 채로. 배속 중 프레임이 몰려도 16ms 에 한 번만 다시 만든다 */
    static Uint32 last = 0; static bool first = true; Uint32 now = SDL_GetTicks();
    if (!first && now - last < 16) return; first = false; last = now;
    bool b = gh_ovl_build(fw, fh); g_ovl_has = b; g_ovl_w = fw; g_ovl_h = fh; g_ovl_new = true;
}

static volatile int g_shot_req = 0; static char g_shot_path[1024];
static volatile int g_shot_crop = 0; static int g_shot_w = 0, g_shot_h = 0;   /* t41: 화면 설정(셰이더·곡면·비율) 적용된 스크린샷 — 화면 영역만 잘라 저장 */
static int g_scr_pend = 0;
static bool g_vsync = false;
static unsigned int g_glmx_ms = 0; static bool g_glmx_done = false;
struct GProg { unsigned int p; int uTex, uSrc, uOut, uCurve, uScan, uMask, uBleed, uGlow, uNear; bool ok, linear; };
static GProg g_prog[SH_N]; static GProg g_prog_ov; static unsigned int g_ov_tex = 0;
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
    if (!gl_load()) return false;
    gh_log("GL: vendor='%s' renderer='%s' version='%s'", (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
    gl_build(g_nearest_src, g_prog[SH_NEAREST], false, "builtin-nearest");
    if (!g_prog[SH_NEAREST].ok) return false;
    gl_build(g_overlay_src, g_prog_ov, true, "builtin-overlay");
    const char * names[SH_N] = { "", "pc98_sharp.glsl", "pc98_crt.glsl" };
    for (int i = SH_SHARP; i < SH_N; i++) {
        std::string src, path = g_shaderdir + "\\" + names[i];
        if (!gl_readfile(path, src)) {   /* r019: 파일이 없으면 내장 셰이더 + 화면 경고 */
            const unsigned char * ep = (i == SH_SHARP) ? emb_sh_sharp : emb_sh_crt; size_t en = (i == SH_SHARP) ? emb_sh_sharp_n : emb_sh_crt_n;
            src.assign((const char *)ep, en); g_builtin_res = true; if (g_osd.empty()) gh_osd(phr_("셰이더 파일이 없어 내장 셰이더로 실행합니다 (exe 옆 shaders 폴더 확인)"), 12000); gh_log("WARN: GL: shader file not found: %s -> 내장 셰이더 사용", path.c_str());
        }
        if (i == SH_CRT && src.find("PC98_FX_T48C") == std::string::npos) { gh_log("GL: pc98_crt.glsl 에 효과 표지(PC98_FX_T48C)가 없다(옛 판) -> 내장 셰이더 사용"); src.assign((const char *)emb_sh_crt, emb_sh_crt_n); }   /* t48c: 옛 res 폴더의 셰이더 파일이 남아 있어도 새 효과가 먹게 한다 */
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
    int tw = 0, th = 0, cw = 0, ch = 0; int ovw = 0, ovh = 0, otw = 0, oth = 0; bool ov_has = false, ov_dirty = false;
    while (!g_gl_stop) {
        int dw = 0, dh = 0; SDL_GL_GetDrawableSize(g_win, &dw, &dh);
        bool nf = false;
        SDL_LockMutex(g_fb_mtx);
        if (g_pub_new) { std::swap(g_pub, g_rbuf); cw = g_pub_w; ch = g_pub_h; g_pub_new = false; nf = true; }
        if (g_ovl_new) { std::swap(g_ovl, g_ovl_gl); ovw = g_ovl_w; ovh = g_ovl_h; ov_has = g_ovl_has; g_ovl_new = false; ov_dirty = true; }

        SDL_UnlockMutex(g_fb_mtx);
        glViewport(0, 0, dw, dh); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        int sx = 0, sy = 0, srw = 0, srh = 0;
        if (cw > 0 && ch > 0 && g_rbuf.size() >= (size_t)cw * ch) {
            glBindTexture(GL_TEXTURE_2D, g_tex_gl);
            if (cw != tw || ch != th) { glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cw, ch, 0, GL_BGRA, GL_UNSIGNED_BYTE, g_rbuf.data()); tw = cw; th = ch; }
            else if (nf) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw, ch, GL_BGRA, GL_UNSIGNED_BYTE, g_rbuf.data());
            int fit = g_fit, sh = g_shader;
            if (fit == FIT_INT) sh = SH_NEAREST;
            const GProg & P = g_prog[sh];
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
            if (P.uCurve >= 0) F.Uniform1f(P.uCurve, g_curve);
            if (P.uScan >= 0) F.Uniform1f(P.uScan, g_scanv);
            if (P.uMask >= 0) F.Uniform1f(P.uMask, g_maskv);
            if (P.uBleed >= 0) F.Uniform1f(P.uBleed, g_bleedv);
            if (P.uGlow >= 0) F.Uniform1f(P.uGlow, g_glowv);
            if (P.uNear >= 0) F.Uniform1f(P.uNear, g_nearv);
            F.BindBuffer(GL_ARRAY_BUFFER, g_vbo);
            F.EnableVertexAttribArray(0); F.EnableVertexAttribArray(1);
            F.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (const void *)0); F.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void *)8);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            if (g_gltime) {   /* t42: 그리기 시간(glFinish 까지) 3초 평균 */
                static double gt_sum = 0, gt_max = 0; static int gt_n = 0; static Uint64 gt_t0 = 0;
                glFinish(); Uint64 gt_b = SDL_GetPerformanceCounter(); double fq = (double)SDL_GetPerformanceFrequency();
                double ms = (double)(gt_b - gt_a) * 1000.0 / fq; gt_sum += ms; if (ms > gt_max) gt_max = ms; gt_n++;
                if (gt_t0 == 0) gt_t0 = gt_b;
                if ((double)(gt_b - gt_t0) / fq >= 3.0) { gh_log("GLTIME 그리기 평균 %.3f ms (최대 %.3f, %d프레임, glFinish 기준) shader=%d scan=%.1f mask=%.2f bleed=%.2f glow=%.2f near=%.0f curve=%.2f out=%dx%d", gt_sum / gt_n, gt_max, gt_n, sh, (double)g_scanv, (double)g_maskv, (double)g_bleedv, (double)g_glowv, (double)g_nearv, (double)g_curve, rw, rh); gt_sum = 0; gt_max = 0; gt_n = 0; gt_t0 = gt_b; }
            }
        }
        if (ov_has && !g_kbshader && !g_shot_req && srw > 0 && srh > 0 && ovw > 0 && ovh > 0 && g_ovl_gl.size() >= (size_t)ovw * ovh && g_prog_ov.ok) {   /* t46z: 셰이더 사각형과 같은 뷰포트에 평면 알파 합성(premultiplied) */
            if (g_ov_tex == 0) { glGenTextures(1, &g_ov_tex); glBindTexture(GL_TEXTURE_2D, g_ov_tex); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); otw = 0; oth = 0; ov_dirty = true; }
            glBindTexture(GL_TEXTURE_2D, g_ov_tex);
            if (ov_dirty) { if (ovw != otw || ovh != oth) { glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ovw, ovh, 0, GL_BGRA, GL_UNSIGNED_BYTE, g_ovl_gl.data()); otw = ovw; oth = ovh; } else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ovw, ovh, GL_BGRA, GL_UNSIGNED_BYTE, g_ovl_gl.data()); ov_dirty = false; }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glViewport(sx, sy, srw, srh);
            glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            F.UseProgram(g_prog_ov.p); if (g_prog_ov.uTex >= 0) F.Uniform1i(g_prog_ov.uTex, 0);
            F.BindBuffer(GL_ARRAY_BUFFER, g_vbo); F.EnableVertexAttribArray(0); F.EnableVertexAttribArray(1);
            F.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (const void *)0); F.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void *)8);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisable(GL_BLEND); glBindTexture(GL_TEXTURE_2D, g_tex_gl);
        }
        if (g_shot_req) {
            int r0x = 0, r0y = 0, rwd = dw, rht = dh;

            if (g_shot_crop && srw > 0 && srh > 0 && sx >= 0 && sy >= 0 && sx + srw <= dw && sy + srh <= dh) { r0x = sx; r0y = sy; rwd = srw; rht = srh; }
            std::vector<uint32_t> px((size_t)rwd * rht), fl((size_t)rwd * rht);
            glReadPixels(r0x, r0y, rwd, rht, GL_BGRA, GL_UNSIGNED_BYTE, px.data());
            for (int yy = 0; yy < rht; yy++) memcpy(&fl[(size_t)yy * rwd], &px[(size_t)(rht - 1 - yy) * rwd], (size_t)rwd * 4);
            gh_png(g_shot_path, fl.data(), rwd, rht);
            g_shot_w = rwd; g_shot_h = rht;
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
        g_pub.assign((size_t)g_fbw * g_fbh, 0); g_pub_w = g_fbw; g_pub_h = g_fbh; g_pub_new = true; gh_ovl_off();
        SDL_UnlockMutex(g_fb_mtx);
    } else if (!g_headless && g_ren) {
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, g_fbw, g_fbh);
        int sc = 1; while ((g_fbw * (sc + 1) <= 1600) && (g_fbh * (sc + 1) <= 1000)) sc++;
        SDL_SetWindowSize(g_win, g_fbw * sc, g_fbh * sc);
        SDL_RenderSetLogicalSize(g_ren, g_fbw, g_fbh);
    }
}
/* ------------------------------------------------------------------ r017: 로딩 덮개 · 프레임 기록 */
static bool gh_text_has_dbx(void) {   /* 텍스트 VRAM(80x25) 에 DOSBox-X 스플래시 글자가 있는가 */
    static const char * const KW[] = { "DOSBox", "DOSBOX", "SPACEBAR", "BIOS setup", "joncampbell", "Welcome", NULL };
    char row[81];
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) { unsigned ch = mem_readw(0xA0000 + (unsigned)(y * 80 + x) * 2u); row[x] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : ' '; }
        row[80] = 0; for (int k = 0; KW[k]; k++) if (strstr(row, KW[k])) return true;
    }
    return false;
}
static void gh_loading_draw(uint32_t * px, int w, int h) {   /* 검은 바탕 + 제품명 + Now Loading... (부팅 연출과 같은 자리) */
    ov::Canvas c = { px, w, h }; c.rect(0, 0, w, h, ov::K);
    int d = (int)((SDL_GetTicks() / 400u) % 4u);
    ov::text(c, 40, 112, std::string(KH_FULLNAME), ov::WH, ov::K);
    ov::text(c, 40, 144, std::string("Now Loading") + std::string((size_t)d, '.') + std::string((size_t)(3 - d), ' '), ov::WH, ov::K);
}
struct FtThumb { std::vector<uint32_t> px; std::string label; };
static std::vector<FtThumb> g_ft_thumbs; static FILE * g_ft_csv = NULL; static int g_ft_slot = 0; static unsigned g_ft_frames = 0, g_ft_rawdbx = 0, g_ft_shown = 0, g_ft_load = 0, g_ft_first_nb = 0xFFFFFFFFu;
static void gh_ft_sheet(void) {   /* 20 장 컨택트 시트 (5x4, 장당 320x200) */
    const int CW = 320, CH = 200, COLS = 5, ROWS = 4; std::vector<uint32_t> sheet((size_t)CW * COLS * CH * ROWS, 0x202020); ov::Canvas sc = { sheet.data(), CW * COLS, CH * ROWS };
    for (size_t i = 0; i < g_ft_thumbs.size() && i < (size_t)(COLS * ROWS); i++) {
        int ox = (int)(i % COLS) * CW, oy = (int)(i / COLS) * CH; const FtThumb & t = g_ft_thumbs[i];
        for (int y = 0; y < CH - 2; y++) for (int x = 0; x < CW - 2; x++) sheet[(size_t)(oy + y) * (CW * COLS) + ox + x] = t.px[(size_t)y * CW + x];
        sc.rect(ox, oy, 170, 18, ov::K); ov::text(sc, ox + 2, oy + 1, t.label, ov::YL, ov::K);
    }
    gh_png((g_ft_dir + "\\contact_sheet.png").c_str(), sheet.data(), CW * COLS, CH * ROWS);
}
/* t35 (#270): 스크린샷. g_fb(에뮬 렌더 결과, 오버레이·메뉴 없음)를 640x400 으로 저장. 프레임 완성 시점(RENDER_EndUpdate)에서만 부른다(메뉴 항목은 게임이 멈춘 상태라 즉시 호출). */
static volatile bool g_scr_req = false; static std::vector<unsigned int> g_scrshots_ms;
static bool gh_shot_save(void) {
    if (g_fb.empty() || g_fbw <= 0 || g_fbh <= 0) { gh_osd(phr_("스크린샷 저장 실패"), 1500); gh_log("SCREENSHOT: no frame"); return false; }
    std::string dir = g_exedir + "\\screenshots"; plat::mkdir(dir.c_str());
    plat::LocalTime st = plat::local_now(); char ts[40]; snprintf(ts, sizeof ts, "%04d%02d%02d-%02d%02d%02d", st.year, st.month, st.day, st.hour, st.minute, st.second);
    std::string base = dir + "\\" + g_prof.id + "_" + ts, path = base + ".png";
    for (int n = 2; n < 100 && plat::exists(path.c_str()); n++) path = base + "_" + std::to_string(n) + ".png";
    if (!g_headless && g_use_gl && g_fb_mtx && g_win) {   /* t41: 창에 보이는 그대로(셰이더·곡면·비율) 화면 영역만 저장 */
        snprintf(g_shot_path, sizeof g_shot_path, "%s", path.c_str()); g_shot_w = g_shot_h = 0; g_shot_crop = 1; g_shot_req = 1;
        for (int t = 0; t < 600 && g_shot_req; t++) SDL_Delay(5);
        bool done = !g_shot_req; g_shot_crop = 0; g_shot_req = 0;
        plat::FileInfo fg; bool okg = done && plat::stat_file(path, fg) && fg.size_lo > 0;
        if (okg) { gh_log("SCREENSHOT OK %s (화면 설정 적용 %dx%d, shader=%d fit=%d curve=%.2f)", path.c_str(), g_shot_w, g_shot_h, (int)g_shader, (int)g_fit, (double)g_curve); gh_osd(phr_("스크린샷 저장됨"), 1500); return true; }
        gh_log("SCREENSHOT GL 경로 실패(done=%d) — 원본 프레임으로 대체", (int)done);
    }
    std::vector<uint32_t> out; const uint32_t * src = g_fb.data();
    if (g_fbw != 640 || g_fbh != 400) { out.assign(640 * 400, 0); ov::Canvas sc = { out.data(), 640, 400 }; ov::blit_scaled(sc, g_fb.data(), g_fbw, g_fbh, 0, 0, 640, 400); src = out.data(); }
    gh_png(path.c_str(), src, 640, 400);
    plat::FileInfo fa; bool ok = plat::stat_file(path, fa) && (fa.size_lo > 0);
    gh_log("SCREENSHOT %s %s (src %dx%d -> 640x400)", ok ? "OK" : "FAIL", path.c_str(), g_fbw, g_fbh);
    gh_osd(ok ? phr_("스크린샷 저장됨") : phr_("스크린샷 저장 실패"), 1500); return ok;
}
/* t48d (#643): first-click diagnostics (raw SDL events, hint value, mouse/keyboard focus) + window-only capture (F12 -> exedir\winshots\win_*.png, whole player window incl. menu/overlay) */
static int g_dg_n[2] = { 0, 0 }, g_dg_m[2] = { 0, 0 };
static void gh_diag_focus(const char * why) {
    SDL_Window * mf = SDL_GetMouseFocus(), * kf = SDL_GetKeyboardFocus(); const char * hv = SDL_GetHint("SDL_MOUSE_FOCUS_CLICKTHROUGH"); unsigned wf = g_win ? SDL_GetWindowFlags(g_win) : 0;
    gh_log("DIAG %s: hint CLICKTHROUGH=%s mouseFocus=%s keyFocus=%s winflags input=%d mouse=%d shown=%d minimized=%d relmouse=%d", why, hv ? hv : "(null)", mf ? (mf == g_win ? "g_win" : "other") : "none", kf ? (kf == g_win ? "g_win" : "other") : "none",
        (wf & SDL_WINDOW_INPUT_FOCUS) ? 1 : 0, (wf & SDL_WINDOW_MOUSE_FOCUS) ? 1 : 0, (wf & SDL_WINDOW_SHOWN) ? 1 : 0, (wf & SDL_WINDOW_MINIMIZED) ? 1 : 0, (int)SDL_GetRelativeMouseMode());
}
static void gh_diag_enter(int tag) { g_dg_n[tag] = 0; g_dg_m[tag] = 0; gh_log("DIAG enter loop=%s", tag ? "game-menu" : "main-menu"); gh_diag_focus("enter"); }
static bool gh_winshot(void) {
    if (g_headless || !g_use_gl || !g_win) { gh_log("WINSHOT: no GL window"); return false; }
    std::string dir = g_exedir + "\\winshots"; plat::mkdir(dir.c_str());
    plat::LocalTime st = plat::local_now(); char ts[64]; snprintf(ts, sizeof ts, "%04d%02d%02d-%02d%02d%02d_%03d", st.year, st.month, st.day, st.hour, st.minute, st.second, st.ms);
    std::string path = dir + "\\win_" + ts + ".png";
    snprintf(g_shot_path, sizeof g_shot_path, "%s", path.c_str()); g_shot_w = g_shot_h = 0; g_shot_crop = 0; g_shot_req = 1;
    for (int t = 0; t < 600 && g_shot_req; t++) SDL_Delay(5);
    bool done = !g_shot_req; g_shot_req = 0;
    gh_log("WINSHOT %s %s %dx%d", done ? "OK" : "FAIL", path.c_str(), g_shot_w, g_shot_h); return done;
}
/* tag: 0 = main menu loop, 1 = in-game menu loop, -1 = in-game (no diag). returns true = event consumed */
/* t49a (#817·#821·#823): 패드 연결/해제 — 메뉴 루프·게임 루프·일시정지 메뉴 어디서든 gh_evpre 가 한 곳에서 처리한다.
   입력층(gensei_input_event)이 열기/닫기·버튼 놓기를 하고, 여기서는 마지막 입력 장치 표기와 2초 알림만 얹는다 */
static void gh_hotplug_hook(bool added, const char * name) {
    ov::g_helpdev = added ? 0 : 1;   /* 연결 = 패드 표기, 해제 = 키보드 표기 */
    std::string m = added ? std::string(phr_("컨트롤러 연결됨: ")) + (name ? name : "?") : std::string(phr_("컨트롤러 해제됨"));
    g_note = m; g_note_until = SDL_GetTicks() + 2000; gh_osd(m, 2000);
    gh_log("HOTPLUG %s (%s) -> 조작 설명 표기 %s", added ? "연결" : "해제", name ? name : "?", added ? "패드" : "키보드");
}
#include "gensei_harness_t49a.h"   /* t49a: 하니스 전용 코드(별도 파일) */
static bool gh_evpre(const SDL_Event & e, int tag) {
    if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED) { gensei_input_event(e); return true; }   /* t49a: 장치 추가/제거는 메뉴·게임 공통으로 여기서 한 번만 */
    if (gensei_raw_event(e)) return true;   /* t48e (#651 2): 표에 없는 패드의 원시 이벤트 → 표준 이벤트로 바꿔 다시 밀어 넣음(메뉴·게임 공통) */
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F12 && !e.key.repeat) { if ((e.key.keysym.mod & KMOD_CTRL) || (SDL_GetModState() & KMOD_CTRL)) { gh_winshot(); return true; } if (tag == 0) return true; /* t49f (#858): 평범한 F12 는 창 캡처가 먹지 않고 게임 중 일시정지 메뉴(게임 루프)·일시정지 메뉴(닫기)로 통과, 창 캡처는 Ctrl+F12 */ }
    if (tag < 0 || g_dg_n[tag] >= 80) return false;
    if (e.type == SDL_MOUSEMOTION) { if (g_dg_m[tag] < 8) { g_dg_m[tag]++; gh_log("DIAG[%d] MOUSEMOTION x=%d y=%d xrel=%d yrel=%d state=%u which=%u", tag, e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel, (unsigned)e.motion.state, (unsigned)e.motion.which); } return false; }
    int n = ++g_dg_n[tag];
    switch (e.type) {
    case SDL_WINDOWEVENT: gh_log("DIAG[%d] ev#%d WINDOWEVENT id=%d d1=%d d2=%d", tag, n, (int)e.window.event, (int)e.window.data1, (int)e.window.data2); if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e.window.event == SDL_WINDOWEVENT_ENTER) gh_diag_focus("window-event"); break;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
        gh_log("DIAG[%d] ev#%d MOUSEBUTTON%s btn=%d clicks=%d x=%d y=%d which=%u state=%d", tag, n, e.type == SDL_MOUSEBUTTONDOWN ? "DOWN" : "UP", (int)e.button.button, (int)e.button.clicks, e.button.x, e.button.y, (unsigned)e.button.which, (int)e.button.state);
        if (e.type == SDL_MOUSEBUTTONDOWN) gh_diag_focus("at-click"); break;
    case SDL_KEYDOWN: case SDL_KEYUP: gh_log("DIAG[%d] ev#%d KEY%s sym=%d scan=%d", tag, n, e.type == SDL_KEYDOWN ? "DOWN" : "UP", (int)e.key.keysym.sym, (int)e.key.keysym.scancode); break;
    default: gh_log("DIAG[%d] ev#%d type=0x%x", tag, n, (unsigned)e.type); break;
    }
    return false;
}
static void gh_frame_hook(void) {
    if (!g_pon_tick || g_fb.empty()) return;
    unsigned wall = SDL_GetTicks() - g_pon_tick; unsigned game = (unsigned)(PIC_Ticks - g_pon_pic);
    size_t n = g_fb.size(), nbc = 0; for (size_t i = 0; i < n; i++) if (g_fb[i] & 0xFFFFFF) nbc++;
    unsigned nb = (unsigned)(nbc * 1000 / n); bool dbx = gh_text_has_dbx();
    g_frames_seen++; if (dbx) g_dbx_raw_frames++;
    if (g_loading) {
        bool end = false; const char * why = "";
        if (g_boot_tick && nb >= 8 && !dbx) { end = true; why = phr_("첫 게임 화면"); }
        else if (wall > 20000) { end = true; why = phr_("시간 초과(20초)"); }
        if (end) { g_loading = false; g_load_end_wall = wall; g_load_end_game = game; gh_log("LOADING end: %s wall=%u game=%u frame=%u nonblack=%u‰", why, wall, game, g_frames_seen, nb); }
    }
    bool shown_dbx = dbx && !(g_loading && !g_noload);
    if (!g_ft_dir.empty() && !g_ft_done) {
        if (!g_ft_csv) { g_ft_csv = fopen((g_ft_dir + "\\frames.csv").c_str(), "wb"); if (g_ft_csv) fprintf(g_ft_csv, "frame,wall_ms,game_ms,nonblack_permille,raw_dbx_text,loading_overlay,shown_dbx_text,boot_started\n"); }
        bool lo = g_loading && !g_noload;
        if (game <= g_ft_ms) {
            g_ft_frames++; if (dbx) g_ft_rawdbx++; if (shown_dbx) g_ft_shown++; if (lo) g_ft_load++; if (nb >= 8 && g_ft_first_nb == 0xFFFFFFFFu) g_ft_first_nb = game;
            if (g_ft_csv) fprintf(g_ft_csv, "%u,%u,%u,%u,%d,%d,%d,%d\n", g_frames_seen, wall, game, nb, (int)dbx, (int)lo, (int)shown_dbx, (int)(g_boot_tick != 0));
            if ((int)(game / 100u) >= g_ft_slot && g_ft_slot < 20) {   /* 100ms 칸마다 첫 프레임 1장: 실제로 보이는 화면(덮개 포함) */
                std::vector<uint32_t> view; if (lo) { view.assign(640 * 400, 0); gh_loading_draw(view.data(), 640, 400); } else view = g_fb;
                int vw = lo ? 640 : g_fbw, vh = lo ? 400 : g_fbh; char nm[64]; snprintf(nm, sizeof nm, "\\ft_%02d_t%04u.png", g_ft_slot, game); gh_png((g_ft_dir + nm).c_str(), view.data(), vw, vh);
                FtThumb t; t.px.assign(320 * 200, 0); for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) t.px[(size_t)y * 320 + x] = view[(size_t)(y * vh / 200) * vw + (x * vw / 320)];
                char lb[64]; snprintf(lb, sizeof lb, "#%d t=%.2fs dbx=%d%s", g_ft_slot, game / 1000.0, (int)shown_dbx, lo ? " L" : ""); t.label = lb; g_ft_thumbs.push_back(t); g_ft_slot++;
            }
        } else {
            g_ft_done = true; if (g_ft_csv) { fclose(g_ft_csv); g_ft_csv = NULL; } gh_ft_sheet();
            gh_log("FRAMETEST done: window=%ums frames=%u raw_dbx_text_frames=%u shown_dbx_text_frames=%u loading_overlay_frames=%u first_nonblack_game_ms=%d splash=%d overlay=%d", g_ft_ms, g_ft_frames, g_ft_rawdbx, g_ft_shown, g_ft_load, g_ft_first_nb == 0xFFFFFFFFu ? -1 : (int)g_ft_first_nb, gensei_nosplash ? 0 : 1, g_noload ? 0 : 1);
        }
    }
}

bool RENDER_StartUpdate(void) { g_cury = 0; g_starts++; return true; }
void RENDER_EndUpdate(bool abort) {
    if (abort) { g_aborts++; return; }
    g_frames++;
    gh_frame_hook();
    if (g_scr_req) { g_scr_req = false; g_scr_pend = (!g_headless && g_use_gl) ? 3 : 1; }   /* t41: GL 은 게시된 프레임이 화면에 올라간 뒤 찍는다 */
    if (!g_headless && g_use_gl && !g_fb.empty()) {
        SDL_LockMutex(g_fb_mtx);
        if (g_pub.size() != g_fb.size()) g_pub.resize(g_fb.size());
        memcpy(g_pub.data(), g_fb.data(), g_fb.size() * 4); g_pub_w = g_fbw; g_pub_h = g_fbh; g_pub_new = true;
        if (g_loading && !g_noload) { gh_loading_draw(g_pub.data(), g_fbw, g_fbh); gh_ovl_off(); }
        else if (!g_menu_open) { if (g_kbshader) { ov::Canvas oc = { g_pub.data(), g_fbw, g_fbh }; kbov::draw(g_pub.data(), g_fbw, g_fbh); gh_boost_osd_draw(oc, g_osd, g_osd_until && SDL_GetTicks() < g_osd_until); gh_ovl_off(); } else gh_ovl_update(g_fbw, g_fbh); }
        else gh_ovl_off();

        SDL_UnlockMutex(g_fb_mtx);
    } else if (!g_headless && g_ren && g_tex && !g_fb.empty()) {
        if (g_loading && !g_noload) { static std::vector<uint32_t> lv; lv.assign(g_fb.size(), 0); gh_loading_draw(lv.data(), g_fbw, g_fbh); SDL_UpdateTexture(g_tex, NULL, lv.data(), g_fbw * 4); } else SDL_UpdateTexture(g_tex, NULL, g_fb.data(), g_fbw * 4);
        SDL_RenderClear(g_ren);
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        SDL_RenderPresent(g_ren);
    }
    if (g_scr_pend > 0 && --g_scr_pend == 0) gh_shot_save();
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
#ifndef GH_DOSKERNEL
uint16_t GetDefaultCP(void) { return 932; }   /* t37cd: CD 빌드는 dos_keyboard_layout.obj 의 실물을 쓴다 */
#endif

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
static FILE * gh_open_rw(const std::string & path, uint64_t * size, bool rw = true) {
    FILE * f = rw ? fopen(path.c_str(), "rb+") : NULL;
    if (!f) f = fopen(path.c_str(), "rb");
    if (!f) return NULL;
    fseeko64(f, 0, SEEK_END); *size = (uint64_t)ftello64(f); fseeko64(f, 0, SEEK_SET);
    return f;
}
/* t43h (#333): 원본 이미지는 절대 바뀌지 않는다 — 첫 실행 때 작품 저장폴더(저장폴더\<id>\disk\)에 작업 사본을 만들고 읽기·쓰기는 사본에만. 이후 실행은 사본 사용. 사본을 못 만들면 원본을 읽기 전용으로 연다. */
static bool gh_copy_file(const std::string & from, const std::string & to) {
    FILE * a = fopen(from.c_str(), "rb"); if (!a) return false; FILE * b = fopen((to + ".tmp").c_str(), "wb"); if (!b) { fclose(a); return false; }
    std::vector<char> buf(1 << 20); bool ok = true; for (;;) { size_t n = fread(&buf[0], 1, buf.size(), a); if (!n) break; if (fwrite(&buf[0], 1, n, b) != n) { ok = false; break; } }
    fclose(a); if (fclose(b) != 0) ok = false; if (!ok) { plat::remove_file((to + ".tmp").c_str()); return false; }
    plat::remove_file(to.c_str()); return plat::move_file(to + ".tmp", to);
}
static std::string gh_disk_dir(void) { return g_savedir + "\\" + std::string(g_prof.id ? g_prof.id : "x") + "\\disk"; }
static std::string gh_workpath(const std::string & name) {
    std::string orig = g_imgdir + "\\" + name, dd = gh_disk_dir(); std::string base = name; { size_t p = base.find_last_of("\\/"); if (p != std::string::npos) base = base.substr(p + 1); }
    std::string wp = dd + "\\" + base;
    plat::FileInfo ao, aw; if (!plat::stat_file(orig, ao)) return orig;
    bool have = plat::stat_file(wp, aw) && aw.size_hi == ao.size_hi && aw.size_lo == ao.size_lo;
    if (!have) { plat::mkdir(g_savedir.c_str()); plat::mkdir((g_savedir + "\\" + std::string(g_prof.id ? g_prof.id : "x")).c_str()); plat::mkdir(dd.c_str());
        if (!gh_copy_file(orig, wp)) { gh_log("작업 사본 만들기 실패 -> 원본을 읽기 전용으로 연다: %s", orig.c_str()); return orig; } gh_log("작업 사본 만듦: %s", wp.c_str()); }
    return wp;
}
static int gh_reset_disks(const std::string & id) {   /* 「원본 디스크로 초기화」: 작품 저장폴더의 디스크 작업 사본을 지운다(다음 실행 때 원본에서 다시 만든다) */
    std::string dd = g_savedir + "\\" + id + "\\disk"; int n = 0;
    plat::find_a(dd + "\\*", [&](const plat::DirEnt & fd) { if (fd.is_dir) return true; if (plat::remove_file(dd + "\\" + fd.name)) n++; return true; });
    plat::remove_dir(dd); gh_log("원본 디스크로 초기화: %s 사본 %d개 삭제", id.c_str(), n); return n;
}
static int g_disk_ix[4] = { -1, -1, -1, -1 };   /* 드라이브 -> g_disks 위치 (교체 때 같은 자리를 갱신) */
static bool gh_attach(int idx, const char * name, bool replace = false) {
    std::string orig_path = g_imgdir + "\\" + name, path = gh_workpath(name); bool iscopy = path != orig_path;   /* t43h: 쓰기는 작업 사본에만 */
    uint64_t sz = 0;
    FILE * f = gh_open_rw(path, &sz, iscopy);
    if (!f) { gh_log("ERROR: cannot open image %s", path.c_str());
        if (!replace) gh_human(std::string(phr_("게임 디스크 이미지 파일을 열 수 없습니다.\n파일: ")) + aw::acp_to_u8(std::string(name)) + phr_("\n찾은 곳: ") + aw::acp_to_u8(g_imgdir) + phr_("\n파일이 없거나 옮겨졌을 수 있습니다. 작품 폴더 game.ini 의 imgdir(이미지 폴더) 줄과 파일 이름을 확인해 주세요.\n[위치 지정] 단추는 다음 판(롬 루트 연결)에서 제공합니다."));
        else { gh_human(std::string(phr_("교체할 디스크 파일을 열 수 없습니다: ")) + aw::acp_to_u8(std::string(name)) + phr_("\n(찾은 곳: ") + aw::acp_to_u8(g_imgdir) + ")"); gh_msgbox(g_errmsg.c_str()); }
        return false; }
    imageDisk * img; { prof::ImgInfo pi = prof::probe(path); uint32_t szk = (uint32_t)(sz / 1024ull);   /* r017: 클래스는 확장자가 아니라 파일 머리로 고른다 */
        if (pi.fmt == prof::F_D88) { img = new imageDiskD88(f, name, szk, false); gh_log("image[%d] class=D88 (header)", idx); }
        else if (pi.fmt == prof::F_NFD0 || pi.fmt == prof::F_NFD1) { img = new imageDiskNFD(f, name, szk, false, pi.fmt == prof::F_NFD1 ? 1u : 0u); gh_log("image[%d] class=NFD r%d (header)", idx, pi.fmt == prof::F_NFD1 ? 1 : 0); }
        else img = new imageDisk(f, name, sz, sz > 2880ull * 1024ull); }
    uint32_t h, c, s, ss; img->Get_Geometry(&h, &c, &s, &ss);
    gh_log("image[%d] %s: size=%llu CHS=%u/%u/%u ss=%u", idx, name, (unsigned long long)sz, c, h, s, ss);
    { /* 스테이트 헤더용 디스크 정보: 부팅 직전(수정 전) 내용 CRC32 */
        Pc98StateDisk d; d.name = name; d.size = sz; d.crc32 = 0xFFFFFFFFu;
        static uint32_t tab[256]; static bool ti = false;
        if (!ti) { for (uint32_t i = 0; i < 256; i++) { uint32_t cc = i; for (int k = 0; k < 8; k++) cc = (cc & 1u) ? (0xEDB88320u ^ (cc >> 1)) : (cc >> 1); tab[i] = cc; } ti = true; }
        std::vector<uint8_t> buf(1 << 20);
        FILE * fc = iscopy ? fopen(orig_path.c_str(), "rb") : f; if (!fc) fc = f;   /* 스테이트 헤더 CRC 는 원본 기준(사본이 바뀌어도 같은 값) */
        fseeko64(fc, 0, SEEK_SET);
        for (;;) { size_t n = fread(&buf[0], 1, buf.size(), fc); if (!n) break; for (size_t i = 0; i < n; i++) d.crc32 = tab[(d.crc32 ^ buf[i]) & 0xFFu] ^ (d.crc32 >> 8); }
        d.crc32 ^= 0xFFFFFFFFu; if (fc != f) fclose(fc); fseeko64(f, 0, SEEK_SET);
        if (replace && idx >= 0 && idx < 4 && g_disk_ix[idx] >= 0 && g_disk_ix[idx] < (int)g_disks.size()) g_disks[g_disk_ix[idx]] = d; else { if (idx >= 0 && idx < 4) g_disk_ix[idx] = (int)g_disks.size(); g_disks.push_back(d); }
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

static void gh_bda_dump(const char * tag) {
    if (!g_bda) return;
    char p[1024]; snprintf(p, sizeof(p), "%s\\bda_%s.bin", g_outdir.c_str(), tag);
    FILE * f = fopen(p, "wb"); if (!f) { gh_log("bda dump: cannot open %s", p); return; }
    for (unsigned a = 0; a < 0x600; a++) { unsigned char b = mem_readb((PhysPt)a); fwrite(&b, 1, 1, f); }
    fclose(f); gh_log("bda dump: %s (0x000-0x5FF)", p);
}

/* t36cd: 내장 DOS 부팅 — sdlmain.cpp 의 VM_Boot_DOSBox_Kernel() 중 "dos_kernel_disabled" 분기를 그대로 옮긴 것(마우스·메뉴 항목 제외) */
static bool gh_boot_dos_kernel() {
    void DOS_Startup(Section *); void DRIVES_Startup(Section *); void DOS_InitClock(); void DOS_KeyboardLayout_Startup(Section *); void XMS_Startup(Section *); void EMS_Startup(Section *);
    void SHELL_MessagesInit(); void CONFIGSHELL_Init(); void CONFIGSHELL_Run(); void SHELL_Init(); void AUTOEXEC_Startup(Section *); void MSCDEX_Startup(Section *); void SHELL_Run();
    void update_pc98_function_row(unsigned char setting, bool force_redraw);
    gh_log("DOS boot: 내장 DOS 커널 시작 (cd=%s)", g_prof.cd.c_str());
    { extern uint8_t MOUSE_IRQ; MOUSE_IRQ = g_mirq12 ? 12 : 13; }
    Init_MemHandles();
    DispatchVMEvent(VM_EVENT_DOS_BOOT);
    dos_kernel_disabled = false;
    DOS_Startup(NULL);
    DRIVES_Startup(NULL);
    if (IS_PC98_ARCH) update_pc98_function_row(1, false);
    DispatchVMEvent(VM_EVENT_DOS_INIT_KERNEL_READY);
    DOS_InitClock();
    DOS_KeyboardLayout_Startup(NULL);
    XMS_Startup(NULL);
    EMS_Startup(NULL);
    SHELL_MessagesInit();
    CONFIGSHELL_Init();
    CONFIGSHELL_Run();
    DispatchVMEvent(VM_EVENT_DOS_INIT_CONFIG_SYS_DONE);
    SHELL_Init();
    DispatchVMEvent(VM_EVENT_DOS_INIT_SHELL_READY);
    AUTOEXEC_Startup(NULL);
    MSCDEX_Startup(NULL);
    DispatchVMEvent(VM_EVENT_DOS_INIT_AUTOEXEC_BAT_DONE);
    DispatchVMEvent(VM_EVENT_DOS_INIT_AT_PROMPT);
    g_boot_tick = SDL_GetTicks(); if (!g_boot_tick) g_boot_tick = 1; g_pic0 = PIC_Ticks;
    if (!g_headless) gh_apply_settings(g_ui.effective(g_cur_game), true);
    gh_log("DOS boot: 프롬프트 도달 — 셸 실행");
    SHELL_Run();
    gh_log("DOS boot: 셸 종료");
    g_quit = true;
    return true;
}

/* BOOT (PC-98 branch) re-done without the DOS kernel; called from cb_bios_boot__func */
bool VM_Boot_DOSBox_Kernel() {
    if (g_dosmode) return gh_boot_dos_kernel();
    int drive = g_prof.bootletter;
    int di = drive - 'A';
    gh_log("direct boot: drive %c", drive);
    gh_bda_dump(g_boot_n > 1 ? "pre2" : "pre");
    /* PC-98 bus mouse tick IRQ: core sets MOUSE_IRQ=13 in MOUSE_OnReset(), which this host never runs -> it stayed 12 (FM board) */
    { extern uint8_t MOUSE_IRQ; MOUSE_IRQ = g_mirq12 ? 12 : 13; }
    /* t32: INT 18h 감싸기는 BIOS 리셋(포트 F0h, 약 2초) 뒤에 한다 — t31 은 리셋이 콜백 영역을 다시 써서 감싼 것이 사라졌다 */
    /* official DOSBox-X boots from inside its DOS kernel; the XMS driver has already taken all extended memory (BDA 0401h = 0), so games see no extended RAM */
    if (g_ext0) mem_writeb(0x401, 0x00);   /* t28: no longer needed - root cause was MOUSE_IRQ (t27); kept only as --ext0 test option */
    if (di < 0 || di >= MAX_DISK_IMAGES || !imageDiskList[di]) { gh_log("ERROR: no image on boot drive"); gh_human(phr_("부팅할 디스크가 연결되어 있지 않습니다.\n작품 폴더 game.ini 의 disk_a / boot 줄을 확인해 주세요.")); gh_msgbox(g_errmsg.c_str()); g_quit = true; return true; }
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
        if (img->Read_Sector(0, 0, 1, (uint8_t *)&bootarea) != Int13Status::NoError) { gh_log("ERROR: boot sector read failed"); gh_human(phr_("첫 디스크의 부트 섹터를 읽지 못했습니다.\n이미지 파일이 손상됐거나 지원하지 않는 형식일 수 있습니다.")); gh_msgbox(g_errmsg.c_str()); g_quit = true; return true; }
    }
    if (bootarea.rawdata[0] == 0 && bootarea.rawdata[1] == 0) { gh_log("ERROR: boot sector empty"); gh_human(phr_("첫 디스크의 부트 영역이 비어 있습니다.\n부팅 디스크가 아닌 이미지(데이터 디스크)를 첫 칸에 넣었을 수 있습니다.")); gh_msgbox(g_errmsg.c_str()); g_quit = true; return true; }

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
    reg_edi = g_di0 ? 0x0000u : 0xFFFEu;   /* official log: first IPL INT 1Bh shows DI=FFFE */

    if (g_boot400) { reg_eax = 0x4200; reg_ecx = 0xC000; CALLBACK_RunRealInt(0x18); }   /* 640x400 (old r020 behaviour, --boot400) */
    else { reg_eax = 0x4200; reg_ecx = 0x8000; CALLBACK_RunRealInt(0x18); }   /* 640x200 = official DOSBox-X default (pc98_640x200) */
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
    if (!(g_boot_n > 1 && g_inwin_testfail == "noboot")) {   /* t51a: 시험 주입 noboot = 두 번째 부팅의 「도달 신호」 를 일부러 내지 않는다 */
        g_boot_tick = SDL_GetTicks();
        if (!g_boot_tick) g_boot_tick = 1;
    }
    g_pic0 = PIC_Ticks;
    if (g_boot_n > 1) gh_log("INWIN 부팅 신호 n=%d (전환 뒤 %u ms)", g_boot_n, SDL_GetTicks() - g_inwin_t0);
    if (!g_headless) gh_apply_settings(g_ui.effective(g_cur_game), true);
    gh_log("boot: CS:IP=%04x:%04x SS:SP=%04x:%04x", SegValue(cs), (unsigned)reg_ip, SegValue(ss), (unsigned)reg_sp);
    gh_bda_dump(g_boot_n > 1 ? "post2" : "post");
    gh_log("boot: regs AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X boot400=%d", (unsigned)reg_ax, (unsigned)reg_bx, (unsigned)reg_cx, (unsigned)reg_dx, (unsigned)reg_si, (unsigned)reg_di, (unsigned)reg_bp, (int)g_boot400);
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
/* r011: 저장 순간 화면(원본 640x400, 셰이더·확대 전)을 PNG 바이트로 — 상태 파일 THMB 항목에 넣는다 */
static void gh_png_wr(png_structp pp, png_bytep d, png_size_t n) { std::vector<uint8_t> * o = (std::vector<uint8_t> *)png_get_io_ptr(pp); o->insert(o->end(), d, d + n); }
static void gh_png_fl(png_structp) {}
static bool gh_png_mem(const uint32_t * fb, int w, int h, std::vector<uint8_t> & out) {
    out.clear();
    png_structp pp = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop pi = png_create_info_struct(pp);
    if (setjmp(png_jmpbuf(pp))) { png_destroy_write_struct(&pp, &pi); out.clear(); return false; }
    png_set_write_fn(pp, &out, gh_png_wr, gh_png_fl);
    png_set_IHDR(pp, pi, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(pp, pi);
    std::vector<unsigned char> row(w * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) { uint32_t v = fb[y * w + x]; row[x * 3] = (v >> 16) & 255; row[x * 3 + 1] = (v >> 8) & 255; row[x * 3 + 2] = v & 255; }
        png_write_row(pp, row.data());
    }
    png_write_end(pp, pi);
    png_destroy_write_struct(&pp, &pi);
    return true;
}
static bool gh_do_save(const std::string & path, bool test_extract) {
    gh_state_diag("before-save");
    std::vector<uint8_t> th;
    if (!g_fb.empty()) gh_png_mem(g_fb.data(), g_fbw, g_fbh, th);
    std::string err;
    bool ok = PC98_SaveState_ToFile(path.c_str(), g_disks, err, &th);
    gh_log("SAVE %s -> %s %s (THMB %u bytes)", path.c_str(), ok ? "OK" : "FAIL", err.c_str(), (unsigned)th.size());
    gh_state_diag("after-save");
    if (ok) {
        std::vector<uint8_t> back; std::string e2;
        bool r = PC98_SaveState_ReadThumb(path.c_str(), back, e2);
        gh_log("THUMB readback %s size=%u same=%d %s", r ? "OK" : "FAIL", (unsigned)back.size(), (int)(back == th), e2.c_str());
        if (test_extract && r && !back.empty()) {
            std::string tp = g_outdir + "\\thumb_extracted_" + g_prof.id + ".png";
            FILE * f = fopen(tp.c_str(), "wb"); if (f) { fwrite(back.data(), 1, back.size(), f); fclose(f); }
        }
    }
    return ok;
}
static bool gh_do_load(const std::string & path) {
    gh_state_diag("before-load");
    std::string err;
    bool ok = PC98_SaveState_FromFile(path.c_str(), g_disks, err);
    gh_log("LOAD %s <- %s %s", path.c_str(), ok ? "OK" : "FAIL", err.c_str());
    gh_state_diag("after-load");
    g_next_key = (unsigned)g_prof.keys.size();   /* 자동 키 중단: 저장 시점 이후는 상태 그대로 */
    g_pic0 = PIC_Ticks;
    return ok;
}
static std::string gh_slot_path(void) {
    plat::mkdir((g_savedir).c_str());
    std::string d = g_savedir + "\\" + g_prof.id; plat::mkdir(d.c_str());
    return d + "\\slot01.g98s";
}
/* ===================================================================== r012: 설정 적용·배속·키보드·화면 게시·게임 중 메뉴·부팅 연출 */
static std::string gh_slot_n(int gi, int slot /*0..7*/) {
    char b[700]; snprintf(b, sizeof b, "%s\\%s\\slot%02d.g98s", g_savedir.c_str(), g_ui.games[gi].id.c_str(), slot + 1); return b;
}
static void gh_publish(const uint32_t * px, int w, int h) {
    if (g_headless || !g_use_gl || !g_fb_mtx) return;
    SDL_LockMutex(g_fb_mtx);
    g_pub.assign(px, px + (size_t)w * h); g_pub_w = w; g_pub_h = h; g_pub_new = true; gh_ovl_off();
    SDL_UnlockMutex(g_fb_mtx);
}
/* t35 (#270): 볼륨 재배정. 믹서 채널 「PC-98」(FM 보드 전체)은 1.0 고정, BGM 슬라이더 = FM·SSG·리듬 부품 볼륨, SE 슬라이더 = ADPCM·PCM86 부품 볼륨 + 스피커 계열 채널.
   부품 볼륨은 코어 np2 사운드 setter(0~128, 기본 128 = 슬라이더 8/8). 「FM」 채널은 코어에 없어 예전 BGM 슬라이더는 무효였다. */
extern "C" { void opngen_setvol(unsigned vol); void psggen_setvol(unsigned vol); void rhythm_setvol(unsigned vol); void rhythm_update(void * rhy);
              void adpcm_setvol(unsigned vol); void adpcm_update(void * ad); void pcm86gen_setvol(unsigned vol); extern char rhythm[]; extern char adpcm[]; }
static void gh_apply_volume(const ui::Settings & s) {
    static const char * const se_names[] = { "SPKR", "PCSPEAKER", "PCM", "SB", NULL };
    static int last_b = -1, last_s = -1;
    MixerChannel * m = MIXER_FindChannel("PC-98"); if (m) m->SetVolume(1.0f, 1.0f);
    for (int i = 0; se_names[i]; i++) { MixerChannel * d = MIXER_FindChannel(se_names[i]); if (d) { float v = s.se_gain(); d->SetVolume(v, v); } }
    unsigned nb = (unsigned)(s.bgm_gain() * 128.0f + 0.5f), ns = (unsigned)(s.se_gain() * 128.0f + 0.5f);
    opngen_setvol(nb); psggen_setvol(nb); rhythm_setvol(nb); rhythm_update(rhythm);
    adpcm_setvol(ns); adpcm_update(adpcm); pcm86gen_setvol(ns);
    if (s.bgm != last_b || s.se != last_s) { last_b = s.bgm; last_s = s.se; gh_log("VOLUME parts: bgm=%d -> fm/ssg/rhythm=%u | se=%d -> adpcm/pcm86=%u + spk | PC-98 ch=%p(1.0) SPKR=%p", s.bgm, nb, s.se, ns, (void *)m, (void *)MIXER_FindChannel("SPKR")); }
}
static void gh_apply_settings(const ui::Settings & s, bool vol) {
    g_bosd_on = s.bosd != 0; kbov::g_btn_on = kbov::g_cli_btn >= 0 ? kbov::g_cli_btn != 0 : s.kbbtn != 0; kbov::g_ov_on = kbov::g_cli_ov >= 0 ? kbov::g_cli_ov != 0 : s.ovbtn != 0; g_kbshader = g_cli_kbsh >= 0 ? g_cli_kbsh : (s.kbshader != 0 ? 1 : 0);
    g_fit = g_menu43 ? (int)FIT_43 : s.ratio; { bool fxany = s.scan > 0 || s.mask > 0 || s.bleed > 0 || s.glow > 0 || s.curve > 0; g_shader = fxany ? SH_CRT : (s.shader == 2 ? SH_NEAREST : SH_SHARP); g_nearv = (s.shader == 2) ? 1.0f : 0.0f;   /* t48c: 효과가 하나라도 켜져 있으면 효과 셰이더(밑 그림은 uNear), 아니면 sharp/nearest 셰이더 */
      static int lfx = -1; int kfx = (s.scan << 0) | (s.mask << 4) | (s.bleed << 8) | (s.glow << 12) | (s.curve << 16) | ((s.shader == 2 ? 1 : 0) << 20); if (kfx != lfx) { lfx = kfx; gh_log("FX filter=%s scan=%d mask=%d rgb=%d glow=%d curve=%d -> shader=%d", s.shader == 2 ? "nearest" : "sharp", s.scan, s.mask, s.bleed, s.glow, s.curve, (int)g_shader); } }
    { static int lk = -1; if (lk != (int)g_kbshader) { lk = (int)g_kbshader; gh_log("KBSHADER %d (0=오버레이 평면 합성, 1=셰이더 적용) cli=%d", (int)g_kbshader, g_cli_kbsh); } }
    g_curve = s.curve_f(); g_scanv = s.scan > 0 ? 2.0f + (float)s.scan : 0.0f; g_maskv = s.mask_f(); g_bleedv = s.bleed_f(); g_glowv = s.glow_f();
    if (vol && g_boot_tick) gh_apply_volume(s);
}
/* ---- 배속 */
static bool g_boost_hold = false, g_boost_tog = false;
static void gh_boost_update(void) {
    const ui::Settings s = g_ui.effective(g_cur_game);
    bool want = !g_menu_open && ((g_boost_hold && s.bmode != 1) || (g_boost_tog && s.bmode != 0));
    if (want == g_boost_on) return;
    g_boost_on = want;
    emulator_speed = want ? 100u * (unsigned)s.boost_x() : 100u;
    { int bs = g_bsound_override >= 0 ? g_bsound_override : s.bsound;   /* r017: 0 끔(무음) · 1 빠르게 · 2 음높이 유지 */
      if (want && bs != 0) { gensei_ba_calls = gensei_ba_under = gensei_ba_drops = 0; gensei_ba_sumsq = 0; gensei_ba_n = 0; gensei_ba_xq = s.boost_x() * 100; gensei_ba_gain = s.bs_gain256(); gensei_ba_mode = bs; try { MENU_mute(false); } catch (...) {} }
      else { gensei_ba_mode = 0; try { MENU_mute(want); } catch (...) { gh_log("WARN: MENU_mute threw"); } }
      if (want) gh_log("BOOSTSOUND mode=%d x=%d gain=%d/256", bs, s.boost_x(), s.bs_gain256()); else gh_log("BOOSTSOUND off (calls=%u under=%u drops=%u rho=%.2f rms=%.1f)", gensei_ba_calls, gensei_ba_under, gensei_ba_drops, gensei_ba_rho, gensei_ba_n ? sqrt(gensei_ba_sumsq / (double)gensei_ba_n) : 0.0); }
    g_boost_xv = s.boost_x(); g_bosd_on = s.bosd != 0; kbov::g_btn_on = kbov::g_cli_btn >= 0 ? kbov::g_cli_btn != 0 : s.kbbtn != 0; kbov::g_ov_on = kbov::g_cli_ov >= 0 ? kbov::g_cli_ov != 0 : s.ovbtn != 0; g_kbshader = g_cli_kbsh >= 0 ? g_cli_kbsh : (s.kbshader != 0 ? 1 : 0);
    gh_log("BOOST %s x%d wall=%u game=%u", want ? "on" : "off", s.boost_x(), g_boot_tick ? SDL_GetTicks() - g_boot_tick : 0u, (unsigned)(PIC_Ticks - g_pic0));
}
/* ---- 키보드 → PC-98 */
static KBD_KEYS gh_sdlkey(SDL_Keycode k) {
    static const KBD_KEYS L[26] = { KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g, KBD_h, KBD_i, KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o, KBD_p, KBD_q, KBD_r, KBD_s, KBD_t, KBD_u, KBD_v, KBD_w, KBD_x, KBD_y, KBD_z };
    static const KBD_KEYS N[10] = { KBD_0, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7, KBD_8, KBD_9 };
    static const KBD_KEYS F[9] = { KBD_f1, KBD_f2, KBD_f3, KBD_f4, KBD_f5, KBD_f6, KBD_f7, KBD_f8, KBD_f9 };
    if (k >= SDLK_a && k <= SDLK_z) return L[k - SDLK_a];
    if (k >= SDLK_0 && k <= SDLK_9) return N[k - SDLK_0];
    if (k >= SDLK_F1 && k <= SDLK_F9) return F[k - SDLK_F1];
    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return KBD_enter; case SDLK_ESCAPE: return KBD_esc; case SDLK_BACKSPACE: return KBD_backspace; case SDLK_TAB: return KBD_tab; case SDLK_SPACE: return KBD_space;
    case SDLK_UP: return KBD_up; case SDLK_DOWN: return KBD_down; case SDLK_LEFT: return KBD_left; case SDLK_RIGHT: return KBD_right;
    case SDLK_INSERT: return KBD_insert; case SDLK_DELETE: return KBD_delete; case SDLK_HOME: return KBD_home; case SDLK_END: return KBD_end; case SDLK_PAGEUP: return KBD_pageup; case SDLK_PAGEDOWN: return KBD_pagedown;
    case SDLK_CAPSLOCK: return KBD_capslock; case SDLK_LSHIFT: return KBD_leftshift; case SDLK_RSHIFT: return KBD_rightshift; case SDLK_LCTRL: return KBD_leftctrl; case SDLK_RCTRL: return KBD_rightctrl;
    case SDLK_MINUS: return KBD_minus; case SDLK_EQUALS: return KBD_equals; case SDLK_LEFTBRACKET: return KBD_leftbracket; case SDLK_RIGHTBRACKET: return KBD_rightbracket;
    case SDLK_SEMICOLON: return KBD_semicolon; case SDLK_QUOTE: return KBD_quote; case SDLK_COMMA: return KBD_comma; case SDLK_PERIOD: return KBD_period; case SDLK_SLASH: return KBD_slash;
    case SDLK_BACKSLASH: return KBD_backslash; case SDLK_BACKQUOTE: return KBD_grave;
    case SDLK_KP_0: return KBD_kp0; case SDLK_KP_1: return KBD_kp1; case SDLK_KP_2: return KBD_kp2; case SDLK_KP_3: return KBD_kp3; case SDLK_KP_4: return KBD_kp4; case SDLK_KP_5: return KBD_kp5;
    case SDLK_KP_6: return KBD_kp6; case SDLK_KP_7: return KBD_kp7; case SDLK_KP_8: return KBD_kp8; case SDLK_KP_9: return KBD_kp9;
    default: return KBD_NONE;
    }
}
/* ---- t46x: 화면 키보드(경로 B) 호스트 글루 — SDL 입력층은 이 블록뿐이다(안드로이드는 이 블록만 교체). 키 표 = gensei_kbtable.h, 상태·그리기 = gensei_kbov.h */
extern bool pc98_force_ibm_layout; extern bool pc98_caps(void); extern bool pc98_kana(void);
static unsigned g_kbsent_total = 0, g_kbsent_text = 0;   /* t48b (#629 3): kbov 가 코어로 보낸 키 수 · 그중 텍스트 입력 화면(S_OSK) 중에 보낸 수 */
static bool g_ob_held = false, g_ob_long = false; static unsigned g_ob_t0 = 0;   /* t48b (#629 2): 패드 ○ 길게 누르기 */
static bool gh_kb_send(int k, bool d) {   /* 코어로 보냄. 부팅 전·never_map 키는 보내지 않는다(기존 gh_game_event 와 같은 조건) */
    KBD_KEYS kk = (KBD_KEYS)k;
    if (!g_boot_tick || kk == KBD_NONE) return false;
    if ((int)kk < 256 && gensei_never[(int)kk]) return false;
    KEYBOARD_AddKey(kk, d); g_kbsent_total++; if (g_ui.scr == ui::S_OSK) g_kbsent_text++; return true;
}
static bool gh_kb_ibm(void) { return pc98_force_ibm_layout; }
static bool gh_kb_caps(void) { return pc98_caps(); }
static bool gh_kb_kana(void) { return pc98_kana(); }
static void gh_kb_log(const char * s) { gh_log("KBOV %s", s); }
/* t52f (#132): 오버레이 버튼 동작. 빠른 저장·불러오기는 Alt+F5 / Alt+F9 와 같은 슬롯 1, 배속은 ScrollLock 과 같은 토글 */
static void gh_ov_act(int a) {
    if (a == 5) { gh_osd(phr_("길게 누르세요"), 1500); gh_log("OVBTN load 짧게 누름 -> 동작 안 함 (「길게 누르세요」 표시)"); return; }
    if (!g_boot_tick) { gh_log("OVBTN act=%d 무시 (아직 부팅 전)", a); return; }
    if (a == 1) { g_menu_req = true; gh_log("OVBTN menu -> g_menu_req"); }
    else if (a == 2) { std::string p = gh_slot_path(); bool ok = gh_do_save(p, false); gh_log("OVBTN save slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 에 저장했습니다") : phr_("저장에 실패했습니다"), 2000); }
    else if (a == 3) { std::string p = gh_slot_path(); bool ok = gh_do_load(p); gh_log("OVBTN load slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 을 불러왔습니다") : phr_("불러오기에 실패했습니다"), 2000); }
    else if (a == 4) { g_boost_tog = !g_boost_tog; gh_boost_update(); gh_log("OVBTN boost toggle -> tog=%d on=%d", (int)g_boost_tog, (int)g_boost_on); }
}
static bool gh_ov_boostlit(void) { return g_boost_on; }
static unsigned gh_ov_now(void) { return (unsigned)SDL_GetTicks(); }
static bool g_kb_inited = (kbov::g_send = gh_kb_send, kbov::g_ibm_fn = gh_kb_ibm, kbov::g_caps_fn = gh_kb_caps, kbov::g_kana_fn = gh_kb_kana, kbov::g_logfn = gh_kb_log, kbov::g_act = gh_ov_act, kbov::g_boost_fn = gh_ov_boostlit, kbov::g_now = gh_ov_now, true);
static int g_kb_prevcur = -1; static unsigned g_kb_cons = 0, g_kb_pass = 0;
static void gh_kb_cursor(void) {   /* 키보드가 보이는 동안은 커서를 보이게(전체화면 마우스 클릭용). 끄면 원래대로 */
    if (!g_win) return;
    if (kbov::g_vis) { if (g_kb_prevcur < 0) g_kb_prevcur = SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE ? 1 : 0; SDL_ShowCursor(SDL_ENABLE); }
    else if (g_kb_prevcur >= 0) { SDL_ShowCursor(g_kb_prevcur ? SDL_ENABLE : SDL_DISABLE); g_kb_prevcur = -1; }
}
static void gh_kb_toggle(void) { kbov::set_visible(!kbov::g_vis); gh_kb_cursor(); gh_osd(kbov::g_vis ? phr_("화면 키보드 켜짐 (Alt+K 로 끔)") : phr_("화면 키보드 꺼짐"), 1500); }
static bool gh_kb_geom(int & ww, int & wh, int & dw, int & dh, int & rx, int & ry, int & rw, int & rh) {   /* 창 ↔ 화면 표시 사각형: GL 스레드와 같은 gl_rect 로 계산(곡면 왜곡은 반영하지 않음) */
    if (!g_win || g_fbw <= 0 || g_fbh <= 0) return false;
    SDL_GetWindowSize(g_win, &ww, &wh); SDL_GL_GetDrawableSize(g_win, &dw, &dh);
    if (ww <= 0 || wh <= 0 || dw <= 0 || dh <= 0) return false;
    gl_rect(dw, dh, g_fbw, g_fbh, (int)g_fit, rx, ry, rw, rh); return rw > 0 && rh > 0;
}
static bool gh_kb_curved(void) { return (int)g_shader == (int)SH_CRT && (int)g_fit != (int)FIT_INT && g_curve > 0.0f && g_kbshader != 0; }   /* GL 스레드와 같은 조건: crt 셰이더 + 곡면 > 0 (정수배는 nearest 로 바뀌어 곡면 없음) */
static void gh_kb_warp(double & u, double & v) {   /* 화면 uv(0..1) → 프레임 uv. pc98_crt_t42.glsl 의 warp() 와 같은 식 */
    double c = (double)g_curve, px = u * 2.0 - 1.0, py = v * 2.0 - 1.0; double nx = px * (1.0 + py * py * c), ny = py * (1.0 + px * px * c * 1.33); u = nx * 0.5 + 0.5; v = ny * 0.5 + 0.5;
}
static bool gh_kb_win2virt(int mx, int my, int & vx, int & vy) {
    int ww, wh, dw, dh, rx, ry, rw, rh; if (!gh_kb_geom(ww, wh, dw, dh, rx, ry, rw, rh)) return false;
    double u = ((double)mx * dw / ww - rx) / rw, v = ((double)my * dh / wh - ry) / rh;
    if (gh_kb_curved()) gh_kb_warp(u, v);   /* 곡면: 화면 위치 → 셰이더가 읽는 프레임 위치(오버레이도 같은 프레임에 그려져 같이 휜다) */
    vx = (int)(u * 640.0 + (u < 0 ? -0.5 : 0.5)); vy = (int)(v * 400.0 + (v < 0 ? -0.5 : 0.5)); return true;
}
static bool gh_menu_win2virt(int mx, int my, int & vx, int & vy) {   /* t48b: 메뉴 프레임(640x400, 창 맞춤 gl_rect) 위의 창 좌표 → 가상 좌표. 곡면이면 셰이더 warp 와 같은 식 */
    if (!g_win) return false; int ww = 0, wh = 0, dw = 0, dh = 0, rx, ry, rw, rh; SDL_GetWindowSize(g_win, &ww, &wh); SDL_GL_GetDrawableSize(g_win, &dw, &dh);
    if (ww <= 0 || wh <= 0 || dw <= 0 || dh <= 0) return false;
    gl_rect(dw, dh, 640, 400, (int)g_fit, rx, ry, rw, rh); if (rw <= 0 || rh <= 0) return false;
    double u = ((double)mx * dw / ww - rx) / rw, v = ((double)my * dh / wh - ry) / rh;
    if ((int)g_shader == (int)SH_CRT && (int)g_fit != (int)FIT_INT && g_curve > 0.0f) gh_kb_warp(u, v);
    vx = (int)(u * 640.0 + (u < 0 ? -0.5 : 0.5)); vy = (int)(v * 400.0 + (v < 0 ? -0.5 : 0.5)); return true;
}
static bool gh_kb_virt2win(int vx, int vy, int & wx, int & wy) {   /* 시험 전용 역변환: 곡면이면 warp 의 역을 고정점 반복으로 푼다 */
    int ww, wh, dw, dh, rx, ry, rw, rh; if (!gh_kb_geom(ww, wh, dw, dh, rx, ry, rw, rh)) return false;
    double qu = vx / 640.0, qv = vy / 400.0, u = qu, v = qv;
    if (gh_kb_curved()) { for (int it = 0; it < 40; it++) { double a = u, b = v; gh_kb_warp(a, b); u -= (a - qu); v -= (b - qv); } }
    wx = (int)((rx + u * rw) * ww / dw + 0.5); wy = (int)((ry + v * rh) * wh / dh + 0.5); return true;
}
/* 마우스 이벤트 → 오버레이. 숨김 상태(+버튼 꺼짐)면 0건 소비. 보임 상태에서도 패널·버튼 영역 밖은 false(게임으로 그대로 감 — 마우스 게임도 같은 규칙, 대장방 판정 #1169 5⒝) */
static bool gh_kbd_event(const SDL_Event & e) {
    if (g_headless || !g_win || !g_use_gl || !g_boot_tick || g_menu_open || (g_loading && !g_noload)) return false;
    int ph, mx, my;
    if (e.type == SDL_MOUSEBUTTONDOWN) { ph = 0; mx = e.button.x; my = e.button.y; }
    else if (e.type == SDL_MOUSEBUTTONUP) { ph = 1; mx = e.button.x; my = e.button.y; }
    else if (e.type == SDL_MOUSEMOTION) { ph = 2; mx = e.motion.x; my = e.motion.y; }
    else return false;
    kbov::wake();   /* t52f: 마우스가 움직이거나 터치가 있으면 버튼 줄을 몇 초 진하게 */
    if (!kbov::active()) return false;
    if (g_relmouse && !kbov::g_vis && !kbov::any_owned()) return false;   /* 마우스 가둠 중(키보드 숨김)에는 창 좌표를 믿을 수 없다 → 버튼 사용 불가, Alt+K·메뉴로 켠다 */
    int vx = -1000, vy = -1000; if (!gh_kb_win2virt(mx, my, vx, vy) && !kbov::any_owned()) return false;
    int btn = (e.type == SDL_MOUSEMOTION) ? SDL_BUTTON_LEFT : e.button.button;
    if (btn != SDL_BUTTON_LEFT) { bool in = kbov::in_ui(vx, vy); if (in && ph != 2) { g_kb_cons++; return true; } return false; }
    bool c = kbov::pointer(0, ph, vx, vy);
    if (ph != 2) { if (c) g_kb_cons++; else g_kb_pass++; }
    return c;
}
/* 시험 전용(--kbshow/--kbhide/--kbclick/--kbbtn): SDL 마우스 이벤트를 창 좌표로 밀어 넣어 hit-test·표시 사각형 변환까지 통과시킨다(키 함수를 직접 부르지 않음) */
static unsigned g_kbt_show_ms = 0, g_kbt_hide_ms = 0, g_kbt_click_ms = 0; static std::vector<std::string> g_kbt_seq;
static void gh_parse_kbclick(const char * a) {   /* "<시작초>:라벨,라벨,..." — 라벨은 키 표의 label, 같은 이름이 여럿이면 라벨#n(0부터), BTN = 토글 버튼, OUT = 영역 밖 */
    g_kbt_click_ms = (unsigned)(atof(a) * 1000.0); const char * p = strchr(a, ':'); if (!p) return; p++;
    std::string cur; for (const char * q = p;; q++) { if (*q == ',' || !*q) { if (!cur.empty()) g_kbt_seq.push_back(cur); cur.clear(); if (!*q) break; } else cur += *q; }
}
static void gh_kbtest_tick(unsigned since) {
    static bool shown = false, hidden = false; static size_t ci = 0; static unsigned nextat = 0; static int phase = 0;
    if (g_kbt_show_ms && !shown && since >= g_kbt_show_ms) { shown = true; kbov::set_visible(true); gh_kb_cursor(); }
    if (g_kbt_hide_ms && !hidden && since >= g_kbt_hide_ms) { hidden = true; kbov::set_visible(false); gh_kb_cursor(); }
    if (ci >= g_kbt_seq.size()) return;
    if (nextat == 0) nextat = g_kbt_click_ms ? g_kbt_click_ms : 1;
    if (since < nextat) return;
    std::string lb = g_kbt_seq[ci]; bool hold = false; if (!lb.empty() && lb[lb.size() - 1] == '!') { hold = true; lb.erase(lb.size() - 1); } int vx = 320, vy = 100;   /* 라벨 끝 ! = 누른 채로 둠(뗌 없음) */
    bool lng = false; int ovs = -1; if (lb.size() >= 3 && lb[0] == 'O' && lb[1] == 'V' && lb[2] >= '1' && lb[2] <= '4') { ovs = lb[2] - '0'; lng = lb.size() > 3 && lb[3] == 'L'; }   /* t52f: OV1 메뉴 · OV2 저장 · OV3 불러오기 · OV4 배속, 끝 L = 0.9초 누름 */
    if (ovs >= 0) { vx = kbov::sx(ovs) + kbov::BW / 2; vy = kbov::BY + kbov::BH / 2; }
    else if (lb == "BTN") { vx = kbov::BX + kbov::BW / 2; vy = kbov::BY + kbov::BH / 2; }
    else if (lb != "OUT") { std::string nm = lb; int nth = 0; size_t h = nm.find('#'); if (h != std::string::npos) { nth = atoi(nm.c_str() + h + 1); nm = nm.substr(0, h); }
        int ix = kbov::find_label(nm.c_str(), nth); if (ix < 0 || !kbov::key_center(ix, vx, vy)) { gh_log("KBTEST 라벨 없음: %s", lb.c_str()); ci++; return; } }
    int wx = 0, wy = 0; if (!gh_kb_virt2win(vx, vy, wx, wy)) { gh_log("KBTEST 창 좌표 변환 실패(%s)", lb.c_str()); ci++; return; }
    SDL_Event se; memset(&se, 0, sizeof se);
    se.type = phase == 0 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; se.button.windowID = SDL_GetWindowID(g_win); se.button.button = SDL_BUTTON_LEFT; se.button.state = phase == 0 ? SDL_PRESSED : SDL_RELEASED; se.button.clicks = 1; se.button.x = wx; se.button.y = wy;
    SDL_PushEvent(&se);
    gh_log("KBTEST %s %s at %u ms win=(%d,%d) virt=(%d,%d) 소비=%u 통과=%u", lb.c_str(), phase == 0 ? "누름" : "뗌", since, wx, wy, vx, vy, g_kb_cons, g_kb_pass);
    if (phase == 0) { if (hold) { ci++; nextat = since + 300; } else { phase = 1; nextat = since + (lng ? 900 : 80); } } else { phase = 0; ci++; nextat = since + 300; }
}
/* 게임 중 이벤트(메뉴가 닫혀 있을 때). true = 소비 */
static bool gh_game_event(const SDL_Event & e) {
    switch (e.type) {
    case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP: {
        bool d = e.type == SDL_CONTROLLERBUTTONDOWN;
        if (e.cbutton.button == SDL_CONTROLLER_BUTTON_Y) { if (d && g_boot_tick) g_menu_req = true; return true; }
        if (e.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK) { if (d) { g_boost_tog = !g_boost_tog; GPLOG("L3 boost toggle -> %d", (int)g_boost_tog); gh_boost_update(); } return true; }
        return false; }
    case SDL_CONTROLLERAXISMOTION: {
        /* r015: 트리거/오른쪽 스틱 축 이벤트를 로그에 남긴다(값이 4096 단위로 바뀔 때만). 배속 R2 임계값: 켜짐 > 16000, 꺼짐 < 8000 (SDL 트리거 범위 0..32767) */
        { static int lastbin[6] = { -99, -99, -99, -99, -99, -99 }; int ax = e.caxis.axis; if (ax >= 2 && ax < 6 && ax != 0) { int bin = e.caxis.value / 4096; if (bin != lastbin[ax]) { lastbin[ax] = bin; GPLOG("AXIS %d(%s) value=%d", ax, SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)ax), (int)e.caxis.value); } } }
        if (e.caxis.axis != SDL_CONTROLLER_AXIS_TRIGGERRIGHT) return false;
        static bool on = false;
        if (!on && e.caxis.value > 16000) { on = true; g_boost_hold = true; GPLOG("R2 boost hold ON (value=%d > 16000)", (int)e.caxis.value); gh_log("R2 hold ON v=%d", (int)e.caxis.value); gh_boost_update(); }
        else if (on && e.caxis.value < 8000) { on = false; g_boost_hold = false; GPLOG("R2 boost hold OFF (value=%d < 8000)", (int)e.caxis.value); gh_log("R2 hold OFF v=%d", (int)e.caxis.value); gh_boost_update(); }
        return true; }
    case SDL_KEYDOWN: case SDL_KEYUP: {
        bool d = e.type == SDL_KEYDOWN; SDL_Keycode k = e.key.keysym.sym;
        if (e.key.repeat) return true;
        if (k == SDLK_F12) { if (d && g_boot_tick) g_menu_req = true; return true; }
        if (k == SDLK_PRINTSCREEN) { if (d && g_boot_tick) g_scr_req = true; return true; }
        if (k == SDLK_F11) { if ((e.key.keysym.mod & KMOD_ALT) || (SDL_GetModState() & KMOD_ALT)) { if (d) { g_boost_tog = !g_boost_tog; gh_boost_update(); } return true; }   /* t52d (#129): Alt+F11 = 배속 토글(ScrollLock 없는 키보드용) */
            g_boost_hold = d; gh_boost_update(); return true; }
        if (k == SDLK_SCROLLLOCK) { if (d) { g_boost_tog = !g_boost_tog; gh_boost_update(); } return true; }
        if (k == SDLK_F10) return true;
        KBD_KEYS kk = gh_sdlkey(k);
        if (kk != KBD_NONE && g_boot_tick && !((int)kk < 256 && gensei_never[(int)kk])) KEYBOARD_AddKey(kk, d);
        return true; }
    case SDL_MOUSEMOTION: {   /* r020 (#249-2): 호스트 마우스 → 코어 PC-98 버스마우스. 창 픽셀 이동량을 640 폭 기준으로 환산 */
        if (!(g_mouse_game && g_boot_tick && !g_menu_open)) { static int sk = 0; if (e.type == SDL_MOUSEMOTION && (sk++ % 200) == 0) gh_log("HOST MOUSEFWD skip mouse_game=%d boot=%d menu=%d x=%d y=%d", (int)g_mouse_game, (int)(g_boot_tick != 0), (int)g_menu_open, e.motion.x, e.motion.y); return false; }   /* t48c (#639 ③) */
        int ww = 640, wh = 400; if (g_win) SDL_GetWindowSize(g_win, &ww, &wh); float sc = 640.0f / (float)(ww > 0 ? ww : 640);
        { float fdx = (float)e.motion.xrel * sc, fdy = (float)e.motion.yrel * sc; gensei_vmouse_move_rel(fdx, fdy);   /* t48c (#639 ③): 코어로 넘긴 이동량을 20번마다(처음 한 번 포함) 한 줄로 */
      static float mfx = 0, mfy = 0; static int mfn = 0; mfx += fdx; mfy += fdy; mfn++;
      if (mfn == 1 || mfn % 20 == 0) { gh_log("HOST MOUSEFWD dx=%.1f dy=%.1f x=%d y=%d btn=%u n=%d", (double)mfx, (double)mfy, e.motion.x, e.motion.y, (unsigned)SDL_GetMouseState(NULL, NULL), mfn); mfx = 0; mfy = 0; } return true; } }
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
        if (!(g_mouse_game && g_boot_tick && !g_menu_open)) { static int sk = 0; if (e.type == SDL_MOUSEMOTION && (sk++ % 200) == 0) gh_log("HOST MOUSEFWD skip mouse_game=%d boot=%d menu=%d x=%d y=%d", (int)g_mouse_game, (int)(g_boot_tick != 0), (int)g_menu_open, e.motion.x, e.motion.y); return false; }   /* t48c (#639 ③) */
        int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
        if (b >= 0) { gensei_vmouse_button(b, e.type == SDL_MOUSEBUTTONDOWN); gh_log("HOST MOUSEFWD dx=0.0 dy=0.0 x=%d y=%d btn=%u button=%d down=%d", e.button.x, e.button.y, (unsigned)SDL_GetMouseState(NULL, NULL), b, e.type == SDL_MOUSEBUTTONDOWN ? 1 : 0); } return true; }
    default: return false;
    }
}
/* ---- 슬롯 정보·THMB */
static bool gh_png_decode(const std::vector<uint8_t> & png, std::vector<uint32_t> & out, int & w, int & h) {
    png_image im; memset(&im, 0, sizeof im); im.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&im, png.data(), png.size())) return false;
    im.format = PNG_FORMAT_BGRA; w = (int)im.width; h = (int)im.height; out.assign((size_t)w * h, 0);
    bool ok = png_image_finish_read(&im, NULL, out.data(), 0, NULL) != 0; png_image_free(&im); return ok;
}
static void gh_refresh_game_slots(int gi) {
    ui::UiGame & G = g_ui.games[gi]; G.lastslot = -1; G.thmb.clear(); unsigned long long best = 0;
    for (int i = 0; i < 8; i++) {
        ui::SlotInfo & s = G.slot[i]; s = ui::SlotInfo();
        std::string p = gh_slot_n(gi, i); plat::FileInfo fa;
        if (!plat::stat_file(p, fa)) continue;
        s.used = true; s.bytes = (int)fa.size_lo;
        plat::LocalTime st = plat::local_from_filetime(fa.mtime_ft);
        char b[32]; snprintf(b, sizeof b, "%02d-%02d %02d:%02d", st.month, st.day, st.hour, st.minute); s.stamp = b;
        unsigned long long t = fa.mtime_ft;
        if (t >= best) { best = t; G.lastslot = i; }
        std::vector<uint8_t> png; std::string err;
        if (PC98_SaveState_ReadThumb(p.c_str(), png, err) && !png.empty()) gh_png_decode(png, s.th, s.tw_, s.th_);
    }
    if (G.lastslot >= 0) { G.thmb = G.slot[G.lastslot].th; G.tw_ = G.slot[G.lastslot].tw_; G.th_ = G.slot[G.lastslot].th_; }
}
/* t48e (#646): 키 이름 → 키. 옛 이름은 gh_keyname 이 먼저 받아 지금과 똑같이 동작하고(rollup/pageup/rolldown/pagedown 포함), 거기에 없는 이름만 kbtable 라벨 표(pn::lookup)로 찾는다 */
static KBD_KEYS gh_keyname_pad(const std::string & n) {
    KBD_KEYS k = gh_keyname(n); if (k != KBD_NONE) return k;
    return pn::lookup(n, gh_kb_ibm());
}
static const char * const GH_PADBTN[7] = { "btn_b", "btn_a", "btn_x", "btn_start", "btn_leftshoulder", "btn_rightshoulder", "btn_back" };
static const int GH_PADIDX[7] = { PB_EAST, PB_SOUTH, PB_WEST, PB_START, PB_L1, PB_R1, PB_BACK };
static bool gh_pad_parse(const std::string & v, GPadBind & b) {   /* none / mouse_left / mouse_right / 키 이름 */
    std::string s = v; for (auto & c : s) c = (char)tolower((unsigned char)c);
    if (s == "none") { b.type = PBT_NONE; b.key = KBD_NONE; return true; }
    if (s == "mouse_left") { b.type = PBT_ML; b.key = KBD_NONE; return true; }
    if (s == "mouse_right") { b.type = PBT_MR; b.key = KBD_NONE; return true; }
    KBD_KEYS k = gh_keyname_pad(s); if (k == KBD_NONE) return false; b.type = PBT_KEY; b.key = k; return true;
}
/* 프로필의 패드 표·never_map 을 입력 층에 적용 (#237: never_map 은 패드·단축키에 절대 배정 금지)
 * t48e: 순서 = 기본값(서양식이면 동·남 맞바꿈) < 전역 settings.ini btn_* < 프로필 옛 o/x/sq < 프로필 btn_* (이 작품 user.ini [pad] 가 가장 늦게 얹혀 이긴다).
 * refresh = true 면 (게임 도중 조작 설정을 바꿨을 때) 이 작품 user.ini [pad] 의 btn_* 를 파일에서 다시 읽는다 */
static void gh_apply_profile_input(const GProfile & p, bool refresh = false) {
    memset(gensei_never, 0, sizeof(gensei_never));
    std::vector<std::string> nm_all;   /* t48e3 (#662): never_map 은 ',' 와 '+' 둘 다 구분자 (예: XFER+ROLLUP+ROLLDOWN) */
    for (auto & part : prof::split_list(p.never_map)) for (auto & sub : prof::split_list(part, '+')) nm_all.push_back(sub);
    for (auto & nm : nm_all) { KBD_KEYS k = gh_keyname_pad(nm); if (k != KBD_NONE && (int)k < 256) { gensei_never[(int)k] = true; gh_log("never_map: %s -> key %d", nm.c_str(), (int)k); } else gh_log("never_map: %s -> (알 수 없는 이름, 무시)", nm.c_str()); }
    if (!refresh) gensei_western = (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size() ? g_ui.effective(g_cur_game) : g_ui.g).pad == 1;
    gensei_pb_defaults();
    std::map<std::string, std::string> pad = p.pad;
    if (refresh && g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) {
        for (auto it = pad.begin(); it != pad.end();) { if (it->first.compare(0, 4, "btn_") == 0) it = pad.erase(it); else ++it; }
        std::map<std::string, std::string> m; prof::get_pad_keys(g_ui.games[g_cur_game].dir, m); for (auto & kv : m) pad[kv.first] = kv.second;
    }
    /* 1) 전역 btn_* */
    for (int i = 0; i < 7; i++) { auto it = g_ui.gpad.find(GH_PADBTN[i]); if (it != g_ui.gpad.end() && !it->second.empty()) { GPadBind b; if (gh_pad_parse(it->second, b)) gensei_pb[GH_PADIDX[i]] = b; else gh_log("pad: 전역 %s=%s 알 수 없는 이름, 무시", GH_PADBTN[i], it->second.c_str()); } }
    /* 2) 프로필 옛 이름 o/x/sq — 옛 gh_keyname 으로 지금과 똑같이 */
    auto get = [&](const char * k) -> std::string { auto it = pad.find(k); return it == pad.end() ? std::string() : it->second; };
    std::string o = get("o"), x = get("x"), sq = get("sq");
    /* t48e3 (#662): 옛 o=/x=/sq= 값이 기본값(space/esc/caps)과 같으면 「기본 따름」 — 결정 방식(일본식/서양식)과 전역 btn_* 을 따른다. 기본과 다른 옛 값만 지금처럼 우선 */
    { auto lc = [](std::string s) { for (auto & c : s) c = (char)tolower((unsigned char)c); return s; };
      if (lc(o) == "space") { gh_log("pad: 옛 o=%s = 기본값 → 기본 따름", o.c_str()); o.clear(); }
      if (lc(x) == "esc") { gh_log("pad: 옛 x=%s = 기본값 → 기본 따름", x.c_str()); x.clear(); }
      if (lc(sq) == "caps") { gh_log("pad: 옛 sq=%s = 기본값 → 기본 따름", sq.c_str()); sq.clear(); } }
    if (o == "mouse_left") { gensei_padmouse = true; gh_log("pad: mouse mode (o=mouse_left)"); }
    else if (!o.empty()) { KBD_KEYS k = gh_keyname(o); if (k != KBD_NONE) { gensei_pb[PB_EAST].type = PBT_KEY; gensei_pb[PB_EAST].key = k; } }
    if (!x.empty() && x != "mouse_right") { KBD_KEYS k = gh_keyname(x); if (k != KBD_NONE) { gensei_pb[PB_SOUTH].type = PBT_KEY; gensei_pb[PB_SOUTH].key = k; } }
    if (!sq.empty()) { KBD_KEYS k = gh_keyname(sq); if (k != KBD_NONE) { gensei_pb[PB_WEST].type = PBT_KEY; gensei_pb[PB_WEST].key = k; } }
    /* 3) 프로필 btn_* (옛 이름 위에 얹는다) */
    for (int i = 0; i < 7; i++) { std::string v = get(GH_PADBTN[i]); if (v.empty()) continue; GPadBind b; if (gh_pad_parse(v, b)) gensei_pb[GH_PADIDX[i]] = b; else gh_log("pad: 프로필 %s=%s 알 수 없는 이름, 무시", GH_PADBTN[i], v.c_str()); }
    /* never_map 에 걸린 키는 옛 규칙대로 되돌린다: 동 → Space(없으면 Esc) · 남 → Esc · 서 → 없음. 그 밖의 버튼은 없음 */
    for (int i = 0; i < PB_N; i++) if (gensei_pb[i].type == PBT_KEY && (int)gensei_pb[i].key < 256 && gensei_never[(int)gensei_pb[i].key]) {
        if (i == PB_EAST) gensei_pb[i].key = KBD_space; else if (i == PB_SOUTH) gensei_pb[i].key = KBD_esc; else { gensei_pb[i].type = PBT_NONE; gensei_pb[i].key = KBD_NONE; } }
    auto nm = [](const GPadBind & b) -> std::string { if (b.type == PBT_NONE) return "none"; if (b.type == PBT_ML) return "mouse_left"; if (b.type == PBT_MR) return "mouse_right"; return std::to_string((int)b.key); };
    gh_log("pad: western=%d east=%s south=%s west=%s start=%s l1=%s r1=%s select=%s%s", gensei_western ? 1 : 0, nm(gensei_pb[PB_EAST]).c_str(), nm(gensei_pb[PB_SOUTH]).c_str(), nm(gensei_pb[PB_WEST]).c_str(), nm(gensei_pb[PB_START]).c_str(), nm(gensei_pb[PB_L1]).c_str(), nm(gensei_pb[PB_R1]).c_str(), nm(gensei_pb[PB_BACK]).c_str(), refresh ? " (갱신)" : "");
    GPLOG("BINDS western=%d east=%s south=%s west=%s start=%s l1=%s r1=%s select=%s", gensei_western ? 1 : 0, nm(gensei_pb[PB_EAST]).c_str(), nm(gensei_pb[PB_SOUTH]).c_str(), nm(gensei_pb[PB_WEST]).c_str(), nm(gensei_pb[PB_START]).c_str(), nm(gensei_pb[PB_L1]).c_str(), nm(gensei_pb[PB_R1]).c_str(), nm(gensei_pb[PB_BACK]).c_str());
}
static std::map<std::string, std::vector<std::string>> g_fdfiles;   /* 작품 id -> 디스크 교체 후보 파일(UiGame::fds 와 같은 순서) */

/* ------------------------------------------------------------------ t39: 실행 방법 UI 가 쓰는 환경(파일·저장) */
static std::map<std::string, std::string> g_vlabel;     /* 변형 폴더 id -> 목록에 보일 이름 */
static std::string g_uikeys_file;                        /* t39: --uikeys <파일> (시험 전용) */
static bool g_titletest = false, g_titlelegacy = false;  /* t39: --titletest / --titlelegacy */
static bool g_helplog = false;   /* t49a: --helplog (조작 설명 폭 실측 로그) */
#define KH_WT_SUFFIX " - " KH_FULLNAME   /* t49a (#811): 창 제목 접미(항상 전부 남김, 30바이트) */
#define KH_WT_TITLEMAX (113 - (int)(sizeof(KH_WT_SUFFIX) - 1))                /* 제목 쪽 상한 = 옛 합계 113 - 접미 30 */
static std::string gh_window_title(const ui::UiGame & tg, bool legacy) {   /* 작품 제목만 줄임, 한글 3바이트 경계 유지 */
    std::string tt = tg.title;
    if (!legacy && tt.size() > KH_WT_TITLEMAX) { if (!tg.short_t.empty()) tt = tg.short_t; if (tt.size() > KH_WT_TITLEMAX) { size_t k = KH_WT_TITLEMAX - 3; while (k > 0 && ((unsigned char)tt[k] & 0xC0) == 0x80) k--; tt = tt.substr(0, k) + "..."; } }
    return tt + KH_WT_SUFFIX;
}
static bool gh_make_blank_hdi(const std::string & path, int mb) {
    const uint32_t ss = 256, sec = 33, sur = 8; uint64_t cyl = ((uint64_t)(mb < 1 ? 1 : mb) * 1048576ull) / (ss * sec * sur); if (cyl < 1) cyl = 1; if (cyl > 4096) cyl = 4096;
    uint32_t sz = (uint32_t)(ss * sec * sur * cyl);
    FILE * f = fopen(path.c_str(), "wb"); if (!f) return false;
    uint32_t h[8] = { 0, 0, 0x1000, sz, ss, sec, sur, (uint32_t)cyl }; fwrite(h, 1, sizeof h, f);
    static unsigned char z[65536]; size_t pad = 0x1000 - sizeof h; bool ok = fwrite(z, 1, pad, f) == pad;
    for (uint32_t left = sz; ok && left;) { size_t k = left > sizeof z ? sizeof z : left; ok = fwrite(z, 1, k, f) == k; left -= (uint32_t)k; }
    fclose(f); if (!ok) plat::remove_file(path.c_str()); return ok;
}
static void gh_ui_env_setup(void) {
    ui::Env & E = g_ui.env; E.sep = '\\';
    E.list_dir = [](const std::string & p) {
        std::vector<ui::FileItem> r; std::string pa = aw::sys_from_ini(p); std::vector<std::string> fs, ds; aw::list_dir(pa, fs, &ds);
        std::sort(ds.begin(), ds.end()); std::sort(fs.begin(), fs.end());
        for (auto & d : ds) { ui::FileItem f; f.name = aw::acp_to_u8(d); f.dir = true; r.push_back(f); }
        for (auto & n : fs) { ui::FileItem f; f.name = aw::acp_to_u8(n); uint64_t sz = 0, mt = 0; if (aw::file_stat(aw::join(pa, n), sz, mt)) f.size = (long long)sz; r.push_back(f); }
        return r; };
    E.roots = []() { std::vector<std::string> r; DWORD m = GetLogicalDrives(); for (int i = 0; i < 26; i++) if (m & (1u << i)) r.push_back(std::string(1, (char)('A' + i)) + ":\\"); return r; };
    E.has_img = [](int g, const std::string & n) {
        if (n.empty() || g < 0 || g >= (int)g_ui.games.size()) return false; std::string pa = aw::sys_from_ini(n);
        bool abs = (pa.size() > 1 && pa[1] == ':') || pa[0] == '\\' || pa[0] == '/'; std::string full = abs ? pa : aw::join(g_ui.imgdir_of(g), pa);
        uint64_t sz = 0, mt = 0; return aw::file_stat(full, sz, mt); };
    E.save = [](int g, const std::string & name, const ent::Kv & kv, const std::string & setdef) { return prof::save_entry(g_ui.games[g].dir, name, kv, setdef); };
    E.del = [](int g, const std::string & name, bool reset) { return prof::delete_entry(g_ui.games[g].dir, name, reset); };
    E.setkey = [](int g, const std::string & k, const std::string & v) { return prof::set_game_key(g_ui.games[g].dir, k, v); };
    E.pad_get = [](int g, std::map<std::string, std::string> & m) { m.clear(); if (g >= 0 && g < (int)g_ui.games.size()) prof::get_pad_keys(g_ui.games[g].dir, m); };   /* t48e (#646): 이 작품 user.ini [pad] btn_* */
    E.pad_set = [](int g, const std::string & k, const std::string & v) { if (g < 0 || g >= (int)g_ui.games.size()) return false; bool ok = prof::set_pad_key(g_ui.games[g].dir, k, v); gh_log("pad_set: %s [pad] %s=%s -> %s", g_ui.games[g].dir.c_str(), k.c_str(), v.c_str(), ok ? "OK" : "FAIL"); return ok; };
    E.padkind = []() { return gensei_padkind(); };
    E.padkeys = []() { return pn::picker_names(); };
    E.hdd_make = [](int g, int mb, std::string & out) {
        std::string dir = g_ui.imgdir_of(g), id = g_ui.games[g].id; for (auto & ch : id) if (!(isalnum((unsigned char)ch) || ch == '_' || ch == '-')) ch = '_';
        for (int i = 1; i < 100; i++) { std::string nm = id + "_hdd_" + std::to_string(i) + ".hdi"; uint64_t sz, mt; if (aw::file_stat(aw::join(dir, nm), sz, mt)) continue;
            if (!gh_make_blank_hdi(aw::join(dir, nm), mb)) return false; out = nm; gh_log("hdd_make: %s %dMB", nm.c_str(), mb); return true; }
        return false; };
    E.save_test = [](int g, const std::string & name, const ent::Kv & kv) {
        (void)g; std::string td = g_exedir + "\\logs\\_test"; plat::mkdir((g_exedir + "\\logs").c_str()); plat::mkdir(td.c_str());
        plat::remove_file((td + "\\user.ini").c_str()); return prof::save_entry(td, name, kv, name); };
    E.resolved = [](int g, ent::Kv & out) {
        out.clear(); if (g < 0 || g >= (int)g_ui.games.size()) return false;
        std::string keep = prof::g_entry_req; prof::g_entry_req.clear(); GProfile p; bool ok = gh_profile(g_ui.games[g].id, p); prof::g_entry_req = keep; if (!ok) return false;
        static const char * K[4] = { "disk_a", "disk_b", "disk_c", "disk_d" };
        for (int i = 0; i < 4; i++) if (p.disk[i]) out[K[i]] = aw::acp_to_u8(p.disk[i]);
        out["boot"] = std::string(1, p.bootletter); { auto it = p.kv.find("cd"); if (it != p.kv.end() && !it->second.empty()) out["cd"] = it->second; }
        for (const char * k : { "mouse", "font", "font_kind", "bootkeys", "autoexec" }) { auto it = p.kv.find(k); if (it != p.kv.end() && !it->second.empty()) out[k] = it->second; }
        return true; };
    E.reload = [](int g) { ulist::reload_one(g_ui.games, g, &g_vlabel); int keep = g_ui.cur; g_ui.rebuild_vis(keep); };
    E.values = [](int g, std::vector<ui::ValRow> & out) {   /* t48a (#614 ③): 키별 최종값·출처·덮인 값(기본 항목 기준) */
        out.clear(); if (g < 0 || g >= (int)g_ui.games.size()) return false;
        std::string keep = prof::g_entry_req; prof::g_entry_req.clear(); GProfile p; bool ok = gh_profile(g_ui.games[g].id, p); prof::g_entry_req = keep; if (!ok) return false;
        static const char * const PRI[] = { "title", "disk_a", "disk_b", "disk_c", "disk_d", "boot", "swap", "cd", "keys", "bootkeys", "autoexec", "cycles", "font", "font_kind", "lang_eff", "lang", "mouse", "sound", "joystick", "entry_name" };
        std::vector<std::string> ks; for (auto & kv : p.kv) if (kv.first.compare(0, 6, "__val.") == 0) ks.push_back(kv.first.substr(6));
        auto pr = [&](const std::string & k) -> int { for (size_t i = 0; i < sizeof PRI / sizeof PRI[0]; i++) if (k == PRI[i]) return (int)i; return 1000; };
        std::stable_sort(ks.begin(), ks.end(), [&](const std::string & a, const std::string & b) { int x = pr(a), y = pr(b); return x != y ? x < y : a < b; });
        for (auto & k : ks) { ui::ValRow r; r.key = k; r.val = p.kv["__val." + k]; auto s = p.kv.find("__src." + k); r.src = s == p.kv.end() ? std::string() : s->second; auto c = p.kv.find("__cov." + k); r.cov = c == p.kv.end() ? std::string() : c->second; if (r.val.empty()) r.val = phr_("(비어 있음)"); out.push_back(r); }
        { ui::ValRow r; r.key = phr_("(실행 막힘 사유)"); r.val = g_ui.games[g].block.empty() ? std::string(phr_("없음 (실행 가능)")) : g_ui.games[g].block; r.src = phr_("계산"); out.push_back(r); }
        return true; };
    E.font_group = [](const std::string & nm) { for (auto & f : g_ui.fonts) if (f.name == nm) return f.group; return std::string(); };
}
static void gh_delete_slot(int gi, int slot) { plat::remove_file(gh_slot_n(gi, slot).c_str()); gh_refresh_game_slots(gi); }   /* S3 retro-test token */
static void gh_spawn_test(const std::string & id, const std::string & entry, int secs) {
    std::string exe = plat::exe_path(); std::string td = g_exedir + "\\logs\\_test";
    std::string cmd = "\"" + exe + "\" \"" + id + "\" --entry \"" + aw::u8_to_acp(entry) + "\" --userini \"" + td + "\\user.ini\" --sec " + std::to_string(secs > 0 ? secs : 20) + " --bkosd 1 --savedir \"" + td + "\\saves\" --windowed --noboot --out \"" + td + "\"";
    unsigned long perr = 0;
    if (plat::spawn(exe, cmd, g_exedir, &perr)) { gh_log("test run: %s", cmd.c_str()); }
    else gh_log("test run FAIL err=%lu", (unsigned long)perr);
}
static void gh_launch_note(ui::Ui & U, const ui::Act & a) {   /* 실행 요청의 항목 이름을 프로필 결정에 넘기고 최근 실행 시각을 적는다 */
    prof::g_entry_req = (a.t == ui::A_START) ? a.entry : (a.entry.empty() ? U.sel_entry : a.entry);
    if (a.game >= 0 && a.game < (int)U.games.size()) U.games[a.game].last_run = (long long)time(NULL);
}
static void gh_rebuild_fds(const GProfile & p, ui::UiGame & g) {   /* 디스크 교체 후보 = 지금 실행하는 항목의 배치 */
    std::string demo = p.kv.count("demo") ? p.kv.at("demo") : std::string(); std::vector<std::string> files; auto addf = [&](const char * f) { if (!f || !*f) return; for (auto & x : files) if (x == f) return; files.push_back(f); };
    for (int i = 0; i < 2; i++) addf(p.disk[i]); for (auto & s : p.swap) addf(s.c_str()); addf(demo.c_str());
    auto flb = [&](const std::string & f) -> std::string { std::string n = f; size_t dd = n.find_last_of('.'); if (dd != std::string::npos && n.size() - dd <= 5) n = n.substr(0, dd); return n; };
    g.fds.clear(); g.cur_fd[0].clear(); g.cur_fd[1].clear(); for (auto & f : files) g.fds.push_back(flb(f));
    for (int i = 0; i < 2; i++) if (p.disk[i]) g.cur_fd[i] = flb(p.disk[i]);
    g_fdfiles[p.id] = files;
}
static void gh_ui_init_games(void) {
    struct ErGuard { std::string k; ErGuard() : k(prof::g_entry_req) { prof::g_entry_req.clear(); } ~ErGuard() { prof::g_entry_req = k; } } er_guard;   /* t48a (#615/#616): 직전 실행의 항목 이름(전역 한 칸)이 모든 작품의 목록 판정에 끼지 않게 한다 */
    g_ui.games.clear(); g_vlabel.clear(); gh_ui_env_setup();
    {   /* t39: 글꼴 폴더 자동 등록 */
        std::string fd = aw::dir_of(gh_res_font_path("kyuhachi_font")); std::vector<std::string> ff, fs2; aw::list_dir(fd, ff, &fs2);
        std::vector<fontreg::Info> inf = fontreg::scan(fd, ff, "\\"); g_ui.fonts.clear();
        for (auto & i : inf) { ui::UiFont u; u.name = i.name; u.group = i.group; u.path = i.path; u.why = i.why; u.ok = i.ok; g_ui.fonts.push_back(u); }
    }
    struct E { int order; ui::UiGame g; };
    std::vector<E> es;
    std::vector<std::string> files, subs; gh_list_dir(g_exedir + "\\games", files, &subs);
    std::sort(subs.begin(), subs.end());
    auto lbl = [](const GProfile & p, int i) -> std::string {
        static const char L[4] = { 'A', 'B', 'C', 'D' };
        std::string k = std::string("label_") + (char)('a' + i); auto it = p.kv.find(k);
        if (it != p.kv.end()) return std::string(1, L[i]) + ": " + it->second;
        if (!p.disk[i]) return std::string(); std::string n = p.disk[i]; size_t d = n.find_last_of('.'); if (d != std::string::npos && n.size() - d <= 5) n = n.substr(0, d);
        if (n.size() > 34) n = n.substr(0, 31) + "...";
        return std::string(1, L[i]) + ": " + n;
    };
    for (auto & d : subs) {
        GProfile p; if (!gh_profile(d, p)) continue;
        E e; e.order = 100; auto o = p.kv.find("order"); if (o != p.kv.end()) e.order = atoi(o->second.c_str());
        e.g.id = d; e.g.title = p.title; e.g.src = p.src; e.g.ov = p.ov;
        e.g.title_db = p.title;   /* t49a (#809): 사용자 지정 표시 제목 — user.ini [game] title_user= (비면 DB·game.ini 제목) */
        { prof::Ini u_; std::string ud = g_exedir + "\\games\\" + d; if (prof::ini_load(ud + "/user.ini", u_) && u_.count("game") && u_["game"].count("title_user")) e.g.title_user = u_["game"]["title_user"]; if (!e.g.title_user.empty()) e.g.title = e.g.title_user; }
        {   /* t39: 언어·short·변형·글꼴 묶음 */
            ulist::fill(e.g, g_exedir + "\\games\\" + d, p.imgdir, p.kv, p.title); e.g.need_install = p.install_required;
            if (!e.g.title_user.empty()) e.g.short_t.clear();   /* t49a: 바꾼 제목은 자동 줄임을 새로 만든다 */
            { auto vl = p.kv.find("variant_label"); if (vl != p.kv.end() && !vl->second.empty()) g_vlabel[d] = vl->second; }
            std::string fn = p.font; { size_t sl = fn.find_last_of("\\/"); if (sl != std::string::npos) fn = fn.substr(sl + 1); if (fn.size() > 4 && (fn.compare(fn.size() - 4, 4, ".bmp") == 0 || fn.compare(fn.size() - 4, 4, ".BMP") == 0)) fn.resize(fn.size() - 4); }
            if (fn.empty()) { auto fk = p.kv.find("font_kind"); e.g.fgroup = (fk != p.kv.end() && fk->second == "jp") ? "jis" : (e.g.lang == "jp" ? "jis" : "ks2350"); }
            else { e.g.fgroup = "custom"; e.g.gfont = fn; for (auto & f : g_ui.fonts) if (f.ok && f.name == fn && p.font.find_first_of("\\/:.") == std::string::npos) e.g.fgroup = f.group; }
        }
        { auto kvs = [&](const char * k) -> std::string { auto it = p.kv.find(k); return it == p.kv.end() ? std::string() : it->second; };
          e.g.company = kvs("company"); e.g.year = kvs("year"); e.g.genre = kvs("genre"); e.g.patch = kvs("patch"); e.g.devices = kvs("devices");
          if (e.g.devices.empty()) { std::string dv; auto add = [&](const char * s) { if (!dv.empty()) dv += " · "; dv += s; }; std::string mo = kvs("mouse"), so = kvs("sound"), jo = kvs("joystick");
              if (so.find("86") != std::string::npos) add(phr_("86 보드")); if (so.find("rhythm") != std::string::npos) add(phr_("리듬 WAV"));
              if (mo == "required") { std::map<std::string, std::string> kk = p.kv; kk["__id"] = d; add(aw::mouse_released(kk) ? phr_("마우스 지원(시험)") : "마우스 필수"); } else if (mo == "pointer") add(phr_("마우스 지원")); else if (mo == "optional") add(phr_("마우스 선택"));
              if (jo.find("opn") != std::string::npos) add(phr_("OPN 조이스틱")); e.g.devices = dv; }
          std::string demo = kvs("demo"); e.g.has_demo = !demo.empty();
          {   /* r017: 판정 등급 · 출처 · 실행 불가 사유(목록을 만들 때마다 계산 — 지원이 늘면 다시 등록하지 않아도 풀린다) */
              bool autoreg = kvs("auto") == "1", curated = p.src == "DB" || kvs("curated") == "1"; e.g.autoreg = autoreg;
              std::string gr = kvs("grade"); e.g.grade = gr.empty() ? '-' : gr[0]; e.g.judge = kvs("judge"); e.g.jshort = kvs("jshort"); e.g.metasrc = kvs("metasrc");
              e.g.orig = kvs("orig"); e.g.dbhit = kvs("dbhit");
              {   /* r017 이전 문구(내부 용어)가 들어 있는 ini 는 쉬운 말로 바꿔 보여 준다 */
                  auto rep = [](std::string & s, const char * a, const char * b) { size_t k; while ((k = s.find(a)) != std::string::npos) s.replace(k, strlen(a), b); };
                  rep(e.g.jshort, "A 큐레이션 DB 일치", "확인됨 (DB 일치)"); rep(e.g.judge, "A 큐레이션 DB(해시 일치): ", "원판과 같은 파일 — "); rep(e.g.judge, "큐레이션 DB", "DB");
                  rep(e.g.jshort, "B 변형 · ", "추정 · "); rep(e.g.jshort, "C 후보 · ", "후보 · "); }
              if (gr.empty() && p.src == "DB") { e.g.grade = 'A'; e.g.dbhit = p.title; e.g.judge = phr_("원판과 같은 파일 — ") + p.title + phr_(" (DB 일치)"); e.g.jshort = phr_("확인됨 (DB 일치)"); e.g.metasrc = e.g.metasrc.empty() ? std::string("DB") : e.g.metasrc; }
              else if (gr.empty() && !autoreg) {   /* game.ini 로 직접 정한 작품: DB 일치 여부도 같이 보인다 */
                  if (!e.g.dbhit.empty()) { e.g.grade = 'A'; e.g.jshort = phr_("직접 지정 · DB 일치"); e.g.judge = phr_("game.ini 로 직접 지정 — 파일은 DB의 '") + e.g.dbhit + phr_("' 와 일치"); }
                  else { e.g.jshort = phr_("직접 지정"); e.g.judge = phr_("game.ini 로 직접 지정 (DB와 일치하는 파일 없음)"); } }
              prof::Profile q; q.install_required = p.install_required; q.reason = p.reason; q.disk.assign(4, std::string()); for (int i = 0; i < 4; i++) if (p.disk[i]) q.disk[i] = p.disk[i];
              std::map<std::string, std::string> kv = p.kv; if (curated) kv["curated"] = "1"; kv["__id"] = d; if (!autoreg) kv.erase("mouse");   /* 직접 정한 작품은 마우스 때문에 막지 않는다 */
              e.g.block = aw::block_reason(q, kv); if (e.g.block.empty() && !kvs("block").empty() && kvs("block").find("마우스 필수") == std::string::npos) e.g.block = kvs("block");   /* t48a (#615): 마우스 필수 문구는 막는 사유가 아님 */
              gh_log("LISTBLK id=%s src=%s auto=%d curated=%d mouse=%s entry_req='%s' block='%s' boot=%s/%s", d.c_str(), p.src.c_str(), autoreg ? 1 : 0, curated ? 1 : 0, kvs("mouse").c_str(), prof::g_entry_req.c_str(), e.g.block.c_str(), kvs("__val.boot").c_str(), kvs("__src.boot").c_str()); }
          auto flb = [&](const std::string & f) -> std::string {
              if (p.disk[0] && f == p.disk[0] && !kvs("label_a").empty()) return kvs("label_a"); if (p.disk[1] && f == p.disk[1] && !kvs("label_b").empty()) return kvs("label_b");
              if (!demo.empty() && f == demo) return kvs("label_demo").empty() ? std::string(phr_("오프닝(데모) 디스크")) : kvs("label_demo");
              std::string n = f; size_t dd = n.find_last_of('.'); if (dd != std::string::npos && n.size() - dd <= 5) n = n.substr(0, dd); return n; };
          std::vector<std::string> files; auto addf = [&](const char * f) { if (!f || !*f) return; for (auto & x : files) if (x == f) return; files.push_back(f); };
          for (int i = 0; i < 2; i++) addf(p.disk[i]); for (auto & s : p.swap) addf(s.c_str()); addf(demo.c_str());
          for (auto & f : files) { e.g.fds.push_back(flb(f)); }
          g_fdfiles[d] = files;
          for (int i = 0; i < 2; i++) if (p.disk[i]) e.g.cur_fd[i] = flb(p.disk[i]);
          int nfd = 0; for (auto & f : files) if (f != demo) nfd++; bool hd = p.disk[2] || p.disk[3];
          e.g.disknote = hd ? (nfd ? "HDD + FD " + std::to_string(nfd) + phr_("장") : std::string("HDD")) : (nfd ? "FD " + std::to_string(nfd) + phr_("장") : std::string("-"));
          if (!demo.empty()) e.g.disknote += phr_(" + 오프닝");
          { std::string mo = kvs("more"); if (!mo.empty()) { e.g.more = mo; size_t k; while ((k = mo.find(" | ")) != std::string::npos) mo.replace(k, 3, " / "); e.g.disknote += phr_("  함께 있는 이미지: ") + mo; } } }
        std::vector<std::string> ls; for (int i = 0; i < 4; i++) { std::string l = lbl(p, i); if (!l.empty()) ls.push_back(l); }
        if (ls.size() > 0) e.g.disk[0] = ls[0]; if (ls.size() > 1) e.g.disk[1] = ls[1];
        e.g.boot_note = p.install_required ? std::string(phr_("하드 설치 전용 (설치 단계 필요)")) : std::string(phr_("부팅  ")) + p.bootletter + phr_(" 드라이브");
        if (p.install_required && ls.empty()) e.g.disk[0] = phr_("부팅 가능한 디스크 없음");
        es.push_back(e);
    }
    std::stable_sort(es.begin(), es.end(), [](const E & a, const E & b) { return a.order < b.order; });
    for (auto & e : es) g_ui.games.push_back(e.g);
    ulist::load_all(g_ui.games, &g_vlabel);
    if (g_ui.games.empty()) {
        ui::UiGame g; g.id = ""; g.title = phr_("작품이 없습니다"); g.disk[0] = phr_("games 폴더에 작품 폴더를 넣어 주세요"); g.boot_note = ""; g_ui.games.push_back(g);
        if (!g_uitest_dir.empty()) {   /* 캡처 시험용 표본 */
            g_ui.games.clear(); const char * T[4][2] = { { "hukyou", phr_("환세풍광전") }, { "kaitou", phr_("환세쾌도전") }, { "kitan", phr_("환세희담") }, { "torimono", phr_("환세포물장") } };
            for (int i = 0; i < 4; i++) { ui::UiGame x; x.id = T[i][0]; x.title = T[i][1]; x.disk[0] = std::string("A: ") + T[i][1] + phr_(" 디스크"); x.boot_note = phr_("부팅  A 드라이브"); g_ui.games.push_back(x); }
        }
    }
    g_ui.build = std::string("v" KH_VERSION);   /* t52a (#53): 내부 빌드명은 로그에만 */
    gh_log("games: %d", (int)g_ui.games.size());
}
/* ------------------------------------------------------------------ r017: 작품 추가 · 게임 폴더 자동 등록 */
#include <SDL_syswm.h>
#include <shlobj.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
static void gh_nk_load_once(void) {
    static bool done = false; if (done) return; done = true;
    std::string p = g_nkdb_path.empty() ? g_exedir + "\\gamedb_nk.tsv" : g_nkdb_path;
    if (nk::load(p, g_nk)) { nk::load_sig(g_exedir + "\\gamedb_nk_sig.tsv", g_nk); gh_log("gamedb_nk: %d games, %d sha1 (nk=%s tosec=%s)", (int)g_nk.g.size(), (int)g_nk.bysha.size(), g_nk.ver_nk.c_str(), g_nk.ver_tosec.c_str()); }
    else gh_log("gamedb_nk: none (%s)", p.c_str());
    g_shacache.load(g_exedir + "\\sha_cache.tsv");
}
static HWND gh_hwnd(void) { SDL_SysWMinfo wi; SDL_VERSION(&wi.version); if (g_win && SDL_GetWindowWMInfo(g_win, &wi)) return wi.info.win.window; return NULL; }

/* ---- t52a (#54/#55): 제목표시줄이 윈도우 앱 테마를 따른다 ---------------------------------------------
 * HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme == 1 이면 밝게, 그 밖(값 없음·0·읽기 실패)은 어둡게.
 * DwmSetWindowAttribute 20(DWMWA_USE_IMMERSIVE_DARK_MODE) 을 먼저, 실패하면 19(구 Win10 1809~1903 값). 둘 다 실패하거나 DWM 이 없으면(Win7/8, Win10 1809 미만)
 * 아무것도 하지 않아 기본 제목표시줄이 그대로 남는다. 실행 중 앱 테마가 바뀌면(WM_SETTINGCHANGE "ImmersiveColorSet") 다시 적용.
 * dwmapi.dll 은 LoadLibrary 로 부르므로 링크 목록은 늘지 않는다. */
static int g_tb_last = -1;          /* -1 아직 안 함 · 0 밝게 · 1 어둡게 · -3 이 윈도우에서는 지원 안 됨 */
static unsigned g_tb_tick = 0;
static bool gh_apps_light(void) {
    return plat::apps_use_light_theme();
}
static void gh_titlebar_apply(const char * why, bool force) {
    try {
        HWND hw = gh_hwnd(); if (!hw) return;
        if (g_tb_last == -3 && !force) return;
        int want = gh_apps_light() ? 0 : 1;
        if (!force && want == g_tb_last) return;
        unsigned at = 0; long r = 0; int dr = plat::dwm_dark_titlebar(hw, want != 0, &at, &r);
        if (dr == 1) { if (g_tb_last != -3) gh_log("TITLEBAR %s: DwmSetWindowAttribute 없음 -> 기본 제목표시줄 유지", why); g_tb_last = -3; return; }
        if (dr == 2) { if (g_tb_last != -3) gh_log("TITLEBAR %s: 실패 hr=0x%08lX -> 기본 제목표시줄 유지 (apps_light=%d)", why, (unsigned long)r, 1 - want); g_tb_last = -3; return; }
        bool changed = (want != g_tb_last); g_tb_last = want;
        if (changed) {
            gh_log("TITLEBAR %s: apps_light=%d -> %s 제목표시줄 (attr=%u)", why, 1 - want, want ? "어두운" : "밝은", (unsigned)at);
            if (g_win && !(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP)) plat::redraw_frame(hw);   /* 바로 다시 그리게 */
        }
    } catch (...) { }
}
static void gh_titlebar_event(const SDL_Event * e) {
    try {
        if (e->type == SDL_SYSWMEVENT) {
            const SDL_SysWMmsg * w = e->syswm.msg;
            if (w && w->subsystem == SDL_SYSWM_WINDOWS && w->msg.win.msg == WM_SETTINGCHANGE && w->msg.win.lParam) {
                const char * a = (const char *)w->msg.win.lParam; const wchar_t * u = (const wchar_t *)w->msg.win.lParam;
                if (strcmp(a, "ImmersiveColorSet") == 0 || (a[0] == 'I' && a[1] == 0 && wcscmp(u, L"ImmersiveColorSet") == 0)) gh_titlebar_apply("settingchange", true);
            }
        } else if (e->type == SDL_WINDOWEVENT && g_win && (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e->window.event == SDL_WINDOWEVENT_RESTORED || e->window.event == SDL_WINDOWEVENT_SHOWN)) {
            unsigned now = SDL_GetTicks(); if (now - g_tb_tick > 400u) { g_tb_tick = now; gh_titlebar_apply("window", true); }   /* 전체화면 <-> 창 전환 뒤에도 유지 */
        }
    } catch (...) { }
}

static bool gh_pick_files(std::vector<std::string> & out) {   /* 시스템(ANSI) 경로로 돌려준다 */
    static char buf[1 << 16]; memset(buf, 0, sizeof buf); OPENFILENAMEA o; memset(&o, 0, sizeof o); o.lStructSize = sizeof o; o.hwndOwner = gh_hwnd();
    std::string t = aw::u8_to_acp(phr_("추가할 디스크 이미지를 고르세요 (여러 개 가능)"));
    std::string f1 = aw::u8_to_acp(phr_("디스크 이미지")), f2 = aw::u8_to_acp(phr_("모든 파일"));
    std::string flt = f1 + std::string(1, '\0') + "*.fdi;*.hdi;*.nhd;*.d88;*.d77;*.88d;*.98d;*.nfd;*.hdm;*.dup;*.xdf;*.dsk;*.fim;*.flp;*.img;*.ima;*.thd;*.hdd;*.hdn;*.dim;*.tfd;*.fdd;*.bin;*.zip" + std::string(1, '\0') + f2 + std::string(1, '\0') + "*.*" + std::string(2, '\0');
    o.lpstrFilter = flt.c_str(); o.lpstrFile = buf; o.nMaxFile = sizeof buf; o.lpstrTitle = t.c_str(); o.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&o)) return false;
    std::string first = buf; const char * p = buf + first.size() + 1;
    if (!*p) out.push_back(first); else { while (*p) { out.push_back(aw::join(first, p)); p += strlen(p) + 1; } }
    return !out.empty();
}
static bool gh_pick_folder(const char * title_u8, std::string & out) {
    static bool com = false; if (!com) { com = true; CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); }
    char disp[MAX_PATH]; BROWSEINFOA b; memset(&b, 0, sizeof b); b.hwndOwner = gh_hwnd(); b.pszDisplayName = disp; std::string t = aw::u8_to_acp(title_u8); b.lpszTitle = t.c_str(); b.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST id = SHBrowseForFolderA(&b); if (!id) return false;
    char path[MAX_PATH]; bool ok = SHGetPathFromIDListA(id, path) != 0; CoTaskMemFree(id); if (ok) out = path; return ok;
}
static aw::Ctx gh_aw_ctx(void) {
    gh_db_load_once(); gh_nk_load_once(); aw::Ctx c; c.games_dir = g_exedir + "\\games"; c.on_dir = gh_res_on_work; c.db = &g_db; c.nk = g_nk.loaded ? &g_nk : NULL; c.cache = &g_shacache; { std::vector<std::string> ml; int mv = aw::merge_dups(c, &ml); for (auto & l : ml) gh_log("%s", l.c_str()); if (mv) gh_log("중복 합침 %d건", mv); } aw::index_registered(c); return c;
}
static void gh_reload_games(void) {   /* 목록을 다시 만들고 보던 작품을 id 로 되찾는다 */
    std::string keep = (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) ? g_ui.games[g_cur_game].id : std::string();
    ui::settings_save(g_ui, g_cfgpath);
    g_fdfiles.clear();
    gh_ui_init_games(); ui::settings_load(g_ui, g_cfgpath);
    for (size_t i = 0; i < g_ui.games.size(); i++) if (g_ui.games[i].id == keep) { g_cur_game = (int)i; g_ui.cur = (int)i; g_ui.pick = (int)i; }
    if (!g_headless) for (size_t i = 0; i < g_ui.games.size(); i++) gh_refresh_game_slots((int)i);
}
static aw::Summary gh_add_works(const std::vector<aw::Work> & ws, const char * what) {
    aw::Ctx c = gh_aw_ctx(); aw::Summary S; aw::run_works(c, ws, S); g_shacache.save();
    char h[200]; snprintf(h, sizeof h, "%s: 추가%d 있음%d 건너뜀%d 불가%d", what, S.added, S.exists, S.skipped, S.blocked);
    g_ui.add_log.clear(); g_ui.add_log.push_back(what); g_ui.add_log.push_back(std::string(phr_("추가")) + std::to_string(S.added) + phr_(" 있음") + std::to_string(S.exists) + phr_(" 건너뜀") + std::to_string(S.skipped) + phr_(" 불가") + std::to_string(S.blocked)); for (auto & l : S.lines) g_ui.add_log.push_back(l);
    gh_log("ADDWORK %s", h); for (auto & l : S.lines) gh_log("ADDWORK   %s", l.c_str());
    gh_log("ADDWORK sha1 cache hits=%u miss=%u", g_shacache.hits, g_shacache.miss);
    return S;
}
/* 게임 폴더: 켤 때마다 훑어 새 작품만 자동 등록 */
static int gh_scan_gamefolder(const char * why) {
    if (g_ui.gamefolder.empty()) return 0;
    std::string dir = aw::u8_to_acp(g_ui.gamefolder); if (!aw::dir_exists(dir)) { gh_log("GAMEFOLDER %s: 폴더 없음 (%s)", why, dir.c_str()); if (!g_ui.add_log.empty()) g_ui.add_log.clear(); g_ui.add_log.push_back(phr_("게임 폴더를 찾을 수 없습니다")); return 0; }
    std::vector<aw::Work> ws; aw::collect(dir, 3, ws); aw::Summary S = gh_add_works(ws, why);
    gh_log("GAMEFOLDER %s: %d 후보, 추가 %d", why, (int)ws.size(), S.added);
    if (S.added > 0) gh_reload_games(); return S.added;
}
static void gh_add_action(int arg) {
    ui::Ui & U = g_ui;
    if (arg == 0) { std::vector<std::string> fs; if (!gh_pick_files(fs)) return; std::vector<aw::Work> ws; aw::from_files(fs, ws); aw::Summary S = gh_add_works(ws, phr_("파일 추가")); if (S.added) { gh_reload_games(); U.add_new = S.added_ids; U.add_new_why = S.added_why; U.open_lang(U.add_new, U.add_new_why, ui::S_ADD); } }
    else if (arg == 1) { std::string d; if (!gh_pick_folder(phr_("추가할 작품 폴더를 고르세요"), d)) return; std::vector<aw::Work> ws; aw::collect(d, 3, ws); aw::Summary S = gh_add_works(ws, phr_("폴더 추가")); if (S.added) { gh_reload_games(); U.add_new = S.added_ids; U.add_new_why = S.added_why; U.open_lang(U.add_new, U.add_new_why, ui::S_ADD); } }
    else if (arg == 2) { std::string d; if (!gh_pick_folder(phr_("「게임 폴더」를 고르세요 — 켤 때마다 새 작품을 찾아 등록합니다"), d)) return; U.gamefolder = aw::acp_to_u8(d); ui::settings_save(U, g_cfgpath); gh_scan_gamefolder(phr_("게임 폴더 지정")); }
    else if (arg == 3) { gh_scan_gamefolder(phr_("다시 검색")); if (!U.add_log.empty() && U.gamefolder.empty()) U.add_log.clear(); }
    else if (arg == 4) { U.gamefolder.clear(); ui::settings_save(U, g_cfgpath); U.add_log.clear(); U.add_log.push_back(phr_("게임 폴더를 해제했습니다 (등록된 작품은 그대로입니다)")); }
}
/* t44a (#494): 사용자 「언어」 지정 — user.ini [game] lang=ko|jp (자동 = 키 삭제). 저장 후 목록을 다시 만들고 보던 작품·항목 화면 대상을 id 로 되찾는다 */
static void gh_setlang(int gi, int mode) {
    ui::Ui & U = g_ui; if (gi < 0 || gi >= (int)U.games.size()) return;
    std::string id = U.games[gi].id, eid = (U.entg >= 0 && U.entg < (int)U.games.size()) ? U.games[U.entg].id : std::string();
    const char * v = mode == 1 ? "ko" : mode == 2 ? "jp" : "";
    bool ok = prof::set_game_key(U.games[gi].dir, "lang", v);
    gh_log("SETLANG %s lang=%s ok=%d", id.c_str(), *v ? v : "(auto)", ok ? 1 : 0);
    gh_reload_games();
    if (!eid.empty()) for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == eid) U.entg = (int)i;
    if (!ok) U.msg = phr_("언어를 저장하지 못했습니다.");
}
/* t49a (#809): 표시 제목 저장 — user.ini [game] title_user= (빈 값 = 키 삭제 = DB 제목 복귀). 원본 DB·game.ini 무변경 */
static void gh_settitle(int gi, const std::string & t) {
    ui::Ui & U = g_ui; if (gi < 0 || gi >= (int)U.games.size()) return;
    std::string id = U.games[gi].id, eid = (U.entg >= 0 && U.entg < (int)U.games.size()) ? U.games[U.entg].id : std::string();
    bool ok = prof::set_game_key(U.games[gi].dir, "title_user", t);
    gh_log("SETTITLE %s title_user=%s ok=%d", id.c_str(), t.empty() ? "(없음)" : t.c_str(), ok ? 1 : 0);
    gh_reload_games();
    if (!eid.empty()) for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == eid) U.entg = (int)i;
    if (!ok) U.msg = phr_("표시 제목을 저장하지 못했습니다.");
    else if (!t.empty()) U.msg = phr_("표시 제목을 바꿨습니다. 원본 DB는 그대로입니다.");
}
/* --addwork PATH [--dry] [--report DIR] : 파일/폴더를 등록(또는 시험 판정)하고 판정표(match_table.tsv)를 남긴다 — 헤드리스 시험용 */
static bool g_aw_dry = false;
static bool g_batest = false;
static void gh_cli_addwork(void) {
    std::vector<aw::Work> ws; std::string p = g_addwork_cli;
    if (aw::dir_exists(p)) aw::collect(p, 3, ws); else { std::vector<std::string> v; v.push_back(p); aw::from_files(v, ws); }
    aw::Ctx c = gh_aw_ctx(); aw::Summary S; std::string tsv = phr_("작업\t파일(크기)\t등급\t판정(짧게)\t판정(전체)\tid\t제목\t제작사\t연도\t출처\t실행불가\t비고\n");
    for (auto & w : ws) {
        aw::Result r = aw::register_work(c, w, g_aw_dry);
        if (r.st == aw::Result::ADDED) { S.added++; if (!r.block.empty()) S.blocked++; } else if (r.st == aw::Result::EXISTS) S.exists++; else S.skipped++;
        std::string fl; for (auto & f : w.files) { uint64_t sz = 0, mt = 0; aw::file_stat(aw::join(w.dir, f), sz, mt); if (!fl.empty()) fl += " | "; fl += aw::acp_to_u8(f) + " (" + std::to_string((unsigned long long)sz) + ")"; }
        auto cl = [](std::string s) { for (auto & ch : s) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' '; return s; };
        tsv += cl(aw::pretty(w.name)) + "\t" + cl(fl) + "\t" + (r.grade.empty() ? std::string("-") : r.grade) + "\t" + cl(r.jshort) + "\t" + cl(r.judge) + "\t" + r.id + "\t" + cl(r.title) + "\t" + cl(r.maker) + "\t" + r.year + "\t" + cl(r.metasrc) + "\t" + cl(r.block) + "\t" + cl(r.note) + "\n";
        gh_log("ADDWORK%s %s | %s | %s | %s", g_aw_dry ? "(dry)" : "", aw::pretty(w.name).c_str(), r.grade.c_str(), r.judge.c_str(), r.note.c_str());
    }
    gh_log("ADDWORK summary: works=%d added=%d exists=%d skipped=%d blocked=%d dry=%d", (int)ws.size(), S.added, S.exists, S.skipped, S.blocked, (int)g_aw_dry);
    if (!g_report_dir.empty()) { plat::mkdir(g_report_dir.c_str()); FILE * f = fopen((g_report_dir + "\\match_table.tsv").c_str(), "wb"); if (f) { fwrite(tsv.data(), 1, tsv.size(), f); fclose(f); } }
    g_shacache.save(); if (!g_aw_dry && S.added) gh_reload_games();
}
/* --batest : 「배속 중 소리」 DSP 단독 시험(소리 장치 없음). 440 Hz 사인을 x 배 속도로 먹이고 출력의 음높이·크기·끊김을 잰다.
 * 기대: 빠르게(1) → 440*x Hz · 음높이 유지(2) → 440 Hz */
static void gh_batest(void) {
    const double SR = 44100.0; const int NEED = 441, CALLS = 700; int xs[5] = { 2, 3, 4, 6, 8 }; int bad = 0;
    for (int xi = 0; xi < 5; xi++) for (int mode = 1; mode <= 2; mode++) {
        int x = xs[xi]; ba::Eng * e = new ba::Eng; static int32_t l[8192], r[8192]; std::vector<float> out; out.reserve((size_t)NEED * CALLS); long long n = 0; int under = 0;
        for (int c = 0; c < CALLS; c++) {
            int fed = NEED * x; for (int i = 0; i < fed; i++, n++) { int32_t s = (int32_t)(8000.0 * sin(2.0 * 3.14159265358979323846 * 440.0 * (double)n / SR)); e->feed1(s, s); }
            e->note(fed, NEED, (double)x); int made = e->render(mode, (double)x, NEED, l, r); if (made < NEED) under++;
            for (int i = 0; i < made; i++) out.push_back((float)l[i]);
            for (int i = made; i < NEED; i++) out.push_back(out.empty() ? 0.0f : out.back());
        }
        size_t a = (size_t)SR; size_t cnt = 0; double ss = 0; for (size_t i = a + 1; i < out.size(); i++) { if (out[i - 1] <= 0 && out[i] > 0) cnt++; ss += (double)out[i] * out[i]; }
        double secs = (double)(out.size() - a) / SR, hz = cnt / secs, rms = sqrt(ss / (double)(out.size() - a)); double want = mode == 1 ? 440.0 * x : 440.0;
        bool ok = fabs(hz - want) / want < 0.04 && under <= CALLS / 50 && rms > 1500;
        if (!ok) bad++;
        gh_log("BATEST x=%d mode=%d(%s) freq=%.1fHz expect=%.0fHz rms=%.0f under=%d/%d drops=%u %s", x, mode, mode == 1 ? "빠르게" : "음높이 유지", hz, want, rms, under, CALLS, e->drops, ok ? "OK" : "FAIL");
        delete e;
    }
    gh_log("BATEST done: %s", bad ? "FAIL" : "ALL OK");
}
/* ---- 입력 → UI 키 */
/* t49a (#812): 메뉴 키 매핑 표 — gh_ev2uk 가 읽고 조작 설명(helpline) 번역도 같은 표에서 이름을 만든다(키 이름 한 곳). 설정 노출 없음 */
struct GhKeyRow { ui::UKey uk; const char * sym; const char * name; int n; SDL_Keycode k[3]; };
static const GhKeyRow GH_KEYTAB[] = {
    { ui::UK_UP, "↑", "↑", 1, { SDLK_UP } }, { ui::UK_DOWN, "↓", "↓", 1, { SDLK_DOWN } }, { ui::UK_LEFT, "←", "←", 1, { SDLK_LEFT } }, { ui::UK_RIGHT, "→", "→", 1, { SDLK_RIGHT } },
    { ui::UK_OK, "○", "Enter", 3, { SDLK_RETURN, SDLK_KP_ENTER, SDLK_SPACE } },
    { ui::UK_CANCEL, "×", "Esc", 3, { SDLK_ESCAPE, SDLK_BACKSPACE, SDLK_F12 } },
    { ui::UK_SQ, "□", "S", 1, { SDLK_s } }, { ui::UK_TRI, "△", "D", 1, { SDLK_d } }, { ui::UK_SELECT, "SEL", "Tab", 1, { SDLK_TAB } },
    { ui::UK_L, "L", "Q", 1, { SDLK_q } }, { ui::UK_R, "R", "E", 1, { SDLK_e } },
};
static ui::UKey gh_keytab_lookup(SDL_Keycode sym) { for (auto & r : GH_KEYTAB) for (int i = 0; i < r.n; i++) if (r.k[i] == sym) return r.uk; return ui::UK_NONE; }
static void gh_help_init(void) {   /* 같은 표에서 helpline 번역표를 만든다(첫 키 이름만; 방향키는 그대로) */
    ov::g_helpkeys.clear(); std::string ln, rn;
    for (auto & r : GH_KEYTAB) { if (r.uk == ui::UK_UP || r.uk == ui::UK_DOWN || r.uk == ui::UK_LEFT || r.uk == ui::UK_RIGHT) continue; if (r.uk == ui::UK_L) { ln = r.name; continue; } if (r.uk == ui::UK_R) { rn = r.name; continue; } ov::HelpKey hk; hk.sym = r.sym; hk.name = r.name; ov::g_helpkeys.push_back(hk); if (r.uk == ui::UK_SELECT) { ov::HelpKey h2; h2.sym = "SELECT"; h2.name = r.name; ov::g_helpkeys.push_back(h2); } }
    ov::HelpKey a; a.sym = "L R"; a.name = ln + " " + rn; ov::g_helpkeys.push_back(a); ov::HelpKey b; b.sym = "L/R"; b.name = ln + "/" + rn; ov::g_helpkeys.push_back(b);
}
static ui::UKey gh_ev2uk(const SDL_Event & e, bool * anykey) {
    using namespace ui;
    if (anykey) *anykey = (e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_MOUSEBUTTONDOWN;
    { int nd = -1; if (e.type == SDL_KEYDOWN && !e.key.repeat) nd = 1; else if (e.type == SDL_CONTROLLERBUTTONDOWN) nd = 0; else if (e.type == SDL_CONTROLLERAXISMOTION && (e.caxis.value > 16000 || e.caxis.value < -16000)) nd = 0;   /* t49a (#812): 마지막 입력 장치(마우스 제외) */
      if (nd >= 0 && ov::g_helpdev != nd) { ov::g_helpdev = nd; if (g_helplog) gh_log("HELPDEV 입력 장치 -> %s (clock=game)", nd ? "키보드" : "패드"); } }
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.mod & KMOD_ALT) return UK_NONE;
        if (g_ui.scr == ui::S_OSK && e.key.keysym.sym >= 0x20 && e.key.keysym.sym < 0x7f && (e.key.keysym.sym != SDLK_SPACE || g_ui.osk_hg)) return UK_NONE;   /* t39: 글자 입력 화면에서는 글자 키가 패드 키로 겹치지 않게 (t48b: 한글 키보드에서는 공백도 글자 이벤트로만) */
        if (g_ui.scr == ui::S_OSK && e.key.keysym.sym == SDLK_BACKSPACE) return UK_SQ;   /* t48b: 글자 입력 화면의 백스페이스 = 지움(예전에는 취소로 나가 버렸다) */
        if (g_ui.scr == ui::S_OSK && g_ui.osk_hg && (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER)) return UK_SELECT;   /* t48b: 한글 키보드에서 엔터 = 완료 */
        return gh_keytab_lookup(e.key.keysym.sym);   /* t49a: 표 한 곳(GH_KEYTAB) */
    }
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_START: return UK_OK;
        case SDL_CONTROLLER_BUTTON_B: return gensei_western ? UK_CANCEL : UK_OK;   /* t48e (#651 4): 일본식 = 동쪽(B/○) 결정 · 서양식 = 남쪽(A/×) 결정 */
        case SDL_CONTROLLER_BUTTON_A: return gensei_western ? UK_OK : UK_CANCEL;
        case SDL_CONTROLLER_BUTTON_X: return UK_SQ; case SDL_CONTROLLER_BUTTON_Y: return UK_TRI; case SDL_CONTROLLER_BUTTON_BACK: return UK_SELECT;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return UK_L; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return UK_R;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return UK_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return UK_DOWN; case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return UK_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return UK_RIGHT;
        default: return UK_NONE; }
    }
    if (e.type == SDL_CONTROLLERAXISMOTION && (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)) {
        static bool st[4] = { false, false, false, false };   /* L R U D */
        int ax = e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ? 0 : 2; int v = e.caxis.value;
        bool & neg = st[ax], & pos = st[ax + 1];
        if (!neg && v < -16000) { neg = true; return ax == 0 ? UK_LEFT : UK_UP; } if (neg && v > -8000) neg = false;
        if (!pos && v > 16000) { pos = true; return ax == 0 ? UK_RIGHT : UK_DOWN; } if (pos && v < 8000) pos = false;
    }
    return UK_NONE;
}
/* ---- 부팅 연출(약 1.8초) · 짧은 로딩 · 삐뽀(음량 ≈1/3 × 효과음 설정) */
static SDL_AudioDeviceID g_beep_dev = 0;
static void gh_beep(float se_gain) {
    if (g_headless) return;
    int amp = (int)(3000.0f * se_gain); if (amp <= 0) return;
    SDL_AudioSpec w; SDL_zero(w); w.freq = 44100; w.format = AUDIO_S16SYS; w.channels = 1; w.samples = 1024; w.callback = NULL;
    SDL_AudioSpec h; g_beep_dev = SDL_OpenAudioDevice(NULL, 0, &w, &h, 0); if (!g_beep_dev) { gh_log("beep: open fail %s", SDL_GetError()); return; }
    std::vector<int16_t> pcm; const int R = 44100; const double CLK = 1996800.0;
    auto tone = [&](int div, int ms) { double f = CLK / div; int n = R * ms / 1000; for (int i = 0; i < n; i++) { double ph = fmod(i * f / R, 1.0); pcm.push_back((int16_t)(ph < 0.5 ? amp : -amp)); } };
    auto sil = [&](int ms) { pcm.insert(pcm.end(), (size_t)(R * ms / 1000), (int16_t)0); };
    tone(998, 90); sil(40); tone(1997, 160); sil(100);
    SDL_QueueAudio(g_beep_dev, pcm.data(), (Uint32)(pcm.size() * 2)); SDL_PauseAudioDevice(g_beep_dev, 0);
    gh_log("beep: amp=%d", amp);
}
static void gh_beep_close(void) { if (g_beep_dev) { SDL_CloseAudioDevice(g_beep_dev); g_beep_dev = 0; } }
/* t52d (#129): 시작 로고 — boot_logo_160.txt 1~18줄(160칸 × 4×8 픽셀: # 꽉 참 · + 체크무늬 50% · . 25% 점) + 아래 가운데 KYUHACHI(비트맵 글자). 검정 바탕 흰색 */
static const char * const KH_LOGO[18] = {
    "                                                                                                                                                                ",
    "                                                                                                                                                                ",
    "                                                                                                                                                                ",
    "                                                       ...........                                                  .++..                                       ",
    "                                                       .........                                            ...+++.........+                                    ",
    "                                                     .+........                                     +.......    .......+                                        ",
    "                                                    +++++++++                                      ++++++++    ++++++++                                         ",
    "                                      ##+++++++++++++++++++++++++++++++++++++                     #++++++#   ..+++++++                                          ",
    "                                                  ########.      +########                      .#######+     ########                                          ",
    "                                                +########       ########.                      +#######.     +#######+                                          ",
    "                                              .########.      +########                      +########       +########                                          ",
    "                                             +++++++++      .++++++++.           +         +++++++++        .+++++++++                                          ",
    "                                          ++.......+       +........           +         +........           ...........                                        ",
    "                                       ++++++++++.       +++..+.++         .++        .++++++++.             ++++++++++++.                                      ",
    "                                   .#+++++++++.        +++++++++        ++++.       #+++++++.                 ++++++++++++++                                    ",
    "                              .+#+++++++++.          ++++++++++++++#++++++.     +##+++++.                     .+++++++#++++++##+.                               ",
    "                       . +#########+.               +##################+   ..#####+.                           ###########++++++++###+  .                       ",
    "                 ++######+...                        ...............   +##+..                                   +##..                                           "
};

static void gh_logo_draw(ov::Canvas & c) {
    c.rect(0, 0, 640, 400, ov::K);
    const int y0 = (400 - 197) / 2;
    for (int r = 0; r < 18; r++) {
        const char * ls = KH_LOGO[r];
        for (int q = 0; q < 160; q++) { char ch = ls[q]; if (ch == ' ') continue;
            for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 4; xx++) {
                int px = q * 4 + xx, py = r * 8 + yy; bool on = false;
                if (ch == '#') on = true; else if (ch == '+') on = (((px + py) & 1) == 0); else if (ch == '.') on = (((px & 1) == 0) && ((py & 1) == 0));
                if (on) c.set(px, y0 + py, ov::WH); } }
    }
    static uint32_t tb[64 * 16]; ov::Canvas t = { tb, 64, 16 }; t.rect(0, 0, 64, 16, ov::K); ov::text(t, 0, 0, "KYUHACHI", ov::WH, ov::K);
    const int tx = 320 - (8 * 24 - 8) / 2, ty = y0 + 155;
    for (int i = 0; i < 8; i++) for (int sy = 0; sy < 16; sy++) for (int sx = 0; sx < 8; sx++) {
        if (tb[sy * 64 + i * 8 + sx] == ov::K) continue;
        int dy0 = (sy * 3) / 2, dy1 = ((sy + 1) * 3) / 2; c.rect(tx + i * 24 + sx * 2, ty + dy0, 2, dy1 - dy0, ov::WH); }
}
static void gh_logo_frame(void) { static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; gh_logo_draw(c); gh_publish(buf, 640, 400); }
static void gh_boot_frame(int stage, bool cells) {
    static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; c.rect(0, 0, 640, 400, ov::K);
    if (stage >= 1) ov::text(c, 40, 112, std::string(KH_FULLNAME), ov::WH, ov::K);
    if (stage >= 2) ov::text(c, 40, 144, std::string("Now Loading") + std::string((size_t)(stage - 1 > 3 ? 3 : stage - 1), '.'), ov::WH, ov::K);
    if (cells) ov::fncells_boot(c);   /* t52a (#49) */
    gh_publish(buf, 640, 400);
}
/* true = 건너뜀. 아무 버튼이나 누르면 건너뛴다. */
static bool gh_boot_seq(bool full) {
    if (g_headless || !g_use_gl) return false;
    if (full) {   /* t52d (#129): 시작 로고 1.4초 — 아무 키·클릭이면 부팅 연출 전체를 건너뜀 */
        unsigned tl = SDL_GetTicks(); bool sk = false;
        for (;;) { if (SDL_GetTicks() - tl >= 1400u) break; SDL_Event e; while (SDL_PollEvent(&e)) { bool ak = false; gh_ev2uk(e, &ak); if (e.type == SDL_QUIT) { g_quit = true; sk = true; } if (ak) sk = true; } if (sk) return true; gh_logo_frame(); SDL_Delay(16); }
    }
    unsigned t0 = SDL_GetTicks(); bool beeped = false; bool skipped = false; unsigned total = full ? 1800u : 500u;
    for (;;) {
        unsigned t = SDL_GetTicks() - t0; if (t >= total) break;
        SDL_Event e; while (SDL_PollEvent(&e)) { bool ak = false; gh_ev2uk(e, &ak); if (e.type == SDL_QUIT) { g_quit = true; skipped = true; } if (ak) skipped = true; }
        if (skipped) break;
        if (full) {
            if (t >= 300 && !beeped) { beeped = true; gh_beep(g_ui.effective(g_cur_game).se_gain()); }
            int stage = t < 300 ? 0 : t < 700 ? 1 : t < 1000 ? 2 : t < 1300 ? 3 : 4; gh_boot_frame(stage, t >= 1300);
        } else gh_boot_frame(t < 150 ? 1 : 4, false);
        SDL_Delay(16);
    }
    if (g_beep_dev && skipped) SDL_ClearQueuedAudio(g_beep_dev);
    if (full && beeped && !skipped) SDL_Delay(0);
    gh_beep_close();
    return skipped;
}
static void gh_spawn_menu(void) {
    std::string exe = plat::exe_path();
    std::string cmd = "\"" + exe + "\" --noboot"; unsigned long perr = 0;
    if (plat::spawn(exe, cmd, g_exedir, &perr)) { gh_log("spawn menu OK"); }
    else gh_log("spawn menu FAIL err=%lu", (unsigned long)perr);
}
/* ---- t49a (#845): 실행 순서(단계) — 항목의 next= 사슬. 「처음부터」 는 사슬의 첫 단계부터, 「이어하기」 는 지금 고른(마지막) 단계로.
   다음 단계로는 아무 입력(키·클릭·패드) 또는 일시정지 메뉴 「다음 단계」 — 프로세스를 새로 띄워(--entry <다음>) 디스크 구성을 바꾼다. 게임 안 화면 감지·자동 전환 없음 */
static std::vector<std::string> g_argv_saved; static bool g_id_given = false, g_nostage = false;
static std::string g_stage_entry, g_stage_next; static bool g_stage_go = false;
static const ui::UiEnt * gh_find_ent(int gi, const std::string & name) { if (gi < 0 || gi >= (int)g_ui.games.size() || name.empty()) return NULL; for (auto & e : g_ui.games[gi].ents) if (e.name == name && e.vgi < 0 && !e.pseudo) return &e; return NULL; }
static std::string gh_ent_next(int gi, const std::string & name) {   /* 다음 단계 이름("-" = 기본 실행). 없거나 가리키는 항목이 없으면 빈 문자열 */
    const ui::UiEnt * e = gh_find_ent(gi, name); if (!e) return std::string(); auto it = e->kv.find("next"); if (it == e->kv.end() || it->second.empty() || it->second == "none") return std::string();
    if (it->second == "-" || gh_find_ent(gi, it->second)) return it->second; return std::string(); }
static std::string gh_stage_label(int gi, const std::string & nx) { if (nx == "-") return phr_("게임"); const ui::UiEnt * e = gh_find_ent(gi, nx); return e ? (e->label.empty() ? e->name : e->label) : nx; }
static std::string gh_chain_head(int gi, const std::string & req) {   /* req(고른 단계)로 이어지는 사슬의 맨 앞 단계 이름. 앞선 단계가 없으면 req 그대로 */
    if (gi < 0 || gi >= (int)g_ui.games.size()) return req; const ui::UiGame & g = g_ui.games[gi];
    std::set<std::string> T; if (req.empty() || req == "-") { T.insert("-"); std::string d = g.defent; if (d.empty()) for (auto & e : g.ents) if (!e.pseudo && !e.hidden && e.vgi < 0) { d = e.name; break; } if (!d.empty()) T.insert(d); } else T.insert(req);
    std::string head = req; std::set<std::string> seen;
    for (int depth = 0; depth < 8; depth++) {
        std::string found; for (auto & e : g.ents) { if (e.pseudo || e.vgi >= 0 || e.name.empty() || seen.count(e.name) || T.count(e.name)) continue; std::string nx = gh_ent_next(gi, e.name); if (!nx.empty() && T.count(nx)) { found = e.name; break; } }
        if (found.empty()) break; seen.insert(found); head = found; T.clear(); T.insert(found);
    }
    return head; }
static void gh_chain_start(ui::Ui & U, int gi) {   /* 메뉴에서 「처음부터」(g_resume_slot == 0) 로 시작할 때 사슬의 첫 단계로 바꾼다 */
    if (g_nostage || g_resume_slot != 0) return; std::string h = gh_chain_head(gi, prof::g_entry_req); if (h != prof::g_entry_req) { gh_log("STAGE 처음부터: %s -> 첫 단계 %s", prof::g_entry_req.c_str(), h.c_str()); prof::g_entry_req = h; } (void)U; }
static void gh_stage_init(void) {   /* 게임 시작 때: 지금 단계의 다음 단계가 있으면 안내·메뉴 항목을 켠다 */
    g_stage_next.clear(); g_ui.stage_next_label.clear(); if (g_nostage) return;
    std::string nm = (!prof::g_entry_req.empty() && prof::g_entry_req != "-") ? prof::g_entry_req : std::string(); if (nm.empty()) { auto it = g_prof.kv.find("entry_name"); if (it != g_prof.kv.end()) nm = it->second; }
    g_stage_entry = nm;
    std::string nx = gh_ent_next(g_cur_game, nm); if (nx.empty()) { gh_log("STAGE entry='%s' next=(없음)", nm.c_str()); return; }
    g_stage_next = nx; const ui::UiEnt * ce = gh_find_ent(g_cur_game, nm); std::string nl = phr_("다음 단계로 넘어가기"); if (ce) { auto li = ce->kv.find("next_label"); if (li != ce->kv.end() && !li->second.empty()) nl = li->second; } /* 속성 「다음 단계로」: 메뉴 이름 = next_label (확장 자리: 속성 종류를 더하려면 여기에 항목을 추가) */
    g_ui.stage_next_label = nl; gh_log("STAGE entry='%s' next='%s' (%s)", nm.c_str(), nx.c_str(), nl.c_str()); }
static void gh_spawn_stage(const std::string & entry) {   /* 같은 실행 인자에서 --demo/--entry 를 빼고 --entry <다음> 을 붙여 새 프로세스로 */
    std::string exe = plat::exe_path(); std::string cmd = "\"" + exe + "\"";
    for (size_t i = 1; i < g_argv_saved.size(); i++) { const std::string & a = g_argv_saved[i]; if (a == "--demo") continue; if (a == "--entry") { i++; continue; } cmd += " \"" + a + "\""; }
    if (!g_id_given) { cmd += " \""; cmd += (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) ? g_ui.games[g_cur_game].id : std::string(g_prof.id ? g_prof.id : ""); cmd += "\" --noboot"; }
    cmd += " --entry \"" + aw::u8_to_acp(entry) + "\"";
    unsigned long perr = 0;
    if (plat::spawn(exe, cmd, g_exedir, &perr)) { gh_log("STAGE 다음 단계 실행: %s", cmd.c_str()); }
    else gh_log("STAGE spawn FAIL err=%lu", (unsigned long)perr);
}
static void gh_sendkey(int arg) {
    static const KBD_KEYS K9[9] = { KBD_f10, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7, KBD_kana };
    if (arg < 0 || arg > 8) return;
    if ((int)K9[arg] < 256 && gensei_never[(int)K9[arg]]) { gh_log("sendkey #%d blocked (never_map)", arg); g_osd = phr_("이 작품에서는 보낼 수 없는 키입니다"); g_osd_until = SDL_GetTicks() + 2000; return; }
    g_key_down = K9[arg]; g_key_release_at = SDL_GetTicks() + 100; KEYBOARD_AddKey(g_key_down, true);
    gh_log("sendkey #%d", arg);
}
static bool gh_play_commit(void) { unsigned s = g_play_ms / 1000; if (!s || g_cur_game < 0 || g_cur_game >= (int)g_ui.games.size()) return false; g_ui.games[g_cur_game].play_s += (int)s; g_play_ms -= s * 1000; return true; }
/* ---- 게임 중 메뉴(에뮬 스레드에서 도는 중첩 루프 = 게임이 멈춘다) */
static void gh_bk_pause_set(bool on, const char * why) {   /* t47a (#559): 자동 입력 타이머만 멈춘다(게임은 계속 돈다) */
    if (on) { if (g_prof.bootkeys.empty()) { gh_osd(phr_("자동 입력 없음"), 1500); return; } if (g_next_bk >= g_prof.bootkeys.size()) { gh_osd(phr_("자동 입력이 이미 끝났습니다"), 1500); return; } }
    if (g_bk_paused == on) return;
    g_bk_paused = on; gh_log("BKPAUSE %s (%s) bk_t=%u next=#%u wall=%u", on ? "정지" : "재개", why, g_bk_t, (unsigned)g_next_bk, g_boot_tick ? SDL_GetTicks() - g_boot_tick : 0u);
    gh_osd(on ? phr_("자동 입력 일시정지 (Alt+P 로 재개)") : phr_("자동 입력 재개"), 1500);
}
static void gh_menu_run(void) {

    if (g_headless || !g_use_gl) return;
    kbov::release_all(); g_ui.kb_vis = kbov::g_vis ? 1 : 0; g_ui.kb_btn = kbov::g_btn_on ? 1 : 0; g_ui.kb_sh = g_kbshader ? 1 : 0; g_menu_open = true; gh_boost_update();
    static uint32_t snap[640 * 400], buf[640 * 400]; ov::Canvas sc = { snap, 640, 400 }, c = { buf, 640, 400 };
    sc.rect(0, 0, 640, 400, ov::K); if (!g_fb.empty()) ov::blit_scaled(sc, g_fb.data(), g_fbw, g_fbh, 0, 0, 640, 400);
    ui::Ui & U = g_ui; U.in_game = true; U.cur = g_cur_game; U.scr = ui::S_GAME; U.sel = 0;
    gh_refresh_game_slots(g_cur_game); U.slotsel = U.games[g_cur_game].lastslot >= 0 ? U.games[g_cur_game].lastslot : 0;
    bool done = false; bool sendk = false; int sendarg = 0;
    gh_diag_enter(1);
    while (!done && !g_quit) {
        gh_inj_poll_menu();   /* t49a: 하니스 inject 를 일시정지 메뉴에서도 */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (gh_evpre(e, 1)) continue;
            if (e.type == SDL_QUIT) { g_quit = true; done = true; break; }
            if (gh_wm_hotkey(e)) continue;
            ui::UKey k = gh_ev2uk(e, NULL); if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) gh_log("DIAG btn -> ev2uk=%d none=%d", (int)k, k == ui::UK_NONE ? 1 : 0); if (k == ui::UK_NONE) continue;
            ui::Act a = U.key(k);
            switch (a.t) {
            case ui::A_RESUME: done = true; break;
            case ui::A_NEXTSTAGE: if (!g_stage_next.empty()) { gh_log("STAGE F12 메뉴 「%s」 -> %s", U.stage_next_label.c_str(), g_stage_next.c_str()); if (g_inwin_enabled && !g_headless && g_use_gl) { g_inwin_req = 1; g_inwin_target = g_stage_next; } else g_stage_go = true; g_quit = true; done = true; } break;   /* t49a (#847) · t51a: 창 안 전환 먼저, 안 되면 gh_inwin_cycle 이 g_stage_go 로 폴백 */
            case ui::A_SAVE: { std::string d = g_savedir + "\\" + g_prof.id; plat::mkdir(g_savedir.c_str()); plat::mkdir(d.c_str());
                bool ok = gh_do_save(gh_slot_n(g_cur_game, a.slot), false); gh_osd(ok ? phr_("슬롯 ") + std::to_string(a.slot + 1) + phr_(" 에 저장했습니다") : std::string(phr_("저장에 실패했습니다")), 2000); done = true; break; }
            case ui::A_LOAD: { if (!U.games[g_cur_game].slot[a.slot].used) { gh_osd(phr_("비어 있는 슬롯입니다"), 1500); break; }
                bool ok = gh_do_load(gh_slot_n(g_cur_game, a.slot)); gh_osd(ok ? phr_("슬롯 ") + std::to_string(a.slot + 1) + phr_(" 을 불러왔습니다") : std::string(phr_("불러오기에 실패했습니다")), 2000); done = true; break; }
            case ui::A_DELSLOT: gh_delete_slot(g_cur_game, a.slot); break;
            case ui::A_APPLY: gh_apply_settings(U.eff(), true); gensei_western = U.eff().pad == 1; gh_apply_profile_input(g_prof, true); break;   /* t48e (#646): 조작 설정 변경을 바로 입력 층에 */
            case ui::A_SHOT: g_scr_req = true; break;   /* t41: 메뉴가 닫힌 뒤 다음 프레임에서 찍는다 */
            case ui::A_WINMULT: gh_win_apply(a.arg, false); break;
            case ui::A_FULLSCR: gh_fs_toggle(); break;
            case ui::A_KBD: kbov::set_visible(!kbov::g_vis); gh_kb_cursor(); gh_osd(kbov::g_vis ? phr_("화면 키보드 켜짐 (Alt+K 로 끔)") : phr_("화면 키보드 꺼짐"), 2000); done = true; break;
            case ui::A_KBSH: { int v = g_kbshader ? 0 : 1; g_kbshader = v; U.g.kbshader = v; if (g_cur_game >= 0 && g_cur_game < (int)ui::Ui::MAXG && U.has_t[g_cur_game]) U.t[g_cur_game].kbshader = v; U.kb_sh = v; U.dirty = true; gh_osd(v ? phr_("키보드·배속 표시에 셰이더를 적용합니다") : phr_("키보드·배속 표시를 셰이더 없이 그립니다"), 2500); break; }
            case ui::A_KBBTN: {
 int v = kbov::g_btn_on ? 0 : 1; kbov::g_btn_on = v != 0; U.g.kbbtn = v; if (g_cur_game >= 0 && g_cur_game < (int)ui::Ui::MAXG && U.has_t[g_cur_game]) U.t[g_cur_game].kbbtn = v; U.kb_btn = v; U.dirty = true; kbov::g_dirty = true; gh_osd(v ? phr_("키보드 버튼을 표시합니다") : phr_("키보드 버튼을 숨깁니다 (Alt+K · 메뉴로 키보드 켜기)"), 2500); break; }
            case ui::A_SENDKEY: sendk = true; sendarg = a.arg; done = true; break;
            case ui::A_DISKSWAP: { auto fit_ = g_fdfiles.find(U.games[g_cur_game].id); bool ok = false;
                if (fit_ != g_fdfiles.end() && a.arg >= 0 && a.arg < (int)fit_->second.size() && a.slot >= 0 && a.slot < 2) { ok = gh_attach(a.slot, fit_->second[a.arg].c_str(), true); if (ok) U.games[g_cur_game].cur_fd[a.slot] = U.games[g_cur_game].fds[a.arg]; }
                gh_log("DISKSWAP drive=%c idx=%d %s", a.slot ? 'B' : 'A', a.arg, ok ? "OK" : "FAIL");
                gh_osd(ok ? std::string(a.slot ? "B" : "A") + phr_(" 드라이브 : ") + U.games[g_cur_game].cur_fd[a.slot] : std::string(phr_("디스크 교체에 실패했습니다")), 3000); done = true; break; }
            case ui::A_MAINMENU: gh_play_commit(); ui::settings_save(U, g_cfgpath); if (g_inwin_enabled && !g_headless && g_use_gl) g_inwin_req = 2; else gh_spawn_menu(); g_quit = true; done = true; break;   /* t51a: 창 안 메뉴 */
            case ui::A_QUIT: gh_play_commit(); if (a.arg) ui::settings_save(U, g_cfgpath); g_quit = true; done = true; break;
            default: break; }
            if (done) break;
        }
        gh_wm_poll();
        if (U.scr == ui::S_GAME || U.scr == ui::S_KEYS || U.scr == ui::S_QUIT || U.scr == ui::S_SWAP || U.scr == ui::S_SWAPI) memcpy(buf, snap, sizeof buf);
        U.fs_now = gh_fs_now(); U.ts_ms = (int)SDL_GetTicks(); U.draw(c); { std::string bw = gh_builtin_warn(); if (!bw.empty()) ov::osd_draw(c, bw, false, 336); } if (g_osd_until && SDL_GetTicks() < g_osd_until) ov::osd_draw(c, g_osd, true);
        gh_publish(buf, 640, 400); SDL_Delay(16);
    }
    if (U.dirty) { ui::settings_save(U, g_cfgpath); U.dirty = false; }
    U.in_game = false; g_menu_open = false; g_bk_last = SDL_GetTicks();   /* t47a: 메뉴에 있던 시간은 자동 입력 시계에서 뺀다 */
    gh_apply_settings(U.effective(g_cur_game), true); gh_boost_update(); kbov::wake();
    if (g_osd_until && !g_boost_on && SDL_GetTicks() < g_osd_until && g_osd.find("▶▶") == 0) g_osd_until = 0;
    if (sendk) gh_sendkey(sendarg);
    if (!g_fb.empty()) gh_publish(g_fb.data(), g_fbw, g_fbh);
}

/* ---- r012: 작품을 고르기 전 UI 루프. true = 게임 시작(out_id), false = 끝내기 */
static int g_cli_lang = -1;   /* t52g 시험 전용(--lang N): 설정의 언어 대신 0 한국어 · 1 English · 2 にほんご */
static bool g_menushot = false;   /* t52f 시험 전용(--menushot): 메뉴 창을 실제로 찍어 4:3 고정을 눈으로 본다 */
static bool gh_shot_to(const std::string & path) {
    snprintf(g_shot_path, sizeof g_shot_path, "%s", path.c_str()); g_shot_w = g_shot_h = 0; g_shot_crop = 0; g_shot_req = 1;
    for (int t = 0; t < 600 && g_shot_req; t++) SDL_Delay(5);
    bool done = !g_shot_req; g_shot_req = 0; gh_log("MENUSHOT %s %s %dx%d fit=%d", done ? "OK" : "FAIL", path.c_str(), g_shot_w, g_shot_h, (int)g_fit); return done;
}
static bool gh_menushot_tick(ui::Ui & U) {   /* true = 시험 끝(메뉴를 닫는다) */
    static unsigned t0 = 0; if (!t0) t0 = SDL_GetTicks(); unsigned t = SDL_GetTicks() - t0; static int st = 0;
    std::string dir = g_outdir.empty() ? g_exedir : g_outdir;
    if (st == 0 && t >= 2500) { st = 1; gh_log("T52F menu main scr=%d cur=%d game_ratio=%d fit=%d menu43=%d", (int)U.scr, U.cur, U.effective(U.cur).ratio, (int)g_fit, (int)g_menu43); gh_shot_to(dir + "\\menu_main.png"); }
    else if (st == 1 && t >= 3500) { st = 2; U.mainsel = 1; U.key(ui::UK_OK); }
    else if (st == 2 && t >= 5000) { st = 3; gh_log("T52F menu list scr=%d cur=%d game_ratio=%d fit=%d menu43=%d", (int)U.scr, U.cur, U.effective(U.cur).ratio, (int)g_fit, (int)g_menu43); gh_shot_to(dir + "\\menu_list.png"); }
    else if (st == 3 && t >= 6000) return true;
    return false;
}
static bool gh_ui_main_(std::string & out_id) {
    ui::Ui & U = g_ui;
    for (size_t i = 0; i < U.games.size(); i++) gh_refresh_game_slots((int)i);
    U.in_game = false; U.scr = ui::S_MAIN; SDL_StartTextInput(); U.now_s = (long long)time(NULL);
    {   /* t39: 설치 실행 뒤 처음 열린 목록에서 「설치를 마쳤습니까?」 */
        std::string pf = g_exedir + "\\install_pending.tsv"; FILE * f = fopen(pf.c_str(), "rb");
        if (f) { char ln[512]; std::string line; if (fgets(ln, sizeof ln, f)) line = ln; fclose(f); plat::remove_file(pf.c_str());
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back(); size_t tb = line.find('\t');
            if (tb != std::string::npos) { std::string gid = line.substr(0, tb), en = line.substr(tb + 1); for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { U.ask_install_done((int)i, en); break; } } }
    }
    if (U.cur < 0 || U.cur >= (int)U.games.size()) U.cur = 0;
    U.pick = U.cur; g_cur_game = U.cur;
    gh_apply_settings(U.effective(U.cur), false);
    if (!g_noboot && U.g.boot == 0) { if (gh_boot_seq(true) && g_quit) return false; }
    static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 };
    gh_diag_enter(0);
    for (;;) {
        gh_inj_poll_menu();   /* t49a: 하니스 inject 를 메뉴 루프에서도 */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (gh_evpre(e, 0)) continue;
            if (e.type == SDL_QUIT) return false;
            if (e.type == SDL_TEXTINPUT) { U.text_in(e.text.text); continue; }
            if (e.type == SDL_TEXTEDITING) { U.ime_edit(e.edit.text); continue; }   /* t48a: IME 조합 중 글자 */
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && U.scr == ui::S_OSK && U.osk_hg) {   /* t48b: 마우스·터치(터치는 SDL 이 마우스로 바꿔 준다) */
                int vx = 0, vy = 0; bool ok = gh_menu_win2virt(e.button.x, e.button.y, vx, vy); bool hit = ok && U.pointer(vx, vy); gh_log("HGPTR win=%d,%d virt=%d,%d hit=%d", e.button.x, e.button.y, vx, vy, hit ? 1 : 0); continue; }
            if ((e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) && e.cbutton.button == (gensei_western ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B) && U.scr == ui::S_OSK && U.osk_hg) {   /* t48b (#629 2): ○ 는 뗄 때 입력, 0.5초 넘게 누르면 SHIFT(어깨 버튼이 없는 패드용) */
                if (e.type == SDL_CONTROLLERBUTTONDOWN) { g_ob_held = true; g_ob_long = false; g_ob_t0 = SDL_GetTicks(); }
                else { if (g_ob_held && !g_ob_long) U.key(ui::UK_OK); g_ob_held = false; }
                continue; }
            if (gh_wm_hotkey(e)) continue;
            ui::UKey k = gh_ev2uk(e, NULL); if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) gh_log("DIAG btn -> ev2uk=%d none=%d", (int)k, k == ui::UK_NONE ? 1 : 0); if (k == ui::UK_NONE) continue;
            ui::Act a = U.key(k);
            switch (a.t) {
            case ui::A_START: if (U.games[a.game].id.empty()) break; if (!gh_font_guard()) break; gh_launch_note(U, a); U.cur = a.game; g_cur_game = a.game; g_resume_slot = a.resume && U.games[a.game].lastslot >= 0 ? U.games[a.game].lastslot + 1 : 0; gh_chain_start(U, a.game); out_id = U.games[a.game].id; ui::settings_save(U, g_cfgpath); return true;
            case ui::A_LOAD: if (U.games[a.game].id.empty() || !U.blk_id(U.games[a.game].id).empty()) break; if (!gh_font_guard()) break; gh_launch_note(U, a); U.cur = g_cur_game = a.game; g_resume_slot = a.slot + 1; out_id = U.games[a.game].id; ui::settings_save(U, g_cfgpath); return true;
            case ui::A_DELSLOT: gh_delete_slot(a.game, a.slot); break;
            case ui::A_APPLY: gh_apply_settings(U.scr == ui::S_SCREEN ? U.eff() : U.effective(U.cur), false); gensei_western = (U.scr == ui::S_PAD || U.scr == ui::S_PADKEY ? U.eff() : U.effective(U.cur)).pad == 1; break;   /* t48e: 시작 메뉴의 결정/취소 버튼도 방식을 따른다 */
            case ui::A_WINMULT: gh_win_apply(a.arg, false); break;
            case ui::A_FULLSCR: gh_fs_toggle(); break;
            case ui::A_ADD: gh_add_action(a.arg); break;
            case ui::A_SETLANG: gh_setlang(a.game, a.arg); break;
            case ui::A_SETTITLE: gh_settitle(a.game, a.entry); break;
            case ui::A_TEST: gh_spawn_test(U.games[a.game].id, a.entry, a.arg); break;
            case ui::A_EXTRA:
                if (a.arg == 1 && !gh_font_guard()) break;
                if (a.arg == 1) gh_launch_note(U, a);
                if (a.arg == 1) { g_noB = true; g_resume_slot = 0; U.cur = g_cur_game = a.game; out_id = U.games[a.game].id; ui::settings_save(U, g_cfgpath); return true; }
                if (a.arg == 2) { gh_boot_seq(true); }
                if (a.arg == 6) { std::string rid = U.games[a.game].id; int nn = gh_reset_disks(rid); g_note = nn > 0 ? phr_("원본 디스크로 초기화했습니다 (사본 ") + std::to_string(nn) + phr_("개 삭제)") : phr_("초기화할 디스크 사본이 없습니다 (원본 그대로)"); g_note_until = SDL_GetTicks() + 3000u; }
                break;
            case ui::A_QUIT: if (a.arg) ui::settings_save(U, g_cfgpath); return false;
            default: break; }
        }
        if (!(U.scr == ui::S_OSK && U.osk_hg)) g_ob_held = false;
        if (g_ob_held && !g_ob_long && SDL_GetTicks() - g_ob_t0 >= 500) { g_ob_long = true; U.key(ui::UK_L); gh_log("HGSHIFT ○ 길게 → SHIFT"); }   /* t48b */
        { static int prev_scr = -1; if (prev_scr == (int)ui::S_OSK && (int)U.scr != (int)ui::S_OSK) gh_log("KBSENT total=%u in_text=%u (이 메뉴 프로세스의 kbov 가 코어로 보낸 키 수 / 그중 텍스트 입력 중)", g_kbsent_total, g_kbsent_text); prev_scr = (int)U.scr; }
        gh_wm_poll();
        if (U.scr != ui::S_SCREEN) gh_apply_settings(U.effective(U.cur), false);
        g_cur_game = U.cur;
        if (U.lang_chg) { U.lang_chg = false; gh_reload_games(); gh_log("T52G 언어 바꿈 -> lang=%d, 목록 다시 만듦", U.lang); }   /* t52g (#133) */
        if (g_menushot && !g_headless && g_win && gh_menushot_tick(U)) return false;   /* t52f 시험 */
        U.fs_now = gh_fs_now(); U.ts_ms = (int)SDL_GetTicks(); U.now_s = (long long)time(NULL); U.draw(c); if (g_note_until && SDL_GetTicks() < g_note_until) ov::osd_draw(c, g_note, false, 336); gh_publish(buf, 640, 400); SDL_Delay(16);
    }
}

/* t52f (#132 2항): 시작 메뉴를 도는 동안은 작품별 화면비와 상관없이 4:3. 메뉴를 나가면(작품 시작) 고른 작품의 화면비를 적용한다 */
static bool gh_ui_main(std::string & out_id) {
    g_menu43 = true; gh_log("T52F menu43 on (시작 메뉴 화면비 4:3 고정)");
    bool r = gh_ui_main_(out_id);
    g_menu43 = false; if (!g_headless) gh_apply_settings(g_ui.effective(g_cur_game), false);
    gh_log("T52F menu43 off -> fit=%d (고른 작품 %s, 작품 화면비 %d)", (int)g_fit, out_id.c_str(), (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) ? g_ui.effective(g_cur_game).ratio : -1);
    return r;
}

/* ---- r012: 창 없이 UI 를 호스트 코드 그대로 그려 PNG 로 (--uitest 폴더). 화면별 캡처 + 부팅 장면 + 설정 저장/읽기 왕복 */
static void gh_uitest(void) {
    ui::Ui & U = g_ui; static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; int n = 0;
    plat::mkdir(g_uitest_dir.c_str());
    for (size_t i = 0; i < U.games.size(); i++) gh_refresh_game_slots((int)i);
    U.cur = U.pick = 2; g_cur_game = 2;
    auto shot = [&](const char * nm) { U.draw(c); { std::string bw = gh_builtin_warn(); if (!bw.empty()) ov::osd_draw(c, bw, false, 336); } char p[700]; if (!g_uikeys_file.empty()) snprintf(p, sizeof p, "%s\\ui_%02d_%s_s%02dt%dc%d.png", g_uitest_dir.c_str(), ++n, nm, (int)U.scr, (int)U.tab, U.sel); else snprintf(p, sizeof p, "%s\\ui_%02d_%s.png", g_uitest_dir.c_str(), ++n, nm);   /* t39ui4: 스크립트 캡처 이름에 화면 번호·탭·선택 줄을 붙인다 */ gh_png(p, buf, 640, 400); };
    auto boot = [&](const char * nm, int stage, bool cells) {
        c.rect(0, 0, 640, 400, ov::K);
        if (stage >= 1) ov::text(c, 40, 112, std::string(KH_FULLNAME), ov::WH, ov::K);
        if (stage >= 2) ov::text(c, 40, 144, std::string("Now Loading") + std::string((size_t)(stage - 1 > 3 ? 3 : stage - 1), '.'), ov::WH, ov::K);
        if (cells) ov::fncells_boot(c);   /* t52a (#49) */
        char p[700]; snprintf(p, sizeof p, "%s\\ui_%02d_%s.png", g_uitest_dir.c_str(), ++n, nm); gh_png(p, buf, 640, 400);
    };
    if (!g_uikeys_file.empty()) {   /* t39: 키 입력 스크립트 시험 — 공백 구분 토큰, # 줄은 주석. shot:이름 text:글자 go:작품id 키(U D L R O C T S E LB RB)*반복 */
        std::string sc; { FILE * kf = fopen(g_uikeys_file.c_str(), "rb"); if (kf) { char ln[1024]; while (fgets(ln, sizeof ln, kf)) { if (ln[0] != '#') sc += ln; } fclose(kf); } else gh_log("uikeys: 파일을 열 수 없음 %s", g_uikeys_file.c_str()); }
        static const std::map<std::string, ui::UKey> KM = { { "U", ui::UK_UP }, { "D", ui::UK_DOWN }, { "L", ui::UK_LEFT }, { "R", ui::UK_RIGHT }, { "O", ui::UK_OK }, { "C", ui::UK_CANCEL }, { "T", ui::UK_TRI }, { "S", ui::UK_SQ }, { "E", ui::UK_SELECT }, { "LB", ui::UK_L }, { "RB", ui::UK_R } };
        U.in_game = false; U.scr = ui::S_MAIN; U.ts_ms = 0; U.now_s = (long long)time(NULL); if (U.cur < 0 || U.cur >= (int)U.games.size()) U.cur = 0; U.pick = U.cur; g_cur_game = U.cur;
        std::istringstream in(sc); std::string tk; int nact = 0;
        while (in >> tk) {
            if (tk.compare(0, 5, "shot:") == 0) { shot(tk.substr(5).c_str()); continue; }
            if (tk.compare(0, 5, "text:") == 0) { U.text_in(tk.substr(5)); continue; }
            if (tk.compare(0, 5, "vals:") == 0) {   /* t48a (#614 ③): 전 작품 적용값 표를 TSV 로 저장(시험 전용) */
                std::string vp = g_uitest_dir + "\\" + tk.substr(5); FILE * vf = fopen(vp.c_str(), "wb"); int nr = 0;
                if (vf) { for (size_t i = 0; i < U.games.size(); i++) { std::vector<ui::ValRow> rows; if (!U.env.values || !U.env.values((int)i, rows)) continue; for (auto & r : rows) { fprintf(vf, "%s\t%s\t%s\t%s\t%s\n", U.games[i].id.c_str(), r.key.c_str(), r.val.c_str(), r.src.c_str(), r.cov.c_str()); nr++; } } fclose(vf); }
                gh_log("uikeys: 적용값 표 %d줄 저장 %s", nr, vp.c_str()); continue; }
            if (tk == "open_vals") { U.open_vals(); continue; }
            if (tk.compare(0, 4, "dev:") == 0) { ov::g_helpdev = (tk.substr(4) == "kbd") ? 1 : 0; continue; }   /* t49a 시험 전용: 조작 설명 표기 장치 지정 (pad / kbd) */
            if (tk.compare(0, 11, "stagelabel:") == 0) { std::string sl = tk.substr(11); for (auto & ch3 : sl) if (ch3 == '_') ch3 = ' '; U.stage_next_label = sl; continue; }   /* t49a 시험 전용: 일시정지 메뉴의 「다음 단계」 줄 켜기 (_ = 공백) */
            if (tk.compare(0, 3, "hk:") == 0) { U.hk_script(tk.substr(3)); continue; }   /* t48b 시험 전용: 한글 키보드 키 누르기 열 */
            if (tk.compare(0, 4, "tap:") == 0) { int vx = 0, vy = 0; sscanf(tk.c_str() + 4, "%d,%d", &vx, &vy); bool hit = U.pointer(vx, vy); gh_log("uikeys: tap %d,%d hit=%d", vx, vy, hit ? 1 : 0); continue; }
            if (tk == "hkdump") { gh_log("uikeys: HKDUMP buf='%s' cur='%s' han=%d shift=%d sel=%d,%d osk_hg=%d scr=%d", U.osk_buf.c_str(), hg::cur_text(U.hc.cur).c_str(), U.hc.han ? 1 : 0, U.osk_shift ? 1 : 0, U.osk_r, U.osk_c, U.osk_hg ? 1 : 0, (int)U.scr); continue; }
            if (tk.compare(0, 8, "newname:") == 0) { std::string gid = tk.substr(8); for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { ent::Kv kv; for (auto & ee : U.games[i].ents) if (!ee.pseudo && ee.vgi < 0) { kv = ee.kv; break; } U.cur = (int)i; U.entg = (int)i; U.begin_new_from_kv((int)i, kv, "새 방법"); break; } continue; }   /* t48b 시험 전용: 새 실행 방법 편집 화면으로 */
            if (tk.compare(0, 8, "entdump:") == 0) { std::string gid = tk.substr(8); for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { for (auto & ee : U.games[i].ents) { std::string hx; for (unsigned char ch2 : ee.name) { char b2[4]; snprintf(b2, sizeof b2, "%02X", ch2); hx += b2; } gh_log("uikeys: ENTS %s name='%s' pseudo=%d hex=%s", gid.c_str(), ee.name.c_str(), ee.pseudo ? 1 : 0, hx.c_str()); } } continue; }
            if (tk == "kbsent") { gh_log("uikeys: KBSENT total=%u in_text=%u", g_kbsent_total, g_kbsent_text); continue; }
            if (tk.compare(0, 11, "launchnote:") == 0) {   /* t48a 시험 전용: 실행 요청이 항목 이름(전역)을 남기는 것을 흉내 */
                std::string r = tk.substr(11); size_t c2 = r.find(':'); std::string gid = r.substr(0, c2), en = c2 == std::string::npos ? std::string() : r.substr(c2 + 1);
                prof::g_entry_req = en; for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { U.cur = (int)i; g_cur_game = (int)i; }
                gh_log("uikeys: 실행 요청 흉내 id=%s entry=%s", gid.c_str(), en.c_str()); continue; }
            if (tk == "reload") { gh_reload_games(); gh_log("uikeys: 목록 다시 만듦 (entry_req='%s')", prof::g_entry_req.c_str()); continue; }
            if (tk == "blkdump") { if (U.pick >= 0 && U.pick < (int)U.vis.size()) { int gi = U.vis[U.pick]; gh_log("uikeys: BLKDUMP row=%s row_old_games_gi_block='%s' row_by_id_block='%s' cur=%s cur_by_id_block='%s' cur_games_cur_block='%s' sel_entry='%s' scr=%d", U.games[gi].id.c_str(), U.games[gi].block.c_str(), U.blk_row(gi).c_str(), U.game() ? U.game()->id.c_str() : "", U.blk_cur().c_str(), U.game() ? U.game()->block.c_str() : "", U.sel_entry.c_str(), (int)U.scr); } continue; }
if (tk == "blocks") { for (auto & gg : U.games) gh_log("uikeys: BLOCKS %s block='%s'", gg.id.c_str(), gg.block.c_str()); continue; }
            if (tk == "home") { U.in_game = false; U.scr = ui::S_MAIN; U.mainsel = 0; U.sel = 0; continue; }   /* t52i 시험 전용: 메인메뉴로 돌아가 맨 위 */
            if (tk == "startpick") { U.start_ret = ui::S_PICK; U.startsel = 1; U.scr = ui::S_START; U.sel = 1; continue; }   /* t52i 시험 전용: 작품 목록에서 연 시작 팝업 */
            if (tk == "valdump") { gh_log("uikeys: T52I VALDUMP bgm=%d se=%d bmult=%d bmode=%d dirty=%d scr=%d", U.g.bgm, U.g.se, U.g.bmult, U.g.bmode, U.dirty ? 1 : 0, (int)U.scr); continue; }   /* t52i 시험 전용: 동작 환경 값 기록 */
            if (tk == "save") { std::string sp = g_uitest_dir + "\\settings_uikeys.ini"; ui::settings_save(U, sp); gh_log("uikeys: 설정 저장 %s (bleed=%d)", sp.c_str(), U.g.bleed); continue; }   /* t42: 저장 시험 */
            if (tk == "ingame") { U.in_game = true; U.scr = ui::S_GAME; U.sel = 0; continue; }   /* t39ui4: 게임 중 메뉴로 진입 */
            if (tk.compare(0, 4, "tab:") == 0) { std::string t = tk.substr(4); int want = t == "ko" ? 0 : t == "jp" ? 1 : 2; for (int gg = 0; gg < 4 && (int)U.tab != want; gg++) U.key(ui::UK_R); continue; }   /* t39ui4: 탭 맞추기 */
            if (tk.compare(0, 3, "go:") == 0) { for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == tk.substr(3)) { int b = U.base_of((int)i); if (!U.row_in_tab(b)) U.tab = ui::Ui::TAB_ALL; U.rebuild_vis((int)i); } continue; }
            if (tk.compare(0, 4, "sel:") == 0) { U.sel = atoi(tk.c_str() + 4); continue; }   /* t44c: 시험 전용 — 선택 행 직접 지정 */
            if (tk == "entlang") { U.sel = (int)std::max<size_t>(U.vents(U.entg).size(), 1) + 1; continue; }   /* t44c: S_ENT 의 「언어」 행 */
            if (tk.compare(0, 7, "addnew:") == 0) { U.add_new.push_back(tk.substr(7)); U.add_new_why.push_back("시험: 자동 판정 근거 줄"); continue; }   /* t44c: 방금 추가한 작품 목록 */
            if (tk == "addscr") { U.scr = ui::S_ADD; U.sel = 0; continue; }
            if (tk.compare(0, 4, "cur:") == 0) { for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == tk.substr(4)) { U.cur = (int)i; U.entg = (int)i; g_cur_game = (int)i; } continue; }   /* t48e2 시험 전용: 현재 작품 지정 */
            if (tk == "padscr") { U.scr = ui::S_PAD; U.sel = 0; continue; }   /* t48e 시험 전용: 조작 설정 화면으로 */
            if (tk.compare(0, 4, "scr:") == 0) { U.scr = (ui::Scr)atoi(tk.c_str() + 4); U.sel = 0; continue; }   /* t50a 시험 전용 (#877): 화면 번호로 직접 이동 — 안 닿던 화면의 도움말 폭 실측용 */
            if (tk == "lang") { std::vector<std::string> ids, why; for (auto & gg : U.games) ids.push_back(gg.id); U.open_lang(ids, why, ui::S_MAIN); continue; }   /* t50a 시험 전용: 언어 화면 */
            if (tk.compare(0, 7, "padkey:") == 0) { U.open_padkey(atoi(tk.c_str() + 7)); continue; }   /* t50a 시험 전용: 패드 키 고르기 화면 */
            if (tk.compare(0, 4, "osk:") == 0) { U.osk_open(atoi(tk.c_str() + 4), "", ui::S_MAIN, false); continue; }   /* t50a 시험 전용: 화면 키보드 (0=이름 · 그 밖=DOS 명령) */
            if (tk.compare(0, 6, "scope:") == 0) { U.scope = atoi(tk.c_str() + 6); if (U.scope == 1 && U.cur >= 0 && U.cur < (int)U.games.size() && !U.has_t[U.cur]) { U.t[U.cur] = U.effective(U.cur); U.has_t[U.cur] = true; } continue; }
            if (tk == "padstate") {
                std::string gp; for (auto & kv : U.gpad) gp += kv.first + "=" + kv.second + " "; std::string wp; if (U.cur >= 0 && U.cur < (int)U.games.size()) { std::map<std::string, std::string> m; prof::get_pad_keys(U.games[U.cur].dir, m); for (auto & kv : m) wp += kv.first + "=" + kv.second + " "; }
                gh_log("uikeys: PADSTATE scr=%d sel=%d scope=%d g.pad=%d eff.pad=%d global[%s] work[%s]", (int)U.scr, U.sel, U.scope, U.g.pad, U.eff().pad, gp.c_str(), wp.c_str());
                if (U.cur >= 0 && U.cur < (int)U.games.size()) { for (int pi = 0; pi < 7; pi++) { bool fw = false, df = false; std::string v = U.pad_val(pi, &fw, &df); gh_log("uikeys: PADROW %d %s -> %s%s", pi, U.pad_row_name(pi).c_str(), v.c_str(), fw ? " (작품)" : df ? " (기본)" : " (전역)"); } }
                continue; }
            if (tk.compare(0, 8, "delslot:") == 0) {   /* S3 retro-test token */
                std::string r = tk.substr(8); size_t cp = r.find(':'); std::string gid = r.substr(0, cp); int sl = cp == std::string::npos ? 0 : atoi(r.c_str() + cp + 1); int gi = -1;
                for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) gi = (int)i;
                if (gi < 0 || sl < 0 || sl > 7) { gh_log("uikeys: DELSLOT bad arg %s", r.c_str()); continue; }
                bool before = plat::exists(gh_slot_n(gi, sl)); gh_delete_slot(gi, sl); gh_log("uikeys: DELSLOT %s slot%02d before=%d after=%d listed_used=%d", gid.c_str(), sl + 1, before ? 1 : 0, plat::exists(gh_slot_n(gi, sl)) ? 1 : 0, U.games[gi].slot[sl].used ? 1 : 0); continue; }
            if (tk.compare(0, 11, "resetdisks:") == 0) { std::string gid = tk.substr(11); int nn = gh_reset_disks(gid); gh_log("uikeys: RESETDISKS %s deleted=%d", gid.c_str(), nn); continue; }   /* S3 retro-test token */
            if (tk.compare(0, 10, "spawntest:") == 0) { gh_spawn_test(tk.substr(10), "", 4); continue; }   /* S3 retro-test token */
            int rep = 1; size_t st = tk.find('*'); if (st != std::string::npos) { rep = atoi(tk.c_str() + st + 1); tk = tk.substr(0, st); }
            auto it = KM.find(tk); if (it == KM.end()) { gh_log("uikeys: 알 수 없는 토큰 %s", tk.c_str()); continue; }
            for (int i = 0; i < rep; i++) { ui::Act a = U.key(it->second); if (a.t != ui::A_NONE) { nact++; gh_log("uikeys: act=%d game=%d arg=%d entry=%s", (int)a.t, a.game, a.arg, a.entry.c_str()); if (a.t == ui::A_SETLANG) gh_setlang(a.game, a.arg); if (a.t == ui::A_SETTITLE) gh_settitle(a.game, a.entry); } }   /* t44c: A_SETLANG 는 호스트 처리기를 그대로 실행 */
        }
        gh_log("uikeys: done acts=%d shots=%d", nact, n); return;
    }
    boot("boot1", 1, false); boot("boot3", 3, false); boot("boot5", 4, true);
    if (U.games.size() >= 3) {   /* 캡처용 표본 정보(시험 전용 값) */
        ui::UiGame & k = U.games[2]; k.company = "컴파일"; k.year = "표본"; k.genre = "RPG"; k.patch = "한글 패치"; k.disknote = "FD 2장 + 오프닝"; k.devices = "86 보드 · 마우스 선택"; k.play_s = 5 * 3600 + 20 * 60; k.has_demo = true;
        k.fds = { "환세희담 시스템 디스크", "환세희담 데이터 디스크", "환세희담 오프닝 디스크" }; k.cur_fd[0] = k.fds[0]; k.cur_fd[1] = k.fds[1]; k.ov["shader"] = "1"; k.ov["boost"] = "3";
    }
    auto shotg = [&](const char * nm) { ov::sample_picture(c, 0, 0, 640, 400); shot(nm); };
    shot("main"); U.mainsel = 0; U.key(ui::UK_OK); shot("start"); U.key(ui::UK_CANCEL);
    U.mainsel = 1; U.key(ui::UK_OK); shot("pick"); U.key(ui::UK_DOWN); U.key(ui::UK_OK); shot("main_after_pick"); U.cur = U.pick = 2; g_cur_game = 2;
    U.mainsel = 2; U.key(ui::UK_OK); shot("screen_sharp"); U.key(ui::UK_DOWN); U.key(ui::UK_DOWN); U.key(ui::UK_RIGHT); shot("screen_crt"); U.key(ui::UK_DOWN); U.key(ui::UK_RIGHT); shot("screen_crt_curve"); U.key(ui::UK_CANCEL);
    U.mainsel = 3; U.key(ui::UK_OK); U.key(ui::UK_DOWN); U.key(ui::UK_LEFT); shot("conf"); for (int i = 0; i < 7; i++) U.key(ui::UK_DOWN); shot("conf_bosd"); U.key(ui::UK_OK);
    U.mainsel = 4; U.key(ui::UK_OK); shot("save"); U.key(ui::UK_CANCEL);
    U.mainsel = 5; U.key(ui::UK_OK); shot("extra"); U.sel = 5; U.key(ui::UK_OK); shot("about"); U.key(ui::UK_CANCEL); U.sel = 0; U.key(ui::UK_DOWN); U.key(ui::UK_DOWN); U.key(ui::UK_DOWN); U.key(ui::UK_OK); shot("help"); U.key(ui::UK_CANCEL); U.key(ui::UK_CANCEL);
    U.mainsel = 6; U.key(ui::UK_OK); shot("quit"); U.key(ui::UK_CANCEL);
    U.in_game = true; U.scr = ui::S_GAME; U.sel = 1; shotg("ingame"); U.scr = ui::S_KEYS; U.sel = 0; shotg("keys");
    U.scr = ui::S_SWAP; U.swapdrv = 0; shotg("swap"); U.scr = ui::S_SWAPI; U.swapsel = 2; shotg("swap_pick"); U.scr = ui::S_QUIT; U.quitsel = 0; shotg("ingame_quit");
    /* t52f (#132): 동작 환경 설정의 새 행 「오버레이 버튼 표시」 */
    { U.in_game = false; U.scr = ui::S_MAIN; U.scope = 0; U.mainsel = 3; U.key(ui::UK_OK); U.sel = 10; int o0 = U.g.ovbtn; U.key(ui::UK_LEFT); int o1 = U.g.ovbtn; shotg("conf_ovbtn_off"); U.key(ui::UK_RIGHT); int o2 = U.g.ovbtn; U.sel = 12; shotg("conf_row_jpfont"); ui::Act ra = U.key(ui::UK_CANCEL); int o3 = U.g.ovbtn; gh_log("T52F conf ovbtn start=%d left=%d right=%d after_cancel=%d rows=%d -> %s", o0, o1, o2, o3, 13, (o0 == 1 && o1 == 0 && o2 == 1 && o3 == 1) ? "PASS" : "FAIL"); (void)ra; U.scr = ui::S_MAIN; U.in_game = false; }
    /* t52g (#133): UI 언어 3종 — 언어마다 주요 화면을 찍고, 설정의 언어 행(←→ 순환)·문구 칸 즉시 교체·settings 저장/읽기를 검사 */
    { static const char * const lnm[3] = { "ko", "en", "ja" }; static const char * const want[3] = { "게임 시작", "Start game", "ゲーム スタート" };
      U.in_game = false; U.scope = 0; U.lang = 0; U.lang_chg = false; phr_set_lang(0); int bad = 0; char nm[48];
      for (int L = 0; L < 3; L++) {
          const char * g0 = phr_("게임 시작"); if (strcmp(g0, want[L]) != 0) { bad++; gh_log("T52G lang %d 게임 시작='%s' want='%s' FAIL", L, g0, want[L]); }
          U.scr = ui::S_MAIN; U.mainsel = 0; snprintf(nm, sizeof nm, "t52g_main_%s", lnm[L]); shotg(nm); gh_log("T52H 글꼴 lang=%d 사용자BMP=%d 동봉FONTX2=%d 일본어켜짐=%d", L, (int)(ov::g_jp_bmpd != NULL), (int)ov::g_jp_fx.ok, (int)ov::g_jp_on);   /* t52h */
          U.scr = ui::S_CONF; U.sel = 13; snprintf(nm, sizeof nm, "t52g_conf_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_SCREEN; U.sel = 0; snprintf(nm, sizeof nm, "t52g_screen_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_SAVE; U.sel = 0; snprintf(nm, sizeof nm, "t52g_save_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_EXTRA; U.sel = 0; snprintf(nm, sizeof nm, "t52g_extra_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_ADD; U.sel = 0; snprintf(nm, sizeof nm, "t52g_add_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_PICK; U.sel = 0; snprintf(nm, sizeof nm, "t52g_pick_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_CONF; U.sel = 13; int l0 = U.lang; U.key(ui::UK_RIGHT); if (U.lang != (l0 + 1) % 3) { bad++; gh_log("T52G 언어 행 → 키: lang %d -> %d FAIL", l0, U.lang); }
      }
      bool flag_ok = U.lang_chg; U.lang_chg = false;
      std::string ip = g_uitest_dir + "\\settings_t52g.ini"; U.lang = 2; ui::settings_save(U, ip); ui::Ui B; B.games = U.games; ui::settings_load(B, ip); bool rt = B.lang == 2;
      U.lang = 0; phr_set_lang(0);
      gh_log("T52G lang cycle bad=%d flag=%d roundtrip=%d (phrases ko=%d) -> %s", bad, (int)flag_ok, (int)rt, phr_loaded_count, (bad == 0 && flag_ok && rt) ? "PASS" : "FAIL"); }
    /* t52e (#131): 확인창 3종 + 동작 환경 설정 × 복원 검사 */
    if (U.games.size() >= 3) { U.cur = U.pick = 2; g_cur_game = 2; } for (auto & gg0 : U.games) { gg0.slot[0].used = true; gg0.slot[0].stamp = "2026-11-02 07:00"; gg0.lastslot = 0; }
    U.in_game = true; U.scr = ui::S_GAME; U.sel = 11; U.key(ui::UK_OK); shotg("yn_mainmenu"); U.key(ui::UK_CANCEL); gh_log("T52E yn_mainmenu back scr=%d (S_GAME=%d)", (int)U.scr, (int)ui::S_GAME);
    U.in_game = false; U.scr = ui::S_SAVE; U.slotsel = 0; gh_log("T52E delslot pre cur=%d used=%d", U.cur, U.game() ? (int)U.game()->slot[0].used : -1); U.key(ui::UK_TRI); gh_log("T52E delslot after TRI scr=%d (S_YN=%d)", (int)U.scr, (int)ui::S_YN); shotg("yn_delslot"); U.key(ui::UK_CANCEL); gh_log("T52E yn_delslot back scr=%d (S_SAVE=%d)", (int)U.scr, (int)ui::S_SAVE);
    U.scr = ui::S_EXTRA; U.sel = 6; U.key(ui::UK_OK); shotg("yn_diskreset"); U.key(ui::UK_CANCEL); gh_log("T52E yn_diskreset back scr=%d (S_EXTRA=%d)", (int)U.scr, (int)ui::S_EXTRA);
    { U.scr = ui::S_MAIN; U.scope = 0; U.mainsel = 3; U.key(ui::UK_OK); U.key(ui::UK_DOWN); int b0 = U.g.bgm; U.key(b0 < 8 ? ui::UK_RIGHT : ui::UK_LEFT); int b1 = U.g.bgm; ui::Act ra = U.key(ui::UK_CANCEL); int b2 = U.g.bgm; gh_log("T52E conf_restore before=%d changed=%d after=%d act=%d -> %s", b0, b1, b2, (int)ra.t, (b0 != b1 && b0 == b2) ? "PASS" : "FAIL"); U.scr = ui::S_MAIN; U.in_game = false; }
    U.in_game = false; U.scr = ui::S_MAIN;
    { /* r017 화면: 회색 목록 · 판정 · 작품 추가 · 창 배율 · 배속 중 소리 */
        if (U.games.size() >= 4) {
            ui::UiGame & a = U.games[1]; a.grade = 'A'; a.jshort = "확인됨 (DB 일치)"; a.judge = "원판과 같은 파일 — " + a.title + " (DB 일치)"; a.metasrc = "DB"; a.company = "컴파일"; a.year = "1995";
            ui::UiGame & b = U.games[3]; b.grade = 'C'; b.block = "마우스 필수 (아직 미지원)"; b.jshort = "후보 · 동급생2"; b.orig = "동급생2"; b.judge = "원판 후보 동급생2 [HD] (확실하지 않음) — 크기 1/1 · 제목 유사 60%"; b.metasrc = "롬셋 폴더"; b.autoreg = true; b.company = "엘프";
            ui::UiGame L = U.games[1]; L.id = "daimado95"; L.title = "대마도전략물어'95 (디스크 스테이션 Vol.8)"; L.grade = 'B'; L.jshort = "추정 (원판 대마도전략물어'95의 변형)"; L.orig = "대마도전략물어'95"; L.autoreg = true;
            L.judge = "원판 대마도전략물어'95 [FD]의 변형(패치판 추정) — 디스크 구성 일치, 크기 다름"; L.metasrc = "TOSEC+롬셋 폴더"; L.company = "디스크 스테이션 편집부 (긴 제작사 이름 시험)"; L.year = "1995"; L.block.clear(); L.thmb.clear();
            ui::UiGame N = U.games[1]; N.id = "nadia"; N.title = "후지의 바다의 나디아"; N.grade = 'A'; N.jshort = "확인됨 (DB 일치)"; N.judge = "원판과 같은 파일 — 후지의 바다의 나디아 [FD] (DB 일치)"; N.metasrc = "롬셋 폴더"; N.company = "Gainax"; N.year = "1991"; N.block = "CD 작품 (CD 미지원)"; N.autoreg = true; N.thmb.clear();
            ui::UiGame D2 = U.games[1]; D2.id = "direct"; D2.title = "환세풍광전"; D2.grade = 'A'; D2.jshort = "직접 지정 · DB 일치"; D2.judge = "game.ini 로 직접 지정 — 파일은 DB의 '환세풍광전' 과 일치"; D2.metasrc.clear(); D2.company.clear(); D2.year.clear(); D2.autoreg = false; D2.block.clear(); D2.thmb.clear();
            U.games.push_back(L); U.games.push_back(N); U.games.push_back(D2);
        }
        U.wmax = 3; U.wmult = 2; U.in_game = false; U.scr = ui::S_PICK; U.pick = 1; shot("r18_pick_judged");
        if (U.games.size() >= 7) {
            U.pick = (int)U.games.size() - 3; U.ts_ms = 0; shot("r18_pick_longtitle_t0"); U.ts_ms = 1000; shot("r18_pick_longtitle_marq_a"); U.ts_ms = 4000; shot("r18_pick_longtitle_marq_b"); U.ts_ms = 0;
            U.pick = (int)U.games.size() - 2; shot("r18_pick_nadia_greybar"); U.pick = (int)U.games.size() - 1; shot("r18_pick_direct_dbmatch"); U.pick = 3; shot("r18_pick_blocked_grey");
            {   /* r019: 마르키 여백 · 차단작(제목 흰색 · 사유만 주황) · 함께 있는 이미지 */
                int lt = (int)U.games.size() - 3; U.pick = lt; U.ts_ms = 2500; shot("r19_pick_longtitle_marq_c"); U.ts_ms = 6000; shot("r19_pick_longtitle_marq_d"); U.ts_ms = 0;
                ui::UiGame P = U.games[1]; P.id = "nadia_h"; P.title = "신비한 바다의 나디아 파란물의 비밀"; P.block.clear(); P.company = "Gainax"; P.year = "1991"; P.metasrc = "DB+롬셋 폴더"; P.autoreg = true;
                P.more = "나디아 Disk A (FD 1장) | 나디아 Option Disk (FD 1장)"; P.disknote = "HDD  함께 있는 이미지: 나디아 Disk A (FD 1장) / 나디아 Option Disk (FD 1장)";
                U.games.push_back(P); U.pick = (int)U.games.size() - 1; shot("r19_pick_companions");
                U.games.back().block = "마우스 필수 (아직 미지원)"; shot("r19_pick_blocked_white_title"); U.games.pop_back();
            }
        }
        U.pick = (int)U.games.size(); shot("r18_pick_add_row");
        U.key(ui::UK_OK); U.add_log = { "폴더 추가: 추가 2 · 이미 있음 1 · 건너뜀 0 (실행 불가 1)", "+ 동급생2  B 변형 추정 · 동급생2 등록함", "+ 샘플 CD 작품  - 등록함 (실행 불가: CD 작품 (CD 미지원))", "이미 있음은 목록에 표시하지 않습니다." }; U.gamefolder = "D:\\PC98\\게임 폴더"; U.sel = 2; shot("r17_add_dialog");
        U.key(ui::UK_CANCEL);
        U.scr = ui::S_SCREEN; U.sel = 6; U.wmult = 2; U.wmax = 3; shot("r17_screen_x2_of_3_grey45");
        ui::Act a1 = U.key(ui::UK_RIGHT); bool okR = a1.t == ui::A_WINMULT && a1.arg == 3 && U.wmult == 3; shot("r17_screen_x3");
        ui::Act a2 = U.key(ui::UK_RIGHT); bool okR2 = a2.t == ui::A_NONE && U.wmult == 3;
        U.wmult = 0; U.winw = 1000; U.winh = 700; shot("r17_screen_custom");
        int up = U.wm_next(1), dn = U.wm_next(-1);
        U.scr = ui::S_CONF; U.sel = 7; U.in_game = false; shot("r17_conf_boost_sound");
        U.scr = ui::S_MAIN; U.sel = 0;
        gh_log("UITEST wm_next: x3->right %s, x3->right-again(no x4: grey) %s, custom1000x700 -> up=x%d down=x%d %s", okR ? "OK" : "FAIL", okR2 ? "OK" : "FAIL", up, dn, (up == 2 && dn == 1) ? "OK" : "FAIL");
        /* 창 크기 이벤트 판정(순수): 우리가 정한 크기가 되돌아오면 무시 · 전체화면/최대화는 무시 · 그 외는 사용자가 끈 것 */
        Uint32 W = 0; unsigned now = 10000, until = 11500;
        bool u1 = gh_win_userresize(1280, 960, W, 1280, 960, now, until) == false, u2 = gh_win_userresize(1000, 700, W, 1280, 960, now, until) == true,
             u3 = gh_win_userresize(1000, 700, SDL_WINDOW_MAXIMIZED, 0, 0, now, until) == false, u4 = gh_win_userresize(1280, 960, W, 1280, 960, now + 2000, until) == true,
             u5 = gh_win_userresize(100, 80, W, 0, 0, now, until) == false, u6 = gh_win_userresize(1000, 700, SDL_WINDOW_FULLSCREEN_DESKTOP, 0, 0, now, until) == false;
        gh_log("UITEST userresize: ours-ignored %s, drag %s, maximized-ignored %s, after-expect-window %s, tiny-ignored %s, fullscreen-ignored %s", u1 ? "OK" : "FAIL", u2 ? "OK" : "FAIL", u3 ? "OK" : "FAIL", u4 ? "OK" : "FAIL", u5 ? "OK" : "FAIL", u6 ? "OK" : "FAIL");
        /* 설정 왕복(창 배율·사용자 지정·게임 폴더·배속 소리) */
        ui::Ui A2; A2.games = U.games; A2.wmult = 0; A2.winw = 1000; A2.winh = 700; A2.gamefolder = "D:\\한글 폴더"; A2.g.bsound = 1; A2.g.bgm = 4;
        std::string ip2 = g_uitest_dir + "\\settings_r17.ini"; ui::settings_save(A2, ip2); ui::Ui B2; B2.games = U.games; ui::settings_load(B2, ip2);
        bool r1 = B2.wmult == 0 && B2.winw == 1000 && B2.winh == 700 && B2.gamefolder == "D:\\한글 폴더" && B2.g.bsound == 1 && B2.g.bgm == 4;
        A2.wmult = 3; ui::settings_save(A2, ip2); ui::Ui B3; B3.games = U.games; ui::settings_load(B3, ip2);
        gh_log("UITEST r017 settings roundtrip: custom %s, mult3 %s (wmult=%d)", r1 ? "OK" : "FAIL", B3.wmult == 3 ? "OK" : "FAIL", B3.wmult);
        {   /* 실행 불가 사유(회색) 판정 표 */
            prof::Profile q; q.disk.assign(4, std::string()); q.disk[0] = "a.fdi"; std::map<std::string, std::string> kv; kv["mouse"] = "required"; std::string b1 = aw::block_reason(q, kv);
            kv.clear(); std::string b2 = aw::block_reason(q, kv);
            prof::Profile q2; q2.disk.assign(4, std::string()); q2.install_required = true; std::string b3 = aw::block_reason(q2, kv);
            prof::Profile q3; q3.disk.assign(4, std::string()); kv["nk_media"] = "CD"; std::string b4 = aw::block_reason(q3, kv);
            kv.clear(); kv["nk_flags"] = "inst"; std::string b5 = aw::block_reason(q, kv); kv["curated"] = "1"; std::string b6 = aw::block_reason(q, kv);
            gh_log("UITEST block_reason: mouse-required=[%s] normal=[%s] install-only=[%s] cd=[%s] hdd-install-flag=[%s] curated-ignores-flag=[%s] %s", b1.c_str(), b2.c_str(), b3.c_str(), b4.c_str(), b5.c_str(), b6.c_str(),
                   (!b1.empty() && b2.empty() && !b3.empty() && !b4.empty() && !b5.empty() && b6.empty()) ? "OK" : "FAIL");
        }
        ui::Settings dflt; gh_log("UITEST default bsound=%d (2 = 음높이 유지) %s", dflt.bsound, dflt.bsound == 2 ? "OK" : "FAIL");
    }
    { /* 배속 OSD: 게임 화면 + 오른쪽 위 ▶▶ x3 (+ 다른 안내가 겹칠 때) */
        ov::sample_picture(c, 0, 0, 640, 400); g_boost_on = true; g_bosd_on = true; g_boost_xv = 3; gh_boost_osd_draw(c, "화면 정수배", true);
        char p[700]; snprintf(p, sizeof p, "%s\\ui_%02d_boost_osd.png", g_uitest_dir.c_str(), ++n); gh_png(p, buf, 640, 400); g_boost_on = false; }
    { gh_logo_draw(c); char p[700]; snprintf(p, sizeof p, "%s\\ui_%02d_boot_logo.png", g_uitest_dir.c_str(), ++n); gh_png(p, buf, 640, 400); }   /* t52d (#129) */
    /* 설정 왕복: 전역 + 작품별 */
    ui::Ui A; A.games = U.games; A.g.shader = 2; A.g.curve = 7; A.g.bgm = 3; A.g.bmult = 2; A.has_t[2] = true; A.t[2] = A.g; A.t[2].se = 5; A.t[2].scan = 9; A.cur = 2;
    std::string ip = g_uitest_dir + "\\settings_test.ini"; ui::settings_save(A, ip);
    ui::Ui B; B.games = U.games; ui::settings_load(B, ip);
    bool ok = B.g.shader == 2 && B.g.curve == 7 && B.g.bgm == 3 && B.g.bmult == 2 && B.has_t[2] && B.t[2].se == 5 && B.t[2].scan == 9 && !B.has_t[0] && B.effective(2).se == 5 && B.effective(0).se == 8 && B.cur == 2;
    gh_log("UITEST settings roundtrip %s (effective(2).se=%d effective(0).se=%d)", ok ? "OK" : "FAIL", B.effective(2).se, B.effective(0).se);
    gh_log("UITEST done: %d images in %s", n, g_uitest_dir.c_str());
}

void GFX_Events(void) {
    static unsigned int last_ev = 0;
    unsigned int now = SDL_GetTicks();
    gensei_input_tick();
    if (now != last_ev) {
        last_ev = now;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (gh_evpre(e, -1)) continue;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) kbov::release_all();   /* t46x: 창 포커스를 잃으면 잠긴 키 포함 전부 해제 */
            if (e.type == SDL_KEYDOWN && (e.key.keysym.mod & KMOD_ALT) && !g_headless && g_win) {   /* r010 화면 단축키(Alt+…): PC-98 에는 Alt 가 없어 게임 키와 겹치지 않음 */
                SDL_Keycode k = e.key.keysym.sym; bool hit = true;
                if (k == SDLK_RETURN) { bool fs = (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP; SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); SDL_ShowCursor(fs ? SDL_ENABLE : SDL_DISABLE); gh_log("SCREEN fullscreen=%d", fs ? 0 : 1); gh_osd(fs ? phr_("창 모드") : phr_("전체화면")); }
                else if (k == SDLK_1) { g_fit = FIT_43; gh_log("SCREEN fit=4:3"); gh_osd(phr_("화면 4:3")); }
                else if (k == SDLK_2) { g_fit = FIT_1610; gh_log("SCREEN fit=16:10"); gh_osd(phr_("화면 16:10")); }
                else if (k == SDLK_3) { g_fit = FIT_INT; gh_log("SCREEN fit=integer"); gh_osd(phr_("화면 정수배")); }
                else if (k == SDLK_m) { g_mouse_free = !g_mouse_free; gh_log("HOTKEY 마우스 가둠 %s", g_mouse_free ? "해제" : "복귀"); gh_osd(g_mouse_free ? phr_("마우스 가둠 해제 (Alt+M 으로 다시 가둠)") : phr_("마우스 가둠 복귀")); }   /* t43m: 가둠 해제키 */
                else if (k == SDLK_s) { g_shader = (g_shader + 1) % SH_N; gh_log("SCREEN shader=%d", (int)g_shader); static const char * const sn[SH_N] = { "nearest", "sharp", "crt" }; gh_osd(std::string(phr_("셰이더 ")) + sn[g_shader % SH_N]); }
                else if (k == SDLK_UP) { float c = g_curve + 0.01f; g_curve = c > 0.10f ? 0.10f : c; gh_log("SCREEN curve=%.2f", (double)g_curve); { char b[32]; snprintf(b, sizeof b, "곡면 %.2f", (double)g_curve); gh_osd(b); } }
                else if (k == SDLK_DOWN) { float c = g_curve - 0.01f; g_curve = c < 0.0f ? 0.0f : c; gh_log("SCREEN curve=%.2f", (double)g_curve); { char b[32]; snprintf(b, sizeof b, "곡면 %.2f", (double)g_curve); gh_osd(b); } }
                else if (k == SDLK_F5) { if (g_boot_tick) { std::string p = gh_slot_path(); bool ok = gh_do_save(p, false); gh_log("HOTKEY save slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 에 저장했습니다") : phr_("저장에 실패했습니다"), 2000); } }
                else if (k == SDLK_F9) { if (g_boot_tick) { std::string p = gh_slot_path(); bool ok = gh_do_load(p); gh_log("HOTKEY load slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 을 불러왔습니다") : phr_("불러오기에 실패했습니다"), 2000); } }
                else if (k == SDLK_k) { if (!g_headless && g_use_gl && g_boot_tick) gh_kb_toggle(); }   /* t46x: Alt+K = 화면 키보드 켜고 끄기 */
                else if (k == SDLK_p) { if (g_boot_tick) gh_bk_pause_set(!g_bk_paused, "Alt+P"); }   /* t47a: Alt+P = 자동 입력 일시정지/재개 */
                else if (k == SDLK_LEFTBRACKET || k == SDLK_RIGHTBRACKET) gh_wm_hotkey(e);
                else hit = false;
                if (hit) continue;
            }
            if (gh_kbd_event(e)) continue;   /* t46x: 화면 키보드·토글 버튼(메뉴 > 키보드 > 게임 순서) */
            if (!g_menu_open && gh_game_event(e)) continue;
            if (gensei_input_event(e)) continue;
            if (e.type == SDL_QUIT) { gh_log("SDL_QUIT"); g_quit = true; }
        }
        if (!g_headless) gh_wm_poll();
        { extern bool user_cursor_locked; bool lk = g_mouse_game && g_boot_tick && !g_menu_open; if (lk != user_cursor_locked) { user_cursor_locked = lk; gh_log("mouse: 코어 user_cursor_locked=%d (PC-98 버스마우스는 이 값이 참이어야 이동량을 받는다)", (int)lk); } }   /* r021: 코어 Mouse_CursorMoved 는 user_cursor_locked 일 때만 pc98_mouse_movement_apply 를 부른다 (r020 에서 커서가 안 움직인 원인) */
        if (!g_headless && g_win) { bool want = g_mouse_game && g_boot_tick && !g_menu_open && !g_mouse_free && !kbov::g_vis; if (want != g_relmouse) { g_relmouse = want; SDL_SetRelativeMouseMode(want ? SDL_TRUE : SDL_FALSE); gh_log("mouse: SDL 상대 마우스 모드 %s", want ? "켬" : "끔"); } }
        if (g_menu_req) { g_menu_req = false; gh_menu_run(); }
        if (g_boot_tick) {
            unsigned int since = now - g_boot_tick;
            if (!g_bk_started) { g_bk_started = true; g_bk_last = now; g_bk_t = since; }   /* t47a: 자동 입력 시계 — 일시정지·메뉴 시간은 빼고 흐른다 */
            { int dd = (int)(now - g_bk_last); if (dd > 0) { if (!g_bk_paused) g_bk_t += (unsigned)dd; g_bk_last = now; } }
            if (g_bkp_to && !g_bkp_a && since >= g_bkp_from) { g_bkp_a = true; gh_bk_pause_set(true, "CLI"); }
            if (g_bkp_to && !g_bkp_b && since >= g_bkp_to) { g_bkp_b = true; gh_bk_pause_set(false, "CLI"); }

            if (g_play_last && !g_menu_open) g_play_ms += now - g_play_last; g_play_last = now;
            while (g_next_mouse < g_amouse.size() && since >= g_amouse[g_next_mouse].at_ms) {   /* r020: 시험용 자동 마우스 — SDL 이벤트로 밀어 넣어 실제 입력 경로(gh_game_event)를 지나게 한다 */
                const GMouse & m = g_amouse[g_next_mouse]; SDL_Event se; memset(&se, 0, sizeof se);
                if (m.act == 0 || m.dx != 0 || m.dy != 0) { se.type = SDL_MOUSEMOTION; se.motion.xrel = (int)m.dx; se.motion.yrel = (int)m.dy; SDL_PushEvent(&se); memset(&se, 0, sizeof se); }
                if (m.act) { se.type = (m.act == 1 || m.act == 3) ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; se.button.button = (m.act <= 2) ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT; se.button.state = (m.act == 1 || m.act == 3) ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&se); }
                gh_log("automouse #%u at %u ms dx=%.0f dy=%.0f act=%d", (unsigned)g_next_mouse, since, (double)m.dx, (double)m.dy, m.act); g_next_mouse++;
            }
            while (g_next_inj < g_inj.size() && since + g_inj_base >= g_inj[g_next_inj].at) { gh_inj_run(g_inj[g_next_inj], since + g_inj_base); g_next_inj++; }   /* t48e (#648): --inject · t51a: 창 안 전환을 건너도 시험 시계가 이어지게 g_inj_base 를 더한다 */
            kbov::tick();   /* t52f: 빠른 불러오기 길게 누름(0.6초) 판정 */
            if (!g_headless && g_win && g_use_gl) gh_kbtest_tick(since);   /* t46x 시험 전용(옵션이 없으면 아무 일도 안 함) */
            gensei_vmouse_pump();   /* t34: 호스트 마우스 이동량을 20ms 마다 ±100 이하로 코어에 */
            if (g_key_down != KBD_NONE && now >= g_key_release_at) { KEYBOARD_AddKey(g_key_down, false); g_key_down = KBD_NONE; }
            if (!g_bk_paused && g_next_bk < g_prof.bootkeys.size() && g_bk_t >= g_prof.bootkeys[g_next_bk].at_ms && g_key_down == KBD_NONE) {   /* t36cd: bootkeys — 실행 항목이 정한 부팅 후 키·명령 */
                g_key_down = g_prof.bootkeys[g_next_bk].key; g_key_release_at = now + g_keyhold;
                KEYBOARD_AddKey(g_key_down, true);
                gh_log("bootkey #%u at %u ms", (unsigned)g_next_bk, g_bk_t); g_next_bk++; }
            { size_t nst = 0; for (const GKey & kk : g_prof.bootkeys) if ((size_t)kk.step > nst) nst = (size_t)kk.step;   /* t47a: 단계 OSD 줄(번호 = 편집 화면 단계 번호, 아직 안 나간 첫 키의 단계) */
              std::string bs;
              if (nst && (g_bk_osd_on || g_bk_paused)) {
                  if (g_next_bk < g_prof.bootkeys.size()) { const GKey & kk = g_prof.bootkeys[g_next_bk]; int rem = (int)kk.at_ms - (int)g_bk_t; if (rem < 0) rem = 0; char bb[96]; snprintf(bb, sizeof bb, "자동입력 %d/%u · 다음 %d.%d초 ", kk.step, (unsigned)nst, rem / 1000, (rem % 1000) / 100); bs = std::string(bb) + kk.lab; }
                  else if (g_bk_osd_on) { if (!g_bk_done_at) g_bk_done_at = now ? now : 1; if (now - g_bk_done_at < 3000) bs = phr_("자동입력 완료 ") + std::to_string(nst) + "/" + std::to_string(nst); }
                  if (g_bk_paused && !bs.empty()) bs = phr_("[일시정지] ") + bs;
              }
              g_bk_osd = bs; }

            if (g_autokeys && g_next_key < g_prof.keys.size() && since >= g_prof.keys[g_next_key].at_ms && g_key_down == KBD_NONE) {
                g_key_down = g_prof.keys[g_next_key].key; g_key_release_at = now + g_keyhold;
                KEYBOARD_AddKey(g_key_down, true);
                gh_log("autokey #%u at %u ms", g_next_key, since);
                g_next_key++;
            }
            if (g_save_ms && !g_save_done && since >= g_save_ms) {
                g_save_done = true;
                gh_dump_named("pre_save", since);
                gh_do_save(g_state_path.empty() ? gh_slot_path() : g_state_path, true);
                g_ev_tick = now ? now : 1;
            }
            if (g_load_ms && !g_load_done && since >= g_load_ms) {
                g_load_done = true;
                gh_dump_named("pre_load", since);
                gh_do_load(g_state_path.empty() ? gh_slot_path() : g_state_path);
                g_ev_tick = now ? now : 1;
            }
            if (g_resume_slot && since >= 2000) {   /* r012: UI 에서 「이어하기」로 시작 */
                int sl = g_resume_slot - 1; g_resume_slot = 0;
                bool ok = gh_do_load(gh_slot_n(g_cur_game, sl)); gh_log("RESUME slot %d %s", sl + 1, ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 ") + std::to_string(sl + 1) + phr_(" 에서 이어합니다") : std::string(phr_("이어하기에 실패했습니다")), 2000);
            }
            if (g_bt_x) {   /* 시험: 배속 비율 실측 (--boosttest 시작초,길이초,배율) */
                static unsigned w0 = 0, p0 = 0; static int ph = 0; static unsigned lastlog = 0;
                if (ph == 0 && since >= g_bt_at) { ph = 1; w0 = now; p0 = (unsigned)PIC_Ticks; for (int i = 0; i < 5; i++) if (ui::Settings().boost_x() >= 0 && (i == 0 ? 2 : i == 1 ? 3 : i == 2 ? 4 : i == 3 ? 6 : 8) == g_bt_x) g_ui.g.bmult = i; g_ui.g.bmode = 2; g_boost_hold = true; gh_boost_update(); gh_log("BOOSTTEST start wall=%u game=%u x=%d", since, p0, g_bt_x); }
                if (ph == 1 && now - lastlog >= 500) { lastlog = now; gh_log("BOOSTTEST wall+%u game+%u", now - w0, (unsigned)PIC_Ticks - p0); }
                if (ph == 1 && now - w0 >= g_bt_len) { ph = 2; unsigned dw = now - w0, dg = (unsigned)PIC_Ticks - p0; g_boost_hold = false; gh_boost_update(); gh_log("BOOSTTEST done wall=%u game=%u ratio=%.3f", dw, dg, dw ? (double)dg / dw : 0.0); }
            }
            if (g_swap_ms && !g_swap_done && since >= g_swap_ms) {   /* 시험: 실행 중 디스크 교체 */
                g_swap_done = true; auto fi = g_fdfiles.find(g_prof.id); bool ok = false;
                if (fi != g_fdfiles.end() && g_swap_idx >= 0 && g_swap_idx < (int)fi->second.size()) ok = gh_attach(g_swap_drv, fi->second[g_swap_idx].c_str(), true);
                gh_log("SWAPTEST drive=%d idx=%d %s", g_swap_drv, g_swap_idx, ok ? "OK" : "FAIL");
            }
            if (g_axistest) {   /* 시험: R2 축 이벤트를 밀어 넣어 배속 on/off 와 로그를 확인 */
                static int ph = 0; SDL_Event ev; SDL_zero(ev); ev.type = SDL_CONTROLLERAXISMOTION; ev.caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
                if (ph == 0 && since >= 2000) { ph = 1; ev.caxis.value = 30000; SDL_PushEvent(&ev); gh_log("AXISTEST push 30000"); }
                if (ph == 1 && since >= 3500) { ph = 2; ev.caxis.value = 0; SDL_PushEvent(&ev); gh_log("AXISTEST push 0"); }
            }
            if (g_caps_len) {   /* 시험: CAPS(빨리 넘김) 누름 (--capstest 시작초,길이초) */
                static int ph = 0;
                if (ph == 0 && since >= g_caps_at) { ph = 1; KEYBOARD_AddKey(KBD_capslock, true); gh_log("CAPSTEST down at %u game=%u", since, (unsigned)(PIC_Ticks - g_pic0)); }
                if (ph == 1 && since >= g_caps_at + g_caps_len) { ph = 2; KEYBOARD_AddKey(KBD_capslock, false); gh_log("CAPSTEST up at %u game=%u", since, (unsigned)(PIC_Ticks - g_pic0)); }
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
            static unsigned int next_scr = 0;
            if (next_scr < g_scrshots_ms.size() && since >= g_scrshots_ms[next_scr]) { g_scr_req = true; next_scr++; }
            if (g_setvol_b >= 0 && g_boot_tick && !g_vol_forced && since >= 3000) { g_vol_forced = true; ui::Settings sv = g_ui.effective(g_cur_game); sv.bgm = g_setvol_b; sv.se = g_setvol_s; gh_apply_volume(sv); }
            if (g_rms_from && !g_rms_started && since >= g_rms_from) { g_rms_started = true; gensei_out_sumsq = 0; gensei_out_n = 0; gh_log("RMS window start @%u ms", since); }
            static unsigned int next_shot = 0;
            if (next_shot < g_shots_ms.size() && since >= g_shots_ms[next_shot]) { gh_dump_shot(g_shots_ms[next_shot]); next_shot++; }
            { static bool n88 = false; if (!n88 && (unsigned)SegValue(cs) == 0xE800u && (reg_eip & 0xffffu) < 0x10u) { n88 = true;   /* t43k (#334 A2): N88-BASIC ROM 진입 = PC 판 미지원 — 안내 후 정상 종료 */
                gh_log("N88: E800:%04x 도달 (t=%u) — 이 작품은 N88-BASIC 필요: PC 판 미지원(노바 np2kai 경로 예정)", (unsigned)(reg_eip & 0xffffu), since); if (!g_headless) { plat::msgbox_utf8(phr_("이 작품은 N88-BASIC 필요:\nPC 판 미지원(노바 np2kai 경로 예정)"), 0, 256); } g_quit = true; } }
            static unsigned int last_stat = 0;
            if (g_dumpcode && since / 2000 != last_stat && since > 12000) {
                PhysPt base = SegPhys(cs) + (reg_eip & (cpu.code.big ? 0xffffffffu : 0xffffu)); char hx[256]; int n = 0;
                for (int i = -16; i < 32; i++) n += snprintf(hx + n, sizeof hx - n, "%02x%s", (unsigned)mem_readb(base + i), i == -1 ? "|" : " ");
                gh_log("dumpcode: CS=%04x EIP=%08x base=%08x pmode=%d big=%d IF=%d | AX=%08x BX=%08x CX=%08x DX=%08x SI=%08x DI=%08x BP=%08x SP=%08x DS=%04x ES=%04x SS=%04x | PICm=%02x PICs=%02x", (unsigned)SegValue(cs), (unsigned)reg_eip, (unsigned)base, (int)cpu.pmode, (int)cpu.code.big, (int)GETFLAG(IF), (unsigned)reg_eax, (unsigned)reg_ebx, (unsigned)reg_ecx, (unsigned)reg_edx, (unsigned)reg_esi, (unsigned)reg_edi, (unsigned)reg_ebp, (unsigned)reg_esp, (unsigned)SegValue(ds), (unsigned)SegValue(es), (unsigned)SegValue(ss), (unsigned)IO_ReadB(0x02), (unsigned)IO_ReadB(0x0a));
                gh_log("dumpcode bytes (-16..+31, '|' = CS:IP 직전): %s", hx);
                { char st[256]; int m = 0; PhysPt sp0 = SegPhys(ss) + (reg_esp & 0xffffu);
                  for (int i = 0; i < 12; i++) m += snprintf(st + m, sizeof st - m, "%04x ", (unsigned)mem_readw(sp0 + i * 2));
                  char iv[400]; int k = 0; static const int vs[] = {0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x12,0x13,0x14,0x1b};
                  for (unsigned i = 0; i < sizeof(vs) / sizeof(vs[0]); i++) k += snprintf(iv + k, sizeof iv - k, "%02x=%04x:%04x ", vs[i], (unsigned)mem_readw((PhysPt)(vs[i] * 4 + 2)), (unsigned)mem_readw((PhysPt)(vs[i] * 4)));
                  unsigned tnz = 0; for (unsigned i = 0; i < 80 * 25; i++) if (mem_readw((PhysPt)(0xA0000 + i * 2)) != 0) tnz++;
                  gh_log("dumpcode2: stack[ss:sp]=%s | ivt %s| textVRAM nonzero=%u gdc0.disp=%d gdc1.disp=%d [0827]@DS=%02x", st, iv, tnz, (int)pc98_gdc[0].display_enable, (int)pc98_gdc[1].display_enable, (unsigned)mem_readb(SegPhys(ds) + 0x827)); { extern uint8_t MOUSE_IRQ, p7fd8_8255_mouse_int_enable; extern unsigned int pc98_mouse_rate_hz; gh_log("dumpcode3: MOUSE_IRQ=%d mouse_int_enable=%d rate=%uHz", (int)MOUSE_IRQ, (int)p7fd8_8255_mouse_int_enable, pc98_mouse_rate_hz); } }
            }
            if (g_trace21 && since > 3000) {   /* r021 진단: 프레임마다 변화만 로그 (각 종류 상한) */
                extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y, p7fd9_8255_mouse_x_latch, p7fd9_8255_mouse_y_latch; extern uint8_t p7fd9_8255_mouse_sel, p7fd9_8255_mouse_latch;
                static unsigned nm = 0, np = 0, nw = 0; static int ls = -1, ll = -1, llx = 999, lly = 999, lpm = -1, lps = -1, lw = -1; static bool d1 = false, d2 = false, d3 = false;
                if (nm < 600 && ((int)p7fd9_8255_mouse_sel != ls || (int)p7fd9_8255_mouse_latch != ll || (int)p7fd9_8255_mouse_x_latch != llx || (int)p7fd9_8255_mouse_y_latch != lly)) {
                    ls = p7fd9_8255_mouse_sel; ll = p7fd9_8255_mouse_latch; llx = p7fd9_8255_mouse_x_latch; lly = p7fd9_8255_mouse_y_latch; nm++;
                    gh_log("tr21 mouse: t=%u sel=%d latch=%d latchx=%d latchy=%d x=%d y=%d CS:IP=%04x:%04x", since, ls, ll, llx, lly, (int)p7fd9_8255_mouse_x, (int)p7fd9_8255_mouse_y, (unsigned)SegValue(cs), (unsigned)reg_eip); }
                { int pm = (int)IO_ReadB(0x02), ps = (int)IO_ReadB(0x0a);
                  if (np < 300 && (pm != lpm || ps != lps)) { lpm = pm; lps = ps; np++; gh_log("tr21 pic: t=%u mask m=%02x s=%02x CS:IP=%04x:%04x", since, pm, ps, (unsigned)SegValue(cs), (unsigned)reg_eip); } }
                if (g_watch_off >= 0) { PhysPt wp = SegPhys(ds) + (PhysPt)g_watch_off; int wv = (int)mem_readb(wp);
                  if (nw < 300 && wv != lw) { nw++; gh_log("tr21 watch: t=%u DS:%04x=%02x (전 %d) DS=%04x phys=%08x CS:IP=%04x:%04x", since, g_watch_off, wv, lw, (unsigned)SegValue(ds), (unsigned)wp, (unsigned)SegValue(cs), (unsigned)reg_eip); lw = wv; } }
                if (!d1 && since > 14000) { d1 = true; gh_tr21_dump_vectors("14s"); }
                if (!d2 && since > 24000) { d2 = true; gh_tr21_dump_vectors("24s"); }
                if (!d3 && since > 40000) { d3 = true; gh_tr21_dump_vectors("40s"); }
            }
            if (g_int18trace && since >= 5000) { static bool i18 = false; if (!i18) { i18 = true; gh_install_int18trace(); } }
            if (g_segdump_ms && since >= g_segdump_ms) {   /* t30 (#267): 대기 루프를 역어셈블하려고 현재 세그먼트 4개를 통째로 저장 */
                static bool sd = false; if (!sd) { sd = true;
                    struct SG { const char * n; PhysPt b; uint16_t sel; } sgs[4] = { { "cs", SegPhys(cs), (uint16_t)SegValue(cs) }, { "ds", SegPhys(ds), (uint16_t)SegValue(ds) }, { "es", SegPhys(es), (uint16_t)SegValue(es) }, { "ss", SegPhys(ss), (uint16_t)SegValue(ss) } };
                    gh_log("segdump: t=%u pmode=%d big=%d CS:EIP=%04x:%08x DS=%04x ES=%04x SS=%04x ESP=%08x", since, (int)cpu.pmode, (int)cpu.code.big, (unsigned)SegValue(cs), (unsigned)reg_eip, (unsigned)SegValue(ds), (unsigned)SegValue(es), (unsigned)SegValue(ss), (unsigned)reg_esp);
                    for (auto & s : sgs) { std::string fn = g_outdir + "\\segdump_" + s.n + ".bin"; FILE * f = fopen(fn.c_str(), "wb"); if (!f) continue; std::vector<uint8_t> buf(0x10000); for (unsigned i = 0; i < 0x10000; i++) buf[i] = mem_readb((PhysPt)(s.b + i)); fwrite(buf.data(), 1, buf.size(), f); fclose(f); gh_log("segdump: %s sel=%04x base=%08x -> %s", s.n, (unsigned)s.sel, (unsigned)s.b, fn.c_str()); }
                    { std::string fn = g_outdir + "\\segdump_low1m.bin"; FILE * f = fopen(fn.c_str(), "wb"); if (f) { std::vector<uint8_t> buf(0x100000); for (unsigned i = 0; i < 0x100000; i++) buf[i] = mem_readb((PhysPt)i); fwrite(buf.data(), 1, buf.size(), f); fclose(f); gh_log("segdump: low 1MB -> %s", fn.c_str()); } } } }
            if (g_hist_to && since >= g_hist_from) {   /* t30: 이름 입력 화면에서 CPU 가 어디를 도는가 */
                static std::map<uint64_t, unsigned> hh; static bool hp = false; static unsigned hn = 0;
                if (since <= g_hist_to) { hh[((uint64_t)(unsigned)SegValue(cs) << 32) | (uint64_t)reg_eip]++; hn++; }
                else if (!hp) { hp = true; std::vector<std::pair<unsigned, uint64_t>> v; for (auto & kv2 : hh) v.push_back({ kv2.second, kv2.first }); std::sort(v.begin(), v.end(), [](const std::pair<unsigned, uint64_t> & a, const std::pair<unsigned, uint64_t> & b) { return a.first > b.first; });
                    gh_log("hist: 표본 %u개, 서로 다른 CS:EIP %u개 (구간 %u~%u ms)", hn, (unsigned)hh.size(), g_hist_from, g_hist_to);
                    for (size_t i = 0; i < v.size() && i < 60; i++) gh_log("hist #%02u %5u회 %04x:%08x", (unsigned)i, v[i].first, (unsigned)(v[i].second >> 32), (unsigned)(v[i].second & 0xffffffffu)); } }
            if (g_keytrace) {   /* t29 (#266): 게임이 키를 읽는가 — BDA 키 버퍼/포인터/키 상태(0x502~0x541) 변화와 PIC 마스크(2초마다) */
                static uint8_t kprev[0x40]; static bool kinit = false; static unsigned knl = 0, kst = 0; uint8_t kc[0x40]; for (int i = 0; i < 0x40; i++) kc[i] = mem_readb((PhysPt)(0x502 + i));
                if (!kinit || memcmp(kprev, kc, sizeof kc) != 0) { if (knl < 400) { knl++; char b[160]; int k = 0; for (int i = 0x22; i < 0x28; i++) k += snprintf(b + k, sizeof b - k, "%02x ", kc[i]); char b2[160]; int k2 = 0; for (int i = 0x28; i < 0x3a; i++) k2 += snprintf(b2 + k2, sizeof b2 - k2, "%02x", kc[i]);
                        gh_log("keytrace: t=%u head/tail/cnt[524..529]=%s buf[502..]=%02x%02x%02x%02x%02x%02x%02x%02x state[52a..]=%s CS:IP=%04x:%04x", since, b, kc[0], kc[1], kc[2], kc[3], kc[4], kc[5], kc[6], kc[7], b2, (unsigned)SegValue(cs), (unsigned)reg_eip); } memcpy(kprev, kc, sizeof kc); kinit = true; }
                if (since / 2000 != kst) { kst = since / 2000; gh_log("keytrace pic: t=%u master mask=%02x slave mask=%02x (IRQ1=키보드: 마스크 비트1)", since, (unsigned)IO_ReadB(0x02), (unsigned)IO_ReadB(0x0a)); } }
            if (g_mouse_game && since / 2000 != last_stat) { extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y, p7fd9_8255_mouse_x_latch, p7fd9_8255_mouse_y_latch; extern uint8_t p7fd9_8255_mouse_sel, p7fd9_8255_mouse_latch; gh_log("mousereg: x=%d y=%d latchx=%d latchy=%d sel=%u latch=%u", (int)p7fd9_8255_mouse_x, (int)p7fd9_8255_mouse_y, (int)p7fd9_8255_mouse_x_latch, (int)p7fd9_8255_mouse_y_latch, (unsigned)p7fd9_8255_mouse_sel, (unsigned)p7fd9_8255_mouse_latch); }   /* r021 진단: 게임이 버스마우스 레지스터를 읽어 가는가 */
            if (since / 2000 != last_stat) { last_stat = since / 2000; gh_log("stat: wall=%u ms game=%u ms (ratio %.3f) frames=%llu starts=%llu aborts=%llu cycles=%d auto=%d pmode=%d pct=%d CS:IP=%04x:%04x", since, (unsigned)(PIC_Ticks - g_pic0), since ? (double)(PIC_Ticks - g_pic0) / (double)since : 0.0, g_frames, g_starts, g_aborts, (int)CPU_CycleMax, (int)CPU_CycleAutoAdjust, (int)cpu.pmode, (int)CPU_CyclePercUsed, SegValue(cs), (unsigned)reg_ip);
                gh_log("audio: dev='%s' freq=%d blk=%d cb=%u underrun=%u drop=%u", gensei_audio_dev ? gensei_audio_dev : "(default)", gensei_audio_obt_freq, gensei_audio_obt_samples, gensei_mix_cb, gensei_mix_under, gensei_mix_drop); gh_log("OUTRMS samples=%llu rms=%.2f (창 시작 %u ms)", gensei_out_n, gensei_out_n ? sqrt(gensei_out_sumsq / (double)gensei_out_n) : 0.0, g_rms_from); }
            if (g_run_ms && since >= g_run_ms) { gh_log("run time reached"); g_quit = true; }
        }
        if (g_inwin_wait) {   /* t51a: 창 안 전환 뒤 부팅 도달 신호(g_boot_tick) 대기 — 8초 안에 없으면 실패로 보고 나간다(폴백은 gh_inwin_cycle) */
            if (g_boot_tick) { g_inwin_wait = false; }
            else if (SDL_GetTicks() - g_inwin_t0 > 8000u) { g_inwin_wait = false; g_inwin_timeout = true; g_quit = true; }
        }
        if (g_quit) throw (int)0;
    }
}

/* ------------------------------------------------------------------ misc */
#if defined(WIN32)
#include <dbghelp.h>
static void gh_dump_stubs(void);
/* r011b: 실패는 로그 파일로만 끝내지 않고 메시지 창으로 이유를 보인다(헤드리스 시험 실행은 제외) */
static volatile bool g_eexit_seen = false;
static void gh_msgbox(const char * utf8text) {
    if (g_headless) return;
    plat::msgbox_utf8(utf8text, 1, 1024);
}
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
    gh_msgbox(phr_("프로그램 내부에서 예기치 않은 오류가 났습니다.\n로그 폴더의 kyuhachi_log.txt 를 보내 주세요."));
    TerminateProcess(GetCurrentProcess(), 0xC5);
    return EXCEPTION_EXECUTE_HANDLER;
}
static int g_traceexc = 0, g_traceexc_n = 0;   /* t37cd: --traceexc — 잡힌 C++ 예외(bad_alloc 등)의 던진 자리를 로그로 (EMS 가 왜 죽는지 찾는 용도) */
static LONG CALLBACK gh_veh(EXCEPTION_POINTERS * ep) {
    if (!g_traceexc || ep->ExceptionRecord->ExceptionCode != 0xE06D7363 || g_traceexc_n >= 6) return EXCEPTION_CONTINUE_SEARCH;
    g_traceexc_n++;
    gh_log("C++EXC #%d (E06D7363)", g_traceexc_n);
    HANDLE proc = GetCurrentProcess(); static bool symok = false;
    if (!symok) { SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS); SymInitialize(proc, NULL, TRUE); symok = true; }
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf; memset(&sf, 0, sizeof(sf));
    sf.AddrPC.Offset = ctx.Rip; sf.AddrPC.Mode = AddrModeFlat; sf.AddrFrame.Offset = ctx.Rsp; sf.AddrFrame.Mode = AddrModeFlat; sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 28; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
        if (!sf.AddrPC.Offset) break;
        char sbuf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO * sym = (SYMBOL_INFO *)sbuf; memset(sym, 0, sizeof(SYMBOL_INFO)); sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
        DWORD64 d64 = 0; DWORD d32 = 0; IMAGEHLP_LINE64 ln; memset(&ln, 0, sizeof(ln)); ln.SizeOfStruct = sizeof(ln);
        bool hs = SymFromAddr(proc, sf.AddrPC.Offset, &d64, sym) != 0; bool hl = SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &d32, &ln) != 0;
        gh_log("  #%02d %s+0x%llx %s:%u", i, hs ? sym->Name : "?", (unsigned long long)d64, hl ? ln.FileName : "", hl ? (unsigned)ln.LineNumber : 0u);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static DWORD WINAPI gh_boxkiller(LPVOID) {
    for (;;) {
        plat::sleep_ms(200);
        if (plat::close_window_titled("E_Exit")) { g_eexit_seen = true; if (g_log) { fprintf(g_log, "HOST E_Exit message box closed by watchdog\n"); fflush(g_log); } }
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
    if (g_stage_go) { g_stage_go = false; gh_spawn_stage(g_stage_next); }   /* t49a (#845): 다음 단계 = 새 프로세스 */
    if (!g_headless && g_wm_dirty) { g_wm_dirty = false; ui::settings_save(g_ui, g_cfgpath); }
    g_shacache.save();
    if (!g_headless && g_boot_tick && gh_play_commit()) ui::settings_save(g_ui, g_cfgpath);
    gh_dump_stubs();
    if (g_log) { fflush(g_log); fclose(g_log); g_log = NULL; }
    fflush(NULL);
#if defined(WIN32)
    if (rc >= 2 && rc <= 4) gh_msgbox(rc == 2 ? (g_errmsg.empty() ? std::string(phr_("게임 디스크 이미지를 열 수 없습니다.\nimg 폴더에 이미지가 있는지 확인해 주세요. (자세한 내용: logs 폴더의 kyuhachi_log.txt)")) : g_errmsg + phr_("\n(자세한 내용: logs 폴더의 kyuhachi_log.txt)")).c_str()
                                   : phr_("실행 중 오류로 종료했습니다.\n(자세한 내용: logs 폴더의 kyuhachi_log.txt)"));
    else if (g_eexit_seen) gh_msgbox(phr_("에뮬레이터 내부 오류로 종료했습니다.\n(자세한 내용: logs 폴더의 kyuhachi_log.txt)"));
#endif
    _exit(rc);
}

/* ---- t51a (#878 묶음 2): 창 안 전환. 일시정지 메뉴의 「다음 단계」·「메인 메뉴로」 를 창·프로세스 그대로 처리한다.
   흐름: RunMachine 을 빠져나온 직후(에뮬이 안 도는 때) 정리 → 「불러오는 중」 → (메인 메뉴면 같은 창에서 gh_ui_main) → 새 프로필 읽기·범위 판정 → 디스크 다시 연결 →
   원본 reboot 순서(CPU_Snap_Back_To_Real_Mode · CPU_Snap_Back_Forget · VM_EVENT_RESET · MAPPER_ReleaseAllKeys/LosingFocus · VM_EVENT_RESET_END) → main 이 RunMachine 을 다시 부른다.
   실패(디스크 연결 · 프로필 · 범위 밖 · 예외 · 8초 부팅 신호 없음)는 현행 재실행 경로(gh_spawn_stage / gh_spawn_menu / gh_spawn_game + gh_finish 의 _exit)로 폴백 — 새 창을 만들기 전에 이 프로세스가 끝나므로 창은 늘 하나. */
static void gh_spawn_game(const std::string & id, const std::string & entry, int resume) {   /* 메뉴에서 고른 작품을 새 프로세스로(폴백 전용) */
    std::string exe = plat::exe_path();
    std::string cmd = "\"" + exe + "\" \"" + id + "\" --noboot"; if (!entry.empty()) cmd += " --entry \"" + aw::u8_to_acp(entry) + "\""; if (resume > 0) cmd += " --resume " + std::to_string(resume);
    unsigned long perr = 0;
    if (plat::spawn(exe, cmd, g_exedir, &perr)) { gh_log("spawn game OK: %s", cmd.c_str()); }
    else gh_log("spawn game FAIL err=%lu", (unsigned long)perr);
}
static std::string gh_conf_sig(const GProfile & p) {   /* 시작 때 한 번만 코어 설정에 들어가는 프로필 값(cycles · 글꼴 · extra · sound). 다르면 같은 창 재부팅으로 못 바꾸므로 폴백 */
    auto kv = [&](const char * k) { auto it = p.kv.find(k); return it == p.kv.end() ? std::string() : it->second; };
    return p.cycles + "|" + p.font + "|" + kv("font_kind") + "|" + p.extra + "|" + kv("sound");
}
static bool gh_prof_isdos(const GProfile & p) {
    bool dosfd = p.kv.count("dosfd") && p.kv.at("dosfd") == "1" && (p.disk[0] != NULL || p.disk[1] != NULL) && p.disk[2] == NULL && p.disk[3] == NULL;
    return dosfd || (p.disk[0] == NULL && p.disk[1] == NULL && p.disk[2] == NULL && p.disk[3] == NULL && !p.cd.empty());
}
static unsigned long long gh_mem_ws(void) {   /* 작업 집합(바이트). psapi 링크 없이 kernel32 의 K32GetProcessMemoryInfo 를 찾아 쓴다 */
    struct PMC { DWORD cb, PageFaultCount; SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage; };
    typedef BOOL (WINAPI * Fn)(HANDLE, PMC *, DWORD); static Fn fn = (Fn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetProcessMemoryInfo");
    PMC m; memset(&m, 0, sizeof m); m.cb = sizeof m; if (fn && fn(GetCurrentProcess(), &m, sizeof m)) return (unsigned long long)m.WorkingSetSize; return 0ull;
}
static void gh_inwin_stat(const char * tag) {   /* 시험 판정용: 프로세스 ID · 창 핸들 · 핸들 수 · 메모리 · 창 크기/전체화면/맞춤/셰이더/배속/패드 */
    DWORD hc = 0; GetProcessHandleCount(GetCurrentProcess(), &hc); int ww = 0, wh = 0; unsigned fl = 0; if (g_win) { SDL_GetWindowSize(g_win, &ww, &wh); fl = SDL_GetWindowFlags(g_win); }
    gh_log("INWIN %s pid=%lu win=%p glctx=%p handles=%lu ws=%lluKB boot_n=%d inwin_n=%d size=%dx%d fs=%d fit=%d shader=%d boost=%d joys=%d fb=%dx%d", tag, (unsigned long)GetCurrentProcessId(), (void *)g_win, (void *)g_glctx, (unsigned long)hc, gh_mem_ws() / 1024ull, g_boot_n, g_inwin_n, ww, wh, (fl & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP ? 1 : 0, (int)g_fit, (int)g_shader, (int)g_boost_on, SDL_NumJoysticks(), g_fbw, g_fbh);
}
static bool gh_inwin_loading(unsigned ms) {   /* 검은 화면 + 「불러오는 중」 한 줄. true = 도중에 창 닫기 요청 */
    if (g_headless || !g_use_gl) return false;
    static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; bool quit = false; unsigned t0 = SDL_GetTicks();
    std::string s = phr_("불러오는 중..."); int x = (640 - ov::tw(s)) / 2; if (x < 0) x = 0;
    do {
        SDL_Event e; while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; else gh_wm_hotkey(e); }
        gh_wm_poll();
        c.rect(0, 0, 640, 400, ov::K); ov::text(c, x, 192, s, ov::WH, ov::K);
        gh_publish(buf, 640, 400);
        if (g_inwin_testfail == "shot" && SDL_GetTicks() - t0 > 200u) { static int shot_n = 0; if (shot_n < 99) { char pp[1024]; snprintf(pp, sizeof pp, "%s\\inwin_loading_%02d.png", g_outdir.c_str(), shot_n++); gh_png(pp, buf, 640, 400); gh_log("INWIN loading 화면 저장 %s", pp); } t0 -= 100000u; }   /* 시험: 불러오는 중 화면 1장 저장(그 뒤 바로 끝냄) */
        SDL_Delay(16);
    } while (!quit && SDL_GetTicks() - t0 < ms);
    return quit;
}
/* true = 같은 창에서 다시 부팅 준비 끝(main 이 RunMachine 재진입) · false = 폴백 준비를 마침(main 이 gh_finish) */
static bool gh_inwin_cycle(void) {
    int kind = g_inwin_req; g_inwin_req = 0; bool tmo = g_inwin_timeout; g_inwin_timeout = false; g_inwin_wait = false;
    auto fail = [&](const std::string & w) -> bool {
        gh_log("STAGE 창 안 전환 실패(%s) → 재실행 (종류=%d)", w.c_str(), g_inw_kind);
        if (g_inw_kind == 2) { if (g_inw_menu_pick) gh_spawn_game(g_inw_sel_id, prof::g_entry_req, g_resume_slot); else gh_spawn_menu(); }
        else { g_stage_next = g_inwin_target; g_stage_go = true; }
        return false; };
    if (tmo) return fail("부팅 신호가 8초 안에 없음");
    g_inw_kind = kind; g_inw_menu_pick = false;
    try {
        gh_inwin_stat(kind == 2 ? "전환 시작(메인 메뉴)" : "전환 시작(다음 단계)");
        gh_log("INWIN rms 이전 구간 samples=%llu rms=%.2f", gensei_out_n, gensei_out_n ? sqrt(gensei_out_sumsq / (double)gensei_out_n) : 0.0);
        { unsigned nw = SDL_GetTicks(); if (g_boot_tick) g_inj_base += nw - g_boot_tick; g_inj_stamp = nw | 1u; g_boot_tick = 0; }   /* 시험 inject 시계는 전환을 건너 이어 간다(제품 동작 무관) */
        gh_play_commit(); if (!g_headless) ui::settings_save(g_ui, g_cfgpath);
        kbov::release_all(); fflush(NULL);   /* 디스크 작업 사본에 쓰던 내용을 먼저 내보낸다(새 연결이 같은 파일을 다시 열기 때문) */
        std::string sel_id = g_pid;
        if (kind == 2) {
            std::string chosen; bool nb = g_noboot; g_noboot = true; g_quit = false; g_resume_slot = 0; prof::g_entry_req.clear();
            bool ok = gh_ui_main(chosen); g_noboot = nb;
            if (!ok) { gh_log("ui: quit (창 안 메인 메뉴)"); gh_finish(0); }
            sel_id = chosen; g_inw_sel_id = chosen; g_inw_menu_pick = true;
            gh_log("INWIN 메인 메뉴에서 고름: '%s' 항목='%s' resume=%d", chosen.c_str(), prof::g_entry_req.c_str(), g_resume_slot);
        } else prof::g_entry_req = g_inwin_target;
        GProfile np; if (!gh_profile(sel_id, np)) return fail("프로필 읽기");
        if (g_dosmode || g_dosfd) return fail("지금 내장 DOS/CD 부팅(범위 밖)");
        if (g_demo) return fail("--demo 시작(범위 밖)");
        if (gh_prof_isdos(np)) return fail("다음이 내장 DOS/CD 부팅(범위 밖)");
        if (np.disk[2] || np.disk[3] || g_prof.disk[2] || g_prof.disk[3] || imageDiskList[2] || imageDiskList[3]) return fail("HDD 칸 있음(범위 밖)");
        if (!np.disk[0] && !np.disk[1]) return fail("부팅 가능한 디스크 없음");
        if (np.bootletter != 'A' && np.bootletter != 'B') return fail("부팅 드라이브가 A/B 가 아님");
        for (int i = 0; i < 2; i++) if (imageDiskList[i] && !np.disk[i]) return fail("다음 항목의 칸이 지금보다 적음");
        if (gh_conf_sig(np) != gh_conf_sig(g_prof)) return fail("코어 설정(cycles·글꼴·extra·sound)이 지금과 다름");
        if (g_inwin_testfail == "attach") return fail("시험 주입: 디스크 연결 실패");
        if (gh_inwin_loading(450)) { gh_log("창 닫기 요청(전환 중)"); gh_finish(0); }
        g_prof = np; g_pid = sel_id;
        for (size_t i = 0; i < g_ui.games.size(); i++) if (g_ui.games[i].id == sel_id) g_cur_game = (int)i;
        if (!g_imgdir_given && !g_prof.imgdir.empty()) g_imgdir = g_prof.imgdir;
        g_disks.clear(); for (int i = 0; i < 4; i++) g_disk_ix[i] = -1;
        for (int i = 0; i < 2; i++) if (g_prof.disk[i] && !(g_noB && i == 1)) if (!gh_attach(i, g_prof.disk[i])) return fail(std::string("디스크 연결 실패: ") + g_prof.disk[i]);
        if (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) gh_rebuild_fds(g_prof, g_ui.games[g_cur_game]);
        { auto mk = g_prof.kv.find("mouse"); g_mouse_game = (mk != g_prof.kv.end() && !mk->second.empty() && mk->second != "none") || !g_amouse.empty(); gh_log("mouse: 이 작품의 마우스 = %s (창 안 전환)", mk == g_prof.kv.end() ? "(미지정)" : mk->second.c_str()); }
        gh_apply_profile_input(g_prof);
        g_ui.cur = g_cur_game;
        if (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) {
            gh_refresh_game_slots(g_cur_game); gh_apply_settings(g_ui.effective(g_cur_game), false);
            if (g_win) SDL_SetWindowTitle(g_win, gh_window_title(g_ui.games[g_cur_game], g_titlelegacy).c_str());
        }
        gh_stage_init();
        if (g_inwin_testfail == "throw") throw std::runtime_error("시험 주입: 예외");
        /* 원본 reboot 순서 (sdlmain.cpp L10601~10628) */
        void CPU_Snap_Back_To_Real_Mode(); void CPU_Snap_Back_Forget(); void MAPPER_ReleaseAllKeys(); void MAPPER_LosingFocus(void);
        CPU_Snap_Back_To_Real_Mode(); CPU_Snap_Back_Forget();
        DispatchVMEvent(VM_EVENT_RESET);
        MAPPER_ReleaseAllKeys(); MAPPER_LosingFocus();
        /* 호스트 쪽 부팅 상태 되돌림(설계 §4) */
        g_boot_tick = 0; g_pon_tick = SDL_GetTicks() | 1u; g_pon_pic = PIC_Ticks; g_loading = true;
        g_next_bk = 0; g_bk_started = false; g_bk_t = 0; g_bk_paused = false; g_bk_last = SDL_GetTicks(); g_bkp_a = g_bkp_b = false;
        g_next_key = 0; g_play_last = 0; g_menu_open = false; g_key_down = KBD_NONE; g_swap_done = false; kbov::wake();
        gensei_out_sumsq = 0.0; gensei_out_n = 0ull;
        g_quit = false; g_boot_n++; g_inwin_n++;
        DispatchVMEvent(VM_EVENT_RESET_END);
        g_inwin_wait = true; g_inwin_t0 = SDL_GetTicks();
        gh_log("STAGE 창 안 전환: 같은 창에서 리셋 n=%d 작품='%s' 항목='%s' A=%s B=%s", g_inwin_n, sel_id.c_str(), prof::g_entry_req.c_str(), g_prof.disk[0] ? g_prof.disk[0] : "-", g_prof.disk[1] ? g_prof.disk[1] : "-");
        gh_inwin_stat("리셋 직후");
        return true;
    } catch (const std::exception & ex) { return fail(std::string("예외 ") + ex.what()); }
    catch (...) { return fail("예외"); }
}

int main(int argc, char * argv[]) {
    SDL_SetMainReady();
    for (int i = 0; i < argc; i++) g_argv_saved.push_back(argv[i]);   /* t49a (#845): 단계 전환 때 같은 인자로 다시 띄운다 */
    std::string id;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--imgdir" && i + 1 < argc) { g_imgdir = argv[++i]; g_imgdir_given = true; }
        else if (a == "--out" && i + 1 < argc) g_outdir = argv[++i];
        else if (a == "--headless") g_headless = true;
        else if (a == "--hidewin") g_hidewin = true;
        else if (a == "--gltime") g_gltime = true;
        else if (a == "--save" && i + 1 < argc) g_save_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--load" && i + 1 < argc) g_load_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--state" && i + 1 < argc) g_state_path = argv[++i];
        else if (a == "--sec" && i + 1 < argc) g_run_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--shot" && i + 1 < argc) { char * s = argv[++i]; while (*s) { g_shots_ms.push_back((unsigned)(atof(s) * 1000.0)); while (*s && *s != ',') s++; if (*s == ',') s++; } }
        else if (a == "--windowed") g_windowed = true;
        else if (a == "--fullscreen") g_windowed = false;
        else if (a == "--noboot") g_noboot = true;
        else if (a == "--uitest" && i + 1 < argc) g_uitest_dir = argv[++i];
        else if (a == "--logoshot" && i + 1 < argc) g_logoshot = argv[++i];
        else if (a == "--boosttest" && i + 1 < argc) { double s0 = 0, l0 = 0; int x0 = 3; if (sscanf(argv[++i], "%lf,%lf,%d", &s0, &l0, &x0) >= 2) { g_bt_at = (unsigned)(s0 * 1000.0); g_bt_len = (unsigned)(l0 * 1000.0); g_bt_x = x0; } }
        else if (a == "--demo") g_demo = true;
        else if (a == "--entry" && i + 1 < argc) { g_entry_cli = argv[++i]; prof::g_entry_req = aw::acp_to_u8(g_entry_cli); }   /* t36cd: 실행 항목 이름 */
        else if (a == "--userini" && i + 1 < argc) prof::g_userini_override = argv[++i];   /* t39: 시험 실행용 user.ini 경로 */
        else if (a == "--uikeys" && i + 1 < argc) g_uikeys_file = argv[++i];
        else if (a == "--hangultest" && i + 1 < argc) {   /* t48b (#628 3 ⒜): 조합기 단위 시험표를 TSV 로 쓰고 끝낸다(창·코어 없음) */
            int hp = 0, ht = 0; std::string tsv = hg::selftest(hp, ht); FILE * hf = fopen(argv[++i], "wb"); if (hf) { fwrite(tsv.data(), 1, tsv.size(), hf); fprintf(hf, "합계\t%d / %d\n", hp, ht); fclose(hf); }
            return hp == ht ? 0 : 1; }
        else if (a == "--titletest") g_titletest = true;
        else if (a == "--helplog") g_helplog = true;
        else if (a == "--inwin" && i + 1 < argc) g_inwin_enabled = atoi(argv[++i]) != 0;   /* t51a: 0 = 창 안 전환 끔(현행 재실행 경로만) */
        else if (a == "--inwintest" && i + 1 < argc) g_inwin_testfail = argv[++i];   /* t51a 시험: attach | noboot | throw */
        else if (a == "--resume" && i + 1 < argc) g_resume_slot = atoi(argv[++i]);   /* t51a: 폴백 재실행(메뉴에서 고른 이어하기 슬롯) */
        else if (a == "--nostage") g_nostage = true;   /* t49a: 부트 순서(단계) 기능 끄기 — 시험용 */
        else if (a == "--titlelegacy") g_titlelegacy = true;
        else if (a == "--axistest") g_axistest = true;
        else if (a == "--swaptest" && i + 1 < argc) { double s0 = 0; int d0 = 0, i0 = 0; if (sscanf(argv[++i], "%lf,%d,%d", &s0, &d0, &i0) == 3) { g_swap_ms = (unsigned)(s0 * 1000.0); g_swap_drv = d0; g_swap_idx = i0; } }
        else if (a == "--capstest" && i + 1 < argc) { double s0 = 0, l0 = 0; if (sscanf(argv[++i], "%lf,%lf", &s0, &l0) == 2) { g_caps_at = (unsigned)(s0 * 1000.0); g_caps_len = (unsigned)(l0 * 1000.0); } }
        else if (a == "--winsize" && i + 1 < argc) { int w = 0, h = 0; if (sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 64 && h > 64) { g_winw = w; g_winh = h; g_winsize_user = true; } }
        else if (a == "--fit" && i + 1 < argc) { std::string v = argv[++i]; g_fit = v == "1610" ? FIT_1610 : (v == "int" ? FIT_INT : FIT_43); }
        else if (a == "--shader" && i + 1 < argc) { std::string v = argv[++i]; g_shader = v == "crt" ? SH_CRT : (v == "nearest" ? SH_NEAREST : SH_SHARP); }
        else if (a == "--curve" && i + 1 < argc) { float c = (float)atof(argv[++i]); g_curve = c < 0 ? 0 : (c > 0.10f ? 0.10f : c); }
        else if (a == "--shaderdir" && i + 1 < argc) g_shaderdir = argv[++i];
        else if (a == "--autokeys") g_autokeys = true;
        else if (a == "--bkosd" && i + 1 < argc) g_bk_osd_on = atoi(argv[++i]) != 0;   /* t47a */
        else if (a == "--bkpause" && i + 1 < argc) { unsigned f = 0, t = 0; if (sscanf(argv[++i], "%u,%u", &f, &t) == 2 && t > f) { g_bkp_from = f; g_bkp_to = t; } }   /* t47a 시험용: 부팅 후 f ms 에 정지, t ms 에 재개 */

        else if (a == "--scrshot" && i + 1 < argc) { char * s = argv[++i]; while (*s) { g_scrshots_ms.push_back((unsigned)(atof(s) * 1000.0)); while (*s && *s != ',') s++; if (*s == ',') s++; } }
        else if (a == "--setvol" && i + 1 < argc) { int b = 8, se = 8; if (sscanf(argv[++i], "%d,%d", &b, &se) == 2) { g_setvol_b = b; g_setvol_s = se; } }
        else if (a == "--rmswin" && i + 1 < argc) { g_rms_from = (unsigned)(atof(argv[++i]) * 1000.0); }
        else if (a == "--mouseok") g_mouseok_cli = true;
        else if (a == "--traceexc") g_traceexc = 1;
        else if (a == "--ems") g_ems_req = 1;
        else if (a == "--noems") g_ems_req = 0;
        else if (a == "--trace21") g_trace21 = true;
        else if (a == "--keytrace") g_keytrace = true;
        else if (a == "--int18trace") g_int18trace = true;
        else if (a == "--segdump" && i + 1 < argc) g_segdump_ms = (unsigned)atoi(argv[++i]);
        else if (a == "--hist" && i + 1 < argc) { const char * s = argv[++i]; g_hist_from = (unsigned)atoi(s); const char * c = strchr(s, ','); g_hist_to = c ? (unsigned)atoi(c + 1) : g_hist_from + 10000; }
        else if (a == "--keyhold" && i + 1 < argc) g_keyhold = (unsigned)atoi(argv[++i]);
        else if (a == "--boot400") g_boot400 = true;
        else if (a == "--bda") g_bda = true;
        else if (a == "--ext14") g_ext14 = true;
        else if (a == "--ext0") g_ext0 = true;
        else if (a == "--di0") g_di0 = true;
        else if (a == "--mouseirq12") g_mirq12 = true;
        else if (a == "--watch" && i + 1 < argc) { unsigned w = 0; if (sscanf(argv[++i], "%x", &w) == 1 && w < 0x10000) g_watch_off = (int)w; }
        else if (a == "--dumpcode") g_dumpcode = true;   /* r021 진단: 2초마다 CS:IP 주변 코드 바이트·레지스터·PIC 마스크를 로그에 */
        else if (a == "--automouse" && i + 1 < argc) gh_parse_amouse(argv[++i]);
        else if (a == "--inject" && i + 1 < argc) { g_inject_file = argv[++i]; bool ok = gh_parse_inject(g_inject_file); gh_log("--inject %s -> %s (%u 줄)", g_inject_file.c_str(), ok ? "OK" : "FAIL", (unsigned)g_inj.size()); }   /* t48e (#648): 시험 전용 입력 주입(SDL 이벤트·가상 조이스틱만) */
        else if (a == "--kbshow" && i + 1 < argc) g_kbt_show_ms = (unsigned)(atof(argv[++i]) * 1000.0);   /* t46x 시험: 초 뒤 화면 키보드 켜기 */
        else if (a == "--kbhide" && i + 1 < argc) g_kbt_hide_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--kbclick" && i + 1 < argc) gh_parse_kbclick(argv[++i]);   /* "<시작초>:STOP,GRPH,X,GRPH,KANA" */
        else if (a == "--kbbtn" && i + 1 < argc) kbov::g_cli_btn = atoi(argv[++i]) ? 1 : 0;
        else if (a == "--ovbtn" && i + 1 < argc) kbov::g_cli_ov = atoi(argv[++i]) ? 1 : 0;   /* t52f 시험: 오버레이 버튼 줄 켬/끔 */
        else if (a == "--ovstrong") kbov::g_force_strong = true;   /* t52f 시험: 버튼 줄을 항상 진하게 */
        else if (a == "--lang" && i + 1 < argc) g_cli_lang = atoi(argv[++i]);   /* t52g 시험 */
        else if (a == "--menushot") g_menushot = true;   /* t52f 시험: 메뉴 창 캡처 후 종료 */
        else if (a == "--kbshader" && i + 1 < argc) g_cli_kbsh = atoi(argv[++i]) ? 1 : 0;
        else if (a == "--savedir" && i + 1 < argc) g_savedir = argv[++i];
        else if (a == "--audiodev" && i + 1 < argc) g_audiodev = argv[++i];
        else if (a == "--listaudio") g_listaudio = true;
        else if (a == "--glmatrix" && i + 1 < argc) g_glmx_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--splash") gensei_nosplash = 0;
        else if (a == "--batest") g_batest = true;
        else if (a == "--noload") g_noload = true;
        else if (a == "--frametest" && i + 1 < argc) g_ft_dir = argv[++i];
        else if (a == "--ftms" && i + 1 < argc) g_ft_ms = (unsigned)atoi(argv[++i]);
        else if (a == "--bsound" && i + 1 < argc) g_bsound_override = atoi(argv[++i]);
        else if (a == "--nkdb" && i + 1 < argc) g_nkdb_path = argv[++i];
        else if (a == "--addwork" && i + 1 < argc) g_addwork_cli = argv[++i];
        else if (a == "--gamefolder" && i + 1 < argc) g_gamefolder_cli = argv[++i];
        else if (a == "--scanonly") g_scanonly = true;
        else if (a == "--dry") g_aw_dry = true;
        else if (a == "--report" && i + 1 < argc) g_report_dir = argv[++i];
        else if (a[0] != '-') { id = a; g_id_given = true; }
    }
    /* r011b/r012: exe 위치 기준 기본값(인자 없이 더블클릭해도 돌도록) */
    { std::string s = plat::exe_path(); size_t sl = s.find_last_of("\\/"); g_exedir = (sl == std::string::npos) ? std::string(".") : s.substr(0, sl); }
    /* t52g (#133): 화면 문구 파일은 설정(언어)을 읽은 뒤 phr_set_lang() 이 읽는다 (strings_ko/en/ja.txt · 한국어는 strings.txt 가 덮어씀) */
    const std::string & exedir = g_exedir;
    if (!plat::exists("res\\fonts\\kyuhachi_font.bmp") && !plat::exists("kyuhachi_font.bmp") && !plat::exists("res\\fonts\\pc98player_font.bmp") && !plat::exists("pc98player_font.bmp")) plat::set_cwd(exedir);   /* t52g: 새·옛 이름 모두 */
    const bool ui_mode = id.empty() && !g_headless;   /* r012: 인자 없음 = 이 실행기의 UI 로 시작 */
    if (g_imgdir == "." && plat::exists((exedir + "\\img").c_str())) g_imgdir = exedir + "\\img";
    if (g_savedir == "saves") g_savedir = exedir + "\\saves";
    g_cfgpath = exedir + "\\settings.ini";
    { prof::Ini fi; if (prof::ini_load(exedir + "\\res\\flags.ini", fi) && fi.count("flags")) { auto it = fi["flags"].find("mouse_support"); if (it != fi["flags"].end() && it->second == "1") aw::mouse_ok() = true; auto mg = fi["flags"].find("mouse_support_games"); if (mg != fi["flags"].end()) { for (auto & s : prof::split_list(mg->second)) { std::string v = s; for (auto & c : v) c = (char)tolower((unsigned char)c); while (!v.empty() && v.back() == ' ') v.pop_back(); size_t b = v.find_first_not_of(' '); if (b != std::string::npos && b) v = v.substr(b); if (!v.empty()) aw::mouse_games().insert(v); } } } if (g_mouseok_cli) aw::mouse_ok() = true;
      { prof::Profile q; q.disk.assign(4, std::string()); q.disk[0] = "a.fdi"; std::string r3[3]; const char * ids[3] = { "nadia", "ultra", "dotb" }; for (int i = 0; i < 3; i++) { std::map<std::string, std::string> kv; kv["mouse"] = "required"; kv["curated"] = "1"; kv["__id"] = ids[i]; r3[i] = aw::block_reason(q, kv); }
        { char fb[400]; snprintf(fb, sizeof fb, "flags: mouse_support=%d mouse_support_games=%d개 | 마우스 필수 판정(nadia/ultra/dotb) = [%s] / [%s] / [%s]  (빈칸 = 풀림)", (int)aw::mouse_ok(), (int)aw::mouse_games().size(), r3[0].c_str(), r3[1].c_str(), r3[2].c_str()); g_flaglog = fb; } } }
    const bool outdir_auto = (g_outdir == ".");
    if (outdir_auto) { std::string d = exedir + "\\logs"; plat::mkdir(d.c_str()); d += "\\" + (ui_mode ? std::string("menu") : (id.empty() ? std::string("unknown") : id)); plat::mkdir(d.c_str()); g_outdir = d; }
    {
        std::string lp = g_outdir + "\\kyuhachi_log.txt";
        g_log = fopen(lp.c_str(), "w");
        if (g_log && !g_flaglog.empty()) gh_log("%s", g_flaglog.c_str());
    }
    if (g_headless) { _putenv("SDL_AUDIODRIVER=dummy"); }   /* 무음: 헤드리스/시험은 소리 장치를 아예 열지 않는다 */
    SDL_SetHint("SDL_IME_SHOW_UI", "1"); SDL_SetHint("SDL_MOUSE_FOCUS_CLICKTHROUGH", "1"); if (g_log) gh_log("HINT SDL_MOUSE_FOCUS_CLICKTHROUGH=1 (t48c #639 ②)");   /* t48a: Windows IME 조합·후보 창을 시스템이 그리게 한다 */
    if (!g_inject_file.empty()) SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");   /* t48e2: --inject 시험 전용(숨김 창에서도 가상 패드 이벤트 수신). 평소 동작 불변 */
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | (g_headless ? 0 : SDL_INIT_VIDEO)) < 0)
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    g_log_t0 = SDL_GetTicks();
    gh_log("gensei_host " GENSEI_BUILD_ID " start, profile='%s' headless=%d ui=%d stubs=%d autokeys=%d", id.c_str(), (int)g_headless, (int)ui_mode, gensei_stub_n, (int)g_autokeys);
    if (g_batest) { gh_batest(); gh_finish(0); }
    {   /* r011: 오디오 출력 장치 목록·선택 (믹서가 열리기 전에 gensei_audio_dev 를 정한다) */
        int nd = SDL_GetNumAudioDevices(0);
        for (int i = 0; i < nd; i++) { const char * n = SDL_GetAudioDeviceName(i, 0); gh_log("audio device #%d: %s", i, n ? n : "?"); if (g_listaudio) printf("audio device #%d: %s\n", i, n ? n : "?"); }
        if (!g_audiodev.empty()) {
            static std::string chosen;
            for (int i = 0; i < nd; i++) { const char * n = SDL_GetAudioDeviceName(i, 0); if (n && strstr(n, g_audiodev.c_str())) { chosen = n; break; } }
            if (!chosen.empty()) { gensei_audio_dev = chosen.c_str(); gh_log("audio device chosen: %s", chosen.c_str()); }
            else gh_log("audio device '%s' not found -> default", g_audiodev.c_str());
        }
        if (g_listaudio) { fflush(stdout); gh_finish(0); }
    }
    if (!ui_mode && g_uitest_dir.empty() && !(id.empty() && (g_scanonly || !g_addwork_cli.empty())) && !gh_profile(id, g_prof)) { fprintf(stderr, "usage: gensei_host <hukyou|kaitou|kitan|torimono> [--imgdir D] [--out D] [--headless] [--sec N] [--shot a,b]\n"); gh_log("ERROR: unknown profile '%s'", id.c_str()); gh_msgbox(phr_("알 수 없는 작품 이름입니다.\nPC-98_PLAYER.exe 를 직접 열어 작품을 고르세요.")); return 1; }
#if defined(WIN32)
    SetUnhandledExceptionFilter(gh_seh); AddVectoredExceptionHandler(1, gh_veh);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CreateThread(NULL, 0, gh_boxkiller, NULL, 0, NULL);
#endif
    gensei_db_dir = g_exedir;   /* t48e (#651 2): <exe>\res\gamecontrollerdb.txt */
    gensei_input_init();
    gensei_hotplug_hook = gh_hotplug_hook;
    gh_help_init(); if (g_helplog) ov::g_help_logfn = gh_help_logfn; ov::g_helpdev = SDL_NumJoysticks() > 0 ? 0 : 1;   /* t49a (#812): 시작값 = 패드가 연결돼 있으면 패드, 없으면 키보드 */
    gensei_lock = true;
    gh_ui_init_games();
    ui::settings_load(g_ui, g_cfgpath);
    if (g_cli_lang >= 0 && g_cli_lang <= 2) g_ui.lang = g_cli_lang;   /* t52g 시험: --lang N */
    phr_set_lang(g_ui.lang); gh_log("T52G lang=%d (0 ko / 1 en / 2 ja) 문구 %d줄 · 게임 시작 -> [%s]", g_ui.lang, phr_loaded_count, phr_("게임 시작"));
    gensei_western = g_ui.g.pad == 1;   /* t48e: 시작 메뉴용(게임 시작 때 작품별 값으로 다시 정함) */
    if (g_uitest_dir.empty()) {   /* r017: 게임 폴더 · 작품 추가(시험용 CLI) */
        gh_nk_load_once();
        if (!g_gamefolder_cli.empty()) g_ui.gamefolder = aw::acp_to_u8(g_gamefolder_cli);
        if (!g_addwork_cli.empty()) gh_cli_addwork();
        if (!g_ui.gamefolder.empty() && (!g_headless || g_scanonly)) gh_scan_gamefolder(phr_("시작"));
        if (g_scanonly || (!g_addwork_cli.empty() && g_headless && id.empty())) { g_shacache.save(); gh_finish(0); }
    }
    if (g_titletest) {   /* t39 (T11): 작품 제목이 창 제목에서 120바이트를 넘는 수 — 새 방식 vs 옛 방식 */
        int over_new = 0, over_old = 0, nlong = 0;
        for (auto & g : g_ui.games) {
            std::string tt = g.title; size_t old_len = tt.size() + sizeof(KH_WT_SUFFIX) - 1, new_len = old_len;
            { std::string wt = gh_window_title(g, false); if (g.title.size() > KH_WT_TITLEMAX) nlong++; new_len = wt.size(); }
            if (old_len > 120) over_old++; if (new_len > 120) over_new++;
            { std::string wt2 = gh_window_title(g, false); gh_log("TITLE %s old=%d new=%d short=%s wt_bytes=%d wt=[%s]", g.id.c_str(), (int)old_len, (int)new_len, g.short_t.c_str(), (int)wt2.size(), wt2.c_str()); }
        }
        gh_log("TITLETEST games=%d long=%d window_over legacy=%d new=%d", (int)g_ui.games.size(), nlong, over_old, over_new); printf("TITLETEST games=%d long=%d window_over legacy=%d new=%d\n", (int)g_ui.games.size(), nlong, over_old, over_new); gh_finish(0);
    }
    if (!g_logoshot.empty()) { gh_font_init(exedir); static uint32_t lb[640 * 400]; ov::Canvas lc = { lb, 640, 400 }; gh_logo_draw(lc); gh_png(g_logoshot.c_str(), lb, 640, 400); gh_log("LOGOSHOT %s", g_logoshot.c_str()); gh_finish(0); }   /* t52d (#129) */
    if (!g_uitest_dir.empty()) { gh_font_init(exedir); gh_uitest(); gh_finish(0); }
    if (!g_headless) {
        if (g_shaderdir.empty()) g_shaderdir = gh_exists(exedir + "\\res\\shaders") ? exedir + "\\res\\shaders" : exedir + "\\shaders";
        gh_font_init(exedir);
        if (!g_winsize_user) {   /* r017: 저장된 배율/사용자 지정 크기를 복원. 처음이면 4:3 정수배(데스크톱의 90% 안에 들어가는 최대 배) */
            gh_wm_refresh(); SDL_Rect ub; ub.x = ub.y = 0; ub.w = 1920; ub.h = 1080; SDL_GetDisplayUsableBounds(0, &ub);
            if (g_ui.wmult > 0) { int m = g_ui.wmult > g_ui.wmax ? g_ui.wmax : g_ui.wmult; g_winw = 640 * m; g_winh = 480 * m; g_ui.wmult = m; }
            else if (g_ui.winw >= 160 && g_ui.winh >= 120) { g_winw = g_ui.winw > ub.w ? ub.w : g_ui.winw; g_winh = g_ui.winh > ub.h ? ub.h : g_ui.winh; g_ui.wmult = 0; }
            else { SDL_DisplayMode dm; int k = 2; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { k = (int)std::min(dm.w * 0.9 / 640.0, dm.h * 0.9 / 480.0); if (k < 1) k = 1; if (k > 4) k = 4; } if (k > g_ui.wmax) k = g_ui.wmax; g_winw = 640 * k; g_winh = 480 * k; g_ui.wmult = k; }
            if (g_ui.wmult == 0 && g_ui.winw < 160) { g_ui.winw = g_winw; g_ui.winh = g_winh; }
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        Uint32 wf = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (g_windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) | (g_hidewin ? SDL_WINDOW_HIDDEN : 0);
        g_win = SDL_CreateWindow(KH_FULLNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_winw, g_winh, wf);
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
                g_win = SDL_CreateWindow(KH_FULLNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_winw, g_winh, SDL_WINDOW_RESIZABLE | (g_windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) | (g_hidewin ? SDL_WINDOW_HIDDEN : 0));
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                if (g_win) { g_ren = SDL_CreateRenderer(g_win, -1, 0); }
            }
        }
        if (!g_win || (!g_use_gl && !g_ren)) { gh_log("ERROR: no window/renderer: %s", SDL_GetError()); gh_msgbox(phr_("화면(창)을 만들 수 없습니다.\n그래픽 드라이버를 확인해 주세요. (로그: logs 폴더)")); gh_finish(5); }
        if (g_win) { SDL_AddEventWatch(gh_ev_watch, NULL); g_wm_ew = g_winw; g_wm_eh = g_winh; g_wm_expect_until = SDL_GetTicks() + 2000u; gh_wm_refresh(); SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE); gh_titlebar_apply("create", true); }
        gh_log("window=%p gl=%d vsync=%d renderer=%p fullscreen=%d size=%dx%d fit=%d shader=%d curve=%.3f shaderdir=%s", (void *)g_win, (int)g_use_gl, (int)g_vsync, (void *)g_ren, g_windowed ? 0 : 1, g_winw, g_winh, (int)g_fit, (int)g_shader, (double)g_curve, g_shaderdir.c_str());
    }
    if (ui_mode) {
        std::string chosen; if (!gh_ui_main(chosen)) { gh_log("ui: quit"); gh_finish(0); }
        id = chosen;
        if (!gh_profile(id, g_prof)) { gh_log("ERROR: unknown profile '%s'", id.c_str()); gh_msgbox(phr_("알 수 없는 작품입니다.")); gh_finish(1); }
        if (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) gh_rebuild_fds(g_prof, g_ui.games[g_cur_game]);
        { auto in = g_prof.kv.find("install"); auto en = g_prof.kv.find("entry_name"); if (in != g_prof.kv.end() && !in->second.empty() && en != g_prof.kv.end()) { FILE * pf = fopen((exedir + "\\install_pending.tsv").c_str(), "wb"); if (pf) { fprintf(pf, "%s\t%s\n", id.c_str(), en->second.c_str()); fclose(pf); } } }
        if (outdir_auto) {   /* UI 단계 로그는 logs\menu, 게임 로그는 logs\<작품> */
            if (g_log) { fflush(g_log); fclose(g_log); }
            std::string d = exedir + "\\logs\\" + id; plat::mkdir(d.c_str()); g_outdir = d; g_log = fopen((d + "\\kyuhachi_log.txt").c_str(), "w"); g_log_t0 = SDL_GetTicks();
            gh_log("gensei_host " GENSEI_BUILD_ID " game start from UI: '%s' resume_slot=%d noB=%d", id.c_str(), g_resume_slot, (int)g_noB);
        }
    } else {
        for (size_t i = 0; i < g_ui.games.size(); i++) if (g_ui.games[i].id == id) g_cur_game = (int)i;
        if (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) gh_rebuild_fds(g_prof, g_ui.games[g_cur_game]);
        if (!g_headless) { gh_refresh_game_slots(g_cur_game); }
    }
    if (g_uitest_dir.empty() && !gh_any_font_bmp(NULL)) {   /* t52b (#127): 글꼴 BMP 0개 — 메뉴 경로는 시작 전에 막지만 바로 실행(인자)·예외 경로의 마지막 방어. 죽지 말고 알리고 끝낸다 */
        gh_log("ERROR: 글꼴 BMP(anex86 형식 524,350 B) 없음: %s\\res\\fonts -> 작품을 시작하지 않음", exedir.c_str());
        if (!g_headless && !g_hidewin) gh_msgbox(phr_("res/fonts 에 anex86 형식 글꼴 BMP 를 넣어 주세요"));
        gh_finish(1);
    }
    g_pid = id;   /* t51a: 창 안 전환이 프로필을 다시 읽을 때 쓰는 작품 id */
    if (g_uitest_dir.empty()) {
        if (!g_imgdir_given && !g_prof.imgdir.empty()) g_imgdir = g_prof.imgdir;
        { auto mk = g_prof.kv.find("mouse"); g_mouse_game = (mk != g_prof.kv.end() && !mk->second.empty() && mk->second != "none") || !g_amouse.empty() || (g_prof.disk[0] == NULL && g_prof.disk[1] == NULL && g_prof.disk[2] == NULL && g_prof.disk[3] == NULL && !g_prof.cd.empty()); /* t38cd: 내장 DOS(CD 슬롯)는 마우스를 기본 연결 */ gh_log("mouse: 이 작품의 마우스 = %s (호스트 마우스 입력 %s)", mk == g_prof.kv.end() ? "(미지정)" : mk->second.c_str(), g_mouse_game ? "연결" : "연결 안 함"); }
        gh_apply_profile_input(g_prof);
        /* t36cd: 디스크는 없고 CD 슬롯만 있으면 내장 DOS(셸)로 부팅해 D: 에 CD 를 올린다. 디스크도 CD 도 있으면 CD 는 이 시험 빌드에서 무시(게스트 IDE 연결은 범위 밖) */
        g_dosfd = g_prof.kv.count("dosfd") && g_prof.kv["dosfd"] == "1" && (g_prof.disk[0] != NULL || g_prof.disk[1] != NULL) && g_prof.disk[2] == NULL && g_prof.disk[3] == NULL;
        g_dosmode = g_dosfd || (g_prof.disk[0] == NULL && g_prof.disk[1] == NULL && g_prof.disk[2] == NULL && g_prof.disk[3] == NULL && !g_prof.cd.empty());
        if (!g_prof.cd.empty() && !g_dosmode) gh_log("WARN: cd 슬롯과 디스크가 함께 있음 — 이 빌드는 게스트 IDE 연결을 하지 않아 cd 를 무시: %s", g_prof.cd.c_str());
        if (!g_dosmode && !g_prof.disk[0] && !g_prof.disk[1] && !g_prof.disk[2] && !g_prof.disk[3]) { gh_log("ERROR: 부팅 가능한 디스크 없음 (%s)", g_prof.reason.c_str()); gh_msgbox(phr_("이 작품은 바로 부팅할 수 있는 디스크가 없습니다.\n(하드 설치 전용이거나 지원하지 않는 형식입니다)")); gh_finish(1); }
    }
    if (!g_headless) {
        g_ui.cur = g_cur_game;
        gh_apply_settings(g_ui.effective(g_cur_game), false);
        if (g_win) {   /* t39 (T11): 창 제목은 120바이트 이내 — 길면 short= 또는 자동 줄임 */
            SDL_SetWindowTitle(g_win, gh_window_title(g_ui.games[g_cur_game], g_titlelegacy).c_str()); }
        if (!g_noboot && g_ui.effective(g_cur_game).boot == 0) gh_boot_seq(false);   /* 짧은 헤더 + Now Loading (≤0.5초), 건너뛰기 가능 */
    }

    try {
        CommandLine com_line(argc, argv);
        Config myconf(&com_line);
        control = &myconf;

        /* the one section that lives in sdlmain.cpp: only what the subset reads */
        { Section_prop * s = control->AddSection_prop("sdl", &Gh_Null_Init); s->Add_string("output", Property::Changeable::Always, "surface"); }
        DOSBOX_SetupConfigSections();
        {   /* r020 (#248): 기본 한글 패치 글꼴 = res\fonts\kyuhachi_font.bmp (t52g: 옛 이름 pc98player_font.bmp · 옛 위치도 허용) */
            std::string cc = g_conf_common; size_t at = cc.find("@FONT@");
            if (at != std::string::npos) cc.replace(at, 6, gh_default_font_path());
            control->ParseConfigString("gensei:common", cc.c_str());
        }
        {   /* 작품별 cycles / 글꼴 / extra (프로필 > 공통) */
            std::string c;
            if (!g_prof.cycles.empty()) { bool num = true; for (char ch : g_prof.cycles) if (ch < '0' || ch > '9') num = false; c += std::string("[cpu]\ncycles=") + (num ? "fixed " : "") + g_prof.cycles + "\n"; }
            {   /* r020 (#247정정): 글꼴은 이름(kr_shinsun · kr_gulim · kr_dotum · jp_default → res\fonts\<이름>.bmp)으로 지정한다. 옛 파일 경로 지정도 읽는다. 보조 zip 은 더 뒤지지 않는다 */
                auto ex = [](const std::string & p) { return plat::exists(p.c_str()); };
                std::string f0 = g_prof.font; bool soft = false;
                { auto fk = g_prof.kv.find("font_kind");   /* t39 (#290/#291): 항목의 font= > 목록 기본 — 원판(jp)은 jp 글꼴, 한글판은 번들 한글(또는 설정에서 고른 같은 묶음 글꼴) */
                  if (f0.empty() && fk != g_prof.kv.end() && fk->second == "jp") { f0 = g_ui.font_jp.empty() ? std::string("jp_default") : g_ui.font_jp; soft = true; }
                  else if (f0.empty() && fk != g_prof.kv.end() && fk->second == "kr" && !g_ui.font_kr.empty()) { f0 = g_ui.font_kr; soft = true; } }
                if (!f0.empty()) {
                    bool isname = f0.find_first_of("\\/:.") == std::string::npos; std::string fp;
                    if (isname) { std::string p = gh_res_font_path(f0); if (ex(p)) { fp = p; gh_log("글꼴 이름 %s -> %s", f0.c_str(), p.c_str()); } }
                    else {
                        bool abs = f0.size() > 1 && (f0[1] == ':' || f0[0] == '\\');
                        std::vector<std::string> cand; if (abs) cand.push_back(f0); else {
                            cand.push_back(g_prof.imgdir + "\\" + f0); auto sd = g_prof.kv.find("srcdir"); if (sd != g_prof.kv.end() && !sd->second.empty()) cand.push_back(sd->second + "\\" + f0);
                            cand.push_back(g_exedir + "\\games\\" + std::string(g_prof.id ? g_prof.id : "") + "\\" + f0); cand.push_back(g_exedir + "\\" + f0); cand.push_back(f0); }
                        for (auto & x : cand) if (ex(x)) { fp = x; break; }
                    }
                    if (fp.empty()) {
                        if (soft) { gh_log("글꼴 종류=jp: res\\fonts\\jp_default.bmp 없음 -> 기본(한글 패치) 글꼴로 대신 (경고 없음)"); }
                        else {
                        std::string bf = gh_default_font_path();
                        gh_log("WARN: 글꼴 '%s' 을(를) 찾지 못함 -> 번들 글꼴 사용 (%s)  (이름 글꼴은 run\\res\\fonts\\<이름>.bmp)", f0.c_str(), bf.c_str());
                        fp = bf; g_osd = phr_("글꼴 '") + f0 + phr_("' 을(를) 찾지 못해 기본 글꼴로 실행합니다 (res\\fonts)"); g_osd_until = SDL_GetTicks() + 6000;
                        }
                    }
                    if (!fp.empty()) c += "[pc98]\npc-98 anex86 font=" + fp + "\n";
                } else gh_log("글꼴 = 기본 (번들 한글 패치 글꼴 %s)", gh_default_font_path().c_str());
            }
            if (!g_prof.extra.empty()) { std::string e = g_prof.extra; for (auto & ch : e) if (ch == '|') ch = '\n'; c += e + "\n"; }
            if (!c.empty()) { gh_log("profile conf:\n%s", c.c_str()); control->ParseConfigString("gensei:profile", c.c_str()); }
        }
        gh_log("config parsed");
        {   /* r020 (#247정정): 리듬 WAV(2608_*.wav) 는 run\res\rhythm\ 한 곳에서만 읽는다(DOSBox-X 가 현재 폴더 기준으로 읽으므로 그 폴더를 현재 폴더로). sound.rom·bios.rom 은 쓰지 않는다 */
            std::string snd = g_prof.kv.count("sound") ? g_prof.kv["sound"] : std::string();
            if (snd.find("rhythm") != std::string::npos || snd.find("2608") != std::string::npos || snd.find("86") != std::string::npos) {
                std::string rd = g_exedir + "\\res\\rhythm"; int n = 0; static const char * const rn[6] = { "bd", "sd", "top", "hh", "tom", "rim" };
                for (int i = 0; i < 6; i++) if (plat::exists((rd + "\\2608_" + rn[i] + ".wav").c_str())) n++;
                if (n > 0) { SetCurrentDirectoryA(rd.c_str()); gh_log("리듬 WAV %d/6 개: %s (현재 폴더로 지정)%s", n, rd.c_str(), n < 6 ? " — 일부 없음" : ""); }
                else { gh_log("리듬음 파일 없음 — 넣는 곳: %s (sound=%s)", rd.c_str(), snd.c_str()); if (g_osd.empty()) { g_osd = phr_("리듬음 파일 없음 — 넣는 곳: run\\res\\rhythm\\"); g_osd_until = SDL_GetTicks() + 10000; } }
            }
        }

        DOSBOX_InitTickLoop();
        DOSBOX_RealInit();
        gh_log("RealInit done, machine=%d", (int)machine);

#define GH_INIT(f) do { gh_log("init " #f); f; } while (0)
        { /* sdlmain.cpp 가 만들던 메뉴항목(get_item 으로 찾기만 하는 것)을 미리 할당 — 목록은 빌드 때 소스에서 뽑는다 */
            static const char * const pre_names[] = {
#include "gensei_menu_names.inc"
                0 };
            for (int i = 0; pre_names[i]; i++) if (!mainMenu.item_exists(pre_names[i])) mainMenu.alloc_item(DOSBoxMenu::item_type_id, pre_names[i]);
#ifdef GH_DOSKERNEL
            { /* t36cd: DOS_EnableDriveMenu 가 조합해서 찾는 드라이브 메뉴 이름(Drive<X>, drive_<X>_*) 미리 할당 */
                static const char * const dsfx[] = { "_mountauto","_mounthd","_mountcd","_mountfd","_mountfro","_mountarc","_mountimg","_mountimgs","_mountiro","_unmount","_swap","_rescan","_info","_boot","_bootimg","_saveimg", 0 };
                for (char dc = 'A'; dc <= 'Z'; dc++) {
                    std::string nm = std::string("Drive") + dc;
                    if (!mainMenu.item_exists(nm)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, nm);
                    for (int k = 0; dsfx[k]; k++) {
                        nm = std::string("drive_") + dc + dsfx[k];
                        if (!mainMenu.item_exists(nm)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, nm);
                    }
                }
                for (int ic = 1; ic <= 8; ic++) for (int ms = 0; ms < 2; ms++) { /* ide.cpp DOS_EnableDriveIDEMenu: IDEDrive<n><m|s> */
                    char bn[32]; snprintf(bn, sizeof bn, "IDEDrive%d%c", ic, ms ? 's' : 'm');
                    std::string nm = bn;
                    if (!mainMenu.item_exists(nm)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, nm);
                    for (int k = 0; dsfx[k]; k++) {
                        std::string n2 = nm + dsfx[k];
                        if (!mainMenu.item_exists(n2)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, n2);
                    }
                }
            }
#endif
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
        if (g_dosmode) {   /* t36cd: 내장 DOS — sdlmain 의 초기화 목록 중 DOS·CD 몫 (마우스는 호스트 몫이라 제외) */
            void DOS_Init(); void XMS_Init(); void EMS_Init(); void DOS_KeyboardLayout_Init(); void CDROM_Image_Init(); void MSCDEX_Init(); void DRIVES_Init(); void AUTOEXEC_Init();
            control->ParseConfigString("gensei:dos0", g_ems_req ? "[dos]\nems=true\n" : "[dos]\nems=false\n");   /* PC-98 에서는 EMS 불필요 — bad_alloc 회피 */
            GH_INIT(DOS_Init()); GH_INIT(XMS_Init()); GH_INIT(EMS_Init()); GH_INIT(DOS_KeyboardLayout_Init()); GH_INIT(CDROM_Image_Init()); GH_INIT(MSCDEX_Init()); GH_INIT(DRIVES_Init()); GH_INIT(AUTOEXEC_Init());
            std::string ae = "[autoexec]\n";
            if (!g_prof.cd.empty()) ae += "imgmount d \"" + g_prof.cd + "\" -t iso\n";
            if (g_dosfd) {   /* t43: A: = 첫 디스크(+교체 목록), B: = 둘째 디스크. 실행 파일이 하나면 자동 실행, 여럿/없음이면 목록만 */
                auto qp = [&](const std::string & n) { return std::string("\"") + gh_workpath(n) + "\""; };   /* t43h: 내장 DOS 의 FD 장착도 작업 사본 */
                std::string d0 = g_prof.disk[0] ? g_prof.disk[0] : g_prof.disk[1], d1 = (g_prof.disk[0] && g_prof.disk[1]) ? g_prof.disk[1] : std::string();
                bool hddf = g_prof.kv.count("doshdd") && g_prof.kv["doshdd"] == "1"; const char L1 = hddf ? 'b' : 'a', L2 = hddf ? 'c' : 'b';   /* t43d: 설치 흐름 = HDD 가 A:, FD 는 B:·C: */
                std::string a = std::string("imgmount ") + L1 + " " + qp(d0); for (auto & sw : g_prof.swap) if (sw != d0 && sw != d1) a += " " + qp(sw); ae += a + " -t floppy\n";
                if (!d1.empty()) ae += std::string("imgmount ") + L2 + " " + qp(d1) + " -t floppy\n";
                std::string dv = g_prof.kv.count("dosdrv") ? g_prof.kv["dosdrv"] : std::string("a"); if (dv != "a" && dv != "b") dv = "a";
                if (hddf) {   /* t43c: C: = 작품별 호스트 폴더 (savedir\<id>\hdd). 비어 있으면 설치기 실행, 채워졌으면 실행 파일 탐지 */
                    std::string sd1 = g_savedir, sd2 = g_savedir + "\\" + std::string(g_prof.id ? g_prof.id : "x"), hd = sd2 + "\\hdd";
                    plat::mkdir(sd1.c_str()); plat::mkdir(sd2.c_str()); plat::mkdir(hd.c_str());
                    ae += "mount a \"" + hd + "\"\n";
                    std::string run, dirp; int nrun = gh_scan_hdd(hd, run, dirp);
                    std::string idv = g_prof.kv.count("dosinstdrv") ? g_prof.kv["dosinstdrv"] : dv, inst = g_prof.kv.count("dosinst") ? g_prof.kv["dosinst"] : std::string(); if (!inst.empty() && g_prof.kv.count("dosinstargs") && !g_prof.kv["dosinstargs"].empty()) inst += " " + g_prof.kv["dosinstargs"];
                    if (nrun == 0 && !inst.empty()) { std::string fl = idv == "b" ? "c" : "b"; size_t pp; while ((pp = inst.find("@F")) != std::string::npos) inst.replace(pp, 2, fl); while ((pp = inst.find("@H")) != std::string::npos) inst.replace(pp, 2, "a"); ae += fl + ":\n" + inst + "\na:\ndir /w\n"; gh_log("HDD 설치 흐름: A: = %s (비어 있음, HDD) → %s: %s 실행", hd.c_str(), fl.c_str(), inst.c_str()); }
                    else if (nrun == 0 && g_prof.kv.count("dossfx") && !g_prof.kv["dossfx"].empty()) {   /* t43p (#344-3): LHA 자기풀림 — A: = 빈 호스트 폴더에서 FD 의 SFX 를 실행해 푼다. 다음 실행부터는 푼 폴더에서 실행 파일 탐지 */
                        ae += "a:\n"; { bool ub_done = false; std::string cur, rs = g_prof.kv["dossfx"]; for (char ch : rs + ",") { if (ch == ',') { if (!cur.empty()) { if (cur[0] == '#') { size_t bar = cur.find('|'); int ix = atoi(cur.c_str() + 1); std::string nm = bar == std::string::npos ? std::string() : cur.substr(bar + 1); if (!nm.empty() && ix >= 2 && (size_t)(ix - 2) < g_prof.swap.size()) { if (!ub_done) { ae += "imgmount -u b\n"; ub_done = true; } ae += "imgmount -u c\n"; ae += std::string("imgmount c ") + qp(g_prof.swap[ix - 2]) + " -t floppy\n"; ae += "c:\\" + nm + "\n"; ae += "a:\n"; } } else ae += cur + "\n"; } cur.clear(); } else cur += ch; } } ae += "dir /w\n";
                        gh_log("HDD 풀기 흐름(LHA 자기풀림): A: = %s (비어 있음, HDD) → %s", hd.c_str(), g_prof.kv["dossfx"].c_str()); }
                    else { g_prof.bootkeys.clear(); ae += "a:\n"; if (!dirp.empty()) ae += "cd \\" + dirp + "\n"; ae += (nrun == 1 ? run : std::string("dir /w")) + "\n"; gh_log("HDD 설치 끝난 폴더: 실행 파일 %d개 → %s", nrun, nrun == 1 ? run.c_str() : "목록"); }
                } else {
                ae += dv + ":\n"; if (g_prof.kv.count("dossub") && !g_prof.kv["dossub"].empty()) ae += "cd \\" + g_prof.kv["dossub"] + "\n";   /* t43s R2 */
                std::vector<std::string> rl; { std::string cur, rs = g_prof.kv.count("dosrun") ? g_prof.kv["dosrun"] : std::string(); for (char ch : rs + ",") { if (ch == ',') { if (!cur.empty()) rl.push_back(cur); cur.clear(); } else cur += ch; } }
                { std::string pk = g_prof.kv.count("dospick") ? g_prof.kv["dospick"] : std::string(); gh_log("DOS 루트 실행 후보(%d): %s | 선택: %s", (int)rl.size(), g_prof.kv.count("dosrunsz") ? g_prof.kv["dosrunsz"].c_str() : "-", pk.empty() ? (rl.size() == 1 ? rl[0].c_str() : "(목록)") : pk.c_str());   /* t43n (#343) */
                  if (rl.size() == 1) ae += rl[0] + "\n"; else if (!pk.empty()) ae += pk + "\n"; else { ae += "dir /w\n"; if (g_prof.kv.count("dosdata")) ae += "echo No executable on this disk (data disk? the main disk is needed)\n"; } }   /* t43s R5 */
                }
            }
            { std::string ln; for (char ch : g_prof.autoexec + "|") { if (ch == '|') { if (!ln.empty()) ae += ln + "\n"; ln.clear(); } else ln += ch; } }
            gh_log("autoexec(내장 DOS):\n%s", ae.c_str());
            control->ParseConfigString("gensei:dos", ae.c_str());
        }
        gh_log("devices initialised");

        gh_stage_init();   /* t49a (#845·#847) */
        if (g_demo) { std::string dn = g_prof.kv.count("demo") ? g_prof.kv["demo"] : std::string(); if (dn.empty() || !gh_attach(0, dn.c_str())) { gh_finish(2); } g_prof.bootletter = 'A'; g_autokeys = false; gh_log("DEMO boot: %s", dn.c_str()); }
        else if (!g_dosfd) for (int i = 0; i < 4; i++) if (g_prof.disk[i] && !(g_noB && i == 1) && !gh_attach(i, g_prof.disk[i])) { gh_finish(2); }

        if (!VM_PowerOn_gensei()) { gh_log("ERROR: power on failed"); gh_finish(3); }
        gh_log("powered on, running machine");
        g_pon_tick = SDL_GetTicks() | 1u; g_pon_pic = PIC_Ticks; g_loading = true;
        if (!g_ft_dir.empty() && !ov::g_font_ok) ov::font_load(g_exedir);
        for (;;) {   /* t51a: 창 안 전환이 요청되면 정리 · 디스크 교체 · 리셋 뒤 같은 창에서 RunMachine 재진입. 요청이 없으면 예전과 같이 한 번 돌고 끝 */
            try {
                DOSBOX_RunMachine();
            } catch (int x) {
                gh_log("RunMachine exit signal %d", x);
            }
            if (!g_inwin_req && !g_inwin_timeout) break;
            if (!gh_inwin_cycle()) break;
        }
        gh_finish(0);
    } catch (const std::exception &ex) {
        gh_log("EXCEPTION escaped main: std::exception %s (%s)", ex.what(), typeid(ex).name());
        gh_finish(4);
    } catch (const char *s) {
        gh_log("EXCEPTION escaped main: const char* %s", s ? s : "(null)");
        gh_finish(4);
    } catch (const std::string &s) {
        gh_log("EXCEPTION escaped main: std::string %s", s.c_str());
        gh_finish(4);
    } catch (int x) {
        gh_log("EXCEPTION escaped main: int %d", x);
        gh_finish(4);
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
