/* platform/plat_str.h -- 문자열 코드 변환·메시지 창 (S3). Windows 구현은 gensei_addwork.h / gensei_host.cpp 에 있던 본문을 옮긴 것. */
#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace plat {
#if defined(_WIN32)
inline std::string conv(const std::string & s, UINT from, UINT to) {
    if (s.empty()) return s; int n = MultiByteToWideChar(from, 0, s.c_str(), (int)s.size(), NULL, 0); if (n <= 0) return s;
    std::wstring w((size_t)n, L'\0'); MultiByteToWideChar(from, 0, s.c_str(), (int)s.size(), &w[0], n);
    int m = WideCharToMultiByte(to, 0, w.c_str(), n, NULL, 0, NULL, NULL); if (m <= 0) return s;
    std::string o((size_t)m, '\0'); WideCharToMultiByte(to, 0, w.c_str(), n, &o[0], m, NULL, NULL); return o;
}
inline std::string acp_to_u8(const std::string & s) { return conv(s, CP_ACP, CP_UTF8); }
/* UTF-8 → 시스템(ANSI) 문자열. 현재 코드 페이지에 없는 글자가 있으면(손실) 짧은 이름(8.3, ASCII)으로 바꾼다. */
inline std::string u8_to_acp(const std::string & s) {
    if (s.empty()) return s; int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0); if (n <= 0) return s;
    std::wstring w((size_t)n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    BOOL lossy = FALSE; int m = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), n, NULL, 0, NULL, &lossy); if (m <= 0) return s;
    std::string o((size_t)m, '\0'); lossy = FALSE; WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), n, &o[0], m, NULL, &lossy);
    if (!lossy) return o;
    wchar_t sp[1024]; DWORD k = GetShortPathNameW(w.c_str(), sp, 1024);
    if (k > 0 && k < 1024) { int q = WideCharToMultiByte(CP_ACP, 0, sp, (int)k, NULL, 0, NULL, NULL); if (q > 0) { std::string r((size_t)q, '\0'); WideCharToMultiByte(CP_ACP, 0, sp, (int)k, &r[0], q, NULL, NULL); return r; } }
    return o;
}
/* UTF-8 → UTF-16 (널 포함 길이로 만들지 않는다). 실패하면 false */
inline bool utf8_to_wide(const std::string & u, std::wstring & w) {
    int wl = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, NULL, 0); if (wl <= 0) return false;
    w.assign((size_t)wl, L'\0'); MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, &w[0], wl); return true;
}
/* ACP 바이트열(길이 n) → UTF-8. 변환 실패면 false (예전 gh_mixed_u8 와 같은 규칙: 실패하면 아무것도 덧붙이지 않는다) */
inline bool acp_bytes_to_u8(const char * s, int n, std::string & out) {
    int wl = MultiByteToWideChar(CP_ACP, 0, s, n, NULL, 0); if (wl <= 0) return false;
    std::wstring w((size_t)wl, L'\0'); MultiByteToWideChar(CP_ACP, 0, s, n, &w[0], wl);
    int ul = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wl, NULL, 0, NULL, NULL); if (ul <= 0) return false;
    out.assign((size_t)ul, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wl, &out[0], ul, NULL, NULL); return true;
}
inline bool wide_to_utf8(const wchar_t * w, std::string & u) {
    int ul = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL); if (ul <= 0) return false;
    u.assign((size_t)ul, '\0'); WideCharToMultiByte(CP_UTF8, 0, w, -1, &u[0], ul, NULL, NULL); u.resize((size_t)ul - 1); return true;
}
/* UTF-8 경로를 유니코드 API 로 연다(ACP 변환을 거치지 않음 — 한글·일본어 폴더에서도 열린다). 실패하면 NULL */
inline FILE * fopen_u8_rb(const std::string & utf8path) {
    std::wstring wp; if (!utf8_to_wide(utf8path, wp)) return NULL; return _wfopen(wp.c_str(), L"rb");
}
/* 글자 하나를 코드 페이지(932/949 …)의 바이트로. 돌려주는 값 = 바이트 수, bad = 대체 글자가 쓰였는지 */
inline int wchar_to_cp(unsigned codepage, wchar_t wc, char * mb, int cap, bool * bad) {
    BOOL b = FALSE; int n = WideCharToMultiByte(codepage, 0, &wc, 1, mb, cap, NULL, &b); if (bad) *bad = b != FALSE; return n;
}
/* 메시지 창(UTF-8 본문). icon 0 = 안내, 1 = 오류. cap = 변환 칸 수(넘으면 창을 띄우지 않는다 — 예전 동작) */
inline void msgbox_utf8(const char * text, int icon, int cap) {
    std::vector<wchar_t> w((size_t)cap); int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, &w[0], cap); if (n <= 0) return;
    MessageBoxW(NULL, &w[0], L"Kyuhachi - PC-98 BOOTLOADER", MB_OK | (icon ? MB_ICONERROR : MB_ICONINFORMATION) | MB_TOPMOST | MB_SETFOREGROUND);
}
#else
inline std::string acp_to_u8(const std::string & s) { return s; }
inline std::string u8_to_acp(const std::string & s) { return s; }
#endif
}
