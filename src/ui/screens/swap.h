#pragma once
/* screens/swap.h -- S8a: S_SWAP screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_swap(Canvas & c) {
    const UiGame * gm = game(); c.rect(120, 16, 400, 336, K); rbox(c, 128, 24, 384, 320, WH, phr_("디스크 교체"));
    if (!gm || gm->fds.empty()) { text(c, 160, 60, phr_("교체할 디스크가 없습니다."), GY, K); helpline(c, phr_("×:돌아가기")); return; }
    std::vector<std::string> a = { phr_("A 드라이브  ") + (gm->cur_fd[0].empty() ? std::string(phr_("(비어 있음)")) : fit(gm->cur_fd[0], 190)), phr_("B 드라이브  ") + (gm->cur_fd[1].empty() ? std::string(phr_("(비어 있음)")) : fit(gm->cur_fd[1], 190)) };
    items(c, 160, 48, 330, a, swapdrv, 30);
    text(c, 160, 130, phr_("바꿀 드라이브를 고릅니다."), WH, K);
    helpline(c, phr_("↑↓:선택, ○:디스크 고르기, ×:돌아가기"));
}
inline void Ui::k_swap(UKey k, Act & a) {
    do {
            if (k == UK_OK) { if (game() && !game()->fds.empty()) { swapsel = 0; const UiGame * g = game(); for (size_t i = 0; i < g->fds.size(); i++) if (g->fds[i] == g->cur_fd[swapdrv]) swapsel = (int)i; go(S_SWAPI, 0); } }
            else if (k == UK_CANCEL) go(S_GAME, 3);
            break;
    } while (0);
}
