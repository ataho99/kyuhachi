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

bool TempLineAlloc(unsigned int w); void TempLineFree(void);  /* vga_draw.cpp: 진짜 RENDER_SetSize(render.cpp:1055)가 하던 몫 */
void RENDER_SetSize(Bitu width, Bitu height, Bitu bpp, float fps, double scrn_ratio) {
    TempLineFree(); TempLineAlloc((unsigned)width);
    gh_log("RENDER_SetSize %ux%u bpp=%u fps=%.3f ratio=%.4f", (unsigned)width, (unsigned)height, (unsigned)bpp, fps, scrn_ratio);
    g_fps = fps;
    if (bpp != 32) gh_log("WARN: bpp %u not supported by host sink", (unsigned)bpp);
    g_fbw = (int)width; g_fbh = (int)height; g_fb.assign((size_t)g_fbw * g_fbh, 0);
    RENDER_DrawLine = gh_drawline;
    if (!g_headless && g_ren) {
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
    if (!g_headless && g_ren && g_tex && !g_fb.empty()) {
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
        else if (a[0] != '-') id = a;
    }
    {
        std::string lp = g_outdir + "\\gensei_host_log.txt";
        g_log = fopen(lp.c_str(), "w");
    }
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | (g_headless ? 0 : SDL_INIT_VIDEO)) < 0)
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    g_log_t0 = SDL_GetTicks();
    gh_log("gensei_host r009 start, profile='%s' headless=%d stubs=%d", id.c_str(), (int)g_headless, gensei_stub_n);
    if (!gh_profile(id, g_prof)) { fprintf(stderr, "usage: gensei_host <hukyou|kaitou|kitan|torimono> [--imgdir D] [--out D] [--headless] [--sec N] [--shot a,b]\n"); return 1; }
#if defined(WIN32)
    SetUnhandledExceptionFilter(gh_seh);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CreateThread(NULL, 0, gh_boxkiller, NULL, 0, NULL);
#endif
    gensei_input_init();
    gensei_lock = true;
    if (!g_headless) {
        g_win = SDL_CreateWindow("GENSEI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800, SDL_WINDOW_RESIZABLE);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        if (g_win) { g_ren = SDL_CreateRenderer(g_win, -1, 0); }
        gh_log("window=%p renderer=%p", (void *)g_win, (void *)g_ren);
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
