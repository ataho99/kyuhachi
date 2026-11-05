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
#include "ui/screens/ent.h"
#include "ui/screens/entmenu.h"
#include "ui/screens/tpl.h"
#include "ui/screens/edit.h"
#include "ui/screens/file.h"
#include "ui/screens/keyseq.h"
#include "ui/screens/step.h"
#include "ui/screens/swaped.h"
#include "ui/screens/name.h"
#include "ui/screens/yn.h"
#include "ui/screens/hddask.h"
#include "ui/screens/screen.h"
#include "ui/screens/conf.h"
#include "ui/screens/save.h"
#include "ui/screens/game.h"
#include "ui/screens/keys.h"
#include "ui/screens/help.h"
#include "ui/screens/vals.h"
#include "ui/screens/pad.h"
#include "ui/screens/padkey.h"
#include "ui/screens/kbpick.h"
#include "ui/screens/set.h"
#include "ui/screens/hot.h"
#include "ui/screens/ext.h"
#include "ui/screens/font.h"
#include "ui/screens/osk.h"
#include "ui/screens/exepick.h"

inline void Ui::draw(Canvas & c) {
    hits.clear();
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
    case S_EXEPICK: d_exepick(c); break;
    case S_PAD: d_pad(c); break; case S_PADKEY: d_padkey(c); break; case S_HOT: d_hot(c); break; case S_EXT: d_ext(c); break; case S_KBPICK: d_kbpick(c); break; case S_SET: d_set(c); break; case S_GKB: d_gkb(c); break;
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
    case S_SCREEN: case S_CONF: return k_screen_conf(k, a);
    case S_SAVE: k_save(k, a); break;
    case S_START: k_start(k, a); break;
    case S_SWAP: k_swap(k, a); break;
    case S_SWAPI: k_swapi(k, a); break;
    case S_ABOUT: k_about(k, a); break;
    case S_EXTRA: k_extra(k, a); break;
    case S_HELP: k_help(k, a); break;
    case S_GAME: k_game(k, a); break;
    case S_KEYS: k_keys(k, a); break;
    default: break;
    }
    return a;
}
