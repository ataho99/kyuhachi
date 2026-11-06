#pragma once
/* screens/phos.h -- t662 (#635): phosphor option page (beam width / persistence / breathing / glow), a sub-page of S_SCREEN. */
/* One packed Settings::phos (2 bits per option: beam | pers << 2 | breath << 4 | glow << 6), own-value bit 22 for the whole bundle. */
inline void Ui::d_phos(Canvas & c) {
    titlebar(c, phr_("ph_title"));
    Settings & s = eff();
    const bool own = scope == 1 && cur >= 0 && cur < MAXG;
    const bool ind = own && (ovm[cur] & (1u << 22));
    const bool offp = s.ratio == 2;   /* integer scaling uses the plain shader: the bundle has no effect there */
    static const char * const nk[4] = { "ph_beam", "ph_pers", "ph_breath", "ph_glow" };
    static const char * const dk[4] = { "desc_ph_beam", "desc_ph_pers", "desc_ph_breath", "desc_ph_glow" };
    rbox(c, 16, 24, 288, 220, WH, phr_("ph_title"));
    std::vector<std::string> n; std::vector<bool> dm;
    for (int i = 0; i < 4; i++) {
        n.push_back((ind ? std::string("● ") : std::string()) + phr_(nk[i])); dm.push_back(offp);
    }
    items(c, 40, 44, 256, n, sel, 16, &dm);
    rbox(c, 320, 24, 304, 220, WH, phr_("ph_choices"));
    const char * lv[4] = { phr_("ph_lv0"), phr_("ph_lv1"), phr_("ph_lv2"), phr_("ph_lv3") };
    const char * pv[4] = { phr_("ph_lv0"), "40 ms", "90 ms", "160 ms" };
    for (int i = 0; i < 4; i++) {
        choices(c, 344, 44 + i * 16, i == 1 ? pv : lv, 4, (s.phos >> (2 * i)) & 3, offp ? GY : WH, sel == i);
    }
    rbox(c, 16, 248, 608, 82, WH, phr_("ph_prev"));
    text(c, 40, 270, std::string(phr_(offp ? "ph_off_note" : "ph_live")), offp ? YL : WH, K);
    desc_mq(c, 324, 44, dkey(phr_(dk[sel >= 0 && sel < 4 ? sel : 0])));
    helpline(c, phr_("ph_help"));
}
inline Act Ui::k_phos(UKey k, Act & a) {
    const int gi = cur; const bool own = scope == 1 && gi >= 0 && gi < MAXG; const unsigned PB = 1u << 22;
    Settings pre; if (own) { sync_t(gi); pre = t[gi]; }
    Settings & s = eff();
    if (k == UK_LEFT || k == UK_RIGHT) {
        const int sh = 2 * (sel < 0 ? 0 : sel > 3 ? 3 : sel);
        const int v = clampi(((s.phos >> sh) & 3) + (k == UK_RIGHT ? 1 : -1), 0, 3);
        s.phos = (s.phos & ~(3 << sh)) | (v << sh); a.t = A_APPLY; dirty = true;
    } else if (k == UK_TRI) {
        if (own && (ovm[gi] & PB)) {   /* marked line: back to global */
            ov_set(gi, ovm[gi] & ~PB); sync_t(gi); a.t = A_APPLY; dirty = true; return a;
        }
        s.phos = 0; a.t = A_APPLY; dirty = true;
    } else if (k == UK_OK || k == UK_CANCEL) { phos_pg = false; sel = rowidx(S_SCREEN, 13); return a; }
    if (own) { const unsigned d = diff_bits(pre, t[gi]); if (d) ov_set(gi, ovm[gi] | d); sync_t(gi); }
    return a;
}
