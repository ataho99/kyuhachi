/* app/main_setup.h -- S9c: main() steps E-I: hooks+input, tests, window, game select, game prep. Bodies moved unchanged from gensei_host.cpp main (as of t54u). Included in place before main; single translation unit. */
#if defined(__ANDROID__)
/* t56a: Android save dir = <chosen game folder>\saves (same sub-layout as <exe>\saves on Windows) */
static void gh_android_savedir(void) {
    if (gApp.ui_.gamefolder.empty()) return;
    std::string sd = gApp.ui_.gamefolder + "\\saves"; plat::mkdir(sd);
    bool ok = plat::exists(sd);
    gh_log("SAVEDIR folder=%s -> %s (%s)", gApp.ui_.gamefolder.c_str(), sd.c_str(), ok ? "ok" : "mkdir failed, keep app dir");
    if (ok) gApp.savedir = sd;
}
#endif
#if defined(__ANDROID__)
/* t58a: diag - can the app list <exedir>\res\fonts ? (font guard uses a directory listing) */
static void gh_android_fontdiag(void) {
    std::string fd = gApp.exedir + "\\res\\fonts"; DIR * d = opendir(fd.c_str()); int en = errno; int n = 0;
    if (d) { while (readdir(d)) n++; closedir(d); }
    gh_log("FONTDIR opendir=%s errno=%d entries=%d", d ? "ok" : "fail", d ? 0 : en, n);
}
#endif
static void gh_main_input_init(std::string & id) {
    gh_crash_install();
    kh_db_dir = gApp.exedir;   /* t48e (#651 2): <exe>\res\gamecontrollerdb.txt */
    kh_input_init();
    kh_hotplug_hook = gh_hotplug_hook;
    gh_help_init(); if (gApp.helplog) ov::g_help_logfn = gh_help_logfn; ov::g_helpdev = SDL_NumJoysticks() > 0 ? 0 : 1;   /* t49a (#812): 시작값 = 패드가 연결돼 있으면 패드, 없으면 키보드 */
    gensei_lock = true;
    gh_ui_init_games();
    ui::settings_load(gApp.ui_, gApp.cfgpath);   { FILE * zf_ = fopen(gApp.cfgpath.c_str(), "rb"); int ex_ = zf_ != NULL; if (zf_) fclose(zf_); int cg_ = gApp.cur_game; bool ht_ = cg_ >= 0 && cg_ < (int)ui::Ui::MAXG && gApp.ui_.has_t[cg_]; gh_log("SETDIAG %s path=%s file=%d g[scan=%d mask=%d bleed=%d glow=%d curve=%d ratio=%d] cur_game=%d has_t=%d games=%d", "after-start-load", gApp.cfgpath.c_str(), ex_, gApp.ui_.g.scan, gApp.ui_.g.mask, gApp.ui_.g.bleed, gApp.ui_.g.glow, gApp.ui_.g.curve, gApp.ui_.g.ratio, cg_, (int)ht_, (int)gApp.ui_.games.size()); }
#if defined(__ANDROID__)
    kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_seed_assets(); kha_cfg_load(); kha_perm_ask_once(); kha_lib_init();   /* t137 */
    gh_android_savedir();
    gh_android_fontdiag();
#endif
    if (gApp.cli_lang >= 0 && gApp.cli_lang <= 2) gApp.ui_.lang = gApp.cli_lang;   /* t52g 시험: --lang N */
    phr_set_lang(gApp.ui_.lang); gh_log("T52G lang=%d (0 ko / 1 en / 2 ja) 문구 %d줄 · 게임 시작 -> [%s]", gApp.ui_.lang, phr_loaded_count, phr_("게임 시작"));
    kh_western = gApp.ui_.g.pad == 1;   /* t48e: 시작 메뉴용(게임 시작 때 작품별 값으로 다시 정함) */
    if (gApp.uitest_dir.empty()) {   /* r017: 게임 폴더 · 작품 추가(시험용 CLI) */
        gh_nk_load_once();
        if (!gApp.gamefolder_cli.empty()) gApp.ui_.gamefolder = aw::acp_to_u8(gApp.gamefolder_cli);
        if (!gApp.addwork_cli.empty()) gh_cli_addwork();
        if (!gApp.ui_.gamefolder.empty() && (!gApp.headless || gApp.scanonly)) gh_scan_gamefolder(phr_("시작"));
        if (gApp.scanonly || (!gApp.addwork_cli.empty() && gApp.headless && id.empty())) { gApp.shacache.save(); gh_finish(0); }
    }
}

static void gh_main_tests(const std::string & exedir) {
    if (gApp.titletest) {   /* t39 (T11): 작품 제목이 창 제목에서 120바이트를 넘는 수 — 새 방식 vs 옛 방식 */
        int over_new = 0, over_old = 0, nlong = 0;
        for (auto & g : gApp.ui_.games) {
            std::string tt = g.title; size_t old_len = tt.size() + sizeof(KH_WT_SUFFIX) - 1, new_len = old_len;
            { std::string wt = gh_window_title(g, false); if (g.title.size() > KH_WT_TITLEMAX) nlong++; new_len = wt.size(); }
            if (old_len > 120) over_old++; if (new_len > 120) over_new++;
            { std::string wt2 = gh_window_title(g, false); gh_log("TITLE %s old=%d new=%d short=%s wt_bytes=%d wt=[%s]", g.id.c_str(), (int)old_len, (int)new_len, g.short_t.c_str(), (int)wt2.size(), wt2.c_str()); }
        }
        gh_log("TITLETEST games=%d long=%d window_over legacy=%d new=%d", (int)gApp.ui_.games.size(), nlong, over_old, over_new); printf("TITLETEST games=%d long=%d window_over legacy=%d new=%d\n", (int)gApp.ui_.games.size(), nlong, over_old, over_new); gh_finish(0);
    }
    if (!gApp.logoshot.empty()) { gh_font_init(exedir); static uint32_t lb[640 * 400]; ov::Canvas lc = { lb, 640, 400 }; gh_logo_draw(lc); gh_png(gApp.logoshot.c_str(), lb, 640, 400); gh_log("LOGOSHOT %s", gApp.logoshot.c_str()); gh_finish(0); }   /* t52d (#129) */
    if (!gApp.uitest_dir.empty()) { gh_font_init(exedir); gh_uitest(); gh_finish(0); }
}

static void gh_main_window(const std::string & exedir) {
    if (!gApp.headless) {
        if (gApp.shaderdir.empty()) gApp.shaderdir = gh_exists(exedir + "\\res\\shaders") ? exedir + "\\res\\shaders" : exedir + "\\shaders";
        gh_font_init(exedir);
        if (!gApp.winsize_user) {   /* r017: 저장된 배율/사용자 지정 크기를 복원. 처음이면 4:3 정수배(데스크톱의 90% 안에 들어가는 최대 배) */
            gh_wm_refresh(); SDL_Rect ub; ub.x = ub.y = 0; ub.w = 1920; ub.h = 1080; SDL_GetDisplayUsableBounds(0, &ub);
            if (gApp.ui_.wmult > 0) { int m = gApp.ui_.wmult > gApp.ui_.wmax ? gApp.ui_.wmax : gApp.ui_.wmult; gApp.winw = 640 * m; gApp.winh = 480 * m; gApp.ui_.wmult = m; }
            else if (gApp.ui_.winw >= 160 && gApp.ui_.winh >= 120) { gApp.winw = gApp.ui_.winw > ub.w ? ub.w : gApp.ui_.winw; gApp.winh = gApp.ui_.winh > ub.h ? ub.h : gApp.ui_.winh; gApp.ui_.wmult = 0; }
            else { SDL_DisplayMode dm; int k = 2; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { k = (int)std::min(dm.w * 0.9 / 640.0, dm.h * 0.9 / 480.0); if (k < 1) k = 1; if (k > 4) k = 4; } if (k > gApp.ui_.wmax) k = gApp.ui_.wmax; gApp.winw = 640 * k; gApp.winh = 480 * k; gApp.ui_.wmult = k; }
            if (gApp.ui_.wmult == 0 && gApp.ui_.winw < 160) { gApp.ui_.winw = gApp.winw; gApp.ui_.winh = gApp.winh; }
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#if defined(__ANDROID__)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);   /* t57a: GLES2 context */
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        Uint32 wf = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (gApp.windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) | (gApp.hidewin ? SDL_WINDOW_HIDDEN : 0);
        gApp.win = SDL_CreateWindow(KH_FULLNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, gApp.winw, gApp.winh, wf);
        if (gApp.win) {
            gApp.glctx = SDL_GL_CreateContext(gApp.win);
            if (gApp.glctx) {
                gApp.fb_mtx = SDL_CreateMutex();
                gApp.use_gl = gl_init_all();
                if (gApp.use_gl) {
                    gApp.vsync = SDL_GL_SetSwapInterval(1) == 0;
                    SDL_GL_MakeCurrent(gApp.win, NULL);
                    gApp.glthr = SDL_CreateThread(gl_thread, "gensei_gl", NULL);
                    if (!gApp.glthr) { gh_log("GL: thread create FAIL"); gApp.use_gl = false; SDL_GL_MakeCurrent(gApp.win, gApp.glctx); }
                }
                if (!gApp.use_gl) { SDL_GL_DeleteContext(gApp.glctx); gApp.glctx = NULL; }
            } else gh_log("GL: context create FAIL: %s", SDL_GetError());
            if (!gApp.use_gl) {   /* 폴백: 소프트웨어 렌더러(nearest, 비율 유지 논리 크기) */
                SDL_DestroyWindow(gApp.win);
                gApp.win = SDL_CreateWindow(KH_FULLNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, gApp.winw, gApp.winh, SDL_WINDOW_RESIZABLE | (gApp.windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) | (gApp.hidewin ? SDL_WINDOW_HIDDEN : 0));
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                if (gApp.win) { gApp.ren = SDL_CreateRenderer(gApp.win, -1, 0); }
            }
        }
        if (!gApp.win || (!gApp.use_gl && !gApp.ren)) { gh_log("ERROR: no window/renderer: %s", SDL_GetError()); gh_msgbox(phr_("화면(창)을 만들 수 없습니다.\n그래픽 드라이버를 확인해 주세요. (로그: logs 폴더)")); gh_finish(5); }
#if defined(__ANDROID__)
        SDL_StopTextInput();   /* SDL2 starts text input by default, which raises the Android IME */
#endif
        if (gApp.win) { SDL_AddEventWatch(gh_ev_watch, NULL); gApp.wm_ew = gApp.winw; gApp.wm_eh = gApp.winh; gApp.wm_expect_until = SDL_GetTicks() + 2000u; gh_wm_refresh(); SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE); gh_titlebar_apply("create", true); }
        gh_log("window=%p gl=%d vsync=%d renderer=%p fullscreen=%d size=%dx%d fit=%d shader=%d curve=%.3f shaderdir=%s", (void *)gApp.win, (int)gApp.use_gl, (int)gApp.vsync, (void *)gApp.ren, gApp.windowed ? 0 : 1, gApp.winw, gApp.winh, (int)gApp.fit, (int)gApp.shader, (double)gApp.curve, gApp.shaderdir.c_str());
    }
}

static void gh_main_select(std::string & id, const bool & ui_mode, const bool & outdir_auto, const std::string & exedir) {
    if (ui_mode) {
        std::string chosen; if (!gh_ui_main(chosen)) { gh_log("ui: quit"); gh_finish(0); }
        id = chosen;
        if (!gh_profile(id, gApp.prof_)) { gh_log("ERROR: unknown profile '%s'", id.c_str()); gh_msgbox(phr_("알 수 없는 작품입니다.")); gh_finish(1); }
        if (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) gh_rebuild_fds(gApp.prof_, gApp.ui_.games[gApp.cur_game]);
        { auto in = gApp.prof_.kv.find("install"); auto en = gApp.prof_.kv.find("entry_name"); if (in != gApp.prof_.kv.end() && !in->second.empty() && en != gApp.prof_.kv.end()) { FILE * pf = fopen((exedir + "\\install_pending.tsv").c_str(), "wb"); if (pf) { fprintf(pf, "%s\t%s\n", id.c_str(), en->second.c_str()); fclose(pf); } } }
        if (outdir_auto) {   /* UI 단계 로그는 logs\menu, 게임 로그는 logs\<작품> */
            { std::string d = exedir + "\\logs\\" + id; plat::mkdir(d.c_str()); FILE * nlg = fopen((d + "\\kyuhachi_log.txt").c_str(), "w");   /* t146: 게임 로그 dir 이 안 열리면 공용(menu) 로그에 계속 기록 */
              if (nlg) { if (gApp.log) { fflush(gApp.log); fclose(gApp.log); } gApp.outdir = d; gApp.log = nlg; gApp.log_t0 = SDL_GetTicks(); }
              else gh_log("LOGFALLBACK: cannot open game log folder, continuing in the shared (menu) log: %s (errno=%d)", d.c_str(), errno); }
            gh_log("gensei_host " GENSEI_BUILD_ID " game start from UI: '%s' resume_slot=%d noB=%d", id.c_str(), gApp.resume_slot, (int)gApp.noB);
        }
    } else {
        for (size_t i = 0; i < gApp.ui_.games.size(); i++) if (gApp.ui_.games[i].id == id) gApp.cur_game = (int)i;
        if (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) gh_rebuild_fds(gApp.prof_, gApp.ui_.games[gApp.cur_game]);
        if (!gApp.headless) { gh_refresh_game_slots(gApp.cur_game); }
    }
}

static void gh_main_prep(std::string & id, const std::string & exedir) {
    if (gApp.uitest_dir.empty() && !gh_any_font_bmp(NULL)) {   /* #500 (t52b #127 대체): 글꼴 BMP 0개여도 막지 않는다 — gh_default_font_path 가 빈 글꼴(res\.nofont.bin)을 건네고 화면 안내만 한다 */
        gh_log("WARN: 글꼴 BMP(anex86 형식 524,350 B) 없음: %s\\res\\fonts -> 빈 글꼴로 시작", exedir.c_str());
        gApp.osd = phr_("글꼴 없음: 기본 글꼴로 실행합니다 (글자가 안 보일 수 있음 — res/fonts 에 글꼴 BMP 를 넣으세요)"); gApp.osd_until = SDL_GetTicks() + 8000u;
    }
    gApp.pid = id;   /* t51a: 창 안 전환이 프로필을 다시 읽을 때 쓰는 작품 id */
    if (gApp.uitest_dir.empty()) {
        if (!gApp.imgdir_given && !gApp.prof_.imgdir.empty()) gApp.imgdir = gApp.prof_.imgdir;
        { auto mk = gApp.prof_.kv.find("mouse"); gApp.mouse_game = (mk != gApp.prof_.kv.end() && !mk->second.empty() && mk->second != "none") || !gApp.amouse.empty() || (gApp.prof_.disk[0] == NULL && gApp.prof_.disk[1] == NULL && gApp.prof_.disk[2] == NULL && gApp.prof_.disk[3] == NULL && !gApp.prof_.cd.empty()); /* t38cd: 내장 DOS(CD 슬롯)는 마우스를 기본 연결 */ gh_log("mouse: 이 작품의 마우스 = %s (호스트 마우스 입력 %s)", mk == gApp.prof_.kv.end() ? "(미지정)" : mk->second.c_str(), gApp.mouse_game ? "연결" : "연결 안 함"); }
        gh_apply_profile_input(gApp.prof_);
        /* t36cd: 디스크는 없고 CD 슬롯만 있으면 내장 DOS(셸)로 부팅해 D: 에 CD 를 올린다. 디스크도 CD 도 있으면 CD 는 이 시험 빌드에서 무시(게스트 IDE 연결은 범위 밖) */
        gApp.dosfd = gApp.prof_.kv.count("dosfd") && gApp.prof_.kv["dosfd"] == "1" && (gApp.prof_.disk[0] != NULL || gApp.prof_.disk[1] != NULL) && gApp.prof_.disk[2] == NULL && gApp.prof_.disk[3] == NULL;
        gApp.dosmode = gApp.dosfd || (gApp.prof_.disk[0] == NULL && gApp.prof_.disk[1] == NULL && gApp.prof_.disk[2] == NULL && gApp.prof_.disk[3] == NULL && !gApp.prof_.cd.empty());
        if (!gApp.prof_.cd.empty() && !gApp.dosmode) gh_log("WARN: cd 슬롯과 디스크가 함께 있음 — 이 빌드는 게스트 IDE 연결을 하지 않아 cd 를 무시: %s", gApp.prof_.cd.c_str());
        if (!gApp.dosmode && !gApp.prof_.disk[0] && !gApp.prof_.disk[1] && !gApp.prof_.disk[2] && !gApp.prof_.disk[3]) { gh_log("ERROR: 부팅 가능한 디스크 없음 (%s)", gApp.prof_.reason.c_str()); gh_msgbox(phr_("이 작품은 바로 부팅할 수 있는 디스크가 없습니다.\n(하드 설치 전용이거나 지원하지 않는 형식입니다)")); gh_finish(1); }
    }
    if (!gApp.headless) {
        gApp.ui_.cur = gApp.cur_game;
        { FILE * zf_ = fopen(gApp.cfgpath.c_str(), "rb"); int ex_ = zf_ != NULL; if (zf_) fclose(zf_); int cg_ = gApp.cur_game; bool ht_ = cg_ >= 0 && cg_ < (int)ui::Ui::MAXG && gApp.ui_.has_t[cg_]; gh_log("SETDIAG %s path=%s file=%d g[scan=%d mask=%d bleed=%d glow=%d curve=%d ratio=%d] cur_game=%d has_t=%d games=%d", "before-first-apply", gApp.cfgpath.c_str(), ex_, gApp.ui_.g.scan, gApp.ui_.g.mask, gApp.ui_.g.bleed, gApp.ui_.g.glow, gApp.ui_.g.curve, gApp.ui_.g.ratio, cg_, (int)ht_, (int)gApp.ui_.games.size()); } gh_apply_settings(gApp.ui_.effective(gApp.cur_game), false);
        if (gApp.win) {   /* t39 (T11): 창 제목은 120바이트 이내 — 길면 short= 또는 자동 줄임 */
            SDL_SetWindowTitle(gApp.win, gh_window_title(gApp.ui_.games[gApp.cur_game], gApp.titlelegacy).c_str()); }
        if (!gApp.noboot && gApp.ui_.effective(gApp.cur_game).boot == 0) gh_boot_seq(false);   /* 짧은 헤더 + Now Loading (≤0.5초), 건너뛰기 가능 */
    }
}
