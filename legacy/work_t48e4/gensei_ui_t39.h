/* gensei_ui.h -- r012 UI 화면 상태기계. gensei_ov.h 위에서 그린다. 입력은 추상 키(UK_*)로 받는다.
 * 호스트가 UiGame 표(제목·디스크명·THMB·슬롯 정보)를 채워 넣고, ui_key() 의 반환 Act 를 처리한다. */
#pragma once
#include "gensei_ov.h"
#include <stdlib.h>
#include <algorithm>
#include <map>
#include <time.h>
#include <string.h>
#include <functional>
#include "gensei_ent.h"
#include "gensei_fontreg.h"
#include "gensei_hangul_t48.h"   /* t48b: 한글 조합기 */

namespace ui {
using namespace ov;

enum UKey { UK_NONE, UK_UP, UK_DOWN, UK_LEFT, UK_RIGHT, UK_OK, UK_CANCEL, UK_TRI, UK_SQ, UK_SELECT, UK_L, UK_R, UK_ANY };
enum ActT { A_NONE, A_START, A_LOAD, A_SAVE, A_RESUME, A_MAINMENU, A_QUIT, A_SENDKEY, A_DISKSWAP, A_APPLY, A_DELSLOT, A_EXTRA, A_ADD, A_WINMULT, A_SHOT, A_TEST, A_SETLANG, A_KBD, A_KBBTN, A_KBSH };   /* t44a (#494): A_SETLANG(game=작품 index, arg 0 자동 1 한글판 2 원판 일본어) */   /* r017: A_ADD(arg 0 파일 1 폴더 2 게임폴더 지정 3 다시 검색 4 해제) · A_WINMULT(arg=배율, 0=사용자 지정) */
struct Act { ActT t = A_NONE; int game = -1, slot = 0, arg = 0; bool resume = false; std::string entry; };   /* t39: entry = 실행할 항목 이름(빈칸 = 폴더 기본, 「-」 = 항목 없이) */

/* ---- 설정 (전역 + 작품별 덮어쓰기) */
struct Settings {
    int ratio = 0;      /* 0 4:3  1 16:10  2 정수배 */
    int shader = 0;     /* t48c: 필터 0 sharp 2 nearest. 1 = 옛 crt 프리셋: 파일에서 읽을 때만 나오며 「sharp + 효과 켬」으로 바뀐다 */
    int curve = 0;      /* 0..10 → 곡면 0.00..0.10 */
    int scan = 0;      /* t48c: 주사선 0..10 (0 = 끔, 1..10 → 옛 SCANLINE 2+n) */       /* 0..10 */
    int mask = 0;      /* t48c: 마스크 0..10 (0 = 끔, n → 옛 CGWG n*0.05) */       /* 0 꺼짐 1 약 2 중 3 강 */
    int glow = 0;       /* t48c (#625): 빛 번짐 0..10 (0 = 끔) */
    int bleed = 0;      /* t48c: 이제 「RGB 번짐」 0..10 (컨버전스 어긋남 + 가로 번짐). 옛 「번짐」은 RGB 번짐 + 빛 번짐 같은 값으로 변환 */      /* t42(#311): 0..10 → 번짐(실물 RGB 모니터 결함: 컨버전스·가로 번짐·형광체 번짐) 0.0..1.0, 0 = 끔 */
    int bgm = 8;        /* 0..8 */
    int se = 8;         /* 0..8 */
    int device = 0;     /* 0 기본 장치 */
    int pad = 0;        /* 0 일본식 ○결정 */
    int bmult = 1;      /* 0..4 = x2 x3 x4 x6 x8 */
    int bmode = 2;      /* 0 홀드 1 토글 2 둘 다 */
    int boot = 0;       /* 0 켬 1 끔 */
    int bosd = 1;       /* 배속 표시(오른쪽 위 ▶▶) 0 끔 1 켬 */
    int kbshader = 0;   /* t46z (#548): 화면 키보드·버튼·배속·OSD 에 셰이더 적용 0 안 함(기본) 1 적용 */
    int kbbtn = 1;      /* t46x: 화면 키보드 토글 버튼 표시 0 숨김 1 표시 */
    int bsound = 2;     /* r017 배속 중 소리: 0 끔 1 빠르게(음높이 상승) 2 음높이 유지 */
    bool lmode = false, lseen = false; int l_sh = 0, l_scan = 5, l_mask = 1, l_bleed = 0, l_curve = 4;   /* t48c: 옛 형식(fx=1 표지 없음) 파일을 읽는 동안만 쓰는 칸. 끝나면 fx_finish 가 새 칸으로 옮긴다 */
    float curve_f() const { return curve * 0.01f; }
    float glow_f() const { return glow * 0.1f; }
    float bleed_f() const { return bleed * 0.1f; }
    float scan_f() const { return scan / 10.0f; }
    float mask_f() const { return (mask < 0 ? 0 : mask > 10 ? 10 : mask) * 0.05f; }   /* t48c: 옛 마스크 1/2/3 = 0.15/0.3/0.5 = 새 3/6/10 단계 */
    int boost_x() const { static const int x[5] = { 2, 3, 4, 6, 8 }; return x[bmult < 0 ? 0 : bmult > 4 ? 4 : bmult]; }
    int bs_gain256() const { return boost_x() >= 3 ? 205 : 256; }   /* x3 이상은 조금 낮춤(-2 dB) */
    float se_gain() const { return se / 8.0f; }
    float bgm_gain() const { return bgm / 8.0f; }
};
/* t48c (#639 ①): 옛 설정(shader=1 crt 프리셋 + scan/mask/bleed) → 새 설정(필터 + 효과). 같은 그림이 나오게 옮긴다 */
static inline int fx_mask_leg(int m) { static const int t[4] = { 0, 3, 6, 10 }; return t[m < 0 ? 0 : m > 3 ? 3 : m]; }
inline void fx_begin(Settings & s) { s.lmode = true; s.lseen = false; s.l_sh = 0; s.l_scan = 5; s.l_mask = 1; s.l_bleed = 0; s.l_curve = 4; }
inline void fx_finish(Settings & s) {
    if (!s.lmode) return; s.lmode = false; if (!s.lseen) return; s.lseen = false;
    if (s.l_sh == 1) { s.shader = 0; s.scan = s.l_scan < 1 ? 1 : s.l_scan; s.mask = fx_mask_leg(s.l_mask); s.bleed = s.l_bleed; s.glow = s.l_bleed; s.curve = s.l_curve; }   /* 옛 번짐 = RGB 번짐 + 빛 번짐 같은 값(가장자리 가중 컨버전스 포함, 화소 동일) */
    else { s.shader = (s.l_sh == 2) ? 2 : 0; s.scan = 0; s.mask = 0; s.bleed = 0; s.glow = 0; s.curve = 0; }   /* 옛 sharp/nearest 에서는 효과가 안 먹었다 */
}
inline void apply_ov(Settings & s, const std::map<std::string, std::string> & m) {   /* 작품 프로필(DB·game.ini [override]) 값을 전역 위에 얹는다 */
    bool fxm = m.count("fx") != 0 || m.count("glow") != 0;   /* t48c: 새 형식 표지가 있으면 값 그대로, 없으면 옛 형식(shader=1 crt · scan/mask/bleed 옛 범위)으로 읽어 변환 */
    { std::map<std::string, std::string>::const_iterator it = m.find("shader"); if (it != m.end()) { int sv = atoi(it->second.c_str());
        if (fxm) s.shader = (sv == 2) ? 2 : 0;
        else if (sv == 1) { s.shader = 0; if (s.scan <= 0 && s.mask <= 0 && s.bleed <= 0 && s.glow <= 0) { s.scan = 5; s.mask = 3; } if (s.curve <= 0 && !m.count("curve")) s.curve = 4; }
        else { s.shader = (sv == 2) ? 2 : 0; s.scan = 0; s.mask = 0; s.bleed = 0; s.glow = 0; s.curve = 0; } } }
    auto cl = [](int v, int a, int b) { return v < a ? a : v > b ? b : v; };
    for (auto & kv : m) {
        int v = atoi(kv.second.c_str()); const std::string & k = kv.first;
        if (k == "ratio") s.ratio = cl(v, 0, 2); else if (k == "shader") { /* t48c: 위의 앞부분에서 처리 */ } else if (k == "curve") s.curve = cl(v, 0, 10); else if (k == "bleed") { s.bleed = cl(v, 0, 10); if (!fxm) s.glow = s.bleed; } else if (k == "glow") s.glow = cl(v, 0, 10);
        else if (k == "scan") s.scan = fxm ? cl(v, 0, 10) : cl(v, 1, 10); else if (k == "mask") s.mask = fxm ? cl(v, 0, 10) : fx_mask_leg(cl(v, 0, 3)); else if (k == "bgm") s.bgm = cl(v, 0, 8);
        else if (k == "se") s.se = cl(v, 0, 8); else if (k == "pad") s.pad = cl(v, 0, 3); else if (k == "boost") { static const int x[5] = { 2, 3, 4, 6, 8 }; int b = 1; for (int i = 0; i < 5; i++) if (x[i] == v) b = i; s.bmult = b; }
        else if (k == "boostmode") s.bmode = cl(v, 0, 2); else if (k == "bosd") s.bosd = cl(v, 0, 1); else if (k == "bsound") s.bsound = cl(v, 0, 2); else if (k == "kbbtn") s.kbbtn = cl(v, 0, 1); else if (k == "kbshader") s.kbshader = cl(v, 0, 1);
    }
}
struct SlotInfo { bool used = false; std::string stamp, play; int bytes = 0; std::vector<uint32_t> th; int tw_ = 0, th_ = 0; };
/* ---- t39: 목록(탭·정렬) · 실행 방법 편집용 자료형 */
struct FileItem { std::string name; bool dir = false; long long size = 0; };
struct UiEnt {   /* 작품의 실행 방법 한 가지 */
    std::string name, label, tag, lang;   /* tag: DB · game.ini · 내 것 · 수정됨 · 변형 */
    bool hidden = false, pseudo = false; int vgi = -1;   /* pseudo = 항목 정의 없음(자동 판정 한 가지) · vgi = 변형 폴더의 작품 index(-1 이면 이 폴더) */
    ent::Kv kv; int warn = -1;   /* warn: -1 미계산 · 0 정상 · 1 경고/오류 */ std::string warn_msg;
};
struct UiFont { std::string name, group, path, why; bool ok = false; };   /* 글꼴 폴더의 bmp 한 개 */
struct ValRow { std::string key, val, src, cov; };   /* t48a (#614 ③): 적용값 표 한 줄 = 키·최종값·출처·덮인 값 */
struct Env {   /* 호스트(또는 하니스)가 채우는 입출력 */
    char sep = '/';
    std::function<std::vector<FileItem>(const std::string &)> list_dir;
    std::function<std::vector<std::string>()> roots;
    std::function<bool(int, const std::string &)> has_img;
    std::function<bool(int, const std::string &, const ent::Kv &, const std::string &)> save;   /* (작품, 항목, 값, 기본 지정할 항목 이름 또는 빈칸) */
    std::function<bool(int, const std::string &, bool)> del;   /* (작품, 항목, 되돌리기만) */
    std::function<bool(int, const std::string &, const std::string &)> setkey;   /* user.ini [game] key=val */
    std::function<bool(int, int, std::string &)> hdd_make;   /* (작품, MB, 만든 파일 이름) */
    std::function<bool(int, const std::string &, const ent::Kv &)> save_test;   /* 임시 user.ini 에 시험용으로 */
    std::function<bool(int, ent::Kv &)> resolved;   /* 지금 실제 배치(자동 판정 포함) */
    std::function<void(int)> reload;   /* 저장 뒤 항목 다시 읽기 */
    std::function<std::string(const std::string &)> font_group;   /* 글꼴 이름 → 묶음 */
    std::function<bool(int, std::vector<ValRow> &)> values;   /* t48a (#614 ③): 작품의 적용값 표 */
    std::function<void(int, std::map<std::string, std::string> &)> pad_get;   /* t48e (#646): 작품 user.ini [pad] 의 btn_* */
    std::function<bool(int, const std::string &, const std::string &)> pad_set;   /* (작품, btn_ 키, 값; 빈 값 = 지움) */
    std::function<int()> padkind;   /* 0 그 밖 1 Xbox 2 PlayStation */
    std::function<std::vector<std::string>()> padkeys;   /* 고를 수 있는 키 이름(저장용, 소문자) */
};
struct UiGame {
    std::string id, title, boot_note; std::string disk[2];
    std::string company, year, genre, patch, devices, disknote;   /* gamedb 항목(인포메이션) */
    int play_s = 0; bool has_demo = false;   /* 플레이 시간(초) · 오프닝(데모) 디스크 유무 */
    std::vector<std::string> fds; std::string cur_fd[2];   /* 디스크 교체 목록(표시 이름) · 지금 A/B 에 들어 있는 것 */ std::map<std::string, std::string> ov; std::string src;
    std::vector<uint32_t> thmb; int tw_ = 0, th_ = 0;  /* 마지막 저장 THMB (0x00RRGGBB) */
    SlotInfo slot[8]; int lastslot = -1;
    /* r017: 판정·출처·실행 불가 */
    char grade = '-'; std::string judge, metasrc, block, jshort; bool autoreg = false;
    std::string lang_exp;   /* t44a (#494): 사용자 지정 언어(user.ini [game] lang=) — ko/jp, 비면 자동 */
    std::string more;   /* r019: 함께 있는 이미지(같은 작품의 다른 이미지) 목록 */
    std::string orig, dbhit;   /* r018: 원판 제목(변형 판정) · DB 일치한 작품 이름 */
    /* t39 */
    std::string lang = "jp", short_t, dir, imgdir, variant_of, fgroup, gfont, defent;   /* lang ko|jp · short= · 폴더 · 이미지 폴더 · 변형이면 기반 id · 글꼴 묶음(전용=custom) · 전용 글꼴 이름 · 기본 항목 */
    int base = -1; bool fav = false, need_install = false; long long last_run = 0; int order_n = 0;   /* base = 변형이면 기반 작품 index */
    std::vector<UiEnt> ents;
};

enum Scr { S_MAIN, S_PICK, S_SCREEN, S_CONF, S_SAVE, S_EXTRA, S_QUIT, S_GAME, S_KEYS, S_HELP, S_START, S_SWAP, S_SWAPI, S_ABOUT, S_ADD, S_SORT, S_ENT, S_ENTMENU, S_TPL, S_EDIT, S_FILE, S_KEYSEQ, S_STEP, S_SWAPED, S_FONT, S_OSK, S_NAME, S_YN, S_HDDASK, S_LANG, S_VALS, S_PAD, S_PADKEY };

struct Ui {
    Scr scr = S_MAIN, back = S_MAIN;
    int sel = 0;               /* 현재 화면 선택 줄 */
    int mainsel = 0, pick = 2, cur = 2;  /* cur = 현재 작품 index */
    int slotsel = 0;
    int quitsel = 0, startsel = 0, swapdrv = 0, swapsel = 0;
    static const int MAXG = 256;
    Settings g, t[MAXG]; bool has_t[MAXG] = { false };
    int scope = 0;             /* 0 전역 1 이 작품 */
    Settings backup; bool backup_has = false;   /* 화면 설정에서 × 로 되돌리기 */
    std::vector<UiGame> games; bool in_game = false; std::string build = "r012"; int kb_vis = 0, kb_btn = 1, kb_sh = 0;   /* t46x: 호스트가 메뉴를 열 때 채우는 화면 키보드 상태(메뉴 글자용) */
    bool dirty = false;
    int ts_ms = 0;             /* 애니메이션용 */
    /* r017: 창 배율(전역) · 작품 추가 */
    int wmult = 0, wmax = 3, winw = 0, winh = 0; int bk_wmult = 0, bk_winw = 0, bk_winh = 0;   /* wmult 0 = 사용자 지정(winw x winh) · wmax = 모니터에 들어가는 최대 배율 */
    std::string gamefolder; std::vector<std::string> add_log; int addsel = 0;
    std::vector<std::string> lang_gid, lang_why; Scr lang_ret = S_ADD; std::vector<std::string> add_new, add_new_why;   /* t44a (#494): 언어 지정 화면의 대상(id)·자동 판정 근거 / 방금 추가한 작품 */
    enum { WMN = 5 };   /* 표시하는 배율 x1..x5 */
    int wm_next(int d) const {   /* d=+1/-1: 지금보다 큰/작은, 모니터에 들어가는 배율. 없으면 0 */
        int curw = wmult ? 640 * wmult : winw; int best = 0;
        if (d > 0) { for (int m = 1; m <= wmax && m <= WMN; m++) if (640 * m > curw) { best = m; break; } }
        else { for (int m = std::min(wmax, (int)WMN); m >= 1; m--) if (640 * m < curw) { best = m; break; } }
        return best;
    }

    Settings & eff() { return (scope == 1 && has_t[cur]) ? t[cur] : g; }
    Settings effective(int gi) const { if (gi >= 0 && gi < MAXG && has_t[gi]) return t[gi]; Settings s = g; if (gi >= 0 && gi < (int)games.size()) apply_ov(s, games[gi].ov); return s; }
    Settings & edit() { if (scope == 1) { if (!has_t[cur]) { t[cur] = effective(cur); has_t[cur] = true; } return t[cur]; } return g; }
    const UiGame * game() const { return (cur >= 0 && cur < (int)games.size()) ? &games[cur] : NULL; }
    /* t48b2 (#632): 실행 막는 사유(block)는 작품 id 로 찾는다 — 배열 위치(행 gi · 현재 cur)로 곧바로 읽지 않는다. 회색 표시 · 시작 팝업 · 정보칸 · 슬롯 불러오기가 모두 이 세 함수를 쓴다 */
    const std::string & blk_id(const std::string & id) const { static const std::string E; if (id.empty()) return E; for (size_t i = 0; i < games.size(); i++) if (games[i].id == id) return games[i].block; return E; }
    const std::string & blk_cur() const { const UiGame * g = game(); return blk_id(g ? g->id : std::string()); }
    const std::string & blk_row(int gi) const { int ei = row_ent(gi); int tg = (ei >= 0 && games[gi].ents[ei].vgi >= 0 && games[gi].ents[ei].vgi < (int)games.size()) ? games[gi].ents[ei].vgi : gi; return blk_id(games[tg].id); }
    /* ================================================================ t39: 목록(탭·정렬) · 실행 방법 · 글꼴 */
    enum { TAB_KO, TAB_JP, TAB_ALL };
    enum { SM_TITLE, SM_ORIG, SM_YEAR, SM_COMPANY, SM_RECENT, SM_PLAY, SM_CUSTOM, SM_N };
    int tab = TAB_KO, sortm = SM_RECENT; bool sortfav = true;
    std::vector<int> vis, rent;   /* vis: 목록에 보이는 작품(index) 순서 · rent[작품]: 행이 가리키는 실행 방법(ents index, -1 = 자동) */
    std::string sel_entry;        /* 고른 실행 방법 이름 — 실행 요청(Act.entry)에 실려 간다. 빈칸 = 폴더 기본 */
    bool reorder = false; long long now_s = 0; Env env;
    std::vector<UiFont> fonts; std::string font_kr, font_jp;   /* 글꼴 폴더 목록 · 전역 선택(빈칸 = 기본) */
    static const char * tab_name(int t) { static const char * n[] = { "한글판", "원판 일본어", "전체" }; return n[t < 0 || t > 2 ? 2 : t]; }
    static const char * sort_name(int m) { static const char * n[] = { "제목", "원제", "연도", "제작사", "최근 실행", "자주 실행", "사용자 지정" }; return n[m < 0 || m >= SM_N ? 0 : m]; }
    static const char * sort_id(int m) { static const char * n[] = { "title", "orig", "year", "company", "recent", "play", "custom" }; return n[m < 0 || m >= SM_N ? 0 : m]; }
    static int sort_from(const std::string & s) { for (int i = 0; i < SM_N; i++) if (s == sort_id(i)) return i; return SM_RECENT; }
    static const char * tab_id(int t) { static const char * n[] = { "ko", "jp", "all" }; return n[t < 0 || t > 2 ? 2 : t]; }
    static int tab_from(const std::string & s) { for (int i = 0; i < 3; i++) if (s == tab_id(i)) return i; return TAB_KO; }

    /* ---- 제목 도우미 (#288): 자리마다 정한 규칙으로 줄인다 */
    static std::string short_of(const UiGame & g) { return g.short_t.empty() ? auto_short(g.title) : g.short_t; }
    static std::string ttl_fit(const UiGame & g, int px) { return fit_title(short_of(g), px); }
    static std::string ttl_suf(const UiGame & g, const char * suf, int px) { int sw = tw(std::string(" ") + suf); return fit_title(short_of(g), px - sw) + " " + suf; }
    /* t44a (#494): 작품 언어 표시 — 사용자 지정(lang_exp) > 자동 판정 */
    static std::string lang_label(const UiGame & g) { if (g.lang_exp == "ko") return "한글판 (지정)"; if (g.lang_exp == "jp") return "원판 일본어 (지정)"; return std::string("자동 → ") + (g.lang == "ko" ? "한글판" : "원판 일본어"); }
    int gidx_of(const std::string & id) const { for (size_t i = 0; i < games.size(); i++) if (games[i].id == id) return (int)i; return -1; }
    void open_lang(const std::vector<std::string> & ids, const std::vector<std::string> & why, Scr ret) { lang_gid = ids; lang_why = why; lang_ret = ret; if (lang_why.size() < lang_gid.size()) lang_why.resize(lang_gid.size()); sel = 0; scr = S_LANG; }
    void d_lang(Canvas & c) {
        titlebar(c, "언어 지정");
        rbox(c, 16, 24, 288, 224, WH, "작품 언어");
        std::vector<std::string> r; std::vector<uint32_t> col;
        for (size_t i = 0; i < lang_gid.size(); i++) { int gi = gidx_of(lang_gid[i]); if (gi < 0) { r.push_back("(없음)"); col.push_back(GY); continue; } const UiGame & g = games[gi]; r.push_back(fit(short_of(g), 150) + "  " + (g.lang_exp.empty() ? std::string("자동") : g.lang == "ko" ? std::string("한글판") : std::string("원판")) ); col.push_back(g.lang_exp.empty() ? 0 : YL); }
        if (r.empty()) { r.push_back("(대상 없음)"); col.push_back(GY); }
        rowlist(c, 40, 48, 256, 7, r, sel, 24, &col);
        rbox(c, 320, 24, 304, 224, WH, "상세");
        int gi = (sel >= 0 && sel < (int)lang_gid.size()) ? gidx_of(lang_gid[sel]) : -1;
        if (gi >= 0) {
            const UiGame & g = games[gi];
            text(c, 340, 44, fit(g.title, 270), WH, K); text(c, 340, 68, "언어", CY, K); text(c, 340 + 76, 68, fit(lang_label(g), 190), YL, K);
            std::string w = (sel < (int)lang_why.size()) ? lang_why[sel] : std::string(); text(c, 340, 92, "자동 판정", CY, K); text(c, 340 + 76, 92, fit(g.lang == "ko" ? std::string("한글 증거 있음") : std::string("한글 증거 없음 (원판 취급)"), 190), WH, K);
            if (!w.empty()) text(c, 340, 116, fit(w, 270), GY, K);
            text(c, 340, 152, "한글판 → 한글 글꼴 / 원판 → 일본어 글꼴", WH, K);
        }
        desc(c, 264, 64, { "○ 또는 ←→:자동 → 한글판 → 원판 일본어 순으로 바꿉니다.", "저장 위치: 작품 폴더의 user.ini [game] lang= (자동이면 지움)." });
        helpline(c, "↑↓:작품, ○/←→:언어 바꾸기, ×:돌아가기");
    }
    static std::string ttl_full(const UiGame & g, int px) { return fit_title(g.title, px); }

    int base_of(int g) const { if (g < 0 || g >= (int)games.size()) return g; int b = games[g].base; return b >= 0 && b < (int)games.size() ? b : g; }
    std::vector<int> vents(int gi) const { std::vector<int> r; if (gi < 0 || gi >= (int)games.size()) return r; for (size_t i = 0; i < games[gi].ents.size(); i++) if (!games[gi].ents[i].hidden) r.push_back((int)i); return r; }
    /* t41 (#306): 탭이 실행 언어를 정한다 — 한글판 탭은 원판(jp) 항목을, 원판 일본어 탭은 jp 가 아닌 항목을 ←→ 순환·행 표시에서 뺀다. 전체 탭은 전부. 항목 언어가 비면 작품 언어를 따른다. 걸러서 비면 전부(안전). 편집 메뉴(vents)는 그대로 전부 본다. */
    std::vector<int> tvents(int gi) const {
        std::vector<int> v = vents(gi); if (tab == TAB_ALL || gi < 0 || gi >= (int)games.size()) return v;
        const UiGame & g = games[gi]; std::vector<int> r;
        for (int i : v) { const std::string & el = g.ents[i].lang.empty() ? g.lang : g.ents[i].lang; if (tab == TAB_JP ? el == "jp" : el != "jp") r.push_back(i); }
        return r.empty() ? v : r;
    }
    int row_ent(int gi) const {   /* 행이 지금 가리키는 실행 방법. 정의가 없으면 -1(자동 판정) */
        std::vector<int> v = tvents(gi); if (v.empty()) return -1; const UiGame & g = games[gi];
        if (gi < (int)rent.size() && rent[gi] >= 0) for (int i : v) if (i == rent[gi]) return i;
        if (tab == TAB_JP) for (int i : v) if (g.ents[i].lang == "jp") return i;
        if (!g.defent.empty()) for (int i : v) if (g.ents[i].vgi < 0 && g.ents[i].name == g.defent) return i;
        for (int i : v) if (g.ents[i].vgi < 0) return i;
        return v[0];
    }
    bool row_in_tab(int gi) const {
        const UiGame & g = games[gi]; if (!g.variant_of.empty()) return false;
        if (tab == TAB_ALL) return true; if (tab == TAB_KO) return g.lang == "ko";
        if (g.lang == "jp") return true; for (auto & e : g.ents) if (!e.hidden && e.lang == "jp") return true; return false;
    }
    int ent_warn(int gi, int ei) {   /* 1 = 경고 있음 */
        if (gi < 0 || gi >= (int)games.size() || ei < 0 || ei >= (int)games[gi].ents.size()) return 0;
        UiEnt & e = games[gi].ents[ei]; if (e.warn >= 0) return e.warn;
        e.warn = 0; if (e.pseudo || !env.has_img) return 0;
        int tg = e.vgi >= 0 ? e.vgi : gi; std::vector<ent::Issue> is = ent::validate(e.kv, [&](const std::string & n) { return env.has_img(tg, n); });
        for (auto & i : is) if (i.err) { e.warn = 1; e.warn_msg = i.msg; break; }
        return e.warn;
    }
    int row_warn(int gi) { int ei = row_ent(gi); return ei >= 0 ? ent_warn(gi, ei) : 0; }
    int pos_of(int g) const { for (size_t k = 0; k < vis.size(); k++) { int gi = vis[k]; if (gi == g) return (int)k; for (auto & e : games[gi].ents) if (e.vgi == g && e.vgi >= 0) return (int)k; } return 0; }
    void sync_cur() {
        if (pick >= 0 && pick < (int)vis.size()) { int gi = vis[pick], ei = row_ent(gi); const UiEnt * e = ei >= 0 ? &games[gi].ents[ei] : NULL; cur = (e && e->vgi >= 0) ? e->vgi : gi; sel_entry = (e && e->vgi < 0 && !e->pseudo) ? e->name : std::string(); }
    }
    static int cls_of(uint32_t cp) { if (cp >= 0xAC00 && cp <= 0xD7A3) return 0; if (cp < 0x80 && isalpha((int)cp)) return 1; if (cp < 0x80 && isdigit((int)cp)) return 2; return 3; }
    static std::string title_key(const std::string & in) {   /* 맨 앞 기호·「한글판」 접두 무시, 한글 → 영문 → 숫자 → 기타 */
        const char * p = in.c_str();
        for (;;) { const char * q = p; uint32_t cp = u8next(q); if (!cp) break; bool skip = cp < 0x80 ? !isalnum((int)cp) : (cp == 0x300C || cp == 0x300E || cp == 0x3010 || cp == 0x2018 || cp == 0x201C || cp == 0x3000 || cp == 0xFF3B); if (!skip) break; p = q; }
        std::string s = p; const std::string han = "한글판"; if (s.compare(0, han.size(), han) == 0) { s = s.substr(han.size()); size_t i = 0; while (i < s.size() && (s[i] == ' ' || s[i] == '-')) i++; s = s.substr(i); }
        const char * r = s.c_str(); uint32_t first = u8next(r); std::string o(1, (char)('0' + cls_of(first))); for (char ch : s) o += (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch; return o;
    }
    static int year_of(const UiGame & g) { for (size_t i = 0; i + 3 < g.year.size(); i++) if (isdigit((unsigned char)g.year[i]) && isdigit((unsigned char)g.year[i + 1]) && isdigit((unsigned char)g.year[i + 2]) && isdigit((unsigned char)g.year[i + 3])) return atoi(g.year.substr(i, 4).c_str()); return 99999; }
    long long row_last(int gi) const { long long m = games[gi].last_run; for (auto & e : games[gi].ents) if (e.vgi >= 0 && e.vgi < (int)games.size()) m = std::max(m, games[e.vgi].last_run); return m; }
    int row_play(int gi) const { int s = games[gi].play_s; for (auto & e : games[gi].ents) if (e.vgi >= 0 && e.vgi < (int)games.size()) s += games[e.vgi].play_s; return s; }
    bool less_rows(int a, int b) const {
        const UiGame & A = games[a], & B = games[b];
        if (sortfav && A.fav != B.fav) return A.fav;
        switch (sortm) {
        case SM_ORIG: { bool ea = A.orig.empty(), eb = B.orig.empty(); if (ea != eb) return eb; if (!ea) { int c = title_key(A.orig).compare(title_key(B.orig)); if (c) return c < 0; } break; }
        case SM_YEAR: { int ya = year_of(A), yb = year_of(B); if (ya != yb) return ya < yb; break; }
        case SM_COMPANY: { bool ea = A.company.empty(), eb = B.company.empty(); if (ea != eb) return eb; if (!ea && A.company != B.company) return A.company < B.company; break; }
        case SM_RECENT: { long long la = row_last(a), lb = row_last(b); if (la != lb) return la > lb; break; }
        case SM_PLAY: { int pa = row_play(a), pb = row_play(b); if (pa != pb) return pa > pb; break; }
        case SM_CUSTOM: { int oa = A.order_n > 0 ? A.order_n : (1 << 30), ob = B.order_n > 0 ? B.order_n : (1 << 30); if (oa != ob) return oa < ob; break; }
        default: break;
        }
        int c = title_key(A.title).compare(title_key(B.title)); if (c) return c < 0; return a < b;
    }
    void rebuild_vis(int keep_game = -2) {
        int keep = keep_game == -2 ? cur : keep_game; if (rent.size() < games.size()) rent.resize(games.size(), -1);
        vis.clear(); for (size_t i = 0; i < games.size(); i++) if (row_in_tab((int)i)) vis.push_back((int)i);
        std::stable_sort(vis.begin(), vis.end(), [&](int a, int b) { return less_rows(a, b); });
        pick = pos_of(keep); if (pick >= (int)vis.size()) pick = 0; sync_cur(); pick_seen = -1;
    }
    void renumber_order() { for (size_t k = 0; k < vis.size(); k++) games[vis[k]].order_n = (int)(k + 1) * 10; }
    /* 처음 한 번: 마지막 작품이 탭에 안 보이면 탭을 맞춘다 */
    void fit_tab_to_cur() { int b = base_of(cur); if (b < 0 || b >= (int)games.size()) return; if (row_in_tab(b)) return; tab = games[b].lang == "jp" ? TAB_JP : TAB_KO; if (!row_in_tab(b)) tab = TAB_ALL; }

    /* ---- 편집 상태 */
    int eg = -1;                 /* 편집 대상 작품(user.ini 를 쓸 폴더) */
    int entg = -1;               /* 실행 방법 목록의 행 작품(base) */
    std::string en_orig; bool enew = false; ent::Kv ed, ed0; std::string ed_name, ed_name0; bool ed_setdef = false;
    std::string msg;             /* 한 줄 알림 */
    std::string yn_msg[3]; int yn_act = 0, ynsel = 1; Scr yn_ret = S_MAIN; int yn_arg = 0;
    enum { YN_DISCARD, YN_DELETE, YN_TEST, YN_INSTDONE, YN_RESET };
    /* 파일 고르기 */
    std::string fl_key, fl_dir; std::vector<std::pair<std::string, std::string> > fl_roots; int fl_ri = 0, fl_swapi = -1; std::vector<FileItem> fl_items; bool fl_sort_kind = false; Scr fl_ret = S_EDIT;
    /* 자동 입력 · 교체 목록 */
    std::vector<ent::Step> ks; int ksel = 0, stepi = 0; std::vector<std::string> sw; int swsel = 0;
    /* 글꼴 선택 */
    /* ---- t48e (#646/#651): 조작 설정 — 저장 키 이름은 SDL 표준 버튼 이름 하나(btn_b btn_a btn_x btn_start btn_leftshoulder btn_rightshoulder btn_back).
       전역 = settings.ini [global], 이 작품 = user.ini [pad] (작품이 이긴다) */
    std::map<std::string, std::string> gpad;   /* 전역 btn_* */
    std::vector<std::string> pk_opts; int pk_row = 0, pad_sel_keep = 0;
    static const char * pad_key(int i) { static const char * const K[7] = { "btn_b", "btn_a", "btn_x", "btn_start", "btn_leftshoulder", "btn_rightshoulder", "btn_back" }; return K[i < 0 ? 0 : i > 6 ? 6 : i]; }
    std::string pad_row_name(int i) const {
        int kd = env.padkind ? env.padkind() : 0;
        static const char * const X[7] = { "B 버튼", "A 버튼", "X 버튼", "START", "LB", "RB", "BACK" };
        static const char * const P[7] = { "○", "×", "□", "START", "L1", "R1", "SELECT" };
        static const char * const O[7] = { "동쪽 버튼", "남쪽 버튼", "서쪽 버튼", "START", "왼쪽 어깨", "오른쪽 어깨", "SELECT" };
        return kd == 1 ? X[i] : kd == 2 ? P[i] : O[i];
    }
    std::string pad_default(int i) {   /* 서양식이면 동쪽·남쪽 기본이 서로 바뀐다 */
        bool w = eff().pad == 1;
        switch (i) { case 0: return w ? "esc" : "space"; case 1: return w ? "space" : "esc"; case 2: return "caps"; case 3: return "ret"; default: return "none"; }
    }
    static std::string pad_show(const std::string & v) {
        if (v == "none") return "배정 없음"; if (v == "mouse_left") return "마우스 왼"; if (v == "mouse_right") return "마우스 오른";
        std::string u = v; for (auto & c : u) c = (char)toupper((unsigned char)c); return u;
    }
    std::string pad_val(int i, bool * from_work = NULL, bool * is_def = NULL) {   /* 지금 이 범위에서 보이는 값 */
        std::string k = pad_key(i), v; if (from_work) *from_work = false; if (is_def) *is_def = false;
        if (scope == 1 && env.pad_get && cur >= 0 && cur < (int)games.size()) { std::map<std::string, std::string> m; env.pad_get(cur, m); auto it = m.find(k); if (it != m.end() && !it->second.empty()) { if (from_work) *from_work = true; return it->second; } }
        auto g2 = gpad.find(k); if (g2 != gpad.end() && !g2->second.empty()) return g2->second;
        if (is_def) *is_def = true; return pad_default(i);
    }
    void open_padkey(int row) {
        pk_row = row; pad_sel_keep = sel; pk_opts.clear(); pk_opts.push_back(std::string()); pk_opts.push_back("none"); pk_opts.push_back("mouse_left"); pk_opts.push_back("mouse_right");
        if (env.padkeys) for (auto & n : env.padkeys()) pk_opts.push_back(n);
        std::string cv; { std::string k = pad_key(row); if (scope == 1 && env.pad_get && cur >= 0 && cur < (int)games.size()) { std::map<std::string, std::string> m; env.pad_get(cur, m); auto it = m.find(k); if (it != m.end()) cv = it->second; } else if (scope == 0) { auto it = gpad.find(k); if (it != gpad.end()) cv = it->second; } }
        sel = 0; for (size_t i = 0; i < pk_opts.size(); i++) if (pk_opts[i] == cv) sel = (int)i; scr = S_PADKEY;
    }
    void d_pad(Canvas & c) {
        titlebar(c, "조작 설정");
        Settings & s = eff();
        rbox(c, 16, 24, 288, 296, WH, "패드 버튼");
        std::vector<std::string> n = { "적용 범위", "결정 방식" }; for (int i = 0; i < 7; i++) n.push_back(pad_row_name(i));
        items(c, 40, 44, 256, n, sel, 24);
        rbox(c, 320, 24, 304, 296, WH, "인포메이션");
        auto row = [&](int i) { return sel == i; };
        auto ry = [](int i) { return 44 + 24 * i; };
        const char * sc[] = { "전역", "이 작품" }; choices(c, 344, ry(0), sc, 2, scope, WH, row(0));
        const char * ly[] = { "일본식(동쪽)", "서양식(남쪽)" }; choices(c, 344, ry(1), ly, 2, s.pad == 1 ? 1 : 0, WH, row(1));
        for (int i = 0; i < 7; i++) {
            bool fw = false, df = false; std::string v = pad_val(i, &fw, &df); std::string t = pad_show(v);
            if (scope == 1 && !fw) t += df ? " (기본)" : " (전역)"; else if (scope == 0 && df) t += " (기본)";
            text(c, 344, ry(2 + i), fit(t, 250), row(2 + i) ? K : WH, row(2 + i) ? WH : K);
        }
        text(c, 344, ry(9) + 4, "△ = 메뉴 열기 (고정)", CY, K);
        static const char * const dd[] = { "전역=모든 작품, 이 작품=이 작품만. 작품 값이 우선.", "결정 방식: 일본식=동쪽 버튼, 서양식=남쪽 버튼이 결정.", "버튼 한 줄입니다. ○:키 고르기.", "버튼 한 줄입니다. ○:키 고르기.", "버튼 한 줄입니다. ○:키 고르기.", "버튼 한 줄입니다. ○:키 고르기.", "버튼 한 줄입니다. ○:키 고르기.", "버튼 한 줄입니다. ○:키 고르기.", "버튼 한 줄입니다. ○:키 고르기." };
        desc(c, 324, 44, { fit(dd[sel < 9 ? sel : 8], 560) });
        helpline(c, "↑↓:선택, ←→:값, ○:키 고르기, ×:뒤로, △:출하 시 설정");
    }
    void d_padkey(Canvas & c) {
        titlebar(c, "");
        rbox(c, 16, 24, 288, 296, WH, ("키 고르기: " + pad_row_name(pk_row)).c_str());
        std::vector<std::string> r; std::vector<uint32_t> col; std::string curv = pad_val(pk_row);
        for (auto & o : pk_opts) { std::string lab = o.empty() ? std::string(scope == 1 ? "전역 따름 (작품 값 지움)" : "기본값") : pad_show(o); r.push_back(std::string(o == curv && !o.empty() ? "● " : "  ") + fit(lab, 220)); col.push_back(0); }
        rowlist(c, 40, 48, 256, 9, r, sel, 24, &col);
        rbox(c, 320, 24, 304, 296, WH, "인포메이션");
        text(c, 340, 44, fit(pad_row_name(pk_row) + " → " + (sel < (int)pk_opts.size() ? (pk_opts[sel].empty() ? std::string("기본") : pad_show(pk_opts[sel])) : std::string("-")), 270), YL, K);
        text(c, 340, 74, fit(scope == 1 ? "저장: 이 작품 (user.ini [pad])" : "저장: 전역 (settings.ini)", 270), CY, K);
        text(c, 340, 104, "배정 없음 = 이 버튼은", WH, K);
        text(c, 340, 124, "아무 것도 보내지 않음", WH, K);
        text(c, 340, 154, "마우스 왼/오른 = 게임 마우스 버튼", WH, K);
        desc(c, 324, 44, { "STOP COPY GRPH XFER NFER HELP 등 PC-98 키를 고를 수 있습니다." });
        helpline(c, "↑↓:선택, ○:정함, ×:취소");
    }
    int fnt_mode = 0;            /* 0 전역 한글 · 1 전역 일본어 · 2 항목 */
    std::vector<std::string> fnt_opts; std::string fnt_need; Scr fnt_ret = S_CONF; std::map<std::string, ov::BmpFont> fnt_cache;
    /* 화면 키보드 */
    std::string osk_buf; int osk_target = 0, osk_r = 0, osk_c = 0; bool osk_upper = false; Scr osk_ret = S_EDIT; std::string ime_comp;   /* t48a: ime_comp = IME 조합 중 글자 */
    hg::Composer hc; bool osk_hg = false, osk_shift = false;   /* t48b: 한글 화면 키보드(이름 칸) — osk_hg = 켜짐, osk_shift = SHIFT 한 번 눌림(다음 자모 하나만) */
    std::vector<ValRow> vals; int vals_top = 0;   /* t48a (#614 ③) */
    int tplsel = 0, menusel = 0, hddsel = 0;
    bool install_start = false;  /* 설치 필요 게임을 처음 시작할 때의 HDD 만들기 */

    static const std::vector<std::string> & key_names() { static std::vector<std::string> v = { "enter", "space", "esc", "tab", "up", "down", "left", "right", "y", "n", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "bs", "del", "caps", "kana", "pageup", "pagedown", "ctrl", "shift" }; return v; }
    static const std::vector<std::string> & type_presets() { static std::vector<std::string> v = { "dir~", "cd..~", "y~", "n~", "1~", "2~", "a:~", "c:~", "d:~" }; return v; }
    static const std::vector<std::string> & name_presets() { static std::vector<std::string> v = { "한글판", "원판 일본어", "CD 모드", "HDD 부팅", "FD 부팅", "데모 먼저", "다장 디스크", "설치 필요", "내 실행 방법" }; return v; }
    std::string imgdir_of(int g) const { return g >= 0 && g < (int)games.size() ? (games[g].imgdir.empty() ? games[g].dir : games[g].imgdir) : std::string(); }
    std::string kv_get(const char * k) const { auto it = ed.find(k); return it == ed.end() || it->second == "-" ? std::string() : it->second; }
    void kv_set(const char * k, const std::string & v) { if (v.empty()) ed.erase(k); else ed[k] = v; }
    std::vector<std::string> ents_used(int g, const std::string & except = std::string()) const { std::vector<std::string> u; if (g >= 0 && g < (int)games.size()) for (auto & e : games[g].ents) if (!e.pseudo && e.name != except) u.push_back(e.name); return u; }
    bool ed_dirty() const { return ed != ed0 || ed_name != ed_name0 || enew; }
    int test_secs() {   /* t47a (#559): 시험 창 초 = 마지막 단계 끝 시각(글자 입력은 글자당 220ms) 올림 + 10, 하한 20(단계 없음 포함)·상한 700 */
        std::vector<ent::Step> v = ent::parse_steps(kv_get("bootkeys")); unsigned long long end = 0;
        for (const ent::Step & s : v) { unsigned long long e = (unsigned long long)(s.ms < 0 ? 0 : s.ms) + (s.type ? (unsigned long long)s.val.size() * 220ull : 0ull); if (e > end) end = e; }
        if (v.empty()) return 20; int sec = (int)((end + 999ull) / 1000ull) + 10; return sec < 20 ? 20 : (sec > 700 ? 700 : sec);
    }

    std::vector<ent::Issue> ed_issues() const { if (!env.has_img) return std::vector<ent::Issue>(); int tg = eg; return ent::validate(ed, [&](const std::string & n) { return env.has_img(tg, n); }); }
    void files_of(int g, std::vector<std::string> & out) const { out.clear(); if (!env.list_dir) return; for (auto & f : env.list_dir(imgdir_of(g))) if (!f.dir && ent::img_kind(f.name)) out.push_back(f.name); }
    static std::string disk_label(const std::string & v) { if (v.empty() || v == "-") return "(없음)"; return ent::base_name(v); }

    /* ---- 그리기 공통 */
    void titlebar(Canvas & c, const char * name) {
        (void)name; text(c, 40, 0, "PC-98 PLAYER", WH, K);   /* 맨 윗줄은 제품명만 (화면 이름은 창 제목에) */
        std::string b = build; int w = tw(b); text(c, 640 - w - 24, 0, b, WH, K);
    }
    void items(Canvas & c, int x, int y, int w, const std::vector<std::string> & a, int sel, int pitch, const std::vector<bool> * dim = NULL) {
        for (size_t i = 0; i < a.size(); i++) {
            int yy = y + (int)i * pitch; bool d = dim && (*dim)[i];
            if ((int)i == sel) c.rect(x - 6, yy - 3, w, 22, d ? GY : WH);
            c.clip(x, yy - 3, w - 12, 22);   /* r018: 글자가 막대·상자 밖으로 나가지 않게 */
            if ((int)i == sel) text(c, x, yy, a[i], K, d ? GY : WH); else text(c, x, yy, a[i], d ? GY : CY, K);
            c.unclip();
        }
    }
    void desc(Canvas & c, int y, int h, const std::vector<std::string> & ln, int orfrom = -1) {
        rbox(c, 16, y, 608, h, OR, NULL); for (size_t i = 0; i < ln.size(); i++) text(c, 40, y + 14 + (int)i * 20, ln[i], (orfrom >= 0 && (int)i >= orfrom) ? OR : WH, K);
    }
    void thumb(Canvas & c, const UiGame * g, int x, int y, int w, int h) {
        c.rect(x - 1, y - 1, w + 2, h + 2, GY); c.rect(x, y, w, h, K);
        if (g && !g->thmb.empty()) blit_scaled(c, g->thmb.data(), g->tw_, g->th_, x, y, w, h);
        else { const char * m = "저장 없음"; text(c, x + (w - tw(m)) / 2, y + h / 2 - 8, m, GY, K); }   /* r018: 표본 그림 대신 빈 칸 (나중에 타이틀 화면 캡처로 채움) */
    }
    static void menu_names(std::vector<std::string> & v) { v = { "게임 시작", "작품 선택", "화면 설정", "소리/패드/배속 설정", "세이브 관리", "부가", "끝내기" }; }
    static std::string play_str(int s) { char b[48]; if (s >= 3600) snprintf(b, sizeof b, "%d시간 %02d분", s / 3600, (s / 60) % 60); else snprintf(b, sizeof b, "%d분", s / 60); return b; }
    /* 확인 창: 화면 가운데, 뒤 화면 어둡게. 반환 = 창 왼쪽 위 x,y */
    void confirm_box(Canvas & c, int w, int h, const char * title, int & bx, int & by) {
        dim(c); bx = (c.w - w) / 2; by = (c.h - h) / 2; c.rect(bx, by, w, h, K); rbox(c, bx, by, w, h, YL, title);
    }
    void sample_or_game(Canvas & c, int x, int y, int w, int h) { thumb(c, game(), x, y, w, h); }

    /* ---- 화면별 그리기 */
    void draw(Canvas & c) {
        if (scr != S_GAME && scr != S_KEYS && !(in_game && (scr == S_QUIT || scr == S_SWAP || scr == S_SWAPI))) c.rect(0, 0, c.w, c.h, K);
        switch (scr) {
        case S_MAIN: d_main(c, 0); break;
        case S_START: d_main(c, 1); break;
        case S_QUIT: if (in_game) d_quit(c); else d_main(c, 2); break;
        case S_SWAP: d_swap(c); break;
        case S_SWAPI: d_swapi(c); break;
        case S_ABOUT: d_about(c); break;
        case S_PICK: d_pick(c); break;
        case S_SCREEN: d_screen(c); break;
        case S_CONF: d_conf(c); break;
        case S_SAVE: d_save(c); break;
        case S_EXTRA: d_extra(c); break;
        case S_GAME: d_game(c); break;
        case S_KEYS: d_keys(c); break;
        case S_HELP: d_help(c); break;
        case S_ADD: d_add(c); break;
        case S_SORT: d_sort(c); break; case S_ENT: d_ent(c); break; case S_ENTMENU: d_entmenu(c); break; case S_TPL: d_tpl(c); break; case S_EDIT: d_edit(c); break;
        case S_FILE: d_file(c); break; case S_KEYSEQ: d_keyseq(c); break; case S_STEP: d_step(c); break; case S_SWAPED: d_swaped(c); break; case S_FONT: d_font(c); break;
        case S_OSK: d_osk(c); break; case S_NAME: d_name(c); break; case S_YN: d_yn(c, yn_ret); break; case S_HDDASK: d_hddask(c); break; case S_LANG: d_lang(c); break; case S_VALS: d_vals(c); break;
        case S_PAD: d_pad(c); break; case S_PADKEY: d_padkey(c); break;
        }
    }
    /* 인포메이션 = 현재 작품의 게임 정보 */
    static std::string src_label(const std::string & m) {   /* 내부 출처 이름 → 화면 글 (내부 용어는 쓰지 않는다) */
        if (m.empty() || m == "none") return std::string();
        if (m == "tosec") return "TOSEC"; if (m == "nkset") return "롬셋 폴더"; if (m == "nk") return "롬셋"; if (m == "curated" || m == "큐레이션") return "DB";
        return m;
    }
    std::string mk_line(const UiGame & g) const {   /* 제작 · 연도 · 장르 + 출처. 빈 값은 「-」 */
        std::string mk = g.company.empty() ? std::string("-") : g.company; if (!g.year.empty()) mk += " · " + g.year; if (!g.genre.empty()) mk += " · " + g.genre;
        std::string sl = src_label(g.metasrc); if (!sl.empty() && !(g.company.empty() && g.year.empty() && g.genre.empty())) mk += " (" + sl + ")";
        return mk;
    }
    void info_box(Canvas & c, int x, int y, int w, int h) {
        rbox(c, x, y, w, h, WH, "인포메이션");
        const UiGame * gm = game(); if (!gm) return;
        const int X = x + 24, LW = 56, VW = w - 48 - LW - 8; int yy = y + 16;
        auto line = [&](const char * lab, const std::string & v, uint32_t col = WH) { text(c, X, yy, lab, CY, K); text(c, X + LW, yy, fit(v, VW), col, K); yy += 20; };
        { std::vector<std::string> tl = title_lines(gm->title, w - 48, 2); for (auto & l : tl) { text(c, X, yy, l, blk_cur().empty() ? YL : GY, K); yy += 20; } yy += 2; }
        line("제작", mk_line(*gm));
        line("패치", gm->patch.empty() ? std::string("-") : gm->patch);
        line("디스크", gm->disknote.empty() ? std::string("-") : gm->disknote);
        line("장치", gm->devices.empty() ? std::string("특별한 장치 불필요") : gm->devices);
        { static const struct { const char * k; const char * n; } T[] = { { "ratio", "비율" }, { "shader", "셰이더" }, { "curve", "곡면" }, { "scan", "주사선" }, { "mask", "마스크" }, { "bleed", "RGB번짐" }, { "glow", "빛번짐" }, { "bgm", "BGM" }, { "se", "효과음" }, { "pad", "패드" }, { "boost", "배속" }, { "boostmode", "배속방식" } };
          std::string o; for (auto & t : T) if (gm->ov.count(t.k)) { if (!o.empty()) o += ", "; o += t.n; }
          if (cur >= 0 && cur < MAXG && has_t[cur]) { if (!o.empty()) o += " + "; o += "내 설정"; }
          line("설정", o.empty() ? std::string("전역과 같음") : o); }
        line("판정", gm->jshort.empty() ? std::string("-") : gm->jshort);
        if (!blk_cur().empty()) line("사유", blk_cur(), OR); else line("플레이", play_str(gm->play_s));
        line("저장", (gm->lastslot >= 0 && gm->slot[gm->lastslot].used) ? "슬롯 " + std::to_string(gm->lastslot + 1) + "  " + gm->slot[gm->lastslot].stamp : std::string("없음"));
    }
    bool has_save(const UiGame * g) const { return g && g->lastslot >= 0 && g->slot[g->lastslot].used; }
    /* mode 0 = 메인, 1 = 게임 시작 창, 2 = 끝내기 확인(타이틀에서) */
    void d_main(Canvas & c, int mode) {
        titlebar(c, "");
        rbox(c, 16, 24, 288, 224, WH, "메인메뉴");
        std::vector<std::string> m; menu_names(m); items(c, 40, 46, 256, m, mode == 2 ? 6 : mainsel, 28);
        info_box(c, 320, 24, 304, 224);
        static const char * md[] = { "현재 작품을 이어하기(마지막 저장) 또는 처음부터 시작합니다.", "작품을 고릅니다. 고른 작품이 현재 작품이 됩니다.", "화면 비율과 셰이더를 바꿉니다. 바꾸면 바로 적용됩니다.", "소리 크기, 패드 배치, 배속을 설정합니다.", "저장 슬롯을 불러오거나 지웁니다.", "디스크 정보, 오프닝 보기, 조작 안내, 정보·라이선스입니다.", "설정을 저장하고 끝냅니다." };
        desc(c, 264, 64, { md[mode == 2 ? 6 : mainsel] });
        if (mode == 0) helpline(c, "↑↓:선택, ○:결정, ×:취소, SELECT:부가");
        if (mode == 1) {
            const UiGame * gm = game(); bool hs = has_save(gm); int bx, by; confirm_box(c, 400, 168, "게임 시작", bx, by);
            bool blk = gm && !blk_cur().empty();
            text(c, bx + 24, by + 24, gm ? ttl_fit(*gm, 352) : std::string(), blk ? GY : CY, K);
            std::vector<std::string> a = { hs ? "이어하기   슬롯 " + std::to_string(gm->lastslot + 1) + "  " + gm->slot[gm->lastslot].stamp : std::string("이어하기   (저장 없음)"), "처음부터" };
            std::vector<bool> dm = { !hs || blk, blk }; items(c, bx + 30, by + 60, 340, a, startsel, 26, &dm);
            if (blk) { text(c, bx + 24, by + 126, fit("실행 불가: " + blk_cur(), 352), OR, K); }
            helpline(c, "↑↓:선택, ○:결정, ×:취소");
        }
        if (mode == 2) d_quit(c);
    }
    void d_quit(Canvas & c) {
        const char * q[] = { "저장하고 끝냄", "저장 안 함", "취소" }; int cw = choices_w(q, 3); int bw = cw + 64 > 360 ? cw + 64 : 360, bx, by;
        confirm_box(c, bw, 112, "확인", bx, by);
        const char * msg = "설정을 저장하고 끝냅니까?"; text(c, bx + ((bw - tw(msg)) >> 1), by + 30, msg, WH, K);
        choices(c, bx + ((bw - cw) >> 1) + 4, by + 70, q, 3, quitsel, WH, false);
        helpline(c, "←→:선택, ○:결정, ×:취소");
    }
    void d_swap(Canvas & c) {
        const UiGame * gm = game(); c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, "디스크 교체");
        if (!gm || gm->fds.empty()) { text(c, 160, 60, "교체할 디스크가 없습니다.", GY, K); helpline(c, "×:돌아가기"); return; }
        std::vector<std::string> a = { "A 드라이브  " + (gm->cur_fd[0].empty() ? std::string("(비어 있음)") : fit(gm->cur_fd[0], 190)), "B 드라이브  " + (gm->cur_fd[1].empty() ? std::string("(비어 있음)") : fit(gm->cur_fd[1], 190)) };
        items(c, 160, 48, 330, a, swapdrv, 30);
        text(c, 160, 130, "바꿀 드라이브를 고릅니다.", WH, K);
        helpline(c, "↑↓:선택, ○:디스크 고르기, ×:돌아가기");
    }
    void d_swapi(Canvas & c) {
        const UiGame * gm = game(); c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, std::string(swapdrv ? "B" : "A").append(" 드라이브에 넣을 디스크").c_str());
        if (!gm) return;
        const int VIS = 9; int n = (int)gm->fds.size(); int top = swapsel - 4; if (top > n - VIS) top = n - VIS; if (top < 0) top = 0;
        std::vector<std::string> w; for (int i = top; i < n && i < top + VIS; i++) w.push_back((gm->fds[i] == gm->cur_fd[swapdrv] ? "● " : "  ") + fit(gm->fds[i], 300));
        items(c, 150, 44, 350, w, swapsel - top, 30);
        helpline(c, "↑↓:선택, ○:넣기, ×:돌아가기");
    }
    void d_about(Canvas & c) {
        titlebar(c, ""); rbox(c, 16, 24, 608, 344, WH, "정보·라이선스");
        static const char * L[] = { "PC-98 PLAYER", "이 프로그램의 PC-98 에뮬레이션은 DOSBox-X 의 구성 요소를 사용합니다.", "DOSBox-X : GNU General Public License v2 이상 (DOSBox-X 팀과 기여자들)", "           소스 코드 : https://github.com/joncampbell123/dosbox-x", "SDL2 : zlib 라이선스     libpng · zlib : 각 라이선스", "UI 글꼴 : Neo둥근모 (SIL Open Font License 1.1)", "", "GPL 조건에 따라 이 프로그램은 보증 없이 제공됩니다.", "게임 디스크 이미지는 이 프로그램에 포함되지 않으며, 각 저작권자의 것입니다." };
        for (int i = 0; i < 9; i++) if (L[i][0]) text(c, 40, 48 + i * 28, L[i], i == 0 ? CY : WH, K);
        helpline(c, "○ 또는 ×:돌아가기");
    }
    static std::vector<std::string> wrap(const std::string & s, int maxw, int maxlines) {   /* 폭(px) 기준 줄바꿈 */
        std::vector<std::string> o; std::string cur; const char * p = s.c_str();
        for (;;) {
            const char * q = p; uint32_t cp = u8next(q); if (!cp) break; std::string ch(p, q - p); p = q;
            if (tw(cur + ch) > maxw && !cur.empty()) { o.push_back(cur); cur.clear(); if ((int)o.size() >= maxlines) { return o; } }
            cur += ch;
        }
        if (!cur.empty() && (int)o.size() < maxlines) o.push_back(cur);
        return o;
    }
    /* ---- r018: 글자 단위 줄바꿈 · 단어 단위 줄바꿈 · 마퀴 */
    static std::vector<std::string> wrap_words(const std::string & s, int w) {   /* 공백에서 우선 줄바꿈, 한 단어가 길면 글자 단위 */
        std::vector<std::string> out; std::string cur; size_t p = 0;
        while (p <= s.size()) {
            size_t q = s.find(' ', p); if (q == std::string::npos) q = s.size(); std::string t = s.substr(p, q - p); p = q + 1;
            if (t.empty()) { if (q >= s.size()) break; continue; }
            std::string cand = cur.empty() ? t : cur + " " + t;
            if (tw(cand) <= w) { cur = cand; }
            else {
                if (!cur.empty()) { out.push_back(cur); cur.clear(); }
                if (tw(t) <= w) cur = t;
                else { const char * pp = t.c_str(); for (;;) { const char * qq = pp; uint32_t cp = u8next(qq); if (!cp) break; std::string ch(pp, qq - pp); pp = qq; if (!cur.empty() && tw(cur + ch) > w) { out.push_back(cur); cur.clear(); } cur += ch; } }
            }
            if (q >= s.size()) break;
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }
    static std::vector<std::string> clamp_lines(std::vector<std::string> v, int maxl, int w) {   /* maxl 줄을 넘으면 마지막 줄을 … 로 줄인다 */
        if ((int)v.size() <= maxl) return v;
        std::string rest; for (size_t k = (size_t)maxl - 1; k < v.size(); k++) { if (k > (size_t)maxl - 1) rest += " "; rest += v[k]; }
        v.resize((size_t)maxl); v[(size_t)maxl - 1] = fit_title(rest, w); return v;
    }
    static std::vector<std::string> title_lines(const std::string & s, int w, int maxl) {   /* 「제목」 / 「(부제)」 로 나눌 수 있으면 그 자리에서 */
        std::vector<std::string> parts; size_t p = s.find(" ("); if (p == std::string::npos) p = s.find(" [");
        if (p != std::string::npos && p > 0 && tw(s.substr(0, p)) <= w) { parts.push_back(s.substr(0, p)); parts.push_back(s.substr(p + 1)); } else parts.push_back(s);
        std::vector<std::string> out; for (auto & part : parts) for (auto & l : wrap_words(part, w)) out.push_back(l);
        return clamp_lines(out, maxl, w);
    }
    int pick_seen = -1, pick_since = 0;
    int mq_off(const std::string & full, int avail) const {   /* 선택된 줄: 1초 쉬고 가로로 흐르다 끝에서 잠깐 멈춘 뒤 처음으로 */
        int t = tw(full); if (t <= avail) return 0;
        int maxoff = t - avail + 8, span = maxoff * 25, hold0 = 1000, hold1 = 800, cyc = hold0 + span + hold1;
        int e = ts_ms - pick_since; if (e < 0) e = 0; int tc = e % cyc;
        if (tc < hold0) return 0; if (tc < hold0 + span) return (tc - hold0) / 25; return maxoff;
    }
    /* ================================================================ t39: 새 화면 그리기 */
    int def_ent(int gi) const {
        std::vector<int> v = vents(gi); if (v.empty()) return -1; const UiGame & g = games[gi];
        if (!g.defent.empty()) for (int i : v) if (g.ents[i].vgi < 0 && g.ents[i].name == g.defent) return i;
        for (int i : v) if (g.ents[i].vgi < 0) return i; return v[0];
    }
    void rowlist(Canvas & c, int x, int y, int w, int nvis, const std::vector<std::string> & r, int s, int pitch, const std::vector<uint32_t> * col = NULL, const std::vector<bool> * dm = NULL) {
        int n = (int)r.size(); int top = s - nvis / 2; if (top > n - nvis) top = n - nvis; if (top < 0) top = 0;
        for (int k = 0; k < nvis && top + k < n; k++) {
            int i = top + k, yy = y + k * pitch; bool d = dm && (*dm)[i]; uint32_t cc = (col && (*col)[i]) ? (*col)[i] : CY;
            if (i == s) c.rect(x - 6, yy - 3, w, 22, d ? GY : WH);
            c.clip(x, yy - 3, w - 12, 22); if (i == s) text(c, x, yy, r[i], K, d ? GY : WH); else text(c, x, yy, r[i], d ? GY : cc, K); c.unclip();
        }
        if (top > 0) text(c, x + w - 30, y - 18, "↑", CY, K); if (top + nvis < n) text(c, x + w - 30, y + nvis * pitch - 4, "↓", CY, K);
    }
    void d_pick(Canvas & c) {
        titlebar(c, "작품 선택");
        std::string bt = std::string("작품 선택  ") + (sortfav ? "★" : "") + sort_name(sortm) + (reorder ? "  [순서 이동]" : "");
        rbox(c, 16, 24, 288, 224, WH, bt.c_str());
        { int x = 36; for (int t = 0; t < 3; t++) { const char * nm = tab_name(t); int w = tw(nm) + 12; if (t == tab) { c.rect(x - 4, 42, w, 21, WH); text(c, x + 2, 45, nm, K, WH); } else text(c, x + 2, 45, nm, CY, K); x += w + 6; } }
        int nv = (int)vis.size(), n = nv + 1;
        if (pick != pick_seen) { pick_seen = pick; pick_since = ts_ms; }
        const int VIS = 7, PT = 24, LX = 40, LWD = 256, AVAIL = 236, Y0 = 70; int top = pick - 3; if (top > n - VIS) top = n - VIS; if (top < 0) top = 0;
        for (int k = 0; k < VIS && top + k < n; k++) {
            int i = top + k, yy = Y0 + k * PT; bool add = i >= nv; int gi = add ? -1 : vis[i];
            bool d = !add && !blk_row(gi).empty(), fv = !add && games[gi].fav; int wn = add ? 0 : row_warn(gi);
            int s0 = fv ? 16 : 0, ww = wn ? 16 : 0, av = AVAIL - s0 - ww; std::string full = add ? std::string("+ 작품 추가") : games[gi].title; bool sl = i == pick;
            if (sl) c.rect(LX - 8, yy - 3, LWD + 2, 22, d ? GY : WH);
            if (fv) text(c, LX, yy, "★", sl ? K : YL, sl ? (d ? GY : WH) : K);
            if (wn) text(c, LX + AVAIL - 16, yy, "※", sl ? K : OR, sl ? (d ? GY : WH) : K);
            c.clip(LX + s0, yy - 3, av, 22);
            if (sl) text(c, LX + s0 - mq_off(full, av), yy, full, K, d ? GY : WH); else text(c, LX + s0, yy, add ? full : ttl_fit(games[gi], av), d ? GY : CY, K);
            c.unclip();
        }
        if (top > 0) text(c, 284, 30, "↑", CY, K); if (top + VIS < n) text(c, 284, 228, "↓", CY, K);
        rbox(c, 320, 24, 304, 224, WH, "인포메이션");
        if (pick >= nv) {
            text(c, 344, 48, "새 작품을 등록합니다.", YL, K);
            text(c, 344, 78, "이미지 파일(zip 포함)이나 폴더를", WH, K); text(c, 344, 98, "고르면 형식과 구조를 판정해", WH, K); text(c, 344, 118, "자동으로 등록합니다.", WH, K);
            text(c, 344, 158, "게임 폴더를 지정해 두면 켤 때마다", CY, K); text(c, 344, 178, "새 작품을 찾아 자동 등록합니다.", CY, K);
            desc(c, 264, 64, { "+ 작품 추가: 이미지 파일 · 폴더 · 게임 폴더 지정", "회색 작품은 아직 실행할 수 없는 작품입니다 (사유는 인포메이션)." });
            helpline(c, "↑↓:선택, ○:작품 추가 화면, ×:취소"); return;
        }
        int gi = vis[pick]; const UiGame & gb = games[gi]; const UiGame & g = games[(cur >= 0 && cur < (int)games.size()) ? cur : gi]; const std::string & bk = blk_cur(); bool blk = !bk.empty();
        const int IX = 344, IW = 256, LB = 48;
        c.clip(IX - 4, 28, IW + 8, 216);
        { std::vector<std::string> tl = title_lines(gb.title, IW, 2); for (size_t i = 0; i < tl.size(); i++) text(c, IX, 34 + (int)i * 20, tl[i], blk ? WH : YL, K); }
        thumb(c, &g, IX, 78, 128, 80);
        { text(c, IX + 140, 80, "디스크", CY, K); std::vector<std::string> dl = clamp_lines(wrap_words(g.disknote.empty() ? std::string("-") : g.disknote, IW - 140), 3, IW - 140); for (size_t i = 0; i < dl.size(); i++) text(c, IX + 140, 98 + (int)i * 18, dl[i], WH, K); }
        int yy = 164;
        auto row = [&](const char * lab, const std::string & v, uint32_t col, int maxl) {
            text(c, IX, yy, lab, CY, K);
            std::vector<std::string> ls = clamp_lines(wrap_words(v.empty() ? std::string("-") : v, IW - LB), maxl, IW - LB); if (ls.empty()) ls.push_back("-");
            for (size_t i = 0; i < ls.size(); i++) text(c, IX + LB, yy + (int)i * 18, ls[i], col, K);
            yy += 18 * (int)ls.size();
        };
        row("제작", mk_line(gb), WH, 1);
        { int ei = row_ent(gi); std::string ev; uint32_t col = WH;
          if (ei < 0) ev = "자동 판정"; else { std::vector<int> v = tvents(gi); int idx = 0; for (size_t q = 0; q < v.size(); q++) if (v[q] == ei) idx = (int)q; const UiEnt & e = gb.ents[ei];
            ev = std::string(v.size() > 1 ? "← " : "") + e.label + (v.size() > 1 ? " →" : "") + " (" + std::to_string(idx + 1) + "/" + std::to_string((int)v.size()) + ")"; if (ent_warn(gi, ei)) col = OR; }
          row("실행", fit(ev, IW - LB), col, 1); }
        row("판정", g.jshort.empty() ? std::string("-") : g.jshort, WH, 1);
        if (blk) row("사유", bk, OR, 1);
        else { std::string st = "없음"; if (g.lastslot >= 0 && g.slot[g.lastslot].used) st = "슬롯 " + std::to_string(g.lastslot + 1) + "  " + g.slot[g.lastslot].stamp; row("저장", st, WH, 1); }
        c.unclip();
        std::vector<std::string> d;
        if (blk) d = { fit_title(gb.title, 560 - tw("   [실행 불가]")) + "   [실행 불가]", fit("사유: " + bk, 560) };
        else { std::string suf = g.lastslot >= 0 && g.slot[g.lastslot].used ? "   마지막 저장 " + g.slot[g.lastslot].stamp : std::string("   처음부터 시작합니다"); d = { fit_title(gb.title, 560 - tw(suf)) + suf, fit(g.judge.empty() ? (g.disk[0] + (g.disk[1].empty() ? std::string() : "    " + g.disk[1])) : g.judge, 560) }; }
        desc(c, 264, 64, d, blk ? 1 : -1);
        helpline(c, reorder ? "↑↓:순서 이동, ○ 또는 ×:끝" : "↑↓:작품, ←→:실행 방법, L R:탭, ○:정함, □:편집, △:정렬, SEL:부가, ×");
    }
    void d_sort(Canvas & c) {
        d_pick(c); int bx, by; confirm_box(c, 360, 262, "정렬", bx, by);
        std::vector<std::string> a; for (int i = 0; i < SM_N; i++) a.push_back(std::string(i == sortm ? "● " : "  ") + sort_name(i));
        a.push_back(std::string("즐겨찾기 먼저: ") + (sortfav ? "켬" : "끔")); a.push_back("사용자 지정 순서 바꾸기");
        std::vector<bool> dm(a.size(), false); dm[8] = sortm != SM_CUSTOM; items(c, bx + 30, by + 22, 310, a, sel, 24, &dm);
        helpline(c, "↑↓:선택, ○:결정, ×:닫기");
    }
    std::string ent_line(const UiGame & g, int ei, int w) { const UiEnt & e = g.ents[ei]; std::string t = e.label; if (!e.tag.empty()) t += "  [" + e.tag + "]"; return fit(t, w); }
    void d_ent(Canvas & c) {
        titlebar(c, ""); const UiGame & g = games[entg];
        rbox(c, 16, 24, 288, 224, WH, ttl_suf(g, "실행 방법", 256).c_str());
        std::vector<int> v = vents(entg); std::vector<std::string> r; std::vector<uint32_t> col; int di = def_ent(entg);
        if (v.empty()) { r.push_back("기본 (자동 판정)"); col.push_back(0); }
        for (int i : v) { const UiEnt & e = g.ents[i]; std::string mk = (i == di) ? "★ " : "  "; r.push_back(mk + fit(e.label + (e.tag.empty() ? std::string() : "  [" + e.tag + "]"), 200) + (ent_warn(entg, i) ? " ※" : "")); col.push_back(ent_warn(entg, i) ? OR : 0); }
        r.push_back("+ 새 실행 방법"); col.push_back(YL);
        r.push_back("언어: " + lang_label(g)); col.push_back(CY);   /* t44a (#494) */
        rowlist(c, 40, 48, 256, 7, r, sel, 24, &col);
        rbox(c, 320, 24, 304, 224, WH, "상세");
        int ni = (int)v.size(); const UiEnt * e = (sel < ni) ? &g.ents[v[sel]] : NULL;
        bool onlang = sel == (int)std::max<size_t>(v.size(), 1) + 1;
        if (onlang) {
            text(c, 340, 44, "이 작품의 언어", CY, K); text(c, 340, 68, fit(lang_label(g), 270), YL, K);
            text(c, 340, 96, "한글판: 한글 글꼴을 붙입니다.", WH, K); text(c, 340, 116, "원판 일본어: 일본어 글꼴을 씁니다.", WH, K); text(c, 340, 136, "자동: 제목·패치 표기로 판정합니다.", WH, K);
            text(c, 340, 168, "○:언어 고르기", CY, K);
        } else if (e && !e->pseudo) {
            int yy = 44; auto ln = [&](const char * lab, const std::string & val, uint32_t col2 = WH) { text(c, 340, yy, lab, CY, K); text(c, 340 + 76, yy, fit(val, 304 - 24 - 76 - 8), col2, K); yy += 20; };
            ln("출처", e->vgi >= 0 ? std::string("변형 폴더") : e->tag);
            const char * dn[] = { "A", "B", "C", "D" }; const char * dk[] = { "disk_a", "disk_b", "disk_c", "disk_d" };
            for (int q = 0; q < 4; q++) { auto it = e->kv.find(dk[q]); if (it != e->kv.end() && it->second != "-" && !it->second.empty()) ln((std::string("디스크") + dn[q]).c_str(), disk_label(it->second)); }
            { auto it = e->kv.find("cd"); if (it != e->kv.end() && !it->second.empty() && it->second != "-") ln("CD", disk_label(it->second)); }
            { auto it = e->kv.find("boot"); std::string b = it == e->kv.end() ? std::string("자동") : it->second; auto ft = e->kv.find("font"); auto fk = e->kv.find("font_kind"); ln("부팅", b + "   글꼴 " + (ft != e->kv.end() ? ft->second : fk != e->kv.end() ? (fk->second == "jp" ? std::string("일본어") : std::string("한글")) : std::string("목록 기본"))); }
            { auto it = e->kv.find("bootkeys"); auto sk = e->kv.find("swap"); int bn = it != e->kv.end() ? (int)ent::parse_steps(it->second).size() : 0, sn = 0; if (sk != e->kv.end()) { std::string s = sk->second; if (!s.empty()) { sn = 1; for (char ch : s) if (ch == ',') sn++; } } ln("자동입력", std::to_string(bn) + "단계   교체 " + std::to_string(sn) + "장"); }
            if (ent_warn(entg, v[sel])) ln("경고", e->warn_msg, OR);
        } else if (e) { text(c, 340, 44, "항목 정의가 없는 작품입니다.", WH, K); text(c, 340, 64, "자동 판정한 배치로 실행합니다.", WH, K); text(c, 340, 96, "□:이 배치를 복사해 편집", CY, K); }
        else { text(c, 340, 44, "새 실행 방법을 만듭니다.", WH, K); text(c, 340, 64, "템플릿 8종 또는 현재 항목 복제.", WH, K); }
        desc(c, 264, 64, { fit(msg.empty() ? std::string("실행 방법: 이 작품을 실행하는 디스크 구성입니다. 원본 이미지는 바뀌지 않습니다.") : msg, 560), fit(std::string("DB·game.ini 항목을 고치면 user.ini 에 덮어쓰기가 저장되고 원본은 그대로입니다."), 560) });
        helpline(c, "↑↓:선택, ○:이걸로 정함, □:편집, △:메뉴, ×:뒤로");
    }
    void d_entmenu(Canvas & c) {
        d_ent(c); int bx, by; confirm_box(c, 340, 214, "실행 방법 메뉴", bx, by);
        std::vector<std::string> a = { "기본으로 지정", "복제", "삭제 (숨김)", "DB 기본값으로 되돌리기", "위로", "아래로" };
        std::vector<bool> dm = menu_dim(); items(c, bx + 30, by + 24, 290, a, menusel, 28, &dm);
        helpline(c, "↑↓:선택, ○:결정, ×:닫기");
    }
    std::vector<bool> menu_dim() const {
        std::vector<bool> dm(6, false); std::vector<int> v = vents(entg); int ei = (menu_ent >= 0 && menu_ent < (int)v.size()) ? v[menu_ent] : -1; const UiGame & g = games[entg];
        if (ei < 0 || g.ents[ei].pseudo || g.ents[ei].vgi >= 0) { for (auto x : { 0, 2, 3, 4, 5 }) dm[x] = true; if (ei < 0 || g.ents[ei].pseudo) dm[1] = true; }
        else { if (g.ents[ei].tag != "수정됨") dm[3] = true; }
        return dm;
    }
    int menu_ent = 0;
    void d_tpl(Canvas & c) {
        titlebar(c, ""); rbox(c, 16, 24, 288, 296, WH, "새 실행 방법");
        std::vector<std::string> r; for (int i = 0; i < 8; i++) r.push_back(std::to_string(i + 1) + " " + ent::TPLS[i].label); r.push_back("- 지금 배치 복제");
        items(c, 40, 46, 256, r, sel, 28);
        rbox(c, 320, 24, 304, 296, WH, "설명");
        static const char * dd[9][4] = {
            { "하드디스크 한 장으로 부팅합니다.", "예: 동급생2, 열락의 학원", "", "" }, { "하드디스크로 부팅하고 FD 를 붙입니다.", "부팅 뒤 자동 입력도 쓸 수 있습니다.", "예: 나디아", "" },
            { "FD 한 장(또는 두 장)으로 부팅합니다.", "예: 대마도95, 키키모라, 바요엔", "", "" }, { "FD 여러 장을 도중에 갈아 끼웁니다.", "교체 목록을 따로 편집합니다.", "예: 마도물어 A.R.S", "" },
            { "CD 이미지를 붙이고 내장 DOS 로 시작합니다.", "자동 실행(autoexec)을 채워야 합니다.", "예: 마도물어 456", "" }, { "데모(오프닝) 디스크를 먼저 실행합니다.", "예: 환세희담", "", "" },
            { "원판 일본어 같은 변형을 만듭니다.", "글꼴을 일본어로 정해 둡니다.", "", "" }, { "하드디스크에 설치해야 하는 게임입니다.", "빈 HDD 를 새 이름으로 만듭니다.", "원본 이미지는 쓰기 대상에서 제외됩니다.", "" },
            { "지금 실제로 쓰는 배치를 그대로 복사해", "새 항목으로 편집합니다.", "", "" } };
        for (int i = 0; i < 4; i++) if (dd[sel < 9 ? sel : 0][i][0]) text(c, 344, 48 + i * 22, dd[sel < 9 ? sel : 0][i], i == 0 ? YL : WH, K);
        text(c, 344, 160, "이미지가 하나뿐이면 알맞은 칸에", CY, K); text(c, 344, 180, "자동으로 채웁니다.", CY, K);
        helpline(c, "↑↓:선택, ○:만들기, ×:뒤로");
    }
    static const char * mouse_label(const std::string & m) { if (m == "none") return "없음"; if (m == "optional") return "선택 사항"; if (m == "pointer") return "포인터"; if (m == "required") return "필수"; return "기본"; }
    std::string font_label(const std::string & f, const std::string & fk) const { if (!f.empty()) return f; if (fk == "jp") return "일본어 (목록 기본)"; if (fk == "kr") return "한글 (목록 기본)"; return "자동 (목록 기본)"; }
    void d_edit(Canvas & c) {
        titlebar(c, ""); const UiGame & g = games[eg];
        rbox(c, 16, 24, 608, 296, WH, ttl_suf(g, "편집", 560).c_str());
        const char * lab[] = { "이름", "디스크 A", "디스크 B", "디스크 C", "디스크 D", "CD", "부팅 대상", "자동 입력", "교체 목록", "글꼴", "마우스", "설치" };
        std::vector<std::string> v(12);
        v[0] = ed_name; const char * dk[] = { "disk_a", "disk_b", "disk_c", "disk_d", "cd" }; for (int i = 0; i < 5; i++) v[1 + i] = disk_label(kv_get(dk[i]));
        { std::string b = kv_get("boot"); v[6] = b.empty() ? "자동" : b; }
        { std::string s = kv_get("bootkeys"); v[7] = s.empty() ? std::string("없음") : std::to_string((int)ent::parse_steps(s).size()) + "단계"; }
        { std::string s = kv_get("swap"); int n = 0; if (!s.empty()) { n = 1; for (char ch : s) if (ch == ',') n++; } v[8] = std::to_string(n) + "장"; }
        v[9] = font_label(kv_get("font"), kv_get("font_kind")); v[10] = mouse_label(kv_get("mouse"));
        v[11] = kv_get("install") == "hdd_new" ? "HDD 새로 만들기 (" + (kv_get("hdd_mb").empty() ? std::string("40") : kv_get("hdd_mb")) + "MB)" : "아님";
        for (int i = 0; i < 12; i++) {
            int yy = 46 + i * 22; bool sl = i == sel; if (sl) c.rect(30, yy - 3, 580, 22, WH);
            text(c, 40, yy, lab[i], sl ? K : CY, sl ? WH : K); c.clip(160, yy - 3, 440, 22);
            bool valarr = (i == 6 || i == 10 || i == 11); text(c, 160, yy, std::string(valarr && sl ? "← " : "") + fit(v[i], 380) + (valarr && sl ? " →" : ""), sl ? K : WH, sl ? WH : K); c.unclip();
        }
        std::vector<ent::Issue> is = ed_issues(); std::vector<std::string> d;
        if (!msg.empty()) d.push_back(fit(msg, 560)); else d.push_back(is.empty() ? std::string("경고 없음") : fit(std::string(is[0].err ? "오류: " : "경고: ") + is[0].msg, 560));
        d.push_back(fit(std::string(ed_setdef ? "저장하면 기본 실행 방법으로 지정합니다." : "기본 지정은 SEL 로 켜고 끕니다.") + (ed_dirty() ? "  (바꾼 값 있음)" : ""), 560));
        desc(c, 324, 58, d, (!msg.empty() || (!is.empty() && is[0].err)) ? 0 : -1);
        helpline(c, "↑↓:선택, ←→:값, ○:고르기, □:저장, △:시험 실행, SEL:기본 지정, ×:나가기");
    }
    int fl_filter() const { if (fl_key == "disk_c" || fl_key == "disk_d") return 2; if (fl_key == "cd") return 3; return 1; }
    static std::string size_str(long long b) { char s[32]; if (b >= 1048576) snprintf(s, sizeof s, "%d.%dMB", (int)(b >> 20), (int)(((b * 10) >> 20) % 10)); else snprintf(s, sizeof s, "%dKB", (int)(b >> 10)); return s; }
    void d_file(Canvas & c) {
        titlebar(c, ""); static const char * kn[] = { "", "FD", "HDD", "CD" };
        std::string ttl = fl_key == "swap_add" ? "교체 목록에 넣을 디스크" : fl_key == "swap_set" ? "교체 디스크 바꾸기" : (fl_key == "cd" ? std::string("CD") : std::string("디스크 ") + (char)toupper(fl_key.back())) + " 고르기";
        rbox(c, 16, 24, 608, 296, WH, ttl.c_str());
        { int x = 36; for (size_t t = 0; t < fl_roots.size() && t < 5; t++) { std::string nm = fit(fl_roots[t].first, 120); int w = tw(nm) + 12; if ((int)t == fl_ri) { c.rect(x - 4, 42, w, 21, WH); text(c, x + 2, 45, nm, K, WH); } else text(c, x + 2, 45, nm, CY, K); x += w + 6; } }
        text(c, 36, 68, fit(fl_dir, 570), YL, K);
        std::vector<std::string> r; std::vector<uint32_t> col;
        for (auto & f : fl_items) { if (f.dir) { r.push_back((f.name == ".." ? std::string(".. (위로)") : "[" + f.name + "]")); col.push_back(YL); } else { std::string e = ent::ext_of(f.name); for (auto & ch : e) ch = (char)toupper((unsigned char)ch); r.push_back(fit(f.name, 400) + "   " + e + " " + size_str(f.size)); col.push_back(0); } }
        if (r.empty()) { r.push_back("(해당하는 파일이 없습니다)"); col.push_back(GY); }
        rowlist(c, 40, 94, 560, 9, r, sel, 24, &col);
        std::vector<std::string> d; d.push_back(fit(std::string("고를 것: ") + kn[fl_filter()] + " 이미지.  이 게임 폴더 안이면 파일 이름만, 밖이면 전체 경로를 저장합니다.", 560)); d.push_back(fit(std::string("정렬: ") + (fl_sort_kind ? "종류순" : "이름순"), 560));
        desc(c, 324, 58, d); helpline(c, "↑↓:선택, ○:고르기/열기, L R:위치, □:비우기, △:정렬, ×:취소");
    }
    void d_keyseq(Canvas & c) {
        titlebar(c, ""); rbox(c, 16, 24, 608, 296, WH, "부팅 후 자동 입력");
        std::vector<std::string> r; for (size_t i = 0; i < ks.size(); i++) r.push_back(std::to_string(i + 1) + "  " + ent::step_text(ks[i])); r.push_back("+ 단계 추가");
        rowlist(c, 40, 48, 560, 9, r, sel, 24);
        desc(c, 324, 58, { "부팅한 뒤 정한 시간에 키를 누르거나 글자를 입력합니다.", "게임 디스크가 잠깐 멈추는 안내 화면 넘기기 등에 씁니다." });
        helpline(c, "↑↓:선택, ○:편집, □:삭제, △:위로, SEL:아래로, ×:완료");
    }
    void d_step(Canvas & c) {
        titlebar(c, ""); rbox(c, 16, 24, 608, 200, WH, "자동 입력 단계");
        const ent::Step & st = ks[stepi]; std::string w; { char b[32]; snprintf(b, sizeof b, "%d.%d초", st.ms / 1000, (st.ms / 100) % 10); w = b; }
        const char * lab[] = { "종류", "대기", st.type ? "입력할 글자" : "누를 키" }; std::string val[3] = { st.type ? "글자 입력" : "키 누르기", w + " 뒤", ent::show_val(st.val) };
        for (int i = 0; i < 3; i++) { int yy = 52 + i * 30; bool sl = i == sel; if (sl) c.rect(30, yy - 3, 580, 22, WH); text(c, 40, yy, lab[i], sl ? K : CY, sl ? WH : K); text(c, 200, yy, std::string(sl && (i < 2 || !st.type) ? "← " : "") + val[i] + (sl && (i < 2 || !st.type) ? " →" : ""), sl ? K : WH, sl ? WH : K); }
        desc(c, 240, 64, { st.type ? "글자 입력: ○ 로 화면 키보드, ←→ 로 자주 쓰는 명령(dir 등)." : "키 누르기: ←→ 로 키를 고릅니다.", "대기는 0.5초 단위입니다. ~ 는 Enter 입니다." });
        helpline(c, "↑↓:선택, ←→:값, ○:입력, ×:돌아가기");
    }
    void d_swaped(Canvas & c) {
        titlebar(c, ""); rbox(c, 16, 24, 608, 296, WH, "교체 목록 (게임 중 메뉴와 같은 순서)");
        std::vector<std::string> r; for (size_t i = 0; i < sw.size(); i++) r.push_back(std::to_string(i + 1) + "  " + fit(sw[i], 520)); r.push_back("+ 디스크 추가 (이미지 고르기)");
        rowlist(c, 40, 48, 560, 9, r, sel, 24);
        desc(c, 324, 58, { "게임 중 메뉴의 「디스크 교체」에 나오는 디스크와 순서입니다.", "시작부터 교체가 필요한 작품은 처음 3초에 안내가 뜹니다." });
        helpline(c, "↑↓:선택, ○:바꾸기, □:삭제, △:위로, SEL:아래로, ×:완료");
    }
    void d_font(Canvas & c) {
        titlebar(c, ""); std::string ttl = fnt_mode == 0 ? "한글 글꼴" : fnt_mode == 1 ? "일본어 글꼴" : "실행 방법 글꼴";
        rbox(c, 16, 24, 288, 296, WH, ttl.c_str());
        std::vector<std::string> r; std::vector<uint32_t> col; std::string curv = fnt_cur();
        for (auto & o : fnt_opts) { r.push_back(std::string(o == curv ? "● " : "  ") + fit(o.empty() ? std::string(fnt_mode == 2 ? "자동 (목록 기본)" : "기본") : o, 220)); col.push_back(0); }
        if (fnt_opts.empty()) { r.push_back("(고를 수 있는 글꼴 없음)"); col.push_back(GY); }
        rowlist(c, 40, 48, 256, 9, r, sel, 24, &col);
        rbox(c, 320, 24, 304, 296, WH, "미리보기");
        std::string pv = sel < (int)fnt_opts.size() ? fnt_opts[sel] : std::string(); if (pv.empty()) pv = fnt_default_name();
        const ov::BmpFont * bf = fnt_font(pv);
        text(c, 340, 44, fit(pv.empty() ? std::string("-") : pv, 270), YL, K);
        { std::string grp; for (auto & f : fonts) if (f.name == pv) grp = f.group; text(c, 340, 64, fit(grp.empty() ? std::string("묶음 -") : std::string(fontreg::group_label(grp)), 270), CY, K); }
        if (bf) { text_bmp(c, *bf, 340, 100, "한글 가나다라마바사", WH, K); text_bmp(c, *bf, 340, 124, "열락의 학원 신비한 바다", WH, K); text_bmp(c, *bf, 340, 148, "あいうえお アイウエオ", WH, K); } else text(c, 340, 100, "미리볼 수 없는 글꼴입니다.", GY, K);
        { int rej = 0; for (auto & f : fonts) if (!f.ok) rej++; if (rej) text(c, 340, 280, fit("거른 파일 " + std::to_string(rej) + "개 (형식·크기 불일치)", 270), OR, K); }
        text(c, 340, 196, fit("같은 묶음의 글꼴만 바꿀 수 있습니다.", 270), WH, K); text(c, 340, 216, fit("다른 묶음은 한글이 깨져 막아 둡니다.", 270), WH, K);
        if (!fnt_need.empty()) text(c, 340, 240, fit("이 작품의 묶음: " + std::string(fontreg::group_label(fnt_need)), 270), CY, K);
        desc(c, 324, 58, { fnt_mode == 2 ? "이 실행 방법의 글꼴입니다. 자동 = 목록 기본(한글판=한글, 원판=일본어)." : "전역 기본 글꼴입니다. 실행 방법이 글꼴을 따로 정하면 그것이 우선입니다.", "글꼴 폴더에 bmp 를 넣으면 자동으로 목록에 나옵니다." });
        helpline(c, "↑↓:선택, ○:정함, ×:취소");
    }
    std::string fnt_default_name() const { if (fnt_mode == 1) return font_jp.empty() ? "jp_default" : font_jp; return font_kr.empty() ? "pc98player_font" : font_kr; }
    std::string fnt_cur() const { if (fnt_mode == 0) return font_kr; if (fnt_mode == 1) return font_jp; return kv_get("font"); }
    const ov::BmpFont * fnt_font(const std::string & name) {
        if (name.empty()) return NULL; auto it = fnt_cache.find(name); if (it != fnt_cache.end()) return it->second.ok ? &it->second : NULL;
        ov::BmpFont & f = fnt_cache[name]; for (auto & x : fonts) if (x.name == name && x.ok) { f.load(x.path); break; } return f.ok ? &f : NULL;
    }
    /* ================= t48b: 한글 화면 키보드 (이름 칸). 패드 · 마우스/터치 · 물리 키보드(보조) 모두. 게임으로 키가 가지 않는다(메뉴 프로세스, kbov 는 메뉴 중 꺼짐) ================= */
    static const char * hk_row(int r) { static const char * const R[4] = { "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm.,_" }; return R[r]; }
    static int hk_len(int r) { return r < 4 ? 10 : 6; }
    static void hk_rect(int r, int q, int & x, int & y, int & w, int & h) {
        h = 28; y = 84 + r * 34;
        if (r < 4) { w = 52; x = 40 + q * 56; return; }
        static const int W[6] = { 84, 84, 120, 84, 84, 84 }; x = 40; for (int i = 0; i < q; i++) x += W[i] + 8; w = W[q];
    }
    std::string hk_label(int r, int q) const {
        if (r == 4) { static const char * const L[6] = { "한/영", "SHIFT", "띄어쓰기", "지움", "완료", "취소" }; return L[q]; }
        char ch = hk_row(r)[q]; bool al = ch >= 'a' && ch <= 'z'; std::string s;
        if (al && hc.han) hg::put_utf8(s, hg::key_to_jamo(ch, osk_shift)); else s += (al && osk_shift) ? (char)(ch - 32) : ch;
        return s;
    }
    void hk_cancel() { scr = osk_ret; if (osk_ret == S_EDIT) sel = 0; }
    void hk_press(int r, int q) {   /* 키 하나를 누른다(패드 ○ · 마우스 · 시험 스크립트 공통). 맨 아랫줄 = 한/영 · SHIFT · 띄어쓰기 · 지움 · 완료 · 취소 */
        hc.buf = osk_buf;
        if (r < 4) {
            char ch = hk_row(r)[q]; bool al = ch >= 'a' && ch <= 'z';
            if (al && hc.han) hc.key_jamo(hg::key_to_jamo(ch, osk_shift)); else hc.put_ascii((al && osk_shift) ? (char)(ch - 32) : ch);
            osk_shift = false;
        } else switch (q) {
            case 0: hc.toggle(); osk_shift = false; break;
            case 1: osk_shift = !osk_shift; break;
            case 2: hc.put_ascii(' '); osk_shift = false; break;
            case 3: hc.back(); osk_shift = false; break;
            case 4: osk_buf = hc.buf; osk_done(); return;
            case 5: osk_buf = hc.buf; hk_cancel(); return; }
        osk_buf = hc.buf;
    }
    void hk_key(UKey k) {   /* 패드: 방향 = 이동, ○ = 누르기, □ = 지움, △ = 한/영, L = SHIFT, R = 띄어쓰기, SELECT = 완료, × = 취소 */
        auto clampc = [&]() { int n = hk_len(osk_r); if (osk_c >= n) osk_c = n - 1; };
        if (k == UK_UP) { osk_r = (osk_r + 4) % 5; clampc(); } else if (k == UK_DOWN) { osk_r = (osk_r + 1) % 5; clampc(); }
        else if (k == UK_LEFT) { int n = hk_len(osk_r); osk_c = (osk_c + n - 1) % n; } else if (k == UK_RIGHT) { int n = hk_len(osk_r); osk_c = (osk_c + 1) % n; }
        else if (k == UK_OK) hk_press(osk_r, osk_c);
        else if (k == UK_SQ) hk_press(4, 3); else if (k == UK_TRI) hk_press(4, 0); else if (k == UK_L) hk_press(4, 1); else if (k == UK_R) hk_press(4, 2);
        else if (k == UK_SELECT) hk_press(4, 4); else if (k == UK_CANCEL) hk_press(4, 5);
    }
    bool pointer(int vx, int vy) {   /* 마우스 · 터치: 가상 640x400 좌표. 키 위면 그 키를 누른다 */
        if (scr != S_OSK || !osk_hg) return false;
        for (int r = 0; r < 5; r++) for (int q = 0; q < hk_len(r); q++) { int x, y, w, h; hk_rect(r, q, x, y, w, h); if (vx >= x && vx < x + w && vy >= y && vy < y + h) { osk_r = r; osk_c = q; hk_press(r, q); return true; } }
        return false;
    }
    void hk_text(const std::string & s) {   /* 물리 키보드(SDL 글자 이벤트): 영문 글자는 한 모드면 두벌식, 그 밖(IME 가 확정한 한글 · 숫자 · 기호)은 그대로 */
        hc.buf = osk_buf; size_t i = 0;
        while (i < s.size()) {
            unsigned char b = (unsigned char)s[i]; size_t n = b < 0x80 ? 1 : (b >= 0xF0 ? 4 : (b >= 0xE0 ? 3 : (b >= 0xC0 ? 2 : 1))); if (i + n > s.size()) n = s.size() - i;
            if (b < 0x80) { if (b >= 0x20 && b != 0x7F) { if (hc.han && ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z'))) hc.key_jamo(hg::key_to_jamo((char)b, b >= 'A' && b <= 'Z')); else hc.put_ascii((char)b); } }
            else hc.put_utf8_text(s.substr(i, n));
            i += n;
        }
        osk_buf = hc.buf;
    }
    void hk_script(const std::string & sc) {   /* 시험 전용: hg::run_script 와 같은 토큰으로 실제 키(hk_press)를 누른다. 영문 = 글자 키, ^ = SHIFT, _ = 띄어쓰기, < = 지움, ! = 한/영 */
        for (char ch : sc) {
            if (ch == '^') { hk_press(4, 1); continue; } if (ch == '!') { hk_press(4, 0); continue; } if (ch == '<') { hk_press(4, 3); continue; } if (ch == '_') { hk_press(4, 2); continue; }
            bool found = false; char lc = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
            for (int r = 0; r < 4 && !found; r++) for (int q = 0; q < 10; q++) if (hk_row(r)[q] == lc) { if (ch >= 'A' && ch <= 'Z') hk_press(4, 1); hk_press(r, q); found = true; break; }
        }
    }
    void d_hosk(Canvas & c) {
        titlebar(c, ""); rbox(c, 16, 24, 608, 236, WH, "이름 입력 (한글 키보드)");
        { std::string sb = fit(osk_buf, 430); int cx = 40 + tw(sb); text(c, 40, 48, sb, YL, K);
          std::string cu = hg::cur_text(hc.cur); if (!cu.empty()) { text(c, cx, 48, cu, CY, K); int cw = tw(cu); c.rect(cx, 66, cw, 2, CY); cx += cw; }
          if (!ime_comp.empty()) { text(c, cx, 48, ime_comp, CY, K); int cw = tw(ime_comp); c.rect(cx, 66, cw, 2, CY); cx += cw; }
          text(c, cx, 48, "_", YL, K); }
        text(c, 480, 48, hc.han ? "[한]" : "[영]", CY, K); if (osk_shift) text(c, 540, 48, "SHIFT", OR, K);
        for (int r = 0; r < 5; r++) for (int q = 0; q < hk_len(r); q++) {
            int x, y, w, h; hk_rect(r, q, x, y, w, h); bool sl = r == osk_r && q == osk_c; std::string lb = hk_label(r, q);
            c.rect(x, y, w, h, sl ? WH : GY); c.rect(x + 1, y + 1, w - 2, h - 2, sl ? WH : K);
            uint32_t fg = (r == 4 && q == 1 && osk_shift) ? OR : ((r == 4 && q == 0) ? CY : WH);
            text(c, x + (w - tw(lb)) / 2, y + 4, lb, sl ? K : fg, sl ? WH : K);
        }
        desc(c, 270, 58, { "SHIFT: 다음 한 자모만 쌍자음·ㅒㅖ", "패드: L=SHIFT △=한/영 □=지움 R=띄우기" });
        helpline(c, "↑↓←→:이동, ○:입력, SEL:완료, ×:취소");
    }
    void d_osk(Canvas & c) {
        if (osk_hg) { d_hosk(c); return; }   /* t48b */
        titlebar(c, ""); rbox(c, 16, 24, 608, 296, WH, osk_target == 0 ? "이름 입력" : "글자 입력");
        { std::string sb = fit(osk_buf, 500); int cx = 40 + tw(sb); text(c, 40, 48, sb, YL, K); if (!ime_comp.empty()) { text(c, cx, 48, ime_comp, CY, K); int cw = tw(ime_comp); c.rect(cx, 66, cw, 2, CY); cx += cw; } text(c, cx, 48, "_", YL, K); }   /* t48a: 조합 중 글자는 밑줄 */
        static const char * rowsl[5] = { "abcdefghij", "klmnopqrst", "uvwxyz0123", "456789.-/\\", ":|~_'() " };
        for (int r = 0; r < 5; r++) for (int q = 0; q < 10 && q < (int)strlen(rowsl[r]); q++) {
            char ch = rowsl[r][q]; if (!ch) continue; if (osk_upper && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32); int x = 120 + q * 40, y = 92 + r * 36; bool sl = r == osk_r && q == osk_c;
            c.rect(x - 4, y - 4, 32, 28, sl ? WH : GY); c.rect(x - 3, y - 3, 30, 26, sl ? WH : K);
            char s[4] = { ch == ' ' ? '_' : ch, 0, 0, 0 }; text(c, x + 4, y, s, sl ? K : WH, sl ? WH : K);
        }
        desc(c, 324, 58, { osk_target == 0 ? "키보드가 있으면 한/영 키로 한글도 그대로 칠 수 있습니다." : "DOS 명령용입니다. 대문자는 소문자로 저장되고 ~ 는 Enter 입니다.", "키보드가 있으면 그대로 쳐도 됩니다." });
        helpline(c, "↑↓←→:이동, ○:입력, □:지움, △:대소문자, SEL:완료, ×:취소");
    }
    void d_name(Canvas & c) {
        titlebar(c, ""); rbox(c, 16, 24, 288, 296, WH, "이름 고르기");
        std::vector<std::string> r = name_presets(); r.push_back("직접 입력 (한글 키보드)"); r.push_back("직접 입력 (영문 글자표)"); items(c, 40, 44, 256, r, sel, 24);   /* t48b: 11줄이라 간격 24 */
        rbox(c, 320, 24, 304, 296, WH, "설명"); text(c, 344, 48, "목록에서 이름을 고르면 같은 이름이", WH, K); text(c, 344, 68, "이미 있을 때 번호를 붙입니다.", WH, K); text(c, 344, 104, "현재 이름", CY, K); text(c, 344, 124, fit(ed_name, 250), YL, K);
        helpline(c, "↑↓:선택, ○:정함, ×:취소");
    }
    void d_yn(Canvas & c, Scr under) {
        switch (under) { case S_EDIT: d_edit(c); break; case S_ENT: d_ent(c); break; case S_MAIN: d_main(c, 0); break; case S_ENTMENU: d_ent(c); break; default: break; }
        int bw = 420, bx, by; confirm_box(c, bw, 150, "확인", bx, by);
        for (int i = 0; i < 3; i++) if (!yn_msg[i].empty()) text(c, bx + 24, by + 22 + i * 22, fit(yn_msg[i], bw - 48), WH, K);
        const char * q[] = { "예", "아니오" }; int cw = choices_w(q, 2); choices(c, bx + ((bw - cw) >> 1) + 4, by + 100, q, 2, ynsel, WH, false);
        helpline(c, "←→:선택, ○:결정, ×:아니오");
    }
    void d_hddask(Canvas & c) {
        titlebar(c, ""); int bx, by; confirm_box(c, 460, 180, "설치가 필요한 게임", bx, by);
        text(c, bx + 24, by + 22, "이 게임은 하드디스크에 설치해야 합니다.", WH, K);
        text(c, bx + 24, by + 44, fit("하드디스크를 만들까요? (" + (kv_get("hdd_mb").empty() ? std::string("40") : kv_get("hdd_mb")) + "MB, 원본은 그대로)", 412), WH, K);
        std::vector<std::string> a = { "예, 새로 만듭니다", "이미 있는 HDD 를 씁니다", "취소" }; items(c, bx + 40, by + 84, 380, a, hddsel, 28);
        helpline(c, "↑↓:선택, ○:결정, ×:취소");
    }
    void d_add(Canvas & c) {
        titlebar(c, "작품 추가");
        rbox(c, 16, 24, 288, 224, WH, "작품 추가");
        bool hasgf = !gamefolder.empty();
        std::vector<std::string> n = { "이미지 파일 고르기", "폴더 고르기", "게임 폴더 지정", "게임 폴더 다시 검색", "게임 폴더 해제", "방금 추가한 작품 언어 지정" };
        std::vector<bool> dm = { false, false, false, !hasgf, !hasgf, add_new.empty() };
        items(c, 40, 48, 256, n, sel, 26, &dm);
        rbox(c, 320, 24, 304, 224, WH, "인포메이션");
        text(c, 344, 44, "게임 폴더", CY, K);
        text(c, 344 + 80, 44, hasgf ? fit(gamefolder, 304 - 24 - 80 - 16) : std::string("(없음)"), hasgf ? WH : GY, K);
        int yy = 72; if (add_log.empty()) text(c, 344, yy, "아직 추가한 작품이 없습니다.", GY, K);
        for (size_t i = 0; i < add_log.size() && i < 8; i++) { text(c, 344, yy, fit(add_log[i], 304 - 48), i == 0 ? YL : WH, K); yy += 20; }
        static const char * dd[] = { "고른 이미지 파일들을 한 작품으로 등록합니다 (여러 장 가능).", "폴더 안의 이미지를 찾아 등록합니다. 하위 폴더는 작품마다 따로.", "이 폴더를 켤 때마다 훑어 새 작품을 자동으로 등록합니다.", "게임 폴더를 지금 다시 훑습니다.", "게임 폴더 지정을 풀고 자동 등록을 멈춥니다 (등록된 작품은 남음).", "방금 추가한 작품마다 한글판 / 원판 일본어 / 자동을 고릅니다." };
        desc(c, 264, 64, { dd[sel < 6 ? sel : 0], "형식은 확장자가 아니라 파일 머리로 판정합니다." });
        helpline(c, "↑↓:선택, ○:결정, ×:돌아가기");
    }
    void d_screen(Canvas & c) {   /* t48c (#624·#625): 필터 1줄 + 효과 4줄(끔~100% 10단계, 좌우 키로 조절) + 곡면. 줄 간격 20 으로 9줄이 같은 상자에 들어간다 */
        titlebar(c, "화면 설정");
        Settings & s = eff();
        rbox(c, 16, 24, 288, 212, WH, "화면 설정");
        std::vector<std::string> n = { "적용 범위", "비율", "필터", "주사선", "마스크", "RGB 번짐", "빛 번짐", "곡면", "창 배율" };
        items(c, 40, 44, 256, n, sel, 20);
        rbox(c, 320, 24, 304, 212, WH, "선택지");
        auto row = [&](int i) { return sel == i; };
        const int PY = 20;
        const char * sc[] = { "전역", "이 작품" }; choices(c, 344, 44, sc, 2, scope, WH, row(0));
        const char * rt[] = { "4:3", "16:10", "정수배" }; choices(c, 344, 44 + PY, rt, 3, s.ratio, WH, row(1));
        const char * sh[] = { "sharp", "nearest" }; choices(c, 344, 44 + 2 * PY, sh, 2, s.shader == 2 ? 1 : 0, WH, row(2));
        char b[16]; const int fxv[4] = { s.scan, s.mask, s.bleed, s.glow };
        for (int i = 0; i < 4; i++) { int y = 44 + (3 + i) * PY; if (fxv[i] <= 0) snprintf(b, sizeof b, "끔"); else snprintf(b, sizeof b, "%d%%", fxv[i] * 10); barv(c, 340, y, 232, fxv[i] / 10.0, "끔", "최대", WH, row(3 + i)); text(c, 578, y, b, WH, K); }
        snprintf(b, sizeof b, "%.2f", s.curve_f()); barv(c, 340, 44 + 7 * PY, 232, s.curve / 10.0, "평면", "최대", WH, row(7)); text(c, 578, 44 + 7 * PY, b, WH, K);
        { const char * wm[] = { "x1", "x2", "x3", "x4", "x5", "사용자 지정" }; unsigned dmk = 0; for (int i = 0; i < WMN; i++) if (i + 1 > wmax) dmk |= 1u << i;
          choicesd(c, 344, 44 + 8 * PY, wm, 6, wmult ? wmult - 1 : 5, WH, dmk); }
        rbox(c, 16, 240, 608, 82, WH, "미리보기");
        thumb(c, game(), 40, 258, 96, 60);
        const UiGame * gm = game();
        { std::string sf = gm && gm->lastslot >= 0 ? "  마지막 저장 (" + gm->slot[gm->lastslot].stamp + ")" : std::string(); text(c, 156, 258, gm && gm->lastslot >= 0 ? fit_title(gm->title, 440 - tw(sf)) + sf : std::string("표본 화면"), CY, K); }
        text(c, 156, 278, "값을 바꾸면 이 화면에도 바로 적용됩니다.", WH, K);
        text(c, 156, 298, "○:확정   ×:바꾸기 전 값으로 되돌림", WH, K);
        char wb[96]; if (wmult) snprintf(wb, sizeof wb, "창 x%d (%dx%d) · 회색 = 모니터에 안 들어감 · Alt+[ ]", wmult, 640 * wmult, 480 * wmult); else snprintf(wb, sizeof wb, "창 사용자 지정 %dx%d · 테두리로 끈 크기 · Alt+[ ]", winw, winh);
        static std::string wd; wd = wb;
        static const char * dd[] = { "적용 범위: 전역은 모든 작품, 이 작품은 고른 작품만.", "화면 비율: 4:3 은 원래 비율, 정수배는 또렷한 배율.", "필터: sharp 는 또렷하게, nearest 는 원본 그대로. 효과는 어느 필터에나 얹습니다.", "주사선: 가로줄 무늬의 진하기 (끔 ~ 100%).", "마스크: 화소 배열 무늬의 진하기 (끔 ~ 100%).", "RGB 번짐: 색 어긋남과 가로 번짐 (가장자리일수록 큼, 끔 ~ 100%).", "빛 번짐: 밝은 곳 주변으로 빛이 퍼짐 (끔 ~ 100%).", "곡면: 화면 가장자리를 둥글게 보이게 합니다." };
        desc(c, 324, 44, { fit(sel < 8 ? std::string(dd[sel]) : wd, 560) });
        helpline(c, "↑↓:선택, ←→:값, ○:결정, ×:취소(되돌림), △:출하 시 설정");
    }
    void d_screen_t48b2_old(Canvas & c) {
        titlebar(c, "화면 설정");
        Settings & s = eff(); bool crt = s.shader == 1;
        rbox(c, 16, 24, 288, 212, WH, "화면 설정");
        std::vector<std::string> n = { "적용 범위", "비율", "셰이더", "곡면", "번짐", "주사선 세기", "마스크", "창 배율" };
        std::vector<bool> dim = { false, false, false, !crt, !crt, !crt, !crt, false };
        items(c, 40, 44, 256, n, sel, 24, &dim);
        rbox(c, 320, 24, 304, 212, WH, "선택지");
        auto row = [&](int i) { return sel == i; };
        const char * sc[] = { "전역", "이 작품" }; choices(c, 344, 44, sc, 2, scope, WH, row(0));
        const char * rt[] = { "4:3", "16:10", "정수배" }; choices(c, 344, 68, rt, 3, s.ratio, WH, row(1));
        const char * sh[] = { "sharp", "crt", "nearest" }; choices(c, 344, 92, sh, 3, s.shader, WH, row(2));
        if (crt) {
            char b[16]; snprintf(b, sizeof b, "%.2f", s.curve_f()); barv(c, 340, 116, 232, s.curve / 10.0, "평면", "최대", WH, row(3)); text(c, 578, 116, b, WH, K);
            snprintf(b, sizeof b, "%d", s.bleed * 10); barv(c, 340, 140, 232, s.bleed / 10.0, "없음", "최대", WH, row(4)); text(c, 578, 140, b, WH, K);
            snprintf(b, sizeof b, "%d", s.scan * 10); barv(c, 340, 164, 232, s.scan / 10.0, "약", "강", WH, row(5)); text(c, 578, 164, b, WH, K);
            const char * mk[] = { "꺼짐", "약", "중", "강" }; choices(c, 344, 188, mk, 4, s.mask, WH, row(6));
        } else for (int i = 3; i < 7; i++) text(c, 344, 44 + i * 24, "(crt 에서만)", GY, K);
        { const char * wm[] = { "x1", "x2", "x3", "x4", "x5", "사용자 지정" }; unsigned dmk = 0; for (int i = 0; i < WMN; i++) if (i + 1 > wmax) dmk |= 1u << i;
          choicesd(c, 344, 212, wm, 6, wmult ? wmult - 1 : 5, WH, dmk); }
        rbox(c, 16, 240, 608, 82, WH, "미리보기");
        thumb(c, game(), 40, 258, 96, 60);
        const UiGame * gm = game();
        { std::string sf = gm && gm->lastslot >= 0 ? "  마지막 저장 (" + gm->slot[gm->lastslot].stamp + ")" : std::string(); text(c, 156, 258, gm && gm->lastslot >= 0 ? fit_title(gm->title, 440 - tw(sf)) + sf : std::string("표본 화면"), CY, K); }
        text(c, 156, 278, "값을 바꾸면 이 화면에도 바로 적용됩니다.", WH, K);
        text(c, 156, 298, "○:확정   ×:바꾸기 전 값으로 되돌림", WH, K);
        char wb[96]; if (wmult) snprintf(wb, sizeof wb, "창 x%d (%dx%d) · 회색 = 모니터에 안 들어감 · Alt+[ ]", wmult, 640 * wmult, 480 * wmult); else snprintf(wb, sizeof wb, "창 사용자 지정 %dx%d · 테두리로 끈 크기 · Alt+[ ]", winw, winh);
        static std::string wd; wd = wb;
        static const char * dd[] = { "적용 범위: 전역은 모든 작품, 이 작품은 고른 작품만.", "화면 비율: 4:3 은 원래 비율, 정수배는 또렷한 배율.", "셰이더: sharp 는 또렷, crt 는 주사선과 마스크, nearest 는 원본.", "모니터 곡면: crt 셰이더에서 화면 가장자리를 둥글게 보이게 합니다.", "번짐: 색 어긋남·가로 번짐·밝은 곳 번짐을 함께 줍니다 (0 = 끔).", "주사선 세기: 가로줄 무늬의 진하기입니다.", "마스크: 화소 배열 무늬의 진하기입니다." };
        desc(c, 324, 44, { fit(sel < 7 ? std::string(dd[sel]) : wd, 560) });
        helpline(c, "↑↓:선택, ←→:값, ○:결정, ×:취소(되돌림), △:출하 시 설정");
    }
    void d_conf(Canvas & c) {
        titlebar(c, "소리/패드/배속 설정");
        Settings & s = eff();
        rbox(c, 16, 24, 288, 296, WH, "동작 환경 설정");
        std::vector<std::string> n = { "적용 범위", "BGM 음량", "효과음 음량", "소리 출력 장치", "조작 설정", "배속 배율", "배속 방식", "배속 중 소리", "부팅 시퀀스", "배속 표시", "한글 글꼴", "일본어 글꼴" };
        items(c, 40, 44, 256, n, sel, 24);
        rbox(c, 320, 24, 304, 296, WH, "인포메이션");
        auto row = [&](int i) { return sel == i; };
        auto ry = [](int i) { return 44 + 24 * i; };
        const char * sc[] = { "전역", "이 작품" }; choices(c, 344, ry(0), sc, 2, scope, WH, row(0));
        char b[8];
        snprintf(b, sizeof b, "%d", s.bgm); barv(c, 340, ry(1), 232, s.bgm / 8.0, "OFF", "최대", WH, row(1)); text(c, 590, ry(1), b, WH, K);
        snprintf(b, sizeof b, "%d", s.se); barv(c, 340, ry(2), 232, s.se / 8.0, "OFF", "최대", WH, row(2)); text(c, 590, ry(2), b, WH, K);
        text(c, 344, ry(3), "기본 장치", row(3) ? K : WH, row(3) ? WH : K);
        text(c, 344, ry(4), s.pad == 1 ? "조작 설정 (서양식)" : "조작 설정 (일본식)", row(4) ? K : WH, row(4) ? WH : K);   /* t48e (#646): 고정 글자 → 조작 설정 화면 입구 */
        const char * bm[] = { "x2", "x3", "x4", "x6", "x8" }; choices(c, 344, ry(5), bm, 5, s.bmult, WH, row(5));
        const char * bk[] = { "홀드", "토글", "둘 다" }; choices(c, 344, ry(6), bk, 3, s.bmode, WH, row(6));
        const char * bs[] = { "끔", "빠르게", "음높이 유지" }; choices(c, 344, ry(7), bs, 3, s.bsound, WH, row(7));
        const char * bt[] = { "켬", "끔" }; choices(c, 344, ry(8), bt, 2, s.boot, WH, row(8));
        { const char * bo[] = { "끔", "켬" }; choices(c, 344, ry(9), bo, 2, s.bosd, WH, row(9)); }
        text(c, 344, ry(10), fit(font_kr.empty() ? std::string("기본 (pc98player_font)") : font_kr, 250), row(10) ? K : WH, row(10) ? WH : K);
        text(c, 344, ry(11), fit(font_jp.empty() ? std::string("기본 (jp_default)") : font_jp, 250), row(11) ? K : WH, row(11) ? WH : K);
        static const char * dd[] = { "적용 범위: 전역은 모든 작품, 이 작품은 고른 작품만.", "BGM 음량입니다. 효과음과 따로 조절합니다.", "효과음 음량입니다. 시작 소리도 이 값을 따릅니다.", "소리가 나오는 장치입니다. 지금은 기본 장치만 씁니다.", "패드 배치입니다. ○가 결정, ×가 취소입니다.", "배속 배율입니다.", "홀드는 누르는 동안, 토글은 눌러 켜고 끄기, 둘 다는 둘 다.", "끔=무음 · 빠르게=음높이 상승 · 음높이 유지=높이 그대로", "시작할 때 나오는 부팅 화면을 켜고 끕니다.", "배속 중에 오른쪽 위에 ▶▶ 배율을 계속 표시합니다. 캡처에는 찍히지 않습니다.", "한글판 기본 글꼴입니다. ○ 로 고릅니다. 같은 배치의 글꼴만 나옵니다.", "원판(일본어) 기본 글꼴입니다. ○ 로 고릅니다." };
        desc(c, 328, 44, { fit(dd[sel], 560) });
        helpline(c, "↑↓:선택, ←→:값, ○:결정, ×:취소, △:출하 시 설정");
    }
    void d_save(Canvas & c) {
        titlebar(c, "세이브 관리");
        const UiGame * gm = game();
        rbox(c, 16, 24, 288, 296, WH, gm ? ttl_suf(*gm, "슬롯", 256).c_str() : "슬롯");
        for (int i = 0; i < 8; i++) {
            int yy = 44 + i * 30; const SlotInfo * si = gm ? &gm->slot[i] : NULL; bool u = si && si->used;
            std::string s = "슬롯 " + std::to_string(i + 1) + "   " + (u ? si->stamp : "(비어 있음)");
            if (i == slotsel) { c.rect(34, yy - 3, 256, 22, WH); text(c, 40, yy, s, K, WH); } else text(c, 40, yy, s, u ? CY : GY, K);
        }
        rbox(c, 320, 24, 304, 296, WH, "인포메이션");
        const SlotInfo * si = gm ? &gm->slot[slotsel] : NULL;
        if (si && si->used) {
            c.rect(343, 43, 258, 162, GY); c.rect(344, 44, 256, 160, K); if (!si->th.empty()) blit_scaled(c, si->th.data(), si->tw_, si->th_, 344, 44, 256, 160); else sample_picture(c, 344, 44, 256, 160);
            std::string l[4] = { "저장 시각  " + si->stamp, "상태 파일  " + std::to_string(si->bytes / 1048576) + "." + std::to_string((si->bytes / 104858) % 10) + "MB", gm->disk[0], gm->disk[1] };
            for (int i = 0; i < 4; i++) if (!l[i].empty()) text(c, 344, 214 + i * 20, l[i], WH, K);
        } else text(c, 344, 120, "비어 있는 슬롯입니다.", GY, K);
        desc(c, 328, 44, { in_game ? "고른 슬롯에 저장하거나 불러옵니다." : "선택한 슬롯에서 이어서 시작합니다." });
        helpline(c, in_game ? "↑↓:선택, ○:불러오기, □:이 슬롯에 저장, △:삭제, ×:취소" : "↑↓:선택, ○:불러오기, △:삭제, ×:취소");
    }
    void d_extra(Canvas & c) {
        titlebar(c, "부가");
        const UiGame * gm = game();
        rbox(c, 16, 24, 288, 244, WH, gm ? ttl_suf(*gm, "부가", 256).c_str() : "부가");
        std::vector<std::string> a = { "디스크 정보", "시스템 디스크만 부팅", "부팅 시퀀스 다시 보기", "조작 안내", "오프닝(데모) 보기", "정보·라이선스", std::string("즐겨찾기: ") + (games.empty() || !games[base_of(cur)].fav ? "☆ 끔" : "★ 켬"), "원본 디스크로 초기화", "적용값 보기 (값·출처 표)" }; std::vector<bool> dm = { false, false, false, false, !(gm && gm->has_demo), false, false, false, false }; items(c, 40, 44, 256, a, sel, 24, &dm);
        rbox(c, 320, 24, 304, 244, WH, "인포메이션");
        if (gm) { std::string l[4] = { gm->disk[0], gm->disk[1], "", gm->boot_note }; for (int i = 0; i < 4; i++) if (!l[i].empty()) text(c, 344, 44 + i * 22, l[i], i == 3 ? CY : WH, K); }
        desc(c, 276, 56, { "고른 작품의 디스크 구성과 부가 기능입니다." });
        helpline(c, "↑↓:선택, ○:결정, ×:취소");
    }
    void d_game(Canvas & c) {
        /* 배경은 호스트가 게임 화면을 깔아 둔다(draw 가 지우지 않도록 호스트가 호출 순서 관리) */
        c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, "일시정지");
        std::vector<std::string> a = { "이어하기", "저장  슬롯 " + std::to_string(slotsel + 1), "불러오기", "디스크 교체", "화면 설정", "소리/패드/배속 설정", "PC-98 키 보내기", std::string("화면 키보드  ") + (kb_vis ? "켜짐" : "꺼짐"), std::string("키보드 버튼  ") + (kb_btn ? "표시" : "숨김"), std::string("키보드·배속 셰이더  ") + (kb_sh ? "적용" : "안 함"), "스크린샷", "메인 메뉴로", "끝내기" };
        items(c, 160, 44, 330, a, sel, 22);

        c.rect(0, 376, 640, 24, K);
        helpline(c, "↑↓:선택, ○:결정, ×:닫기(이어하기)");
    }
    void open_vals() {   /* t48a (#614 ③): 적용값 표 열기 — 호스트가 층별 출처를 채우고, 작품별 설정(settings.ini [id])의 전역과 다른 값은 여기서 덧붙인다 */
        vals.clear(); vals_top = 0;
        if (cur >= 0 && cur < (int)games.size() && env.values) env.values(cur, vals);
        if (cur >= 0 && cur < MAXG && cur < (int)games.size() && has_t[cur]) {
            const Settings & t0 = t[cur]; const Settings & g0 = g; const std::string & id = games[cur].id;
#define VSR(f) if (t0.f != g0.f) { ValRow r; r.key = std::string("설정:" #f); r.val = std::to_string((int)t0.f); r.src = "settings.ini [" + id + "]"; r.cov = std::to_string((int)g0.f) + " (전역)"; vals.push_back(r); }
            VSR(ratio) VSR(shader) VSR(curve) VSR(bleed) VSR(glow) VSR(scan) VSR(mask) VSR(bgm) VSR(se) VSR(device) VSR(pad) VSR(bmult) VSR(bmode) VSR(boot) VSR(bosd) VSR(bsound) VSR(kbbtn) VSR(kbshader)
#undef VSR
        }
        sel = 0; scr = S_VALS;
    }
    void d_vals(Canvas & c) {   /* t48a (#614 ③): 키·최종값·출처·덮인 값 */
        titlebar(c, ""); const UiGame * gm = game();
        rbox(c, 8, 20, 624, 356, WH, gm ? ttl_suf(*gm, "적용값", 560).c_str() : "적용값");
        text(c, 20, 40, "키", CY, K); text(c, 150, 40, "최종값", CY, K); text(c, 350, 40, "출처", CY, K); text(c, 470, 40, "덮인 값", CY, K);
        int n = (int)vals.size(); const int VN = 13;
        if (sel >= n) sel = n > 0 ? n - 1 : 0; if (sel < 0) sel = 0;
        if (sel < vals_top) vals_top = sel; if (sel >= vals_top + VN) vals_top = sel - VN + 1;
        for (int i = 0; i < VN && vals_top + i < n; i++) {
            const ValRow & r = vals[vals_top + i]; int y = 60 + i * 21; bool s = vals_top + i == sel;
            if (s) c.rect(14, y - 2, 612, 21, WH);
            text(c, 20, y, fit(r.key, 126), s ? K : CY, s ? WH : K); text(c, 150, y, fit(r.val, 194), s ? K : WH, s ? WH : K);
            text(c, 350, y, fit(r.src, 114), s ? K : YL, s ? WH : K); text(c, 470, y, fit(r.cov, 150), s ? K : GY, s ? WH : K);
        }
        if (n == 0) text(c, 20, 60, "표시할 값이 없습니다.", GY, K);
        else { const ValRow & r = vals[sel]; text(c, 20, 340, fit("전체: " + r.val, 596), YL, K); if (!r.cov.empty()) text(c, 20, 357, fit("덮인 값: " + r.cov, 596), GY, K); }
        helpline(c, "↑↓:이동, L/R:한 쪽, ○ 또는 ×:돌아가기");
    }
    void d_help(Canvas & c) {
        titlebar(c, "조작 안내");
        rbox(c, 16, 24, 608, 344, WH, "조작 안내");
        static const char * L[] = { "패드    ○:결정(스페이스)  ×:취소(Esc)  □:빨리 넘김(CAPS)", "        △:이 메뉴  START:Enter  십자키/왼스틱:방향키", "        R2 누르는 동안:배속  L3:배속 켜고 끄기", "키보드  F12:이 메뉴  F11 누르는 동안:배속  ScrollLock:배속 토글", "        CAPS:빨리 넘김  그 밖의 키는 PC-98 키로 그대로 전달", "        F10 / 숫자 1-7 / KANA 는 메뉴의 「PC-98 키 보내기」로만 보냄", "화면    Alt+Enter:전체화면  Alt+1/2/3:비율  Alt+S:셰이더", "        Alt+위/아래:곡면 세기  (바꿀 때마다 화면 구석에 표시)", "저장    메뉴의 저장/불러오기, Alt+F5 / Alt+F9 는 슬롯 1" };
        for (int i = 0; i < 9; i++) text(c, 40, 48 + i * 28, L[i], i % 3 == 0 ? CY : WH, K);
        helpline(c, "○ 또는 ×:돌아가기");
    }
    void d_keys(Canvas & c) {
        c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, "PC-98 키 보내기");
        std::vector<std::string> a = { "F10", "1", "2", "3", "4", "5", "6", "7", "KANA" }; items(c, 160, 48, 330, a, sel, 28);
        c.rect(0, 376, 640, 24, K); helpline(c, "↑↓:선택, ○:보내기, ×:닫기");
    }

    /* ================================================================ t39: 새 화면 입력 */
    bool new_scr() const { switch (scr) { case S_SORT: case S_ENT: case S_ENTMENU: case S_TPL: case S_EDIT: case S_FILE: case S_KEYSEQ: case S_STEP: case S_SWAPED: case S_FONT: case S_OSK: case S_NAME: case S_YN: case S_HDDASK: case S_LANG: case S_VALS: case S_PAD: case S_PADKEY: return true; default: return false; } }
    static std::string parent_of(const std::string & p) { size_t d = p.find_last_of("\\/"); return d == std::string::npos || d == 0 ? std::string() : p.substr(0, d); }
    static std::vector<std::string> split_commas(const std::string & s) { std::vector<std::string> r; size_t p = 0; while (p <= s.size() && !s.empty()) { size_t e = s.find(',', p); if (e == std::string::npos) e = s.size(); if (e > p) r.push_back(s.substr(p, e - p)); p = e + 1; } return r; }
    static std::string join_commas(const std::vector<std::string> & v) { std::string o; for (auto & x : v) { if (!o.empty()) o += ","; o += x; } return o; }
    void yn_open(Scr ret, int act, const std::string & a0, const std::string & a1 = std::string(), const std::string & a2 = std::string(), int arg = 0) { yn_msg[0] = a0; yn_msg[1] = a1; yn_msg[2] = a2; yn_act = act; yn_ret = ret; ynsel = 1; yn_arg = arg; scr = S_YN; }
    std::string default_label(int g) const { return games[g].lang == "jp" ? std::string("원판 일본어") : std::string("한글판"); }
    void resolved_into(int g, ent::Kv & out) { out.clear(); if (env.resolved) env.resolved(g, out); }
    void begin_new_from_kv(int g, const ent::Kv & kv, const std::string & name) { eg = g; ed = kv; ed_name = ent::unique_name(name, ents_used(g)); en_orig = ed_name; ed0 = ent::Kv(); ed_name0 = ed_name; enew = true; ed_setdef = false; msg.clear(); sel = 0; scr = S_EDIT; }
    void begin_edit_ent(int gi, int vi) {
        std::vector<int> v = vents(gi); const UiEnt * e = (vi >= 0 && vi < (int)v.size()) ? &games[gi].ents[v[vi]] : NULL;
        if (!e || e->pseudo) { ent::Kv kv; resolved_into(gi, kv); begin_new_from_kv(gi, kv, e ? e->label : default_label(gi)); return; }
        if (e->vgi >= 0) {
            int tg = e->vgi; int di = def_ent(tg);
            if (di >= 0 && !games[tg].ents[di].pseudo) { const UiEnt & e2 = games[tg].ents[di]; eg = tg; ed = e2.kv; ed_name = e2.name; en_orig = e2.name; ed0 = ed; ed_name0 = ed_name; enew = false; ed_setdef = false; msg.clear(); sel = 0; scr = S_EDIT; return; }
            ent::Kv kv; resolved_into(tg, kv); begin_new_from_kv(tg, kv, e->label); return;
        }
        eg = gi; ed = e->kv; ed_name = e->name; en_orig = e->name; ed0 = ed; ed_name0 = ed_name; enew = false; ed_setdef = false; msg.clear(); sel = 0; scr = S_EDIT;
    }
    void begin_new_tpl(int t) {
        int g = entg; ent::Kv kv; std::string nm;
        if (t < 8) { kv = ent::tpl_kv(t); std::vector<std::string> fl; files_of(g, fl); ent::autofill(kv, fl); nm = kv["label"]; if (t == 6 && games[g].lang == "ko") nm = "원판 일본어"; }
        else { resolved_into(g, kv); nm = default_label(g) + " 복사"; }
        begin_new_from_kv(g, kv, nm);
    }
    bool save_ed(int * out_index = NULL) {
        if (ed_name.empty()) { msg = "이름이 비어 있습니다."; return false; }
        std::string nm = ed_name; if (enew || nm != en_orig) nm = ent::unique_name(nm, ents_used(eg, en_orig));
        ed["label"] = nm; ed_name = nm; std::string setdef = ed_setdef ? nm : std::string();
        if (setdef.empty() && games[eg].defent.empty()) { bool anyreal = false; for (auto & e : games[eg].ents) if (!e.pseudo && e.vgi < 0 && !e.hidden) anyreal = true; if (!anyreal) setdef = nm; }   /* 첫 항목은 자동으로 기본 */
        if (!enew && nm != en_orig && games[eg].defent == en_orig) setdef = nm;
        bool ok = env.save && env.save(eg, nm, ed, setdef);
        if (ok && !enew && nm != en_orig && env.del) env.del(eg, en_orig, false);
        if (ok) { if (env.reload) env.reload(eg); ed0 = ed; ed_name0 = nm; en_orig = nm; enew = false; msg = "저장했습니다: " + nm; dirty = true; } else msg = "저장하지 못했습니다.";
        (void)out_index; return ok;
    }
    void leave_edit_to_ent() { scr = S_ENT; std::vector<int> v = vents(entg); sel = 0; for (size_t i = 0; i < v.size(); i++) if (games[entg].ents[v[i]].name == en_orig && games[entg].ents[v[i]].vgi < 0) sel = (int)i; }
    void fl_goto(const std::string & p) {
        fl_dir = p; fl_items.clear(); std::string root = fl_ri < (int)fl_roots.size() ? fl_roots[fl_ri].second : std::string();
        if (!p.empty() && p != root) { FileItem u; u.name = ".."; u.dir = true; fl_items.push_back(u); }
        std::vector<FileItem> all; if (env.list_dir) all = env.list_dir(p); std::vector<FileItem> ds, fs;
        for (auto & f : all) { if (f.dir) ds.push_back(f); else if (ent::img_kind(f.name) == fl_filter()) fs.push_back(f); }
        std::sort(ds.begin(), ds.end(), [](const FileItem & a, const FileItem & b) { return ent::lower(a.name) < ent::lower(b.name); });
        std::sort(fs.begin(), fs.end(), [&](const FileItem & a, const FileItem & b) { if (fl_sort_kind) { std::string ea = ent::ext_of(a.name), eb = ent::ext_of(b.name); if (ea != eb) return ea < eb; } return ent::lower(a.name) < ent::lower(b.name); });
        for (auto & f : ds) fl_items.push_back(f); for (auto & f : fs) fl_items.push_back(f); sel = 0;
    }
    int fl_prev_sel = 0;
    void fl_open(const std::string & key, int swi = -1) {
        fl_key = key; fl_swapi = swi; fl_ret = key.compare(0, 4, "swap") == 0 ? S_SWAPED : S_EDIT; fl_prev_sel = sel; fl_roots.clear();
        fl_roots.push_back(std::make_pair(std::string("이 게임 폴더"), imgdir_of(eg))); std::string par = parent_of(games[eg].dir); if (!par.empty()) fl_roots.push_back(std::make_pair(std::string("작품 폴더들"), par));
        if (env.roots) for (auto & r : env.roots()) fl_roots.push_back(std::make_pair(r, r));
        fl_ri = 0; scr = S_FILE; fl_goto(fl_roots[0].second);
    }
    std::string fl_value(const std::string & name) const {
        std::string path = fl_dir + std::string(1, env.sep) + name, base = imgdir_of(eg) + std::string(1, env.sep);
        if (path.compare(0, base.size(), base) == 0) return path.substr(base.size()); return path;
    }
    void fl_apply(const std::string & v) {   /* v 가 빈칸이면 비움 */
        if (fl_key == "swap_add") { if (!v.empty()) sw.push_back(v); }
        else if (fl_key == "swap_set") { if (fl_swapi >= 0 && fl_swapi < (int)sw.size()) { if (v.empty()) sw.erase(sw.begin() + fl_swapi); else sw[fl_swapi] = v; } }
        else kv_set(fl_key.c_str(), v);
        scr = fl_ret; sel = fl_prev_sel; if (fl_ret == S_SWAPED && sel >= (int)sw.size() + 1) sel = (int)sw.size();
    }
    void open_font(int mode, Scr ret) {
        fnt_mode = mode; fnt_ret = ret; fnt_prev_sel = sel; fnt_opts.clear(); fnt_need.clear(); fnt_opts.push_back(std::string());
        if (mode == 2) {
            const UiGame & g = games[eg]; std::string fk = kv_get("font_kind"), lg = kv_get("lang"); std::string lang = !lg.empty() ? lg : fk == "jp" ? "jp" : fk == "kr" ? "ko" : g.lang;
            fnt_need = !g.fgroup.empty() ? g.fgroup : (lang == "jp" ? "jis" : "ks2350");
            if (fnt_need != "custom") for (auto & f : fonts) if (f.ok && f.group == fnt_need) fnt_opts.push_back(f.name);
        } else { std::string grp = mode == 0 ? "ks2350" : "jis"; fnt_need = grp; for (auto & f : fonts) if (f.ok && f.group == grp) fnt_opts.push_back(f.name); }
        sel = 0; std::string cv = fnt_cur(); for (size_t i = 0; i < fnt_opts.size(); i++) if (fnt_opts[i] == cv) sel = (int)i; scr = S_FONT;
    }
    int fnt_prev_sel = 0;
    void osk_open(int target, const std::string & init, Scr ret, bool hangul = false) { osk_target = target; osk_buf = init; osk_ret = ret; osk_r = 0; osk_c = 0; osk_upper = false; ime_comp.clear(); osk_hg = hangul && target == 0; osk_shift = false; hc = hg::Composer(); hc.buf = init; hc.han = true; scr = S_OSK; }   /* t48b: hangul = 이름 칸의 한글 키보드 */
    void text_in(const std::string & s) {   /* 호스트가 물리 키보드 글자를 넘긴다(S_OSK 일 때만) */
        if (scr != S_OSK) return;
        ime_comp.clear();
        if (osk_hg) { hk_text(s); return; }   /* t48b: 한글 키보드 켜짐 — 물리 키(영문)는 두벌식으로, IME 가 확정한 글자는 그대로 */
        for (char ch : s) { if (osk_target == 1) { char l = (char)tolower((unsigned char)ch); if (!(isalnum((unsigned char)l) || strchr(".-/\\:|~_'() ", l))) continue; osk_buf += l; } else if ((unsigned char)ch >= 0x20) osk_buf += ch; }
        if (osk_buf.size() > 60) { size_t cut = 60; while (cut > 0 && ((unsigned char)osk_buf[cut] & 0xC0) == 0x80) cut--; osk_buf.resize(cut); }   /* t48a: 한글 3바이트 중간에서 자르지 않는다 */
    }
    void ime_edit(const char * s) { if (scr == S_OSK) ime_comp = s ? s : ""; }   /* t48a: SDL_TEXTEDITING = 조합 중 글자 */
    void osk_back() { while (!osk_buf.empty()) { unsigned char c0 = (unsigned char)osk_buf.back(); osk_buf.pop_back(); if ((c0 & 0xC0) != 0x80) break; } }
    void osk_done() {
        if (osk_hg) { hc.buf = osk_buf; hc.flush(); osk_buf = hc.buf; }   /* t48b: 조합 중 글자 확정 */
        if (osk_target == 0) { std::string n = ent::clean_name(osk_buf); if (!n.empty()) ed_name = ent::unique_name(n, ents_used(eg, en_orig)); sel = 0; }
        else if (osk_target == 1 && stepi >= 0 && stepi < (int)ks.size()) { ks[stepi].val = osk_buf.empty() ? std::string("enter") : osk_buf; if (osk_buf.empty()) ks[stepi].type = false; sel = 2; }
        scr = osk_ret;
    }
    void begin_install(int g) {
        ent::Kv kv; resolved_into(g, kv); kv["install"] = "hdd_new"; if (!kv.count("hdd_mb")) kv["hdd_mb"] = "40"; kv.erase("disk_c");
        eg = g; entg = base_of(g); begin_new_from_kv(g, kv, "설치"); install_start = true; hddsel = 0; scr = S_HDDASK;
    }
    void ask_install_done(int g, const std::string & entry) { eg = g; en_orig = entry; yn_open(S_MAIN, YN_INSTDONE, "설치를 마쳤습니까?", "예 = 이 작품의 실행 방법을 「HDD 부팅」으로 바꿉니다.", "아니오 = 그대로 두고 설치기를 다시 실행할 수 있습니다."); }
    bool finish_install_start(Act & a) {
        std::string nm = ed_name; ed["label"] = nm; bool ok = env.save && env.save(eg, nm, ed, nm); if (ok && env.reload) env.reload(eg);
        install_start = false; if (!ok) { msg = "저장하지 못했습니다."; scr = S_MAIN; return false; }
        a.t = A_START; a.game = eg; a.entry = nm; a.resume = false; scr = S_MAIN; return true;
    }
    Act yn_do() {
        Act a; int act = yn_act;
        if (act == YN_DISCARD) { leave_edit_to_ent(); msg.clear(); }
        else if (act == YN_DELETE || act == YN_RESET) {
            std::vector<int> v = vents(entg); if (yn_arg >= 0 && yn_arg < (int)v.size()) { const UiEnt & e = games[entg].ents[v[yn_arg]]; if (env.del) env.del(entg, e.name, act == YN_RESET); if (env.reload) env.reload(entg); dirty = true; }
            scr = S_ENT; sel = 0; msg = act == YN_RESET ? "DB 기본값으로 되돌렸습니다." : "삭제(숨김)했습니다.";
        }
        else if (act == YN_TEST) { if (env.save_test) env.save_test(eg, ed_name, ed); a.t = A_TEST; a.game = eg; a.entry = ed_name; a.arg = test_secs(); scr = S_EDIT; }
        else if (act == YN_INSTDONE) {
            int g = eg; std::string nm = en_orig; ent::Kv kv; for (auto & e : games[g].ents) if (e.name == nm) kv = e.kv;
            kv.erase("install"); kv.erase("hdd_mb"); kv["boot"] = "C"; kv.erase("disk_a"); std::string n2 = ent::unique_name("HDD 부팅", ents_used(g)); kv["label"] = n2;
            if (env.save && env.save(g, n2, kv, n2)) { if (env.reload) env.reload(g); dirty = true; } scr = S_MAIN;
        }
        return a;
    }
    Act key2(UKey k) {
        Act a; if (k == UK_NONE) return a;
        auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
        switch (scr) {
        case S_SORT:
            mv(sel, 9);
            if (k == UK_OK) {
                if (sel < SM_N) { int keep = cur; sortm = sel; rebuild_vis(keep); dirty = true; scr = S_PICK; }
                else if (sel == 7) { int keep = cur; sortfav = !sortfav; rebuild_vis(keep); dirty = true; }
                else if (sel == 8 && sortm == SM_CUSTOM) { renumber_order(); reorder = true; scr = S_PICK; }
            } else if (k == UK_CANCEL) scr = S_PICK;
            break;
        case S_ENT: {
            std::vector<int> v = vents(entg); int rows = (int)std::max<size_t>(v.size(), 1) + 2; mv(sel, rows);   /* t44a (#494): 마지막 행 = 언어, 그 앞 = 새 실행 방법 */
            if (k == UK_OK || k == UK_SQ) {
                if (sel == rows - 1) { if (entg >= 0 && entg < (int)games.size()) open_lang({ games[entg].id }, { std::string() }, S_ENT); }
                else if (sel == rows - 2) { sel = 0; scr = S_TPL; }
                else if (k == UK_SQ) begin_edit_ent(entg, sel);
                else { if (!v.empty()) { if ((int)rent.size() <= entg) rent.resize(games.size(), -1); rent[entg] = v[sel]; } sync_cur(); scr = S_PICK; }
            }
            else if (k == UK_TRI) { if (sel < rows - 2 && !v.empty()) { menu_ent = sel; menusel = 0; scr = S_ENTMENU; } }
            else if (k == UK_CANCEL) { msg.clear(); scr = S_PICK; }
            break; }
        case S_ENTMENU: {
            mv(menusel, 6); std::vector<bool> dm = menu_dim(); std::vector<int> v = vents(entg); int ei = (menu_ent >= 0 && menu_ent < (int)v.size()) ? v[menu_ent] : -1;
            if (k == UK_CANCEL) { scr = S_ENT; break; }
            if (k != UK_OK || dm[menusel] || ei < 0) break; const UiEnt & e = games[entg].ents[ei];
            if (menusel == 0) { if (env.setkey) env.setkey(entg, "entry", e.name); if (env.reload) env.reload(entg); dirty = true; msg = "기본 실행 방법으로 지정했습니다."; scr = S_ENT; }
            else if (menusel == 1) { ent::Kv kv = e.kv; begin_new_from_kv(entg, kv, e.label + " 복사"); }
            else if (menusel == 2) yn_open(S_ENT, YN_DELETE, "「" + fit(e.label, 300) + "」 을(를) 목록에서 지웁니까?", e.tag == "내 것" ? "내가 만든 항목이라 완전히 지워집니다." : "원본(DB·game.ini)은 그대로이고 숨김 처리만 합니다.", std::string(), menu_ent);
            else if (menusel == 3) yn_open(S_ENT, YN_RESET, "「" + fit(e.label, 300) + "」 의 내 수정을 버리고", "DB·game.ini 의 기본값으로 되돌립니까?", std::string(), menu_ent);
            else {
                std::vector<std::string> names; for (size_t i = 0; i < games[entg].ents.size(); i++) if (!games[entg].ents[i].pseudo && games[entg].ents[i].vgi < 0) names.push_back(games[entg].ents[i].name);
                int a0 = -1; for (size_t i = 0; i < names.size(); i++) if (names[i] == e.name) a0 = (int)i;
                int b0 = menusel == 4 ? menu_ent - 1 : menu_ent + 1; std::vector<int> vn; for (int i : v) if (games[entg].ents[i].vgi < 0) vn.push_back(i);
                if (a0 >= 0 && b0 >= 0 && b0 < (int)v.size() && games[entg].ents[v[b0]].vgi < 0) {
                    std::string other = games[entg].ents[v[b0]].name; int o0 = -1; for (size_t i = 0; i < names.size(); i++) if (names[i] == other) o0 = (int)i;
                    if (o0 >= 0) { std::swap(names[a0], names[o0]); if (env.setkey) env.setkey(entg, "entry_order", join_commas(names)); if (env.reload) env.reload(entg); dirty = true; sel = b0; menu_ent = b0; }
                }
            }
            break; }
        case S_TPL:
            mv(sel, 9);
            if (k == UK_OK) begin_new_tpl(sel); else if (k == UK_CANCEL) { sel = 0; scr = S_ENT; }
            break;
        case S_EDIT: {
            mv(sel, 12); if (k == UK_UP || k == UK_DOWN) { msg.clear(); break; }
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = k == UK_RIGHT ? 1 : -1; msg.clear();
                if (sel == 6) { static const char * bs[] = { "", "A", "B", "C", "D" }; int cu = 0; std::string b = kv_get("boot"); for (int i = 0; i < 5; i++) if (b == bs[i]) cu = i; cu = (cu + d + 5) % 5; kv_set("boot", bs[cu]); }
                else if (sel == 10) { static const char * ms[] = { "", "none", "optional", "pointer", "required" }; int cu = 0; std::string m = kv_get("mouse"); for (int i = 0; i < 5; i++) if (m == ms[i]) cu = i; cu = (cu + d + 5) % 5; kv_set("mouse", ms[cu]); }
                else if (sel == 11) { if (kv_get("install") == "hdd_new") { ed.erase("install"); ed.erase("hdd_mb"); } else { ed["install"] = "hdd_new"; if (!ed.count("hdd_mb")) ed["hdd_mb"] = "40"; } }
                break;
            }
            if (k == UK_OK) {
                static const char * dk[] = { "", "disk_a", "disk_b", "disk_c", "disk_d", "cd" }; msg.clear();
                if (sel == 0) { sel = 0; scr = S_NAME; }
                else if (sel >= 1 && sel <= 5) fl_open(dk[sel]);
                else if (sel == 7) { ks = ent::parse_steps(kv_get("bootkeys")); sel = 0; scr = S_KEYSEQ; }
                else if (sel == 8) { sw = split_commas(kv_get("swap")); sel = 0; scr = S_SWAPED; }
                else if (sel == 9) open_font(2, S_EDIT);
                else if (sel == 11) { if (kv_get("install") == "hdd_new") { if (kv_get("disk_c").empty()) { hddsel = 0; install_start = false; scr = S_HDDASK; } } else { ed["install"] = "hdd_new"; if (!ed.count("hdd_mb")) ed["hdd_mb"] = "40"; } }
            }
            else if (k == UK_SQ) { if (save_ed()) leave_edit_to_ent(); }
            else if (k == UK_TRI) { std::vector<ent::Issue> is = ed_issues(); if (ent::has_error(is)) { msg = "오류가 있어 시험할 수 없습니다: " + is[0].msg; } else yn_open(S_EDIT, YN_TEST, "저장하지 않은 값으로 " + std::to_string(test_secs()) + "초 실행해 봅니다.", "세이브는 임시 폴더에 쓰고 끝나면 지웁니다.", "실행할까요?"); }
            else if (k == UK_SELECT) ed_setdef = !ed_setdef;
            else if (k == UK_CANCEL) { if (ed_dirty()) yn_open(S_EDIT, YN_DISCARD, "바꾼 내용을 저장하지 않고 나갑니까?"); else leave_edit_to_ent(); }
            break; }
        case S_FILE: {
            int cnt = (int)fl_items.size(); mv(sel, cnt);
            if (k == UK_L || k == UK_R) { int n = (int)fl_roots.size(); if (n > 0) { fl_ri = (fl_ri + (k == UK_R ? 1 : n - 1)) % n; fl_goto(fl_roots[fl_ri].second); } }
            else if (k == UK_OK && cnt > 0) {
                const FileItem f = fl_items[sel];
                if (f.dir) { if (f.name == "..") fl_goto(parent_of(fl_dir)); else fl_goto(fl_dir + std::string(1, env.sep) + f.name); }
                else fl_apply(fl_value(f.name));
            }
            else if (k == UK_SQ) fl_apply(std::string());
            else if (k == UK_TRI) { fl_sort_kind = !fl_sort_kind; fl_goto(fl_dir); }
            else if (k == UK_CANCEL) { scr = fl_ret; sel = fl_prev_sel; }
            break; }
        case S_KEYSEQ: {
            int rows = (int)ks.size() + 1; mv(sel, rows);
            if (k == UK_OK) { if (sel == (int)ks.size()) { ent::Step s; s.ms = ks.empty() ? 3000 : ks.back().ms + 3000; ks.push_back(s); } stepi = sel; sel = 0; scr = S_STEP; }
            else if (k == UK_SQ && sel < (int)ks.size()) { ks.erase(ks.begin() + sel); if (sel >= (int)ks.size() + 1) sel = (int)ks.size(); }
            else if (k == UK_TRI && sel > 0 && sel < (int)ks.size()) { std::swap(ks[sel], ks[sel - 1]); sel--; }
            else if (k == UK_SELECT && sel + 1 < (int)ks.size()) { std::swap(ks[sel], ks[sel + 1]); sel++; }
            else if (k == UK_CANCEL) { kv_set("bootkeys", ent::join_steps(ks)); sel = 7; scr = S_EDIT; }
            break; }
        case S_STEP: {
            mv(sel, 3); ent::Step & st = ks[stepi];
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = k == UK_RIGHT ? 1 : -1;
                if (sel == 0) { st.type = !st.type; st.val = st.type ? "dir~" : "enter"; }
                else if (sel == 1) { st.ms += d * 500; if (st.ms < 0) st.ms = 0; if (st.ms > 600000) st.ms = 600000; }
                else { const std::vector<std::string> & L = st.type ? type_presets() : key_names(); int cu = -1; for (size_t i = 0; i < L.size(); i++) if (L[i] == st.val) cu = (int)i; cu = (cu < 0 ? (d > 0 ? 0 : (int)L.size() - 1) : (cu + d + (int)L.size()) % (int)L.size()); st.val = L[cu]; }
            }
            else if (k == UK_OK && sel == 2 && st.type) osk_open(1, st.val, S_STEP);
            else if (k == UK_CANCEL) { sel = stepi; scr = S_KEYSEQ; }
            break; }
        case S_SWAPED: {
            int rows = (int)sw.size() + 1; mv(sel, rows);
            if (k == UK_OK) { if (sel == (int)sw.size()) fl_open("swap_add"); else fl_open("swap_set", sel); }
            else if (k == UK_SQ && sel < (int)sw.size()) { sw.erase(sw.begin() + sel); if (sel >= (int)sw.size() + 1) sel = (int)sw.size(); }
            else if (k == UK_TRI && sel > 0 && sel < (int)sw.size()) { std::swap(sw[sel], sw[sel - 1]); sel--; }
            else if (k == UK_SELECT && sel + 1 < (int)sw.size()) { std::swap(sw[sel], sw[sel + 1]); sel++; }
            else if (k == UK_CANCEL) { kv_set("swap", join_commas(sw)); sel = 8; scr = S_EDIT; }
            break; }
        case S_VALS: {   /* t48a (#614 ③) */
            int n = (int)vals.size();
            if (k == UK_UP && sel > 0) sel--; else if (k == UK_DOWN && sel + 1 < n) sel++;
            else if (k == UK_L) sel = sel > 13 ? sel - 13 : 0; else if (k == UK_R) sel = sel + 13 < n ? sel + 13 : (n > 0 ? n - 1 : 0);
            else if (k == UK_OK || k == UK_CANCEL) go(S_EXTRA, 8);
            break; }
        case S_FONT:
            mv(sel, (int)fnt_opts.size());
            if (k == UK_OK && sel < (int)fnt_opts.size()) {
                const std::string o = fnt_opts[sel];
                if (fnt_mode == 0) font_kr = (o == "pc98player_font") ? std::string() : o; else if (fnt_mode == 1) font_jp = (o == "jp_default") ? std::string() : o;
                else { if (o.empty()) { ed.erase("font"); ed.erase("font_kind"); } else { ed["font"] = o; std::string g; for (auto & f : fonts) if (f.name == o) g = f.group; ed["font_kind"] = g == "jis" ? "jp" : "kr"; } }
                dirty = true; scr = fnt_ret; sel = fnt_prev_sel; a.t = A_APPLY;
            } else if (k == UK_CANCEL) { scr = fnt_ret; sel = fnt_prev_sel; }
            break;
        case S_PAD: {   /* t48e (#646): 조작 설정 */
            mv(sel, 9);
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = (k == UK_RIGHT) ? 1 : -1;
                if (sel == 0) { scope = clampi(scope + d, 0, 1); if (scope == 1 && !has_t[cur]) { t[cur] = effective(cur); has_t[cur] = true; } }
                else if (sel == 1) { Settings & s = edit(); int nv = clampi(s.pad + d, 0, 1); if (nv != s.pad) { s.pad = nv; dirty = true; a.t = A_APPLY; } }
            } else if (k == UK_OK) {
                if (sel >= 2) open_padkey(sel - 2);
                else if (sel == 1) { Settings & s = edit(); s.pad = s.pad == 1 ? 0 : 1; dirty = true; a.t = A_APPLY; }
            } else if (k == UK_TRI) {   /* 출하 시 설정: 이 범위의 버튼 줄을 모두 지움, 결정 방식은 일본식 */
                if (scope == 0) { gpad.clear(); eff().pad = 0; }
                else if (cur >= 0 && cur < (int)games.size()) { if (env.pad_get && env.pad_set) { std::map<std::string, std::string> m; env.pad_get(cur, m); for (auto & kv : m) env.pad_set(cur, kv.first, std::string()); } edit().pad = g.pad; }
                dirty = true; a.t = A_APPLY;
            } else if (k == UK_CANCEL) { go(S_CONF, 4); }
            break; }
        case S_PADKEY:
            mv(sel, (int)pk_opts.size());
            if (k == UK_OK && sel < (int)pk_opts.size()) {
                const std::string o = pk_opts[sel]; std::string key = pad_key(pk_row);
                if (scope == 1) { if (env.pad_set && cur >= 0 && cur < (int)games.size()) env.pad_set(cur, key, o); }
                else { if (o.empty()) gpad.erase(key); else gpad[key] = o; }
                dirty = true; scr = S_PAD; sel = pad_sel_keep; a.t = A_APPLY;
            } else if (k == UK_CANCEL) { scr = S_PAD; sel = pad_sel_keep; }
            break;
        case S_OSK: {
            if (osk_hg) { hk_key(k); break; }   /* t48b */
            static const char * rowsl[5] = { "abcdefghij", "klmnopqrst", "uvwxyz0123", "456789.-/\\", ":|~_'() " };
            auto cellok = [&](int r, int q) { return q < (int)strlen(rowsl[r]); };
            if (k == UK_UP) osk_r = (osk_r + 4) % 5; else if (k == UK_DOWN) osk_r = (osk_r + 1) % 5; else if (k == UK_LEFT) osk_c = (osk_c + 9) % 10; else if (k == UK_RIGHT) osk_c = (osk_c + 1) % 10;
            else if (k == UK_OK && cellok(osk_r, osk_c)) { char ch = rowsl[osk_r][osk_c]; if (osk_target == 1 || !osk_upper) { if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32); } else if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32); if (osk_buf.size() < 48) osk_buf += ch; }
            else if (k == UK_SQ) osk_back(); else if (k == UK_TRI && osk_target == 0) osk_upper = !osk_upper;
            else if (k == UK_SELECT) osk_done(); else if (k == UK_CANCEL) { scr = osk_ret; if (osk_ret == S_EDIT) sel = 0; }
            break; }
        case S_NAME: {
            int cnt = (int)name_presets().size() + 2; mv(sel, cnt);   /* t48b: 끝에 「한글 키보드」 「영문 글자표」 두 줄 */
            if (k == UK_OK) { if (sel < cnt - 2) { ed_name = ent::unique_name(name_presets()[sel], ents_used(eg, en_orig)); sel = 0; scr = S_EDIT; } else osk_open(0, ed_name, S_EDIT, sel == cnt - 2); }
            else if (k == UK_CANCEL) { sel = 0; scr = S_EDIT; }
            break; }
        case S_YN:
            if (k == UK_LEFT) ynsel = 0; else if (k == UK_RIGHT) ynsel = 1;
            else if (k == UK_OK) { if (ynsel == 0) a = yn_do(); else scr = yn_ret; }
            else if (k == UK_CANCEL) scr = yn_ret;
            break;
        case S_LANG: {   /* t44a (#494): 자동 → 한글판 → 원판 일본어 → 자동 */
            int n = std::max<int>((int)lang_gid.size(), 1); mv(sel, n);
            if (k == UK_OK || k == UK_RIGHT || k == UK_LEFT) {
                int gi = (sel < (int)lang_gid.size()) ? gidx_of(lang_gid[sel]) : -1;
                if (gi >= 0) { const std::string & cur = games[gi].lang_exp; int v0 = cur == "ko" ? 1 : cur == "jp" ? 2 : 0; int d = (k == UK_LEFT) ? 2 : 1; a.t = A_SETLANG; a.game = gi; a.arg = (v0 + d) % 3; dirty = true; }
            } else if (k == UK_CANCEL) { scr = lang_ret; sel = (lang_ret == S_ENT) ? (int)std::max<size_t>(vents(entg).size(), 1) + 1 : 5; }
            break; }
        case S_HDDASK:
            mv(hddsel, 3);
            if (k == UK_OK) {
                if (hddsel == 0) {
                    std::string nm; int mb = atoi(kv_get("hdd_mb").empty() ? "40" : kv_get("hdd_mb").c_str());
                    if (env.hdd_make && env.hdd_make(eg, mb, nm)) { ed["disk_c"] = nm; ed["install"] = "hdd_new"; msg = "HDD 를 만들었습니다: " + nm; if (install_start) finish_install_start(a); else scr = S_EDIT; }
                    else { msg = "HDD 를 만들지 못했습니다."; if (install_start) { install_start = false; scr = S_MAIN; } else scr = S_EDIT; }
                } else if (hddsel == 1) { if (install_start) { install_start = false; scr = S_MAIN; } else fl_open("disk_c"); }
                else { if (install_start) { install_start = false; scr = S_MAIN; } else scr = S_EDIT; }
            } else if (k == UK_CANCEL) { if (install_start) { install_start = false; scr = S_MAIN; } else scr = S_EDIT; }
            break;
        default: break;
        }
        return a;
    }
    /* ---- 입력 */
    int nitems() const {
        switch (scr) { case S_MAIN: return 7; case S_START: return 2; case S_SWAP: return 2; case S_SWAPI: return (game() ? (int)game()->fds.size() : 1); case S_ABOUT: return 1; case S_PICK: return (int)vis.size() + 1; case S_ADD: return 6; case S_SCREEN: return 9; case S_CONF: return 12; case S_SAVE: return 8; case S_EXTRA: return 9; case S_GAME: return 13; case S_KEYS: return 9; case S_HELP: return 1; default: return 1; }
    }
    int & curSel() { if (scr == S_MAIN) return mainsel; if (scr == S_PICK) return pick; if (scr == S_SAVE) return slotsel; if (scr == S_START) return startsel; if (scr == S_SWAP) return swapdrv; if (scr == S_SWAPI) return swapsel; return sel; }
    void go(Scr s, int selv = 0) { scr = s; sel = selv; }
    void enter_screen_cfg() { backup = eff(); backup_has = true; bk_wmult = wmult; bk_winw = winw; bk_winh = winh; }
    static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

    Act key(UKey k) {
        Act a; if (k == UK_NONE) return a;
        if (new_scr()) return key2(k);
        if (scr == S_QUIT) {
            if (k == UK_LEFT && quitsel > 0) quitsel--; if (k == UK_RIGHT && quitsel < 2) quitsel++;
            if (k == UK_CANCEL) { if (in_game) go(S_GAME, 12); else scr = S_MAIN; }
            if (k == UK_OK) { if (quitsel == 2) { if (in_game) go(S_GAME, 12); else scr = S_MAIN; } else { a.t = A_QUIT; a.arg = quitsel == 0 ? 1 : 0; } }
            return a;
        }
        int n = nitems(), & cs = curSel(); if (n < 1) n = 1;
        if (k == UK_UP || k == UK_DOWN) {
            if (scr == S_PICK && reorder) { int p2 = pick + (k == UK_UP ? -1 : 1); if (pick < (int)vis.size() && p2 >= 0 && p2 < (int)vis.size()) { std::swap(vis[pick], vis[p2]); pick = p2; renumber_order(); dirty = true; } return a; }
            cs = (k == UK_UP) ? (cs + n - 1) % n : (cs + 1) % n; if (scr == S_PICK && pick < (int)vis.size()) sync_cur(); return a;
        }
        switch (scr) {
        case S_MAIN:
            if (k == UK_OK) {
                static const Scr to[7] = { S_START, S_PICK, S_SCREEN, S_CONF, S_SAVE, S_EXTRA, S_QUIT };
                back = S_MAIN; if (to[mainsel] == S_SCREEN) enter_screen_cfg(); if (to[mainsel] == S_PICK) { pick = pos_of(cur); reorder = false; } if (to[mainsel] == S_QUIT) quitsel = 0;
                if (to[mainsel] == S_START) { startsel = has_save(game()) ? 0 : 1; }
                go(to[mainsel], 0); if (to[mainsel] == S_SAVE) slotsel = 0;
            } else if (k == UK_CANCEL) { go(S_QUIT, 0); quitsel = 0; }
            else if (k == UK_SELECT) go(S_EXTRA, 0);
            break;
        case S_PICK: {
            int nv = (int)vis.size();
            if (reorder) { if (k == UK_OK || k == UK_CANCEL) { reorder = false; dirty = true; a.t = A_APPLY; } break; }
            if (k == UK_L || k == UK_R) { int keep = cur; tab = (tab + (k == UK_R ? 1 : 2)) % 3; rebuild_vis(keep); dirty = true; break; }
            if (pick >= nv) { if (k == UK_OK) go(S_ADD, 0); else if (k == UK_CANCEL) go(S_MAIN, 0); break; }
            int gi = vis[pick];
            if (k == UK_LEFT || k == UK_RIGHT) { std::vector<int> v = tvents(gi); if (v.size() > 1) { int ei = row_ent(gi), idx = 0; for (size_t q = 0; q < v.size(); q++) if (v[q] == ei) idx = (int)q; idx = (idx + (k == UK_RIGHT ? 1 : (int)v.size() - 1)) % (int)v.size(); if ((int)rent.size() <= gi) rent.resize(games.size(), -1); rent[gi] = v[idx]; sync_cur(); } break; }
            if (k == UK_OK) { sync_cur(); mainsel = 0; go(S_MAIN, 0); }
            else if (k == UK_CANCEL) go(S_MAIN, 0);
            else if (k == UK_SELECT) { sync_cur(); go(S_EXTRA, 0); back = S_PICK; }
            else if (k == UK_SQ) { entg = gi; msg.clear(); std::vector<int> v = vents(gi); int ei = row_ent(gi), idx = 0; for (size_t q = 0; q < v.size(); q++) if (v[q] == ei) idx = (int)q; scr = S_ENT; sel = idx; }
            else if (k == UK_TRI) { sel = sortm; scr = S_SORT; }
            break; }
        case S_ADD:
            if (k == UK_OK) {
                if (sel == 5) { if (!add_new.empty()) open_lang(add_new, add_new_why, S_ADD); break; }   /* t44a (#494) */
                if (sel >= 3 && gamefolder.empty()) break;
                a.t = A_ADD; a.arg = sel;
            } else if (k == UK_CANCEL) { go(S_PICK, 0); pick = (int)vis.size(); }
            break;
        case S_SCREEN: case S_CONF: {
            if (scr == S_CONF && sel >= 10 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT || k == UK_TRI)) { if (k == UK_OK) open_font(sel - 10, S_CONF); break; }
            if (scr == S_CONF && sel == 4 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT)) { if (k == UK_OK) go(S_PAD, 0); break; }   /* t48e (#646) */
            bool scr_s = scr == S_SCREEN; int nrow = scr_s ? 9 : 12;
            Settings & s = (sel == 0) ? eff() : edit();
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = (k == UK_RIGHT) ? 1 : -1;
                if (scr_s && sel == 8) {   /* 창 배율(전역): 모니터에 들어가는 다음/이전 배율 */
                    int m = wm_next(d); if (m > 0) { wmult = m; a.t = A_WINMULT; a.arg = m; dirty = true; }
                    return a;
                }
                if (sel == 0) { scope = clampi(scope + d, 0, 1); if (scope == 1 && !has_t[cur]) { t[cur] = effective(cur); has_t[cur] = true; } }
                else if (scr_s) {
                    if (sel == 1) s.ratio = clampi(s.ratio + d, 0, 2);
                    if (sel == 2) s.shader = d > 0 ? 2 : 0;   /* t48c: 필터 = sharp / nearest */
                    { if (sel == 3) s.scan = clampi(s.scan + d, 0, 10); if (sel == 4) s.mask = clampi(s.mask + d, 0, 10); if (sel == 5) s.bleed = clampi(s.bleed + d, 0, 10); if (sel == 6) s.glow = clampi(s.glow + d, 0, 10); if (sel == 7) s.curve = clampi(s.curve + d, 0, 10); }   /* t48c: 필터와 상관없이 효과를 켠다. 바꾸면 A_APPLY 로 곧바로 화면에 반영 */
                    a.t = A_APPLY;
                } else {
                    if (sel == 1) s.bgm = clampi(s.bgm + d, 0, 8);
                    if (sel == 2) s.se = clampi(s.se + d, 0, 8);
                    if (sel == 5) s.bmult = clampi(s.bmult + d, 0, 4);
                    if (sel == 6) s.bmode = clampi(s.bmode + d, 0, 2);
                    if (sel == 7) s.bsound = clampi(s.bsound + d, 0, 2);
                    if (sel == 8) s.boot = clampi(s.boot + d, 0, 1); if (sel == 9) s.bosd = clampi(s.bosd + d, 0, 1);
                    a.t = A_APPLY;
                }
                dirty = true; (void)nrow;
            } else if (k == UK_TRI) { Settings def; Settings & e = eff(); if (scr_s) { e.ratio = def.ratio; e.shader = def.shader; e.curve = def.curve; e.bleed = def.bleed; e.glow = def.glow; e.scan = def.scan; e.mask = def.mask; } else { e.bgm = def.bgm; e.se = def.se; e.bmult = def.bmult; e.bmode = def.bmode; e.boot = def.boot; e.bosd = def.bosd; e.bsound = def.bsound; } dirty = true; a.t = A_APPLY; }
            else if (k == UK_CANCEL) { if (scr_s && backup_has) { eff() = backup; backup_has = false; a.t = A_APPLY; if (wmult != bk_wmult || winw != bk_winw || winh != bk_winh) { wmult = bk_wmult; winw = bk_winw; winh = bk_winh; a.t = A_WINMULT; a.arg = wmult; } } go(in_game ? S_GAME : S_MAIN, in_game ? (scr == S_SCREEN ? 4 : 5) : 0); if (!in_game) sel = 0; }
            else if (k == UK_OK) { backup_has = false; dirty = true; a.t = A_APPLY; go(in_game ? S_GAME : S_MAIN, in_game ? (scr == S_SCREEN ? 4 : 5) : 0); }
            break; }
        case S_SAVE:
            if (k == UK_OK) { if (!in_game && !blk_cur().empty()) { startsel = 1; go(S_START, 1); break; } a.t = A_LOAD; a.game = cur; a.slot = slotsel; }   /* t48b2 (#632): 메뉴에서 슬롯 불러오기도 block 이면 같은 「게임 시작」 팝업(사유 표시)으로 막는다. 게임 안(in_game)은 이미 돌고 있어 해당 없음 */
            else if (k == UK_SQ && in_game) { a.t = A_SAVE; a.game = cur; a.slot = slotsel; }
            else if (k == UK_TRI) { a.t = A_DELSLOT; a.game = cur; a.slot = slotsel; }
            else if (k == UK_CANCEL) { go(in_game ? S_GAME : S_MAIN, 0); if (in_game) sel = 2; }
            break;
        case S_START:
            if (k == UK_OK) { if (startsel == 0 && !has_save(game())) break; if (game() && (game()->id.empty() || !blk_cur().empty())) break; if (startsel == 1 && game() && game()->need_install) { begin_install(cur); break; } a.t = A_START; a.game = cur; a.entry = sel_entry; a.resume = (startsel == 0); }
            else if (k == UK_CANCEL) go(S_MAIN, 0);
            break;
        case S_SWAP:
            if (k == UK_OK) { if (game() && !game()->fds.empty()) { swapsel = 0; const UiGame * g = game(); for (size_t i = 0; i < g->fds.size(); i++) if (g->fds[i] == g->cur_fd[swapdrv]) swapsel = (int)i; go(S_SWAPI, 0); } }
            else if (k == UK_CANCEL) go(S_GAME, 3);
            break;
        case S_SWAPI:
            if (k == UK_OK) { if (game() && swapsel < (int)game()->fds.size()) { a.t = A_DISKSWAP; a.game = cur; a.slot = swapdrv; a.arg = swapsel; } }
            else if (k == UK_CANCEL) go(S_SWAP, 0);
            break;
        case S_ABOUT:
            if (k == UK_OK || k == UK_CANCEL) go(S_EXTRA, 5);
            break;
        case S_EXTRA:
            if (k == UK_OK && sel == 3) go(S_HELP, 0);
            else if (k == UK_OK && sel == 5) go(S_ABOUT, 0);
            else if (k == UK_OK && sel == 8) open_vals();   /* t48a (#614 ③) */
            else if (k == UK_OK && sel == 6) { int keep = cur, b = base_of(cur); if (b >= 0 && b < (int)games.size()) { games[b].fav = !games[b].fav; dirty = true; rebuild_vis(keep); } }
            else if (k == UK_OK && sel == 4 && !(game() && game()->has_demo)) break;
            else if (k == UK_OK) { a.t = A_EXTRA; a.arg = sel; a.game = cur; }
            else if (k == UK_CANCEL) go(back == S_PICK ? S_PICK : S_MAIN, 0);
            break;
        case S_HELP:
            if (k == UK_OK || k == UK_CANCEL) go(S_EXTRA, 3);
            break;
        case S_GAME:
            if (k == UK_LEFT && sel == 1) slotsel = (slotsel + 7) % 8; if (k == UK_RIGHT && sel == 1) slotsel = (slotsel + 1) % 8;
            if (k == UK_CANCEL) a.t = A_RESUME;
            else if (k == UK_OK) {
                switch (sel) {
                case 0: a.t = A_RESUME; break;
                case 1: a.t = A_SAVE; a.game = cur; a.slot = slotsel; break;
                case 2: go(S_SAVE, 0); break;
                case 3: swapdrv = 0; go(S_SWAP, 0); break;
                case 4: enter_screen_cfg(); go(S_SCREEN, 0); break;
                case 5: go(S_CONF, 0); break;
                case 6: go(S_KEYS, 0); break;
                case 7: a.t = A_KBD; break;
                case 8: a.t = A_KBBTN; break;
                case 9: a.t = A_KBSH; break;
                case 10: a.t = A_SHOT; break;
                case 11: a.t = A_MAINMENU; break;
                case 12: go(S_QUIT, 0); quitsel = 0; break;

                }
            }
            break;
        case S_KEYS:
            if (k == UK_OK) { a.t = A_SENDKEY; a.arg = sel; } else if (k == UK_CANCEL) go(S_GAME, 6);
            break;
        default: break;
        }
        return a;
    }
};

/* ---- 설정 파일 (settings.ini) -- 전역 [global] + 작품별 [id] */
static void ini_put(FILE * f, const Settings & s) {
    fprintf(f, "fx=1\nratio=%d\nshader=%d\ncurve=%d\nbleed=%d\nglow=%d\nscan=%d\nmask=%d\nbgm=%d\nse=%d\ndevice=%d\npad=%d\nbmult=%d\nbmode=%d\nboot=%d\nbosd=%d\nbsound=%d\nkbbtn=%d\nkbshader=%d\n", s.ratio, (s.shader == 2 ? 2 : 0), s.curve, s.bleed, s.glow, s.scan, s.mask, s.bgm, s.se, s.device, s.pad, s.bmult, s.bmode, s.boot, s.bosd, s.bsound, s.kbbtn, s.kbshader);
}
static bool ini_set(Settings & s, const std::string & k, int v) {
    if (k == "fx") { s.lmode = false; }   /* t48c: 새 형식 표지. 이 구역의 화면 키는 새 범위로 읽는다 */
    else if (s.lmode && (k == "shader" || k == "curve" || k == "bleed" || k == "scan" || k == "mask")) { s.lseen = true; if (k == "shader") s.l_sh = v; else if (k == "curve") s.l_curve = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "bleed") s.l_bleed = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "scan") s.l_scan = v < 0 ? 0 : v > 10 ? 10 : v; else s.l_mask = v < 0 ? 0 : v > 3 ? 3 : v; }   /* 옛 형식: 구역이 끝날 때 fx_finish 가 옮긴다 */
    else if (k == "ratio") s.ratio = v; else if (k == "shader") s.shader = (v == 2) ? 2 : 0; else if (k == "curve") s.curve = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "bleed") s.bleed = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "glow") s.glow = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "scan") s.scan = v < 0 ? 0 : v > 10 ? 10 : v; else if (k == "mask") s.mask = v < 0 ? 0 : v > 10 ? 10 : v;
    else if (k == "bgm") s.bgm = v; else if (k == "se") s.se = v; else if (k == "device") s.device = v; else if (k == "pad") s.pad = v; else if (k == "bmult") s.bmult = v; else if (k == "bmode") s.bmode = v; else if (k == "boot") s.boot = v; else if (k == "bosd") s.bosd = v; else if (k == "bsound") s.bsound = v; else if (k == "kbbtn") s.kbbtn = v < 0 ? 0 : v > 1 ? 1 : v; else if (k == "kbshader") s.kbshader = v < 0 ? 0 : v > 1 ? 1 : v; else return false;
    return true;
}
static void settings_save(const Ui & u, const std::string & path) {
    FILE * f = fopen(path.c_str(), "wb"); if (!f) return;
    fprintf(f, "; PC-98 PLAYER 설정 -- [global] 전역, [작품id] 작품별 덮어쓰기(작품별 > 전역)\n[global]\n"); ini_put(f, u.g);
    fprintf(f, "lastgame=%d\nwmult=%d\nwinw=%d\nwinh=%d\ngamefolder=%s\n", u.cur, u.wmult, u.winw, u.winh, u.gamefolder.c_str());
    fprintf(f, "lastgame_id=%s\nlastentry=%s\nsort=%s\nsortfav=%d\ntab=%s\nfont_kr=%s\nfont_jp=%s\n", (u.cur >= 0 && u.cur < (int)u.games.size()) ? u.games[u.cur].id.c_str() : "", u.sel_entry.c_str(), Ui::sort_id(u.sortm), u.sortfav ? 1 : 0, Ui::tab_id(u.tab), u.font_kr.c_str(), u.font_jp.c_str());
    for (auto & kv : u.gpad) if (!kv.second.empty()) fprintf(f, "%s=%s\n", kv.first.c_str(), kv.second.c_str());   /* t48e (#646): 전역 패드 배정 btn_* (표준 버튼 이름) */
    fprintf(f, "[last]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].last_run > 0) fprintf(f, "%s=%lld\n", u.games[i].id.c_str(), u.games[i].last_run);
    fprintf(f, "[fav]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].fav) fprintf(f, "%s=1\n", u.games[i].id.c_str());
    fprintf(f, "[order]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].order_n > 0) fprintf(f, "%s=%d\n", u.games[i].id.c_str(), u.games[i].order_n);
    fprintf(f, "[play]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].play_s > 0) fprintf(f, "%s=%d\n", u.games[i].id.c_str(), u.games[i].play_s);
    for (size_t i = 0; i < u.games.size() && i < (size_t)Ui::MAXG; i++) if (u.has_t[i]) { fprintf(f, "[%s]\n", u.games[i].id.c_str()); ini_put(f, u.t[i]); }
    fclose(f);
}
static void settings_load(Ui & u, const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) { u.fit_tab_to_cur(); u.rebuild_vis(u.cur); return; }   /* t39ui2: 설정 파일이 없어도 목록을 만든다 */
    char ln[2048]; Settings * cur = NULL; bool in_play = false; int in_sec = 0; std::string last_id; bool has_id = false;   /* in_sec: 1 last · 2 fav · 3 order */
    while (fgets(ln, sizeof ln, f)) {
        std::string s = ln; while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (s.empty() || s[0] == ';' || s[0] == '#') continue;
        if (s[0] == '[') {
            std::string id = s.substr(1, s.size() - 2); if (cur) fx_finish(*cur); cur = NULL; in_play = (id == "play"); in_sec = id == "last" ? 1 : id == "fav" ? 2 : id == "order" ? 3 : 0; if (in_sec) continue;
            if (id == "global") cur = &u.g; else for (size_t i = 0; i < u.games.size() && i < (size_t)Ui::MAXG; i++) if (u.games[i].id == id) { u.has_t[i] = true; u.t[i] = u.g; cur = &u.t[i]; }
            if (cur) fx_begin(*cur);   /* t48c: 구역마다 「옛 형식으로 가정」 → fx=1 줄을 만나면 새 형식 */
            continue;
        }
        if (in_sec) { size_t e2 = s.find('='); if (e2 != std::string::npos) { std::string id = s.substr(0, e2); long long v = atoll(s.c_str() + e2 + 1); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].id == id) { if (in_sec == 1) u.games[i].last_run = v; else if (in_sec == 2) u.games[i].fav = v != 0; else u.games[i].order_n = (int)v; } } continue; }
        if (in_play) { size_t e2 = s.find('='); if (e2 != std::string::npos) { std::string id = s.substr(0, e2); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].id == id) u.games[i].play_s = atoi(s.c_str() + e2 + 1); } continue; }
        if (cur == &u.g && s.compare(0, 6, "wmult=") == 0) { u.wmult = atoi(s.c_str() + 6); continue; }
        if (cur == &u.g && s.compare(0, 5, "winw=") == 0) { u.winw = atoi(s.c_str() + 5); continue; }
        if (cur == &u.g && s.compare(0, 5, "winh=") == 0) { u.winh = atoi(s.c_str() + 5); continue; }
        if (cur == &u.g && s.compare(0, 11, "gamefolder=") == 0) { u.gamefolder = s.substr(11); continue; }
        if (cur == &u.g && s.compare(0, 12, "lastgame_id=") == 0) { last_id = s.substr(12); has_id = true; for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].id == last_id) { u.cur = (int)i; u.pick = (int)i; } continue; }
        if (cur == &u.g && s.compare(0, 10, "lastentry=") == 0) { u.sel_entry = s.substr(10); continue; }
        if (cur == &u.g && s.compare(0, 5, "sort=") == 0) { u.sortm = Ui::sort_from(s.substr(5)); continue; }
        if (cur == &u.g && s.compare(0, 8, "sortfav=") == 0) { u.sortfav = atoi(s.c_str() + 8) != 0; continue; }
        if (cur == &u.g && s.compare(0, 4, "tab=") == 0) { u.tab = Ui::tab_from(s.substr(4)); continue; }
        if (cur == &u.g && s.compare(0, 8, "font_kr=") == 0) { u.font_kr = s.substr(8); continue; }
        if (cur == &u.g && s.compare(0, 8, "font_jp=") == 0) { u.font_jp = s.substr(8); continue; }
        if (cur == &u.g && s.compare(0, 4, "btn_") == 0) { size_t e3 = s.find('='); if (e3 != std::string::npos && e3 > 4) { std::string kk = s.substr(0, e3); for (auto & ch : kk) ch = (char)tolower((unsigned char)ch); u.gpad[kk] = s.substr(e3 + 1); } continue; }   /* t48e (#646) */
        if (s.compare(0, 9, "lastgame=") == 0) { if (has_id) continue; int v = atoi(s.c_str() + 9); if (v >= 0 && v < (int)u.games.size()) { u.cur = v; u.pick = v; } continue; }
        size_t e = s.find('='); if (e == std::string::npos || !cur) continue;
        ini_set(*cur, s.substr(0, e), atoi(s.c_str() + e + 1));
    }
    fclose(f);
    u.fit_tab_to_cur(); u.rebuild_vis(u.cur);   /* t39: 탭·정렬을 적용한 목록 만들기 */   if (cur) fx_finish(*cur);   /* t48c: 마지막 구역의 옛 형식 변환 */
}

} /* namespace ui */
