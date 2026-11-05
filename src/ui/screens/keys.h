#pragma once
/* screens/keys.h -- S8b: S_KEYS screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_keys(Canvas & c) {
    c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, phr_("PC-98 키 보내기"));
    std::vector<std::string> a = { "F10", "1", "2", "3", "4", "5", "6", "7", "KANA" }; items(c, 160, 48, 330, a, sel, 28);
    c.rect(0, 376, 640, 24, K); helpline(c, phr_("↑↓:선택, ○:보내기, ×:닫기"));
}
inline void Ui::k_keys(UKey k, Act & a) {
    do {
        if (k == UK_OK) { a.t = A_SENDKEY; a.arg = sel; } else if (k == UK_CANCEL) go(S_GKB, 0);
        break;
    } while (0);
}
