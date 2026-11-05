/* app/uitest.h -- S9b: menu shot, ui main loop, ui test. Moved unchanged from gensei_host.cpp lines 1514-1794 (as of t54t). Included in place; single translation unit. */
/* ---- r012: 작품을 고르기 전 UI 루프. true = 게임 시작(out_id), false = 끝내기 */
static bool gh_shot_to(const std::string & path) {
    snprintf(gApp.shot_path, sizeof gApp.shot_path, "%s", path.c_str()); gApp.shot_w = gApp.shot_h = 0; gApp.shot_crop = 0; gApp.shot_req = 1;
    for (int t = 0; t < 600 && gApp.shot_req; t++) SDL_Delay(5);
    bool done = !gApp.shot_req; gApp.shot_req = 0; gh_log("MENUSHOT %s %s %dx%d fit=%d", done ? "OK" : "FAIL", path.c_str(), gApp.shot_w, gApp.shot_h, (int)gApp.fit); return done;
}
static bool gh_menushot_tick(ui::Ui & U) {   /* true = 시험 끝(메뉴를 닫는다) */
    static unsigned t0 = 0; if (!t0) t0 = SDL_GetTicks(); unsigned t = SDL_GetTicks() - t0; static int st = 0;
    std::string dir = gApp.outdir.empty() ? gApp.exedir : gApp.outdir;
    if (st == 0 && t >= 2500) { st = 1; gh_log("T52F menu main scr=%d cur=%d game_ratio=%d fit=%d menu43=%d", (int)U.scr, U.cur, U.effective(U.cur).ratio, (int)gApp.fit, (int)gApp.menu43); gh_shot_to(dir + "\\menu_main.png"); }
    else if (st == 1 && t >= 3500) { st = 2; U.mainsel = 1; U.key(ui::UK_OK); }
    else if (st == 2 && t >= 5000) { st = 3; gh_log("T52F menu list scr=%d cur=%d game_ratio=%d fit=%d menu43=%d", (int)U.scr, U.cur, U.effective(U.cur).ratio, (int)gApp.fit, (int)gApp.menu43); gh_shot_to(dir + "\\menu_list.png"); }
    else if (st == 3 && t >= 6000) return true;
    return false;
}
static bool gh_ui_main_(std::string & out_id) {
    ui::Ui & U = gApp.ui_;
    for (size_t i = 0; i < U.games.size(); i++) gh_refresh_game_slots((int)i);
    U.in_game = false; U.scr = ui::S_MAIN; SDL_StartTextInput(); U.now_s = (long long)time(NULL);
    {   /* t39: 설치 실행 뒤 처음 열린 목록에서 「설치를 마쳤습니까?」 */
        std::string pf = gApp.exedir + "\\install_pending.tsv"; FILE * f = fopen(pf.c_str(), "rb");
        if (f) { char ln[512]; std::string line; if (fgets(ln, sizeof ln, f)) line = ln; fclose(f); plat::remove_file(pf.c_str());
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back(); size_t tb = line.find('\t');
            if (tb != std::string::npos) { std::string gid = line.substr(0, tb), en = line.substr(tb + 1); for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { U.ask_install_done((int)i, en); break; } } }
    }
    if (U.cur < 0 || U.cur >= (int)U.games.size()) U.cur = 0;
    U.pick = U.cur; gApp.cur_game = U.cur;
    gh_apply_settings(U.effective(U.cur), false);
    if (!gApp.noboot && U.g.boot == 0) { if (gh_boot_seq(true) && gApp.quit) return false; }
    static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 };
#if defined(__ANDROID__)
    if (U.games.empty() && U.gamefolder.empty() && !gApp.headless) gh_add_action(2);   /* t55b: first run, no works registered -> pick the game folder */
#endif
    gh_diag_enter(0);
    for (;;) {
        gh_inj_poll_menu();   /* t49a: 하니스 inject 를 메뉴 루프에서도 */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (gh_evpre(e, 0)) continue;
            if (e.type == SDL_QUIT) return false;
            if (e.type == SDL_TEXTINPUT) { U.text_in(e.text.text); continue; }
            if (e.type == SDL_TEXTEDITING) { U.ime_edit(e.edit.text); continue; }   /* t48a: IME 조합 중 글자 */
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && U.scr == ui::S_OSK && U.osk_hg) {   /* t48b: 마우스·터치(터치는 SDL 이 마우스로 바꿔 준다) */
                int vx = 0, vy = 0; bool ok = gh_menu_win2virt(e.button.x, e.button.y, vx, vy); bool hit = ok && U.pointer(vx, vy); gh_log("HGPTR win=%d,%d virt=%d,%d hit=%d", e.button.x, e.button.y, vx, vy, hit ? 1 : 0); continue; }
            if ((e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) && e.cbutton.button == (kh_western ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B) && U.scr == ui::S_OSK && U.osk_hg) {   /* t48b (#629 2): ○ 는 뗄 때 입력, 0.5초 넘게 누르면 SHIFT(어깨 버튼이 없는 패드용) */
                if (e.type == SDL_CONTROLLERBUTTONDOWN) { gApp.ob_held = true; gApp.ob_long = false; gApp.ob_t0 = SDL_GetTicks(); }
                else { if (gApp.ob_held && !gApp.ob_long) U.key(ui::UK_OK); gApp.ob_held = false; }
                continue; }
            if (gh_wm_hotkey(e)) continue;
#if defined(__ANDROID__)
            if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP) { gh_touch_feed(e); continue; }   /* t55a: menu touch */
#endif
            ui::UKey k = gh_ev2uk(e, NULL); if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) gh_log("DIAG btn -> ev2uk=%d none=%d", (int)k, k == ui::UK_NONE ? 1 : 0); if (k == ui::UK_NONE) continue;
            ui::Act a = U.key(k);
            switch (a.t) {
            case ui::A_START: if (U.games[a.game].id.empty()) break; if (!gh_font_guard()) break; if (!gh_disk_guard(U.games[a.game].id)) break; gh_launch_note(U, a); U.cur = a.game; gApp.cur_game = a.game; gApp.resume_slot = a.resume && U.games[a.game].lastslot >= 0 ? U.games[a.game].lastslot + 1 : 0; gh_chain_start(U, a.game); out_id = U.games[a.game].id; ui::settings_save(U, gApp.cfgpath); return true;
            case ui::A_LOAD: if (U.games[a.game].id.empty() || !U.blk_id(U.games[a.game].id).empty()) break; if (!gh_font_guard()) break; if (!gh_disk_guard(U.games[a.game].id)) break; gh_launch_note(U, a); U.cur = gApp.cur_game = a.game; gApp.resume_slot = a.slot + 1; out_id = U.games[a.game].id; ui::settings_save(U, gApp.cfgpath); return true;
            case ui::A_DELSLOT: gh_delete_slot(a.game, a.slot); break;
            case ui::A_APPLY: gh_apply_settings(U.scr == ui::S_SCREEN ? U.eff() : U.effective(U.cur), false); kh_western = (U.scr == ui::S_PAD || U.scr == ui::S_PADKEY ? U.eff() : U.effective(U.cur)).pad == 1; break;   /* t48e: 시작 메뉴의 결정/취소 버튼도 방식을 따른다 */
            case ui::A_WINMULT: gh_win_apply(a.arg, false); break;
            case ui::A_FULLSCR: gh_fs_toggle(); break;
            case ui::A_ADD: gh_add_action(a.arg); break;
            case ui::A_SETLANG: gh_setlang(a.game, a.arg); break;
            case ui::A_SETTITLE: gh_settitle(a.game, a.entry); break;
            case ui::A_TEST: gh_spawn_test(U.games[a.game].id, a.entry, a.arg); break;
            case ui::A_EXTRA:
                if (a.arg == 1 && (!gh_font_guard() || !gh_disk_guard(U.games[a.game].id))) break;
                if (a.arg == 1) gh_launch_note(U, a);
                if (a.arg == 1) { gApp.noB = true; gApp.resume_slot = 0; U.cur = gApp.cur_game = a.game; out_id = U.games[a.game].id; ui::settings_save(U, gApp.cfgpath); return true; }
                if (a.arg == 2) { gh_boot_seq(true); }
                if (a.arg == 6) { std::string rid = U.games[a.game].id, miss; if (!gh_orig_present(rid, miss)) { gApp.note = std::string(phr_("원본 이미지가 없습니다: ")) + aw::acp_to_u8(miss); gApp.note_until = SDL_GetTicks() + 6000u; break; }   /* t601: 원본이 없으면 초기화 막음(사본 보존) */
                    int nn = gh_reset_disks(rid); gApp.note = nn > 0 ? phr_("원본 디스크로 초기화했습니다 (사본 ") + std::to_string(nn) + phr_("개 삭제)") : phr_("초기화할 디스크 사본이 없습니다 (원본 그대로)"); gApp.note_until = SDL_GetTicks() + 3000u; }
                break;
            case ui::A_QUIT: if (a.arg) ui::settings_save(U, gApp.cfgpath); return false;
            default: break; }
        }
        if (!(U.scr == ui::S_OSK && U.osk_hg)) gApp.ob_held = false;
        if (gApp.ob_held && !gApp.ob_long && SDL_GetTicks() - gApp.ob_t0 >= 500) { gApp.ob_long = true; U.key(ui::UK_L); gh_log("HGSHIFT ○ 길게 → SHIFT"); }   /* t48b */
        { static int prev_scr = -1; if (prev_scr == (int)ui::S_OSK && (int)U.scr != (int)ui::S_OSK) gh_log("KBSENT total=%u in_text=%u (이 메뉴 프로세스의 kbov 가 코어로 보낸 키 수 / 그중 텍스트 입력 중)", gApp.kbsent_total, gApp.kbsent_text); prev_scr = (int)U.scr; }
        gh_wm_poll();
        if (U.scr != ui::S_SCREEN) gh_apply_settings(U.effective(U.cur), false);
        gApp.cur_game = U.cur;
        if (U.lang_chg) { U.lang_chg = false; gh_reload_games(); gh_log("T52G 언어 바꿈 -> lang=%d, 목록 다시 만듦", U.lang); }   /* t52g (#133) */
        if (gApp.menushot && !gApp.headless && gApp.win && gh_menushot_tick(U)) return false;   /* t52f 시험 */
        U.fs_now = gh_fs_now(); U.ts_ms = (int)SDL_GetTicks(); U.now_s = (long long)time(NULL); U.draw(c); if (gApp.note_until && SDL_GetTicks() < gApp.note_until) ov::osd_draw(c, gApp.note, false, 336); gh_publish(buf, 640, 400); SDL_Delay(16);
    }
}

/* t52f (#132 2항): 시작 메뉴를 도는 동안은 작품별 화면비와 상관없이 4:3. 메뉴를 나가면(작품 시작) 고른 작품의 화면비를 적용한다 */
static bool gh_ui_main(std::string & out_id) {
    gApp.menu43 = true; gh_log("T52F menu43 on (시작 메뉴 화면비 4:3 고정)");
    bool r = gh_ui_main_(out_id);
    gApp.menu43 = false; if (!gApp.headless) gh_apply_settings(gApp.ui_.effective(gApp.cur_game), false);
    gh_log("T52F menu43 off -> fit=%d (고른 작품 %s, 작품 화면비 %d)", (int)gApp.fit, out_id.c_str(), (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) ? gApp.ui_.effective(gApp.cur_game).ratio : -1);
    return r;
}

/* ---- r012: 창 없이 UI 를 호스트 코드 그대로 그려 PNG 로 (--uitest 폴더). 화면별 캡처 + 부팅 장면 + 설정 저장/읽기 왕복 */
static void gh_uitest(void) {
    ui::Ui & U = gApp.ui_; static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; int n = 0;
    plat::mkdir(gApp.uitest_dir.c_str());
    for (size_t i = 0; i < U.games.size(); i++) gh_refresh_game_slots((int)i);
    U.cur = U.pick = 2; gApp.cur_game = 2;
    auto shot = [&](const char * nm) { U.draw(c); { std::string bw = gh_builtin_warn(); if (!bw.empty()) ov::osd_draw(c, bw, false, 336); } char p[700]; if (!gApp.uikeys_file.empty()) snprintf(p, sizeof p, "%s\\ui_%02d_%s_s%02dt%dc%d.png", gApp.uitest_dir.c_str(), ++n, nm, (int)U.scr, (int)U.tab, U.sel); else snprintf(p, sizeof p, "%s\\ui_%02d_%s.png", gApp.uitest_dir.c_str(), ++n, nm);   /* t39ui4: 스크립트 캡처 이름에 화면 번호·탭·선택 줄을 붙인다 */ gh_png(p, buf, 640, 400); };
    auto boot = [&](const char * nm, int stage, bool cells) {
        c.rect(0, 0, 640, 400, ov::K);
        if (stage >= 1) ov::text(c, 40, 112, std::string(KH_FULLNAME), ov::WH, ov::K);
        if (stage >= 2) ov::text(c, 40, 144, std::string("Now Loading") + std::string((size_t)(stage - 1 > 3 ? 3 : stage - 1), '.'), ov::WH, ov::K);
        if (cells) ov::fncells_boot(c);   /* t52a (#49) */
        char p[700]; snprintf(p, sizeof p, "%s\\ui_%02d_%s.png", gApp.uitest_dir.c_str(), ++n, nm); gh_png(p, buf, 640, 400);
    };
    if (!gApp.uikeys_file.empty()) {   /* t39: 키 입력 스크립트 시험 — 공백 구분 토큰, # 줄은 주석. shot:이름 text:글자 go:작품id 키(U D L R O C T S E LB RB)*반복 */
        std::string sc; { FILE * kf = fopen(gApp.uikeys_file.c_str(), "rb"); if (kf) { char ln[1024]; while (fgets(ln, sizeof ln, kf)) { if (ln[0] != '#') sc += ln; } fclose(kf); } else gh_log("uikeys: 파일을 열 수 없음 %s", gApp.uikeys_file.c_str()); }
        static const std::map<std::string, ui::UKey> KM = { { "U", ui::UK_UP }, { "D", ui::UK_DOWN }, { "L", ui::UK_LEFT }, { "R", ui::UK_RIGHT }, { "O", ui::UK_OK }, { "C", ui::UK_CANCEL }, { "T", ui::UK_TRI }, { "S", ui::UK_SQ }, { "E", ui::UK_SELECT }, { "LB", ui::UK_L }, { "RB", ui::UK_R } };
        U.in_game = false; U.scr = ui::S_MAIN; U.ts_ms = 0; U.now_s = (long long)time(NULL); if (U.cur < 0 || U.cur >= (int)U.games.size()) U.cur = 0; U.pick = U.cur; gApp.cur_game = U.cur;
        std::istringstream in(sc); std::string tk; int nact = 0;
        while (in >> tk) {
            if (tk.compare(0, 5, "shot:") == 0) { shot(tk.substr(5).c_str()); continue; }
            if (tk.compare(0, 5, "text:") == 0) { U.text_in(tk.substr(5)); continue; }
            if (tk.compare(0, 5, "vals:") == 0) {   /* t48a (#614 ③): 전 작품 적용값 표를 TSV 로 저장(시험 전용) */
                std::string vp = gApp.uitest_dir + "\\" + tk.substr(5); FILE * vf = fopen(vp.c_str(), "wb"); int nr = 0;
                if (vf) { for (size_t i = 0; i < U.games.size(); i++) { std::vector<ui::ValRow> rows; if (!U.env.values || !U.env.values((int)i, rows)) continue; for (auto & r : rows) { fprintf(vf, "%s\t%s\t%s\t%s\t%s\n", U.games[i].id.c_str(), r.key.c_str(), r.val.c_str(), r.src.c_str(), r.cov.c_str()); nr++; } } fclose(vf); }
                gh_log("uikeys: 적용값 표 %d줄 저장 %s", nr, vp.c_str()); continue; }
            if (tk == "open_vals") { U.open_vals(); continue; }
            if (tk.compare(0, 4, "dev:") == 0) { ov::g_helpdev = (tk.substr(4) == "kbd") ? 1 : 0; continue; }   /* t49a 시험 전용: 조작 설명 표기 장치 지정 (pad / kbd) */
            if (tk.compare(0, 11, "stagelabel:") == 0) { std::string sl = tk.substr(11); for (auto & ch3 : sl) if (ch3 == '_') ch3 = ' '; U.stage_next_label = sl; continue; }   /* t49a 시험 전용: 일시정지 메뉴의 「다음 단계」 줄 켜기 (_ = 공백) */
            if (tk.compare(0, 3, "hk:") == 0) { U.hk_script(tk.substr(3)); continue; }   /* t48b 시험 전용: 한글 키보드 키 누르기 열 */
            if (tk.compare(0, 4, "tap:") == 0) { int vx = 0, vy = 0; sscanf(tk.c_str() + 4, "%d,%d", &vx, &vy); bool hit = U.pointer(vx, vy); gh_log("uikeys: tap %d,%d hit=%d", vx, vy, hit ? 1 : 0); continue; }
            if (tk == "hkdump") { gh_log("uikeys: HKDUMP buf='%s' cur='%s' han=%d shift=%d sel=%d,%d osk_hg=%d scr=%d", U.osk_buf.c_str(), hg::cur_text(U.hc.cur).c_str(), U.hc.han ? 1 : 0, U.osk_shift ? 1 : 0, U.osk_r, U.osk_c, U.osk_hg ? 1 : 0, (int)U.scr); continue; }
            if (tk.compare(0, 8, "newname:") == 0) { std::string gid = tk.substr(8); for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { ent::Kv kv; for (auto & ee : U.games[i].ents) if (!ee.pseudo && ee.vgi < 0) { kv = ee.kv; break; } U.cur = (int)i; U.entg = (int)i; U.begin_new_from_kv((int)i, kv, "새 방법"); break; } continue; }   /* t48b 시험 전용: 새 실행 방법 편집 화면으로 */
            if (tk.compare(0, 8, "entdump:") == 0) { std::string gid = tk.substr(8); for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { for (auto & ee : U.games[i].ents) { std::string hx; for (unsigned char ch2 : ee.name) { char b2[4]; snprintf(b2, sizeof b2, "%02X", ch2); hx += b2; } gh_log("uikeys: ENTS %s name='%s' pseudo=%d hex=%s", gid.c_str(), ee.name.c_str(), ee.pseudo ? 1 : 0, hx.c_str()); } } continue; }
            if (tk == "kbsent") { gh_log("uikeys: KBSENT total=%u in_text=%u", gApp.kbsent_total, gApp.kbsent_text); continue; }
            if (tk.compare(0, 11, "launchnote:") == 0) {   /* t48a 시험 전용: 실행 요청이 항목 이름(전역)을 남기는 것을 흉내 */
                std::string r = tk.substr(11); size_t c2 = r.find(':'); std::string gid = r.substr(0, c2), en = c2 == std::string::npos ? std::string() : r.substr(c2 + 1);
                prof::g_entry_req = en; for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) { U.cur = (int)i; gApp.cur_game = (int)i; }
                gh_log("uikeys: 실행 요청 흉내 id=%s entry=%s", gid.c_str(), en.c_str()); continue; }
            if (tk == "reload") { gh_reload_games(); gh_log("uikeys: 목록 다시 만듦 (entry_req='%s')", prof::g_entry_req.c_str()); continue; }
            if (tk == "blkdump") { if (U.pick >= 0 && U.pick < (int)U.vis.size()) { int gi = U.vis[U.pick]; gh_log("uikeys: BLKDUMP row=%s row_old_games_gi_block='%s' row_by_id_block='%s' cur=%s cur_by_id_block='%s' cur_games_cur_block='%s' sel_entry='%s' scr=%d", U.games[gi].id.c_str(), U.games[gi].block.c_str(), U.blk_row(gi).c_str(), U.game() ? U.game()->id.c_str() : "", U.blk_cur().c_str(), U.game() ? U.game()->block.c_str() : "", U.sel_entry.c_str(), (int)U.scr); } continue; }
if (tk == "blocks") { for (auto & gg : U.games) gh_log("uikeys: BLOCKS %s block='%s'", gg.id.c_str(), gg.block.c_str()); continue; }
            if (tk == "home") { U.in_game = false; U.scr = ui::S_MAIN; U.mainsel = 0; U.sel = 0; continue; }   /* t52i 시험 전용: 메인메뉴로 돌아가 맨 위 */
            if (tk == "startpick") { U.start_ret = ui::S_PICK; U.startsel = 1; U.scr = ui::S_START; U.sel = 1; continue; }   /* t52i 시험 전용: 작품 목록에서 연 시작 팝업 */
            if (tk == "valdump") { gh_log("uikeys: T52I VALDUMP bgm=%d se=%d bmult=%d bmode=%d dirty=%d scr=%d", U.g.bgm, U.g.se, U.g.bmult, U.g.bmode, U.dirty ? 1 : 0, (int)U.scr); continue; }   /* t52i 시험 전용: 동작 환경 값 기록 */
            if (tk.compare(0, 8, "fpsdump:") == 0) {   /* FPS test only: value per scope */
                const int ci = U.cur; const bool ok = ci >= 0 && ci < (int)ui::Ui::MAXG;
                gh_log("uikeys: FPSDUMP %s scr=%d scope=%d sel=%d g=%d t=%d has_t=%d nofollow=%d eff=%d effective=%d all_global=%d", tk.c_str() + 8, (int)U.scr, U.scope, U.sel, U.g.fpsd, ok ? U.t[ci].fpsd : -1, ok && U.has_t[ci] ? 1 : 0, ok && U.nofollow[ci] ? 1 : 0, U.eff().fpsd, U.effective(ci).fpsd, U.all_global ? 1 : 0); continue; }
            if (tk.compare(0, 7, "saveas:") == 0) { std::string sp = gApp.uitest_dir + "\\" + tk.substr(7); ui::settings_save(U, sp); gh_log("uikeys: saved %s", sp.c_str()); continue; }
            if (tk.compare(0, 7, "loadas:") == 0) {   /* clear like a fresh start, then read that file */
                std::string sp = gApp.uitest_dir + "\\" + tk.substr(7); U.g = ui::Settings(); U.all_global = false; for (int gg = 0; gg < (int)ui::Ui::MAXG; gg++) { U.has_t[gg] = false; U.nofollow[gg] = false; }
                ui::settings_load(U, sp); gh_log("uikeys: loaded %s", sp.c_str()); continue; }
            if (tk.compare(0, 8, "cfgdump:") == 0) {   /* test only: individual-line mask, global vs effective */
                const int ci = U.cur; const bool ok = ci >= 0 && ci < (int)ui::Ui::MAXG; const ui::Settings ef = U.effective(ci);
                gh_log("uikeys: CFGDUMP %s scr=%d scope=%d ovm=%x g[ratio=%d scan=%d bgm=%d se=%d bmult=%d fps=%d] eff[ratio=%d scan=%d bgm=%d se=%d bmult=%d fps=%d]", tk.c_str() + 8, (int)U.scr, U.scope, ok ? U.ovm[ci] : 0u, U.g.ratio, U.g.scan, U.g.bgm, U.g.se, U.g.bmult, U.g.fpsd, ef.ratio, ef.scan, ef.bgm, ef.se, ef.bmult, ef.fpsd); continue; }
            if (tk.compare(0, 8, "predump:") == 0) {   /* 빌드 6 test only: presets + effective 7 values */
                const ui::Settings pe = U.eff(); const int pci = U.cur; std::string pl; for (size_t q = 0; q < U.presets.size(); q++) { const ui::Settings & ps = U.presets[q].st; char pb[160]; snprintf(pb, sizeof pb, "|%d,%d,%d,%d,%d,%d,%d", ps.ratio, ps.shader, ps.curve, ps.bleed, ps.glow, ps.scan, ps.mask); pl += "[" + U.presets[q].name + pb + "]"; }
                gh_log("uikeys: PREDUMP %s scr=%d scope=%d np=%d pre_i=%d pos=%d ovm=%x eff[%d,%d,%d,%d,%d,%d,%d] presets=%s", tk.c_str() + 8, (int)U.scr, U.scope, (int)U.presets.size(), U.pre_i, U.pre_pos(), (pci >= 0 && pci < (int)ui::Ui::MAXG) ? U.ovm[pci] : 0u, pe.ratio, pe.shader, pe.curve, pe.bleed, pe.glow, pe.scan, pe.mask, pl.c_str()); continue; }
            if (tk == "save") { std::string sp = gApp.uitest_dir + "\\settings_uikeys.ini"; ui::settings_save(U, sp); gh_log("uikeys: 설정 저장 %s (bleed=%d)", sp.c_str(), U.g.bleed); continue; }   /* t42: 저장 시험 */
            if (tk == "ingame") { U.in_game = true; U.scr = ui::S_GAME; U.sel = 0; continue; }   /* t39ui4: 게임 중 메뉴로 진입 */
            if (tk.compare(0, 4, "tab:") == 0) { std::string t = tk.substr(4); int want = t == "ko" ? 0 : t == "jp" ? 1 : t == "un" ? 2 : 3; for (int gg = 0; gg < 5 && (int)U.tab != want; gg++) U.key(ui::UK_R); continue; }   /* t39ui4: 탭 맞추기 */
            if (tk.compare(0, 3, "go:") == 0) { for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == tk.substr(3)) { int b = U.base_of((int)i); if (!U.row_in_tab(b)) U.tab = ui::Ui::TAB_ALL; U.rebuild_vis((int)i); } continue; }
            if (tk.compare(0, 4, "sel:") == 0) { U.sel = atoi(tk.c_str() + 4); continue; }   /* t44c: 시험 전용 — 선택 행 직접 지정 */
            if (tk == "entlang") { U.sel = (int)std::max<size_t>(U.vents(U.entg).size(), 1) + 1; continue; }   /* t44c: S_ENT 의 「언어」 행 */
            if (tk.compare(0, 7, "addnew:") == 0) { U.add_new.push_back(tk.substr(7)); U.add_new_why.push_back("시험: 자동 판정 근거 줄"); continue; }   /* t44c: 방금 추가한 작품 목록 */
            if (tk == "addscr") { U.scr = ui::S_ADD; U.sel = 0; continue; }
            if (tk.compare(0, 4, "cur:") == 0) { for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == tk.substr(4)) { U.cur = (int)i; U.entg = (int)i; gApp.cur_game = (int)i; } continue; }   /* t48e2 시험 전용: 현재 작품 지정 */
            if (tk == "padscr") { U.scr = ui::S_PAD; U.sel = 0; continue; }
            if (tk == "hotscr") { U.scr = ui::S_HOT; U.sel = 0; continue; }   /* build 5a-2 test only: hotkey tab */
            if (tk.compare(0, 5, "kind:") == 0) { gensei_padkind_v = atoi(tk.c_str() + 5); continue; }   /* test only: pad kind (3 = Retroid) */
            if (tk.compare(0, 7, "hotcap:") == 0) { if (kh_cap_on) { kh_cap_pb = atoi(tk.c_str() + 7) & 31; kh_cap_sel = tk.back() == 's'; } else gh_log("uikeys: HOTCAP not waiting"); continue; }
            if (tk.compare(0, 6, "kbscr:") == 0) { U.open_padkey(atoi(tk.c_str() + 6)); U.kb_open(); continue; }   /* test only: key picker on the layout */
            if (tk.compare(0, 8, "padjump:") == 0) { U.pad_jump(atoi(tk.c_str() + 8)); continue; }   /* test only: pad press jump */
            if (tk == "kbstate") { std::string us; for (auto & kc : U.kb_cells) if (U.kb_used(kc.name)) us += kc.name + " "; const std::string nm = U.kb_sel >= 0 && U.kb_sel < (int)U.kb_cells.size() ? U.kb_cells[U.kb_sel].name : std::string("-");
                gh_log("uikeys: KBSTATE scr=%d sel=%d name=%s n=%d row=%d used[%s]", (int)U.scr, U.kb_sel, nm.c_str(), (int)U.kb_cells.size(), U.pk_row, us.c_str()); continue; }
            if (tk == "extscr") { U.scr = ui::S_EXT; U.sel = 0; U.ext_wait = false; continue; }   /* test only: extra-buttons tab */
            if (tk.compare(0, 6, "xbadd:") == 0) { int fs = -1; for (int xi = 0; xi < 8 && fs < 0; xi++) if (U.xb_get(xi).empty()) fs = xi; if (fs >= 0) U.gpad[std::string("btn_xreg") + std::to_string(fs)] = tk.substr(6); if (U.env.pad_cap) { int z = 0; bool zb = false; U.env.pad_cap(3, z, zb); } continue; }   /* test only: register an extra button */
            if (tk == "xbstate") { std::string xs, xr; for (int xi = 0; xi < 8; xi++) if (!U.xb_get(xi).empty()) xs += std::to_string(xi) + ":" + U.xb_get(xi) + "=" + U.pad_val(ui::Ui::PAD_N + xi) + " "; for (int xi = 0; xi < 8; xi++) xr += std::to_string(xi) + ":" + std::to_string(kh_xb[xi].kind) + "/" + std::to_string(kh_xb[xi].code) + " "; gh_log("uikeys: XBSTATE scr=%d sel=%d cnt=%d rows[%s] xb[%s]", (int)U.scr, U.sel, U.xb_cnt(), xs.c_str(), xr.c_str()); continue; }
            if (tk == "hotstate") {
                std::string gp, tb, rw; for (auto & kv : U.gpad) if (kv.first.compare(0, 8, "btn_hot_") == 0) gp += kv.first + "=" + kv.second + " ";
                for (int q = 1; q < FN_N; q++) tb += std::to_string(q) + ":" + std::to_string(kh_hot[q].btn) + (kh_hot[q].sel ? "s" : "") + " ";
                for (int k2 = 0; k2 < ui::Ui::HOT_N; k2++) { const int q = ui::Ui::hot_fn(k2); rw += std::string(ui::Ui::pad_fn_name(q)) + "=" + U.hot_text(U.hot_val(q)) + (U.hot_overlap(q) ? "(OVERLAP)" : "") + " "; }
                gh_log("uikeys: HOTSTATE scr=%d sel=%d cap=%d gpad[%s] table[%s] rows[%s]", (int)U.scr, U.sel, U.hot_cap ? 1 : 0, gp.c_str(), tb.c_str(), rw.c_str()); continue; }   /* t48e 시험 전용: 조작 설정 화면으로 */
            if (tk.compare(0, 4, "scr:") == 0) { U.scr = (ui::Scr)atoi(tk.c_str() + 4); U.sel = 0; continue; }   /* t50a 시험 전용 (#877): 화면 번호로 직접 이동 — 안 닿던 화면의 도움말 폭 실측용 */
            if (tk == "lang") { std::vector<std::string> ids, why; for (auto & gg : U.games) ids.push_back(gg.id); U.open_lang(ids, why, ui::S_MAIN); continue; }   /* t50a 시험 전용: 언어 화면 */
            if (tk.compare(0, 7, "padkey:") == 0) { U.open_padkey(atoi(tk.c_str() + 7)); continue; }   /* t50a 시험 전용: 패드 키 고르기 화면 */
            if (tk.compare(0, 4, "osk:") == 0) { U.osk_open(atoi(tk.c_str() + 4), "", ui::S_MAIN, false); continue; }   /* t50a 시험 전용: 화면 키보드 (0=이름 · 그 밖=DOS 명령) */
            if (tk.compare(0, 6, "scope:") == 0) { U.scope = atoi(tk.c_str() + 6); if (U.scope == 1 && U.cur >= 0 && U.cur < (int)U.games.size() && !U.has_t[U.cur]) { U.t[U.cur] = U.effective(U.cur); U.has_t[U.cur] = true; } continue; }
            if (tk == "padstate") {
                std::string gp; for (auto & kv : U.gpad) gp += kv.first + "=" + kv.second + " "; std::string wp; if (U.cur >= 0 && U.cur < (int)U.games.size()) { std::map<std::string, std::string> m; prof::get_pad_keys(U.games[U.cur].dir, m); for (auto & kv : m) wp += kv.first + "=" + kv.second + " "; }
                gh_log("uikeys: PADSTATE scr=%d sel=%d scope=%d g.pad=%d eff.pad=%d global[%s] work[%s]", (int)U.scr, U.sel, U.scope, U.g.pad, U.eff().pad, gp.c_str(), wp.c_str());
                if (U.cur >= 0 && U.cur < (int)U.games.size()) { for (int pi = 0; pi < ui::Ui::PAD_N; pi++) { bool fw = false, df = false; std::string v = U.pad_val(pi, &fw, &df); gh_log("uikeys: PADROW %d %s -> %s%s", pi, U.pad_row_name(pi).c_str(), v.c_str(), fw ? " (작품)" : df ? " (기본)" : " (전역)"); } }
                continue; }
            if (tk.compare(0, 8, "delslot:") == 0) {   /* S3 retro-test token */
                std::string r = tk.substr(8); size_t cp = r.find(':'); std::string gid = r.substr(0, cp); int sl = cp == std::string::npos ? 0 : atoi(r.c_str() + cp + 1); int gi = -1;
                for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == gid) gi = (int)i;
                if (gi < 0 || sl < 0 || sl > 7) { gh_log("uikeys: DELSLOT bad arg %s", r.c_str()); continue; }
                bool before = plat::exists(gh_slot_n(gi, sl)); gh_delete_slot(gi, sl); gh_log("uikeys: DELSLOT %s slot%02d before=%d after=%d listed_used=%d", gid.c_str(), sl + 1, before ? 1 : 0, plat::exists(gh_slot_n(gi, sl)) ? 1 : 0, U.games[gi].slot[sl].used ? 1 : 0); continue; }
            if (tk.compare(0, 11, "resetdisks:") == 0) { std::string gid = tk.substr(11); int nn = gh_reset_disks(gid); gh_log("uikeys: RESETDISKS %s deleted=%d", gid.c_str(), nn); continue; }   /* S3 retro-test token */
            if (tk.compare(0, 10, "spawntest:") == 0) { gh_spawn_test(tk.substr(10), "", 4); continue; }   /* S3 retro-test token */
            if (tk.compare(0, 6, "guard:") == 0) { gApp.note.clear(); gApp.note_until = 0; bool gok = gh_disk_guard(tk.substr(6)); gh_log("uikeys: GUARD id=%s ok=%d note='%s'", tk.substr(6).c_str(), gok ? 1 : 0, gApp.note.c_str()); continue; }   /* #451 test-only: disk preflight result */
            int rep = 1; size_t st = tk.find('*'); if (st != std::string::npos) { rep = atoi(tk.c_str() + st + 1); tk = tk.substr(0, st); }
            auto it = KM.find(tk); if (it == KM.end()) { gh_log("uikeys: 알 수 없는 토큰 %s", tk.c_str()); continue; }
            for (int i = 0; i < rep; i++) { ui::Act a = U.key(it->second); if (a.t != ui::A_NONE) { nact++; gh_log("uikeys: act=%d game=%d arg=%d entry=%s", (int)a.t, a.game, a.arg, a.entry.c_str()); if (a.t == ui::A_SETLANG) gh_setlang(a.game, a.arg); if (a.t == ui::A_SETTITLE) gh_settitle(a.game, a.entry); } }   /* t44c: A_SETLANG 는 호스트 처리기를 그대로 실행 */
        }
        gh_log("uikeys: done acts=%d shots=%d", nact, n); return;
    }
    boot("boot1", 1, false); boot("boot3", 3, false); boot("boot5", 4, true);
    if (U.games.size() >= 3) {   /* 캡처용 표본 정보(시험 전용 값) */
        ui::UiGame & k = U.games[2]; k.company = "컴파일"; k.year = "표본"; k.genre = "RPG"; k.patch = "한글 패치"; k.disknote = "FD 2장 + 오프닝"; k.devices = "86 보드 · 마우스 선택"; k.play_s = 5 * 3600 + 20 * 60; k.has_demo = true;
        k.fds = { "환세희담 시스템 디스크", "환세희담 데이터 디스크", "환세희담 오프닝 디스크" }; k.cur_fd[0] = k.fds[0]; k.cur_fd[1] = k.fds[1]; k.ov["shader"] = "1"; k.ov["boost"] = "3";
    }
    auto shotg = [&](const char * nm) { ov::sample_picture(c, 0, 0, 640, 400); shot(nm); };
    shot("main"); U.mainsel = 0; U.key(ui::UK_OK); shot("start"); U.key(ui::UK_CANCEL);
    U.mainsel = 1; U.key(ui::UK_OK); shot("pick"); U.key(ui::UK_DOWN); U.key(ui::UK_OK); shot("main_after_pick"); U.cur = U.pick = 2; gApp.cur_game = 2;
    U.mainsel = 2; U.key(ui::UK_OK); shot("screen_sharp"); U.key(ui::UK_DOWN); U.key(ui::UK_DOWN); U.key(ui::UK_RIGHT); shot("screen_crt"); U.key(ui::UK_DOWN); U.key(ui::UK_RIGHT); shot("screen_crt_curve"); U.key(ui::UK_CANCEL);
    U.mainsel = 3; U.key(ui::UK_OK); U.key(ui::UK_DOWN); U.key(ui::UK_LEFT); shot("conf"); for (int i = 0; i < 7; i++) U.key(ui::UK_DOWN); shot("conf_bosd"); U.key(ui::UK_OK);
    U.mainsel = 4; U.key(ui::UK_OK); shot("save"); U.key(ui::UK_CANCEL);
    U.mainsel = 5; U.key(ui::UK_OK); shot("extra"); U.sel = 5; U.key(ui::UK_OK); shot("about"); U.key(ui::UK_CANCEL); U.sel = 0; U.key(ui::UK_DOWN); U.key(ui::UK_DOWN); U.key(ui::UK_DOWN); U.key(ui::UK_OK); shot("help"); U.key(ui::UK_CANCEL); U.key(ui::UK_CANCEL);
    U.mainsel = 6; U.key(ui::UK_OK); shot("quit"); U.key(ui::UK_CANCEL);
    U.in_game = true; U.scr = ui::S_GAME; U.sel = 1; shotg("ingame"); U.scr = ui::S_KEYS; U.sel = 0; shotg("keys");
    U.scr = ui::S_SWAP; U.swapdrv = 0; shotg("swap"); U.scr = ui::S_SWAPI; U.swapsel = 2; shotg("swap_pick"); U.scr = ui::S_QUIT; U.quitsel = 0; shotg("ingame_quit");
    /* t52f (#132): 동작 환경 설정의 새 행 「오버레이 버튼 표시」 */
    { U.in_game = false; U.scr = ui::S_MAIN; U.scope = 0; U.mainsel = 3; U.key(ui::UK_OK); U.sel = 10; int o0 = U.g.ovbtn; U.key(ui::UK_LEFT); int o1 = U.g.ovbtn; shotg("conf_ovbtn_off"); U.key(ui::UK_RIGHT); int o2 = U.g.ovbtn; U.sel = 12; shotg("conf_row_jpfont"); ui::Act ra = U.key(ui::UK_CANCEL); int o3 = U.g.ovbtn; gh_log("T52F conf ovbtn start=%d left=%d right=%d after_cancel=%d rows=%d -> %s", o0, o1, o2, o3, 13, (o0 == 1 && o1 == 0 && o2 == 1 && o3 == 1) ? "PASS" : "FAIL"); (void)ra; U.scr = ui::S_MAIN; U.in_game = false; }
    /* t52g (#133): UI 언어 3종 — 언어마다 주요 화면을 찍고, 설정의 언어 행(←→ 순환)·문구 칸 즉시 교체·settings 저장/읽기를 검사 */
    { static const char * const lnm[3] = { "ko", "en", "ja" }; static const char * const want[3] = { "게임 시작", "Start game", "ゲーム スタート" };
      U.in_game = false; U.scope = 0; U.lang = 0; U.lang_chg = false; phr_set_lang(0); int bad = 0; char nm[48];
      for (int L = 0; L < 3; L++) {
          const char * g0 = phr_("게임 시작"); if (strcmp(g0, want[L]) != 0) { bad++; gh_log("T52G lang %d 게임 시작='%s' want='%s' FAIL", L, g0, want[L]); }
          U.scr = ui::S_MAIN; U.mainsel = 0; snprintf(nm, sizeof nm, "t52g_main_%s", lnm[L]); shotg(nm); gh_log("T52H 글꼴 lang=%d 사용자BMP=%d 동봉FONTX2=%d 일본어켜짐=%d", L, (int)(ov::g_jp_bmpd != NULL), (int)ov::g_jp_fx.ok, (int)ov::g_jp_on);   /* t52h */
          U.scr = ui::S_CONF; U.sel = 13; snprintf(nm, sizeof nm, "t52g_conf_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_SCREEN; U.sel = 0; snprintf(nm, sizeof nm, "t52g_screen_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_SAVE; U.sel = 0; snprintf(nm, sizeof nm, "t52g_save_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_EXTRA; U.sel = 0; snprintf(nm, sizeof nm, "t52g_extra_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_ADD; U.sel = 0; snprintf(nm, sizeof nm, "t52g_add_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_PICK; U.sel = 0; snprintf(nm, sizeof nm, "t52g_pick_%s", lnm[L]); shotg(nm);
          U.scr = ui::S_CONF; U.sel = 13; int l0 = U.lang; U.key(ui::UK_RIGHT); if (U.lang != (l0 + 1) % 3) { bad++; gh_log("T52G 언어 행 → 키: lang %d -> %d FAIL", l0, U.lang); }
      }
      bool flag_ok = U.lang_chg; U.lang_chg = false;
      std::string ip = gApp.uitest_dir + "\\settings_t52g.ini"; U.lang = 2; ui::settings_save(U, ip); ui::Ui B; B.games = U.games; ui::settings_load(B, ip); bool rt = B.lang == 2;
      U.lang = 0; phr_set_lang(0);
      gh_log("T52G lang cycle bad=%d flag=%d roundtrip=%d (phrases ko=%d) -> %s", bad, (int)flag_ok, (int)rt, phr_loaded_count, (bad == 0 && flag_ok && rt) ? "PASS" : "FAIL"); }
    /* t52e (#131): 확인창 3종 + 동작 환경 설정 × 복원 검사 */
    if (U.games.size() >= 3) { U.cur = U.pick = 2; gApp.cur_game = 2; } for (auto & gg0 : U.games) { gg0.slot[0].used = true; gg0.slot[0].stamp = "2026-11-02 07:00"; gg0.lastslot = 0; }
    U.in_game = true; U.scr = ui::S_GAME; U.sel = 11; U.key(ui::UK_OK); shotg("yn_mainmenu"); U.key(ui::UK_CANCEL); gh_log("T52E yn_mainmenu back scr=%d (S_GAME=%d)", (int)U.scr, (int)ui::S_GAME);
    U.in_game = false; U.scr = ui::S_SAVE; U.slotsel = 0; gh_log("T52E delslot pre cur=%d used=%d", U.cur, U.game() ? (int)U.game()->slot[0].used : -1); U.key(ui::UK_TRI); gh_log("T52E delslot after TRI scr=%d (S_YN=%d)", (int)U.scr, (int)ui::S_YN); shotg("yn_delslot"); U.key(ui::UK_CANCEL); gh_log("T52E yn_delslot back scr=%d (S_SAVE=%d)", (int)U.scr, (int)ui::S_SAVE);
    U.scr = ui::S_EXTRA; U.sel = 6; U.key(ui::UK_OK); shotg("yn_diskreset"); U.key(ui::UK_CANCEL); gh_log("T52E yn_diskreset back scr=%d (S_EXTRA=%d)", (int)U.scr, (int)ui::S_EXTRA);
    { U.scr = ui::S_MAIN; U.scope = 0; U.mainsel = 3; U.key(ui::UK_OK); U.key(ui::UK_DOWN); int b0 = U.g.bgm; U.key(b0 < 8 ? ui::UK_RIGHT : ui::UK_LEFT); int b1 = U.g.bgm; ui::Act ra = U.key(ui::UK_CANCEL); int b2 = U.g.bgm; gh_log("T52E conf_restore before=%d changed=%d after=%d act=%d -> %s", b0, b1, b2, (int)ra.t, (b0 != b1 && b0 == b2) ? "PASS" : "FAIL"); U.scr = ui::S_MAIN; U.in_game = false; }
    U.in_game = false; U.scr = ui::S_MAIN;
    { /* r017 화면: 회색 목록 · 판정 · 작품 추가 · 창 배율 · 배속 중 소리 */
        if (U.games.size() >= 4) {
            ui::UiGame & a = U.games[1]; a.grade = 'A'; a.jshort = "확인됨 (DB 일치)"; a.judge = "원판과 같은 파일 — " + a.title + " (DB 일치)"; a.metasrc = "DB"; a.company = "컴파일"; a.year = "1995";
            ui::UiGame & b = U.games[3]; b.grade = 'C'; b.block = "마우스 필수 (아직 미지원)"; b.jshort = "후보 · 동급생2"; b.orig = "동급생2"; b.judge = "원판 후보 동급생2 [HD] (확실하지 않음) — 크기 1/1 · 제목 유사 60%"; b.metasrc = "롬셋 폴더"; b.autoreg = true; b.company = "엘프";
            ui::UiGame L = U.games[1]; L.id = "daimado95"; L.title = "대마도전략물어'95 (디스크 스테이션 Vol.8)"; L.grade = 'B'; L.jshort = "추정 (원판 대마도전략물어'95의 변형)"; L.orig = "대마도전략물어'95"; L.autoreg = true;
            L.judge = "원판 대마도전략물어'95 [FD]의 변형(패치판 추정) — 디스크 구성 일치, 크기 다름"; L.metasrc = "TOSEC+롬셋 폴더"; L.company = "디스크 스테이션 편집부 (긴 제작사 이름 시험)"; L.year = "1995"; L.block.clear(); L.thmb.clear();
            ui::UiGame N = U.games[1]; N.id = "nadia"; N.title = "후지의 바다의 나디아"; N.grade = 'A'; N.jshort = "확인됨 (DB 일치)"; N.judge = "원판과 같은 파일 — 후지의 바다의 나디아 [FD] (DB 일치)"; N.metasrc = "롬셋 폴더"; N.company = "Gainax"; N.year = "1991"; N.block = "CD 작품 (CD 미지원)"; N.autoreg = true; N.thmb.clear();
            ui::UiGame D2 = U.games[1]; D2.id = "direct"; D2.title = "환세풍광전"; D2.grade = 'A'; D2.jshort = "직접 지정 · DB 일치"; D2.judge = "game.ini 로 직접 지정 — 파일은 DB의 '환세풍광전' 과 일치"; D2.metasrc.clear(); D2.company.clear(); D2.year.clear(); D2.autoreg = false; D2.block.clear(); D2.thmb.clear();
            U.games.push_back(L); U.games.push_back(N); U.games.push_back(D2);
        }
        U.wmax = 3; U.wmult = 2; U.in_game = false; U.scr = ui::S_PICK; U.pick = 1; shot("r18_pick_judged");
        if (U.games.size() >= 7) {
            U.pick = (int)U.games.size() - 3; U.ts_ms = 0; shot("r18_pick_longtitle_t0"); U.ts_ms = 1000; shot("r18_pick_longtitle_marq_a"); U.ts_ms = 4000; shot("r18_pick_longtitle_marq_b"); U.ts_ms = 0;
            U.pick = (int)U.games.size() - 2; shot("r18_pick_nadia_greybar"); U.pick = (int)U.games.size() - 1; shot("r18_pick_direct_dbmatch"); U.pick = 3; shot("r18_pick_blocked_grey");
            {   /* r019: 마르키 여백 · 차단작(제목 흰색 · 사유만 주황) · 함께 있는 이미지 */
                int lt = (int)U.games.size() - 3; U.pick = lt; U.ts_ms = 2500; shot("r19_pick_longtitle_marq_c"); U.ts_ms = 6000; shot("r19_pick_longtitle_marq_d"); U.ts_ms = 0;
                ui::UiGame P = U.games[1]; P.id = "nadia_h"; P.title = "신비한 바다의 나디아 파란물의 비밀"; P.block.clear(); P.company = "Gainax"; P.year = "1991"; P.metasrc = "DB+롬셋 폴더"; P.autoreg = true;
                P.more = "나디아 Disk A (FD 1장) | 나디아 Option Disk (FD 1장)"; P.disknote = "HDD  함께 있는 이미지: 나디아 Disk A (FD 1장) / 나디아 Option Disk (FD 1장)";
                U.games.push_back(P); U.pick = (int)U.games.size() - 1; shot("r19_pick_companions");
                U.games.back().block = "마우스 필수 (아직 미지원)"; shot("r19_pick_blocked_white_title"); U.games.pop_back();
            }
        }
        U.pick = (int)U.games.size(); shot("r18_pick_add_row");
        U.key(ui::UK_OK); U.add_log = { "폴더 추가: 추가 2 · 이미 있음 1 · 건너뜀 0 (실행 불가 1)", "+ 동급생2  B 변형 추정 · 동급생2 등록함", "+ 샘플 CD 작품  - 등록함 (실행 불가: CD 작품 (CD 미지원))", "이미 있음은 목록에 표시하지 않습니다." }; U.gamefolder = "D:\\PC98\\게임 폴더"; U.sel = 2; shot("r17_add_dialog");
        U.key(ui::UK_CANCEL);
        U.scr = ui::S_SCREEN; U.sel = 6; U.wmult = 2; U.wmax = 3; shot("r17_screen_x2_of_3_grey45");
        ui::Act a1 = U.key(ui::UK_RIGHT); bool okR = a1.t == ui::A_WINMULT && a1.arg == 3 && U.wmult == 3; shot("r17_screen_x3");
        ui::Act a2 = U.key(ui::UK_RIGHT); bool okR2 = a2.t == ui::A_NONE && U.wmult == 3;
        U.wmult = 0; U.winw = 1000; U.winh = 700; shot("r17_screen_custom");
        int up = U.wm_next(1), dn = U.wm_next(-1);
        U.scr = ui::S_CONF; U.sel = 7; U.in_game = false; shot("r17_conf_boost_sound");
        U.scr = ui::S_MAIN; U.sel = 0;
        gh_log("UITEST wm_next: x3->right %s, x3->right-again(no x4: grey) %s, custom1000x700 -> up=x%d down=x%d %s", okR ? "OK" : "FAIL", okR2 ? "OK" : "FAIL", up, dn, (up == 2 && dn == 1) ? "OK" : "FAIL");
        /* 창 크기 이벤트 판정(순수): 우리가 정한 크기가 되돌아오면 무시 · 전체화면/최대화는 무시 · 그 외는 사용자가 끈 것 */
        Uint32 W = 0; unsigned now = 10000, until = 11500;
        bool u1 = gh_win_userresize(1280, 960, W, 1280, 960, now, until) == false, u2 = gh_win_userresize(1000, 700, W, 1280, 960, now, until) == true,
             u3 = gh_win_userresize(1000, 700, SDL_WINDOW_MAXIMIZED, 0, 0, now, until) == false, u4 = gh_win_userresize(1280, 960, W, 1280, 960, now + 2000, until) == true,
             u5 = gh_win_userresize(100, 80, W, 0, 0, now, until) == false, u6 = gh_win_userresize(1000, 700, SDL_WINDOW_FULLSCREEN_DESKTOP, 0, 0, now, until) == false;
        gh_log("UITEST userresize: ours-ignored %s, drag %s, maximized-ignored %s, after-expect-window %s, tiny-ignored %s, fullscreen-ignored %s", u1 ? "OK" : "FAIL", u2 ? "OK" : "FAIL", u3 ? "OK" : "FAIL", u4 ? "OK" : "FAIL", u5 ? "OK" : "FAIL", u6 ? "OK" : "FAIL");
        /* 설정 왕복(창 배율·사용자 지정·게임 폴더·배속 소리) */
        ui::Ui A2; A2.games = U.games; A2.wmult = 0; A2.winw = 1000; A2.winh = 700; A2.gamefolder = "D:\\한글 폴더"; A2.g.bsound = 1; A2.g.bgm = 4;
        std::string ip2 = gApp.uitest_dir + "\\settings_r17.ini"; ui::settings_save(A2, ip2); ui::Ui B2; B2.games = U.games; ui::settings_load(B2, ip2);
        bool r1 = B2.wmult == 0 && B2.winw == 1000 && B2.winh == 700 && B2.gamefolder == "D:\\한글 폴더" && B2.g.bsound == 1 && B2.g.bgm == 4;
        A2.wmult = 3; ui::settings_save(A2, ip2); ui::Ui B3; B3.games = U.games; ui::settings_load(B3, ip2);
        gh_log("UITEST r017 settings roundtrip: custom %s, mult3 %s (wmult=%d)", r1 ? "OK" : "FAIL", B3.wmult == 3 ? "OK" : "FAIL", B3.wmult);
        {   /* 실행 불가 사유(회색) 판정 표 */
            prof::Profile q; q.disk.assign(4, std::string()); q.disk[0] = "a.fdi"; std::map<std::string, std::string> kv; kv["mouse"] = "required"; std::string b1 = aw::block_reason(q, kv);
            kv.clear(); std::string b2 = aw::block_reason(q, kv);
            prof::Profile q2; q2.disk.assign(4, std::string()); q2.install_required = true; std::string b3 = aw::block_reason(q2, kv);
            prof::Profile q3; q3.disk.assign(4, std::string()); kv["nk_media"] = "CD"; std::string b4 = aw::block_reason(q3, kv);
            kv.clear(); kv["nk_flags"] = "inst"; std::string b5 = aw::block_reason(q, kv); kv["curated"] = "1"; std::string b6 = aw::block_reason(q, kv);
            gh_log("UITEST block_reason: mouse-required=[%s] normal=[%s] install-only=[%s] cd=[%s] hdd-install-flag=[%s] curated-ignores-flag=[%s] %s", b1.c_str(), b2.c_str(), b3.c_str(), b4.c_str(), b5.c_str(), b6.c_str(),
                   (!b1.empty() && b2.empty() && !b3.empty() && !b4.empty() && !b5.empty() && b6.empty()) ? "OK" : "FAIL");
        }
        ui::Settings dflt; gh_log("UITEST default bsound=%d (2 = 음높이 유지) %s", dflt.bsound, dflt.bsound == 2 ? "OK" : "FAIL");
    }
    { /* 배속 OSD: 게임 화면 + 오른쪽 위 ▶▶ x3 (+ 다른 안내가 겹칠 때) */
        ov::sample_picture(c, 0, 0, 640, 400); gApp.boost_on = true; gApp.bosd_on = true; gApp.boost_xv = 3; gh_boost_osd_draw(c, "화면 정수배", true);
        char p[700]; snprintf(p, sizeof p, "%s\\ui_%02d_boost_osd.png", gApp.uitest_dir.c_str(), ++n); gh_png(p, buf, 640, 400); gApp.boost_on = false; }
    { gh_logo_draw(c); char p[700]; snprintf(p, sizeof p, "%s\\ui_%02d_boot_logo.png", gApp.uitest_dir.c_str(), ++n); gh_png(p, buf, 640, 400); }   /* t52d (#129) */
    /* 설정 왕복: 전역 + 작품별 */
    ui::Ui A; A.games = U.games; A.g.shader = 2; A.g.curve = 7; A.g.bgm = 3; A.g.bmult = 2; A.ov_set(2, (1u << 8) | (1u << 5)); A.t[2] = A.g; A.t[2].se = 5; A.t[2].scan = 9; A.cur = 2;
    std::string ip = gApp.uitest_dir + "\\settings_test.ini"; ui::settings_save(A, ip);
    ui::Ui B; B.games = U.games; ui::settings_load(B, ip);
    bool ok = B.g.shader == 2 && B.g.curve == 7 && B.g.bgm == 3 && B.g.bmult == 2 && B.has_t[2] && B.t[2].se == 5 && B.t[2].scan == 9 && !B.has_t[0] && B.effective(2).se == 5 && B.effective(0).se == 8 && B.cur == 2;
    gh_log("UITEST settings roundtrip %s (effective(2).se=%d effective(0).se=%d)", ok ? "OK" : "FAIL", B.effective(2).se, B.effective(0).se);
    gh_log("UITEST done: %d images in %s", n, gApp.uitest_dir.c_str());
}

