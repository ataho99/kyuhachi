/* platform/plat_window.h -- 창·테마 (S3). 제목표시줄 어둡게/밝게(DWM)와 앱 테마 읽기. Windows 구현 = 예전 gensei_host.cpp 의 호출 그대로. */
#pragma once
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
namespace plat {
/* HKCU\...\Personalize\AppsUseLightTheme == 1 이면 true, 그 밖(값 없음·0·읽기 실패)은 false */
inline bool apps_use_light_theme() {
    HKEY k; DWORD v = 0, sz = sizeof v, ty = 0; bool light = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, L"AppsUseLightTheme", NULL, &ty, (LPBYTE)&v, &sz) == ERROR_SUCCESS && ty == REG_DWORD && v == 1) light = true;
        RegCloseKey(k);
    }
    return light;
}
/* DwmSetWindowAttribute 20 먼저, 실패하면 19. 돌려주는 값: 0 성공(attr 에 쓴 번호) · 1 함수 없음 · 2 실패(hr) */
inline int dwm_dark_titlebar(HWND hw, bool dark, unsigned * attr, long * hr_out) {
    HMODULE m = LoadLibraryW(L"dwmapi.dll");
    typedef HRESULT (WINAPI * PF)(HWND, DWORD, LPCVOID, DWORD);
    PF f = m ? (PF)GetProcAddress(m, "DwmSetWindowAttribute") : NULL;
    if (!f) return 1;
    BOOL b = dark ? TRUE : FALSE; DWORD at = 20; HRESULT r = f(hw, 20, &b, sizeof b);
    if (FAILED(r)) { at = 19; r = f(hw, 19, &b, sizeof b); }
    if (hr_out) *hr_out = (long)r; if (attr) *attr = (unsigned)at;
    return FAILED(r) ? 2 : 0;
}
/* 틀을 바로 다시 그리게 한다 */
inline void redraw_frame(HWND hw) { SetWindowPos(hw, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED); }
/* 제목이 title 인 최상위 창을 찾아 닫기 신호를 보낸다(E_Exit 상자 감시용). 찾았으면 true */
inline bool close_window_titled(const char * title) {
    HWND h = FindWindowA(NULL, title); if (!h) return false;
    PostMessage(h, WM_CLOSE, 0, 0); PostMessage(h, WM_COMMAND, IDOK, 0); return true;
}
}
#endif
