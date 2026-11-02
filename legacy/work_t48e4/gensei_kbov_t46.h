/* gensei_kbov_t46.h -- t46x 화면 키보드 오버레이 (경로 B: 호스트가 게시 프레임 g_pub 위에 직접 합성).
 * 호스트 gensei_host.cpp 가 include (단일 번역 단위). SDL 이벤트는 모른다: 포인터는 가상 640x400 좌표로 받는다(kbov::pointer).
 * 입력층(SDL)은 호스트의 gh_kbd_event 한 함수 — 안드로이드는 그 함수만 바꾼다. 키 표 = gensei_kbtable_t46.h.
 * 눌림 상태는 이 안의 별도 집합이다 — 호스트의 g_key_down(sendkey·bootkeys·autokeys 공유 1칸)을 건드리지 않는다. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "gensei_kbtable.h"

namespace kbov {
/* ---- 배치 (가상 640x400) */
static const int PX = 8, PY = 214, PW = 624, PH = 150;            /* 키보드 패널 */
static const int GX = PX + 6, GY = PY + 3;                         /* 키 격자 원점 */
static const int BX = 4, BY = 368, BW = 40, BH = 28;               /* 토글 버튼(보이는 크기) */
static const int HX = 0, HY = 364, HW = 48, HH = 36;               /* 토글 버튼 히트 영역 */

/* ---- 호스트가 채우는 연결 */
typedef bool (*SendFn)(int kbd, bool down);   /* true = 실제로 코어에 보냈음 */
typedef bool (*BoolFn)(void);
typedef void (*LogFn)(const char * s);
static SendFn g_send = 0; static BoolFn g_ibm_fn = 0, g_caps_fn = 0, g_kana_fn = 0; static LogFn g_logfn = 0;
static void logf_(const char * fmt, ...) { if (!g_logfn) return; char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); g_logfn(b); }

/* ---- 상태 */
static bool g_vis = false;            /* 키보드 보임 */
static bool g_btn_on = true;          /* 토글 버튼 표시(설정 kbbtn) */
static int g_cli_btn = -1;            /* 시험: --kbbtn 값(−1 = 설정을 따름) */
static bool g_dirty = true, g_btn_down = false;
static bool g_pressed[160];                           /* 키 index → 눌림(일반·토글) */
static bool g_latch[160];                             /* 수식키 잠금 */
static bool g_sent[160];                              /* 코어에 실제로 down 을 보냈는가 */
static int g_cnt[512];                                /* KBD 값별 down 개수(같은 KBD 가 둘 이상 겹치지 않게) */
struct Ptr { bool act; int key; };                    /* key: −1 키 없음(빈틈), −2 토글 버튼, ≥0 키 index */
static Ptr g_p[4] = { { false, -1 }, { false, -1 }, { false, -1 }, { false, -1 } };
static bool g_lastcaps = false, g_lastkana = false;
static uint32_t g_img[640 * 400];                     /* 가상 640x400 ARGB (알파 = 상위 바이트) */

static inline int key_x(int i) { return GX + kbt::g_keys[i].x4 * (kbt::KB_UNIT_PX / 4); }
static inline int key_y(int i) { return GY + kbt::g_keys[i].row * kbt::KB_UNIT_PX; }
static inline int key_w(int i) { return kbt::g_keys[i].w4 * (kbt::KB_UNIT_PX / 4) - 2; }
static inline int key_h(int i) { return kbt::g_keys[i].rows * kbt::KB_UNIT_PX - 2; }
static inline bool in_rect(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
static inline bool in_btn(int x, int y) { return g_btn_on && in_rect(x, y, HX, HY, HW, HH); }
static inline bool in_panel(int x, int y) { return g_vis && in_rect(x, y, PX, PY, PW, PH); }
static bool in_ui(int x, int y) { return in_btn(x, y) || in_panel(x, y); }
static bool any_owned(void) { for (int i = 0; i < 4; i++) if (g_p[i].act) return true; return false; }
static bool active(void) { return g_vis || g_btn_on || any_owned(); }
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
    g_btn_down = false; g_dirty = true;
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
/* 포인터: phase 0 누름 · 1 뗌 · 2 이동. (vx,vy) = 가상 640x400 좌표. true = 소비(게임으로 안 감) */
static bool pointer(int id, int phase, int vx, int vy) {
    if (id < 0 || id >= 4) return false;
    Ptr & p = g_p[id];
    if (phase == 0) {
        if (in_btn(vx, vy)) { p.act = true; p.key = -2; g_btn_down = true; g_dirty = true; return true; }
        if (g_vis && in_panel(vx, vy)) { int k = hit_key(vx, vy); p.act = true; p.key = k; if (k >= 0) key_down(k); g_dirty = true; return true; }
        return false;
    }
    if (!p.act) return false;
    if (phase == 2) return true;
    if (p.key == -2) { if (in_rect(vx, vy, HX, HY, HW, HH)) set_visible(!g_vis); g_btn_down = false; }
    else if (p.key >= 0) key_up(p.key);
    p.act = false; p.key = -1; g_dirty = true; return true;
}

/* ---- 그리기 (가상 캔버스에 ARGB 로 그린 뒤 프레임에 알파 합성) */
static void fill(int x, int y, int w, int h, uint32_t c) { for (int j = y < 0 ? 0 : y; j < y + h && j < 400; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < 640; i++) g_img[j * 640 + i] = c; }
static void rebuild(void) {
    memset(g_img, 0, sizeof g_img);
    ov::Canvas cv = { g_img, 640, 400 };
    bool caps = g_caps_fn ? g_caps_fn() : false, kana = g_kana_fn ? g_kana_fn() : false; g_lastcaps = caps; g_lastkana = kana;
    if (g_vis) {
        fill(PX, PY, PW, PH, 0x96000000u);
        for (int i = 0; i < kbt::g_nkeys; i++) {
            const kbt::Key & k = kbt::g_keys[i]; int x = key_x(i), y = key_y(i), w = key_w(i), h = key_h(i);
            bool on = (k.kind == kbt::K_MOD && g_latch[i]) || g_pressed[i] || (k.kind == kbt::K_TOG && ((k.kbd == (int)KBD_capslock && caps) || (k.kbd == (int)KBD_kana && kana)));
            uint32_t face = on ? 0xF0FFC000u : 0xD0283050u, edge = on ? 0xFFFFFFFFu : 0xE0A0A8C0u, fg = on ? 0xFF000000u : 0xFFFFFFFFu;
            fill(x, y, w, h, edge); fill(x + 1, y + 1, w - 2, h - 2, face);
            int tw = ov::tw(k.label); cv.clip(x + 1, y + 1, w - 2, h - 2); ov::text(cv, x + (w - tw) / 2, y + (h - 16) / 2, k.label, fg); cv.unclip();
        }
    }
    if (g_btn_on) {   /* 키보드 아이콘: 윗줄 6칸 · 가운데 6칸 · 아랫줄 스페이스 모양 */
        bool on = g_vis || g_btn_down;
        uint32_t face = on ? 0xB8FFC000u : 0x73202840u, edge = on ? 0xD8FFFFFFu : 0x73C0C8E0u, dot = on ? 0xFF000000u : 0xA0FFFFFFu;
        fill(BX, BY, BW, BH, edge); fill(BX + 1, BY + 1, BW - 2, BH - 2, face);
        for (int c = 0; c < 6; c++) { fill(BX + 5 + c * 5, BY + 6, 3, 4, dot); fill(BX + 5 + c * 5, BY + 12, 3, 4, dot); }
        fill(BX + 11, BY + 18, 18, 4, dot);
    }
    g_dirty = false;
}
static void blend_region(uint32_t * fr, int fw, int fh, int vx0, int vy0, int vw, int vh) {
    int fy0 = vy0 * fh / 400, fy1 = ((vy0 + vh) * fh + 399) / 400, fx0 = vx0 * fw / 640, fx1 = ((vx0 + vw) * fw + 639) / 640;
    if (fy1 > fh) fy1 = fh; if (fx1 > fw) fx1 = fw;
    for (int fy = fy0; fy < fy1; fy++) {
        int vy = fy * 400 / fh; const uint32_t * srow = g_img + (size_t)vy * 640; uint32_t * drow = fr + (size_t)fy * fw;
        for (int fx = fx0; fx < fx1; fx++) {
            uint32_t s = srow[fx * 640 / fw]; uint32_t a = s >> 24; if (!a) continue;
            uint32_t d = drow[fx], ia = 255 - a;
            uint32_t r = (((s >> 16) & 255) * a + ((d >> 16) & 255) * ia) / 255, g = (((s >> 8) & 255) * a + ((d >> 8) & 255) * ia) / 255, b = ((s & 255) * a + (d & 255) * ia) / 255;
            drow[fx] = (d & 0xFF000000u) | (r << 16) | (g << 8) | b;
        }
    }
}
/* 게시 프레임(g_pub)에 합성. 숨김 + 버튼 꺼짐이면 아무것도 하지 않는다. */
static void draw(uint32_t * fr, int fw, int fh) {
    if (!g_vis && !g_btn_on) return;
    if (fw <= 0 || fh <= 0) return;
    bool caps = g_caps_fn ? g_caps_fn() : false, kana = g_kana_fn ? g_kana_fn() : false;
    if (g_dirty || caps != g_lastcaps || kana != g_lastkana) rebuild();
    if (g_vis) blend_region(fr, fw, fh, PX, PY, PW, PH);
    if (g_btn_on) blend_region(fr, fw, fh, BX, BY, BW, BH);
}
}   /* namespace kbov */
