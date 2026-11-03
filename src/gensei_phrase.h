/* gensei_phrase.h -- t52a (지시 #49) 화면 문구 외부 파일 · t52g (지시 #133) UI 언어 3종(한국어·English·にほんご).
 * 소스의 화면용 문구는 phr_("원문") 으로 감싼다. exe 옆 문구 파일에 「원문=바꿀문구」 줄이 있으면 그 문구를 쓴다.
 *   한국어 = strings_ko.txt 를 읽고 그 위에 strings.txt(사용자 덮어쓰기)를 읽는다 · English = strings_en.txt · にほんご = strings_ja.txt
 * 파일이 없거나 줄이 깨졌거나 키가 없으면 원문(내장 기본값)을 그대로 쓴다 — 어떤 경우에도 실행이 멈추지 않는다.
 * t52g: phr_ 가 돌려주는 포인터는 문구마다 고정된 칸(슬롯)이라, 언어를 바꾸면 칸의 내용만 바뀐다 →
 *       `static const char * n[] = { phr_("…") }` 처럼 처음 한 번 받아 둔 포인터도 다시 시작 없이 새 언어로 보인다.
 * 줄 형식: `#` 시작 = 주석 · 키와 값은 첫 번째 `=` 로 나눔(키 안의 `=` 는 `\=`) · `\n` 줄바꿈 · `\\` 역슬래시 · 키 첫 글자 `#` 는 `\#` */
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <windows.h>

static std::map<std::string, std::string> & phr_tbl() { static std::map<std::string, std::string> m; return m; }   /* 지금 언어의 「원문 → 바꿀문구」 */
struct PhrSlot { char * buf; size_t cap; };
static std::map<std::string, PhrSlot> & phr_slots() { static std::map<std::string, PhrSlot> m; return m; }       /* 원문마다 고정 칸 */
static int phr_loaded_count = 0;   /* 지금 언어에서 읽은 줄 수(로그용) */
static int phr_lang_cur = 0;       /* 0 한국어 · 1 English · 2 にほんご */
struct PhrLock { CRITICAL_SECTION cs; PhrLock() { InitializeCriticalSection(&cs); } };
static PhrLock g_phr_lock_o;   /* 구형 도구 모음에도 있는 CRITICAL_SECTION (SRWLOCK 은 컴파일러가 모름) */   /* 칸 만들기·언어 바꾸기를 서로 막는다(문구를 부르는 쪽이 둘 이상의 스레드일 수 있음) */
static int phr_width(const std::string & s) {   /* ov::tw 와 같은 규칙: ASCII 8 · 그 밖 16 (UTF-8 글자 수 기준) */
    int w = 0; for (size_t i = 0; i < s.size(); ) { unsigned char c = (unsigned char)s[i]; size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; w += c < 0x80 ? 8 : 16; i += n; } return w; }
static std::string phr_clip(const std::string & s, int maxw) {   /* 표시 폭이 넘치면 글자 경계에서 잘라낸다 */
    int w = 0; size_t i = 0; while (i < s.size()) { unsigned char c = (unsigned char)s[i]; size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; int cw = c < 0x80 ? 8 : 16; if (w + cw > maxw || i + n > s.size()) break; w += cw; i += n; }
    return s.substr(0, i); }
/* t52g: 사용자 덮어쓰기(strings.txt)용 — 줄(\n 으로 나뉜 한 줄)마다 폭을 잰다. 언어 파일은 자르지 않는다(긴 안내창 문구가 잘리던 문제) */
static std::string phr_clip_lines(const std::string & s, int maxw) {
    std::string o; size_t p = 0; while (p <= s.size()) { size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size(); if (p) o += '\n'; o += phr_clip(s.substr(p, e - p), maxw); p = e + 1; } return o; }
static std::string phr_unesc_val(const std::string & s) {
    std::string o; for (size_t i = 0; i < s.size(); i++) { if (s[i] == '\\' && i + 1 < s.size()) { char n = s[i + 1]; if (n == 'n') { o += '\n'; i++; } else if (n == '\\') { o += '\\'; i++; } else o += s[i]; } else o += s[i]; } return o; }
/* 한 줄 → 키·값. 성공하면 true. 주석·빈 줄·= 없는 줄은 false */
static bool phr_parse_line(std::string ln, std::string & key, std::string & val) {
    while (!ln.empty() && (ln.back() == '\r' || ln.back() == '\n')) ln.pop_back();
    if (ln.empty() || ln[0] == '#') return false;
    std::string k; size_t i = 0; bool found = false;
    for (; i < ln.size(); i++) {
        if (ln[i] == '\\' && i + 1 < ln.size()) { char n = ln[i + 1]; if (n == 'n') k += '\n'; else if (n == '=' || n == '\\' || n == '#') k += n; else { k += ln[i]; k += n; } i++; }
        else if (ln[i] == '=') { found = true; break; }
        else k += ln[i];
    }
    if (!found || k.empty()) return false;
    key = k; val = phr_unesc_val(ln.substr(i + 1)); return true;
}
/* 문구 파일 한 개 읽기(키가 겹치면 나중 파일이 이긴다). 실패해도(없음·잘림·깨짐) 아무 일도 없다. maxw > 0 이면 줄마다 그 폭으로 자른다 */
static void phr_load(const std::string & utf8path, int maxw = 0) {
    try {
        int wl = MultiByteToWideChar(CP_UTF8, 0, utf8path.c_str(), -1, NULL, 0); if (wl <= 0) return;
        std::wstring wp((size_t)wl, L'\0'); MultiByteToWideChar(CP_UTF8, 0, utf8path.c_str(), -1, &wp[0], wl);
        FILE * f = _wfopen(wp.c_str(), L"rb"); if (!f) return;
        std::string all; char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) { all.append(buf, n); if (all.size() > (4u << 20)) break; }
        fclose(f);
        if (all.size() >= 3 && (unsigned char)all[0] == 0xEF && (unsigned char)all[1] == 0xBB && (unsigned char)all[2] == 0xBF) all.erase(0, 3);   /* BOM 있어도 없어도 */
        size_t p = 0; while (p <= all.size()) {
            size_t e = all.find('\n', p); if (e == std::string::npos) e = all.size();
            std::string k, v; if (phr_parse_line(all.substr(p, e - p), k, v) && !v.empty()) { phr_tbl()[k] = (maxw > 0 && v != k) ? phr_clip_lines(v, maxw) : v; phr_loaded_count++; }   /* 원문 그대로(v==k)인 줄은 자르지 않는다 */
            p = e + 1;
        }
    } catch (...) { }
}
/* exe 와 같은 폴더의 파일을 유니코드 경로로 연다 — 한글·일본어 폴더에 풀어도 읽힌다(ACP 변환을 거치지 않음) */
static void phr_load_beside_exe(const wchar_t * name, int maxw = 0) {
    try {
        wchar_t mp[1024]; DWORD n = GetModuleFileNameW(NULL, mp, 1024); if (n == 0 || n >= 1024) return;
        std::wstring p(mp, n); size_t sl = p.find_last_of(L"\\/"); p = (sl == std::wstring::npos ? std::wstring() : p.substr(0, sl + 1)) + name;
        int ul = WideCharToMultiByte(CP_UTF8, 0, p.c_str(), -1, NULL, 0, NULL, NULL); if (ul <= 0) return;
        std::string u((size_t)ul, '\0'); WideCharToMultiByte(CP_UTF8, 0, p.c_str(), -1, &u[0], ul, NULL, NULL); u.resize((size_t)ul - 1);
        phr_load(u, maxw);
    } catch (...) { }
}
/* 칸 하나를 지금 언어의 문구(없으면 원문)로 채운다. 칸이 모자라면 글자 경계에서 자른다 */
static void phr_slot_fill(const std::string & key, PhrSlot & s) {
    auto it = phr_tbl().find(key); const std::string & v = it == phr_tbl().end() ? key : it->second;
    size_t n = v.size() < s.cap - 1 ? v.size() : s.cap - 1;
    while (n > 0 && n < v.size() && ((unsigned char)v[n] & 0xC0) == 0x80) n--;
    memcpy(s.buf, v.data(), n); s.buf[n] = 0;
}
/* 화면용 문구: 칸의 포인터(프로그램 끝까지 유효). 칸은 처음 부를 때 만들고, 내용은 언어를 바꿀 때 제자리에서 바뀐다 */
static const char * phr_(const char * def) {
    if (!def) return def;
    EnterCriticalSection(&g_phr_lock_o.cs); const char * r = def;
    auto & m = phr_slots(); auto it = m.find(def);
    if (it == m.end()) {
        size_t dl = strlen(def); PhrSlot s; s.cap = dl + 1 + 240; s.buf = (char *)malloc(s.cap); if (!s.buf) { LeaveCriticalSection(&g_phr_lock_o.cs); return def; }   /* 번역이 원문보다 길어도 들어가도록 여유 */
        phr_slot_fill(def, s); it = m.insert(std::make_pair(std::string(def), s)).first;
    }
    r = it->second.buf; LeaveCriticalSection(&g_phr_lock_o.cs); return r;
}
/* UI 언어 지정(0 한국어 · 1 English · 2 にほんご). 문구 파일을 다시 읽고 모든 칸을 새 언어로 바꾼다 — 재시작 불필요 */
static void phr_set_lang(int lang) {
    if (lang < 0 || lang > 2) lang = 0;
    EnterCriticalSection(&g_phr_lock_o.cs);
    phr_lang_cur = lang; phr_tbl().clear(); phr_loaded_count = 0;
    if (lang == 0) { phr_load_beside_exe(L"strings_ko.txt"); phr_load_beside_exe(L"strings.txt", 600); }   /* 한국어: 사용자 덮어쓰기 strings.txt 가 마지막 */
    else if (lang == 1) phr_load_beside_exe(L"strings_en.txt");
    else phr_load_beside_exe(L"strings_ja.txt");
    for (auto & kv : phr_slots()) phr_slot_fill(kv.first, kv.second);
    LeaveCriticalSection(&g_phr_lock_o.cs);
}
