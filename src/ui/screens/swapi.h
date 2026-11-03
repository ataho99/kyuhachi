#pragma once
/* screens/swapi.h -- S8a: S_SWAPI screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_swapi(Canvas & c) {
    const UiGame * gm = game(); c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, std::string(swapdrv ? "B" : "A").append(phr_(" 드라이브에 넣을 디스크")).c_str());
    if (!gm) return;
    const int VIS = 9; int n = (int)gm->fds.size(); int top = swapsel - 4; if (top > n - VIS) top = n - VIS; if (top < 0) top = 0;
    std::vector<std::string> w; for (int i = top; i < n && i < top + VIS; i++) w.push_back((gm->fds[i] == gm->cur_fd[swapdrv] ? "● " : "  ") + fit(gm->fds[i], 300));
    items(c, 150, 44, 350, w, swapsel - top, 30);
    helpline(c, phr_("↑↓:선택, ○:넣기, ×:돌아가기"));
}
inline void Ui::k_swapi(UKey k, Act & a) {
    do {
            if (k == UK_OK) { if (game() && swapsel < (int)game()->fds.size()) { a.t = A_DISKSWAP; a.game = cur; a.slot = swapdrv; a.arg = swapsel; } }
            else if (k == UK_CANCEL) go(S_SWAP, 0);
            break;
    } while (0);
}
