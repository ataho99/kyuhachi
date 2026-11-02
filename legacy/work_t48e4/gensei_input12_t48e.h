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
#include "gensei_kbtable.h"

bool gensei_lock = false;              /* true when -gensei: mapper hot keys (edit/config/etc.) blocked */
static bool gensei_padmouse = false;
/* t48e: 버튼 배정표. 위치 기준(동=○ 남=× 서=□). 기본은 옛 t34 와 같다: 동 Space · 남 Esc · 서 CapsLock · START Enter · L1 R1 SELECT 없음 */
enum { PB_EAST = 0, PB_SOUTH, PB_WEST, PB_START, PB_L1, PB_R1, PB_BACK, PB_N };
enum { PBT_KEY = 0, PBT_NONE, PBT_ML, PBT_MR };
struct GPadBind { int type; KBD_KEYS key; };
static GPadBind gensei_pb[PB_N];
static bool gensei_western = false;    /* t48e (#651 4): 서양식 = 남쪽 버튼이 결정. settings.ini pad=1 */
static void gensei_pb_defaults(void) {
    gensei_pb[PB_EAST].type = PBT_KEY;  gensei_pb[PB_EAST].key = gensei_western ? KBD_esc : KBD_space;
    gensei_pb[PB_SOUTH].type = PBT_KEY; gensei_pb[PB_SOUTH].key = gensei_western ? KBD_space : KBD_esc;
    gensei_pb[PB_WEST].type = PBT_KEY;  gensei_pb[PB_WEST].key = KBD_capslock;
    gensei_pb[PB_START].type = PBT_KEY; gensei_pb[PB_START].key = KBD_enter;
    gensei_pb[PB_L1].type = PBT_NONE;   gensei_pb[PB_L1].key = KBD_NONE;
    gensei_pb[PB_R1].type = PBT_NONE;   gensei_pb[PB_R1].key = KBD_NONE;
    gensei_pb[PB_BACK].type = PBT_NONE; gensei_pb[PB_BACK].key = KBD_NONE;
}
static bool gensei_pb_init_done = (gensei_pb_defaults(), true);
static bool gensei_never[256];   /* #237 never_map: 패드·단축키에 절대 보내지 않는 키(KBD_KEYS 값) */   /* env GENSEI_PADMOUSE=1 : D-pad moves the cursor, A/B = L/R click */
static std::map<SDL_JoystickID, SDL_GameController*> gensei_pads;
static std::map<SDL_JoystickID, SDL_Joystick*> gensei_rawpads;   /* t48e: 표(매핑)에 없는 패드 — 원시 번호 폴백 */
static std::map<SDL_JoystickID, int> gensei_rawhat;               /* 폴백 패드의 십자 상태(SDL_HAT_* 비트) */
static int gensei_padkind_v = 0;       /* 0 그 밖 1 Xbox 2 PlayStation (첫 번째로 열린 패드의 이름으로 판정) */
static FILE* gensei_plog = NULL;
static bool gensei_axis_dir[2][4];     /* [pad-slot][L,R,U,D] stick->arrow state (pad 0 only used) */
static int  gensei_dpad_mouse[4];      /* L,R,U,D held (padmouse mode) */

static void gensei_plogf(const char* fmt, ...) {
    if (!gensei_plog) return;
    va_list va; va_start(va, fmt); vfprintf(gensei_plog, fmt, va); va_end(va);
    fputc('\n', gensei_plog); fflush(gensei_plog);
}
#define GPLOG(...) gensei_plogf(__VA_ARGS__)

/* ---- virtual mouse layer (device -> vmouse -> PC-98 mouse). Nova's own method plugs in here later. ---- */
/* t34 (#268): 코어 버스마우스 이동량 레지스터는 int8 — 한 틱에 127 넘게 몰아 주면 부호가 뒤집힌다(실측: y -150 한 번에 → 이동 안 됨). 대기열에 모아 20ms 마다 ±100 이하로 나눠 넣는다. */
static float gensei_vm_px = 0, gensei_vm_py = 0; static Uint32 gensei_vm_last = 0;
static void gensei_vmouse_move_rel(float dx, float dy) { gensei_vm_px += dx; gensei_vm_py += dy; }
static void gensei_vmouse_pump() {
    extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y;
    int ix = (int)gensei_vm_px, iy = (int)gensei_vm_py; if (!ix && !iy) return;
    Uint32 now = SDL_GetTicks(); if (now - gensei_vm_last < 20) return; gensei_vm_last = now;
    int lox = -100, hix = 100, loy = -100, hiy = 100;
    { int a = -128 - (int)p7fd9_8255_mouse_x, b = 127 - (int)p7fd9_8255_mouse_x; if (a > lox) lox = a; if (b < hix) hix = b; }
    { int a = -128 - (int)p7fd9_8255_mouse_y, b = 127 - (int)p7fd9_8255_mouse_y; if (a > loy) loy = a; if (b < hiy) hiy = b; }
    int ax = ix < lox ? lox : ix > hix ? hix : ix, ay = iy < loy ? loy : iy > hiy ? hiy : iy;
    if (ax || ay) { Mouse_CursorMoved((float)ax, (float)ay, 0, 0, false); gensei_vm_px -= (float)ax; gensei_vm_py -= (float)ay; }
}
static void gensei_vmouse_button(int b /*0 L,1 R,2 M*/, bool down) {
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

static void gensei_open_pad(int idx) {
    if (!SDL_IsGameController(idx)) {
        /* t48e (#651 2): 표에 없는 패드 — GUID 한 줄을 남기고 원시 번호로 폴백 */
        char gs[64] = "?"; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), gs, sizeof gs);
        const char* jn = SDL_JoystickNameForIndex(idx);
        SDL_Joystick* js = SDL_JoystickOpen(idx);
        if (!js) { GPLOG("PADMAP unknown joystick %d guid=%s name='%s' — open fail: %s", idx, gs, jn ? jn : "?", SDL_GetError()); return; }
        SDL_JoystickID id = SDL_JoystickInstanceID(js);
        gensei_rawpads[id] = js; gensei_rawhat[id] = 0;
        GPLOG("PADMAP unknown joystick idx=%d id=%d guid=%s name='%s' buttons=%d hats=%d axes=%d -> RAW fallback (0=남 1=동 2=서 3=북 4=L1 5=R1 6=SELECT 7=START, 십자=hat0, 왼스틱=축0/1)", idx, (int)id, gs, jn ? jn : "?", SDL_JoystickNumButtons(js), SDL_JoystickNumHats(js), SDL_JoystickNumAxes(js));
        if (!gensei_padkind_v) gensei_padkind_v = gensei_padkind_of(jn);
        return;
    }
    SDL_GameController* gc = SDL_GameControllerOpen(idx);
    if (!gc) { GPLOG("open fail %d: %s", idx, SDL_GetError()); return; }
    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    if (gensei_pads.count(id)) { GPLOG("PAD id=%d 이미 열려 있음 (중복 이벤트 무시)", (int)id); return; }
    gensei_pads[id] = gc;
    const char* nm = SDL_GameControllerName(gc);
    char gs[64] = "?"; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), gs, sizeof gs);
    char* mp = SDL_GameControllerMapping(gc);
    GPLOG("PAD opened idx=%d id=%d name='%s' guid=%s -> SDL GameController 표준 배치 (매핑 %.70s%s)", idx, (int)id, nm ? nm : "?", gs, mp ? mp : "?", (mp && strlen(mp) > 70) ? "..." : "");
    if (mp) SDL_free(mp);
    if (gensei_pads.size() == 1 || !gensei_padkind_v) gensei_padkind_v = gensei_padkind_of(nm);
    GPLOG("PADKIND %d (0 그 밖 1 Xbox 2 PlayStation)", gensei_padkind_v);
}

/* t48e (#651 2): SDL_GameControllerDB(zlib) 를 기동 때 읽는다. 없으면 로그만 남기고 계속 */
static std::string gensei_db_dir;   /* 호스트가 exe 폴더를 넣는다 (기동 때 <exe>\res\gamecontrollerdb.txt) */
static void gensei_load_db(void) {
    std::vector<std::string> P;
    if (!gensei_db_dir.empty()) { P.push_back(gensei_db_dir + "\\res\\gamecontrollerdb.txt"); P.push_back(gensei_db_dir + "/res/gamecontrollerdb.txt"); }
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

static void gensei_input_init(void) {
    gensei_lock = true;
    const char* pm = getenv("GENSEI_PADMOUSE");
    gensei_padmouse = pm && pm[0] == '1';
    gensei_plog = fopen("pc98player_pad_log.txt", "a");
    time_t t = time(NULL);
    GPLOG("== GENSEI r004 input layer start %s padmouse=%d", ctime(&t), (int)gensei_padmouse);
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) { GPLOG("GAMECONTROLLER init fail: %s", SDL_GetError()); return; }
    { SDL_version cv; SDL_VERSION(&cv); SDL_version lv; SDL_GetVersion(&lv); GPLOG("SDL compiled %d.%d.%d linked %d.%d.%d", (int)cv.major, (int)cv.minor, (int)cv.patch, (int)lv.major, (int)lv.minor, (int)lv.patch); }
    gensei_load_db();
    SDL_GameControllerEventState(SDL_ENABLE);
    int n = SDL_NumJoysticks();
    GPLOG("SDL joysticks=%d", n);
    for (int i = 0; i < n; i++) gensei_open_pad(i);
}

/* t48e: 코어로 보내는 PC-98 키의 스캔코드 (kbtable = DOSBox-X keyboard.cpp KEYBOARD_PC98_AddKey 의 switch 값) — 진단 로그용 */
static int gensei_scan_of(KBD_KEYS k) {
    for (int i = 0; i < kbt::g_nkeys; i++) if ((int)k == kbt::g_keys[i].kbd || (kbt::g_keys[i].kbd_ibm >= 0 && (int)k == kbt::g_keys[i].kbd_ibm)) return kbt::g_keys[i].scan;
    return -1;
}

static void gensei_key(KBD_KEYS k, bool down, const char* why) {
    if ((int)k >= 0 && (int)k < 256 && gensei_never[(int)k]) { GPLOG("  %s -> key %d BLOCKED (never_map)", why, (int)k); return; }
    KEYBOARD_AddKey(k, down);
    int sc = gensei_scan_of(k);
    if (sc >= 0) GPLOG("  %s -> key %d scan=0x%02X %s", why, (int)k, sc, down ? "down" : "up");
    else GPLOG("  %s -> key %d %s", why, (int)k, down ? "down" : "up");
}

/* t48e: 배정표대로 한 버튼을 실행 */
static void gensei_bind_fire(int i, bool down, const char* why) {
    const GPadBind& b = gensei_pb[i];
    switch (b.type) {
    case PBT_KEY:  gensei_key(b.key, down, why); break;
    case PBT_ML:   gensei_vmouse_button(0, down); GPLOG("  %s -> vmouse L %d", why, (int)down); break;
    case PBT_MR:   gensei_vmouse_button(1, down); GPLOG("  %s -> vmouse R %d", why, (int)down); break;
    default:       GPLOG("  %s -> (배정 없음)", why); break;
    }
}

/* Japanese layout (기본): east button (PlayStation ○, Xbox B) = confirm(Space), south (✕ / A) = cancel(Esc),
 * west (□ / X) = fast(CapsLock), north (△ / Y) = our menu, D-pad / left stick = arrows, Start = Enter.
 * t48e: 위 배정은 gensei_pb[] 로 바꿀 수 있고(서양식은 동·남의 기본이 서로 바뀐다), L1 R1 SELECT 도 배정할 수 있다. */
static bool gensei_button(Uint8 b, bool down) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_B:          if (gensei_padmouse) { gensei_vmouse_button(0, down); GPLOG("  EAST -> vmouse L %d", (int)down); } else gensei_bind_fire(PB_EAST, down, "EAST(confirm)"); return true;
    case SDL_CONTROLLER_BUTTON_A:          if (gensei_padmouse) { gensei_vmouse_button(1, down); GPLOG("  SOUTH -> vmouse R %d", (int)down); } else gensei_bind_fire(PB_SOUTH, down, "SOUTH(cancel)"); return true;
    case SDL_CONTROLLER_BUTTON_X:          gensei_bind_fire(PB_WEST, down, "WEST(fast)"); return true;
    case SDL_CONTROLLER_BUTTON_Y:          return true;   /* r012: △ = 우리 메뉴(호스트가 먼저 가로챔). F10 은 메뉴의 「PC-98 키 보내기」로만 */
    case SDL_CONTROLLER_BUTTON_START:      gensei_bind_fire(PB_START, down, "START(enter)"); return true;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  gensei_bind_fire(PB_L1, down, "L1"); return true;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: gensei_bind_fire(PB_R1, down, "R1"); return true;
    case SDL_CONTROLLER_BUTTON_BACK:       gensei_bind_fire(PB_BACK, down, "SELECT"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    if (gensei_padmouse) gensei_dpad_mouse[2] = down; else gensei_key(KBD_up, down, "DPAD_UP"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  if (gensei_padmouse) gensei_dpad_mouse[3] = down; else gensei_key(KBD_down, down, "DPAD_DOWN"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  if (gensei_padmouse) gensei_dpad_mouse[0] = down; else gensei_key(KBD_left, down, "DPAD_LEFT"); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: if (gensei_padmouse) gensei_dpad_mouse[1] = down; else gensei_key(KBD_right, down, "DPAD_RIGHT"); return true;
    default: GPLOG("  button %d %s (unassigned)", (int)b, down ? "down" : "up"); return true;
    }
}

static void gensei_stick(int axis, Sint16 v) {
    /* left stick -> arrows with hysteresis (on >16000, off <8000) */
    static const KBD_KEYS neg[2] = { KBD_left, KBD_up }, pos[2] = { KBD_right, KBD_down };
    int a = (axis == SDL_CONTROLLER_AXIS_LEFTX) ? 0 : 1;
    bool& n = gensei_axis_dir[0][a * 2]; bool& p = gensei_axis_dir[0][a * 2 + 1];
    if (!n && v < -16000) { n = true;  gensei_key(neg[a], true, "LSTICK-"); }
    if (n && v > -8000)   { n = false; gensei_key(neg[a], false, "LSTICK-"); }
    if (!p && v > 16000)  { p = true;  gensei_key(pos[a], true, "LSTICK+"); }
    if (p && v < 8000)    { p = false; gensei_key(pos[a], false, "LSTICK+"); }
}

/* per-frame: padmouse cursor motion */
static void gensei_input_tick(void) {
    if (!gensei_padmouse) return;
    float dx = (float)(gensei_dpad_mouse[1] - gensei_dpad_mouse[0]) * 4.0f;
    float dy = (float)(gensei_dpad_mouse[3] - gensei_dpad_mouse[2]) * 4.0f;
    if (dx != 0 || dy != 0) gensei_vmouse_move_rel(dx, dy);
}

/* t48e (#651 2): 폴백 패드의 원시 이벤트 → 표준 GameController 이벤트로 바꿔 다시 밀어 넣는다(메뉴·게임이 같은 경로를 탄다).
 * 표에 있는 패드(SDL 이 GameController 이벤트를 따로 내는 장치)의 JOY* 이벤트는 건드리지 않는다. true = 소비 */
static void gensei_push_cbtn(SDL_JoystickID id, Uint8 btn, bool down) {
    SDL_Event ev; SDL_zero(ev); ev.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP; ev.cbutton.which = id; ev.cbutton.button = btn; ev.cbutton.state = down ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&ev);
}
static bool gensei_raw_event(const SDL_Event& e) {
    static const Uint8 MAP[10] = { SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK };
    switch (e.type) {
    case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP: {
        if (!gensei_rawpads.count(e.jbutton.which)) return false;
        bool down = e.type == SDL_JOYBUTTONDOWN;
        if (e.jbutton.button < 10) { GPLOG("RAW btn %d %s -> std %s", (int)e.jbutton.button, down ? "down" : "up", SDL_GameControllerGetStringForButton((SDL_GameControllerButton)MAP[e.jbutton.button])); gensei_push_cbtn(e.jbutton.which, MAP[e.jbutton.button], down); }
        else GPLOG("RAW btn %d %s (미배정)", (int)e.jbutton.button, down ? "down" : "up");
        return true; }
    case SDL_JOYHATMOTION: {
        if (!gensei_rawpads.count(e.jhat.which) || e.jhat.hat != 0) return false;
        int& old = gensei_rawhat[e.jhat.which]; int nw = e.jhat.value;
        static const struct { int bit; Uint8 b; const char* nm; } H[4] = { { SDL_HAT_UP, SDL_CONTROLLER_BUTTON_DPAD_UP, "UP" }, { SDL_HAT_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "RIGHT" }, { SDL_HAT_DOWN, SDL_CONTROLLER_BUTTON_DPAD_DOWN, "DOWN" }, { SDL_HAT_LEFT, SDL_CONTROLLER_BUTTON_DPAD_LEFT, "LEFT" } };
        for (int i = 0; i < 4; i++) { bool o = (old & H[i].bit) != 0, n = (nw & H[i].bit) != 0; if (o != n) { GPLOG("RAW hat %s %s", H[i].nm, n ? "down" : "up"); gensei_push_cbtn(e.jhat.which, H[i].b, n); } }
        old = nw; return true; }
    case SDL_JOYAXISMOTION: {
        if (!gensei_rawpads.count(e.jaxis.which) || e.jaxis.axis > 1) return false;
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
        auto it = gensei_pads.find(e.cdevice.which);
        if (it != gensei_pads.end()) { SDL_GameControllerClose(it->second); gensei_pads.erase(it); }
        if (gensei_pads.empty() && gensei_rawpads.empty()) gensei_padkind_v = 0;
        return true; }
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        bool down = e.type == SDL_CONTROLLERBUTTONDOWN;
        GPLOG("BUTTON id=%d btn=%d(%s) %s", (int)e.cbutton.which, (int)e.cbutton.button,
              SDL_GameControllerGetStringForButton((SDL_GameControllerButton)e.cbutton.button), down ? "down" : "up");
        return gensei_button(e.cbutton.button, down); }
    case SDL_CONTROLLERAXISMOTION:
        if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) gensei_stick(e.caxis.axis, e.caxis.value);
        return true;
    case SDL_JOYDEVICEADDED:   /* 표에 없는 패드만 여기서 연다(표에 있는 패드는 SDL_CONTROLLERDEVICEADDED 가 따로 온다) */
        if (!SDL_IsGameController(e.jdevice.which)) { GPLOG("JOYDEVICE added idx=%d (not a game controller)", (int)e.jdevice.which); gensei_open_pad(e.jdevice.which); }
        return true;
    case SDL_JOYDEVICEREMOVED: {
        auto it = gensei_rawpads.find(e.jdevice.which);
        if (it != gensei_rawpads.end()) { GPLOG("RAW pad removed id=%d", (int)e.jdevice.which); SDL_JoystickClose(it->second); gensei_rawpads.erase(it); gensei_rawhat.erase(e.jdevice.which); if (gensei_pads.empty() && gensei_rawpads.empty()) gensei_padkind_v = 0; }
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
