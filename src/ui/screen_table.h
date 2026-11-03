#pragma once
/* screen_table.h -- S8a: Ui::draw and Ui::key dispatch. The switch bodies are the original ones; home-flow screens call screens/*.h. */
#include "ui/screens/main.h"
#include "ui/screens/pick.h"
#include "ui/screens/start.h"
#include "ui/screens/extra.h"
#include "ui/screens/about.h"
#include "ui/screens/quit.h"
#include "ui/screens/lang.h"
#include "ui/screens/sort.h"
#include "ui/screens/swap.h"
#include "ui/screens/swapi.h"
#include "ui/screens/add.h"

inline void Ui::draw(Canvas & c) {
    jp_refresh();   /* t52h (#134) */
    if (scr != S_GAME && scr != S_KEYS && !(in_game && (scr == S_QUIT || scr == S_SWAP || scr == S_SWAPI))) c.rect(0, 0, c.w, c.h, K);
    switch (scr) {
    case S_MAIN: d_main(c, 0); break;
    case S_START: if (start_ret == S_PICK) { d_pick(c); d_main(c, 3); } else d_main(c, 1); break;
    case S_QUIT: if (in_game) d_quit(c); else d_main(c, 2); break;
    case S_SWAP: d_swap(c); break;
    case S_SWAPI: d_swapi(c); break;
    case S_ABOUT: d_about(c); break;
    case S_PICK: d_pick(c); break;
    case S_SCREEN: d_screen(c); break;
    case S_CONF: d_conf(c); break;
    case S_SAVE: d_save(c); break;
    case S_EXTRA: d_extra(c); break;
    case S_GAME: d_game(c); break;
    case S_KEYS: d_keys(c); break;
    case S_HELP: d_help(c); break;
    case S_ADD: d_add(c); break;
    case S_SORT: d_sort(c); break; case S_ENT: d_ent(c); break; case S_ENTMENU: d_entmenu(c); break; case S_TPL: d_tpl(c); break; case S_EDIT: d_edit(c); break;
    case S_FILE: d_file(c); break; case S_KEYSEQ: d_keyseq(c); break; case S_STEP: d_step(c); break; case S_SWAPED: d_swaped(c); break; case S_FONT: d_font(c); break;
    case S_OSK: d_osk(c); break; case S_NAME: d_name(c); break; case S_YN: d_yn(c, yn_ret); break; case S_HDDASK: d_hddask(c); break; case S_LANG: d_lang(c); break; case S_VALS: d_vals(c); break;
    case S_PAD: d_pad(c); break; case S_PADKEY: d_padkey(c); break;
    }
}

inline Act Ui::key(UKey k) {
    Act a; if (k == UK_NONE) return a;
    if (new_scr()) { Act r2 = key2(k); if (pend.t != A_NONE) { r2 = pend; pend = Act(); } return r2; }
    if (scr == S_QUIT) { k_quit(k, a); return a; }
    int n = nitems(), & cs = curSel(); if (n < 1) n = 1;
    if (k == UK_UP || k == UK_DOWN) {
        if (scr == S_PICK && reorder) { int p2 = pick + (k == UK_UP ? -1 : 1); if (pick < (int)vis.size() && p2 >= 0 && p2 < (int)vis.size()) { std::swap(vis[pick], vis[p2]); pick = p2; renumber_order(); dirty = true; } return a; }
        cs = (k == UK_UP) ? (cs + n - 1) % n : (cs + 1) % n; if (scr == S_PICK && pick < (int)vis.size()) sync_cur(); return a;
    }
    switch (scr) {
    case S_MAIN: k_main(k, a); break;
    case S_PICK: k_pick(k, a); break;
    case S_ADD: k_add(k, a); break;
    case S_SCREEN: case S_CONF: {
        if (scr == S_CONF && sel == 13 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT)) { int dl = (k == UK_LEFT) ? -1 : 1; lang = (lang + dl + 3) % 3; phr_set_lang(lang); lang_chg = true; dirty = true; break; }   /* t52g (#133): 언어 — 문구 칸이 제자리에서 바뀌어 바로 보인다 */
        if (scr == S_CONF && sel >= 11 && sel <= 12 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT || k == UK_TRI)) { if (k == UK_OK) open_font(sel - 11, S_CONF); break; }   /* t52f: 글꼴 행이 11·12 로 한 칸씩 내려감 */
        if (scr == S_CONF && sel == 4 && (k == UK_OK || k == UK_LEFT || k == UK_RIGHT)) { if (k == UK_OK) go(S_PAD, 0); break; }   /* t48e (#646) */
        bool scr_s = scr == S_SCREEN; int nrow = scr_s ? 10 : 14;
        if (scr_s && sel == 9 && (k == UK_LEFT || k == UK_RIGHT || k == UK_OK)) { a.t = A_FULLSCR; return a; }   /* t52i (B3): 전체화면 줄 — 좌우·○ 로 바로 오간다 */
        Settings & s = (sel == 0) ? eff() : edit();
        if (k == UK_LEFT || k == UK_RIGHT) {
            int d = (k == UK_RIGHT) ? 1 : -1;
            if (scr_s && sel == 8) {   /* 창 배율(전역): 모니터에 들어가는 다음/이전 배율 */
                int m = wm_next(d); if (m > 0) { wmult = m; a.t = A_WINMULT; a.arg = m; dirty = true; }
                return a;
            }
            if (sel == 0) { scope = clampi(scope + d, 0, 1); if (scope == 1 && !has_t[cur]) { t[cur] = effective(cur); has_t[cur] = true; } }
            else if (scr_s) {
                if (sel == 1) s.ratio = clampi(s.ratio + d, 0, 2);
                if (sel == 2) s.shader = d > 0 ? 2 : 0;   /* t48c: 필터 = sharp / nearest */
                { if (sel == 3) s.scan = clampi(s.scan + d, 0, 10); if (sel == 4) s.mask = clampi(s.mask + d, 0, 10); if (sel == 5) s.bleed = clampi(s.bleed + d, 0, 10); if (sel == 6) s.glow = clampi(s.glow + d, 0, 10); if (sel == 7) s.curve = clampi(s.curve + d, 0, 10); }   /* t48c: 필터와 상관없이 효과를 켠다. 바꾸면 A_APPLY 로 곧바로 화면에 반영 */
                a.t = A_APPLY;
            } else {
                if (sel == 1) s.bgm = clampi(s.bgm + d, 0, 8);
                if (sel == 2) s.se = clampi(s.se + d, 0, 8);
                if (sel == 5) s.bmult = clampi(s.bmult + d, 0, 4);
                if (sel == 6) s.bmode = clampi(s.bmode + d, 0, 2);
                if (sel == 7) s.bsound = clampi(s.bsound + d, 0, 2);
                if (sel == 8) s.boot = clampi(s.boot + d, 0, 1); if (sel == 9) s.bosd = clampi(s.bosd + d, 0, 1); if (sel == 10) s.ovbtn = clampi(s.ovbtn + d, 0, 1);
                a.t = A_APPLY;
            }
            dirty = true; (void)nrow;
        } else if (k == UK_TRI) { Settings def; Settings & e = eff(); if (scr_s) { e.ratio = def.ratio; e.shader = def.shader; e.curve = def.curve; e.bleed = def.bleed; e.glow = def.glow; e.scan = def.scan; e.mask = def.mask; } else { e.bgm = def.bgm; e.se = def.se; e.bmult = def.bmult; e.bmode = def.bmode; e.boot = def.boot; e.bosd = def.bosd; e.bsound = def.bsound; e.ovbtn = def.ovbtn; } dirty = true; a.t = A_APPLY; }
        else if (k == UK_CANCEL) { if (!scr_s) { cb_has = false; a.t = A_APPLY; dirty = true; }   /* t52i: × = 뒤로. 값은 바뀐 순간부터 저장 대상 */ if (scr_s && backup_has) { eff() = backup; backup_has = false; a.t = A_APPLY; if (wmult != bk_wmult || winw != bk_winw || winh != bk_winh) { wmult = bk_wmult; winw = bk_winw; winh = bk_winh; a.t = A_WINMULT; a.arg = wmult; } } go(in_game ? S_GAME : S_MAIN, in_game ? (scr == S_SCREEN ? 4 : 5) : 0); if (!in_game) sel = 0; }
        else if (k == UK_OK) { backup_has = false; cb_has = false; dirty = true; a.t = A_APPLY; go(in_game ? S_GAME : S_MAIN, in_game ? (scr == S_SCREEN ? 4 : 5) : 0); }
        break; }
    case S_SAVE:
        if (k == UK_OK) { if (!in_game && !blk_cur().empty()) { startsel = 1; start_ret = S_MAIN; go(S_START, 1); break; } a.t = A_LOAD; a.game = cur; a.slot = slotsel; }   /* t48b2 (#632): 메뉴에서 슬롯 불러오기도 block 이면 같은 「게임 시작」 팝업(사유 표시)으로 막는다. 게임 안(in_game)은 이미 돌고 있어 해당 없음 */
        else if (k == UK_SQ && in_game) { a.t = A_SAVE; a.game = cur; a.slot = slotsel; }
        else if (k == UK_TRI) { const UiGame * gm0 = game(); if (gm0 && slotsel >= 0 && slotsel < 8 && gm0->slot[slotsel].used) yn_open(S_SAVE, YN_DELSLOT, phr_("슬롯 ") + std::to_string(slotsel + 1) + phr_("의 저장을 지웁니다."), phr_("지운 저장은 되돌릴 수 없습니다."), phr_("지울까요?"), slotsel); }   /* t52e (#131 ②) */
        else if (k == UK_CANCEL) { go(in_game ? S_GAME : S_MAIN, 0); if (in_game) sel = 2; }
        break;
    case S_START: k_start(k, a); break;
    case S_SWAP: k_swap(k, a); break;
    case S_SWAPI: k_swapi(k, a); break;
    case S_ABOUT: k_about(k, a); break;
    case S_EXTRA: k_extra(k, a); break;
    case S_HELP:
        if (k == UK_OK || k == UK_CANCEL) go(S_EXTRA, 3);
        break;
    case S_GAME:
        if (k == UK_LEFT && sel == 1) slotsel = (slotsel + 7) % 8; if (k == UK_RIGHT && sel == 1) slotsel = (slotsel + 1) % 8;
        if (k == UK_CANCEL) a.t = A_RESUME;
        else if (k == UK_OK) {
            switch (sel) {
            case 0: a.t = A_RESUME; break;
            case 1: a.t = A_SAVE; a.game = cur; a.slot = slotsel; break;
            case 2: go(S_SAVE, 0); break;
            case 3: swapdrv = 0; go(S_SWAP, 0); break;
            case 4: enter_screen_cfg(); go(S_SCREEN, 0); break;
            case 5: enter_conf_cfg(); go(S_CONF, 0); break;
            case 6: go(S_KEYS, 0); break;
            case 7: a.t = A_KBD; break;
            case 8: a.t = A_KBBTN; break;
            case 9: a.t = A_KBSH; break;
            case 10: a.t = A_SHOT; break;
            case 11: yn_open(S_GAME, YN_MAINMENU, phr_("게임을 끝내고 메인 메뉴로 갑니다."), phr_("저장하지 않은 진행은 사라집니다."), phr_("메인 메뉴로 갑니까?")); break;   /* t52e (#131 ①): 기본 = 아니오 */
            case 12: go(S_QUIT, 0); quitsel = 0; break;
            case 13: if (!stage_next_label.empty()) a.t = A_NEXTSTAGE; break;

            }
        }
        break;
    case S_KEYS:
        if (k == UK_OK) { a.t = A_SENDKEY; a.arg = sel; } else if (k == UK_CANCEL) go(S_GAME, 6);
        break;
    default: break;
    }
    return a;
}
