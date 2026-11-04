/* platform/plat_time.h -- 시계 (S3). */
#pragma once
#include <stdint.h>
#if !defined(_WIN32)
#include <time.h>
#include <unistd.h>
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace plat {
struct LocalTime { int year, month, day, hour, minute, second, ms; };
#if defined(_WIN32)
inline LocalTime local_now() { SYSTEMTIME st; GetLocalTime(&st); LocalTime t = { (int)st.wYear, (int)st.wMonth, (int)st.wDay, (int)st.wHour, (int)st.wMinute, (int)st.wSecond, (int)st.wMilliseconds }; return t; }
/* 파일 시각(FILETIME 100ns 값) → 지역 시각 */
inline LocalTime local_from_filetime(uint64_t ft) {
    FILETIME f, l; SYSTEMTIME st; f.dwHighDateTime = (DWORD)(ft >> 32); f.dwLowDateTime = (DWORD)(ft & 0xFFFFFFFFu);
    FileTimeToLocalFileTime(&f, &l); FileTimeToSystemTime(&l, &st); LocalTime t = { (int)st.wYear, (int)st.wMonth, (int)st.wDay, (int)st.wHour, (int)st.wMinute, (int)st.wSecond, (int)st.wMilliseconds }; return t;
}
inline void sleep_ms(unsigned ms) { Sleep(ms); }
#else
/* S9e-N 4: POSIX bodies */
inline LocalTime local_now() {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); time_t s = ts.tv_sec; struct tm t; localtime_r(&s, &t);
    LocalTime r = { t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, (int)(ts.tv_nsec / 1000000) }; return r;
}
inline LocalTime local_from_filetime(uint64_t ft) {
    time_t s = (time_t)(ft / 10000000ULL - 11644473600ULL); struct tm t; localtime_r(&s, &t);
    LocalTime r = { t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 0 }; return r;
}
inline void sleep_ms(unsigned ms) { usleep(ms * 1000u); }
#endif
}
