#pragma once
/* screens/ext.h -- S_EXT: extra-buttons tab (build 5a-2b). Shows the raw code the app receives for every device button
   (KEY / JOY button / PAD / HAT / AXIS), newest first. All pad and key input is swallowed while open; a touch or click closes it. */
inline void Ui::d_ext(Canvas & c) {
    if (env.raw_line && env.raw_line(-1) == "EXIT") { if (env.raw_ctl) env.raw_ctl(0); scr = S_HOT; sel = 0; return; }
    titlebar(c, phr_("hot_title"));
    rbox(c, 16, 24, 608, 296, WH, (std::string(phr_("hot_tabpad")) + " [" + phr_("tab_ext") + "]").c_str());
    int shown = 0;
    for (int k = 0; k < 8; k++) {
        const std::string ln = env.raw_line ? env.raw_line(k) : std::string();
        if (ln.empty()) continue;
        c.clip(32, 40 + k * 30, 576, 28);
        text(c, 40, 44 + k * 30, fit(ln, 560), k == 0 ? YL : CY, K);
        c.unclip(); shown++;
    }
    if (!shown) text(c, 40, 44, fit(phr_("ext_empty"), 560), GY, K);
    Hit ht = { 16, 24, 608, 296, 0, 0 }; hits.push_back(ht);
    std::vector<std::string> ln; ln.push_back(fit(phr_("ext_desc"), 560));
    desc(c, 324, 44, ln);
    helpline(c, phr_("ext_help"));
}
inline void Ui::k_ext(UKey k, Act & a) {
    (void)a;
    if (k == UK_OK || k == UK_L) { if (env.raw_ctl) env.raw_ctl(0); scr = S_HOT; sel = 0; }
}
