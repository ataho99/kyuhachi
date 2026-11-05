/* kh_ent.h -- t39: 실행 방법(entry) 편집 로직 (UI·호스트·하니스 공용, 화면과 무관한 순수 함수) */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>

namespace ent {
typedef std::map<std::string, std::string> Kv;

inline std::string lower(std::string s) { for (auto & c : s) c = (char)tolower((unsigned char)c); return s; }
inline std::string ext_of(const std::string & n) { size_t d = n.find_last_of('.'); return d == std::string::npos ? std::string() : lower(n.substr(d + 1)); }
inline bool is_hex40(const std::string & s) { if (s.size() != 40) return false; for (char c : s) if (!isxdigit((unsigned char)c)) return false; return true; }
inline std::string base_name(const std::string & p) { size_t d = p.find_last_of("\\/"); return d == std::string::npos ? p : p.substr(d + 1); }

/* 이미지 종류: 0 아님 · 1 FD · 2 HDD · 3 CD */
inline int img_kind(const std::string & name) {
    std::string e = ext_of(name);
    static const char * fd[] = { "hdm", "fdi", "d88", "d98", "fdd", "xdf", "dup", "2hd", "tfd", "nfd", "fim", "hd4", "dim", "dsk", "img", "bin_fd" };
    static const char * hd[] = { "hdi", "hdd", "nhd", "thd", "vhd", "hdn", "nvd", "sasi", "slh" };
    static const char * cd[] = { "iso", "cue", "ccd", "mds", "mdf", "chd" };
    for (auto x : fd) if (e == x) return 1; for (auto x : hd) if (e == x) return 2; for (auto x : cd) if (e == x) return 3;
    return 0;
}

/* ---- 템플릿 8유형 + 복제 */
struct Tpl { const char * id; const char * label; const char * sample; };
static const Tpl TPLS[8] = {
    { "T1", "HDD 단독", "동급생2·열락" }, { "T2", "HDD + FD 부착", "나디아" }, { "T3", "독립 FD 부팅", "대마도95·키키모라·바요엔" }, { "T4", "다장 + 교체", "A.R.S" },
    { "T5", "CD + 내장 DOS", "마도456" }, { "T6", "데모 먼저", "환세희담" }, { "T7", "원판/한글판 변형 (글꼴 지정)", "열락 jp 등" }, { "T8", "설치 필요 (HDD 생성)", "설치기 게임" } };
inline Kv tpl_kv(int t) {
    Kv k; k["tpl"] = TPLS[t].id;
    switch (t) {
    case 0: k["label"] = "HDD 부팅"; k["boot"] = "C"; break;
    case 1: k["label"] = "HDD 부팅 (FD 부착)"; k["boot"] = "C"; break;
    case 2: k["label"] = "FD 부팅"; k["boot"] = "A"; break;
    case 3: k["label"] = "다장 디스크"; k["boot"] = "A"; break;
    case 4: k["label"] = "CD 모드"; k["autoexec"] = "d:|"; break;
    case 5: k["label"] = "데모 먼저"; k["boot"] = "A"; break;
    case 6: k["label"] = "원판 일본어"; k["font_kind"] = "jp"; break;
    case 7: k["label"] = "설치 필요"; k["install"] = "hdd_new"; k["hdd_mb"] = "40"; k["boot"] = "A"; break;
    }
    return k;
}
/* 이미지가 하나뿐이면 알맞은 칸에 자동으로 채운다. files = 게임 폴더의 이미지 파일 이름들 */
inline void autofill(Kv & k, const std::vector<std::string> & files) {
    std::vector<std::string> fd, hd, cd; for (auto & f : files) { int t = img_kind(f); if (t == 1) fd.push_back(f); else if (t == 2) hd.push_back(f); else if (t == 3) cd.push_back(f); }
    std::string tpl = k.count("tpl") ? k["tpl"] : std::string();
    if ((tpl == "T1" || tpl == "T2") && hd.size() == 1 && !k.count("disk_c")) k["disk_c"] = hd[0];
    if (tpl == "T2" && fd.size() >= 1 && !k.count("disk_a")) k["disk_a"] = fd[0];
    if ((tpl == "T2" || tpl == "T3" || tpl == "T6") && fd.size() > 1 && !k.count("swap")) {
        std::string sw; for (auto & f : fd) { if (!sw.empty()) sw += ","; sw += f; } k["swap"] = sw; }
    if ((tpl == "T3" || tpl == "T4" || tpl == "T6" || tpl == "T7") && fd.size() >= 1 && !k.count("disk_a") && tpl != "T7") k["disk_a"] = fd[0];
    if (tpl == "T4") { if (fd.size() >= 2 && !k.count("disk_b")) k["disk_b"] = fd[1]; std::string sw; for (auto & f : fd) { if (!sw.empty()) sw += ","; sw += f; } if (fd.size() > 1) k["swap"] = sw; }
    if (tpl == "T5" && cd.size() == 1 && !k.count("cd")) k["cd"] = cd[0];
}

/* ---- 부팅 후 자동 입력 (bootkeys=<ms>:<키> , <ms>:type=<글자들>) */
struct Step { int ms = 3000; bool type = false; std::string val = "enter"; };
inline std::vector<Step> parse_steps(const std::string & s) {
    std::vector<Step> v; size_t p = 0;
    while (p < s.size()) {
        size_t e = s.find(',', p); if (e == std::string::npos) e = s.size(); std::string t = s.substr(p, e - p); p = e + 1;
        size_t c = t.find(':'); if (c == std::string::npos) continue;
        Step st; st.ms = atoi(t.substr(0, c).c_str()); std::string r = t.substr(c + 1);
        if (r.compare(0, 5, "type=") == 0) { st.type = true; st.val = r.substr(5); } else st.val = r;
        v.push_back(st);
    }
    return v;
}
inline std::string join_steps(const std::vector<Step> & v) {
    std::string o; for (auto & s : v) { if (!o.empty()) o += ","; o += std::to_string(s.ms) + ":" + (s.type ? "type=" : "") + s.val; } return o;
}
inline std::string show_val(const std::string & v) { std::string o; for (char c : v) { if (c == (char)126) o += "[Enter]"; else o += c; } return o; }
inline std::string step_text(const Step & s) {
    char b[48]; snprintf(b, sizeof b, "%d.%d\xEC\xB4\x88 \xED\x9B\x84", s.ms / 1000, (s.ms / 100) % 10);   /* "N.N초 후" */
    return std::string(b) + (s.type ? "  \xEC\x9E\x85\xEB\xA0\xA5  " : "  \xED\x82\xA4  ") + show_val(s.val);   /* 입력 / 키 */
}

/* ---- 검증 (S9). has(이름) = 그 이미지가 있는가(이름은 imgdir 기준 또는 절대 경로, sha1 은 호출 전 걸러짐) */
struct Issue { bool err; std::string msg; };
typedef std::function<bool(const std::string &)> HasFn;
inline std::vector<Issue> validate(const Kv & kv, const HasFn & has) {
    std::vector<Issue> r; auto g = [&](const char * k) -> std::string { auto it = kv.find(k); return it == kv.end() || it->second == "-" ? std::string() : it->second; };
    auto chk = [&](const char * lab, const std::string & v) { if (v.empty() || is_hex40(v)) return; if (!has(v)) r.push_back({ true, std::string("\xEC\x97\x86\xEB\x8A\x94 \xED\x8C\x8C\xEC\x9D\xBC(") + lab + "): " + base_name(v) }); };   /* 없는 파일(..) */
    const char * dn[4] = { "A", "B", "C", "D" }; const char * dk[4] = { "disk_a", "disk_b", "disk_c", "disk_d" };
    for (int i = 0; i < 4; i++) chk(dn[i], g(dk[i]));
    chk("CD", g("cd")); chk("\xEB\x8D\xB0\xEB\xAA\xA8", g("demo"));   /* 데모 */
    std::vector<std::string> sw; { std::string s = g("swap"); size_t p = 0; while (p <= s.size() && !s.empty()) { size_t e = s.find(',', p); if (e == std::string::npos) e = s.size(); if (e > p) sw.push_back(s.substr(p, e - p)); p = e + 1; } }
    for (auto & f : sw) chk("\xEA\xB5\x90\xEC\xB2\xB4", f);   /* 교체 */
    std::string b = g("boot"); if (b.size() == 1 && toupper((unsigned char)b[0]) >= 'A' && toupper((unsigned char)b[0]) <= 'D') {
        int i = toupper((unsigned char)b[0]) - 'A'; if (g(dk[i]).empty() && g("cd").empty()) r.push_back({ true, std::string("\xEB\xB6\x80\xED\x8C\x85 \xEB\x8C\x80\xEC\x83\x81 ") + dn[i] + " \xEC\xB9\xB8\xEC\x9D\xB4 \xEB\xB9\x84\xEC\x96\xB4 \xEC\x9E\x88\xEC\x9D\x8C" });   /* 부팅 대상 X 칸이 비어 있음 */
    }
    if (!g("cd").empty() && g("autoexec").size() <= 3) r.push_back({ false, "CD \xEB\xAA\xA8\xEB\x93\x9C\xEC\x9D\xB8\xEB\x8D\xB0 \xEC\x9E\x90\xEB\x8F\x99 \xEC\x8B\xA4\xED\x96\x89(autoexec)\xEC\x9D\xB4 \xEC\x97\x86\xEC\x9D\x8C" });   /* CD 모드인데 자동 실행(autoexec)이 없음 */
    { std::set<std::string> seen; for (int i = 0; i < 4; i++) { std::string v = lower(g(dk[i])); if (v.empty()) continue; if (!seen.insert(v).second) { r.push_back({ false, "\xEA\xB0\x99\xEC\x9D\x80 \xED\x8C\x8C\xEC\x9D\xBC\xEC\x9D\x84 \xEB\x91\x90 \xEC\xB9\xB8\xEC\x97\x90: " + base_name(g(dk[i])) }); break; } } }   /* 같은 파일을 두 칸에 */
    if (!sw.empty()) { std::set<std::string> in; for (auto & f : sw) in.insert(lower(f)); for (int i = 0; i < 2; i++) { std::string v = lower(g(dk[i])); if (!v.empty() && !in.count(v)) { r.push_back({ false, std::string("\xEA\xB5\x90\xEC\xB2\xB4 \xEB\xAA\xA9\xEB\xA1\x9D\xEC\x97\x90 \xEC\x97\x86\xEC\x9D\x8C: ") + base_name(g(dk[i])) }); break; } } }   /* 교체 목록에 없음 */
    if (g("install") == "hdd_new" && g("disk_c").empty()) r.push_back({ false, "\xEC\x84\xA4\xEC\xB9\x98 \xED\x95\x84\xEC\x9A\x94: HDD \xEB\xA5\xBC \xEB\xA7\x8C\xEB\x93\xA4\xEC\x96\xB4\xEC\x95\xBC \xED\x95\xA8" });   /* 설치 필요: HDD 를 만들어야 함 */
    if (g("disk_a").empty() && g("disk_b").empty() && g("disk_c").empty() && g("disk_d").empty() && g("cd").empty() && g("install").empty()) r.push_back({ false, "\xEB\x94\x94\xEC\x8A\xA4\xED\x81\xAC\xEA\xB0\x80 \xED\x95\x98\xEB\x82\x98\xEB\x8F\x84 \xEC\x97\x86\xEC\x9D\x8C(\xEA\xB8\xB0\xEB\xB3\xB8 \xEB\xB0\xB0\xEC\xB9\x98 \xEC\x82\xAC\xEC\x9A\xA9)" });   /* 디스크가 하나도 없음(기본 배치 사용) */
    return r;
}
inline bool has_error(const std::vector<Issue> & v) { for (auto & i : v) if (i.err) return true; return false; }

/* ---- 새 항목 이름: 비어 있지 않고 ':' ']' '[' 금지, 중복이면 번호 붙임 */
inline std::string clean_name(std::string n) {
    std::string o; for (char c : n) { if (c == ':' || c == ']' || c == '[' || c == '\n' || c == '\r' || c == '=') c = ' '; o += c; }
    while (!o.empty() && o.back() == ' ') o.pop_back(); size_t i = 0; while (i < o.size() && o[i] == ' ') i++; o = o.substr(i);
    if (o.size() > 60) { size_t cut = 60; while (cut > 0 && ((unsigned char)o[cut] & 0xC0) == 0x80) cut--; o = o.substr(0, cut); }
    return o;
}
inline std::string unique_name(const std::string & want, const std::vector<std::string> & used) {
    std::string b = clean_name(want); if (b.empty()) b = "\xEC\x83\x88 \xEC\x8B\xA4\xED\x96\x89 \xEB\xB0\xA9\xEB\xB2\x95";   /* 새 실행 방법 */
    auto has = [&](const std::string & n) { for (auto & u : used) if (u == n) return true; return false; };
    if (!has(b)) return b; for (int i = 2; i < 999; i++) { std::string c = b + " " + std::to_string(i); if (!has(c)) return c; } return b;
}
} // namespace ent
