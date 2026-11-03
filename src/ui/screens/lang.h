#pragma once
/* screens/lang.h -- S8a: S_LANG screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_lang(Canvas & c) {
    titlebar(c, phr_("언어 지정"));
    rbox(c, 16, 24, 288, 224, WH, phr_("작품 언어"));
    std::vector<std::string> r; std::vector<uint32_t> col;
    for (size_t i = 0; i < lang_gid.size(); i++) { int gi = gidx_of(lang_gid[i]); if (gi < 0) { r.push_back(phr_("(없음)")); col.push_back(GY); continue; } const UiGame & g = games[gi]; r.push_back(fit(short_of(g), 150) + "  " + (g.lang_exp.empty() ? std::string("자동") : g.lang == "ko" ? std::string(phr_("한글판")) : std::string(phr_("원판"))) ); col.push_back(g.lang_exp.empty() ? 0 : YL); }
    if (r.empty()) { r.push_back(phr_("(대상 없음)")); col.push_back(GY); }
    rowlist(c, 40, 48, 256, 7, r, sel, 24, &col);
    rbox(c, 320, 24, 304, 224, WH, phr_("상세"));
    int gi = (sel >= 0 && sel < (int)lang_gid.size()) ? gidx_of(lang_gid[sel]) : -1;
    if (gi >= 0) {
        const UiGame & g = games[gi];
        text(c, 340, 44, fit(g.title, 270), WH, K); text(c, 340, 68, phr_("언어"), CY, K); text(c, 340 + 76, 68, fit(lang_label(g), 190), YL, K);
        std::string w = (sel < (int)lang_why.size()) ? lang_why[sel] : std::string(); text(c, 340, 92, phr_("자동 판정"), CY, K); text(c, 340 + 76, 92, fit(g.lang == "ko" ? std::string(phr_("한글 증거 있음")) : std::string(phr_("한글 증거 없음 (원판 취급)")), 190), WH, K);
        if (!w.empty()) text(c, 340, 116, fit(w, 270), GY, K);
        text(c, 340, 152, phr_("한글판 → 한글 글꼴 / 원판 → 일본어 글꼴"), WH, K);
    }
    desc(c, 264, 64, { dkey(phr_("{○} 또는 ←→:자동 → 한글판 → 원판 일본어 순으로 바꿉니다.")), phr_("저장 위치: 작품 폴더의 user.ini [game] lang= (자동이면 지움).") });
    helpline(c, phr_("↑↓:작품, ○/←→:언어 바꾸기, ×:돌아가기"));
}
// t44a (#494): 자동 → 한글판 → 원판 일본어 → 자동
inline void Ui::k_lang(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            int n = std::max<int>((int)lang_gid.size(), 1); mv(sel, n);
            if (k == UK_OK || k == UK_RIGHT || k == UK_LEFT) {
                int gi = (sel < (int)lang_gid.size()) ? gidx_of(lang_gid[sel]) : -1;
                if (gi >= 0) { const std::string & cur = games[gi].lang_exp; int v0 = cur == "ko" ? 1 : cur == "jp" ? 2 : 0; int d = (k == UK_LEFT) ? 2 : 1; a.t = A_SETLANG; a.game = gi; a.arg = (v0 + d) % 3; dirty = true; }
            } else if (k == UK_CANCEL) { scr = lang_ret; sel = (lang_ret == S_ENT) ? (int)std::max<size_t>(vents(entg).size(), 1) + 1 : 5; }
            break; }
    } while (0);
}
