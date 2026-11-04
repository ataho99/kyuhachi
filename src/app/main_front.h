/* app/main_front.h -- S9c: main() steps A-D: args, env, log+SDL init, audio+profile. Bodies moved unchanged from gensei_host.cpp main (as of t54u). Included in place before main; single translation unit. */
static int gh_main_args(int argc, char * argv[], std::string & id) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--imgdir" && i + 1 < argc) { gApp.imgdir = argv[++i]; gApp.imgdir_given = true; }
        else if (a == "--out" && i + 1 < argc) gApp.outdir = argv[++i];
        else if (a == "--headless") gApp.headless = true;
        else if (a == "--hidewin") gApp.hidewin = true;
        else if (a == "--gltime") gApp.gltime = true;
        else if (a == "--save" && i + 1 < argc) gApp.save_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--load" && i + 1 < argc) gApp.load_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--state" && i + 1 < argc) gApp.state_path = argv[++i];
        else if (a == "--sec" && i + 1 < argc) gApp.run_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--shot" && i + 1 < argc) { char * s = argv[++i]; while (*s) { gApp.shots_ms.push_back((unsigned)(atof(s) * 1000.0)); while (*s && *s != ',') s++; if (*s == ',') s++; } }
        else if (a == "--windowed") gApp.windowed = true;
        else if (a == "--fullscreen") gApp.windowed = false;
        else if (a == "--noboot") gApp.noboot = true;
        else if (a == "--uitest" && i + 1 < argc) gApp.uitest_dir = argv[++i];
        else if (a == "--logoshot" && i + 1 < argc) gApp.logoshot = argv[++i];
        else if (a == "--boosttest" && i + 1 < argc) { double s0 = 0, l0 = 0; int x0 = 3; if (sscanf(argv[++i], "%lf,%lf,%d", &s0, &l0, &x0) >= 2) { gApp.bt_at = (unsigned)(s0 * 1000.0); gApp.bt_len = (unsigned)(l0 * 1000.0); gApp.bt_x = x0; } }
        else if (a == "--demo") gApp.demo = true;
        else if (a == "--entry" && i + 1 < argc) { gApp.entry_cli = argv[++i]; prof::g_entry_req = aw::acp_to_u8(gApp.entry_cli); }   /* t36cd: 실행 항목 이름 */
        else if (a == "--userini" && i + 1 < argc) prof::g_userini_override = argv[++i];   /* t39: 시험 실행용 user.ini 경로 */
        else if (a == "--uikeys" && i + 1 < argc) gApp.uikeys_file = argv[++i];
        else if (a == "--hangultest" && i + 1 < argc) {   /* t48b (#628 3 ⒜): 조합기 단위 시험표를 TSV 로 쓰고 끝낸다(창·코어 없음) */
            int hp = 0, ht = 0; std::string tsv = hg::selftest(hp, ht); FILE * hf = fopen(argv[++i], "wb"); if (hf) { fwrite(tsv.data(), 1, tsv.size(), hf); fprintf(hf, "합계\t%d / %d\n", hp, ht); fclose(hf); }
            return hp == ht ? 0 : 1; }
        else if (a == "--titletest") gApp.titletest = true;
        else if (a == "--helplog") gApp.helplog = true;
        else if (a == "--inwin" && i + 1 < argc) gApp.inwin_enabled = atoi(argv[++i]) != 0;   /* t51a: 0 = 창 안 전환 끔(현행 재실행 경로만) */
        else if (a == "--inwintest" && i + 1 < argc) gApp.inwin_testfail = argv[++i];   /* t51a 시험: attach | noboot | throw */
        else if (a == "--resume" && i + 1 < argc) gApp.resume_slot = atoi(argv[++i]);   /* t51a: 폴백 재실행(메뉴에서 고른 이어하기 슬롯) */
        else if (a == "--nostage") gApp.nostage = true;   /* t49a: 부트 순서(단계) 기능 끄기 — 시험용 */
        else if (a == "--titlelegacy") gApp.titlelegacy = true;
        else if (a == "--axistest") gApp.axistest = true;
        else if (a == "--swaptest" && i + 1 < argc) { double s0 = 0; int d0 = 0, i0 = 0; if (sscanf(argv[++i], "%lf,%d,%d", &s0, &d0, &i0) == 3) { gApp.swap_ms = (unsigned)(s0 * 1000.0); gApp.swap_drv = d0; gApp.swap_idx = i0; } }
        else if (a == "--capstest" && i + 1 < argc) { double s0 = 0, l0 = 0; if (sscanf(argv[++i], "%lf,%lf", &s0, &l0) == 2) { gApp.caps_at = (unsigned)(s0 * 1000.0); gApp.caps_len = (unsigned)(l0 * 1000.0); } }
        else if (a == "--winsize" && i + 1 < argc) { int w = 0, h = 0; if (sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 64 && h > 64) { gApp.winw = w; gApp.winh = h; gApp.winsize_user = true; } }
        else if (a == "--fit" && i + 1 < argc) { std::string v = argv[++i]; gApp.fit = v == "1610" ? FIT_1610 : (v == "int" ? FIT_INT : FIT_43); }
        else if (a == "--shader" && i + 1 < argc) { std::string v = argv[++i]; gApp.shader = v == "crt" ? SH_CRT : (v == "nearest" ? SH_NEAREST : SH_SHARP); }
        else if (a == "--curve" && i + 1 < argc) { float c = (float)atof(argv[++i]); gApp.curve = c < 0 ? 0 : (c > 0.10f ? 0.10f : c); }
        else if (a == "--shaderdir" && i + 1 < argc) gApp.shaderdir = argv[++i];
        else if (a == "--autokeys") gApp.autokeys = true;
        else if (a == "--bkosd" && i + 1 < argc) gApp.bk_osd_on = atoi(argv[++i]) != 0;   /* t47a */
        else if (a == "--bkpause" && i + 1 < argc) { unsigned f = 0, t = 0; if (sscanf(argv[++i], "%u,%u", &f, &t) == 2 && t > f) { gApp.bkp_from = f; gApp.bkp_to = t; } }   /* t47a 시험용: 부팅 후 f ms 에 정지, t ms 에 재개 */

        else if (a == "--scrshot" && i + 1 < argc) { char * s = argv[++i]; while (*s) { gApp.scrshots_ms.push_back((unsigned)(atof(s) * 1000.0)); while (*s && *s != ',') s++; if (*s == ',') s++; } }
        else if (a == "--setvol" && i + 1 < argc) { int b = 8, se = 8; if (sscanf(argv[++i], "%d,%d", &b, &se) == 2) { gApp.setvol_b = b; gApp.setvol_s = se; } }
        else if (a == "--rmswin" && i + 1 < argc) { gApp.rms_from = (unsigned)(atof(argv[++i]) * 1000.0); }
        else if (a == "--mouseok") gApp.mouseok_cli = true;
        else if (a == "--traceexc") gApp.traceexc = 1;
        else if (a == "--ems") gApp.ems_req = 1;
        else if (a == "--noems") gApp.ems_req = 0;
        else if (a == "--trace21") gApp.trace21 = true;
        else if (a == "--keytrace") gApp.keytrace = true;
        else if (a == "--int18trace") gApp.int18trace = true;
        else if (a == "--segdump" && i + 1 < argc) gApp.segdump_ms = (unsigned)atoi(argv[++i]);
        else if (a == "--hist" && i + 1 < argc) { const char * s = argv[++i]; gApp.hist_from = (unsigned)atoi(s); const char * c = strchr(s, ','); gApp.hist_to = c ? (unsigned)atoi(c + 1) : gApp.hist_from + 10000; }
        else if (a == "--keyhold" && i + 1 < argc) gApp.keyhold = (unsigned)atoi(argv[++i]);
        else if (a == "--boot400") gApp.boot400 = true;
        else if (a == "--bda") gApp.bda = true;
        else if (a == "--ext14") gApp.ext14 = true;
        else if (a == "--ext0") gApp.ext0 = true;
        else if (a == "--di0") gApp.di0 = true;
        else if (a == "--mouseirq12") gApp.mirq12 = true;
        else if (a == "--watch" && i + 1 < argc) { unsigned w = 0; if (sscanf(argv[++i], "%x", &w) == 1 && w < 0x10000) gApp.watch_off = (int)w; }
        else if (a == "--dumpcode") gApp.dumpcode = true;   /* r021 진단: 2초마다 CS:IP 주변 코드 바이트·레지스터·PIC 마스크를 로그에 */
        else if (a == "--automouse" && i + 1 < argc) gh_parse_amouse(argv[++i]);
        else if (a == "--inject" && i + 1 < argc) { gApp.inject_file = argv[++i]; bool ok = gh_parse_inject(gApp.inject_file); gh_log("--inject %s -> %s (%u 줄)", gApp.inject_file.c_str(), ok ? "OK" : "FAIL", (unsigned)gApp.inj.size()); }   /* t48e (#648): 시험 전용 입력 주입(SDL 이벤트·가상 조이스틱만) */
        else if (a == "--kbshow" && i + 1 < argc) gApp.kbt_show_ms = (unsigned)(atof(argv[++i]) * 1000.0);   /* t46x 시험: 초 뒤 화면 키보드 켜기 */
        else if (a == "--kbhide" && i + 1 < argc) gApp.kbt_hide_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--kbclick" && i + 1 < argc) gh_parse_kbclick(argv[++i]);   /* "<시작초>:STOP,GRPH,X,GRPH,KANA" */
        else if (a == "--kbbtn" && i + 1 < argc) kbov::g_cli_btn = atoi(argv[++i]) ? 1 : 0;
        else if (a == "--vpad" && i + 1 < argc) vpad::g_cli = atoi(argv[++i]);   // test: virtual pad 0 off 1 on 2 auto
        else if (a == "--ovbtn" && i + 1 < argc) kbov::g_cli_ov = atoi(argv[++i]) ? 1 : 0;   /* t52f 시험: 오버레이 버튼 줄 켬/끔 */
        else if (a == "--ovstrong") kbov::g_force_strong = true;   /* t52f 시험: 버튼 줄을 항상 진하게 */
        else if (a == "--lang" && i + 1 < argc) gApp.cli_lang = atoi(argv[++i]);   /* t52g 시험 */
        else if (a == "--menushot") gApp.menushot = true;   /* t52f 시험: 메뉴 창 캡처 후 종료 */
        else if (a == "--kbshader" && i + 1 < argc) gApp.cli_kbsh = atoi(argv[++i]) ? 1 : 0;
        else if (a == "--savedir" && i + 1 < argc) gApp.savedir = argv[++i];
        else if (a == "--audiodev" && i + 1 < argc) gApp.audiodev = argv[++i];
        else if (a == "--listaudio") gApp.listaudio = true;
        else if (a == "--glmatrix" && i + 1 < argc) gApp.glmx_ms = (unsigned)(atof(argv[++i]) * 1000.0);
        else if (a == "--splash") gensei_nosplash = 0;
        else if (a == "--batest") gApp.batest = true;
        else if (a == "--noload") gApp.noload = true;
        else if (a == "--frametest" && i + 1 < argc) gApp.ft_dir = argv[++i];
        else if (a == "--ftms" && i + 1 < argc) gApp.ft_ms = (unsigned)atoi(argv[++i]);
        else if (a == "--bsound" && i + 1 < argc) gApp.bsound_override = atoi(argv[++i]);
        else if (a == "--nkdb" && i + 1 < argc) gApp.nkdb_path = argv[++i];
        else if (a == "--addwork" && i + 1 < argc) gApp.addwork_cli = argv[++i];
        else if (a == "--gamefolder" && i + 1 < argc) gApp.gamefolder_cli = argv[++i];
        else if (a == "--scanonly") gApp.scanonly = true;
        else if (a == "--dry") gApp.aw_dry = true;
        else if (a == "--report" && i + 1 < argc) gApp.report_dir = argv[++i];
        else if (a[0] != '-') { id = a; gApp.id_given = true; }
    }
    return -1;   /* S9c */
}

static void gh_main_env(const std::string & exedir) {
    if (gApp.imgdir == "." && plat::exists((exedir + "\\img").c_str())) gApp.imgdir = exedir + "\\img";
    if (gApp.savedir == "saves") gApp.savedir = exedir + "\\saves";
    gApp.cfgpath = exedir + "\\settings.ini";
    { prof::Ini fi; if (prof::ini_load(exedir + "\\res\\flags.ini", fi) && fi.count("flags")) { auto it = fi["flags"].find("mouse_support"); if (it != fi["flags"].end() && it->second == "1") aw::mouse_ok() = true; auto mg = fi["flags"].find("mouse_support_games"); if (mg != fi["flags"].end()) { for (auto & s : prof::split_list(mg->second)) { std::string v = s; for (auto & c : v) c = (char)tolower((unsigned char)c); while (!v.empty() && v.back() == ' ') v.pop_back(); size_t b = v.find_first_not_of(' '); if (b != std::string::npos && b) v = v.substr(b); if (!v.empty()) aw::mouse_games().insert(v); } } } if (gApp.mouseok_cli) aw::mouse_ok() = true;
      { prof::Profile q; q.disk.assign(4, std::string()); q.disk[0] = "a.fdi"; std::string r3[3]; const char * ids[3] = { "nadia", "ultra", "dotb" }; for (int i = 0; i < 3; i++) { std::map<std::string, std::string> kv; kv["mouse"] = "required"; kv["curated"] = "1"; kv["__id"] = ids[i]; r3[i] = aw::block_reason(q, kv); }
        { char fb[400]; snprintf(fb, sizeof fb, "flags: mouse_support=%d mouse_support_games=%d개 | 마우스 필수 판정(nadia/ultra/dotb) = [%s] / [%s] / [%s]  (빈칸 = 풀림)", (int)aw::mouse_ok(), (int)aw::mouse_games().size(), r3[0].c_str(), r3[1].c_str(), r3[2].c_str()); gApp.flaglog = fb; } } }
}

static void gh_main_logsdl(const std::string & exedir, const bool & ui_mode, std::string & id, const bool & outdir_auto) {
    if (outdir_auto) { std::string d = exedir + "\\logs"; plat::mkdir(d.c_str()); d += "\\" + (ui_mode ? std::string("menu") : (id.empty() ? std::string("unknown") : id)); plat::mkdir(d.c_str()); gApp.outdir = d; }
    {
        std::string lp = gApp.outdir + "\\kyuhachi_log.txt";
        gApp.log = fopen(lp.c_str(), "w");
        if (gApp.log && !gApp.flaglog.empty()) gh_log("%s", gApp.flaglog.c_str());
    }
    if (gApp.headless) { plat::putenv_a("SDL_AUDIODRIVER=dummy"); }   /* 무음: 헤드리스/시험은 소리 장치를 아예 열지 않는다 */
#if defined(__ANDROID__)
    SDL_SetHint("SDL_ENABLE_SCREEN_KEYBOARD", "0");   /* no Android soft keyboard over the game (our own kbov handles on-screen keys) */
#endif
    SDL_SetHint("SDL_IME_SHOW_UI", "1"); SDL_SetHint("SDL_MOUSE_FOCUS_CLICKTHROUGH", "1"); if (gApp.log) gh_log("HINT SDL_MOUSE_FOCUS_CLICKTHROUGH=1 (t48c #639 ②)");   /* t48a: Windows IME 조합·후보 창을 시스템이 그리게 한다 */
    if (!gApp.inject_file.empty()) SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");   /* t48e2: --inject 시험 전용(숨김 창에서도 가상 패드 이벤트 수신). 평소 동작 불변 */
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | (gApp.headless ? 0 : SDL_INIT_VIDEO)) < 0)
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    gApp.log_t0 = SDL_GetTicks();
    gh_log("gensei_host " GENSEI_BUILD_ID " start, profile='%s' headless=%d ui=%d stubs=%d autokeys=%d", id.c_str(), (int)gApp.headless, (int)ui_mode, gensei_stub_n, (int)gApp.autokeys);
    if (gApp.batest) { gh_batest(); gh_finish(0); }
}

static int gh_main_audio_prof(std::string & id, const bool & ui_mode) {
    {   /* r011: 오디오 출력 장치 목록·선택 (믹서가 열리기 전에 gensei_audio_dev 를 정한다) */
        int nd = SDL_GetNumAudioDevices(0);
        for (int i = 0; i < nd; i++) { const char * n = SDL_GetAudioDeviceName(i, 0); gh_log("audio device #%d: %s", i, n ? n : "?"); if (gApp.listaudio) printf("audio device #%d: %s\n", i, n ? n : "?"); }
        if (!gApp.audiodev.empty()) {
            static std::string chosen;
            for (int i = 0; i < nd; i++) { const char * n = SDL_GetAudioDeviceName(i, 0); if (n && strstr(n, gApp.audiodev.c_str())) { chosen = n; break; } }
            if (!chosen.empty()) { gensei_audio_dev = chosen.c_str(); gh_log("audio device chosen: %s", chosen.c_str()); }
            else gh_log("audio device '%s' not found -> default", gApp.audiodev.c_str());
        }
        if (gApp.listaudio) { fflush(stdout); gh_finish(0); }
    }
    if (!ui_mode && gApp.uitest_dir.empty() && !(id.empty() && (gApp.scanonly || !gApp.addwork_cli.empty())) && !gh_profile(id, gApp.prof_)) { fprintf(stderr, "usage: gensei_host <hukyou|kaitou|kitan|torimono> [--imgdir D] [--out D] [--headless] [--sec N] [--shot a,b]\n"); gh_log("ERROR: unknown profile '%s'", id.c_str()); gh_msgbox(phr_("알 수 없는 작품 이름입니다.\nPC-98_PLAYER.exe 를 직접 열어 작품을 고르세요.")); return 1; }
    return -1;   /* S9c */
}
