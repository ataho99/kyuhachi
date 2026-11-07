/* kh_nkdb.h -- r017 (#239 5번): Neo Kobe dat 기반 게임 DB(gamedb_nk.tsv) 읽기 + 변형(패치판 추정) 판정.
 * 호스트(Windows)·시험 도구(Linux) 공용. 입력은 prof::ImgInfo 목록(경로·크기·sha1·형식·라벨·FAT 서명).
 * 판정 등급:  A = 모든 이미지 sha1 일치(원판)  B = 크기·구성이 원판과 같고 제목/라벨·실행파일명/크기 유일성 중 하나 이상이 받쳐 줌(변형, 패치판 추정)
 *            C = 크기만 같고 근거가 약함(후보)  - = 근거 없음 */
#pragma once
#include "settings/kh_prof.h"
#include <unordered_map>
#include <set>

namespace nk {

struct Disk { std::string sha1, name, role, fmt; uint64_t size = 0; bool alt = false; };
struct Game {
    std::string gid, title, media, year, maker, ysrc, msrc, flags, bootseq, tkey;
    int ndisk = 0; std::vector<Disk> disks;
    bool has_flag(const char * f) const { return ("," + flags + ",").find(std::string(",") + f + ",") != std::string::npos; }
};
struct Sig { std::string label, exes; };
struct Db {
    std::string ver_nk, ver_tosec; std::vector<Game> g;
    std::unordered_map<std::string, std::pair<int, int> > bysha;      // sha1 -> (게임, 디스크)
    std::unordered_map<uint64_t, std::vector<int> > bysize;           // 크기 -> 게임들
    std::unordered_map<std::string, std::vector<Sig> > sigs;          // gid -> 서명들
    bool loaded = false;
};

/* 제목 정규화(생성기의 norm 과 같은 규칙) */
inline std::string normkey(const std::string & s) {
    std::string o; for (unsigned char c : s) { if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a'); if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o += (char)c; }
    auto rep = [&](const char * a, const char * b) { size_t p; while ((p = o.find(a)) != std::string::npos) o.replace(p, strlen(a), b); };
    rep("ou", "o"); rep("oo", "o"); rep("uu", "u"); rep("ei", "e"); return o;
}
inline std::vector<std::string> tabs(const std::string & ln) { std::vector<std::string> v; size_t p = 0; for (;;) { size_t q = ln.find('\t', p); if (q == std::string::npos) { v.push_back(ln.substr(p)); break; } v.push_back(ln.substr(p, q - p)); p = q + 1; } return v; }

inline bool load(const std::string & path, Db & db) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
    std::string data; { char b[1 << 16]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) data.append(b, n); } fclose(f);
    db = Db(); size_t pos = 0;
    while (pos < data.size()) {
        size_t e = data.find('\n', pos); if (e == std::string::npos) e = data.size();
        std::string ln = data.substr(pos, e - pos); pos = e + 1; if (!ln.empty() && ln.back() == '\r') ln.pop_back(); if (ln.empty()) continue;
        std::vector<std::string> t = tabs(ln);
        if (t[0] == "#GENSEI-NKDB") { for (auto & x : t) { if (x.compare(0, 3, "nk=") == 0) db.ver_nk = x.substr(3); if (x.compare(0, 6, "tosec=") == 0) db.ver_tosec = x.substr(6); } continue; }
        if (t[0] == "G" && t.size() >= 10) {
            Game g; g.gid = t[1]; g.title = t[2]; g.media = t[3]; g.year = t[4]; g.maker = t[5]; g.ysrc = t[6]; g.msrc = t[7]; g.flags = t[8]; g.ndisk = atoi(t[9].c_str()); if (t.size() > 10) g.bootseq = t[10];
            g.tkey = normkey(g.title); db.g.push_back(g);
        } else if (t[0] == "D" && t.size() >= 7 && !db.g.empty()) {
            Disk d; d.sha1 = t[1]; d.size = strtoull(t[2].c_str(), NULL, 10); d.name = t[3]; d.role = t[4]; d.fmt = t[5]; d.alt = t[6] == "1";
            Game & g = db.g.back(); g.disks.push_back(d); int gi = (int)db.g.size() - 1, di = (int)g.disks.size() - 1;
            if (!db.bysha.count(d.sha1)) db.bysha[d.sha1] = std::make_pair(gi, di);
            std::vector<int> & v = db.bysize[d.size]; if (v.empty() || v.back() != gi) v.push_back(gi);
        }
    }
    db.loaded = !db.g.empty(); return db.loaded;
}
/* 서명 파일: S<TAB>gid<TAB>볼륨라벨<TAB>실행파일들(콤마) — --sigscan 이 원판 폴더에서 만든다 */
inline bool load_sig(const std::string & path, Db & db) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
    std::string data; { char b[1 << 14]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) data.append(b, n); } fclose(f);
    size_t pos = 0; int n = 0;
    while (pos < data.size()) {
        size_t e = data.find('\n', pos); if (e == std::string::npos) e = data.size();
        std::string ln = data.substr(pos, e - pos); pos = e + 1; if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        std::vector<std::string> t = tabs(ln); if (t.size() >= 4 && t[0] == "S") { Sig s; s.label = t[2]; s.exes = t[3]; db.sigs[t[1]].push_back(s); n++; }
    }
    return n > 0;
}

/* 파일/폴더 이름 → 비교용 제목 문자열 (확장자·괄호·대괄호 묶음 제거) */
inline std::string strip_name(const std::string & s) {
    std::string o; int dp = 0; size_t dot = s.find_last_of('.'); std::string b = (dot != std::string::npos && s.size() - dot <= 5) ? s.substr(0, dot) : s;
    for (char c : b) { if (c == '(' || c == '[') { dp++; continue; } if (c == ')' || c == ']') { if (dp > 0) dp--; continue; } if (!dp) o += c; }
    return o;
}
inline std::vector<std::string> words(const std::string & s) {
    std::vector<std::string> w; std::string cur; for (unsigned char c : s) { if (isalnum(c)) cur += (char)tolower(c); else { if (cur.size() >= 3) w.push_back(cur); cur.clear(); } } if (cur.size() >= 3) w.push_back(cur); return w;
}
inline double tsim(const std::string & name, const std::string & title) {
    std::string a = normkey(strip_name(name)), b = normkey(title);
    if (a.empty() || b.empty()) return 0.0;
    if (a == b) return 1.0;
    if (std::min(a.size(), b.size()) >= 4 && (a.find(b) != std::string::npos || b.find(a) != std::string::npos)) return 0.9;
    std::vector<std::string> wa = words(strip_name(name)), wb = words(title); if (wa.empty() || wb.empty()) return 0.0;
    std::set<std::string> A(wa.begin(), wa.end()), B(wb.begin(), wb.end()); int inter = 0; for (auto & x : A) if (B.count(x)) inter++;
    int uni = (int)(A.size() + B.size()) - inter; return uni ? (double)inter / uni : 0.0;
}
inline int exe_overlap(const std::string & a, const std::string & b) {   // 콤마 목록 둘의 겹치는 개수
    if (a.empty() || b.empty()) return 0; std::set<std::string> A; { size_t p = 0; while (p <= a.size()) { size_t q = a.find(',', p); if (q == std::string::npos) q = a.size(); A.insert(a.substr(p, q - p)); p = q + 1; } }
    int n = 0; size_t p = 0; while (p <= b.size()) { size_t q = b.find(',', p); if (q == std::string::npos) q = b.size(); if (A.count(b.substr(p, q - p))) n++; p = q + 1; } return n;
}

struct Result {
    char grade = '-'; int game = -1; int exact = 0, total = 0; bool variant = false;
    double ts = 0; bool uniq = false, sigok = false, comp = false; int szmatch = 0;
    std::string text, shortt;       // 인포메이션용 판정 문장 · 짧은 표시
};

/* imgs: 작품 하나를 이루는 이미지들(sha1 은 비어 있을 수 있음). hint = 폴더 이름 등 제목 단서(빈칸 가능). */
inline Result match(const Db & db, const std::vector<prof::ImgInfo> & imgs, const std::string & hint) {
    Result R; R.total = (int)imgs.size(); if (!db.loaded || imgs.empty()) { R.text = "-"; R.shortt = "-"; return R; }
    // 1) 해시 일치
    std::map<int, int> votes;
    for (auto & im : imgs) { if (im.sha1.empty()) continue; auto it = db.bysha.find(im.sha1); if (it != db.bysha.end()) votes[it->second.first]++; }
    int bestg = -1, bestv = 0; for (auto & kv : votes) if (kv.second > bestv) { bestv = kv.second; bestg = kv.first; }
    if (bestg >= 0 && bestv == R.total) {
        const Game & g = db.g[bestg]; R.grade = 'A'; R.game = bestg; R.exact = bestv;
        R.text = khtr::fmt(khtr::tr("원판과 같은 파일 — %s [%s] (DB 일치)"), g.title.c_str(), g.media.c_str()); R.shortt = khtr::tr("확인됨 (DB 일치)"); return R;
    }
    // 2) 변형: 크기·구성·제목·서명
    std::set<int> cand; if (bestg >= 0) cand.insert(bestg);
    for (auto & im : imgs) for (uint64_t sz : { im.size, im.payload }) { if (!sz) continue; auto it = db.bysize.find(sz); if (it != db.bysize.end() && it->second.size() <= 400) for (int gi : it->second) cand.insert(gi); }
    int nfd = 0, nhd = 0; for (auto & im : imgs) { if (im.hdd) nhd++; else nfd++; }
    double bestscore = -1; int bg = -1; Result bestr;
    for (int gi : cand) {
        const Game & g = db.g[gi]; if (g.media == "EXTRA" || g.media == "ROM") continue;
        Result r; r.game = gi; r.total = R.total;
        int sm = 0, ex = 0; double ts = 0; bool sigok = false;
        for (auto & im : imgs) {
            bool hit = false;
            if (!im.sha1.empty()) { auto it = db.bysha.find(im.sha1); if (it != db.bysha.end() && it->second.first == gi) { hit = true; ex++; } }
            if (!hit) for (auto & d : g.disks) if (d.size == im.size || (im.payload && d.size == im.payload)) { hit = true; break; }
            if (hit) sm++;
            double t = tsim(im.base, g.title); if (t > ts) ts = t;
            auto si = db.sigs.find(g.gid);
            if (si != db.sigs.end()) for (auto & s : si->second) { if (!im.vlabel.empty() && im.vlabel == s.label) sigok = true; if (exe_overlap(s.exes, im.exes) >= 2) sigok = true; }
        }
        if (!hint.empty()) { double t = tsim(hint, g.title); if (t > ts) ts = t; }
        bool comp;
        if (g.media == "HD") comp = nhd >= 1; else if (g.media == "FD" || g.media == "CDFD") comp = nfd >= std::max(1, g.ndisk) && nfd <= g.ndisk + 2; else comp = false;
        std::set<std::string> tk; for (auto & im : imgs) for (uint64_t sz : { im.size, im.payload }) { auto it = db.bysize.find(sz); if (it != db.bysize.end()) for (int x : it->second) tk.insert(db.g[x].tkey); }
        bool uniq = false; { // 이미지 하나(보통 HDD)가 가진 크기를 가진 제목이 하나뿐
            if (imgs.size() == 1 && imgs[0].hdd) { std::set<std::string> t1; for (uint64_t sz : { imgs[0].size, imgs[0].payload }) { auto it = db.bysize.find(sz); if (it != db.bysize.end()) for (int x : it->second) if (db.g[x].media == "HD") t1.insert(db.g[x].tkey); } uniq = (t1.size() == 1 && t1.count(g.tkey)); } }
        double szfrac = (double)sm / R.total;
        r.szmatch = sm; r.ts = ts; r.uniq = uniq; r.sigok = sigok; r.comp = comp; r.exact = ex;
        if (szfrac >= 1.0 && comp && (ts >= 0.5 || sigok || uniq)) r.grade = 'B';
        else if (szfrac >= 0.5 && (ts >= 0.4 || uniq || sigok)) r.grade = 'C';
        else continue;
        double score = 3 * szfrac + 2 * ts + (comp ? 1 : 0) + (sigok ? 2 : 0) + (uniq ? 1.5 : 0) + 2.0 * ex / R.total + (r.grade == 'B' ? 3 : 0);
        if (g.media == "HD" && nhd >= 1) score += 0.2; if (g.media == "FD" && nhd == 0) score += 0.2;
        if (g.gid.find("-set-") != std::string::npos) score -= 0.1;
        if (score > bestscore) { bestscore = score; bg = gi; bestr = r; }
    }
    if (bg >= 0) {
        R = bestr; R.variant = true; const Game & g = db.g[bg];
        std::string ev; char b[64];
        if (R.exact > 0) { snprintf(b, sizeof b, khtr::tr("일부 해시 일치 %d/%d"), R.exact, R.total); ev += b; }
        { snprintf(b, sizeof b, khtr::tr("크기 %d/%d"), R.szmatch, R.total); if (!ev.empty()) ev += ", "; ev += b; }
        if (R.comp) ev += std::string(", ") + khtr::tr("구성 일치");
        if (R.ts >= 0.5) { snprintf(b, sizeof b, khtr::tr("제목 유사 %d%%"), (int)(R.ts * 100)); ev += ", "; ev += b; }
        if (R.sigok) ev += std::string(", ") + khtr::tr("라벨/실행파일 일치");
        if (R.uniq) ev += std::string(", ") + khtr::tr("크기 유일");
        if (R.grade == 'B') { R.text = khtr::fmt(khtr::tr("원판 %s [%s]의 변형(패치판 추정) — %s"), g.title.c_str(), g.media.c_str(), ev.c_str()); R.shortt = khtr::fmt(khtr::tr("추정 (원판 %s의 변형)"), g.title.c_str()); }
        else { R.text = khtr::fmt(khtr::tr("원판 후보 %s [%s] (확실하지 않음) — %s"), g.title.c_str(), g.media.c_str(), ev.c_str()); R.shortt = khtr::fmt(khtr::tr("후보 (원판 %s 추정, 확실하지 않음)"), g.title.c_str()); }
        return R;
    }
    R.text = khtr::tr("- 판정 근거 없음 (DB 에 없는 작품이거나 크기·구성이 다름)"); R.shortt = "-"; return R;
}

/* 실행 불가 사유(빈칸 = 실행 가능): NK 항목 + 프로필. curated = game.ini/게임DB 로 직접 정해진 작품(NK 의 설치 표시로 막지 않는다) */
inline std::string nk_block(const Db & db, const Result & r, bool curated) {
    if (r.game < 0 || r.game >= (int)db.g.size() || r.grade == 'C' || r.grade == '-') return "";
    const Game & g = db.g[r.game];
    if (g.media == "CD" || g.media == "CDFD") return khtr::tr("CD 작품입니다 — CD 드라이브 지원 전이라 아직 실행할 수 없습니다");
    if (!curated && g.has_flag("inst")) return khtr::tr("하드디스크 설치가 필요한 작품입니다 (설치 단계 지원 전)");
    return "";
}

} /* namespace nk */
