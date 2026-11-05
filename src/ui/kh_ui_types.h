#pragma once
/* kh_ui_types.h -- S7: kh_ui.h 앞쪽 자료형(Act·SlotInfo·Env·UiGame·Scr 등)을 코드 그대로 옮김. struct Ui 는 kh_ui.h 에 남는다(S8 에서 화면별 분할). */
#include <stdlib.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include "ui/kh_draw.h"
#include "settings/kh_ent.h"
#include "settings/kh_settings.h"
#include "input/kh_ukey.h"

namespace ui {
using namespace ov;

enum ActT { A_NONE, A_START, A_LOAD, A_SAVE, A_RESUME, A_MAINMENU, A_QUIT, A_SENDKEY, A_DISKSWAP, A_APPLY, A_DELSLOT, A_EXTRA, A_ADD, A_WINMULT, A_FULLSCR, A_SHOT, A_TEST, A_SETLANG, A_KBD, A_KBBTN, A_KBSH, A_SETTITLE, A_NEXTSTAGE };   /* t44a (#494): A_SETLANG(game=작품 index, arg 0 자동 1 한글판 2 원판 일본어) */   /* r017: A_ADD(arg 0 파일 1 폴더 2 게임폴더 지정 3 다시 검색 4 해제) · A_WINMULT(arg=배율, 0=사용자 지정) */
struct Act { ActT t = A_NONE; int game = -1, slot = 0, arg = 0; bool resume = false; std::string entry; };   /* t39: entry = 실행할 항목 이름(빈칸 = 폴더 기본, 「-」 = 항목 없이) */

inline void apply_ov(Settings & s, const std::map<std::string, std::string> & m) {   /* 작품 프로필(DB·game.ini [override]) 값을 전역 위에 얹는다 */
    bool fxm = m.count("fx") != 0 || m.count("glow") != 0;   /* t48c: 새 형식 표지가 있으면 값 그대로, 없으면 옛 형식(shader=1 crt · scan/mask/bleed 옛 범위)으로 읽어 변환 */
    { std::map<std::string, std::string>::const_iterator it = m.find("shader"); if (it != m.end()) { int sv = atoi(it->second.c_str());
        if (fxm) s.shader = (sv == 2) ? 2 : 0;
        else if (sv == 1) { s.shader = 0; if (s.scan <= 0 && s.mask <= 0 && s.bleed <= 0 && s.glow <= 0) { s.scan = 5; s.mask = 3; } if (s.curve <= 0 && !m.count("curve")) s.curve = 4; }
        else { s.shader = (sv == 2) ? 2 : 0; } } }   /* t147c (답526-④): 옛 shader= 만으로 효과 5개를 0 으로 지우지 않는다 */
    auto cl = [](int v, int a, int b) { return v < a ? a : v > b ? b : v; };
    for (auto & kv : m) {
        int v = atoi(kv.second.c_str()); const std::string & k = kv.first;
        if (k == "ratio") s.ratio = cl(v, 0, 2); else if (k == "shader") { /* t48c: 위의 앞부분에서 처리 */ } else if (k == "curve") s.curve = cl(v, 0, 10); else if (k == "bleed") { s.bleed = cl(v, 0, 10); if (!fxm) s.glow = s.bleed; } else if (k == "glow") s.glow = cl(v, 0, 10);
        else if (k == "scan") s.scan = fxm ? cl(v, 0, 10) : cl(v, 1, 10); else if (k == "mask") s.mask = fxm ? cl(v, 0, 10) : fx_mask_leg(cl(v, 0, 3)); else if (k == "bgm") s.bgm = cl(v, 0, 8);
        else if (k == "se") s.se = cl(v, 0, 8); else if (k == "pad") s.pad = cl(v, 0, 3); else if (k == "boost") { static const int x[5] = { 2, 3, 4, 6, 8 }; int b = 1; for (int i = 0; i < 5; i++) if (x[i] == v) b = i; s.bmult = b; }
        else if (k == "boostmode") s.bmode = cl(v, 0, 2); else if (k == "bosd") s.bosd = cl(v, 0, 1); else if (k == "bsound") s.bsound = cl(v, 0, 2); else if (k == "kbbtn") s.kbbtn = cl(v, 0, 1); else if (k == "ovbtn") s.ovbtn = cl(v, 0, 1); else if (k == "padbtn") s.padbtn = cl(v, 0, 2); else if (k == "kbshader") s.kbshader = cl(v, 0, 1); else if (k == "fpsdisp") s.fpsd = cl(v, 0, 1);
    }
}
struct SlotInfo { bool used = false; std::string stamp, play; int bytes = 0; std::vector<uint32_t> th; int tw_ = 0, th_ = 0; };
/* ---- t39: 목록(탭·정렬) · 실행 방법 편집용 자료형 */
struct FileItem { std::string name; bool dir = false; long long size = 0; };
struct UiEnt {   /* 작품의 실행 방법 한 가지 */
    std::string name, label, tag, lang;   /* tag: DB · game.ini · 내 것 · 수정됨 · 변형 */
    bool hidden = false, pseudo = false; int vgi = -1;   /* pseudo = 항목 정의 없음(자동 판정 한 가지) · vgi = 변형 폴더의 작품 index(-1 이면 이 폴더) */
    ent::Kv kv; int warn = -1;   /* warn: -1 미계산 · 0 정상 · 1 경고/오류 */ std::string warn_msg;
};
struct UiFont { std::string name, group, path, why; bool ok = false; };   /* 글꼴 폴더의 bmp 한 개 */
struct ValRow { std::string key, val, src, cov; };   /* t48a (#614 ③): 적용값 표 한 줄 = 키·최종값·출처·덮인 값 */
struct Env {   /* 호스트(또는 하니스)가 채우는 입출력 */
    char sep = '/';
    std::function<std::vector<FileItem>(const std::string &)> list_dir;
    std::function<std::vector<std::string>()> roots;
    std::function<bool(int, const std::string &)> has_img;
    std::function<bool(int, const std::string &, const ent::Kv &, const std::string &)> save;   /* (작품, 항목, 값, 기본 지정할 항목 이름 또는 빈칸) */
    std::function<bool(int, const std::string &, bool)> del;   /* (작품, 항목, 되돌리기만) */
    std::function<bool(int, const std::string &, const std::string &)> setkey;   /* user.ini [game] key=val */
    std::function<bool(int, int, std::string &)> hdd_make;   /* (작품, MB, 만든 파일 이름) */
    std::function<bool(int, const std::string &, const ent::Kv &)> save_test;   /* 임시 user.ini 에 시험용으로 */
    std::function<bool(int, ent::Kv &)> resolved;   /* 지금 실제 배치(자동 판정 포함) */
    std::function<void(int)> reload;   /* 저장 뒤 항목 다시 읽기 */
    std::function<std::string(const std::string &)> font_group;   /* 글꼴 이름 → 묶음 */
    std::function<bool(int, std::vector<ValRow> &)> values;   /* t48a (#614 ③): 작품의 적용값 표 */
    std::function<void(int, std::map<std::string, std::string> &)> pad_get;   /* t48e (#646): 작품 user.ini [pad] 의 btn_* */
    std::function<bool(int, const std::string &, const std::string &)> pad_set;   /* (작품, btn_ 키, 값; 빈 값 = 지움) */
    std::function<int(int, int &, bool &)> pad_cap;   /* hotkey combo capture: 0 begin - 1 poll (0 wait 1 got 2 ended) - 2 end - 3 reload the input-layer table */
    std::function<void(int)> raw_ctl;   /* extra-buttons tab: 1 = start watching raw input, 0 = stop */
    std::function<std::string(int)> raw_line;   /* i >= 0: i-th newest raw input line; -1: "EXIT" when the tab should close */
    std::function<int()> padkind;   /* 0 그 밖 1 Xbox 2 PlayStation 3 Retroid(닌텐도 배치) */
    std::function<bool(int, const std::string &, const std::string &, std::vector<FileItem> &)> exelist;   /* 2b: (작품, 디스크 이름·경로, 이미지 안 폴더, 목록 = 하위 폴더 + 실행 파일) */
    std::function<std::vector<std::string>()> padkeys;   /* 고를 수 있는 키 이름(저장용, 소문자) */
};
struct UiGame {
    std::string id, title, boot_note; std::string disk[2];
    std::string company, year, genre, patch, devices, disknote;   /* gamedb 항목(인포메이션) */
    int play_s = 0; bool has_demo = false;   /* 플레이 시간(초) · 오프닝(데모) 디스크 유무 */
    std::vector<std::string> fds; std::string cur_fd[2];   /* 디스크 교체 목록(표시 이름) · 지금 A/B 에 들어 있는 것 */ std::map<std::string, std::string> ov; std::string src;
    std::vector<uint32_t> thmb; int tw_ = 0, th_ = 0;  /* 마지막 저장 THMB (0x00RRGGBB) */
    SlotInfo slot[5]; int lastslot = -1;   /* 1b: 5칸 = 0 자동 · 1 퀵 · 2~4 일반 1~3 */
    /* r017: 판정·출처·실행 불가 */
    char grade = '-'; std::string judge, metasrc, block, jshort, dosnote; bool autoreg = false;
    std::string title_db, title_user;   /* t49a (#809): DB·game.ini 제목 / 사용자 지정 표시 제목(user.ini [game] title_user=, 비면 없음) */
    std::string lang_exp;   /* t44a (#494): 사용자 지정 언어(user.ini [game] lang=) — ko/jp, 비면 자동 */
    std::string more;   /* r019: 함께 있는 이미지(같은 작품의 다른 이미지) 목록 */
    std::string orig, dbhit;   /* r018: 원판 제목(변형 판정) · DB 일치한 작품 이름 */
    /* t39 */
    std::string lang, short_t, dir, imgdir, variant_of, fgroup, gfont, defent;   /* lang ko|jp|빈값(미분류) · short= · 폴더 · 이미지 폴더 · 변형이면 기반 id · 글꼴 묶음(전용=custom) · 전용 글꼴 이름 · 기본 항목 */
    int base = -1; bool fav = false, need_install = false; long long last_run = 0; int order_n = 0;   /* base = 변형이면 기반 작품 index */
    std::vector<UiEnt> ents;
};

enum Scr { S_MAIN, S_PICK, S_SCREEN, S_CONF, S_SAVE, S_EXTRA, S_QUIT, S_GAME, S_KEYS, S_HELP, S_START, S_SWAP, S_SWAPI, S_ABOUT, S_ADD, S_SORT, S_ENT, S_ENTMENU, S_TPL, S_EDIT, S_FILE, S_KEYSEQ, S_STEP, S_SWAPED, S_FONT, S_OSK, S_NAME, S_YN, S_HDDASK, S_LANG, S_VALS, S_PAD, S_PADKEY, S_EXEPICK, S_HOT, S_EXT };

}   /* namespace ui */
