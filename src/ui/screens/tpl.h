#pragma once
/* screens/tpl.h -- S8c: S_TPL screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::d_tpl(Canvas & c) {
    titlebar(c, ""); rbox(c, 16, 24, 288, 296, WH, phr_("새 실행 방법"));
    std::vector<std::string> r; for (int i = 0; i < 8; i++) r.push_back(std::to_string(i + 1) + " " + phr_(ent::TPLS[i].label)); r.push_back(phr_("- 지금 배치 복제")); r.push_back(phr_("shr_in"));
    items(c, 40, 46, 256, r, sel, 26);
    rbox(c, 320, 24, 304, 296, WH, phr_("설명"));
    static const char * dd[10][4] = {
        { phr_("하드디스크 한 장으로 부팅합니다."), phr_("예: 동급생2, 열락의 학원"), "", "" }, { phr_("하드디스크로 부팅하고 FD 를 붙입니다."), phr_("부팅 뒤 자동 입력도 쓸 수 있습니다."), phr_("예: 나디아"), "" },
        { phr_("FD 한 장(또는 두 장)으로 부팅합니다."), phr_("예: 대마도95, 키키모라, 바요엔"), "", "" }, { phr_("FD 여러 장을 도중에 갈아 끼웁니다."), phr_("교체 목록을 따로 편집합니다."), phr_("예: 마도물어 A.R.S"), "" },
        { phr_("CD 이미지를 붙이고 내장 DOS 로 시작합니다."), phr_("자동 실행(autoexec)을 채워야 합니다."), phr_("예: 마도물어 456"), "" }, { phr_("데모(오프닝) 디스크를 먼저 실행합니다."), phr_("예: 환세희담"), "", "" },
        { phr_("원판 일본어 같은 변형을 만듭니다."), phr_("글꼴을 일본어로 정해 둡니다."), "", "" }, { phr_("하드디스크에 설치해야 하는 게임입니다."), phr_("빈 HDD 를 새 이름으로 만듭니다."), phr_("원본 이미지는 쓰기 대상에서 제외됩니다."), "" },
        { phr_("지금 실제로 쓰는 배치를 그대로 복사해"), phr_("새 항목으로 편집합니다."), "", "" }, { phr_("shr_d1"), phr_("shr_d2"), "", "" } };
    for (int i = 0; i < 4; i++) if (dd[sel < 10 ? sel : 0][i][0]) text(c, 344, 48 + i * 22, dd[sel < 10 ? sel : 0][i], i == 0 ? YL : WH, K);
    text(c, 344, 160, phr_("이미지가 하나뿐이면 알맞은 칸에"), CY, K); text(c, 344, 180, phr_("자동으로 채웁니다."), CY, K);
    helpline(c, phr_("↑↓:선택, ○:만들기, ×:뒤로"));
}
inline void Ui::k_tpl(UKey k, Act & a) {
    auto mv = [&](int & s, int cnt) { if (cnt < 1) cnt = 1; if (k == UK_UP) s = (s + cnt - 1) % cnt; else if (k == UK_DOWN) s = (s + 1) % cnt; };
    do {
            mv(sel, 10);
            if (k == UK_OK) { if (sel == 9) shr_open_import(); else begin_new_tpl(sel); } else if (k == UK_CANCEL) { sel = 0; scr = S_ENT; }
            break;
    } while (0);
}
