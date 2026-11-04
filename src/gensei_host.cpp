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
#include "platform/plat_os.h"   /* S9e-ㄴ: windows.h 는 platform 층에서 포함 */
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

#include "app/state.h"   /* S9d */
#include "app/opts_log.h"   /* S9b */
#include "app/res_prof.h"   /* S9b */
#include "app/video_sink.h"   /* S9a */
#include "app/frame.h"   /* S9a */
void RENDER_SetPal(uint8_t, uint8_t, uint8_t, uint8_t) {}
bool RENDER_IsScalerCompatibleWithDoublescan(void) { return false; }
SDL_Window * GFX_GetSDLWindow(void) { return gApp.win; }

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
#include "app/dosmount.h"   /* #451 step 2 */
#include "app/state_io.h"   /* S9a */
#include "app/kbd_event.h"   /* S9b */
#include "app/ui_env.h"   /* S9b */
#include "app/menu_boot.h"   /* S9b */
#include "app/uitest.h"   /* S9b */
#include "app/gfx_events.h"   /* S9b */
#if defined(__ANDROID__)
#include "app/folder_pick.h"   /* t55b */
#endif
#include "app/crash_inwin.h"   /* S9b */
#include "app/main_front.h"   /* S9c */
#include "app/main_setup.h"   /* S9c */
#include "app/main_core.h"   /* S9c */

int main(int argc, char * argv[]) {
    SDL_SetMainReady();
    for (int i = 0; i < argc; i++) gApp.argv_saved.push_back(argv[i]);   /* t49a (#845): 단계 전환 때 같은 인자로 다시 띄운다 */
    std::string id;
    { int rc = gh_main_args(argc, argv, id); if (rc >= 0) return rc; }   /* S9c */
    /* r011b/r012: exe 위치 기준 기본값(인자 없이 더블클릭해도 돌도록) */
    { std::string s = plat::exe_path(); size_t sl = s.find_last_of("\\/"); gApp.exedir = (sl == std::string::npos) ? std::string(".") : s.substr(0, sl); }
    /* t52g (#133): 화면 문구 파일은 설정(언어)을 읽은 뒤 phr_set_lang() 이 읽는다 (strings_ko/en/ja.txt · 한국어는 strings.txt 가 덮어씀) */
    const std::string & exedir = gApp.exedir;
    if (!plat::exists("res\\fonts\\kyuhachi_font.bmp") && !plat::exists("kyuhachi_font.bmp") && !plat::exists("res\\fonts\\pc98player_font.bmp") && !plat::exists("pc98player_font.bmp")) plat::set_cwd(exedir);   /* t52g: 새·옛 이름 모두 */
    const bool ui_mode = id.empty() && !gApp.headless;   /* r012: 인자 없음 = 이 실행기의 UI 로 시작 */
    gh_main_env(exedir);   /* S9c */
    const bool outdir_auto = (gApp.outdir == ".");
    gh_main_logsdl(exedir, ui_mode, id, outdir_auto);   /* S9c */
    { int rc = gh_main_audio_prof(id, ui_mode); if (rc >= 0) return rc; }   /* S9c */
    gh_main_input_init(id);   /* S9c */
    gh_main_tests(exedir);   /* S9c */
    gh_main_window(exedir);   /* S9c */
    gh_main_select(id, ui_mode, outdir_auto, exedir);   /* S9c */
    gh_main_prep(id, exedir);   /* S9c */

    try {
        CommandLine com_line(argc, argv);
        Config myconf(&com_line);
        control = &myconf;

        gh_main_conf();   /* S9c */
        gh_main_devinit();   /* S9c */
        gh_main_run();   /* S9c */
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
