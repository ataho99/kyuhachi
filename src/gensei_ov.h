/* gensei_ov.h -- r012 UI/OSD 그리기 도구: 640x400 가상 화면(Canvas) · FONTX2 비트맵 글꼴 · 둥근 흰 1선 창 · 막대 · OSD
 * 호스트 gensei_host.cpp 가 include(단일 번역 단위). PC-98 디지털 8색 + 회색, 안티앨리어싱 없음. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <iconv.h>
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "gensei_phrase.h"   /* t52a (#49): windows.h 와 그 앞의 LEAN_AND_MEAN 정의 뒤에 둔다 */

namespace ov {

/* ---- 색 (0x00RRGGBB) */
enum : uint32_t { K = 0x000000, WH = 0xFFFFFF, CY = 0x00FFFF, YL = 0xFFFF00, OR = 0xFFA000, GY = 0x666666, GR = 0x00FF00, RD = 0xFF0000, NOBG = 0xFF000000u };

struct Canvas {
    uint32_t * p; int w, h;
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

/* ---- FONTX2 */
struct FontX {
    std::vector<uint8_t> d; int w = 0, h = 0, type = 0, bpr = 0, gsz = 0; size_t base = 0; bool ok = false;
    struct Blk { unsigned s, e; int idx; };
    std::vector<Blk> blk;
    bool load(const std::string & path) {
        ok = false; FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
        d.clear(); uint8_t b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) d.insert(d.end(), b, b + n);
        fclose(f);
        return parse();
    }
    bool load_mem(const unsigned char * p, size_t n) { ok = false; d.assign(p, p + n); return parse(); }
    bool parse() {
        ok = false;
        if (d.size() < 18 || memcmp(&d[0], "FONTX2", 6) != 0) return false;
        w = d[14]; h = d[15]; type = d[16]; bpr = (w + 7) >> 3; gsz = bpr * h; blk.clear();
        if (type == 0) base = 17;
        else {
            int nb = d[17]; size_t off = 18; int idx = 0;
            if (d.size() < off + (size_t)nb * 4) return false;
            for (int i = 0; i < nb; i++) { unsigned s = d[off] | (d[off + 1] << 8), e = d[off + 2] | (d[off + 3] << 8); off += 4; blk.push_back({ s, e, idx }); idx += (int)(e - s + 1); }
            base = off;
        }
        ok = true; return true;
    }
    const uint8_t * glyph(unsigned code) const {
        if (!ok) return NULL;
        if (type == 0) { if (code < 256 && base + (size_t)(code + 1) * gsz <= d.size()) return &d[base + (size_t)code * gsz]; return NULL; }
        for (size_t i = 0; i < blk.size(); i++) if (code >= blk[i].s && code <= blk[i].e) {
            size_t o = base + (size_t)(blk[i].idx + (int)(code - blk[i].s)) * gsz;
            if (o + gsz <= d.size()) return &d[o];
        }
        return NULL;
    }
};
static FontX g_sb, g_db;
/* t52h (#134): 일본어 글꼴. 한자(U+4E00~9FFF)는 어떤 UI 언어에서도, 가나·일본어 문장부호·전각 ASCII 는 UI 언어가 일본어일 때만 그린다.
 * 순서: ①res\fonts 의 일본어 글꼴 BMP(g_jp_bmpd, anex86 · 16x16 전각) ②동봉 shinonome16_jp.fnt(FONTX2, g_jp_fx) ③기존 메뉴 글꼴(네오둥근모 — 한자 없음, 가나는 있음).
 * 반각(ASCII)은 지금처럼 메뉴 글꼴의 8x16 을 쓴다. */
static bool g_jp_on = false;
static const std::vector<uint8_t> * g_jp_bmpd = NULL;
static FontX g_jp_fx; static std::string g_jp_src;
static bool jp_fx_load(const std::string & dir) {
    g_jp_src.clear(); const std::string c[2] = { dir + "/res/fonts/shinonome16_jp.fnt", dir + "/shinonome16_jp.fnt" };
    for (int q = 0; q < 2; q++) if (g_jp_fx.load(c[q])) { if (g_jp_fx.w == 16 && g_jp_fx.h == 16) { g_jp_src = c[q]; return true; } g_jp_fx.ok = false; }
    g_jp_fx.ok = false; return false;
}
static bool jp_cp_ok(uint32_t cp) {
    if (cp >= 0x4E00 && cp <= 0x9FFF) return true;
    if (!g_jp_on) return false;
    return (cp >= 0x3000 && cp <= 0x30FF) || (cp >= 0xFF01 && cp <= 0xFF5E);
}
/* 글자 → 16x16(32 B) 글리프. 없거나 빈 칸이면 NULL(→ 기존 경로). tmp = 호출한 쪽의 32 B 임시 칸 */
static const uint8_t * jp_glyph(uint32_t cp, uint8_t * tmp) {
    if (!g_jp_bmpd && !g_jp_fx.ok) return NULL;
    if (!jp_cp_ok(cp)) return NULL;
    wchar_t w[2] = { (wchar_t)cp, 0 }; char mb[4] = { 0, 0, 0, 0 }; BOOL used = FALSE;
    int n = WideCharToMultiByte(932, 0, w, 1, mb, 4, NULL, &used);
    if (n != 2 || used) return NULL;
    int s1 = (uint8_t)mb[0], s2 = (uint8_t)mb[1];
    if (!((s1 >= 0x81 && s1 <= 0x9F) || (s1 >= 0xE0 && s1 <= 0xEA))) return NULL;
    const uint8_t * r = NULL;
    if (g_jp_bmpd) {
        int a = s1 < 0xA0 ? s1 - 0x81 : s1 - 0xC1, j1 = 0x21 + 2 * a, j2;
        if (s2 >= 0x9F) { j1++; j2 = s2 - 0x7E; } else { int t = s2 >= 0x80 ? s2 - 1 : s2; j2 = t - 0x1F; }
        int ku = j1 - 0x20, ten = j2 - 0x20;
        if (ku >= 1 && ku <= 94 && ten >= 1 && ten <= 94) {
            const std::vector<uint8_t> & d = *g_jp_bmpd; memset(tmp, 0, 32); int ink = 0;
            for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) {
                int x = ku * 16 + i, y = (ten + 32) * 16 + j, ry = 2047 - y;
                if (!((d[62 + (size_t)ry * 256 + (x >> 3)] >> (7 - (x & 7))) & 1)) { tmp[j * 2 + (i >> 3)] |= (uint8_t)(0x80 >> (i & 7)); ink++; }
            }
            if (ink) r = tmp;
        }
    }
    if (!r && g_jp_fx.ok) {
        const uint8_t * g = g_jp_fx.glyph((unsigned)((s1 << 8) | s2));
        if (g) { int any = 0; for (int k = 0; k < 32; k++) any |= g[k]; if (any) r = g; }
    }
    return r;
}
static bool g_font_ok = false;
static std::string g_font_src;   /* t52g: font_load 가 읽은 .fnt 경로 */
static bool g_font_builtin = false;   /* r019: 외부 글꼴 파일이 없어 내장 글꼴을 쓰는 중 */
static bool font_load(const std::string & dir) {
    jp_fx_load(dir);   /* t52h (#134): 동봉 일본어 글꼴(menu 글꼴이 내장으로 대체돼도 따로 읽는다) */
    /* r020 (#248): res\fonts 우선, 옛 위치(exe 옆)도 읽는다 */
    const std::string rf = dir + "/res/fonts";
    /* t52g (#133): 새 이름(kyuhachi_font*.fnt)을 먼저, 옛 이름(pc98player_font*.fnt)도 계속 읽는다. 읽은 경로는 g_font_src 에 남긴다(로그·시험용) */
    const std::string sbn[5] = { rf + "/kyuhachi_font_sbcs.fnt", dir + "/kyuhachi_font_sbcs.fnt", rf + "/pc98player_font_sbcs.fnt", dir + "/pc98player_font_sbcs.fnt", dir + "/gensei16_neodgm_sbcs3.fnt" };
    const std::string dbn[5] = { rf + "/kyuhachi_font.fnt", dir + "/kyuhachi_font.fnt", rf + "/pc98player_font.fnt", dir + "/pc98player_font.fnt", dir + "/gensei16_neodgm.fnt" };
    bool a = false, b = false; g_font_src.clear();
    for (int q = 0; q < 5 && !a; q++) if (g_sb.load(sbn[q])) { a = true; g_font_src += sbn[q]; }
    for (int q = 0; q < 5 && !b; q++) if (g_db.load(dbn[q])) { b = true; g_font_src += " | " + dbn[q]; }
    g_font_ok = a && b; return g_font_ok;
}
static bool font_load_mem(const unsigned char * sb, size_t sbn, const unsigned char * db, size_t dbn) { bool a = g_sb.load_mem(sb, sbn), b = g_db.load_mem(db, dbn); g_font_ok = a && b; g_font_builtin = g_font_ok; return g_font_ok; }

/* ---- 문자 → 글꼴 코드. 환세 한글 배치 = KS X 1001 2,350자를 JIS 16~40구에 얹은 것(가=889F). */
static unsigned jis2sjis(int j1, int j2) {
    int s1 = ((j1 - 0x21) >> 1) + 0x81; if (s1 > 0x9F) s1 += 0x40;
    int s2; if (j1 & 1) { s2 = j2 + 0x1F; if (s2 >= 0x7F) s2++; } else s2 = j2 + 0x7E;
    return (unsigned)((s1 << 8) | s2);
}
struct Sym { uint32_t cp; unsigned code; };
static const Sym g_syms[] = {
    { 0x2500, 0x849F }, { 0x2502, 0x84A0 }, { 0x250C, 0x84A1 }, { 0x2510, 0x84A2 }, { 0x2518, 0x84A3 }, { 0x2514, 0x84A4 }, { 0x251C, 0x84A5 }, { 0x252C, 0x84A6 }, { 0x2524, 0x84A7 }, { 0x2534, 0x84A8 }, { 0x253C, 0x84A9 },
    { 0x25CB, 0x819B }, { 0x25CF, 0x819C }, { 0x25CE, 0x819D }, { 0x25C7, 0x819E }, { 0x25C6, 0x819F }, { 0x25A1, 0x81A0 }, { 0x25A0, 0x81A1 }, { 0x25B3, 0x81A2 }, { 0x25B2, 0x81A3 }, { 0x25BD, 0x81A4 }, { 0x25BC, 0x81A5 },
    { 0x3001, 0x8141 }, { 0x3002, 0x8142 }, { 0x30FB, 0x8145 }, { 0x30FC, 0x815B }, { 0x300C, 0x8175 }, { 0x300D, 0x8176 },   /* t52g: 、。・ー「」 */
    { 0x203B, 0x81A6 }, { 0x2192, 0x81A8 }, { 0x2190, 0x81A9 }, { 0x2191, 0x81AA }, { 0x2193, 0x81AB }, { 0x00D7, 0x817E }, { 0x3000, 0x8140 }, { 0x2605, 0x819A }, { 0x2606, 0x8199 }, { 0x266A, 0x81F4 },
};
static unsigned cp2code(uint32_t cp) {
    for (size_t i = 0; i < sizeof(g_syms) / sizeof(g_syms[0]); i++) if (g_syms[i].cp == cp) return g_syms[i].code;
    /* t52g (#133): 가나 — 메뉴 글꼴(FONTX2)의 SJIS 가나 칸(히라가나 0x829F~0x82F1 · 가타카나 0x8340~0x8396)을 쓴다. 한자는 글꼴에 없다 */
    if (cp >= 0x3041 && cp <= 0x3093) return 0x829Fu + (cp - 0x3041);
    if (cp >= 0x30A1 && cp <= 0x30F6) { uint32_t ki = cp - 0x30A1; return ki < 63 ? 0x8340u + ki : 0x8380u + (ki - 63); }
#if defined(_WIN32)
    if (cp < 0x10000) {
        wchar_t wc = (wchar_t)cp; char mb[4]; BOOL bad = FALSE;
        int n = WideCharToMultiByte(949, 0, &wc, 1, mb, 4, NULL, &bad);
        if (n == 2 && !bad) { unsigned a = (uint8_t)mb[0], b = (uint8_t)mb[1]; if (a >= 0xB0 && a <= 0xC8 && b >= 0xA1 && b <= 0xFE) return jis2sjis((int)(a - 0xA0) + 0x20, (int)(b - 0xA0) + 0x20); }
    }
#else
    if (cp >= 0xAC00 && cp <= 0xD7A3) {
        static std::vector<uint32_t> tab; if (tab.empty()) { tab.assign(0xB000, 0); }
        iconv_t ic = iconv_open("EUC-KR", "UTF-32LE"); if (ic != (iconv_t)-1) { char in[4] = { (char)(cp & 255), (char)((cp >> 8) & 255), (char)((cp >> 16) & 255), 0 }; char out[8]; char * ip = in, * op = out; size_t il = 4, ol = 8; size_t r = iconv(ic, &ip, &il, &op, &ol); iconv_close(ic); if (r != (size_t)-1 && op - out == 2) { unsigned a = (uint8_t)out[0], b = (uint8_t)out[1]; if (a >= 0xB0 && a <= 0xC8 && b >= 0xA1) return jis2sjis((int)(a - 0xA0) + 0x20, (int)(b - 0xA0) + 0x20); } }
    }
#endif
    return 0;
}
/* UTF-8 한 글자 읽기 */
static uint32_t u8next(const char *& s) {
    unsigned char c = (unsigned char)*s;
    if (!c) return 0;
    if (c < 0x80) { s++; return c; }
    if ((c & 0xE0) == 0xC0 && s[1]) { uint32_t r = ((c & 0x1F) << 6) | (s[1] & 0x3F); s += 2; return r; }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2]) { uint32_t r = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); s += 3; return r; }
    s++; return '?';
}
static int tw(const char * s) { int w = 0; for (;;) { uint32_t cp = u8next(s); if (!cp) break; w += cp < 0x80 ? 8 : 16; } return w; }
static int tw(const std::string & s) { return tw(s.c_str()); }

/* t48c2 (#641): 한글 낱자모(호환 자모 U+3131~U+3163) 글리프. 글꼴(FONTX2)에 완성형(가~힣)만 있고 낱자모가 없어서
 * 화면 키보드 자판 칸 · 조합 중 글자가 빈칸이 되었다. 16x16 칸에 획(사각형 R · 2픽셀 선 L · 점 P)으로 직접 그린다.
 * 좌표 = 칸 안 픽셀 (0~15). 겹글자(ㄳ ㅘ 등)는 낱자 둘을 가로 반으로 줄여 나란히 놓는다. */
namespace jm {
struct Def { uint32_t cp; const char * ops; };
static const Def kBasic[] = {
    { 0x3131, "R3,3,12,4;R11,3,12,12" },
    { 0x3132, "R2,3,6,4;R5,3,6,12;R9,3,13,4;R12,3,13,12" },
    { 0x3134, "R3,3,4,12;R3,11,12,12" },
    { 0x3137, "R3,3,12,4;R3,3,4,12;R3,11,12,12" },
    { 0x3138, "R2,3,6,4;R2,3,3,12;R2,11,6,12;R9,3,13,4;R9,3,10,12;R9,11,13,12" },
    { 0x3139, "R3,3,12,4;R11,3,12,7;R3,7,12,8;R3,7,4,12;R3,11,12,12" },
    { 0x3141, "R3,3,12,4;R3,3,4,12;R11,3,12,12;R3,11,12,12" },
    { 0x3142, "R3,2,4,12;R11,2,12,12;R3,7,12,8;R3,11,12,12" },
    { 0x3143, "R2,2,3,12;R5,2,6,12;R2,7,6,8;R2,11,6,12;R9,2,10,12;R12,2,13,12;R9,7,13,8;R9,11,13,12" },
    { 0x3145, "L7,3,3,12;L8,3,12,12" },
    { 0x3146, "L4,3,2,12;L5,3,6,12;L11,3,9,12;L12,3,13,12" },
    { 0x3147, "R5,3,10,4;R5,11,10,12;R3,5,4,10;R11,5,12,10;P4,4;P11,4;P4,11;P11,11" },
    { 0x3148, "R3,3,12,4;L7,5,3,12;L8,5,12,12" },
    { 0x3149, "R2,3,6,4;R9,3,13,4;L4,5,2,12;L5,5,6,12;L11,5,9,12;L12,5,13,12" },
    { 0x314A, "R7,2,8,3;R3,5,12,6;L7,7,3,12;L8,7,12,12" },
    { 0x314B, "R3,3,12,4;R11,3,12,12;R3,7,11,8" },
    { 0x314C, "R3,3,12,4;R3,3,4,12;R3,11,12,12;R3,7,11,8" },
    { 0x314D, "R3,3,12,4;R3,11,12,12;R5,5,6,10;R9,5,10,10" },
    { 0x314E, "R7,2,8,3;R3,5,12,6;R6,8,9,9;R6,12,9,13;R4,10,5,11;R10,10,11,11;P5,9;P10,9;P5,12;P10,12" },
    { 0x314F, "R6,2,7,13;R8,7,11,8" },
    { 0x3150, "R4,2,5,13;R6,7,8,8;R9,2,10,13" },
    { 0x3151, "R6,2,7,13;R8,5,11,6;R8,9,11,10" },
    { 0x3152, "R4,2,5,13;R6,5,8,6;R6,9,8,10;R9,2,10,13" },
    { 0x3153, "R8,2,9,13;R4,7,7,8" },
    { 0x3154, "R3,7,6,8;R7,2,8,13;R11,2,12,13" },
    { 0x3155, "R8,2,9,13;R4,5,7,6;R4,9,7,10" },
    { 0x3156, "R3,5,6,6;R3,9,6,10;R7,2,8,13;R11,2,12,13" },
    { 0x3157, "R7,5,8,10;R2,10,13,11" },
    { 0x315B, "R5,5,6,10;R9,5,10,10;R2,10,13,11" },
    { 0x315C, "R2,5,13,6;R7,7,8,12" },
    { 0x3160, "R2,4,13,5;R5,6,6,12;R9,6,10,12" },
    { 0x3161, "R2,7,13,8" },
    { 0x3163, "R7,2,8,13" },
};
struct Comp { uint32_t cp, a, b; };
static const Comp kComp[] = {
    { 0x3133, 0x3131, 0x3145 }, { 0x3135, 0x3134, 0x3148 }, { 0x3136, 0x3134, 0x314E }, { 0x313A, 0x3139, 0x3131 }, { 0x313B, 0x3139, 0x3141 },
    { 0x313C, 0x3139, 0x3142 }, { 0x313D, 0x3139, 0x3145 }, { 0x313E, 0x3139, 0x314C }, { 0x313F, 0x3139, 0x314D }, { 0x3140, 0x3139, 0x314E },
    { 0x3144, 0x3142, 0x3145 }, { 0x3158, 0x3157, 0x314F }, { 0x3159, 0x3157, 0x3150 }, { 0x315A, 0x3157, 0x3163 }, { 0x315D, 0x315C, 0x3153 },
    { 0x315E, 0x315C, 0x3154 }, { 0x315F, 0x315C, 0x3163 }, { 0x3162, 0x3161, 0x3163 },
};
static void rect(uint16_t * rw, int x0, int y0, int x1, int y1) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (x >= 0 && x < 16 && y >= 0 && y < 16) rw[y] |= (uint16_t)(0x8000u >> x); }
static void line(uint16_t * rw, int x0, int y0, int x1, int y1) {   /* 2픽셀 굵기: 가파르면 가로로 2칸, 완만하면 세로로 2칸 */
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, n = dx > dy ? dx : dy; if (n == 0) n = 1;
    for (int i = 0; i <= n; i++) {
        int x = (int)(x0 + (double)(x1 - x0) * i / n + 0.5), y = (int)(y0 + (double)(y1 - y0) * i / n + 0.5);
        if (dy >= dx) rect(rw, x, y, x + 1, y); else rect(rw, x, y, x, y + 1);
    }
}
static void draw_ops(uint16_t * rw, const char * s) {
    while (*s) {
        char t = *s++; int v[4] = { 0, 0, 0, 0 }, n = 0;
        while (*s && *s != ';') { if (*s >= '0' && *s <= '9') { int x = 0; while (*s >= '0' && *s <= '9') x = x * 10 + (*s++ - '0'); if (n < 4) v[n++] = x; } else s++; }
        if (*s == ';') s++;
        if (t == 'R') rect(rw, v[0], v[1], v[2], v[3]); else if (t == 'L') line(rw, v[0], v[1], v[2], v[3]); else if (t == 'P') rect(rw, v[0], v[1], v[0], v[1]);
    }
}
static bool basic_rows(uint32_t cp, uint16_t * rw) { for (size_t i = 0; i < sizeof(kBasic) / sizeof(kBasic[0]); i++) if (kBasic[i].cp == cp) { draw_ops(rw, kBasic[i].ops); return true; } return false; }
static unsigned half8(unsigned r) { unsigned o = 0; for (int j = 0; j < 8; j++) { unsigned b = ((r >> (15 - 2 * j)) | (r >> (14 - 2 * j))) & 1u; o |= b << (7 - j); } return o; }
/* 16x16 비트맵(행마다 2바이트, 왼쪽 위가 첫 비트 — FONTX2 전각과 같은 배치). 없으면 NULL */
static const uint8_t * glyph(uint32_t cp) {
    static uint8_t g[51][32]; static uint8_t st[51];   /* st: 0 안 만듦 · 1 있음 · 2 없음 */
    if (cp < 0x3131 || cp > 0x3163) return NULL;
    int i = (int)(cp - 0x3131); if (st[i] == 1) return g[i]; if (st[i] == 2) return NULL;
    uint16_t rw[16]; for (int y = 0; y < 16; y++) rw[y] = 0;
    bool ok = basic_rows(cp, rw);
    if (!ok) for (size_t k = 0; k < sizeof(kComp) / sizeof(kComp[0]); k++) if (kComp[k].cp == cp) {
        uint16_t a[16], b[16]; for (int y = 0; y < 16; y++) a[y] = b[y] = 0;
        if (basic_rows(kComp[k].a, a) && basic_rows(kComp[k].b, b)) { for (int y = 0; y < 16; y++) rw[y] = (uint16_t)((half8(a[y]) << 8) | half8(b[y])); ok = true; }
        break;
    }
    if (!ok) { st[i] = 2; return NULL; }
    for (int y = 0; y < 16; y++) { g[i][y * 2] = (uint8_t)(rw[y] >> 8); g[i][y * 2 + 1] = (uint8_t)(rw[y] & 255); }
    st[i] = 1; return g[i];
}
}   /* namespace jm */

static void blit_glyph(Canvas & c, const uint8_t * g, int gw, int bpr, int x, int y, uint32_t fg, uint32_t bg) {
    for (int j = 0; j < 16; j++) for (int i = 0; i < gw; i++) {
        int on = (g[j * bpr + (i >> 3)] >> (7 - (i & 7))) & 1;
        if (on) c.set(x + i, y + j, fg); else if (bg != NOBG) c.set(x + i, y + j, bg);
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
/* ---- t39: 글꼴 미리보기 — anex86 구조 bmp (2048x2048 1bpp, 16x16 칸: 열 = 구, 행 = 점+32, 잉크 = 0비트) */
struct BmpFont {
    std::vector<uint8_t> d; bool ok = false;
    static const size_t SIZE = 524350;
    static bool check_mem(const std::vector<uint8_t> & b, std::string * why) {
        auto r32 = [&](size_t o) { return (uint32_t)b[o] | ((uint32_t)b[o + 1] << 8) | ((uint32_t)b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24); };
        auto bad = [&](const char * w) { if (why) *why = w; return false; };
        if (b.size() != SIZE) return bad(phr_("\xED\x81\xAC\xEA\xB8\xB0\xEA\xB0\x80 524,350 B \xEA\xB0\x80 \xEC\x95\x84\xEB\x8B\x98"));   /* 크기가 524,350 B 가 아님 */
        if (b[0] != 'B' || b[1] != 'M') return bad(phr_("BMP \xEA\xB0\x80 \xEC\x95\x84\xEB\x8B\x98"));
        if (r32(14) != 40 || r32(18) != 2048 || (int32_t)r32(22) != 2048 || (b[28] | (b[29] << 8)) != 1 || r32(30) != 0 || r32(10) != 62) return bad(phr_("\xED\x98\x95\xEC\x8B\x9D \xEB\xB6\x88\xEC\x9D\xBC\xEC\xB9\x98(2048x2048 1bpp)"));   /* 형식 불일치 */
        int ink = 0; for (int y = 0; y < 2048; y += 3) for (int x = 0; x < 2048; x += 3) { int ry = 2047 - y; if (!((b[62 + (size_t)ry * 256 + (x >> 3)] >> (7 - (x & 7))) & 1)) ink++; }
        if (ink < 2000) return bad(phr_("\xEA\xB1\xB0\xEC\x9D\x98 \xEB\xB9\x84\xEC\x96\xB4 \xEC\x9E\x88\xEC\x9D\x8C"));   /* 거의 비어 있음 */
        return true;
    }
    bool load(const std::string & path, std::string * why = NULL) {
        ok = false; FILE * f = fopen(path.c_str(), "rb"); if (!f) { if (why) *why = phr_("\xEC\x97\xB4 \xEC\x88\x98 \xEC\x97\x86\xEC\x9D\x8C"); return false; }
        d.clear(); uint8_t b[8192]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0 && d.size() <= SIZE + 8192) d.insert(d.end(), b, b + n); fclose(f);
        ok = check_mem(d, why); return ok;
    }
    bool ink(int x, int y) const { int ry = 2047 - y; return !((d[62 + (size_t)ry * 256 + (x >> 3)] >> (7 - (x & 7))) & 1); }
};
static bool sjis2kuten(unsigned code, int & ku, int & ten) {
    int s1 = (int)(code >> 8), s2 = (int)(code & 255); int r = s1 < 0xA0 ? s1 - 0x81 : s1 - 0xC1; int j1 = 0x21 + 2 * r, j2;
    if (s2 >= 0x9F) { j1++; j2 = s2 - 0x7E; } else { int t = s2 >= 0x80 ? s2 - 1 : s2; j2 = t - 0x1F; }
    ku = j1 - 0x20; ten = j2 - 0x20; return ku >= 1 && ku <= 94 && ten >= 1 && ten <= 94;
}
/* 미리보기 글자 → (구,점). 한글(KS 2350 배치) · 히라가나/가타카나 · 기호. 없으면 false */
static bool bmp_cell(uint32_t cp, int & ku, int & ten) {
    if (cp >= 0x3041 && cp <= 0x3093) { ku = 4; ten = (int)(cp - 0x3041) + 1; return true; }
    if (cp >= 0x30A1 && cp <= 0x30F6) { ku = 5; ten = (int)(cp - 0x30A1) + 1; return true; }
    unsigned code = cp2code(cp); return code && sjis2kuten(code, ku, ten);
}
static int text_bmp(Canvas & c, const BmpFont & f, int x, int y, const std::string & str, uint32_t fg, uint32_t bg = NOBG) {
    int cx = x; const char * s = str.c_str();
    for (;;) {
        uint32_t cp = u8next(s); if (!cp) break;
        if (cp < 0x80) { const uint8_t * g = g_sb.glyph(cp); if (g) blit_glyph(c, g, 8, 1, cx, y, fg, bg); cx += 8; continue; }
        int ku, ten; if (f.ok && bmp_cell(cp, ku, ten)) {
            for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) { if (f.ink(ku * 16 + i, (ten + 32) * 16 + j)) c.set(cx + i, y + j, fg); else if (bg != NOBG) c.set(cx + i, y + j, bg); }
        } else if (bg != NOBG) c.rect(cx, y, 16, 16, bg);
        cx += 16;
    }
    return cx - x;
}
/* t52h (#134): 지금 UI 언어(일본어 여부)와 사용자 일본어 글꼴 BMP 를 알린다(UI 가 프레임마다 부름) */
static void jp_set(bool jp_ui, const BmpFont * bf) { g_jp_on = jp_ui; g_jp_bmpd = (bf && bf->ok) ? &bf->d : NULL; }
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
    for (const char * y : sy) { std::string tag = std::string("{") + y + "}"; s = help_rep(s, tag, g_helpdev == 1 ? help_name(y) : std::string(y)); }
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
    return tw(g_helpdev == 1 ? sk : sp) > c.w - 16 ? 2 : 1;
}
static void helpline(Canvas & c, const std::string & s0) {
    const int maxw = c.w - 16; const bool osk = g_help_osk; g_help_osk = false;
    std::string sp = help_common(s0), sk = help_kbd(sp);
    if (osk && g_helpdev == 1) sk = phr_("키보드로 입력  Enter:완료  Esc:취소  Backspace:지움  한/영 키:한글/영문");
    std::string s = g_helpdev == 1 ? sk : sp;
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
