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
#include "input/kh_input12.h"
#include "input/kh_padnames_t48e.h"   /* t48e (#646): 키 이름 표 확장 */
#include "input/kh_inputtypes.h"   /* S6 */
#include "ui/kh_ui.h"
#include "settings/kh_prof.h"
#include "pc98_savestate.h"
#include "pc98_gdc.h"
#include "imagedisk_d88.h"
#include "imagedisk_nfd.h"
#include "gensei_boostaud.h"
#include "settings/kh_nkdb.h"
#include "settings/kh_addwork.h"
#include "platform/plat.h" /* S3: platform layer */
#include "ui/kh_uilist.h"
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

#include "app/opts_log.h"   /* S9b */
#include "app/res_prof.h"   /* S9b */
#include "app/video_sink.h"   /* S9a */
#include "app/frame.h"   /* S9a */
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

#include "app/boot.h"   /* S9a */
#include "app/state_io.h"   /* S9a */
#include "app/kbd_event.h"   /* S9b */
#include "app/ui_env.h"   /* S9b */
#include "app/menu_boot.h"   /* S9b */
#include "app/uitest.h"   /* S9b */
#include "app/gfx_events.h"   /* S9b */
#include "app/crash_inwin.h"   /* S9b */

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
    kh_db_dir = g_exedir;   /* t48e (#651 2): <exe>\res\gamecontrollerdb.txt */
    kh_input_init();
    kh_hotplug_hook = gh_hotplug_hook;
    gh_help_init(); if (g_helplog) ov::g_help_logfn = gh_help_logfn; ov::g_helpdev = SDL_NumJoysticks() > 0 ? 0 : 1;   /* t49a (#812): 시작값 = 패드가 연결돼 있으면 패드, 없으면 키보드 */
    gensei_lock = true;
    gh_ui_init_games();
    ui::settings_load(g_ui, g_cfgpath);
    if (g_cli_lang >= 0 && g_cli_lang <= 2) g_ui.lang = g_cli_lang;   /* t52g 시험: --lang N */
    phr_set_lang(g_ui.lang); gh_log("T52G lang=%d (0 ko / 1 en / 2 ja) 문구 %d줄 · 게임 시작 -> [%s]", g_ui.lang, phr_loaded_count, phr_("게임 시작"));
    kh_western = g_ui.g.pad == 1;   /* t48e: 시작 메뉴용(게임 시작 때 작품별 값으로 다시 정함) */
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
