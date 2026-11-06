/* app/opts_log.h -- S9b: log, profile struct, run-option globals, diagnostics, inject. Moved unchanged from gensei_host.cpp lines 124-361 (as of t54t). Included in place; single translation unit. */
/* ------------------------------------------------------------------ log */
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
    if (!gApp.log) return;
    if (gApp.log_bytes > (48ull << 20)) return;   /* cap 48 MB */
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    buf[n] = 0;
    unsigned int t = SDL_GetTicks() - gApp.log_t0;
    std::string u8 = gh_mixed_u8(buf, n);
    gApp.log_bytes += fprintf(gApp.log, "%7u %s%s%s", t, pfx, u8.c_str(), (n > 0 && buf[n - 1] == '\n') ? "" : "\n");
}
static void gh_log(const char * fmt, ...) { va_list ap; va_start(ap, fmt); gh_vlog("HOST ", fmt, ap); va_end(ap); fflush(gApp.log); }
static void gh_msgbox(const char * utf8text);
static void gh_human(const std::string & msg) { gApp.errmsg = msg; gh_log("안내(사용자 화면): %s", msg.c_str()); }

void DEBUG_ShowMsg(char const * format, ...) { va_list ap; va_start(ap, format); gh_vlog("MSG  ", format, ap); va_end(ap); }
void LOG::operator() (char const * format, ...) {
    const char * sv = "";
    switch (d_severity) { case LOG_DEBUG: sv = "D "; break; case LOG_NORMAL: sv = "N "; break; case LOG_WARN: sv = "W "; break; case LOG_ERROR: sv = "E "; break; case LOG_FATAL: sv = "F "; break; default: break; }
    va_list ap; va_start(ap, format); gh_vlog(sv, format, ap); va_end(ap);
}

/* ------------------------------------------------------------------ profile */
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
extern double gensei_out_sumsq; extern unsigned long long gensei_out_n;
/* r009 세이브스테이트 시험: --save SEC / --load SEC (게임 부팅 후 벽시계 초) --state FILE, 이벤트 뒤 0.1s·1s·3s 캡처 */
/* t47a (#559): 자동 입력 전용 시계 g_bk_t — 일시정지(Alt+P)·게임 메뉴가 열린 시간은 흐르지 않는다. 멈춘 적이 없으면 부팅 후 경과(since)와 같다. */

static Bitu gh_int18_wrap(void) {
    unsigned ax = (unsigned)reg_ax; gApp.i18_ax[ax]++; gApp.i18_n++;
    { PhysPt sp = SegPhys(ss) + (cpu.stack.big ? reg_esp : (reg_sp & 0xffff)); unsigned ip = mem_readw(sp), cs_ = mem_readw(sp + 2); gApp.i18_caller[(cs_ << 16) | ip]++; }
    if ((ax >> 8) != 0x04 && gApp.i18_non04 < 60) { gApp.i18_non04++; gh_log("int18: AX=%04x BX=%04x CX=%04x DX=%04x (AH=%02x 기능, 04 이외)", ax, (unsigned)reg_bx, (unsigned)reg_cx, (unsigned)reg_dx, ax >> 8); }
    unsigned now = SDL_GetTicks();
    if (now - gApp.i18_last >= 2000) { gApp.i18_last = now;
        std::vector<std::pair<unsigned, unsigned>> v; for (auto & kv : gApp.i18_ax) v.push_back({ kv.second, kv.first }); std::sort(v.begin(), v.end(), [](const std::pair<unsigned, unsigned> & a, const std::pair<unsigned, unsigned> & b) { return a.first > b.first; });
        std::string s; for (size_t i = 0; i < v.size() && i < 8; i++) { char b[40]; snprintf(b, sizeof b, " %04x:%u", v[i].second, v[i].first); s += b; }
        std::vector<std::pair<unsigned, unsigned>> c; for (auto & kv : gApp.i18_caller) c.push_back({ kv.second, kv.first }); std::sort(c.begin(), c.end(), [](const std::pair<unsigned, unsigned> & a, const std::pair<unsigned, unsigned> & b) { return a.first > b.first; });
        std::string t; for (size_t i = 0; i < c.size() && i < 5; i++) { char b[48]; snprintf(b, sizeof b, " %04x:%04x=%u", c[i].second >> 16, c[i].second & 0xffff, c[i].first); t += b; }
        gh_log("int18sum: 누적 %u회 | AX별 상위:%s | 호출 위치 상위:%s", gApp.i18_n, s.c_str(), t.c_str()); gApp.i18_ax.clear(); gApp.i18_caller.clear(); }
    return gApp.orig18 ? gApp.orig18() : 0;
}
static void gh_install_int18trace(void) {
    unsigned off = mem_readw(0x18 * 4), seg = mem_readw(0x18 * 4 + 2); PhysPt p = (PhysPt)(seg * 16 + off);
    for (int i = 0; i < 8; i++) if (mem_readb(p + i) == 0xFE && mem_readb(p + i + 1) == 0x38) { unsigned nr = mem_readw(p + i + 2); gApp.orig18 = CallBack_Handlers[nr]; Bitu n2 = CALLBACK_Allocate(); CallBack_Handlers[n2] = gh_int18_wrap; phys_writew(p + i + 2, (uint16_t)n2); gh_log("int18trace: IVT18=%04x:%04x 콜백 %u -> %u 로 감쌈 (쓰기 뒤 읽어 보기: %u, 핸들러 교체 %s)", seg, off, nr, (unsigned)n2, (unsigned)mem_readw(p + i + 2), CallBack_Handlers[n2] == gh_int18_wrap ? "ok" : "NO"); return; }
    gh_log("int18trace: IVT18=%04x:%04x 에서 FE 38 콜백 명령을 못 찾음", seg, off);
}
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
        char wr[96] = ""; if (gApp.watch_off >= 0) {   /* 핸들러 앞 192바이트에서 DS:off 에 쓰는 명령(c6/c7/fe/ff 06 lo hi, 88/89 .. 06 lo hi) 찾기 */
            unsigned lo = (unsigned)gApp.watch_off & 0xff, hi = ((unsigned)gApp.watch_off >> 8) & 0xff; int m = 0;
            for (int i = 0; i < 192 && m < 3; i++) { unsigned b0 = mem_readb(hp + i), b1 = mem_readb(hp + i + 1), b2 = mem_readb(hp + i + 2), b3 = mem_readb(hp + i + 3);
                if ((b0 == 0xc6 || b0 == 0xc7 || b0 == 0xfe || b0 == 0xff || b0 == 0x88 || b0 == 0x89 || b0 == 0xa2 || b0 == 0xa3) && ((b1 == 0x06 && b2 == lo && b3 == hi) || ((b0 == 0xa2 || b0 == 0xa3) && b1 == lo && b2 == hi)))
                { char t[24]; snprintf(t, sizeof t, " WR@+%d(op%02x)", i, b0); if (strlen(wr) + strlen(t) < sizeof wr) strcat(wr, t); m++; } } }
        gh_log("tr21 vec %02x: %04x:%08x phys=%08x | %s%s", v, sel, off, (unsigned)hp, hx, wr);
    }
}
static bool gh_parse_amouse(const std::string & s) {   /* "ms:dx,dy,act;..." act = - | ld | lu | rd | ru */
    size_t p = 0; while (p < s.size()) { size_t e = s.find(';', p); if (e == std::string::npos) e = s.size(); std::string it = s.substr(p, e - p); p = e + 1; if (it.empty()) continue;
        GMouse m; m.at_ms = (unsigned)atoi(it.c_str()); m.dx = m.dy = 0; m.act = 0; size_t c = it.find(':'); if (c == std::string::npos) continue; std::string r = it.substr(c + 1);
        size_t c1 = r.find(','); if (c1 == std::string::npos) continue; m.dx = (float)atof(r.c_str()); size_t c2 = r.find(',', c1 + 1); m.dy = (float)atof(r.c_str() + c1 + 1);
        if (c2 != std::string::npos) { std::string a = r.substr(c2 + 1); m.act = a == "ld" ? 1 : a == "lu" ? 2 : a == "rd" ? 3 : a == "ru" ? 4 : 0; } gApp.amouse.push_back(m); }
    return !gApp.amouse.empty();
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
static bool gh_parse_inject(const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false; char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        std::string s = ln; while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back(); if (s.empty() || s[0] == '#') continue;
        GInj g; size_t p = 0; while (p < s.size()) { while (p < s.size() && s[p] == ' ') p++; size_t e = s.find(' ', p); if (e == std::string::npos) e = s.size(); if (e > p) g.a.push_back(s.substr(p, e - p)); p = e; }
        if (g.a.size() < 2) continue; g.at = (unsigned)atoi(g.a[0].c_str()); g.a.erase(g.a.begin());
        if (g.a[0] == "tap" && g.a.size() >= 2) { GInj d = g; d.a = { "cbtn", g.a[1], "1" }; GInj u = g; u.at = g.at + 120; u.a = { "cbtn", g.a[1], "0" }; gApp.inj.push_back(d); gApp.inj.push_back(u); continue; }
        if (g.a[0] == "vtap" && g.a.size() >= 2) { GInj d = g; d.a = { "vstd", g.a[1], "1" }; GInj u = g; u.at = g.at + 120; u.a = { "vstd", g.a[1], "0" }; gApp.inj.push_back(d); gApp.inj.push_back(u); continue; }
        gApp.inj.push_back(g);
    }
    fclose(f); std::stable_sort(gApp.inj.begin(), gApp.inj.end(), [](const GInj & x, const GInj & y) { return x.at < y.at; });
    return !gApp.inj.empty();
}
static std::string gh_mapfield(const std::string & map, const std::string & nm) {   /* "…,nm:xx,…" 의 xx */
    std::string k = "," + nm + ":"; size_t p = map.find(k); if (p == std::string::npos) return std::string(); p += k.size(); size_t e = map.find(',', p); return map.substr(p, e == std::string::npos ? std::string::npos : e - p);
}
static void gh_inj_run(const GInj & g, unsigned since) {
    const std::vector<std::string> & a = g.a; const std::string & c = a[0];
    static unsigned inj_ax = 0; if (!(c == "caxis" && (++inj_ax % 500) != 1)) gh_log("INJECT t=%u %s%s%s%s%s%s", since, c.c_str(), a.size() > 1 ? " " : "", a.size() > 1 ? a[1].c_str() : "", a.size() > 2 ? " " : "", a.size() > 2 ? a[2].c_str() : "", a.size() > 3 ? " ..." : "");
    if (c == "cbtn" && a.size() >= 3) { SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(a[1].c_str()); if (b == SDL_CONTROLLER_BUTTON_INVALID) { gh_log("INJECT cbtn 알 수 없는 이름 %s", a[1].c_str()); return; }
        bool d = a[2] != "0"; SDL_Event e; SDL_zero(e); e.type = d ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP; e.cbutton.which = gApp.vj ? SDL_JoystickInstanceID(gApp.vj) : 0; e.cbutton.button = (Uint8)b; e.cbutton.state = d ? SDL_PRESSED : SDL_RELEASED; SDL_PushEvent(&e); return; }
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
        gApp.vj_idx = idx; gApp.vj = SDL_JoystickOpen(idx); char gs[64] = "?"; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), gs, sizeof gs);
        gh_log("INJECT vpad(%s) attached idx=%d guid=%s open=%d", a[1].c_str(), idx, gs, gApp.vj ? 1 : 0); gApp.vj_map.clear();
        if (a[1] != "unknown") {
            const char * dbg = a[1] == "xinput" ? "030000005e0400008e02000000000000" : "030000006d04000016c2000000000000";
            char * m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(dbg));
            gh_log("INJECT DB lookup %s guid=%s found=%d map=%.100s", a[1].c_str(), dbg, m ? 1 : 0, m ? m : "-"); GPLOG("DBCHECK %s guid=%s found=%d", a[1].c_str(), dbg, m ? 1 : 0);
            if (m && strlen(m) > 33) { std::string nm = std::string(gs) + std::string(m + 32); gApp.vj_map = nm; int r = SDL_GameControllerAddMapping(nm.c_str()); gh_log("INJECT AddMapping(virtual guid + DB mapping) -> %d is_controller=%d", r, (int)SDL_IsGameController(idx)); }
            if (m) SDL_free(m);
            gensei_open_pad(idx);
        } else { gh_log("INJECT unknown guid=%s mapping_for_guid=%d is_controller=%d (0 이어야 폴백)", gs, SDL_GameControllerMappingForGUID(SDL_JoystickGetDeviceGUID(idx)) ? 1 : 0, (int)SDL_IsGameController(idx)); }
        if (a.size() >= 3) gensei_padkind_v = atoi(a[2].c_str());
        return; }
    if (c == "vstd" && a.size() >= 3 && gApp.vj) {
        std::string f = gh_mapfield(gApp.vj_map, a[1]); if (f.empty()) { gh_log("INJECT vstd %s: 매핑에 없음", a[1].c_str()); return; } bool d = a[2] != "0";
        if (f[0] == 'b') SDL_JoystickSetVirtualButton(gApp.vj, atoi(f.c_str() + 1), d ? 1 : 0);
        else if (f[0] == 'h') { int hv = 0; const char * dot = strchr(f.c_str(), '.'); if (dot) hv = atoi(dot + 1); SDL_JoystickSetVirtualHat(gApp.vj, 0, d ? (Uint8)hv : (Uint8)0); }
        else gh_log("INJECT vstd %s: 지원 안 하는 매핑 %s", a[1].c_str(), f.c_str());
        return; }
    if (c == "vraw" && a.size() >= 3 && gApp.vj) { SDL_JoystickSetVirtualButton(gApp.vj, atoi(a[1].c_str()), a[2] != "0" ? 1 : 0); return; }
    if (c == "vhat" && a.size() >= 2 && gApp.vj) { SDL_JoystickSetVirtualHat(gApp.vj, 0, (Uint8)atoi(a[1].c_str())); return; }
    if (c == "vaxis" && a.size() >= 3 && gApp.vj) { SDL_JoystickSetVirtualAxis(gApp.vj, atoi(a[1].c_str()), (Sint16)atoi(a[2].c_str())); return; }
    if (c == "vdetach") { if (gApp.vj) { SDL_JoystickClose(gApp.vj); gApp.vj = NULL; } if (gApp.vj_idx >= 0) { SDL_JoystickDetachVirtual(gApp.vj_idx); gApp.vj_idx = -1; } return; }
#else
    if (c.compare(0, 1, "v") == 0) { gh_log("INJECT %s: SDL < 2.0.14 — 가상 조이스틱 없음", c.c_str()); return; }
#endif
    gh_log("INJECT 알 수 없는 명령 %s", c.c_str());
}
