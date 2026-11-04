/* platform/plat_proc.h -- 프로세스 (S3). Windows 구현 = 예전 호출부의 GetModuleFileNameA / CreateProcessA 그대로. */
#pragma once
#include <string>
#include <vector>
#include <string.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
namespace plat {
inline std::string exe_path() { char mp[MAX_PATH]; DWORD n = GetModuleFileNameA(NULL, mp, MAX_PATH); return std::string(mp, n); }
/* exe 를 cmdline(따옴표 포함 전체 명령줄)으로 workdir 에서 새로 띄운다. 실패하면 false, err 에 GetLastError */
/* exe 와 같은 폴더의 name 파일 경로를 UTF-8 로(유니코드 API 로 얻음). 실패하면 false */
inline bool beside_exe_u8(const wchar_t * name, std::string & out) {
    wchar_t mp[1024]; DWORD n = GetModuleFileNameW(NULL, mp, 1024); if (n == 0 || n >= 1024) return false;
    std::wstring p(mp, n); size_t sl = p.find_last_of(L"\\/"); p = (sl == std::wstring::npos ? std::wstring() : p.substr(0, sl + 1)) + name;
    int ul = WideCharToMultiByte(CP_UTF8, 0, p.c_str(), -1, NULL, 0, NULL, NULL); if (ul <= 0) return false;
    out.assign((size_t)ul, '\0'); WideCharToMultiByte(CP_UTF8, 0, p.c_str(), -1, &out[0], ul, NULL, NULL); out.resize((size_t)ul - 1); return true;
}
inline bool spawn(const std::string & exe, const std::string & cmdline, const std::string & workdir, unsigned long * err) {
    STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si; PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    std::vector<char> cb(cmdline.begin(), cmdline.end()); cb.push_back(0);
    if (CreateProcessA(exe.c_str(), cb.data(), NULL, NULL, FALSE, 0, NULL, workdir.c_str(), &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return true; }
    if (err) *err = (unsigned long)GetLastError(); return false;
}
}
#else
#include <unistd.h>
namespace plat {
/* S9e-N 4: POSIX bodies. On Android /proc/self/exe is the zygote helper; the Android main sets the app folder itself. */
inline std::string exe_path() { char b[4096]; ssize_t n = readlink("/proc/self/exe", b, sizeof b - 1); if (n <= 0) return std::string(); return std::string(b, (size_t)n); }
inline bool beside_exe_u8(const wchar_t * name, std::string & out) {
    std::string p = exe_path(); if (p.empty()) return false; size_t sl = p.find_last_of('/'); p = (sl == std::string::npos) ? std::string() : p.substr(0, sl + 1);
    for (const wchar_t * w = name; *w; ++w) { if (*w >= 0x80) return false; p.push_back((char)*w); }
    out = p; return true;
}
inline bool spawn(const std::string &, const std::string &, const std::string &, unsigned long * err) { if (err) *err = 38; return false; }   /* ENOSYS: no second process on this platform */
}
#endif
