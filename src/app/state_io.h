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
#include <zlib.h>
/* ---- 1b: 세이브 + 디스크 작업 사본 압축본. 상태 파일(slotNN.g98s) 옆 동반 파일 <상태>.dsk 에 작업 사본(쓰기 가능한 FD/HDD)의 zlib 압축본을 담는다.
   불러올 때 상태를 먼저 적용한 뒤 같은 핸들로 사본에 되써서 「디스크 내용 = 저장 시점」으로 맞춘다. 동반 파일이 없거나 짝이 안 맞으면 옛 방식(디스크는 지금 그대로). */
static bool gh_z_pack(const std::string & path, std::vector<uint8_t> & out, uint64_t & rawsz) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
    z_stream z; memset(&z, 0, sizeof z); if (deflateInit(&z, 1) != Z_OK) { fclose(f); return false; }
    std::vector<uint8_t> in(1 << 20), ob(1 << 20); rawsz = 0; bool ok = true; int fl = Z_NO_FLUSH;
    do {
        size_t n = fread(in.data(), 1, in.size(), f); rawsz += n; fl = n < in.size() ? Z_FINISH : Z_NO_FLUSH;
        z.next_in = in.data(); z.avail_in = (uInt)n;
        do { z.next_out = ob.data(); z.avail_out = (uInt)ob.size(); int r = deflate(&z, fl); if (r == Z_STREAM_ERROR) { ok = false; break; } out.insert(out.end(), ob.data(), ob.data() + (ob.size() - z.avail_out)); } while (z.avail_out == 0);
    } while (ok && fl != Z_FINISH);
    deflateEnd(&z); fclose(f); return ok;
}
static bool gh_z_unpack(const uint8_t * p, size_t n, uint64_t rawsz, FILE * f) {
    z_stream z; memset(&z, 0, sizeof z); if (inflateInit(&z) != Z_OK) return false;
    std::vector<uint8_t> ob(1 << 20); z.next_in = (Bytef *)p; z.avail_in = (uInt)n; uint64_t tot = 0; bool ok = fseeko64(f, 0, SEEK_SET) == 0; int r = Z_OK;
    while (ok && r != Z_STREAM_END) {
        z.next_out = ob.data(); z.avail_out = (uInt)ob.size(); r = inflate(&z, Z_NO_FLUSH);
        if (r != Z_OK && r != Z_STREAM_END) { ok = false; break; }
        size_t g = ob.size() - z.avail_out; if (g && fwrite(ob.data(), 1, g, f) != g) { ok = false; break; } tot += g;
        if (r == Z_OK && g == 0 && z.avail_in == 0) { ok = false; break; }
    }
    inflateEnd(&z); fflush(f); return ok && tot == rawsz;
}
static uint32_t gh_state_tailcrc(const std::string & sp) {   /* 상태 파일 맨 끝 4바이트 = 전체 CRC32 (동반 파일과의 짝 확인용) */
    FILE * f = fopen(sp.c_str(), "rb"); if (!f) return 0; uint8_t b[4] = { 0, 0, 0, 0 }; if (fseeko64(f, -4, SEEK_END) == 0) { if (fread(b, 1, 4, f) != 4) b[0] = b[1] = b[2] = b[3] = 0; } fclose(f);
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static void gh_dp32(std::vector<uint8_t> & o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back((uint8_t)(v >> (i * 8))); }
static void gh_dp64(std::vector<uint8_t> & o, uint64_t v) { for (int i = 0; i < 8; i++) o.push_back((uint8_t)(v >> (i * 8))); }
static uint32_t gh_dg32(const uint8_t * p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t gh_dg64(const uint8_t * p) { return (uint64_t)gh_dg32(p) | ((uint64_t)gh_dg32(p + 4) << 32); }
static size_t gh_dsk_save(const std::string & sp) {   /* 돌려주는 값 = 동반 파일 바이트(0 = 없음/실패) */
    std::string dp = sp + ".dsk"; std::vector<uint8_t> o; const char mg[4] = { 'G', '9', '8', 'D' }; o.insert(o.end(), mg, mg + 4); gh_dp32(o, 1); gh_dp32(o, gh_state_tailcrc(sp)); gh_dp32(o, 0);
    uint32_t cnt = 0;
    for (int d = 0; d < 4; d++) {
        if (gApp.disk_wp[d].empty() || !gApp.disk_fh[d]) continue;
        fflush(gApp.disk_fh[d]); std::vector<uint8_t> z; uint64_t raw = 0;
        if (!gh_z_pack(gApp.disk_wp[d], z, raw)) { gh_log("DSK pack FAIL drive %d %s", d, gApp.disk_wp[d].c_str()); plat::remove_file(dp.c_str()); return 0; }
        gh_dp32(o, (uint32_t)d); const std::string & nm = gApp.disk_nm[d]; o.push_back((uint8_t)(nm.size() & 255)); o.push_back((uint8_t)(nm.size() >> 8)); o.insert(o.end(), nm.begin(), nm.end());
        gh_dp64(o, raw); gh_dp64(o, (uint64_t)z.size()); o.insert(o.end(), z.begin(), z.end()); cnt++;
        gh_log("DSK drive %d %s raw=%llu zip=%llu", d, nm.c_str(), (unsigned long long)raw, (unsigned long long)z.size());
    }
    if (!cnt) { plat::remove_file(dp.c_str()); return 0; }
    for (int i = 0; i < 4; i++) o[12 + i] = (uint8_t)(cnt >> (i * 8));
    gh_dp32(o, gh_crc(o.data(), o.size()));
    std::string tmp = dp + ".tmp"; FILE * f = fopen(tmp.c_str(), "wb"); if (!f) return 0;
    size_t w = fwrite(o.data(), 1, o.size(), f); fclose(f); if (w != o.size()) { plat::remove_file(tmp.c_str()); return 0; }
    plat::remove_file(dp.c_str()); if (!plat::move_file(tmp, dp)) return 0;
    return o.size();
}
static bool gh_dsk_load(const std::string & sp) {   /* 상태 적용이 성공한 뒤 호출. 동반 파일이 없으면 true(옛 저장) */
    std::string dp = sp + ".dsk"; FILE * f = fopen(dp.c_str(), "rb"); if (!f) return true;
    std::vector<uint8_t> b; { std::vector<uint8_t> t(1 << 20); for (;;) { size_t n = fread(t.data(), 1, t.size(), f); if (!n) break; b.insert(b.end(), t.begin(), t.begin() + n); } } fclose(f);
    if (b.size() < 20 || memcmp(b.data(), "G98D", 4) != 0 || gh_dg32(&b[b.size() - 4]) != gh_crc(b.data(), b.size() - 4)) { gh_log("DSK companion file corrupt - disks left as is (%s)", dp.c_str()); return false; }
    if (gh_dg32(&b[8]) != gh_state_tailcrc(sp)) { gh_log("DSK companion does not match the state file - disks left as is (%s)", dp.c_str()); return false; }
    uint32_t cnt = gh_dg32(&b[12]); size_t pos = 16, end = b.size() - 4; bool all = true;
    for (uint32_t k = 0; k < cnt; k++) {
        if (pos + 6 > end) { all = false; break; } uint32_t d = gh_dg32(&b[pos]); pos += 4; size_t nl = (size_t)b[pos] | ((size_t)b[pos + 1] << 8); pos += 2;
        if (pos + nl + 16 > end) { all = false; break; } std::string nm((const char *)&b[pos], nl); pos += nl; uint64_t raw = gh_dg64(&b[pos]), zs = gh_dg64(&b[pos + 8]); pos += 16;
        if (pos + zs > end) { all = false; break; }
        if (d < 4 && gApp.disk_fh[d] && !gApp.disk_wp[d].empty() && gApp.disk_nm[d] == nm) { bool ok = gh_z_unpack(&b[pos], (size_t)zs, raw, gApp.disk_fh[d]); gh_log("DSK restore drive %u %s %s", d, nm.c_str(), ok ? "OK" : "FAIL"); if (!ok) all = false; }
        else gh_log("DSK skip drive %u %s (no matching disk in that drive)", d, nm.c_str());
        pos += (size_t)zs;
    }
    return all;
}
static bool gh_do_save(const std::string & path, bool test_extract) {
    gh_state_diag("before-save");
    std::vector<uint8_t> th;
    if (!gApp.fb.empty()) gh_png_mem(gApp.fb.data(), gApp.fbw, gApp.fbh, th);
    std::string err;
    bool ok = PC98_SaveState_ToFile(path.c_str(), gApp.disks, err, &th);
    gh_log("SAVE %s -> %s %s (THMB %u bytes)", path.c_str(), ok ? "OK" : "FAIL", err.c_str(), (unsigned)th.size());
    if (ok) { size_t zb = gh_dsk_save(path); plat::FileInfo fi; unsigned long sb = plat::stat_file(path, fi) ? (unsigned long)fi.size_lo : 0ul; gh_log("SAVE-SIZE state=%lu dsk=%zu total=%lu bytes", sb, zb, sb + (unsigned long)zb); }
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
    if (ok) { bool dk = gh_dsk_load(path); gh_log("LOAD-DSK %s", dk ? "OK" : "SKIP/FAIL"); }
    gh_state_diag("after-load");
    gApp.next_key = (unsigned)gApp.prof_.keys.size();   /* 자동 키 중단: 저장 시점 이후는 상태 그대로 */
    gApp.pic0 = PIC_Ticks;
    kha_ld_diag("load");
    return ok;
}
static std::string gh_slot_path(void) {
    plat::mkdir((gApp.savedir).c_str());
    std::string d = gApp.savedir + "\\" + gApp.prof_.id; plat::mkdir(d.c_str());
    return d + "\\quick.g98s";   /* 1b: F5/F9·화면 버튼 = 퀵 슬롯 */
}
/* ===================================================================== r012: 설정 적용·배속·키보드·화면 게시·게임 중 메뉴·부팅 연출 */
static std::string gh_slot_n(int gi, int slot /*1b: 0 자동 · 1 퀵 · 2~4 일반 1~3 (옛 slot01~03 = 일반 1~3 그대로, 옛 04~08 은 숨김)*/) {
    char b[700]; const char * id = gApp.ui_.games[gi].id.c_str();
    if (slot == 0) snprintf(b, sizeof b, "%s\\%s\\auto.g98s", gApp.savedir.c_str(), id); else if (slot == 1) snprintf(b, sizeof b, "%s\\%s\\quick.g98s", gApp.savedir.c_str(), id);
    else snprintf(b, sizeof b, "%s\\%s\\slot%02d.g98s", gApp.savedir.c_str(), id, slot - 1);
    return b;
}
static void gh_autosave(void) {   /* 1b: 「메인 메뉴로」·「끝내기」 때 자동 슬롯에 저장 */
    if (!gApp.boot_tick || gApp.cur_game < 0 || gApp.cur_game >= (int)gApp.ui_.games.size()) return;
    plat::mkdir(gApp.savedir.c_str()); plat::mkdir((gApp.savedir + "\\" + gApp.ui_.games[gApp.cur_game].id).c_str());
    bool ok = gh_do_save(gh_slot_n(gApp.cur_game, 0), false); gh_log("AUTOSAVE %s", ok ? "OK" : "FAIL");
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
