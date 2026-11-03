/* kh_hangul_t48.h — t48b: 한글 조합기 (두벌식, 화면 키보드·물리 키 공용). 헤더만, 외부 의존 없음(표준 라이브러리만).
   자모 단위 보관: cur = 지금 조합 중인 글자의 「단자모」 목록(겹받침·겹모음도 낱자로 들고 있어 지우기가 한 번에 한 자모씩 준다).
   확정된 글자는 buf(UTF-8)에 쌓인다. 호환 자모 U+3131~U+3163 만 쓴다(글꼴에 있는 KS X 1001 자모 영역). */
#pragma once
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>

namespace hg {

static const unsigned kLead[19]  = { 0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E };
static const unsigned kVow[21]   = { 0x314F, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155, 0x3156, 0x3157, 0x3158, 0x3159, 0x315A, 0x315B, 0x315C, 0x315D, 0x315E, 0x315F, 0x3160, 0x3161, 0x3162, 0x3163 };
static const unsigned kTail[28]  = { 0, 0x3131, 0x3132, 0x3133, 0x3134, 0x3135, 0x3136, 0x3137, 0x3139, 0x313A, 0x313B, 0x313C, 0x313D, 0x313E, 0x313F, 0x3140, 0x3141, 0x3142, 0x3144, 0x3145, 0x3146, 0x3147, 0x3148, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E };
struct Pair { unsigned a, b, r; };
static const Pair kVowPair[7] = { { 0x3157, 0x314F, 0x3158 }, { 0x3157, 0x3150, 0x3159 }, { 0x3157, 0x3163, 0x315A }, { 0x315C, 0x3153, 0x315D }, { 0x315C, 0x3154, 0x315E }, { 0x315C, 0x3163, 0x315F }, { 0x3161, 0x3163, 0x3162 } };
static const Pair kTailPair[11] = { { 0x3131, 0x3145, 0x3133 }, { 0x3134, 0x3148, 0x3135 }, { 0x3134, 0x314E, 0x3136 }, { 0x3139, 0x3131, 0x313A }, { 0x3139, 0x3141, 0x313B }, { 0x3139, 0x3142, 0x313C }, { 0x3139, 0x3145, 0x313D }, { 0x3139, 0x314C, 0x313E }, { 0x3139, 0x314D, 0x313F }, { 0x3139, 0x314E, 0x3140 }, { 0x3142, 0x3145, 0x3144 } };

inline bool is_cons(unsigned j) { return j >= 0x3131 && j <= 0x314E; }
inline bool is_vow(unsigned j) { return j >= 0x314F && j <= 0x3163; }
inline int lead_idx(unsigned j) { for (int i = 0; i < 19; i++) if (kLead[i] == j) return i; return -1; }
inline int vow_idx(unsigned j) { for (int i = 0; i < 21; i++) if (kVow[i] == j) return i; return -1; }
inline int tail_idx(unsigned j) { for (int i = 1; i < 28; i++) if (kTail[i] == j) return i; return -1; }
inline unsigned pair_of(const Pair * t, int n, unsigned a, unsigned b) { for (int i = 0; i < n; i++) if (t[i].a == a && t[i].b == b) return t[i].r; return 0; }
inline void put_utf8(std::string & s, unsigned cp) {
    if (cp < 0x80) s += (char)cp;
    else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
    else { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
}
/* 마지막 UTF-8 글자 하나의 시작 위치와 코드 */
inline bool last_cp(const std::string & s, size_t & pos, unsigned & cp) {
    if (s.empty()) return false; size_t p = s.size() - 1; while (p > 0 && ((unsigned char)s[p] & 0xC0) == 0x80) p--;
    const unsigned char * u = (const unsigned char *)s.c_str() + p; size_t n = s.size() - p;
    if (n == 1) cp = u[0]; else if (n == 2) cp = ((u[0] & 0x1F) << 6) | (u[1] & 0x3F); else if (n == 3) cp = ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F); else cp = 0xFFFD;
    pos = p; return true;
}
/* 완성형 한 글자 → 단자모 목록(겹받침·겹모음은 낱자로). 한글 음절이 아니면 false */
inline void split_vow(unsigned v, std::vector<unsigned> & out) { for (int i = 0; i < 7; i++) if (kVowPair[i].r == v) { out.push_back(kVowPair[i].a); out.push_back(kVowPair[i].b); return; } out.push_back(v); }
inline void split_tail(unsigned t, std::vector<unsigned> & out) { for (int i = 0; i < 11; i++) if (kTailPair[i].r == t) { out.push_back(kTailPair[i].a); out.push_back(kTailPair[i].b); return; } out.push_back(t); }
inline bool decompose(unsigned cp, std::vector<unsigned> & out) {
    out.clear();
    if (cp >= 0xAC00 && cp <= 0xD7A3) { unsigned x = cp - 0xAC00; int l = (int)(x / 588), v = (int)((x % 588) / 28), t = (int)(x % 28); out.push_back(kLead[l]); split_vow(kVow[v], out); if (t) split_tail(kTail[t], out); return true; }
    if (is_vow(cp)) { split_vow(cp, out); return true; }
    if (is_cons(cp)) { split_tail(cp, out); return true; }   /* ㄳ 같은 겹받침 호환 자모도 낱자로 */
    return false;
}

/* 조합 중 목록 → 한 글자. 완성 글자가 안 되면 호환 자모 그대로(자음·모음 단독) */
struct Parsed { unsigned lead = 0; std::vector<unsigned> v, t; };
inline Parsed parse(const std::vector<unsigned> & cur) {
    Parsed p; size_t i = 0; if (i < cur.size() && is_cons(cur[i])) p.lead = cur[i++];
    while (i < cur.size() && is_vow(cur[i])) p.v.push_back(cur[i++]);
    while (i < cur.size() && is_cons(cur[i])) p.t.push_back(cur[i++]);
    return p;
}
inline unsigned join_vow(const std::vector<unsigned> & v) { if (v.empty()) return 0; if (v.size() == 1) return v[0]; return pair_of(kVowPair, 7, v[0], v[1]); }
inline unsigned join_tail(const std::vector<unsigned> & t) { if (t.empty()) return 0; if (t.size() == 1) return t[0]; return pair_of(kTailPair, 11, t[0], t[1]); }
inline std::string cur_text(const std::vector<unsigned> & cur) {
    std::string s; if (cur.empty()) return s; Parsed p = parse(cur);
    if (p.lead && !p.v.empty()) {
        int l = lead_idx(p.lead), v = vow_idx(join_vow(p.v)); unsigned tj = join_tail(p.t); int t = tj ? tail_idx(tj) : 0;
        if (l >= 0 && v >= 0 && t >= 0) { put_utf8(s, 0xAC00 + (unsigned)((l * 21 + v) * 28 + t)); return s; }
    }
    for (size_t i = 0; i < cur.size(); i++) put_utf8(s, cur[i]);   /* 단독 자음/모음(여기서는 한 자모뿐) */
    if (!p.lead && !p.v.empty() && p.t.empty()) { s.clear(); put_utf8(s, join_vow(p.v)); }
    return s;
}

/* 두벌식 자판: 영문 키 → 자모. shift 는 쌍자음·ㅒㅖ 만 바뀐다 */
inline unsigned key_to_jamo(char k, bool shift) {
    char c = (k >= 'A' && k <= 'Z') ? (char)(k + 32) : k;
    switch (c) {
    case 'q': return shift ? 0x3143 : 0x3142; case 'w': return shift ? 0x3149 : 0x3148; case 'e': return shift ? 0x3138 : 0x3137;
    case 'r': return shift ? 0x3132 : 0x3131; case 't': return shift ? 0x3146 : 0x3145; case 'y': return 0x315B; case 'u': return 0x3155; case 'i': return 0x3151;
    case 'o': return shift ? 0x3152 : 0x3150; case 'p': return shift ? 0x3156 : 0x3154;
    case 'a': return 0x3141; case 's': return 0x3134; case 'd': return 0x3147; case 'f': return 0x3139; case 'g': return 0x314E;
    case 'h': return 0x3157; case 'j': return 0x3153; case 'k': return 0x314F; case 'l': return 0x3163;
    case 'z': return 0x314B; case 'x': return 0x314C; case 'c': return 0x314A; case 'v': return 0x314D; case 'b': return 0x3160; case 'n': return 0x315C; case 'm': return 0x3161;
    default: return 0; }
}

struct Composer {
    std::string buf;                 /* 확정된 글자(UTF-8) */
    std::vector<unsigned> cur;       /* 조합 중 단자모 */
    bool han = true;                 /* 한/영 */
    size_t maxb = 60;                /* 전체 최대 바이트(확정 + 조합 중) */

    std::string text() const { return buf + cur_text(cur); }
    void flush() { if (!cur.empty()) { buf += cur_text(cur); cur.clear(); } }
    void commit_new() { flush(); }
    bool fits() const { return text().size() <= maxb; }
    /* 자모 하나를 친다. 되돌릴 수 있게 상태를 복사해 두었다가 60바이트를 넘으면 원상 복구 */
    bool key_jamo(unsigned j) {
        std::string ob = buf; std::vector<unsigned> oc = cur; apply_jamo(j);
        if (!fits()) { buf = ob; cur = oc; return false; } return true;
    }
    bool put_ascii(char ch) {
        std::string ob = buf; std::vector<unsigned> oc = cur; flush(); buf += ch;
        if (!fits()) { buf = ob; cur = oc; return false; } return true;
    }
    bool put_utf8_text(const std::string & s) {
        std::string ob = buf; std::vector<unsigned> oc = cur; flush(); buf += s;
        if (!fits()) { buf = ob; cur = oc; return false; } return true;
    }
    void toggle() { flush(); han = !han; }
    /* 지우기: 조합 중이면 마지막 자모, 없으면 확정 글자를 다시 자모로 풀어 마지막 자모 */
    void back() {
        if (cur.empty()) {
            size_t pos; unsigned cp; if (!last_cp(buf, pos, cp)) return;
            std::vector<unsigned> d; buf.resize(pos); if (!decompose(cp, d)) return;   /* 자모가 아닌 글자(영문·숫자·공백)는 통째로 */
            cur = d;
        }
        if (!cur.empty()) cur.pop_back();
    }
private:
    void apply_jamo(unsigned j) {
        if (cur.empty()) { cur.push_back(j); return; }
        Parsed p = parse(cur);
        if (is_vow(j)) {
            if (p.lead && p.v.empty()) { cur.push_back(j); return; }                               /* 자음 + 모음 */
            if (p.v.size() == 1 && p.t.empty() && pair_of(kVowPair, 7, p.v[0], j)) { cur.push_back(j); return; }   /* 겹모음 */
            if (p.lead && !p.v.empty() && !p.t.empty()) {                                           /* 받침 뒤 모음: 받침(의 끝 자모)이 다음 글자 첫소리로 */
                unsigned mv = cur.back(); cur.pop_back(); flush(); cur.push_back(mv); cur.push_back(j); return;
            }
            flush(); cur.push_back(j); return;                                                    /* 그 밖: 지금 것 확정, 모음 단독으로 새로 */
        }
        /* 자음 */
        if (p.lead && !p.v.empty()) {
            if (p.t.empty()) { if (tail_idx(j) > 0) { cur.push_back(j); return; } flush(); cur.push_back(j); return; }   /* ㄸㅃㅉ 는 받침 불가 */
            if (p.t.size() == 1 && pair_of(kTailPair, 11, p.t[0], j)) { cur.push_back(j); return; }                 /* 겹받침 */
        }
        flush(); cur.push_back(j);
    }
};

/* ---- 자체 시험표. 토큰: 영문 소문자 = 키, ^ + 영문 = SHIFT 키, _ = 띄우기, < = 지우기, ! = 한/영, 그 밖(숫자 등) = 그대로 입력 */
inline std::string run_script(const char * script, bool * han_end = NULL) {
    Composer c; bool shift = false; (void)han_end;
    for (const char * p = script; *p; p++) {
        char ch = *p;
        if (ch == '^') { shift = true; continue; }
        if (ch == '!') { c.toggle(); shift = false; continue; }
        if (ch == '<') { c.back(); shift = false; continue; }
        if (ch == '_') { c.put_ascii(' '); shift = false; continue; }
        if (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))) {
            if (c.han) c.key_jamo(key_to_jamo(ch, shift || (ch >= 'A' && ch <= 'Z')));
            else c.put_ascii(shift && ch >= 'a' && ch <= 'z' ? (char)(ch - 32) : ch);
            shift = false; continue;
        }
        c.put_ascii(ch); shift = false;
    }
    return c.text();
}
struct TestRow { const char * label; const char * input; const char * expect; };
inline const TestRow * test_rows(int & n) {
    static const TestRow T[] = {
        { "환세 테스트", "ghkstp_xptmxm", "환세 테스트" },
        { "닭 (겹받침 ㄺ)", "ekfr", "닭" },
        { "값 (겹받침 ㅄ)", "rkqt", "값" },
        { "뷁 (겹모음 ㅞ + 겹받침 ㄺ)", "qnpfr", "뷁" },
        { "까치 (SHIFT r k c l)", "^rkcl", "까치" },
        { "ㅒ (SHIFT o)", "^o", "ㅒ" },
        { "얘 (d SHIFT o)", "d^o", "얘" },
        { "받침이 다음 글자로 (갑시다 → 가+ㅂ+ㅅ+ㅣ)", "rkqtl", "갑시" },
        { "겹받침이 갈라짐 (닭+ㅣ → 달기)", "ekfrl", "달기" },
        { "BS 닭 → 달", "ekfr<", "달" },
        { "BS 닭 → 다", "ekfr<<", "다" },
        { "BS 닭 → ㄷ", "ekfr<<<", "ㄷ" },
        { "BS 닭 → 빈 문자열", "ekfr<<<<", "" },
        { "BS 뷁 → 뷀 → 뷔 → ... 한 번에 한 자모", "qnpfr<", "뷀" },
        { "BS 확정 글자 되살려 자모 단위 (환+ㅅ 에서 BS 2 → 화)", "ghkst<<", "화" },
        { "BS 확정 글자 되살려 자모 단위 (BS 3 → 호)", "ghkst<<<", "호" },
        { "BS 확정 글자 되살려 자모 단위 (까치 BS 1)", "^rkcl<", "까ㅊ" },
        { "BS 확정 글자 되살려 자모 단위 (까치 BS 2)", "^rkcl<<", "까" },
        { "BS 겹모음 (과 → 고)", "rhk<", "고" },
        { "한/영 섞기 abc한1", "!abc!gks!1", "abc한1" },
        { "영문 SHIFT 한 글자 (^a b)", "!^ab", "Ab" },
        { "모음 단독 + 모음 겹침 (ㅗㅏ → ㅘ)", "hk", "ㅘ" },
        { "ㅃㅉㄸ 은 받침이 못 됨 (가 SHIFT q → 가ㅃ)", "rk^q", "가ㅃ" },
        { "ㄲ 받침 (각 → 갂 아님: r k SHIFT r → 갂)", "rk^r", "갂" },
        { "공백 뒤 새 글자", "gks_gks", "한 한" },
        { "빈 입력 BS", "<", "" },
        { "정의: 의 (ㅇ ㅡ ㅣ)", "dml", "의" },
    };
    n = (int)(sizeof T / sizeof T[0]); return T;
}
/* 60바이트 한계: 한글 21자(63바이트) 입력 시 20자(60바이트)에서 멈춘다 */
inline bool test_limit(std::string & detail) {
    Composer c; int ok = 0; for (int i = 0; i < 25; i++) { c.key_jamo(0x3131); c.key_jamo(0x314F); if (c.fits()) ok++; }
    c.flush(); detail = "입력 25회 시도 → " + std::to_string(c.buf.size()) + " 바이트 (한계 60)"; return c.buf.size() <= 60 && c.buf.size() >= 57;
}
/* 모든 표를 TSV 문자열로: 행 = 이름 \t 입력 \t 기대 \t 실제 \t 결과 */
inline std::string selftest(int & pass, int & total) {
    std::string out = "이름\t입력 열\t기대\t실제\t결과\n"; int n; const TestRow * T = test_rows(n); pass = 0; total = 0;
    for (int i = 0; i < n; i++) { std::string got = run_script(T[i].input); bool ok = got == T[i].expect; total++; if (ok) pass++; out += std::string(T[i].label) + "\t" + T[i].input + "\t" + T[i].expect + "\t" + got + "\t" + (ok ? "PASS" : "FAIL") + "\n"; }
    std::string d; bool ok = test_limit(d); total++; if (ok) pass++; out += "60바이트 한계\t(가 25회)\t57~60바이트\t" + d + "\t" + (ok ? "PASS" : "FAIL") + "\n";
    return out;
}

}  /* namespace hg */
