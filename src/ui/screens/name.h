#pragma once
/* screens/name.h -- S8c: S_NAME screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_name(Canvas & c) {
    titlebar(c, ""); rbox(c, 16, 24, 288, 296, WH, phr_("이름 고르기"));
    std::vector<std::string> r = name_presets(); r.push_back(phr_("직접 입력 (한글 키보드)")); r.push_back(phr_("직접 입력 (영문 글자표)")); items(c, 40, 44, 256, r, sel, 24);   /* t48b: 11줄이라 간격 24 */
    rbox(c, 320, 24, 304, 296, WH, phr_("설명")); text(c, 344, 48, phr_("목록에서 이름을 고르면 같은 이름이"), WH, K); text(c, 344, 68, phr_("이미 있을 때 번호를 붙입니다."), WH, K); text(c, 344, 104, phr_("현재 이름"), CY, K); text(c, 344, 124, fit(ed_name, 250), YL, K);
    helpline(c, phr_("↑↓:선택, ○:정함, ×:취소"));
}
inline void Ui::k_name(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            int cnt = (int)name_presets().size() + 2; mv(sel, cnt);   /* t48b: 끝에 「한글 키보드」 「영문 글자표」 두 줄 */
            if (k == UK_OK) { if (sel < cnt - 2) { ed_name = ent::unique_name(name_presets()[sel], ents_used(eg, en_orig)); sel = 0; scr = S_EDIT; } else osk_open(0, ed_name, S_EDIT, sel == cnt - 2); }
            else if (k == UK_CANCEL) { sel = 0; scr = S_EDIT; }
            break; }
    } while (0);
}
