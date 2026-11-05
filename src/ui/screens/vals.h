#pragma once
/* screens/vals.h -- S8b: S_VALS screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_vals(Canvas & c) {   /* t48a (#614 ③): 키·최종값·출처·덮인 값 */
    titlebar(c, ""); const UiGame * gm = game();
    rbox(c, 8, 20, 624, 356, WH, gm ? ttl_suf(*gm, phr_("적용값"), 560).c_str() : phr_("적용값"));
    text(c, 20, 40, phr_("키"), CY, K); text(c, 150, 40, phr_("최종값"), CY, K); text(c, 350, 40, phr_("출처"), CY, K); text(c, 470, 40, phr_("덮인 값"), CY, K);
    int n = (int)vals.size(); const int VN = 13;
    if (sel >= n) sel = n > 0 ? n - 1 : 0; if (sel < 0) sel = 0;
    if (sel < vals_top) vals_top = sel; if (sel >= vals_top + VN) vals_top = sel - VN + 1;
    for (int i = 0; i < VN && vals_top + i < n; i++) {
        const ValRow & r = vals[vals_top + i]; int y = 60 + i * 21; bool s = vals_top + i == sel;
        if (s) c.rect(14, y - 2, 612, 21, WH);
        text(c, 20, y, fit(r.key, 126), s ? K : CY, s ? WH : K); text(c, 150, y, fit(r.val, 194), s ? K : WH, s ? WH : K);
        text(c, 350, y, fit(r.src, 114), s ? K : YL, s ? WH : K); text(c, 470, y, fit(r.cov, 150), s ? K : GY, s ? WH : K);
    }
    if (n == 0) text(c, 20, 60, phr_("표시할 값이 없습니다."), GY, K);
    else { const ValRow & r = vals[sel]; text(c, 20, 340, fit(phr_("전체: ") + r.val, 596), YL, K); if (!r.cov.empty()) text(c, 20, 357, fit(phr_("덮인 값: ") + r.cov, 596), GY, K); }
    helpline(c, phr_("↑↓:이동, L/R:한 쪽, ○ 또는 ×:돌아가기"));
}
inline void Ui::k_vals(UKey k, Act & a) {
    do { {
            int n = (int)vals.size();
            if (k == UK_UP && sel > 0) sel--; else if (k == UK_DOWN && sel + 1 < n) sel++;
            else if (k == UK_L) sel = sel > 13 ? sel - 13 : 0; else if (k == UK_R) sel = sel + 13 < n ? sel + 13 : (n > 0 ? n - 1 : 0);
            else if (k == UK_OK || k == UK_CANCEL) go(S_EXTRA, extra_row(7));
            break; }
    } while (0);
}
