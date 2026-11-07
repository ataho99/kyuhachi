/* kh_addwork.h -- r017 (#239 4번): 작품 추가 · 게임 폴더 자동 등록. 호스트(Windows)·시험 도구(Linux) 공용.
 * 흐름: 이미지 모음(Work) → 형식 판별(헤더) → sha1(캐시) → 게임 DB/NK DB 판정 → 구조 판정(prof::auto_profile) → games\<id>\game.ini 기록.
 * 경로는 "시스템 문자열"(Windows 에서는 ANSI) 그대로, 화면에 보이는 글(제목·판정)은 UTF-8. */
#pragma once
#include <set>
#include <cctype>
#include "settings/kh_prof.h"
#include "settings/kh_nkdb.h"
#include "settings/kh_zip.h"
#include <sys/stat.h>
#include <set>
#include "platform/plat.h" /* S3: platform layer */

namespace aw {

inline std::string acp_to_u8(const std::string & s) { return plat::acp_to_u8(s); }
/* UTF-8 → 시스템(ANSI) 문자열. 현재 코드 페이지에 없는 글자가 있으면(손실) 짧은 이름(8.3, ASCII)으로 바꾼다. (본문: platform/plat_str.h) */
inline std::string u8_to_acp(const std::string & s) { return plat::u8_to_acp(s); }
/* 엄격한 UTF-8 검사 · 높은 바이트 유무 */
inline bool u8_valid(const std::string & s) {
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { i++; continue; }
        int k = (c >= 0xC2 && c <= 0xDF) ? 1 : ((c & 0xF0) == 0xE0) ? 2 : ((c & 0xF8) == 0xF0 && c <= 0xF4) ? 3 : -1;
        if (k < 0 || i + (size_t)k >= n) return false;
        for (int j = 1; j <= k; j++) if (((unsigned char)s[i + (size_t)j] & 0xC0) != 0x80) return false;
        i += (size_t)k + 1;
    }
    return true;
}
inline bool has_hi(const std::string & s) { for (unsigned char c : s) if (c >= 0x80) return true; return false; }
/* ini 의 경로·파일 이름(UTF-8) → 시스템 문자열. r017 이전 방식(시스템 바이트 그대로 기록)이면 그대로 둔다. */
inline std::string sys_from_ini(const std::string & v) { return (has_hi(v) && u8_valid(v)) ? u8_to_acp(v) : v; }

inline void list_dir(const std::string & dir, std::vector<std::string> & files, std::vector<std::string> * subs) {
    plat::list_dir_acp(dir, files, subs);
}
inline char sep() { return plat::sep(); }
inline std::string join(const std::string & a, const std::string & b) { if (a.empty()) return b; char c = a.back(); return (c == '\\' || c == '/') ? a + b : a + sep() + b; }
inline std::string base_of(const std::string & p) { size_t s = p.find_last_of("\\/"); return s == std::string::npos ? p : p.substr(s + 1); }
inline std::string dir_of(const std::string & p) { size_t s = p.find_last_of("\\/"); return s == std::string::npos ? std::string(".") : p.substr(0, s); }
inline bool file_stat(const std::string & p, uint64_t & size, uint64_t & mtime) {
    struct stat st; if (stat(p.c_str(), &st) != 0) return false; size = (uint64_t)st.st_size; mtime = (uint64_t)st.st_mtime; return true;
}
inline bool dir_exists(const std::string & p) { struct stat st; return stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFDIR); }
inline bool make_dir(const std::string & p) { return plat::make_dir(p); }
inline std::string lower(std::string s) { for (auto & c : s) c = (char)tolower((unsigned char)c); return s; }
inline std::string hex64(uint64_t v) { char b[24]; snprintf(b, sizeof b, "%016llx", (unsigned long long)v); return b; }
inline uint64_t fnv(const std::string & s, uint64_t h = 1469598103934665603ull) { for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; } return h; }

/* ---- sha1 캐시: 크기·수정시각이 같으면 다시 계산하지 않는다 (게임 폴더를 켤 때마다 훑어도 빠르게) */
struct ShaCache {
    std::map<std::string, std::string> m; bool dirty = false; std::string path; unsigned hits = 0, miss = 0;
    static std::string key(const std::string & p, uint64_t sz, uint64_t mt) { return p + "|" + std::to_string(sz) + "|" + std::to_string(mt); }
    void load(const std::string & f) {
        path = f; FILE * fp = fopen(f.c_str(), "rb"); if (!fp) return; char ln[4096];
        while (fgets(ln, sizeof ln, fp)) { std::string s = ln; while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back(); size_t t = s.find('\t'); if (t == std::string::npos) continue; m[s.substr(t + 1)] = s.substr(0, t); }
        fclose(fp);
    }
    void save() {
        if (!dirty || path.empty()) return; FILE * fp = fopen(path.c_str(), "wb"); if (!fp) return;
        for (auto & kv : m) fprintf(fp, "%s\t%s\n", kv.second.c_str(), kv.first.c_str());
        fclose(fp); dirty = false;
    }
    std::string sha1(const std::string & p) {
        uint64_t sz = 0, mt = 0; if (!file_stat(p, sz, mt)) return std::string();
        std::string k = key(p, sz, mt); auto it = m.find(k); if (it != m.end()) { hits++; return it->second; }
        std::string h = prof::sha1_file(p); miss++; if (!h.empty()) { m[k] = h; dirty = true; } return h;
    }
};

/* ---- 이미지 후보 확장자(형식 자체는 확장자가 아니라 파일 머리로 판별한다) */
inline bool img_ext(const std::string & name) {
    static const char * const E[] = { ".fdi", ".hdi", ".nhd", ".d88", ".d77", ".88d", ".98d", ".nfd", ".hdm", ".dup", ".xdf", ".dsk", ".fim", ".flp", ".img", ".ima", ".thd", ".vhd", ".td0", ".iso", ".cue", ".mdf", ".ccd", ".hdd", ".hdn", ".dim", ".tfd", ".fdd", ".bin", NULL };
    std::string l = lower(name); size_t d = l.find_last_of('.'); if (d == std::string::npos) return false; std::string e = l.substr(d);
    for (int i = 0; E[i]; i++) if (e == E[i]) return true; return false;
}
/* 이름에서 디스크 번호·괄호 묶음을 뺀 줄기 — 같은 작품의 여러 장을 묶는 키 */
inline std::string stem_of(const std::string & name) {
    std::string s = nk::strip_name(name); std::string l = lower(s);
    static const char * const M[] = { " disk ", " disc ", " fd ", " side ", "_disk", "-disk", " disk", "_fd", "(disk", NULL };
    size_t cut = std::string::npos; for (int i = 0; M[i]; i++) { size_t p = l.rfind(M[i]); if (p != std::string::npos && p > 2 && p < cut) cut = p; }
    if (cut != std::string::npos) s = s.substr(0, cut);
    while (!s.empty() && (s.back() == ' ' || s.back() == '_' || s.back() == '-' || s.back() == '.')) s.pop_back();
    size_t a = 0; while (a < s.size() && s[a] == ' ') a++; return s.substr(a);
}

struct Work { std::string name, dir; std::vector<std::string> files; /* 파일 이름(폴더 제외) */ std::string zip; /* r018: zip 작품이면 zip 경로 (풀기 전까지 dir·files 는 비어 있음) */ };
inline bool is_zip(const std::string & name) { std::string l = lower(name); return l.size() > 4 && l.compare(l.size() - 4, 4, ".zip") == 0; }

/* 폴더를 훑어 작품 후보를 모은다. 한 폴더에 줄기가 다른 이미지가 여럿이면 줄기별로 나눈다. */
inline void collect(const std::string & dir, int depth, std::vector<Work> & out, bool zips = true) {
    std::vector<std::string> files, subs; list_dir(dir, files, &subs); std::sort(files.begin(), files.end()); std::sort(subs.begin(), subs.end());
    if (zips) for (auto & f : files) if (is_zip(f)) { Work w; w.dir = dir; w.zip = join(dir, f); w.name = stem_of(f); if (w.name.empty()) w.name = f; out.push_back(w); }
    std::vector<std::string> imgs; for (auto & f : files) if (img_ext(f)) imgs.push_back(f);
    if (!imgs.empty()) {
        std::map<std::string, std::vector<std::string> > grp; std::vector<std::string> order;
        for (auto & f : imgs) { std::string k = nk::normkey(stem_of(f)); if (k.empty()) k = "_"; if (!grp.count(k)) order.push_back(k); grp[k].push_back(f); }
        if (grp.size() <= 1 || imgs.size() <= 6) { Work w; w.dir = dir; w.files = imgs; w.name = base_of(dir); out.push_back(w); }
        else for (auto & k : order) { Work w; w.dir = dir; w.files = grp[k]; w.name = stem_of(grp[k][0]); if (w.name.empty()) w.name = grp[k][0]; out.push_back(w); }
    }
    if (depth > 0) for (auto & s : subs) { if (!s.empty() && s[0] == '_') continue; collect(join(dir, s), depth - 1, out, zips); }
}
/* 직접 고른 파일들(같은 폴더끼리 한 작품) */
inline void from_files(const std::vector<std::string> & paths, std::vector<Work> & out) {
    std::map<std::string, Work> m; std::vector<std::string> order;
    for (auto & p : paths) {
        if (is_zip(p)) { Work w; w.dir = dir_of(p); w.zip = p; w.name = stem_of(base_of(p)); if (w.name.empty()) w.name = base_of(p); out.push_back(w); continue; }
        std::string d = dir_of(p); if (!m.count(d)) { order.push_back(d); m[d].dir = d; } m[d].files.push_back(base_of(p));
    }
    for (auto & d : order) { Work w = m[d]; std::sort(w.files.begin(), w.files.end()); w.name = w.files.size() == 1 ? stem_of(w.files[0]) : stem_of(w.files[0]); if (w.name.empty()) w.name = base_of(d); out.push_back(w); }
}
inline std::string signature(const Work & w) {
    if (!w.zip.empty()) { uint64_t sz = 0, mt = 0; file_stat(w.zip, sz, mt); return hex64(fnv(lower(base_of(w.zip)) + ":" + std::to_string(sz) + ":" + std::to_string(mt))); }   /* zip 은 풀지 않고 이름·크기·시각으로 */
    std::vector<std::string> v; for (auto & f : w.files) { uint64_t sz = 0, mt = 0; file_stat(join(w.dir, f), sz, mt); v.push_back(f + ":" + std::to_string(sz) + ":" + std::to_string(mt)); }
    std::sort(v.begin(), v.end()); uint64_t h = fnv(w.dir); for (auto & s : v) h = fnv(s + ";", h); return hex64(h);
}

struct Ctx {
    std::string games_dir;
    const std::vector<prof::DbGame> * db = NULL;
    const nk::Db * nk = NULL;
    ShaCache * cache = NULL;
    std::map<std::string, std::string> reg;      /* sig → id (이미 등록된 것) */
    std::set<std::string> ids;                   /* 게임 폴더에 있는 id 들 */
    std::map<std::string, std::string> hashidx;  /* r019: 이미지 sha1 → 등록된 작품 id (주 이미지 + 함께 있는 이미지) */
    std::map<std::string, std::string> dbidx;    /* r019: DB 작품 id → 등록된 작품 id */
    std::map<std::string, std::string> nkidx;    /* r019: Neo Kobe gid → 등록된 작품 id */
    struct Pend { std::string dbid, label, sig; std::vector<std::string> sha; std::string company, year, msrc; };
    std::vector<Pend> pend;                      /* r019: 주 작품이 아직 없는 보조 이미지(주 작품이 등록되면 붙인다) */
    void (*on_dir)(const std::string &) = NULL;  /* r020: 작품 추가 때 작품 폴더를 알려 준다(공용 자원 가져오기) */
};
struct Result { enum St { ADDED, EXISTS, SKIP } st = SKIP; std::string id, title, note, grade, jshort, judge, block, metasrc, year, maker; };

/* games\*\game.ini 의 sig 를 모아 둔다 */
inline void index_registered(Ctx & c) {
    c.reg.clear(); c.ids.clear(); c.hashidx.clear(); c.dbidx.clear(); c.nkidx.clear(); std::vector<std::string> files, subs; list_dir(c.games_dir, files, &subs);
    for (auto & s : subs) {
        c.ids.insert(lower(s)); prof::Ini gi; if (!prof::ini_load(join(join(c.games_dir, s), "game.ini"), gi) || !gi.count("game")) continue;
        auto & g = gi["game"]; const std::string tgt = g.count("dup_of") ? g["dup_of"] : s;   /* 1b-3: 숨긴 중복 작품은 주 작품으로 연결 */
        auto it = g.find("sig"); if (it != g.end() && !it->second.empty()) c.reg[it->second] = tgt;
        auto is = g.find("sigs"); if (is != g.end()) for (auto & x : prof::split_list(is->second)) if (!x.empty()) c.reg[x] = tgt;
        auto ih = g.find("sha1s"); if (ih != g.end()) for (auto & x : prof::split_list(ih->second)) if (x.size() == 40 && !c.hashidx.count(x)) c.hashidx[x] = tgt;
        auto id = g.find("dbid"); if (id != g.end() && !id->second.empty() && !c.dbidx.count(id->second)) c.dbidx[id->second] = tgt;
        auto ig = g.find("nk_gid"); if (ig != g.end() && !ig->second.empty() && !c.nkidx.count(ig->second)) c.nkidx[ig->second] = tgt;
    }
}
/* 한글 → 로마자(국어의 로마자 표기법, 소리 변화 없이 글자 그대로) */
inline std::string romanize(const std::string & u8) {
    static const char * const L[19] = { "g", "kk", "n", "d", "tt", "r", "m", "b", "pp", "s", "ss", "", "j", "jj", "ch", "k", "t", "p", "h" };
    static const char * const V[21] = { "a", "ae", "ya", "yae", "eo", "e", "yeo", "ye", "o", "wa", "wae", "oe", "yo", "u", "wo", "we", "wi", "yu", "eu", "ui", "i" };
    static const char * const T[28] = { "", "k", "kk", "k", "n", "n", "n", "t", "l", "k", "m", "p", "l", "l", "p", "l", "m", "p", "p", "t", "t", "ng", "t", "t", "k", "t", "p", "t" };
    std::string o; const unsigned char * p = (const unsigned char *)u8.c_str();
    while (*p) {
        uint32_t cp; int k;
        if (*p < 0x80) { cp = *p; k = 1; } else if ((*p & 0xE0) == 0xC0 && p[1]) { cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F); k = 2; } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); k = 3; } else { cp = '?'; k = 1; }
        p += k;
        if (cp >= 0xAC00 && cp <= 0xD7A3) { uint32_t s = cp - 0xAC00; o += L[s / 588]; o += V[(s % 588) / 28]; o += T[s % 28]; }
        else if (cp < 0x80 && isalnum((int)cp)) o += (char)tolower((int)cp);
        else o += '-';
    }
    return o;
}
/* 작품 id (games\<id>, logs\<id>, saves\<id> 폴더 이름): DB 작품 id → Neo Kobe gid → 제목 로마자. 소문자·숫자·'-' 만. */
inline std::string make_id(const std::string & dbid, const std::string & nkgid, const std::string & title, const std::string & sig, const std::set<std::string> & used) {
    std::string raw = !dbid.empty() ? dbid : !nkgid.empty() ? nkgid : romanize(title);
    std::string o; for (unsigned char ch : raw) { char c = (char)tolower(ch); if (isalnum((unsigned char)c) || c == '_') o += c; else if (c == '-' || c == ' ' || c == '.' || c == '(' || c == ')' || c == ',' || c == '\'') { if (!o.empty() && o.back() != '-') o += '-'; } }
    while (!o.empty() && o.back() == '-') o.pop_back();
    int alnum = 0; for (char ch : o) if (isalnum((unsigned char)ch)) alnum++;
    if (alnum < 3) o = "work-" + sig.substr(0, 8);
    if (o.size() > 56) { o = o.substr(0, 56); while (!o.empty() && o.back() == '-') o.pop_back(); }
    std::string id = o; int n = 2; while (used.count(lower(id))) { id = o + "-" + std::to_string(n++); }
    return id;
}

/* 이름 줄(제목)에 쓸 보기 좋은 이름: NK 제목 > 작업 이름. 이미 UTF-8 로 바꾼 문자열을 돌려준다. */
inline std::string pretty(const std::string & name_sys) { return acp_to_u8(name_sys); }

/* 실행 불가 사유(빈칸 = 실행 가능). kv = game.ini/DB 의 값. 호스트가 목록을 만들 때마다 계산 → 지원이 늘면 다시 등록하지 않아도 풀린다. */
inline bool & mouse_ok() { static bool v = false; return v; }   /* r020: run\res\flags.ini 의 mouse_support=1 (실조작 시험 통과 뒤 데이터로 켠다) */
inline std::set<std::string> & mouse_games() { static std::set<std::string> s; return s; }   /* t29 (#266): flags.ini mouse_support_games=nadia,ultra — 작품 폴더 id 또는 nk_gid 가 들어 있는 작품만 마우스 필수를 푼다(전역 mouse_support=1 과 별개) */
inline bool mouse_released(const std::map<std::string, std::string> & kv) {
    if (mouse_ok()) return true; auto f = [&](const char * k) -> bool { auto it = kv.find(k); if (it == kv.end() || it->second.empty()) return false; std::string v = it->second; for (auto & c : v) c = (char)tolower((unsigned char)c); return mouse_games().count(v) != 0; };
    return f("__id") || f("nk_gid");
}
inline std::string block_reason_raw(const prof::Profile & p, const std::map<std::string, std::string> & kv, bool * soft) {   /* 2d (#580): soft = 막지 않고 내장 DOS 프롬프트로 여는 사유 */
    auto g = [&](const char * k) -> std::string { auto it = kv.find(k); return it == kv.end() ? std::string() : it->second; };
    /* t48a (#615): 「마우스 필수」는 실행을 막는 사유에서 뺐다(마우스 지원됨). 화면에는 「장치: 마우스 필수」 표시만 남는다. */
    bool nodisk = true; for (auto & d : p.disk) if (!d.empty()) nodisk = false;
    if (nodisk && !g("cd").empty() && g("cd") != "-") return "";   /* t40: CD 이미지가 잡힌 작품 = 내장 DOS 로 D: 에 올려 실행 */
    if (p.install_required && nodisk) { if (soft) *soft = true; return khtr::tr("설치 전용 작품 (설치 단계 미지원)"); }
    if (g("curated") != "1") {
        std::string fl = "," + g("nk_flags") + ","; std::string md = g("nk_media");
        if (md == "CD" || md == "CDFD" || fl.find(",cd,") != std::string::npos) { if (nodisk) return khtr::tr("CD 작품 (CD 미지원)"); }
        bool hd = !p.disk[2].empty() || !p.disk[3].empty();
        if (fl.find(",inst,") != std::string::npos && !hd) { if (soft) *soft = true; return khtr::tr("하드 설치가 필요한 작품 (설치 단계 미지원)"); }
    }
    if (nodisk) { if (soft) *soft = true; return p.reason.empty() ? std::string(khtr::tr("부팅할 수 있는 디스크가 없음")) : p.reason; }
    return "";
}
inline std::string block_reason(const prof::Profile & p, const std::map<std::string, std::string> & kv) { bool s = false; std::string r = block_reason_raw(p, kv, &s); return s ? std::string() : r; }   /* 2d: 막는 사유(C2 CD 미지원 등)만 */
inline std::string dos_note(const prof::Profile & p, const std::map<std::string, std::string> & kv) { bool s = false; std::string r = block_reason_raw(p, kv, &s); return s ? r : std::string(); }   /* 2d: 막지 않고 DOS 프롬프트로 열 때 정보창에 보일 사유 */

/* r019: zip 안의 디스크 이미지 — 먼저 메모리로 풀어 sha1 을 구하고, 이미 등록된 해시면 디스크에 쓰지 않는다 (같은 이미지는 한 번만).
   새 이미지는 games\_zip\<첫 이미지 sha1 앞 12자>\ 에 푼다 (이름이 아니라 해시가 키 → 같은 zip 을 날짜만 바꿔 복사해도 한 벌).
   knownid 가 채워지면 = zip 안 이미지가 전부 이미 등록돼 있음(풀지 않음). */
/* r020 (#248): 우리가 만드는 파일 이름은 ASCII — 한글 등 비ASCII 바이트 연속은 _ 하나로 */
inline std::string ascii_safe(const std::string & s) { std::string o; bool pend = false; for (unsigned char ch : s) { if (ch >= 0x80) { if (!pend) { o += '_'; pend = true; } } else { o += (char)ch; pend = false; } } return o; }
inline bool unzip_images(Ctx & c, const std::string & zipfile, const std::string & sig, std::string & outdir, std::vector<std::string> & files, std::string & err, std::string * knownid = NULL) {
    (void)sig;
    zp::Zip z; if (!zp::open(zipfile, z, &err)) return false;
    std::vector<const zp::Entry *> im; for (auto & e : z.e) if (!e.isdir && img_ext(base_of(e.name)) && e.usize > 0) im.push_back(&e);
    if (im.empty()) { err = khtr::tr("zip 안에 디스크 이미지가 없음"); return false; }
    struct It { const zp::Entry * e; std::string bn, sha; std::vector<uint8_t> buf; bool known; };
    std::vector<It> its; std::set<std::string> used; size_t nknown = 0; std::string kid;
    for (auto * e : im) {
        It t; t.e = e; t.bn = ascii_safe(sys_from_ini(base_of(e->name))); std::string er;
        std::string key = lower(t.bn); int k = 2; while (used.count(key)) { size_t d = t.bn.find_last_of('.'); std::string b2 = d == std::string::npos ? t.bn : t.bn.substr(0, d), x = d == std::string::npos ? "" : t.bn.substr(d); t.bn = b2 + "_" + std::to_string(k++) + x; key = lower(t.bn); }
        used.insert(key);
        if (!zp::extract(z, *e, t.buf, er)) { err = e->name + ": " + er; return false; }
        prof::Sha1 s; if (!t.buf.empty()) s.add(&t.buf[0], t.buf.size()); t.sha = s.hex();
        auto h = c.hashidx.find(t.sha); t.known = h != c.hashidx.end(); if (t.known) { nknown++; kid = h->second; t.buf.clear(); t.buf.shrink_to_fit(); }
        its.push_back(std::move(t));
    }
    if (nknown == its.size()) { if (knownid) *knownid = kid; return true; }
    std::sort(its.begin(), its.end(), [](const It & a, const It & b) { return lower(a.bn) < lower(b.bn); });
    make_dir(c.games_dir); std::string zr = join(c.games_dir, "_zip"); make_dir(zr); outdir = join(zr, its[0].sha.substr(0, 12)); make_dir(outdir);
    for (auto & t : its) {
        std::string dst = join(outdir, t.bn); uint64_t sz = 0, mt = 0;
        if (file_stat(dst, sz, mt) && sz == t.e->usize) { files.push_back(t.bn); continue; }
        if (t.known) { std::vector<uint8_t> b2; std::string er; if (!zp::extract(z, *t.e, b2, er)) { err = er; return false; } t.buf.swap(b2); }   /* 일부만 새로운 zip: 같은 작품 폴더에 모아 둔다 */
        FILE * f = fopen(dst.c_str(), "wb"); if (!f) { err = khtr::tr("풀 폴더에 쓸 수 없음"); return false; }
        bool ok = t.buf.empty() || fwrite(t.buf.data(), 1, t.buf.size(), f) == t.buf.size(); fclose(f); if (!ok) { err = khtr::tr("쓰기 실패 (디스크 공간?)"); return false; }
        files.push_back(t.bn);
    }
    std::sort(files.begin(), files.end()); return true;
}

/* ---- r019: game.ini 의 [game] 구역 값을 고친다 (mode 0 = 없을 때만 넣음, 1 = 쉼표 목록에 더함, 2 = " | " 목록에 더함, 3 = 덮어씀) */
struct KVu { std::string k, v; int mode; };
inline bool ini_patch(const std::string & path, const std::vector<KVu> & ups) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false; std::string all; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) all.append(b, n); fclose(f);
    std::vector<std::string> L; { size_t p = 0; while (p <= all.size()) { size_t q = all.find('\n', p); if (q == std::string::npos) { if (p < all.size()) L.push_back(all.substr(p)); break; } L.push_back(all.substr(p, q - p)); p = q + 1; } }
    size_t gs = L.size(); for (size_t i = 0; i < L.size(); i++) { std::string t = L[i]; while (!t.empty() && (t.back() == '\r' || t.back() == ' ')) t.pop_back(); if (t == "[game]") { gs = i; break; } }
    if (gs == L.size()) { L.push_back("[game]"); }
    size_t ge = gs + 1; while (ge < L.size() && (L[ge].empty() || L[ge][0] != '[')) ge++;
    for (auto & u : ups) {
        size_t at = std::string::npos; for (size_t i = gs + 1; i < ge; i++) { size_t e = L[i].find('='); if (e != std::string::npos && L[i].compare(0, e, u.k) == 0 && e == u.k.size()) { at = i; break; } }
        if (at == std::string::npos) { L.insert(L.begin() + gs + 1, u.k + "=" + u.v); ge++; continue; }
        std::string cur = L[at].substr(u.k.size() + 1); while (!cur.empty() && (cur.back() == '\r')) cur.pop_back();
        if (u.mode == 0) { if (cur.empty()) L[at] = u.k + "=" + u.v; continue; }
        if (u.mode == 3) { L[at] = u.k + "=" + u.v; continue; }
        std::string sepv = u.mode == 1 ? "," : " | "; bool has = false;
        { size_t p = 0; while (p <= cur.size()) { size_t q = cur.find(sepv, p); std::string it = cur.substr(p, q == std::string::npos ? std::string::npos : q - p); if (it == u.v) has = true; if (q == std::string::npos) break; p = q + sepv.size(); } }
        if (!has) L[at] = u.k + "=" + (cur.empty() ? u.v : cur + sepv + u.v);
    }
    std::string o; for (auto & l : L) { o += l; o += '\n'; }
    FILE * w = fopen(path.c_str(), "wb"); if (!w) return false; bool ok = fwrite(o.data(), 1, o.size(), w) == o.size(); fclose(w); return ok;
}
/* DB 의 ignore 목록(보조 이미지) 에 있는 이미지만으로 된 묶음이면 그 DB 작품을 돌려준다 */
inline const prof::DbGame * db_by_ignore(const std::vector<prof::DbGame> * db, const std::vector<prof::ImgInfo> & imgs) {
    if (!db || imgs.empty()) return NULL;
    for (auto & d : *db) { auto it = d.g.find("ignore"); if (it == d.g.end()) continue; std::vector<std::string> L = prof::split_list(it->second); size_t m = 0;
        for (auto & i : imgs) for (auto & h : L) if (!i.sha1.empty() && i.sha1 == h) { m++; break; }
        if (m == imgs.size()) return &d; }
    return NULL;
}
inline std::string img_set_label(const std::string & title, const std::vector<prof::ImgInfo> & imgs) {
    bool hd = false; for (auto & i : imgs) if (i.fmt == prof::F_HDI || i.fmt == prof::F_NHD || i.fmt == prof::F_RAWHD || i.fmt == prof::F_VHD) hd = true;
    return title + " (" + (hd ? "HDD" : "FD") + (imgs.size() > 1 || !hd ? " " + std::to_string(imgs.size()) + khtr::tr("장") : std::string()) + ")";
}
/* 같은 작품의 다른 이미지를 이미 등록된 작품(tgt)에 「함께 있는 이미지」로 붙인다. 주 작품에 제작사·연도가 비어 있으면 상속 */
inline void attach_companion(Ctx & c, const std::string & tgt, const std::string & label, const std::string & sig, const std::vector<std::string> & sha, const std::string & company, const std::string & year, const std::string & msrc) {
    std::vector<KVu> u; u.push_back({ "more", label, 2 }); u.push_back({ "sigs", sig, 1 });
    for (auto & h : sha) { u.push_back({ "sha1s", h, 1 }); c.hashidx[h] = tgt; }
    if (!company.empty()) u.push_back({ "company", company, 0 }); if (!year.empty()) u.push_back({ "year", year, 0 }); if (!msrc.empty()) u.push_back({ "metasrc", msrc, 0 });
    ini_patch(join(join(c.games_dir, tgt), "game.ini"), u); c.reg[sig] = tgt;
}
/* 옛 등록(r018 이전)에서 생긴 같은 이미지 중복을 합친다: 해시 집합이 같은 자동 등록 작품은 먼저 등록된 것만 남기고 나머지는 games\_중복\ 로 옮긴다(지우지 않음) */
inline int merge_dups(Ctx & c, std::vector<std::string> * log) {
    std::vector<std::string> files, subs; list_dir(c.games_dir, files, &subs);
    std::sort(subs.begin(), subs.end(), [](const std::string & a, const std::string & b) { return a.size() != b.size() ? a.size() < b.size() : a < b; });
    std::map<std::string, std::string> first; int moved = 0;
    for (auto & s : subs) {
        if (s.empty() || s[0] == '_') continue; std::string gi = join(join(c.games_dir, s), "game.ini"); prof::Ini ini; if (!prof::ini_load(gi, ini) || !ini.count("game")) continue;
        auto & g = ini["game"]; if (g.count("dup_of")) continue; if (g.count("auto") == 0 || g["auto"] != "1") continue;   /* 1b-3: 이미 중복 표시한 작품은 건너뜀 */
        std::string imgdir = sys_from_ini(g.count("imgdir") ? g["imgdir"] : std::string()); if (imgdir.empty() || !dir_exists(imgdir)) continue;
        std::vector<std::string> f2; list_dir(imgdir, f2, NULL); std::set<std::string> hs; for (auto & f : f2) if (img_ext(f)) { std::string h = c.cache ? c.cache->sha1(join(imgdir, f)) : prof::sha1_file(join(imgdir, f)); if (!h.empty()) hs.insert(h); }
        if (hs.empty()) continue;
        std::string same; bool all = true; for (auto & h : hs) { auto it = first.find(h); if (it == first.end()) { all = false; break; } if (same.empty()) same = it->second; else if (same != it->second) { all = false; break; } }
        if (all && !same.empty()) {
            /* 1b-3 (#510-4): 사용자 폴더는 옮기거나 이름을 바꾸지 않는다 — game.ini 에 dup_of 만 적어 목록에서 숨긴다 */
            std::string src = g.count("srcdir") ? g["srcdir"] : std::string();
            if (ini_patch(gi, { { "dup_of", same, 3 } })) {
                moved++; if (!src.empty()) ini_patch(join(join(c.games_dir, same), "game.ini"), { { "alt_src", src, 2 } });
                if (log) log->push_back("중복 합침: " + s + " → " + same + " (목록에서만 숨김, 폴더는 그대로)");
            }
            continue;
        }
        for (auto & h : hs) if (!first.count(h)) first[h] = s;
        if (g.count("sha1s") == 0) { std::string v; for (auto & h : hs) v += (v.empty() ? "" : ",") + h; ini_patch(gi, { { "sha1s", v, 3 } }); }
    }
    return moved;
}

/* 한 작품을 등록(또는 이미 있으면 건너뜀). 이미지는 w.dir 의 w.files (zip 작품은 먼저 풀어서 같은 흐름으로). */
inline Result register_core(Ctx & c, const Work & w, const std::string & sig, const std::string & srcpath, bool dry) {
    Result R;
    std::vector<prof::ImgInfo> imgs; std::vector<std::string> skipped;
    for (auto & f : w.files) {
        std::string path = join(w.dir, f); prof::ImgInfo i = prof::probe(path);
        if (i.fmt == prof::F_UNKNOWN || i.fmt == prof::F_NOTIMG) { skipped.push_back(f); continue; }
        if (i.size < (96ull << 20)) i.sha1 = c.cache ? c.cache->sha1(path) : prof::sha1_file(path);
        prof::probe_boot(i); prof::fat_sig(i); imgs.push_back(i);
    }
    if (imgs.empty()) { R.st = Result::SKIP; R.note = khtr::tr("이미지 형식을 알 수 없음"); return R; }
    std::vector<std::string> shas; for (auto & i : imgs) if (!i.sha1.empty()) shas.push_back(i.sha1);
    {   /* r019 (#247-1): 이미지가 전부 이미 등록된 해시면 같은 작품 — 처음 등록한 경로만 남긴다 */
        size_t nk_ = 0; std::string kid; for (auto & i : imgs) { auto h = c.hashidx.find(i.sha1); if (!i.sha1.empty() && h != c.hashidx.end()) { nk_++; kid = h->second; } }
        if (nk_ == imgs.size()) { R.st = Result::EXISTS; R.id = kid; R.note = std::string(khtr::tr("같은 이미지가 이미 등록됨")) + " → " + kid; if (!dry) { ini_patch(join(join(c.games_dir, kid), "game.ini"), { { "sigs", sig, 1 } }); c.reg[sig] = kid; } return R; }
    }
    static const std::vector<prof::DbGame> none; prof::Profile q = prof::resolve("", imgs, c.db ? *c.db : none, w.name);
    bool curated = q.src == "DB";
    nk::Result r; if (c.nk && c.nk->loaded) r = nk::match(*c.nk, imgs, w.name);
    std::string title = pretty(w.name), year, maker, msrc, genre;
    const nk::Game * g = (r.game >= 0 && (r.grade == 'A' || r.grade == 'B')) ? &c.nk->g[r.game] : NULL;
    if (curated && !q.title.empty()) title = q.title;
    if (g) {
        if (!curated) title = g->title;
        bool hy = g->year != "" && g->year != "-" && g->year != "????" && g->ysrc != "none", hm = g->maker != "" && g->maker != "-" && g->msrc != "none";
        if (hy) year = g->year; if (hm) maker = g->maker;
        if (hy) msrc = g->ysrc; if (hm && msrc.find(g->msrc) == std::string::npos) msrc += (msrc.empty() ? "" : ",") + g->msrc;   /* 연도 출처 + 제작사 출처 */
    } else if (r.game >= 0 && r.grade == 'C') { /* 후보는 제목을 바꾸지 않는다 */ }
    {   /* r019 (#247-1): 같은 작품의 다른 이미지(또는 DB ignore 에 오른 보조 이미지)는 새 작품으로 만들지 않고 주 작품에 붙인다 */
        const prof::DbGame * ig = curated ? NULL : db_by_ignore(c.db, imgs);
        std::string tgt; if (ig) { auto it = c.dbidx.find(ig->id); if (it != c.dbidx.end()) tgt = it->second; }
        else if (curated && !q.dbhit_id.empty()) { auto it = c.dbidx.find(q.dbhit_id); if (it != c.dbidx.end()) tgt = it->second; }
        else if (g) { auto it = c.nkidx.find(g->gid); if (it != c.nkidx.end()) tgt = it->second; }
        std::string lab = img_set_label(g ? g->title : pretty(w.name), imgs);
        if (!tgt.empty()) {
            R.st = Result::EXISTS; R.id = tgt; R.note = khtr::fmt(khtr::tr("같은 작품의 다른 이미지 → %s 의 「함께 있는 이미지」로 붙임"), tgt.c_str());
            if (!dry) attach_companion(c, tgt, lab, sig, shas, maker, year, msrc);
            return R;
        }
        if (ig) {   /* 주 작품이 아직 없음: 주 작품을 등록할 때 붙인다 (이 묶음은 아직 등록된 것으로 치지 않아 다음 훑기에서도 다시 시도) */
            R.st = Result::EXISTS; R.id = ig->id; R.note = khtr::fmt(khtr::tr("보조 이미지 — 주 작품(%s)이 등록되면 붙음"), ig->id.c_str());
            if (!dry) { Ctx::Pend pd; pd.dbid = ig->id; pd.label = lab; pd.sig = sig; pd.sha = shas; pd.company = maker; pd.year = year; pd.msrc = msrc; c.pend.push_back(pd); }
            return R;
        }
    }
    std::string id = make_id(curated ? q.dbhit_id : std::string(), g ? g->gid : std::string(), title, sig, c.ids);
    std::string jshort = curated ? std::string(khtr::tr("확인됨 (DB 일치)")) : r.shortt, judge = curated ? khtr::fmt(khtr::tr("DB 일치 — %s"), (q.title.empty() ? q.reason : q.title).c_str()) : r.text;
    R.judge = judge; R.id = id; R.title = title; R.grade = curated ? "A" : std::string(1, r.grade); R.jshort = jshort; R.metasrc = msrc; R.year = year; R.maker = maker;
    R.block = block_reason(q, std::map<std::string, std::string>());
    {   /* 등록 시점 판정에 쓸 kv (실행 불가는 목록을 만들 때 다시 계산) */
        std::map<std::string, std::string> kv; if (g) { kv["nk_flags"] = g->flags; kv["nk_media"] = g->media; } if (curated) kv["curated"] = "1";
        for (auto & k : q.kv) kv[k.first] = k.second; R.block = block_reason(q, kv);
    }
    if (dry) { R.st = Result::ADDED; R.note = R.block.empty() ? std::string(khtr::tr("등록 가능")) : std::string(khtr::tr("등록하되 실행 불가")) + ": " + R.block; return R; }
    std::string d = join(c.games_dir, id); make_dir(c.games_dir); if (!make_dir(d)) { R.st = Result::SKIP; R.note = khtr::tr("games 폴더에 쓸 수 없음"); return R; }
    FILE * f = fopen(join(d, "game.ini").c_str(), "wb"); if (!f) { R.st = Result::SKIP; R.note = khtr::tr("game.ini 를 쓸 수 없음"); return R; }
    auto U = [](const std::string & s) { return acp_to_u8(s); };   /* game.ini 는 UTF-8 (BOM 없음) */
    fprintf(f, "; PC-98 PLAYER 자동 등록 (r019) -- 직접 고쳐도 됩니다. UTF-8. auto=1 은 자동 등록 표시.\n[game]\nauto=1\ntitle=%s\nimgdir=%s\nsrcdir=%s\nsig=%s\n", title.c_str(), U(w.dir).c_str(), U(srcpath).c_str(), sig.c_str());
    { std::string sv; for (auto & h : shas) sv += (sv.empty() ? "" : ",") + h; if (!sv.empty()) fprintf(f, "sha1s=%s\n", sv.c_str()); }
    if (curated && !q.dbhit_id.empty()) fprintf(f, "dbid=%s\n", q.dbhit_id.c_str());
    if (!curated) {   /* DB 큐레이션 작품은 디스크 배치를 DB 가 정하므로 적지 않는다 */
        static const char * const dk[4] = { "disk_a", "disk_b", "disk_c", "disk_d" };
        for (int i = 0; i < 4; i++) if (!q.disk[i].empty()) fprintf(f, "%s=%s\n", dk[i], U(q.disk[i]).c_str());
        fprintf(f, "boot=%c\n", q.bootletter);
        if (!q.swap.empty()) { fprintf(f, "swap="); for (size_t i = 0; i < q.swap.size(); i++) fprintf(f, "%s%s", i ? "," : "", U(q.swap[i]).c_str()); fprintf(f, "\n"); }
        if (q.install_required) fprintf(f, "install_required=1\n");
        { auto cdi = q.kv.find("cd"); if (cdi != q.kv.end() && !cdi->second.empty()) fprintf(f, "cd=%s\n", U(cdi->second).c_str()); }   /* t40: CD 자동 인식 */
    }
    if (!curated && g && r.grade == 'A') fprintf(f, "font_kind=jp\n");   /* r019 (#247-10): 롬셋과 해시가 같은 일본어 원판은 일본어 글꼴 */
    if (!maker.empty()) fprintf(f, "company=%s\n", maker.c_str());
    if (!year.empty()) fprintf(f, "year=%s\n", year.c_str());
    if (!msrc.empty()) fprintf(f, "metasrc=%s\n", msrc.c_str());
    if (g) { fprintf(f, "nk_gid=%s\nnk_flags=%s\nnk_media=%s\n", g->gid.c_str(), g->flags.c_str(), g->media.c_str()); }
    if (!curated && r.game >= 0 && (r.grade == 'B' || r.grade == 'C')) fprintf(f, "orig=%s\n", c.nk->g[r.game].title.c_str());
    if (curated && !q.dbhit_title.empty()) fprintf(f, "dbhit=%s\n", q.dbhit_title.c_str());
    fprintf(f, "grade=%s\njshort=%s\njudge=%s\nauto_note=%s\n", R.grade.c_str(), jshort.c_str(), judge.c_str(), q.reason.c_str());
    fclose(f);
    c.reg[sig] = id; c.ids.insert(lower(id));
    for (auto & h : shas) c.hashidx[h] = id; if (curated && !q.dbhit_id.empty()) c.dbidx[q.dbhit_id] = id; if (g) c.nkidx[g->gid] = id;
    if (curated && !q.dbhit_id.empty()) for (size_t pi = 0; pi < c.pend.size();) { if (c.pend[pi].dbid == q.dbhit_id) { auto & pd = c.pend[pi]; attach_companion(c, id, pd.label, pd.sig, pd.sha, pd.company, pd.year, pd.msrc); c.pend.erase(c.pend.begin() + pi); } else pi++; }
    R.st = Result::ADDED; R.note = R.block.empty() ? std::string(khtr::tr("등록함")) : std::string(khtr::tr("등록함 (실행 불가")) + ": " + R.block + ")"; return R;
}
inline Result register_work(Ctx & c, const Work & w, bool dry = false) {
    if (!dry && c.on_dir) c.on_dir(w.dir);
    Result R; std::string sig = signature(w);
    { auto it = c.reg.find(sig); if (it != c.reg.end()) { R.st = Result::EXISTS; R.id = it->second; R.note = khtr::tr("이미 등록됨"); return R; } }
    if (!w.zip.empty()) {
        Work w2 = w; std::string err, ex; std::vector<std::string> fl;
        std::string kid; if (!unzip_images(c, w.zip, sig, ex, fl, err, &kid)) { R.st = Result::SKIP; R.note = "zip: " + err; return R; }
        if (!kid.empty()) { R.st = Result::EXISTS; R.id = kid; R.note = std::string(khtr::tr("zip 안 이미지가 이미 등록됨")) + " → " + kid; if (!dry) { ini_patch(join(join(c.games_dir, kid), "game.ini"), { { "sigs", sig, 1 } }); c.reg[sig] = kid; } return R; }
        w2.dir = ex; w2.files = fl; w2.zip.clear();
        return register_core(c, w2, sig, w.zip, dry);
    }
    return register_core(c, w, sig, w.dir, dry);
}

struct Summary { int added = 0, exists = 0, skipped = 0, blocked = 0; std::vector<std::string> lines; std::vector<std::string> added_ids, added_why; };   /* t44a (#494): 이번에 추가된 작품 id · 자동 판정 근거 한 줄 */
inline void run_works(Ctx & c, const std::vector<Work> & ws, Summary & S, bool dry = false) {
    for (auto & w : ws) {
        Result r = register_work(c, w, dry);
        if (r.st == Result::ADDED) { S.added++; if (!r.block.empty()) S.blocked++; S.added_ids.push_back(r.id); S.added_why.push_back(r.note); } else if (r.st == Result::EXISTS) S.exists++; else S.skipped++;
        if ((r.st != Result::EXISTS || r.note.find("→") != std::string::npos) && S.lines.size() < 400) S.lines.push_back(std::string(r.st == Result::ADDED ? "+ " : "- ") + pretty(w.name) + "  " + (r.grade.empty() ? std::string() : r.grade + " ") + r.note);
    }
}

} /* namespace aw */
