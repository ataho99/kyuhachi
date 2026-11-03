#pragma once
/* screens/swaped.h -- S8c: S_SWAPED screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_swaped(Canvas & c) {
    titlebar(c, ""); rbox(c, 16, 24, 608, 296, WH, phr_("교체 목록 (게임 중 메뉴와 같은 순서)"));
    std::vector<std::string> r; for (size_t i = 0; i < sw.size(); i++) r.push_back(std::to_string(i + 1) + "  " + fit(sw[i], 520)); r.push_back(phr_("+ 디스크 추가 (이미지 고르기)"));
    rowlist(c, 40, 48, 560, 9, r, sel, 24);
    { const char * H = phr_("↑↓:선택, ○:바꾸기, □:삭제, △:위로, SEL:아래로, ×:완료"); desc(c, 324, 58, { phr_("게임 중 메뉴의 「디스크 교체」에 나오는 디스크와 순서입니다."), phr_("시작부터 교체가 필요한 작품은 처음 3초에 안내가 뜹니다.") }, -1, H); helpline(c, H); }
}
inline void Ui::k_swaped(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            int rows = (int)sw.size() + 1; mv(sel, rows);
            if (k == UK_OK) { if (sel == (int)sw.size()) fl_open("swap_add"); else fl_open("swap_set", sel); }
            else if (k == UK_SQ && sel < (int)sw.size()) { sw.erase(sw.begin() + sel); if (sel >= (int)sw.size() + 1) sel = (int)sw.size(); }
            else if (k == UK_TRI && sel > 0 && sel < (int)sw.size()) { std::swap(sw[sel], sw[sel - 1]); sel--; }
            else if (k == UK_SELECT && sel + 1 < (int)sw.size()) { std::swap(sw[sel], sw[sel + 1]); sel++; }
            else if (k == UK_CANCEL) { kv_set("swap", join_commas(sw)); sel = 8; scr = S_EDIT; }
            break; }
    } while (0);
}
