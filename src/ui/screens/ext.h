#pragma once
/* screens/ext.h -- S_EXT: extra-buttons tab (build 5a-2b). Register a device button by pressing it (key code or joystick button),
   then give it a key / function with the normal picker.
   Stored global: btn_xreg<n> = key|<code>|<name> (joy|<n>|<name>), btn_ext<n> = value. */
inline int Ui::xb_cnt() const { int c = 0; for (int n = 0; n < 8; n++) if (!xb_get(n).empty()) c++; return c; }
inline int Ui::xb_slot(int row) const {
    int c = 0; for (int n = 0; n < 8; n++) if (!xb_get(n).empty()) { if (c == row) return n; c++; }
    return -1; }
inline void Ui::d_ext(Canvas & c) {
    if (ext_wait) {   /* waiting for the device button to register */
        const std::string r = env.raw_line ? env.raw_line(-2) : std::string();
        if (!r.empty()) {
            int fs = -1; for (int n = 0; n < 8 && fs < 0; n++) if (xb_get(n).empty()) fs = n;
            if (fs >= 0) { gpad[std::string("btn_xreg") + std::to_string(fs)] = r; dirty = true; sel = xb_cnt() - 1; if (sel < 0) sel = 0; }
            ext_wait = false; if (env.raw_ctl) env.raw_ctl(0); if (env.pad_cap) { int z = 0; bool zb = false; env.pad_cap(3, z, zb); }
        } else if ((env.raw_line && env.raw_line(-1) == "EXIT") || ts_ms - ext_t0 > 20000) {
            ext_wait = false; if (env.raw_ctl) env.raw_ctl(0); }
    }
    titlebar(c, phr_("hot_title"));
    rbox(c, 16, 24, 608, 296, WH, (std::string(phr_("hot_tabpad")) + " [" + phr_("tab_ext") + "]").c_str());
    const int cnt = xb_cnt(), n = cnt < 8 ? cnt + 1 : cnt;
    for (int k = 0; k < n; k++) {
        const int yy = 44 + k * 32; const bool on = sel == k;
        if (on) c.rect(28, yy - 4, 584, 28, WH);
        c.clip(32, yy - 4, 576, 28);
        if (k < cnt) {
            const int s = xb_slot(k); const std::string v = xb_get(s);
            const size_t a = v.find('|'), b = a == std::string::npos ? a : v.find('|', a + 1);
            const std::string kind = v.substr(0, a == std::string::npos ? 0 : a);
            const std::string code = b == std::string::npos ? std::string() : v.substr(a + 1, b - a - 1);
            text(c, 40, yy, fit(xb_name(s), 180), on ? K : WH, on ? WH : K);
            text(c, 230, yy, fit((kind == "joy" ? std::string("JOY ") : std::string("KEY ")) + code, 170), on ? K : GY, on ? WH : K);
            text(c, 410, yy, fit(pad_show(pad_val(PAD_N + s)), 196), on ? K : CY, on ? WH : K);
        } else text(c, 40, yy, fit(phr_("ext_add"), 560), on ? K : YL, on ? WH : K);
        c.unclip();
        Hit ht = { 28, yy - 4, 584, 28, k, sel }; hits.push_back(ht);
    }
    if (!cnt) text(c, 40, 44 + 40 * 5, fit(phr_("ext_empty"), 560), GY, K);
    if (ext_wait) {
        fillbox(c, 100, 110, 440, 110, K, YL, NULL);
        text(c, 124, 128, fit(phr_("ext_wait_title"), 400), YL, K);
        text(c, 124, 160, fit(phr_("ext_wait_sub"), 400), WH, K);
        int left = 20 - (ts_ms - ext_t0) / 1000; if (left < 0) left = 0;
        text(c, 124, 188, std::to_string(left) + " s", CY, K);
    }
    std::vector<std::string> ln; ln.push_back(fit(phr_("ext_desc"), 560));
    desc(c, 324, 44, ln);
    helpline(c, phr_("ext_help"));
}
inline void Ui::k_ext(UKey k, Act & a) {
    (void)a;
    if (ext_wait) return;
    const int cnt = xb_cnt(), n = cnt < 8 ? cnt + 1 : cnt;
    if (k == UK_UP) sel = (sel + n - 1) % n;
    else if (k == UK_DOWN) sel = (sel + 1) % n;
    else if (k == UK_L) { scr = S_HOT; sel = 0; }
    else if (k == UK_CANCEL) { go(S_CONF, rowidx(S_CONF, 4)); }
    else if (k == UK_OK) {
        if (sel < cnt) open_padkey(PAD_N + xb_slot(sel));
        else if (env.raw_ctl) { ext_wait = true; ext_t0 = ts_ms; env.raw_ctl(2); }
    } else if (k == UK_TRI && sel < cnt) {
        const int s = xb_slot(sel);
        gpad.erase(std::string("btn_xreg") + std::to_string(s)); gpad.erase(std::string("btn_ext") + std::to_string(s)); dirty = true;
        if (env.pad_cap) { int z = 0; bool zb = false; env.pad_cap(3, z, zb); }
        a.t = A_APPLY; if (sel > 0 && sel >= xb_cnt()) sel = xb_cnt() > 0 ? xb_cnt() - 1 : 0;
    }
}
