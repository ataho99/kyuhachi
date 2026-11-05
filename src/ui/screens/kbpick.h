#pragma once
/* screens/kbpick.h -- S_KBPICK: pick a PC-98 key on the keyboard layout (build 5a-2c). Cells come from the host (env.kbcells).
   Orange = already used by another button. Cursor moves by geometry. A = confirm, cancel = back to the list. */
inline bool Ui::kb_used(const std::string & nm) const {
    for (int i = 0; i < PAD_N + 8; i++) {
        if (i == pk_row) continue;
        if (i >= PAD_N && xb_get(i - PAD_N).empty()) continue;
        if (const_cast<Ui *>(this)->pad_val(i) == nm) return true;
    }
    return false;
}
inline void Ui::kb_open() {
    kb_cells = env.kbcells ? env.kbcells() : std::vector<KbCell>();
    kb_sel = 0; const std::string cv = pad_val(pk_row);
    for (size_t i = 0; i < kb_cells.size(); i++) if (kb_cells[i].name == cv) kb_sel = (int)i;
    scr = S_KBPICK;
}
inline void Ui::d_kbpick(Canvas & c) {
    titlebar(c, "");
    rbox(c, 8, 24, 624, 296, WH, (phr_("kbp_title") + std::string(": ") + pad_row_name(pk_row)).c_str());
    int mx = 1, mr = 1;
    for (auto & k : kb_cells) { if (k.x4 + k.w4 > mx) mx = k.x4 + k.w4; if (k.row + k.rows > mr) mr = k.row + k.rows; }
    const int sc = 600 / mx > 0 ? 600 / mx : 1, rh = 220 / mr > 34 ? 34 : 220 / mr;
    const std::string cv = pad_val(pk_row);
    for (int i = 0; i < (int)kb_cells.size(); i++) {
        const KbCell & k = kb_cells[i];
        const int x = 16 + k.x4 * sc, y = 36 + k.row * rh, w = k.w4 * sc - 2, h = k.rows * rh - 2;
        const bool on = i == kb_sel, us = kb_used(k.name), cu = k.name == cv;
        const uint32_t bg = on ? WH : 0x202020, fg = on ? K : us ? OR : cu ? CY : WH;
        c.rect(x, y, w, h, bg); c.clip(x, y, w, h); text(c, x + 2, y + 4, k.label, fg, bg); c.unclip();
    }
    if (kb_sel >= 0 && kb_sel < (int)kb_cells.size()) {
        const KbCell & k = kb_cells[kb_sel];
        std::string t = k.label + "  (" + k.name + ")"; if (kb_used(k.name)) t += std::string("  ") + phr_("kbp_used");
        text(c, 16, 36 + mr * rh + 10, fit(t, 600), YL, K);
    }
    helpline(c, phr_("kbp_help"));
}
inline void Ui::k_kbpick(UKey k, Act & a) {
    const int n = (int)kb_cells.size(); if (n < 1) { scr = S_PADKEY; return; }
    const KbCell & cu = kb_cells[kb_sel < n ? kb_sel : 0];
    const int cx = cu.x4 * 2 + cu.w4, cy = cu.row * 2 + cu.rows;   /* doubled centre */
    if (k == UK_LEFT || k == UK_RIGHT) {
        int best = -1, bd = 1 << 30;
        for (int i = 0; i < n; i++) { const KbCell & o = kb_cells[i]; if (i == kb_sel) continue;
            if (!(o.row < cu.row + cu.rows && cu.row < o.row + o.rows)) continue;
            const int dx = (o.x4 * 2 + o.w4) - cx; if (k == UK_RIGHT ? dx <= 0 : dx >= 0) continue;
            const int d = dx < 0 ? -dx : dx; if (d < bd) { bd = d; best = i; } }
        if (best >= 0) kb_sel = best;
    } else if (k == UK_UP || k == UK_DOWN) {
        int best = -1, bd = 1 << 30;
        for (int i = 0; i < n; i++) { const KbCell & o = kb_cells[i]; if (i == kb_sel) continue;
            const bool ok = k == UK_UP ? (o.row + o.rows <= cu.row) : (o.row >= cu.row + cu.rows); if (!ok) continue;
            const int dy = (o.row * 2 + o.rows) - cy, dx = (o.x4 * 2 + o.w4) - cx;
            const int d = (dy < 0 ? -dy : dy) * 1000 + (dx < 0 ? -dx : dx); if (d < bd) { bd = d; best = i; } }
        if (best >= 0) kb_sel = best;
    } else if (k == UK_OK) {
        const std::string o = cu.name, key = pad_key(pk_row);
        if (scope == 1 && pk_row < PAD_N) { if (env.pad_set && cur >= 0 && cur < (int)games.size()) env.pad_set(cur, key, o); }
        else gpad[key] = o;
        dirty = true; scr = pk_row >= PAD_N ? S_EXT : S_PAD; sel = pad_sel_keep; a.t = A_APPLY;
    } else if (k == UK_CANCEL) { open_padkey(pk_row); }
}
