#pragma once
/* screens/sort.h -- S8a: S_SORT screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_sort(Canvas & c) {
    d_pick(c); int bx, by; confirm_box(c, 360, 262, phr_("정렬"), bx, by);
    std::vector<std::string> a; for (int i = 0; i < SM_N; i++) a.push_back(std::string(i == sortm ? "● " : "  ") + sort_name(i));
    a.push_back(std::string(phr_("즐겨찾기 먼저: ")) + (sortfav ? phr_("켬") : phr_("끔"))); a.push_back(phr_("사용자 지정 순서 바꾸기"));
    std::vector<bool> dm(a.size(), false); dm[8] = sortm != SM_CUSTOM; items(c, bx + 30, by + 22, 310, a, sel, 24, &dm);
    helpline(c, phr_("↑↓:선택, ○:결정, ×:닫기"));
}
inline void Ui::k_sort(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do {
            mv(sel, 9);
            if (k == UK_OK) {
                if (sel < SM_N) { int keep = cur; sortm = sel; rebuild_vis(keep); dirty = true; scr = S_PICK; }
                else if (sel == 7) { int keep = cur; sortfav = !sortfav; rebuild_vis(keep); dirty = true; }
                else if (sel == 8 && sortm == SM_CUSTOM) { renumber_order(); reorder = true; scr = S_PICK; }
            } else if (k == UK_CANCEL) scr = S_PICK;
            break;
    } while (0);
}
