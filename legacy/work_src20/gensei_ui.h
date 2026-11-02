/* gensei_ui.h -- r012 UI 화면 상태기계. gensei_ov.h 위에서 그린다. 입력은 추상 키(UK_*)로 받는다.
 * 호스트가 UiGame 표(제목·디스크명·THMB·슬롯 정보)를 채워 넣고, ui_key() 의 반환 Act 를 처리한다. */
#pragma once
#include "gensei_ov.h"
#include <stdlib.h>
#include <algorithm>
#include <map>
#include <time.h>

namespace ui {
using namespace ov;

enum UKey { UK_NONE, UK_UP, UK_DOWN, UK_LEFT, UK_RIGHT, UK_OK, UK_CANCEL, UK_TRI, UK_SQ, UK_SELECT, UK_ANY };
enum ActT { A_NONE, A_START, A_LOAD, A_SAVE, A_RESUME, A_MAINMENU, A_QUIT, A_SENDKEY, A_DISKSWAP, A_APPLY, A_DELSLOT, A_EXTRA, A_ADD, A_WINMULT };   /* r017: A_ADD(arg 0 파일 1 폴더 2 게임폴더 지정 3 다시 검색 4 해제) · A_WINMULT(arg=배율, 0=사용자 지정) */
struct Act { ActT t = A_NONE; int game = -1, slot = 0, arg = 0; bool resume = false; };

/* ---- 설정 (전역 + 작품별 덮어쓰기) */
struct Settings {
    int ratio = 0;      /* 0 4:3  1 16:10  2 정수배 */
    int shader = 0;     /* 0 sharp 1 crt 2 nearest */
    int curve = 4;      /* 0..10 → 곡면 0.00..0.10 */
    int scan = 5;       /* 0..10 */
    int mask = 1;       /* 0 꺼짐 1 약 2 중 3 강 */
    int bgm = 8;        /* 0..8 */
    int se = 8;         /* 0..8 */
    int device = 0;     /* 0 기본 장치 */
    int pad = 0;        /* 0 일본식 ○결정 */
    int bmult = 1;      /* 0..4 = x2 x3 x4 x6 x8 */
    int bmode = 2;      /* 0 홀드 1 토글 2 둘 다 */
    int boot = 0;       /* 0 켬 1 끔 */
    int bosd = 1;       /* 배속 표시(오른쪽 위 ▶▶) 0 끔 1 켬 */
    int bsound = 2;     /* r017 배속 중 소리: 0 끔 1 빠르게(음높이 상승) 2 음높이 유지 */
    float curve_f() const { return curve * 0.01f; }
    float scan_f() const { return scan / 10.0f; }
    float mask_f() const { static const float m[4] = { 0.f, 0.15f, 0.3f, 0.5f }; return m[mask & 3]; }
    int boost_x() const { static const int x[5] = { 2, 3, 4, 6, 8 }; return x[bmult < 0 ? 0 : bmult > 4 ? 4 : bmult]; }
    int bs_gain256() const { return boost_x() >= 3 ? 205 : 256; }   /* x3 이상은 조금 낮춤(-2 dB) */
    float se_gain() const { return se / 8.0f; }
    float bgm_gain() const { return bgm / 8.0f; }
};
inline void apply_ov(Settings & s, const std::map<std::string, std::string> & m) {   /* 작품 프로필(DB·game.ini [override]) 값을 전역 위에 얹는다 */
    auto cl = [](int v, int a, int b) { return v < a ? a : v > b ? b : v; };
    for (auto & kv : m) {
        int v = atoi(kv.second.c_str()); const std::string & k = kv.first;
        if (k == "ratio") s.ratio = cl(v, 0, 2); else if (k == "shader") s.shader = cl(v, 0, 2); else if (k == "curve") s.curve = cl(v, 0, 10);
        else if (k == "scan") s.scan = cl(v, 0, 10); else if (k == "mask") s.mask = cl(v, 0, 3); else if (k == "bgm") s.bgm = cl(v, 0, 8);
        else if (k == "se") s.se = cl(v, 0, 8); else if (k == "pad") s.pad = cl(v, 0, 3); else if (k == "boost") { static const int x[5] = { 2, 3, 4, 6, 8 }; int b = 1; for (int i = 0; i < 5; i++) if (x[i] == v) b = i; s.bmult = b; }
        else if (k == "boostmode") s.bmode = cl(v, 0, 2); else if (k == "bosd") s.bosd = cl(v, 0, 1); else if (k == "bsound") s.bsound = cl(v, 0, 2);
    }
}
struct SlotInfo { bool used = false; std::string stamp, play; int bytes = 0; std::vector<uint32_t> th; int tw_ = 0, th_ = 0; };
struct UiGame {
    std::string id, title, boot_note; std::string disk[2];
    std::string company, year, genre, patch, devices, disknote;   /* gamedb 항목(인포메이션) */
    int play_s = 0; bool has_demo = false;   /* 플레이 시간(초) · 오프닝(데모) 디스크 유무 */
    std::vector<std::string> fds; std::string cur_fd[2];   /* 디스크 교체 목록(표시 이름) · 지금 A/B 에 들어 있는 것 */ std::map<std::string, std::string> ov; std::string src;
    std::vector<uint32_t> thmb; int tw_ = 0, th_ = 0;  /* 마지막 저장 THMB (0x00RRGGBB) */
    SlotInfo slot[8]; int lastslot = -1;
    /* r017: 판정·출처·실행 불가 */
    char grade = '-'; std::string judge, metasrc, block, jshort; bool autoreg = false;
    std::string more;   /* r019: 함께 있는 이미지(같은 작품의 다른 이미지) 목록 */
    std::string orig, dbhit;   /* r018: 원판 제목(변형 판정) · DB 일치한 작품 이름 */
};

enum Scr { S_MAIN, S_PICK, S_SCREEN, S_CONF, S_SAVE, S_EXTRA, S_QUIT, S_GAME, S_KEYS, S_HELP, S_START, S_SWAP, S_SWAPI, S_ABOUT, S_ADD };

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
    std::vector<UiGame> games; bool in_game = false; std::string build = "r012";
    bool dirty = false;
    int ts_ms = 0;             /* 애니메이션용 */
    /* r017: 창 배율(전역) · 작품 추가 */
    int wmult = 0, wmax = 3, winw = 0, winh = 0; int bk_wmult = 0, bk_winw = 0, bk_winh = 0;   /* wmult 0 = 사용자 지정(winw x winh) · wmax = 모니터에 들어가는 최대 배율 */
    std::string gamefolder; std::vector<std::string> add_log; int addsel = 0;
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
        { std::vector<std::string> tl = title_lines(gm->title, w - 48, 2); for (auto & l : tl) { text(c, X, yy, l, gm->block.empty() ? YL : GY, K); yy += 20; } yy += 2; }
        line("제작", mk_line(*gm));
        line("패치", gm->patch.empty() ? std::string("-") : gm->patch);
        line("디스크", gm->disknote.empty() ? std::string("-") : gm->disknote);
        line("장치", gm->devices.empty() ? std::string("특별한 장치 불필요") : gm->devices);
        { static const struct { const char * k; const char * n; } T[] = { { "ratio", "비율" }, { "shader", "셰이더" }, { "curve", "곡면" }, { "scan", "주사선" }, { "mask", "마스크" }, { "bgm", "BGM" }, { "se", "효과음" }, { "pad", "패드" }, { "boost", "배속" }, { "boostmode", "배속방식" } };
          std::string o; for (auto & t : T) if (gm->ov.count(t.k)) { if (!o.empty()) o += ", "; o += t.n; }
          if (cur >= 0 && cur < MAXG && has_t[cur]) { if (!o.empty()) o += " + "; o += "내 설정"; }
          line("설정", o.empty() ? std::string("전역과 같음") : o); }
        line("판정", gm->jshort.empty() ? std::string("-") : gm->jshort);
        if (!gm->block.empty()) line("사유", gm->block, OR); else line("플레이", play_str(gm->play_s));
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
            bool blk = gm && !gm->block.empty();
            text(c, bx + 24, by + 24, fit(gm ? gm->title : std::string(), 352), blk ? GY : CY, K);
            std::vector<std::string> a = { hs ? "이어하기   슬롯 " + std::to_string(gm->lastslot + 1) + "  " + gm->slot[gm->lastslot].stamp : std::string("이어하기   (저장 없음)"), "처음부터" };
            std::vector<bool> dm = { !hs || blk, blk }; items(c, bx + 30, by + 60, 340, a, startsel, 30, &dm);
            if (blk) { text(c, bx + 24, by + 126, fit("실행 불가: " + gm->block, 352), OR, K); }
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
        v.resize((size_t)maxl); v[(size_t)maxl - 1] = fit(rest, w); return v;
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
    void d_pick(Canvas & c) {
        titlebar(c, "작품 선택");
        rbox(c, 16, 24, 288, 224, WH, "작품 선택");
        int n = (int)games.size() + 1;   /* 맨 아래 「+ 작품 추가」 */
        if (pick != pick_seen) { pick_seen = pick; pick_since = ts_ms; }
        {   /* 7줄 창 + 스크롤. 글자는 상자 폭에서 자른다: 선택 안 된 줄 = 끝 …, 선택된 줄 = 마퀴 */
            const int VIS = 7, PT = 24, LX = 40, LWD = 256, AVAIL = 236; int top = pick - 3; if (top > n - VIS) top = n - VIS; if (top < 0) top = 0;
            for (int k = 0; k < VIS && top + k < n; k++) {
                int i = top + k, yy = 48 + k * PT; bool add = i >= (int)games.size(); bool d = !add && !games[i].block.empty();
                std::string full = add ? std::string("+ 작품 추가") : games[i].title;
                if (i == pick) { c.rect(LX - 8, yy - 3, LWD + 2, 22, d ? GY : WH); c.clip(LX, yy - 3, AVAIL, 22);   /* r019: 막대 안쪽에 좌우 여백을 두고 그 경계에서 자른다 */ text(c, LX - mq_off(full, AVAIL), yy, full, K, d ? GY : WH); }
                else { c.clip(LX, yy - 3, AVAIL, 22); text(c, LX, yy, fit(full, AVAIL), d ? GY : CY, K); }
                c.unclip();
            }
            if (top > 0) text(c, 284, 30, "↑", CY, K); if (top + VIS < n) text(c, 284, 228, "↓", CY, K);
        }
        rbox(c, 320, 24, 304, 224, WH, "인포메이션");
        if (pick >= (int)games.size()) {
            text(c, 344, 48, "새 작품을 등록합니다.", YL, K);
            text(c, 344, 78, "이미지 파일(zip 포함)이나 폴더를", WH, K); text(c, 344, 98, "고르면 형식과 구조를 판정해", WH, K); text(c, 344, 118, "자동으로 등록합니다.", WH, K);
            text(c, 344, 158, "게임 폴더를 지정해 두면 켤 때마다", CY, K); text(c, 344, 178, "새 작품을 찾아 자동 등록합니다.", CY, K);
            desc(c, 264, 64, { "+ 작품 추가: 이미지 파일 · 폴더 · 게임 폴더 지정", "회색 작품은 아직 실행할 수 없는 작품입니다 (사유는 인포메이션)." });
            helpline(c, "↑↓:선택, ○:작품 추가 화면, ×:취소");
            return;
        }
        const UiGame & g = games[pick]; bool blk = !g.block.empty();
        const int IX = 344, IW = 256, LB = 48;
        c.clip(IX - 4, 28, IW + 8, 216);   /* 인포메이션 상자 밖으로는 절대 나가지 않게 */
        { std::vector<std::string> tl = title_lines(g.title, IW, 2); for (size_t i = 0; i < tl.size(); i++) text(c, IX, 36 + (int)i * 20, tl[i], blk ? WH : YL, K); }   /* r019: 실행 불가여도 제목은 흰색, 사유 줄만 주황 */
        thumb(c, &g, IX, 80, 128, 80);
        { text(c, IX + 140, 82, "디스크", CY, K); std::vector<std::string> dl = clamp_lines(wrap_words(g.disknote.empty() ? std::string("-") : g.disknote, IW - 140), 3, IW - 140); for (size_t i = 0; i < dl.size(); i++) text(c, IX + 140, 100 + (int)i * 18, dl[i], WH, K); }
        int yy = 166;
        auto row = [&](const char * lab, const std::string & v, uint32_t col, int maxl) {
            text(c, IX, yy, lab, CY, K);
            std::vector<std::string> ls = clamp_lines(wrap_words(v.empty() ? std::string("-") : v, IW - LB), maxl, IW - LB);
            if (ls.empty()) ls.push_back("-");
            for (size_t i = 0; i < ls.size(); i++) text(c, IX + LB, yy + (int)i * 18, ls[i], col, K);
            yy += 18 * (int)ls.size();
        };
        row("제작", mk_line(g), WH, 1);
        row("판정", g.jshort.empty() ? std::string("-") : g.jshort, WH, 2);
        if (blk) row("사유", g.block, OR, 1);
        else { std::string st = "없음"; if (g.lastslot >= 0 && g.slot[g.lastslot].used) st = "슬롯 " + std::to_string(g.lastslot + 1) + "  " + g.slot[g.lastslot].stamp; row("저장", st, WH, 1); }
        c.unclip();
        std::vector<std::string> d;
        if (blk) { d = { g.title + "   [실행 불가]", "사유: " + g.block }; }
        else { d = { g.title + (g.lastslot >= 0 && g.slot[g.lastslot].used ? "   마지막 저장 " + g.slot[g.lastslot].stamp : std::string("   처음부터 시작합니다")), g.judge.empty() ? (g.disk[0] + (g.disk[1].empty() ? std::string() : "    " + g.disk[1])) : g.judge }; }
        for (auto & l : d) l = fit(l, 560);
        desc(c, 264, 64, d, blk ? 1 : -1);
        helpline(c, "↑↓:선택, ○:이 작품으로 정함, ×:취소, SELECT:부가");
    }
    void d_add(Canvas & c) {
        titlebar(c, "작품 추가");
        rbox(c, 16, 24, 288, 224, WH, "작품 추가");
        bool hasgf = !gamefolder.empty();
        std::vector<std::string> n = { "이미지 파일 고르기", "폴더 고르기", "게임 폴더 지정", "게임 폴더 다시 검색", "게임 폴더 해제" };
        std::vector<bool> dm = { false, false, false, !hasgf, !hasgf };
        items(c, 40, 48, 256, n, sel, 30, &dm);
        rbox(c, 320, 24, 304, 224, WH, "인포메이션");
        text(c, 344, 44, "게임 폴더", CY, K);
        text(c, 344 + 80, 44, hasgf ? fit(gamefolder, 304 - 24 - 80 - 16) : std::string("(없음)"), hasgf ? WH : GY, K);
        int yy = 72; if (add_log.empty()) text(c, 344, yy, "아직 추가한 작품이 없습니다.", GY, K);
        for (size_t i = 0; i < add_log.size() && i < 8; i++) { text(c, 344, yy, fit(add_log[i], 304 - 48), i == 0 ? YL : WH, K); yy += 20; }
        static const char * dd[] = { "고른 이미지 파일들을 한 작품으로 등록합니다 (여러 장 가능).", "폴더 안의 이미지를 찾아 등록합니다. 하위 폴더는 작품마다 따로.", "이 폴더를 켤 때마다 훑어 새 작품을 자동으로 등록합니다.", "게임 폴더를 지금 다시 훑습니다.", "게임 폴더 지정을 풀고 자동 등록을 멈춥니다 (등록된 작품은 남음)." };
        desc(c, 264, 64, { dd[sel < 5 ? sel : 0], "형식은 확장자가 아니라 파일 머리로 판정합니다." });
        helpline(c, "↑↓:선택, ○:결정, ×:돌아가기");
    }
    void d_screen(Canvas & c) {
        titlebar(c, "화면 설정");
        Settings & s = eff(); bool crt = s.shader == 1;
        rbox(c, 16, 24, 288, 184, WH, "화면 설정");
        std::vector<std::string> n = { "적용 범위", "비율", "셰이더", "곡면", "주사선 세기", "마스크", "창 배율" };
        std::vector<bool> dim = { false, false, false, !crt, !crt, !crt, false };
        items(c, 40, 44, 256, n, sel, 24, &dim);
        rbox(c, 320, 24, 304, 184, WH, "선택지");
        auto row = [&](int i) { return sel == i; };
        const char * sc[] = { "전역", "이 작품" }; choices(c, 344, 44, sc, 2, scope, WH, row(0));
        const char * rt[] = { "4:3", "16:10", "정수배" }; choices(c, 344, 68, rt, 3, s.ratio, WH, row(1));
        const char * sh[] = { "sharp", "crt", "nearest" }; choices(c, 344, 92, sh, 3, s.shader, WH, row(2));
        if (crt) {
            char b[16]; snprintf(b, sizeof b, "%.2f", s.curve_f()); barv(c, 340, 116, 232, s.curve / 10.0, "평면", "최대", WH, row(3)); text(c, 578, 116, b, WH, K);
            snprintf(b, sizeof b, "%d", s.scan * 10); barv(c, 340, 140, 232, s.scan / 10.0, "약", "강", WH, row(4)); text(c, 578, 140, b, WH, K);
            const char * mk[] = { "꺼짐", "약", "중", "강" }; choices(c, 344, 164, mk, 4, s.mask, WH, row(5));
        } else for (int i = 3; i < 6; i++) text(c, 344, 44 + i * 24, "(crt 에서만)", GY, K);
        { const char * wm[] = { "x1", "x2", "x3", "x4", "x5", "사용자 지정" }; unsigned dmk = 0; for (int i = 0; i < WMN; i++) if (i + 1 > wmax) dmk |= 1u << i;
          choicesd(c, 344, 188, wm, 6, wmult ? wmult - 1 : 5, WH, dmk); }
        rbox(c, 16, 216, 608, 100, WH, "미리보기");
        thumb(c, game(), 40, 232, 128, 80);
        const UiGame * gm = game();
        text(c, 192, 236, gm && gm->lastslot >= 0 ? gm->title + "  마지막 저장 (" + gm->slot[gm->lastslot].stamp + ")" : std::string("표본 화면"), CY, K);
        text(c, 192, 256, "값을 바꾸면 이 화면에도 바로 적용됩니다.", WH, K);
        text(c, 192, 276, "○:확정   ×:바꾸기 전 값으로 되돌림", WH, K);
        char wb[96]; if (wmult) snprintf(wb, sizeof wb, "창 x%d (%dx%d) · 회색 = 모니터에 안 들어감 · Alt+[ ]", wmult, 640 * wmult, 480 * wmult); else snprintf(wb, sizeof wb, "창 사용자 지정 %dx%d · 테두리로 끈 크기 · Alt+[ ]", winw, winh);
        static std::string wd; wd = wb;
        static const char * dd[] = { "적용 범위: 전역은 모든 작품, 이 작품은 고른 작품만.", "화면 비율: 4:3 은 원래 비율, 정수배는 또렷한 배율.", "셰이더: sharp 는 또렷, crt 는 주사선과 마스크, nearest 는 원본.", "모니터 곡면: crt 셰이더에서 화면 가장자리를 둥글게 보이게 합니다.", "주사선 세기: 가로줄 무늬의 진하기입니다.", "마스크: 화소 배열 무늬의 진하기입니다." };
        desc(c, 324, 44, { fit(sel < 6 ? std::string(dd[sel]) : wd, 560) });
        helpline(c, "↑↓:선택, ←→:값, ○:결정, ×:취소(되돌림), △:출하 시 설정");
    }
    void d_conf(Canvas & c) {
        titlebar(c, "소리/패드/배속 설정");
        Settings & s = eff();
        rbox(c, 16, 24, 288, 296, WH, "동작 환경 설정");
        std::vector<std::string> n = { "적용 범위", "BGM 음량", "효과음 음량", "소리 출력 장치", "패드 배치", "배속 배율", "배속 방식", "배속 중 소리", "부팅 시퀀스", "배속 표시" };
        items(c, 40, 44, 256, n, sel, 27);
        rbox(c, 320, 24, 304, 296, WH, "인포메이션");
        auto row = [&](int i) { return sel == i; };
        auto ry = [](int i) { return 44 + 27 * i; };
        const char * sc[] = { "전역", "이 작품" }; choices(c, 344, ry(0), sc, 2, scope, WH, row(0));
        char b[8];
        snprintf(b, sizeof b, "%d", s.bgm); barv(c, 340, ry(1), 232, s.bgm / 8.0, "OFF", "최대", WH, row(1)); text(c, 590, ry(1), b, WH, K);
        snprintf(b, sizeof b, "%d", s.se); barv(c, 340, ry(2), 232, s.se / 8.0, "OFF", "최대", WH, row(2)); text(c, 590, ry(2), b, WH, K);
        text(c, 344, ry(3), "기본 장치", row(3) ? K : WH, row(3) ? WH : K);
        text(c, 344, ry(4), "일본식 ○결정", row(4) ? K : WH, row(4) ? WH : K);
        const char * bm[] = { "x2", "x3", "x4", "x6", "x8" }; choices(c, 344, ry(5), bm, 5, s.bmult, WH, row(5));
        const char * bk[] = { "홀드", "토글", "둘 다" }; choices(c, 344, ry(6), bk, 3, s.bmode, WH, row(6));
        const char * bs[] = { "끔", "빠르게", "음높이 유지" }; choices(c, 344, ry(7), bs, 3, s.bsound, WH, row(7));
        const char * bt[] = { "켬", "끔" }; choices(c, 344, ry(8), bt, 2, s.boot, WH, row(8));
        { const char * bo[] = { "끔", "켬" }; choices(c, 344, ry(9), bo, 2, s.bosd, WH, row(9)); }
        static const char * dd[] = { "적용 범위: 전역은 모든 작품, 이 작품은 고른 작품만.", "BGM 음량입니다. 효과음과 따로 조절합니다.", "효과음 음량입니다. 시작 소리도 이 값을 따릅니다.", "소리가 나오는 장치입니다. 지금은 기본 장치만 씁니다.", "패드 배치입니다. ○가 결정, ×가 취소입니다.", "배속 배율입니다.", "홀드는 누르는 동안, 토글은 눌러 켜고 끄기, 둘 다는 둘 다.", "끔=무음 · 빠르게=음높이 상승 · 음높이 유지=높이 그대로", "시작할 때 나오는 부팅 화면을 켜고 끕니다.", "배속 중에 오른쪽 위에 ▶▶ 배율을 계속 표시합니다. 캡처에는 찍히지 않습니다." };
        desc(c, 328, 44, { fit(dd[sel], 560) });
        helpline(c, "↑↓:선택, ←→:값, ○:결정, ×:취소, △:출하 시 설정");
    }
    void d_save(Canvas & c) {
        titlebar(c, "세이브 관리");
        const UiGame * gm = game();
        rbox(c, 16, 24, 288, 296, WH, gm ? (gm->title + " 슬롯").c_str() : "슬롯");
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
        rbox(c, 16, 24, 288, 224, WH, gm ? (gm->title + " 부가").c_str() : "부가");
        std::vector<std::string> a = { "디스크 정보", "시스템 디스크만 부팅", "부팅 시퀀스 다시 보기", "조작 안내", "오프닝(데모) 보기", "정보·라이선스" }; std::vector<bool> dm = { false, false, false, false, !(gm && gm->has_demo), false }; items(c, 40, 46, 256, a, sel, 30, &dm);
        rbox(c, 320, 24, 304, 224, WH, "인포메이션");
        if (gm) { std::string l[4] = { gm->disk[0], gm->disk[1], "", gm->boot_note }; for (int i = 0; i < 4; i++) if (!l[i].empty()) text(c, 344, 44 + i * 22, l[i], i == 3 ? CY : WH, K); }
        desc(c, 264, 64, { "고른 작품의 디스크 구성과 부가 기능입니다." });
        helpline(c, "↑↓:선택, ○:결정, ×:취소");
    }
    void d_game(Canvas & c) {
        /* 배경은 호스트가 게임 화면을 깔아 둔다(draw 가 지우지 않도록 호스트가 호출 순서 관리) */
        c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, "일시정지");
        std::vector<std::string> a = { "이어하기", "저장  슬롯 " + std::to_string(slotsel + 1), "불러오기", "디스크 교체", "화면 설정", "소리/패드/배속 설정", "PC-98 키 보내기", "메인 메뉴로", "끝내기" };
        items(c, 160, 44, 330, a, sel, 29);
        c.rect(0, 376, 640, 24, K);
        helpline(c, "↑↓:선택, ○:결정, ×:닫기(이어하기)");
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

    /* ---- 입력 */
    int nitems() const {
        switch (scr) { case S_MAIN: return 7; case S_START: return 2; case S_SWAP: return 2; case S_SWAPI: return (game() ? (int)game()->fds.size() : 1); case S_ABOUT: return 1; case S_PICK: return (int)games.size() + 1; case S_ADD: return 5; case S_SCREEN: return 7; case S_CONF: return 10; case S_SAVE: return 8; case S_EXTRA: return 6; case S_GAME: return 9; case S_KEYS: return 9; case S_HELP: return 1; default: return 1; }
    }
    int & curSel() { if (scr == S_MAIN) return mainsel; if (scr == S_PICK) return pick; if (scr == S_SAVE) return slotsel; if (scr == S_START) return startsel; if (scr == S_SWAP) return swapdrv; if (scr == S_SWAPI) return swapsel; return sel; }
    void go(Scr s, int selv = 0) { scr = s; sel = selv; }
    void enter_screen_cfg() { backup = eff(); backup_has = true; bk_wmult = wmult; bk_winw = winw; bk_winh = winh; }
    static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

    Act key(UKey k) {
        Act a; if (k == UK_NONE) return a;
        if (scr == S_QUIT) {
            if (k == UK_LEFT && quitsel > 0) quitsel--; if (k == UK_RIGHT && quitsel < 2) quitsel++;
            if (k == UK_CANCEL) { if (in_game) go(S_GAME, 8); else scr = S_MAIN; }
            if (k == UK_OK) { if (quitsel == 2) { if (in_game) go(S_GAME, 8); else scr = S_MAIN; } else { a.t = A_QUIT; a.arg = quitsel == 0 ? 1 : 0; } }
            return a;
        }
        int n = nitems(), & cs = curSel(); if (n < 1) n = 1;
        if (k == UK_UP) { cs = (cs + n - 1) % n; if (scr == S_PICK && pick < (int)games.size()) cur = pick; return a; }
        if (k == UK_DOWN) { cs = (cs + 1) % n; if (scr == S_PICK && pick < (int)games.size()) cur = pick; return a; }
        switch (scr) {
        case S_MAIN:
            if (k == UK_OK) {
                static const Scr to[7] = { S_START, S_PICK, S_SCREEN, S_CONF, S_SAVE, S_EXTRA, S_QUIT };
                back = S_MAIN; if (to[mainsel] == S_SCREEN) enter_screen_cfg(); if (to[mainsel] == S_PICK) pick = cur; if (to[mainsel] == S_QUIT) quitsel = 0;
                if (to[mainsel] == S_START) { startsel = has_save(game()) ? 0 : 1; }
                go(to[mainsel], 0); if (to[mainsel] == S_SAVE) slotsel = 0;
            } else if (k == UK_CANCEL) { go(S_QUIT, 0); quitsel = 0; }
            else if (k == UK_SELECT) go(S_EXTRA, 0);
            break;
        case S_PICK:
            if (pick >= (int)games.size()) {   /* 맨 아래 「+ 작품 추가」 */
                if (k == UK_OK) go(S_ADD, 0); else if (k == UK_CANCEL) go(S_MAIN, 0);
                break;
            }
            if (k == UK_OK) { cur = pick; mainsel = 0; go(S_MAIN, 0); }
            else if (k == UK_CANCEL) go(S_MAIN, 0);
            else if (k == UK_SELECT) { cur = pick; go(S_EXTRA, 0); back = S_PICK; }
            break;
        case S_ADD:
            if (k == UK_OK) {
                if (sel >= 3 && gamefolder.empty()) break;
                a.t = A_ADD; a.arg = sel;
            } else if (k == UK_CANCEL) { go(S_PICK, 0); pick = (int)games.size(); }
            break;
        case S_SCREEN: case S_CONF: {
            bool scr_s = scr == S_SCREEN; int nrow = scr_s ? 7 : 10;
            Settings & s = (sel == 0) ? eff() : edit();
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = (k == UK_RIGHT) ? 1 : -1;
                if (scr_s && sel == 6) {   /* 창 배율(전역): 모니터에 들어가는 다음/이전 배율 */
                    int m = wm_next(d); if (m > 0) { wmult = m; a.t = A_WINMULT; a.arg = m; dirty = true; }
                    return a;
                }
                if (sel == 0) { scope = clampi(scope + d, 0, 1); if (scope == 1 && !has_t[cur]) { t[cur] = effective(cur); has_t[cur] = true; } }
                else if (scr_s) {
                    if (sel == 1) s.ratio = clampi(s.ratio + d, 0, 2);
                    if (sel == 2) s.shader = clampi(s.shader + d, 0, 2);
                    if (s.shader == 1) { if (sel == 3) s.curve = clampi(s.curve + d, 0, 10); if (sel == 4) s.scan = clampi(s.scan + d, 0, 10); if (sel == 5) s.mask = clampi(s.mask + d, 0, 3); }
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
            } else if (k == UK_TRI) { Settings def; Settings & e = eff(); if (scr_s) { e.ratio = def.ratio; e.shader = def.shader; e.curve = def.curve; e.scan = def.scan; e.mask = def.mask; } else { e.bgm = def.bgm; e.se = def.se; e.bmult = def.bmult; e.bmode = def.bmode; e.boot = def.boot; e.bosd = def.bosd; e.bsound = def.bsound; } dirty = true; a.t = A_APPLY; }
            else if (k == UK_CANCEL) { if (scr_s && backup_has) { eff() = backup; backup_has = false; a.t = A_APPLY; if (wmult != bk_wmult || winw != bk_winw || winh != bk_winh) { wmult = bk_wmult; winw = bk_winw; winh = bk_winh; a.t = A_WINMULT; a.arg = wmult; } } go(in_game ? S_GAME : S_MAIN, in_game ? (scr == S_SCREEN ? 4 : 5) : 0); if (!in_game) sel = 0; }
            else if (k == UK_OK) { backup_has = false; dirty = true; a.t = A_APPLY; go(in_game ? S_GAME : S_MAIN, in_game ? (scr == S_SCREEN ? 4 : 5) : 0); }
            break; }
        case S_SAVE:
            if (k == UK_OK) { a.t = A_LOAD; a.game = cur; a.slot = slotsel; }
            else if (k == UK_SQ && in_game) { a.t = A_SAVE; a.game = cur; a.slot = slotsel; }
            else if (k == UK_TRI) { a.t = A_DELSLOT; a.game = cur; a.slot = slotsel; }
            else if (k == UK_CANCEL) { go(in_game ? S_GAME : S_MAIN, 0); if (in_game) sel = 2; }
            break;
        case S_START:
            if (k == UK_OK) { if (startsel == 0 && !has_save(game())) break; if (game() && (game()->id.empty() || !game()->block.empty())) break; a.t = A_START; a.game = cur; a.resume = (startsel == 0); }
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
                case 7: a.t = A_MAINMENU; break;
                case 8: go(S_QUIT, 0); quitsel = 0; break;
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
    fprintf(f, "ratio=%d\nshader=%d\ncurve=%d\nscan=%d\nmask=%d\nbgm=%d\nse=%d\ndevice=%d\npad=%d\nbmult=%d\nbmode=%d\nboot=%d\nbosd=%d\nbsound=%d\n", s.ratio, s.shader, s.curve, s.scan, s.mask, s.bgm, s.se, s.device, s.pad, s.bmult, s.bmode, s.boot, s.bosd, s.bsound);
}
static bool ini_set(Settings & s, const std::string & k, int v) {
    if (k == "ratio") s.ratio = v; else if (k == "shader") s.shader = v; else if (k == "curve") s.curve = v; else if (k == "scan") s.scan = v; else if (k == "mask") s.mask = v;
    else if (k == "bgm") s.bgm = v; else if (k == "se") s.se = v; else if (k == "device") s.device = v; else if (k == "pad") s.pad = v; else if (k == "bmult") s.bmult = v; else if (k == "bmode") s.bmode = v; else if (k == "boot") s.boot = v; else if (k == "bosd") s.bosd = v; else if (k == "bsound") s.bsound = v; else return false;
    return true;
}
static void settings_save(const Ui & u, const std::string & path) {
    FILE * f = fopen(path.c_str(), "wb"); if (!f) return;
    fprintf(f, "; PC-98 PLAYER 설정 -- [global] 전역, [작품id] 작품별 덮어쓰기(작품별 > 전역)\n[global]\n"); ini_put(f, u.g);
    fprintf(f, "lastgame=%d\nwmult=%d\nwinw=%d\nwinh=%d\ngamefolder=%s\n", u.cur, u.wmult, u.winw, u.winh, u.gamefolder.c_str());
    fprintf(f, "[play]\n"); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].play_s > 0) fprintf(f, "%s=%d\n", u.games[i].id.c_str(), u.games[i].play_s);
    for (size_t i = 0; i < u.games.size() && i < (size_t)Ui::MAXG; i++) if (u.has_t[i]) { fprintf(f, "[%s]\n", u.games[i].id.c_str()); ini_put(f, u.t[i]); }
    fclose(f);
}
static void settings_load(Ui & u, const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return;
    char ln[2048]; Settings * cur = NULL; bool in_play = false;
    while (fgets(ln, sizeof ln, f)) {
        std::string s = ln; while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (s.empty() || s[0] == ';' || s[0] == '#') continue;
        if (s[0] == '[') {
            std::string id = s.substr(1, s.size() - 2); cur = NULL; in_play = (id == "play");
            if (id == "global") cur = &u.g; else for (size_t i = 0; i < u.games.size() && i < (size_t)Ui::MAXG; i++) if (u.games[i].id == id) { u.has_t[i] = true; u.t[i] = u.g; cur = &u.t[i]; }
            continue;
        }
        if (in_play) { size_t e2 = s.find('='); if (e2 != std::string::npos) { std::string id = s.substr(0, e2); for (size_t i = 0; i < u.games.size(); i++) if (u.games[i].id == id) u.games[i].play_s = atoi(s.c_str() + e2 + 1); } continue; }
        if (cur == &u.g && s.compare(0, 6, "wmult=") == 0) { u.wmult = atoi(s.c_str() + 6); continue; }
        if (cur == &u.g && s.compare(0, 5, "winw=") == 0) { u.winw = atoi(s.c_str() + 5); continue; }
        if (cur == &u.g && s.compare(0, 5, "winh=") == 0) { u.winh = atoi(s.c_str() + 5); continue; }
        if (cur == &u.g && s.compare(0, 11, "gamefolder=") == 0) { u.gamefolder = s.substr(11); continue; }
        if (s.compare(0, 9, "lastgame=") == 0) { int v = atoi(s.c_str() + 9); if (v >= 0 && v < (int)u.games.size()) { u.cur = v; u.pick = v; } continue; }
        size_t e = s.find('='); if (e == std::string::npos || !cur) continue;
        ini_set(*cur, s.substr(0, e), atoi(s.c_str() + e + 1));
    }
    fclose(f);
}

} /* namespace ui */
