#pragma once
/* kh_settings.h -- S5: ui::Settings 구조체·옛 형식 변환(fx_*)·settings.ini 한 구역 읽기/쓰기(ini_put·ini_set). kh_ui.h 에서 코드 그대로 옮김, 네임스페이스 ui:: 유지(이름 정리는 S7) */
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
namespace ui {
/* ---- 설정 (전역 + 작품별 덮어쓰기) */
struct Settings {
    int ratio = 0;      /* 0 4:3  1 16:10  2 정수배 */
    int shader = 0;     /* t48c: 필터 0 sharp 2 nearest. 1 = 옛 crt 프리셋: 파일에서 읽을 때만 나오며 「sharp + 효과 켬」으로 바뀐다 */
    int curve = 0;      /* 0..10 → 곡면 0.00..0.10 */
    int scan = 0;      /* t48c: 주사선 0..10 (0 = 끔, 1..10 → 옛 SCANLINE 2+n) */       /* 0..10 */
    int mask = 0;      /* t48c: 마스크 0..10 (0 = 끔, n → 옛 CGWG n*0.05) */       /* 0 꺼짐 1 약 2 중 3 강 */
    int glow = 0;       /* t48c (#625): 빛 번짐 0..10 (0 = 끔) */
    int bleed = 0;      /* t48c: 이제 「RGB 번짐」 0..10 (컨버전스 어긋남 + 가로 번짐). 옛 「번짐」은 RGB 번짐 + 빛 번짐 같은 값으로 변환 */      /* t42(#311): 0..10 → 번짐(실물 RGB 모니터 결함: 컨버전스·가로 번짐·형광체 번짐) 0.0..1.0, 0 = 끔 */
    int bgm = 8;        /* 0..8 */
    int se = 8;         /* 0..8 */
    int device = 0;     /* 0 기본 장치 */
    int pad = 0;        /* 0 일본식 ○결정 */
    int bmult = 1;      /* 0..4 = x2 x3 x4 x6 x8 */
    int bmode = 2;      /* 0 홀드 1 토글 2 둘 다 */
    int boot = 0;       /* 0 켬 1 끔 */
    int bosd = 1;       /* 배속 표시(오른쪽 위 ▶▶) 0 끔 1 켬 */
    int fpsd = 0;       /* FPS / speed display 0 off 1 on (ini key fpsdisp) */
    int kbshader = 0;   /* t46z (#548): 화면 키보드·버튼·배속·OSD 에 셰이더 적용 0 안 함(기본) 1 적용 */
    int kbbtn = 1;      /* t46x: 화면 키보드 토글 버튼 표시 0 숨김 1 표시 */
    int ovbtn = 1;      /* t52f (#132): 오버레이 버튼 줄(메뉴·저장·불러오기·배속·키보드) 표시 0 숨김 1 표시 */
    int padbtn = 2;     // virtual pad in the side bands: 0 off, 1 on, 2 auto (on with a touch screen and no real pad)
    int bsound = 2;     /* r017 배속 중 소리: 0 끔 1 빠르게(음높이 상승) 2 음높이 유지 */
    bool lmode = false, lseen = false; int l_sh = 0, l_scan = 5, l_mask = 1, l_bleed = 0, l_curve = 4;   /* t48c: 옛 형식(fx=1 표지 없음) 파일을 읽는 동안만 쓰는 칸. 끝나면 fx_finish 가 새 칸으로 옮긴다 */
    float curve_f() const { return curve * 0.01f; }
    float glow_f() const { return glow * 0.1f; }
    float bleed_f() const { return bleed * 0.1f; }
    float scan_f() const { return scan / 10.0f; }
    float mask_f() const { return (mask < 0 ? 0 : mask > 10 ? 10 : mask) * 0.05f; }   /* t48c: 옛 마스크 1/2/3 = 0.15/0.3/0.5 = 새 3/6/10 단계 */
    int boost_x() const { static const int x[5] = { 2, 3, 4, 6, 8 }; return x[bmult < 0 ? 0 : bmult > 4 ? 4 : bmult]; }
    int bs_gain256() const { return boost_x() >= 3 ? 205 : 256; }   /* x3 이상은 조금 낮춤(-2 dB) */
    float se_gain() const { return se / 8.0f; }
    float bgm_gain() const { return bgm / 8.0f; }
};
/* t48c (#639 ①): 옛 설정(shader=1 crt 프리셋 + scan/mask/bleed) → 새 설정(필터 + 효과). 같은 그림이 나오게 옮긴다 */
static inline int fx_mask_leg(int m) { static const int t[4] = { 0, 3, 6, 10 }; return t[m < 0 ? 0 : m > 3 ? 3 : m]; }
inline void fx_begin(Settings & s) { s.lmode = true; s.lseen = false; s.l_sh = 0; s.l_scan = 5; s.l_mask = 1; s.l_bleed = 0; s.l_curve = 4; }
inline void fx_finish(Settings & s) {
    if (!s.lmode) return; s.lmode = false; if (!s.lseen) return; s.lseen = false;
    if (s.l_sh == 1) { s.shader = 0; s.scan = s.l_scan < 1 ? 1 : s.l_scan; s.mask = fx_mask_leg(s.l_mask); s.bleed = s.l_bleed; s.glow = s.l_bleed; s.curve = s.l_curve; }   /* 옛 번짐 = RGB 번짐 + 빛 번짐 같은 값(가장자리 가중 컨버전스 포함, 화소 동일) */
    else { s.shader = (s.l_sh == 2) ? 2 : 0; s.scan = 0; s.mask = 0; s.bleed = 0; s.glow = 0; s.curve = 0; }   /* 옛 sharp/nearest 에서는 효과가 안 먹었다 */
}

/* ---- 설정 파일 (settings.ini) -- 전역 [global] + 작품별 [id] */
static void ini_put(FILE * f, const Settings & s) {
    fprintf(f, "fx=1\nratio=%d\nshader=%d\ncurve=%d\nbleed=%d\nglow=%d\nscan=%d\nmask=%d\nbgm=%d\nse=%d\ndevice=%d\npad=%d\nbmult=%d\nbmode=%d\nboot=%d\nbosd=%d\nbsound=%d\nkbbtn=%d\nkbshader=%d\novbtn=%d\npadbtn=%d\n", s.ratio, (s.shader == 2 ? 2 : 0), s.curve, s.bleed, s.glow, s.scan, s.mask, s.bgm, s.se, s.device, s.pad, s.bmult, s.bmode, s.boot, s.bosd, s.bsound, s.kbbtn, s.kbshader, s.ovbtn, s.padbtn);
    if (s.fpsd) fprintf(f, "fpsdisp=1\n");
}
static bool ini_set(Settings & s, const std::string & k, int v) {
    if (k == "fx") { s.lmode = false; }   /* t48c: 새 형식 표지. 이 구역의 화면 키는 새 범위로 읽는다 */
    else if (s.lmode && (k == "shader" || k == "curve" || k == "bleed" || k == "scan" || k == "mask")) { s.lseen = true; if (k == "shader") s.l_sh = v; else if (k == "curve") s.l_curve = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "bleed") s.l_bleed = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "scan") s.l_scan = v < 0 ? 0 : v > 10 ? 10 : v; else s.l_mask = v < 0 ? 0 : v > 3 ? 3 : v; }   /* 옛 형식: 구역이 끝날 때 fx_finish 가 옮긴다 */
    else if (k == "ratio") s.ratio = v; else if (k == "shader") s.shader = (v == 2) ? 2 : 0; else if (k == "curve") s.curve = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "bleed") s.bleed = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "glow") s.glow = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "scan") s.scan = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "mask") s.mask = v < 0 ? 0 : v > 10 ? 10 : v;
    else if (k == "bgm") s.bgm = v; else if (k == "se") s.se = v; else if (k == "device") s.device = v; else if (k == "pad") s.pad = v; else if (k == "bmult") s.bmult = v; else if (k == "bmode") s.bmode = v; else if (k == "boot") s.boot = v; else if (k == "bosd") s.bosd = v; else if (k == "bsound") s.bsound = v; else if (k == "kbbtn") s.kbbtn = v < 0 ? 0 : v > 1 ? 1 : v; else if (k == "ovbtn") s.ovbtn = v < 0 ? 0 : v > 1 ? 1 : v; else if (k == "padbtn") s.padbtn = v < 0 ? 0 : v > 2 ? 2 : v; else if (k == "kbshader") s.kbshader = v < 0 ? 0 : v > 1 ? 1 : v; else if (k == "fpsdisp") s.fpsd = v ? 1 : 0; else return false;
    return true;
}

} /* namespace ui */
