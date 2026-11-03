#pragma once
/* screens/font.h -- S8b: S_FONT screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_font(Canvas & c) {
    titlebar(c, ""); std::string ttl = fnt_mode == 0 ? phr_("한글 글꼴") : fnt_mode == 1 ? phr_("일본어 글꼴") : phr_("실행 방법 글꼴");
    rbox(c, 16, 24, 288, 296, WH, ttl.c_str());
    std::vector<std::string> r; std::vector<uint32_t> col; std::string curv = fnt_cur();
    for (auto & o : fnt_opts) { r.push_back(std::string(o == curv ? "● " : "  ") + fit(o.empty() ? std::string(fnt_mode == 2 ? phr_("자동 (목록 기본)") : phr_("기본")) : o, 220)); col.push_back(0); }
    if (fnt_opts.empty()) { r.push_back(phr_("(고를 수 있는 글꼴 없음)")); col.push_back(GY); }
    rowlist(c, 40, 48, 256, 9, r, sel, 24, &col);
    rbox(c, 320, 24, 304, 296, WH, phr_("미리보기"));
    std::string pv = sel < (int)fnt_opts.size() ? fnt_opts[sel] : std::string(); if (pv.empty()) pv = fnt_default_name();
    const ov::BmpFont * bf = fnt_font(pv);
    text(c, 340, 44, fit(pv.empty() ? std::string("-") : pv, 270), YL, K);
    { std::string grp; for (auto & f : fonts) if (f.name == pv) grp = f.group; text(c, 340, 64, fit(grp.empty() ? std::string(phr_("묶음 -")) : std::string(fontreg::group_label(grp)), 270), CY, K); }
    if (bf) { text_bmp(c, *bf, 340, 100, phr_("한글 가나다라마바사"), WH, K); text_bmp(c, *bf, 340, 124, phr_("열락의 학원 신비한 바다"), WH, K); text_bmp(c, *bf, 340, 148, "あいうえお アイウエオ", WH, K); } else text(c, 340, 100, phr_("미리볼 수 없는 글꼴입니다."), GY, K);
    { int rej = 0; for (auto & f : fonts) if (!f.ok) rej++; if (rej) text(c, 340, 280, fit(phr_("거른 파일 ") + std::to_string(rej) + phr_("개 (형식·크기 불일치)"), 270), OR, K); }
    text(c, 340, 196, fit(phr_("같은 묶음의 글꼴만 바꿀 수 있습니다."), 270), WH, K); text(c, 340, 216, fit(phr_("다른 묶음은 한글이 깨져 막아 둡니다."), 270), WH, K);
    if (!fnt_need.empty()) text(c, 340, 240, fit(phr_("이 작품의 묶음: ") + std::string(fontreg::group_label(fnt_need)), 270), CY, K);
    desc(c, 324, 58, { fnt_mode == 2 ? phr_("이 실행 방법의 글꼴입니다. 자동 = 목록 기본(한글판=한글, 원판=일본어).") : phr_("전역 기본 글꼴입니다. 실행 방법이 글꼴을 따로 정하면 그것이 우선입니다."), phr_("글꼴 폴더에 bmp 를 넣으면 자동으로 목록에 나옵니다.") }, -1, phr_("↑↓:선택, ○:정함, ×:취소"));
    helpline(c, phr_("↑↓:선택, ○:정함, ×:취소"));
}
inline void Ui::k_font(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do {
            mv(sel, (int)fnt_opts.size());
            if (k == UK_OK && sel < (int)fnt_opts.size()) {
                const std::string o = fnt_opts[sel];
                if (fnt_mode == 0) font_kr = (o == "kyuhachi_font" || o == "pc98player_font") ? std::string() : o; else if (fnt_mode == 1) font_jp = (o == "jp_default") ? std::string() : o;
                else { if (o.empty()) { ed.erase("font"); ed.erase("font_kind"); } else { ed["font"] = o; std::string g; for (auto & f : fonts) if (f.name == o) g = f.group; if (g == fnt_need) ed["font_kind"] = g == "jis" ? "jp" : "kr"; /* t52c: 다른 묶음 글꼴을 고르면 작품 종류(font_kind)는 그대로 둔다 */ } }
                dirty = true; scr = fnt_ret; sel = fnt_prev_sel; a.t = A_APPLY;
            } else if (k == UK_CANCEL) { scr = fnt_ret; sel = fnt_prev_sel; }
            break;
    } while (0);
}
