/* gensei_padnames_t48e.h -- t48e (#646/#651): 키 이름 표 확장. kbtable 의 모든 라벨을 저장용 이름(소문자 ASCII)으로 바꾸고 되찾는다.
 * 옛 이름(rollup/pageup/rolldown/pagedown 포함)은 호스트의 gh_keyname 이 먼저 처리해 지금과 똑같이 동작한다 — 여기는 옛 이름에 없는 것만 맡는다.
 * 새 이름: rlup = ROLL UP(0x36, KBD_pagedown) · rldn = ROLL DOWN(0x37, KBD_pageup). */
#pragma once
#include <string>
#include <vector>
#include <set>
#include <ctype.h>
#include "keyboard.h"
#include "gensei_kbtable.h"

namespace pn {
struct E { std::string name; int idx; };
inline std::string lower(std::string s) { for (auto & c : s) c = (char)tolower((unsigned char)c); return s; }
inline std::string upper(std::string s) { for (auto & c : s) c = (char)toupper((unsigned char)c); return s; }
inline std::string sym(const std::string & l, bool kp) {
    if (l == "\\") return "yen"; if (l == "-") return "minus"; if (l == "^") return "caret"; if (l == "@") return "at"; if (l == ";") return "semicolon"; if (l == ":") return "colon";
    if (l == ",") return "comma"; if (l == ".") return "period"; if (l == "/") return "slash"; if (l == "_") return "underscore"; if (l == "[") return "lbracket"; if (l == "]") return "rbracket";
    if (kp) { if (l == "*") return "star"; if (l == "+") return "plus"; if (l == "=") return "equals"; if (l == "EN") return "enter"; }
    return lower(l);
}
inline const std::vector<E> & table() {
    static std::vector<E> v; static bool done = false;
    if (done) return v; done = true; std::set<std::string> used;
    for (int i = 0; i < kbt::g_nkeys; i++) {
        const kbt::Key & k = kbt::g_keys[i]; bool kp = k.x4 >= 86 && k.row >= 1 && k.row <= 5 && !(k.row == 0);
        std::string n = sym(k.label, kp); if (kp) n = "kp" + n;
        if (used.count(n)) n = "r" + n;
        while (used.count(n)) n += "2";
        used.insert(n); E e; e.name = n; e.idx = i; v.push_back(e);
    }
    return v;
}
/* 이름 → 키 (ibm = pc98_force_ibm_layout 플래그: [ ] 두 키만 달라진다). 못 찾으면 KBD_NONE */
inline KBD_KEYS lookup(const std::string & s0, bool ibm) {
    std::string s = lower(s0);
    for (auto & e : table()) if (e.name == s) { const kbt::Key & k = kbt::g_keys[e.idx]; return (KBD_KEYS)((ibm && k.kbd_ibm >= 0) ? k.kbd_ibm : k.kbd); }
    if (s == "ret" || s == "enter" || s == "return") return KBD_enter;
    return KBD_NONE;
}
/* 고르기 목록: 자주 쓰는 것 먼저, 그다음 표 순서 */
inline std::vector<std::string> picker_names() {
    static const char * const PRI[] = { "stop", "copy", "grph", "nfer", "xfer", "help", "space", "ret", "esc", "tab", "bs", "del", "ins", "home", "ctrl", "shift", "caps", "kana", "rlup", "rldn", "up", "dn", "lt", "rt" };
    std::vector<std::string> r; std::set<std::string> done;
    for (const char * p : PRI) for (auto & e : table()) if (e.name == p) { r.push_back(p); done.insert(p); }
    for (auto & e : table()) if (!done.count(e.name)) r.push_back(e.name);
    return r;
}
} // namespace pn
