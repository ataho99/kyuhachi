#pragma once
/* screens/extra.h -- S8a: S_EXTRA screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_extra(Canvas & c) {
    titlebar(c, phr_("부가"));
    const UiGame * gm = game();
    rbox(c, 16, 24, 288, 244, WH, gm ? ttl_suf(*gm, phr_("부가"), 256).c_str() : phr_("부가"));
    std::vector<std::string> a = { phr_("디스크 정보"), phr_("시스템 디스크만 부팅"), phr_("부팅 시퀀스 다시 보기"), phr_("조작 안내"), phr_("정보·라이선스"), std::string(phr_("즐겨찾기: ")) + (games.empty() || !games[base_of(cur)].fav ? phr_("☆ 끔") : phr_("★ 켬")), phr_("원본 디스크로 초기화"), phr_("적용값 보기 (값·출처 표)") }; std::vector<bool> dm = { false, false, false, false, false, false, false, false }; items(c, 40, 44, 256, a, sel, 24, &dm);
    rbox(c, 320, 24, 304, 244, WH, phr_("인포메이션"));
    if (gm) { std::string l[4] = { gm->disk[0], gm->disk[1], "", gm->boot_note }; for (int i = 0; i < 4; i++) if (!l[i].empty()) text(c, 344, 44 + i * 22, l[i], i == 3 ? CY : WH, K); }
    desc(c, 276, 56, { phr_("고른 작품의 디스크 구성과 부가 기능입니다.") });
    helpline(c, phr_("↑↓:선택, ○:결정, ×:취소"));
}
inline void Ui::k_extra(UKey k, Act & a) {
    do {
            if (k == UK_OK && sel == 3) go(S_HELP, 0);
            else if (k == UK_OK && sel == 4) go(S_ABOUT, 0);   /* t49a (#814③): 「오프닝 보기」 삭제로 5번 이후 한 칸씩 당김 */
            else if (k == UK_OK && sel == 7) open_vals();   /* t48a (#614 ③) */
            else if (k == UK_OK && sel == 5) { int keep = cur, b = base_of(cur); if (b >= 0 && b < (int)games.size()) { games[b].fav = !games[b].fav; dirty = true; rebuild_vis(keep); } }
            else if (k == UK_OK && sel == 6) yn_open(S_EXTRA, YN_DISKRESET, phr_("게임 안 세이브가 지워지고 디스크가"), phr_("원본 상태로 돌아갑니다 (이전 사본은 .bak)."), phr_("계속할까요?"));   /* t52e (#131 ②) */
            else if (k == UK_OK) { a.t = A_EXTRA; a.arg = sel; a.game = cur; }
            else if (k == UK_CANCEL) go(back == S_PICK ? S_PICK : S_MAIN, 0);
            break;
    } while (0);
}
