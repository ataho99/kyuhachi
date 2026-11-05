/* app/ui_env.h -- S9b: UI environment, game list, titlebar, add-works, help, key map. Moved unchanged from gensei_host.cpp lines 841-1313 (as of t54t). Included in place; single translation unit. */
/* ---- 슬롯 정보·THMB */
static bool gh_png_decode(const std::vector<uint8_t> & png, std::vector<uint32_t> & out, int & w, int & h) {
    png_image im; memset(&im, 0, sizeof im); im.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&im, png.data(), png.size())) return false;
    im.format = PNG_FORMAT_BGRA; w = (int)im.width; h = (int)im.height; out.assign((size_t)w * h, 0);
    bool ok = png_image_finish_read(&im, NULL, out.data(), 0, NULL) != 0; png_image_free(&im); return ok;
}
static void gh_refresh_game_slots(int gi) {
    ui::UiGame & G = gApp.ui_.games[gi]; G.lastslot = -1; G.thmb.clear(); unsigned long long best = 0;
    for (int i = 0; i < 5; i++) {
        ui::SlotInfo & s = G.slot[i]; s = ui::SlotInfo();
        std::string p = gh_slot_n(gi, i); plat::FileInfo fa;
        if (!plat::stat_file(p, fa)) continue;
        s.used = true; s.bytes = (int)fa.size_lo; { plat::FileInfo fd_; if (plat::stat_file(p + ".dsk", fd_)) s.bytes += (int)fd_.size_lo; }   /* 1b: 디스크 압축본 포함 */
        plat::LocalTime st = plat::local_from_filetime(fa.mtime_ft);
        char b[32]; snprintf(b, sizeof b, "%02d-%02d %02d:%02d", st.month, st.day, st.hour, st.minute); s.stamp = b;
        unsigned long long t = fa.mtime_ft;
        if (t >= best) { best = t; G.lastslot = i; }
        std::vector<uint8_t> png; std::string err;
        if (PC98_SaveState_ReadThumb(p.c_str(), png, err) && !png.empty()) gh_png_decode(png, s.th, s.tw_, s.th_);
    }
    if (G.lastslot >= 0) { G.thmb = G.slot[G.lastslot].th; G.tw_ = G.slot[G.lastslot].tw_; G.th_ = G.slot[G.lastslot].th_; }
}
/* t48e (#646): 키 이름 → 키. 옛 이름은 gh_keyname 이 먼저 받아 지금과 똑같이 동작하고(rollup/pageup/rolldown/pagedown 포함), 거기에 없는 이름만 kbtable 라벨 표(pn::lookup)로 찾는다 */
static KBD_KEYS gh_keyname_pad(const std::string & n) {
    KBD_KEYS k = gh_keyname(n); if (k != KBD_NONE) return k;
    return pn::lookup(n, gh_kb_ibm());
}
static const char * const GH_PADBTN[7] = { "btn_b", "btn_a", "btn_x", "btn_start", "btn_leftshoulder", "btn_rightshoulder", "btn_back" };
static const int GH_PADIDX[7] = { PB_EAST, PB_SOUTH, PB_WEST, PB_START, PB_L1, PB_R1, PB_BACK };
static bool gh_pad_parse(const std::string & v, GPadBind & b) {   /* none / mouse_left / mouse_right / 키 이름 */
    std::string s = v; for (auto & c : s) c = (char)tolower((unsigned char)c);
    if (s == "none") { b.type = PBT_NONE; b.key = KBD_NONE; return true; }
    if (s == "mouse_left") { b.type = PBT_ML; b.key = KBD_NONE; return true; }
    if (s == "mouse_right") { b.type = PBT_MR; b.key = KBD_NONE; return true; }
    KBD_KEYS k = gh_keyname_pad(s); if (k == KBD_NONE) return false; b.type = PBT_KEY; b.key = k; return true;
}
/* 프로필의 패드 표·never_map 을 입력 층에 적용 (#237: never_map 은 패드·단축키에 절대 배정 금지)
 * t48e: 순서 = 기본값(서양식이면 동·남 맞바꿈) < 전역 settings.ini btn_* < 프로필 옛 o/x/sq < 프로필 btn_* (이 작품 user.ini [pad] 가 가장 늦게 얹혀 이긴다).
 * refresh = true 면 (게임 도중 조작 설정을 바꿨을 때) 이 작품 user.ini [pad] 의 btn_* 를 파일에서 다시 읽는다 */
static void gh_apply_profile_input(const GProfile & p, bool refresh = false) {
    memset(kh_never, 0, sizeof(kh_never));
    std::vector<std::string> nm_all;   /* t48e3 (#662): never_map 은 ',' 와 '+' 둘 다 구분자 (예: XFER+ROLLUP+ROLLDOWN) */
    for (auto & part : prof::split_list(p.never_map)) for (auto & sub : prof::split_list(part, '+')) nm_all.push_back(sub);
    for (auto & nm : nm_all) { KBD_KEYS k = gh_keyname_pad(nm); if (k != KBD_NONE && (int)k < 256) { kh_never[(int)k] = true; gh_log("never_map: %s -> key %d", nm.c_str(), (int)k); } else gh_log("never_map: %s -> (알 수 없는 이름, 무시)", nm.c_str()); }
    if (!refresh) kh_western = (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size() ? gApp.ui_.effective(gApp.cur_game) : gApp.ui_.g).pad == 1;
    kh_pb_defaults();
    std::map<std::string, std::string> pad = p.pad;
    if (refresh && gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) {
        for (auto it = pad.begin(); it != pad.end();) { if (it->first.compare(0, 4, "btn_") == 0) it = pad.erase(it); else ++it; }
        std::map<std::string, std::string> m; prof::get_pad_keys(gApp.ui_.games[gApp.cur_game].dir, m); for (auto & kv : m) pad[kv.first] = kv.second;
    }
    /* 1) 전역 btn_* */
    for (int i = 0; i < 7; i++) { auto it = gApp.ui_.gpad.find(GH_PADBTN[i]); if (it != gApp.ui_.gpad.end() && !it->second.empty()) { GPadBind b; if (gh_pad_parse(it->second, b)) kh_pb[GH_PADIDX[i]] = b; else gh_log("pad: 전역 %s=%s 알 수 없는 이름, 무시", GH_PADBTN[i], it->second.c_str()); } }
    /* 2) 프로필 옛 이름 o/x/sq — 옛 gh_keyname 으로 지금과 똑같이 */
    auto get = [&](const char * k) -> std::string { auto it = pad.find(k); return it == pad.end() ? std::string() : it->second; };
    std::string o = get("o"), x = get("x"), sq = get("sq");
    /* t48e3 (#662): 옛 o=/x=/sq= 값이 기본값(space/esc/caps)과 같으면 「기본 따름」 — 결정 방식(일본식/서양식)과 전역 btn_* 을 따른다. 기본과 다른 옛 값만 지금처럼 우선 */
    { auto lc = [](std::string s) { for (auto & c : s) c = (char)tolower((unsigned char)c); return s; };
      if (lc(o) == "space") { gh_log("pad: 옛 o=%s = 기본값 → 기본 따름", o.c_str()); o.clear(); }
      if (lc(x) == "esc") { gh_log("pad: 옛 x=%s = 기본값 → 기본 따름", x.c_str()); x.clear(); }
      if (lc(sq) == "caps") { gh_log("pad: 옛 sq=%s = 기본값 → 기본 따름", sq.c_str()); sq.clear(); } }
    if (o == "mouse_left") { kh_padmouse = true; gh_log("pad: mouse mode (o=mouse_left)"); }
    else if (!o.empty()) { KBD_KEYS k = gh_keyname(o); if (k != KBD_NONE) { kh_pb[PB_EAST].type = PBT_KEY; kh_pb[PB_EAST].key = k; } }
    if (!x.empty() && x != "mouse_right") { KBD_KEYS k = gh_keyname(x); if (k != KBD_NONE) { kh_pb[PB_SOUTH].type = PBT_KEY; kh_pb[PB_SOUTH].key = k; } }
    if (!sq.empty()) { KBD_KEYS k = gh_keyname(sq); if (k != KBD_NONE) { kh_pb[PB_WEST].type = PBT_KEY; kh_pb[PB_WEST].key = k; } }
    /* 3) 프로필 btn_* (옛 이름 위에 얹는다) */
    for (int i = 0; i < 7; i++) { std::string v = get(GH_PADBTN[i]); if (v.empty()) continue; GPadBind b; if (gh_pad_parse(v, b)) kh_pb[GH_PADIDX[i]] = b; else gh_log("pad: 프로필 %s=%s 알 수 없는 이름, 무시", GH_PADBTN[i], v.c_str()); }
    /* never_map 에 걸린 키는 옛 규칙대로 되돌린다: 동 → Space(없으면 Esc) · 남 → Esc · 서 → 없음. 그 밖의 버튼은 없음 */
    for (int i = 0; i < PB_N; i++) if (kh_pb[i].type == PBT_KEY && (int)kh_pb[i].key < 256 && kh_never[(int)kh_pb[i].key]) {
        if (i == PB_EAST) kh_pb[i].key = KBD_space; else if (i == PB_SOUTH) kh_pb[i].key = KBD_esc; else { kh_pb[i].type = PBT_NONE; kh_pb[i].key = KBD_NONE; } }
    auto nm = [](const GPadBind & b) -> std::string { if (b.type == PBT_NONE) return "none"; if (b.type == PBT_ML) return "mouse_left"; if (b.type == PBT_MR) return "mouse_right"; return std::to_string((int)b.key); };
    gh_log("pad: western=%d east=%s south=%s west=%s start=%s l1=%s r1=%s select=%s%s", kh_western ? 1 : 0, nm(kh_pb[PB_EAST]).c_str(), nm(kh_pb[PB_SOUTH]).c_str(), nm(kh_pb[PB_WEST]).c_str(), nm(kh_pb[PB_START]).c_str(), nm(kh_pb[PB_L1]).c_str(), nm(kh_pb[PB_R1]).c_str(), nm(kh_pb[PB_BACK]).c_str(), refresh ? " (갱신)" : "");
    GPLOG("BINDS western=%d east=%s south=%s west=%s start=%s l1=%s r1=%s select=%s", kh_western ? 1 : 0, nm(kh_pb[PB_EAST]).c_str(), nm(kh_pb[PB_SOUTH]).c_str(), nm(kh_pb[PB_WEST]).c_str(), nm(kh_pb[PB_START]).c_str(), nm(kh_pb[PB_L1]).c_str(), nm(kh_pb[PB_R1]).c_str(), nm(kh_pb[PB_BACK]).c_str());
}

/* ------------------------------------------------------------------ t39: 실행 방법 UI 가 쓰는 환경(파일·저장) */
#define KH_WT_SUFFIX " - " KH_FULLNAME   /* t49a (#811): 창 제목 접미(항상 전부 남김, 30바이트) */
#define KH_WT_TITLEMAX (113 - (int)(sizeof(KH_WT_SUFFIX) - 1))                /* 제목 쪽 상한 = 옛 합계 113 - 접미 30 */
static std::string gh_window_title(const ui::UiGame & tg, bool legacy) {   /* 작품 제목만 줄임, 한글 3바이트 경계 유지 */
    std::string tt = tg.title;
    if (!legacy && tt.size() > KH_WT_TITLEMAX) { if (!tg.short_t.empty()) tt = tg.short_t; if (tt.size() > KH_WT_TITLEMAX) { size_t k = KH_WT_TITLEMAX - 3; while (k > 0 && ((unsigned char)tt[k] & 0xC0) == 0x80) k--; tt = tt.substr(0, k) + "..."; } }
    return tt + KH_WT_SUFFIX;
}
static bool gh_make_blank_hdi(const std::string & path, int mb) {
    const uint32_t ss = 256, sec = 33, sur = 8; uint64_t cyl = ((uint64_t)(mb < 1 ? 1 : mb) * 1048576ull) / (ss * sec * sur); if (cyl < 1) cyl = 1; if (cyl > 4096) cyl = 4096;
    uint32_t sz = (uint32_t)(ss * sec * sur * cyl);
    FILE * f = fopen(path.c_str(), "wb"); if (!f) return false;
    uint32_t h[8] = { 0, 0, 0x1000, sz, ss, sec, sur, (uint32_t)cyl }; fwrite(h, 1, sizeof h, f);
    static unsigned char z[65536]; size_t pad = 0x1000 - sizeof h; bool ok = fwrite(z, 1, pad, f) == pad;
    for (uint32_t left = sz; ok && left;) { size_t k = left > sizeof z ? sizeof z : left; ok = fwrite(z, 1, k, f) == k; left -= (uint32_t)k; }
    fclose(f); if (!ok) plat::remove_file(path.c_str()); return ok;
}
static void gh_ui_env_setup(void) {
    ui::Env & E = gApp.ui_.env; E.sep = '\\';
    E.list_dir = [](const std::string & p) {
        std::vector<ui::FileItem> r; std::string pa = aw::sys_from_ini(p); std::vector<std::string> fs, ds; aw::list_dir(pa, fs, &ds);
        std::sort(ds.begin(), ds.end()); std::sort(fs.begin(), fs.end());
        for (auto & d : ds) { ui::FileItem f; f.name = aw::acp_to_u8(d); f.dir = true; r.push_back(f); }
        for (auto & n : fs) { ui::FileItem f; f.name = aw::acp_to_u8(n); uint64_t sz = 0, mt = 0; if (aw::file_stat(aw::join(pa, n), sz, mt)) f.size = (long long)sz; r.push_back(f); }
        return r; };
    E.roots = []() { return plat::drive_roots(); };
    E.has_img = [](int g, const std::string & n) {
        if (n.empty() || g < 0 || g >= (int)gApp.ui_.games.size()) return false; std::string pa = aw::sys_from_ini(n);
        bool abs = (pa.size() > 1 && pa[1] == ':') || pa[0] == '\\' || pa[0] == '/'; std::string full = abs ? pa : aw::join(gApp.ui_.imgdir_of(g), pa);
        uint64_t sz = 0, mt = 0; return aw::file_stat(full, sz, mt); };
    E.save = [](int g, const std::string & name, const ent::Kv & kv, const std::string & setdef) { return prof::save_entry(gApp.ui_.games[g].dir, name, kv, setdef); };
    E.del = [](int g, const std::string & name, bool reset) { return prof::delete_entry(gApp.ui_.games[g].dir, name, reset); };
    E.setkey = [](int g, const std::string & k, const std::string & v) { return prof::set_game_key(gApp.ui_.games[g].dir, k, v); };
    E.pad_get = [](int g, std::map<std::string, std::string> & m) { m.clear(); if (g >= 0 && g < (int)gApp.ui_.games.size()) prof::get_pad_keys(gApp.ui_.games[g].dir, m); };   /* t48e (#646): 이 작품 user.ini [pad] btn_* */
    E.pad_set = [](int g, const std::string & k, const std::string & v) { if (g < 0 || g >= (int)gApp.ui_.games.size()) return false; bool ok = prof::set_pad_key(gApp.ui_.games[g].dir, k, v); gh_log("pad_set: %s [pad] %s=%s -> %s", gApp.ui_.games[g].dir.c_str(), k.c_str(), v.c_str(), ok ? "OK" : "FAIL"); return ok; };
    E.padkind = []() { return gensei_padkind(); };
    E.padkeys = []() { return pn::picker_names(); };
    E.hdd_make = [](int g, int mb, std::string & out) {
        std::string dir = gApp.ui_.imgdir_of(g), id = gApp.ui_.games[g].id; for (auto & ch : id) if (!(isalnum((unsigned char)ch) || ch == '_' || ch == '-')) ch = '_';
        for (int i = 1; i < 100; i++) { std::string nm = id + "_hdd_" + std::to_string(i) + ".hdi"; uint64_t sz, mt; if (aw::file_stat(aw::join(dir, nm), sz, mt)) continue;
            if (!gh_make_blank_hdi(aw::join(dir, nm), mb)) return false; out = nm; gh_log("hdd_make: %s %dMB", nm.c_str(), mb); return true; }
        return false; };
    E.save_test = [](int g, const std::string & name, const ent::Kv & kv) {
        (void)g; std::string td = gApp.exedir + "\\logs\\_test"; plat::mkdir((gApp.exedir + "\\logs").c_str()); plat::mkdir(td.c_str());
        plat::remove_file((td + "\\user.ini").c_str()); return prof::save_entry(td, name, kv, name); };
    E.resolved = [](int g, ent::Kv & out) {
        out.clear(); if (g < 0 || g >= (int)gApp.ui_.games.size()) return false;
        std::string keep = prof::g_entry_req; prof::g_entry_req.clear(); GProfile p; bool ok = gh_profile(gApp.ui_.games[g].id, p); prof::g_entry_req = keep; if (!ok) return false;
        static const char * K[4] = { "disk_a", "disk_b", "disk_c", "disk_d" };
        for (int i = 0; i < 4; i++) if (p.disk[i]) out[K[i]] = aw::acp_to_u8(p.disk[i]);
        out["boot"] = std::string(1, p.bootletter); { auto it = p.kv.find("cd"); if (it != p.kv.end() && !it->second.empty()) out["cd"] = it->second; }
        for (const char * k : { "mouse", "font", "font_kind", "bootkeys", "autoexec", "dosexe", "dosargs" }) { auto it = p.kv.find(k); if (it != p.kv.end() && !it->second.empty()) out[k] = it->second; }
        return true; };
    E.exelist = [](int g, const std::string & dk, const std::string & dir, std::vector<ui::FileItem> & out) {   /* 2b: 이미지 안 파일 목록 */
        out.clear(); if (g < 0 || g >= (int)gApp.ui_.games.size() || dk.empty()) return false;
        std::string keep = prof::g_entry_req; prof::g_entry_req.clear(); GProfile p; bool ok = gh_profile(gApp.ui_.games[g].id, p); prof::g_entry_req = keep; if (!ok) return false;
        std::string full = aw::u8_to_acp(dk); if (!(full.size() > 1 && (full[1] == (char)58 || full[0] == (char)47))) full = p.imgdir + "\\" + full;   /* 이름이면 작품 이미지 폴더 기준 */
        std::vector<prof::DirEnt> v; if (!prof::dos_browse(full, dir, v)) return false;
        for (auto & e : v) { ui::FileItem x; x.name = e.name; x.dir = e.dir; x.size = (long long)e.size; out.push_back(x); } return true; };
    E.reload = [](int g) { ulist::reload_one(gApp.ui_.games, g, &gApp.vlabel); int keep = gApp.ui_.cur; gApp.ui_.rebuild_vis(keep); };
    E.values = [](int g, std::vector<ui::ValRow> & out) {   /* t48a (#614 ③): 키별 최종값·출처·덮인 값(기본 항목 기준) */
        out.clear(); if (g < 0 || g >= (int)gApp.ui_.games.size()) return false;
        std::string keep = prof::g_entry_req; prof::g_entry_req.clear(); GProfile p; bool ok = gh_profile(gApp.ui_.games[g].id, p); prof::g_entry_req = keep; if (!ok) return false;
        static const char * const PRI[] = { "title", "disk_a", "disk_b", "disk_c", "disk_d", "boot", "swap", "cd", "keys", "bootkeys", "autoexec", "cycles", "font", "font_kind", "lang_eff", "lang", "mouse", "sound", "joystick", "entry_name" };
        std::vector<std::string> ks; for (auto & kv : p.kv) if (kv.first.compare(0, 6, "__val.") == 0) ks.push_back(kv.first.substr(6));
        auto pr = [&](const std::string & k) -> int { for (size_t i = 0; i < sizeof PRI / sizeof PRI[0]; i++) if (k == PRI[i]) return (int)i; return 1000; };
        std::stable_sort(ks.begin(), ks.end(), [&](const std::string & a, const std::string & b) { int x = pr(a), y = pr(b); return x != y ? x < y : a < b; });
        for (auto & k : ks) { ui::ValRow r; r.key = k; r.val = p.kv["__val." + k]; auto s = p.kv.find("__src." + k); r.src = s == p.kv.end() ? std::string() : s->second; auto c = p.kv.find("__cov." + k); r.cov = c == p.kv.end() ? std::string() : c->second; if (r.val.empty()) r.val = phr_("(비어 있음)"); out.push_back(r); }
        { ui::ValRow r; r.key = phr_("(실행 막힘 사유)"); r.val = gApp.ui_.games[g].block.empty() ? std::string(phr_("없음 (실행 가능)")) : gApp.ui_.games[g].block; r.src = phr_("계산"); out.push_back(r); }
        return true; };
    E.font_group = [](const std::string & nm) { for (auto & f : gApp.ui_.fonts) if (f.name == nm) return f.group; return std::string(); };
}
static void gh_delete_slot(int gi, int slot) { plat::remove_file(gh_slot_n(gi, slot).c_str()); plat::remove_file((gh_slot_n(gi, slot) + ".dsk").c_str()); gh_refresh_game_slots(gi); }   /* S3 retro-test token */
static void gh_spawn_test(const std::string & id, const std::string & entry, int secs) {
    std::string exe = plat::exe_path(); std::string td = gApp.exedir + "\\logs\\_test";
    std::string cmd = "\"" + exe + "\" \"" + id + "\" --entry \"" + aw::u8_to_acp(entry) + "\" --userini \"" + td + "\\user.ini\" --sec " + std::to_string(secs > 0 ? secs : 20) + " --bkosd 1 --savedir \"" + td + "\\saves\" --windowed --noboot --out \"" + td + "\"";
    unsigned long perr = 0;
    if (plat::spawn(exe, cmd, gApp.exedir, &perr)) { gh_log("test run: %s", cmd.c_str()); }
    else gh_log("test run FAIL err=%lu", (unsigned long)perr);
}
static void gh_launch_note(ui::Ui & U, const ui::Act & a) {   /* 실행 요청의 항목 이름을 프로필 결정에 넘기고 최근 실행 시각을 적는다 */
    prof::g_entry_req = (a.t == ui::A_START) ? a.entry : (a.entry.empty() ? U.sel_entry : a.entry);
    if (a.game >= 0 && a.game < (int)U.games.size()) U.games[a.game].last_run = (long long)time(NULL);
}
static void gh_rebuild_fds(const GProfile & p, ui::UiGame & g) {   /* 디스크 교체 후보 = 지금 실행하는 항목의 배치 */
    std::string demo = p.kv.count("demo") ? p.kv.at("demo") : std::string(); std::vector<std::string> files; auto addf = [&](const char * f) { if (!f || !*f) return; for (auto & x : files) if (x == f) return; files.push_back(f); };
    for (int i = 0; i < 2; i++) addf(p.disk[i]); for (auto & s : p.swap) addf(s.c_str()); addf(demo.c_str());
    auto flb = [&](const std::string & f) -> std::string { std::string n = f; size_t dd = n.find_last_of('.'); if (dd != std::string::npos && n.size() - dd <= 5) n = n.substr(0, dd); return n; };
    g.fds.clear(); g.cur_fd[0].clear(); g.cur_fd[1].clear(); for (auto & f : files) g.fds.push_back(flb(f));
    for (int i = 0; i < 2; i++) if (p.disk[i]) g.cur_fd[i] = flb(p.disk[i]);
    gApp.fdfiles[p.id] = files;
}
static void gh_ui_init_games(void) {
    struct ErGuard { std::string k; ErGuard() : k(prof::g_entry_req) { prof::g_entry_req.clear(); } ~ErGuard() { prof::g_entry_req = k; } } er_guard;   /* t48a (#615/#616): 직전 실행의 항목 이름(전역 한 칸)이 모든 작품의 목록 판정에 끼지 않게 한다 */
    gApp.ui_.games.clear(); gApp.vlabel.clear(); gh_ui_env_setup();
    {   /* t39: 글꼴 폴더 자동 등록 */
        std::string fd = aw::dir_of(gh_res_font_path("kyuhachi_font")); std::vector<std::string> ff, fs2; aw::list_dir(fd, ff, &fs2);
        std::vector<fontreg::Info> inf = fontreg::scan(fd, ff, "\\"); gApp.ui_.fonts.clear();
        for (auto & i : inf) { ui::UiFont u; u.name = i.name; u.group = i.group; u.path = i.path; u.why = i.why; u.ok = i.ok; gApp.ui_.fonts.push_back(u); }
    }
    struct E { int order; ui::UiGame g; };
    std::vector<E> es;
    std::vector<std::string> files, subs; gh_list_dir(gApp.exedir + "\\games", files, &subs);
    std::sort(subs.begin(), subs.end());
    auto lbl = [](const GProfile & p, int i) -> std::string {
        static const char L[4] = { 'A', 'B', 'C', 'D' };
        std::string k = std::string("label_") + (char)('a' + i); auto it = p.kv.find(k);
        if (it != p.kv.end()) return std::string(1, L[i]) + ": " + it->second;
        if (!p.disk[i]) return std::string(); std::string n = p.disk[i]; size_t d = n.find_last_of('.'); if (d != std::string::npos && n.size() - d <= 5) n = n.substr(0, d);
        if (n.size() > 34) n = n.substr(0, 31) + "...";
        return std::string(1, L[i]) + ": " + n;
    };
    for (auto & d : subs) {
        GProfile p; if (!gh_profile(d, p)) continue;
        if (p.kv.count("dup_of")) continue;   /* 1b-3: 같은 이미지의 중복 작품은 목록에서만 숨긴다(폴더는 옮기지 않음) */
        E e; e.order = 100; auto o = p.kv.find("order"); if (o != p.kv.end()) e.order = atoi(o->second.c_str());
        e.g.id = d; e.g.title = p.title; e.g.src = p.src; e.g.ov = p.ov;
        e.g.title_db = p.title;   /* t49a (#809): 사용자 지정 표시 제목 — user.ini [game] title_user= (비면 DB·game.ini 제목) */
        { prof::Ini u_; std::string ud = gApp.exedir + "\\games\\" + d; if (prof::ini_load(ud + "/user.ini", u_) && u_.count("game") && u_["game"].count("title_user")) e.g.title_user = u_["game"]["title_user"]; if (!e.g.title_user.empty()) e.g.title = e.g.title_user; }
        {   /* t39: 언어·short·변형·글꼴 묶음 */
            ulist::fill(e.g, gApp.exedir + "\\games\\" + d, p.imgdir, p.kv, p.title); e.g.need_install = p.install_required;
            if (!e.g.title_user.empty()) e.g.short_t.clear();   /* t49a: 바꾼 제목은 자동 줄임을 새로 만든다 */
            { auto vl = p.kv.find("variant_label"); if (vl != p.kv.end() && !vl->second.empty()) gApp.vlabel[d] = vl->second; }
            std::string fn = p.font; { size_t sl = fn.find_last_of("\\/"); if (sl != std::string::npos) fn = fn.substr(sl + 1); if (fn.size() > 4 && (fn.compare(fn.size() - 4, 4, ".bmp") == 0 || fn.compare(fn.size() - 4, 4, ".BMP") == 0)) fn.resize(fn.size() - 4); }
            if (fn.empty()) { auto fk = p.kv.find("font_kind"); e.g.fgroup = (fk != p.kv.end() && fk->second == "jp") ? "jis" : (e.g.lang == "jp" ? "jis" : "ks2350"); }
            else { e.g.fgroup = "custom"; e.g.gfont = fn; for (auto & f : gApp.ui_.fonts) if (f.ok && f.name == fn && p.font.find_first_of("\\/:.") == std::string::npos) e.g.fgroup = f.group; }
        }
        { auto kvs = [&](const char * k) -> std::string { auto it = p.kv.find(k); return it == p.kv.end() ? std::string() : it->second; };
          e.g.company = kvs("company"); e.g.year = kvs("year"); e.g.genre = kvs("genre"); e.g.patch = kvs("patch"); e.g.devices = kvs("devices");
          if (e.g.devices.empty()) { std::string dv; auto add = [&](const char * s) { if (!dv.empty()) dv += " · "; dv += s; }; std::string mo = kvs("mouse"), so = kvs("sound"), jo = kvs("joystick");
              if (so.find("86") != std::string::npos) add(phr_("86 보드")); if (so.find("rhythm") != std::string::npos) add(phr_("리듬 WAV"));
              if (mo == "required") { std::map<std::string, std::string> kk = p.kv; kk["__id"] = d; add(aw::mouse_released(kk) ? phr_("마우스 지원(시험)") : "마우스 필수"); } else if (mo == "pointer") add(phr_("마우스 지원")); else if (mo == "optional") add(phr_("마우스 선택"));
              if (jo.find("opn") != std::string::npos) add(phr_("OPN 조이스틱")); e.g.devices = dv; }
          std::string demo = kvs("demo"); e.g.has_demo = !demo.empty();
          {   /* r017: 판정 등급 · 출처 · 실행 불가 사유(목록을 만들 때마다 계산 — 지원이 늘면 다시 등록하지 않아도 풀린다) */
              bool autoreg = kvs("auto") == "1", curated = p.src == "DB" || kvs("curated") == "1"; e.g.autoreg = autoreg;
              std::string gr = kvs("grade"); e.g.grade = gr.empty() ? '-' : gr[0]; e.g.judge = kvs("judge"); e.g.jshort = kvs("jshort"); e.g.metasrc = kvs("metasrc");
              e.g.orig = kvs("orig"); e.g.dbhit = kvs("dbhit");
              {   /* r017 이전 문구(내부 용어)가 들어 있는 ini 는 쉬운 말로 바꿔 보여 준다 */
                  auto rep = [](std::string & s, const char * a, const char * b) { size_t k; while ((k = s.find(a)) != std::string::npos) s.replace(k, strlen(a), b); };
                  rep(e.g.jshort, "A 큐레이션 DB 일치", "확인됨 (DB 일치)"); rep(e.g.judge, "A 큐레이션 DB(해시 일치): ", "원판과 같은 파일 — "); rep(e.g.judge, "큐레이션 DB", "DB");
                  rep(e.g.jshort, "B 변형 · ", "추정 · "); rep(e.g.jshort, "C 후보 · ", "후보 · "); }
              if (gr.empty() && p.src == "DB") { e.g.grade = 'A'; e.g.dbhit = p.title; e.g.judge = phr_("원판과 같은 파일 — ") + p.title + phr_(" (DB 일치)"); e.g.jshort = phr_("확인됨 (DB 일치)"); e.g.metasrc = e.g.metasrc.empty() ? std::string("DB") : e.g.metasrc; }
              else if (gr.empty() && !autoreg) {   /* game.ini 로 직접 정한 작품: DB 일치 여부도 같이 보인다 */
                  if (!e.g.dbhit.empty()) { e.g.grade = 'A'; e.g.jshort = phr_("직접 지정 · DB 일치"); e.g.judge = phr_("game.ini 로 직접 지정 — 파일은 DB의 '") + e.g.dbhit + phr_("' 와 일치"); }
                  else { e.g.jshort = phr_("직접 지정"); e.g.judge = phr_("game.ini 로 직접 지정 (DB와 일치하는 파일 없음)"); } }
              prof::Profile q; q.install_required = p.install_required; q.reason = p.reason; q.disk.assign(4, std::string()); for (int i = 0; i < 4; i++) if (p.disk[i]) q.disk[i] = p.disk[i];
              std::map<std::string, std::string> kv = p.kv; if (curated) kv["curated"] = "1"; kv["__id"] = d; if (!autoreg) kv.erase("mouse");   /* 직접 정한 작품은 마우스 때문에 막지 않는다 */
              e.g.block = aw::block_reason(q, kv); if (e.g.block.empty() && !kvs("block").empty() && kvs("block").find("마우스 필수") == std::string::npos) e.g.block = kvs("block");   /* t48a (#615): 마우스 필수 문구는 막는 사유가 아님 */
              gh_log("LISTBLK id=%s src=%s auto=%d curated=%d mouse=%s entry_req='%s' block='%s' boot=%s/%s", d.c_str(), p.src.c_str(), autoreg ? 1 : 0, curated ? 1 : 0, kvs("mouse").c_str(), prof::g_entry_req.c_str(), e.g.block.c_str(), kvs("__val.boot").c_str(), kvs("__src.boot").c_str()); }
          auto flb = [&](const std::string & f) -> std::string {
              if (p.disk[0] && f == p.disk[0] && !kvs("label_a").empty()) return kvs("label_a"); if (p.disk[1] && f == p.disk[1] && !kvs("label_b").empty()) return kvs("label_b");
              if (!demo.empty() && f == demo) return kvs("label_demo").empty() ? std::string(phr_("오프닝(데모) 디스크")) : kvs("label_demo");
              std::string n = f; size_t dd = n.find_last_of('.'); if (dd != std::string::npos && n.size() - dd <= 5) n = n.substr(0, dd); return n; };
          std::vector<std::string> files; auto addf = [&](const char * f) { if (!f || !*f) return; for (auto & x : files) if (x == f) return; files.push_back(f); };
          for (int i = 0; i < 2; i++) addf(p.disk[i]); for (auto & s : p.swap) addf(s.c_str()); addf(demo.c_str());
          for (auto & f : files) { e.g.fds.push_back(flb(f)); }
          gApp.fdfiles[d] = files;
          for (int i = 0; i < 2; i++) if (p.disk[i]) e.g.cur_fd[i] = flb(p.disk[i]);
          int nfd = 0; for (auto & f : files) if (f != demo) nfd++; bool hd = p.disk[2] || p.disk[3];
          e.g.disknote = hd ? (nfd ? "HDD + FD " + std::to_string(nfd) + phr_("장") : std::string("HDD")) : (nfd ? "FD " + std::to_string(nfd) + phr_("장") : std::string("-"));
          if (!demo.empty()) e.g.disknote += phr_(" + 오프닝");
          { std::string mo = kvs("more"); if (!mo.empty()) { e.g.more = mo; size_t k; while ((k = mo.find(" | ")) != std::string::npos) mo.replace(k, 3, " / "); e.g.disknote += phr_("  함께 있는 이미지: ") + mo; } } }
        std::vector<std::string> ls; for (int i = 0; i < 4; i++) { std::string l = lbl(p, i); if (!l.empty()) ls.push_back(l); }
        if (ls.size() > 0) e.g.disk[0] = ls[0]; if (ls.size() > 1) e.g.disk[1] = ls[1];
        e.g.boot_note = p.install_required ? std::string(phr_("하드 설치 전용 (설치 단계 필요)")) : std::string(phr_("부팅  ")) + p.bootletter + phr_(" 드라이브");
        if (p.install_required && ls.empty()) e.g.disk[0] = phr_("부팅 가능한 디스크 없음");
        es.push_back(e);
    }
    std::stable_sort(es.begin(), es.end(), [](const E & a, const E & b) { return a.order < b.order; });
    for (auto & e : es) gApp.ui_.games.push_back(e.g);
    ulist::load_all(gApp.ui_.games, &gApp.vlabel);
    if (gApp.ui_.games.empty()) {
        ui::UiGame g; g.id = ""; g.title = phr_("작품이 없습니다"); g.disk[0] = phr_("games 폴더에 작품 폴더를 넣어 주세요"); g.boot_note = ""; gApp.ui_.games.push_back(g);
        if (!gApp.uitest_dir.empty()) {   /* 캡처 시험용 표본 */
            gApp.ui_.games.clear(); const char * T[4][2] = { { "hukyou", phr_("환세풍광전") }, { "kaitou", phr_("환세쾌도전") }, { "kitan", phr_("환세희담") }, { "torimono", phr_("환세포물장") } };
            for (int i = 0; i < 4; i++) { ui::UiGame x; x.id = T[i][0]; x.title = T[i][1]; x.disk[0] = std::string("A: ") + T[i][1] + phr_(" 디스크"); x.boot_note = phr_("부팅  A 드라이브"); gApp.ui_.games.push_back(x); }
        }
    }
    gApp.ui_.build = std::string("v" KH_VERSION);   /* t52a (#53): 내부 빌드명은 로그에만 */
    gh_log("games: %d", (int)gApp.ui_.games.size());
}
/* ------------------------------------------------------------------ r017: 작품 추가 · 게임 폴더 자동 등록 */
#include "platform/plat_dialog.h"   /* S9e-N 3: OS headers and dialogs live in platform */
static void gh_nk_load_once(void) {
    static bool done = false; if (done) return; done = true;
    std::string p = gApp.nkdb_path.empty() ? gApp.exedir + "\\gamedb_nk.tsv" : gApp.nkdb_path;
    if (nk::load(p, gApp.nk_)) { nk::load_sig(gApp.exedir + "\\gamedb_nk_sig.tsv", gApp.nk_); gh_log("gamedb_nk: %d games, %d sha1 (nk=%s tosec=%s)", (int)gApp.nk_.g.size(), (int)gApp.nk_.bysha.size(), gApp.nk_.ver_nk.c_str(), gApp.nk_.ver_tosec.c_str()); }
    else gh_log("gamedb_nk: none (%s)", p.c_str());
    gApp.shacache.load(gApp.exedir + "\\sha_cache.tsv");
}
static plat::native_win gh_hwnd(void) { return gApp.win ? plat::window_native(gApp.win) : NULL; }

/* ---- t52a (#54/#55): 제목표시줄이 윈도우 앱 테마를 따른다 ---------------------------------------------
 * HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme == 1 이면 밝게, 그 밖(값 없음·0·읽기 실패)은 어둡게.
 * DwmSetWindowAttribute 20(DWMWA_USE_IMMERSIVE_DARK_MODE) 을 먼저, 실패하면 19(구 Win10 1809~1903 값). 둘 다 실패하거나 DWM 이 없으면(Win7/8, Win10 1809 미만)
 * 아무것도 하지 않아 기본 제목표시줄이 그대로 남는다. 실행 중 앱 테마가 바뀌면(WM_SETTINGCHANGE "ImmersiveColorSet") 다시 적용.
 * dwmapi.dll 은 LoadLibrary 로 부르므로 링크 목록은 늘지 않는다. */
static bool gh_apps_light(void) {
    return plat::apps_use_light_theme();
}
static void gh_titlebar_apply(const char * why, bool force) {
    try {
        plat::native_win hw = gh_hwnd(); if (!hw) return;
        if (gApp.tb_last == -3 && !force) return;
        int want = gh_apps_light() ? 0 : 1;
        if (!force && want == gApp.tb_last) return;
        unsigned at = 0; long r = 0; int dr = plat::dwm_dark_titlebar(hw, want != 0, &at, &r);
        if (dr == 1) { if (gApp.tb_last != -3) gh_log("TITLEBAR %s: DwmSetWindowAttribute 없음 -> 기본 제목표시줄 유지", why); gApp.tb_last = -3; return; }
        if (dr == 2) { if (gApp.tb_last != -3) gh_log("TITLEBAR %s: 실패 hr=0x%08lX -> 기본 제목표시줄 유지 (apps_light=%d)", why, (unsigned long)r, 1 - want); gApp.tb_last = -3; return; }
        bool changed = (want != gApp.tb_last); gApp.tb_last = want;
        if (changed) {
            gh_log("TITLEBAR %s: apps_light=%d -> %s 제목표시줄 (attr=%u)", why, 1 - want, want ? "어두운" : "밝은", (unsigned)at);
            if (gApp.win && !(SDL_GetWindowFlags(gApp.win) & SDL_WINDOW_FULLSCREEN_DESKTOP)) plat::redraw_frame(hw);   /* 바로 다시 그리게 */
        }
    } catch (...) { }
}
static void gh_titlebar_event(const SDL_Event * e) {
    try {
        if (plat::is_theme_change_event(e)) {
            gh_titlebar_apply("settingchange", true);
        } else if (e->type == SDL_WINDOWEVENT && gApp.win && (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e->window.event == SDL_WINDOWEVENT_RESTORED || e->window.event == SDL_WINDOWEVENT_SHOWN)) {
            unsigned now = SDL_GetTicks(); if (now - gApp.tb_tick > 400u) { gApp.tb_tick = now; gh_titlebar_apply("window", true); }   /* 전체화면 <-> 창 전환 뒤에도 유지 */
        }
    } catch (...) { }
}

static bool gh_pick_files(std::vector<std::string> & out) {   /* 시스템(ANSI) 경로로 돌려준다 */
    std::string t = aw::u8_to_acp(phr_("추가할 디스크 이미지를 고르세요 (여러 개 가능)"));
    std::string f1 = aw::u8_to_acp(phr_("디스크 이미지")), f2 = aw::u8_to_acp(phr_("모든 파일"));
    std::string flt = f1 + std::string(1, '\0') + "*.fdi;*.hdi;*.nhd;*.d88;*.d77;*.88d;*.98d;*.nfd;*.hdm;*.dup;*.xdf;*.dsk;*.fim;*.flp;*.img;*.ima;*.thd;*.hdd;*.hdn;*.dim;*.tfd;*.fdd;*.bin;*.zip" + std::string(1, '\0') + f2 + std::string(1, '\0') + "*.*" + std::string(2, '\0');
    std::vector<std::string> parts; if (!plat::pick_files_a(gh_hwnd(), t, flt, parts)) return false;
    if (parts.size() == 1) out.push_back(parts[0]); else { for (size_t i = 1; i < parts.size(); i++) out.push_back(aw::join(parts[0], parts[i])); }
    return !out.empty();
}
#if defined(__ANDROID__)
static bool gh_folder_explorer(const char * title_u8, std::string & out);   /* app/folder_pick.h */
#endif
static bool gh_pick_folder(const char * title_u8, std::string & out) {
#if defined(__ANDROID__)
    return gh_folder_explorer(title_u8, out);
#else
    std::string t = aw::u8_to_acp(title_u8);
    return plat::pick_folder_a(gh_hwnd(), t, out);
#endif
}
static aw::Ctx gh_aw_ctx(void) {
    gh_db_load_once(); gh_nk_load_once(); aw::Ctx c; c.games_dir = gApp.exedir + "\\games"; c.on_dir = gh_res_on_work; c.db = &gApp.db; c.nk = gApp.nk_.loaded ? &gApp.nk_ : NULL; c.cache = &gApp.shacache; { std::vector<std::string> ml; int mv = aw::merge_dups(c, &ml); for (auto & l : ml) gh_log("%s", l.c_str()); if (mv) gh_log("중복 합침 %d건", mv); } aw::index_registered(c); return c;
}
static void gh_reload_games(void) {   /* 목록을 다시 만들고 보던 작품을 id 로 되찾는다 */
    std::string keep = (gApp.cur_game >= 0 && gApp.cur_game < (int)gApp.ui_.games.size()) ? gApp.ui_.games[gApp.cur_game].id : std::string();
    ui::settings_save(gApp.ui_, gApp.cfgpath);
    gApp.fdfiles.clear();
    gh_ui_init_games(); ui::settings_load(gApp.ui_, gApp.cfgpath);   { FILE * zf_ = fopen(gApp.cfgpath.c_str(), "rb"); int ex_ = zf_ != NULL; if (zf_) fclose(zf_); int cg_ = gApp.cur_game; bool ht_ = cg_ >= 0 && cg_ < (int)ui::Ui::MAXG && gApp.ui_.has_t[cg_]; gh_log("SETDIAG %s path=%s file=%d g[scan=%d mask=%d bleed=%d glow=%d curve=%d ratio=%d] cur_game=%d has_t=%d games=%d", "after-list-reload", gApp.cfgpath.c_str(), ex_, gApp.ui_.g.scan, gApp.ui_.g.mask, gApp.ui_.g.bleed, gApp.ui_.g.glow, gApp.ui_.g.curve, gApp.ui_.g.ratio, cg_, (int)ht_, (int)gApp.ui_.games.size()); }
    for (size_t i = 0; i < gApp.ui_.games.size(); i++) if (gApp.ui_.games[i].id == keep) { gApp.cur_game = (int)i; gApp.ui_.cur = (int)i; gApp.ui_.pick = (int)i; }
    if (!gApp.headless) for (size_t i = 0; i < gApp.ui_.games.size(); i++) gh_refresh_game_slots((int)i);
}
static aw::Summary gh_add_works(const std::vector<aw::Work> & ws, const char * what) {
    aw::Ctx c = gh_aw_ctx(); aw::Summary S; aw::run_works(c, ws, S); gApp.shacache.save();
    char h[200]; snprintf(h, sizeof h, "%s: 추가%d 있음%d 건너뜀%d 불가%d", what, S.added, S.exists, S.skipped, S.blocked);
    gApp.ui_.add_log.clear(); gApp.ui_.add_log.push_back(what); gApp.ui_.add_log.push_back(std::string(phr_("추가")) + std::to_string(S.added) + phr_(" 있음") + std::to_string(S.exists) + phr_(" 건너뜀") + std::to_string(S.skipped) + phr_(" 불가") + std::to_string(S.blocked)); for (auto & l : S.lines) gApp.ui_.add_log.push_back(l);
    gh_log("ADDWORK %s", h); for (auto & l : S.lines) gh_log("ADDWORK   %s", l.c_str());
    gh_log("ADDWORK sha1 cache hits=%u miss=%u", gApp.shacache.hits, gApp.shacache.miss);
    return S;
}
/* 게임 폴더: 켤 때마다 훑어 새 작품만 자동 등록 */
static int gh_scan_gamefolder(const char * why) {
    kha_lib_init();   /* t137 */
    if (gApp.ui_.gamefolder.empty()) return 0;
    std::string dir = aw::u8_to_acp(gApp.ui_.gamefolder); if (!aw::dir_exists(dir)) { gh_log("GAMEFOLDER %s: 폴더 없음 (%s)", why, dir.c_str()); if (!gApp.ui_.add_log.empty()) gApp.ui_.add_log.clear(); gApp.ui_.add_log.push_back(phr_("게임 폴더를 찾을 수 없습니다")); return 0; }
    std::vector<aw::Work> ws; aw::collect(dir, 3, ws); aw::Summary S = gh_add_works(ws, why);
    gh_log("GAMEFOLDER %s: %d 후보, 추가 %d", why, (int)ws.size(), S.added);
    S.added += kha_scan_extra(why);   /* t137: 등록 폴더 */
    if (S.added > 0) gh_reload_games(); return S.added;
}
#if defined(__ANDROID__)
static const char * fpt(int i);
static void gh_android_savedir(void);
#endif
static void gh_add_action(int arg) {
    ui::Ui & U = gApp.ui_;
    if (arg == 0) { std::vector<std::string> fs; if (!gh_pick_files(fs)) return; std::vector<aw::Work> ws; aw::from_files(fs, ws); aw::Summary S = gh_add_works(ws, phr_("파일 추가")); if (S.added) { gh_reload_games(); U.add_new = S.added_ids; U.add_new_why = S.added_why; U.open_lang(U.add_new, U.add_new_why, ui::S_ADD); } }
    else if (arg == 1) { std::string d; if (!gh_pick_folder(phr_("추가할 작품 폴더를 고르세요"), d)) return; kha_lib_add(d); std::vector<aw::Work> ws; aw::collect(d, 3, ws); aw::Summary S = gh_add_works(ws, phr_("폴더 추가")); if (S.added) { gh_reload_games(); U.add_new = S.added_ids; U.add_new_why = S.added_why; U.open_lang(U.add_new, U.add_new_why, ui::S_ADD); } }
    else if (arg == 2) { std::string d; if (!gh_pick_folder(phr_("「게임 폴더」를 고르세요 — 켤 때마다 새 작품을 찾아 등록합니다"), d)) return; U.gamefolder = aw::acp_to_u8(d); ui::settings_save(U, gApp.cfgpath); gh_scan_gamefolder(phr_("게임 폴더 지정"));
#if defined(__ANDROID__)
    gh_android_savedir();
    if (U.games.size() == 1 && U.games[0].id.empty()) { U.add_log = { fpt(14), fpt(15) }; U.scr = ui::S_ADD; }   /* t55e: empty folder notice */
#endif
    }
    else if (arg == 3) { gh_scan_gamefolder(phr_("다시 검색")); if (!U.add_log.empty() && U.gamefolder.empty()) U.add_log.clear(); }
    else if (arg == 4) { kha_lib_clear(); U.gamefolder.clear(); ui::settings_save(U, gApp.cfgpath); U.add_log.clear(); U.add_log.push_back(phr_("게임 폴더를 해제했습니다 (등록된 작품은 그대로입니다)")); }
}
/* t44a (#494): 사용자 「언어」 지정 — user.ini [game] lang=ko|jp (자동 = 키 삭제). 저장 후 목록을 다시 만들고 보던 작품·항목 화면 대상을 id 로 되찾는다 */
static void gh_setlang(int gi, int mode) {
    ui::Ui & U = gApp.ui_; if (gi < 0 || gi >= (int)U.games.size()) return;
    std::string id = U.games[gi].id, eid = (U.entg >= 0 && U.entg < (int)U.games.size()) ? U.games[U.entg].id : std::string();
    const char * v = mode == 1 ? "ko" : mode == 2 ? "jp" : "";
    bool ok = prof::set_game_key(U.games[gi].dir, "lang", v);
    gh_log("SETLANG %s lang=%s ok=%d", id.c_str(), *v ? v : "(auto)", ok ? 1 : 0);
    gh_reload_games();
    if (!eid.empty()) for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == eid) U.entg = (int)i;
    if (!ok) U.msg = phr_("언어를 저장하지 못했습니다.");
}
/* t49a (#809): 표시 제목 저장 — user.ini [game] title_user= (빈 값 = 키 삭제 = DB 제목 복귀). 원본 DB·game.ini 무변경 */
static void gh_settitle(int gi, const std::string & t) {
    ui::Ui & U = gApp.ui_; if (gi < 0 || gi >= (int)U.games.size()) return;
    std::string id = U.games[gi].id, eid = (U.entg >= 0 && U.entg < (int)U.games.size()) ? U.games[U.entg].id : std::string();
    bool ok = prof::set_game_key(U.games[gi].dir, "title_user", t);
    gh_log("SETTITLE %s title_user=%s ok=%d", id.c_str(), t.empty() ? "(없음)" : t.c_str(), ok ? 1 : 0);
    gh_reload_games();
    if (!eid.empty()) for (size_t i = 0; i < U.games.size(); i++) if (U.games[i].id == eid) U.entg = (int)i;
    if (!ok) U.msg = phr_("표시 제목을 저장하지 못했습니다.");
    else if (!t.empty()) U.msg = phr_("표시 제목을 바꿨습니다. 원본 DB는 그대로입니다.");
}
/* --addwork PATH [--dry] [--report DIR] : 파일/폴더를 등록(또는 시험 판정)하고 판정표(match_table.tsv)를 남긴다 — 헤드리스 시험용 */
static void gh_cli_addwork(void) {
    std::vector<aw::Work> ws; std::string p = gApp.addwork_cli;
    if (aw::dir_exists(p)) aw::collect(p, 3, ws); else { std::vector<std::string> v; v.push_back(p); aw::from_files(v, ws); }
    aw::Ctx c = gh_aw_ctx(); aw::Summary S; std::string tsv = phr_("작업\t파일(크기)\t등급\t판정(짧게)\t판정(전체)\tid\t제목\t제작사\t연도\t출처\t실행불가\t비고\n");
    for (auto & w : ws) {
        aw::Result r = aw::register_work(c, w, gApp.aw_dry);
        if (r.st == aw::Result::ADDED) { S.added++; if (!r.block.empty()) S.blocked++; } else if (r.st == aw::Result::EXISTS) S.exists++; else S.skipped++;
        std::string fl; for (auto & f : w.files) { uint64_t sz = 0, mt = 0; aw::file_stat(aw::join(w.dir, f), sz, mt); if (!fl.empty()) fl += " | "; fl += aw::acp_to_u8(f) + " (" + std::to_string((unsigned long long)sz) + ")"; }
        auto cl = [](std::string s) { for (auto & ch : s) if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' '; return s; };
        tsv += cl(aw::pretty(w.name)) + "\t" + cl(fl) + "\t" + (r.grade.empty() ? std::string("-") : r.grade) + "\t" + cl(r.jshort) + "\t" + cl(r.judge) + "\t" + r.id + "\t" + cl(r.title) + "\t" + cl(r.maker) + "\t" + r.year + "\t" + cl(r.metasrc) + "\t" + cl(r.block) + "\t" + cl(r.note) + "\n";
        gh_log("ADDWORK%s %s | %s | %s | %s", gApp.aw_dry ? "(dry)" : "", aw::pretty(w.name).c_str(), r.grade.c_str(), r.judge.c_str(), r.note.c_str());
    }
    gh_log("ADDWORK summary: works=%d added=%d exists=%d skipped=%d blocked=%d dry=%d", (int)ws.size(), S.added, S.exists, S.skipped, S.blocked, (int)gApp.aw_dry);
    if (!gApp.report_dir.empty()) { plat::mkdir(gApp.report_dir.c_str()); FILE * f = fopen((gApp.report_dir + "\\match_table.tsv").c_str(), "wb"); if (f) { fwrite(tsv.data(), 1, tsv.size(), f); fclose(f); } }
    gApp.shacache.save(); if (!gApp.aw_dry && S.added) gh_reload_games();
}
/* --batest : 「배속 중 소리」 DSP 단독 시험(소리 장치 없음). 440 Hz 사인을 x 배 속도로 먹이고 출력의 음높이·크기·끊김을 잰다.
 * 기대: 빠르게(1) → 440*x Hz · 음높이 유지(2) → 440 Hz */
static void gh_batest(void) {
    const double SR = 44100.0; const int NEED = 441, CALLS = 700; int xs[5] = { 2, 3, 4, 6, 8 }; int bad = 0;
    for (int xi = 0; xi < 5; xi++) for (int mode = 1; mode <= 2; mode++) {
        int x = xs[xi]; ba::Eng * e = new ba::Eng; static int32_t l[8192], r[8192]; std::vector<float> out; out.reserve((size_t)NEED * CALLS); long long n = 0; int under = 0;
        for (int c = 0; c < CALLS; c++) {
            int fed = NEED * x; for (int i = 0; i < fed; i++, n++) { int32_t s = (int32_t)(8000.0 * sin(2.0 * 3.14159265358979323846 * 440.0 * (double)n / SR)); e->feed1(s, s); }
            e->note(fed, NEED, (double)x); int made = e->render(mode, (double)x, NEED, l, r); if (made < NEED) under++;
            for (int i = 0; i < made; i++) out.push_back((float)l[i]);
            for (int i = made; i < NEED; i++) out.push_back(out.empty() ? 0.0f : out.back());
        }
        size_t a = (size_t)SR; size_t cnt = 0; double ss = 0; for (size_t i = a + 1; i < out.size(); i++) { if (out[i - 1] <= 0 && out[i] > 0) cnt++; ss += (double)out[i] * out[i]; }
        double secs = (double)(out.size() - a) / SR, hz = cnt / secs, rms = sqrt(ss / (double)(out.size() - a)); double want = mode == 1 ? 440.0 * x : 440.0;
        bool ok = fabs(hz - want) / want < 0.04 && under <= CALLS / 50 && rms > 1500;
        if (!ok) bad++;
        gh_log("BATEST x=%d mode=%d(%s) freq=%.1fHz expect=%.0fHz rms=%.0f under=%d/%d drops=%u %s", x, mode, mode == 1 ? "빠르게" : "음높이 유지", hz, want, rms, under, CALLS, e->drops, ok ? "OK" : "FAIL");
        delete e;
    }
    gh_log("BATEST done: %s", bad ? "FAIL" : "ALL OK");
}
/* ---- 입력 → UI 키 */
/* t49a (#812): 메뉴 키 매핑 표 — gh_ev2uk 가 읽고 조작 설명(helpline) 번역도 같은 표에서 이름을 만든다(키 이름 한 곳). 설정 노출 없음 */
struct GhKeyRow { ui::UKey uk; const char * sym; const char * name; int n; SDL_Keycode k[3]; };
static const GhKeyRow GH_KEYTAB[] = {
    { ui::UK_UP, "↑", "↑", 1, { SDLK_UP } }, { ui::UK_DOWN, "↓", "↓", 1, { SDLK_DOWN } }, { ui::UK_LEFT, "←", "←", 1, { SDLK_LEFT } }, { ui::UK_RIGHT, "→", "→", 1, { SDLK_RIGHT } },
    { ui::UK_OK, "○", "Enter", 3, { SDLK_RETURN, SDLK_KP_ENTER, SDLK_SPACE } },
    { ui::UK_CANCEL, "×", "Esc", 3, { SDLK_ESCAPE, SDLK_BACKSPACE, SDLK_F12 } },
    { ui::UK_SQ, "□", "S", 1, { SDLK_s } }, { ui::UK_TRI, "△", "D", 1, { SDLK_d } }, { ui::UK_SELECT, "SEL", "Tab", 1, { SDLK_TAB } },
    { ui::UK_L, "L", "Q", 1, { SDLK_q } }, { ui::UK_R, "R", "E", 1, { SDLK_e } },
};
static ui::UKey gh_keytab_lookup(SDL_Keycode sym) { for (auto & r : GH_KEYTAB) for (int i = 0; i < r.n; i++) if (r.k[i] == sym) return r.uk; return ui::UK_NONE; }
static void gh_help_init(void) {   /* 같은 표에서 helpline 번역표를 만든다(첫 키 이름만; 방향키는 그대로) */
    ov::g_helpkeys.clear(); std::string ln, rn;
    for (auto & r : GH_KEYTAB) { if (r.uk == ui::UK_UP || r.uk == ui::UK_DOWN || r.uk == ui::UK_LEFT || r.uk == ui::UK_RIGHT) continue; if (r.uk == ui::UK_L) { ln = r.name; continue; } if (r.uk == ui::UK_R) { rn = r.name; continue; } ov::HelpKey hk; hk.sym = r.sym; hk.name = r.name; ov::g_helpkeys.push_back(hk); if (r.uk == ui::UK_SELECT) { ov::HelpKey h2; h2.sym = "SELECT"; h2.name = r.name; ov::g_helpkeys.push_back(h2); } }
    ov::HelpKey a; a.sym = "L R"; a.name = ln + " " + rn; ov::g_helpkeys.push_back(a); ov::HelpKey b; b.sym = "L/R"; b.name = ln + "/" + rn; ov::g_helpkeys.push_back(b);
}
#if defined(__ANDROID__)
/* t55a: 메뉴 터치 최소판. SDL 손가락 이벤트를 키 이벤트로 바꿔 밀어 넣어 기존 메뉴 루프(게임 중·시작 메뉴 둘 다)가 그대로 처리하게 한다.
 * 탭 = 그 줄로 이동 + 결정(Enter) · 4:3 틀 바깥 탭 = 취소(Esc) · 세로 끌기 = 위/아래 한 칸씩. 좌표는 gh_publish 의 소프트웨어 경로(논리 640x480 레터박스; 16:10 이면 400줄 가운데, 4:3 이면 세로 늘림)와 같은 식. */
static void gh_touch_key(SDL_Keycode k) { SDL_Event ke; SDL_zero(ke); ke.type = SDL_KEYDOWN; ke.key.keysym.sym = k; ke.key.state = SDL_PRESSED; SDL_PushEvent(&ke); ke.type = SDL_KEYUP; ke.key.state = SDL_RELEASED; SDL_PushEvent(&ke); }
static int gh_touch_fl(double v) { int i = (int)v; return (v < (double)i) ? i - 1 : i; }
static bool gh_touch_pos(float fx, float fy, int & vx, int & vy) {   /* finger (window 0..1) to menu canvas (640x400); outside the frame gives out-of-range values */
    if (gApp.use_gl && gApp.win) {   /* t57a: GL output -- same rectangle the render thread draws into (gl_rect) */
        int dw = 0, dh = 0, x = 0, y = 0, rw = 0, rh = 0; SDL_GL_GetDrawableSize(gApp.win, &dw, &dh); if (dw <= 0 || dh <= 0) return false;
        gl_rect(dw, dh, gApp.pub_w > 0 ? gApp.pub_w : 640, gApp.pub_h > 0 ? gApp.pub_h : 400, gApp.fit, x, y, rw, rh); if (rw <= 0 || rh <= 0) return false;
        vx = gh_touch_fl(((double)fx * dw - x) * 640.0 / rw); vy = gh_touch_fl(((double)fy * dh - y) * 400.0 / rh); return true;
    }
    int ow = 0, oh = 0; if (!gApp.ren || SDL_GetRendererOutputSize(gApp.ren, &ow, &oh) != 0 || ow <= 0 || oh <= 0) return false;
    double sx = ow / 640.0, sy = oh / 480.0, sc = sx < sy ? sx : sy, ox = (ow - 640.0 * sc) / 2.0, oy = (oh - 480.0 * sc) / 2.0;
    double lx = ((double)fx * ow - ox) / sc, ly = ((double)fy * oh - oy) / sc;
    vx = gh_touch_fl(lx); vy = (gApp.fit == FIT_1610) ? gh_touch_fl(ly - 40.0) : gh_touch_fl(ly * 400.0 / 480.0); return true;
}
static void gh_touch_feed(const SDL_Event & e) {
    static bool dn = false, mv = false; static SDL_FingerID fid = 0; static float x0 = 0, y0 = 0; static int ay = 0;
    const int STEP = 22, SLOP = 14;
    if (e.type == SDL_FINGERDOWN) { if (dn) return; int vx, vy; if (!gh_touch_pos(e.tfinger.x, e.tfinger.y, vx, vy)) return; dn = true; mv = false; fid = e.tfinger.fingerId; x0 = e.tfinger.x; y0 = e.tfinger.y; ay = vy; return; }
    if (!dn || e.tfinger.fingerId != fid) return;
    int vx, vy; if (!gh_touch_pos(e.tfinger.x, e.tfinger.y, vx, vy)) { if (e.type == SDL_FINGERUP) dn = false; return; }
    if (e.type == SDL_FINGERMOTION) {
        if (!mv) { int sx, sy; gh_touch_pos(x0, y0, sx, sy); if (abs(vy - sy) < SLOP && abs(vx - sx) < SLOP) return; mv = true; ay = sy; }
        while (vy - ay >= STEP) { gh_touch_key(SDLK_UP); ay += STEP; }
        while (ay - vy >= STEP) { gh_touch_key(SDLK_DOWN); ay -= STEP; }
        return; }
    if (e.type == SDL_FINGERUP) {
        dn = false; if (mv) { gh_log("TOUCH swipe end v=(%d,%d)", vx, vy); return; }
        if (vx < 0 || vx >= 640 || vy < 0 || vy >= 400) { gh_log("TOUCH tap v=(%d,%d) outside -> cancel", vx, vy); gh_touch_key(SDLK_ESCAPE); return; }
        ui::Ui & U = gApp.ui_;
        for (int h = (int)U.hits.size() - 1; h >= 0; h--) {   /* rows drawn later (on top) first */
            const ui::Ui::Hit & t = U.hits[h];
            if (vx >= t.x && vx < t.x + t.w && vy >= t.y && vy < t.y + t.h) {
                int d = t.i - t.sel; gh_log("TOUCH tap v=(%d,%d) row %d (sel %d, move %d) -> ok", vx, vy, t.i, t.sel, d);
                for (; d > 0; d--) gh_touch_key(SDLK_DOWN); for (; d < 0; d++) gh_touch_key(SDLK_UP);
                gh_touch_key(SDLK_RETURN); return; } }
        gh_log("TOUCH tap v=(%d,%d) no row (ignored)", vx, vy);
    }
}
#endif
static ui::UKey gh_ev2uk(const SDL_Event & e, bool * anykey) {
    using namespace ui;
    if (anykey) *anykey = (e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_MOUSEBUTTONDOWN;
    { int nd = -1; if (e.type == SDL_KEYDOWN && !e.key.repeat) nd = 1; else if (e.type == SDL_CONTROLLERBUTTONDOWN) nd = 0; else if (e.type == SDL_CONTROLLERAXISMOTION && (e.caxis.value > 16000 || e.caxis.value < -16000)) nd = 0;   /* t49a (#812): 마지막 입력 장치(마우스 제외) */
      if (nd >= 0 && ov::g_helpdev != nd) { ov::g_helpdev = nd; if (gApp.helplog) gh_log("HELPDEV 입력 장치 -> %s (clock=game)", nd ? "키보드" : "패드"); } }
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.mod & KMOD_ALT) return UK_NONE;
        if (gApp.ui_.scr == ui::S_OSK && e.key.keysym.sym >= 0x20 && e.key.keysym.sym < 0x7f && (e.key.keysym.sym != SDLK_SPACE || gApp.ui_.osk_hg)) return UK_NONE;   /* t39: 글자 입력 화면에서는 글자 키가 패드 키로 겹치지 않게 (t48b: 한글 키보드에서는 공백도 글자 이벤트로만) */
        if (gApp.ui_.scr == ui::S_OSK && e.key.keysym.sym == SDLK_BACKSPACE) return UK_SQ;   /* t48b: 글자 입력 화면의 백스페이스 = 지움(예전에는 취소로 나가 버렸다) */
        if (gApp.ui_.scr == ui::S_OSK && gApp.ui_.osk_hg && (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER)) return UK_SELECT;   /* t48b: 한글 키보드에서 엔터 = 완료 */
        return gh_keytab_lookup(e.key.keysym.sym);   /* t49a: 표 한 곳(GH_KEYTAB) */
    }
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_START: return UK_OK;
        case SDL_CONTROLLER_BUTTON_B: return kh_western ? UK_CANCEL : UK_OK;   /* t48e (#651 4): 일본식 = 동쪽(B/○) 결정 · 서양식 = 남쪽(A/×) 결정 */
        case SDL_CONTROLLER_BUTTON_A: return kh_western ? UK_OK : UK_CANCEL;
        case SDL_CONTROLLER_BUTTON_X: return UK_SQ; case SDL_CONTROLLER_BUTTON_Y: return UK_TRI; case SDL_CONTROLLER_BUTTON_BACK: return UK_SELECT;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return UK_L; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return UK_R;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return UK_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return UK_DOWN; case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return UK_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return UK_RIGHT;
        default: return UK_NONE; }
    }
    if (e.type == SDL_CONTROLLERAXISMOTION && (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)) {
        static bool st[4] = { false, false, false, false };   /* L R U D */
        int ax = e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ? 0 : 2; int v = e.caxis.value;
        bool & neg = st[ax], & pos = st[ax + 1];
        if (!neg && v < -16000) { neg = true; return ax == 0 ? UK_LEFT : UK_UP; } if (neg && v > -8000) neg = false;
        if (!pos && v > 16000) { pos = true; return ax == 0 ? UK_RIGHT : UK_DOWN; } if (pos && v < 8000) pos = false;
    }
    return UK_NONE;
}
