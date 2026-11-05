#pragma once
/* screens/extra.h -- S_EXTRA: work menu. build 5b-3: favorites + language sit under "this game's settings" (sub list, extra_sub = 1);
   "boot system disk only" lives under Start. Rows are looked up by id (Ui::extra_ids), not by number. */
inline void Ui::d_extra(Canvas & c) {
    titlebar(c, phr_("부가"));
    const UiGame * gm = game();
    const char * ttl = extra_sub ? phr_("this_set") : phr_("extra_ttl");
    rbox(c, 16, 24, 288, 244, WH, gm ? ttl_suf(*gm, ttl, 256).c_str() : ttl);
    const std::string fv = std::string(phr_("즐겨찾기: ")) + (games.empty() || !games[base_of(cur)].fav ? phr_("☆ 끔") : phr_("★ 켬"));
    std::vector<std::string> a;
    if (extra_sub) a = { phr_("lang_set"), fv };
    else a = { phr_("디스크 정보"), phr_("부팅 시퀀스 다시 보기"), phr_("조작 안내"), phr_("정보·라이선스"), phr_("적용값 보기 (값·출처 표)"), std::string(phr_("this_set")) + "  >", phr_("원본 디스크로 초기화") };
    std::vector<bool> dm(a.size(), false); items(c, 40, 44, 256, a, sel, 24, &dm);
    rbox(c, 320, 24, 304, 244, WH, phr_("인포메이션"));
    if (gm) { std::string l[4] = { gm->disk[0], gm->disk[1], "", gm->boot_note }; for (int i = 0; i < 4; i++) if (!l[i].empty()) text(c, 344, 44 + i * 22, l[i], i == 3 ? CY : WH, K); }
    desc(c, 276, 56, { phr_(extra_sub ? "this_desc" : "extra_desc") });
    helpline(c, phr_("↑↓:선택, ○:결정, ×:취소"));
}
inline void Ui::k_extra(UKey k, Act & a) {
    const std::vector<int> ids = extra_ids(); const int id = (sel >= 0 && sel < (int)ids.size()) ? ids[sel] : -1;
    if (k == UK_OK) {
        if (id == 3) go(S_HELP, 0);
        else if (id == 4) go(S_ABOUT, 0);
        else if (id == 8) { int b = base_of(cur); if (b >= 0 && b < (int)games.size()) open_lang(std::vector<std::string>(1, games[b].id), std::vector<std::string>(), S_EXTRA); }   /* build 3: language per work */
        else if (id == 7) open_vals();   /* t48a (#614 3) */
        else if (id == 5) { int keep = cur, b = base_of(cur); if (b >= 0 && b < (int)games.size()) { games[b].fav = !games[b].fav; dirty = true; rebuild_vis(keep); } }
        else if (id == 6) yn_open(S_EXTRA, YN_DISKRESET, phr_("게임 안 세이브가 지워지고 디스크가"), phr_("원본 상태로 돌아갑니다 (이전 사본은 .bak)."), phr_("계속할까요?"));   /* t52e (#131 2) */
        else if (id == 100) { extra_sub = 1; sel = 0; }
        else if (id >= 0) { a.t = A_EXTRA; a.arg = id; a.game = cur; }
    } else if (k == UK_CANCEL) {
        if (extra_sub) { extra_sub = 0; sel = extra_row(100); }
        else go(back == S_PICK ? S_PICK : S_MAIN, 0);
    }
}
