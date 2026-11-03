/* app/crash_inwin.h -- S9b: crash handlers, finish, spawn, in-window cycle. Moved unchanged from gensei_host.cpp lines 1993-2204 (as of t54t). Included in place; single translation unit. */
/* ------------------------------------------------------------------ misc */
#if defined(WIN32)
#include <dbghelp.h>
static void gh_dump_stubs(void);
/* r011b: 실패는 로그 파일로만 끝내지 않고 메시지 창으로 이유를 보인다(헤드리스 시험 실행은 제외) */
static void gh_msgbox(const char * utf8text) {
    if (gApp.headless) return;
    plat::msgbox_utf8(utf8text, 1, 1024);
}
static LONG WINAPI gh_seh(EXCEPTION_POINTERS * ep) {
    static volatile LONG busy = 0;
    if (InterlockedExchange(&busy, 1)) TerminateProcess(GetCurrentProcess(), 0xC5);
    EXCEPTION_RECORD * er = ep->ExceptionRecord;
    gh_log("CRASH code=%08lx addr=%p", (unsigned long)er->ExceptionCode, er->ExceptionAddress);
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        gh_log("  access violation: %s address %p", er->ExceptionInformation[0] == 1 ? "write" : (er->ExceptionInformation[0] == 8 ? "exec" : "read"), (void *)er->ExceptionInformation[1]);
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf; memset(&sf, 0, sizeof(sf));
    sf.AddrPC.Offset = ctx.Rip; sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Rsp; sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 40; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
        if (!sf.AddrPC.Offset) break;
        char sbuf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO * sym = (SYMBOL_INFO *)sbuf; memset(sym, 0, sizeof(SYMBOL_INFO));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
        DWORD64 d64 = 0; DWORD d32 = 0; IMAGEHLP_LINE64 ln; memset(&ln, 0, sizeof(ln)); ln.SizeOfStruct = sizeof(ln);
        bool hs = SymFromAddr(proc, sf.AddrPC.Offset, &d64, sym) != 0;
        bool hl = SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &d32, &ln) != 0;
        gh_log("  #%02d %p %s+0x%llx %s:%u", i, (void *)sf.AddrPC.Offset, hs ? sym->Name : "?", (unsigned long long)d64, hl ? ln.FileName : "", hl ? (unsigned)ln.LineNumber : 0u);
    }
    gh_dump_stubs();
    if (gApp.log) { fflush(gApp.log); }
    gh_msgbox(phr_("프로그램 내부에서 예기치 않은 오류가 났습니다.\n로그 폴더의 kyuhachi_log.txt 를 보내 주세요."));
    TerminateProcess(GetCurrentProcess(), 0xC5);
    return EXCEPTION_EXECUTE_HANDLER;
}
static LONG CALLBACK gh_veh(EXCEPTION_POINTERS * ep) {
    if (!gApp.traceexc || ep->ExceptionRecord->ExceptionCode != 0xE06D7363 || gApp.traceexc_n >= 6) return EXCEPTION_CONTINUE_SEARCH;
    gApp.traceexc_n++;
    gh_log("C++EXC #%d (E06D7363)", gApp.traceexc_n);
    HANDLE proc = GetCurrentProcess(); static bool symok = false;
    if (!symok) { SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS); SymInitialize(proc, NULL, TRUE); symok = true; }
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf; memset(&sf, 0, sizeof(sf));
    sf.AddrPC.Offset = ctx.Rip; sf.AddrPC.Mode = AddrModeFlat; sf.AddrFrame.Offset = ctx.Rsp; sf.AddrFrame.Mode = AddrModeFlat; sf.AddrStack.Offset = ctx.Rsp; sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 28; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
        if (!sf.AddrPC.Offset) break;
        char sbuf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO * sym = (SYMBOL_INFO *)sbuf; memset(sym, 0, sizeof(SYMBOL_INFO)); sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
        DWORD64 d64 = 0; DWORD d32 = 0; IMAGEHLP_LINE64 ln; memset(&ln, 0, sizeof(ln)); ln.SizeOfStruct = sizeof(ln);
        bool hs = SymFromAddr(proc, sf.AddrPC.Offset, &d64, sym) != 0; bool hl = SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &d32, &ln) != 0;
        gh_log("  #%02d %s+0x%llx %s:%u", i, hs ? sym->Name : "?", (unsigned long long)d64, hl ? ln.FileName : "", hl ? (unsigned)ln.LineNumber : 0u);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static DWORD WINAPI gh_boxkiller(LPVOID) {
    for (;;) {
        plat::sleep_ms(200);
        if (plat::close_window_titled("E_Exit")) { gApp.eexit_seen = true; if (gApp.log) { fprintf(gApp.log, "HOST E_Exit message box closed by watchdog\n"); fflush(gApp.log); } }
    }
    return 0;
}
/* S9e-N 2: install crash handlers and watchdog in one function; callers carry no OS names */
static void gh_crash_install(void) {
    SetUnhandledExceptionFilter(gh_seh); AddVectoredExceptionHandler(1, gh_veh);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    CreateThread(NULL, 0, gh_boxkiller, NULL, 0, NULL);
}
#else
static void gh_crash_install(void) {}   /* non-Windows: nothing to install */
#endif

static void gh_dump_stubs(void) {
    FILE * f = NULL;
    std::string p = gApp.outdir + "\\stub_calls.txt";
    f = fopen(p.c_str(), "w");
    if (!f) return;
    int n = gensei_stub_n, called = 0;
    for (int i = 0; i < n; i++) if (gensei_stub_counts[i]) { fprintf(f, "%u\t%s\n", gensei_stub_counts[i], gensei_stub_names[i]); called++; }
    fprintf(f, "# stubs total=%d called=%d\n", n, called);
    fclose(f);
}

static void Gh_Null_Init(Section *) {}

/* dump + hard exit: the extracted modules are not built to be torn down (skip static/Config destructors) */
static void gh_finish(int rc) {
    gh_log("exit rc=%d frames=%llu", rc, gApp.frames);
    if (gApp.stage_go) { gApp.stage_go = false; gh_spawn_stage(gApp.stage_next); }   /* t49a (#845): 다음 단계 = 새 프로세스 */
    if (!gApp.headless && gApp.wm_dirty) { gApp.wm_dirty = false; ui::settings_save(gApp.ui_, gApp.cfgpath); }
    gApp.shacache.save();
    if (!gApp.headless && gApp.boot_tick && gh_play_commit()) ui::settings_save(gApp.ui_, gApp.cfgpath);
    gh_dump_stubs();
    if (gApp.log) { fflush(gApp.log); fclose(gApp.log); gApp.log = NULL; }
    fflush(NULL);
#if defined(WIN32)
    if (rc >= 2 && rc <= 4) gh_msgbox(rc == 2 ? (gApp.errmsg.empty() ? std::string(phr_("게임 디스크 이미지를 열 수 없습니다.\nimg 폴더에 이미지가 있는지 확인해 주세요. (자세한 내용: logs 폴더의 kyuhachi_log.txt)")) : gApp.errmsg + phr_("\n(자세한 내용: logs 폴더의 kyuhachi_log.txt)")).c_str()
                                   : phr_("실행 중 오류로 종료했습니다.\n(자세한 내용: logs 폴더의 kyuhachi_log.txt)"));
    else if (gApp.eexit_seen) gh_msgbox(phr_("에뮬레이터 내부 오류로 종료했습니다.\n(자세한 내용: logs 폴더의 kyuhachi_log.txt)"));
#endif
    _exit(rc);
}

/* ---- t51a (#878 묶음 2): 창 안 전환. 일시정지 메뉴의 「다음 단계」·「메인 메뉴로」 를 창·프로세스 그대로 처리한다.
   흐름: RunMachine 을 빠져나온 직후(에뮬이 안 도는 때) 정리 → 「불러오는 중」 → (메인 메뉴면 같은 창에서 gh_ui_main) → 새 프로필 읽기·범위 판정 → 디스크 다시 연결 →
   원본 reboot 순서(CPU_Snap_Back_To_Real_Mode · CPU_Snap_Back_Forget · VM_EVENT_RESET · MAPPER_ReleaseAllKeys/LosingFocus · VM_EVENT_RESET_END) → main 이 RunMachine 을 다시 부른다.
   실패(디스크 연결 · 프로필 · 범위 밖 · 예외 · 8초 부팅 신호 없음)는 현행 재실행 경로(gh_spawn_stage / gh_spawn_menu / gh_spawn_game + gh_finish 의 _exit)로 폴백 — 새 창을 만들기 전에 이 프로세스가 끝나므로 창은 늘 하나. */
static void gh_spawn_game(const std::string & id, const std::string & entry, int resume) {   /* 메뉴에서 고른 작품을 새 프로세스로(폴백 전용) */
    std::string exe = plat::exe_path();
    std::string cmd = "\"" + exe + "\" \"" + id + "\" --noboot"; if (!entry.empty()) cmd += " --entry \"" + aw::u8_to_acp(entry) + "\""; if (resume > 0) cmd += " --resume " + std::to_string(resume);
    unsigned long perr = 0;
    if (plat::spawn(exe, cmd, gApp.exedir, &perr)) { gh_log("spawn game OK: %s", cmd.c_str()); }
    else gh_log("spawn game FAIL err=%lu", (unsigned long)perr);
}
static std::string gh_conf_sig(const GProfile & p) {   /* 시작 때 한 번만 코어 설정에 들어가는 프로필 값(cycles · 글꼴 · extra · sound). 다르면 같은 창 재부팅으로 못 바꾸므로 폴백 */
    auto kv = [&](const char * k) { auto it = p.kv.find(k); return it == p.kv.end() ? std::string() : it->second; };
    return p.cycles + "|" + p.font + "|" + kv("font_kind") + "|" + p.extra + "|" + kv("sound");
}
static bool gh_prof_isdos(const GProfile & p) {
    bool dosfd = p.kv.count("dosfd") && p.kv.at("dosfd") == "1" && (p.disk[0] != NULL || p.disk[1] != NULL) && p.disk[2] == NULL && p.disk[3] == NULL;
    return dosfd || (p.disk[0] == NULL && p.disk[1] == NULL && p.disk[2] == NULL && p.disk[3] == NULL && !p.cd.empty());
}
static unsigned long long gh_mem_ws(void) {   /* 작업 집합(바이트). psapi 링크 없이 kernel32 의 K32GetProcessMemoryInfo 를 찾아 쓴다 */
    struct PMC { DWORD cb, PageFaultCount; SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage; };
    typedef BOOL (WINAPI * Fn)(HANDLE, PMC *, DWORD); static Fn fn = (Fn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetProcessMemoryInfo");
    PMC m; memset(&m, 0, sizeof m); m.cb = sizeof m; if (fn && fn(GetCurrentProcess(), &m, sizeof m)) return (unsigned long long)m.WorkingSetSize; return 0ull;
}
static void gh_inwin_stat(const char * tag) {   /* 시험 판정용: 프로세스 ID · 창 핸들 · 핸들 수 · 메모리 · 창 크기/전체화면/맞춤/셰이더/배속/패드 */
    DWORD hc = 0; GetProcessHandleCount(GetCurrentProcess(), &hc); int ww = 0, wh = 0; unsigned fl = 0; if (gApp.win) { SDL_GetWindowSize(gApp.win, &ww, &wh); fl = SDL_GetWindowFlags(gApp.win); }
    gh_log("INWIN %s pid=%lu win=%p glctx=%p handles=%lu ws=%lluKB boot_n=%d inwin_n=%d size=%dx%d fs=%d fit=%d shader=%d boost=%d joys=%d fb=%dx%d", tag, (unsigned long)GetCurrentProcessId(), (void *)gApp.win, (void *)gApp.glctx, (unsigned long)hc, gh_mem_ws() / 1024ull, gApp.boot_n, gApp.inwin_n, ww, wh, (fl & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP ? 1 : 0, (int)gApp.fit, (int)gApp.shader, (int)gApp.boost_on, SDL_NumJoysticks(), gApp.fbw, gApp.fbh);
}
static bool gh_inwin_loading(unsigned ms) {   /* 검은 화면 + 「불러오는 중」 한 줄. true = 도중에 창 닫기 요청 */
    if (gApp.headless || !gApp.use_gl) return false;
    static uint32_t buf[640 * 400]; ov::Canvas c = { buf, 640, 400 }; bool quit = false; unsigned t0 = SDL_GetTicks();
    std::string s = phr_("불러오는 중..."); int x = (640 - ov::tw(s)) / 2; if (x < 0) x = 0;
    do {
        SDL_Event e; while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; else gh_wm_hotkey(e); }
        gh_wm_poll();
        c.rect(0, 0, 640, 400, ov::K); ov::text(c, x, 192, s, ov::WH, ov::K);
        gh_publish(buf, 640, 400);
        if (gApp.inwin_testfail == "shot" && SDL_GetTicks() - t0 > 200u) { static int shot_n = 0; if (shot_n < 99) { char pp[1024]; snprintf(pp, sizeof pp, "%s\\inwin_loading_%02d.png", gApp.outdir.c_str(), shot_n++); gh_png(pp, buf, 640, 400); gh_log("INWIN loading 화면 저장 %s", pp); } t0 -= 100000u; }   /* 시험: 불러오는 중 화면 1장 저장(그 뒤 바로 끝냄) */
        SDL_Delay(16);
    } while (!quit && SDL_GetTicks() - t0 < ms);
    return quit;
}
/* true = 같은 창에서 다시 부팅 준비 끝(main 이 RunMachine 재진입) · false = 폴백 준비를 마침(main 이 gh_finish) */
static bool gh_inwin_cycle(void) {
    int kind = gApp.inwin_req; gApp.inwin_req = 0; bool tmo = gApp.inwin_timeout; gApp.inwin_timeout = false; gApp.inwin_wait = false;
    auto fail = [&](const std::string & w) -> bool {
        gh_log("STAGE 창 안 전환 실패(%s) → 재실행 (종류=%d)", w.c_str(), gApp.inw_kind);
        if (gApp.inw_kind == 2) { if (gApp.inw_menu_pick) gh_spawn_game(gApp.inw_sel_id, prof::g_entry_req, gApp.resume_slot); else gh_spawn_menu(); }
        else { gApp.stage_next = gApp.inwin_target; gApp.stage_go = true; }
        return false; };
    if (tmo) return fail("부팅 신호가 8초 안에 없음");
    gApp.inw_kind = kind; gApp.inw_menu_pick = false;
    try {
        gh_inwin_stat(kind == 2 ? "전환 시작(메인 메뉴)" : "전환 시작(다음 단계)");
        gh_log("INWIN rms 이전 구간 samples=%llu rms=%.2f", gensei_out_n, gensei_out_n ? sqrt(gensei_out_sumsq / (double)gensei_out_n) : 0.0);
        { unsigned nw = SDL_GetTicks(); if (gApp.boot_tick) gApp.inj_base += nw - gApp.boot_tick; gApp.inj_stamp = nw | 1u; gApp.boot_tick = 0; }   /* 시험 inject 시계는 전환을 건너 이어 간다(제품 동작 무관) */
        gh_play_commit(); if (!gApp.headless) ui::settings_save(gApp.ui_, gApp.cfgpath);
        kbov::release_all(); fflush(NULL);   /* 디스크 작업 사본에 쓰던 내용을 먼저 내보낸다(새 연결이 같은 파일을 다시 열기 때문) */
        std::string sel_id = gApp.pid;
        if (kind == 2) {
            std::string chosen; bool nb = gApp.noboot; gApp.noboot = true; gApp.quit = false; gApp.resume_slot = 0; prof::g_entry_req.clear();
            bool ok = gh_ui_main(chosen); gApp.noboot = nb;
            if (!ok) { gh_log("ui: quit (창 안 메인 메뉴)"); gh_finish(0); }
            sel_id = chosen; gApp.inw_sel_id = chosen; gApp.inw_menu_pick = true;
            gh_log("INWIN 메인 메뉴에서 고름: '%s' 항목='%s' resume=%d", chosen.c_str(), prof::g_entry_req.c_str(), gApp.resume_slot);
        } else prof::g_entry_req = gApp.inwin_target;
        GProfile np; if (!gh_profile(sel_id, np)) return fail("프로필 읽기");
        if (gApp.dosmode || gApp.dosfd) return fail("지금 내장 DOS/CD 부팅(범위 밖)");
        if (gApp.demo) return fail("--demo 시작(범위 밖)");
        if (gh_prof_isdos(np)) return fail("다음이 내장 DOS/CD 부팅(범위 밖)");
        if (np.disk[2] || np.disk[3] || gApp.prof_.disk[2] || gApp.prof_.disk[3] || imageDiskList[2] || imageDiskList[3]) return fail("HDD 칸 있음(범위 밖)");
        if (!np.disk[0] && !np.disk[1]) return fail("부팅 가능한 디스크 없음");
        if (np.bootletter != 'A' && np.bootletter != 'B') return fail("부팅 드라이브가 A/B 가 아님");
        for (int i = 0; i < 2; i++) if (imageDiskList[i] && !np.disk[i]) return fail("다음 항목의 칸이 지금보다 적음");
        if (gh_conf_sig(np) != gh_conf_sig(gApp.prof_)) return fail("코어 설정(cycles·글꼴·extra·sound)이 지금과 다름");
        if (gApp.inwin_testfail == "attach") return fail("시험 주입: 디스크 연결 실패");
        if (gh_inwin_loading(450)) { gh_log("창 닫기 요청(전환 중)"); gh_finish(0); }
        gApp.prof_ = np; gApp.pid = sel_id;
        for (size_t i = 0; i < gApp.ui_.games.size(); i++) if (gApp.ui_.games[i].id == sel_id) gApp.cur_game = (int)i;
        if (!gApp.imgdir_given && !gApp.prof_.imgdir.empty()) gApp.imgdir = gApp.prof_.imgdir;
        gApp.disks.clear(); for (int i = 0; i < 4; i++) gApp.disk_ix[i] = -1;
        for (int i = 0; i < 2; i++) if (gApp.prof_.disk[i] && !(gApp.noB && i == 1)) if (!gh_attach(i, gApp.prof_.disk[i])) return fail(std::string("디스크 연결 실패: ") + gApp.prof_.disk[i]);
        if (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) gh_rebuild_fds(gApp.prof_, gApp.ui_.games[gApp.cur_game]);
        { auto mk = gApp.prof_.kv.find("mouse"); gApp.mouse_game = (mk != gApp.prof_.kv.end() && !mk->second.empty() && mk->second != "none") || !gApp.amouse.empty(); gh_log("mouse: 이 작품의 마우스 = %s (창 안 전환)", mk == gApp.prof_.kv.end() ? "(미지정)" : mk->second.c_str()); }
        gh_apply_profile_input(gApp.prof_);
        gApp.ui_.cur = gApp.cur_game;
        if (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) {
            gh_refresh_game_slots(gApp.cur_game); gh_apply_settings(gApp.ui_.effective(gApp.cur_game), false);
            if (gApp.win) SDL_SetWindowTitle(gApp.win, gh_window_title(gApp.ui_.games[gApp.cur_game], gApp.titlelegacy).c_str());
        }
        gh_stage_init();
        if (gApp.inwin_testfail == "throw") throw std::runtime_error("시험 주입: 예외");
        /* 원본 reboot 순서 (sdlmain.cpp L10601~10628) */
        void CPU_Snap_Back_To_Real_Mode(); void CPU_Snap_Back_Forget(); void MAPPER_ReleaseAllKeys(); void MAPPER_LosingFocus(void);
        CPU_Snap_Back_To_Real_Mode(); CPU_Snap_Back_Forget();
        DispatchVMEvent(VM_EVENT_RESET);
        MAPPER_ReleaseAllKeys(); MAPPER_LosingFocus();
        /* 호스트 쪽 부팅 상태 되돌림(설계 §4) */
        gApp.boot_tick = 0; gApp.pon_tick = SDL_GetTicks() | 1u; gApp.pon_pic = PIC_Ticks; gApp.loading = true;
        gApp.next_bk = 0; gApp.bk_started = false; gApp.bk_t = 0; gApp.bk_paused = false; gApp.bk_last = SDL_GetTicks(); gApp.bkp_a = gApp.bkp_b = false;
        gApp.next_key = 0; gApp.play_last = 0; gApp.menu_open = false; gApp.key_down = KBD_NONE; gApp.swap_done = false; kbov::wake();
        gensei_out_sumsq = 0.0; gensei_out_n = 0ull;
        gApp.quit = false; gApp.boot_n++; gApp.inwin_n++;
        DispatchVMEvent(VM_EVENT_RESET_END);
        gApp.inwin_wait = true; gApp.inwin_t0 = SDL_GetTicks();
        gh_log("STAGE 창 안 전환: 같은 창에서 리셋 n=%d 작품='%s' 항목='%s' A=%s B=%s", gApp.inwin_n, sel_id.c_str(), prof::g_entry_req.c_str(), gApp.prof_.disk[0] ? gApp.prof_.disk[0] : "-", gApp.prof_.disk[1] ? gApp.prof_.disk[1] : "-");
        gh_inwin_stat("리셋 직후");
        return true;
    } catch (const std::exception & ex) { return fail(std::string("예외 ") + ex.what()); }
    catch (...) { return fail("예외"); }
}
