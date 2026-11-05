#pragma once
/* screens/pad.h -- S_PAD: 패드 버튼 배정 (빌드 5a: 버튼 16개 · 패드 모양 배치. 왼쪽 목록 = 고르는 줄, 오른쪽 = 배치 그림) */
inline void Ui::d_pad(Canvas & c) {
    titlebar(c, phr_("조작 설정"));
    Settings & s = eff();
    std::map<std::string, std::string> wm; if (scope == 1 && env.pad_get && cur >= 0 && cur < (int)games.size()) env.pad_get(cur, wm);   /* 한 번만 읽는다 */
    auto val = [&](int i, bool & fw, bool & df) -> std::string {
        std::string k = pad_key(i); fw = false; df = false;
        auto it = wm.find(k); if (it != wm.end() && !it->second.empty()) { fw = true; return it->second; }
        auto g2 = gpad.find(k); if (g2 != gpad.end() && !g2->second.empty()) return g2->second;
        df = true; return pad_default(i); };
    rbox(c, 16, 24, 288, 296, WH, (std::string("[") + phr_("패드 버튼") + "] " + phr_("tab_hot") + " (R)").c_str());
    std::vector<std::string> r = { phr_("적용 범위"), phr_("pad_decide") };
    for (int i = 0; i < PAD_N; i++) { bool fw, df; val(i, fw, df); r.push_back(std::string(fw ? "* " : "  ") + fit(pad_row_name(i), 220)); }
    std::vector<uint32_t> col(r.size(), 0u);
    rowlist(c, 40, 44, 256, 11, r, sel, 24, &col);
    rbox(c, 320, 24, 304, 296, WH, phr_("인포메이션"));
    const char * sc[] = { phr_("전역"), phr_("scope_this") }; choices(c, 344, 44, sc, 2, scope, WH, sel == 0);
    const std::string ly0 = std::string("\xE2\x86\x92") + (pad_tag(0).empty() ? "" : " (" + pad_tag(0) + ")"), ly1 = std::string("\xE2\x86\x93") + (pad_tag(1).empty() ? "" : " (" + pad_tag(1) + ")");
    const char * ly[] = { ly0.c_str(), ly1.c_str() }; choices(c, 344, 68, ly, 2, s.pad == 1 ? 1 : 0, WH, sel == 1);
    static const int LC[8] = { 12, 4, 8, 10, 11, 9, 14, 6 }, RC[8] = { 13, 5, 7, 2, 0, 1, 15, 3 };   /* 패드 왼쪽 · 오른쪽 버튼 (위에서 아래로) */
    for (int side = 0; side < 2; side++) {
        const int gx = side ? 476 : 328;
        rbox(c, gx, 96, 140, 204, GY, NULL);
        for (int k = 0; k < 8; k++) {
            const int b = side ? RC[k] : LC[k]; bool fw, df; std::string v = val(b, fw, df);
            const std::string t = std::string(fw ? "*" : "") + pad_tag(b) + " " + pad_show_s(v);
            const int yy = 104 + k * 24; const bool on = sel == b + 2;
            if (on) c.rect(gx + 4, yy - 3, 132, 22, WH);
            c.clip(gx + 8, yy - 3, 126, 22); text(c, gx + 8, yy, t, on ? K : WH, on ? WH : K); c.unclip();
        }
    }
    text(c, 332, 302, fit(phr_("pad_menu_fixed"), 284), CY, K);
    std::string ln;
    if (sel < 2) ln = sel == 0 ? phr_("전역=모든 작품, 이 작품=이 작품만. 작품 값이 우선.") : phr_("pad_decide_help");
    else { bool fw, df; std::string v = val(sel - 2, fw, df); ln = pad_row_name(sel - 2) + " → " + pad_show(v) + ((scope == 1 && !fw) ? (df ? phr_(" (기본)") : phr_(" (전역)")) : (scope == 0 && df ? phr_(" (기본)") : ""));
        if (sel >= 10 && sel <= 13) ln += std::string("  ") + phr_("pad_stick_note"); }
    desc(c, 324, 44, { fit(dkey(ln), 560) });
    helpline(c, phr_("↑↓:선택, ←→:값, ○:키 고르기, ×:뒤로, △:출하 시 설정"));
}
inline void Ui::k_pad(UKey k, Act & a) {
    struct Gp { Ui & u; bool have; Settings pre; int gi; ~Gp() { if (have && u.scope == 1 && u.cur == gi && gi >= 0 && gi < Ui::MAXG) { unsigned d = Ui::diff_bits(pre, u.t[gi]); if (d) u.ov_set(gi, u.ovm[gi] | d); u.sync_t(gi); } } } gp{ *this, false, Settings(), cur };   /* #587: 이 작품에서 바꾼 줄만 개별 값 */
    if (scope == 1 && cur >= 0 && cur < MAXG) { sync_t(cur); gp.pre = t[cur]; gp.have = true; }
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            if (k == UK_R) { pad_sel_keep = sel; scr = S_HOT; sel = 0; hot_cap = false; break; }
            mv(sel, PAD_N + 2);
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = (k == UK_RIGHT) ? 1 : -1;
                if (sel == 0) { scope_step(d); }
                else if (sel == 1) { Settings & s = edit(); int nv = clampi(s.pad + d, 0, 1); if (nv != s.pad) { s.pad = nv; dirty = true; a.t = A_APPLY; } }
            } else if (k == UK_OK) {
                if (sel >= 2) open_padkey(sel - 2);
                else if (sel == 1) { Settings & s = edit(); s.pad = s.pad == 1 ? 0 : 1; dirty = true; a.t = A_APPLY; }
            } else if (k == UK_TRI) {   /* 출하 시 설정: 이 범위의 버튼 줄을 모두 지움, 결정 방식은 일본식 */
                if (scope == 0) { gpad.clear(); eff().pad = 0; }
                else if (cur >= 0 && cur < (int)games.size()) { if (env.pad_get && env.pad_set) { std::map<std::string, std::string> m; env.pad_get(cur, m); for (auto & kv : m) env.pad_set(cur, kv.first, std::string()); } gp.have = false; ov_set(cur, ovm[cur] & ~(1u << 9)); sync_t(cur); }
                dirty = true; a.t = A_APPLY;
            } else if (k == UK_CANCEL) { go(S_CONF, rowidx(S_CONF, 4)); }
            break; }
    } while (0);
}
