#pragma once
/* screens/osk.h -- S8b: S_OSK screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_hosk(Canvas & c) {
    titlebar(c, ""); rbox(c, 16, 24, 608, 236, WH, osk_target == 2 ? phr_("표시 제목 입력 (한글 키보드)") : osk_target == 3 ? phr_("메뉴 이름 입력 (한글 키보드)") : phr_("이름 입력 (한글 키보드)"));
    { std::string sb = fit(osk_buf, 430); int cx = 40 + tw(sb); text(c, 40, 48, sb, YL, K);
      std::string cu = hg::cur_text(hc.cur); if (!cu.empty()) { text(c, cx, 48, cu, CY, K); int cw = tw(cu); c.rect(cx, 66, cw, 2, CY); cx += cw; }
      if (!ime_comp.empty()) { text(c, cx, 48, ime_comp, CY, K); int cw = tw(ime_comp); c.rect(cx, 66, cw, 2, CY); cx += cw; }
      text(c, cx, 48, "_", YL, K); }
    text(c, 480, 48, hc.han ? phr_("[한]") : phr_("[영]"), CY, K); if (osk_shift) text(c, 540, 48, "SHIFT", OR, K);
    for (int r = 0; r < 5; r++) for (int q = 0; q < hk_len(r); q++) {
        int x, y, w, h; hk_rect(r, q, x, y, w, h); bool sl = r == osk_r && q == osk_c; std::string lb = hk_label(r, q);
        c.rect(x, y, w, h, sl ? WH : GY); c.rect(x + 1, y + 1, w - 2, h - 2, sl ? WH : K);
        uint32_t fg = (r == 4 && q == 1 && osk_shift) ? OR : ((r == 4 && q == 0) ? CY : WH);
        text(c, x + (w - tw(lb)) / 2, y + 4, lb, sl ? K : fg, sl ? WH : K);
    }
    desc(c, 270, 58, { phr_("SHIFT: 다음 한 자모만 쌍자음·ㅒㅖ"), phr_("패드: L=SHIFT △=한/영 □=지움 R=띄우기") });
    g_help_osk = true; helpline(c, phr_("↑↓←→:이동, ○:입력, SEL:완료, ×:취소"));
}
inline void Ui::d_osk(Canvas & c) {
    if (osk_hg) { d_hosk(c); return; }   /* t48b */
    titlebar(c, ""); rbox(c, 16, 24, 608, 296, WH, osk_target == 0 ? phr_("이름 입력") : phr_("글자 입력"));
    { std::string sb = fit(osk_buf, 500); int cx = 40 + tw(sb); text(c, 40, 48, sb, YL, K); if (!ime_comp.empty()) { text(c, cx, 48, ime_comp, CY, K); int cw = tw(ime_comp); c.rect(cx, 66, cw, 2, CY); cx += cw; } text(c, cx, 48, "_", YL, K); }   /* t48a: 조합 중 글자는 밑줄 */
    static const char * rowsl[5] = { "abcdefghij", "klmnopqrst", "uvwxyz0123", "456789.-/\\", ":|~_'() " };
    for (int r = 0; r < 5; r++) for (int q = 0; q < 10 && q < (int)strlen(rowsl[r]); q++) {
        char ch = rowsl[r][q]; if (!ch) continue; if (osk_upper && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32); int x = 120 + q * 40, y = 92 + r * 36; bool sl = r == osk_r && q == osk_c;
        c.rect(x - 4, y - 4, 32, 28, sl ? WH : GY); c.rect(x - 3, y - 3, 30, 26, sl ? WH : K);
        char s[4] = { ch == ' ' ? '_' : ch, 0, 0, 0 }; text(c, x + 4, y, s, sl ? K : WH, sl ? WH : K);
    }
    desc(c, 324, 58, { osk_target == 0 ? phr_("키보드가 있으면 한/영 키로 한글도 그대로 칠 수 있습니다.") : phr_("DOS 명령용입니다. 대문자는 소문자로 저장되고 ~ 는 Enter 입니다."), phr_("키보드가 있으면 그대로 쳐도 됩니다.") }, -1, phr_("↑↓←→:이동, ○:입력, □:지움, △:대소문자, SEL:완료, ×:취소"));
    helpline(c, phr_("↑↓←→:이동, ○:입력, □:지움, △:대소문자, SEL:완료, ×:취소"));
}
inline void Ui::k_osk(UKey k, Act & a) {
    do { {
            if (osk_hg) { hk_key(k); break; }   /* t48b */
            static const char * rowsl[5] = { "abcdefghij", "klmnopqrst", "uvwxyz0123", "456789.-/\\", ":|~_'() " };
            auto cellok = [&](int r, int q) { return q < (int)strlen(rowsl[r]); };
            if (k == UK_UP) osk_r = (osk_r + 4) % 5; else if (k == UK_DOWN) osk_r = (osk_r + 1) % 5; else if (k == UK_LEFT) osk_c = (osk_c + 9) % 10; else if (k == UK_RIGHT) osk_c = (osk_c + 1) % 10;
            else if (k == UK_OK && cellok(osk_r, osk_c)) { char ch = rowsl[osk_r][osk_c]; if (osk_target == 1 || !osk_upper) { if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32); } else if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32); if (osk_buf.size() < 48) osk_buf += ch; }
            else if (k == UK_SQ) osk_back(); else if (k == UK_TRI && osk_target == 0) osk_upper = !osk_upper;
            else if (k == UK_SELECT) osk_done(); else if (k == UK_CANCEL) { scr = osk_ret; if (osk_ret == S_EDIT) sel = 0; }
            break; }
    } while (0);
}
