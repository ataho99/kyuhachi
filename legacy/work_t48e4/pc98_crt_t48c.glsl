/*
   pc98_crt.glsl — PC-98 미니 에뮬 화면 효과 셰이더 (t48c)   [표지: PC98_FX_T48C — 호스트가 이 글자가 없는 옛 파일은 쓰지 않고 내장판을 쓴다]
   원본: CRT - Guest mini (crt-gdv-mini), edited by metallic77, Copyright (C) 2017-2018 guest(r)
     libretro/glsl-shaders crt/shaders/crt-gdv-mini.glsl
   변경(대장방 2026-10-23): RetroArch 규약을 우리 규약으로 평탄화, PC-98 모니터 기본값, 볼록(곡면) 세기를 uniform 으로.
   변경(지시 #311·#313 2026-10-26): 번짐 한 개(BLEED)로 컨버전스 어긋남 + 가로 번짐 + 형광체 번짐을 함께 조절.
   변경(지시 #624·#625·#639 2026-10-29, t48c): 「필터 1줄 + 효과 4줄」 구조.
     - 필터(밑 그림): sharp(uNear=0) / nearest(uNear=1). 효과는 어느 필터에나 얹는다. 프리셋(옛 crt)은 없다.
     - 효과 4개 각각의 세기를 uniform 으로 받는다: uScan(주사선) · uMask(마스크) · uBleed(RGB 번짐) · uGlow(빛 번짐). 0 = 끔.
     - 모든 효과가 꺼져 있고 곡면도 0 이면 호스트는 이 셰이더를 쓰지 않고 sharp/nearest 셰이더를 쓴다.
     - 효과 섞는 순서(고정): ①곡면(화면 좌표 휨) → ②밑 그림 읽기 + RGB 번짐(신호·컨버전스 단계: 색 어긋남과 가로 번짐은 빔이 그리기 전 신호에 생긴다)
       → ③주사선(빔 모양, 선형 빛 공간) → ④빛 번짐(형광체 번짐: 주사선으로 줄이 생긴 뒤의 빛이 퍼진다) → ⑤마스크(마지막 형광체 격자 — 번진 빛도 격자를 통과한다)
       → ⑥출력 감마 · 밝기 보정(주사선·마스크로 어두워진 만큼만).
     - 옛 crt 프리셋의 기본 그림(주사선 7.0, 마스크 0.15, 번짐 0, 곡면 설정값)은 uScan=7 · uMask=0.15 · uNear=0 에서 화소가 같다.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   호스트 약속(pc98_sharp.glsl 과 같음)
    - 맨 앞에 버전 줄 + "#define VERTEX"/"#define FRAGMENT" 를 붙여 컴파일. GL 2.1 / GLES2 / GL3 / GLES3 공통.
    - attribute aPos(-1..1), aUV(0..1, 위=0)
    - uniform uTex(원본 640x400 텍스처, 필터 GL_LINEAR, CLAMP_TO_EDGE), uSrcSize(640,400), uOutSize(사각형의 실제 화면 화소)
    - uniform uCurve 볼록 세기(0 = 평면) · uScan 주사선(0 = 끔, 3..12) · uMask 마스크(0 = 끔, 0.05..0.50) · uBleed RGB 번짐(0..1) · uGlow 빛 번짐(0..1) · uNear 밑 그림 nearest(1) / sharp(0)
    - 4:3 비율은 호스트가 사각형을 4:3 으로 잡아서 맞춘다. 곡면을 켜면 모서리 밖은 검정.
*/
/* t48d2c (daejang #644): 세기 곡선 다듬기 — 마스크 1단계부터 보임(보정 곱셈 제거) · 빛번짐 1단계 약하게/상한 낮춤 · 주사선 1배 창에서 줄 번갈아 어둡게(빔 꼬리 제거) · RGB 번짐 단계 고르게. 효과 0 경로 불변. */
/* t48d (daejang #643): stronger mask (stripes+grid), RGB bleed = R/B horizontal misregistration (no blur), glow = wider/stronger halo. All effects at 0 -> same pixels as t48c2. Marker PC98_FX_T48C kept on purpose. */
#define BRIGHTBOOST 1.20   // 밝기 보정(주사선·마스크로 어두워진 만큼)
#define SAT         1.00   // 채도. 1.0 = 원판 팔레트 그대로
#define BEAM_MIN    1.20   // 어두운 곳 빔 굵기(작을수록 굵음 = 줄 옅음)
#define BEAM_MAX    0.95   // 밝은 곳 빔 굵기
#define H_SHARP     2.00   // 가로 선명도
#define GAMMA_OUT   0.50   // 출력 감마(내부는 제곱 선형화)
#define MASK_TYPE   0.0    // -1 없음, 0 CGWG(세로 2줄 마스크), 2 애퍼처 그릴
#define MASK_SIZE   1.0    // 4K 화면이면 2.0
#define MASK_DARK   0.50
#define MASK_LIGHT  1.50

#ifdef GL_ES
  #ifdef GL_FRAGMENT_PRECISION_HIGH
    precision highp float;
  #else
    precision mediump float;
  #endif
#endif

#if __VERSION__ >= 130
  #ifdef VERTEX
    #define VARY out
    #define ATTR in
  #else
    #define VARY in
    out vec4 FragColor;
  #endif
  #define TEX texture
#else
  #define VARY varying
  #define ATTR attribute
  #define FragColor gl_FragColor
  #define TEX texture2D
#endif

uniform vec2 uSrcSize;
uniform vec2 uOutSize;
uniform float uCurve;
uniform float uScan;    // 주사선: 0 = 끔, 아니면 옛 SCANLINE 값(3..12, 옛 기본 7.0)
uniform float uMask;    // 마스크 세기: 0 = 끔, 옛 CGWG 값(0.05..0.50, 옛 기본 0.15)
uniform float uBleed;   // RGB 번짐 0..1: 컨버전스 어긋남(가장자리일수록 큼) + 가로 번짐
uniform float uGlow;    // 빛 번짐 0..1: 밝은 곳 주변으로 퍼지는 형광체 빛
uniform float uNear;    // 밑 그림: 0 = sharp, 1 = nearest

VARY vec2 vUV;
VARY vec2 vFrag;   // 출력 화면 화소 좌표(마스크용)

#ifdef VERTEX
ATTR vec2 aPos;
ATTR vec2 aUV;
void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV   = aUV;
    vFrag = aUV * uOutSize;
}
#endif

#ifdef FRAGMENT
uniform sampler2D uTex;

const vec3 LUMW = vec3(0.30, 0.59, 0.11);

float gSh = 0.0;   // 컨버전스 어긋남(원본 화소 단위). 중앙 0, 좌우 가장자리로 갈수록 커짐

// 원본 텍스처 한 점. uBleed=0 이면 그냥 한 번 읽음. 켜면 가로 5탭 번짐 + R·B 가로 이동(컨버전스)
vec3 smp(vec2 c)
{
    if (uBleed <= 0.0) return TEX(uTex, c).rgb;
    // t48d: not blur - R/G/B planes misregistered horizontally (color fringes). G stays, R shifts +d, B shifts -d. d = uBleed*2.4 source px (max 2.4).
    float bb10 = uBleed * 10.0;                                                    // t48d2c: 단계 1..10
    float dpx = (bb10 <= 5.0) ? (0.24 + (bb10 - 1.0) * 0.171) : (0.925 + (bb10 - 5.0) * 0.295);
    float d = dpx / uSrcSize.x;
    return vec3(TEX(uTex, c + vec2(d, 0.0)).r, TEX(uTex, c).g, TEX(uTex, c - vec2(d, 0.0)).b);
}

// 형광체 번짐: 주변 밝은 화소의 빛만 약하게 퍼짐(자기 자신 제외, 선형 공간)
vec3 bloom(vec2 uvc)
{
    vec2 ps = 1.0 / uSrcSize;
    vec3 s = vec3(0.0);
    float ws = 0.0;
    // t48d: radius 3 (7x7), wider halo; centre excluded from both sum and weight
    for (int j = -3; j <= 3; j++) {
        for (int i = -3; i <= 3; i++) {
            if (i == 0 && j == 0) continue;
            float r2 = float(i * i + j * j);
            float w = exp(-r2 * 0.18);
            ws += w;
            vec3 t = TEX(uTex, uvc + vec2(float(i), float(j)) * ps).rgb; t *= t;
            s += w * t * smoothstep(0.35, 0.85, dot(t, LUMW));
        }
    }
    return s / ws;
}

float beam(float y, float l)
{
    float scan = mix(uScan - 2.0, uScan, y);
    float w = mix(BEAM_MIN, BEAM_MAX, l);
    float ex = y * w;
    return exp2(-scan * ex * ex);
}

vec3 mask(vec2 pos, vec3 c)
{
    pos = floor(pos / MASK_SIZE);
    vec3 m = vec3(MASK_DARK);
    if (MASK_TYPE < -0.5) {
        m = vec3(1.0);
    } else if (MASK_TYPE < 0.5) {
        // t48d2c: 단계 st = 1..10 → lv = 0.15 .. 1.0 (선형). 어둡기는 출력(감마 뒤) 공간 기준으로 정하고 선형 공간으로 제곱 변환 → 평균 차이가 단계에 거의 비례.
        float st = clamp(uMask / 0.05, 1.0, 10.0);
        float lv = 0.15 + 0.85 * (st - 1.0) / 9.0;
        float sd = 0.55 * lv, gd = 0.235 * lv;
        float mc = (1.0 - sd) * (1.0 - sd);
        if (fract(pos.x * 0.5) < 0.5) m = vec3(1.0, mc, 1.0);
        else                          m = vec3(mc, 1.0, mc);
        if (fract(pos.y * 0.5) >= 0.5) m *= (1.0 - gd) * (1.0 - gd);   // 한 줄 건너 어둡게 = 격자
    } else {
        float x = fract(pos.x / 3.0);
        if      (x < 0.3333) m.b = MASK_LIGHT;
        else if (x < 0.6666) m.g = MASK_LIGHT;
        else                 m.r = MASK_LIGHT;
    }
    return m;
}

vec2 warp(vec2 uv)
{
    vec2 p = uv * 2.0 - 1.0;
    p *= vec2(1.0 + p.y * p.y * uCurve, 1.0 + p.x * p.x * uCurve * 1.33); // 4:3 화면이라 세로 휨을 조금 더
    return p * 0.5 + 0.5;
}

void main()
{
    vec2 uv = warp(vUV);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

    gSh = uBleed * 0.30 * (uv.x * 2.0 - 1.0);
    float sy = uOutSize.y / uSrcSize.y;                  // t48d2c: 출력 화소 / 원본 줄. 2 이상이면 빔 모양이 보임
    float lowres = clamp(2.0 - sy, 0.0, 1.0);            // 1 = 1배 이하(줄 번갈아 어둡게), 0 = 2배 이상(옛 빔 그대로)
    vec2 ps = 1.0 / uSrcSize;
    vec3 col;

    if (uScan > 0.0) {
        // 주사선 켬: 옛 crt 경로 그대로(uNear=0 이면 화소 동일). nearest 밑 그림이면 가로 보간만 계단으로.
        vec2 tp = uv * uSrcSize - vec2(0.5, 0.0);      // 가로는 화소 중심 기준, 세로는 주사선 위쪽 기준
        vec2 fp = fract(tp);
        vec2 c0 = (floor(tp) + vec2(0.5)) * ps;

        vec3 l0 = smp(c0);
        vec3 r0 = smp(c0 + vec2(ps.x, 0.0));
        vec3 l1 = smp(c0 + vec2(0.0, ps.y));
        vec3 r1 = smp(c0 + ps);

        float lx, rx;
        if (uNear > 0.5) { lx = (fp.x < 0.5) ? 1.0 : 0.0; rx = 1.0 - lx; }
        else             { lx = pow(1.0 - fp.x, H_SHARP); rx = pow(fp.x, H_SHARP); }
        vec3 a = (l0 * lx + r0 * rx) / (lx + rx);
        vec3 b = (l1 * lx + r1 * rx) / (lx + rx);
        a *= a; b *= b;                                 // 선형화(감마 2.0 근사)

        float f = fp.y - 0.5;                           // 주사선 중심 = 화소 세로 중심
        float la = dot(a, LUMW);
        float lb = dot(b, LUMW);
        if (f >= 0.0) col = a * beam(f, la) + b * beam(1.0 - f, lb) * (1.0 - lowres);
        else {
            vec3 u = smp(c0 - vec2(0.0, ps.y));
            vec3 ur = smp(c0 + vec2(ps.x, -ps.y));
            vec3 up = (u * lx + ur * rx) / (lx + rx); up *= up;
            col = a * beam(-f, la) + up * beam(1.0 + f, dot(up, LUMW)) * (1.0 - lowres);
        }
    } else {
        // 주사선 끔: 밑 그림 그대로(sharp = pc98_sharp.glsl 과 같은 면적 평균, nearest = 화소 중심) 위에 나머지 효과만.
        vec2 t;
        if (uNear > 0.5) {
            t = floor(uv * uSrcSize) + vec2(0.5);
        } else {
            vec2 sc  = uOutSize / uSrcSize;
            vec2 inv = uSrcSize / uOutSize;
            vec2 pix = uv * uOutSize;
            vec2 p_tl = floor(pix);
            vec2 p_br = ceil(pix);
            vec2 t_tl = floor(inv * p_tl);
            vec2 t_br = floor(inv * p_br);
            t = t_br + vec2(0.5);
            t -= (vec2(1.0) - step(t_br, t_tl)) * (sc * t_br - p_tl);
        }
        vec3 s = smp(t * ps);
        col = s * s;
    }

    // t48d2c: 주사선 저해상도 보강 — 단계 = uScan-2 (1..10), 출력 공간 5%/단계, 한 줄 건너 어둡게.
    if (uScan > 0.0 && lowres > 0.0) {
        float sst = clamp(uScan - 2.0, 1.0, 10.0);
        float rr = 0.05 * sst * lowres;
        float odd = step(0.5, fract(floor(vFrag.y) * 0.5));
        col *= mix(1.0, (1.0 - rr) * (1.0 - rr), odd);
    }
    // t48d2c: 빛번짐 단계 st=1..10 → lg = 0.12..1.0, 선형 이득 = 최대 0.35 * lg^2 (출력 공간에서 단계에 비례, 최대에서 흰 막대 안 글자가 읽히게 t48d 1.5 에서 낮춤)
    if (uGlow > 0.0) {
        float gst = clamp(uGlow / 0.1, 1.0, 10.0);
        float lg = 0.12 + 0.88 * (gst - 1.0) / 9.0;
        col += 0.35 * lg * lg * bloom(uv);
    }
    if (uMask > 0.0) col *= mask(vFrag, col);
    col = pow(max(col, 0.0), vec3(GAMMA_OUT));
    float lum = dot(col, LUMW);
    float bf = (uScan > 0.0) ? (1.0 - lowres) : 0.0;   // t48d2c: 주사선 있고 2배 이상일 때만 보정(옛과 같음). 마스크 단독·저해상도는 보정 없음
    float bb = mix(1.0, BRIGHTBOOST, bf);
    col *= mix(1.0, bb, lum);
    col = mix(vec3(lum), col, SAT);
    FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
#endif
