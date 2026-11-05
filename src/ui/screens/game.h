#pragma once
/* screens/game.h -- pause menu (build 5b-1: 8 lines; settings hub, keyboard submenu, 8-slot save / load screen). */
inline void Ui::d_game(Canvas & c) {
    /* the host lays the game picture underneath (draw does not clear it) */
    const int exr = stage_next_label.empty() ? 0 : 22;
    c.rect(120, 16, 400, 256 + exr, K); rbox(c, 128, 24, 384, 240 + exr, WH, phr_("pg_title"));
    std::vector<std::string> a = { phr_("pg_resume"), phr_("pg_save"), phr_("pg_load"), phr_("pg_swap"),
        phr_("pg_set"), phr_("pg_kb"), phr_("pg_main"), phr_("pg_quit") };
    if (!stage_next_label.empty()) a.push_back(fit(stage_next_label, 280));
    items(c, 160, 44, 330, a, sel, 22);
    c.rect(0, 376, 640, 24, K);
    helpline(c, phr_("pg_help"));
}
inline void Ui::k_game(UKey k, Act & a) {
    if (k == UK_CANCEL) { a.t = A_RESUME; return; }
    if (k != UK_OK) return;
    switch (sel) {
    case 0: a.t = A_RESUME; break;
    case 1: save_mode = true; go(S_SAVE, 0); break;
    case 2: save_mode = false; go(S_SAVE, 0); break;
    case 3: swapdrv = 0; go(S_SWAP, 0); break;
    case 4: conf_grp = 0; enter_conf_cfg(); go(S_SET, 0); break;
    case 5: go(S_GKB, 0); break;
    case 6: yn_open(S_GAME, YN_MAINMENU, phr_("pg_yn1"), phr_("pg_yn2"), phr_("pg_yn3")); break;
    case 7: go(S_QUIT, 0); quitsel = 0; break;
    case 8: if (!stage_next_label.empty()) a.t = A_NEXTSTAGE; break;
    }
}
