#pragma once
/* screens/yn.h -- S8c: S_YN screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_yn(Canvas & c, Scr under) {
    switch (under) { case S_EDIT: d_edit(c); break; case S_ENT: d_ent(c); break; case S_MAIN: d_main(c, 0); break; case S_ENTMENU: d_ent(c); break; case S_GAME: d_game(c); break; case S_SAVE: d_save(c); break; case S_EXTRA: d_extra(c); break; default: break; }   /* t52e (#131): 게임 메뉴·슬롯·부가 위에도 확인창 */
    int bw = 520, bx, by;   /* t672 (#656): 420 -> 520 (영어 본문이 … 로 잘리던 것) */ confirm_box(c, bw, 150, phr_("확인"), bx, by);
    for (int i = 0; i < 3; i++) if (!yn_msg[i].empty()) text(c, bx + 24, by + 22 + i * 22, fit(yn_msg[i], bw - 48), WH, K);
    const char * q[] = { phr_("예"), phr_("아니오") }; int cw = choices_w(q, 2); const int cx0 = bx + ((bw - cw) >> 1) + 4;
    choices(c, cx0, by + 100, q, 2, ynsel, WH, false);
    hits.clear(); ov::g_chit.clear();   /* 4: 확인창 터치 — 예/아니오 두 칸만 받는다(뒤 화면 줄은 막음). 탭 = 위/아래 키 + 결정 */
    for (int i = 0, hx = cx0; i < 2; i++) { Hit ht = { hx - 2, by + 97, tw(q[i]) + 12, 24, i, ynsel }; hits.push_back(ht); hx += tw(q[i]) + 16; }
    helpline(c, phr_("←→:선택, ○:결정, ×:아니오"));
}
inline void Ui::k_yn(UKey k, Act & a) {
    do {
            if (k == UK_LEFT || k == UK_UP) ynsel = 0;   /* 4: 위·아래는 터치가 보내는 키(탭한 칸으로 옮김) */
            else if (k == UK_RIGHT || k == UK_DOWN) ynsel = 1;
            else if (k == UK_OK) { if (ynsel == 0) a = yn_do(); else scr = yn_ret; }
            else if (k == UK_CANCEL) scr = yn_ret;
            break;
    } while (0);
}
