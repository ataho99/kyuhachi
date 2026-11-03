/* platform/plat_sync.h -- 잠금 (S3). Windows = CRITICAL_SECTION (SRWLOCK 은 구형 도구 모음이 몰라서 그대로), 그 밖 = std::recursive_mutex. */
#pragma once
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
namespace plat {
struct Lock { CRITICAL_SECTION cs; Lock() { InitializeCriticalSection(&cs); } void lock() { EnterCriticalSection(&cs); } void unlock() { LeaveCriticalSection(&cs); } };
}
#else
#include <mutex>
namespace plat {
struct Lock { std::recursive_mutex m; void lock() { m.lock(); } void unlock() { m.unlock(); } };
}
#endif
