/* platform/plat_proc.h -- 프로세스 (S3). Windows 구현 = 예전 호출부의 GetModuleFileNameA / CreateProcessA 그대로. */
#pragma once
#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
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
/* S9e-N 5: process helpers moved out of app code (same calls, same results) */
inline void putenv_a(const char * s) { _putenv(s); }
inline unsigned long long working_set_bytes() {   /* K32GetProcessMemoryInfo looked up at run time, no psapi link */
    struct PMC { DWORD cb, PageFaultCount; SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage; };
    typedef BOOL (WINAPI * Fn)(HANDLE, PMC *, DWORD); static Fn fn = (Fn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetProcessMemoryInfo");
    PMC m; memset(&m, 0, sizeof m); m.cb = sizeof m; if (fn && fn(GetCurrentProcess(), &m, sizeof m)) return (unsigned long long)m.WorkingSetSize; return 0ull;
}
inline unsigned long process_id() { return (unsigned long)GetCurrentProcessId(); }
inline unsigned long handle_count() { DWORD hc = 0; GetProcessHandleCount(GetCurrentProcess(), &hc); return (unsigned long)hc; }
}
#else
#if defined(__ANDROID__)
extern "C" bool kha_relaunch_c(const char * cmd);
#endif
#include <unistd.h>
namespace plat {
/* S9e-N 4: POSIX bodies. On Android /proc/self/exe is the zygote helper; the Android main sets the app folder itself. */
inline std::string exe_path() { char b[4096]; ssize_t n = readlink("/proc/self/exe", b, sizeof b - 1); if (n <= 0) return std::string(); return std::string(b, (size_t)n); }
inline bool beside_exe_u8(const wchar_t * name, std::string & out) {
    std::string p = exe_path(); if (p.empty()) return false; size_t sl = p.find_last_of('/'); p = (sl == std::string::npos) ? std::string() : p.substr(0, sl + 1);
    for (const wchar_t * w = name; *w; ++w) { if (*w >= 0x80) return false; p.push_back((char)*w); }
    out = p; return true;
}
#if defined(__ANDROID__)
inline bool spawn(const std::string &, const std::string & cmd, const std::string &, unsigned long * err) { if (kha_relaunch_c(cmd.c_str())) return true; if (err) *err = 38; return false; }   /* t145: 안드로이드 = 앱 프로세스 재시작 */
#else
inline bool spawn(const std::string &, const std::string &, const std::string &, unsigned long * err) { if (err) *err = 38; return false; }
#endif   /* ENOSYS: no second process on this platform */
/* S9e-N 5: POSIX bodies of the process helpers */
inline void putenv_a(const char * s) { putenv(strdup(s)); }
inline unsigned long long working_set_bytes() {
    FILE * f = fopen("/proc/self/statm", "r"); if (!f) return 0ull;
    unsigned long long a = 0, r = 0; int n = fscanf(f, "%llu %llu", &a, &r); fclose(f); return n == 2 ? r * 4096ull : 0ull;
}
inline unsigned long process_id() { return (unsigned long)getpid(); }
inline unsigned long handle_count() { return 0ul; }
}
#endif
