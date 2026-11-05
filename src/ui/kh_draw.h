/* kh_draw.h -- r012 UI/OSD 그리기 도구: 640x400 가상 화면(Canvas) · FONTX2 비트맵 글꼴 · 둥근 흰 1선 창 · 막대 · OSD
 * 호스트 gensei_host.cpp 가 include(단일 번역 단위). PC-98 디지털 8색 + 회색, 안티앨리어싱 없음. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <iconv.h>
#endif
#include "platform/plat.h" /* S3: platform layer */
#include "text/kh_phrase.h"   /* t52a (#49): windows.h 와 그 앞의 LEAN_AND_MEAN 정의 뒤에 둔다 */
#include "text/kh_font.h"   /* S7: font and glyph composition split out to text */

namespace ov {

/* ---- 색 (0x00RRGGBB) */
enum : uint32_t { K = 0x000000, WH = 0xFFFFFF, CY = 0x00FFFF, YL = 0xFFFF00, OR = 0xFFA000, GY = 0x666666, GR = 0x00FF00, RD = 0xFF0000, NOBG = 0xFF000000u };

struct ShimCfg { int mode = 0, amp = 4, spd = 3, glow = 6, col = 0, ms = 0; };
static ShimCfg g_shim;
static std::vector<uint8_t> g_shim_mask;   /* 1 = dot drawn by shimmered text this frame (glow source) */
/* build 7: shimmer. Brightness of one lit dot; mode 1 = same phase for the whole glyph (origin ox,oy), mode 2 = every dot on its own. */
static uint32_t shim_fg(uint32_t fg, int ox, int oy, int px, int py) {
    if (fg == K) return fg;
    static const uint32_t tint[4] = { 0x7CF2B0u, 0xFFB547u, 0xE8EEF0u, 0x5FD7FFu };
    const int ax = g_shim.mode == 2 ? px : ox, ay = g_shim.mode == 2 ? py : oy;
    uint32_t hh = ((uint32_t)ax * 73856093u) ^ ((uint32_t)ay * 19349663u);
    hh ^= hh >> 13; hh *= 0x5BD1E995u; hh ^= hh >> 15;
    const double lp = (hh & 0xFFFFu) / 65536.0 * 6.2831853, lf = 0.6 + ((hh >> 16) & 0xFFu) / 255.0 * 0.9;
    const double t = g_shim.ms / 1000.0 * (g_shim.spd * 0.3);
    const double q = 0.5 + 0.5 * sin(t * lf + lp), pw = q * q * q;
    const double amp = g_shim.amp * 0.055, kk = (1.0 - amp) + amp * 2.2 * pw;
    const uint32_t col = (fg == WH) ? tint[g_shim.col & 3] : fg;
    const double base = kk < 1.0 ? kk : 1.0, over = kk > 1.0 ? kk - 1.0 : 0.0;
    uint32_t out = 0;
    for (int sh = 0; sh <= 16; sh += 8) {
        const double v = ((col >> sh) & 0xFFu) * base + 255.0 * over;
        out |= (uint32_t)(v > 255.0 ? 255.0 : v) << sh;
    }
    return out;
}

struct Canvas {
    uint32_t * p; int w, h; bool shim = false;
    int clx0 = 0, cly0 = 0, clx1 = 1 << 30, cly1 = 1 << 30;   /* r018: 그리기 제한 영역 (긴 제목을 상자 안에서 자르기) */
    void clip(int x, int y, int cw, int ch) { clx0 = x; cly0 = y; clx1 = x + cw; cly1 = y + ch; }
    void unclip() { clx0 = 0; cly0 = 0; clx1 = 1 << 30; cly1 = 1 << 30; }
    void set(int x, int y, uint32_t c) { if ((unsigned)x < (unsigned)w && (unsigned)y < (unsigned)h && x >= clx0 && x < clx1 && y >= cly0 && y < cly1) p[(size_t)y * w + x] = c; }
    void rect(int x, int y, int rw, int rh, uint32_t c) {
        int x0 = x < clx0 ? clx0 : x, y0 = y < cly0 ? cly0 : y, x1 = x + rw > clx1 ? clx1 : x + rw, y1 = y + rh > cly1 ? cly1 : y + rh;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > w) x1 = w; if (y1 > h) y1 = h;
        for (int j = y0; j < y1; j++) for (int i = x0; i < x1; i++) p[(size_t)j * w + i] = c;
    }
};


/* one lit dot of shimmered text; remembered in the mask so that only text glows, not solid areas */
static void shim_put(Canvas & c, int px, int py, uint32_t fg, int ox, int oy) {
    if (fg == K) { c.set(px, py, fg); return; }
    c.set(px, py, shim_fg(fg, ox, oy, px, py));
    if ((unsigned)px >= (unsigned)c.w || (unsigned)py >= (unsigned)c.h) return;
    if (g_shim_mask.size() == (size_t)c.w * c.h) g_shim_mask[(size_t)py * c.w + px] = 1;
}
/* build 7: soft glow around bright dots (box blur of the bright part, added back) */
static void shim_glow(Canvas & c) {
    const int w = c.w, h = c.h, R = 2; const double ga = g_shim.glow * 0.1;
    if (ga <= 0.0 || w < 8 || h < 8) return;
    static std::vector<float> ch[3], tmp; static std::vector<char> rowhas;
    const size_t n = (size_t)w * h; bool any = false;
    for (int k = 0; k < 3; k++) ch[k].assign(n, 0.0f);
    tmp.assign(n, 0.0f); rowhas.assign((size_t)h, 0);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        const size_t i = (size_t)y * w + x; const uint32_t px = c.p[i];
        const int r = (px >> 16) & 255, g = (px >> 8) & 255, b = px & 255;
        const int lum = r > g ? (r > b ? r : b) : (g > b ? g : b);
        if (lum < 140 || g_shim_mask.size() != n || !g_shim_mask[i]) continue;
        const float a = (float)(ga * (lum - 140) / 115.0);
        ch[0][i] = r * a; ch[1][i] = g * a; ch[2][i] = b * a; rowhas[(size_t)y] = 1; any = true;
    }
    if (!any) return;
    for (int k = 0; k < 3; k++) {
        float * s = ch[k].data(); float * t = tmp.data();
        for (int y = 0; y < h; y++) {
            if (!rowhas[(size_t)y]) { for (int x = 0; x < w; x++) t[(size_t)y * w + x] = 0.0f; continue; }
            for (int x = 0; x < w; x++) {
                float acc = 0.0f;
                for (int d = -R; d <= R; d++) { const int xx = x + d; if (xx >= 0 && xx < w) acc += s[(size_t)y * w + xx]; }
                t[(size_t)y * w + x] = acc * (1.0f / (2 * R + 1));
            }
        }
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            float acc = 0.0f;
            for (int d = -R; d <= R; d++) { const int yy = y + d; if (yy >= 0 && yy < h) acc += t[(size_t)yy * w + x]; }
            s[(size_t)y * w + x] = acc * (1.0f / (2 * R + 1));
        }
    }
    for (size_t i = 0; i < n; i++) {
        const float a0 = ch[0][i], a1 = ch[1][i], a2 = ch[2][i];
        if (a0 + a1 + a2 < 1.0f) continue;
        const uint32_t px = c.p[i]; const float gain = 1.6f;
        const float r = (float)((px >> 16) & 255) + a0 * gain, g = (float)((px >> 8) & 255) + a1 * gain, b = (float)(px & 255) + a2 * gain;
        c.p[i] = ((uint32_t)(r > 255.0f ? 255.0f : r) << 16) | ((uint32_t)(g > 255.0f ? 255.0f : g) << 8) | (uint32_t)(b > 255.0f ? 255.0f : b);
    }
}

static void blit_glyph(Canvas & c, const uint8_t * g, int gw, int bpr, int x, int y, uint32_t fg, uint32_t bg) {
    for (int j = 0; j < 16; j++) for (int i = 0; i < gw; i++) {
        int on = (g[j * bpr + (i >> 3)] >> (7 - (i & 7))) & 1;
        if (on) { if (c.shim) shim_put(c, x + i, y + j, fg, x, y); else c.set(x + i, y + j, fg); } else if (bg != NOBG) c.set(x + i, y + j, bg);
    }
}
/* 오른쪽 삼각형(▶) 직접 그리기: 16x16 칸 */
static void tri_right(Canvas & c, int x, int y, uint32_t fg, uint32_t bg) {
    if (bg != NOBG) c.rect(x, y, 16, 16, bg);
    for (int j = 2; j < 14; j++) { int half = j < 8 ? j - 1 : 14 - j; int len = half * 2; if (len > 12) len = 12; c.rect(x + 2, y + j, len > 0 ? len : 1, 1, fg); }
}
/* 텍스트 그리기. 반각 8px, 전각 16px. 폭 반환. bg=NOBG 면 투명. */
static int text(Canvas & c, int x, int y, const char * s, uint32_t fg, uint32_t bg = NOBG) {
    int cx = x;
    for (;;) {
        uint32_t cp = u8next(s); if (!cp) break;
        if (cp < 0x80) { const uint8_t * g = g_sb.glyph(cp); if (g) blit_glyph(c, g, 8, 1, cx, y, fg, bg); else if (bg != NOBG) c.rect(cx, y, 8, 16, bg); cx += 8; continue; }
        if (cp == 0x25B6) { tri_right(c, cx, y, fg, bg); cx += 16; continue; }
        if (cp == 0xB7 || cp == 0x2026) {   /* r018: 글꼴에 없는 점 모양 글자(가운뎃점 · 말줄임표)는 직접 그린다 */
            if (bg != NOBG) c.rect(cx, y, 16, 16, bg);
            if (cp == 0xB7) c.rect(cx + 7, y + 7, 2, 2, fg); else { c.rect(cx + 2, y + 12, 2, 2, fg); c.rect(cx + 7, y + 12, 2, 2, fg); c.rect(cx + 12, y + 12, 2, 2, fg); }
            cx += 16; continue;
        }
        if (cp >= 0x3131 && cp <= 0x3163) {   /* t48c2 (#641): hangul compat jamo are drawn in code (font has syllables only) */
            const uint8_t * jg = jm::glyph(cp); if (jg) { blit_glyph(c, jg, 16, 2, cx, y, fg, bg); cx += 16; continue; }
        }
        { uint8_t jb[32]; const uint8_t * jg = jp_glyph(cp, jb); if (jg) { blit_glyph(c, jg, 16, 2, cx, y, fg, bg); cx += 16; continue; } }   /* t52h (#134): 일본어 글꼴 */
        unsigned code = cp2code(cp); const uint8_t * g = code ? g_db.glyph(code) : NULL;
        if (g) blit_glyph(c, g, 16, 2, cx, y, fg, bg); else if (bg != NOBG) c.rect(cx, y, 16, 16, bg);
        cx += 16;
    }
    return cx - x;
}
static int text(Canvas & c, int x, int y, const std::string & s, uint32_t fg, uint32_t bg = NOBG) { return text(c, x, y, s.c_str(), fg, bg); }

/* ---- 창: 1선 둥근 모서리, 창 제목은 테두리 위 가운데 */
static int text_bmp(Canvas & c, const BmpFont & f, int x, int y, const std::string & str, uint32_t fg, uint32_t bg = NOBG) {
    int cx = x; const char * s = str.c_str();
    for (;;) {
        uint32_t cp = u8next(s); if (!cp) break;
        if (cp < 0x80) { const uint8_t * g = g_sb.glyph(cp); if (g) blit_glyph(c, g, 8, 1, cx, y, fg, bg); cx += 8; continue; }
        int ku, ten; if (f.ok && bmp_cell(cp, ku, ten)) {
            for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) { if (f.ink(ku * 16 + i, (ten + 32) * 16 + j)) { if (c.shim) shim_put(c, cx + i, y + j, fg, cx, y); else c.set(cx + i, y + j, fg); } else if (bg != NOBG) c.set(cx + i, y + j, bg); }
        } else if (bg != NOBG) c.rect(cx, y, 16, 16, bg);
        cx += 16;
    }
    return cx - x;
}
static std::string fit(const std::string & s, int maxw);
static void rbox(Canvas & c, int x, int y, int w, int h, uint32_t col, const char * title) {
    c.rect(x + 3, y, w - 6, 1, col); c.rect(x + 3, y + h - 1, w - 6, 1, col); c.rect(x, y + 3, 1, h - 6, col); c.rect(x + w - 1, y + 3, 1, h - 6, col);
    c.set(x + 2, y + 1, col); c.set(x + 1, y + 2, col); c.set(x + w - 3, y + 1, col); c.set(x + w - 2, y + 2, col);
    c.set(x + 2, y + h - 2, col); c.set(x + 1, y + h - 3, col); c.set(x + w - 3, y + h - 2, col); c.set(x + w - 2, y + h - 3, col);
    if (title && *title) { std::string tt = title; if (tw(tt) + 8 > w - 16) tt = fit(tt, w - 16 - 8);   /* t39: 상자 제목이 상자 폭을 넘으면 안전망으로 줄인다 */
        int t = tw(tt) + 8; int tx = x + ((w - t) >> 1); c.rect(tx, y - 1, t, 3, K); text(c, tx + 4, y - 8, tt, WH, K); }
}
static void fillbox(Canvas & c, int x, int y, int w, int h, uint32_t bg, uint32_t col, const char * title) { c.rect(x, y, w, h, bg); rbox(c, x, y, w, h, col, title); }

/* ---- 막대: 왼쪽 라벨 · 트랙(눈금) · 마커 · 오른쪽 라벨. v = 0..1 */
static void barv(Canvas & c, int x, int y, int w, double v, const char * l, const char * r, uint32_t fg, bool sel) {
    int lw = tw(l) + 8, rw = tw(r) + 8, tx = x + lw, tl = w - lw - rw;
    text(c, x, y, l, fg, K);
    c.rect(tx, y + 7, tl, 1, fg);
    for (int i = 0; i <= 10; i++) c.rect(tx + (int)(i * (tl - 1) / 10.0 + 0.5), y + 4, 1, 7, fg);
    if (v < 0) v = 0; if (v > 1) v = 1;
    int mx = tx + (int)(v * (tl - 1) + 0.5);
    c.rect(mx - 2, y + 2, 5, 12, WH);
    text(c, tx + tl + 8, y, r, fg, K);
}
/* 선택지 나열: 현재값 반전. 반환 = 끝 x */
static int choices(Canvas & c, int x, int y, const char * const * arr, int n, int cur, uint32_t fg, bool rowsel) {
    int cx = x;
    for (int i = 0; i < n; i++) {
        int w = tw(arr[i]) + 8;
        if (i == cur) { c.rect(cx - 4, y - 2, w, 20, WH); text(c, cx, y, arr[i], K, WH); }
        else text(c, cx, y, arr[i], fg, K);
        cx += w + 8;
    }
    return cx;
}

/* r017: 못 고르는 선택지는 회색(dimmask 의 i 번째 비트가 1). cur 은 반전 */
static int choicesd(Canvas & c, int x, int y, const char * const * arr, int n, int cur, uint32_t fg, unsigned dimmask) {
    int cx = x;
    for (int i = 0; i < n; i++) {
        int w = tw(arr[i]) + 8; bool d = (dimmask >> i) & 1u;
        if (i == cur) { c.rect(cx - 4, y - 2, w, 20, d ? GY : WH); text(c, cx, y, arr[i], K, d ? GY : WH); }
        else text(c, cx, y, arr[i], d ? GY : fg, K);
        cx += w + 8;
    }
    return cx;
}

/* ---- OSD(화면 구석 1~2초) */
static void osd_draw(Canvas & c, const std::string & s, bool right, int y = 8) {
    if (s.empty() || !g_font_ok) return;
    int w = tw(s) + 16, x = right ? c.w - w - 8 : 8;
    c.rect(x, y, w, 24, K); c.rect(x, y, w, 1, WH); c.rect(x, y + 23, w, 1, WH); c.rect(x, y, 1, 24, WH); c.rect(x + w - 1, y, 1, 24, WH);
    text(c, x + 8, y + 4, s, WH, K);
}

/* ---- 아래 칸(B안 방식): 청록 칸 5+5 */
static void fncells(Canvas & c, const char * const * it, int n) {
    const int CW = 58, GAP = 4, MID = 16, X0 = 6, Y = c.h - 17;
    int extra = 0;   /* t52a (#49): 라벨이 칸보다 넓으면 그 칸만 넓히고 같은 묶음(5+5)의 뒤 칸을 민다 */
    for (int i = 0; i < n && i < 10; i++) { if (i == 5) extra = 0; if (!it[i] || !*it[i]) continue; int w = tw(it[i]); int cw = w + 8 > CW ? w + 8 : CW; int x = X0 + i * (CW + GAP) + (i >= 5 ? MID - GAP : 0) + extra; c.rect(x, Y, cw, 16, CY); text(c, x + ((cw - w) >> 1), Y, it[i], K, CY); extra += cw - CW; }
}

/* ---- 맨 아랫줄(A안): 노란 글자 한 줄. t49a (#812): 마지막 입력 장치(패드/키보드)에 맞춰 기호↔키 이름을 이 한 곳에서 번역하고, 구분자를 줄이며, 폭을 넘으면 두 줄로 나눈다 */
static int g_helpdev = 0;                       /* 0 = 패드 기호, 1 = 키보드 키 이름 (마지막 입력 장치; 마우스는 바꾸지 않음) */
static bool g_help_osk = false;                 /* 한글 화면 키보드 화면(글자 키가 입력이라 별도 문구) */
struct HelpKey { std::string sym, name; };
static std::vector<HelpKey> g_helpkeys;         /* 호스트가 실제 키 매핑 표(gh_keytab)에서 채운다 — 표 한 곳 */
typedef void (*HelpLogFn)(const std::string & text, int padw, int kbdw, int cellw, bool two);
static HelpLogFn g_help_logfn = NULL;
static std::string help_rep(std::string s, const std::string & from, const std::string & to) { size_t p = 0; while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); } return s; }
#if defined(__ANDROID__)
#define KH_SCR_ROWS 9
#define KH_SNDLABEL phr_("소리 버퍼 (다음 시작부터)")
static int g_lblcfg = 0;   /* t137: 패드 라벨 방식 0 자동 1 retro 2 xbox */
static int g_sndbuf = 1;   /* t137: 소리 버퍼 단계 0..3 */
static void (*g_kha_save)(void) = 0;
static std::string kh_lbl(std::string s) { int m = g_lblcfg ? g_lblcfg : (gensei_padkind_v == 1 ? 2 : 1); const char * Lb[4] = { m == 1 ? "A" : "B", m == 1 ? "B" : "A", m == 1 ? "Y" : "X", m == 1 ? "X" : "Y" }; s = help_rep(s, "○", Lb[0]); s = help_rep(s, "×", Lb[1]); s = help_rep(s, "□", Lb[2]); s = help_rep(s, "△", Lb[3]); return s; }
#else
#define KH_SCR_ROWS 11
#define KH_SNDLABEL phr_("소리 출력 장치")
static std::string kh_lbl(std::string s) { return s; }
#endif
static std::string help_common(std::string s) {   /* 장치와 무관한 잘림 수정: 구분 「, 」 → 공백 두 칸, 끝의 외로운 「×」 에 「:나감」 */
    s = help_rep(s, ", ", "  ");
    const std::string x = "×"; if (s.size() >= x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0) s += phr_(":나감");
    return s;
}
static std::string help_name(const char * sym) { for (auto & k : g_helpkeys) if (k.sym == sym) return k.name; return sym; }
static std::string help_kbd(std::string s) {
    s = help_rep(s, "○ 또는 ×", help_name("○") + " 또는 " + help_name("×"));
    for (auto & k : g_helpkeys) s = help_rep(s, k.sym + ":", k.name + ":");
    return s;
}
/* t50a (#873 안 C): 알림 문구의 장치 표식 {SEL} {○} {□} {△} {×} 를 마지막 입력 장치(패드/키보드)에 맞춰 바꾼다 — 도움말과 같은 표(g_helpkeys) 한 곳 */
static std::string dkey(std::string s) {
    { const std::string sel = help_name("SEL"); s = help_rep(s, "{SEL}{로}", g_helpdev == 1 ? sel + (sel == "Tab" ? "으로" : "로") : std::string("SEL로")); }   /* t52a (#49): 「Tab」은 받침 ㅂ 이라 「으로」 */
    static const char * sy[] = { "SEL", "○", "□", "△", "×" };
    for (const char * y : sy) { std::string tag = std::string("{") + y + "}"; s = help_rep(s, tag, g_helpdev == 1 ? help_name(y) : kh_lbl(std::string(y))); }
    return s;
}
/* t52a (#49): 부팅 화면 아래 F키 줄 — 마지막 입력 장치에 맞춰 기호↔키 이름. 라벨은 문구 파일에서 바꿀 수 있다 */
static void fncells_boot(Canvas & c) {
    std::string a = dkey(phr_("{○}결정")), b = dkey(phr_("{×}취소")), d = dkey(phr_("{△}초기")), u = phr_("↑↓");
    const char * C[10] = { a.c_str(), b.c_str(), d.c_str(), "", "", "", "", "", "", u.c_str() }; fncells(c, C, 10);
}
/* t50a (#873 안 A): 이 도움말이 지금 한 줄인지 두 줄인지(helpline 과 같은 판정, g_help_osk 는 소비하지 않음) */
static int help_rows(const Canvas & c, const std::string & s0) {
    std::string sp = help_common(s0), sk = help_kbd(sp);
    if (g_help_osk && g_helpdev == 1) sk = phr_("키보드로 입력  Enter:완료  Esc:취소  Backspace:지움  한/영 키:한글/영문");
    return tw(g_helpdev == 1 ? sk : kh_lbl(sp)) > c.w - 16 ? 2 : 1;
}
static void helpline(Canvas & c, const std::string & s0) {
    const int maxw = c.w - 16; const bool osk = g_help_osk; g_help_osk = false;
    std::string sp = help_common(s0), sk = help_kbd(sp);
    if (osk && g_helpdev == 1) sk = phr_("키보드로 입력  Enter:완료  Esc:취소  Backspace:지움  한/영 키:한글/영문");
    std::string s = g_helpdev == 1 ? sk : kh_lbl(sp);
    bool two = tw(s) > maxw;
    if (g_help_logfn) g_help_logfn(s0, tw(sp), tw(sk), maxw, tw(sp) > maxw || tw(sk) > maxw);
    if (!two) { c.rect(0, c.h - 16, c.w, 16, K); text(c, 8, c.h - 16, s, YL, K); return; }
    const int maxw2 = c.w - 32;   /* t50a (안 D): 두 줄일 때는 틀(x=16) 안쪽에 맞춘다 */
    std::string l1 = s, l2; size_t cut = std::string::npos, p = 0;   /* 구분 공백 두 칸에서 나눔: 윗줄이 들어가는 마지막 위치 */
    while ((p = s.find("  ", p)) != std::string::npos) { if (tw(s.substr(0, p)) <= maxw2) cut = p; else break; p += 2; }
    if (cut != std::string::npos) { l1 = s.substr(0, cut); l2 = s.substr(cut + 2); }
    c.rect(0, c.h - 32, c.w, 32, K); text(c, 16, c.h - 32, fit(l1, maxw2), YL, K); if (!l2.empty()) text(c, 16, c.h - 16, fit(l2, maxw2), YL, K);
}
/* 뒤 화면 어둡게(확인 창 아래) */
static void dim(Canvas & c) { for (int i = 0; i < c.w * c.h; i++) c.p[i] = (c.p[i] >> 1) & 0x7F7F7F; }
static int choices_w(const char * const * arr, int n) { int w = 0; for (int i = 0; i < n; i++) w += tw(arr[i]) + 8 + (i ? 8 : 0); return w; }
/* 폭을 넘으면 … 로 자른다 */
static std::string fit(const std::string & s, int maxw) { if (tw(s) <= maxw) return s; std::string o; const char * p = s.c_str(); for (;;) { const char * q = p; uint32_t cp = u8next(q); if (!cp) break; std::string ch(p, q - p); if (tw(o + ch + "\xE2\x80\xA6") > maxw) break; o += ch; p = q; } return o + "\xE2\x80\xA6"; }

/* ---- t39: 제목 표시 도우미 (지시 #288) */
/* 끝의 [..] 또는 (..) 묶음(꼬리표)을 보존하며 줄인다: 「대마도전략물어… [원판 일본어]」. 꼬리표가 없거나 머리가 3자(48px) 미만이면 끝 … */
static std::string fit_title(const std::string & s, int maxw) {
    if (tw(s) <= maxw) return s;
    size_t n = s.size(); while (n && s[n - 1] == ' ') n--;
    if (n && (s[n - 1] == ']' || s[n - 1] == ')')) {
        char open = s[n - 1] == ']' ? '[' : '('; size_t p = s.rfind(open, n - 1);
        if (p != std::string::npos && p > 0) {
            std::string tag = s.substr(p, n - p), head = s.substr(0, p); while (!head.empty() && head.back() == ' ') head.pop_back();
            int room = maxw - tw(tag) - 8; if (room >= 48 && !head.empty()) return fit(head, room) + " " + tag;
        }
    }
    return fit(s, maxw);
}
/* 자동 단축: 끝의 ( … )·[ … ] 묶음을 모두 떼고, 끝의 「한글판」「한글패치」를 뗀다 */
static std::string auto_short(const std::string & t) {
    std::string s = t;
    for (int k = 0; k < 4; k++) {
        size_t n = s.size(); while (n && s[n - 1] == ' ') n--; s.resize(n);
        if (!n || (s[n - 1] != ']' && s[n - 1] != ')')) break;
        char open = s[n - 1] == ']' ? '[' : '('; size_t p = s.rfind(open, n - 1); if (p == std::string::npos || p == 0) break; s.resize(p);
    }
    for (int k = 0; k < 2; k++) {
        size_t n = s.size(); while (n && s[n - 1] == ' ') n--; s.resize(n);
        static const char * sf[] = { phr_("\xED\x95\x9C\xEA\xB8\x80\xED\x8C\x90"), phr_("\xED\x95\x9C\xEA\xB8\x80\xED\x8C\xA8\xEC\xB9\x98") };   /* 한글판 · 한글패치 */
        for (const char * x : sf) { size_t L = strlen(x); if (s.size() > L && s.compare(s.size() - L, L, x) == 0) { s.resize(s.size() - L); break; } }
    }
    size_t n = s.size(); while (n && s[n - 1] == ' ') n--; s.resize(n);
    return s.empty() ? t : s;
}
/* 새로 만드는 파일 이름용: 금지 문자·제어 문자 → _, 끝의 . 과 공백 제거, 예약 이름 뒤에 _, 최대 maxch 글자 */
static std::string safe_name(const std::string & in, int maxch = 48) {
    std::string o; int cnt = 0; const char * p = in.c_str();
    for (;;) {
        const char * q = p; uint32_t cp = u8next(q); if (!cp) break; std::string ch(p, q - p); p = q;
        if (cp < 0x20 || cp == '\\' || cp == '/' || cp == ':' || cp == '*' || cp == '?' || cp == '"' || cp == '<' || cp == '>' || cp == '|') ch = "_";
        if (cnt >= maxch) break; o += ch; cnt++;
    }
    while (!o.empty() && (o.back() == '.' || o.back() == ' ')) o.pop_back();
    while (!o.empty() && o[0] == ' ') o.erase(0, 1);
    if (o.empty()) o = "_";
    std::string u = o; for (auto & ch : u) ch = (char)toupper((unsigned char)ch);
    static const char * rs[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    size_t dot = u.find('.'); std::string stem = dot == std::string::npos ? u : u.substr(0, dot);
    for (const char * r : rs) if (stem == r) { o += "_"; break; }
    return o;
}

/* ---- 간이 축소 복사(최근접). src 는 0x00RRGGBB 배열 */
static void blit_scaled(Canvas & c, const uint32_t * src, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (!src || sw <= 0 || sh <= 0) return;
    for (int y = 0; y < dh; y++) { int sy = (int)((long long)y * sh / dh); for (int x = 0; x < dw; x++) { int sx = (int)((long long)x * sw / dw); c.set(dx + x, dy + y, src[(size_t)sy * sw + sx] & 0xFFFFFF); } }
}
/* 표본 그림(저장 캡처가 없을 때): 코드로 그린 PC-98풍 화면 한 장 */
static void sample_picture(Canvas & c, int dx, int dy, int dw, int dh) {
    c.rect(dx, dy, dw, dh, 0x000077);
    for (int y = 0; y < dh; y += 8) c.rect(dx, dy + y, dw, 1, 0x0000FF);
    c.rect(dx + dw / 8, dy + dh / 5, dw * 3 / 4, dh * 3 / 5, 0x005500);
    c.rect(dx + dw / 8 + 4, dy + dh / 5 + 4, dw * 3 / 4 - 8, dh * 3 / 5 - 8, K);
    text(c, dx + dw / 2 - 28, dy + dh / 2 - 8, "SAMPLE", WH, K);
}

} /* namespace ov */
