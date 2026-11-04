/* app/state_io.h -- S9a: dump helpers, save/load state, publish, apply settings, boost update. Moved unchanged from gensei_host.cpp lines 1400-1527 (as of t54s). Included in place; single translation unit. */
/* ------------------------------------------------------------------ events (called every emulated 1 ms tick) */
static void gh_dump_shot(unsigned int ms) {
    char p[1024];
    snprintf(p, sizeof(p), "%s\\shot_%s_%05u.png", gApp.outdir.c_str(), gApp.prof_.id, ms);
    if (!gApp.fb.empty()) gh_png(p, gApp.fb.data(), gApp.fbw, gApp.fbh); else gh_log("shot at %u ms: no frame yet", ms);
}
static uint32_t gh_crc(const uint8_t * p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); }
    return c ^ 0xFFFFFFFFu;
}
/* 저장 직전·로드 직후 상태 지문(설계서 §5 자동화): VRAM·GDC·본체 RAM·CPU */
static void gh_state_diag(const char * tag) {
    uint32_t c_vram = vga.mem.linear ? gh_crc(vga.mem.linear, (size_t)vga.mem.memsize) : 0;
    uint32_t c_gdc = gh_crc((const uint8_t *)&pc98_gdc[0], sizeof(pc98_gdc));
    std::vector<uint8_t> ram(0xA0000);
    for (unsigned i = 0; i < 0xA0000; i++) ram[i] = mem_readb((PhysPt)i);
    uint32_t c_ram = gh_crc(&ram[0], ram.size());
    gh_log("STATE-DIAG %s: vram_crc=%08x gdc_crc=%08x ram640_crc=%08x ticks=%u CS:IP=%04x:%04x AX=%04x SP=%04x FLAGS=%04x", tag, c_vram, c_gdc, c_ram, (unsigned)PIC_Ticks, SegValue(cs), (unsigned)reg_ip, (unsigned)reg_ax, (unsigned)reg_sp, (unsigned)reg_flags);
}
static void gh_dump_named(const char * tag, unsigned int ms) {
    char p[1024];
    snprintf(p, sizeof(p), "%s\\%s_%s_%05u.png", gApp.outdir.c_str(), tag, gApp.prof_.id, ms);
    if (!gApp.fb.empty()) gh_png(p, gApp.fb.data(), gApp.fbw, gApp.fbh); else gh_log("shot %s: no frame yet", tag);
}
/* r011: 저장 순간 화면(원본 640x400, 셰이더·확대 전)을 PNG 바이트로 — 상태 파일 THMB 항목에 넣는다 */
static void gh_png_wr(png_structp pp, png_bytep d, png_size_t n) { std::vector<uint8_t> * o = (std::vector<uint8_t> *)png_get_io_ptr(pp); o->insert(o->end(), d, d + n); }
static void gh_png_fl(png_structp) {}
static bool gh_png_mem(const uint32_t * fb, int w, int h, std::vector<uint8_t> & out) {
    out.clear();
    png_structp pp = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop pi = png_create_info_struct(pp);
    if (setjmp(png_jmpbuf(pp))) { png_destroy_write_struct(&pp, &pi); out.clear(); return false; }
    png_set_write_fn(pp, &out, gh_png_wr, gh_png_fl);
    png_set_IHDR(pp, pi, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(pp, pi);
    std::vector<unsigned char> row(w * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) { uint32_t v = fb[y * w + x]; row[x * 3] = (v >> 16) & 255; row[x * 3 + 1] = (v >> 8) & 255; row[x * 3 + 2] = v & 255; }
        png_write_row(pp, row.data());
    }
    png_write_end(pp, pi);
    png_destroy_write_struct(&pp, &pi);
    return true;
}
static bool gh_do_save(const std::string & path, bool test_extract) {
    gh_state_diag("before-save");
    std::vector<uint8_t> th;
    if (!gApp.fb.empty()) gh_png_mem(gApp.fb.data(), gApp.fbw, gApp.fbh, th);
    std::string err;
    bool ok = PC98_SaveState_ToFile(path.c_str(), gApp.disks, err, &th);
    gh_log("SAVE %s -> %s %s (THMB %u bytes)", path.c_str(), ok ? "OK" : "FAIL", err.c_str(), (unsigned)th.size());
    gh_state_diag("after-save");
    if (ok) {
        std::vector<uint8_t> back; std::string e2;
        bool r = PC98_SaveState_ReadThumb(path.c_str(), back, e2);
        gh_log("THUMB readback %s size=%u same=%d %s", r ? "OK" : "FAIL", (unsigned)back.size(), (int)(back == th), e2.c_str());
        if (test_extract && r && !back.empty()) {
            std::string tp = gApp.outdir + "\\thumb_extracted_" + gApp.prof_.id + ".png";
            FILE * f = fopen(tp.c_str(), "wb"); if (f) { fwrite(back.data(), 1, back.size(), f); fclose(f); }
        }
    }
    return ok;
}
static bool gh_do_load(const std::string & path) {
    gh_state_diag("before-load");
    std::string err;
    bool ok = PC98_SaveState_FromFile(path.c_str(), gApp.disks, err);
    gh_log("LOAD %s <- %s %s", path.c_str(), ok ? "OK" : "FAIL", err.c_str());
    gh_state_diag("after-load");
    gApp.next_key = (unsigned)gApp.prof_.keys.size();   /* 자동 키 중단: 저장 시점 이후는 상태 그대로 */
    gApp.pic0 = PIC_Ticks;
    kha_ld_diag("load");
    return ok;
}
static std::string gh_slot_path(void) {
    plat::mkdir((gApp.savedir).c_str());
    std::string d = gApp.savedir + "\\" + gApp.prof_.id; plat::mkdir(d.c_str());
    return d + "\\slot01.g98s";
}
/* ===================================================================== r012: 설정 적용·배속·키보드·화면 게시·게임 중 메뉴·부팅 연출 */
static std::string gh_slot_n(int gi, int slot /*0..7*/) {
    char b[700]; snprintf(b, sizeof b, "%s\\%s\\slot%02d.g98s", gApp.savedir.c_str(), gApp.ui_.games[gi].id.c_str(), slot + 1); return b;
}
static void gh_publish(const uint32_t * px, int w, int h) {
#if defined(__ANDROID__)
    /* Android software path (no GL): draw the menu/boot canvas into the 4:3 outer frame; touch/keys come through the normal SDL events */
    if (!gApp.headless && !gApp.use_gl && gApp.ren && gApp.win) {
        static SDL_Texture * mt = NULL; static int mw = 0, mh = 0;
        if (!mt || mw != w || mh != h) { if (mt) SDL_DestroyTexture(mt); mt = SDL_CreateTexture(gApp.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h); mw = w; mh = h; }
        if (mt) { SDL_UpdateTexture(mt, NULL, px, w * 4); SDL_RenderSetLogicalSize(gApp.ren, w, w * 3 / 4); SDL_RenderClear(gApp.ren); SDL_Rect d; d.x = 0; d.w = w; if (gApp.fit == FIT_1610) { d.h = h; d.y = (w * 3 / 4 - h) / 2; } else { d.y = 0; d.h = w * 3 / 4; } SDL_RenderCopy(gApp.ren, mt, NULL, &d); SDL_RenderPresent(gApp.ren); }
        return;
    }
#endif
    if (gApp.headless || !gApp.use_gl || !gApp.fb_mtx) return;
    SDL_LockMutex(gApp.fb_mtx);
    gApp.pub.assign(px, px + (size_t)w * h); gApp.pub_w = w; gApp.pub_h = h; gApp.pub_new = true; gh_ovl_off();
    SDL_UnlockMutex(gApp.fb_mtx);
}
/* t35 (#270): 볼륨 재배정. 믹서 채널 「PC-98」(FM 보드 전체)은 1.0 고정, BGM 슬라이더 = FM·SSG·리듬 부품 볼륨, SE 슬라이더 = ADPCM·PCM86 부품 볼륨 + 스피커 계열 채널.
   부품 볼륨은 코어 np2 사운드 setter(0~128, 기본 128 = 슬라이더 8/8). 「FM」 채널은 코어에 없어 예전 BGM 슬라이더는 무효였다. */
extern "C" { void opngen_setvol(unsigned vol); void psggen_setvol(unsigned vol); void rhythm_setvol(unsigned vol); void rhythm_update(void * rhy);
              void adpcm_setvol(unsigned vol); void adpcm_update(void * ad); void pcm86gen_setvol(unsigned vol); extern char rhythm[]; extern char adpcm[]; }
static void gh_apply_volume(const ui::Settings & s) {
    static const char * const se_names[] = { "SPKR", "PCSPEAKER", "PCM", "SB", NULL };
    static int last_b = -1, last_s = -1;
    MixerChannel * m = MIXER_FindChannel("PC-98"); if (m) m->SetVolume(1.0f, 1.0f);
    for (int i = 0; se_names[i]; i++) { MixerChannel * d = MIXER_FindChannel(se_names[i]); if (d) { float v = s.se_gain(); d->SetVolume(v, v); } }
    unsigned nb = (unsigned)(s.bgm_gain() * 128.0f + 0.5f), ns = (unsigned)(s.se_gain() * 128.0f + 0.5f);
    opngen_setvol(nb); psggen_setvol(nb); rhythm_setvol(nb); rhythm_update(rhythm);
    adpcm_setvol(ns); adpcm_update(adpcm); pcm86gen_setvol(ns);
    if (s.bgm != last_b || s.se != last_s) { last_b = s.bgm; last_s = s.se; gh_log("VOLUME parts: bgm=%d -> fm/ssg/rhythm=%u | se=%d -> adpcm/pcm86=%u + spk | PC-98 ch=%p(1.0) SPKR=%p", s.bgm, nb, s.se, ns, (void *)m, (void *)MIXER_FindChannel("SPKR")); }
}
static void gh_apply_settings(const ui::Settings & s, bool vol) {
    gApp.bosd_on = s.bosd != 0; gApp.fps_on = s.fpsd != 0; kbov::g_btn_on = kbov::g_cli_btn >= 0 ? kbov::g_cli_btn != 0 : s.kbbtn != 0; kbov::g_ov_on = kbov::g_cli_ov >= 0 ? kbov::g_cli_ov != 0 : s.ovbtn != 0; vpad::g_mode = vpad::g_cli >= 0 ? vpad::g_cli : s.padbtn; gApp.kbshader = gApp.cli_kbsh >= 0 ? gApp.cli_kbsh : (s.kbshader != 0 ? 1 : 0);
    gApp.fit = gApp.menu43 ? (int)FIT_43 : s.ratio; { bool fxany = s.scan > 0 || s.mask > 0 || s.bleed > 0 || s.glow > 0 || s.curve > 0; gApp.shader = fxany ? SH_CRT : (s.shader == 2 ? SH_NEAREST : SH_SHARP); gApp.nearv = (s.shader == 2) ? 1.0f : 0.0f;   /* t48c: 효과가 하나라도 켜져 있으면 효과 셰이더(밑 그림은 uNear), 아니면 sharp/nearest 셰이더 */
      static int lfx = -1; int kfx = (s.scan << 0) | (s.mask << 4) | (s.bleed << 8) | (s.glow << 12) | (s.curve << 16) | ((s.shader == 2 ? 1 : 0) << 20); if (kfx != lfx) { lfx = kfx; gh_log("FX filter=%s scan=%d mask=%d rgb=%d glow=%d curve=%d -> shader=%d", s.shader == 2 ? "nearest" : "sharp", s.scan, s.mask, s.bleed, s.glow, s.curve, (int)gApp.shader); } }
    { static int lk = -1; if (lk != (int)gApp.kbshader) { lk = (int)gApp.kbshader; gh_log("KBSHADER %d (0=오버레이 평면 합성, 1=셰이더 적용) cli=%d", (int)gApp.kbshader, gApp.cli_kbsh); } }
    gApp.curve = s.curve_f(); gApp.scanv = s.scan > 0 ? 2.0f + (float)s.scan : 0.0f; gApp.maskv = s.mask_f(); gApp.bleedv = s.bleed_f(); gApp.glowv = s.glow_f();
    if (vol && gApp.boot_tick) gh_apply_volume(s);
}
/* ---- 배속 */
static void gh_boost_update(void) {
    const ui::Settings s = gApp.ui_.effective(gApp.cur_game);
    bool want = !gApp.menu_open && ((gApp.boost_hold && s.bmode != 1) || (gApp.boost_tog && s.bmode != 0));
    if (want == gApp.boost_on) return;
    gApp.boost_on = want;
    emulator_speed = want ? 100u * (unsigned)s.boost_x() : 100u;
    { int bs = gApp.bsound_override >= 0 ? gApp.bsound_override : s.bsound;   /* r017: 0 끔(무음) · 1 빠르게 · 2 음높이 유지 */
      if (want && bs != 0) { gensei_ba_calls = gensei_ba_under = gensei_ba_drops = 0; gensei_ba_sumsq = 0; gensei_ba_n = 0; gensei_ba_xq = s.boost_x() * 100; gensei_ba_gain = s.bs_gain256(); gensei_ba_mode = bs; try { MENU_mute(false); } catch (...) {} }
      else { gensei_ba_mode = 0; try { MENU_mute(want); } catch (...) { gh_log("WARN: MENU_mute threw"); } }
      if (want) gh_log("BOOSTSOUND mode=%d x=%d gain=%d/256", bs, s.boost_x(), s.bs_gain256()); else gh_log("BOOSTSOUND off (calls=%u under=%u drops=%u rho=%.2f rms=%.1f)", gensei_ba_calls, gensei_ba_under, gensei_ba_drops, gensei_ba_rho, gensei_ba_n ? sqrt(gensei_ba_sumsq / (double)gensei_ba_n) : 0.0); }
    gApp.boost_xv = s.boost_x(); gApp.bosd_on = s.bosd != 0; gApp.fps_on = s.fpsd != 0; kbov::g_btn_on = kbov::g_cli_btn >= 0 ? kbov::g_cli_btn != 0 : s.kbbtn != 0; kbov::g_ov_on = kbov::g_cli_ov >= 0 ? kbov::g_cli_ov != 0 : s.ovbtn != 0; vpad::g_mode = vpad::g_cli >= 0 ? vpad::g_cli : s.padbtn; gApp.kbshader = gApp.cli_kbsh >= 0 ? gApp.cli_kbsh : (s.kbshader != 0 ? 1 : 0);
    gh_log("BOOST %s x%d wall=%u game=%u", want ? "on" : "off", s.boost_x(), gApp.boot_tick ? SDL_GetTicks() - gApp.boot_tick : 0u, (unsigned)(PIC_Ticks - gApp.pic0));
}
