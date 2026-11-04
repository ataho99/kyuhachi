/* app/menu_boot.h -- S9b: beep, logo, boot sequence, menu run. Moved unchanged from gensei_host.cpp lines 1314-1513 (as of t54t). Included in place; single translation unit. */
/* ---- 부팅 연출(약 1.8초) · 짧은 로딩 · 삐뽀(음량 ≈1/3 × 효과음 설정) */
static void gh_beep(float se_gain) {
    if (gApp.headless) return;
    int amp = (int)(3000.0f * se_gain); if (amp <= 0) return;
    SDL_AudioSpec w; SDL_zero(w); w.freq = 44100; w.format = AUDIO_S16SYS; w.channels = 1; w.samples = 1024; w.callback = NULL;
    SDL_AudioSpec h; gApp.beep_dev = SDL_OpenAudioDevice(NULL, 0, &w, &h, 0); if (!gApp.beep_dev) { gh_log("beep: open fail %s", SDL_GetError()); return; }
    std::vector<int16_t> pcm; const int R = 44100; const double CLK = 1996800.0;
    auto tone = [&](int div, int ms) { double f = CLK / div; int n = R * ms / 1000; for (int i = 0; i < n; i++) { double ph = fmod(i * f / R, 1.0); pcm.push_back((int16_t)(ph < 0.5 ? amp : -amp)); } };
    auto sil = [&](int ms) { pcm.insert(pcm.end(), (size_t)(R * ms / 1000), (int16_t)0); };
    tone(998, 90); sil(40); tone(1997, 160); sil(100);
    SDL_QueueAudio(gApp.beep_dev, pcm.data(), (Uint32)(pcm.size() * 2)); SDL_PauseAudioDevice(gApp.beep_dev, 0);
    gh_log("beep: amp=%d", amp);
}
static void gh_beep_close(void) { if (gApp.beep_dev) { SDL_CloseAudioDevice(gApp.beep_dev); gApp.beep_dev = 0; } }
/* t52d (#129): 시작 로고 — boot_logo_160.txt 1~18줄(160칸 × 4×8 픽셀: # 꽉 참 · + 체크무늬 50% · . 25% 점) + 아래 가운데 KYUHACHI(비트맵 글자). 검정 바탕 흰색 */
static const char * const KH_LOGO[18] = {
    "                                                                                                                                                                ",
    "                                                                                                                                                                ",
    "                                                                                                                                                                ",
    "                                                       ...........                                                  .++..                                       ",
    "                                                       .........                                            ...+++.........+                                    ",
    "                                                     .+........                                     +.......    .......+                                        ",
    "                                                    +++++++++                                      ++++++++    ++++++++                                         ",
    "                                      ##+++++++++++++++++++++++++++++++++++++                     #++++++#   ..+++++++                                          ",
    "                                                  ########.      +########                      .#######+     ########                                          ",
    "                                                +########       ########.                      +#######.     +#######+                                          ",
    "                                              .########.      +########                      +########       +########                                          ",
    "                                             +++++++++      .++++++++.           +         +++++++++        .+++++++++                                          ",
    "                                          ++.......+       +........           +         +........           ...........                                        ",
    "                                       ++++++++++.       +++..+.++         .++        .++++++++.             ++++++++++++.                                      ",
    "                                   .#+++++++++.        +++++++++        ++++.       #+++++++.                 ++++++++++++++                                    ",
    "                              .+#+++++++++.          ++++++++++++++#++++++.     +##+++++.                     .+++++++#++++++##+.                               ",
    "                       . +#########+.               +##################+   ..#####+.                           ###########++++++++###+  .                       ",
    "                 ++######+...                        ...............   +##+..                                   +##..                                           "
};

static void gh_logo_draw(ov::Canvas & c) {
    c.rect(0, 0, 640, 400, ov::K);
    const int y0 = (400 - 197) / 2;
    for (int r = 0; r < 18; r++) {
        const char * ls = KH_LOGO[r];
        for (int q = 0; q < 160; q++) { char ch = ls[q]; if (ch == ' ') continue;
            for (int yy = 0; yy < 8; yy++) for (int xx = 0; xx < 4; xx++) {
                int px = q * 4 + xx, py = r * 8 + yy; bool on = false;
                if (ch == '#') on = true; else if (ch == '+') on = (((px + py) & 1) == 0); else if (ch == '.') on = (((px & 1) == 0) && ((py & 1) == 0));
                if (on) c.set(px, y0 + py, ov::WH); } }
    }
    static uint32_t tb[64 * 16]; ov::Canvas t = { tb, 64, 16 }; t.rect(0, 0, 64, 16, ov::K); ov::text(t, 0, 0, "KYUHACHI", ov::WH, ov::K);
    const int tx = 320 - (8 * 24 - 8) / 2, ty = y0 + 155;
    for (int i = 0; i < 8; i++) for (int sy = 0; sy < 16; sy++) for (int sx = 0; sx < 8; sx++) {
        if (tb[sy * 64 + i * 8 + sx] == ov::K) continue;
        int dy0 = (sy * 3) / 2, dy1 = ((sy + 1) * 3) / 2; c.rect(tx + i * 24 + sx * 2, ty + dy0, 2, dy1 - dy0, ov::WH); }
}
static void gh_logo_frame(void) { static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; gh_logo_draw(c); gh_publish(buf, 640, 400); }
static void gh_boot_frame(int stage, bool cells) {
    static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; c.rect(0, 0, 640, 400, ov::K);
    if (stage >= 1) ov::text(c, 40, 112, std::string(KH_FULLNAME), ov::WH, ov::K);
    if (stage >= 2) ov::text(c, 40, 144, std::string("Now Loading") + std::string((size_t)(stage - 1 > 3 ? 3 : stage - 1), '.'), ov::WH, ov::K);
    if (cells) ov::fncells_boot(c);   /* t52a (#49) */
    gh_publish(buf, 640, 400);
}
/* true = 건너뜀. 아무 버튼이나 누르면 건너뛴다. */
static bool gh_boot_seq(bool full) {
    if (gApp.headless || !gApp.use_gl) return false;
    if (full) {   /* t52d (#129): 시작 로고 1.4초 — 아무 키·클릭이면 부팅 연출 전체를 건너뜀 */
        unsigned tl = SDL_GetTicks(); bool sk = false;
        for (;;) { if (SDL_GetTicks() - tl >= 1400u) break; SDL_Event e; while (SDL_PollEvent(&e)) { bool ak = false; gh_ev2uk(e, &ak); if (e.type == SDL_QUIT) { GH_QUITSET(); sk = true; } if (ak) sk = true; } if (sk) return true; gh_logo_frame(); SDL_Delay(16); }
    }
    unsigned t0 = SDL_GetTicks(); bool beeped = false; bool skipped = false; unsigned total = full ? 1800u : 500u;
    for (;;) {
        unsigned t = SDL_GetTicks() - t0; if (t >= total) break;
        SDL_Event e; while (SDL_PollEvent(&e)) { bool ak = false; gh_ev2uk(e, &ak); if (e.type == SDL_QUIT) { GH_QUITSET(); skipped = true; } if (ak) skipped = true; }
        if (skipped) break;
        if (full) {
            if (t >= 300 && !beeped) { beeped = true; gh_beep(gApp.ui_.effective(gApp.cur_game).se_gain()); }
            int stage = t < 300 ? 0 : t < 700 ? 1 : t < 1000 ? 2 : t < 1300 ? 3 : 4; gh_boot_frame(stage, t >= 1300);
        } else gh_boot_frame(t < 150 ? 1 : 4, false);
        SDL_Delay(16);
    }
    if (gApp.beep_dev && skipped) SDL_ClearQueuedAudio(gApp.beep_dev);
    if (full && beeped && !skipped) SDL_Delay(0);
    gh_beep_close();
    return skipped;
}
static void gh_spawn_menu(void) {
    std::string exe = plat::exe_path();
    std::string cmd = "\"" + exe + "\" --noboot"; unsigned long perr = 0;
    if (plat::spawn(exe, cmd, gApp.exedir, &perr)) { gh_log("spawn menu OK"); }
    else gh_log("spawn menu FAIL err=%lu", (unsigned long)perr);
}
/* ---- t49a (#845): 실행 순서(단계) — 항목의 next= 사슬. 「처음부터」 는 사슬의 첫 단계부터, 「이어하기」 는 지금 고른(마지막) 단계로.
   다음 단계로는 아무 입력(키·클릭·패드) 또는 일시정지 메뉴 「다음 단계」 — 프로세스를 새로 띄워(--entry <다음>) 디스크 구성을 바꾼다. 게임 안 화면 감지·자동 전환 없음 */
static const ui::UiEnt * gh_find_ent(int gi, const std::string & name) { if (gi < 0 || gi >= (int)gApp.ui_.games.size() || name.empty()) return NULL; for (auto & e : gApp.ui_.games[gi].ents) if (e.name == name && e.vgi < 0 && !e.pseudo) return &e; return NULL; }
static std::string gh_ent_next(int gi, const std::string & name) {   /* 다음 단계 이름("-" = 기본 실행). 없거나 가리키는 항목이 없으면 빈 문자열 */
    const ui::UiEnt * e = gh_find_ent(gi, name); if (!e) return std::string(); auto it = e->kv.find("next"); if (it == e->kv.end() || it->second.empty() || it->second == "none") return std::string();
    if (it->second == "-" || gh_find_ent(gi, it->second)) return it->second; return std::string(); }
static std::string gh_stage_label(int gi, const std::string & nx) { if (nx == "-") return phr_("게임"); const ui::UiEnt * e = gh_find_ent(gi, nx); return e ? (e->label.empty() ? e->name : e->label) : nx; }
static std::string gh_chain_head(int gi, const std::string & req) {   /* req(고른 단계)로 이어지는 사슬의 맨 앞 단계 이름. 앞선 단계가 없으면 req 그대로 */
    if (gi < 0 || gi >= (int)gApp.ui_.games.size()) return req; const ui::UiGame & g = gApp.ui_.games[gi];
    std::set<std::string> T; if (req.empty() || req == "-") { T.insert("-"); std::string d = g.defent; if (d.empty()) for (auto & e : g.ents) if (!e.pseudo && !e.hidden && e.vgi < 0) { d = e.name; break; } if (!d.empty()) T.insert(d); } else T.insert(req);
    std::string head = req; std::set<std::string> seen;
    for (int depth = 0; depth < 8; depth++) {
        std::string found; for (auto & e : g.ents) { if (e.pseudo || e.vgi >= 0 || e.name.empty() || seen.count(e.name) || T.count(e.name)) continue; std::string nx = gh_ent_next(gi, e.name); if (!nx.empty() && T.count(nx)) { found = e.name; break; } }
        if (found.empty()) break; seen.insert(found); head = found; T.clear(); T.insert(found);
    }
    return head; }
static void gh_chain_start(ui::Ui & U, int gi) {   /* 메뉴에서 「처음부터」(g_resume_slot == 0) 로 시작할 때 사슬의 첫 단계로 바꾼다 */
    if (gApp.nostage || gApp.resume_slot != 0) return; std::string h = gh_chain_head(gi, prof::g_entry_req); if (h != prof::g_entry_req) { gh_log("STAGE 처음부터: %s -> 첫 단계 %s", prof::g_entry_req.c_str(), h.c_str()); prof::g_entry_req = h; } (void)U; }
static void gh_stage_init(void) {   /* 게임 시작 때: 지금 단계의 다음 단계가 있으면 안내·메뉴 항목을 켠다 */
    gApp.stage_next.clear(); gApp.ui_.stage_next_label.clear(); if (gApp.nostage) return;
    std::string nm = (!prof::g_entry_req.empty() && prof::g_entry_req != "-") ? prof::g_entry_req : std::string(); if (nm.empty()) { auto it = gApp.prof_.kv.find("entry_name"); if (it != gApp.prof_.kv.end()) nm = it->second; }
    gApp.stage_entry = nm;
    std::string nx = gh_ent_next(gApp.cur_game, nm); if (nx.empty()) { gh_log("STAGE entry='%s' next=(없음)", nm.c_str()); return; }
    gApp.stage_next = nx; const ui::UiEnt * ce = gh_find_ent(gApp.cur_game, nm); std::string nl = phr_("다음 단계로 넘어가기"); if (ce) { auto li = ce->kv.find("next_label"); if (li != ce->kv.end() && !li->second.empty()) nl = li->second; } /* 속성 「다음 단계로」: 메뉴 이름 = next_label (확장 자리: 속성 종류를 더하려면 여기에 항목을 추가) */
    gApp.ui_.stage_next_label = nl; gh_log("STAGE entry='%s' next='%s' (%s)", nm.c_str(), nx.c_str(), nl.c_str()); }
static void gh_spawn_stage(const std::string & entry) {   /* 같은 실행 인자에서 --demo/--entry 를 빼고 --entry <다음> 을 붙여 새 프로세스로 */
    std::string exe = plat::exe_path(); std::string cmd = "\"" + exe + "\"";
    for (size_t i = 1; i < gApp.argv_saved.size(); i++) { const std::string & a = gApp.argv_saved[i]; if (a == "--demo") continue; if (a == "--entry") { i++; continue; } cmd += " \"" + a + "\""; }
    if (!gApp.id_given) { cmd += " \""; cmd += (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) ? gApp.ui_.games[gApp.cur_game].id : std::string(gApp.prof_.id ? gApp.prof_.id : ""); cmd += "\" --noboot"; }
    cmd += " --entry \"" + aw::u8_to_acp(entry) + "\"";
    unsigned long perr = 0;
    if (plat::spawn(exe, cmd, gApp.exedir, &perr)) { gh_log("STAGE 다음 단계 실행: %s", cmd.c_str()); }
    else gh_log("STAGE spawn FAIL err=%lu", (unsigned long)perr);
}
static void gh_sendkey(int arg) {
    static const KBD_KEYS K9[9] = { KBD_f10, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7, KBD_kana };
    if (arg < 0 || arg > 8) return;
    if ((int)K9[arg] < 256 && kh_never[(int)K9[arg]]) { gh_log("sendkey #%d blocked (never_map)", arg); gApp.osd = phr_("이 작품에서는 보낼 수 없는 키입니다"); gApp.osd_until = SDL_GetTicks() + 2000; return; }
    gApp.key_down = K9[arg]; gApp.key_release_at = SDL_GetTicks() + 100; KEYBOARD_AddKey(gApp.key_down, true);
    gh_log("sendkey #%d", arg);
}
static bool gh_play_commit(void) { unsigned s = gApp.play_ms / 1000; if (!s || gApp.cur_game < 0 || gApp.cur_game >= (int)gApp.ui_.games.size()) return false; gApp.ui_.games[gApp.cur_game].play_s += (int)s; gApp.play_ms -= s * 1000; return true; }
/* ---- 게임 중 메뉴(에뮬 스레드에서 도는 중첩 루프 = 게임이 멈춘다) */
static void gh_bk_pause_set(bool on, const char * why) {   /* t47a (#559): 자동 입력 타이머만 멈춘다(게임은 계속 돈다) */
    if (on) { if (gApp.prof_.bootkeys.empty()) { gh_osd(phr_("자동 입력 없음"), 1500); return; } if (gApp.next_bk >= gApp.prof_.bootkeys.size()) { gh_osd(phr_("자동 입력이 이미 끝났습니다"), 1500); return; } }
    if (gApp.bk_paused == on) return;
    gApp.bk_paused = on; gh_log("BKPAUSE %s (%s) bk_t=%u next=#%u wall=%u", on ? "정지" : "재개", why, gApp.bk_t, (unsigned)gApp.next_bk, gApp.boot_tick ? SDL_GetTicks() - gApp.boot_tick : 0u);
    gh_osd(on ? phr_("자동 입력 일시정지 (Alt+P 로 재개)") : phr_("자동 입력 재개"), 1500);
}
static void gh_menu_run(void) {

    if (gApp.headless || !gApp.use_gl) return;
    kbov::release_all(); gApp.ui_.kb_vis = kbov::g_vis ? 1 : 0; gApp.ui_.kb_btn = kbov::g_btn_on ? 1 : 0; gApp.ui_.kb_sh = gApp.kbshader ? 1 : 0; gApp.menu_open = true; gh_boost_update();
    static uint32_t snap[640 * 400], buf[640 * 400]; ov::Canvas sc = { snap, 640, 400 }, c = { buf, 640, 400 };
    sc.rect(0, 0, 640, 400, ov::K); if (!gApp.fb.empty()) ov::blit_scaled(sc, gApp.fb.data(), gApp.fbw, gApp.fbh, 0, 0, 640, 400);
    ui::Ui & U = gApp.ui_; U.in_game = true; U.cur = gApp.cur_game; U.scr = ui::S_GAME; U.sel = 0;
    gh_refresh_game_slots(gApp.cur_game); U.slotsel = U.games[gApp.cur_game].lastslot >= 0 ? U.games[gApp.cur_game].lastslot : 0;
    bool done = false; bool sendk = false; int sendarg = 0;
    gh_diag_enter(1);
    while (!done && !gApp.quit) {
        gh_inj_poll_menu();   /* t49a: 하니스 inject 를 일시정지 메뉴에서도 */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (gh_evpre(e, 1)) continue;
            if (e.type == SDL_QUIT) { GH_QUITSET(); done = true; break; }
            if (gh_wm_hotkey(e)) continue;
#if defined(__ANDROID__)
            if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP) { gh_touch_feed(e); continue; }   /* t55a: menu touch */
#endif
            ui::UKey k = gh_ev2uk(e, NULL); if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) gh_log("DIAG btn -> ev2uk=%d none=%d", (int)k, k == ui::UK_NONE ? 1 : 0); if (k == ui::UK_NONE) continue;
            ui::Act a = U.key(k);
            switch (a.t) {
            case ui::A_RESUME: done = true; break;
            case ui::A_NEXTSTAGE: if (!gApp.stage_next.empty()) { gh_log("STAGE F12 menu '%s' -> %s", U.stage_next_label.c_str(), gApp.stage_next.c_str()); if (gApp.inwin_enabled && !gApp.headless && gApp.use_gl) { gApp.inwin_req = 1; gApp.inwin_target = gApp.stage_next; } else gApp.stage_go = true; GH_QUITSET(); done = true; } break;   /* t49a (#847) · t51a: 창 안 전환 먼저, 안 되면 gh_inwin_cycle 이 g_stage_go 로 폴백 */
            case ui::A_SAVE: { std::string d = gApp.savedir + "\\" + gApp.prof_.id; plat::mkdir(gApp.savedir.c_str()); plat::mkdir(d.c_str());
                bool ok = gh_do_save(gh_slot_n(gApp.cur_game, a.slot), false); gh_osd(ok ? phr_("슬롯 ") + std::to_string(a.slot + 1) + phr_(" 에 저장했습니다") : std::string(phr_("저장에 실패했습니다")), 2000); done = true; break; }
            case ui::A_LOAD: { if (!U.games[gApp.cur_game].slot[a.slot].used) { gh_osd(phr_("비어 있는 슬롯입니다"), 1500); break; }
                bool ok = gh_do_load(gh_slot_n(gApp.cur_game, a.slot)); gh_osd(ok ? phr_("슬롯 ") + std::to_string(a.slot + 1) + phr_(" 을 불러왔습니다") : std::string(phr_("불러오기에 실패했습니다")), 2000); done = true; break; }
            case ui::A_DELSLOT: gh_delete_slot(gApp.cur_game, a.slot); break;
            case ui::A_APPLY: gh_apply_settings(U.eff(), true); kh_western = U.eff().pad == 1; gh_apply_profile_input(gApp.prof_, true); break;   /* t48e (#646): 조작 설정 변경을 바로 입력 층에 */
            case ui::A_SHOT: gApp.scr_req = true; break;   /* t41: 메뉴가 닫힌 뒤 다음 프레임에서 찍는다 */
            case ui::A_WINMULT: gh_win_apply(a.arg, false); break;
            case ui::A_FULLSCR: gh_fs_toggle(); break;
            case ui::A_KBD: kbov::set_visible(!kbov::g_vis); gh_kb_cursor(); gh_osd(kbov::g_vis ? phr_("화면 키보드 켜짐 (Alt+K 로 끔)") : phr_("화면 키보드 꺼짐"), 2000); done = true; break;
            case ui::A_KBSH: { int v = gApp.kbshader ? 0 : 1; gApp.kbshader = v; U.g.kbshader = v; if (gApp.cur_game >= 0 && gApp.cur_game < (int)ui::Ui::MAXG && U.has_t[gApp.cur_game]) U.t[gApp.cur_game].kbshader = v; U.kb_sh = v; U.dirty = true; gh_osd(v ? phr_("키보드·배속 표시에 셰이더를 적용합니다") : phr_("키보드·배속 표시를 셰이더 없이 그립니다"), 2500); break; }
            case ui::A_KBBTN: {
 int v = kbov::g_btn_on ? 0 : 1; kbov::g_btn_on = v != 0; U.g.kbbtn = v; if (gApp.cur_game >= 0 && gApp.cur_game < (int)ui::Ui::MAXG && U.has_t[gApp.cur_game]) U.t[gApp.cur_game].kbbtn = v; U.kb_btn = v; U.dirty = true; kbov::g_dirty = true; gh_osd(v ? phr_("키보드 버튼을 표시합니다") : phr_("키보드 버튼을 숨깁니다 (Alt+K · 메뉴로 키보드 켜기)"), 2500); break; }
            case ui::A_SENDKEY: sendk = true; sendarg = a.arg; done = true; break;
            case ui::A_DISKSWAP: { auto fit_ = gApp.fdfiles.find(U.games[gApp.cur_game].id); bool ok = false;
                if (fit_ != gApp.fdfiles.end() && a.arg >= 0 && a.arg < (int)fit_->second.size() && a.slot >= 0 && a.slot < 2) { ok = gh_attach(a.slot, fit_->second[a.arg].c_str(), true); if (ok) U.games[gApp.cur_game].cur_fd[a.slot] = U.games[gApp.cur_game].fds[a.arg]; }
                gh_log("DISKSWAP drive=%c idx=%d %s", a.slot ? 'B' : 'A', a.arg, ok ? "OK" : "FAIL");
                gh_osd(ok ? std::string(a.slot ? "B" : "A") + phr_(" 드라이브 : ") + U.games[gApp.cur_game].cur_fd[a.slot] : std::string(phr_("디스크 교체에 실패했습니다")), 3000); done = true; break; }
            case ui::A_MAINMENU: gh_play_commit(); ui::settings_save(U, gApp.cfgpath); if (gApp.inwin_enabled && !gApp.headless && gApp.use_gl) gApp.inwin_req = 2; else gh_spawn_menu(); GH_QUITSET(); done = true; break;   /* t51a: 창 안 메뉴 */
            case ui::A_QUIT: gh_play_commit(); if (a.arg) ui::settings_save(U, gApp.cfgpath); GH_QUITSET(); done = true; break;
            default: break; }
            if (done) break;
        }
        gh_wm_poll();
        if (U.scr == ui::S_GAME || U.scr == ui::S_KEYS || U.scr == ui::S_QUIT || U.scr == ui::S_SWAP || U.scr == ui::S_SWAPI) memcpy(buf, snap, sizeof buf);
        U.fs_now = gh_fs_now(); U.ts_ms = (int)SDL_GetTicks(); U.draw(c); { std::string bw = gh_builtin_warn(); if (!bw.empty()) ov::osd_draw(c, bw, false, 336); } if (gApp.osd_until && SDL_GetTicks() < gApp.osd_until) ov::osd_draw(c, gApp.osd, true);
        gh_publish(buf, 640, 400); SDL_Delay(16);
    }
    if (U.dirty) { ui::settings_save(U, gApp.cfgpath); U.dirty = false; }
    U.in_game = false; gApp.menu_open = false; gApp.bk_last = SDL_GetTicks();   /* t47a: 메뉴에 있던 시간은 자동 입력 시계에서 뺀다 */
    { int cg_ = gApp.cur_game; std::string o_; if (cg_ >= 0 && cg_ < (int)U.games.size()) for (auto & kv_ : U.games[cg_].ov) o_ += kv_.first + "=" + kv_.second + " "; gh_log("APPLYDIAG id=%s has_t=%d ov[%s] g.scan=%d glow=%d curve=%d", (cg_ >= 0 && cg_ < (int)U.games.size()) ? U.games[cg_].id.c_str() : "-", (int)(cg_ >= 0 && cg_ < (int)ui::Ui::MAXG && U.has_t[cg_]), o_.c_str(), U.g.scan, U.g.glow, U.g.curve); } gh_apply_settings(U.effective(gApp.cur_game), true); gh_boost_update(); kbov::wake();
    if (gApp.osd_until && !gApp.boost_on && SDL_GetTicks() < gApp.osd_until && gApp.osd.find("▶▶") == 0) gApp.osd_until = 0;
    if (sendk) gh_sendkey(sendarg);
    if (!gApp.fb.empty()) gh_publish(gApp.fb.data(), gApp.fbw, gApp.fbh);
}

