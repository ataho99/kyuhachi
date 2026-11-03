#pragma once
/* screens/hddask.h -- S8c: S_HDDASK screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_hddask(Canvas & c) {
    titlebar(c, ""); int bx, by; confirm_box(c, 460, 180, phr_("설치가 필요한 게임"), bx, by);
    text(c, bx + 24, by + 22, phr_("이 게임은 하드디스크에 설치해야 합니다."), WH, K);
    text(c, bx + 24, by + 44, fit(phr_("하드디스크를 만들까요? (") + (kv_get("hdd_mb").empty() ? std::string("40") : kv_get("hdd_mb")) + phr_("MB, 원본은 그대로)"), 412), WH, K);
    std::vector<std::string> a = { phr_("예, 새로 만듭니다"), phr_("이미 있는 HDD 를 씁니다"), phr_("취소") }; items(c, bx + 40, by + 84, 380, a, hddsel, 28);
    helpline(c, phr_("↑↓:선택, ○:결정, ×:취소"));
}
inline void Ui::k_hddask(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do {
            mv(hddsel, 3);
            if (k == UK_OK) {
                if (hddsel == 0) {
                    std::string nm; int mb = atoi(kv_get("hdd_mb").empty() ? "40" : kv_get("hdd_mb").c_str());
                    if (env.hdd_make && env.hdd_make(eg, mb, nm)) { ed["disk_c"] = nm; ed["install"] = "hdd_new"; msg = phr_("HDD 를 만들었습니다: ") + nm; if (install_start) finish_install_start(a); else scr = S_EDIT; }
                    else { msg = phr_("HDD 를 만들지 못했습니다."); if (install_start) { install_start = false; scr = S_MAIN; } else scr = S_EDIT; }
                } else if (hddsel == 1) { if (install_start) { install_start = false; scr = S_MAIN; } else fl_open("disk_c"); }
                else { if (install_start) { install_start = false; scr = S_MAIN; } else scr = S_EDIT; }
            } else if (k == UK_CANCEL) { if (install_start) { install_start = false; scr = S_MAIN; } else scr = S_EDIT; }
            break;
    } while (0);
}
