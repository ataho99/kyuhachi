/* app/main_setup.h -- S9c: main() steps E-I: hooks+input, tests, window, game select, game prep. Bodies moved unchanged from gensei_host.cpp main (as of t54u). Included in place before main; single translation unit. */
static void gh_main_input_init(std::string & id) {
#if defined(WIN32)
    SetUnhandledExceptionFilter(gh_seh); AddVectoredExceptionHandler(1, gh_veh);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CreateThread(NULL, 0, gh_boxkiller, NULL, 0, NULL);
#endif
    kh_db_dir = g_exedir;   /* t48e (#651 2): <exe>\res\gamecontrollerdb.txt */
    kh_input_init();
    kh_hotplug_hook = gh_hotplug_hook;
    gh_help_init(); if (g_helplog) ov::g_help_logfn = gh_help_logfn; ov::g_helpdev = SDL_NumJoysticks() > 0 ? 0 : 1;   /* t49a (#812): 시작값 = 패드가 연결돼 있으면 패드, 없으면 키보드 */
    gensei_lock = true;
    gh_ui_init_games();
    ui::settings_load(g_ui, g_cfgpath);
    if (g_cli_lang >= 0 && g_cli_lang <= 2) g_ui.lang = g_cli_lang;   /* t52g 시험: --lang N */
    phr_set_lang(g_ui.lang); gh_log("T52G lang=%d (0 ko / 1 en / 2 ja) 문구 %d줄 · 게임 시작 -> [%s]", g_ui.lang, phr_loaded_count, phr_("게임 시작"));
    kh_western = g_ui.g.pad == 1;   /* t48e: 시작 메뉴용(게임 시작 때 작품별 값으로 다시 정함) */
    if (g_uitest_dir.empty()) {   /* r017: 게임 폴더 · 작품 추가(시험용 CLI) */
        gh_nk_load_once();
        if (!g_gamefolder_cli.empty()) g_ui.gamefolder = aw::acp_to_u8(g_gamefolder_cli);
        if (!g_addwork_cli.empty()) gh_cli_addwork();
        if (!g_ui.gamefolder.empty() && (!g_headless || g_scanonly)) gh_scan_gamefolder(phr_("시작"));
        if (g_scanonly || (!g_addwork_cli.empty() && g_headless && id.empty())) { g_shacache.save(); gh_finish(0); }
    }
}

static void gh_main_tests(const std::string & exedir) {
    if (g_titletest) {   /* t39 (T11): 작품 제목이 창 제목에서 120바이트를 넘는 수 — 새 방식 vs 옛 방식 */
        int over_new = 0, over_old = 0, nlong = 0;
        for (auto & g : g_ui.games) {
            std::string tt = g.title; size_t old_len = tt.size() + sizeof(KH_WT_SUFFIX) - 1, new_len = old_len;
            { std::string wt = gh_window_title(g, false); if (g.title.size() > KH_WT_TITLEMAX) nlong++; new_len = wt.size(); }
            if (old_len > 120) over_old++; if (new_len > 120) over_new++;
            { std::string wt2 = gh_window_title(g, false); gh_log("TITLE %s old=%d new=%d short=%s wt_bytes=%d wt=[%s]", g.id.c_str(), (int)old_len, (int)new_len, g.short_t.c_str(), (int)wt2.size(), wt2.c_str()); }
        }
        gh_log("TITLETEST games=%d long=%d window_over legacy=%d new=%d", (int)g_ui.games.size(), nlong, over_old, over_new); printf("TITLETEST games=%d long=%d window_over legacy=%d new=%d\n", (int)g_ui.games.size(), nlong, over_old, over_new); gh_finish(0);
    }
    if (!g_logoshot.empty()) { gh_font_init(exedir); static uint32_t lb[640 * 400]; ov::Canvas lc = { lb, 640, 400 }; gh_logo_draw(lc); gh_png(g_logoshot.c_str(), lb, 640, 400); gh_log("LOGOSHOT %s", g_logoshot.c_str()); gh_finish(0); }   /* t52d (#129) */
    if (!g_uitest_dir.empty()) { gh_font_init(exedir); gh_uitest(); gh_finish(0); }
}

static void gh_main_window(const std::string & exedir) {
    if (!g_headless) {
        if (g_shaderdir.empty()) g_shaderdir = gh_exists(exedir + "\\res\\shaders") ? exedir + "\\res\\shaders" : exedir + "\\shaders";
        gh_font_init(exedir);
        if (!g_winsize_user) {   /* r017: 저장된 배율/사용자 지정 크기를 복원. 처음이면 4:3 정수배(데스크톱의 90% 안에 들어가는 최대 배) */
            gh_wm_refresh(); SDL_Rect ub; ub.x = ub.y = 0; ub.w = 1920; ub.h = 1080; SDL_GetDisplayUsableBounds(0, &ub);
            if (g_ui.wmult > 0) { int m = g_ui.wmult > g_ui.wmax ? g_ui.wmax : g_ui.wmult; g_winw = 640 * m; g_winh = 480 * m; g_ui.wmult = m; }
            else if (g_ui.winw >= 160 && g_ui.winh >= 120) { g_winw = g_ui.winw > ub.w ? ub.w : g_ui.winw; g_winh = g_ui.winh > ub.h ? ub.h : g_ui.winh; g_ui.wmult = 0; }
            else { SDL_DisplayMode dm; int k = 2; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { k = (int)std::min(dm.w * 0.9 / 640.0, dm.h * 0.9 / 480.0); if (k < 1) k = 1; if (k > 4) k = 4; } if (k > g_ui.wmax) k = g_ui.wmax; g_winw = 640 * k; g_winh = 480 * k; g_ui.wmult = k; }
            if (g_ui.wmult == 0 && g_ui.winw < 160) { g_ui.winw = g_winw; g_ui.winh = g_winh; }
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        Uint32 wf = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (g_windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) | (g_hidewin ? SDL_WINDOW_HIDDEN : 0);
        g_win = SDL_CreateWindow(KH_FULLNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_winw, g_winh, wf);
        if (g_win) {
            g_glctx = SDL_GL_CreateContext(g_win);
            if (g_glctx) {
                g_fb_mtx = SDL_CreateMutex();
                g_use_gl = gl_init_all();
                if (g_use_gl) {
                    g_vsync = SDL_GL_SetSwapInterval(1) == 0;
                    SDL_GL_MakeCurrent(g_win, NULL);
                    g_glthr = SDL_CreateThread(gl_thread, "gensei_gl", NULL);
                    if (!g_glthr) { gh_log("GL: thread create FAIL"); g_use_gl = false; SDL_GL_MakeCurrent(g_win, g_glctx); }
                }
                if (!g_use_gl) { SDL_GL_DeleteContext(g_glctx); g_glctx = NULL; }
            } else gh_log("GL: context create FAIL: %s", SDL_GetError());
            if (!g_use_gl) {   /* 폴백: 소프트웨어 렌더러(nearest, 비율 유지 논리 크기) */
                SDL_DestroyWindow(g_win);
                g_win = SDL_CreateWindow(KH_FULLNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_winw, g_winh, SDL_WINDOW_RESIZABLE | (g_windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) | (g_hidewin ? SDL_WINDOW_HIDDEN : 0));
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                if (g_win) { g_ren = SDL_CreateRenderer(g_win, -1, 0); }
            }
        }
        if (!g_win || (!g_use_gl && !g_ren)) { gh_log("ERROR: no window/renderer: %s", SDL_GetError()); gh_msgbox(phr_("화면(창)을 만들 수 없습니다.\n그래픽 드라이버를 확인해 주세요. (로그: logs 폴더)")); gh_finish(5); }
        if (g_win) { SDL_AddEventWatch(gh_ev_watch, NULL); g_wm_ew = g_winw; g_wm_eh = g_winh; g_wm_expect_until = SDL_GetTicks() + 2000u; gh_wm_refresh(); SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE); gh_titlebar_apply("create", true); }
        gh_log("window=%p gl=%d vsync=%d renderer=%p fullscreen=%d size=%dx%d fit=%d shader=%d curve=%.3f shaderdir=%s", (void *)g_win, (int)g_use_gl, (int)g_vsync, (void *)g_ren, g_windowed ? 0 : 1, g_winw, g_winh, (int)g_fit, (int)g_shader, (double)g_curve, g_shaderdir.c_str());
    }
}

static void gh_main_select(std::string & id, const bool & ui_mode, const bool & outdir_auto, const std::string & exedir) {
    if (ui_mode) {
        std::string chosen; if (!gh_ui_main(chosen)) { gh_log("ui: quit"); gh_finish(0); }
        id = chosen;
        if (!gh_profile(id, g_prof)) { gh_log("ERROR: unknown profile '%s'", id.c_str()); gh_msgbox(phr_("알 수 없는 작품입니다.")); gh_finish(1); }
        if (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) gh_rebuild_fds(g_prof, g_ui.games[g_cur_game]);
        { auto in = g_prof.kv.find("install"); auto en = g_prof.kv.find("entry_name"); if (in != g_prof.kv.end() && !in->second.empty() && en != g_prof.kv.end()) { FILE * pf = fopen((exedir + "\\install_pending.tsv").c_str(), "wb"); if (pf) { fprintf(pf, "%s\t%s\n", id.c_str(), en->second.c_str()); fclose(pf); } } }
        if (outdir_auto) {   /* UI 단계 로그는 logs\menu, 게임 로그는 logs\<작품> */
            if (g_log) { fflush(g_log); fclose(g_log); }
            std::string d = exedir + "\\logs\\" + id; plat::mkdir(d.c_str()); g_outdir = d; g_log = fopen((d + "\\kyuhachi_log.txt").c_str(), "w"); g_log_t0 = SDL_GetTicks();
            gh_log("gensei_host " GENSEI_BUILD_ID " game start from UI: '%s' resume_slot=%d noB=%d", id.c_str(), g_resume_slot, (int)g_noB);
        }
    } else {
        for (size_t i = 0; i < g_ui.games.size(); i++) if (g_ui.games[i].id == id) g_cur_game = (int)i;
        if (g_cur_game >= 0 && g_cur_game < (int)g_ui.games.size()) gh_rebuild_fds(g_prof, g_ui.games[g_cur_game]);
        if (!g_headless) { gh_refresh_game_slots(g_cur_game); }
    }
}

static void gh_main_prep(std::string & id, const std::string & exedir) {
    if (g_uitest_dir.empty() && !gh_any_font_bmp(NULL)) {   /* t52b (#127): 글꼴 BMP 0개 — 메뉴 경로는 시작 전에 막지만 바로 실행(인자)·예외 경로의 마지막 방어. 죽지 말고 알리고 끝낸다 */
        gh_log("ERROR: 글꼴 BMP(anex86 형식 524,350 B) 없음: %s\\res\\fonts -> 작품을 시작하지 않음", exedir.c_str());
        if (!g_headless && !g_hidewin) gh_msgbox(phr_("res/fonts 에 anex86 형식 글꼴 BMP 를 넣어 주세요"));
        gh_finish(1);
    }
    g_pid = id;   /* t51a: 창 안 전환이 프로필을 다시 읽을 때 쓰는 작품 id */
    if (g_uitest_dir.empty()) {
        if (!g_imgdir_given && !g_prof.imgdir.empty()) g_imgdir = g_prof.imgdir;
        { auto mk = g_prof.kv.find("mouse"); g_mouse_game = (mk != g_prof.kv.end() && !mk->second.empty() && mk->second != "none") || !g_amouse.empty() || (g_prof.disk[0] == NULL && g_prof.disk[1] == NULL && g_prof.disk[2] == NULL && g_prof.disk[3] == NULL && !g_prof.cd.empty()); /* t38cd: 내장 DOS(CD 슬롯)는 마우스를 기본 연결 */ gh_log("mouse: 이 작품의 마우스 = %s (호스트 마우스 입력 %s)", mk == g_prof.kv.end() ? "(미지정)" : mk->second.c_str(), g_mouse_game ? "연결" : "연결 안 함"); }
        gh_apply_profile_input(g_prof);
        /* t36cd: 디스크는 없고 CD 슬롯만 있으면 내장 DOS(셸)로 부팅해 D: 에 CD 를 올린다. 디스크도 CD 도 있으면 CD 는 이 시험 빌드에서 무시(게스트 IDE 연결은 범위 밖) */
        g_dosfd = g_prof.kv.count("dosfd") && g_prof.kv["dosfd"] == "1" && (g_prof.disk[0] != NULL || g_prof.disk[1] != NULL) && g_prof.disk[2] == NULL && g_prof.disk[3] == NULL;
        g_dosmode = g_dosfd || (g_prof.disk[0] == NULL && g_prof.disk[1] == NULL && g_prof.disk[2] == NULL && g_prof.disk[3] == NULL && !g_prof.cd.empty());
        if (!g_prof.cd.empty() && !g_dosmode) gh_log("WARN: cd 슬롯과 디스크가 함께 있음 — 이 빌드는 게스트 IDE 연결을 하지 않아 cd 를 무시: %s", g_prof.cd.c_str());
        if (!g_dosmode && !g_prof.disk[0] && !g_prof.disk[1] && !g_prof.disk[2] && !g_prof.disk[3]) { gh_log("ERROR: 부팅 가능한 디스크 없음 (%s)", g_prof.reason.c_str()); gh_msgbox(phr_("이 작품은 바로 부팅할 수 있는 디스크가 없습니다.\n(하드 설치 전용이거나 지원하지 않는 형식입니다)")); gh_finish(1); }
    }
    if (!g_headless) {
        g_ui.cur = g_cur_game;
        gh_apply_settings(g_ui.effective(g_cur_game), false);
        if (g_win) {   /* t39 (T11): 창 제목은 120바이트 이내 — 길면 short= 또는 자동 줄임 */
            SDL_SetWindowTitle(g_win, gh_window_title(g_ui.games[g_cur_game], g_titlelegacy).c_str()); }
        if (!g_noboot && g_ui.effective(g_cur_game).boot == 0) gh_boot_seq(false);   /* 짧은 헤더 + Now Loading (≤0.5초), 건너뛰기 가능 */
    }
}
