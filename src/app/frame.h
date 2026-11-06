/* app/frame.h -- S9a: loading cover, screenshots, event pre-hook, frame hook, RENDER_EndUpdate. Moved unchanged from gensei_host.cpp lines 1009-1158 (as of t54s). Included in place; single translation unit. */
/* ------------------------------------------------------------------ r017: 로딩 덮개 · 프레임 기록 */
static bool gh_text_has_dbx(void) {   /* 텍스트 VRAM(80x25) 에 DOSBox-X 스플래시 글자가 있는가 */
    static const char * const KW[] = { "DOSBox", "DOSBOX", "SPACEBAR", "BIOS setup", "joncampbell", "Welcome", NULL };
    char row[81];
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) { unsigned ch = mem_readw(0xA0000 + (unsigned)(y * 80 + x) * 2u); row[x] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : ' '; }
        row[80] = 0; for (int k = 0; KW[k]; k++) if (strstr(row, KW[k])) return true;
    }
    return false;
}
static void gh_loading_draw(uint32_t * px, int w, int h) {   /* 검은 바탕 + 제품명 + Now Loading... (부팅 연출과 같은 자리) */
    ov::Canvas c = { px, w, h }; c.rect(0, 0, w, h, ov::K);
    int d = (int)((SDL_GetTicks() / 400u) % 4u);
    ov::text(c, 40, 112, std::string(KH_FULLNAME), ov::WH, ov::K);
    ov::text(c, 40, 144, std::string("Now Loading") + std::string((size_t)d, '.') + std::string((size_t)(3 - d), ' '), ov::WH, ov::K);
}
static void gh_ft_sheet(void) {   /* 20 장 컨택트 시트 (5x4, 장당 320x200) */
    const int CW = 320, CH = 200, COLS = 5, ROWS = 4; std::vector<uint32_t> sheet((size_t)CW * COLS * CH * ROWS, 0x202020); ov::Canvas sc = { sheet.data(), CW * COLS, CH * ROWS };
    for (size_t i = 0; i < gApp.ft_thumbs.size() && i < (size_t)(COLS * ROWS); i++) {
        int ox = (int)(i % COLS) * CW, oy = (int)(i / COLS) * CH; const FtThumb & t = gApp.ft_thumbs[i];
        for (int y = 0; y < CH - 2; y++) for (int x = 0; x < CW - 2; x++) sheet[(size_t)(oy + y) * (CW * COLS) + ox + x] = t.px[(size_t)y * CW + x];
        sc.rect(ox, oy, 170, 18, ov::K); ov::text(sc, ox + 2, oy + 1, t.label, ov::YL, ov::K);
    }
    gh_png((gApp.ft_dir + "\\contact_sheet.png").c_str(), sheet.data(), CW * COLS, CH * ROWS);
}
/* t35 (#270): 스크린샷. g_fb(에뮬 렌더 결과, 오버레이·메뉴 없음)를 640x400 으로 저장. 프레임 완성 시점(RENDER_EndUpdate)에서만 부른다(메뉴 항목은 게임이 멈춘 상태라 즉시 호출). */
static bool gh_shot_save(void) {
    if (gApp.fb.empty() || gApp.fbw <= 0 || gApp.fbh <= 0) { gh_osd(phr_("스크린샷 저장 실패"), 1500); gh_log("SCREENSHOT: no frame"); return false; }
    std::string dir = gApp.exedir + "\\screenshots"; plat::mkdir(dir.c_str());
    plat::LocalTime st = plat::local_now(); char ts[40]; snprintf(ts, sizeof ts, "%04d%02d%02d-%02d%02d%02d", st.year, st.month, st.day, st.hour, st.minute, st.second);
    std::string base = dir + "\\" + gApp.prof_.id + "_" + ts, path = base + ".png";
    for (int n = 2; n < 100 && plat::exists(path.c_str()); n++) path = base + "_" + std::to_string(n) + ".png";
    if (!gApp.headless && gApp.use_gl && gApp.fb_mtx && gApp.win) {   /* t41: 창에 보이는 그대로(셰이더·곡면·비율) 화면 영역만 저장 */
        snprintf(gApp.shot_path, sizeof gApp.shot_path, "%s", path.c_str()); gApp.shot_w = gApp.shot_h = 0; gApp.shot_crop = 1; gApp.shot_req = 1;
        for (int t = 0; t < 600 && gApp.shot_req; t++) SDL_Delay(5);
        bool done = !gApp.shot_req; gApp.shot_crop = 0; gApp.shot_req = 0;
        plat::FileInfo fg; bool okg = done && plat::stat_file(path, fg) && fg.size_lo > 0;
        if (okg) { gh_log("SCREENSHOT OK %s (화면 설정 적용 %dx%d, shader=%d fit=%d curve=%.2f)", path.c_str(), gApp.shot_w, gApp.shot_h, (int)gApp.shader, (int)gApp.fit, (double)gApp.curve); gh_osd(phr_("스크린샷 저장됨"), 1500); return true; }
        gh_log("SCREENSHOT GL 경로 실패(done=%d) — 원본 프레임으로 대체", (int)done);
    }
    std::vector<uint32_t> out; const uint32_t * src = gApp.fb.data();
    if (gApp.fbw != 640 || gApp.fbh != 400) { out.assign(640 * 400, 0); ov::Canvas sc = { out.data(), 640, 400 }; ov::blit_scaled(sc, gApp.fb.data(), gApp.fbw, gApp.fbh, 0, 0, 640, 400); src = out.data(); }
    gh_png(path.c_str(), src, 640, 400);
    plat::FileInfo fa; bool ok = plat::stat_file(path, fa) && (fa.size_lo > 0);
    gh_log("SCREENSHOT %s %s (src %dx%d -> 640x400)", ok ? "OK" : "FAIL", path.c_str(), gApp.fbw, gApp.fbh);
    gh_osd(ok ? phr_("스크린샷 저장됨") : phr_("스크린샷 저장 실패"), 1500); return ok;
}
/* t48d (#643): first-click diagnostics (raw SDL events, hint value, mouse/keyboard focus) + window-only capture (F12 -> exedir\winshots\win_*.png, whole player window incl. menu/overlay) */
static void gh_diag_focus(const char * why) {
    SDL_Window * mf = SDL_GetMouseFocus(), * kf = SDL_GetKeyboardFocus(); const char * hv = SDL_GetHint("SDL_MOUSE_FOCUS_CLICKTHROUGH"); unsigned wf = gApp.win ? SDL_GetWindowFlags(gApp.win) : 0;
    gh_log("DIAG %s: hint CLICKTHROUGH=%s mouseFocus=%s keyFocus=%s winflags input=%d mouse=%d shown=%d minimized=%d relmouse=%d", why, hv ? hv : "(null)", mf ? (mf == gApp.win ? "g_win" : "other") : "none", kf ? (kf == gApp.win ? "g_win" : "other") : "none",
        (wf & SDL_WINDOW_INPUT_FOCUS) ? 1 : 0, (wf & SDL_WINDOW_MOUSE_FOCUS) ? 1 : 0, (wf & SDL_WINDOW_SHOWN) ? 1 : 0, (wf & SDL_WINDOW_MINIMIZED) ? 1 : 0, (int)SDL_GetRelativeMouseMode());
}
static void gh_diag_enter(int tag) { gApp.dg_n[tag] = 0; gApp.dg_m[tag] = 0; gh_log("DIAG enter loop=%s", tag ? "game-menu" : "main-menu"); gh_diag_focus("enter"); }
static bool gh_winshot(void) {
    if (gApp.headless || !gApp.use_gl || !gApp.win) { gh_log("WINSHOT: no GL window"); return false; }
    std::string dir = gApp.exedir + "\\winshots"; plat::mkdir(dir.c_str());
    plat::LocalTime st = plat::local_now(); char ts[64]; snprintf(ts, sizeof ts, "%04d%02d%02d-%02d%02d%02d_%03d", st.year, st.month, st.day, st.hour, st.minute, st.second, st.ms);
    std::string path = dir + "\\win_" + ts + ".png";
    snprintf(gApp.shot_path, sizeof gApp.shot_path, "%s", path.c_str()); gApp.shot_w = gApp.shot_h = 0; gApp.shot_crop = 0; gApp.shot_req = 1;
    for (int t = 0; t < 600 && gApp.shot_req; t++) SDL_Delay(5);
    bool done = !gApp.shot_req; gApp.shot_req = 0;
    gh_log("WINSHOT %s %s %dx%d", done ? "OK" : "FAIL", path.c_str(), gApp.shot_w, gApp.shot_h); return done;
}
/* tag: 0 = main menu loop, 1 = in-game menu loop, -1 = in-game (no diag). returns true = event consumed */
/* t49a (#817·#821·#823): 패드 연결/해제 — 메뉴 루프·게임 루프·일시정지 메뉴 어디서든 gh_evpre 가 한 곳에서 처리한다.
   입력층(gensei_input_event)이 열기/닫기·버튼 놓기를 하고, 여기서는 마지막 입력 장치 표기와 2초 알림만 얹는다 */
static void gh_hotplug_hook(bool added, const char * name) {
    ov::g_helpdev = added ? 0 : 1;   /* 연결 = 패드 표기, 해제 = 키보드 표기 */
    std::string m = added ? std::string(phr_("컨트롤러 연결됨: ")) + (name ? name : "?") : std::string(phr_("컨트롤러 해제됨"));
    gApp.note = m; gApp.note_until = SDL_GetTicks() + 2000; gh_osd(m, 2000);
    gh_log("HOTPLUG %s (%s) -> 조작 설명 표기 %s", added ? "연결" : "해제", name ? name : "?", added ? "패드" : "키보드");
}
#include "gensei_harness_t49a.h"   /* t49a: 하니스 전용 코드(별도 파일) */
static bool gh_evpre(SDL_Event & e, int tag) {
    if (kh_stk_ev(e)) return true;   /* #619/#620: stick off / deadzone, before anyone looks at the event. #634: a switched-off stick's axis events are dropped here */
    if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED) { gensei_input_event(e); return true; }   /* t49a: 장치 추가/제거는 메뉴·게임 공통으로 여기서 한 번만 */
    if (gensei_raw_event(e)) return true;   /* t48e (#651 2): 표에 없는 패드의 원시 이벤트 → 표준 이벤트로 바꿔 다시 밀어 넣음(메뉴·게임 공통) */
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F12 && !e.key.repeat) { if ((e.key.keysym.mod & KMOD_CTRL) || (SDL_GetModState() & KMOD_CTRL)) { gh_winshot(); return true; } if (tag == 0) return true; /* t49f (#858): 평범한 F12 는 창 캡처가 먹지 않고 게임 중 일시정지 메뉴(게임 루프)·일시정지 메뉴(닫기)로 통과, 창 캡처는 Ctrl+F12 */ }
    if (tag < 0 || gApp.dg_n[tag] >= 80) return false;
    if (e.type == SDL_MOUSEMOTION) { if (gApp.dg_m[tag] < 8) { gApp.dg_m[tag]++; gh_log("DIAG[%d] MOUSEMOTION x=%d y=%d xrel=%d yrel=%d state=%u which=%u", tag, e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel, (unsigned)e.motion.state, (unsigned)e.motion.which); } return false; }
    int n = ++gApp.dg_n[tag];
    switch (e.type) {
    case SDL_WINDOWEVENT: gh_log("DIAG[%d] ev#%d WINDOWEVENT id=%d d1=%d d2=%d", tag, n, (int)e.window.event, (int)e.window.data1, (int)e.window.data2); if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e.window.event == SDL_WINDOWEVENT_ENTER) gh_diag_focus("window-event"); break;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
        gh_log("DIAG[%d] ev#%d MOUSEBUTTON%s btn=%d clicks=%d x=%d y=%d which=%u state=%d", tag, n, e.type == SDL_MOUSEBUTTONDOWN ? "DOWN" : "UP", (int)e.button.button, (int)e.button.clicks, e.button.x, e.button.y, (unsigned)e.button.which, (int)e.button.state);
        if (e.type == SDL_MOUSEBUTTONDOWN) gh_diag_focus("at-click"); break;
    case SDL_KEYDOWN: case SDL_KEYUP: gh_log("DIAG[%d] ev#%d KEY%s sym=%d scan=%d", tag, n, e.type == SDL_KEYDOWN ? "DOWN" : "UP", (int)e.key.keysym.sym, (int)e.key.keysym.scancode); break;
    default: gh_log("DIAG[%d] ev#%d type=0x%x", tag, n, (unsigned)e.type); break;
    }
    return false;
}
static void gh_frame_hook(void) {
    if (!gApp.pon_tick || gApp.fb.empty()) return;
    unsigned wall = SDL_GetTicks() - gApp.pon_tick; unsigned game = (unsigned)(PIC_Ticks - gApp.pon_pic);
    size_t n = gApp.fb.size(), nbc = 0; for (size_t i = 0; i < n; i++) if (gApp.fb[i] & 0xFFFFFF) nbc++;
    unsigned nb = (unsigned)(nbc * 1000 / n); bool dbx = gh_text_has_dbx();
    gApp.frames_seen++; if (dbx) gApp.dbx_raw_frames++;
    if (gApp.loading) {
        bool end = false; const char * why = "";
        if (gApp.boot_tick && nb >= 8 && !dbx) { end = true; why = phr_("첫 게임 화면"); }
        else if (wall > 20000) { end = true; why = phr_("시간 초과(20초)"); }
        if (end) { gApp.loading = false; gApp.load_end_wall = wall; gApp.load_end_game = game; gh_log("LOADING end: %s wall=%u game=%u frame=%u nonblack=%u‰", why, wall, game, gApp.frames_seen, nb); }
    }
    bool shown_dbx = dbx && !(gApp.loading && !gApp.noload);
    if (!gApp.ft_dir.empty() && !gApp.ft_done) {
        if (!gApp.ft_csv) { gApp.ft_csv = fopen((gApp.ft_dir + "\\frames.csv").c_str(), "wb"); if (gApp.ft_csv) fprintf(gApp.ft_csv, "frame,wall_ms,game_ms,nonblack_permille,raw_dbx_text,loading_overlay,shown_dbx_text,boot_started\n"); }
        bool lo = gApp.loading && !gApp.noload;
        if (game <= gApp.ft_ms) {
            gApp.ft_frames++; if (dbx) gApp.ft_rawdbx++; if (shown_dbx) gApp.ft_shown++; if (lo) gApp.ft_load++; if (nb >= 8 && gApp.ft_first_nb == 0xFFFFFFFFu) gApp.ft_first_nb = game;
            if (gApp.ft_csv) fprintf(gApp.ft_csv, "%u,%u,%u,%u,%d,%d,%d,%d\n", gApp.frames_seen, wall, game, nb, (int)dbx, (int)lo, (int)shown_dbx, (int)(gApp.boot_tick != 0));
            if ((int)(game / 100u) >= gApp.ft_slot && gApp.ft_slot < 20) {   /* 100ms 칸마다 첫 프레임 1장: 실제로 보이는 화면(덮개 포함) */
                std::vector<uint32_t> view; if (lo) { view.assign(640 * 400, 0); gh_loading_draw(view.data(), 640, 400); } else view = gApp.fb;
                int vw = lo ? 640 : gApp.fbw, vh = lo ? 400 : gApp.fbh; char nm[64]; snprintf(nm, sizeof nm, "\\ft_%02d_t%04u.png", gApp.ft_slot, game); gh_png((gApp.ft_dir + nm).c_str(), view.data(), vw, vh);
                FtThumb t; t.px.assign(320 * 200, 0); for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) t.px[(size_t)y * 320 + x] = view[(size_t)(y * vh / 200) * vw + (x * vw / 320)];
                char lb[64]; snprintf(lb, sizeof lb, "#%d t=%.2fs dbx=%d%s", gApp.ft_slot, game / 1000.0, (int)shown_dbx, lo ? " L" : ""); t.label = lb; gApp.ft_thumbs.push_back(t); gApp.ft_slot++;
            }
        } else {
            gApp.ft_done = true; if (gApp.ft_csv) { fclose(gApp.ft_csv); gApp.ft_csv = NULL; } gh_ft_sheet();
            gh_log("FRAMETEST done: window=%ums frames=%u raw_dbx_text_frames=%u shown_dbx_text_frames=%u loading_overlay_frames=%u first_nonblack_game_ms=%d splash=%d overlay=%d", gApp.ft_ms, gApp.ft_frames, gApp.ft_rawdbx, gApp.ft_shown, gApp.ft_load, gApp.ft_first_nb == 0xFFFFFFFFu ? -1 : (int)gApp.ft_first_nb, gensei_nosplash ? 0 : 1, gApp.noload ? 0 : 1);
        }
    }
}

bool RENDER_StartUpdate(void) { gApp.cury = 0; gApp.starts++; return true; }
void RENDER_EndUpdate(bool abort) {
    if (abort) { gApp.aborts++; return; }
    gApp.frames++;
    gh_frame_hook();
    if (gApp.scr_req) { gApp.scr_req = false; gApp.scr_pend = (!gApp.headless && gApp.use_gl) ? 3 : 1; }   /* t41: GL 은 게시된 프레임이 화면에 올라간 뒤 찍는다 */
    if (!gApp.headless && gApp.use_gl && !gApp.fb.empty()) {
        SDL_LockMutex(gApp.fb_mtx);
        if (gApp.pub.size() != gApp.fb.size()) gApp.pub.resize(gApp.fb.size());
        memcpy(gApp.pub.data(), gApp.fb.data(), gApp.fb.size() * 4); gApp.pub_w = gApp.fbw; gApp.pub_h = gApp.fbh; gApp.pub_new = true;
        if (gApp.loading && !gApp.noload) { gh_loading_draw(gApp.pub.data(), gApp.fbw, gApp.fbh); gh_ovl_off(); }
        else if (!gApp.menu_open) { if (gApp.kbshader) { ov::Canvas oc = { gApp.pub.data(), gApp.fbw, gApp.fbh }; kbov::draw(gApp.pub.data(), gApp.fbw, gApp.fbh); gh_boost_osd_draw(oc, gApp.osd, gApp.osd_until && SDL_GetTicks() < gApp.osd_until); gh_ovl_off(); } else gh_ovl_update(gApp.fbw, gApp.fbh); }
        else gh_ovl_off();

        SDL_UnlockMutex(gApp.fb_mtx);
    } else if (!gApp.headless && gApp.ren && gApp.tex && !gApp.fb.empty()) {
        if (gApp.loading && !gApp.noload) { static std::vector<uint32_t> lv; lv.assign(gApp.fb.size(), 0); gh_loading_draw(lv.data(), gApp.fbw, gApp.fbh); SDL_UpdateTexture(gApp.tex, NULL, lv.data(), gApp.fbw * 4); } else { const uint32_t * sp = gApp.fb.data(); static std::vector<uint32_t> ob; bool osd_on = gApp.osd_until && SDL_GetTicks() < gApp.osd_until;
            if (!gApp.menu_open && (kbov::active() || gApp.fps_on || osd_on || (gApp.boost_on && gApp.bosd_on) || !gApp.bk_osd.empty())) { ob = gApp.fb; ov::Canvas oc = { ob.data(), gApp.fbw, gApp.fbh }; kbov::draw(ob.data(), gApp.fbw, gApp.fbh); gh_boost_osd_draw(oc, gApp.osd, osd_on); sp = ob.data(); }
            SDL_UpdateTexture(gApp.tex, NULL, sp, gApp.fbw * 4); }
        SDL_RenderClear(gApp.ren);

#if defined(__ANDROID__)
        /* Android: fixed 4:3 outer frame; inner picture 4:3 (stretched) or 16:10 (letterboxed) by gApp.fit */
        { SDL_Rect d; d.x = 0; d.w = gApp.fbw; if (gApp.fit == FIT_43) { d.y = 0; d.h = gApp.fbw * 3 / 4; } else { d.h = gApp.fbh; d.y = (gApp.fbw * 3 / 4 - d.h) / 2; } SDL_RenderCopy(gApp.ren, gApp.tex, NULL, &d); }
#else
        SDL_RenderCopy(gApp.ren, gApp.tex, NULL, NULL);
#endif

        SDL_RenderPresent(gApp.ren);
    }
    if (gApp.scr_pend > 0 && --gApp.scr_pend == 0) gh_shot_save();
}
