#pragma once
/* screens/game.h -- S8b: S_GAME screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_game(Canvas & c) {
    /* 배경은 호스트가 게임 화면을 깔아 둔다(draw 가 지우지 않도록 호스트가 호출 순서 관리) */
    int exr = stage_next_label.empty() ? 0 : 22; c.rect(120, 16, 400, 336 + exr, K); rbox(c, 128, 24, 384, 320 + exr, WH, phr_("일시정지"));
    std::vector<std::string> a = { phr_("이어하기"), phr_("저장  슬롯 ") + slot_lbl(slotsel), phr_("불러오기"), phr_("디스크 교체"), phr_("화면 설정"), phr_("소리/패드/배속 설정"), phr_("PC-98 키 보내기"), std::string(phr_("화면 키보드  ")) + (kb_vis ? phr_("켜짐") : phr_("꺼짐")), std::string(phr_("키보드 버튼  ")) + (kb_btn ? phr_("표시") : phr_("숨김")), std::string(phr_("키보드·배속 셰이더  ")) + (kb_sh ? phr_("적용") : phr_("안 함")), phr_("스크린샷"), phr_("메인 메뉴로"), phr_("끝내기") }; if (!stage_next_label.empty()) a.push_back(fit(stage_next_label, 280));
    items(c, 160, 44, 330, a, sel, 22);

    c.rect(0, 376, 640, 24, K);
    helpline(c, phr_("↑↓:선택, ○:결정, ×:닫기(이어하기)"));
}
inline void Ui::k_game(UKey k, Act & a) {
    do {
        if (k == UK_LEFT && sel == 1) slotsel = (slotsel + 4) % 5; if (k == UK_RIGHT && sel == 1) slotsel = (slotsel + 1) % 5;
        if (k == UK_CANCEL) a.t = A_RESUME;
        else if (k == UK_OK) {
            switch (sel) {
            case 0: a.t = A_RESUME; break;
            case 1: a.t = A_SAVE; a.game = cur; a.slot = slotsel; break;
            case 2: go(S_SAVE, 0); break;
            case 3: swapdrv = 0; go(S_SWAP, 0); break;
            case 4: enter_screen_cfg(); go(S_SCREEN, 0); break;
            case 5: enter_conf_cfg(); go(S_CONF, 0); break;
            case 6: go(S_KEYS, 0); break;
            case 7: a.t = A_KBD; break;
            case 8: a.t = A_KBBTN; break;
            case 9: a.t = A_KBSH; break;
            case 10: a.t = A_SHOT; break;
            case 11: yn_open(S_GAME, YN_MAINMENU, phr_("게임을 끝내고 메인 메뉴로 갑니다."), phr_("저장하지 않은 진행은 사라집니다."), phr_("메인 메뉴로 갑니까?")); break;   /* t52e (#131 ①): 기본 = 아니오 */
            case 12: go(S_QUIT, 0); quitsel = 0; break;
            case 13: if (!stage_next_label.empty()) a.t = A_NEXTSTAGE; break;

            }
        }
        break;
    } while (0);
}
