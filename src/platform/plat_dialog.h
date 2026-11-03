/* platform/plat_dialog.h -- S9e-N 3: native window handle, system dialogs, theme-change event, drive roots.
 * Windows bodies are the former calls from app/ui_env.h unchanged. Non-Windows: no dialog, no handle. */
#pragma once
#include <string>
#include <vector>
#include <cstring>
#include "plat_window.h"
struct SDL_Window;
union SDL_Event;
#if defined(_WIN32)
#include <SDL_syswm.h>
#include <shlobj.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
namespace plat {
inline native_win window_native(SDL_Window * w) { SDL_SysWMinfo wi; SDL_VERSION(&wi.version); if (w && SDL_GetWindowWMInfo(w, &wi)) return wi.info.win.window; return NULL; }
/* true when the event is WM_SETTINGCHANGE "ImmersiveColorSet" (apps theme switched) */
inline bool is_theme_change_event(const SDL_Event * e) {
    if (e->type != SDL_SYSWMEVENT) return false;
    const SDL_SysWMmsg * w = e->syswm.msg;
    if (w && w->subsystem == SDL_SYSWM_WINDOWS && w->msg.win.msg == WM_SETTINGCHANGE && w->msg.win.lParam) {
        const char * a = (const char *)w->msg.win.lParam; const wchar_t * u = (const wchar_t *)w->msg.win.lParam;
        if (strcmp(a, "ImmersiveColorSet") == 0 || (a[0] == 'I' && a[1] == 0 && wcscmp(u, L"ImmersiveColorSet") == 0)) return true;
    }
    return false;
}
/* open-file dialog (multi select). filter = ANSI NUL-separated pairs, ending with two NULs. parts = [folder-or-single-file, name, name...] */
inline bool pick_files_a(native_win owner, const std::string & title, const std::string & filter, std::vector<std::string> & parts) {
    static char buf[1 << 16]; memset(buf, 0, sizeof buf); OPENFILENAMEA o; memset(&o, 0, sizeof o); o.lStructSize = sizeof o; o.hwndOwner = owner;
    o.lpstrFilter = filter.c_str(); o.lpstrFile = buf; o.nMaxFile = sizeof buf; o.lpstrTitle = title.c_str(); o.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&o)) return false;
    parts.clear(); const char * p = buf;
    for (;;) { std::string s = p; parts.push_back(s); p += s.size() + 1; if (!*p) break; }
    return true;
}
inline bool pick_folder_a(native_win owner, const std::string & title, std::string & out) {
    static bool com = false; if (!com) { com = true; CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); }
    char disp[MAX_PATH]; BROWSEINFOA b; memset(&b, 0, sizeof b); b.hwndOwner = owner; b.pszDisplayName = disp; b.lpszTitle = title.c_str(); b.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST id = SHBrowseForFolderA(&b); if (!id) return false;
    char path[MAX_PATH]; bool ok = SHGetPathFromIDListA(id, path) != 0; CoTaskMemFree(id); if (ok) out = path; return ok;
}
}
#else
namespace plat {
inline native_win window_native(SDL_Window *) { return NULL; }
inline bool is_theme_change_event(const SDL_Event *) { return false; }
inline bool pick_files_a(native_win, const std::string &, const std::string &, std::vector<std::string> &) { return false; }
inline bool pick_folder_a(native_win, const std::string &, std::string &) { return false; }
}
#endif
