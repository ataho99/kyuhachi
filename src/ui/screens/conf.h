#pragma once
/* screens/conf.h -- S8b: S_CONF screen. #587: 범위 둘(전역/이 작품) · 이 작품에서는 전역 전용 줄(소리 버퍼·글꼴·언어)을 숨기고 맨 아래 「개별 설정 초기화」 · 개별 값 줄 앞 ● */
inline void Ui::d_conf(Canvas & c) {
    titlebar(c, conf_grp == 1 ? phr_("set_sound") : conf_grp == 2 ? phr_("set_control") : conf_grp == 3 ? phr_("set_system") : phr_("소리/패드/배속 설정"));
    Settings & s = eff();
    rbox(c, 16, 24, 288, 296, WH, phr_("동작 환경 설정"));
    const std::vector<int> ri = rowids(S_CONF);
    const bool own = scope == 1 && cur >= 0 && cur < MAXG; const bool ind = own && ovm[cur] != 0u;
    static const unsigned rb[15] = { 0u, 1u << 7, 1u << 8, 0u, (1u << 9) | (31u << 17), 1u << 10, 1u << 11, 1u << 14, 1u << 12, 1u << 13, 1u << 15, 0u, 0u, 0u, 0u };
    std::vector<std::string> n;
    { const std::string nm[15] = { phr_("적용 범위"), phr_("BGM 음량"), phr_("효과음 음량"), KH_SNDLABEL, phr_("조작 설정"), phr_("배속 배율"), phr_("배속 방식"), phr_("배속 중 소리"), phr_("부팅 시퀀스"), phr_("배속 표시"), phr_("오버레이 버튼 표시"), phr_("한글 글꼴"), phr_("일본어 글꼴"), phr_("언어 / Language / 言語"), phr_("indiv_reset") };
      for (int id : ri) n.push_back(id == 15 ? std::string("FPS / Speed") : ((own && (ovm[cur] & rb[id])) ? std::string("● ") + nm[id] : nm[id])); }
    const int rs = (sel >= 0 && sel < (int)ri.size()) ? ri[sel] : 0;
    items(c, 40, 44, 256, n, sel, 19);
    rbox(c, 320, 24, 304, 296, WH, phr_("인포메이션"));
    auto row = [&](int i) { return rs == i; };
    auto ry = [&](int id) { int k = -1000; for (size_t i = 0; i < ri.size(); i++) if (ri[i] == id) k = (int)i; return 44 + 19 * k; };
    const char * sc[] = { phr_("전역"), phr_("scope_this") }; choices(c, 344, ry(0), sc, 2, scope, WH, row(0));
    char b[8];
    snprintf(b, sizeof b, "%d", s.bgm); barv(c, 340, ry(1), 232, s.bgm / 8.0, "OFF", phr_("최대"), WH, row(1)); text(c, 590, ry(1), b, WH, K);
    snprintf(b, sizeof b, "%d", s.se); barv(c, 340, ry(2), 232, s.se / 8.0, "OFF", phr_("최대"), WH, row(2)); text(c, 590, ry(2), b, WH, K);
    if (scope == 0) {
#if defined(__ANDROID__)
    { const char * sb[] = { "512", "1024", "2048", "4096" }; choices(c, 344, ry(3), sb, 4, g_sndbuf, WH, row(3)); }
#else
    text(c, 344, ry(3), phr_("기본 장치"), row(3) ? K : WH, row(3) ? WH : K);
#endif
    }
    text(c, 344, ry(4), s.pad == 1 ? phr_("조작 설정 (서양식)") : phr_("조작 설정 (일본식)"), row(4) ? K : WH, row(4) ? WH : K);   /* t48e (#646): 고정 글자 → 조작 설정 화면 입구 */
    const char * bm[] = { "x2", "x3", "x4", "x6", "x8" }; choices(c, 344, ry(5), bm, 5, s.bmult, WH, row(5));
    const char * bk[] = { phr_("홀드"), phr_("토글"), phr_("둘 다") }; choices(c, 344, ry(6), bk, 3, s.bmode, WH, row(6));
    const char * bs[] = { phr_("끔"), phr_("빠르게"), phr_("음높이 유지") }; choices(c, 344, ry(7), bs, 3, s.bsound, WH, row(7));
    const char * bt[] = { phr_("켬"), phr_("끔") }; choices(c, 344, ry(8), bt, 2, s.boot, WH, row(8));
    { const char * bo[] = { phr_("끔"), phr_("켬") }; choices(c, 344, ry(9), bo, 2, s.bosd, WH, row(9)); }
    { const char * ob[] = { phr_("끔"), phr_("켬") }; choices(c, 344, ry(10), ob, 2, s.ovbtn, WH, row(10)); }   /* t52f (#132) */
    if (scope == 0) { const char * fp[] = { "OFF", "ON" }; choices(c, 344, ry(15), fp, 2, g.fpsd ? 1 : 0, WH, row(15)); }   /* #639 */
    if (scope == 0) {
    text(c, 344, ry(11), fit(font_kr.empty() ? std::string(phr_("기본 (kyuhachi_font)")) : font_kr, 250), row(11) ? K : WH, row(11) ? WH : K);
    text(c, 344, ry(12), fit(font_jp.empty() ? std::string(phr_("기본 (jp_default)")) : font_jp, 250), row(12) ? K : WH, row(12) ? WH : K);
    { const char * lg[] = { "한국어", "English", "にほんご" }; choices(c, 344, ry(13), lg, 3, lang, WH, row(13)); }   /* t52g (#133): 언어 */
    }
#if defined(__ANDROID__)
    static const char * dd[] = { "desc_scope", "desc_bgm", "desc_se", "desc_sndbuf", "desc_pad", "desc_bmult", "desc_bmode", "desc_bsound", "desc_boot", "desc_bosd", "desc_ovbtn", "desc_fontkr", "desc_fontjp", "desc_lang", "desc_indiv_reset" };
#else
    static const char * dd[] = { "desc_scope", "desc_bgm", "desc_se", "desc_snddev", "desc_pad", "desc_bmult", "desc_bmode", "desc_bsound", "desc_boot", "desc_bosd", "desc_ovbtn", "desc_fontkr", "desc_fontjp", "desc_lang", "desc_indiv_reset" };
#endif
    std::string dt = dkey(phr_(rs == 15 ? "desc_fps" : dd[rs]));
    if (ind) dt = std::string(phr_("indiv_has")) + "  ·  " + dt;
    desc_mq(c, 328, 44, dt, phr_("↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정"));
    if (scope == 0 && (rs == 11 || rs == 12)) { const std::string & fl = font_now(); if (!fl.empty()) text(c, 344, 288, fit(fl, 270), CY, K); }   /* build 5b-3: font now applied to the current work */
    const bool ibk = own && (ovm[cur] & indiv_bit(S_CONF, rs));
    helpline(c, ibk ? phr_("indiv_help") : phr_("↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정"));
}
/* build 5b-3: "now applied: <font> (source)" for the current work. priority: launch method (font=) > work language default (font_kind) > system. */
inline const std::string & Ui::font_now() {
    if (fnow_cur == cur && fnow_kr == font_kr && fnow_jp == font_jp) return fnow_line;
    fnow_cur = cur; fnow_kr = font_kr; fnow_jp = font_jp; fnow_line.clear();
    if (cur < 0 || cur >= (int)games.size() || !env.values) return fnow_line;
    std::vector<ValRow> vr; if (!env.values(cur, vr)) return fnow_line;
    std::string fv, kd;
    for (auto & r : vr) { if (r.val.empty() || r.val[0] == '(') continue; if (r.key == "font") fv = r.val; else if (r.key == "font_kind") kd = r.val; }
    std::string nm, src;
    if (!fv.empty()) { nm = fv; src = phr_("fsrc_ent"); }
    else if (kd == "jp") { nm = font_jp.empty() ? std::string("jp_default") : font_jp; src = phr_("fsrc_lang"); }
    else if (kd == "kr") { nm = font_kr.empty() ? std::string("kyuhachi_font") : font_kr; src = phr_("fsrc_lang"); }
    else { nm = font_kr.empty() ? std::string("kyuhachi_font") : font_kr; src = phr_("fsrc_sys"); }
    fnow_line = std::string(phr_("font_now")) + " " + nm + " (" + src + ")";
    return fnow_line;
}
