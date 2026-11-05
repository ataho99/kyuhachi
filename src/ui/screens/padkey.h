#pragma once
/* screens/padkey.h -- S8b: S_PADKEY screen. Code moved unchanged from kh_ui.h / screen_table.h (members of struct Ui, defined out of line). */
inline void Ui::d_padkey(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 288, 296, WH, (phr_("키 고르기: ") + pad_row_name(pk_row)).c_str());
    std::vector<std::string> r; std::vector<uint32_t> col; std::string curv = pad_val(pk_row);
    for (auto & o : pk_opts) { std::string lab = o.empty() ? std::string((scope == 1 && pk_row < PAD_N) ? phr_("전역 따름 (작품 값 지움)") : phr_("기본값")) : pad_show(o); r.push_back(std::string(o == curv && !o.empty() ? "● " : "  ") + fit(lab, 220)); col.push_back(0); }
    rowlist(c, 40, 48, 256, 9, r, sel, 24, &col);
    rbox(c, 320, 24, 304, 296, WH, phr_("인포메이션"));
    text(c, 340, 44, fit(pad_row_name(pk_row) + " → " + (sel < (int)pk_opts.size() ? (pk_opts[sel].empty() ? std::string(phr_("기본")) : pad_show(pk_opts[sel])) : std::string("-")), 270), YL, K);
    text(c, 340, 74, fit((scope == 1 && pk_row < PAD_N) ? phr_("저장: 이 작품 (user.ini [pad])") : phr_("저장: 전역 (settings.ini)"), 270), CY, K);
    text(c, 340, 104, phr_("배정 없음 = 이 버튼은"), WH, K);
    text(c, 340, 124, phr_("아무 것도 보내지 않음"), WH, K);
    text(c, 340, 154, phr_("마우스 왼/오른 = 게임 마우스 버튼"), WH, K);
    desc(c, 324, 44, { phr_("STOP COPY GRPH XFER NFER HELP 등 PC-98 키를 고를 수 있습니다.") });
    helpline(c, phr_("↑↓:선택, ○:정함, ×:취소"));
}
inline void Ui::k_padkey(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do {
            mv(sel, (int)pk_opts.size());
            if (k == UK_OK && sel < (int)pk_opts.size()) {
                const std::string o = pk_opts[sel]; std::string key = pad_key(pk_row);
                if (scope == 1 && pk_row < PAD_N) { if (env.pad_set && cur >= 0 && cur < (int)games.size()) env.pad_set(cur, key, o); }
                else { if (o.empty()) gpad.erase(key); else gpad[key] = o; }
                dirty = true; if (pk_row >= PAD_N) { scr = S_EXT; sel = pad_sel_keep; } else { scr = S_PAD; sel = pad_sel_keep; } a.t = A_APPLY;
            } else if (k == UK_CANCEL) { if (pk_row >= PAD_N) { scr = S_EXT; sel = pad_sel_keep; } else { scr = S_PAD; sel = pad_sel_keep; } }
            break;
    } while (0);
}
