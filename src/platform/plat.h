/* platform/plat.h -- S3 (module_plan v1.1): OS 의존 호출을 모은 얇은 층. 호출부는 이 헤더의 plat:: 함수만 쓴다.
 * 규칙: Windows 구현 = 예전 호출부와 같은 API 호출 그대로(동작 불변). 비 Windows 는 시험 도구(Linux)가 쓰는 만큼만 둔다. */
#pragma once
#include "plat_str.h"
#include "plat_fs.h"
#include "plat_proc.h"
#include "plat_time.h"
#include "plat_sync.h"
#include "plat_window.h"
