#!/usr/bin/perl
# 지시 #136 t52i: B1(작품 목록 ○=시작, 시작 팝업에 「시스템 디스크만 부팅」) · B2(조작 안내 한 장 표) · B3(배속 입력→동작 표 · 화면 단축키 줄 · 화면 설정 「전체화면」 줄) · 동작 환경 × = 뒤로(값은 바로 저장) · 버전
# usage: perl patch_t52i.pl <host.cpp> <ui.h> <ov.h> <kh_version.h>
use strict; use warnings;
my ($hf, $uf, $of, $vf) = @ARGV;
sub slurp { my $f = shift; open(my $h, "<:raw", $f) or die "read $f"; local $/; my $s = <$h>; close $h; $s }
sub spit { my ($f, $s) = @_; open(my $h, ">:raw", $f) or die "write $f"; print $h $s; close $h; }
sub rep { my ($sr, $a, $b) = @_; my $n = () = $$sr =~ /\Q$a\E/g; die "anchor count $n for: $a" if $n != 1; my $i = index($$sr, $a); substr($$sr, $i, length($a)) = $b; }
sub repn { my ($sr, $a, $b, $cnt) = @_; my $n = () = $$sr =~ /\Q$a\E/g; die "anchor count $n (want $cnt) for: $a" if $n != $cnt; $$sr =~ s/\Q$a\E/$b/g; }

# ================= ui.h =================
my $u = slurp($uf);
# 동작 종류 + 상태
rep(\$u, "A_WINMULT, A_SHOT,", "A_WINMULT, A_FULLSCR, A_SHOT,");
rep(\$u, "Settings backup; bool backup_has = false;", "bool fs_now = false; Scr start_ret = S_MAIN;   /* t52i: fs_now = 호스트가 알려 주는 전체화면 여부 · start_ret = 시작 팝업을 연 화면(S_MAIN/S_PICK) */\n    Settings backup; bool backup_has = false;");
# 그림: 시작 팝업은 연 화면 위에
rep(\$u, "case S_START: d_main(c, 1); break;", "case S_START: if (start_ret == S_PICK) { d_pick(c); d_main(c, 3); } else d_main(c, 1); break;");
# d_main: 모드 3 = 팝업만
rep(\$u, "    void d_main(Canvas & c, int mode) {\n        titlebar(c, \"\");", "    void d_main(Canvas & c, int mode) {\n        if (mode != 3) {\n        titlebar(c, \"\");");
rep(\$u, "        if (mode == 0) helpline(c, phr_(\"↑↓:선택, ○:결정, ×:취소, SELECT:부가\"));\n        if (mode == 1) {", "        if (mode == 0) helpline(c, phr_(\"↑↓:선택, ○:결정, ×:취소, SELECT:부가\"));\n        }\n        if (mode == 1 || mode == 3) {");
rep(\$u, "confirm_box(c, 400, 168, phr_(\"게임 시작\"), bx, by);", "confirm_box(c, 400, 196, phr_(\"게임 시작\"), bx, by);");
rep(\$u, "phr_(\"이어하기   (저장 없음)\")), phr_(\"처음부터\") };", "phr_(\"이어하기   (저장 없음)\")), phr_(\"처음부터\"), phr_(\"시스템 디스크만 부팅\") };");
rep(\$u, "std::vector<bool> dm = { !hs || blk, blk }; items(c, bx + 30, by + 60, 340, a, startsel, 26, &dm);", "std::vector<bool> dm = { !hs || blk, blk, blk }; items(c, bx + 30, by + 60, 340, a, startsel, 26, &dm);");
rep(\$u, "text(c, bx + 24, by + 126, fit(phr_(\"실행 불가: \") + blk_cur(), 352), OR, K);", "text(c, bx + 24, by + 154, fit(phr_(\"실행 불가: \") + blk_cur(), 352), OR, K);");
# 항목 수
rep(\$u, "case S_START: return 2;", "case S_START: return 3;");
rep(\$u, "case S_SCREEN: return 9; case S_CONF: return 14;", "case S_SCREEN: return 10; case S_CONF: return 14;");
# 시작 진입점: 메인메뉴 / 저장 화면(막힘)
rep(\$u, "if (to[mainsel] == S_START) { startsel = has_save(game()) ? 0 : 1; }", "if (to[mainsel] == S_START) { startsel = has_save(game()) ? 0 : 1; start_ret = S_MAIN; }");
rep(\$u, "{ startsel = 1; go(S_START, 1); break; }", "{ startsel = 1; start_ret = S_MAIN; go(S_START, 1); break; }");
# 시작 확정 함수(팝업 ○ 와 작품 목록 ○ 가 같이 쓴다)
rep(\$u, "    void open_vals() {", <<'CPP' . "    void open_vals() {");
    void start_ok(Act & a) {   /* t52i (B1): 0 이어하기 · 1 처음부터 · 2 시스템 디스크만 부팅(= 부가 메뉴의 같은 항목, 호스트 A_EXTRA arg 1) */
        if (startsel == 0 && !has_save(game())) return;
        if (game() && (game()->id.empty() || !blk_cur().empty())) return;
        if (startsel >= 1 && game() && game()->need_install) { begin_install(cur); return; }
        if (startsel == 2) { a.t = A_EXTRA; a.arg = 1; a.game = cur; return; }
        a.t = A_START; a.game = cur; a.entry = sel_entry; a.resume = (startsel == 0);
    }
CPP
rep(\$u, "            if (k == UK_OK) { if (startsel == 0 && !has_save(game())) break; if (game() && (game()->id.empty() || !blk_cur().empty())) break; if (startsel == 1 && game() && game()->need_install) { begin_install(cur); break; } a.t = A_START; a.game = cur; a.entry = sel_entry; a.resume = (startsel == 0); }\n            else if (k == UK_CANCEL) go(S_MAIN, 0);",
  "            if (k == UK_OK) start_ok(a);\n            else if (k == UK_CANCEL) go(start_ret, 0);");
# 작품 목록 ○ = 시작(저장 있으면 팝업, 없으면 바로 처음부터)
rep(\$u, "            if (k == UK_OK) { sync_cur(); mainsel = 0; go(S_MAIN, 0); }\n            else if (k == UK_CANCEL) go(S_MAIN, 0);\n            else if (k == UK_SELECT) { sync_cur(); go(S_EXTRA, 0); back = S_PICK; }",
  "            if (k == UK_OK) { sync_cur(); if (!game() || game()->id.empty()) { mainsel = 0; go(S_MAIN, 0); } else { start_ret = S_PICK; if (has_save(game()) || !blk_cur().empty()) { startsel = has_save(game()) ? 0 : 1; go(S_START, startsel); } else { startsel = 1; start_ok(a); } } }   /* t52i (B1): 홈 = 작품 목록, ○ = 시작 */\n            else if (k == UK_CANCEL) go(S_MAIN, 0);\n            else if (k == UK_SELECT) { sync_cur(); go(S_EXTRA, 0); back = S_PICK; }");
# 화면 설정: 전체화면 줄(10줄, 줄 간격 18)
rep(\$u, "phr_(\"곡면\"), phr_(\"창 배율\") };\n        items(c, 40, 44, 256, n, sel, 20);", "phr_(\"곡면\"), phr_(\"창 배율\"), phr_(\"전체화면\") };\n        items(c, 40, 44, 256, n, sel, 18);");
rep(\$u, "        const int PY = 20;\n        const char * sc[]", "        const int PY = 18;\n        const char * sc[]");
rep(\$u, "choicesd(c, 344, 44 + 8 * PY, wm, 6, wmult ? wmult - 1 : 5, WH, dmk); }", "choicesd(c, 344, 44 + 8 * PY, wm, 6, wmult ? wmult - 1 : 5, WH, dmk); }\n        { const char * fsc[] = { phr_(\"창 모드\"), phr_(\"전체화면\") }; choices(c, 344, 44 + 9 * PY, fsc, 2, fs_now ? 1 : 0, WH, row(9)); }   /* t52i (B3) */");
rep(\$u, "desc(c, 324, 44, { fit(sel < 8 ? std::string(dd[sel]) : wd, 560) });", "desc(c, 324, 44, { fit(sel < 8 ? std::string(dd[sel]) : sel == 9 ? std::string(phr_(\"전체화면: 창과 전체화면을 오갑니다. Alt+Enter 로도 됩니다.\")) : wd, 560) });");
rep(\$u, "bool scr_s = scr == S_SCREEN; int nrow = scr_s ? 9 : 14;", "bool scr_s = scr == S_SCREEN; int nrow = scr_s ? 10 : 14;\n            if (scr_s && sel == 9 && (k == UK_LEFT || k == UK_RIGHT || k == UK_OK)) { a.t = A_FULLSCR; return a; }   /* t52i (B3): 전체화면 줄 — 좌우·○ 로 바로 오간다 */");
# 동작 환경 ×: 되돌리지 않고 뒤로(바뀐 값은 이미 바로 저장 대상)
rep(\$u, "if (!scr_s && conf_restore()) { a.t = A_APPLY; dirty = true; }", "if (!scr_s) { cb_has = false; a.t = A_APPLY; dirty = true; }   /* t52i: × = 뒤로. 값은 바뀐 순간부터 저장 대상 */");
repn(\$u, "phr_(\"↑↓:선택, ←→:값, ○:결정, ×:취소, △:출하 시 설정\")", "phr_(\"↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정\")", 2);
# 조작 안내: 한 장 표
{
  my $a = index($u, "    void d_help(Canvas & c) {"); die "d_help" if $a < 0;
  my $b = index($u, "    void d_keys(Canvas & c) {", $a); die "d_keys" if $b < 0;
  my $new = <<'CPP';
    void d_help(Canvas & c) {   /* t52i (B2·B3): 조작 안내 = 한 장 표. 배속은 「입력 → 동작」 표 */
        titlebar(c, phr_("조작 안내"));
        rbox(c, 16, 24, 608, 344, WH, phr_("조작 안내"));
        static const char * L[] = {
            phr_("패드    ○:결정(스페이스)  ×:취소(Esc)  □:빨리 넘김(CAPS)"),
            phr_("        △:메뉴(F12)  START:Enter  십자키/왼스틱:방향키"),
            phr_("배속    입력 → 동작"),
            phr_("        누르는 동안   R2 / F11"),
            phr_("        켜고 끄기     L3 / ScrollLock / Alt+F11"),
            phr_("        빨리 넘김     □ / CAPS  (글 넘기기)"),
            phr_("화면    Alt+Enter:전체화면  Alt+1/2/3:비율  Alt+S:셰이더"),
            phr_("        Alt+위/아래:곡면 세기  Alt+[ ]:창 배율"),
            phr_("저장    메뉴의 저장/불러오기, Alt+F5 / Alt+F9 는 슬롯 1"),
            phr_("키보드  그 밖의 키는 PC-98 키로 그대로 전달"),
            phr_("        F10 / 숫자 1-7 / KANA 는 메뉴의 「PC-98 키 보내기」로만 보냄") };
        for (int i = 0; i < 11; i++) text(c, 40, 46 + i * 28, L[i], L[i][0] != ' ' ? CY : WH, K);
        helpline(c, phr_("○ 또는 ×:돌아가기"));
    }
CPP
  substr($u, $a, $b - $a) = $new;
}
spit($uf, $u);

# ================= host.cpp =================
my $h = slurp($hf);
rep(\$h, "static void gh_win_apply(int m, bool osd) {", <<'CPP' . "static void gh_win_apply(int m, bool osd) {");
static void gh_fs_toggle(void) {   /* t52i (B3): 화면 설정 「전체화면」 줄 · Alt+Enter 와 같은 동작 */
    if (!g_win) return;
    bool fs = (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); SDL_ShowCursor(fs ? SDL_ENABLE : SDL_DISABLE);
    gh_log("SCREEN fullscreen=%d (menu)", fs ? 0 : 1); gh_osd(fs ? phr_("창 모드") : phr_("전체화면"));
}
static bool gh_fs_now(void) { return g_win && (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP; }
CPP
repn(\$h, "case ui::A_WINMULT: gh_win_apply(a.arg, false); break;", "case ui::A_WINMULT: gh_win_apply(a.arg, false); break;\n            case ui::A_FULLSCR: gh_fs_toggle(); break;", 2);
rep(\$h, q~if (tk == "save") {~, q~if (tk == "home") { U.in_game = false; U.scr = ui::S_MAIN; U.mainsel = 0; U.sel = 0; continue; }   /* t52i 시험 전용: 메인메뉴로 돌아가 맨 위 */
            if (tk == "startpick") { U.start_ret = ui::S_PICK; U.startsel = 1; U.scr = ui::S_START; U.sel = 1; continue; }   /* t52i 시험 전용: 작품 목록에서 연 시작 팝업 */
            if (tk == "valdump") { gh_log("uikeys: T52I VALDUMP bgm=%d se=%d bmult=%d bmode=%d dirty=%d scr=%d", U.g.bgm, U.g.se, U.g.bmult, U.g.bmode, U.dirty ? 1 : 0, (int)U.scr); continue; }   /* t52i 시험 전용: 동작 환경 값 기록 */
            if (tk == "save") {~);
repn(\$h, "U.ts_ms = (int)SDL_GetTicks();", "U.fs_now = gh_fs_now(); U.ts_ms = (int)SDL_GetTicks();", 2);
spit($hf, $h);

# ================= ov.h: (변경 없음) =================
# ================= kh_version.h =================
my $v = slurp($vf);
$v =~ s/#define KH_VER_BUILD \d+/#define KH_VER_BUILD 60/ or die "ver build"; $v =~ s/#define KH_BUILD "[^"]*"/#define KH_BUILD "t52i"/ or die "ver bld"; $v =~ s/#define KH_VERSION "[^"]*"/#define KH_VERSION "0.5.0-t52i"/ or die "ver str";
spit($vf, $v);
print "patch t52i ok\n";
