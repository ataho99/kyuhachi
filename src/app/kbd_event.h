/* app/kbd_event.h -- S9b: keyboard overlay glue, key/game events. Moved unchanged from gensei_host.cpp lines 672-840 (as of t54t). Included in place; single translation unit. */
#if defined(__ANDROID__)
/* t85: min key-hold table (key id < 256); gh_kbhold_tick() is called every frame from gfx_events.h */
static unsigned gh_kbhold_down[256]; static bool gh_kbhold_pend[256]; static unsigned gh_kbhold_due[256];
static void gh_kbhold_tick(void) { unsigned now = SDL_GetTicks(); for (int i = 0; i < 256; i++) if (gh_kbhold_pend[i] && (int)(now - gh_kbhold_due[i]) >= 0) { gh_kbhold_pend[i] = false; gh_kbhold_down[i] = 0; KEYBOARD_AddKey((KBD_KEYS)i, false); } }
#endif
/* ---- 키보드 → PC-98 */
static KBD_KEYS gh_sdlkey(SDL_Keycode k) {
    static const KBD_KEYS L[26] = { KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g, KBD_h, KBD_i, KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o, KBD_p, KBD_q, KBD_r, KBD_s, KBD_t, KBD_u, KBD_v, KBD_w, KBD_x, KBD_y, KBD_z };
    static const KBD_KEYS N[10] = { KBD_0, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7, KBD_8, KBD_9 };
    static const KBD_KEYS F[9] = { KBD_f1, KBD_f2, KBD_f3, KBD_f4, KBD_f5, KBD_f6, KBD_f7, KBD_f8, KBD_f9 };
    if (k >= SDLK_a && k <= SDLK_z) return L[k - SDLK_a];
    if (k >= SDLK_0 && k <= SDLK_9) return N[k - SDLK_0];
    if (k >= SDLK_F1 && k <= SDLK_F9) return F[k - SDLK_F1];
    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return KBD_enter; case SDLK_ESCAPE: return KBD_esc; case SDLK_BACKSPACE: return KBD_backspace; case SDLK_TAB: return KBD_tab; case SDLK_SPACE: return KBD_space;
    case SDLK_UP: return KBD_up; case SDLK_DOWN: return KBD_down; case SDLK_LEFT: return KBD_left; case SDLK_RIGHT: return KBD_right;
    case SDLK_INSERT: return KBD_insert; case SDLK_DELETE: return KBD_delete; case SDLK_HOME: return KBD_home; case SDLK_END: return KBD_end; case SDLK_PAGEUP: return KBD_pageup; case SDLK_PAGEDOWN: return KBD_pagedown;
    case SDLK_CAPSLOCK: return KBD_capslock; case SDLK_LSHIFT: return KBD_leftshift; case SDLK_RSHIFT: return KBD_rightshift; case SDLK_LCTRL: return KBD_leftctrl; case SDLK_RCTRL: return KBD_rightctrl;
    case SDLK_MINUS: return KBD_minus; case SDLK_EQUALS: return KBD_equals; case SDLK_LEFTBRACKET: return KBD_leftbracket; case SDLK_RIGHTBRACKET: return KBD_rightbracket;
    case SDLK_SEMICOLON: return KBD_semicolon; case SDLK_QUOTE: return KBD_quote; case SDLK_COMMA: return KBD_comma; case SDLK_PERIOD: return KBD_period; case SDLK_SLASH: return KBD_slash;
    case SDLK_BACKSLASH: return KBD_backslash; case SDLK_BACKQUOTE: return KBD_grave;
    case SDLK_KP_0: return KBD_kp0; case SDLK_KP_1: return KBD_kp1; case SDLK_KP_2: return KBD_kp2; case SDLK_KP_3: return KBD_kp3; case SDLK_KP_4: return KBD_kp4; case SDLK_KP_5: return KBD_kp5;
    case SDLK_KP_6: return KBD_kp6; case SDLK_KP_7: return KBD_kp7; case SDLK_KP_8: return KBD_kp8; case SDLK_KP_9: return KBD_kp9;
    default: return KBD_NONE;
    }
}
/* ---- t46x: 화면 키보드(경로 B) 호스트 글루 — SDL 입력층은 이 블록뿐이다(안드로이드는 이 블록만 교체). 키 표 = gensei_kbtable.h, 상태·그리기 = gensei_kbov.h */
extern bool pc98_force_ibm_layout; extern bool pc98_caps(void); extern bool pc98_kana(void);
static bool gh_kb_send(int k, bool d) {   /* 코어로 보냄. 부팅 전·never_map 키는 보내지 않는다(기존 gh_game_event 와 같은 조건) */
    KBD_KEYS kk = (KBD_KEYS)k;
    if (!gApp.boot_tick || kk == KBD_NONE) return false;
    if ((int)kk < 256 && kh_never[(int)kk]) return false;
    KEYBOARD_AddKey(kk, d); gApp.kbsent_total++; if (gApp.ui_.scr == ui::S_OSK) gApp.kbsent_text++; return true;
}
static bool gh_kb_ibm(void) { return pc98_force_ibm_layout; }
static bool gh_kb_caps(void) { return pc98_caps(); }
static bool gh_kb_kana(void) { return pc98_kana(); }
static void gh_kb_log(const char * s) { gh_log("KBOV %s", s); }
/* t52f (#132): 오버레이 버튼 동작. 빠른 저장·불러오기는 Alt+F5 / Alt+F9 와 같은 슬롯 1, 배속은 ScrollLock 과 같은 토글 */
static void gh_ov_act(int a) {
    if (a == 5) { gh_osd(phr_("길게 누르세요"), 1500); gh_log("OVBTN load 짧게 누름 -> 동작 안 함 (「길게 누르세요」 표시)"); return; }
    if (!gApp.boot_tick) { gh_log("OVBTN act=%d 무시 (아직 부팅 전)", a); return; }
    if (a == 1) { gApp.menu_req = true; gh_log("OVBTN menu -> g_menu_req"); }
    else if (a == 2) { std::string p = gh_slot_path(); bool ok = gh_do_save(p, false); gh_log("OVBTN save quick %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("퀵 슬롯에 저장했습니다") : phr_("저장에 실패했습니다"), 2000); }
    else if (a == 3) { std::string p = gh_slot_path(); bool ok = gh_do_load(p); gh_log("OVBTN load quick %s", ok ? "OK" : "FAIL"); gh_osd(ok ? phr_("퀵 슬롯을 불러왔습니다") : phr_("불러오기에 실패했습니다"), 2000); }
    else if (a == 4) { gApp.boost_tog = !gApp.boost_tog; gh_boost_update(); gh_log("OVBTN boost toggle -> tog=%d on=%d", (int)gApp.boost_tog, (int)gApp.boost_on); }
}
static bool gh_ov_boostlit(void) { return gApp.boost_on; }
static unsigned gh_ov_now(void) { return (unsigned)SDL_GetTicks(); }
static bool g_kb_inited = (kbov::g_send = gh_kb_send, kbov::g_ibm_fn = gh_kb_ibm, kbov::g_caps_fn = gh_kb_caps, kbov::g_kana_fn = gh_kb_kana, kbov::g_logfn = gh_kb_log, kbov::g_act = gh_ov_act, kbov::g_boost_fn = gh_ov_boostlit, kbov::g_now = gh_ov_now, true);
static void gh_kb_cursor(void) {   /* 키보드가 보이는 동안은 커서를 보이게(전체화면 마우스 클릭용). 끄면 원래대로 */
    if (!gApp.win) return;
    if (kbov::g_vis) { if (gApp.kb_prevcur < 0) gApp.kb_prevcur = SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE ? 1 : 0; SDL_ShowCursor(SDL_ENABLE); }
    else if (gApp.kb_prevcur >= 0) { SDL_ShowCursor(gApp.kb_prevcur ? SDL_ENABLE : SDL_DISABLE); gApp.kb_prevcur = -1; }
}
static void gh_kb_toggle(void) { kbov::set_visible(!kbov::g_vis); gh_kb_cursor(); gh_osd(kbov::g_vis ? phr_("화면 키보드 켜짐 (Alt+K 로 끔)") : phr_("화면 키보드 꺼짐"), 1500); }
/* 빌드 5a: 패드 버튼에 배정한 큐하치 기능 (kh_input12 가 부른다). 메뉴가 열려 있거나 부팅 전이면 무시, 배속 누름의 놓기는 항상 받는다 */
static void gh_next_disk(void) {   /* build 5a-2: next disk of the swap list (drive B if it holds a disk, else A) */
    ui::Ui & U = gApp.ui_; const int gi = gApp.cur_game;
    if (gi < 0 || gi >= (int)U.games.size()) return;
    auto fit = gApp.fdfiles.find(U.games[gi].id);
    if (fit == gApp.fdfiles.end() || fit->second.size() < 2) { gh_osd(phr_("pad_nodisk"), 2500); return; }
    const int slot = U.games[gi].cur_fd[1].empty() ? 0 : 1; const int n = (int)fit->second.size(); int idx = -1;
    for (int i = 0; i < n && i < (int)U.games[gi].fds.size(); i++) if (U.games[gi].fds[i] == U.games[gi].cur_fd[slot]) idx = i;
    const int nx = (idx + 1) % n;
    const bool ok = gh_attach(slot, fit->second[nx].c_str(), true);
    if (ok && nx < (int)U.games[gi].fds.size()) U.games[gi].cur_fd[slot] = U.games[gi].fds[nx];
    gh_log("DISKSWAP(pad) drive=%c idx=%d %s", slot ? 'B' : 'A', nx, ok ? "OK" : "FAIL");
    gh_osd(ok ? std::string(slot ? "B" : "A") + std::string(" ") + phr_("pad_drv") + std::string(" ") + (nx < (int)U.games[gi].fds.size() ? U.games[gi].fds[nx] : std::string()) : std::string(phr_("pad_swapfail")), 3000);
}
static void gh_pad_fn(int fn, bool d) {
    static const char * const NM[FN_N] = { "-", "menu", "boost_hold", "boost_toggle", "quick_save", "quick_load", "osk", "screenshot", "next_disk", "fps" };
    gh_log("PADFN %s %s", fn > 0 && fn < FN_N ? NM[fn] : "?", d ? "down" : "up");
    if (fn == FN_BOOST_HOLD) { if (d && (gApp.menu_open || !gApp.boot_tick)) return; gApp.boost_hold = d; gh_boost_update(); return; }
    if (!d || gApp.menu_open || !gApp.boot_tick) return;
    switch (fn) {
    case FN_MENU: gApp.menu_req = true; break;
    case FN_BOOST_TOG: gApp.boost_tog = !gApp.boost_tog; gh_boost_update(); break;
    case FN_QSAVE: gh_ov_act(2); break;
    case FN_QLOAD: gh_ov_act(3); break;
    case FN_OSK: gh_kb_toggle(); break;
    case FN_SHOT: gApp.scr_req = true; break;
    case FN_NEXTDISK: gh_next_disk(); break;
    case FN_FPS: gh_fps_toggle(); break;
    default: break;
    }
}
static bool g_padfn_inited = (kh_fn_hook = gh_pad_fn, true);
static bool gh_kb_geom(int & ww, int & wh, int & dw, int & dh, int & rx, int & ry, int & rw, int & rh) {   /* 창 ↔ 화면 표시 사각형: GL 스레드와 같은 gl_rect 로 계산(곡면 왜곡은 반영하지 않음) */
    if (!gApp.win || gApp.fbw <= 0 || gApp.fbh <= 0) return false;
    SDL_GetWindowSize(gApp.win, &ww, &wh); SDL_GL_GetDrawableSize(gApp.win, &dw, &dh);
    if (ww <= 0 || wh <= 0 || dw <= 0 || dh <= 0) return false;
    gl_rect(dw, dh, gApp.fbw, gApp.fbh, (int)gApp.fit, rx, ry, rw, rh); return rw > 0 && rh > 0;
}
static bool gh_kb_curved(void) { return (int)gApp.shader == (int)SH_CRT && (int)gApp.fit != (int)FIT_INT && gApp.curve > 0.0f && gApp.kbshader != 0; }   /* GL 스레드와 같은 조건: crt 셰이더 + 곡면 > 0 (정수배는 nearest 로 바뀌어 곡면 없음) */
static void gh_kb_warp(double & u, double & v) {   /* 화면 uv(0..1) → 프레임 uv. pc98_crt_t42.glsl 의 warp() 와 같은 식 */
    double c = (double)gApp.curve, px = u * 2.0 - 1.0, py = v * 2.0 - 1.0; double nx = px * (1.0 + py * py * c), ny = py * (1.0 + px * px * c * 1.33); u = nx * 0.5 + 0.5; v = ny * 0.5 + 0.5;
}
static bool gh_kb_win2virt(int mx, int my, int & vx, int & vy) {
    int ww, wh, dw, dh, rx, ry, rw, rh; if (!gh_kb_geom(ww, wh, dw, dh, rx, ry, rw, rh)) return false;
    double u = ((double)mx * dw / ww - rx) / rw, v = ((double)my * dh / wh - ry) / rh;
    if (gh_kb_curved()) gh_kb_warp(u, v);   /* 곡면: 화면 위치 → 셰이더가 읽는 프레임 위치(오버레이도 같은 프레임에 그려져 같이 휜다) */
    vx = (int)(u * 640.0 + (u < 0 ? -0.5 : 0.5)); vy = (int)(v * 400.0 + (v < 0 ? -0.5 : 0.5)); return true;
}
static bool gh_menu_win2virt(int mx, int my, int & vx, int & vy) {   /* t48b: 메뉴 프레임(640x400, 창 맞춤 gl_rect) 위의 창 좌표 → 가상 좌표. 곡면이면 셰이더 warp 와 같은 식 */
    if (!gApp.win) return false; int ww = 0, wh = 0, dw = 0, dh = 0, rx, ry, rw, rh; SDL_GetWindowSize(gApp.win, &ww, &wh); SDL_GL_GetDrawableSize(gApp.win, &dw, &dh);
    if (ww <= 0 || wh <= 0 || dw <= 0 || dh <= 0) return false;
    gl_rect(dw, dh, 640, 400, (int)gApp.fit, rx, ry, rw, rh); if (rw <= 0 || rh <= 0) return false;
    double u = ((double)mx * dw / ww - rx) / rw, v = ((double)my * dh / wh - ry) / rh;
    if ((int)gApp.shader == (int)SH_CRT && (int)gApp.fit != (int)FIT_INT && gApp.curve > 0.0f) gh_kb_warp(u, v);
    vx = (int)(u * 640.0 + (u < 0 ? -0.5 : 0.5)); vy = (int)(v * 400.0 + (v < 0 ? -0.5 : 0.5)); return true;
}
static bool gh_kb_virt2win(int vx, int vy, int & wx, int & wy) {   /* 시험 전용 역변환: 곡면이면 warp 의 역을 고정점 반복으로 푼다 */
    int ww, wh, dw, dh, rx, ry, rw, rh; if (!gh_kb_geom(ww, wh, dw, dh, rx, ry, rw, rh)) return false;
    double qu = vx / 640.0, qv = vy / 400.0, u = qu, v = qv;
    if (gh_kb_curved()) { for (int it = 0; it < 40; it++) { double a = u, b = v; gh_kb_warp(a, b); u -= (a - qu); v -= (b - qv); } }
    wx = (int)((rx + u * rw) * ww / dw + 0.5); wy = (int)((ry + v * rh) * wh / dh + 0.5); return true;
}
/* 마우스 이벤트 → 오버레이. 숨김 상태(+버튼 꺼짐)면 0건 소비. 보임 상태에서도 패널·버튼 영역 밖은 false(게임으로 그대로 감 — 마우스 게임도 같은 규칙, 대장방 판정 #1169 5⒝) */
static bool gh_kbd_event(const SDL_Event & e) {
    if (gApp.headless || !gApp.win || !gApp.use_gl || !gApp.boot_tick || gApp.menu_open || (gApp.loading && !gApp.noload)) return false;
    int ph, mx, my;
    bool fin = false; static bool fown = false;   /* t147: 손가락 좌표로 직접 판정 + 소비한 터치의 마우스 에코는 삼킨다 */
    if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION) { int ww0 = 640, wh0 = 400; SDL_GetWindowSize(gApp.win, &ww0, &wh0); ph = e.type == SDL_FINGERDOWN ? 0 : e.type == SDL_FINGERUP ? 1 : 2; mx = (int)(e.tfinger.x * (float)ww0); my = (int)(e.tfinger.y * (float)wh0); fin = true; }
    else if ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && e.button.which == SDL_TOUCH_MOUSEID && fown) { if (e.type == SDL_MOUSEBUTTONUP) fown = false; return true; }
    else if (e.type == SDL_MOUSEMOTION && e.motion.which == SDL_TOUCH_MOUSEID && fown) return true;
    else if (e.type == SDL_MOUSEBUTTONDOWN) { ph = 0; mx = e.button.x; my = e.button.y; }
    else if (e.type == SDL_MOUSEBUTTONUP) { ph = 1; mx = e.button.x; my = e.button.y; }
    else if (e.type == SDL_MOUSEMOTION) { ph = 2; mx = e.motion.x; my = e.motion.y; }
    else return false;
    kbov::wake();   /* t52f: 마우스가 움직이거나 터치가 있으면 버튼 줄을 몇 초 진하게 */
    if (!kbov::active()) return false;
    if (!fin && gApp.relmouse && !kbov::g_vis && !kbov::any_owned()) return false;   /* 마우스 가둠 중(키보드 숨김)에는 창 좌표를 믿을 수 없다 → 버튼 사용 불가, Alt+K·메뉴로 켠다 */
    int vx = -1000, vy = -1000; if (!gh_kb_win2virt(mx, my, vx, vy) && !kbov::any_owned()) return false;
    int btn = (e.type == SDL_MOUSEMOTION || fin) ? SDL_BUTTON_LEFT : e.button.button;
    if (btn != SDL_BUTTON_LEFT) { bool in = kbov::in_ui(vx, vy); if (in && ph != 2) { gApp.kb_cons++; return true; } return false; }
    bool c = kbov::pointer(0, ph, vx, vy);
    if (fin) { if (ph == 0) fown = c; static int fl = 0; if (ph == 0 && fl < 40) { fl++; gh_log("KHA ov finger x=%d y=%d v=(%d,%d) relmouse=%d -> %s (%d/40)", mx, my, vx, vy, (int)gApp.relmouse, c ? "overlay" : "passthrough", fl); } if (ph == 1) fown = fown && c; }
    if (ph != 2) { if (c) gApp.kb_cons++; else gApp.kb_pass++; }
    return c;
}
/* ---- virtual pad glue (window-space side bands): same actions as a real pad via the shared bindings */
static void gh_vpad_fire(int b, bool d) {
    static const KBD_KEYS K[4] = { KBD_up, KBD_down, KBD_left, KBD_right };
    static const char * W[vpad::NB] = { "VPAD UP", "VPAD DOWN", "VPAD LEFT", "VPAD RIGHT", "VPAD O", "VPAD X", "VPAD SELECT", "VPAD START" };
    gh_log("VPAD %s %s", W[b], d ? "down" : "up");
    if (b < 4) kh_key(K[b], d, W[b]);
    else kh_bind_fire(b == vpad::B_O ? PB_EAST : b == vpad::B_X ? PB_SOUTH : b == vpad::B_SEL ? PB_BACK : PB_START, d, W[b]);
}
static bool g_vpad_inited = (vpad::g_fire = gh_vpad_fire, true);
static bool gh_vpad_update(vpad::Geo & g, int & ww, int & wh, int & dw, int & dh) {   /* decides whether the pad is shown and active */
    bool ok = !(gApp.headless || !gApp.win || !gApp.use_gl || !gApp.boot_tick || gApp.menu_open || (gApp.loading && !gApp.noload));
    if (ok && vpad::g_mode == 0) ok = false;
    if (ok && vpad::g_mode == 2 && (!kh_pads.empty() || SDL_GetNumTouchDevices() <= 0)) ok = false;   /* auto: touch screen and no real pad */
    int rx = 0, ry = 0, rw = 0, rh = 0;
    if (ok) ok = gh_kb_geom(ww, wh, dw, dh, rx, ry, rw, rh);
    if (ok) { vpad::layout(dw, dh, rx, rw, g); ok = g.on; }
    vpad::g_show = ok ? 1 : 0;
    static int lastk = -1; int gk = ok ? (dw * 7 + dh * 13 + rx * 17 + rw * 19 + 1) : 0;   // log only when geometry or visibility changes
    if (gk != lastk) {
        lastk = gk;
        if (ok) gh_log("VPAD geo dw=%d dh=%d fx=%d fw=%d START=(%d,%d) O=(%d,%d)", dw, dh, rx, rw,
                       g.r[vpad::B_START].x + g.r[vpad::B_START].w / 2, g.r[vpad::B_START].y + g.r[vpad::B_START].h / 2,
                       g.r[vpad::B_O].x + g.r[vpad::B_O].w / 2, g.r[vpad::B_O].y + g.r[vpad::B_O].h / 2);
        else gh_log("VPAD hidden");
    }
    if (!ok) vpad::release_all();
    return ok;
}
static void gh_vpad_tick(void) { vpad::Geo g; int a = 0, b = 0, c = 0, d = 0; gh_vpad_update(g, a, b, c, d); }
static bool gh_vpad_testpos(const char * nm, int & wx, int & wy) {   /* test only: window position of a pad button */
    static const char * N[vpad::NB] = { "UP", "DOWN", "LEFT", "RIGHT", "O", "X", "SEL", "START" };
    vpad::Geo g; int ww = 0, wh = 0, dw = 0, dh = 0; if (!gh_vpad_update(g, ww, wh, dw, dh)) return false;
    for (int b = 0; b < vpad::NB; b++) if (!strcmp(nm, N[b])) {
        wx = (g.r[b].x + g.r[b].w / 2) * ww / dw; wy = (g.r[b].y + g.r[b].h / 2) * wh / dh; return true;
    }
    return false;
}
static bool gh_vpad_event(const SDL_Event & e) {
    vpad::Geo g; int ww = 0, wh = 0, dw = 0, dh = 0;
    if (!gh_vpad_update(g, ww, wh, dw, dh)) return false;
    long long id = -1; int ph = 0, x = 0, y = 0; bool echo = false;
    if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION) {
        id = (long long)e.tfinger.fingerId; ph = e.type == SDL_FINGERDOWN ? 0 : e.type == SDL_FINGERUP ? 1 : 2;
        x = (int)(e.tfinger.x * dw); y = (int)(e.tfinger.y * dh);
    } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button != SDL_BUTTON_LEFT) return false;
        ph = e.type == SDL_MOUSEBUTTONDOWN ? 0 : 1; x = e.button.x * dw / ww; y = e.button.y * dh / wh; echo = e.button.which == SDL_TOUCH_MOUSEID;
    } else if (e.type == SDL_MOUSEMOTION) {
        ph = 2; x = e.motion.x * dw / ww; y = e.motion.y * dh / wh; echo = e.motion.which == SDL_TOUCH_MOUSEID;
    } else return false;
    if (echo) return vpad::hit(g, x, y) >= 0 || vpad::owned();   /* finger events drive the pad; swallow their mouse echo */
    return vpad::pointer(g, id, ph, x, y);
}
/* 시험 전용(--kbshow/--kbhide/--kbclick/--kbbtn): SDL 마우스 이벤트를 창 좌표로 밀어 넣어 hit-test·표시 사각형 변환까지 통과시킨다(키 함수를 직접 부르지 않음) */
static void gh_parse_kbclick(const char * a) {   /* "<시작초>:라벨,라벨,..." — 라벨은 키 표의 label, 같은 이름이 여럿이면 라벨#n(0부터), BTN = 토글 버튼, OUT = 영역 밖 */
    gApp.kbt_click_ms = (unsigned)(atof(a) * 1000.0); const char * p = strchr(a, ':'); if (!p) return; p++;
    std::string cur; for (const char * q = p;; q++) { if (*q == ',' || !*q) { if (!cur.empty()) gApp.kbt_seq.push_back(cur); cur.clear(); if (!*q) break; } else cur += *q; }
}
static void gh_kbtest_tick(unsigned since) {
    static bool shown = false, hidden = false; static size_t ci = 0; static unsigned nextat = 0; static int phase = 0;
    if (gApp.kbt_show_ms && !shown && since >= gApp.kbt_show_ms) { shown = true; kbov::set_visible(true); gh_kb_cursor(); }
    if (gApp.kbt_hide_ms && !hidden && since >= gApp.kbt_hide_ms) { hidden = true; kbov::set_visible(false); gh_kb_cursor(); }
    if (ci >= gApp.kbt_seq.size()) return;
    if (nextat == 0) nextat = gApp.kbt_click_ms ? gApp.kbt_click_ms : 1;
    if (since < nextat) return;
    std::string lb = gApp.kbt_seq[ci]; bool hold = false; if (!lb.empty() && lb[lb.size() - 1] == '!') { hold = true; lb.erase(lb.size() - 1); } int vx = 320, vy = 100;   /* 라벨 끝 ! = 누른 채로 둠(뗌 없음) */
    int wx = 0, wy = 0; bool padl = false;
    if (lb == "WINSHOT") { gh_winshot(); gh_log("KBTEST WINSHOT at %u ms", since); ci++; nextat = since + 300; return; }   // test: full window capture
    bool lng = false; int ovs = -1; if (lb.size() >= 3 && lb[0] == 'O' && lb[1] == 'V' && lb[2] >= '1' && lb[2] <= '4') { ovs = lb[2] - '0'; lng = lb.size() > 3 && lb[3] == 'L'; }   /* t52f: OV1 메뉴 · OV2 저장 · OV3 불러오기 · OV4 배속, 끝 L = 0.9초 누름 */
    if (ovs >= 0) { vx = kbov::sx(ovs) + kbov::BW / 2; vy = kbov::BY + kbov::BH / 2; }
    else if (lb == "BTN") { vx = kbov::BX + kbov::BW / 2; vy = kbov::BY + kbov::BH / 2; }
    else if (lb.compare(0, 4, "PAD_") == 0) {
        padl = gh_vpad_testpos(lb.c_str() + 4, wx, wy);
        if (!padl) { gh_log("KBTEST pad button not available: %s", lb.c_str()); ci++; return; }
    }
    else if (lb != "OUT") { std::string nm = lb; int nth = 0; size_t h = nm.find('#'); if (h != std::string::npos) { nth = atoi(nm.c_str() + h + 1); nm = nm.substr(0, h); }
        int ix = kbov::find_label(nm.c_str(), nth); if (ix < 0 || !kbov::key_center(ix, vx, vy)) { gh_log("KBTEST 라벨 없음: %s", lb.c_str()); ci++; return; } }
    if (!padl && !gh_kb_virt2win(vx, vy, wx, wy)) { gh_log("KBTEST 창 좌표 변환 실패(%s)", lb.c_str()); ci++; return; }
    SDL_Event se; memset(&se, 0, sizeof se);
    se.type = phase == 0 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; se.button.windowID = SDL_GetWindowID(gApp.win); se.button.button = SDL_BUTTON_LEFT; se.button.state = phase == 0 ? SDL_PRESSED : SDL_RELEASED; se.button.clicks = 1; se.button.x = wx; se.button.y = wy;
    SDL_PushEvent(&se);
    gh_log("KBTEST %s %s at %u ms win=(%d,%d) virt=(%d,%d) 소비=%u 통과=%u", lb.c_str(), phase == 0 ? "누름" : "뗌", since, wx, wy, vx, vy, gApp.kb_cons, gApp.kb_pass);
    if (phase == 0) { if (hold) { ci++; nextat = since + 300; } else { phase = 1; nextat = since + (lng ? 900 : 80); } } else { phase = 0; ci++; nextat = since + 300; }
}
/* 게임 중 이벤트(메뉴가 닫혀 있을 때). true = 소비 */
static bool gh_game_event(const SDL_Event & e) {
    switch (e.type) {
    case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP: {
        bool d = e.type == SDL_CONTROLLERBUTTONDOWN;
        (void)d;   /* 빌드 5a: Y · L3 · 트리거는 이제 입력 층의 배정표(kh_pb)가 처리한다 */

        return false; }
    case SDL_CONTROLLERAXISMOTION: {
        /* r015: 트리거/오른쪽 스틱 축 이벤트를 로그에 남긴다(값이 4096 단위로 바뀔 때만). 배속 R2 임계값: 켜짐 > 16000, 꺼짐 < 8000 (SDL 트리거 범위 0..32767) */
        if (kh_dbg_pad) { static int lastbin[6] = { -99, -99, -99, -99, -99, -99 }; int ax = e.caxis.axis; if (ax >= 2 && ax < 6 && ax != 0) { int bin = e.caxis.value / 4096; if (bin != lastbin[ax]) { lastbin[ax] = bin; GPLOG("AXIS %d(%s) value=%d", ax, SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)ax), (int)e.caxis.value); } } }
        return false; }   /* 빌드 5a: 트리거(L2·R2)는 입력 층(kh_trigger)이 배정표대로 처리 */
    case SDL_KEYDOWN: case SDL_KEYUP: {
        bool d = e.type == SDL_KEYDOWN; SDL_Keycode k = e.key.keysym.sym;
        if (kh_xb_event(e)) return true;
        if (e.key.repeat) return true;
        if (k == SDLK_F12) { if (d && gApp.boot_tick) gApp.menu_req = true; return true; }
        if (k == SDLK_PRINTSCREEN) { if (d && gApp.boot_tick) gApp.scr_req = true; return true; }
        if (k == SDLK_F11) { if ((e.key.keysym.mod & KMOD_ALT) || (SDL_GetModState() & KMOD_ALT)) { if (d) { gApp.boost_tog = !gApp.boost_tog; gh_boost_update(); } return true; }   /* t52d (#129): Alt+F11 = 배속 토글(ScrollLock 없는 키보드용) */
            gApp.boost_hold = d; gh_boost_update(); return true; }
        if (k == SDLK_SCROLLLOCK) { if (d) { gApp.boost_tog = !gApp.boost_tog; gh_boost_update(); } return true; }
        if (k == SDLK_F10) return true;
        KBD_KEYS kk = gh_sdlkey(k);
        if (kk != KBD_NONE && gApp.boot_tick && !((int)kk < 256 && kh_never[(int)kk])) {
#if defined(__ANDROID__)
            /* t85 (#453-1): adb/touch send KEYDOWN->KEYUP within 3 ms and the game never saw the key (keytrace). Android keeps every key down for at least 100 ms */
            if ((int)kk < 256) {
                unsigned now = SDL_GetTicks();
                if (d) { gh_kbhold_down[(int)kk] = now; gh_kbhold_pend[(int)kk] = false; KEYBOARD_AddKey(kk, true); }
                else if (gh_kbhold_down[(int)kk] != 0 && now - gh_kbhold_down[(int)kk] < 100u) { gh_kbhold_pend[(int)kk] = true; gh_kbhold_due[(int)kk] = gh_kbhold_down[(int)kk] + 100u; }
                else { gh_kbhold_down[(int)kk] = 0; KEYBOARD_AddKey(kk, false); }
                return true;
            }
#endif
            KEYBOARD_AddKey(kk, d);
        }
        return true; }
    case SDL_MOUSEMOTION: {   /* r020 (#249-2): 호스트 마우스 → 코어 PC-98 버스마우스. 창 픽셀 이동량을 640 폭 기준으로 환산 */
        if (!(gApp.mouse_game && gApp.boot_tick && !gApp.menu_open)) { static int sk = 0; if (e.type == SDL_MOUSEMOTION && (sk++ % 200) == 0) gh_log("HOST MOUSEFWD skip mouse_game=%d boot=%d menu=%d x=%d y=%d", (int)gApp.mouse_game, (int)(gApp.boot_tick != 0), (int)gApp.menu_open, e.motion.x, e.motion.y); return false; }   /* t48c (#639 ③) */
        int ww = 640, wh = 400; if (gApp.win) SDL_GetWindowSize(gApp.win, &ww, &wh); float sc = 640.0f / (float)(ww > 0 ? ww : 640);
        { float fdx = (float)e.motion.xrel * sc, fdy = (float)e.motion.yrel * sc; kh_vmouse_move_rel(fdx, fdy);   /* t48c (#639 ③): 코어로 넘긴 이동량을 20번마다(처음 한 번 포함) 한 줄로 */
      static float mfx = 0, mfy = 0; static int mfn = 0; mfx += fdx; mfy += fdy; mfn++;
      if (mfn == 1 || mfn % 20 == 0) { gh_log("HOST MOUSEFWD dx=%.1f dy=%.1f x=%d y=%d btn=%u n=%d", (double)mfx, (double)mfy, e.motion.x, e.motion.y, (unsigned)SDL_GetMouseState(NULL, NULL), mfn); mfx = 0; mfy = 0; } return true; } }
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
        if (!(gApp.mouse_game && gApp.boot_tick && !gApp.menu_open)) { static int sk = 0; if (e.type == SDL_MOUSEMOTION && (sk++ % 200) == 0) gh_log("HOST MOUSEFWD skip mouse_game=%d boot=%d menu=%d x=%d y=%d", (int)gApp.mouse_game, (int)(gApp.boot_tick != 0), (int)gApp.menu_open, e.motion.x, e.motion.y); return false; }   /* t48c (#639 ③) */
        int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
        if (b >= 0) { kh_vmouse_button(b, e.type == SDL_MOUSEBUTTONDOWN); gh_log("HOST MOUSEFWD dx=0.0 dy=0.0 x=%d y=%d btn=%u button=%d down=%d", e.button.x, e.button.y, (unsigned)SDL_GetMouseState(NULL, NULL), b, e.type == SDL_MOUSEBUTTONDOWN ? 1 : 0); } return true; }
    default: return false;
    }
}
