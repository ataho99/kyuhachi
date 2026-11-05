#pragma once
/* screens/shim.h -- build 7: menu text shimmer settings (unit / strength / speed / glow / color), live preview on this screen */
inline void Ui::d_shim(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 288, 224, WH, phr_("shim_title"));
    const char * md[] = { phr_("shim_m0"), phr_("shim_m1"), phr_("shim_m2") };
    const char * cl[] = { phr_("shim_c0"), phr_("shim_c1"), phr_("shim_c2"), phr_("shim_c3") };
    const std::string sp = "  ";
    std::vector<std::string> a = {
        std::string(phr_("shim_mode")) + sp + md[clampi(g.shim, 0, 2)],
        std::string(phr_("shim_amp")) + sp + std::to_string(g.shim_amp),
        std::string(phr_("shim_spd")) + sp + std::to_string(g.shim_spd),
        std::string(phr_("shim_glow")) + sp + std::to_string(g.shim_glow),
        std::string(phr_("shim_col")) + sp + cl[clampi(g.shim_col, 0, 3)] };
    items(c, 40, 46, 256, a, sel, 28);
    info_box(c, 320, 24, 304, 224);
    static const char * dd[] = { "shim_d0", "shim_d1", "shim_d2", "shim_d3", "shim_d4" };
    desc(c, 264, 64, { phr_(dd[sel >= 0 && sel < 5 ? sel : 0]) });
    helpline(c, phr_("shim_help"));
}
inline void Ui::k_shim(UKey k, Act & a) {
    if (k == UK_UP) sel = (sel + 4) % 5;
    else if (k == UK_DOWN) sel = (sel + 1) % 5;
    else if (k == UK_LEFT || k == UK_RIGHT || k == UK_OK) {
        const bool pick = sel == 0 || sel == 4;
        if (k == UK_OK && !pick) return;
        const int d = (k == UK_LEFT) ? -1 : 1;
        int * v[5] = { &g.shim, &g.shim_amp, &g.shim_spd, &g.shim_glow, &g.shim_col };
        const int lo[5] = { 0, 1, 1, 0, 0 }, hi[5] = { 2, 10, 10, 10, 3 };
        int nv = *v[sel] + d;
        if (pick) nv = nv < lo[sel] ? hi[sel] : nv > hi[sel] ? lo[sel] : nv; else nv = clampi(nv, lo[sel], hi[sel]);
        if (nv != *v[sel]) { *v[sel] = nv; dirty = true; a.t = A_APPLY; }
    } else if (k == UK_CANCEL) go(S_SET, 4);
}
