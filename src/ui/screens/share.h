#pragma once
/* screens/share.h -- build 5b-2: run-method share code. Make (S_SHARE), pick a source (S_SHIMP), preview and add (S_SHPREV). */
inline void Ui::shr_open_make(int g, int menu_i) {
    entg = g; const std::vector<int> v = vents(g);
    if (menu_i < 0 || menu_i >= (int)v.size()) return;
    const UiEnt & e = games[g].ents[v[menu_i]];
    ent::Kv kv = e.kv;
    if (e.pseudo || kv.empty()) { ent::Kv r; if (env.resolved && env.resolved(g, r)) kv = r; }
    if (!kv.count("label")) kv["label"] = e.label;
    const std::string title = games[g].title_user.empty() ? games[g].title : games[g].title_user;
    auto sha = [&](const std::string & n) { return env.sha_of ? env.sha_of(g, n) : std::string(); };
    sh_code = share::encode(title, kv, sha); sh_ename = e.label; sh_msg.clear();
    if (env.clip_set) { env.clip_set(sh_code); sh_msg = phr_("shr_copied"); }
    if (env.khx_write && env.khx_write(g, e.name, sh_code)) sh_msg += std::string("  ") + phr_("shr_saved");
    scr = S_SHARE; sel = 0;
}
inline void Ui::d_share(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 608, 296, WH, phr_("shr_title"));
    text(c, 32, 44, fit(sh_ename, 560), YL, K);
    int y = 70;
    for (size_t i = 0; i < sh_code.size() && y < 270; i += 64, y += 18) text(c, 32, y, sh_code.substr(i, 64), CY, K);
    text(c, 32, 288, fit(sh_msg, 576), WH, K);
    helpline(c, phr_("shr_help_make"));
}
inline void Ui::k_share(UKey k, Act & a) {
    (void)a;
    if (k == UK_OK) { if (env.clip_set) { env.clip_set(sh_code); sh_msg = phr_("shr_copied"); } }
    else if (k == UK_CANCEL) { scr = S_ENT; sel = menu_ent; }
}
inline void Ui::shr_open_import() {
    sh_files = env.khx_list ? env.khx_list(entg) : std::vector<std::string>();
    sh_msg.clear(); scr = S_SHIMP; sel = 0;
}
inline void Ui::shr_preview(const std::string & code) {
    sh_dec = share::decode(code); sh_missing.clear(); sh_found = 0; sh_total = 0;
    if (sh_dec.status != 0) {
        sh_msg = sh_dec.status == 2 ? phr_("shr_crc") : sh_dec.status == 3 ? phr_("shr_newer") : phr_("shr_bad");
        return;
    }
    auto find = [&](const std::string & s) { return env.find_sha ? env.find_sha(entg, s) : std::string(); };
    sh_total = (int)sh_dec.refs.size(); sh_found = share::resolve(sh_dec, find, sh_missing);
    sh_msg.clear(); scr = S_SHPREV; sel = 0;
}
inline void Ui::d_shimp(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 608, 296, WH, phr_("shr_imp_title"));
    std::vector<std::string> r = { phr_("shr_clip") };
    for (auto & f : sh_files) r.push_back(fit(f, 540));
    items(c, 40, 46, 560, r, sel, 26);
    text(c, 32, 288, fit(sh_msg, 576), OR, K);
    helpline(c, phr_("shr_imp_help"));
}
inline void Ui::k_shimp(UKey k, Act & a) {
    (void)a; const int n = 1 + (int)sh_files.size();
    if (k == UK_UP) sel = (sel + n - 1) % n;
    else if (k == UK_DOWN) sel = (sel + 1) % n;
    else if (k == UK_OK) {
        std::string code;
        if (sel == 0) { if (env.clip_get) code = env.clip_get(); }
        else if (env.khx_read) code = env.khx_read(entg, sh_files[sel - 1]);
        if (code.empty()) sh_msg = phr_("shr_empty"); else shr_preview(code);
    } else if (k == UK_CANCEL) { scr = S_TPL; sel = 9; }
}
inline void Ui::d_shprev(Canvas & c) {
    titlebar(c, "");
    rbox(c, 16, 24, 608, 296, WH, phr_("shr_prev_title"));
    auto lab = [&](int y, const char * k, const std::string & v) { text(c, 32, y, std::string(phr_(k)) + ": " + v, WH, K); };
    lab(46, "shr_game", fit(sh_dec.title, 420));
    auto it = sh_dec.kv.find("label"); lab(68, "shr_entry", fit(it == sh_dec.kv.end() ? std::string() : it->second, 420));
    lab(90, "shr_found", std::to_string(sh_found) + " / " + std::to_string(sh_total));
    int y = 112;
    for (size_t i = 0; i < sh_missing.size() && i < 4; i++, y += 20)
        text(c, 48, y, fit(std::string(phr_("shr_missing")) + ": " + sh_missing[i], 540), OR, K);
    if (sh_total > 0 && sh_found == 0) text(c, 32, 196, fit(phr_("shr_nogame"), 576), OR, K);
    const std::vector<std::string> r = { phr_("shr_add"), phr_("shr_add_title"), phr_("shr_cancel") };
    items(c, 40, 226, 540, r, sel, 28);
    helpline(c, phr_("shr_prev_help"));
}
inline void Ui::k_shprev(UKey k, Act & a) {
    (void)a;
    if (k == UK_UP) sel = (sel + 2) % 3;
    else if (k == UK_DOWN) sel = (sel + 1) % 3;
    else if (k == UK_CANCEL || (k == UK_OK && sel == 2)) { scr = S_SHIMP; sel = 0; }
    else if (k == UK_OK) {
        auto it = sh_dec.kv.find("label");
        const std::string nm = ent::unique_name(it == sh_dec.kv.end() ? std::string(phr_("shr_default")) : it->second, ents_used(entg));
        const bool ok = env.save && env.save(entg, nm, sh_dec.kv, std::string());
        if (ok && sel == 1 && env.setkey && !sh_dec.title.empty()) env.setkey(entg, "title_user", sh_dec.title);
        if (ok && env.reload) env.reload(entg);
        dirty = true; msg = ok ? phr_("shr_added") : phr_("shr_fail"); scr = S_ENT; sel = 0;
    }
}
