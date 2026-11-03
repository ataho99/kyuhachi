#pragma once
/* screens/file.h -- S8c: S_FILE screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_file(Canvas & c) {
    titlebar(c, ""); static const char * kn[] = { "", "FD", "HDD", "CD" };
    std::string ttl = fl_key == "swap_add" ? phr_("교체 목록에 넣을 디스크") : fl_key == "swap_set" ? phr_("교체 디스크 바꾸기") : (fl_key == "cd" ? std::string("CD") : std::string(phr_("디스크 ")) + (char)toupper(fl_key.back())) + phr_(" 고르기");
    rbox(c, 16, 24, 608, 296, WH, ttl.c_str());
    { int x = 36; for (size_t t = 0; t < fl_roots.size() && t < 5; t++) { std::string nm = fit(fl_roots[t].first, 120); int w = tw(nm) + 12; if ((int)t == fl_ri) { c.rect(x - 4, 42, w, 21, WH); text(c, x + 2, 45, nm, K, WH); } else text(c, x + 2, 45, nm, CY, K); x += w + 6; } }
    text(c, 36, 68, fit(fl_dir, 570), YL, K);
    std::vector<std::string> r; std::vector<uint32_t> col;
    for (auto & f : fl_items) { if (f.dir) { r.push_back((f.name == ".." ? std::string(phr_(".. (위로)")) : "[" + f.name + "]")); col.push_back(YL); } else { std::string e = ent::ext_of(f.name); for (auto & ch : e) ch = (char)toupper((unsigned char)ch); r.push_back(fit(f.name, 400) + "   " + e + " " + size_str(f.size)); col.push_back(0); } }
    if (r.empty()) { r.push_back(phr_("(해당하는 파일이 없습니다)")); col.push_back(GY); }
    rowlist(c, 40, 94, 560, 9, r, sel, 24, &col);
    std::vector<std::string> d; d.push_back(fit(std::string(phr_("고를 것: ")) + kn[fl_filter()] + phr_(" 이미지.  이 게임 폴더 안이면 파일 이름만, 밖이면 전체 경로를 저장합니다."), 560)); d.push_back(fit(std::string(phr_("정렬: ")) + (fl_sort_kind ? phr_("종류순") : phr_("이름순")), 560));
    { const char * H = phr_("↑↓:선택, ○:고르기/열기, L R:위치, □:비우기, △:정렬, ×:취소"); desc(c, 324, 58, d, -1, H); helpline(c, H); }
}
inline void Ui::k_file(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do { {
            int cnt = (int)fl_items.size(); mv(sel, cnt);
            if (k == UK_L || k == UK_R) { int n = (int)fl_roots.size(); if (n > 0) { fl_ri = (fl_ri + (k == UK_R ? 1 : n - 1)) % n; fl_goto(fl_roots[fl_ri].second); } }
            else if (k == UK_OK && cnt > 0) {
                const FileItem f = fl_items[sel];
                if (f.dir) { if (f.name == "..") fl_goto(parent_of(fl_dir)); else fl_goto(fl_dir + std::string(1, env.sep) + f.name); }
                else fl_apply(fl_value(f.name));
            }
            else if (k == UK_SQ) fl_apply(std::string());
            else if (k == UK_TRI) { fl_sort_kind = !fl_sort_kind; fl_goto(fl_dir); }
            else if (k == UK_CANCEL) { scr = fl_ret; sel = fl_prev_sel; }
            break; }
    } while (0);
}
