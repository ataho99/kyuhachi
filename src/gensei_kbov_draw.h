#pragma once
/* kbov drawing half (uses ov::Canvas/ov::text, so it cannot live in input). Split from input/gensei_kbov.h in S6, code unchanged. Moves to ui/ in S7. */
namespace kbov {

/* ---- 그리기 (가상 캔버스에 ARGB 로 그린 뒤 프레임에 알파 합성) */
static void fill(int x, int y, int w, int h, uint32_t c) { for (int j = y < 0 ? 0 : y; j < y + h && j < 400; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < 640; i++) g_img[j * 640 + i] = c; }
/* PC-98 느낌의 단색 비트맵 아이콘(16x16 칸 안에 칸 단위 사각형으로). ix,iy = 아이콘 원점 */
static void icon(int s, int x, uint32_t ink) {
    int ix = x + 12, iy = BY + 6;
    if (s == OV_KBD) {   /* 윗줄 6칸 · 가운데 6칸 · 아랫줄 스페이스 모양 */
        for (int c = 0; c < 6; c++) { fill(x + 5 + c * 5, BY + 6, 3, 4, ink); fill(x + 5 + c * 5, BY + 12, 3, 4, ink); }
        fill(x + 11, BY + 18, 18, 4, ink);
    } else if (s == OV_MENU) {   /* 세 줄 */
        fill(ix + 1, iy + 2, 14, 3, ink); fill(ix + 1, iy + 7, 14, 3, ink); fill(ix + 1, iy + 12, 14, 3, ink);
    } else if (s == OV_SAVE) {   /* 아래 화살표 + 받침 */
        fill(ix + 6, iy, 4, 7, ink); for (int r = 0; r < 5; r++) fill(ix + 2 + r, iy + 7 + r, 12 - 2 * r, 1, ink);
        fill(ix + 1, iy + 13, 14, 2, ink); fill(ix + 1, iy + 10, 2, 5, ink); fill(ix + 13, iy + 10, 2, 5, ink);
    } else if (s == OV_LOAD) {   /* 위 화살표 + 받침 */
        for (int r = 0; r < 5; r++) fill(ix + 6 - r, iy + r, 4 + 2 * r, 1, ink); fill(ix + 6, iy + 5, 4, 7, ink);
        fill(ix + 1, iy + 13, 14, 2, ink); fill(ix + 1, iy + 10, 2, 5, ink); fill(ix + 13, iy + 10, 2, 5, ink);
    } else if (s == OV_BOOST) {   /* ▶▶ */
        for (int t = 0; t < 2; t++) for (int j = 0; j < 12; j++) { int d = j < 6 ? j : 11 - j; fill(ix + 1 + 7 * t, iy + 2 + j, d * 6 / 5 + 1, 1, ink); }
    }
}
static bool slot_lit(int s) { return s == OV_KBD ? g_vis : s == OV_BOOST ? (g_boost_fn ? g_boost_fn() : false) : false; }
static void rebuild(void) {
    memset(g_img, 0, sizeof g_img);
    ov::Canvas cv = { g_img, 640, 400 };
    bool caps = g_caps_fn ? g_caps_fn() : false, kana = g_kana_fn ? g_kana_fn() : false; g_lastcaps = caps; g_lastkana = kana; g_lastboost = g_boost_fn ? g_boost_fn() : false;
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
    for (int s = 0; s < NSLOT; s++) {   /* 버튼 줄: 켜짐(키보드 보임·배속 중·누르는 중)은 밝게 반전 */
        if (!slot_shown(s)) continue;
        int x = sx(s); bool on = slot_lit(s) || g_down_slot == s;
        uint32_t face = on ? 0xFFFFC000u : 0xF0202840u, edge = on ? 0xFFFFFFFFu : 0xF0C0C8E0u, ink = on ? 0xFF000000u : 0xFFFFFFFFu;
        fill(x, BY, BW, BH, edge); fill(x + 1, BY + 1, BW - 2, BH - 2, face);
        icon(s, x, ink);
        if (s == OV_LOAD && g_lp_on) { int w = (int)((now_() - g_lp_t0) * (unsigned)(BW - 4) / LP_MS); if (w > BW - 4) w = BW - 4; fill(x + 2, BY + BH - 5, BW - 4, 3, 0xFF404040u); fill(x + 2, BY + BH - 5, w, 3, 0xFF00E0FFu); }   /* 길게 누르는 진행 막대 */
    }
    g_dirty = false;
}
static void blend_region(uint32_t * fr, int fw, int fh, int vx0, int vy0, int vw, int vh, int mul = 255) {
    int fy0 = vy0 * fh / 400, fy1 = ((vy0 + vh) * fh + 399) / 400, fx0 = vx0 * fw / 640, fx1 = ((vx0 + vw) * fw + 639) / 640;
    if (fy1 > fh) fy1 = fh; if (fx1 > fw) fx1 = fw;
    for (int fy = fy0; fy < fy1; fy++) {
        int vy = fy * 400 / fh; const uint32_t * srow = g_img + (size_t)vy * 640; uint32_t * drow = fr + (size_t)fy * fw;
        for (int fx = fx0; fx < fx1; fx++) {
            uint32_t s = srow[fx * 640 / fw]; uint32_t a = (s >> 24) * (uint32_t)mul / 255u; if (!a) continue;
            uint32_t d = drow[fx], ia = 255 - a;
            uint32_t r = (((s >> 16) & 255) * a + ((d >> 16) & 255) * ia) / 255, g = (((s >> 8) & 255) * a + ((d >> 8) & 255) * ia) / 255, b = ((s & 255) * a + (d & 255) * ia) / 255;
            drow[fx] = (d & 0xFF000000u) | (r << 16) | (g << 8) | b;
        }
    }
}
/* 버튼 줄의 현재 진하기: 움직임·터치 뒤 WAKE_MS 동안 진하게, 이어서 FADE_MS 동안 옅어져 IDLE_A 로 */
static int cur_mul(void) {
    if (g_force_strong || g_vis || g_down_slot >= 0 || g_lp_on) return 255;
    unsigned dt = now_() - g_wake_t;
    if (dt < WAKE_MS) return 255;
    if (dt < WAKE_MS + FADE_MS) return 255 - (255 - IDLE_A) * (int)(dt - WAKE_MS) / (int)FADE_MS;
    return IDLE_A;
}
/* 게시 프레임(g_pub)에 합성. 숨김 + 버튼 줄 꺼짐이면 아무것도 하지 않는다. */
static void draw(uint32_t * fr, int fw, int fh) {
    if (!g_vis && !g_ov_on) return;
    if (fw <= 0 || fh <= 0) return;
    bool caps = g_caps_fn ? g_caps_fn() : false, kana = g_kana_fn ? g_kana_fn() : false, bo = g_boost_fn ? g_boost_fn() : false;
    if (g_dirty || g_lp_on || caps != g_lastcaps || kana != g_lastkana || bo != g_lastboost) rebuild();
    if (g_vis) blend_region(fr, fw, fh, PX, PY, PW, PH);
    if (g_ov_on) { int m = cur_mul(); for (int s = 0; s < NSLOT; s++) if (slot_shown(s)) blend_region(fr, fw, fh, sx(s), BY, BW, BH, slot_lit(s) && m < 200 ? 200 : m); }
}
}   /* namespace kbov */
