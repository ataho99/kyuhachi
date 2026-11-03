#pragma once
/* screens/keyseq.h -- S8c: S_KEYSEQ screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_keyseq(Canvas & c) {
    titlebar(c, ""); rbox(c, 16, 24, 608, 296, WH, phr_("부팅 후 자동 입력"));
    std::vector<std::string> r; for (size_t i = 0; i < ks.size(); i++) r.push_back(std::to_string(i + 1) + "  " + ent::step_text(ks[i])); r.push_back(phr_("+ 단계 추가"));
    rowlist(c, 40, 48, 560, 9, r, sel, 24);
    { const char * H = phr_("↑↓:선택, ○:편집, □:삭제, △:위로, SEL:아래로, ×:완료"); desc(c, 324, 58, { phr_("부팅한 뒤 정한 시간에 키를 누르거나 글자를 입력합니다."), phr_("게임 디스크가 잠깐 멈추는 안내 화면 넘기기 등에 씁니다.") }, -1, H); helpline(c, H); }
}
inline void Ui::k_keyseq(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            int rows = (int)ks.size() + 1; mv(sel, rows);
            if (k == UK_OK) { if (sel == (int)ks.size()) { ent::Step s; s.ms = ks.empty() ? 3000 : ks.back().ms + 3000; ks.push_back(s); } stepi = sel; sel = 0; scr = S_STEP; }
            else if (k == UK_SQ && sel < (int)ks.size()) { ks.erase(ks.begin() + sel); if (sel >= (int)ks.size() + 1) sel = (int)ks.size(); }
            else if (k == UK_TRI && sel > 0 && sel < (int)ks.size()) { std::swap(ks[sel], ks[sel - 1]); sel--; }
            else if (k == UK_SELECT && sel + 1 < (int)ks.size()) { std::swap(ks[sel], ks[sel + 1]); sel++; }
            else if (k == UK_CANCEL) { kv_set("bootkeys", ent::join_steps(ks)); sel = 7; scr = S_EDIT; }
            break; }
    } while (0);
}
