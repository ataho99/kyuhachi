/* kh_ui.h -- r012 UI 화면 상태기계. kh_draw.h 위에서 그린다. 입력은 추상 키(UK_*)로 받는다.
 * 호스트가 UiGame 표(제목·디스크명·THMB·슬롯 정보)를 채워 넣고, ui_key() 의 반환 Act 를 처리한다. */
#pragma once
#ifndef KH_PRODUCT   /* t49a (#810/#811): 제품명·부제 상수 — 화면/창/문서 표기는 여기서만 */
#define KH_PRODUCT "Kyuhachi"
#define KH_SUBTITLE "PC-98 BOOTLOADER"
#include "kh_version.h"   /* t52a (#53): 버전 단일 소스 */
#define KH_FULLNAME KH_PRODUCT " v" KH_VERSION " - " KH_SUBTITLE
#endif
#include "ui/kh_draw.h"
#include <stdlib.h>
#include <algorithm>
#include <map>
#include <time.h>
#include <string.h>
#include <functional>
#include "settings/kh_ent.h"
#include "settings/kh_settings.h"   /* S5: Settings struct moved out */
#include "input/kh_ukey.h"   /* S6 */
#include "text/kh_fontreg.h"
#include "text/kh_hangul_t48.h"   /* t48b: 한글 조합기 */

#include "ui/kh_ui_types.h"   /* S7: ui data types */
#include "settings/kh_share.h"   /* build 5b-2: share code */
namespace ui {
using namespace ov;

struct Ui {
    Scr scr = S_MAIN, back = S_MAIN;
    int sel = 0;               /* 현재 화면 선택 줄 */
    int mainsel = 0, pick = 2, cur = 2;  /* cur = 현재 작품 index */
    int slotsel = 0;
    int quitsel = 0, startsel = 0, swapdrv = 0, swapsel = 0;
    static const int MAXG = 256;
    Settings g, t[MAXG]; bool has_t[MAXG] = { false };   bool nofollow[MAXG] = { false }; bool all_global = false; Settings fscratch; unsigned ovm[MAXG] = { 0 }; unsigned bk_ovm = 0, cb_ovm = 0; int bk_cur = -1;   /* #587: 작품별 개별 값 표시(줄 비트) */   /* t147d: 작품별 「전역 따르기」 끔 표시 · 전역 일괄 · 따르는 중 편집을 버리는 임시 칸 */
    int scope = 0;             /* 0 전역 1 이 작품 */
    Settings cb_g, cb_t; bool cb_has = false, cb_has_t = false; int cb_scope = 0, cb_cur = -1; std::string cb_fkr, cb_fjp;   /* t52e (#131 ③): 동작 환경 설정 진입 때 값 백업 */
    bool fs_now = false; Scr start_ret = S_MAIN;   /* t52i: fs_now = 호스트가 알려 주는 전체화면 여부 · start_ret = 시작 팝업을 연 화면(S_MAIN/S_PICK) */
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

    int scope3() const { return scope; }
    void scope_step(int d) { int i = scope + d; if (i < 0) i = 0; if (i > 1) i = 1; scope = i; }   /* #587: 범위는 「전역」 / 「이 작품」 둘뿐 */
    static unsigned fbit_of(const std::string & k) { static const char * const KN[17] = { "ratio", "shader", "curve", "bleed", "glow", "scan", "mask", "bgm", "se", "pad", "bmult", "bmode", "boot", "bosd", "bsound", "ovbtn", "fpsdisp" }; for (unsigned i = 0; i < 17; i++) if (k == KN[i]) return 1u << i; return 0u; }   /* #587: 개별 값을 줄 단위로 */
    static unsigned fbits_all() { return (1u << 17) - 1u; }
#define KH_FIELDS(X) X(ratio, 0) X(shader, 1) X(curve, 2) X(bleed, 3) X(glow, 4) X(scan, 5) X(mask, 6) X(bgm, 7) X(se, 8) X(pad, 9) X(bmult, 10) X(bmode, 11) X(boot, 12) X(bosd, 13) X(bsound, 14) X(ovbtn, 15) X(fpsd, 16)
    Settings overlay(Settings s, int gi) const { if (gi >= 0 && gi < MAXG && ovm[gi]) { const Settings & o = t[gi];
#define KH_X(f, b) if (ovm[gi] & (1u << b)) s.f = o.f;
        KH_FIELDS(KH_X)
#undef KH_X
    } return s; }
    static unsigned diff_bits(const Settings & a, const Settings & b2) { unsigned m = 0u;
#define KH_X(f, b) if (a.f != b2.f) m |= 1u << b;
        KH_FIELDS(KH_X)
#undef KH_X
        return m; }
    Settings effective(int gi) const { return overlay(follow_base(gi), gi); }
    void sync_t(int gi) { if (gi >= 0 && gi < MAXG) { Settings e = effective(gi); t[gi] = e; } }   /* 이 작품 칸 = 전역(+프로파일) 위에 개별 값을 얹은 모양으로 늘 맞춘다 */
    void ov_set(int gi, unsigned m) { if (gi >= 0 && gi < MAXG) { ovm[gi] = m; has_t[gi] = m != 0u; nofollow[gi] = m != 0u; } }
    Settings & eff() { if (scope == 1 && cur >= 0 && cur < MAXG) { sync_t(cur); return t[cur]; } return g; }
    std::vector<int> rowids(Scr sc) const {   /* #587: 화면에 보이는 줄의 실제 번호. 이 작품 범위에서는 전역 전용 줄을 숨기고 맨 아래에 「개별 설정 초기화」 */
        std::vector<int> r;
        if (sc == S_SCREEN) { r.push_back(0); r.push_back(12); for (int i = 1; i <= 7; i++) r.push_back(i);   /* 빌드 6: 줄 12 = 프리셋 (적용 범위 바로 아래) */
#if !defined(__ANDROID__)
            if (scope == 0) { r.push_back(8); r.push_back(9); }
#endif
            r.push_back(10); if (scope == 1) r.push_back(11); }
        else { for (int i = 0; i <= 13; i++) { if (scope == 1 && (i == 3 || i >= 11)) continue; if (conf_grp && i != 0 && conf_grp_of(i) != conf_grp) continue; r.push_back(i); } if (scope == 1) r.push_back(14); }
        return r; }
    int rowidx(Scr sc, int id) const { std::vector<int> r = rowids(sc); for (size_t i = 0; i < r.size(); i++) if (r[i] == id) return (int)i; return 0; }
    Settings follow_base(int gi) const {   /* 전역 + 프로파일 값(화면 효과 키 제외). 개별 값은 overlay 가 얹는다 */
        Settings s = g; if (gi >= 0 && gi < (int)games.size()) { std::map<std::string, std::string> m = games[gi].ov; static const char * const fxk[] = { "ratio", "shader", "curve", "bleed", "glow", "scan", "mask", "fx" }; for (const char * k : fxk) m.erase(k); apply_ov(s, m); }
        return s; }
    Settings & edit() { return eff(); }   /* #587: 이 작품 범위의 값 쓰기는 이 작품 칸에 — 끝난 뒤 키 처리부가 바뀐 줄만 개별 값으로 표시 */
    const UiGame * game() const { return (cur >= 0 && cur < (int)games.size()) ? &games[cur] : NULL; }
    /* t48b2 (#632): 실행 막는 사유(block)는 작품 id 로 찾는다 — 배열 위치(행 gi · 현재 cur)로 곧바로 읽지 않는다. 회색 표시 · 시작 팝업 · 정보칸 · 슬롯 불러오기가 모두 이 세 함수를 쓴다 */
    const std::string & blk_id(const std::string & id) const { static const std::string E; if (id.empty()) return E; for (size_t i = 0; i < games.size(); i++) if (games[i].id == id) return games[i].block; return E; }
    const std::string & blk_cur() const { const UiGame * g = game(); return blk_id(g ? g->id : std::string()); }
    const std::string & dn_cur() const { static const std::string E; const UiGame * g = game(); if (!g || g->id.empty()) return E; for (size_t i = 0; i < games.size(); i++) if (games[i].id == g->id) return games[i].dosnote; return E; }   /* 2d (#580): 막지 않는 작품의 안내(DOS 프롬프트로 열림 사유) */
    const std::string & blk_row(int gi) const { int ei = row_ent(gi); int tg = (ei >= 0 && games[gi].ents[ei].vgi >= 0 && games[gi].ents[ei].vgi < (int)games.size()) ? games[gi].ents[ei].vgi : gi; return blk_id(games[tg].id); }
    /* ================================================================ t39: 목록(탭·정렬) · 실행 방법 · 글꼴 */
    enum { TAB_KO, TAB_JP, TAB_UN, TAB_ALL };
    enum { SM_TITLE, SM_ORIG, SM_YEAR, SM_COMPANY, SM_RECENT, SM_PLAY, SM_CUSTOM, SM_N };
    int tab = TAB_KO, sortm = SM_RECENT; bool sortfav = true;
    std::vector<int> vis, rent;   /* vis: 목록에 보이는 작품(index) 순서 · rent[작품]: 행이 가리키는 실행 방법(ents index, -1 = 자동) */
    std::string sel_entry;        /* 고른 실행 방법 이름 — 실행 요청(Act.entry)에 실려 간다. 빈칸 = 폴더 기본 */
    bool reorder = false; long long now_s = 0; Env env;
    std::vector<UiFont> fonts; std::string font_kr, font_jp; int lang = 0; bool lang_chg = false;   /* t52g (#133): UI 언어 0 한국어 1 English 2 にほんご (전역 전용 · settings.ini [global] lang=) · 바뀌면 목록을 다시 만든다 */   /* 글꼴 폴더 목록 · 전역 선택(빈칸 = 기본) */
    static const char * tab_name(int t) { const char * n[] = { phr_("한글판"), phr_("원판"), phr_("미분류"), phr_("전체") }; return n[t < 0 || t > 3 ? 3 : t]; }
    static const char * sort_name(int m) { static const char * n[] = { phr_("제목"), phr_("원제"), phr_("연도"), phr_("제작사"), phr_("최근 실행"), phr_("자주 실행"), phr_("사용자 지정") }; return n[m < 0 || m >= SM_N ? 0 : m]; }
    static const char * sort_id(int m) { static const char * n[] = { "title", "orig", "year", "company", "recent", "play", "custom" }; return n[m < 0 || m >= SM_N ? 0 : m]; }
    static int sort_from(const std::string & s) { for (int i = 0; i < SM_N; i++) if (s == sort_id(i)) return i; return SM_RECENT; }
    static const char * tab_id(int t) { static const char * n[] = { "ko", "jp", "un", "all" }; return n[t < 0 || t > 3 ? 3 : t]; }
    static int tab_from(const std::string & s) { for (int i = 0; i < 4; i++) if (s == tab_id(i)) return i; return TAB_ALL; }

    /* ---- 제목 도우미 (#288): 자리마다 정한 규칙으로 줄인다 */
    static std::string short_of(const UiGame & g) { return g.short_t.empty() ? auto_short(g.title) : g.short_t; }
    static std::string ttl_fit(const UiGame & g, int px) { return fit_title(short_of(g), px); }
    static std::string ttl_suf(const UiGame & g, const char * suf, int px) { int sw = tw(std::string(" ") + suf); return fit_title(short_of(g), px - sw) + " " + suf; }
    /* t44a (#494): 작품 언어 표시 — 사용자 지정(lang_exp) > 자동 판정 */
    static std::string lang_label(const UiGame & g) { if (g.lang_exp == "ko") return phr_("한글판 (지정)"); if (g.lang_exp == "jp") return phr_("원판 일본어 (지정)"); if (g.lang == "ko") return phr_("한글판 (gamedb)"); if (g.lang == "jp") return phr_("원판 일본어 (gamedb)"); return phr_("미분류"); }
    int gidx_of(const std::string & id) const { for (size_t i = 0; i < games.size(); i++) if (games[i].id == id) return (int)i; return -1; }
    void open_lang(const std::vector<std::string> & ids, const std::vector<std::string> & why, Scr ret) { lang_gid = ids; lang_why = why; lang_ret = ret; if (lang_why.size() < lang_gid.size()) lang_why.resize(lang_gid.size()); sel = 0; scr = S_LANG; }
    void d_lang(Canvas & c);
    static std::string ttl_full(const UiGame & g, int px) { return fit_title(g.title, px); }

    int base_of(int g) const { if (g < 0 || g >= (int)games.size()) return g; int b = games[g].base; return b >= 0 && b < (int)games.size() ? b : g; }
    std::vector<int> vents(int gi) const { std::vector<int> r; if (gi < 0 || gi >= (int)games.size()) return r; for (size_t i = 0; i < games[gi].ents.size(); i++) if (!games[gi].ents[i].hidden) r.push_back((int)i); return r; }
    /* t41 (#306): 탭이 실행 언어를 정한다 — 한글판 탭은 원판(jp) 항목을, 원판 일본어 탭은 jp 가 아닌 항목을 ←→ 순환·행 표시에서 뺀다. 전체 탭은 전부. 항목 언어가 비면 작품 언어를 따른다. 걸러서 비면 전부(안전). 편집 메뉴(vents)는 그대로 전부 본다. */
    std::vector<int> tvents(int gi) const {
        std::vector<int> v = vents(gi); if (tab == TAB_ALL || tab == TAB_UN || gi < 0 || gi >= (int)games.size()) return v;
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
        if (tab == TAB_ALL) return true; if (tab == TAB_KO) return g.lang == "ko"; if (tab == TAB_UN) return g.lang.empty();
        if (g.lang.empty()) return false; if (g.lang == "jp") return true; for (auto & e : g.ents) if (!e.hidden && e.lang == "jp") return true; return false;
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
        std::string s = p; const std::string han = phr_("한글판"); if (s.compare(0, han.size(), han) == 0) { s = s.substr(han.size()); size_t i = 0; while (i < s.size() && (s[i] == ' ' || s[i] == '-')) i++; s = s.substr(i); }
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
    void fit_tab_to_cur() { int b = base_of(cur); if (b < 0 || b >= (int)games.size()) return; if (row_in_tab(b)) return; tab = games[b].lang == "jp" ? TAB_JP : games[b].lang == "ko" ? TAB_KO : TAB_UN; if (!row_in_tab(b)) tab = TAB_ALL; }

    /* ---- 편집 상태 */
    int eg = -1;                 /* 편집 대상 작품(user.ini 를 쓸 폴더) */
    int entg = -1;               /* 실행 방법 목록의 행 작품(base) */
    std::string en_orig; bool enew = false; ent::Kv ed, ed0; std::string ed_name, ed_name0; bool ed_setdef = false;
    std::string msg;             /* 한 줄 알림 */
    std::string yn_msg[3]; int yn_act = 0, ynsel = 1; Scr yn_ret = S_MAIN; int yn_arg = 0;
    enum { YN_DISCARD, YN_DELETE, YN_TEST, YN_INSTDONE, YN_RESET, YN_TITLE, YN_MAINMENU, YN_DELSLOT, YN_DISKRESET, YN_INDIVRESET, YN_INDIVALL, YN_PREDEL };   /* t52e (#131) */
    /* 파일 고르기 */
    std::string fl_key, fl_dir; std::vector<std::pair<std::string, std::string> > fl_roots; int fl_ri = 0, fl_swapi = -1; std::vector<FileItem> fl_items; bool fl_sort_kind = false; Scr fl_ret = S_EDIT;
    /* 자동 입력 · 교체 목록 */
    std::vector<ent::Step> ks; int ksel = 0, stepi = 0; std::vector<std::string> sw; int swsel = 0;
    /* 글꼴 선택 */
    /* ---- t48e (#646/#651): 조작 설정 — 저장 키 이름은 SDL 표준 버튼 이름 하나(btn_b btn_a btn_x btn_start btn_leftshoulder btn_rightshoulder btn_back).
       전역 = settings.ini [global], 이 작품 = user.ini [pad] (작품이 이긴다) */
    std::map<std::string, std::string> gpad;   /* 전역 btn_* */
    std::vector<std::string> pk_opts; int pk_row = 0, pad_sel_keep = 0; bool hot_cap = false; int hot_cap_t0 = 0; bool ext_wait = false; int ext_t0 = 0;
    std::string xb_get(int n) const { auto it = gpad.find(std::string("btn_xreg") + std::to_string(n)); return it == gpad.end() ? std::string() : it->second; }   /* build 5a-2b: registered extra button n = kind|code|name */
    std::string xb_name(int n) const { const std::string v = xb_get(n); const size_t a = v.find('|'), b = a == std::string::npos ? a : v.find('|', a + 1); return b == std::string::npos ? std::string("?") : v.substr(b + 1); }
    int xb_cnt() const; int xb_slot(int row) const;
    std::vector<KbCell> kb_cells; int kb_sel = 0;   /* build 5a-2c: key picker on the PC-98 layout (screens/kbpick.h) */
    bool kb_used(const std::string & nm) const; void kb_open(); void d_kbpick(Canvas & c); void k_kbpick(UKey k, Act & a);
    int conf_grp = 0; bool save_mode = false; Scr scr_from = S_MAIN;   /* build 5b-1: settings group (0 all, 1 sound, 2 control, 3 system) */
    void d_set(Canvas & c); void k_set(UKey k, Act & a); void d_gkb(Canvas & c); void k_gkb(UKey k, Act & a);
    std::string sh_code, sh_ename, sh_msg; share::Decoded sh_dec; std::vector<std::string> sh_missing, sh_files; int sh_found = 0, sh_total = 0;
    void shr_open_make(int g, int menu_i); void shr_open_import(); void shr_preview(const std::string & code);
    void d_share(Canvas & c); void k_share(UKey k, Act & a); void d_shimp(Canvas & c); void k_shimp(UKey k, Act & a);
    void d_shprev(Canvas & c); void k_shprev(UKey k, Act & a);
    static int conf_grp_of(int i) { return (i >= 1 && i <= 3) ? 1 : (i == 8 || i >= 11) ? 3 : 2; }
    void leave_cfg() {   /* leaving S_SCREEN / S_CONF: back to the settings hub when it was opened from there */
        const bool sc = scr == S_SCREEN;
        if (sc && scr_from == S_SET) go(S_SET, 0);
        else if (!sc && conf_grp) go(S_SET, conf_grp);
        else { go(in_game ? S_GAME : S_MAIN, in_game ? 4 : 0); if (!in_game) sel = 0; }
    }
    void pad_jump(int pb) { if (scr == S_PAD && pb >= 0 && pb < PAD_N) sel = pb + 2; }   /* a pad press on L2 R2 L3 R3 moves the cursor to that button */
    static std::string pad_show_s(const std::string & v) {   /* short label for the layout panel */
        if (v == "fn:boost_hold") return phr_("fn_boost_hold_s"); if (v == "fn:boost_toggle") return phr_("fn_boost_toggle_s"); return pad_show(v); }
    enum { PAD_N = 16, PAD_FN_N = 10, HOT_N = 6 };   /* 빌드 5a: 배정 가능 버튼 = 16 (옛 7 + 북 · 십자 4 · L2 R2 · L3 R3) */
    static const char * pad_key(int i) { static const char * const K[PAD_N + 8] = { "btn_b", "btn_a", "btn_x", "btn_start", "btn_leftshoulder", "btn_rightshoulder", "btn_back", "btn_y", "btn_dpup", "btn_dpdown", "btn_dpleft", "btn_dpright", "btn_lefttrigger", "btn_righttrigger", "btn_leftstick", "btn_rightstick", "btn_ext0", "btn_ext1", "btn_ext2", "btn_ext3", "btn_ext4", "btn_ext5", "btn_ext6", "btn_ext7" }; return K[i < 0 ? 0 : i >= PAD_N + 8 ? PAD_N + 7 : i]; }
    static const char * pad_fn_name(int q) { static const char * const N[PAD_FN_N] = { "", "menu", "boost_hold", "boost_toggle", "quick_save", "quick_load", "osk", "screenshot", "next_disk", "fps" }; return N[q < 0 || q >= PAD_FN_N ? 0 : q]; }   /* kh_input12.h FN_* 순서와 같게 */
    static const char * pad_fn_key(int q) { static const char * const N[PAD_FN_N] = { "", "fn_menu", "fn_boost_hold", "fn_boost_toggle", "fn_quick_save", "fn_quick_load", "fn_osk", "fn_screenshot", "fn_next_disk", "fn_fps" }; return N[q < 0 || q >= PAD_FN_N ? 0 : q]; }
    std::string pad_row_name(int i) const {
        int kd = env.padkind ? env.padkind() : 0;
        static const char * const X[PAD_N] = { phr_("B 버튼"), phr_("A 버튼"), phr_("X 버튼"), "START", "LB", "RB", "BACK", phr_("pad_n_y"), "D-PAD UP", "D-PAD DOWN", "D-PAD LEFT", "D-PAD RIGHT", "LT", "RT", "L-STICK", "R-STICK" };
        static const char * const P[PAD_N] = { "○", "×", "□", "START", "L1", "R1", "SELECT", phr_("pad_t_n"), "D-PAD UP", "D-PAD DOWN", "D-PAD LEFT", "D-PAD RIGHT", "L2", "R2", "L3", "R3" };
        static const char * const O[PAD_N] = { phr_("동쪽 버튼"), phr_("남쪽 버튼"), phr_("서쪽 버튼"), "START", phr_("왼쪽 어깨"), phr_("오른쪽 어깨"), "SELECT", phr_("pad_n_north"), "D-PAD UP", "D-PAD DOWN", "D-PAD LEFT", "D-PAD RIGHT", "L2", "R2", "L3", "R3" };
        if (i >= PAD_N && i < PAD_N + 8) return xb_name(i - PAD_N);
        if (i < 0 || i >= PAD_N) i = 0;
        if ((kd == 0 || kd == 3) && (i == 0 || i == 1 || i == 2 || i == 7)) {   /* arrow = position on the pad, brackets = the name printed on it (Retroid) */
            static const char * const AR[PAD_N] = { "\xE2\x86\x92 ", "\xE2\x86\x93 ", "\xE2\x86\x90 ", "", "", "", "", "\xE2\x86\x91 ", "", "", "", "", "", "", "", "" };
            static const char * const RN[PAD_N] = { " (A)", " (B)", " (Y)", "", "", "", "", " (X)", "", "", "", "", "", "", "", "" };
            return std::string(AR[i]) + phr_("pad_btn") + (kd == 3 ? RN[i] : ""); }
        return kd == 1 ? X[i] : kd == 2 ? P[i] : O[i];
    }
    std::string pad_tag(int i) const {   /* 빌드 5a: 배치 그림 칸의 짧은 이름 */
        int kd = env.padkind ? env.padkind() : 0;
        static const char * const T[PAD_N] = { "", "", "", "ST", "L1", "R1", "SE", "", "UP", "DN", "LF", "RG", "L2", "R2", "L3", "R3" };
        if (i == 0 || i == 1 || i == 2 || i == 7) {
            const int f = i == 7 ? 3 : i;
            if (kd == 1) { static const char * const XB[4] = { "B", "A", "X", "Y" }; return XB[f]; }
            if (kd == 2) { static const char * const PK[4] = { "pad_t_e", "pad_t_s", "pad_t_w", "pad_t_n" }; return phr_(PK[f]); }
            if (kd == 3) { static const char * const NT[4] = { "A", "B", "Y", "X" }; return NT[f]; }
            static const char * const CP[4] = { "E", "S", "W", "N" }; return CP[f]; }
        return T[i < 0 || i >= PAD_N ? 0 : i];
    }
    std::string pad_default(int i) {   /* 서양식이면 동쪽·남쪽 기본이 서로 바뀐다 */
        bool w = eff().pad == 1;
        switch (i) { case 0: return w ? "esc" : "space"; case 1: return w ? "space" : "esc"; case 2: return "caps"; case 3: return "ret"; case 7: return "fn:menu"; case 8: return "up"; case 9: return "down"; case 10: return "left"; case 11: return "right"; case 13: return "fn:boost_hold"; case 14: return "fn:boost_toggle"; default: return "none"; }
    }
    static std::string pad_show(const std::string & v) {
        if (v.compare(0, 3, "fn:") == 0) { for (int q = 1; q < PAD_FN_N; q++) if (v.compare(3, std::string::npos, pad_fn_name(q)) == 0) return phr_(pad_fn_key(q)); }
        if (v == "none") return phr_("배정 없음"); if (v == "mouse_left") return phr_("마우스 왼"); if (v == "mouse_right") return phr_("마우스 오른");
        std::string u = v; for (auto & c : u) c = (char)toupper((unsigned char)c); return u;
    }
    std::string pad_val(int i, bool * from_work = NULL, bool * is_def = NULL) {   /* 지금 이 범위에서 보이는 값 */
        std::string k = pad_key(i), v; if (from_work) *from_work = false; if (is_def) *is_def = false;
        if (scope == 1 && i < PAD_N && env.pad_get && cur >= 0 && cur < (int)games.size()) { std::map<std::string, std::string> m; env.pad_get(cur, m); auto it = m.find(k); if (it != m.end() && !it->second.empty()) { if (from_work) *from_work = true; return it->second; } }
        auto g2 = gpad.find(k); if (g2 != gpad.end() && !g2->second.empty()) return g2->second;
        if (is_def) *is_def = true; return pad_default(i);
    }
    void open_padkey(int row) {
        pk_row = row; pad_sel_keep = sel; pk_opts.clear(); pk_opts.push_back(std::string()); pk_opts.push_back("none"); pk_opts.push_back("mouse_left"); pk_opts.push_back("mouse_right"); for (int q = 1; q < PAD_FN_N; q++) pk_opts.push_back(std::string("fn:") + pad_fn_name(q));   /* 빌드 5a: 큐하치 기능 */
        if (env.padkeys) for (auto & n : env.padkeys()) pk_opts.push_back(n);
        std::string cv; { std::string k = pad_key(row); if (scope == 1 && row < PAD_N && env.pad_get && cur >= 0 && cur < (int)games.size()) { std::map<std::string, std::string> m; env.pad_get(cur, m); auto it = m.find(k); if (it != m.end()) cv = it->second; } else if (scope == 0 || row >= PAD_N) { auto it = gpad.find(k); if (it != gpad.end()) cv = it->second; } }
        sel = 0; for (size_t i = 0; i < pk_opts.size(); i++) if (pk_opts[i] == cv) sel = (int)i; scr = S_PADKEY;
    }
    void d_pad(Canvas & c);
    void d_padkey(Canvas & c);
    void d_hot(Canvas & c); void k_hot(UKey k, Act & a); void hot_poll(); void d_ext(Canvas & c); void k_ext(UKey k, Act & a);   /* build 5a-2: hotkey tab (screens/hot.h) */
    static int hot_fn(int k) { static const int F[HOT_N] = { 4, 5, 6, 7, 8, 9 }; return F[k < 0 || k >= HOT_N ? 0 : k]; }   /* pad_fn_name numbers: quick save/load, osk, screenshot, next disk, fps */
    static std::string hot_default(int q); std::string hot_val(int q, bool * custom = NULL); int hot_btn(const std::string & v, bool * sel);
    std::string hot_text(const std::string & v); bool hot_overlap(int q);
    int fnt_mode = 0;            /* 0 전역 한글 · 1 전역 일본어 · 2 항목 */
    std::vector<std::string> fnt_opts; std::string fnt_need; Scr fnt_ret = S_CONF; std::map<std::string, ov::BmpFont> fnt_cache;
    /* 화면 키보드 */
    Act pend;   /* t49a: 키 처리 중 만들어진 동작(표시 제목 저장) */
    std::string stage_next_label;   /* t49a (#845): 지금 단계의 다음 단계 이름(비면 없음) — 일시정지 메뉴에 「다음 단계」 */
    std::string osk_buf; int osk_target = 0, osk_r = 0, osk_c = 0; bool osk_upper = false; Scr osk_ret = S_EDIT; std::string ime_comp;   /* t48a: ime_comp = IME 조합 중 글자 */
    hg::Composer hc; bool osk_hg = false, osk_shift = false;   /* t48b: 한글 화면 키보드(이름 칸) — osk_hg = 켜짐, osk_shift = SHIFT 한 번 눌림(다음 자모 하나만) */
    std::vector<ValRow> vals; int vals_top = 0;   /* t48a (#614 ③) */
    int tplsel = 0, menusel = 0, hddsel = 0;
    bool install_start = false;  /* 설치 필요 게임을 처음 시작할 때의 HDD 만들기 */

    static const std::vector<std::string> & key_names() { static std::vector<std::string> v = { "enter", "space", "esc", "tab", "up", "down", "left", "right", "y", "n", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "bs", "del", "caps", "kana", "pageup", "pagedown", "ctrl", "shift" }; return v; }
    static const std::vector<std::string> & type_presets() { static std::vector<std::string> v = { "dir~", "cd..~", "y~", "n~", "1~", "2~", "a:~", "c:~", "d:~" }; return v; }
    static const std::vector<std::string> & name_presets() { static std::vector<std::string> v = { phr_("한글판"), phr_("원판 일본어"), phr_("CD 모드"), phr_("HDD 부팅"), phr_("FD 부팅"), phr_("데모 먼저"), phr_("다장 디스크"), phr_("설치 필요"), phr_("내 실행 방법") }; return v; }
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
    static std::string disk_label(const std::string & v) { if (v.empty() || v == "-") return phr_("(없음)"); return ent::base_name(v); }

    /* ---- 그리기 공통 */
    void titlebar(Canvas & c, const char * name) {
        (void)name; text(c, 40, 0, KH_FULLNAME, WH, K);   /* 맨 윗줄은 제품명만 (화면 이름은 창 제목에) */
        std::string b = build; int w = tw(b); text(c, 640 - w - 24, 0, b, WH, K);
    }
    struct Hit { int x, y, w, h, i, sel; };   /* t55a: touch hit-test rows (rect, index, selection at draw time) */
    std::vector<Hit> hits;
    void items(Canvas & c, int x, int y, int w, const std::vector<std::string> & a, int sel, int pitch, const std::vector<bool> * dim = NULL) {
        for (size_t i = 0; i < a.size(); i++) {
            int yy = y + (int)i * pitch; bool d = dim && (*dim)[i]; { Hit ht = { x - 6, yy - 3, w, 22, (int)i, sel }; hits.push_back(ht); }
            if ((int)i == sel) c.rect(x - 6, yy - 3, w, 22, d ? GY : WH);
            c.clip(x, yy - 3, w - 12, 22);   /* r018: 글자가 막대·상자 밖으로 나가지 않게 */
            if ((int)i == sel) text(c, x, yy, a[i], K, d ? GY : WH); else text(c, x, yy, a[i], d ? GY : CY, K);
            c.unclip();
        }
    }
    /* t50a (#873 안 A): help 를 받으면 도움말이 두 줄이 될 때 알림 칸이 도움말 시작(c.h-32)을 침범하지 않게 칸을 줄이고 글줄 간격을 16 으로 모은다 (줄은 하나도 빼지 않는다) */
    void desc(Canvas & c, int y, int h, const std::vector<std::string> & ln, int orfrom = -1, const char * help = NULL) {
        int y0 = 14, dy = 20;
        if (help && y + h > c.h - 32 && help_rows(c, help) == 2) { h = c.h - 32 - 1 - y; y0 = 6; dy = 16; }
        rbox(c, 16, y, 608, h, OR, NULL); for (size_t i = 0; i < ln.size(); i++) text(c, 40, y + y0 + (int)i * dy, ln[i], (orfrom >= 0 && (int)i >= orfrom) ? OR : WH, K);
    }
    void thumb(Canvas & c, const UiGame * g, int x, int y, int w, int h) {
        c.rect(x - 1, y - 1, w + 2, h + 2, GY); c.rect(x, y, w, h, K);
        if (g && !g->thmb.empty()) blit_scaled(c, g->thmb.data(), g->tw_, g->th_, x, y, w, h);
        else { const char * m = phr_("저장 없음"); text(c, x + (w - tw(m)) / 2, y + h / 2 - 8, m, GY, K); }   /* r018: 표본 그림 대신 빈 칸 (나중에 타이틀 화면 캡처로 채움) */
    }
    static void menu_names(std::vector<std::string> & v) { v = { phr_("게임 시작"), phr_("작품 선택"), phr_("화면 설정"), phr_("mm_settings"), phr_("세이브 관리"), phr_("부가"), phr_("끝내기") }; }
    static std::string play_str(int s) { char b[48]; if (s >= 3600) snprintf(b, sizeof b, "%d시간 %02d분", s / 3600, (s / 60) % 60); else snprintf(b, sizeof b, "%d분", s / 60); return b; }
    /* 확인 창: 화면 가운데, 뒤 화면 어둡게. 반환 = 창 왼쪽 위 x,y */
    void confirm_box(Canvas & c, int w, int h, const char * title, int & bx, int & by) {
        dim(c); bx = (c.w - w) / 2; by = (c.h - h) / 2; c.rect(bx, by, w, h, K); rbox(c, bx, by, w, h, YL, title);
    }
    void sample_or_game(Canvas & c, int x, int y, int w, int h) { thumb(c, game(), x, y, w, h); }

    /* ---- 화면별 그리기 */
    void draw(Canvas & c); void draw_body(Canvas & c);
    void d_shim(Canvas & c); void k_shim(UKey k, Act & a);
    /* 인포메이션 = 현재 작품의 게임 정보 */
    static std::string src_label(const std::string & m) {   /* 내부 출처 이름 → 화면 글 (내부 용어는 쓰지 않는다) */
        if (m.empty() || m == "none") return std::string();
        if (m == "tosec") return "TOSEC"; if (m == "nkset") return phr_("롬셋 폴더"); if (m == "nk") return phr_("롬셋"); if (m == "curated" || m == "큐레이션") return "DB";
        return m;
    }
    std::string mk_line(const UiGame & g) const {   /* 제작 · 연도 · 장르 + 출처. 빈 값은 「-」 */
        std::string mk = g.company.empty() ? std::string("-") : g.company; if (!g.year.empty()) mk += " · " + g.year; if (!g.genre.empty()) mk += " · " + g.genre;
        std::string sl = src_label(g.metasrc); if (!sl.empty() && !(g.company.empty() && g.year.empty() && g.genre.empty())) mk += " (" + sl + ")";
        return mk;
    }
    void info_box(Canvas & c, int x, int y, int w, int h) {
        rbox(c, x, y, w, h, WH, phr_("인포메이션"));
        const UiGame * gm = game(); if (!gm) return;
        int LW = 56;   /* t52h (#134): 라벨 칸 폭 = 가장 긴 라벨 + 8 (한국어는 그대로 56, 일본어·영어는 넓어짐) */
        { const char * const lb[9] = { phr_("제작"), phr_("패치"), phr_("디스크"), phr_("장치"), phr_("설정"), phr_("판정"), phr_("사유"), phr_("플레이"), phr_("저장") }; for (int q = 0; q < 9; q++) { int t = tw(lb[q]) + 8; if (t > LW) LW = t; } }
        const int X = x + 24, VW = w - 48 - LW - 8; int yy = y + 16;
        auto line = [&](const char * lab, const std::string & v, uint32_t col = WH) { text(c, X, yy, lab, CY, K); text(c, X + LW, yy, fit(v, VW), col, K); yy += 20; };
        { std::vector<std::string> tl = title_lines(gm->title, w - 48, 2); for (auto & l : tl) { text(c, X, yy, l, blk_cur().empty() ? YL : GY, K); yy += 20; } yy += 2; }
        line(phr_("제작"), mk_line(*gm));
        line(phr_("패치"), gm->patch.empty() ? std::string("-") : gm->patch);
        line(phr_("디스크"), gm->disknote.empty() ? std::string("-") : gm->disknote);
        line(phr_("장치"), gm->devices.empty() ? std::string(phr_("특별한 장치 불필요")) : gm->devices);
        { static const struct { const char * k; const char * n; } T[] = { { "ratio", phr_("비율") }, { "shader", phr_("셰이더") }, { "curve", phr_("곡면") }, { "scan", phr_("주사선") }, { "mask", phr_("마스크") }, { "bleed", phr_("RGB번짐") }, { "glow", phr_("빛번짐") }, { "bgm", "BGM" }, { "se", phr_("효과음") }, { "pad", phr_("패드") }, { "boost", phr_("배속") }, { "boostmode", phr_("배속방식") } };
          std::string o; for (auto & t : T) if (gm->ov.count(t.k)) { if (!o.empty()) o += ", "; o += t.n; }
          if (cur >= 0 && cur < MAXG && ovm[cur]) { if (!o.empty()) o += " + "; o += phr_("내 설정"); }
          line(phr_("설정"), o.empty() ? std::string(phr_("전역과 같음")) : o); }
        line(phr_("판정"), gm->jshort.empty() ? std::string("-") : gm->jshort);
        if (!blk_cur().empty()) line(phr_("사유"), blk_cur(), OR); else line(phr_("플레이"), play_str(gm->play_s));
        line(phr_("저장"), (gm->lastslot >= 0 && gm->slot[gm->lastslot].used) ? phr_("슬롯 ") + slot_lbl(gm->lastslot) + "  " + gm->slot[gm->lastslot].stamp : std::string(phr_("없음")));
    }
    static std::string slot_lbl(int i) { return i == 0 ? std::string(phr_("자동")) : i == 1 ? std::string(phr_("퀵")) : std::string(phr_("일반 ")) + std::to_string(i - 1); }   /* 1b */
    bool has_save(const UiGame * g) const { return g && g->lastslot >= 0 && g->slot[g->lastslot].used; }
    /* mode 0 = 메인, 1 = 게임 시작 창, 2 = 끝내기 확인(타이틀에서) */
    void d_main(Canvas & c, int mode);
    void d_quit(Canvas & c);
    void d_swap(Canvas & c);
    void d_swapi(Canvas & c);
    void d_about(Canvas & c);
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
    int dq_key = -1, dq_since = 0;
    void dline(Canvas & c, int x, int y, int w, const std::string & full) {   /* 4 (#543): 한 줄 설명이 칸보다 길면 가로로 흐른다(1초 쉼 → 흐름 → 끝에서 잠깐 멈춤 → 처음). 줄이 바뀌면 처음부터 */
        const int key = (int)scr * 256 + sel; if (key != dq_key) { dq_key = key; dq_since = ts_ms; }
        const int t = tw(full); if (t <= w) { text(c, x, y, full, WH, K); return; }
        const int maxoff = t - w + 8, span = maxoff * 25, hold0 = 1000, hold1 = 800, cyc = hold0 + span + hold1;
        int e = ts_ms - dq_since; if (e < 0) e = 0; const int tc = e % cyc; const int off = tc < hold0 ? 0 : tc < hold0 + span ? (tc - hold0) / 25 : maxoff;
        c.clip(x, y - 3, w, 22); text(c, x - off, y, full, WH, K); c.unclip();
    }
    void desc_mq(Canvas & c, int y, int h, const std::string & full, const char * help = NULL) {   /* desc() 와 같은 상자 + 한 줄 흐름 */
        int y0 = 14; if (help && y + h > c.h - 32 && help_rows(c, help) == 2) { h = c.h - 32 - 1 - y; y0 = 6; }
        rbox(c, 16, y, 608, h, OR, NULL); dline(c, 40, y + y0, 560, full);
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
    void d_pick(Canvas & c);
    void d_sort(Canvas & c);
    std::string ent_line(const UiGame & g, int ei, int w) { const UiEnt & e = g.ents[ei]; std::string t = e.label; if (!e.tag.empty()) t += "  [" + e.tag + "]"; return fit(t, w); }
    void d_ent(Canvas & c);
    void d_entmenu(Canvas & c);
    /* t49a (#845): 실행 순서 = 항목의 next= 사슬. 후보: 없음 → (정의 없는 작품이면 기본 실행) → 같은 폴더의 다른 항목들 */
    std::vector<std::string> next_cands(int gi, int ei) const {
        std::vector<std::string> r; r.push_back("none"); if (gi < 0 || gi >= (int)games.size()) return r; const UiGame & g = games[gi];
        bool pse = false; for (auto & e : g.ents) if (e.pseudo) pse = true; if (pse) r.push_back("-");
        for (size_t i = 0; i < g.ents.size(); i++) if ((int)i != ei && !g.ents[i].pseudo && !g.ents[i].hidden && g.ents[i].vgi < 0) r.push_back(g.ents[i].name);
        return r;
    }
    std::string next_menu_name(int gi, int menu_i) const {   /* F12 메뉴에 나타나는 이름(속성 「다음 단계로」 의 메뉴 이름). 비면 기본 문구 */
        std::vector<int> v = vents(gi); if (menu_i < 0 || menu_i >= (int)v.size()) return std::string(); const UiEnt & e = games[gi].ents[v[menu_i]]; auto it = e.kv.find("next_label");
        return (it != e.kv.end() && !it->second.empty()) ? it->second : std::string(phr_("다음 단계로 넘어가기")); }
    std::string nl_name;   /* 메뉴 이름을 고치는 중인 항목 이름 */
    std::string next_label(int gi, int menu_i) const {
        std::vector<int> v = vents(gi); if (menu_i < 0 || menu_i >= (int)v.size()) return phr_("없음"); const UiGame & g = games[gi]; auto it = g.ents[v[menu_i]].kv.find("next");
        if (it == g.ents[v[menu_i]].kv.end() || it->second.empty() || it->second == "none") return phr_("없음"); if (it->second == "-") return phr_("기본 실행");
        for (auto & e : g.ents) if (e.name == it->second) return e.label.empty() ? e.name : e.label; return it->second;
    }
    std::vector<bool> menu_dim() const {
        std::vector<bool> dm(9, false); std::vector<int> v = vents(entg); int ei = (menu_ent >= 0 && menu_ent < (int)v.size()) ? v[menu_ent] : -1; const UiGame & g = games[entg];
        if (ei < 0 || g.ents[ei].pseudo || g.ents[ei].vgi >= 0) { for (auto x : { 0, 2, 3, 4, 5, 6, 7 }) dm[x] = true; if (ei < 0 || g.ents[ei].pseudo) dm[1] = true; }
        else if (g.ents[ei].tag == "자동") { for (auto x : { 0, 2, 3, 4, 5 }) dm[x] = true; }
        else { if (g.ents[ei].tag != "수정됨") dm[3] = true; }
        return dm;
    }
    int menu_ent = 0;
    void d_tpl(Canvas & c);
    static const char * mouse_label(const std::string & m) { if (m == "none") return phr_("없음"); if (m == "optional") return phr_("선택 사항"); if (m == "pointer") return phr_("포인터"); if (m == "required") return phr_("필수"); return phr_("기본"); }
    std::string font_label(const std::string & f, const std::string & fk) const { if (!f.empty()) return f; if (fk == "jp") return phr_("일본어 (목록 기본)"); if (fk == "kr") return phr_("한글 (목록 기본)"); return phr_("자동 (목록 기본)"); }
    void d_edit(Canvas & c);
    int fl_filter() const { if (fl_key == "disk_c" || fl_key == "disk_d") return 2; if (fl_key == "cd") return 3; return 1; }
    static std::string size_str(long long b) { char s[32]; if (b >= 1048576) snprintf(s, sizeof s, "%d.%dMB", (int)(b >> 20), (int)(((b * 10) >> 20) % 10)); else snprintf(s, sizeof s, "%dKB", (int)(b >> 10)); return s; }
    void d_file(Canvas & c);
    void d_keyseq(Canvas & c);
    void d_step(Canvas & c);
    void d_swaped(Canvas & c);
    void d_exepick(Canvas & c);
    void k_exepick(UKey k, Act & a);
    /* 2b: 실행 파일 고르기 — 이미지 안 폴더 보기 */
    std::vector<FileItem> xp_items; std::string xp_dir, xp_dk[2]; int xp_drv = 0, xp_nd = 1; bool xp_err = false;
    void xp_load() { xp_items.clear(); xp_err = !(env.exelist && !xp_dk[xp_drv].empty() && env.exelist(eg, xp_dk[xp_drv], xp_dir, xp_items)); }
    void xp_open() {   /* A: = 첫 FD(디스크 A, 없으면 B), B: = 둘째 FD (둘 다 있을 때만) — 내장 DOS 장착 순서와 같다 */
        std::string da = kv_get("disk_a"), db = kv_get("disk_b"); xp_dk[0] = da.empty() ? db : da; xp_dk[1] = (!da.empty() && !db.empty()) ? db : std::string();
        xp_nd = xp_dk[1].empty() ? 1 : 2; xp_drv = 0; xp_dir.clear(); xp_load(); sel = 0; scr = S_EXEPICK;
    }
    void d_font(Canvas & c);
    std::string fnt_default_name() const { if (fnt_mode == 1) return font_jp.empty() ? "jp_default" : font_jp; return font_kr.empty() ? "kyuhachi_font" : font_kr; }   /* t52g: 임시 이름 pc98player_font → kyuhachi_font */
    void jp_refresh() { const ov::BmpFont * bf = fnt_font(font_jp.empty() ? std::string("jp_default") : font_jp); ov::jp_set(lang == 2, bf); }   /* t52h (#134): 일본어 글꼴 BMP · 언어를 글자 그리기에 알림 */
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
        if (r == 4) { static const char * const L[6] = { phr_("한/영"), "SHIFT", phr_("띄어쓰기"), phr_("지움"), phr_("완료"), phr_("취소") }; return L[q]; }
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
    void d_hosk(Canvas & c);
    void d_osk(Canvas & c);
    void d_name(Canvas & c);
    void d_yn(Canvas & c, Scr under);
    void d_hddask(Canvas & c);
    void d_add(Canvas & c);
    void d_screen(Canvas & c);
    void d_screen_t48b2_old(Canvas & c) {
        titlebar(c, phr_("화면 설정"));
        Settings & s = eff(); bool crt = s.shader == 1;
        rbox(c, 16, 24, 288, 212, WH, phr_("화면 설정"));
        std::vector<std::string> n = { phr_("적용 범위"), phr_("비율"), phr_("셰이더"), phr_("곡면"), phr_("번짐"), phr_("주사선 세기"), phr_("마스크"), phr_("창 배율") };
        std::vector<bool> dim = { false, false, false, !crt, !crt, !crt, !crt, false };
        items(c, 40, 44, 256, n, sel, 24, &dim);
        rbox(c, 320, 24, 304, 212, WH, phr_("선택지"));
        auto row = [&](int i) { return sel == i; };
        const char * sc[] = { phr_("전역"), phr_("작품·따름"), phr_("작품·전용") }; choices(c, 344, 44, sc, 3, scope3(), WH, row(0));
        const char * rt[] = { "4:3", "16:10", phr_("정수배") }; choices(c, 344, 68, rt, 3, s.ratio, WH, row(1));
        const char * sh[] = { "sharp", "crt", "nearest" }; choices(c, 344, 92, sh, 3, s.shader, WH, row(2));
        if (crt) {
            char b[16]; snprintf(b, sizeof b, "%.2f", s.curve_f()); barv(c, 340, 116, 232, s.curve / 10.0, phr_("평면"), phr_("최대"), WH, row(3)); text(c, 578, 116, b, WH, K);
            snprintf(b, sizeof b, "%d", s.bleed * 10); barv(c, 340, 140, 232, s.bleed / 10.0, phr_("없음"), phr_("최대"), WH, row(4)); text(c, 578, 140, b, WH, K);
            snprintf(b, sizeof b, "%d", s.scan * 10); barv(c, 340, 164, 232, s.scan / 10.0, phr_("약"), phr_("강"), WH, row(5)); text(c, 578, 164, b, WH, K);
            const char * mk[] = { phr_("꺼짐"), phr_("약"), phr_("중"), phr_("강") }; choices(c, 344, 188, mk, 4, s.mask, WH, row(6));
        } else for (int i = 3; i < 7; i++) text(c, 344, 44 + i * 24, phr_("(crt 에서만)"), GY, K);
        { const char * wm[] = { "x1", "x2", "x3", "x4", "x5", phr_("사용자 지정") }; unsigned dmk = 0; for (int i = 0; i < WMN; i++) if (i + 1 > wmax) dmk |= 1u << i;
          choicesd(c, 344, 212, wm, 6, wmult ? wmult - 1 : 5, WH, dmk); }
        rbox(c, 16, 240, 608, 82, WH, phr_("미리보기"));
        thumb(c, game(), 40, 258, 96, 60);
        const UiGame * gm = game();
        { std::string sf = gm && gm->lastslot >= 0 ? phr_("  마지막 저장 (") + gm->slot[gm->lastslot].stamp + ")" : std::string(); text(c, 156, 258, gm && gm->lastslot >= 0 ? fit_title(gm->title, 440 - tw(sf)) + sf : std::string(phr_("표본 화면")), CY, K); }
        text(c, 156, 278, phr_("값을 바꾸면 이 화면에도 바로 적용됩니다."), WH, K);
        text(c, 156, 298, dkey(phr_("{○}:확정   {×}:바꾸기 전 값으로 되돌림")), WH, K);
        char wb[96]; if (wmult) snprintf(wb, sizeof wb, "창 x%d (%dx%d) · 회색 = 모니터에 안 들어감 · Alt+[ ]", wmult, 640 * wmult, 480 * wmult); else snprintf(wb, sizeof wb, "창 사용자 지정 %dx%d · 테두리로 끈 크기 · Alt+[ ]", winw, winh);
        static std::string wd; wd = wb;
        static const char * dd[] = { phr_("적용 범위: 전역은 모든 작품, 이 작품은 고른 작품만."), phr_("화면 비율: 4:3 은 원래 비율, 정수배는 또렷한 배율."), phr_("셰이더: sharp 는 또렷, crt 는 주사선과 마스크, nearest 는 원본."), phr_("모니터 곡면: crt 셰이더에서 화면 가장자리를 둥글게 보이게 합니다."), phr_("번짐: 색 어긋남·가로 번짐·밝은 곳 번짐을 함께 줍니다 (0 = 끔)."), phr_("주사선 세기: 가로줄 무늬의 진하기입니다."), phr_("마스크: 화소 배열 무늬의 진하기입니다.") };
        desc(c, 324, 44, { fit(sel < 7 ? std::string(dd[sel]) : wd, 560) });
        helpline(c, phr_("↑↓:선택, ←→:값, ○:결정, ×:취소(되돌림), △:출하 시 설정"));
    }
    void d_conf(Canvas & c);
    void d_save(Canvas & c);
    void d_extra(Canvas & c);
    void d_game(Canvas & c);
    void start_ok(Act & a) {   /* t52i (B1): 0 이어하기 · 1 처음부터 · 2 시스템 디스크만 부팅(= 부가 메뉴의 같은 항목, 호스트 A_EXTRA arg 1) */
        if (startsel == 0 && !has_save(game())) return;
        if (game() && (game()->id.empty() || !blk_cur().empty())) return;
        if (startsel >= 1 && game() && game()->need_install) { begin_install(cur); return; }
        if (startsel == 2) { a.t = A_EXTRA; a.arg = 1; a.game = cur; return; }
        a.t = A_START; a.game = cur; a.entry = sel_entry; a.resume = (startsel == 0);
    }
    void open_vals() {   /* t48a (#614 ③): 적용값 표 열기 — 호스트가 층별 출처를 채우고, 작품별 설정(settings.ini [id])의 전역과 다른 값은 여기서 덧붙인다 */
        vals.clear(); vals_top = 0;
        if (cur >= 0 && cur < (int)games.size() && env.values) env.values(cur, vals);
        if (cur >= 0 && cur < MAXG && cur < (int)games.size() && ovm[cur]) {
            const Settings t0 = overlay(g, cur); const Settings & g0 = g; const std::string & id = games[cur].id;
#define VSR(f) if (t0.f != g0.f) { ValRow r; r.key = std::string("설정:" #f); r.val = std::to_string((int)t0.f); r.src = "settings.ini [" + id + "]"; r.cov = std::to_string((int)g0.f) + " (전역)"; vals.push_back(r); }
            VSR(ratio) VSR(shader) VSR(curve) VSR(bleed) VSR(glow) VSR(scan) VSR(mask) VSR(bgm) VSR(se) VSR(device) VSR(pad) VSR(bmult) VSR(bmode) VSR(boot) VSR(bosd) VSR(bsound) VSR(kbbtn) VSR(kbshader) VSR(ovbtn)
#undef VSR
        }
        sel = 0; scr = S_VALS;
    }
    void d_vals(Canvas & c);
    void d_help(Canvas & c);
    void d_keys(Canvas & c);

    /* ================================================================ t39: 새 화면 입력 */
    bool new_scr() const { switch (scr) { case S_SORT: case S_ENT: case S_ENTMENU: case S_TPL: case S_EDIT: case S_FILE: case S_KEYSEQ: case S_STEP: case S_SWAPED: case S_FONT: case S_OSK: case S_NAME: case S_YN: case S_HDDASK: case S_LANG: case S_VALS: case S_PAD: case S_PADKEY: case S_HOT: case S_EXT: case S_KBPICK: case S_SET: case S_GKB: case S_SHARE: case S_SHIMP: case S_SHPREV: case S_SHIM: case S_EXEPICK: return true; default: return false; } }
    static std::string parent_of(const std::string & p) { size_t d = p.find_last_of("\\/"); return d == std::string::npos || d == 0 ? std::string() : p.substr(0, d); }
    static std::vector<std::string> split_commas(const std::string & s) { std::vector<std::string> r; size_t p = 0; while (p <= s.size() && !s.empty()) { size_t e = s.find(',', p); if (e == std::string::npos) e = s.size(); if (e > p) r.push_back(s.substr(p, e - p)); p = e + 1; } return r; }
    static std::string join_commas(const std::vector<std::string> & v) { std::string o; for (auto & x : v) { if (!o.empty()) o += ","; o += x; } return o; }
    void yn_open(Scr ret, int act, const std::string & a0, const std::string & a1 = std::string(), const std::string & a2 = std::string(), int arg = 0) { yn_msg[0] = a0; yn_msg[1] = a1; yn_msg[2] = a2; yn_act = act; yn_ret = ret; ynsel = 1; yn_arg = arg; scr = S_YN; }
    std::string default_label(int g) const { return games[g].lang == "jp" ? std::string(phr_("원판 일본어")) : games[g].lang == "ko" ? std::string(phr_("한글판")) : std::string(phr_("기본")); }
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
        if (t < 8) { kv = ent::tpl_kv(t); std::vector<std::string> fl; files_of(g, fl); ent::autofill(kv, fl); nm = kv["label"]; if (t == 6 && games[g].lang == "ko") nm = phr_("원판 일본어"); }
        else { resolved_into(g, kv); nm = default_label(g) + phr_(" 복사"); }
        begin_new_from_kv(g, kv, nm);
    }
    bool save_ed(int * out_index = NULL) {
        if (ed_name.empty()) { msg = phr_("이름이 비어 있습니다."); return false; }
        std::string nm = ed_name; if (enew || nm != en_orig) nm = ent::unique_name(nm, ents_used(eg, en_orig));
        ed["label"] = nm; ed_name = nm; std::string setdef = ed_setdef ? nm : std::string();
        if (setdef.empty() && games[eg].defent.empty()) { bool anyreal = false; for (auto & e : games[eg].ents) if (!e.pseudo && e.vgi < 0 && !e.hidden) anyreal = true; if (!anyreal) setdef = nm; }   /* 첫 항목은 자동으로 기본 */
        if (!enew && nm != en_orig && games[eg].defent == en_orig) setdef = nm;
        bool ok = env.save && env.save(eg, nm, ed, setdef);
        if (ok && !enew && nm != en_orig && env.del) env.del(eg, en_orig, false);
        if (ok) { if (env.reload) env.reload(eg); ed0 = ed; ed_name0 = nm; en_orig = nm; enew = false; msg = phr_("저장했습니다: ") + nm; dirty = true; } else msg = phr_("저장하지 못했습니다.");
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
        fl_roots.push_back(std::make_pair(std::string(phr_("이 게임 폴더")), imgdir_of(eg))); std::string par = parent_of(games[eg].dir); if (!par.empty()) fl_roots.push_back(std::make_pair(std::string(phr_("작품 폴더들")), par));
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
            for (auto & f : fonts) if (f.ok && f.group == fnt_need) fnt_opts.push_back(f.name);   /* t52c: 이 작품의 묶음 글꼴을 먼저, 그다음 다른 묶음·전용 글꼴도 고를 수 있다(전용 글꼴 작품용) */
            for (auto & f : fonts) if (f.ok && f.group != fnt_need) fnt_opts.push_back(f.name);
        } else { std::string grp = mode == 0 ? "ks2350" : "jis"; fnt_need = grp; for (auto & f : fonts) if (f.ok && f.group == grp) fnt_opts.push_back(f.name); for (auto & f : fonts) if (f.ok && f.group != grp) fnt_opts.push_back(f.name); }   /* t52c: 다른 묶음도 고를 수 있다 */
        sel = 0; std::string cv = fnt_cur(); for (size_t i = 0; i < fnt_opts.size(); i++) if (fnt_opts[i] == cv) sel = (int)i; scr = S_FONT;
    }
    int fnt_prev_sel = 0;
    void osk_open(int target, const std::string & init, Scr ret, bool hangul = false) { osk_target = target; osk_buf = init; osk_ret = ret; osk_r = 0; osk_c = 0; osk_upper = false; ime_comp.clear(); osk_hg = hangul && (target == 0 || target == 2 || target == 3); osk_shift = false; hc = hg::Composer(); hc.buf = init; hc.han = true; scr = S_OSK; }   /* t48b: hangul = 이름 칸의 한글 키보드 */
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
        else if (osk_target == 3) { std::string t = osk_buf; while (!t.empty() && (unsigned char)t.back() <= 0x20) t.pop_back(); size_t b = 0; while (b < t.size() && (unsigned char)t[b] <= 0x20) b++; t = t.substr(b); if (!t.empty() && !nl_name.empty()) { for (auto & e : games[entg].ents) if (e.name == nl_name) { ent::Kv kv = e.kv; kv["next_label"] = t; std::string nm = e.name; bool ok = env.save && env.save(entg, nm, kv, std::string()); if (ok && env.reload) env.reload(entg); dirty = true; msg = ok ? phr_("메뉴 이름을 바꿨습니다.") : phr_("저장하지 못했습니다."); break; } } }
        else if (osk_target == 4) { std::string t = osk_buf; while (!t.empty() && (unsigned char)t.back() <= 0x20) t.pop_back(); size_t b = 0; while (b < t.size() && (unsigned char)t[b] <= 0x20) b++; t = t.substr(b); kv_set("dosargs", t); sel = 13; }   /* 2b: 실행 인자 한 줄 */
        else if (osk_target == 2) { std::string t = osk_buf; while (!t.empty() && (unsigned char)t.back() <= 0x20) t.pop_back(); size_t b = 0; while (b < t.size() && (unsigned char)t[b] <= 0x20) b++; t = t.substr(b); std::string t2; for (char ch : t) if ((unsigned char)ch >= 0x20) t2 += ch; if (!t2.empty() && entg >= 0 && entg < (int)games.size()) { pend.t = A_SETTITLE; pend.game = entg; pend.entry = t2; dirty = true; } }
        else if (osk_target == 1 && stepi >= 0 && stepi < (int)ks.size()) { ks[stepi].val = osk_buf.empty() ? std::string("enter") : osk_buf; if (osk_buf.empty()) ks[stepi].type = false; sel = 2; }
        scr = osk_ret;
    }
    void begin_install(int g) {
        ent::Kv kv; resolved_into(g, kv); kv["install"] = "hdd_new"; if (!kv.count("hdd_mb")) kv["hdd_mb"] = "40"; kv.erase("disk_c");
        eg = g; entg = base_of(g); begin_new_from_kv(g, kv, phr_("설치")); install_start = true; hddsel = 0; scr = S_HDDASK;
    }
    void ask_install_done(int g, const std::string & entry) { eg = g; en_orig = entry; yn_open(S_MAIN, YN_INSTDONE, phr_("설치를 마쳤습니까?"), phr_("예 = 이 작품의 실행 방법을 「HDD 부팅」으로 바꿉니다."), phr_("아니오 = 그대로 두고 설치기를 다시 실행할 수 있습니다.")); }
    bool finish_install_start(Act & a) {
        std::string nm = ed_name; ed["label"] = nm; bool ok = env.save && env.save(eg, nm, ed, nm); if (ok && env.reload) env.reload(eg);
        install_start = false; if (!ok) { msg = phr_("저장하지 못했습니다."); scr = S_MAIN; return false; }
        a.t = A_START; a.game = eg; a.entry = nm; a.resume = false; scr = S_MAIN; return true;
    }
    Act yn_do() {
        Act a; int act = yn_act;
        if (act == YN_DISCARD) { leave_edit_to_ent(); msg.clear(); }
        else if (act == YN_TITLE) { a.t = A_SETTITLE; a.game = entg; a.entry.clear(); scr = S_ENT; msg = phr_("DB 제목으로 되돌렸습니다."); }
        else if (act == YN_DELETE || act == YN_RESET) {
            std::vector<int> v = vents(entg); if (yn_arg >= 0 && yn_arg < (int)v.size()) { const UiEnt & e = games[entg].ents[v[yn_arg]]; if (env.del) env.del(entg, e.name, act == YN_RESET); if (env.reload) env.reload(entg); dirty = true; }
            scr = S_ENT; sel = 0; msg = act == YN_RESET ? phr_("DB 기본값으로 되돌렸습니다.") : phr_("삭제(숨김)했습니다.");
        }
        else if (act == YN_INDIVRESET || act == YN_INDIVALL) {   /* #587: 개별 설정 초기화 — 값(줄 비트)과 작품별 패드 배정을 지우고 전역을 따른다 */
            for (int gi = (act == YN_INDIVALL ? 0 : cur); gi < (act == YN_INDIVALL ? (int)games.size() : cur + 1); gi++) { if (gi < 0 || gi >= (int)games.size()) continue; if (gi < MAXG) { ov_set(gi, 0u); sync_t(gi); } if (env.pad_get && env.pad_set) { std::map<std::string, std::string> m; env.pad_get(gi, m); for (auto & kv : m) env.pad_set(gi, kv.first, std::string()); } }
            dirty = true; a.t = A_APPLY; scr = yn_ret;
        }
        else if (act == YN_PREDEL) { if (yn_arg >= PRE_NB && yn_arg < PRE_NB + (int)presets.size()) { presets.erase(presets.begin() + (yn_arg - PRE_NB)); pre_i = clampi(pre_i, 0, pre_total() - 1); dirty = true; } scr = yn_ret; }   /* 빌드 6: 내 프리셋 삭제(지금 화면 값은 그대로) */
        else if (act == YN_MAINMENU) { a.t = A_MAINMENU; scr = S_GAME; }   /* t52e (#131 ①) */
        else if (act == YN_DELSLOT) { a.t = A_DELSLOT; a.game = cur; a.slot = yn_arg; scr = S_SAVE; }   /* t52e (#131 ②) */
        else if (act == YN_DISKRESET) { a.t = A_EXTRA; a.arg = 6; a.game = cur; scr = S_EXTRA; }   /* t52e (#131 ②) */
        else if (act == YN_TEST) { if (env.save_test) env.save_test(eg, ed_name, ed); a.t = A_TEST; a.game = eg; a.entry = ed_name; a.arg = test_secs(); scr = S_EDIT; }
        else if (act == YN_INSTDONE) {
            int g = eg; std::string nm = en_orig; ent::Kv kv; for (auto & e : games[g].ents) if (e.name == nm) kv = e.kv;
            kv.erase("install"); kv.erase("hdd_mb"); kv["boot"] = "C"; kv.erase("disk_a"); std::string n2 = ent::unique_name(phr_("HDD 부팅"), ents_used(g)); kv["label"] = n2;
            if (env.save && env.save(g, n2, kv, n2)) { if (env.reload) env.reload(g); dirty = true; } scr = S_MAIN;
        }
        return a;
    }
    Act key2(UKey k) {
        Act a; if (k == UK_NONE) return a;
        switch (scr) {
        case S_SORT: k_sort(k, a); break;
        case S_ENT: k_ent(k, a); break;
        case S_ENTMENU: k_entmenu(k, a); break;
        case S_TPL: k_tpl(k, a); break;
        case S_EDIT: k_edit(k, a); break;
        case S_FILE: k_file(k, a); break;
        case S_KEYSEQ: k_keyseq(k, a); break;
        case S_STEP: k_step(k, a); break;
        case S_SWAPED: k_swaped(k, a); break;
        case S_EXEPICK: k_exepick(k, a); break;
        case S_VALS: k_vals(k, a); break;   /* t48a (#614 ③) */
        case S_FONT: k_font(k, a); break;
        case S_PAD: k_pad(k, a); break;   /* t48e (#646): 조작 설정 */
        case S_PADKEY: k_padkey(k, a); break;
        case S_HOT: k_hot(k, a); break;
        case S_EXT: k_ext(k, a); break;
        case S_KBPICK: k_kbpick(k, a); break;
        case S_SET: k_set(k, a); break;
        case S_GKB: k_gkb(k, a); break;
        case S_SHARE: k_share(k, a); break;
        case S_SHIMP: k_shimp(k, a); break;
        case S_SHPREV: k_shprev(k, a); break;
        case S_SHIM: k_shim(k, a); break;
        case S_OSK: k_osk(k, a); break;
        case S_NAME: k_name(k, a); break;
        case S_YN: k_yn(k, a); break;
        case S_LANG: k_lang(k, a); break;
        case S_HDDASK: k_hddask(k, a); break;
        default: break;
        }
        return a;
    }
    /* ---- 입력 */
    struct Preset { std::string name; Settings st; };   /* 빌드 6: 화면 프리셋 = 비율·필터·주사선·마스크·RGB 번짐·빛 번짐·곡면 한 묶음 */
    std::vector<Preset> presets; int pre_i = 0;   /* pre_i: 프리셋 줄에서 마지막으로 고른 칸 */
    enum { PRE_NB = 3, PRE_MAX = 12 };   /* 기본 제공 3 · 내 프리셋 최대 12 · 맨 끝에 「지금 값을 저장」 칸 */
    static Settings pre_builtin(int i) { Settings s; if (i == 1) { s.scan = 3; s.mask = 3; s.bleed = 2; s.glow = 2; s.curve = 2; } else if (i == 2) { s.scan = 6; s.mask = 6; s.bleed = 4; s.glow = 4; s.curve = 4; } return s; }
    static bool pre_same(const Settings & a, const Settings & b) { return a.ratio == b.ratio && (a.shader == 2) == (b.shader == 2) && a.curve == b.curve && a.bleed == b.bleed && a.glow == b.glow && a.scan == b.scan && a.mask == b.mask; }
    static void pre_copy(Settings & d, const Settings & p) { d.ratio = p.ratio < 0 ? 0 : p.ratio > 2 ? 2 : p.ratio; d.shader = p.shader == 2 ? 2 : 0; d.curve = p.curve; d.bleed = p.bleed; d.glow = p.glow; d.scan = p.scan; d.mask = p.mask; }
    int pre_total() const { return PRE_NB + (int)presets.size() + 1; }
    Settings pre_get(int i) const { return i < PRE_NB ? pre_builtin(i) : presets[(size_t)(i - PRE_NB)].st; }
    int pre_match() { const Settings e = eff(); for (int i = 0; i < PRE_NB + (int)presets.size(); i++) if (pre_same(e, pre_get(i))) return i; return -1; }
    int pre_pos() { const int nb = PRE_NB + (int)presets.size(); if (pre_i >= 0 && pre_i <= nb) { if (pre_i >= nb) return pre_i; if (pre_same(eff(), pre_get(pre_i))) return pre_i; } return pre_match(); }   /* 지금 줄이 가리키는 칸(-1 = 직접 조정) */
    std::string pre_name(int i) const { if (i < PRE_NB) { static const char * const KN[3] = { "preset_b0", "preset_b1", "preset_b2" }; return std::string(phr_(KN[i])); } if (i < PRE_NB + (int)presets.size()) return presets[(size_t)(i - PRE_NB)].name; return std::string(phr_("preset_new")); }
    void pre_add(const Settings & s) { if ((int)presets.size() >= PRE_MAX) return; std::string nm; for (int n = 1; n < 100; n++) { nm = std::string(phr_("preset_user")) + " " + std::to_string(n); bool dup = false; for (size_t q = 0; q < presets.size(); q++) if (presets[q].name == nm) dup = true; if (!dup) break; } Preset p; p.name = nm; pre_copy(p.st, s); presets.push_back(p); pre_i = PRE_NB + (int)presets.size() - 1; dirty = true; }
    int nitems() const {
        switch (scr) { case S_MAIN: return 7; case S_START: return 3; case S_SWAP: return 2; case S_SWAPI: return (game() ? (int)game()->fds.size() : 1); case S_ABOUT: return 1; case S_PICK: return (int)vis.size() + 1; case S_ADD: return 6; case S_SCREEN: return (int)rowids(S_SCREEN).size(); case S_CONF: return (int)rowids(S_CONF).size(); case S_SAVE: return 8; case S_SET: return 5; case S_SHIM: return 5; case S_GKB: return 5; case S_EXTRA: return 9; case S_GAME: return 8 + (stage_next_label.empty() ? 0 : 1); case S_KEYS: return 9; case S_HELP: return 1; default: return 1; }
    }
    int & curSel() { if (scr == S_MAIN) return mainsel; if (scr == S_PICK) return pick; if (scr == S_SAVE) return slotsel; if (scr == S_START) return startsel; if (scr == S_SWAP) return swapdrv; if (scr == S_SWAPI) return swapsel; return sel; }
    void go(Scr s, int selv = 0) { scr = s; sel = selv; }
    void enter_conf_cfg() { cb_g = g; cb_cur = cur; cb_has_t = false; if (cur >= 0 && cur < MAXG) { cb_has_t = has_t[cur]; cb_t = t[cur]; } cb_scope = scope; cb_ovm = (cur >= 0 && cur < MAXG) ? ovm[cur] : 0u; cb_fkr = font_kr; cb_fjp = font_jp; cb_has = true; }
    static void conf_fields(Settings & d, const Settings & s) { d.bgm = s.bgm; d.se = s.se; d.device = s.device; d.bmult = s.bmult; d.bmode = s.bmode; d.bsound = s.bsound; d.boot = s.boot; d.bosd = s.bosd; d.ovbtn = s.ovbtn; }   /* 조작 설정(pad)은 하위 화면 [S_PAD] 몫이라 복원하지 않는다 */
    bool conf_restore() { if (!cb_has) return false; cb_has = false; conf_fields(g, cb_g); if (cb_cur >= 0 && cb_cur < MAXG) { if (cb_has_t) { has_t[cb_cur] = true; conf_fields(t[cb_cur], cb_t); } else has_t[cb_cur] = false; } if (cb_cur >= 0 && cb_cur < MAXG) { ov_set(cb_cur, cb_ovm); sync_t(cb_cur); } scope = cb_scope; font_kr = cb_fkr; font_jp = cb_fjp; return true; }
    void enter_screen_cfg() { pre_i = 0; bk_cur = cur; bk_ovm = (cur >= 0 && cur < MAXG) ? ovm[cur] : 0u; backup = eff(); backup_has = true; bk_wmult = wmult; bk_winw = winw; bk_winh = winh; }
    static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

    void k_quit(UKey k, Act & a);
    void k_main(UKey k, Act & a);
    void k_pick(UKey k, Act & a);
    void k_start(UKey k, Act & a);
    void k_extra(UKey k, Act & a);
    void k_about(UKey k, Act & a);
    void k_lang(UKey k, Act & a);
    void k_sort(UKey k, Act & a);
    void k_swap(UKey k, Act & a);
    void k_swapi(UKey k, Act & a);
    void k_add(UKey k, Act & a);
    void k_vals(UKey k, Act & a);
    void k_pad(UKey k, Act & a);
    void k_padkey(UKey k, Act & a);
    void k_font(UKey k, Act & a);
    void k_osk(UKey k, Act & a);
    Act k_screen_conf(UKey k, Act & a);
    void k_save(UKey k, Act & a);
    void k_game(UKey k, Act & a);
    void k_keys(UKey k, Act & a);
    void k_help(UKey k, Act & a);
    void k_ent(UKey k, Act & a);
    void k_entmenu(UKey k, Act & a);
    void k_tpl(UKey k, Act & a);
    void k_edit(UKey k, Act & a);
    void k_file(UKey k, Act & a);
    void k_keyseq(UKey k, Act & a);
    void k_step(UKey k, Act & a);
    void k_swaped(UKey k, Act & a);
    void k_name(UKey k, Act & a);
    void k_yn(UKey k, Act & a);
    void k_hddask(UKey k, Act & a);
    Act key(UKey k);
};
#include "ui/screen_table.h"   /* S8a: draw/key dispatch and the home-flow screens */

static void settings_save(const Ui & u, const std::string & path) {
    FILE * f = fopen(path.c_str(), "wb"); if (!f) return;
    fprintf(f, "; " KH_FULLNAME " 설정 -- [global] 전역, [작품id] 작품별 덮어쓰기(작품별 > 전역)\n[global]\n"); ini_put(f, u.g); fprintf(f, "all_global=%d\n", u.all_global ? 1 : 0);
    fprintf(f, "lastgame=%d\nwmult=%d\nwinw=%d\nwinh=%d\ngamefolder=%s\nlang=%d\n", u.cur, u.wmult, u.winw, u.winh, u.gamefolder.c_str(), u.lang);
    fprintf(f, "lastgame_id=%s\nlastentry=%s\nsort=%s\nsortfav=%d\ntab=%s\nfont_kr=%s\nfont_jp=%s\n", (u.cur >= 0 && u.cur < (int)u.games.size()) ? u.games[u.cur].id.c_str() : "", u.sel_entry.c_str(), Ui::sort_id(u.sortm), u.sortfav ? 1 : 0, Ui::tab_id(u.tab), u.font_kr.c_str(), u.font_jp.c_str());
    for (auto & kv : u.gpad) if (!kv.second.empty()) fprintf(f, "%s=%s\n", kv.first.c_str(), kv.second.c_str());   /* t48e (#646): 전역 패드 배정 btn_* (표준 버튼 이름) */
    fprintf(f, "[last]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].last_run > 0) fprintf(f, "%s=%lld\n", u.games[i].id.c_str(), u.games[i].last_run);
    fprintf(f, "[fav]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].fav) fprintf(f, "%s=1\n", u.games[i].id.c_str());
    fprintf(f, "[order]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].order_n > 0) fprintf(f, "%s=%d\n", u.games[i].id.c_str(), u.games[i].order_n);
    fprintf(f, "[play]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].play_s > 0) fprintf(f, "%s=%d\n", u.games[i].id.c_str(), u.games[i].play_s);
    for (size_t i = 0; i < u.games.size() && i < (size_t)Ui::MAXG; i++) if (u.ovm[i]) { fprintf(f, "[%s]\nindiv=1\nfx=1\n", u.games[i].id.c_str()); ini_put_masked(f, u.t[i], u.ovm[i]); }   /* #587: 바꾼 줄만 */
    for (size_t i = 0; i < u.presets.size(); i++) { const Settings & p = u.presets[i].st; fprintf(f, "[preset.%s]\nfx=1\nratio=%d\nshader=%d\ncurve=%d\nbleed=%d\nglow=%d\nscan=%d\nmask=%d\n", u.presets[i].name.c_str(), p.ratio, p.shader == 2 ? 2 : 0, p.curve, p.bleed, p.glow, p.scan, p.mask); }   /* 빌드 6: 내 프리셋 */
    fclose(f);
}
static void settings_load(Ui & u, const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) { u.fit_tab_to_cur(); u.rebuild_vis(u.cur); return; }   /* t39ui2: 설정 파일이 없어도 목록을 만든다 */
    std::vector<unsigned> seen((size_t)Ui::MAXG, 0u); std::vector<char> indiv((size_t)Ui::MAXG, 0), fullold((size_t)Ui::MAXG, 0); for (int i0 = 0; i0 < (int)Ui::MAXG; i0++) u.ov_set(i0, 0u); u.presets.clear();   /* #587 · 빌드 6 */
    char ln[2048]; Settings * pre_p = NULL; Settings * cur = NULL; bool in_play = false; int in_sec = 0; std::string last_id; bool has_id = false;   /* in_sec: 1 last · 2 fav · 3 order */
    while (fgets(ln, sizeof ln, f)) {
        std::string s = ln; while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (s.empty() || s[0] == ';' || s[0] == '#') continue;
        if (s[0] == '[') {
            std::string id = s.substr(1, s.size() - 2); if (cur) fx_finish(*cur); cur = NULL; in_play = (id == "play"); in_sec = id == "last" ? 1 : id == "fav" ? 2 : id == "order" ? 3 : 0; if (in_sec) continue;
            pre_p = NULL; if (id.compare(0, 7, "preset.") == 0) { std::string pn = id.substr(7); bool dup = pn.empty(); for (size_t q = 0; q < u.presets.size(); q++) if (u.presets[q].name == pn) dup = true; if (!dup && (int)u.presets.size() < Ui::PRE_MAX) { Ui::Preset np; np.name = pn; u.presets.push_back(np); pre_p = &u.presets.back().st; } continue; }   /* 빌드 6: [preset.이름] — 빈 이름·중복·13번째부터는 버린다 */
            if (id == "global") cur = &u.g; else for (size_t i = 0; i < u.games.size() && i < (size_t)Ui::MAXG; i++) if (u.games[i].id == id) { u.has_t[i] = true; u.t[i] = u.g; cur = &u.t[i]; }
            if (cur) fx_begin(*cur);   /* t48c: 구역마다 「옛 형식으로 가정」 → fx=1 줄을 만나면 새 형식 */
            continue;
        }
        if (in_sec) { size_t e2 = s.find('='); if (e2 != std::string::npos) { std::string id = s.substr(0, e2); long long v = atoll(s.c_str() + e2 + 1); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].id == id) { if (in_sec == 1) u.games[i].last_run = v; else if (in_sec == 2) u.games[i].fav = v != 0; else u.games[i].order_n = (int)v; } } continue; }
        if (in_play) { size_t e2 = s.find('='); if (e2 != std::string::npos) { std::string id = s.substr(0, e2); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].id == id) u.games[i].play_s = atoi(s.c_str() + e2 + 1); } continue; }
        if (pre_p) { size_t ep = s.find('='); if (ep != std::string::npos) ini_set(*pre_p, s.substr(0, ep), atoi(s.c_str() + ep + 1)); continue; }   /* 빌드 6 */
        if (cur && cur != &u.g && s.compare(0, 14, "follow_global=") == 0) { size_t ix = (size_t)(cur - u.t); if (ix < (size_t)Ui::MAXG) fullold[ix] = atoi(s.c_str() + 14) == 0; continue; }   /* #587: 옛 파일 — follow_global=0 이면 구역 값 전부 개별 값, 1 이면 없음 */
        if (cur && cur != &u.g && s.compare(0, 6, "indiv=") == 0) { size_t ix = (size_t)(cur - u.t); if (ix < (size_t)Ui::MAXG) indiv[ix] = 1; continue; }   /* t147d: 없으면 따름(기본) — 옛 통째 복사 구역도 값은 보관만 */
        if (cur == &u.g && s.compare(0, 11, "all_global=") == 0) { u.all_global = atoi(s.c_str() + 11) != 0; continue; }
        if (cur == &u.g && s.compare(0, 6, "wmult=") == 0) { u.wmult = atoi(s.c_str() + 6); continue; }
        if (cur == &u.g && s.compare(0, 5, "winw=") == 0) { u.winw = atoi(s.c_str() + 5); continue; }
        if (cur == &u.g && s.compare(0, 5, "winh=") == 0) { u.winh = atoi(s.c_str() + 5); continue; }
        if (cur == &u.g && s.compare(0, 11, "gamefolder=") == 0) { u.gamefolder = s.substr(11); continue; }
        if (cur == &u.g && s.compare(0, 12, "lastgame_id=") == 0) { last_id = s.substr(12); has_id = true; for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].id == last_id) { u.cur = (int)i; u.pick = (int)i; } continue; }
        if (cur == &u.g && s.compare(0, 10, "lastentry=") == 0) { u.sel_entry = s.substr(10); continue; }
        if (cur == &u.g && s.compare(0, 5, "sort=") == 0) { u.sortm = Ui::sort_from(s.substr(5)); continue; }
        if (cur == &u.g && s.compare(0, 8, "sortfav=") == 0) { u.sortfav = atoi(s.c_str() + 8) != 0; continue; }
        if (cur == &u.g && s.compare(0, 4, "tab=") == 0) { u.tab = Ui::tab_from(s.substr(4)); continue; }
        if (cur == &u.g && s.compare(0, 5, "lang=") == 0) { int lv = atoi(s.c_str() + 5); u.lang = (lv >= 0 && lv <= 2) ? lv : 0; continue; }   /* t52g (#133) */
        if (cur == &u.g && s.compare(0, 8, "font_kr=") == 0) { u.font_kr = s.substr(8); continue; }
        if (cur == &u.g && s.compare(0, 8, "font_jp=") == 0) { u.font_jp = s.substr(8); continue; }
        if (cur == &u.g && s.compare(0, 4, "btn_") == 0) { size_t e3 = s.find('='); if (e3 != std::string::npos && e3 > 4) { std::string kk = s.substr(0, e3); for (auto & ch : kk) ch = (char)tolower((unsigned char)ch); u.gpad[kk] = s.substr(e3 + 1); } continue; }   /* t48e (#646) */
        if (s.compare(0, 9, "lastgame=") == 0) { if (has_id) continue; int v = atoi(s.c_str() + 9); if (v >= 0 && v < (int)u.games.size()) { u.cur = v; u.pick = v; } continue; }
        size_t e = s.find('='); if (e == std::string::npos || !cur) continue;
        ini_set(*cur, s.substr(0, e), atoi(s.c_str() + e + 1));
        if (cur != &u.g) { size_t ix = (size_t)(cur - u.t); if (ix < (size_t)Ui::MAXG) seen[ix] |= Ui::fbit_of(s.substr(0, e)); }
    }
    fclose(f);
    for (int i1 = 0; i1 < (int)Ui::MAXG; i1++) if (u.has_t[i1]) { unsigned m = indiv[i1] ? seen[i1] : fullold[i1] ? Ui::fbits_all() : 0u; u.ov_set(i1, m); }   /* #587: 읽을 때 해석 — 다음 저장 때 새 모양 */
    if (u.all_global) { for (int i1 = 0; i1 < (int)Ui::MAXG; i1++) u.ov_set(i1, 0u); u.all_global = false; }   /* 옛 「전역 일괄 적용」 = 모든 작품의 개별 값 없음 */
    u.fit_tab_to_cur(); u.rebuild_vis(u.cur);   /* t39: 탭·정렬을 적용한 목록 만들기 */   if (cur) fx_finish(*cur);   /* t48c: 마지막 구역의 옛 형식 변환 */
}

} /* namespace ui */
