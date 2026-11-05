#pragma once
/* screens/help.h -- S8b: S_HELP screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
/* 5b-4 (#479): help body symbols to Retroid labels (position + name), same rule as the pad screen */
static inline std::string help_btn(std::string s) {
#if defined(__ANDROID__)
    int m = g_lblcfg ? g_lblcfg : (gensei_padkind_v == 1 ? 2 : 1);
    if (m == 1) {
        s = help_rep(s, "\xE2\x97\x8B", "\xE2\x86\x92(A)");
        s = help_rep(s, "\xC3\x97", "\xE2\x86\x93(B)");
        s = help_rep(s, "\xE2\x96\xA1", "\xE2\x86\x90(Y)");
        s = help_rep(s, "\xE2\x96\xB3", "\xE2\x86\x91(X)");
        return s;
    }
#endif
    return kh_lbl(s);
}
inline void Ui::d_help(Canvas & c) {   /* t52i (B2·B3): 조작 안내 = 한 장 표. 배속은 「입력 → 동작」 표 */
    titlebar(c, phr_("조작 안내"));
    rbox(c, 16, 24, 608, 344, WH, phr_("조작 안내"));
    static const char * L[] = {
        phr_("패드    ○:결정(스페이스)  ×:취소(Esc)  □:빨리 넘김(CAPS)"),
        phr_("        △:메뉴(F12)  START:Enter  십자키/왼스틱:방향키"),
        phr_("배속    입력 → 동작"),
        phr_("        누르는 동안   R2 / F11"),
        phr_("        켜고 끄기     L3 / ScrollLock / Alt+F11"),
        phr_("        빨리 넘김     □ / CAPS  (글 넘기기)"),
        phr_("화면    Alt+Enter:전체화면  Alt+1/2/3:비율  Alt+S:셰이더"),
        phr_("        Alt+위/아래:곡면 세기  Alt+[ ]:창 배율"),
        phr_("저장    메뉴의 저장/불러오기, Alt+F5 / Alt+F9 는 슬롯 1"),
        phr_("키보드  그 밖의 키는 PC-98 키로 그대로 전달"),
        phr_("        F10 / 숫자 1-7 / KANA 는 메뉴의 「PC-98 키 보내기」로만 보냄") };
    for (int i = 0; i < 11; i++) text(c, 40, 46 + i * 28, help_btn(L[i]), L[i][0] != ' ' ? CY : WH, K);
    helpline(c, phr_("○ 또는 ×:돌아가기"));
}
inline void Ui::k_help(UKey k, Act & a) {
    do {
        if (k == UK_OK || k == UK_CANCEL) go(S_EXTRA, extra_row(3));
        break;
    } while (0);
}
