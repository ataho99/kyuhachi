/* gensei_kbov_t52f.h -- t46x 화면 키보드 오버레이 + t52f(#132) 오버레이 버튼 5개 (경로 B: 호스트가 게시 프레임 g_pub 위에 직접 합성).
 * 호스트 gensei_host.cpp 가 include (단일 번역 단위). SDL 이벤트는 모른다: 포인터는 가상 640x400 좌표로 받는다(kbov::pointer).
 * 입력층(SDL)은 호스트의 gh_kbd_event 한 함수 — 안드로이드는 그 함수만 바꾼다. 키 표 = gensei_kbtable_t46.h.
 * 눌림 상태는 이 안의 별도 집합이다 — 호스트의 g_key_down(sendkey·bootkeys·autokeys 공유 1칸)을 건드리지 않는다.
 * t52f: 버튼 줄 = [0]화면 키보드(기존 자리) [1]메뉴 [2]빠른 저장 [3]빠른 불러오기(길게) [4]배속 토글. 동작은 호스트 콜백 g_act 로. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "input/kh_kbtable.h"

namespace kbov {
/* ---- 배치 (가상 640x400) */
static const int PX = 8, PY = 214, PW = 624, PH = 150;            /* 키보드 패널 */
static const int GX = PX + 6, GY = PY + 3;                         /* 키 격자 원점 */
static const int BX = 4, BY = 368, BW = 40, BH = 28;               /* 버튼(보이는 크기). 칸 0 = 화면 키보드 버튼 */
static const int HX = 0, HY = 364, HW = 48, HH = 36;               /* 칸 0 히트 영역(기존) */
static const int NSLOT = 5, PITCH = 44;                            /* t52f: 버튼 5칸, 피치 44 (히트 영역은 칸마다 ±2 로 빈틈 없이) */
static const unsigned LP_MS = 600, WAKE_MS = 3000, FADE_MS = 1000; /* 길게 누름 · 진하게 유지 · 옅어지는 시간 */
static const int IDLE_A = 110;                                     /* 평소 불투명도(0..255) */
enum { OV_KBD = 0, OV_MENU = 1, OV_SAVE = 2, OV_LOAD = 3, OV_BOOST = 4 };
enum { ACT_MENU = 1, ACT_SAVE = 2, ACT_LOAD = 3, ACT_BOOST = 4, ACT_LOADSHORT = 5 };
static inline int sx(int s) { return BX + s * PITCH; }

/* ---- 호스트가 채우는 연결 */
typedef bool (*SendFn)(int kbd, bool down);   /* true = 실제로 코어에 보냈음 */
typedef bool (*BoolFn)(void);
typedef void (*LogFn)(const char * s);
typedef void (*ActFn)(int act);
typedef unsigned (*NowFn)(void);
static SendFn g_send = 0; static BoolFn g_ibm_fn = 0, g_caps_fn = 0, g_kana_fn = 0, g_boost_fn = 0; static LogFn g_logfn = 0; static ActFn g_act = 0; static NowFn g_now = 0;
static void logf_(const char * fmt, ...) { if (!g_logfn) return; char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); g_logfn(b); }

/* ---- 상태 */
static bool g_vis = false;            /* 키보드 보임 */
static bool g_btn_on = true;          /* 화면 키보드 버튼(칸 0) 표시(설정 kbbtn) */
static int g_cli_btn = -1;            /* 시험: --kbbtn 값(−1 = 설정을 따름) */
static bool g_ov_on = true;           /* t52f: 오버레이 버튼 줄 전체 표시(설정 ovbtn). 꺼지면 버튼은 모두 숨고 Alt+K 는 그대로 */
static int g_cli_ov = -1;             /* 시험: --ovbtn 값 */
static bool g_force_strong = false;   /* 시험: --ovstrong (항상 진하게) */
static bool g_dirty = true;
static int g_down_slot = -1;          /* 누르고 있는 칸 */
static unsigned g_wake_t = 0; static bool g_lp_on = false, g_lp_fired = false; static unsigned g_lp_t0 = 0; static bool g_lastboost = false;
static bool g_pressed[160];                           /* 키 index → 눌림(일반·토글) */
static bool g_latch[160];                             /* 수식키 잠금 */
static bool g_sent[160];                              /* 코어에 실제로 down 을 보냈는가 */
static int g_cnt[512];                                /* KBD 값별 down 개수(같은 KBD 가 둘 이상 겹치지 않게) */
struct Ptr { bool act; int key; };                    /* key: −1 키 없음(빈틈), −2 키보드 버튼(칸 0), −10−s 버튼 칸 s(1..4), ≥0 키 index */
static Ptr g_p[4] = { { false, -1 }, { false, -1 }, { false, -1 }, { false, -1 } };
static bool g_lastcaps = false, g_lastkana = false;
static uint32_t g_img[640 * 400];                     /* 가상 640x400 ARGB (알파 = 상위 바이트) */

static inline unsigned now_(void) { return g_now ? g_now() : 0u; }
static inline int key_x(int i) { return GX + kbt::g_keys[i].x4 * (kbt::KB_UNIT_PX / 4); }
static inline int key_y(int i) { return GY + kbt::g_keys[i].row * kbt::KB_UNIT_PX; }
static inline int key_w(int i) { return kbt::g_keys[i].w4 * (kbt::KB_UNIT_PX / 4) - 2; }
static inline int key_h(int i) { return kbt::g_keys[i].rows * kbt::KB_UNIT_PX - 2; }
static inline bool in_rect(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
static bool slot_shown(int s) { return g_ov_on && (s != OV_KBD || g_btn_on); }
static int slot_at(int x, int y) {   /* 히트 칸: 칸 0 = 0..45, 칸 s = sx(s)-2 .. +44 */
    if (!g_ov_on || y < HY || y >= HY + HH) return -1;
    for (int s = 0; s < NSLOT; s++) { if (!slot_shown(s)) continue; int x0 = s == 0 ? HX : sx(s) - 2, w = s == 0 ? PITCH + 2 : PITCH; if (x >= x0 && x < x0 + w) return s; }
    return -1;
}
static inline bool in_btn(int x, int y) { return slot_at(x, y) >= 0; }
static inline bool in_panel(int x, int y) { return g_vis && in_rect(x, y, PX, PY, PW, PH); }
static bool in_ui(int x, int y) { return in_btn(x, y) || in_panel(x, y); }
static bool any_owned(void) { for (int i = 0; i < 4; i++) if (g_p[i].act) return true; return false; }
static bool active(void) { return g_vis || g_ov_on || any_owned(); }
static void wake(void) { g_wake_t = now_(); }
static int find_label(const char * lbl, int nth = 0) { for (int i = 0; i < kbt::g_nkeys; i++) if (!strcmp(kbt::g_keys[i].label, lbl)) { if (nth-- == 0) return i; } return -1; }
static bool key_center(int i, int & vx, int & vy) { if (i < 0 || i >= kbt::g_nkeys) return false; vx = key_x(i) + key_w(i) / 2; vy = key_y(i) + key_h(i) / 2; return true; }
static int hit_key(int vx, int vy) { for (int i = 0; i < kbt::g_nkeys; i++) if (in_rect(vx, vy, key_x(i), key_y(i), key_w(i), key_h(i))) return i; return -1; }
static int kbd_of(int i) { const kbt::Key & k = kbt::g_keys[i]; bool ibm = g_ibm_fn ? g_ibm_fn() : false; return (ibm && k.kbd_ibm >= 0) ? k.kbd_ibm : k.kbd; }

/* 코어로 보내기: 같은 KBD 값은 0→1 일 때만 down, 1→0 일 때만 up */
static void send_key(int i, bool down) {
    int kv = kbd_of(i); if (kv < 0 || kv >= 512) return;
    if (down) {
        if (g_cnt[kv] == 0 && g_send) { g_sent[i] = g_send(kv, true); }
        else if (g_cnt[kv] == 0) g_sent[i] = false; else g_sent[i] = true;
        if (g_sent[i]) g_cnt[kv]++;
    } else {
        if (g_sent[i]) { if (--g_cnt[kv] == 0 && g_send) g_send(kv, false); g_sent[i] = false; }
    }
    logf_("key=%s kbd=%d scan=0x%02X %s%s", kbt::g_keys[i].label, kv, kbt::g_keys[i].scan, down ? "down" : "up", (down && !g_sent[i]) ? " (미전송: never_map 또는 부팅 전)" : "");
    g_dirty = true;
}
static void release_all(void) {
    int n = 0;
    for (int i = 0; i < kbt::g_nkeys && i < 160; i++) { if (g_sent[i]) { send_key(i, false); n++; } g_pressed[i] = false; g_latch[i] = false; }
    for (int i = 0; i < 4; i++) { g_p[i].act = false; g_p[i].key = -1; }
    g_down_slot = -1; g_lp_on = false; g_lp_fired = false; g_dirty = true;
    if (n) logf_("release_all: %d키 해제", n);
}
static void set_visible(bool v) {
    if (g_vis == v) return;
    if (!v) release_all();
    g_vis = v; g_dirty = true;
    logf_("키보드 %s (ibm배열=%d caps=%d kana=%d)", v ? "켬" : "끔", g_ibm_fn ? (int)g_ibm_fn() : -1, g_caps_fn ? (int)g_caps_fn() : -1, g_kana_fn ? (int)g_kana_fn() : -1);
}
static void key_down(int i) {
    const kbt::Key & k = kbt::g_keys[i];
    if (k.kind == kbt::K_MOD) {   /* 탭 잠금 / 재탭 해제 */
        if (!g_latch[i]) { g_latch[i] = true; send_key(i, true); } else { g_latch[i] = false; send_key(i, false); }
    } else { g_pressed[i] = true; send_key(i, true); }
}
static void key_up(int i) {
    const kbt::Key & k = kbt::g_keys[i];
    if (k.kind == kbt::K_MOD) return;
    g_pressed[i] = false; send_key(i, false);
}
static const char * slot_name(int s) { return s == OV_MENU ? "menu" : s == OV_SAVE ? "save" : s == OV_LOAD ? "load" : s == OV_BOOST ? "boost" : "kbd"; }
/* 빠른 불러오기 길게 누름: 호스트 메인 루프가 매 프레임 부른다 */
static void tick(void) {
    if (g_lp_on && !g_lp_fired && now_() - g_lp_t0 >= LP_MS) {
        g_lp_fired = true; logf_("OVBTN load 길게 눌림 %u ms -> 실행", now_() - g_lp_t0); if (g_act) g_act(ACT_LOAD); g_dirty = true;
    }
}
/* 포인터: phase 0 누름 · 1 뗌 · 2 이동. (vx,vy) = 가상 640x400 좌표. true = 소비(게임으로 안 감) */
static bool pointer(int id, int phase, int vx, int vy) {
    if (id < 0 || id >= 4) return false;
    wake();   /* 마우스가 움직이거나 터치가 있으면 몇 초 진하게 */
    Ptr & p = g_p[id];
    if (phase == 0) {
        int s = slot_at(vx, vy);
        if (s >= 0) {
            p.act = true; p.key = s == OV_KBD ? -2 : -10 - s; g_down_slot = s; g_dirty = true;
            if (s == OV_LOAD) { g_lp_on = true; g_lp_fired = false; g_lp_t0 = now_(); logf_("OVBTN load 누름"); }
            return true;
        }
        if (g_vis && in_panel(vx, vy)) { int k = hit_key(vx, vy); p.act = true; p.key = k; if (k >= 0) key_down(k); g_dirty = true; return true; }
        return false;
    }
    if (!p.act) return false;
    if (phase == 2) return true;
    if (p.key == -2) { if (slot_at(vx, vy) == OV_KBD) set_visible(!g_vis); g_down_slot = -1; }
    else if (p.key <= -11) {
        int s = -10 - p.key; bool inside = slot_at(vx, vy) == s; g_down_slot = -1;
        if (s == OV_LOAD) {
            if (g_lp_on && !g_lp_fired && inside) { logf_("OVBTN load 짧게 누름 %u ms -> 동작 안 함", now_() - g_lp_t0); if (g_act) g_act(ACT_LOADSHORT); }
            g_lp_on = false;
        } else if (inside) { logf_("OVBTN %s 눌림 -> 실행", slot_name(s)); if (g_act) g_act(s == OV_MENU ? ACT_MENU : s == OV_SAVE ? ACT_SAVE : ACT_BOOST); }
    }
    else if (p.key >= 0) key_up(p.key);
    p.act = false; p.key = -1; g_dirty = true; return true;
}

}   /* namespace kbov */
