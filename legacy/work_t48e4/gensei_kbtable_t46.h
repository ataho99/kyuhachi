/* gensei_kbtable_t46.h -- t46x 화면 키보드 키 표 (플랫폼 무관 자료).
 * 이 파일은 keyboard.h 의 KBD_KEYS 열거만 쓴다. SDL·Win32·ov 를 include 하지 않는다 (안드로이드는 입력층만 바꾼다, 대장방 #534 ⒜).
 * 좌표 = 격자 단위: x4 = 1/4 키 단위(1u = 24px 피치), w4 = 폭(1/4 단위), row = 0..5, rows = 세로 칸 수. 픽셀 환산은 gensei_kbov_t46.h.
 * 스캔코드 = DOSBox-X keyboard.cpp KEYBOARD_PC98_AddKey 의 switch 값 (실기 PC-98 키보드와 대조하지 않았음 [미확인]).
 * 주의 (#534 ⒟): PC-98 ROLL UP(0x36) = KBD_pagedown, ROLL DOWN(0x37) = KBD_pageup — DOSBox-X 열거 이름이 반대다.
 * 주의: pc98_force_ibm_layout 에 따라 달라지는 키는 [ 와 ] 둘뿐 — kbd(기본, 플래그 false) · kbd_ibm(플래그 true) 둘 다 적는다. 나머지는 kbd_ibm = -1. */
#pragma once
#include <stdint.h>
#include "keyboard.h"

namespace kbt {
enum Kind { K_NORM = 0, K_MOD = 1, K_TOG = 2 };   /* 일반 · 수식(탭 잠금) · 토글(CAPS·かな: 코어가 누를 때만 받음) */
struct Key { const char * label; int kbd; int kbd_ibm; int x4, w4, row, rows; int kind; int scan; };
#define KBK_N(lbl, kbdv, x4v, w4v, rowv, scanv) { lbl, (int)(kbdv), -1, x4v, w4v, rowv, 1, kbt::K_NORM, scanv }
#define KBK_M(lbl, kbdv, x4v, w4v, rowv, scanv) { lbl, (int)(kbdv), -1, x4v, w4v, rowv, 1, kbt::K_MOD, scanv }
#define KBK_T(lbl, kbdv, x4v, w4v, rowv, scanv) { lbl, (int)(kbdv), -1, x4v, w4v, rowv, 1, kbt::K_TOG, scanv }
static const Key g_keys[] = {
    /* ---- R0: STOP COPY f·1~f·10 | vf·1~5 */
    KBK_N("STOP", KBD_stop, 0, 6, 0, 0x60), KBK_N("COPY", KBD_copy, 6, 6, 0, 0x61),
    KBK_N("f1", KBD_f1, 12, 6, 0, 0x62), KBK_N("f2", KBD_f2, 18, 6, 0, 0x63), KBK_N("f3", KBD_f3, 24, 6, 0, 0x64), KBK_N("f4", KBD_f4, 30, 6, 0, 0x65), KBK_N("f5", KBD_f5, 36, 6, 0, 0x66),
    KBK_N("f6", KBD_f6, 42, 6, 0, 0x67), KBK_N("f7", KBD_f7, 48, 6, 0, 0x68), KBK_N("f8", KBD_f8, 54, 6, 0, 0x69), KBK_N("f9", KBD_f9, 60, 6, 0, 0x6A), KBK_N("f10", KBD_f10, 66, 6, 0, 0x6B),
    KBK_N("v1", KBD_vf1, 74, 4, 0, 0x52), KBK_N("v2", KBD_vf2, 78, 4, 0, 0x53), KBK_N("v3", KBD_vf3, 82, 4, 0, 0x54), KBK_N("v4", KBD_vf4, 86, 4, 0, 0x55), KBK_N("v5", KBD_vf5, 90, 4, 0, 0x56),
    /* ---- R1: ESC 1~0 - ^ ¥ BS | INS DEL | 패드 - / */
    KBK_N("ESC", KBD_esc, 0, 6, 1, 0x00),
    KBK_N("1", KBD_1, 6, 4, 1, 0x01), KBK_N("2", KBD_2, 10, 4, 1, 0x02), KBK_N("3", KBD_3, 14, 4, 1, 0x03), KBK_N("4", KBD_4, 18, 4, 1, 0x04), KBK_N("5", KBD_5, 22, 4, 1, 0x05),
    KBK_N("6", KBD_6, 26, 4, 1, 0x06), KBK_N("7", KBD_7, 30, 4, 1, 0x07), KBK_N("8", KBD_8, 34, 4, 1, 0x08), KBK_N("9", KBD_9, 38, 4, 1, 0x09), KBK_N("0", KBD_0, 42, 4, 1, 0x0A),
    KBK_N("-", KBD_minus, 46, 4, 1, 0x0B), KBK_N("^", KBD_caret, 50, 4, 1, 0x0C), KBK_N("\\", KBD_jp_yen, 54, 4, 1, 0x0D),   /* ¥ 키 (글꼴에 ¥ 가 없어 \ 로 표시) */
    KBK_N("BS", KBD_backspace, 58, 8, 1, 0x0E),
    KBK_N("INS", KBD_insert, 67, 6, 1, 0x38), KBK_N("DEL", KBD_delete, 73, 6, 1, 0x39),
    KBK_N("-", KBD_kpminus, 86, 4, 1, 0x40), KBK_N("/", KBD_kpdivide, 90, 4, 1, 0x41),
    /* ---- R2: TAB Q~P @ [ RETURN | ROLL UP ROLL DOWN | 패드 7 8 9 * */
    KBK_N("TAB", KBD_tab, 0, 12, 2, 0x0F),
    KBK_N("Q", KBD_q, 12, 4, 2, 0x10), KBK_N("W", KBD_w, 16, 4, 2, 0x11), KBK_N("E", KBD_e, 20, 4, 2, 0x12), KBK_N("R", KBD_r, 24, 4, 2, 0x13), KBK_N("T", KBD_t, 28, 4, 2, 0x14),
    KBK_N("Y", KBD_y, 32, 4, 2, 0x15), KBK_N("U", KBD_u, 36, 4, 2, 0x16), KBK_N("I", KBD_i, 40, 4, 2, 0x17), KBK_N("O", KBD_o, 44, 4, 2, 0x18), KBK_N("P", KBD_p, 48, 4, 2, 0x19),
    KBK_N("@", KBD_atsign, 52, 4, 2, 0x1A),
    { "[", (int)KBD_rightbracket, (int)KBD_leftbracket, 56, 4, 2, 1, K_NORM, 0x1B },   /* [ = 플래그 false: KBD_rightbracket(0x1B) / true: KBD_leftbracket(0x1B) */
    { "RET", (int)KBD_enter, -1, 60, 6, 2, 2, K_NORM, 0x1C },                          /* RETURN (세로 2칸) */
    KBK_N("RLUP", KBD_pagedown, 67, 6, 2, 0x36), KBK_N("RLDN", KBD_pageup, 73, 6, 2, 0x37),   /* ROLL UP = KBD_pagedown, ROLL DOWN = KBD_pageup (열거 이름 반대) */
    KBK_N("7", KBD_kp7, 86, 4, 2, 0x42), KBK_N("8", KBD_kp8, 90, 4, 2, 0x43), KBK_N("9", KBD_kp9, 94, 4, 2, 0x44), KBK_N("*", KBD_kpmultiply, 98, 4, 2, 0x45),
    /* ---- R3: CTRL CAPS A~L ; : ] | HOME/CLR HELP | 패드 4 5 6 + */
    KBK_M("CTRL", KBD_leftctrl, 0, 6, 3, 0x74), KBK_T("CAPS", KBD_capslock, 6, 6, 3, 0x71),
    KBK_N("A", KBD_a, 12, 4, 3, 0x1D), KBK_N("S", KBD_s, 16, 4, 3, 0x1E), KBK_N("D", KBD_d, 20, 4, 3, 0x1F), KBK_N("F", KBD_f, 24, 4, 3, 0x20), KBK_N("G", KBD_g, 28, 4, 3, 0x21),
    KBK_N("H", KBD_h, 32, 4, 3, 0x22), KBK_N("J", KBD_j, 36, 4, 3, 0x23), KBK_N("K", KBD_k, 40, 4, 3, 0x24), KBK_N("L", KBD_l, 44, 4, 3, 0x25),
    KBK_N(";", KBD_semicolon, 48, 4, 3, 0x26), KBK_N(":", KBD_colon, 52, 4, 3, 0x27),
    { "]", (int)KBD_backslash, (int)KBD_rightbracket, 56, 4, 3, 1, K_NORM, 0x28 },      /* ] = 플래그 false: KBD_backslash(0x28) / true: KBD_rightbracket(0x28) */
    KBK_N("HOME", KBD_home, 67, 6, 3, 0x3E), KBK_N("HELP", KBD_help, 73, 6, 3, 0x3F),
    KBK_N("4", KBD_kp4, 86, 4, 3, 0x46), KBK_N("5", KBD_kp5, 90, 4, 3, 0x47), KBK_N("6", KBD_kp6, 94, 4, 3, 0x48), KBK_N("+", KBD_kpplus, 98, 4, 3, 0x49),
    /* ---- R4: SHIFT Z~M , . / _ SHIFT | UP | 패드 1 2 3 = */
    KBK_M("SHIFT", KBD_leftshift, 0, 12, 4, 0x70),
    KBK_N("Z", KBD_z, 12, 4, 4, 0x29), KBK_N("X", KBD_x, 16, 4, 4, 0x2A), KBK_N("C", KBD_c, 20, 4, 4, 0x2B), KBK_N("V", KBD_v, 24, 4, 4, 0x2C), KBK_N("B", KBD_b, 28, 4, 4, 0x2D),
    KBK_N("N", KBD_n, 32, 4, 4, 0x2E), KBK_N("M", KBD_m, 36, 4, 4, 0x2F),
    KBK_N(",", KBD_comma, 40, 4, 4, 0x30), KBK_N(".", KBD_period, 44, 4, 4, 0x31), KBK_N("/", KBD_slash, 48, 4, 4, 0x32), KBK_N("_", KBD_jp_ro, 52, 4, 4, 0x33),
    KBK_M("SHIFT", KBD_rightshift, 56, 10, 4, 0x70),
    KBK_N("UP", KBD_up, 73, 6, 4, 0x3A),
    KBK_N("1", KBD_kp1, 86, 4, 4, 0x4A), KBK_N("2", KBD_kp2, 90, 4, 4, 0x4B), KBK_N("3", KBD_kp3, 94, 4, 4, 0x4C), KBK_N("=", KBD_kpequals, 98, 4, 4, 0x4D),
    /* ---- R5: かな GRPH NFER SPACE XFER | LEFT DOWN RIGHT | 패드 0 , . ENTER */
    KBK_T("KANA", KBD_kana, 0, 8, 5, 0x72), KBK_M("GRPH", KBD_leftalt, 8, 8, 5, 0x73), KBK_N("NFER", KBD_nfer, 16, 8, 5, 0x51),
    KBK_N("SPACE", KBD_space, 24, 34, 5, 0x34), KBK_N("XFER", KBD_xfer, 58, 8, 5, 0x35),
    KBK_N("LT", KBD_left, 67, 6, 5, 0x3B), KBK_N("DN", KBD_down, 73, 6, 5, 0x3D), KBK_N("RT", KBD_right, 79, 6, 5, 0x3C),
    KBK_N("0", KBD_kp0, 86, 4, 5, 0x4E), KBK_N(",", KBD_kpcomma, 90, 4, 5, 0x4F), KBK_N(".", KBD_kpperiod, 94, 4, 5, 0x50), KBK_N("EN", KBD_kpenter, 98, 4, 5, 0x1C),
};
static const int g_nkeys = (int)(sizeof(g_keys) / sizeof(g_keys[0]));
static_assert(sizeof(g_keys) / sizeof(g_keys[0]) <= 160, "kbov 상태 배열 크기(160)를 넘음");
static const int KB_UNIT_PX = 24;       /* 1 키 단위 = 24px 피치(간격 2px 포함, 그려지는 키 폭 = 피치 - 2) */
static const int KB_ROWS = 6;
static const int KB_GRID_W4 = 102;      /* 가장 긴 행의 끝 = 25.5u = 612px */
}
