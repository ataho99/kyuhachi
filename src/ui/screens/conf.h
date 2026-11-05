#pragma once
/* screens/conf.h -- S8b: S_CONF screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_conf(Canvas & c) {
    titlebar(c, phr_("소리/패드/배속 설정"));
    Settings & s = eff();
    rbox(c, 16, 24, 288, 296, WH, phr_("동작 환경 설정"));
    std::vector<std::string> n = { phr_("적용 범위"), phr_("BGM 음량"), phr_("효과음 음량"), KH_SNDLABEL, phr_("조작 설정"), phr_("배속 배율"), phr_("배속 방식"), phr_("배속 중 소리"), phr_("부팅 시퀀스"), phr_("배속 표시"), phr_("오버레이 버튼 표시"), phr_("한글 글꼴"), phr_("일본어 글꼴"), phr_("언어 / Language / 言語") };
    items(c, 40, 44, 256, n, sel, 19);
    rbox(c, 320, 24, 304, 296, WH, phr_("인포메이션"));
    auto row = [&](int i) { return sel == i; };
    auto ry = [](int i) { return 44 + 19 * i; };
    const char * sc[] = { phr_("전역"), phr_("작품·따름"), phr_("작품·전용") }; choices(c, 344, ry(0), sc, 3, scope3(), WH, row(0));
    char b[8];
    snprintf(b, sizeof b, "%d", s.bgm); barv(c, 340, ry(1), 232, s.bgm / 8.0, "OFF", phr_("최대"), WH, row(1)); text(c, 590, ry(1), b, WH, K);
    snprintf(b, sizeof b, "%d", s.se); barv(c, 340, ry(2), 232, s.se / 8.0, "OFF", phr_("최대"), WH, row(2)); text(c, 590, ry(2), b, WH, K);
#if defined(__ANDROID__)
    { const char * sb[] = { "512", "1024", "2048", "4096" }; choices(c, 344, ry(3), sb, 4, g_sndbuf, WH, row(3)); }
#else
    text(c, 344, ry(3), phr_("기본 장치"), row(3) ? K : WH, row(3) ? WH : K);
#endif
    text(c, 344, ry(4), s.pad == 1 ? phr_("조작 설정 (서양식)") : phr_("조작 설정 (일본식)"), row(4) ? K : WH, row(4) ? WH : K);   /* t48e (#646): 고정 글자 → 조작 설정 화면 입구 */
    const char * bm[] = { "x2", "x3", "x4", "x6", "x8" }; choices(c, 344, ry(5), bm, 5, s.bmult, WH, row(5));
    const char * bk[] = { phr_("홀드"), phr_("토글"), phr_("둘 다") }; choices(c, 344, ry(6), bk, 3, s.bmode, WH, row(6));
    const char * bs[] = { phr_("끔"), phr_("빠르게"), phr_("음높이 유지") }; choices(c, 344, ry(7), bs, 3, s.bsound, WH, row(7));
    const char * bt[] = { phr_("켬"), phr_("끔") }; choices(c, 344, ry(8), bt, 2, s.boot, WH, row(8));
    { const char * bo[] = { phr_("끔"), phr_("켬") }; choices(c, 344, ry(9), bo, 2, s.bosd, WH, row(9)); }
    { const char * ob[] = { phr_("끔"), phr_("켬") }; choices(c, 344, ry(10), ob, 2, s.ovbtn, WH, row(10)); }   /* t52f (#132) */
    text(c, 344, ry(11), fit(font_kr.empty() ? std::string(phr_("기본 (kyuhachi_font)")) : font_kr, 250), row(11) ? K : WH, row(11) ? WH : K);
    text(c, 344, ry(12), fit(font_jp.empty() ? std::string(phr_("기본 (jp_default)")) : font_jp, 250), row(12) ? K : WH, row(12) ? WH : K);
    { const char * lg[] = { "한국어", "English", "にほんご" }; choices(c, 344, ry(13), lg, 3, lang, WH, row(13)); }   /* t52g (#133): 언어 */
#if defined(__ANDROID__)
    static const char * dd[] = { "desc_scope", "desc_bgm", "desc_se", "desc_sndbuf", "desc_pad", "desc_bmult", "desc_bmode", "desc_bsound", "desc_boot", "desc_bosd", "desc_ovbtn", "desc_fontkr", "desc_fontjp", "desc_lang" };
#else
    static const char * dd[] = { "desc_scope", "desc_bgm", "desc_se", "desc_snddev", "desc_pad", "desc_bmult", "desc_bmode", "desc_bsound", "desc_boot", "desc_bosd", "desc_ovbtn", "desc_fontkr", "desc_fontjp", "desc_lang" };
#endif
    desc_mq(c, 328, 44, dkey(phr_(dd[sel])), phr_("↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정"));
    helpline(c, phr_("↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정"));
}
