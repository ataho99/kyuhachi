/* GENSEI r004: SDL2 GameController input layer + virtual mouse layer (diagnostic build).
 * Device -> (this layer) -> PC-98 keyboard / PC-98 mouse. Included only by sdlmain.cpp. */
#pragma once
#include <SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <time.h>
#include "keyboard.h"
#include "mouse.h"

bool gensei_lock = false;              /* true when -gensei: mapper hot keys (edit/config/etc.) blocked */
static bool gensei_padmouse = false;
static KBD_KEYS gensei_pad_o = KBD_space, gensei_pad_x = KBD_esc, gensei_pad_sq = KBD_capslock, gensei_pad_start = KBD_enter;   /* #237: 작품 프로필 [pad] 로 바꾼다 */
static bool gensei_never[256];   /* #237 never_map: 패드·단축키에 절대 보내지 않는 키(KBD_KEYS 값) */   /* env GENSEI_PADMOUSE=1 : D-pad moves the cursor, A/B = L/R click */
static std::map<SDL_JoystickID, SDL_GameController*> gensei_pads;
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
static void gensei_vmouse_move_rel(float dx, float dy) { Mouse_CursorMoved(dx, dy, 0, 0, false); }
static void gensei_vmouse_button(int b /*0 L,1 R,2 M*/, bool down) {
    if (down) Mouse_ButtonPressed((uint8_t)b); else Mouse_ButtonReleased((uint8_t)b);
}

static void gensei_open_pad(int idx) {
    if (!SDL_IsGameController(idx)) { GPLOG("joystick %d is not a known game controller (no mapping)", idx); return; }
    SDL_GameController* gc = SDL_GameControllerOpen(idx);
    if (!gc) { GPLOG("open fail %d: %s", idx, SDL_GetError()); return; }
    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    gensei_pads[id] = gc;
    const char* nm = SDL_GameControllerName(gc);
    GPLOG("PAD opened idx=%d id=%d name='%s'", idx, (int)id, nm ? nm : "?");
}

static void gensei_input_init(void) {
    gensei_lock = true;
    const char* pm = getenv("GENSEI_PADMOUSE");
    gensei_padmouse = pm && pm[0] == '1';
    gensei_plog = fopen("pc98player_pad_log.txt", "a");
    time_t t = time(NULL);
    GPLOG("== GENSEI r004 input layer start %s padmouse=%d", ctime(&t), (int)gensei_padmouse);
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) { GPLOG("GAMECONTROLLER init fail: %s", SDL_GetError()); return; }
    SDL_GameControllerEventState(SDL_ENABLE);
    int n = SDL_NumJoysticks();
    GPLOG("SDL joysticks=%d", n);
    for (int i = 0; i < n; i++) gensei_open_pad(i);
}

static void gensei_key(KBD_KEYS k, bool down, const char* why) {
    if ((int)k >= 0 && (int)k < 256 && gensei_never[(int)k]) { GPLOG("  %s -> key %d BLOCKED (never_map)", why, (int)k); return; }
    KEYBOARD_AddKey(k, down);
    GPLOG("  %s -> key %d %s", why, (int)k, down ? "down" : "up");
}

/* Japanese layout: east button (PlayStation ○, Xbox B) = confirm(Space), south (✕ / A) = cancel(Esc),
 * west (□ / X) = fast(CapsLock), north (△ / Y) = F10, D-pad / left stick = arrows, Start = Enter. */
static bool gensei_button(Uint8 b, bool down) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_B:          if (gensei_padmouse) { gensei_vmouse_button(0, down); GPLOG("  EAST -> vmouse L %d", (int)down); } else gensei_key(gensei_pad_o, down, "EAST(confirm)"); return true;
    case SDL_CONTROLLER_BUTTON_A:          if (gensei_padmouse) { gensei_vmouse_button(1, down); GPLOG("  SOUTH -> vmouse R %d", (int)down); } else gensei_key(gensei_pad_x, down, "SOUTH(cancel)"); return true;
    case SDL_CONTROLLER_BUTTON_X:          gensei_key(gensei_pad_sq, down, "WEST(fast)"); return true;
    case SDL_CONTROLLER_BUTTON_Y:          return true;   /* r012: △ = 우리 메뉴(호스트가 먼저 가로챔). F10 은 메뉴의 「PC-98 키 보내기」로만 */
    case SDL_CONTROLLER_BUTTON_START:      gensei_key(gensei_pad_start, down, "START(enter)"); return true;
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

/* returns true if the event was consumed */
static bool gensei_input_event(const SDL_Event& e) {
    switch (e.type) {
    case SDL_CONTROLLERDEVICEADDED:   GPLOG("DEVICE added idx=%d", (int)e.cdevice.which); gensei_open_pad(e.cdevice.which); return true;
    case SDL_CONTROLLERDEVICEREMOVED: {
        GPLOG("DEVICE removed id=%d", (int)e.cdevice.which);
        auto it = gensei_pads.find(e.cdevice.which);
        if (it != gensei_pads.end()) { SDL_GameControllerClose(it->second); gensei_pads.erase(it); }
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
    case SDL_JOYAXISMOTION: case SDL_JOYBALLMOTION: case SDL_JOYHATMOTION:
    case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
    case SDL_JOYDEVICEADDED: case SDL_JOYDEVICEREMOVED:
        return true;   /* raw joystick events are swallowed: the mapper must not see them */
    }
    return false;
}

/* mapper hand_* entries that stay allowed while gensei_lock is on */
bool gensei_hand_allowed(const char* name) {
    return name && (!strcmp(name, "fullscr") || !strcmp(name, "hand_fullscr"));
}
