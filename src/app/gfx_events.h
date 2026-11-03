/* app/gfx_events.h -- S9b: GFX_Events. Moved unchanged from gensei_host.cpp lines 1795-1992 (as of t54t). Included in place; single translation unit. */
void GFX_Events(void) {
    static unsigned int last_ev = 0;
    unsigned int now = SDL_GetTicks();
    kh_input_tick();
    if (now != last_ev) {
        last_ev = now;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (gh_evpre(e, -1)) continue;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) kbov::release_all();   /* t46x: 창 포커스를 잃으면 잠긴 키 포함 전부 해제 */
            if (e.type == SDL_KEYDOWN && (e.key.keysym.mod & KMOD_ALT) && !g_headless && g_win) {   /* r010 화면 단축키(Alt+…): PC-98 에는 Alt 가 없어 게임 키와 겹치지 않음 */
                SDL_Keycode k = e.key.keysym.sym; bool hit = true;
                if (k == SDLK_RETURN) { bool fs = (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP; SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); SDL_ShowCursor(fs ? SDL_ENABLE : SDL_DISABLE); gh_log("SCREEN fullscreen=%d", fs ? 0 : 1); gh_osd(fs ? phr_("창 모드") : phr_("전체화면")); }
                else if (k == SDLK_1) { g_fit = FIT_43; gh_log("SCREEN fit=4:3"); gh_osd(phr_("화면 4:3")); }
                else if (k == SDLK_2) { g_fit = FIT_1610; gh_log("SCREEN fit=16:10"); gh_osd(phr_("화면 16:10")); }
                else if (k == SDLK_3) { g_fit = FIT_INT; gh_log("SCREEN fit=integer"); gh_osd(phr_("화면 정수배")); }
                else if (k == SDLK_m) { g_mouse_free = !g_mouse_free; gh_log("HOTKEY 마우스 가둠 %s", g_mouse_free ? "해제" : "복귀"); gh_osd(g_mouse_free ? phr_("마우스 가둠 해제 (Alt+M 으로 다시 가둠)") : phr_("마우스 가둠 복귀")); }   /* t43m: 가둠 해제키 */
                else if (k == SDLK_s) { g_shader = (g_shader + 1) % SH_N; gh_log("SCREEN shader=%d", (int)g_shader); static const char * const sn[SH_N] = { "nearest", "sharp", "crt" }; gh_osd(std::string(phr_("셰이더 ")) + sn[g_shader % SH_N]); }
                else if (k == SDLK_UP) { float c = g_curve + 0.01f; g_curve = c > 0.10f ? 0.10f : c; gh_log("SCREEN curve=%.2f", (double)g_curve); { char b[32]; snprintf(b, sizeof b, "곡면 %.2f", (double)g_curve); gh_osd(b); } }
                else if (k == SDLK_DOWN) { float c = g_curve - 0.01f; g_curve = c < 0.0f ? 0.0f : c; gh_log("SCREEN curve=%.2f", (double)g_curve); { char b[32]; snprintf(b, sizeof b, "곡면 %.2f", (double)g_curve); gh_osd(b); } }
                else if (k == SDLK_F5) { if (g_boot_tick) { std::string p = gh_slot_path(); bool ok = gh_do_save(p, false); gh_log("HOTKEY save slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 에 저장했습니다") : phr_("저장에 실패했습니다"), 2000); } }
                else if (k == SDLK_F9) { if (g_boot_tick) { std::string p = gh_slot_path(); bool ok = gh_do_load(p); gh_log("HOTKEY load slot01 %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 1 을 불러왔습니다") : phr_("불러오기에 실패했습니다"), 2000); } }
                else if (k == SDLK_k) { if (!g_headless && g_use_gl && g_boot_tick) gh_kb_toggle(); }   /* t46x: Alt+K = 화면 키보드 켜고 끄기 */
                else if (k == SDLK_p) { if (g_boot_tick) gh_bk_pause_set(!g_bk_paused, "Alt+P"); }   /* t47a: Alt+P = 자동 입력 일시정지/재개 */
                else if (k == SDLK_LEFTBRACKET || k == SDLK_RIGHTBRACKET) gh_wm_hotkey(e);
                else hit = false;
                if (hit) continue;
            }
            if (gh_kbd_event(e)) continue;   /* t46x: 화면 키보드·토글 버튼(메뉴 > 키보드 > 게임 순서) */
            if (!g_menu_open && gh_game_event(e)) continue;
            if (gensei_input_event(e)) continue;
            if (e.type == SDL_QUIT) { gh_log("SDL_QUIT"); g_quit = true; }
        }
        if (!g_headless) gh_wm_poll();
        { extern bool user_cursor_locked; bool lk = g_mouse_game && g_boot_tick && !g_menu_open; if (lk != user_cursor_locked) { user_cursor_locked = lk; gh_log("mouse: 코어 user_cursor_locked=%d (PC-98 버스마우스는 이 값이 참이어야 이동량을 받는다)", (int)lk); } }   /* r021: 코어 Mouse_CursorMoved 는 user_cursor_locked 일 때만 pc98_mouse_movement_apply 를 부른다 (r020 에서 커서가 안 움직인 원인) */
        if (!g_headless && g_win) { bool want = g_mouse_game && g_boot_tick && !g_menu_open && !g_mouse_free && !kbov::g_vis; if (want != g_relmouse) { g_relmouse = want; SDL_SetRelativeMouseMode(want ? SDL_TRUE : SDL_FALSE); gh_log("mouse: SDL 상대 마우스 모드 %s", want ? "켬" : "끔"); } }
        if (g_menu_req) { g_menu_req = false; gh_menu_run(); }
        if (g_boot_tick) {
            unsigned int since = now - g_boot_tick;
            if (!g_bk_started) { g_bk_started = true; g_bk_last = now; g_bk_t = since; }   /* t47a: 자동 입력 시계 — 일시정지·메뉴 시간은 빼고 흐른다 */
            { int dd = (int)(now - g_bk_last); if (dd > 0) { if (!g_bk_paused) g_bk_t += (unsigned)dd; g_bk_last = now; } }
            if (g_bkp_to && !g_bkp_a && since >= g_bkp_from) { g_bkp_a = true; gh_bk_pause_set(true, "CLI"); }
            if (g_bkp_to && !g_bkp_b && since >= g_bkp_to) { g_bkp_b = true; gh_bk_pause_set(false, "CLI"); }

            if (g_play_last && !g_menu_open) g_play_ms += now - g_play_last; g_play_last = now;
            while (g_next_mouse < g_amouse.size() && since >= g_amouse[g_next_mouse].at_ms) {   /* r020: 시험용 자동 마우스 — SDL 이벤트로 밀어 넣어 실제 입력 경로(gh_game_event)를 지나게 한다 */
                const GMouse & m = g_amouse[g_next_mouse]; SDL_Event se; memset(&se, 0, sizeof se);
                if (m.act == 0 || m.dx != 0 || m.dy != 0) { se.type = SDL_MOUSEMOTION; se.motion.xrel = (int)m.dx; se.motion.yrel = (int)m.dy; SDL_PushEvent(&se); memset(&se, 0, sizeof se); }
                if (m.act) { se.type = (m.act == 1 || m.act == 3) ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; se.button.button = (m.act <= 2) ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT; se.button.state = (m.act == 1 || m.act == 3) ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&se); }
                gh_log("automouse #%u at %u ms dx=%.0f dy=%.0f act=%d", (unsigned)g_next_mouse, since, (double)m.dx, (double)m.dy, m.act); g_next_mouse++;
            }
            while (g_next_inj < g_inj.size() && since + g_inj_base >= g_inj[g_next_inj].at) { gh_inj_run(g_inj[g_next_inj], since + g_inj_base); g_next_inj++; }   /* t48e (#648): --inject · t51a: 창 안 전환을 건너도 시험 시계가 이어지게 g_inj_base 를 더한다 */
            kbov::tick();   /* t52f: 빠른 불러오기 길게 누름(0.6초) 판정 */
            if (!g_headless && g_win && g_use_gl) gh_kbtest_tick(since);   /* t46x 시험 전용(옵션이 없으면 아무 일도 안 함) */
            kh_vmouse_pump();   /* t34: 호스트 마우스 이동량을 20ms 마다 ±100 이하로 코어에 */
            if (g_key_down != KBD_NONE && now >= g_key_release_at) { KEYBOARD_AddKey(g_key_down, false); g_key_down = KBD_NONE; }
            if (!g_bk_paused && g_next_bk < g_prof.bootkeys.size() && g_bk_t >= g_prof.bootkeys[g_next_bk].at_ms && g_key_down == KBD_NONE) {   /* t36cd: bootkeys — 실행 항목이 정한 부팅 후 키·명령 */
                g_key_down = g_prof.bootkeys[g_next_bk].key; g_key_release_at = now + g_keyhold;
                KEYBOARD_AddKey(g_key_down, true);
                gh_log("bootkey #%u at %u ms", (unsigned)g_next_bk, g_bk_t); g_next_bk++; }
            { size_t nst = 0; for (const GKey & kk : g_prof.bootkeys) if ((size_t)kk.step > nst) nst = (size_t)kk.step;   /* t47a: 단계 OSD 줄(번호 = 편집 화면 단계 번호, 아직 안 나간 첫 키의 단계) */
              std::string bs;
              if (nst && (g_bk_osd_on || g_bk_paused)) {
                  if (g_next_bk < g_prof.bootkeys.size()) { const GKey & kk = g_prof.bootkeys[g_next_bk]; int rem = (int)kk.at_ms - (int)g_bk_t; if (rem < 0) rem = 0; char bb[96]; snprintf(bb, sizeof bb, "자동입력 %d/%u · 다음 %d.%d초 ", kk.step, (unsigned)nst, rem / 1000, (rem % 1000) / 100); bs = std::string(bb) + kk.lab; }
                  else if (g_bk_osd_on) { if (!g_bk_done_at) g_bk_done_at = now ? now : 1; if (now - g_bk_done_at < 3000) bs = phr_("자동입력 완료 ") + std::to_string(nst) + "/" + std::to_string(nst); }
                  if (g_bk_paused && !bs.empty()) bs = phr_("[일시정지] ") + bs;
              }
              g_bk_osd = bs; }

            if (g_autokeys && g_next_key < g_prof.keys.size() && since >= g_prof.keys[g_next_key].at_ms && g_key_down == KBD_NONE) {
                g_key_down = g_prof.keys[g_next_key].key; g_key_release_at = now + g_keyhold;
                KEYBOARD_AddKey(g_key_down, true);
                gh_log("autokey #%u at %u ms", g_next_key, since);
                g_next_key++;
            }
            if (g_save_ms && !g_save_done && since >= g_save_ms) {
                g_save_done = true;
                gh_dump_named("pre_save", since);
                gh_do_save(g_state_path.empty() ? gh_slot_path() : g_state_path, true);
                g_ev_tick = now ? now : 1;
            }
            if (g_load_ms && !g_load_done && since >= g_load_ms) {
                g_load_done = true;
                gh_dump_named("pre_load", since);
                gh_do_load(g_state_path.empty() ? gh_slot_path() : g_state_path);
                g_ev_tick = now ? now : 1;
            }
            if (g_resume_slot && since >= 2000) {   /* r012: UI 에서 「이어하기」로 시작 */
                int sl = g_resume_slot - 1; g_resume_slot = 0;
                bool ok = gh_do_load(gh_slot_n(g_cur_game, sl)); gh_log("RESUME slot %d %s", sl + 1, ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("슬롯 ") + std::to_string(sl + 1) + phr_(" 에서 이어합니다") : std::string(phr_("이어하기에 실패했습니다")), 2000);
            }
            if (g_bt_x) {   /* 시험: 배속 비율 실측 (--boosttest 시작초,길이초,배율) */
                static unsigned w0 = 0, p0 = 0; static int ph = 0; static unsigned lastlog = 0;
                if (ph == 0 && since >= g_bt_at) { ph = 1; w0 = now; p0 = (unsigned)PIC_Ticks; for (int i = 0; i < 5; i++) if (ui::Settings().boost_x() >= 0 && (i == 0 ? 2 : i == 1 ? 3 : i == 2 ? 4 : i == 3 ? 6 : 8) == g_bt_x) g_ui.g.bmult = i; g_ui.g.bmode = 2; g_boost_hold = true; gh_boost_update(); gh_log("BOOSTTEST start wall=%u game=%u x=%d", since, p0, g_bt_x); }
                if (ph == 1 && now - lastlog >= 500) { lastlog = now; gh_log("BOOSTTEST wall+%u game+%u", now - w0, (unsigned)PIC_Ticks - p0); }
                if (ph == 1 && now - w0 >= g_bt_len) { ph = 2; unsigned dw = now - w0, dg = (unsigned)PIC_Ticks - p0; g_boost_hold = false; gh_boost_update(); gh_log("BOOSTTEST done wall=%u game=%u ratio=%.3f", dw, dg, dw ? (double)dg / dw : 0.0); }
            }
            if (g_swap_ms && !g_swap_done && since >= g_swap_ms) {   /* 시험: 실행 중 디스크 교체 */
                g_swap_done = true; auto fi = g_fdfiles.find(g_prof.id); bool ok = false;
                if (fi != g_fdfiles.end() && g_swap_idx >= 0 && g_swap_idx < (int)fi->second.size()) ok = gh_attach(g_swap_drv, fi->second[g_swap_idx].c_str(), true);
                gh_log("SWAPTEST drive=%d idx=%d %s", g_swap_drv, g_swap_idx, ok ? "OK" : "FAIL");
            }
            if (g_axistest) {   /* 시험: R2 축 이벤트를 밀어 넣어 배속 on/off 와 로그를 확인 */
                static int ph = 0; SDL_Event ev; SDL_zero(ev); ev.type = SDL_CONTROLLERAXISMOTION; ev.caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
                if (ph == 0 && since >= 2000) { ph = 1; ev.caxis.value = 30000; SDL_PushEvent(&ev); gh_log("AXISTEST push 30000"); }
                if (ph == 1 && since >= 3500) { ph = 2; ev.caxis.value = 0; SDL_PushEvent(&ev); gh_log("AXISTEST push 0"); }
            }
            if (g_caps_len) {   /* 시험: CAPS(빨리 넘김) 누름 (--capstest 시작초,길이초) */
                static int ph = 0;
                if (ph == 0 && since >= g_caps_at) { ph = 1; KEYBOARD_AddKey(KBD_capslock, true); gh_log("CAPSTEST down at %u game=%u", since, (unsigned)(PIC_Ticks - g_pic0)); }
                if (ph == 1 && since >= g_caps_at + g_caps_len) { ph = 2; KEYBOARD_AddKey(KBD_capslock, false); gh_log("CAPSTEST up at %u game=%u", since, (unsigned)(PIC_Ticks - g_pic0)); }
            }
            if (g_ev_tick) {
                static const unsigned offs[3] = { 100, 1000, 3000 };
                static unsigned next_post = 0;
                if (next_post < 3 && now - g_ev_tick >= offs[next_post]) { gh_dump_named(g_save_done && !g_load_done ? "post_save" : "post_load", offs[next_post]); next_post++; }
            }
            if (g_glmx_ms && !g_glmx_done && g_use_gl && since >= g_glmx_ms) {   /* r010 시험: 화면 설정 조합별 렌더 결과를 GL 에서 읽어 PNG 로 */
                g_glmx_done = true;
                struct C { int fit, sh; float cv; const char * tag; };
                static const C cs[] = { {FIT_43, SH_SHARP, 0.f, "fit43_sharp"}, {FIT_43, SH_CRT, 0.f, "fit43_crt_flat"}, {FIT_43, SH_CRT, 0.04f, "fit43_crt_curve004"},
                                        {FIT_43, SH_NEAREST, 0.f, "fit43_nearest"}, {FIT_1610, SH_SHARP, 0.f, "fit1610_sharp"}, {FIT_INT, SH_NEAREST, 0.f, "int_nearest"} };
                int w = 0, h = 0; SDL_GL_GetDrawableSize(g_win, &w, &h);
                gh_log("GLMATRIX start drawable=%dx%d frame=%dx%d vsync=%d", w, h, g_fbw, g_fbh, (int)g_vsync);
                for (unsigned i = 0; i < sizeof(cs) / sizeof(cs[0]); i++) {
                    g_fit = cs[i].fit; g_shader = cs[i].sh; g_curve = cs[i].cv; SDL_Delay(250);
                    snprintf(g_shot_path, sizeof(g_shot_path), "%s\\gl_%s_%s.png", g_outdir.c_str(), g_prof.id, cs[i].tag);
                    g_shot_req = 1; for (int t = 0; t < 400 && g_shot_req; t++) SDL_Delay(5);
                    gh_log("GLMATRIX %s done=%d", cs[i].tag, g_shot_req ? 0 : 1);
                }
                g_fit = FIT_43; g_shader = SH_SHARP; g_curve = 0;
            }
            static unsigned int next_scr = 0;
            if (next_scr < g_scrshots_ms.size() && since >= g_scrshots_ms[next_scr]) { g_scr_req = true; next_scr++; }
            if (g_setvol_b >= 0 && g_boot_tick && !g_vol_forced && since >= 3000) { g_vol_forced = true; ui::Settings sv = g_ui.effective(g_cur_game); sv.bgm = g_setvol_b; sv.se = g_setvol_s; gh_apply_volume(sv); }
            if (g_rms_from && !g_rms_started && since >= g_rms_from) { g_rms_started = true; gensei_out_sumsq = 0; gensei_out_n = 0; gh_log("RMS window start @%u ms", since); }
            static unsigned int next_shot = 0;
            if (next_shot < g_shots_ms.size() && since >= g_shots_ms[next_shot]) { gh_dump_shot(g_shots_ms[next_shot]); next_shot++; }
            { static bool n88 = false; if (!n88 && (unsigned)SegValue(cs) == 0xE800u && (reg_eip & 0xffffu) < 0x10u) { n88 = true;   /* t43k (#334 A2): N88-BASIC ROM 진입 = PC 판 미지원 — 안내 후 정상 종료 */
                gh_log("N88: E800:%04x 도달 (t=%u) — 이 작품은 N88-BASIC 필요: PC 판 미지원(노바 np2kai 경로 예정)", (unsigned)(reg_eip & 0xffffu), since); if (!g_headless) { plat::msgbox_utf8(phr_("이 작품은 N88-BASIC 필요:\nPC 판 미지원(노바 np2kai 경로 예정)"), 0, 256); } g_quit = true; } }
            static unsigned int last_stat = 0;
            if (g_dumpcode && since / 2000 != last_stat && since > 12000) {
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
            if (g_trace21 && since > 3000) {   /* r021 진단: 프레임마다 변화만 로그 (각 종류 상한) */
                extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y, p7fd9_8255_mouse_x_latch, p7fd9_8255_mouse_y_latch; extern uint8_t p7fd9_8255_mouse_sel, p7fd9_8255_mouse_latch;
                static unsigned nm = 0, np = 0, nw = 0; static int ls = -1, ll = -1, llx = 999, lly = 999, lpm = -1, lps = -1, lw = -1; static bool d1 = false, d2 = false, d3 = false;
                if (nm < 600 && ((int)p7fd9_8255_mouse_sel != ls || (int)p7fd9_8255_mouse_latch != ll || (int)p7fd9_8255_mouse_x_latch != llx || (int)p7fd9_8255_mouse_y_latch != lly)) {
                    ls = p7fd9_8255_mouse_sel; ll = p7fd9_8255_mouse_latch; llx = p7fd9_8255_mouse_x_latch; lly = p7fd9_8255_mouse_y_latch; nm++;
                    gh_log("tr21 mouse: t=%u sel=%d latch=%d latchx=%d latchy=%d x=%d y=%d CS:IP=%04x:%04x", since, ls, ll, llx, lly, (int)p7fd9_8255_mouse_x, (int)p7fd9_8255_mouse_y, (unsigned)SegValue(cs), (unsigned)reg_eip); }
                { int pm = (int)IO_ReadB(0x02), ps = (int)IO_ReadB(0x0a);
                  if (np < 300 && (pm != lpm || ps != lps)) { lpm = pm; lps = ps; np++; gh_log("tr21 pic: t=%u mask m=%02x s=%02x CS:IP=%04x:%04x", since, pm, ps, (unsigned)SegValue(cs), (unsigned)reg_eip); } }
                if (g_watch_off >= 0) { PhysPt wp = SegPhys(ds) + (PhysPt)g_watch_off; int wv = (int)mem_readb(wp);
                  if (nw < 300 && wv != lw) { nw++; gh_log("tr21 watch: t=%u DS:%04x=%02x (전 %d) DS=%04x phys=%08x CS:IP=%04x:%04x", since, g_watch_off, wv, lw, (unsigned)SegValue(ds), (unsigned)wp, (unsigned)SegValue(cs), (unsigned)reg_eip); lw = wv; } }
                if (!d1 && since > 14000) { d1 = true; gh_tr21_dump_vectors("14s"); }
                if (!d2 && since > 24000) { d2 = true; gh_tr21_dump_vectors("24s"); }
                if (!d3 && since > 40000) { d3 = true; gh_tr21_dump_vectors("40s"); }
            }
            if (g_int18trace && since >= 5000) { static bool i18 = false; if (!i18) { i18 = true; gh_install_int18trace(); } }
            if (g_segdump_ms && since >= g_segdump_ms) {   /* t30 (#267): 대기 루프를 역어셈블하려고 현재 세그먼트 4개를 통째로 저장 */
                static bool sd = false; if (!sd) { sd = true;
                    struct SG { const char * n; PhysPt b; uint16_t sel; } sgs[4] = { { "cs", SegPhys(cs), (uint16_t)SegValue(cs) }, { "ds", SegPhys(ds), (uint16_t)SegValue(ds) }, { "es", SegPhys(es), (uint16_t)SegValue(es) }, { "ss", SegPhys(ss), (uint16_t)SegValue(ss) } };
                    gh_log("segdump: t=%u pmode=%d big=%d CS:EIP=%04x:%08x DS=%04x ES=%04x SS=%04x ESP=%08x", since, (int)cpu.pmode, (int)cpu.code.big, (unsigned)SegValue(cs), (unsigned)reg_eip, (unsigned)SegValue(ds), (unsigned)SegValue(es), (unsigned)SegValue(ss), (unsigned)reg_esp);
                    for (auto & s : sgs) { std::string fn = g_outdir + "\\segdump_" + s.n + ".bin"; FILE * f = fopen(fn.c_str(), "wb"); if (!f) continue; std::vector<uint8_t> buf(0x10000); for (unsigned i = 0; i < 0x10000; i++) buf[i] = mem_readb((PhysPt)(s.b + i)); fwrite(buf.data(), 1, buf.size(), f); fclose(f); gh_log("segdump: %s sel=%04x base=%08x -> %s", s.n, (unsigned)s.sel, (unsigned)s.b, fn.c_str()); }
                    { std::string fn = g_outdir + "\\segdump_low1m.bin"; FILE * f = fopen(fn.c_str(), "wb"); if (f) { std::vector<uint8_t> buf(0x100000); for (unsigned i = 0; i < 0x100000; i++) buf[i] = mem_readb((PhysPt)i); fwrite(buf.data(), 1, buf.size(), f); fclose(f); gh_log("segdump: low 1MB -> %s", fn.c_str()); } } } }
            if (g_hist_to && since >= g_hist_from) {   /* t30: 이름 입력 화면에서 CPU 가 어디를 도는가 */
                static std::map<uint64_t, unsigned> hh; static bool hp = false; static unsigned hn = 0;
                if (since <= g_hist_to) { hh[((uint64_t)(unsigned)SegValue(cs) << 32) | (uint64_t)reg_eip]++; hn++; }
                else if (!hp) { hp = true; std::vector<std::pair<unsigned, uint64_t>> v; for (auto & kv2 : hh) v.push_back({ kv2.second, kv2.first }); std::sort(v.begin(), v.end(), [](const std::pair<unsigned, uint64_t> & a, const std::pair<unsigned, uint64_t> & b) { return a.first > b.first; });
                    gh_log("hist: 표본 %u개, 서로 다른 CS:EIP %u개 (구간 %u~%u ms)", hn, (unsigned)hh.size(), g_hist_from, g_hist_to);
                    for (size_t i = 0; i < v.size() && i < 60; i++) gh_log("hist #%02u %5u회 %04x:%08x", (unsigned)i, v[i].first, (unsigned)(v[i].second >> 32), (unsigned)(v[i].second & 0xffffffffu)); } }
            if (g_keytrace) {   /* t29 (#266): 게임이 키를 읽는가 — BDA 키 버퍼/포인터/키 상태(0x502~0x541) 변화와 PIC 마스크(2초마다) */
                static uint8_t kprev[0x40]; static bool kinit = false; static unsigned knl = 0, kst = 0; uint8_t kc[0x40]; for (int i = 0; i < 0x40; i++) kc[i] = mem_readb((PhysPt)(0x502 + i));
                if (!kinit || memcmp(kprev, kc, sizeof kc) != 0) { if (knl < 400) { knl++; char b[160]; int k = 0; for (int i = 0x22; i < 0x28; i++) k += snprintf(b + k, sizeof b - k, "%02x ", kc[i]); char b2[160]; int k2 = 0; for (int i = 0x28; i < 0x3a; i++) k2 += snprintf(b2 + k2, sizeof b2 - k2, "%02x", kc[i]);
                        gh_log("keytrace: t=%u head/tail/cnt[524..529]=%s buf[502..]=%02x%02x%02x%02x%02x%02x%02x%02x state[52a..]=%s CS:IP=%04x:%04x", since, b, kc[0], kc[1], kc[2], kc[3], kc[4], kc[5], kc[6], kc[7], b2, (unsigned)SegValue(cs), (unsigned)reg_eip); } memcpy(kprev, kc, sizeof kc); kinit = true; }
                if (since / 2000 != kst) { kst = since / 2000; gh_log("keytrace pic: t=%u master mask=%02x slave mask=%02x (IRQ1=키보드: 마스크 비트1)", since, (unsigned)IO_ReadB(0x02), (unsigned)IO_ReadB(0x0a)); } }
            if (g_mouse_game && since / 2000 != last_stat) { extern int8_t p7fd9_8255_mouse_x, p7fd9_8255_mouse_y, p7fd9_8255_mouse_x_latch, p7fd9_8255_mouse_y_latch; extern uint8_t p7fd9_8255_mouse_sel, p7fd9_8255_mouse_latch; gh_log("mousereg: x=%d y=%d latchx=%d latchy=%d sel=%u latch=%u", (int)p7fd9_8255_mouse_x, (int)p7fd9_8255_mouse_y, (int)p7fd9_8255_mouse_x_latch, (int)p7fd9_8255_mouse_y_latch, (unsigned)p7fd9_8255_mouse_sel, (unsigned)p7fd9_8255_mouse_latch); }   /* r021 진단: 게임이 버스마우스 레지스터를 읽어 가는가 */
            if (since / 2000 != last_stat) { last_stat = since / 2000; gh_log("stat: wall=%u ms game=%u ms (ratio %.3f) frames=%llu starts=%llu aborts=%llu cycles=%d auto=%d pmode=%d pct=%d CS:IP=%04x:%04x", since, (unsigned)(PIC_Ticks - g_pic0), since ? (double)(PIC_Ticks - g_pic0) / (double)since : 0.0, g_frames, g_starts, g_aborts, (int)CPU_CycleMax, (int)CPU_CycleAutoAdjust, (int)cpu.pmode, (int)CPU_CyclePercUsed, SegValue(cs), (unsigned)reg_ip);
                gh_log("audio: dev='%s' freq=%d blk=%d cb=%u underrun=%u drop=%u", gensei_audio_dev ? gensei_audio_dev : "(default)", gensei_audio_obt_freq, gensei_audio_obt_samples, gensei_mix_cb, gensei_mix_under, gensei_mix_drop); gh_log("OUTRMS samples=%llu rms=%.2f (창 시작 %u ms)", gensei_out_n, gensei_out_n ? sqrt(gensei_out_sumsq / (double)gensei_out_n) : 0.0, g_rms_from); }
            if (g_run_ms && since >= g_run_ms) { gh_log("run time reached"); g_quit = true; }
        }
        if (g_inwin_wait) {   /* t51a: 창 안 전환 뒤 부팅 도달 신호(g_boot_tick) 대기 — 8초 안에 없으면 실패로 보고 나간다(폴백은 gh_inwin_cycle) */
            if (g_boot_tick) { g_inwin_wait = false; }
            else if (SDL_GetTicks() - g_inwin_t0 > 8000u) { g_inwin_wait = false; g_inwin_timeout = true; g_quit = true; }
        }
        if (g_quit) throw (int)0;
    }
}

