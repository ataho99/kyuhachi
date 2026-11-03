#pragma once
/* S6: plain input data types moved out of gensei_host.cpp (code unchanged). parsers and gh_* glue stay in the host until S9. */
#include <string>
#include <vector>
#include "keyboard.h"

struct GKey { unsigned int at_ms; KBD_KEYS key; int step = 0; std::string lab; };   /* t47a: step = 편집 화면 단계 번호(1 부터), lab = OSD 에 보일 이름 */
struct GInj { unsigned at; std::vector<std::string> a; };
