#pragma once
/* kh_vpad.h -- on-screen virtual pad drawn in the side bands of a 4:3 frame shown in a wider window.
 * Window space (drawable pixels). No SDL here: the host feeds pointers (finger or mouse) and gets fire(btn, down). */
#include <stdint.h>
namespace vpad {
enum { B_UP = 0, B_DOWN, B_LEFT, B_RIGHT, B_O, B_X, B_SEL, B_START, NB };
struct Rect { int x, y, w, h; };
struct Geo { bool on; Rect r[NB]; };
typedef void (*FireFn)(int btn, bool down);
static FireFn g_fire = 0;
static volatile int g_show = 0;        /* main thread -> render thread: draw the pad */
static volatile unsigned g_mask = 0;   /* pressed buttons, one bit each */
static int g_cli = -1;                 /* test option --vpad 0|1|2 */
static int g_mode = 2;                 /* settings padbtn: 0 off, 1 on, 2 auto */
static int g_cnt[NB];
struct Ptr { bool act; long long id; int btn; };
static Ptr g_p[8];

static inline bool in_r(const Rect & r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }
/* fx, fw = frame rectangle inside the window; the bands are the two strips left and right of it */
static void layout(int dw, int dh, int fx, int fw, Geo & g) {
    g.on = false;
    int lw = fx, rw = dw - (fx + fw), bw = lw < rw ? lw : rw;
    if (dh < 240 || bw < 96) return;
    int s = bw / 3; if (s > dh / 6) s = dh / 6; if (s < 32) return;
    int cy = dh * 6 / 10, cx = lw / 2, x0 = fx + fw;
    g.r[B_UP] = Rect{ cx - s / 2, cy - s * 3 / 2, s, s };
    g.r[B_DOWN] = Rect{ cx - s / 2, cy + s / 2, s, s };
    g.r[B_LEFT] = Rect{ cx - s * 3 / 2, cy - s / 2, s, s };
    g.r[B_RIGHT] = Rect{ cx + s / 2, cy - s / 2, s, s };
    g.r[B_O] = Rect{ x0 + rw * 7 / 10 - s / 2, cy - s * 55 / 100 - s / 2, s, s };
    g.r[B_X] = Rect{ x0 + rw * 3 / 10 - s / 2, cy + s * 55 / 100 - s / 2, s, s };
    int sw = rw * 42 / 100, sh = s / 2, sy = dh * 2 / 10;
    g.r[B_SEL] = Rect{ x0 + rw * 6 / 100, sy, sw, sh };
    g.r[B_START] = Rect{ x0 + rw * 52 / 100, sy, sw, sh };
    g.on = true;
}
static int hit(const Geo & g, int x, int y) { for (int b = 0; b < NB; b++) if (in_r(g.r[b], x, y)) return b; return -1; }
static void press(int b) { if (g_cnt[b]++ == 0) { g_mask = g_mask | (1u << b); if (g_fire) g_fire(b, true); } }
static void release(int b) { if (g_cnt[b] > 0 && --g_cnt[b] == 0) { g_mask = g_mask & ~(1u << b); if (g_fire) g_fire(b, false); } }
static bool owned(void) { for (int i = 0; i < 8; i++) if (g_p[i].act) return true; return false; }
static void release_all(void) {
    for (int i = 0; i < 8; i++) if (g_p[i].act) { if (g_p[i].btn >= 0) release(g_p[i].btn); g_p[i].act = false; g_p[i].btn = -1; }
}
/* phase 0 down, 1 up, 2 move. Sliding off a button releases it, sliding onto another presses it. true = consumed */
static bool pointer(const Geo & g, long long id, int phase, int x, int y) {
    int si = -1; for (int i = 0; i < 8; i++) if (g_p[i].act && g_p[i].id == id) si = i;
    if (phase == 0) {
        int b = hit(g, x, y); if (b < 0) return false;
        if (si >= 0) { if (g_p[si].btn >= 0) release(g_p[si].btn); g_p[si].act = false; }
        for (int i = 0; i < 8; i++) if (!g_p[i].act) { g_p[i] = Ptr{ true, id, b }; press(b); return true; }
        return false;
    }
    if (si < 0) return false;
    Ptr & p = g_p[si];
    if (phase == 2) {
        int nb = hit(g, x, y);
        if (nb != p.btn) { if (p.btn >= 0) release(p.btn); p.btn = nb; if (nb >= 0) press(nb); }
        return true;
    }
    if (p.btn >= 0) release(p.btn);
    p.act = false; p.btn = -1; return true;
}
}   /* namespace vpad */
