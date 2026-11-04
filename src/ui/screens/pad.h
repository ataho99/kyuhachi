#pragma once
/* screens/pad.h -- S8b: S_PAD screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_pad(Canvas & c) {
    titlebar(c, phr_("조작 설정"));
    Settings & s = eff();
    rbox(c, 16, 24, 288, 296, WH, phr_("패드 버튼"));
    std::vector<std::string> n = { phr_("적용 범위"), phr_("결정 방식") }; for (int i = 0; i < 7; i++) n.push_back(pad_row_name(i));
    items(c, 40, 44, 256, n, sel, 24);
    rbox(c, 320, 24, 304, 296, WH, phr_("인포메이션"));
    auto row = [&](int i) { return sel == i; };
    auto ry = [](int i) { return 44 + 24 * i; };
    const char * sc[] = { phr_("전역"), phr_("작품·따름"), phr_("작품·전용") }; choices(c, 344, ry(0), sc, 3, scope3(), WH, row(0));
    const char * ly[] = { phr_("일본식(동쪽)"), phr_("서양식(남쪽)") }; choices(c, 344, ry(1), ly, 2, s.pad == 1 ? 1 : 0, WH, row(1));
    for (int i = 0; i < 7; i++) {
        bool fw = false, df = false; std::string v = pad_val(i, &fw, &df); std::string t = pad_show(v);
        if (scope == 1 && !fw) t += df ? phr_(" (기본)") : phr_(" (전역)"); else if (scope == 0 && df) t += phr_(" (기본)");
        text(c, 344, ry(2 + i), fit(t, 250), row(2 + i) ? K : WH, row(2 + i) ? WH : K);
    }
    text(c, 344, ry(9) + 4, dkey(phr_("{△} = 메뉴 열기 (고정)")), CY, K);
    static const char * const dd[] = { phr_("전역=모든 작품, 이 작품=이 작품만. 작품 값이 우선."), phr_("결정 방식: 일본식=동쪽 버튼, 서양식=남쪽 버튼이 결정."), phr_("버튼 한 줄입니다. {○}:키 고르기."), phr_("버튼 한 줄입니다. {○}:키 고르기."), phr_("버튼 한 줄입니다. {○}:키 고르기."), phr_("버튼 한 줄입니다. {○}:키 고르기."), phr_("버튼 한 줄입니다. {○}:키 고르기."), phr_("버튼 한 줄입니다. {○}:키 고르기."), phr_("버튼 한 줄입니다. {○}:키 고르기.") };
    desc(c, 324, 44, { fit(dkey(dd[sel < 9 ? sel : 8]), 560) });
    helpline(c, phr_("↑↓:선택, ←→:값, ○:키 고르기, ×:뒤로, △:출하 시 설정"));
}
inline void Ui::k_pad(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            mv(sel, 9);
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = (k == UK_RIGHT) ? 1 : -1;
                if (sel == 0) { scope_step(d); }
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
    } while (0);
}
