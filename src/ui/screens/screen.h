#pragma once
/* screens/screen.h -- S8b: S_SCREEN screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_screen(Canvas & c) {   /* t48c (#624·#625): 필터 1줄 + 효과 4줄 + 곡면. #587: 범위 둘(전역/이 작품) · 이 작품에서는 전역 전용 줄을 숨기고 맨 아래 「개별 설정 초기화」 · 개별 값 줄 앞 ● */
    titlebar(c, phr_("화면 설정"));
    Settings & s = eff();
    rbox(c, 16, 24, 288, 220, WH, phr_("화면 설정"));
    const std::vector<int> ri = rowids(S_SCREEN);
    const bool own = scope == 1 && cur >= 0 && cur < MAXG; const bool ind = own && ovm[cur] != 0u;
    static const unsigned rb[13] = { 0u, 1u << 0, 1u << 1, 1u << 5, 1u << 6, 1u << 3, 1u << 4, 1u << 2, 0u, 0u, 1u << 16, 0u, 127u };
    std::vector<std::string> n;
    { const std::string nm[13] = { phr_("적용 범위"), phr_("비율"), phr_("필터"), phr_("주사선"), phr_("마스크"), phr_("RGB 번짐"), phr_("빛 번짐"), phr_("곡면"), phr_("창 배율"), phr_("전체화면"), "FPS / Speed", phr_("indiv_reset"), phr_("preset") };
      for (int id : ri) n.push_back((own && (ovm[cur] & rb[id])) ? std::string("● ") + nm[id] : nm[id]); }
    const int rs = (sel >= 0 && sel < (int)ri.size()) ? ri[sel] : 0;
    items(c, 40, 44, 256, n, sel, 16);
    rbox(c, 320, 24, 304, 220, WH, phr_("선택지"));
    auto row = [&](int i) { return rs == i; };
    const int PY = 16;
    auto Y = [&](int id) { int k = 0; for (size_t i = 0; i < ri.size(); i++) if (ri[i] == id) k = (int)i; return 44 + k * PY; };
    const char * sc[] = { phr_("전역"), phr_("scope_this") }; choices(c, 344, Y(0), sc, 2, scope, WH, row(0));
    { const int pp = pre_pos(); const int nb = PRE_NB + (int)presets.size(); const std::string pn = pp >= 0 ? pre_name(pp) : std::string(phr_("preset_custom")); (void)nb; text(c, 344, Y(12), std::string("< ") + pn + " >", row(12) ? CY : WH, K); }   /* 빌드 6: 프리셋 줄 */
    const char * rt[] = { "4:3", "16:10", phr_("정수배") }; choices(c, 344, Y(1), rt, 3, s.ratio, WH, row(1));
    const char * sh[] = { "sharp", "nearest" }; choices(c, 344, Y(2), sh, 2, s.shader == 2 ? 1 : 0, WH, row(2));
    char b[16]; const int fxv[4] = { s.scan, s.mask, s.bleed, s.glow };
    for (int i = 0; i < 4; i++) { int y = Y(3 + i); if (fxv[i] <= 0) snprintf(b, sizeof b, "끔"); else snprintf(b, sizeof b, "%d%%", fxv[i] * 10); barv(c, 340, y, 232, fxv[i] / 10.0, phr_("끔"), phr_("최대"), WH, row(3 + i)); text(c, 578, y, b, WH, K); }
    snprintf(b, sizeof b, "%.2f", s.curve_f()); barv(c, 340, Y(7), 232, s.curve / 10.0, phr_("평면"), phr_("최대"), WH, row(7)); text(c, 578, Y(7), b, WH, K);
#if !defined(__ANDROID__)
    if (scope == 0) { const char * wm[] = { "x1", "x2", "x3", "x4", "x5", phr_("사용자 지정") }; unsigned dmk = 0; for (int i = 0; i < WMN; i++) if (i + 1 > wmax) dmk |= 1u << i;
      choicesd(c, 344, Y(8), wm, 6, wmult ? wmult - 1 : 5, WH, dmk);
      const char * fsc[] = { phr_("창 모드"), phr_("전체화면") }; choices(c, 344, Y(9), fsc, 2, fs_now ? 1 : 0, WH, row(9)); }   /* t52i (B3) */
#endif
    { const char * fp[] = { "OFF", "ON" }; choices(c, 344, Y(10), fp, 2, s.fpsd ? 1 : 0, WH, row(10)); }
    rbox(c, 16, 248, 608, 82, WH, phr_("미리보기"));
    thumb(c, game(), 40, 266, 96, 60);
    const UiGame * gm = game();
    { std::string sf = gm && gm->lastslot >= 0 ? phr_("  마지막 저장 (") + gm->slot[gm->lastslot].stamp + ")" : std::string(); text(c, 156, 266, gm && gm->lastslot >= 0 ? fit_title(gm->title, 440 - tw(sf)) + sf : std::string(phr_("표본 화면")), CY, K); }
    text(c, 156, 286, rs == 0 && scope == 0 ? dkey(phr_("scr_allglobal_off")) : (ind ? std::string(phr_("indiv_has")) : std::string(phr_("값을 바꾸면 이 화면에도 바로 적용됩니다."))), WH, K);
    text(c, 156, 306, dkey(phr_("{○}:확정   {×}:바꾸기 전 값으로 되돌림")), WH, K);
    char wb[96]; if (wmult) snprintf(wb, sizeof wb, "창 x%d (%dx%d) · 회색 = 모니터에 안 들어감 · Alt+[ ]", wmult, 640 * wmult, 480 * wmult); else snprintf(wb, sizeof wb, "창 사용자 지정 %dx%d · 테두리로 끈 크기 · Alt+[ ]", winw, winh);
    static std::string wd; wd = wb;
    static const char * dd[] = { "desc_scope", "desc_ratio", "desc_filter", "desc_scan", "desc_mask", "desc_bleed", "desc_glow", "desc_curve" };
    desc_mq(c, 324, 44, rs < 8 ? dkey(phr_(dd[rs])) : rs == 12 ? dkey(phr_("desc_preset")) : rs == 11 ? dkey(phr_("desc_indiv_reset")) : rs == 10 ? dkey(phr_("desc_fps")) : rs == 9 ? dkey(phr_("desc_fullscr")) : wd);
    helpline(c, phr_("↑↓:선택, ←→:값, ○:결정, ×:취소(되돌림), △:출하 시 설정"));
}
inline Act Ui::k_screen_conf(UKey k, Act & a) {
    const Scr scr0 = scr; const std::vector<int> ri = rowids(scr0); const int sidx = sel; const int sid = (sidx >= 0 && sidx < (int)ri.size()) ? ri[sidx] : 0;
    struct Gd { Ui & u; Scr s0; int sidx, sid, gi; bool have; Settings pre;   /* #587: 줄 위치 ↔ 실제 줄 번호 · 이 작품 범위에서 바뀐 줄만 개별 값으로 */
        ~Gd() { if (u.scr != s0) return; if (u.sel == sid) u.sel = sidx;
                if (have && u.scope == 1 && u.cur == gi && gi >= 0 && gi < Ui::MAXG) { unsigned d = Ui::diff_bits(pre, u.t[gi]); if (d) u.ov_set(gi, u.ovm[gi] | d); u.sync_t(gi); } } } gd{ *this, scr0, sidx, sid, cur, false, Settings() };
    if (scope == 1 && cur >= 0 && cur < MAXG) { sync_t(cur); gd.pre = t[cur]; gd.have = true; }
    sel = sid;
    do { {
        if ((scr == S_SCREEN && sel == 11) || (scr == S_CONF && sel == 14)) { if (k == UK_OK) { sel = sidx; yn_open(scr0, YN_INDIVRESET, phr_("indiv_q1"), phr_("indiv_q2")); } break; }   /* 개별 설정 초기화 */
        if (scr == S_CONF && sel == 13 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT)) { int dl = (k == UK_LEFT) ? -1 : 1; lang = (lang + dl + 3) % 3; phr_set_lang(lang); lang_chg = true; dirty = true; break; }   /* t52g (#133): 언어 — 문구 칸이 제자리에서 바뀌어 바로 보인다 */
        if (scr == S_CONF && sel >= 11 && sel <= 12 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT || k == UK_TRI)) { if (k == UK_OK) open_font(sel - 11, S_CONF); break; }   /* t52f: 글꼴 행이 11·12 로 한 칸씩 내려감 */
        if (scr == S_CONF && sel == 4 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT)) { if (k == UK_OK) go(S_PAD, 0); break; }   /* t48e (#646) */
#if defined(__ANDROID__)
        if (scr == S_CONF && sel == 3 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT)) { g_sndbuf = (g_sndbuf + (k == UK_LEFT ? 3 : 1)) % 4; if (g_kha_save) g_kha_save(); dirty = true; break; }   /* t137: 소리 버퍼 */
#endif
        bool scr_s = scr == S_SCREEN; int nrow = scr_s ? 11 : 14;
        if (scr_s && sel == 9 && (k == UK_LEFT || k == UK_RIGHT || k == UK_OK)) { a.t = A_FULLSCR; return a; }   /* t52i (B3): 전체화면 줄 — 좌우·○ 로 바로 오간다 */
        if (scr_s && sel == 12 && (k == UK_LEFT || k == UK_RIGHT || k == UK_OK)) {   /* 빌드 6: 프리셋 — ←→ 고르면 바로 적용(즉시 미리보기), ○ = 저장 칸에서 저장 · 내 프리셋에서 삭제 확인 */
            const int np = (int)presets.size(), tot = pre_total(); int p = pre_pos();
            if (k == UK_OK) {
                if (p >= PRE_NB && p < PRE_NB + np) { sel = sidx; yn_open(scr0, YN_PREDEL, phr_("preset_q1"), phr_("preset_q2"), std::string(), p); }
                else if (p == PRE_NB + np && np < PRE_MAX) { pre_add(eff()); }
                break; }
            if (p < 0) p = (k == UK_RIGHT) ? tot - 1 : 0;
            p = (p + (k == UK_RIGHT ? 1 : tot - 1)) % tot; pre_i = p;
            if (p < PRE_NB + np) { pre_copy(eff(), pre_get(p)); a.t = A_APPLY; dirty = true; }
            return a; }
        if (scr_s && sel == 10 && (k == UK_LEFT || k == UK_RIGHT || k == UK_OK)) { Settings & fs = eff(); fs.fpsd = fs.fpsd ? 0 : 1; a.t = A_APPLY; dirty = true; return a; }
        if (sel == 0 && k == UK_OK && scope == 0) { sel = sidx; yn_open(scr0, YN_INDIVALL, phr_("indiv_allq1"), phr_("indiv_q2")); break; }   /* t147d: 전역 범위에서 ○ = 전역 일괄 적용 켬/끔 */
        Settings & s = (sel == 0) ? eff() : edit();
        if (k == UK_LEFT || k == UK_RIGHT) {
            int d = (k == UK_RIGHT) ? 1 : -1;
            if (scr_s && sel == 8) {   /* 창 배율(전역): 모니터에 들어가는 다음/이전 배율 */
                int m = wm_next(d); if (m > 0) { wmult = m; a.t = A_WINMULT; a.arg = m; dirty = true; }
                return a;
            }
            if (sel == 0) { scope_step(d); }
            else if (scr_s) {
                if (sel == 1) s.ratio = clampi(s.ratio + d, 0, 2);
                if (sel == 2) s.shader = d > 0 ? 2 : 0;   /* t48c: 필터 = sharp / nearest */
                { if (sel == 3) s.scan = clampi(s.scan + d, 0, 10); if (sel == 4) s.mask = clampi(s.mask + d, 0, 10); if (sel == 5) s.bleed = clampi(s.bleed + d, 0, 10); if (sel == 6) s.glow = clampi(s.glow + d, 0, 10); if (sel == 7) s.curve = clampi(s.curve + d, 0, 10); }   /* t48c: 필터와 상관없이 효과를 켠다. 바꾸면 A_APPLY 로 곧바로 화면에 반영 */
                a.t = A_APPLY;
            } else {
                if (sel == 1) s.bgm = clampi(s.bgm + d, 0, 8);
                if (sel == 2) s.se = clampi(s.se + d, 0, 8);
                if (sel == 5) s.bmult = clampi(s.bmult + d, 0, 4);
                if (sel == 6) s.bmode = clampi(s.bmode + d, 0, 2);
                if (sel == 7) s.bsound = clampi(s.bsound + d, 0, 2);
                if (sel == 8) s.boot = clampi(s.boot + d, 0, 1); if (sel == 9) s.bosd = clampi(s.bosd + d, 0, 1); if (sel == 10) s.ovbtn = clampi(s.ovbtn + d, 0, 1);
                a.t = A_APPLY;
            }
            dirty = true; (void)nrow;
        } else if (k == UK_TRI) { Settings def; Settings & e = eff(); if (scr_s) { e.ratio = def.ratio; e.shader = def.shader; e.curve = def.curve; e.bleed = def.bleed; e.glow = def.glow; e.scan = def.scan; e.mask = def.mask; e.fpsd = def.fpsd; } else { e.bgm = def.bgm; e.se = def.se; e.bmult = def.bmult; e.bmode = def.bmode; e.boot = def.boot; e.bosd = def.bosd; e.bsound = def.bsound; e.ovbtn = def.ovbtn; } dirty = true; a.t = A_APPLY; }
        else if (k == UK_CANCEL) { if (!scr_s) { cb_has = false; a.t = A_APPLY; dirty = true; }   /* t52i: × = 뒤로. 값은 바뀐 순간부터 저장 대상 */ if (scr_s && backup_has) { eff() = backup; if (bk_cur >= 0 && bk_cur < MAXG) { ov_set(bk_cur, bk_ovm); sync_t(bk_cur); } backup_has = false; a.t = A_APPLY; if (wmult != bk_wmult || winw != bk_winw || winh != bk_winh) { wmult = bk_wmult; winw = bk_winw; winh = bk_winh; a.t = A_WINMULT; a.arg = wmult; } } leave_cfg(); }
        else if (k == UK_OK) { backup_has = false; cb_has = false; dirty = true; a.t = A_APPLY; leave_cfg(); }
        break; }
    } while (0);
    return a;
}
