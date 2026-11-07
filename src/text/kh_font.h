#pragma once
/* kh_font.h -- S7: ov 의 글꼴·글자 조합 부분(그리기 없음, Canvas 안 씀). gensei_ov.h 에서 코드 그대로 분할. ui/kh_draw.h 가 include 한다. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <iconv.h>
#if defined(__ANDROID__)
#include "ksx_hangul_tab.h"
#endif
#include "jis_sjis_tab.h"
#endif
#include "platform/plat.h" /* S3: platform layer */
#include "text/kh_phrase.h"   /* t52a (#49): windows.h 와 그 앞의 LEAN_AND_MEAN 정의 뒤에 둔다 */

namespace ov {

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
#if !defined(_WIN32)
static unsigned sjis_of(uint32_t cp) {
    if (cp > 0xFFFF) return 0;
    int lo = 0, hi = kSjisN - 1;
    while (lo <= hi) { int m = (lo + hi) >> 1; if (kSjisCp[m] == cp) return kSjisCode[m]; if (kSjisCp[m] < cp) lo = m + 1; else hi = m - 1; }
    return 0;
}
#endif
/* 글자 → 16x16(32 B) 글리프. 없거나 빈 칸이면 NULL(→ 기존 경로). tmp = 호출한 쪽의 32 B 임시 칸 */
static const uint8_t * jp_glyph(uint32_t cp, uint8_t * tmp) {
    if (!g_jp_bmpd && !g_jp_fx.ok) return NULL;
    if (!jp_cp_ok(cp)) return NULL;
#if defined(_WIN32)
    char mb[4] = { 0, 0, 0, 0 }; bool used = false;
    int n = plat::wchar_to_cp(932, (wchar_t)cp, mb, 4, &used);
    if (n != 2 || used) return NULL;
    int s1 = (uint8_t)mb[0], s2 = (uint8_t)mb[1];
#else
    /* t672 (#656): bionic has no CP932 iconv -> every kanji came out blank on Android. Use the embedded table (jis_sjis_tab.h). */
    unsigned sj = sjis_of(cp); if (!sj) return NULL;
    int s1 = (int)(sj >> 8), s2 = (int)(sj & 255);
#endif
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
        char mb[4]; bool bad = false;
        int n = plat::wchar_to_cp(949, (wchar_t)cp, mb, 4, &bad);
        if (n == 2 && !bad) { unsigned a = (uint8_t)mb[0], b = (uint8_t)mb[1]; if (a >= 0xB0 && a <= 0xC8 && b >= 0xA1 && b <= 0xFE) return jis2sjis((int)(a - 0xA0) + 0x20, (int)(b - 0xA0) + 0x20); }
    }
#else
    if (cp >= 0xAC00 && cp <= 0xD7A3) {
#if defined(__ANDROID__)
        /* no EUC-KR iconv on Android: look the syllable up in the KS X 1001 table (index = EUC-KR cell order) */
        { int lo = 0, hi = 2349; while (lo <= hi) { int m = (lo + hi) >> 1; if (kKsxTab[m] == cp) { unsigned a = 0xB0u + (unsigned)(m / 94), b = 0xA1u + (unsigned)(m % 94); return jis2sjis((int)(a - 0xA0) + 0x20, (int)(b - 0xA0) + 0x20); } if (kKsxTab[m] < cp) lo = m + 1; else hi = m - 1; } return 0; }
#endif
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
/* t52h (#134): 지금 UI 언어(일본어 여부)와 사용자 일본어 글꼴 BMP 를 알린다(UI 가 프레임마다 부름) */
static void jp_set(bool jp_ui, const BmpFont * bf) { g_jp_on = jp_ui; g_jp_bmpd = (bf && bf->ok) ? &bf->d : NULL; }

} /* namespace ov */
