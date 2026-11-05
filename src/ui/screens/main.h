#pragma once
/* screens/main.h -- S8a: S_MAIN screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_main(Canvas & c, int mode) {
    if (mode != 3) {
    titlebar(c, "");
    rbox(c, 16, 24, 288, 224, WH, phr_("메인메뉴"));
    std::vector<std::string> m; menu_names(m); items(c, 40, 46, 256, m, mode == 2 ? 6 : mainsel, 28);
    info_box(c, 320, 24, 304, 224);
    static const char * md[] = { phr_("현재 작품을 이어하기(마지막 저장) 또는 처음부터 시작합니다."), phr_("작품을 고릅니다. 고른 작품이 현재 작품이 됩니다."), phr_("화면 비율과 셰이더를 바꿉니다. 바꾸면 바로 적용됩니다."), phr_("소리 크기, 패드 배치, 배속을 설정합니다."), phr_("저장 슬롯을 불러오거나 지웁니다."), phr_("디스크 정보, 오프닝 보기, 조작 안내, 정보·라이선스입니다."), phr_("설정을 저장하고 끝냅니다.") };
    desc(c, 264, 64, { md[mode == 2 ? 6 : mainsel] });
    if (mode == 0) helpline(c, phr_("↑↓:선택, ○:결정, ×:취소, SELECT:부가"));
    }
    if (mode == 1 || mode == 3) {
        const UiGame * gm = game(); bool hs = has_save(gm); int bx, by; confirm_box(c, 400, 196, phr_("게임 시작"), bx, by);
        bool blk = gm && !blk_cur().empty();
        text(c, bx + 24, by + 24, gm ? ttl_fit(*gm, 352) : std::string(), blk ? GY : CY, K);
        std::vector<std::string> a = { hs ? phr_("이어하기   슬롯 ") + slot_lbl(gm->lastslot) + "  " + gm->slot[gm->lastslot].stamp : std::string(phr_("이어하기   (저장 없음)")), phr_("처음부터"), phr_("시스템 디스크만 부팅") };
        std::vector<bool> dm = { !hs || blk, blk, blk }; items(c, bx + 30, by + 60, 340, a, startsel, 26, &dm);
        if (blk) { text(c, bx + 24, by + 154, fit(phr_("실행 불가: ") + blk_cur(), 352), OR, K); }
        helpline(c, phr_("↑↓:선택, ○:결정, ×:취소"));
    }
    if (mode == 2) d_quit(c);
}
inline void Ui::k_main(UKey k, Act & a) {
    do {
            if (k == UK_OK) {
                static const Scr to[7] = { S_START, S_PICK, S_SCREEN, S_CONF, S_SAVE, S_EXTRA, S_QUIT };
                back = S_MAIN; if (to[mainsel] == S_SCREEN) enter_screen_cfg(); if (to[mainsel] == S_CONF) enter_conf_cfg(); if (to[mainsel] == S_PICK) { pick = pos_of(cur); reorder = false; } if (to[mainsel] == S_QUIT) quitsel = 0;
                if (to[mainsel] == S_START) { startsel = has_save(game()) ? 0 : 1; start_ret = S_MAIN; }
                go(to[mainsel], 0); if (to[mainsel] == S_SAVE) slotsel = 0;
            } else if (k == UK_CANCEL) { go(S_QUIT, 0); quitsel = 0; }
            else if (k == UK_SELECT) go(S_EXTRA, 0);
            break;
    } while (0);
}
