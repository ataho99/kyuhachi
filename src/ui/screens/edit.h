#pragma once
/* screens/edit.h -- S8c: S_EDIT screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_edit(Canvas & c) {
    titlebar(c, ""); const UiGame & g = games[eg];
    rbox(c, 16, 24, 608, 296, WH, ttl_suf(g, phr_("편집"), 560).c_str());
    const char * lab[] = { phr_("이름"), phr_("디스크 A"), phr_("디스크 B"), phr_("디스크 C"), phr_("디스크 D"), "CD", phr_("부팅 대상"), phr_("자동 입력"), phr_("교체 목록"), phr_("글꼴"), phr_("마우스"), phr_("설치"), phr_("실행 파일"), phr_("실행 인자") };
    std::vector<std::string> v(14);
    v[0] = ed_name; const char * dk[] = { "disk_a", "disk_b", "disk_c", "disk_d", "cd" }; for (int i = 0; i < 5; i++) v[1 + i] = disk_label(kv_get(dk[i]));
    { std::string b = kv_get("boot"); v[6] = b.empty() ? std::string(phr_("자동")) : b; }
    { std::string s = kv_get("bootkeys"); v[7] = s.empty() ? std::string(phr_("없음")) : std::to_string((int)ent::parse_steps(s).size()) + phr_("단계"); }
    { std::string s = kv_get("swap"); int n = 0; if (!s.empty()) { n = 1; for (char ch : s) if (ch == ',') n++; } v[8] = std::to_string(n) + phr_("장"); }
    v[9] = font_label(kv_get("font"), kv_get("font_kind")); v[10] = mouse_label(kv_get("mouse"));
    v[11] = kv_get("install") == "hdd_new" ? phr_("HDD 새로 만들기 (") + (kv_get("hdd_mb").empty() ? std::string("40") : kv_get("hdd_mb")) + "MB)" : phr_("아님");
    { std::string x = kv_get("dosexe"); if (x.empty()) v[12] = phr_("자동"); else if (x == "*") v[12] = phr_("DOS 프롬프트"); else { x[0] = (char)toupper((unsigned char)x[0]); v[12] = x; } }   /* 2b */
    { std::string s = kv_get("dosargs"); v[13] = s.empty() ? std::string(phr_("없음")) : s; }
    for (int i = 0; i < 14; i++) {
        int yy = 42 + i * 19; bool sl = i == sel; if (sl) c.rect(30, yy - 3, 580, 19, WH);
        text(c, 40, yy, lab[i], sl ? K : CY, sl ? WH : K); c.clip(160, yy - 3, 440, 22);
        bool valarr = (i == 6 || i == 10 || i == 11); text(c, 160, yy, std::string(valarr && sl ? "← " : "") + fit(v[i], 380) + (valarr && sl ? " →" : ""), sl ? K : WH, sl ? WH : K); c.unclip();
    }
    std::vector<ent::Issue> is = ed_issues(); std::vector<std::string> d;
    if (!msg.empty()) d.push_back(fit(msg, 560)); else d.push_back(is.empty() ? std::string(phr_("경고 없음")) : fit(std::string(is[0].err ? phr_("오류: ") : phr_("경고: ")) + is[0].msg, 560));
    d.push_back(fit(dkey(std::string(ed_setdef ? phr_("저장하면 기본 실행 방법으로 지정합니다.") : phr_("기본 지정은 {SEL}{로} 켜고 끕니다.")) + (ed_dirty() ? phr_("  (바꾼 값 있음)") : "")), 560));
    static const char * H_EDIT = phr_("↑↓:선택, ←→:값, ○:고르기, □:저장, △:시험, SEL:기본 지정, ×:나가기");   /* t50a 안 B: 「시험 실행」→「시험」 (한 줄에 들어감) */
    desc(c, 324, 58, d, (!msg.empty() || (!is.empty() && is[0].err)) ? 0 : -1, H_EDIT);
    helpline(c, H_EDIT);
}
inline void Ui::k_edit(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            mv(sel, 14); if (k == UK_UP || k == UK_DOWN) { msg.clear(); break; }
            if (k == UK_LEFT || k == UK_RIGHT) {
                int d = k == UK_RIGHT ? 1 : -1; msg.clear();
                if (sel == 6) { static const char * bs[] = { "", "A", "B", "C", "D" }; int cu = 0; std::string b = kv_get("boot"); for (int i = 0; i < 5; i++) if (b == bs[i]) cu = i; cu = (cu + d + 5) % 5; kv_set("boot", bs[cu]); }
                else if (sel == 10) { static const char * ms[] = { "", "none", "optional", "pointer", "required" }; int cu = 0; std::string m = kv_get("mouse"); for (int i = 0; i < 5; i++) if (m == ms[i]) cu = i; cu = (cu + d + 5) % 5; kv_set("mouse", ms[cu]); }
                else if (sel == 11) { if (kv_get("install") == "hdd_new") { ed.erase("install"); ed.erase("hdd_mb"); } else { ed["install"] = "hdd_new"; if (!ed.count("hdd_mb")) ed["hdd_mb"] = "40"; } }
                break;
            }
            if (k == UK_OK) {
                static const char * dk[] = { "", "disk_a", "disk_b", "disk_c", "disk_d", "cd" }; msg.clear();
                if (sel == 0) { sel = 0; scr = S_NAME; }
                else if (sel >= 1 && sel <= 5) fl_open(dk[sel]);
                else if (sel == 7) { ks = ent::parse_steps(kv_get("bootkeys")); sel = 0; scr = S_KEYSEQ; }
                else if (sel == 8) { sw = split_commas(kv_get("swap")); sel = 0; scr = S_SWAPED; }
                else if (sel == 9) open_font(2, S_EDIT);
                else if (sel == 12) xp_open();
                else if (sel == 13) osk_open(4, kv_get("dosargs"), S_EDIT);
                else if (sel == 11) { if (kv_get("install") == "hdd_new") { if (kv_get("disk_c").empty()) { hddsel = 0; install_start = false; scr = S_HDDASK; } } else { ed["install"] = "hdd_new"; if (!ed.count("hdd_mb")) ed["hdd_mb"] = "40"; } }
            }
            else if (k == UK_SQ) { if (save_ed()) leave_edit_to_ent(); }
            else if (k == UK_TRI) { std::vector<ent::Issue> is = ed_issues(); if (ent::has_error(is)) { msg = phr_("오류가 있어 시험할 수 없습니다: ") + is[0].msg; } else yn_open(S_EDIT, YN_TEST, phr_("저장하지 않은 값으로 ") + std::to_string(test_secs()) + phr_("초 실행해 봅니다."), phr_("세이브는 임시 폴더에 쓰고 끝나면 지웁니다."), phr_("실행할까요?")); }
            else if (k == UK_SELECT) ed_setdef = !ed_setdef;
            else if (k == UK_CANCEL) { if (ed_dirty()) yn_open(S_EDIT, YN_DISCARD, phr_("바꾼 내용을 저장하지 않고 나갑니까?")); else leave_edit_to_ent(); }
            break; }
    } while (0);
}
