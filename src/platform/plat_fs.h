/* platform/plat_fs.h -- 파일·폴더 (S3). 경로는 호출부가 쓰던 「시스템 문자열(ANSI)」 그대로 받는다. Windows 구현 = 같은 API 호출. */
#pragma once
#include <string>
#include <stdint.h>
#include <functional>
#include <vector>
#include <wchar.h>
#include <stdio.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <dirent.h>
#include <unistd.h>
#include <fnmatch.h>
#endif
#if defined(_WIN32) && defined(PLAT_NARROW)
/* PLAT_NARROW: windows.h 를 끌어오지 않는 번역 단위(pc98_savestate.cpp, DOSBox 헤더와 충돌 회피)용 — kernel32 를 직접 선언. 좁은 문자열 API 라 fopen 과 경로 규약이 같다. */
extern "C" {
__declspec(dllimport) int __stdcall CopyFileA(const char *, const char *, int);
__declspec(dllimport) int __stdcall MoveFileExA(const char *, const char *, unsigned long);
__declspec(dllimport) int __stdcall DeleteFileA(const char *);
}
namespace plat {
inline bool remove_file(const std::string & p) { return DeleteFileA(p.c_str()) != 0; }
inline bool copy_file(const std::string & a, const std::string & b, bool fail_if_exists) { return CopyFileA(a.c_str(), b.c_str(), fail_if_exists ? 1 : 0) != 0; }
inline bool replace_file(const std::string & tmp, const std::string & dst) { return MoveFileExA(tmp.c_str(), dst.c_str(), 1 /*REPLACE_EXISTING*/ | 8 /*WRITE_THROUGH*/) != 0; }
}
#else
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace plat {
inline char sep() {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}
/* 64비트 파일 위치(stdio) */
inline int seek64(FILE * f, uint64_t off) {
#if defined(_WIN32)
    return _fseeki64(f, (long long)off, SEEK_SET);
#else
    return fseeko(f, (off_t)off, SEEK_SET);
#endif
}
inline uint64_t tell64(FILE * f) {
#if defined(_WIN32)
    return (uint64_t)_ftelli64(f);
#else
    return (uint64_t)ftello(f);
#endif
}
struct FileInfo { uint32_t size_hi, size_lo; uint64_t mtime_ft; bool is_dir; };
struct DirEnt { std::string name; bool is_dir; uint32_t size_hi, size_lo; };
#if defined(_WIN32)
inline bool exists(const std::string & p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
inline bool exists(const char * p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
inline void mkdir(const std::string & p) { CreateDirectoryA(p.c_str(), NULL); }
inline bool make_dir(const std::string & p) { return CreateDirectoryA(p.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS; }
inline bool remove_file(const std::string & p) { return DeleteFileA(p.c_str()) != 0; }
inline bool remove_dir(const std::string & p) { return RemoveDirectoryA(p.c_str()) != 0; }
inline bool move_file(const std::string & a, const std::string & b) { return MoveFileA(a.c_str(), b.c_str()) != 0; }
inline bool copy_file(const std::string & a, const std::string & b, bool fail_if_exists) { return CopyFileA(a.c_str(), b.c_str(), fail_if_exists ? 1 : 0) != 0; }
inline bool replace_file(const std::string & tmp, const std::string & dst) { return MoveFileExA(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0; }   /* 1|8 */
inline bool set_cwd(const std::string & p) { return SetCurrentDirectoryA(p.c_str()) != 0; }
inline bool stat_file(const std::string & p, FileInfo & fi) {
    WIN32_FILE_ATTRIBUTE_DATA a; if (!GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &a)) return false;
    fi.size_hi = a.nFileSizeHigh; fi.size_lo = a.nFileSizeLow; fi.is_dir = (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    fi.mtime_ft = ((uint64_t)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime; return true;
}
/* FindFirstFileA 패턴("폴더\*" · "폴더\*.bmp" · 파일 하나)에 맞는 항목을 돌며 cb 를 부른다. cb 가 false 면 멈춘다. 처음부터 없으면 false */
inline bool find_a(const std::string & pattern, const std::function<bool(const DirEnt &)> & cb) {
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern.c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return false;
    do { DirEnt e; e.name = fd.cFileName; e.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; e.size_hi = fd.nFileSizeHigh; e.size_lo = fd.nFileSizeLow; if (!cb(e)) break; } while (FindNextFileA(h, &fd));
    FindClose(h); return true;
}

/* 폴더 훑기(작품 추가용): 와이드(UTF-16) API 로 훑고, 현재 코드 페이지에 없는 글자가 든 이름은 짧은 이름(8.3)으로 돌려준다(그대로 열 수 있게). 파일 → files, 폴더 → subs */
inline void list_dir_acp(const std::string & dir, std::vector<std::string> & files, std::vector<std::string> * subs) {
    std::wstring wd; { int n = MultiByteToWideChar(CP_ACP, 0, dir.c_str(), (int)dir.size(), NULL, 0); wd.assign((size_t)(n > 0 ? n : 0), L'\0'); if (n > 0) MultiByteToWideChar(CP_ACP, 0, dir.c_str(), (int)dir.size(), &wd[0], n); }
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((wd + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return;
    auto toacp = [](const wchar_t * w, bool & lossy) -> std::string {
        int m = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w, -1, NULL, 0, NULL, NULL); if (m <= 0) { lossy = true; return std::string(); }
        std::string o((size_t)m, '\0'); BOOL l = FALSE; WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w, -1, &o[0], m, NULL, &l); o.resize((size_t)m - 1); lossy = l != FALSE; return o; };
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        bool lossy = false; std::string n = toacp(fd.cFileName, lossy);
        if (lossy && fd.cAlternateFileName[0]) { bool l2 = false; std::string a = toacp(fd.cAlternateFileName, l2); if (!a.empty()) n = a; }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (subs) subs->push_back(n); } else files.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
#else
inline bool dir_exists(const std::string & p) { struct stat st; return stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFDIR); }
inline bool make_dir(const std::string & p) { return mkdir(p.c_str(), 0755) == 0 || dir_exists(p); }
inline void list_dir_acp(const std::string & dir, std::vector<std::string> & files, std::vector<std::string> * subs) {
    DIR * d = opendir(dir.c_str()); if (!d) return; struct dirent * e;
    while ((e = readdir(d))) { std::string n = e->d_name; if (n == "." || n == "..") continue; struct stat st; if (stat((dir + "/" + n).c_str(), &st) != 0) continue; if (S_ISDIR(st.st_mode)) { if (subs) subs->push_back(n); } else files.push_back(n); }
    closedir(d);
}
/* S9e-N 4: POSIX bodies of the calls the app layer makes. Same names, same results as the Windows bodies above. */
inline bool exists(const std::string & p) { struct stat st; return stat(p.c_str(), &st) == 0; }
inline bool exists(const char * p) { struct stat st; return stat(p, &st) == 0; }
inline void mkdir(const std::string & p) { ::mkdir(p.c_str(), 0755); }
inline bool remove_file(const std::string & p) { return unlink(p.c_str()) == 0; }
inline bool remove_dir(const std::string & p) { return rmdir(p.c_str()) == 0; }
inline bool move_file(const std::string & a, const std::string & b) { return rename(a.c_str(), b.c_str()) == 0; }
inline bool replace_file(const std::string & tmp, const std::string & dst) { return rename(tmp.c_str(), dst.c_str()) == 0; }
inline bool set_cwd(const std::string & p) { return chdir(p.c_str()) == 0; }
inline bool copy_file(const std::string & a, const std::string & b, bool fail_if_exists) {
    if (fail_if_exists && exists(b)) return false;
    FILE * in = fopen(a.c_str(), "rb"); if (!in) return false; FILE * out = fopen(b.c_str(), "wb"); if (!out) { fclose(in); return false; }
    char buf[65536]; size_t n; bool ok = true; while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
    fclose(in); if (fclose(out) != 0) ok = false; return ok;
}
inline bool stat_file(const std::string & p, FileInfo & fi) {
    struct stat st; if (stat(p.c_str(), &st) != 0) return false;
    fi.size_hi = (uint32_t)((uint64_t)st.st_size >> 32); fi.size_lo = (uint32_t)((uint64_t)st.st_size & 0xFFFFFFFFu); fi.is_dir = S_ISDIR(st.st_mode) != 0;
    fi.mtime_ft = ((uint64_t)st.st_mtime + 11644473600ULL) * 10000000ULL; return true;
}
/* pattern = "dir/*" , "dir/*.ext" or one file (Windows FindFirstFile rules; "*.*" matches every name) */
inline bool find_a(const std::string & pattern, const std::function<bool(const DirEnt &)> & cb) {
    size_t sl = pattern.find_last_of("\\/"); std::string dir = (sl == std::string::npos) ? std::string(".") : pattern.substr(0, sl); std::string pat = (sl == std::string::npos) ? pattern : pattern.substr(sl + 1);
    if (dir.empty()) dir = "/";
    if (pat == "*.*") pat = "*";
    if (pat.find_first_of("*?") == std::string::npos) {
        struct stat st; if (stat(pattern.c_str(), &st) != 0) return false;
        DirEnt e; e.name = pat; e.is_dir = S_ISDIR(st.st_mode) != 0; e.size_hi = (uint32_t)((uint64_t)st.st_size >> 32); e.size_lo = (uint32_t)((uint64_t)st.st_size & 0xFFFFFFFFu); cb(e); return true;
    }
    DIR * d = opendir(dir.c_str()); if (!d) return false; struct dirent * de;
    while ((de = readdir(d))) {
        if (fnmatch(pat.c_str(), de->d_name, 0) != 0) continue;
        struct stat st; if (stat((dir + "/" + de->d_name).c_str(), &st) != 0) continue;
        DirEnt e; e.name = de->d_name; e.is_dir = S_ISDIR(st.st_mode) != 0; e.size_hi = (uint32_t)((uint64_t)st.st_size >> 32); e.size_lo = (uint32_t)((uint64_t)st.st_size & 0xFFFFFFFFu);
        if (!cb(e)) break;
    }
    closedir(d); return true;
}
#endif
#if defined(_WIN32)
inline std::vector<std::string> drive_roots() {
    std::vector<std::string> r; DWORD m = GetLogicalDrives(); for (int i = 0; i < 26; i++) if (m & (1u << i)) r.push_back(std::string(1, (char)('A' + i)) + ":\\"); return r;
}
#else
inline std::vector<std::string> drive_roots() { return std::vector<std::string>(1, "/"); }
#endif
}
#endif
