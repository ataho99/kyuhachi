#pragma once
/* screens/pick.h -- S8a: S_PICK screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_pick(Canvas & c) {
    titlebar(c, phr_("작품 선택"));
    std::string bt = std::string(phr_("작품 선택  ")) + (sortfav ? "★" : "") + sort_name(sortm) + (reorder ? phr_("  [순서 이동]") : "");
    rbox(c, 16, 24, 288, 224, WH, bt.c_str());
    { int x = 30, nun = 0; for (size_t q = 0; q < games.size(); q++) if (games[q].variant_of.empty() && games[q].lang.empty()) nun++; for (int t = 0; t < 4; t++) { std::string nm = tab_name(t); if (t == TAB_UN && nun > 0) nm += "(" + std::to_string(nun) + ")"; int w = tw(nm) + 12; if (t == tab) { c.rect(x - 4, 42, w, 21, WH); text(c, x + 2, 45, nm, K, WH); } else text(c, x + 2, 45, nm, CY, K); x += w + 4; } }
    int nv = (int)vis.size(), n = nv + 1;
    if (pick != pick_seen) { pick_seen = pick; pick_since = ts_ms; }
    const int VIS = 7, PT = 24, LX = 40, LWD = 256, AVAIL = 236, Y0 = 70; int top = pick - 3; if (top > n - VIS) top = n - VIS; if (top < 0) top = 0;
    for (int k = 0; k < VIS && top + k < n; k++) {
        int i = top + k, yy = Y0 + k * PT; bool add = i >= nv; int gi = add ? -1 : vis[i];
        bool d = !add && !blk_row(gi).empty(), fv = !add && games[gi].fav; int wn = add ? 0 : row_warn(gi);
        int s0 = fv ? 16 : 0, ww = wn ? 16 : 0, av = AVAIL - s0 - ww; std::string full = add ? std::string(phr_("+ 작품 추가")) : games[gi].title; bool sl = i == pick;
        { Hit ht = { LX - 8, yy - 3, LWD + 2, 22, i, pick }; hits.push_back(ht); }   /* t55a */
        if (sl) c.rect(LX - 8, yy - 3, LWD + 2, 22, d ? GY : WH);
        if (fv) text(c, LX, yy, "★", sl ? K : YL, sl ? (d ? GY : WH) : K);
        if (wn) text(c, LX + AVAIL - 16, yy, "※", sl ? K : OR, sl ? (d ? GY : WH) : K);
        c.clip(LX + s0, yy - 3, av, 22);
        if (sl) text(c, LX + s0 - mq_off(full, av), yy, full, K, d ? GY : WH); else text(c, LX + s0, yy, add ? full : ttl_fit(games[gi], av), d ? GY : CY, K);
        c.unclip();
    }
    if (top > 0) text(c, 284, 30, "↑", CY, K); if (top + VIS < n) text(c, 284, 228, "↓", CY, K);
    rbox(c, 320, 24, 304, 224, WH, phr_("인포메이션"));
    if (pick >= nv) {
        text(c, 344, 48, phr_("새 작품을 등록합니다."), YL, K);
        text(c, 344, 78, phr_("이미지 파일(zip 포함)이나 폴더를"), WH, K); text(c, 344, 98, phr_("고르면 형식과 구조를 판정해"), WH, K); text(c, 344, 118, phr_("자동으로 등록합니다."), WH, K);
        text(c, 344, 158, phr_("게임 폴더를 지정해 두면 켤 때마다"), CY, K); text(c, 344, 178, phr_("새 작품을 찾아 자동 등록합니다."), CY, K);
        desc(c, 264, 64, { phr_("+ 작품 추가: 이미지 파일 · 폴더 · 게임 폴더 지정"), phr_("회색 작품은 아직 실행할 수 없는 작품입니다 (사유는 인포메이션).") });
        helpline(c, phr_("↑↓:선택, ○:작품 추가 화면, ×:취소")); return;
    }
    int gi = vis[pick]; const UiGame & gb = games[gi]; const UiGame & g = games[(cur >= 0 && cur < (int)games.size()) ? cur : gi]; const std::string & bk = blk_cur(); bool blk = !bk.empty();
    const int IX = 344, IW = 256; int LB = 48;   /* t52h (#134): 라벨 칸 폭 = 가장 긴 라벨 + 8 */
    { const char * const lb[5] = { phr_("제작"), phr_("실행"), phr_("판정"), phr_("사유"), phr_("저장") }; for (int q = 0; q < 5; q++) { int t = tw(lb[q]) + 8; if (t > LB) LB = t; } }
    c.clip(IX - 4, 28, IW + 8, 216);
    { std::vector<std::string> tl = title_lines(gb.title, IW, 2); for (size_t i = 0; i < tl.size(); i++) text(c, IX, 34 + (int)i * 20, tl[i], blk ? WH : YL, K); }
    thumb(c, &g, IX, 78, 128, 80);
    { text(c, IX + 140, 80, phr_("디스크"), CY, K); std::vector<std::string> dl = clamp_lines(wrap_words(g.disknote.empty() ? std::string("-") : g.disknote, IW - 140), 3, IW - 140); for (size_t i = 0; i < dl.size(); i++) text(c, IX + 140, 98 + (int)i * 18, dl[i], WH, K); }
    int yy = 164;
    auto row = [&](const char * lab, const std::string & v, uint32_t col, int maxl) {
        text(c, IX, yy, lab, CY, K);
        std::vector<std::string> ls = clamp_lines(wrap_words(v.empty() ? std::string("-") : v, IW - LB), maxl, IW - LB); if (ls.empty()) ls.push_back("-");
        for (size_t i = 0; i < ls.size(); i++) text(c, IX + LB, yy + (int)i * 18, ls[i], col, K);
        yy += 18 * (int)ls.size();
    };
    row(phr_("제작"), mk_line(gb), WH, 1);
    { int ei = row_ent(gi); std::string ev; uint32_t col = WH;
      if (ei < 0) ev = phr_("자동 판정"); else { std::vector<int> v = tvents(gi); int idx = 0; for (size_t q = 0; q < v.size(); q++) if (v[q] == ei) idx = (int)q; const UiEnt & e = gb.ents[ei];
        ev = std::string(v.size() > 1 ? "← " : "") + e.label + (v.size() > 1 ? " →" : "") + " (" + std::to_string(idx + 1) + "/" + std::to_string((int)v.size()) + ")"; if (ent_warn(gi, ei)) col = OR; }
      row(phr_("실행"), fit(ev, IW - LB), col, 1); }
    row(phr_("판정"), g.jshort.empty() ? std::string("-") : g.jshort, WH, 1);
    if (blk) row(phr_("사유"), bk, OR, 1);
    else { std::string st = phr_("없음"); if (g.lastslot >= 0 && g.slot[g.lastslot].used) st = phr_("슬롯 ") + slot_lbl(g.lastslot) + "  " + g.slot[g.lastslot].stamp; row(phr_("저장"), st, WH, 1); }
    c.unclip();
    std::vector<std::string> d;
    if (blk) d = { fit_title(gb.title, 560 - tw(phr_("   [실행 불가]"))) + phr_("   [실행 불가]"), fit(phr_("사유: ") + bk, 560) };
    else { std::string suf = g.lastslot >= 0 && g.slot[g.lastslot].used ? phr_("   마지막 저장 ") + g.slot[g.lastslot].stamp : std::string(phr_("   처음부터 시작합니다")); d = { fit_title(gb.title, 560 - tw(suf)) + suf, fit(g.judge.empty() ? (g.disk[0] + (g.disk[1].empty() ? std::string() : "    " + g.disk[1])) : g.judge, 560) }; }
    desc(c, 264, 64, d, blk ? 1 : -1);
    helpline(c, reorder ? phr_("↑↓:순서 이동, ○ 또는 ×:끝") : phr_("↑↓:작품, ←→:실행 방법, L R:탭, ○:정함, □:편집, △:정렬, SEL:부가, ×"));
}
inline void Ui::k_pick(UKey k, Act & a) {
    do { {
            int nv = (int)vis.size();
            if (reorder) { if (k == UK_OK || k == UK_CANCEL) { reorder = false; dirty = true; a.t = A_APPLY; } break; }
            if (k == UK_L || k == UK_R) { int keep = cur; tab = (tab + (k == UK_R ? 1 : 3)) % 4; rebuild_vis(keep); dirty = true; break; }
            if (pick >= nv) { if (k == UK_OK) go(S_ADD, 0); else if (k == UK_CANCEL) go(S_MAIN, 0); break; }
            int gi = vis[pick];
            if (k == UK_LEFT || k == UK_RIGHT) { std::vector<int> v = tvents(gi); if (v.size() > 1) { int ei = row_ent(gi), idx = 0; for (size_t q = 0; q < v.size(); q++) if (v[q] == ei) idx = (int)q; idx = (idx + (k == UK_RIGHT ? 1 : (int)v.size() - 1)) % (int)v.size(); if ((int)rent.size() <= gi) rent.resize(games.size(), -1); rent[gi] = v[idx]; sync_cur(); } break; }
            if (k == UK_OK) { sync_cur(); if (!game() || game()->id.empty()) { mainsel = 0; go(S_MAIN, 0); } else { start_ret = S_PICK; if (has_save(game()) || !blk_cur().empty()) { startsel = has_save(game()) ? 0 : 1; go(S_START, startsel); } else { startsel = 1; start_ok(a); } } }   /* t52i (B1): 홈 = 작품 목록, ○ = 시작 */
            else if (k == UK_CANCEL) go(S_MAIN, 0);
            else if (k == UK_SELECT) { sync_cur(); go(S_EXTRA, 0); back = S_PICK; }
            else if (k == UK_SQ) { entg = gi; msg.clear(); std::vector<int> v = vents(gi); int ei = row_ent(gi), idx = 0; for (size_t q = 0; q < v.size(); q++) if (v[q] == ei) idx = (int)q; scr = S_ENT; sel = idx; }
            else if (k == UK_TRI) { sel = sortm; scr = S_SORT; }
            break; }
    } while (0);
}
