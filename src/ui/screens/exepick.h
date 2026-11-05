#pragma once
/* screens/exepick.h -- 2b: S_EXEPICK. Pick the run file inside the disk image (sub folders too).
   Result: ed["dosexe"] = "<lower drive letter>:<dir\file>" or "*" (DOS prompt). */
inline void Ui::d_exepick(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 608, 296, WH, phr_("실행 파일 고르기"));
    int x = 36;
    for (int t = 0; t < xp_nd; t++) {
        std::string nm = std::string(1, (char)('A' + t)) + ":"; int w = tw(nm) + 12;
        if (t == xp_drv) { c.rect(x - 4, 42, w, 21, WH); text(c, x + 2, 45, nm, K, WH); } else text(c, x + 2, 45, nm, CY, K);
        x += w + 6;
    }
    text(c, 36, 68, fit(std::string(1, (char)('A' + xp_drv)) + ":\\" + xp_dir, 570), YL, K);
    std::vector<std::string> r; std::vector<uint32_t> col;
    r.push_back(phr_("(DOS 프롬프트로 시작 — 아무것도 실행하지 않음)")); col.push_back(GY);
    r.push_back(phr_("(자동으로 고름)")); col.push_back(GY);
    if (!xp_dir.empty()) { r.push_back(phr_(".. (위로)")); col.push_back(YL); }
    for (auto & f : xp_items) {
        if (f.dir) { r.push_back("[" + f.name + "]"); col.push_back(YL); }
        else { r.push_back(fit(f.name, 380) + "  " + std::to_string((long long)f.size) + " B"); col.push_back(0); }
    }
    rowlist(c, 40, 94, 560, 9, r, sel, 24, &col);
    std::vector<std::string> d;
    d.push_back(xp_err ? std::string(phr_("디스크 이미지를 읽지 못했습니다."))
                       : std::string(phr_("디스크 이미지 안의 실행 파일을 직접 고릅니다.")));
    d.push_back(phr_("내장 DOS 로 시작하는 작품에 적용됩니다."));
    const char * H = phr_("↑↓:선택, ○:고르기/열기, L R:디스크 A/B, ×:취소");
    desc(c, 324, 58, d, -1, H); helpline(c, H);
}
inline void Ui::k_exepick(UKey k, Act & a) {
    (void)a;
    auto mv = [&](int & s, int cnt) {
        if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    const int base = 2 + (xp_dir.empty() ? 0 : 1); mv(sel, base + (int)xp_items.size());
    auto back = [&]() { scr = S_EDIT; sel = 12; };
    if (k == UK_L || k == UK_R) { if (xp_nd > 1) { xp_drv = 1 - xp_drv; xp_dir.clear(); xp_load(); sel = 0; } }
    else if (k == UK_OK) {
        if (sel == 0) { ed["dosexe"] = "*"; back(); }
        else if (sel == 1) { ed.erase("dosexe"); back(); }
        else if (!xp_dir.empty() && sel == 2) {
            size_t p = xp_dir.find_last_of('\\'); xp_dir = p == std::string::npos ? std::string() : xp_dir.substr(0, p);
            xp_load(); sel = 0;
        } else if (sel - base >= 0 && sel - base < (int)xp_items.size()) {
            const FileItem f = xp_items[sel - base];
            if (f.dir) { xp_dir += (xp_dir.empty() ? std::string() : std::string("\\")) + f.name; xp_load(); sel = 0; }
            else {
                ed["dosexe"] = std::string(1, (char)('a' + xp_drv)) + ":" + (xp_dir.empty() ? std::string() : xp_dir + "\\") + f.name;
                back();
            }
        }
    }
    else if (k == UK_CANCEL) back();
}
