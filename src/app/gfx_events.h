static void gh_finish(int rc);   /* t139: 전방 선언 */
/* app/gfx_events.h -- S9b: GFX_Events. Moved unchanged from gensei_host.cpp lines 1795-1992 (as of t54t). Included in place; single translation unit. */
void GFX_Events(void) {
    static unsigned int last_ev = 0;
    unsigned int now = SDL_GetTicks();
    kh_input_tick();
    kha_ld_tick(now);
    if (now != last_ev) {
        last_ev = now;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (gh_evpre(e, -1)) continue;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) kbov::release_all();   /* t46x: 창 포커스를 잃으면 잠긴 키 포함 전부 해제 */
            if (e.type == SDL_KEYDOWN && (e.key.keysym.mod & KMOD_ALT) && !gApp.headless && gApp.win) {   /* r010 화면 단축키(Alt+…): PC-98 에는 Alt 가 없어 게임 키와 겹치지 않음 */
                SDL_Keycode k = e.key.keysym.sym; bool hit = true;
                if (k == SDLK_RETURN) { bool fs = (SDL_GetWindowFlags(gApp.win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP; SDL_SetWindowFullscreen(gApp.win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); SDL_ShowCursor(fs ? SDL_ENABLE : SDL_DISABLE); gh_log("SCREEN fullscreen=%d", fs ? 0 : 1); gh_osd(fs ? phr_("창 모드") : phr_("전체화면")); }
                else if (k == SDLK_1) { gApp.fit = FIT_43; gh_log("SCREEN fit=4:3"); gh_osd(phr_("화면 4:3")); }
                else if (k == SDLK_2) { gApp.fit = FIT_1610; gh_log("SCREEN fit=16:10"); gh_osd(phr_("화면 16:10")); }
                else if (k == SDLK_3) { gApp.fit = FIT_INT; gh_log("SCREEN fit=integer"); gh_osd(phr_("화면 정수배")); }
                else if (k == SDLK_m) { gApp.mouse_free = !gApp.mouse_free; gh_log("HOTKEY 마우스 가둠 %s", gApp.mouse_free ? "해제" : "복귀"); gh_osd(gApp.mouse_free ? phr_("마우스 가둠 해제 (Alt+M 으로 다시 가둠)") : phr_("마우스 가둠 복귀")); }   /* t43m: 가둠 해제키 */
                else if (k == SDLK_s) { gApp.shader = (gApp.shader + 1) % SH_N; gh_log("SCREEN shader=%d", (int)gApp.shader); static const char * const sn[SH_N] = { "nearest", "sharp", "crt" }; gh_osd(std::string(phr_("셰이더 ")) + sn[gApp.shader % SH_N]); }
                else if (k == SDLK_UP) { float c = gApp.curve + 0.01f; gApp.curve = c > 0.10f ? 0.10f : c; gh_log("SCREEN curve=%.2f", (double)gApp.curve); { char b[32]; snprintf(b, sizeof b, "곡면 %.2f", (double)gApp.curve); gh_osd(b); } }
                else if (k == SDLK_DOWN) { float c = gApp.curve - 0.01f; gApp.curve = c < 0.0f ? 0.0f : c; gh_log("SCREEN curve=%.2f", (double)gApp.curve); { char b[32]; snprintf(b, sizeof b, "곡면 %.2f", (double)gApp.curve); gh_osd(b); } }
                else if (k == SDLK_F5) { if (gApp.boot_tick) { std::string p = gh_slot_path(); bool ok = gh_do_save(p, false); gh_log("HOTKEY save slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 에 저장했습니다") : phr_("저장에 실패했습니다"), 2000); } }
                else if (k == SDLK_F9) { if (gApp.boot_tick) { std::string p = gh_slot_path(); bool ok = gh_do_load(p); gh_log("HOTKEY load slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 을 불러왔습니다") : phr_("불러오기에 실패했습니다"), 2000); } }
                else if (k == SDLK_k) { if (!gApp.headless && gApp.use_gl && gApp.boot_tick) gh_kb_toggle(); }   /* t46x: Alt+K = 화면 키보드 켜고 끄기 */
                else if (k == SDLK_p) { if (gApp.boot_tick) gh_bk_pause_set(!gApp.bk_paused, "Alt+P"); }   /* t47a: Alt+P = 자동 입력 일시정지/재개 */
                else if (k == SDLK_LEFTBRACKET || k == SDLK_RIGHTBRACKET) gh_wm_hotkey(e);
                else hit = false;
                if (hit) continue;
            }
            if (gh_vpad_event(e)) continue;   // virtual pad (side bands)
            if (gh_kbd_event(e)) continue;   /* t46x: 화면 키보드·토글 버튼(메뉴 > 키보드 > 게임 순서) */
            if (!gApp.menu_open && gh_game_event(e)) continue;
            if (gensei_input_event(e)) continue;
            if (e.type == SDL_QUIT) { gh_log("SDL_QUIT"); GH_QUITSET(); }
        }
        if (!gApp.headless) gh_wm_poll();
        { extern bool user_cursor_locked; bool lk = gApp.mouse_game && gApp.boot_tick && !gApp.menu_open; if (lk != user_cursor_locked) { user_cursor_locked = lk; gh_log("mouse: 코어 user_cursor_locked=%d (PC-98 버스마우스는 이 값이 참이어야 이동량을 받는다)", (int)lk); } }   /* r021: 코어 Mouse_CursorMoved 는 user_cursor_locked 일 때만 pc98_mouse_movement_apply 를 부른다 (r020 에서 커서가 안 움직인 원인) */
        if (!gApp.headless && gApp.win) { bool want = gApp.mouse_game && gApp.boot_tick && !gApp.menu_open && !gApp.mouse_free && !kbov::g_vis; if (want != gApp.relmouse) { gApp.relmouse = want; SDL_SetRelativeMouseMode(want ? SDL_TRUE : SDL_FALSE); gh_log("mouse: SDL 상대 마우스 모드 %s", want ? "켬" : "끔"); } }
        if (gApp.menu_req) { gApp.menu_req = false; gh_menu_run(); }
        if (gApp.boot_tick) {
            unsigned int since = now - gApp.boot_tick;
            if (!gApp.bk_started) { gApp.bk_started = true; gApp.bk_last = now; gApp.bk_t = since; }   /* t47a: 자동 입력 시계 — 일시정지·메뉴 시간은 빼고 흐른다 */
            { int dd = (int)(now - gApp.bk_last); if (dd > 0) { if (!gApp.bk_paused) gApp.bk_t += (unsigned)dd; gApp.bk_last = now; } }
            if (gApp.bkp_to && !gApp.bkp_a && since >= gApp.bkp_from) { gApp.bkp_a = true; gh_bk_pause_set(true, "CLI"); }
            if (gApp.bkp_to && !gApp.bkp_b && since >= gApp.bkp_to) { gApp.bkp_b = true; gh_bk_pause_set(false, "CLI"); }

            if (gApp.play_last && !gApp.menu_open) gApp.play_ms += now - gApp.play_last; gApp.play_last = now;
            while (gApp.next_mouse < gApp.amouse.size() && since >= gApp.amouse[gApp.next_mouse].at_ms) {   /* r020: 시험용 자동 마우스 — SDL 이벤트로 밀어 넣어 실제 입력 경로(gh_game_event)를 지나게 한다 */
                const GMouse & m = gApp.amouse[gApp.next_mouse]; SDL_Event se; memset(&se, 0, sizeof se);
                if (m.act == 0 || m.dx != 0 || m.dy != 0) { se.type = SDL_MOUSEMOTION; se.motion.xrel = (int)m.dx; se.motion.yrel = (int)m.dy; SDL_PushEvent(&se); memset(&se, 0, sizeof se); }
                if (m.act) { se.type = (m.act == 1 || m.act == 3) ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; se.button.button = (m.act <= 2) ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT; se.button.state = (m.act == 1 || m.act == 3) ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&se); }
                gh_log("automouse #%u at %u ms dx=%.0f dy=%.0f act=%d", (unsigned)gApp.next_mouse, since, (double)m.dx, (double)m.dy, m.act); gApp.next_mouse++;
            }
            while (gApp.next_inj < gApp.inj.size() && since + gApp.inj_base >= gApp.inj[gApp.next_inj].at) { gh_inj_run(gApp.inj[gApp.next_inj], since + gApp.inj_base); gApp.next_inj++; }   /* t48e (#648): --inject · t51a: 창 안 전환을 건너도 시험 시계가 이어지게 g_inj_base 를 더한다 */
            kbov::tick(); gh_vpad_tick();   /* t52f: 빠른 불러오기 길게 누름(0.6초) 판정 */
            if (!gApp.headless && gApp.win && gApp.use_gl) gh_kbtest_tick(since);   /* t46x 시험 전용(옵션이 없으면 아무 일도 안 함) */
            kh_vmouse_pump();   /* t34: 호스트 마우스 이동량을 20ms 마다 ±100 이하로 코어에 */
#if defined(__ANDROID__)
            gh_kbhold_tick();   /* t85: deferred release of short key presses */
#endif
            if (gApp.key_down != KBD_NONE && now >= gApp.key_release_at) { KEYBOARD_AddKey(gApp.key_down, false); gApp.key_down = KBD_NONE; }
            if (!gApp.bk_paused && gApp.next_bk < gApp.prof_.bootkeys.size() && gApp.bk_t >= gApp.prof_.bootkeys[gApp.next_bk].at_ms && gApp.key_down == KBD_NONE) {   /* t36cd: bootkeys — 실행 항목이 정한 부팅 후 키·명령 */
                gApp.key_down = gApp.prof_.bootkeys[gApp.next_bk].key; gApp.key_release_at = now + gApp.keyhold;
                KEYBOARD_AddKey(gApp.key_down, true);
                gh_log("bootkey #%u at %u ms", (unsigned)gApp.next_bk, gApp.bk_t); gApp.next_bk++; }
            { size_t nst = 0; for (const GKey & kk : gApp.prof_.bootkeys) if ((size_t)kk.step > nst) nst = (size_t)kk.step;   /* t47a: 단계 OSD 줄(번호 = 편집 화면 단계 번호, 아직 안 나간 첫 키의 단계) */
              std::string bs;
              if (nst && (gApp.bk_osd_on || gApp.bk_paused)) {
                  if (gApp.next_bk < gApp.prof_.bootkeys.size()) { const GKey & kk = gApp.prof_.bootkeys[gApp.next_bk]; int rem = (int)kk.at_ms - (int)gApp.bk_t; if (rem < 0) rem = 0; char bb[96]; snprintf(bb, sizeof bb, "자동입력 %d/%u · 다음 %d.%d초 ", kk.step, (unsigned)nst, rem / 1000, (rem % 1000) / 100); bs = std::string(bb) + kk.lab; }
                  else if (gApp.bk_osd_on) { if (!gApp.bk_done_at) gApp.bk_done_at = now ? now : 1; if (now - gApp.bk_done_at < 3000) bs = phr_("자동입력 완료 ") + std::to_string(nst) + "/" + std::to_string(nst); }
                  if (gApp.bk_paused && !bs.empty()) bs = phr_("[일시정지] ") + bs;
              }
              gApp.bk_osd = bs; }

            if (gApp.autokeys && gApp.next_key < gApp.prof_.keys.size() && since >= gApp.prof_.keys[gApp.next_key].at_ms && gApp.key_down == KBD_NONE) {
                gApp.key_down = gApp.prof_.keys[gApp.next_key].key; gApp.key_release_at = now + gApp.keyhold;
                KEYBOARD_AddKey(gApp.key_down, true);
                gh_log("autokey #%u at %u ms", gApp.next_key, since);
                gApp.next_key++;
            }
            if (gApp.save_ms && !gApp.save_done && since >= gApp.save_ms) {
                gApp.save_done = true;
                gh_dump_named("pre_save", since);
                gh_do_save(gApp.state_path.empty() ? gh_slot_path() : gApp.state_path, true);
                gApp.ev_tick = now ? now : 1;
            }
            if (gApp.load_ms && !gApp.load_done && since >= gApp.load_ms) {
                gApp.load_done = true;
                gh_dump_named("pre_load", since);
                gh_do_load(gApp.state_path.empty() ? gh_slot_path() : gApp.state_path);
                gApp.ev_tick = now ? now : 1;
            }
            if (gApp.resume_slot && since >= 2000) {   /* r012: UI 에서 「이어하기」로 시작 */
                int sl = gApp.resume_slot - 1; gApp.resume_slot = 0;
                bool ok = gh_do_load(gh_slot_n(gApp.cur_game, sl)); gh_log("RESUME slot %d %s", sl + 1, ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 ") + std::to_string(sl + 1) + phr_(" 에서 이어합니다") : std::string(phr_("이어하기에 실패했습니다")), 2000);
            }
            if (gApp.bt_x) {   /* 시험: 배속 비율 실측 (--boosttest 시작초,길이초,배율) */
                static unsigned w0 = 0, p0 = 0; static int ph = 0; static unsigned lastlog = 0;
                if (ph == 0 && since >= gApp.bt_at) { ph = 1; w0 = now; p0 = (unsigned)PIC_Ticks; for (int i = 0; i < 5; i++) if (ui::Settings().boost_x() >= 0 && (i == 0 ? 2 : i == 1 ? 3 : i == 2 ? 4 : i == 3 ? 6 : 8) == gApp.bt_x) gApp.ui_.g.bmult = i; gApp.ui_.g.bmode = 2; gApp.boost_hold = true; gh_boost_update(); gh_log("BOOSTTEST start wall=%u game=%u x=%d", since, p0, gApp.bt_x); }
                if (ph == 1 && now - lastlog >= 500) { lastlog = now; gh_log("BOOSTTEST wall+%u game+%u", now - w0, (unsigned)PIC_Ticks - p0); }
                if (ph == 1 && now - w0 >= gApp.bt_len) { ph = 2; unsigned dw = now - w0, dg = (unsigned)PIC_Ticks - p0; gApp.boost_hold = false; gh_boost_update(); gh_log("BOOSTTEST done wall=%u game=%u ratio=%.3f", dw, dg, dw ? (double)dg / dw : 0.0); }
            }
            if (gApp.swap_ms && !gApp.swap_done && since >= gApp.swap_ms) {   /* 시험: 실행 중 디스크 교체 */
                gApp.swap_done = true; auto fi = gApp.fdfiles.find(gApp.prof_.id); bool ok = false;
                if (fi != gApp.fdfiles.end() && gApp.swap_idx >= 0 && gApp.swap_idx < (int)fi->second.size()) ok = gh_attach(gApp.swap_drv, fi->second[gApp.swap_idx].c_str(), true);
                gh_log("SWAPTEST drive=%d idx=%d %s", gApp.swap_drv, gApp.swap_idx, ok ? "OK" : "FAIL");
            }
            if (gApp.axistest) {   /* 시험: R2 축 이벤트를 밀어 넣어 배속 on/off 와 로그를 확인 */
                static int ph = 0; SDL_Event ev; SDL_zero(ev); ev.type = SDL_CONTROLLERAXISMOTION; ev.caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
                if (ph == 0 && since >= 2000) { ph = 1; ev.caxis.value = 30000; SDL_PushEvent(&ev); gh_log("AXISTEST push 30000"); }
                if (ph == 1 && since >= 3500) { ph = 2; ev.caxis.value = 0; SDL_PushEvent(&ev); gh_log("AXISTEST push 0"); }
            }
            if (gApp.caps_len) {   /* 시험: CAPS(빨리 넘김) 누름 (--capstest 시작초,길이초) */
                static int ph = 0;
                if (ph == 0 && since >= gApp.caps_at) { ph = 1; KEYBOARD_AddKey(KBD_capslock, true); gh_log("CAPSTEST down at %u game=%u", since, (unsigned)(PIC_Ticks - gApp.pic0)); }
                if (ph == 1 && since >= gApp.caps_at + gApp.caps_len) { ph = 2; KEYBOARD_AddKey(KBD_capslock, false); gh_log("CAPSTEST up at %u game=%u", since, (unsigned)(PIC_Ticks - gApp.pic0)); }
            }
            if (gApp.ev_tick) {
                static const unsigned offs[3] = { 100, 1000, 3000 };
                static unsigned next_post = 0;
                if (next_post < 3 && now - gApp.ev_tick >= offs[next_post]) { gh_dump_named(gApp.save_done && !gApp.load_done ? "post_save" : "post_load", offs[next_post]); next_post++; }
            }
            if (gApp.glmx_ms && !gApp.glmx_done && gApp.use_gl && since >= gApp.glmx_ms) {   /* r010 시험: 화면 설정 조합별 렌더 결과를 GL 에서 읽어 PNG 로 */
                gApp.glmx_done = true;
                struct C { int fit, sh; float cv; const char * tag; };
                static const C cs[] = { {FIT_43, SH_SHARP, 0.f, "fit43_sharp"}, {FIT_43, SH_CRT, 0.f, "fit43_crt_flat"}, {FIT_43, SH_CRT, 0.04f, "fit43_crt_curve004"},
                                        {FIT_43, SH_NEAREST, 0.f, "fit43_nearest"}, {FIT_1610, SH_SHARP, 0.f, "fit1610_sharp"}, {FIT_INT, SH_NEAREST, 0.f, "int_nearest"} };
                int w = 0, h = 0; SDL_GL_GetDrawableSize(gApp.win, &w, &h);
                gh_log("GLMATRIX start drawable=%dx%d frame=%dx%d vsync=%d", w, h, gApp.fbw, gApp.fbh, (int)gApp.vsync);
                for (unsigned i = 0; i < sizeof(cs) / sizeof(cs[0]); i++) {
                    gApp.fit = cs[i].fit; gApp.shader = cs[i].sh; gApp.curve = cs[i].cv; SDL_Delay(250);
                    snprintf(gApp.shot_path, sizeof(gApp.shot_path), "%s\\gl_%s_%s.png", gApp.outdir.c_str(), gApp.prof_.id, cs[i].tag);
                    gApp.shot_req = 1; for (int t = 0; t < 400 && gApp.shot_req; t++) SDL_Delay(5);
                    gh_log("GLMATRIX %s done=%d", cs[i].tag, gApp.shot_req ? 0 : 1);
                }
                gApp.fit = FIT_43; gApp.shader = SH_SHARP; gApp.curve = 0;
            }
            static unsigned int next_scr = 0;
            if (next_scr < gApp.scrshots_ms.size() && since >= gApp.scrshots_ms[next_scr]) { gApp.scr_req = true; next_scr++; }
            if (gApp.setvol_b >= 0 && gApp.boot_tick && !gApp.vol_forced && since >= 3000) { gApp.vol_forced = true; ui::Settings sv = gApp.ui_.effective(gApp.cur_game); sv.bgm = gApp.setvol_b; sv.se = gApp.setvol_s; gh_apply_volume(sv); }
            if (gApp.rms_from && !gApp.rms_started && since >= gApp.rms_from) { gApp.rms_started = true; gensei_out_sumsq = 0; gensei_out_n = 0; gh_log("RMS window start @%u ms", since); }
            static unsigned int next_shot = 0;
            if (next_shot < gApp.shots_ms.size() && since >= gApp.shots_ms[next_shot]) { gh_dump_shot(gApp.shots_ms[next_shot]); next_shot++; }
            { static bool n88 = false; if (!n88 && (unsigned)SegValue(cs) == 0xE800u && (reg_eip & 0xffffu) < 0x10u) { n88 = true;   /* t43k (#334 A2): N88-BASIC ROM 진입 = PC 판 미지원 — 안내 후 정상 종료 */
                gh_log("N88: E800:%04x 도달 (t=%u) — 이 작품은 N88-BASIC 필요: PC 판 미지원(노바 np2kai 경로 예정)", (unsigned)(reg_eip & 0xffffu), since); if (!gApp.headless) { plat::msgbox_utf8(phr_("이 작품은 N88-BASIC 필요:\nPC 판 미지원(노바 np2kai 경로 예정)"), 0, 256); } GH_QUITSET(); } }
            static unsigned int last_stat = 0;
            if (gApp.dumpcode && since / 2000 != last_stat && since > 12000) {
                PhysPt base = SegPhys(cs) + (reg_eip & (cpu.code.big ? 0xffffffffu : 0xffffu)); char hx[256]; int n = 0;
                for (int i = -16; i < 32; i++) n += snprintf(hx + n, sizeof hx - n, "%02x%s", (unsigned)mem_readb(base + i), i == -1 ? "|" : " ");
                gh_log("dumpcode: CS=%04x EIP=%08x base=%08x pmode=%d big=%d IF=%d | AX=%08x BX=%08x CX=%08x DX=%08x SI=%08x DI=%08x BP=%08x SP=%08x DS=%04x ES=%04x SS=%04x | PICm=%02x PICs=%02x", (unsigned)SegValue(cs), (unsigned)reg_eip, (unsigned)base, (int)cpu.pmode, (int)cpu.code.big, (int)GETFLAG(IF), (unsigned)reg_eax, (unsigned)reg_ebx, (unsigned)reg_ecx, (unsigned)reg_edx, (unsigned)reg_esi, (unsigned)reg_edi, (unsigned)reg_ebp, (unsigned)reg_esp, (unsigned)SegValue(ds), (unsigned)SegValue(es), (unsigned)SegValue(ss), (unsigned)IO_ReadB(0x02), (unsigned)IO_ReadB(0x0a));
                gh_log("dumpcode bytes (-16..+31, '|' = CS:IP 직전): %s", hx);
                { char st[256]; int m = 0; PhysPt sp0 = SegPhys(ss) + (reg_esp & 0xffffu);
                  for (int i = 0; i < 12; i++) m += snprintf(st + m, sizeof st - m, "%04x ", (unsigned)mem_readw(sp0 + i * 2));
                  char iv[400]; int k = 0; static const int vs[] = {0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x12,0x13,0x14,0x1b};
                  for (unsigned i = 0; i < sizeof(vs) / sizeof(vs[0]); i++) k += snprintf(iv + k, sizeof iv - k, "%02x=%04x:%04x ", vs[i], (unsigned)mem_readw((PhysPt)(vs[i] * 4 + 2)), (unsigned)mem_readw((PhysPt)(vs[i] * 4)));
                  unsigned tnz = 0; for (unsigned i = 0; i < 80 * 25; i++) if (mem_readw((PhysPt)(0xA0000 + i * 2)) != 0) tnz++;
                  gh_log("dumpcode2: stack[ss:sp]=%s | ivt %s| textVRAM nonzero=%u gdc0.disp=%d gdc1.disp=%d [0827]@DS=%02x", st, iv, tnz, (int)pc98_gdc[0].display_enable, (int)pc98_gdc[1].display_enable, (unsigned)mem_readb(SegPhys(ds) + 0x827)); { extern uint8_t MOUSE_IRQ, p7fd8_8255_mouse_int_enable; extern unsigned int pc98_mouse_rate_hz; gh_log("dumpcode3: MOUSE_IRQ=%d mouse_int_enable=%d rate=%uHz", (int)MOUSE_IRQ, (int)p7fd8_8255_mouse_int_enable, pc98_mouse_rate_hz); } }
            }
            if (gApp.trace21 && since > 3000) {   /* r021 진단: 프레임마다 변화만 로그 (각 종류 상한) */
                extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y, p7fd9_8255_mouse_x_latch, p7fd9_8255_mouse_y_latch; extern uint8_t p7fd9_8255_mouse_sel, p7fd9_8255_mouse_latch;
                static unsigned nm = 0, np = 0, nw = 0; static int ls = -1, ll = -1, llx = 999, lly = 999, lpm = -1, lps = -1, lw = -1; static bool d1 = false, d2 = false, d3 = false;
                if (nm < 600 && ((int)p7fd9_8255_mouse_sel != ls || (int)p7fd9_8255_mouse_latch != ll || (int)p7fd9_8255_mouse_x_latch != llx || (int)p7fd9_8255_mouse_y_latch != lly)) {
                    ls = p7fd9_8255_mouse_sel; ll = p7fd9_8255_mouse_latch; llx = p7fd9_8255_mouse_x_latch; lly = p7fd9_8255_mouse_y_latch; nm++;
                    gh_log("tr21 mouse: t=%u sel=%d latch=%d latchx=%d latchy=%d x=%d y=%d CS:IP=%04x:%04x", since, ls, ll, llx, lly, (int)p7fd9_8255_mouse_x, (int)p7fd9_8255_mouse_y, (unsigned)SegValue(cs), (unsigned)reg_eip); }
                { int pm = (int)IO_ReadB(0x02), ps = (int)IO_ReadB(0x0a);
                  if (np < 300 && (pm != lpm || ps != lps)) { lpm = pm; lps = ps; np++; gh_log("tr21 pic: t=%u mask m=%02x s=%02x CS:IP=%04x:%04x", since, pm, ps, (unsigned)SegValue(cs), (unsigned)reg_eip); } }
                if (gApp.watch_off >= 0) { PhysPt wp = SegPhys(ds) + (PhysPt)gApp.watch_off; int wv = (int)mem_readb(wp);
                  if (nw < 300 && wv != lw) { nw++; gh_log("tr21 watch: t=%u DS:%04x=%02x (전 %d) DS=%04x phys=%08x CS:IP=%04x:%04x", since, gApp.watch_off, wv, lw, (unsigned)SegValue(ds), (unsigned)wp, (unsigned)SegValue(cs), (unsigned)reg_eip); lw = wv; } }
                if (!d1 && since > 14000) { d1 = true; gh_tr21_dump_vectors("14s"); }
                if (!d2 && since > 24000) { d2 = true; gh_tr21_dump_vectors("24s"); }
                if (!d3 && since > 40000) { d3 = true; gh_tr21_dump_vectors("40s"); }
            }
            if (gApp.int18trace && since >= 5000) { static bool i18 = false; if (!i18) { i18 = true; gh_install_int18trace(); } }
            if (gApp.segdump_ms && since >= gApp.segdump_ms) {   /* t30 (#267): 대기 루프를 역어셈블하려고 현재 세그먼트 4개를 통째로 저장 */
                static bool sd = false; if (!sd) { sd = true;
                    struct SG { const char * n; PhysPt b; uint16_t sel; } sgs[4] = { { "cs", SegPhys(cs), (uint16_t)SegValue(cs) }, { "ds", SegPhys(ds), (uint16_t)SegValue(ds) }, { "es", SegPhys(es), (uint16_t)SegValue(es) }, { "ss", SegPhys(ss), (uint16_t)SegValue(ss) } };
                    gh_log("segdump: t=%u pmode=%d big=%d CS:EIP=%04x:%08x DS=%04x ES=%04x SS=%04x ESP=%08x", since, (int)cpu.pmode, (int)cpu.code.big, (unsigned)SegValue(cs), (unsigned)reg_eip, (unsigned)SegValue(ds), (unsigned)SegValue(es), (unsigned)SegValue(ss), (unsigned)reg_esp);
                    for (auto & s : sgs) { std::string fn = gApp.outdir + "\\segdump_" + s.n + ".bin"; FILE * f = fopen(fn.c_str(), "wb"); if (!f) continue; std::vector<uint8_t> buf(0x10000); for (unsigned i = 0; i < 0x10000; i++) buf[i] = mem_readb((PhysPt)(s.b + i)); fwrite(buf.data(), 1, buf.size(), f); fclose(f); gh_log("segdump: %s sel=%04x base=%08x -> %s", s.n, (unsigned)s.sel, (unsigned)s.b, fn.c_str()); }
                    { std::string fn = gApp.outdir + "\\segdump_low1m.bin"; FILE * f = fopen(fn.c_str(), "wb"); if (f) { std::vector<uint8_t> buf(0x100000); for (unsigned i = 0; i < 0x100000; i++) buf[i] = mem_readb((PhysPt)i); fwrite(buf.data(), 1, buf.size(), f); fclose(f); gh_log("segdump: low 1MB -> %s", fn.c_str()); } } } }
            if (gApp.hist_to && since >= gApp.hist_from) {   /* t30: 이름 입력 화면에서 CPU 가 어디를 도는가 */
                static std::map<uint64_t, unsigned> hh; static bool hp = false; static unsigned hn = 0;
                if (since <= gApp.hist_to) { hh[((uint64_t)(unsigned)SegValue(cs) << 32) | (uint64_t)reg_eip]++; hn++; }
                else if (!hp) { hp = true; std::vector<std::pair<unsigned, uint64_t>> v; for (auto & kv2 : hh) v.push_back({ kv2.second, kv2.first }); std::sort(v.begin(), v.end(), [](const std::pair<unsigned, uint64_t> & a, const std::pair<unsigned, uint64_t> & b) { return a.first > b.first; });
                    gh_log("hist: 표본 %u개, 서로 다른 CS:EIP %u개 (구간 %u~%u ms)", hn, (unsigned)hh.size(), gApp.hist_from, gApp.hist_to);
                    for (size_t i = 0; i < v.size() && i < 60; i++) gh_log("hist #%02u %5u회 %04x:%08x", (unsigned)i, v[i].first, (unsigned)(v[i].second >> 32), (unsigned)(v[i].second & 0xffffffffu)); } }
            if (gApp.keytrace) {   /* t29 (#266): 게임이 키를 읽는가 — BDA 키 버퍼/포인터/키 상태(0x502~0x541) 변화와 PIC 마스크(2초마다) */
                static uint8_t kprev[0x40]; static bool kinit = false; static unsigned knl = 0, kst = 0; uint8_t kc[0x40]; for (int i = 0; i < 0x40; i++) kc[i] = mem_readb((PhysPt)(0x502 + i));
                if (!kinit || memcmp(kprev, kc, sizeof kc) != 0) { if (knl < 400) { knl++; char b[160]; int k = 0; for (int i = 0x22; i < 0x28; i++) k += snprintf(b + k, sizeof b - k, "%02x ", kc[i]); char b2[160]; int k2 = 0; for (int i = 0x28; i < 0x3a; i++) k2 += snprintf(b2 + k2, sizeof b2 - k2, "%02x", kc[i]);
                        gh_log("keytrace: t=%u head/tail/cnt[524..529]=%s buf[502..]=%02x%02x%02x%02x%02x%02x%02x%02x state[52a..]=%s CS:IP=%04x:%04x", since, b, kc[0], kc[1], kc[2], kc[3], kc[4], kc[5], kc[6], kc[7], b2, (unsigned)SegValue(cs), (unsigned)reg_eip); } memcpy(kprev, kc, sizeof kc); kinit = true; }
                if (since / 2000 != kst) { kst = since / 2000; gh_log("keytrace pic: t=%u master mask=%02x slave mask=%02x (IRQ1=키보드: 마스크 비트1)", since, (unsigned)IO_ReadB(0x02), (unsigned)IO_ReadB(0x0a)); } }
            if (gApp.mouse_game && since / 2000 != last_stat) { extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y, p7fd9_8255_mouse_x_latch, p7fd9_8255_mouse_y_latch; extern uint8_t p7fd9_8255_mouse_sel, p7fd9_8255_mouse_latch; gh_log("mousereg: x=%d y=%d latchx=%d latchy=%d sel=%u latch=%u", (int)p7fd9_8255_mouse_x, (int)p7fd9_8255_mouse_y, (int)p7fd9_8255_mouse_x_latch, (int)p7fd9_8255_mouse_y_latch, (unsigned)p7fd9_8255_mouse_sel, (unsigned)p7fd9_8255_mouse_latch); }   /* r021 진단: 게임이 버스마우스 레지스터를 읽어 가는가 */
            if (since / 2000 != last_stat) { last_stat = since / 2000; gh_log("stat: wall=%u ms game=%u ms (ratio %.3f) frames=%llu starts=%llu aborts=%llu cycles=%d auto=%d pmode=%d pct=%d CS:IP=%04x:%04x", since, (unsigned)(PIC_Ticks - gApp.pic0), since ? (double)(PIC_Ticks - gApp.pic0) / (double)since : 0.0, gApp.frames, gApp.starts, gApp.aborts, (int)CPU_CycleMax, (int)CPU_CycleAutoAdjust, (int)cpu.pmode, (int)CPU_CyclePercUsed, SegValue(cs), (unsigned)reg_ip);
                gh_log("audio: dev='%s' freq=%d blk=%d cb=%u underrun=%u drop=%u", gensei_audio_dev ? gensei_audio_dev : "(default)", gensei_audio_obt_freq, gensei_audio_obt_samples, gensei_mix_cb, gensei_mix_under, gensei_mix_drop); gh_log("OUTRMS samples=%llu rms=%.2f (창 시작 %u ms)", gensei_out_n, gensei_out_n ? sqrt(gensei_out_sumsq / (double)gensei_out_n) : 0.0, gApp.rms_from); }
            if (gApp.run_ms && since >= gApp.run_ms) { gh_log("run time reached"); GH_QUITSET(); }
        }
        if (gApp.inwin_wait) {   /* t51a: 창 안 전환 뒤 부팅 도달 신호(g_boot_tick) 대기 — 8초 안에 없으면 실패로 보고 나간다(폴백은 gh_inwin_cycle) */
            if (gApp.boot_tick) { gApp.inwin_wait = false; }
            else if (SDL_GetTicks() - gApp.inwin_t0 > 8000u) { gApp.inwin_wait = false; gApp.inwin_timeout = true; GH_QUITSET(); }
        }
        if (gApp.quit) throw (int)0;
    }
}

