#pragma once
/* screens/add.h -- S8a: S_ADD screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_add(Canvas & c) {
    titlebar(c, phr_("작품 추가"));
    rbox(c, 16, 24, 288, 224, WH, phr_("작품 추가"));
    bool hasgf = !gamefolder.empty();
    std::vector<std::string> n = { phr_("이미지 파일 고르기"), phr_("폴더 고르기"), phr_("게임 폴더 지정"), phr_("게임 폴더 다시 검색"), phr_("게임 폴더 해제"), phr_("방금 추가한 작품 언어 지정") };
    std::vector<bool> dm = { false, false, false, !hasgf, !hasgf, add_new.empty() };
    items(c, 40, 48, 256, n, sel, 26, &dm);
    rbox(c, 320, 24, 304, 224, WH, phr_("인포메이션"));
    const int gfx = std::max(80, tw(phr_("게임 폴더")) + 12);   /* t672 (#656): 라벨이 길면 값 칸을 오른쪽으로 */
    text(c, 344, 44, phr_("게임 폴더"), CY, K);
    text(c, 344 + gfx, 44, hasgf ? fit(gamefolder, 304 - 24 - gfx - 16) : std::string(phr_("(없음)")), hasgf ? WH : GY, K);
    int yy = 72; if (add_log.empty()) text(c, 344, yy, phr_("아직 추가한 작품이 없습니다."), GY, K);
    for (size_t i = 0; i < add_log.size() && i < 8; i++) { text(c, 344, yy, fit(add_log[i], 304 - 48), i == 0 ? YL : WH, K); yy += 20; }
    static const char * dd[] = { phr_("고른 이미지 파일들을 한 작품으로 등록합니다 (여러 장 가능)."), phr_("폴더 안의 이미지를 찾아 등록합니다. 하위 폴더는 작품마다 따로."), phr_("이 폴더를 켤 때마다 훑어 새 작품을 자동으로 등록합니다."), phr_("게임 폴더를 지금 다시 훑습니다."), phr_("게임 폴더 지정을 풀고 자동 등록을 멈춥니다 (등록된 작품은 남음)."), phr_("방금 추가한 작품마다 한글판 / 원판 일본어 / 자동을 고릅니다.") };
    desc(c, 264, 64, { dd[sel < 6 ? sel : 0], phr_("형식은 확장자가 아니라 파일 머리로 판정합니다.") });
    helpline(c, phr_("↑↓:선택, ○:결정, ×:돌아가기"));
}
inline void Ui::k_add(UKey k, Act & a) {
    do {
            if (k == UK_OK) {
                if (sel == 5) { if (!add_new.empty()) open_lang(add_new, add_new_why, S_ADD); break; }   /* t44a (#494) */
                if (sel >= 3 && gamefolder.empty()) break;
                a.t = A_ADD; a.arg = sel;
            } else if (k == UK_CANCEL) { go(S_PICK, 0); pick = (int)vis.size(); }
            break;
    } while (0);
}
