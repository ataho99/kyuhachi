#pragma once
/* screens/hot.h -- S_HOT: hotkey tab (build 5a-2). Function <-> how to press it (SELECT held + button, or a single button).
   Global only: settings.ini btn_hot_<fn> = sel+<button> | <button> | none. Menu = SELECT+START is fixed. */
inline std::string Ui::hot_default(int q) {
    switch (q) {
    case 4: return "sel+rightshoulder";
    case 5: return "sel+leftshoulder";
    case 6: return "sel+y";
    case 7: return "sel+rightstick";
    case 8: return "sel+righttrigger";
    default: return "none"; }
}
inline std::string Ui::hot_val(int q, bool * custom) {
    auto it = gpad.find(std::string("btn_hot_") + pad_fn_name(q));
    const bool cu = it != gpad.end() && !it->second.empty();
    if (custom) *custom = cu;
    return cu ? it->second : hot_default(q);
}
inline int Ui::hot_btn(const std::string & v, bool * sel) {
    std::string s = v; bool sl = false;
    if (s.compare(0, 4, "sel+") == 0) { sl = true; s = s.substr(4); }
    if (sel) *sel = sl;
    if (s.empty() || s == "none") return -1;
    for (int i = 0; i < PAD_N; i++) if (std::string(pad_key(i)).compare(4, std::string::npos, s) == 0) return i;
    return -1;
}
inline std::string Ui::hot_text(const std::string & v) {
    bool sl = false; const int b = hot_btn(v, &sl);
    if (b < 0) return phr_("hot_none");
    return std::string(sl ? "SELECT+" : "") + pad_tag(b);
}
inline bool Ui::hot_overlap(int q) {   /* single button that is also a game binding, or the same combo on two functions */
    bool sl = false; const int b = hot_btn(hot_val(q), &sl);
    if (b < 0) return false;
    if (!sl) { const std::string pv = pad_val(b); if (!pv.empty() && pv != "none") return true; }
    for (int k = 0; k < HOT_N; k++) {
        const int q2 = hot_fn(k); bool s2 = false;
        if (q2 != q && hot_btn(hot_val(q2), &s2) == b && s2 == sl) return true; }
    return false;
}
inline void Ui::hot_poll() {
    if (!hot_cap) return;
    int pb = -1; bool sl = false;
    const int r = env.pad_cap ? env.pad_cap(1, pb, sl) : 2;
    if (r == 1 && pb >= 0 && sel >= 1 && sel <= HOT_N) {
        gpad[std::string("btn_hot_") + pad_fn_name(hot_fn(sel - 1))] = std::string(sl ? "sel+" : "") + (pad_key(pb) + 4);
        hot_cap = false; dirty = true;
        int z = 0; bool zb = false; env.pad_cap(3, z, zb);
    } else if (r == 2) hot_cap = false;
}
inline void Ui::d_hot(Canvas & c) {
    hot_poll();
    titlebar(c, phr_("hot_title"));
    rbox(c, 16, 24, 608, 296, WH, (std::string(phr_("hot_tabpad")) + " [" + phr_("tab_hot") + "]").c_str());
    for (int k = 0; k <= HOT_N; k++) {
        const int yy = 44 + k * 32; const bool on = sel == k; const int q = k ? hot_fn(k - 1) : 1;
        const std::string nm = phr_(pad_fn_key(q));
        const std::string cb = k ? hot_text(hot_val(q)) : std::string("SELECT+START");
        bool cu = false; if (k) hot_val(q, &cu);
        const bool ov = k && hot_overlap(q);
        if (on) c.rect(28, yy - 4, 584, 28, WH);
        c.clip(32, yy - 4, 576, 28);
        text(c, 40, yy, std::string(cu ? "* " : "  ") + fit(nm, 250), on ? K : WH, on ? WH : K);
        text(c, 350, yy, cb, on ? K : CY, on ? WH : K);
        if (k == 0) text(c, 520, yy, phr_("hot_fixed"), on ? K : GY, on ? WH : K);
        else if (ov) text(c, 520, yy, phr_("hot_overlap"), RD, on ? WH : K);
        c.unclip();
        Hit ht = { 28, yy - 4, 584, 28, k, sel }; hits.push_back(ht);
    }
    text(c, 40, 288, fit(phr_("hot_save"), 560), CY, K);
    if (hot_cap) {
        fillbox(c, 100, 110, 440, 110, K, YL, NULL);
        text(c, 124, 128, fit(phr_("hot_cap_title"), 400), YL, K);
        text(c, 124, 160, fit(phr_("hot_cap_sub"), 400), WH, K);
        int left = 8 - (ts_ms - hot_cap_t0) / 1000; if (left < 0) left = 0;
        text(c, 124, 188, std::to_string(left) + " s", CY, K);
    }
    std::vector<std::string> ln;
    ln.push_back(fit(sel == 0 ? phr_("pad_menu_fixed") : phr_("hot_desc"), 560));
    desc(c, 324, 44, ln);
    helpline(c, phr_("hot_help"));
}
inline void Ui::k_hot(UKey k, Act & a) {
    (void)a;
    hot_poll();
    if (hot_cap) {
        if (k == UK_CANCEL) { hot_cap = false; int z = 0; bool zb = false; if (env.pad_cap) env.pad_cap(2, z, zb); }
        return;
    }
    const int n = HOT_N + 1;
    if (k == UK_UP) sel = (sel + n - 1) % n;
    else if (k == UK_DOWN) sel = (sel + 1) % n;
    else if (k == UK_L) { scr = S_PAD; sel = pad_sel_keep; }
    else if (k == UK_CANCEL) { go(S_CONF, rowidx(S_CONF, 4)); }
    else if (k == UK_OK && sel >= 1 && env.pad_cap) {
        int z = 0; bool zb = false; env.pad_cap(0, z, zb); hot_cap = true; hot_cap_t0 = ts_ms;
    }
    else if (k == UK_TRI && sel >= 1) {
        gpad.erase(std::string("btn_hot_") + pad_fn_name(hot_fn(sel - 1))); dirty = true;
        if (env.pad_cap) { int z = 0; bool zb = false; env.pad_cap(3, z, zb); }
    }
}
