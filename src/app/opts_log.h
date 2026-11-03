/* app/opts_log.h -- S9b: log, profile struct, run-option globals, diagnostics, inject. Moved unchanged from gensei_host.cpp lines 124-361 (as of t54t). Included in place; single translation unit. */
/* ------------------------------------------------------------------ log */
static FILE * g_log = NULL;
static unsigned long long g_log_bytes = 0;
static unsigned int g_log_t0 = 0;
/* r019: 로그는 UTF-8. 한글 문장(소스는 UTF-8)과 시스템 경로(ACP)가 한 줄에 섞이므로 3바이트 한글은 그대로, 나머지 8비트 구간은 ACP 로 보고 UTF-8 로 바꾼다 */
static std::string gh_mixed_u8(const char * s, int n) {
    std::string o; int i = 0;
    auto cont = [&](int k) { return k < n && (((unsigned char)s[k]) & 0xC0) == 0x80; };
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { o += (char)c; i++; continue; }
        if ((c & 0xF0) == 0xE0 && cont(i + 1) && cont(i + 2)) { o.append(s + i, 3); i += 3; continue; }
        if (c == 0xC2 && i + 1 < n && (unsigned char)s[i + 1] == 0xB7 && (i == 0 || s[i - 1] == ' ') && (i + 2 >= n || s[i + 2] == ' ')) { o.append(s + i, 2); i += 2; continue; }
        int j = i; while (j < n && (unsigned char)s[j] >= 0x80) { if ((((unsigned char)s[j]) & 0xF0) == 0xE0 && cont(j + 1) && cont(j + 2) && j > i) break; j++; }
        { std::string u; if (plat::acp_bytes_to_u8(s + i, j - i, u)) o += u; }
        i = j;
    }
    return o;
}
static void gh_vlog(const char * pfx, const char * fmt, va_list ap) {
    if (!g_log) return;
    if (g_log_bytes > (48ull << 20)) return;   /* cap 48 MB */
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    buf[n] = 0;
    unsigned int t = SDL_GetTicks() - g_log_t0;
    std::string u8 = gh_mixed_u8(buf, n);
    g_log_bytes += fprintf(g_log, "%7u %s%s%s", t, pfx, u8.c_str(), (n > 0 && buf[n - 1] == '\n') ? "" : "\n");
}
static void gh_log(const char * fmt, ...) { va_list ap; va_start(ap, fmt); gh_vlog("HOST ", fmt, ap); va_end(ap); fflush(g_log); }
static std::string g_errmsg;   /* t48a (#614 ④): 사용자 화면에 보일 사람 말 오류 문구(비면 일반 문구) */
static void gh_msgbox(const char * utf8text);
static void gh_human(const std::string & msg) { g_errmsg = msg; gh_log("안내(사용자 화면): %s", msg.c_str()); }

void DEBUG_ShowMsg(char const * format, ...) { va_list ap; va_start(ap, format); gh_vlog("MSG  ", format, ap); va_end(ap); }
void LOG::operator() (char const * format, ...) {
    const char * sv = "";
    switch (d_severity) { case LOG_DEBUG: sv = "D "; break; case LOG_NORMAL: sv = "N "; break; case LOG_WARN: sv = "W "; break; case LOG_ERROR: sv = "E "; break; case LOG_FATAL: sv = "F "; break; default: break; }
    va_list ap; va_start(ap, format); gh_vlog(sv, format, ap); va_end(ap);
}

/* ------------------------------------------------------------------ profile */
struct GProfile {
    const char * id = "";
    const char * disk[4] = { NULL, NULL, NULL, NULL };   /* index 0=A 1=B 2=C 3=D (bios_disk index) */
    char bootletter = 'A';       /* 'A'..'D' */
    std::vector<GKey> keys;      /* ms after guest boot */
    std::vector<GKey> bootkeys;  /* t36cd: 부팅 후 키·명령 주입(--autokeys 없이도 동작). bootkeys=3000:type=dir~,9000:enter */
    std::string cd, autoexec;    /* t36cd: CD 슬롯(이미지 경로 또는 이름) · 내장 DOS 의 autoexec 줄(| 로 구분) */
    std::string title, engine, src, reason, imgdir, cycles, font, extra, never_map;
    std::map<std::string, std::string> ov, pad, kv;
    std::vector<std::string> swap; bool install_required = false;
};
static const char * const g_conf_common =
    "[dosbox]\n"
    "machine=pc98\n"
    "captures=capture\n"
    "[cpu]\n"
    "core=normal\n"
    "cycles=fixed 3000\n"
    "[render]\n"
    "scaler=none\n"
    "aspect=false\n"
    "[mixer]\n"
    "rate=44100\n"
    "blocksize=512\n"
    "prebuffer=20\n"
    "[pc98]\n"
    "pc-98 anex86 font=@FONT@\n";


/* ------------------------------------------------------------------ run options */
static std::string g_imgdir = ".", g_outdir = "."; static bool g_imgdir_given = false;
static bool g_headless = false;
static bool g_gltime = false;   /* t42(#313): --gltime — 렌더 스레드 그리기 시간(glFinish 기준) 3초 평균을 로그 한 줄로 */
static bool g_hidewin = false;   /* t41b (#308): 시험 전용 — 보이지 않는 GL 창(SDL_WINDOW_HIDDEN)으로 실행, 슨상 화면에 창이 뜨지 않음 */
static unsigned int g_run_ms = 0;
static std::vector<unsigned int> g_shots_ms;
static bool g_vol_forced = false; extern double gensei_out_sumsq; extern unsigned long long gensei_out_n;
static int g_setvol_b = -1, g_setvol_s = -1; static unsigned int g_rms_from = 0; static bool g_rms_started = false;   /* t35 시험용 */
static GProfile g_prof;
/* r009 세이브스테이트 시험: --save SEC / --load SEC (게임 부팅 후 벽시계 초) --state FILE, 이벤트 뒤 0.1s·1s·3s 캡처 */
static unsigned int g_save_ms = 0, g_load_ms = 0;
static std::string g_state_path;
static std::string g_note; static uint32_t g_note_until = 0;   /* t43h: 메뉴 안내 한 줄(3초) */
static std::string g_savedir = "saves";   /* r011: 슬롯 저장 폴더(저장/<게임>/slot01.g98s) */
static bool g_autokeys = false;           /* r011: 시험용 자동 키 입력(--autokeys). 실기 실행에서는 꺼짐 */
static size_t g_next_bk = 0;              /* t36cd: bootkeys 다음 순번(--autokeys 와 무관하게 항상 동작) */
/* t47a (#559): 자동 입력 전용 시계 g_bk_t — 일시정지(Alt+P)·게임 메뉴가 열린 시간은 흐르지 않는다. 멈춘 적이 없으면 부팅 후 경과(since)와 같다. */
static bool g_bk_osd_on = false;          /* --bkosd 1 (시험 실행 △ 이 붙임): 단계 OSD 줄 표시 */
static bool g_bk_paused = false, g_bk_started = false;
static unsigned g_bk_t = 0, g_bk_last = 0, g_bk_done_at = 0;
static unsigned g_bkp_from = 0, g_bkp_to = 0; static bool g_bkp_a = false, g_bkp_b = false;   /* --bkpause 시작ms,끝ms (시험용) */
static std::string g_bk_osd;              /* 비면 줄 없음. 주 스레드가 쓰고 오버레이 합성(같은 스레드)이 읽는다 */

static bool g_dosfd = false;              /* t43: FD 가 스스로 부팅 못 함(DOS 시스템 없음) → 내장 DOS 에 A:/B: 장착 */
static bool g_dosmode = false;            /* t36cd: CD 슬롯 → 내장 DOS 로 부팅(디스크가 없고 cd= 가 있을 때) */
static std::string g_entry_cli;           /* t36cd: --entry <이름> */
struct GMouse { unsigned at_ms; float dx, dy; int act; };   /* act: 0 이동만, 1 좌 누름, 2 좌 뗌, 3 우 누름, 4 우 뗌 */
static bool g_dumpcode = false;
static bool g_boot400 = false;
static bool g_bda = false;
static bool g_mirq12 = false;   /* --mouseirq12: old r020 (MOUSE_IRQ stayed at the AT default 12) */
static std::string g_flaglog; static unsigned g_keyhold = 80; static bool g_keytrace = false;   /* t29 (#266): --keyhold <ms> 자동키 누름 길이 · --keytrace BDA 키 버퍼(0x502~)·PIC 마스크 로그 */
static bool g_int18trace = false;   /* t31 (#267): --int18trace  INT 18h 콜백을 감싸 AX(AH=기능,AL=그룹)·호출 위치를 2초마다 요약 */
static CallBack_Handler g_orig18 = NULL; static std::map<unsigned, unsigned> g_i18_ax, g_i18_caller; static unsigned g_i18_last = 0, g_i18_n = 0, g_i18_non04 = 0;
static Bitu gh_int18_wrap(void) {
    unsigned ax = (unsigned)reg_ax; g_i18_ax[ax]++; g_i18_n++;
    { PhysPt sp = SegPhys(ss) + (cpu.stack.big ? reg_esp : (reg_sp & 0xffff)); unsigned ip = mem_readw(sp), cs_ = mem_readw(sp + 2); g_i18_caller[(cs_ << 16) | ip]++; }
    if ((ax >> 8) != 0x04 && g_i18_non04 < 60) { g_i18_non04++; gh_log("int18: AX=%04x BX=%04x CX=%04x DX=%04x (AH=%02x 기능, 04 이외)", ax, (unsigned)reg_bx, (unsigned)reg_cx, (unsigned)reg_dx, ax >> 8); }
    unsigned now = SDL_GetTicks();
    if (now - g_i18_last >= 2000) { g_i18_last = now;
        std::vector<std::pair<unsigned, unsigned>> v; for (auto & kv : g_i18_ax) v.push_back({ kv.second, kv.first }); std::sort(v.begin(), v.end(), [](const std::pair<unsigned, unsigned> & a, const std::pair<unsigned, unsigned> & b) { return a.first > b.first; });
        std::string s; for (size_t i = 0; i < v.size() && i < 8; i++) { char b[40]; snprintf(b, sizeof b, " %04x:%u", v[i].second, v[i].first); s += b; }
        std::vector<std::pair<unsigned, unsigned>> c; for (auto & kv : g_i18_caller) c.push_back({ kv.second, kv.first }); std::sort(c.begin(), c.end(), [](const std::pair<unsigned, unsigned> & a, const std::pair<unsigned, unsigned> & b) { return a.first > b.first; });
        std::string t; for (size_t i = 0; i < c.size() && i < 5; i++) { char b[48]; snprintf(b, sizeof b, " %04x:%04x=%u", c[i].second >> 16, c[i].second & 0xffff, c[i].first); t += b; }
        gh_log("int18sum: 누적 %u회 | AX별 상위:%s | 호출 위치 상위:%s", g_i18_n, s.c_str(), t.c_str()); g_i18_ax.clear(); g_i18_caller.clear(); }
    return g_orig18 ? g_orig18() : 0;
}
static void gh_install_int18trace(void) {
    unsigned off = mem_readw(0x18 * 4), seg = mem_readw(0x18 * 4 + 2); PhysPt p = (PhysPt)(seg * 16 + off);
    for (int i = 0; i < 8; i++) if (mem_readb(p + i) == 0xFE && mem_readb(p + i + 1) == 0x38) { unsigned nr = mem_readw(p + i + 2); g_orig18 = CallBack_Handlers[nr]; Bitu n2 = CALLBACK_Allocate(); CallBack_Handlers[n2] = gh_int18_wrap; phys_writew(p + i + 2, (uint16_t)n2); gh_log("int18trace: IVT18=%04x:%04x 콜백 %u -> %u 로 감쌈 (쓰기 뒤 읽어 보기: %u, 핸들러 교체 %s)", seg, off, nr, (unsigned)n2, (unsigned)mem_readw(p + i + 2), CallBack_Handlers[n2] == gh_int18_wrap ? "ok" : "NO"); return; }
    gh_log("int18trace: IVT18=%04x:%04x 에서 FE 38 콜백 명령을 못 찾음", seg, off);
}
static unsigned g_segdump_ms = 0, g_hist_from = 0, g_hist_to = 0;   /* t30 (#267): --segdump <ms> CS/DS/ES/SS 64KB 덤프 · --hist <from>,<to> CS:EIP 표본 히스토그램 */
static bool g_di0 = true;   /* DI=0 (old r020); FFFE hypothesis rejected in t26 */
static bool g_ext14 = false;   /* --ext14: keep r020 behaviour (BDA 0401h = extended memory 14MB at boot) */
static bool g_ext0 = false;
static bool g_trace21 = false; static int g_watch_off = -1;   /* r021 진단: --trace21 (IDT/IVT 핸들러 덤프·PIC 마스크 변화·마우스 레지스터 변화 로그), --watch <16진 오프셋> (DS:off 바이트 변화 로그) */
static void gh_tr21_dump_vectors(const char * tag) {
    gh_log("tr21 vectors [%s] pmode=%d", tag, (int)cpu.pmode);
    for (int v = 0; v < 0x20; v++) {
        unsigned sel = 0, off = 0; PhysPt hp = 0; bool ok = true;
        if (cpu.pmode) {
            PhysPt g = cpu.idt.GetBase() + (PhysPt)v * 8u; unsigned type = (unsigned)mem_readb(g + 5) & 0x1fu;
            off = (unsigned)mem_readw(g); if (type & 8u) off |= ((unsigned)mem_readw(g + 6) << 16);
            sel = (unsigned)mem_readw(g + 2);
            if ((sel & 4u) || (sel & ~7u) == 0) ok = false;
            else { PhysPt d = cpu.gdt.GetBase() + (PhysPt)(sel & ~7u); hp = (PhysPt)(((unsigned)mem_readw(d + 2)) | (((unsigned)mem_readb(d + 4)) << 16) | (((unsigned)mem_readb(d + 7)) << 24)) + off; }
        } else { off = (unsigned)mem_readw((PhysPt)v * 4u); sel = (unsigned)mem_readw((PhysPt)v * 4u + 2u); hp = ((PhysPt)sel << 4) + off; }
        if (!ok) { gh_log("tr21 vec %02x: sel=%04x off=%08x (LDT/널 — 생략)", v, sel, off); continue; }
        char hx[128]; int n = 0; for (int i = 0; i < 24; i++) n += snprintf(hx + n, sizeof hx - n, "%02x ", (unsigned)mem_readb(hp + i));
        char wr[96] = ""; if (g_watch_off >= 0) {   /* 핸들러 앞 192바이트에서 DS:off 에 쓰는 명령(c6/c7/fe/ff 06 lo hi, 88/89 .. 06 lo hi) 찾기 */
            unsigned lo = (unsigned)g_watch_off & 0xff, hi = ((unsigned)g_watch_off >> 8) & 0xff; int m = 0;
            for (int i = 0; i < 192 && m < 3; i++) { unsigned b0 = mem_readb(hp + i), b1 = mem_readb(hp + i + 1), b2 = mem_readb(hp + i + 2), b3 = mem_readb(hp + i + 3);
                if ((b0 == 0xc6 || b0 == 0xc7 || b0 == 0xfe || b0 == 0xff || b0 == 0x88 || b0 == 0x89 || b0 == 0xa2 || b0 == 0xa3) && ((b1 == 0x06 && b2 == lo && b3 == hi) || ((b0 == 0xa2 || b0 == 0xa3) && b1 == lo && b2 == hi)))
                { char t[24]; snprintf(t, sizeof t, " WR@+%d(op%02x)", i, b0); if (strlen(wr) + strlen(t) < sizeof wr) strcat(wr, t); m++; } } }
        gh_log("tr21 vec %02x: %04x:%08x phys=%08x | %s%s", v, sel, off, (unsigned)hp, hx, wr);
    }
}
static std::vector<GMouse> g_amouse; static size_t g_next_mouse = 0; static bool g_mouse_game = false, g_relmouse = false, g_mouse_free = false;   /* r020: 시험용 자동 마우스(--automouse) · 이 작품이 마우스를 쓰는가 · SDL 상대 마우스 모드 상태 */
static bool g_mouseok_cli = false; static int g_ems_req = 1;   /* t38cd: EMS 기본 켬(t37cd 시험에서 마도456 60초 정상) · --noems 로 끄기 */
static bool gh_parse_amouse(const std::string & s) {   /* "ms:dx,dy,act;..." act = - | ld | lu | rd | ru */
    size_t p = 0; while (p < s.size()) { size_t e = s.find(';', p); if (e == std::string::npos) e = s.size(); std::string it = s.substr(p, e - p); p = e + 1; if (it.empty()) continue;
        GMouse m; m.at_ms = (unsigned)atoi(it.c_str()); m.dx = m.dy = 0; m.act = 0; size_t c = it.find(':'); if (c == std::string::npos) continue; std::string r = it.substr(c + 1);
        size_t c1 = r.find(','); if (c1 == std::string::npos) continue; m.dx = (float)atof(r.c_str()); size_t c2 = r.find(',', c1 + 1); m.dy = (float)atof(r.c_str() + c1 + 1);
        if (c2 != std::string::npos) { std::string a = r.substr(c2 + 1); m.act = a == "ld" ? 1 : a == "lu" ? 2 : a == "rd" ? 3 : a == "ru" ? 4 : 0; } g_amouse.push_back(m); }
    return !g_amouse.empty();
}
/* ---- t48e (#648/#651/#652): --inject <파일> — 시험 전용 입력 주입. 슨상의 실제 OS 마우스·키는 건드리지 않는다(SetCursorPos/mouse_event/SendInput 금지):
 *   ① SDL_PushEvent(컨트롤러 버튼·축 · 마우스 이동 xrel/yrel · 키)  ② SDL 가상 조이스틱(SDL_JoystickAttachVirtual)  ③ 플레이어 안 스크립트(uikeys)
 * 파일 한 줄 = "<ms> <명령> [인자...]" ('#' 줄·빈 줄 무시). ms 는 게임 시작(since) 기준.
 *   cbtn <표준버튼이름> <0|1>      SDL_CONTROLLERBUTTON 이벤트를 직접 밀어 넣음 (a b x y back start leftshoulder rightshoulder leftstick dpup dpdown dpleft dpright)
 *   tap <표준버튼이름>             누름 → 120ms 뒤 뗌
 *   caxis <lefty|leftx|triggerright|…> <값>   SDL_CONTROLLERAXISMOTION
 *   vpad <xinput|dinput|unknown> [kind]       가상 조이스틱 붙이기. xinput = Xbox 360 GUID(030000005e0400008e02000000000000)의 DB 매핑 · dinput = 일반 DInput(Logitech Dual Action 030000006d04000016c2000000000000)의 DB 매핑 · unknown = 매핑 없음(폴백)
 *   vstd <표준버튼이름> <0|1>      (vpad 로 붙인 장치) 그 매핑이 가리키는 물리 버튼/십자를 가상 조이스틱에 세팅 — SDL 매핑을 실제로 거친다
 *   vraw <버튼번호> <0|1> · vhat <값>   가상 조이스틱 원시 번호
 *   vdetach                        가상 조이스틱 떼기
 *   kind <0|1|2>                   표시용 패드 종류 강제(0 그 밖 1 Xbox 2 PlayStation)
 *   dbcheck <GUID 32자>            그 GUID 가 gamecontrollerdb 로 매핑이 잡히는지 로그
 *   mm <dx> <dy> · mb <l|r> <0|1>  SDL_MOUSEMOTION(xrel,yrel) / 버튼 이벤트
 *   key <SDL키이름> <0|1>          SDL_KEYDOWN/UP (창 포커스 없이)
 *   log <글>                       로그에 한 줄 */
static std::vector<GInj> g_inj; static size_t g_next_inj = 0; static std::string g_inject_file;
static SDL_Joystick * g_vj = NULL; static int g_vj_idx = -1; static std::string g_vj_map;
static bool gh_parse_inject(const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        std::string s = ln; while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back(); if (s.empty() || s[0] == '#') continue;
        GInj g; size_t p = 0; while (p < s.size()) { while (p < s.size() && s[p] == ' ') p++; size_t e = s.find(' ', p); if (e == std::string::npos) e = s.size(); if (e > p) g.a.push_back(s.substr(p, e - p)); p = e; }
        if (g.a.size() < 2) continue; g.at = (unsigned)atoi(g.a[0].c_str()); g.a.erase(g.a.begin());
        if (g.a[0] == "tap" && g.a.size() >= 2) { GInj d = g; d.a = { "cbtn", g.a[1], "1" }; GInj u = g; u.at = g.at + 120; u.a = { "cbtn", g.a[1], "0" }; g_inj.push_back(d); g_inj.push_back(u); continue; }
        if (g.a[0] == "vtap" && g.a.size() >= 2) { GInj d = g; d.a = { "vstd", g.a[1], "1" }; GInj u = g; u.at = g.at + 120; u.a = { "vstd", g.a[1], "0" }; g_inj.push_back(d); g_inj.push_back(u); continue; }
        g_inj.push_back(g);
    }
    fclose(f); std::stable_sort(g_inj.begin(), g_inj.end(), [](const GInj & x, const GInj & y) { return x.at < y.at; });
    return !g_inj.empty();
}
static std::string gh_mapfield(const std::string & map, const std::string & nm) {   /* "…,nm:xx,…" 의 xx */
    std::string k = "," + nm + ":"; size_t p = map.find(k); if (p == std::string::npos) return std::string(); p += k.size(); size_t e = map.find(',', p); return map.substr(p, e == std::string::npos ? std::string::npos : e - p);
}
static void gh_inj_run(const GInj & g, unsigned since) {
    const std::vector<std::string> & a = g.a; const std::string & c = a[0];
    gh_log("INJECT t=%u %s%s%s%s%s%s", since, c.c_str(), a.size() > 1 ? " " : "", a.size() > 1 ? a[1].c_str() : "", a.size() > 2 ? " " : "", a.size() > 2 ? a[2].c_str() : "", a.size() > 3 ? " ..." : "");
    if (c == "cbtn" && a.size() >= 3) { SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(a[1].c_str()); if (b == SDL_CONTROLLER_BUTTON_INVALID) { gh_log("INJECT cbtn 알 수 없는 이름 %s", a[1].c_str()); return; }
        bool d = a[2] != "0"; SDL_Event e; SDL_zero(e); e.type = d ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP; e.cbutton.which = g_vj ? SDL_JoystickInstanceID(g_vj) : 0; e.cbutton.button = (Uint8)b; e.cbutton.state = d ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&e); return; }
    if (c == "caxis" && a.size() >= 3) { SDL_GameControllerAxis x = SDL_GameControllerGetAxisFromString(a[1].c_str()); if (x == SDL_CONTROLLER_AXIS_INVALID) return; SDL_Event e; SDL_zero(e); e.type = SDL_CONTROLLERAXISMOTION; e.caxis.axis = (Uint8)x; e.caxis.value = (Sint16)atoi(a[2].c_str()); SDL_PushEvent(&e); return; }
    if (c == "kind" && a.size() >= 2) { gensei_padkind_v = atoi(a[1].c_str()); gh_log("INJECT padkind -> %d", gensei_padkind_v); return; }
    if (c == "log") { std::string t; for (size_t i = 1; i < a.size(); i++) t += (i > 1 ? " " : "") + a[i]; gh_log("INJECT-LOG %s", t.c_str()); return; }
    if (c == "mm" && a.size() >= 3) { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEMOTION; e.motion.xrel = atoi(a[1].c_str()); e.motion.yrel = atoi(a[2].c_str()); SDL_PushEvent(&e); return; }
    if (c == "mb" && a.size() >= 3) { SDL_Event e; SDL_zero(e); bool d = a[2] != "0"; e.type = d ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; e.button.button = a[1] == "r" ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT; e.button.state = d ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&e); return; }
    if (c == "key" && a.size() >= 3) { SDL_Keycode k = SDL_GetKeyFromName(a[1].c_str()); if (k == SDLK_UNKNOWN) return; SDL_Event e; SDL_zero(e); bool d = a[2] != "0"; e.type = d ? SDL_KEYDOWN : SDL_KEYUP; e.key.keysym.sym = k; e.key.keysym.scancode = SDL_GetScancodeFromKey(k); if (a.size() >= 4 && a[3] == "ctrl") e.key.keysym.mod = KMOD_LCTRL; e.key.state = d ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&e); return; }
    if (c == "dbcheck" && a.size() >= 2) { SDL_JoystickGUID gu = SDL_JoystickGetGUIDFromString(a[1].c_str()); char * m = SDL_GameControllerMappingForGUID(gu);
        gh_log("DBCHECK guid=%s found=%d map=%.90s", a[1].c_str(), m ? 1 : 0, m ? m : "-"); GPLOG("DBCHECK guid=%s found=%d", a[1].c_str(), m ? 1 : 0); if (m) SDL_free(m); return; }
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (c == "vpad" && a.size() >= 2) {
        { SDL_version lv; SDL_GetVersion(&lv); gh_log("INJECT SDL linked %d.%d.%d", (int)lv.major, (int)lv.minor, (int)lv.patch); }
        int idx = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 15, 1); if (idx < 0) { gh_log("INJECT vpad attach FAIL: %s", SDL_GetError()); return; }
        g_vj_idx = idx; g_vj = SDL_JoystickOpen(idx); char gs[64] = "?"; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), gs, sizeof gs);
        gh_log("INJECT vpad(%s) attached idx=%d guid=%s open=%d", a[1].c_str(), idx, gs, g_vj ? 1 : 0); g_vj_map.clear();
        if (a[1] != "unknown") {
            const char * dbg = a[1] == "xinput" ? "030000005e0400008e02000000000000" : "030000006d04000016c2000000000000";
            char * m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(dbg));
            gh_log("INJECT DB lookup %s guid=%s found=%d map=%.100s", a[1].c_str(), dbg, m ? 1 : 0, m ? m : "-"); GPLOG("DBCHECK %s guid=%s found=%d", a[1].c_str(), dbg, m ? 1 : 0);
            if (m && strlen(m) > 33) { std::string nm = std::string(gs) + std::string(m + 32); g_vj_map = nm; int r = SDL_GameControllerAddMapping(nm.c_str()); gh_log("INJECT AddMapping(virtual guid + DB mapping) -> %d is_controller=%d", r, (int)SDL_IsGameController(idx)); }
            if (m) SDL_free(m);
            gensei_open_pad(idx);
        } else { gh_log("INJECT unknown guid=%s mapping_for_guid=%d is_controller=%d (0 이어야 폴백)", gs, SDL_GameControllerMappingForGUID(SDL_JoystickGetDeviceGUID(idx)) ? 1 : 0, (int)SDL_IsGameController(idx)); }
        if (a.size() >= 3) gensei_padkind_v = atoi(a[2].c_str());
        return; }
    if (c == "vstd" && a.size() >= 3 && g_vj) {
        std::string f = gh_mapfield(g_vj_map, a[1]); if (f.empty()) { gh_log("INJECT vstd %s: 매핑에 없음", a[1].c_str()); return; } bool d = a[2] != "0";
        if (f[0] == 'b') SDL_JoystickSetVirtualButton(g_vj, atoi(f.c_str() + 1), d ? 1 : 0);
        else if (f[0] == 'h') { int hv = 0; const char * dot = strchr(f.c_str(), '.'); if (dot) hv = atoi(dot + 1); SDL_JoystickSetVirtualHat(g_vj, 0, d ? (Uint8)hv : (Uint8)0); }
        else gh_log("INJECT vstd %s: 지원 안 하는 매핑 %s", a[1].c_str(), f.c_str());
        return; }
    if (c == "vraw" && a.size() >= 3 && g_vj) { SDL_JoystickSetVirtualButton(g_vj, atoi(a[1].c_str()), a[2] != "0" ? 1 : 0); return; }
    if (c == "vhat" && a.size() >= 2 && g_vj) { SDL_JoystickSetVirtualHat(g_vj, 0, (Uint8)atoi(a[1].c_str())); return; }
    if (c == "vaxis" && a.size() >= 3 && g_vj) { SDL_JoystickSetVirtualAxis(g_vj, atoi(a[1].c_str()), (Sint16)atoi(a[2].c_str())); return; }
    if (c == "vdetach") { if (g_vj) { SDL_JoystickClose(g_vj); g_vj = NULL; } if (g_vj_idx >= 0) { SDL_JoystickDetachVirtual(g_vj_idx); g_vj_idx = -1; } return; }
#else
    if (c.compare(0, 1, "v") == 0) { gh_log("INJECT %s: SDL < 2.0.14 — 가상 조이스틱 없음", c.c_str()); return; }
#endif
    gh_log("INJECT 알 수 없는 명령 %s", c.c_str());
}
