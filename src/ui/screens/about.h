#pragma once
/* screens/about.h -- S8a: S_ABOUT screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_about(Canvas & c) {
    titlebar(c, ""); rbox(c, 16, 24, 608, 344, WH, phr_("정보·라이선스"));
    const char * L[] = { KH_FULLNAME " (큐하치)", phr_("이 프로그램의 PC-98 에뮬레이션은 DOSBox-X 의 구성 요소를 사용합니다."), phr_("DOSBox-X : GNU General Public License v2 이상 (DOSBox-X 팀과 기여자들)"), "           소스 코드 : https://github.com/joncampbell123/dosbox-x", phr_("SDL2 : zlib 라이선스     libpng · zlib : 각 라이선스"), phr_("UI 글꼴 : Neo둥근모 (SIL Open Font License 1.1)"), "", phr_("GPL 조건에 따라 이 프로그램은 보증 없이 제공됩니다."), phr_("게임 디스크 이미지는 이 프로그램에 포함되지 않으며, 각 저작권자의 것입니다.") };
    { int ex = 0; for (int i = 0; i < 9; i++) if (L[i][0]) {   /* t674 (#658): 상자(576 px)를 넘는 줄은 단어 단위로 접는다 */
        if (tw(L[i]) > 576) { std::vector<std::string> ls = wrap_words(L[i], 576); for (size_t j = 0; j < ls.size(); j++) text(c, 40, 48 + i * 28 + ex + (int)j * 20, ls[j], WH, K); ex += ((int)ls.size() - 1) * 20; }
        else text(c, 40, 48 + i * 28 + ex, L[i], i == 0 ? CY : WH, K); } }
    helpline(c, phr_("○ 또는 ×:돌아가기"));
}
inline void Ui::k_about(UKey k, Act & a) {
    do {
            if (k == UK_OK || k == UK_CANCEL) go(S_EXTRA, extra_row(4));
            break;
    } while (0);
}
