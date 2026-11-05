#pragma once
/* screens/save.h -- S8b: S_SAVE screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_save(Canvas & c) {
    titlebar(c, (in_game && save_mode) ? phr_("sv_title_save") : phr_("세이브 관리"));
    const UiGame * gm = game();
    rbox(c, 16, 24, 288, 296, WH, gm ? ttl_suf(*gm, phr_("슬롯"), 256).c_str() : phr_("슬롯"));
    for (int i = 0; i < 5; i++) {
        int yy = 44 + i * 30; const SlotInfo * si = gm ? &gm->slot[i] : NULL; bool u = si && si->used;
        std::string s = phr_("슬롯 ") + slot_lbl(i) + "   " + (u ? si->stamp : phr_("(비어 있음)"));
        if (i == slotsel) { c.rect(34, yy - 3, 256, 22, WH); text(c, 40, yy, s, K, WH); } else text(c, 40, yy, s, u ? CY : GY, K);
    }
    rbox(c, 320, 24, 304, 296, WH, phr_("인포메이션"));
    const SlotInfo * si = gm ? &gm->slot[slotsel] : NULL;
    if (si && si->used) {
        c.rect(343, 43, 258, 162, GY); c.rect(344, 44, 256, 160, K); if (!si->th.empty()) blit_scaled(c, si->th.data(), si->tw_, si->th_, 344, 44, 256, 160); else sample_picture(c, 344, 44, 256, 160);
        std::string l[4] = { phr_("저장 시각  ") + si->stamp, phr_("상태 파일  ") + std::to_string(si->bytes / 1048576) + "." + std::to_string((si->bytes / 104858) % 10) + "MB", gm->disk[0], gm->disk[1] };
        for (int i = 0; i < 4; i++) if (!l[i].empty()) text(c, 344, 214 + i * 20, l[i], WH, K);
    } else text(c, 344, 120, phr_("비어 있는 슬롯입니다."), GY, K);
    desc(c, 328, 44, { in_game ? (save_mode ? phr_("sv_desc_save") : phr_("고른 슬롯에 저장하거나 불러옵니다.")) : phr_("선택한 슬롯에서 이어서 시작합니다.") }, -1, in_game ? (save_mode ? phr_("sv_help_save") : phr_("↑↓:선택, ○:불러오기, □:이 슬롯에 저장, △:삭제, ×:취소")) : phr_("↑↓:선택, ○:불러오기, △:삭제, ×:취소"));
    helpline(c, in_game ? (save_mode ? phr_("sv_help_save") : phr_("↑↓:선택, ○:불러오기, □:이 슬롯에 저장, △:삭제, ×:취소")) : phr_("↑↓:선택, ○:불러오기, △:삭제, ×:취소"));
}
inline void Ui::k_save(UKey k, Act & a) {
    do {
        if (k == UK_OK && in_game && save_mode) { a.t = A_SAVE; a.game = cur; a.slot = slotsel; }
        else if (k == UK_OK) { if (!in_game && !blk_cur().empty()) { startsel = 1; start_ret = S_MAIN; go(S_START, 1); break; } a.t = A_LOAD; a.game = cur; a.slot = slotsel; }   /* t48b2 (#632): 메뉴에서 슬롯 불러오기도 block 이면 같은 「게임 시작」 팝업(사유 표시)으로 막는다. 게임 안(in_game)은 이미 돌고 있어 해당 없음 */
        else if (k == UK_SQ && in_game) { a.t = A_SAVE; a.game = cur; a.slot = slotsel; }
        else if (k == UK_TRI) { const UiGame * gm0 = game(); if (gm0 && slotsel >= 0 && slotsel < 5 && gm0->slot[slotsel].used) yn_open(S_SAVE, YN_DELSLOT, phr_("슬롯 ") + slot_lbl(slotsel) + phr_("의 저장을 지웁니다."), phr_("지운 저장은 되돌릴 수 없습니다."), phr_("지울까요?"), slotsel); }   /* t52e (#131 ②) */
        else if (k == UK_CANCEL) { go(in_game ? S_GAME : S_MAIN, 0); if (in_game) sel = save_mode ? 1 : 2; }
        break;
    } while (0);
}
