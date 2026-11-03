/* platform/plat_os.h -- S9e-ㄴ 1단계: OS 헤더 포함을 한 곳에 모음. 호출부(gensei_host.cpp)는 windows.h 를 직접 포함하지 않는다.
 * 규칙: Windows = 예전과 같은 <windows.h> (NOMINMAX 는 호출부가 먼저 정의). 비 Windows = 아무것도 포함하지 않음. */
#pragma once
#if defined(WIN32)
# include <windows.h>
#endif
