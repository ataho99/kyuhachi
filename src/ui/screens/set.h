#pragma once
/* screens/set.h -- build 5b-1: settings hub (screen / sound / control / system) and the in-game keyboard submenu. */
inline void Ui::d_set(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 288, 224, WH, phr_("set_title"));
    std::vector<std::string> m = { phr_("set_screen"), phr_("set_sound"), phr_("set_control"), phr_("set_system"), phr_("set_shim") };
    items(c, 40, 46, 256, m, sel, 28);
    info_box(c, 320, 24, 304, 224);
    static const char * md[] = { "set_d0", "set_d1", "set_d2", "set_d3", "set_d4" };
    desc(c, 264, 64, { phr_(md[sel >= 0 && sel < 5 ? sel : 0]) });
    helpline(c, phr_("set_help"));
}
inline void Ui::k_set(UKey k, Act & a) {
    (void)a;
    if (k == UK_UP) sel = (sel + 4) % 5;
    else if (k == UK_DOWN) sel = (sel + 1) % 5;
    else if (k == UK_OK) {
        if (sel == 0) { scr_from = S_SET; enter_screen_cfg(); go(S_SCREEN, 0); }
        else if (sel == 4) go(S_SHIM, 0);
        else { conf_grp = sel; enter_conf_cfg(); go(S_CONF, 0); }
    } else if (k == UK_CANCEL) {
        conf_grp = 0;
        if (in_game) go(S_GAME, 4); else go(S_MAIN, 0);
    }
}
inline void Ui::d_gkb(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 368, 224, WH, phr_("pg_kb"));
    const std::string sp = "  ";
    std::vector<std::string> a = { phr_("gkb_keys"),
        std::string(phr_("gkb_kbd")) + sp + (kb_vis ? phr_("gkb_on") : phr_("gkb_off")),
        std::string(phr_("gkb_btn")) + sp + (kb_btn ? phr_("gkb_show") : phr_("gkb_hide")),
        std::string(phr_("gkb_sh")) + sp + (kb_sh ? phr_("gkb_apply") : phr_("gkb_no")), phr_("gkb_shot") };
    items(c, 40, 46, 336, a, sel, 28);
    helpline(c, phr_("gkb_help"));
}
inline void Ui::k_gkb(UKey k, Act & a) {
    if (k == UK_UP) sel = (sel + 4) % 5;
    else if (k == UK_DOWN) sel = (sel + 1) % 5;
    else if (k == UK_OK) {
        if (sel == 0) go(S_KEYS, 0);
        else if (sel == 1) a.t = A_KBD;
        else if (sel == 2) a.t = A_KBBTN;
        else if (sel == 3) a.t = A_KBSH;
        else a.t = A_SHOT;
    } else if (k == UK_CANCEL) go(S_GAME, 5);
}
