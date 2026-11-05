/* GENSEI r004: SDL2 GameController input layer + virtual mouse layer (diagnostic build).
 * Device -> (this layer) -> PC-98 keyboard / PC-98 mouse. Included only by sdlmain.cpp.
 * t48e (#646/#651/#652): 패드 버튼 배정표(○ × □ START L1 R1 SELECT) · 서양식(남쪽=결정) · gamecontrollerdb · 표에 없는 패드 원시 번호 폴백 */
#pragma once
#include <SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <time.h>
#include "keyboard.h"
#include "mouse.h"
#include "input/kh_kbtable.h"

bool gensei_lock = false;              /* true when -gensei: mapper hot keys (edit/config/etc.) blocked */
static bool kh_padmouse = false;
/* t48e: 버튼 배정표. 위치 기준(동=○ 남=× 서=□). 기본은 옛 t34 와 같다: 동 Space · 남 Esc · 서 CapsLock · START Enter · L1 R1 SELECT 없음 */
enum { PB_EAST = 0, PB_SOUTH, PB_WEST, PB_START, PB_L1, PB_R1, PB_BACK, PB_NORTH, PB_DUP, PB_DDOWN, PB_DLEFT, PB_DRIGHT, PB_L2, PB_R2, PB_L3, PB_R3, PB_N };   /* 빌드 5a: 7 → 16 (옛 번호 0~6 그대로) */
enum { PBT_KEY = 0, PBT_NONE, PBT_ML, PBT_MR, PBT_FN };   /* 빌드 5a: PBT_FN = 큐하치 기능(fn) 배정 */
enum { FN_MENU = 1, FN_BOOST_HOLD, FN_BOOST_TOG, FN_QSAVE, FN_QLOAD, FN_OSK, FN_SHOT, FN_NEXTDISK, FN_FPS, FN_N };   /* ui_env.h GH_PADFN · kh_ui.h pad_fn_name 과 같은 순서 */
struct GPadBind { int type; KBD_KEYS key; int fn; };
static GPadBind kh_pb[PB_N];
static bool kh_western = false;    /* t48e (#651 4): 서양식 = 남쪽 버튼이 결정. settings.ini pad=1 */
static void kh_pb_defaults(void) {
    for (int i = 0; i < PB_N; i++) { kh_pb[i].type = PBT_NONE; kh_pb[i].key = KBD_NONE; kh_pb[i].fn = 0; }
    kh_pb[PB_EAST].type = PBT_KEY;  kh_pb[PB_EAST].key = kh_western ? KBD_esc : KBD_space;
    kh_pb[PB_SOUTH].type = PBT_KEY; kh_pb[PB_SOUTH].key = kh_western ? KBD_space : KBD_esc;
    kh_pb[PB_WEST].type = PBT_KEY;  kh_pb[PB_WEST].key = KBD_capslock;
    kh_pb[PB_START].type = PBT_KEY; kh_pb[PB_START].key = KBD_enter;
    kh_pb[PB_L1].type = PBT_NONE;   kh_pb[PB_L1].key = KBD_NONE;
    kh_pb[PB_R1].type = PBT_NONE;   kh_pb[PB_R1].key = KBD_NONE;
    kh_pb[PB_BACK].type = PBT_NONE; kh_pb[PB_BACK].key = KBD_NONE;
    /* 빌드 5a: 기본값 = 옛 동작 그대로 (Y=메뉴 · 십자=화살표 · R2=배속 누름 · L3=배속 켜고 끄기) */
    kh_pb[PB_NORTH].type = PBT_FN;  kh_pb[PB_NORTH].fn = FN_MENU;
    kh_pb[PB_DUP].type = PBT_KEY;   kh_pb[PB_DUP].key = KBD_up;
    kh_pb[PB_DDOWN].type = PBT_KEY; kh_pb[PB_DDOWN].key = KBD_down;
    kh_pb[PB_DLEFT].type = PBT_KEY; kh_pb[PB_DLEFT].key = KBD_left;
    kh_pb[PB_DRIGHT].type = PBT_KEY; kh_pb[PB_DRIGHT].key = KBD_right;
    kh_pb[PB_R2].type = PBT_FN;     kh_pb[PB_R2].fn = FN_BOOST_HOLD;
    kh_pb[PB_L3].type = PBT_FN;     kh_pb[PB_L3].fn = FN_BOOST_TOG;
}
static bool kh_pb_init_done = (kh_pb_defaults(), true);
static void (*kh_fn_hook)(int fn, bool down) = NULL;   /* 빌드 5a: 호스트(kbd_event.h gh_pad_fn)가 채운다 */
static bool kh_trig_on[2];                              /* L2·R2 눌림(히스테리시스 켜짐 > 16000 · 꺼짐 < 8000) */
static bool kh_sel_dn, kh_st_dn, kh_sw_sel, kh_sw_st;
struct GHot { int btn; bool sel; };   /* hotkey = button (+ SELECT held). btn -1 = none. Menu (SELECT+START) is fixed and not in this table */
static GHot kh_hot[FN_N]; static bool kh_hot_sw[PB_N];
static void kh_hot_defaults(void) {
    for (int q = 0; q < FN_N; q++) { kh_hot[q].btn = -1; kh_hot[q].sel = false; }
    kh_hot[FN_QSAVE].btn = PB_R1;  kh_hot[FN_QSAVE].sel = true;
    kh_hot[FN_QLOAD].btn = PB_L1;  kh_hot[FN_QLOAD].sel = true;
    kh_hot[FN_OSK].btn = PB_NORTH; kh_hot[FN_OSK].sel = true;
    kh_hot[FN_SHOT].btn = PB_R3;   kh_hot[FN_SHOT].sel = true;
    kh_hot[FN_NEXTDISK].btn = PB_R2; kh_hot[FN_NEXTDISK].sel = true;
}
static bool kh_hot_init_done = (kh_hot_defaults(), true);
static bool kh_cap_on = false, kh_cap_sel = false, kh_cap_selheld = false; static int kh_cap_pb = -1; static Uint32 kh_cap_t0 = 0;   /* combo capture (menu open) */   /* SELECT+START 메뉴 조합(고정) */
static bool kh_never[256];   /* #237 never_map: 패드·단축키에 절대 보내지 않는 키(KBD_KEYS 값) */   /* env GENSEI_PADMOUSE=1 : D-pad moves the cursor, A/B = L/R click */
static std::map<SDL_JoystickID, SDL_GameController*> kh_pads;
static std::map<SDL_JoystickID, SDL_Joystick*> kh_rawpads;   /* t48e: 표(매핑)에 없는 패드 — 원시 번호 폴백 */
static std::map<SDL_JoystickID, int> kh_rawhat;               /* 폴백 패드의 십자 상태(SDL_HAT_* 비트) */
static int gensei_padkind_v = 0;
static bool gensei_pad_retro = false;   /* Retroid / Nintendo layout: east=A south=B west=Y north=X */       /* 0 그 밖 1 Xbox 2 PlayStation (첫 번째로 열린 패드의 이름으로 판정) */
static FILE* kh_plog = NULL;
static bool kh_axis_dir[2][4];     /* [pad-slot][L,R,U,D] stick->arrow state (pad 0 only used) */
static int  kh_dpad_mouse[4];      /* L,R,U,D held (padmouse mode) */

static void kh_plogf(const char* fmt, ...) {
    if (!kh_plog) return;
    va_list va; va_start(va, fmt); vfprintf(kh_plog, fmt, va); va_end(va);
    fputc('\n', kh_plog); fflush(kh_plog);
}
#define GPLOG(...) kh_plogf(__VA_ARGS__)

/* ---- virtual mouse layer (device -> vmouse -> PC-98 mouse). Nova's own method plugs in here later. ---- */
/* t34 (#268): 코어 버스마우스 이동량 레지스터는 int8 — 한 틱에 127 넘게 몰아 주면 부호가 뒤집힌다(실측: y -150 한 번에 → 이동 안 됨). 대기열에 모아 20ms 마다 ±100 이하로 나눠 넣는다. */
static float kh_vm_px = 0, kh_vm_py = 0; static Uint32 kh_vm_last = 0;
static void kh_vmouse_move_rel(float dx, float dy) { kh_vm_px += dx; kh_vm_py += dy; }
static void kh_vmouse_pump() {
    extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y;
    int ix = (int)kh_vm_px, iy = (int)kh_vm_py; if (!ix && !iy) return;
    Uint32 now = SDL_GetTicks(); if (now - kh_vm_last < 20) return; kh_vm_last = now;
    int lox = -100, hix = 100, loy = -100, hiy = 100;
    { int a = -128 - (int)p7fd9_8255_mouse_x, b = 127 - (int)p7fd9_8255_mouse_x; if (a > lox) lox = a; if (b < hix) hix = b; }
    { int a = -128 - (int)p7fd9_8255_mouse_y, b = 127 - (int)p7fd9_8255_mouse_y; if (a > loy) loy = a; if (b < hiy) hiy = b; }
    int ax = ix < lox ? lox : ix > hix ? hix : ix, ay = iy < loy ? loy : iy > hiy ? hiy : iy;
    if (ax || ay) { Mouse_CursorMoved((float)ax, (float)ay, 0, 0, false); kh_vm_px -= (float)ax; kh_vm_py -= (float)ay; }
}
static void kh_vmouse_button(int b /*0 L,1 R,2 M*/, bool down) {
    if (down) Mouse_ButtonPressed((uint8_t)b); else Mouse_ButtonReleased((uint8_t)b);
}

/* t48e: 패드 종류 (표시 이름용). 이름으로만 판정한다 — SDL 버전과 무관 */
static int gensei_padkind_of(const char* nm) {
    if (!nm) return 0; std::string s = nm; for (auto & c : s) c = (char)tolower((unsigned char)c);
    if (s.find("xbox") != std::string::npos || s.find("xinput") != std::string::npos) return 1;
    if (s.find("playstation") != std::string::npos || s.find("ps3") != std::string::npos || s.find("ps4") != std::string::npos || s.find("ps5") != std::string::npos || s.find("dualshock") != std::string::npos || s.find("dualsense") != std::string::npos) return 2;
    return 0;
}
static int gensei_padkind(void) { return gensei_padkind_v; }
static bool gensei_is_retro(const char* nm) {
    if (!nm) return false; std::string s = nm; for (auto & c : s) c = (char)tolower((unsigned char)c);
    return s.find("retroid") != std::string::npos || s.find("nintendo") != std::string::npos;
}

static int kh_pad_opens = 0, kh_pad_closes = 0;   /* t49a (#823): 같은 장치 재연결 시 열림/닫힘 횟수 로그 */
static void (*kh_hotplug_hook)(bool added, const char * name) = NULL;   /* 호스트가 채움: 연결/해제 알림·입력 장치 표기 */
static void gensei_open_pad(int idx) {
    if (!SDL_IsGameController(idx)) {
        /* t48e (#651 2): 표에 없는 패드 — GUID 한 줄을 남기고 원시 번호로 폴백 */
        char gs[64] = "?"; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), gs, sizeof gs);
        const char* jn = SDL_JoystickNameForIndex(idx);
        SDL_Joystick* js = SDL_JoystickOpen(idx);
        if (!js) { GPLOG("PADMAP unknown joystick %d guid=%s name='%s' — open fail: %s", idx, gs, jn ? jn : "?", SDL_GetError()); return; }
        SDL_JoystickID id = SDL_JoystickInstanceID(js);
        kh_rawpads[id] = js; kh_rawhat[id] = 0;
        GPLOG("PADMAP unknown joystick idx=%d id=%d guid=%s name='%s' buttons=%d hats=%d axes=%d -> RAW fallback (0=남 1=동 2=서 3=북 4=L1 5=R1 6=SELECT 7=START, 십자=hat0, 왼스틱=축0/1)", idx, (int)id, gs, jn ? jn : "?", SDL_JoystickNumButtons(js), SDL_JoystickNumHats(js), SDL_JoystickNumAxes(js));
        if (!gensei_padkind_v) gensei_padkind_v = gensei_padkind_of(jn); if (kh_hotplug_hook) kh_hotplug_hook(true, jn ? jn : "?");
        return;
    }
    { SDL_JoystickID id0 = SDL_JoystickGetDeviceInstanceID(idx); if (id0 >= 0 && kh_pads.count(id0)) { GPLOG("PAD id=%d 이미 열려 있음 — 열기 호출 없이 무시 (중복 이벤트)", (int)id0); return; } }   /* t49a (#823): 중복 확인을 열기 전에 (열기 호출이 참조 수만 올리던 문제) */
    SDL_GameController* gc = SDL_GameControllerOpen(idx);
    if (!gc) { GPLOG("open fail %d: %s", idx, SDL_GetError()); return; }
    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    if (kh_pads.count(id)) { GPLOG("PAD id=%d 이미 열려 있음 (중복 이벤트 무시)", (int)id); SDL_GameControllerClose(gc); return; }
    kh_pads[id] = gc; kh_pad_opens++;
    const char* nm = SDL_GameControllerName(gc);
    char gs[64] = "?"; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), gs, sizeof gs);
    char* mp = SDL_GameControllerMapping(gc);
    GPLOG("PAD opened idx=%d id=%d name='%s' guid=%s -> SDL GameController 표준 배치 (매핑 %.70s%s)", idx, (int)id, nm ? nm : "?", gs, mp ? mp : "?", (mp && strlen(mp) > 70) ? "..." : "");
    if (mp) SDL_free(mp);
    if (kh_pads.size() == 1 || !gensei_padkind_v) { gensei_padkind_v = gensei_padkind_of(nm); gensei_pad_retro = gensei_is_retro(nm); }
    GPLOG("PADKIND %d (0 그 밖 1 Xbox 2 PlayStation)", gensei_padkind_v);
    GPLOG("PADCOUNT open=%d close=%d live=%d", kh_pad_opens, kh_pad_closes, (int)kh_pads.size());
    if (kh_hotplug_hook) kh_hotplug_hook(true, nm ? nm : "?");
}

/* t48e (#651 2): SDL_GameControllerDB(zlib) 를 기동 때 읽는다. 없으면 로그만 남기고 계속 */
static std::string kh_db_dir;   /* 호스트가 exe 폴더를 넣는다 (기동 때 <exe>\res\gamecontrollerdb.txt) */
static void kh_load_db(void) {
    std::vector<std::string> P;
    if (!kh_db_dir.empty()) { P.push_back(kh_db_dir + "\\res\\gamecontrollerdb.txt"); P.push_back(kh_db_dir + "/res/gamecontrollerdb.txt"); }
    P.push_back("res/gamecontrollerdb.txt"); P.push_back("gamecontrollerdb.txt");
    for (size_t i = 0; i < P.size(); i++) {
        FILE* t = fopen(P[i].c_str(), "rb"); if (!t) continue;
        std::string buf; char tmp[65536]; size_t nr; while ((nr = fread(tmp, 1, sizeof tmp, t)) > 0) buf.append(tmp, nr); fclose(t);
        SDL_RWops* rw = SDL_RWFromConstMem(buf.data(), (int)buf.size());
        int n = rw ? SDL_GameControllerAddMappingsFromRW(rw, 1) : -1;   /* SDL_GameControllerAddMappingsFromFile 와 같다 (파일 경로의 한글/ACP 문제를 피해 fopen 으로 읽음) */
        GPLOG("gamecontrollerdb: %s (%u bytes) -> %d 매핑 추가", P[i].c_str(), (unsigned)buf.size(), n);
        return;
    }
    GPLOG("gamecontrollerdb: 파일 없음 (res/gamecontrollerdb.txt) — SDL 내장 표만 씀");
}

static void kh_input_init(void) {
    gensei_lock = true;
    const char* pm = getenv("GENSEI_PADMOUSE");
    kh_padmouse = pm && pm[0] == '1';
    kh_plog = fopen("kyuhachi_pad_log.txt", "a");
    time_t t = time(NULL);
    GPLOG("== GENSEI r004 input layer start %s padmouse=%d", ctime(&t), (int)kh_padmouse);
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) { GPLOG("GAMECONTROLLER init fail: %s", SDL_GetError()); return; }
    { SDL_version cv; SDL_VERSION(&cv); SDL_version lv; SDL_GetVersion(&lv); GPLOG("SDL compiled %d.%d.%d linked %d.%d.%d", (int)cv.major, (int)cv.minor, (int)cv.patch, (int)lv.major, (int)lv.minor, (int)lv.patch); }
    kh_load_db();
    SDL_GameControllerEventState(SDL_ENABLE);
    int n = SDL_NumJoysticks();
    GPLOG("SDL joysticks=%d", n);
    for (int i = 0; i < n; i++) gensei_open_pad(i);
}

/* t48e: 코어로 보내는 PC-98 키의 스캔코드 (kbtable = DOSBox-X keyboard.cpp KEYBOARD_PC98_AddKey 의 switch 값) — 진단 로그용 */
static int kh_scan_of(KBD_KEYS k) {
    for (int i = 0; i < kbt::g_nkeys; i++) if ((int)k == kbt::g_keys[i].kbd || (kbt::g_keys[i].kbd_ibm >= 0 && (int)k == kbt::g_keys[i].kbd_ibm)) return kbt::g_keys[i].scan;
    return -1;
}

static void kh_key(KBD_KEYS k, bool down, const char* why) {
    if ((int)k >= 0 && (int)k < 256 && kh_never[(int)k]) { GPLOG("  %s -> key %d BLOCKED (never_map)", why, (int)k); return; }
    KEYBOARD_AddKey(k, down);
    int sc = kh_scan_of(k);
    if (sc >= 0) GPLOG("  %s -> key %d scan=0x%02X %s", why, (int)k, sc, down ? "down" : "up");
    else GPLOG("  %s -> key %d %s", why, (int)k, down ? "down" : "up");
}

/* t48e: 배정표대로 한 버튼을 실행 */
static int kh_pb_of_sdl(Uint8 b) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_B: return PB_EAST;  case SDL_CONTROLLER_BUTTON_A: return PB_SOUTH;
    case SDL_CONTROLLER_BUTTON_X: return PB_WEST;  case SDL_CONTROLLER_BUTTON_Y: return PB_NORTH;
    case SDL_CONTROLLER_BUTTON_START: return PB_START; case SDL_CONTROLLER_BUTTON_BACK: return PB_BACK;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return PB_L1; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return PB_R1;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return PB_L3; case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return PB_R3;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return PB_DUP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return PB_DDOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return PB_DLEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return PB_DRIGHT;
    default: return -1; }
}
static void kh_cap_begin(bool on) { kh_cap_on = on; kh_cap_pb = -1; kh_cap_selheld = false; kh_cap_t0 = SDL_GetTicks(); }
static int kh_cap_poll(int & pb, bool & sel) {   /* 0 waiting - 1 got one - 2 ended (timeout 8 s) */
    if (!kh_cap_on) return 2;
    if (kh_cap_pb >= 0) { pb = kh_cap_pb; sel = kh_cap_sel; kh_cap_pb = -1; kh_cap_on = false; return 1; }
    if (SDL_GetTicks() - kh_cap_t0 > 8000u) { kh_cap_on = false; return 2; }
    return 0;
}
static bool kh_raw_on = false, kh_raw_exit = false; static std::string kh_raw_l[8]; static int kh_raw_n = 0; static Uint32 kh_raw_t0 = 0;
static void kh_raw_push(const std::string & s) {   /* newest first; the same line twice in a row is dropped */
    if (kh_raw_n > 0 && kh_raw_l[0] == s) return;
    for (int i = 7; i > 0; i--) kh_raw_l[i] = kh_raw_l[i - 1];
    kh_raw_l[0] = s; kh_raw_n++; GPLOG("RAW %s", s.c_str());
}
static bool kh_raw_event(const SDL_Event & e) {   /* extra-buttons tab: show what the app receives. true = swallowed (no menu navigation) */
    if (!kh_raw_on) return false;
    char b[96];
    switch (e.type) {
    case SDL_KEYDOWN: if (e.key.repeat) return true; snprintf(b, sizeof b, "KEY %s  sym=%d scan=%d", SDL_GetKeyName(e.key.keysym.sym), (int)e.key.keysym.sym, (int)e.key.keysym.scancode); kh_raw_push(b); return true;
    case SDL_KEYUP: return true;
    case SDL_JOYBUTTONDOWN: snprintf(b, sizeof b, "JOY button %d (dev %d)", (int)e.jbutton.button, (int)e.jbutton.which); kh_raw_push(b); return true;
    case SDL_JOYBUTTONUP: return true;
    case SDL_CONTROLLERBUTTONDOWN: { const char * q = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)e.cbutton.button); snprintf(b, sizeof b, "PAD %s", q ? q : "?"); kh_raw_push(b); return true; }
    case SDL_CONTROLLERBUTTONUP: return true;
    case SDL_JOYHATMOTION: if (e.jhat.value) { snprintf(b, sizeof b, "HAT %d value=%d", (int)e.jhat.hat, (int)e.jhat.value); kh_raw_push(b); } return true;
    case SDL_CONTROLLERAXISMOTION: if (e.caxis.value > 16000 || e.caxis.value < -16000) { const char * q = SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)e.caxis.axis); snprintf(b, sizeof b, "AXIS %s %s", q ? q : "?", e.caxis.value > 0 ? "+" : "-"); kh_raw_push(b); } return true;
    case SDL_JOYAXISMOTION: case SDL_FINGERMOTION: case SDL_FINGERUP: case SDL_MOUSEBUTTONUP: case SDL_MOUSEMOTION: return true;
    case SDL_FINGERDOWN: case SDL_MOUSEBUTTONDOWN: kh_raw_exit = true; return true;
    default: return false; }
}
static bool kh_cap_event(const SDL_Event & e) {   /* swallow pad input while the hotkey screen waits for a combo */
    if (!kh_cap_on) return false;
    if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
        const int pb = kh_pb_of_sdl(e.cbutton.button); const bool dn = e.type == SDL_CONTROLLERBUTTONDOWN;
        if (pb == PB_BACK) { kh_cap_selheld = dn; return true; }
        if (dn && pb >= 0 && pb != PB_START) { kh_cap_pb = pb; kh_cap_sel = kh_cap_selheld; GPLOG("CAPTURE pb=%d sel=%d", pb, (int)kh_cap_sel); }
        return true; }
    if (e.type == SDL_CONTROLLERAXISMOTION && (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
        static bool on[2]; const int q = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? 1 : 0;
        if (!on[q] && e.caxis.value > 16000) { on[q] = true; kh_cap_pb = q ? PB_R2 : PB_L2; kh_cap_sel = kh_cap_selheld; GPLOG("CAPTURE pb=%d sel=%d", kh_cap_pb, (int)kh_cap_sel); }
        else if (on[q] && e.caxis.value < 8000) on[q] = false;
        return true; }
    return false;
}
static bool kh_hot_try(int i, bool down) {   /* SELECT held + button (or a single button) = hotkey; the button is swallowed */
    if (i == PB_BACK || i == PB_START) return false;
    if (down) {
        for (int q = 1; q < FN_N; q++) if (kh_hot[q].btn == i && kh_hot[q].sel == kh_sel_dn) {
            kh_hot_sw[i] = true; GPLOG("  HOTKEY %s%d -> fn %d", kh_sel_dn ? "SEL+" : "", i, q); if (kh_fn_hook) kh_fn_hook(q, true); return true; }
        return false; }
    if (kh_hot_sw[i]) { kh_hot_sw[i] = false; return true; }
    return false;
}
static void kh_bind_fire(int i, bool down, const char* why) {
    if (kh_cap_on) return;
    if (kh_hot_try(i, down)) return;
    const GPadBind& b = kh_pb[i];
    switch (b.type) {
    case PBT_KEY:  kh_key(b.key, down, why); break;
    case PBT_ML:   kh_vmouse_button(0, down); GPLOG("  %s -> vmouse L %d", why, (int)down); break;
    case PBT_MR:   kh_vmouse_button(1, down); GPLOG("  %s -> vmouse R %d", why, (int)down); break;
    case PBT_FN:   GPLOG("  %s -> fn %d %s", why, b.fn, down ? "down" : "up"); if (kh_fn_hook) kh_fn_hook(b.fn, down); break;
    default:       GPLOG("  %s -> (배정 없음)", why); break;
    }
}

/* Japanese layout (기본): east button (PlayStation ○, Xbox B) = confirm(Space), south (✕ / A) = cancel(Esc),
 * west (□ / X) = fast(CapsLock), north (△ / Y) = our menu, D-pad / left stick = arrows, Start = Enter.
 * t48e: 위 배정은 kh_pb[] 로 바꿀 수 있고(서양식은 동·남의 기본이 서로 바뀐다), L1 R1 SELECT 도 배정할 수 있다. */
static bool kh_button(Uint8 b, bool down) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_B:          if (kh_padmouse) { kh_vmouse_button(0, down); GPLOG("  EAST -> vmouse L %d", (int)down); } else kh_bind_fire(PB_EAST, down, "EAST(confirm)"); return true;
    case SDL_CONTROLLER_BUTTON_A:          if (kh_padmouse) { kh_vmouse_button(1, down); GPLOG("  SOUTH -> vmouse R %d", (int)down); } else kh_bind_fire(PB_SOUTH, down, "SOUTH(cancel)"); return true;
    case SDL_CONTROLLER_BUTTON_X:          kh_bind_fire(PB_WEST, down, "WEST(fast)"); return true;
    case SDL_CONTROLLER_BUTTON_Y:          kh_bind_fire(PB_NORTH, down, "NORTH(menu)"); return true;   /* 빌드 5a: 기본 = 큐하치 메뉴(fn:menu), 바꿀 수 있다. 메뉴는 SELECT+START 로도 항상 열린다 */
    case SDL_CONTROLLER_BUTTON_START:
        kh_st_dn = down;
        if (down && kh_sel_dn) { kh_sw_st = true; GPLOG("  SELECT+START -> menu (fixed)"); if (kh_fn_hook) kh_fn_hook(FN_MENU, true); return true; }
        if (!down && kh_sw_st) { kh_sw_st = false; return true; }
        kh_bind_fire(PB_START, down, "START(enter)"); return true;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  kh_bind_fire(PB_L1, down, "L1"); return true;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: kh_bind_fire(PB_R1, down, "R1"); return true;
    case SDL_CONTROLLER_BUTTON_BACK:
        kh_sel_dn = down;
        if (down && kh_st_dn) { kh_sw_sel = true; GPLOG("  SELECT+START -> menu (fixed)"); if (kh_fn_hook) kh_fn_hook(FN_MENU, true); return true; }
        if (!down && kh_sw_sel) { kh_sw_sel = false; return true; }
        kh_bind_fire(PB_BACK, down, "SELECT"); return true;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:  kh_bind_fire(PB_L3, down, "L3"); return true;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: kh_bind_fire(PB_R3, down, "R3"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    if (kh_padmouse) kh_dpad_mouse[2] = down; else kh_bind_fire(PB_DUP, down, "DPAD_UP"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  if (kh_padmouse) kh_dpad_mouse[3] = down; else kh_bind_fire(PB_DDOWN, down, "DPAD_DOWN"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  if (kh_padmouse) kh_dpad_mouse[0] = down; else kh_bind_fire(PB_DLEFT, down, "DPAD_LEFT"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: if (kh_padmouse) kh_dpad_mouse[1] = down; else kh_bind_fire(PB_DRIGHT, down, "DPAD_RIGHT"); return true;
    default: GPLOG("  button %d %s (unassigned)", (int)b, down ? "down" : "up"); return true;
    }
}

/* 빌드 5a: L2·R2 트리거 → 배정표 (옛 R2 배속 임계값 그대로: 켜짐 > 16000, 꺼짐 < 8000) */
static void kh_trigger(int axis, Sint16 v) {
    int q = axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? 1 : 0; bool & on = kh_trig_on[q];
    if (!on && v > 16000) { on = true; GPLOG("  %s ON (value=%d)", q ? "R2" : "L2", (int)v); kh_bind_fire(q ? PB_R2 : PB_L2, true, q ? "R2" : "L2"); }
    else if (on && v < 8000) { on = false; GPLOG("  %s OFF (value=%d)", q ? "R2" : "L2", (int)v); kh_bind_fire(q ? PB_R2 : PB_L2, false, q ? "R2" : "L2"); }
}
static void kh_stick(int axis, Sint16 v) {
    /* left stick -> arrows with hysteresis (on >16000, off <8000) */
    static const int neg[2] = { PB_DLEFT, PB_DUP }, pos[2] = { PB_DRIGHT, PB_DDOWN };   /* 빌드 5a: 왼쪽 스틱 = 십자키 배정을 따른다 */
    int a = (axis == SDL_CONTROLLER_AXIS_LEFTX) ? 0 : 1;
    bool& n = kh_axis_dir[0][a * 2]; bool& p = kh_axis_dir[0][a * 2 + 1];
    if (!n && v < -16000) { n = true;  kh_bind_fire(neg[a], true, "LSTICK-"); }
    if (n && v > -8000)   { n = false; kh_bind_fire(neg[a], false, "LSTICK-"); }
    if (!p && v > 16000)  { p = true;  kh_bind_fire(pos[a], true, "LSTICK+"); }
    if (p && v < 8000)    { p = false; kh_bind_fire(pos[a], false, "LSTICK+"); }
}

/* t49a (#823): 패드 제거 때 눌려 있던 입력을 모두 놓는다(제거 이벤트가 오면 버튼 UP 이 오지 않아 키가 눌린 채 남는 것을 막는다) */
static void kh_release_all(void) {
    static const Uint8 B[] = { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT };
    for (Uint8 b : B) kh_button(b, false);
    kh_button(SDL_CONTROLLER_BUTTON_LEFTSTICK, false); kh_button(SDL_CONTROLLER_BUTTON_RIGHTSTICK, false);
    for (int q = 0; q < 2; q++) if (kh_trig_on[q]) { kh_trig_on[q] = false; kh_bind_fire(q ? PB_R2 : PB_L2, false, "TRIGGER release"); }
    kh_sel_dn = kh_st_dn = kh_sw_sel = kh_sw_st = false; memset(kh_hot_sw, 0, sizeof kh_hot_sw);
    kh_stick(SDL_CONTROLLER_AXIS_LEFTX, 0); kh_stick(SDL_CONTROLLER_AXIS_LEFTY, 0);
    for (int q = 0; q < 4; q++) kh_dpad_mouse[q] = false;
    GPLOG("RELEASE ALL (패드 제거)");
}
/* per-frame: padmouse cursor motion */
static void kh_input_tick(void) {
    if (!kh_padmouse) return;
    float dx = (float)(kh_dpad_mouse[1] - kh_dpad_mouse[0]) * 4.0f;
    float dy = (float)(kh_dpad_mouse[3] - kh_dpad_mouse[2]) * 4.0f;
    if (dx != 0 || dy != 0) kh_vmouse_move_rel(dx, dy);
}

/* t48e (#651 2): 폴백 패드의 원시 이벤트 → 표준 GameController 이벤트로 바꿔 다시 밀어 넣는다(메뉴·게임이 같은 경로를 탄다).
 * 표에 있는 패드(SDL 이 GameController 이벤트를 따로 내는 장치)의 JOY* 이벤트는 건드리지 않는다. true = 소비 */
static void kh_push_cbtn(SDL_JoystickID id, Uint8 btn, bool down) {
    SDL_Event ev; SDL_zero(ev); ev.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP; ev.cbutton.which = id; ev.cbutton.button = btn; ev.cbutton.state = down ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&ev);
}
static bool gensei_raw_event(const SDL_Event& e) {
    static const Uint8 MAP[10] = { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK };
    switch (e.type) {
    case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP: {
        if (!kh_rawpads.count(e.jbutton.which)) return false;
        bool down = e.type == SDL_JOYBUTTONDOWN;
        if (e.jbutton.button < 10) { GPLOG("RAW btn %d %s -> std %s", (int)e.jbutton.button, down ? "down" : "up", SDL_GameControllerGetStringForButton((SDL_GameControllerButton)MAP[e.jbutton.button])); kh_push_cbtn(e.jbutton.which, MAP[e.jbutton.button], down); }
        else GPLOG("RAW btn %d %s (미배정)", (int)e.jbutton.button, down ? "down" : "up");
        return true; }
    case SDL_JOYHATMOTION: {
        if (!kh_rawpads.count(e.jhat.which) || e.jhat.hat != 0) return false;
        int& old = kh_rawhat[e.jhat.which]; int nw = e.jhat.value;
        static const struct { int bit; Uint8 b; const char* nm; } H[4] = { { SDL_HAT_UP, SDL_CONTROLLER_BUTTON_DPAD_UP, "UP" }, { SDL_HAT_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "RIGHT" }, { SDL_HAT_DOWN, SDL_CONTROLLER_BUTTON_DPAD_DOWN, "DOWN" }, { SDL_HAT_LEFT, SDL_CONTROLLER_BUTTON_DPAD_LEFT, "LEFT" } };
        for (int i = 0; i < 4; i++) { bool o = (old & H[i].bit) != 0, n = (nw & H[i].bit) != 0; if (o != n) { GPLOG("RAW hat %s %s", H[i].nm, n ? "down" : "up"); kh_push_cbtn(e.jhat.which, H[i].b, n); } }
        old = nw; return true; }
    case SDL_JOYAXISMOTION: {
        if (!kh_rawpads.count(e.jaxis.which) || e.jaxis.axis > 1) return false;
        SDL_Event ev; SDL_zero(ev); ev.type = SDL_CONTROLLERAXISMOTION; ev.caxis.which = e.jaxis.which; ev.caxis.axis = e.jaxis.axis == 0 ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_LEFTY; ev.caxis.value = e.jaxis.value; SDL_PushEvent(&ev);
        return true; }
    }
    return false;
}

/* returns true if the event was consumed */
static bool gensei_input_event(const SDL_Event& e) {
    switch (e.type) {
    case SDL_CONTROLLERDEVICEADDED:   GPLOG("DEVICE added idx=%d", (int)e.cdevice.which); gensei_open_pad(e.cdevice.which); return true;
    case SDL_CONTROLLERDEVICEREMOVED: {
        GPLOG("DEVICE removed id=%d", (int)e.cdevice.which);
        auto it = kh_pads.find(e.cdevice.which);
        std::string rmn = "?"; if (it != kh_pads.end()) { const char * q = SDL_GameControllerName(it->second); if (q) rmn = q; SDL_GameControllerClose(it->second); kh_pads.erase(it); kh_pad_closes++; GPLOG("PADCOUNT open=%d close=%d live=%d", kh_pad_opens, kh_pad_closes, (int)kh_pads.size()); kh_release_all(); if (kh_hotplug_hook) kh_hotplug_hook(false, rmn.c_str()); }
        if (kh_pads.empty() && kh_rawpads.empty()) gensei_padkind_v = 0;
        return true; }
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        bool down = e.type == SDL_CONTROLLERBUTTONDOWN;
        GPLOG("BUTTON id=%d btn=%d(%s) %s", (int)e.cbutton.which, (int)e.cbutton.button,
              SDL_GameControllerGetStringForButton((SDL_GameControllerButton)e.cbutton.button), down ? "down" : "up");
        return kh_button(e.cbutton.button, down); }
    case SDL_CONTROLLERAXISMOTION:
        if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) kh_stick(e.caxis.axis, e.caxis.value);
        else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) kh_trigger(e.caxis.axis, e.caxis.value);
        return true;
    case SDL_JOYDEVICEADDED:   /* 표에 없는 패드만 여기서 연다(표에 있는 패드는 SDL_CONTROLLERDEVICEADDED 가 따로 온다) */
        if (!SDL_IsGameController(e.jdevice.which)) { GPLOG("JOYDEVICE added idx=%d (not a game controller)", (int)e.jdevice.which); gensei_open_pad(e.jdevice.which); }
        return true;
    case SDL_JOYDEVICEREMOVED: {
        auto it = kh_rawpads.find(e.jdevice.which);
        if (it != kh_rawpads.end()) { GPLOG("RAW pad removed id=%d", (int)e.jdevice.which); std::string rwn = "?"; { const char * q = SDL_JoystickName(it->second); if (q) rwn = q; } SDL_JoystickClose(it->second); kh_rawpads.erase(it); kh_rawhat.erase(e.jdevice.which); if (kh_pads.empty() && kh_rawpads.empty()) gensei_padkind_v = 0; kh_release_all(); if (kh_hotplug_hook) kh_hotplug_hook(false, rwn.c_str()); }
        return true; }
    case SDL_JOYAXISMOTION: case SDL_JOYBALLMOTION: case SDL_JOYHATMOTION:
    case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
        return true;   /* raw joystick events are swallowed: the mapper must not see them (폴백 패드는 gensei_raw_event 가 먼저 표준 이벤트로 바꿔 놓았다) */
    }
    return false;
}

/* mapper hand_* entries that stay allowed while gensei_lock is on */
bool gensei_hand_allowed(const char* name) {
    return name && (!strcmp(name, "fullscr") || !strcmp(name, "hand_fullscr"));
}
