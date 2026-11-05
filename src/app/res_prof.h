/* app/res_prof.h -- S9b: shared state, resources, fonts, key names, hdd scan, profile load. Moved unchanged from gensei_host.cpp lines 362-633 (as of t54t). Included in place; single translation unit. */
/* r011: 믹서 계수(mixer.cpp 사본 패치에서 정의) */
extern unsigned gensei_mix_cb, gensei_mix_under, gensei_mix_drop;
extern int gensei_audio_obt_freq, gensei_audio_obt_samples;
extern const char * gensei_audio_dev;

/* ------------------------------------------------------------------ r017 공유 상태 */
extern int gensei_nosplash;                                   /* bios.cpp 사본: 1 = DOSBox-X BIOS 스플래시 생략 */
extern int gensei_ba_mode, gensei_ba_xq, gensei_ba_gain;      /* mixer.cpp 사본: 배속 소리 (모드 · 배율x100 · 이득/256) */
extern unsigned gensei_ba_calls, gensei_ba_under, gensei_ba_drops; extern double gensei_ba_rho, gensei_ba_sumsq; extern unsigned long long gensei_ba_n;

/* ------------------------------------------------------------------ r012: 공유 상태(OSD·UI·배속) */
extern uint32_t emulator_speed;       /* dosbox.cpp: 시계 배율(%) */
void MENU_mute(bool enabled);         /* mixer.cpp(믹서 사본) */
static void gh_osd(const std::string & s, unsigned int ms = 1500) { gApp.osd = s; gApp.osd_until = SDL_GetTicks() + ms; }
/* r019 (#246-3): 외부 글꼴·셰이더가 없어 내장본을 쓰는 중이면 화면 맨 아래에 경고 한 줄 */
static std::string gh_builtin_warn(void) {
    if (gApp.builtin_res_font && gApp.builtin_res) return phr_("글꼴·셰이더 파일을 찾지 못해 내장본으로 실행 중 (run\\res 폴더 확인)");
    if (gApp.builtin_res_font) return phr_("글꼴 파일을 찾지 못해 내장 글꼴로 실행 중 (run\\res\\fonts 에 kyuhachi_font*.fnt)");
    if (gApp.builtin_res) return phr_("셰이더 파일을 찾지 못해 내장 셰이더로 실행 중 (run\\res\\shaders 폴더)");
    return std::string();
}
/* r019: UI 글꼴 — 외부 파일(exe 옆) 우선, 없으면 내장 글꼴 (빈 화면 금지) */
static bool gh_font_init(const std::string & exedir) {
    bool fl_ = ov::font_load(exedir); gh_log("T52H 일본어 글꼴: 동봉 FONTX2 = %s", ov::g_jp_fx.ok ? ov::g_jp_src.c_str() : "없음");   /* t52h (#134) */
    if (fl_) { gh_log("T52G UI 글꼴 .fnt 읽음: %s", ov::g_font_src.c_str()); return true; }
    gh_log("WARN: UI 글꼴 파일을 %s 에서 찾지 못함 -> 내장 글꼴 사용", exedir.c_str());
    ov::font_load_mem(emb_font_sb, emb_font_sb_n, emb_font_db, emb_font_db_n); gApp.builtin_res_font = true;
    gh_osd(phr_("글꼴 파일이 없어 내장 글꼴로 실행합니다 (run\\res\\fonts · run\\res\\shaders 폴더를 확인하세요)"), 12000); return ov::g_font_ok;
}
static void gh_fps_draw(ov::Canvas & oc) {   /* FPS / speed display: frames per second of the host + emulated-time / wall-time ratio, refreshed once a second */
    static Uint32 t0 = 0; static unsigned long long f0 = 0; static unsigned p0 = 0; static char buf[48] = "FPS --";
    Uint32 now = SDL_GetTicks(); unsigned long long f = (unsigned long long)gApp.frames; unsigned p = (unsigned)PIC_Ticks;
    if (t0 == 0 || now - t0 >= 1000u) { if (t0 != 0) { double dt = (double)(now - t0); snprintf(buf, sizeof buf, "FPS %.1f | SPEED %d%%", (double)(f - f0) * 1000.0 / dt, (int)((double)(p - p0) * 100.0 / dt + 0.5)); } t0 = now; f0 = f; p0 = p; }
    ov::osd_draw(oc, buf, false, 8);
}
static void gh_fps_toggle(void) {
    int v = gApp.fps_on ? 0 : 1; gApp.fps_on = v != 0; gApp.ui_.g.fpsd = v;
    if (gApp.cur_game >= 0 && gApp.cur_game < (int)ui::Ui::MAXG && gApp.ui_.has_t[gApp.cur_game]) gApp.ui_.t[gApp.cur_game].fpsd = v;
    gApp.ui_.dirty = true; gh_osd(v ? "FPS ON" : "FPS OFF");
}
static void gh_boost_osd_draw(ov::Canvas & oc, const std::string & other_osd, bool other_on) {   /* 오른쪽 위: 배속 중 계속 표시 + 그 밑에 다른 안내 */
    int y = 8; if (gApp.boost_on && gApp.bosd_on) { ov::osd_draw(oc, "▶▶ x" + std::to_string(gApp.boost_xv), true, 8); y = 38; }
    if (other_on) { ov::osd_draw(oc, other_osd, true, y); y += 30; } if (!gApp.bk_osd.empty()) ov::osd_draw(oc, gApp.bk_osd, true, y); if (gApp.fps_on) gh_fps_draw(oc);   /* t47a: 자동 입력 단계 줄 */
}
/* ---- t46x: 화면 키보드 오버레이 (경로 B) — 키 표(플랫폼 무관) + 상태·그리기. SDL 입력층은 gh_kbd_event 한 함수(아래 글루). */
#include "input/kh_kbtable.h"
#include "input/kh_kbov.h"
#include "ui/kh_kbov_draw.h"   /* S6: kbov drawing half */
#include "input/kh_vpad.h"
#include "ui/kh_vpad_draw.h"
/* ------------------------------------------------------------------ r020 (#247정정·#248): 공용 자원 run\res\fonts · run\res\rhythm */
static bool gh_exists(const std::string & p) { return plat::exists(p.c_str()); }
static std::string gh_res_font_path(const std::string & name) {   /* 글꼴 이름 → 파일 (res\fonts 우선, 옛 exe 옆도 허용). 없으면 res 쪽 경로를 돌려준다 */
    { std::string k = kha_font_lookup(name); if (!k.empty()) return k; }   /* t137 */
    std::string a = gApp.exedir + "\\res\\fonts\\" + name + ".bmp", b = gApp.exedir + "\\" + name + ".bmp";
    if (gh_exists(a)) return a; if (gh_exists(b)) return b; return a;
}
/* t52b (#127): anex86 형식 글꼴 BMP(524,350 B)가 하나도 없으면 PC-98 글꼴 로딩 뒤 널 읽기(c0000005)로 죽었다.
 * → 작품을 시작하기 전에 막고 안내한다. 일부만 있으면 있는 것으로 대신한다(글자 모양은 달라도 죽지 않음). */
static bool gh_font_bmp_ok(const std::string & p) { bool ok = false; if (!plat::find_a(p, [&](const plat::DirEnt & e) { ok = !e.is_dir && e.size_hi == 0 && e.size_lo == 524350u; return false; })) return false; return ok; }
static bool gh_any_font_bmp(std::string * which) {
    std::string dir = gApp.exedir + "\\res\\fonts\\"; bool found = false;
    plat::find_a(dir + "*.bmp", [&](const plat::DirEnt & e) { if (!e.is_dir && e.size_hi == 0 && e.size_lo == 524350u) { if (which) *which = dir + e.name; found = true; return false; } return true; });
    if (found) return true;
    { static const char * const nm[4] = { "\\res\\fonts\\kyuhachi_font.bmp", "\\res\\fonts\\pc98player_font.bmp", "\\kyuhachi_font.bmp", "\\pc98player_font.bmp" };   /* t52g: 새 이름 먼저, 옛 이름도 읽는다 · t79: 폴더 목록이 막힌 곳(안드로이드 errno=13)에서도 이름으로 직접 확인 */
      for (int q = 0; q < 4; q++) { std::string b = gApp.exedir + nm[q]; if (gh_font_bmp_ok(b)) { if (which) *which = b; return true; } } }
    return false;
}
static std::string gh_default_font_path(void) { std::string p = gh_res_font_path("kyuhachi_font"); if (gh_font_bmp_ok(p)) return p; std::string p0 = gh_res_font_path("pc98player_font"); if (gh_font_bmp_ok(p0)) return p0; std::string f; if (gh_any_font_bmp(&f)) return f; return p; }   /* t52g: 새 이름 → 옛 이름 → 아무 글꼴 */
static bool gh_font_guard(void) { if (gh_any_font_bmp(NULL)) return true; gh_log("ERROR: 글꼴 BMP 없음 -> 시작 막음 (%s\\res\\fonts)", gApp.exedir.c_str()); gApp.note = phr_("res/fonts 에 anex86 형식 글꼴 BMP 를 넣어 주세요"); gApp.note_until = SDL_GetTicks() + 8000u; gh_osd(phr_("글꼴 BMP(kyuhachi_font.bmp)가 없어 시작할 수 없습니다 — res/fonts 에 넣어 주세요"), 8000); return false; }
static std::string gh_sha1_buf(const std::vector<uint8_t> & b) { prof::Sha1 s; if (!b.empty()) s.add(&b[0], b.size()); return s.hex(); }
static bool gh_read_all(const std::string & p, std::vector<uint8_t> & out) {
    FILE * f = fopen(p.c_str(), "rb"); if (!f) return false; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); if (n < 0) { fclose(f); return false; }
    out.resize((size_t)n); size_t r = n ? fread(&out[0], 1, (size_t)n, f) : 0; fclose(f); return r == (size_t)n;
}
static bool gh_write_all(const std::string & p, const std::vector<uint8_t> & b) { FILE * f = fopen(p.c_str(), "wb"); if (!f) return false; if (!b.empty()) fwrite(&b[0], 1, b.size(), f); fclose(f); return true; }
/* res\fonts\fonts.ini: 이름=sha1 (한 줄에 하나, ; 주석) — 가져올 때 해시가 맞으면 그 이름으로 등록 */
static std::string gh_font_name_for(const std::string & sha) {
    std::vector<uint8_t> b; if (!gh_read_all(gApp.exedir + "\\res\\fonts\\fonts.ini", b)) return std::string();
    std::string s((const char *)(b.empty() ? (const uint8_t *)"" : &b[0]), b.size()); size_t p = 0;
    while (p < s.size()) { size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size(); std::string ln = s.substr(p, e - p); p = e + 1;
        while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ')) ln.pop_back(); if (ln.empty() || ln[0] == ';' || ln[0] == '#') continue;
        size_t q = ln.find('='); if (q == std::string::npos) continue; if (aw::lower(ln.substr(q + 1)) == sha) return ln.substr(0, q); }
    return std::string();
}
/* 작품 추가 때 한 번: 게임 폴더(또는 그 안 zip)에 font.bmp·anex86.bmp·2608_*.wav 가 보이면 해시를 확인해 res\ 로 복사. 같은 해시가 이미 있으면 건너뜀. 원본은 건드리지 않는다 */
static void gh_res_put(const std::string & from, const std::string & bn, const std::vector<uint8_t> & buf) {
    std::string lb = aw::lower(bn), root = gApp.exedir + "\\res";
    if (lb.size() > 4 && lb.compare(lb.size() - 4, 4, ".wav") == 0) {
        if (lb.compare(0, 5, "2608_") != 0) return;
        aw::make_dir(root); std::string rd = root + "\\rhythm"; aw::make_dir(rd); std::string dst = rd + "\\" + lb; std::vector<uint8_t> old;
        if (gh_read_all(dst, old) && gh_sha1_buf(old) == gh_sha1_buf(buf)) { gh_log("자원 가져오기: 리듬 %s — 같은 해시가 이미 있음, 건너뜀", lb.c_str()); return; }
        if (gh_exists(dst)) { gh_log("자원 가져오기: 리듬 %s — 다른 파일이 이미 있어 덮어쓰지 않음 (%s)", lb.c_str(), from.c_str()); return; }
        if (gh_write_all(dst, buf)) gh_log("자원 가져오기: 리듬 %s → res\\rhythm\\%s (출처 %s)", lb.c_str(), lb.c_str(), from.c_str());
        return;
    }
    if (lb == "font.bmp" || lb == "anex86.bmp") {
        std::string sha = gh_sha1_buf(buf); aw::make_dir(root); std::string fd = root + "\\fonts"; aw::make_dir(fd);
        std::vector<std::string> fl; aw::list_dir(fd, fl, NULL);
        for (auto & f : fl) { std::string l = aw::lower(f); if (l.size() < 5 || l.compare(l.size() - 4, 4, ".bmp") != 0) continue; std::vector<uint8_t> o; if (gh_read_all(fd + "\\" + f, o) && o.size() == buf.size() && gh_sha1_buf(o) == sha) { gh_log("자원 가져오기: 글꼴 %s — 같은 해시가 이미 있음(%s), 건너뜀", lb.c_str(), f.c_str()); return; } }
        std::string nm = gh_font_name_for(sha); if (nm.empty()) nm = "imp_" + sha.substr(0, 8);
        std::string dst = fd + "\\" + nm + ".bmp"; if (gh_exists(dst)) { gh_log("자원 가져오기: 글꼴 %s — 이름 %s 이(가) 이미 다른 파일로 있어 건너뜀", lb.c_str(), nm.c_str()); return; }
        if (gh_write_all(dst, buf)) gh_log("자원 가져오기: 글꼴 %s → res\\fonts\\%s.bmp (sha1 %s, 출처 %s)", lb.c_str(), nm.c_str(), sha.c_str(), from.c_str());
    }
}
static void gh_res_import_dir(const std::string & dir) {
    static std::set<std::string> done; if (dir.empty() || !done.insert(aw::lower(dir)).second || !aw::dir_exists(dir)) return;
    std::vector<std::string> fl; aw::list_dir(dir, fl, NULL);
    for (auto & f : fl) {
        std::string l = aw::lower(f);
        if (l == "font.bmp" || l == "anex86.bmp" || (l.size() > 9 && l.compare(0, 5, "2608_") == 0 && l.compare(l.size() - 4, 4, ".wav") == 0)) { std::vector<uint8_t> b; if (gh_read_all(aw::join(dir, f), b)) gh_res_put(aw::join(dir, f), f, b); continue; }
        if (!aw::is_zip(f)) continue; std::string zp_ = aw::join(dir, f), err; zp::Zip z; if (!zp::open(zp_, z, &err)) continue;
        for (auto & e : z.e) { if (e.isdir || e.usize == 0 || e.usize > 8u * 1024 * 1024) continue; std::string bn = aw::base_of(e.name), lb = aw::lower(bn);
            bool want = lb == "font.bmp" || lb == "anex86.bmp" || (lb.size() > 9 && lb.compare(0, 5, "2608_") == 0 && lb.compare(lb.size() - 4, 4, ".wav") == 0); if (!want) continue;
            std::vector<uint8_t> b; std::string er; if (zp::extract(z, e, b, er)) gh_res_put(zp_ + "!" + e.name, bn, b); }
    }
}
static void gh_res_on_work(const std::string & dir) { gh_res_import_dir(dir); }
/* ------------------------------------------------------------------ 작품 프로필 (#230/#237): game.ini > 게임 DB(sha1) > 자동(원판 파일 구조) */
static KBD_KEYS gh_keyname(std::string n) {
    for (auto & c : n) c = (char)tolower((unsigned char)c);
    if (n.size() == 1) { if (n[0] >= 'a' && n[0] <= 'z') { static const KBD_KEYS L[26] = { KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g, KBD_h, KBD_i, KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o, KBD_p, KBD_q, KBD_r, KBD_s, KBD_t, KBD_u, KBD_v, KBD_w, KBD_x, KBD_y, KBD_z }; return L[n[0] - 'a']; }
        if (n[0] >= '0' && n[0] <= '9') { static const KBD_KEYS N[10] = { KBD_0, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7, KBD_8, KBD_9 }; return N[n[0] - '0']; } }
    if (n == "enter" || n == "return") return KBD_enter; if (n == "space") return KBD_space; if (n == "esc" || n == "escape") return KBD_esc; if (n == "tab") return KBD_tab;
    if (n == "bs" || n == "backspace") return KBD_backspace; if (n == "del" || n == "delete") return KBD_delete; if (n == "up") return KBD_up; if (n == "down") return KBD_down;
    if (n == "left") return KBD_left; if (n == "right") return KBD_right; if (n == "caps" || n == "capslock") return KBD_capslock; if (n == "ctrl") return KBD_leftctrl;
    if (n == "shift") return KBD_leftshift; if (n == "kana") return KBD_kana; if (n == "pageup" || n == "rollup") return KBD_pageup; if (n == "pagedown" || n == "rolldown") return KBD_pagedown;
    if (n.size() == 2 && n[0] == 'f' && n[1] >= '1' && n[1] <= '9') { static const KBD_KEYS F[9] = { KBD_f1, KBD_f2, KBD_f3, KBD_f4, KBD_f5, KBD_f6, KBD_f7, KBD_f8, KBD_f9 }; return F[n[1] - '1']; }
    if (n == "f10") return KBD_f10;
    return KBD_NONE;
}
static bool gh_parse_keys(const std::string & s, std::vector<GKey> & out) {
    for (auto & it : prof::split_list(s)) { size_t c = it.find(':'); if (c == std::string::npos) continue; KBD_KEYS k = gh_keyname(it.substr(c + 1)); if (k == KBD_NONE) continue; GKey g; g.at_ms = (unsigned)atoi(it.substr(0, c).c_str()); g.key = k; out.push_back(g); }
    return !out.empty();
}
/* t36cd: bootkeys=<ms>:<키이름>,<ms>:type=<글자들> — type= 은 글자마다 220ms 간격으로 눌러 쓴다. ~ = Enter, 지원 글자: a-z 0-9 공백 . - / \ (대문자는 소문자로) */
static KBD_KEYS gh_bk_char(char ch, bool & ok) {
    ok = true; char c = (char)tolower((unsigned char)ch);
    if (c >= 'a' && c <= 'z') return gh_keyname(std::string(1, c)); if (c >= '0' && c <= '9') return gh_keyname(std::string(1, c));
    if (c == ' ') return KBD_space; if (c == '~') return KBD_enter; if (c == '.') return KBD_period; if (c == '-') return KBD_minus; if (c == '/') return KBD_slash; if (c == '\\') return KBD_backslash;
    ok = false; return KBD_NONE;
}
static bool gh_parse_bootkeys(const std::string & s, std::vector<GKey> & out) { int stepn = 0; for (auto & it : prof::split_list(s)) { ++stepn;
        size_t c = it.find(':'); if (c == std::string::npos) continue; unsigned at = (unsigned)atoi(it.substr(0, c).c_str()); std::string rest = it.substr(c + 1);
        if (rest.compare(0, 5, "type=") == 0) { std::string tx = rest.substr(5); unsigned t = at; for (char ch : tx) { bool ok; KBD_KEYS k = gh_bk_char(ch, ok); if (!ok) continue; GKey g; g.at_ms = t; g.key = k; g.step = stepn; g.lab = tx; out.push_back(g); t += 220; } continue; }
        KBD_KEYS k = gh_keyname(rest); if (k == KBD_NONE) continue; GKey g; g.at_ms = at; g.key = k; g.step = stepn; g.lab = rest; out.push_back(g);
    }
    std::stable_sort(out.begin(), out.end(), [](const GKey & a, const GKey & b) { return a.at_ms < b.at_ms; });
    return !out.empty();
}
static void gh_list_dir(const std::string & dir, std::vector<std::string> & files, std::vector<std::string> * subdirs) {
    aw::list_dir(dir, files, subdirs);   /* r018: 와이드(UTF-16) API 로 열거 — 한글 폴더·파일 이름 */
}
static void gh_nk_load_once(void);
/* t43c (#330 C1): 설치 대상 호스트 폴더 훑기 — 깊이 2 까지 EXE/COM/BAT 중 설치기·압축 도구 이름을 뺀 것. 반환 = 개수, run/dirp = 하나 고를 때의 파일·폴더(루트면 빈칸) */
static int gh_scan_hdd(const std::string & root, std::string & run, std::string & dirp) {
    struct F { std::string d, n; uint64_t sz; }; std::vector<F> v;
    std::function<void(const std::string &, const std::string &, int)> rec = [&](const std::string & ph, const std::string & rel, int dep) {
        plat::find_a(ph + "\\*", [&](const plat::DirEnt & fd) -> bool {
        std::string n = fd.name; if (n == "." || n == "..") return true;
            if (fd.is_dir) { if (dep < 2) rec(ph + "\\" + n, rel.empty() ? n : rel + "\\" + n, dep + 1); return true; }
            std::string u; for (char c : n) u += (char)toupper((unsigned char)c); size_t d = u.find_last_of('.'); if (d == std::string::npos) return true; std::string x = u.substr(d + 1), st = u.substr(0, d);
            if (x != "EXE" && x != "COM" && x != "BAT") return true; if (prof::inst_rank(n) || prof::is_tool_stem(st) || st == "COMMAND" || st == "LHA" || st == "LHARC" || st == "UNLHA" || st == "LHX" || st == "DELETE" || st == "UNINST") return true;
            v.push_back({ rel, u, ((uint64_t)fd.size_hi << 32) | fd.size_lo }); return true; }); };
    rec(root, "", 0);
    if (v.size() > 1) { std::vector<F> b; for (auto & f : v) if (f.n.size() > 4 && f.n.substr(f.n.size() - 4) == ".BAT") b.push_back(f); if (b.size() == 1) { run = b[0].n; dirp = b[0].d; return 1; } }
    if (v.size() > 1) { std::vector<prof::RunEnt> ents; for (auto & f : v) if (f.d.empty()) { prof::RunEnt e; e.name = f.n; e.size = (uint32_t)f.sz; ents.push_back(e); }   /* t43r: 풀린(설치된) 폴더 루트에 실행 파일이 여럿이면 선택 규칙(dos_pick)으로 하나 고른다 */
        if (ents.size() > 1) { std::string pk = prof::dos_pick(ents, ""); if (!pk.empty()) { run = pk; dirp = ""; return 1; } } }
    if (v.size() >= 1) { run = v[0].n; dirp = v[0].d; } return (int)v.size();
}
static void gh_db_load_once(void) {
    if (gApp.db_loaded) return; gApp.db_loaded = true; gh_nk_load_once();
    if (prof::db_load(gApp.exedir + "\\gamedb.ini", gApp.db)) gh_log("gamedb.ini: %d games", (int)gApp.db.size()); else gh_log("gamedb.ini: none");
}
/* id = 작품 폴더 이름(games\<id>) 또는 폴더 경로. 채워지는 것: p(디스크·부팅·자동키·제목·cycles·font·extra·덮어쓰기·패드), g_imgdir */
/* r019 (#247-6): 작품 폴더의 보조 zip(RetroArch.zip 등) 안에서 글꼴·사운드 롬·리듬 WAV 를 찾아 games\_zip\_aux\<키>\ 에 풀어 둔다.
   want = 찾을 파일 이름(대소문자 무시) · 빈칸이면 font.bmp/sound.rom/bios.rom/*.wav 전부. 돌려주는 값 = want 가 들어 있던 풀린 폴더(없으면 빈칸) · got 에 푼 파일 목록 */
static std::string gh_aux_extract(const std::vector<std::string> & dirs, const std::string & want, std::vector<std::string> * got) {
    std::string wl = aw::lower(aw::base_of(want)), found; std::set<std::string> seen;
    auto wanted = [&](const std::string & bn) { std::string b = aw::lower(bn); if (!wl.empty()) return b == wl; size_t d = b.find_last_of('.'); std::string x = d == std::string::npos ? "" : b.substr(d); return b == "font.bmp" || b == "sound.rom" || b == "bios.rom" || x == ".wav"; };
    for (auto & dir : dirs) {
        if (dir.empty() || !seen.insert(aw::lower(dir)).second || !aw::dir_exists(dir)) continue;
        std::vector<std::string> fl; aw::list_dir(dir, fl, NULL);
        for (auto & f : fl) {
            if (!aw::is_zip(f)) continue; std::string zp_ = aw::join(dir, f), err; zp::Zip z; if (!zp::open(zp_, z, &err)) continue;
            uint64_t zs = 0, zm = 0; aw::file_stat(zp_, zs, zm); std::string out = aw::join(aw::join(aw::join(gApp.exedir, "games"), "_zip"), "_aux"); aw::make_dir(aw::join(gApp.exedir, "games")); aw::make_dir(aw::join(aw::join(gApp.exedir, "games"), "_zip")); aw::make_dir(out);
            out = aw::join(out, aw::hex64(aw::fnv(aw::lower(zp_) + ":" + std::to_string(zs))).substr(0, 12)); bool made = false;
            for (auto & e : z.e) {
                if (e.isdir || !wanted(aw::base_of(e.name))) continue;
                if (!made) { aw::make_dir(out); made = true; }
                std::string bn = aw::sys_from_ini(aw::base_of(e.name)), dst = aw::join(out, bn); uint64_t sz = 0, mt = 0;
                if (!(aw::file_stat(dst, sz, mt) && sz == e.usize)) { std::vector<uint8_t> buf; std::string er; if (!zp::extract(z, e, buf, er)) { gh_log("WARN: 보조 zip %s: %s: %s", zp_.c_str(), e.name.c_str(), er.c_str()); continue; }
                    FILE * fo = fopen(dst.c_str(), "wb"); if (!fo) continue; if (!buf.empty()) fwrite(buf.data(), 1, buf.size(), fo); fclose(fo); }
                if (got) got->push_back(dst); if (!wl.empty() && aw::lower(bn) == wl && found.empty()) found = out;
            }
            if (!found.empty()) return found;
        }
    }
    return found;
}
static bool gh_profile(const std::string & id, GProfile & p, bool quick = false) {
    p = GProfile();
    gh_db_load_once();
    bool path = id.find('\\') != std::string::npos || id.find('/') != std::string::npos || id.find(':') != std::string::npos;
    std::string dir = path ? id : gApp.exedir + "\\games\\" + id;
    std::vector<std::string> files, subs; gh_list_dir(dir, files, &subs);
    prof::Ini gi; bool have_ini = prof::ini_load(dir + "\\game.ini", gi) && gi.count("game");
    std::string imgdir = dir;
    if (have_ini && gi["game"].count("imgdir")) { std::string r = aw::sys_from_ini(gi["game"]["imgdir"]); imgdir =(r.size() > 1 && (r[1] == ':' || r[0] == '/')) ? r : gApp.exedir + "\\" + r; files.clear(); gh_list_dir(imgdir, files, NULL); }
    if (!have_ini && files.empty()) { gh_log("profile '%s': 폴더 없음/비어 있음 (%s)", id.c_str(), dir.c_str()); return false; }
    std::vector<prof::ImgInfo> imgs;
    for (auto & f : files) {
        std::string lf = f; for (auto & c : lf) c = (char)tolower((unsigned char)c);
        if (lf == "game.ini" || (lf.size() > 4 && (lf.substr(lf.size() - 4) == ".bmp" || lf.substr(lf.size() - 4) == ".txt" || lf.substr(lf.size() - 4) == ".png"))) continue;
        prof::ImgInfo i = prof::probe(imgdir + "\\" + f);
        if (i.fmt == prof::F_UNKNOWN || i.fmt == prof::F_NOTIMG) continue;
        if (!quick && i.size < (96ull << 20)) i.sha1 = gApp.shacache.sha1(i.path);
        prof::probe_boot(i); imgs.push_back(i);
    }
    prof::Profile q = prof::resolve(dir, imgs, gApp.db, id);
    for (auto & dk : q.disk) dk = aw::sys_from_ini(dk);          /* game.ini 는 UTF-8 — 시스템 문자열로 */
    for (auto & sw : q.swap) sw = aw::sys_from_ini(sw);
    if (q.kv.count("srcdir")) q.kv["srcdir"] = aw::sys_from_ini(q.kv["srcdir"]);
    if (!q.dbhit_title.empty() && !q.kv.count("dbhit")) q.kv["dbhit"] = q.dbhit_title;
    gApp.pst_id = path ? id.substr(id.find_last_of("\\/") + 1) : id; if (!q.id.empty() && have_ini) gApp.pst_id = q.id;
    if (!q.id.empty() && !have_ini && q.src == "DB") gApp.pst_id = path ? gApp.pst_id : id;   /* 슬롯·로그 폴더 이름은 폴더 이름을 유지(저장 호환) */
    gApp.pst_pool.push_back(gApp.pst_id); p.id = gApp.pst_pool.back().c_str(); p.title = q.title.empty() ? gApp.pst_id : q.title; p.engine = q.engine; p.src = q.src; p.reason = q.reason;
    for (int i = 0; i < 4; i++) { if (q.disk[i].empty()) { p.disk[i] = NULL; } else { gApp.pst_pool.push_back(q.disk[i]); p.disk[i] = gApp.pst_pool.back().c_str(); } }
    p.bootletter = q.bootletter; p.swap = q.swap; p.install_required = q.install_required; p.imgdir = imgdir;
    p.ov = q.ov; p.pad = q.pad; p.kv = q.kv;
    if (q.kv.count("cycles")) p.cycles = q.kv["cycles"]; if (q.kv.count("font")) p.font = q.kv["font"]; if (q.kv.count("extra")) p.extra = q.kv["extra"];
    if (q.kv.count("never_map")) p.never_map = q.kv["never_map"];
    if (q.kv.count("keys")) gh_parse_keys(q.kv["keys"], p.keys);
    if (q.kv.count("bootkeys")) gh_parse_bootkeys(q.kv["bootkeys"], p.bootkeys);
    if (q.kv.count("cd")) { p.cd = aw::sys_from_ini(q.kv["cd"]); if (!p.cd.empty() && !(p.cd.size() > 1 && p.cd[1] == ':') && p.cd[0] != '\\') p.cd = imgdir + "\\" + p.cd; }
    if (q.kv.count("autoexec")) p.autoexec = aw::sys_from_ini(q.kv["autoexec"]);
    if (!quick) gh_log("profile '%s' [%s] 제목='%s' 부팅=%c A=%s B=%s C=%s D=%s swap=%d keys=%d cycles=%s font=%s cd=%s bootkeys=%d | %s", id.c_str(), p.src.c_str(), p.title.c_str(), p.bootletter, p.disk[0] ? p.disk[0] : "-", p.disk[1] ? p.disk[1] : "-", p.disk[2] ? p.disk[2] : "-", p.disk[3] ? p.disk[3] : "-", (int)p.swap.size(), (int)p.keys.size(), p.cycles.c_str(), p.font.c_str(), p.cd.empty() ? "-" : p.cd.c_str(), (int)p.bootkeys.size(), p.reason.c_str());
    return true;
}

/* #451 step 1: pre-flight. Every disk / CD image the chosen work needs must open; otherwise stay in the menu and say why (same slot as gh_font_guard). */
/* screen phrase via the phrase table (key = original Korean text); thin wrapper so the T11 literal check sees keyed phrases */
static inline const char * T(const char * s) { return phr_(s); }
static bool gh_disk_guard(const std::string & id) {
    GProfile p; if (id.empty() || !gh_profile(id, p, true)) return true;   /* unknown profile: the normal path reports it */
    std::string dir = gApp.imgdir_given ? gApp.imgdir : (p.imgdir.empty() ? gApp.imgdir : p.imgdir);
    std::vector<std::string> need;
    for (int i = 0; i < 4; i++) if (p.disk[i]) need.push_back(dir + "\\" + p.disk[i]);
    for (auto & sw : p.swap) need.push_back(dir + "\\" + sw);
    if (!p.cd.empty()) need.push_back(p.cd);
    for (auto & path : need) {
        size_t b0 = path.find_last_of("\\/"); std::string nm0 = b0 == std::string::npos ? path : path.substr(b0 + 1);
        { FILE * wf = fopen((gApp.savedir + "\\" + id + "\\disk\\" + nm0).c_str(), "rb"); if (wf) { fclose(wf); continue; } }   /* t601: 작업 사본이 있으면 원본이 없어도 시작(사본 우선) */
        FILE * f = fopen(path.c_str(), "rb"); if (f) { fclose(f); continue; }
        int e = errno; std::string nm = nm0;
        std::string why = e == ENOENT ? T("파일이 없습니다") : (e == EACCES ? T("열 권한이 없습니다") : T("열지 못했습니다"));
        gh_log("ERROR: disk preflight failed -> start blocked: %s (errno=%d)", path.c_str(), e);
        std::string msg = e == ENOENT ? std::string(T("원본 이미지가 없습니다: ")) + aw::acp_to_u8(path) : std::string(T("디스크를 열 수 없습니다: ")) + aw::acp_to_u8(nm) + " (" + why + ")";
        gApp.note = msg; gApp.note_until = SDL_GetTicks() + 8000u; gh_osd(msg, 8000); return false;
    }
    return true;
}
/* t601 (#544): 「원본 디스크로 초기화」는 원본이 있어야 한다 — 없으면 사본을 건드리지 않고 없는 경로를 돌려준다 */
static bool gh_orig_present(const std::string & id, std::string & missing) {
    GProfile p; if (id.empty() || !gh_profile(id, p, true)) return true;
    std::string dir = gApp.imgdir_given ? gApp.imgdir : (p.imgdir.empty() ? gApp.imgdir : p.imgdir);
    std::vector<std::string> need;
    for (int i = 0; i < 4; i++) if (p.disk[i]) need.push_back(dir + "\\" + p.disk[i]);
    for (auto & sw : p.swap) need.push_back(dir + "\\" + sw);
    for (auto & path : need) { FILE * f = fopen(path.c_str(), "rb"); if (f) { fclose(f); continue; } missing = path; return false; }
    return true;
}

/* t51a (#878 묶음 2): 창 안 전환 — 일시정지 메뉴 「다음 단계」·「메인 메뉴로」 가 창·프로세스를 닫지 않고 같은 창에서 디스크를 바꿔 리셋한다. 실패하면 현행 재실행 경로(폴백, 설계_창안전환_874.md §9) */
static void gh_apply_settings(const ui::Settings & s, bool vol);


