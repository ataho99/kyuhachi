#pragma once
/* screens/entmenu.h -- S8c: S_ENTMENU screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_entmenu(Canvas & c) {
    d_ent(c); int bx, by; confirm_box(c, 340, 300, phr_("실행 방법 메뉴"), bx, by);
    std::vector<std::string> a = { phr_("기본으로 지정"), phr_("복제"), phr_("삭제 (숨김)"), phr_("DB 기본값으로 되돌리기"), phr_("위로"), phr_("아래로"), fit(phr_("다음 단계: ") + next_label(entg, menu_ent), 270), fit(phr_("단계 메뉴 이름: ") + next_menu_name(entg, menu_ent), 270), phr_("shr_make") };   /* t49a (#845·#847) */
    std::vector<bool> dm = menu_dim(); items(c, bx + 30, by + 24, 290, a, menusel, 28, &dm);
    helpline(c, phr_("↑↓:선택, ○:결정, ×:닫기"));
}
inline void Ui::k_entmenu(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            mv(menusel, 9); std::vector<bool> dm = menu_dim(); std::vector<int> v = vents(entg); int ei = (menu_ent >= 0 && menu_ent < (int)v.size()) ? v[menu_ent] : -1;
            if (k == UK_CANCEL) { scr = S_ENT; break; }
            if (k != UK_OK || dm[menusel] || ei < 0) break; const UiEnt & e = games[entg].ents[ei];
            if (menusel == 0) { if (env.setkey) env.setkey(entg, "entry", e.name); if (env.reload) env.reload(entg); dirty = true; msg = phr_("기본 실행 방법으로 지정했습니다."); scr = S_ENT; }
            else if (menusel == 1) { ent::Kv kv = e.kv; begin_new_from_kv(entg, kv, e.label + phr_(" 복사")); }
            else if (menusel == 2) yn_open(S_ENT, YN_DELETE, "「" + fit(e.label, 300) + phr_("」 을(를) 목록에서 지웁니까?"), e.tag == "내 것" ? phr_("내가 만든 항목이라 완전히 지워집니다.") : phr_("원본(DB·game.ini)은 그대로이고 숨김 처리만 합니다."), std::string(), menu_ent);
            else if (menusel == 3) yn_open(S_ENT, YN_RESET, "「" + fit(e.label, 300) + phr_("」 의 내 수정을 버리고"), phr_("DB·game.ini 의 기본값으로 되돌립니까?"), std::string(), menu_ent);
            else if (menusel == 8) { shr_open_make(entg, menu_ent); }
            else if (menusel == 7) { nl_name = e.name; osk_open(3, next_menu_name(entg, menu_ent), S_ENT, true); }   /* t49a (#847): 속성 「다음 단계로」 의 메뉴 이름 바꾸기 */
            else if (menusel == 6) {   /* t49a (#845): 다음 단계 돌려 고르기 — user.ini [entry:<이름>] next= (기존 저장 경로) */
                std::vector<std::string> cs = next_cands(entg, v[menu_ent]); auto it = e.kv.find("next"); std::string cur_n = it == e.kv.end() || it->second.empty() ? std::string("none") : it->second; size_t ci = 0; for (size_t i = 0; i < cs.size(); i++) if (cs[i] == cur_n) ci = i;
                std::string nv = cs[(ci + 1) % cs.size()]; ent::Kv kv = e.kv; kv["next"] = nv; std::string nm = e.name;
                bool ok = env.save && env.save(entg, nm, kv, std::string()); if (ok && env.reload) env.reload(entg); dirty = true; msg = ok ? std::string(phr_("다음 단계: ")) + next_label(entg, menu_ent) + phr_("(으)로 바꿨습니다.") : std::string(phr_("저장하지 못했습니다."));   /* 1b-3: 바뀐 값을 알림에 넣는다 */
            }
            else {
                std::vector<std::string> names; for (size_t i = 0; i < games[entg].ents.size(); i++) if (!games[entg].ents[i].pseudo && games[entg].ents[i].vgi < 0) names.push_back(games[entg].ents[i].name);
                int a0 = -1; for (size_t i = 0; i < names.size(); i++) if (names[i] == e.name) a0 = (int)i;
                int b0 = menusel == 4 ? menu_ent - 1 : menu_ent + 1; std::vector<int> vn; for (int i : v) if (games[entg].ents[i].vgi < 0) vn.push_back(i);
                if (a0 >= 0 && b0 >= 0 && b0 < (int)v.size() && games[entg].ents[v[b0]].vgi < 0) {
                    std::string other = games[entg].ents[v[b0]].name; int o0 = -1; for (size_t i = 0; i < names.size(); i++) if (names[i] == other) o0 = (int)i;
                    if (o0 >= 0) { std::swap(names[a0], names[o0]); if (env.setkey) env.setkey(entg, "entry_order", join_commas(names)); if (env.reload) env.reload(entg); dirty = true; sel = b0; menu_ent = b0; }
                }
            }
            break; }
    } while (0);
}
