#pragma once
/* screens/ent.h -- S8c: S_ENT screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_ent(Canvas & c) {
    titlebar(c, ""); const UiGame & g = games[entg];
    rbox(c, 16, 24, 288, 224, WH, ttl_suf(g, phr_("실행 방법"), 256).c_str());
    std::vector<int> v = vents(entg); std::vector<std::string> r; std::vector<uint32_t> col; int di = def_ent(entg);
    if (v.empty()) { r.push_back(phr_("기본 (자동 판정)")); col.push_back(0); }
    for (int i : v) { const UiEnt & e = g.ents[i]; std::string mk = (i == di) ? "★ " : "  "; r.push_back(mk + fit(std::string(phr_(e.label.c_str())) + (e.tag.empty() ? std::string() : "  [" + tag_disp(e.tag) + "]"), 200) + (ent_warn(entg, i) ? " ※" : "")); col.push_back(ent_warn(entg, i) ? OR : 0); }
    r.push_back(phr_("+ 새 실행 방법")); col.push_back(YL);
    r.push_back(phr_("언어: ") + lang_label(g)); col.push_back(CY);   /* t44a (#494) */
    r.push_back(phr_("표시 제목: ") + fit(g.title, 150)); col.push_back(CY);   /* t49a (#809) */
    rowlist(c, 40, 48, 256, 7, r, sel, 24, &col);
    rbox(c, 320, 24, 304, 224, WH, phr_("상세"));
    int ni = (int)v.size(); const UiEnt * e = (sel < ni) ? &g.ents[v[sel]] : NULL;
    bool onlang = sel == (int)std::max<size_t>(v.size(), 1) + 1;
    bool ontitle = sel == (int)std::max<size_t>(v.size(), 1) + 2;   /* t49a (#809) */
    if (ontitle) {   /* t49a (#809): 현재 표시 제목 전체(줄임 없이 줄 나눔) · DB 제목 */
        text(c, 340, 44, phr_("표시 제목"), CY, K); { std::string rest = g.title; int yy = 68; for (int ln = 0; ln < 3 && !rest.empty(); ln++) { size_t n = 0, last = 0; while (n < rest.size()) { size_t m = n + 1; while (m < rest.size() && ((unsigned char)rest[m] & 0xC0) == 0x80) m++; if (tw(rest.substr(0, m)) > 270) break; n = m; } if (n == 0) n = rest.size(); (void)last; text(c, 340, yy, rest.substr(0, n), YL, K); rest = rest.substr(n); yy += 20; } }
        text(c, 340, 132, phr_("DB 제목"), CY, K); text(c, 340 + 76, 132, fit(g.title_db.empty() ? g.title : g.title_db, 190), WH, K);
        text(c, 340, 168, dkey(phr_("{○}:고치기")), CY, K); text(c, 340, 188, dkey(phr_("{□}:DB 제목으로 되돌림")), CY, K);
    } else if (onlang) {
        text(c, 340, 44, phr_("이 작품의 언어"), CY, K); text(c, 340, 68, fit(lang_label(g), 270), YL, K);
        text(c, 340, 96, phr_("한글판: 한글 글꼴을 붙입니다."), WH, K); text(c, 340, 116, phr_("원판 일본어: 일본어 글꼴을 씁니다."), WH, K); text(c, 340, 136, phr_("자동: 제목·패치 표기로 판정합니다."), WH, K);
        text(c, 340, 168, dkey(phr_("{○}:언어 고르기")), CY, K);
    } else if (e && !e->pseudo) {
        int yy = 44; auto ln = [&](const char * lab, const std::string & val, uint32_t col2 = WH) { text(c, 340, yy, lab, CY, K); text(c, 340 + 76, yy, fit(val, 304 - 24 - 76 - 8), col2, K); yy += 20; };
        ln(phr_("출처"), e->vgi >= 0 ? std::string(phr_("변형 폴더")) : e->tag);
        const char * dn[] = { "A", "B", "C", "D" }; const char * dk[] = { "disk_a", "disk_b", "disk_c", "disk_d" };
        for (int q = 0; q < 4; q++) { auto it = e->kv.find(dk[q]); if (it != e->kv.end() && it->second != "-" && !it->second.empty()) ln((std::string(phr_("디스크")) + dn[q]).c_str(), disk_label(it->second)); }
        { auto it = e->kv.find("cd"); if (it != e->kv.end() && !it->second.empty() && it->second != "-") ln("CD", disk_label(it->second)); }
        { auto it = e->kv.find("boot"); std::string b = it == e->kv.end() ? std::string(phr_("자동")) : it->second; auto ft = e->kv.find("font"); auto fk = e->kv.find("font_kind"); ln(phr_("부팅"), b + phr_("   글꼴 ") + (ft != e->kv.end() ? ft->second : fk != e->kv.end() ? (fk->second == "jp" ? std::string(phr_("일본어")) : std::string(phr_("한글"))) : std::string(phr_("목록 기본")))); }
        { auto it = e->kv.find("bootkeys"); auto sk = e->kv.find("swap"); int bn = it != e->kv.end() ? (int)ent::parse_steps(it->second).size() : 0, sn = 0; if (sk != e->kv.end()) { std::string s = sk->second; if (!s.empty()) { sn = 1; for (char ch : s) if (ch == ',') sn++; } } ln(phr_("자동입력"), std::to_string(bn) + phr_("단계   교체 ") + std::to_string(sn) + phr_("장")); }
        if (ent_warn(entg, v[sel])) ln(phr_("경고"), e->warn_msg, OR);
    } else if (e) { text(c, 340, 44, phr_("항목 정의가 없는 작품입니다."), WH, K); text(c, 340, 64, phr_("자동 판정한 배치로 실행합니다."), WH, K); text(c, 340, 96, dkey(phr_("{□}:이 배치를 복사해 편집")), CY, K); }
    else { text(c, 340, 44, phr_("새 실행 방법을 만듭니다."), WH, K); text(c, 340, 64, phr_("템플릿 8종 또는 현재 항목 복제."), WH, K); }
    desc(c, 264, 64, { fit(msg.empty() ? std::string(phr_("실행 방법: 이 작품을 실행할 디스크 구성 (원본 이미지는 그대로)")) : msg, 560), fit(std::string(phr_("DB·game.ini 항목을 고치면 user.ini 에 따로 저장, 원본은 그대로")), 560) });
    helpline(c, phr_("↑↓:선택, ○:이걸로 정함, □:편집, △:메뉴, ×:뒤로"));
}
inline void Ui::k_ent(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            std::vector<int> v = vents(entg); int rows = (int)std::max<size_t>(v.size(), 1) + 3; mv(sel, rows);   /* t49a (#809): 마지막 행 = 표시 제목, 그 앞 = 언어, 그 앞 = 새 실행 방법 */   /* t44a (#494): 마지막 행 = 언어, 그 앞 = 새 실행 방법 */
            if (k == UK_OK || k == UK_SQ) {
                if (sel == rows - 1) { if (entg >= 0 && entg < (int)games.size()) { if (k == UK_OK) osk_open(2, games[entg].title, S_ENT, true); else if (!games[entg].title_user.empty()) yn_open(S_ENT, YN_TITLE, phr_("표시 제목을 DB 제목으로 되돌립니까?"), phr_("목록·정렬·창 제목이 원래 제목으로 돌아옵니다."), std::string()); } }
                else if (sel == rows - 2) { if (entg >= 0 && entg < (int)games.size()) open_lang({ games[entg].id }, { std::string() }, S_ENT); }
                else if (sel == rows - 3) { sel = 0; scr = S_TPL; }
                else if (k == UK_SQ) begin_edit_ent(entg, sel);
                else { if (!v.empty()) { if ((int)rent.size() <= entg) rent.resize(games.size(), -1); rent[entg] = v[sel]; } sync_cur(); scr = S_PICK; }
            }
            else if (k == UK_TRI) { if (sel < rows - 3 && !v.empty()) { menu_ent = sel; menusel = 0; scr = S_ENTMENU; } }
            else if (k == UK_CANCEL) { msg.clear(); scr = S_PICK; }
            break; }
    } while (0);
}
