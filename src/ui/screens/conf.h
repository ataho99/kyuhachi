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
    static const char * dd[] = { phr_("적용 범위: 전역은 모든 작품, 이 작품은 고른 작품만."), phr_("BGM 음량입니다. 효과음과 따로 조절합니다."), phr_("효과음 음량입니다. 시작 소리도 이 값을 따릅니다."), phr_("소리가 나오는 장치입니다. 지금은 기본 장치만 씁니다."), phr_("패드 배치입니다. ○가 결정, ×가 취소입니다."), phr_("배속 배율입니다."), phr_("홀드는 누르는 동안, 토글은 눌러 켜고 끄기, 둘 다는 둘 다."), phr_("끔=무음 · 빠르게=음높이 상승 · 음높이 유지=높이 그대로"), phr_("시작할 때 나오는 부팅 화면을 켜고 끕니다."), phr_("배속 중에 오른쪽 위에 ▶▶ 배율을 계속 표시합니다. 캡처에는 찍히지 않습니다."), phr_("게임 중 화면 아래의 오버레이 버튼(메뉴·저장·불러오기·배속·키보드)을 보입니다. 끄면 전부 숨깁니다."), phr_("한글판 기본 글꼴입니다. {○} 로 고릅니다. 같은 배치의 글꼴만 나옵니다."), phr_("원판(일본어) 기본 글꼴입니다. {○} 로 고릅니다."), phr_("화면 문구 언어입니다. 바꾸면 바로 적용되고 저장됩니다.") };
    desc(c, 328, 44, { fit(dkey(dd[sel]), 560) }, -1, phr_("↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정"));
    helpline(c, phr_("↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정"));
}
