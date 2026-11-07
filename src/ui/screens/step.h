#pragma once
/* screens/step.h -- S8c: S_STEP screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_step(Canvas & c) {
    titlebar(c, ""); rbox(c, 16, 24, 608, 200, WH, phr_("자동 입력 단계"));
    const ent::Step & st = ks[stepi]; std::string w; { char b[32]; snprintf(b, sizeof b, phr_("%d.%d초"), st.ms / 1000, (st.ms / 100) % 10); w = b; }
    const char * lab[] = { phr_("종류"), phr_("대기"), st.type ? phr_("입력할 글자") : phr_("누를 키") }; std::string val[3] = { st.type ? phr_("글자 입력") : phr_("키 누르기"), w + phr_(" 뒤"), ent::show_val(st.val) };
    for (int i = 0; i < 3; i++) { int yy = 52 + i * 30; bool sl = i == sel; if (sl) c.rect(30, yy - 3, 580, 22, WH); text(c, 40, yy, lab[i], sl ? K : CY, sl ? WH : K); text(c, 200, yy, std::string(sl && (i < 2 || !st.type) ? "← " : "") + val[i] + (sl && (i < 2 || !st.type) ? " →" : ""), sl ? K : WH, sl ? WH : K); }
    desc(c, 240, 64, { st.type ? dkey(phr_("글자 입력: {○} 로 화면 키보드, ←→ 로 자주 쓰는 명령(dir 등).")) : phr_("키 누르기: ←→ 로 키를 고릅니다."), phr_("대기는 0.5초 단위입니다. ~ 는 Enter 입니다.") });
    helpline(c, phr_("↑↓:선택, ←→:값, ○:입력, ×:돌아가기"));
}
inline void Ui::k_step(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
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
    } while (0);
}
