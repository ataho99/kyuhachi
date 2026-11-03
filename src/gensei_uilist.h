/* gensei_uilist.h -- t39: 작품 목록의 t39 필드 채우기(언어·short·변형 접기·실행 방법 목록). 호스트와 하니스가 같은 코드를 쓴다. */
#pragma once
#include "gensei_ui.h"
#include "settings/gensei_prof.h"
#include <set>

namespace ulist {
inline std::string kvs(const std::map<std::string, std::string> & kv, const char * k) { auto it = kv.find(k); return it == kv.end() ? std::string() : it->second; }
/* 한 작품 폴더의 t39 필드. kv = 프로필 kv(또는 game.ini [game] 값) · title = 표시 제목 */
inline void fill(ui::UiGame & g, const std::string & dir, const std::string & imgdir, const std::map<std::string, std::string> & kv, const std::string & title) {
    g.dir = dir; g.imgdir = imgdir.empty() ? dir : imgdir;
    std::string l = kvs(kv, "lang_eff"); if (l != "ko" && l != "jp") { prof::Profile q; q.kv = kv; q.title = title; l = prof::eff_lang(q); }
    g.lang = l; g.short_t = kvs(kv, "short"); g.variant_of = kvs(kv, "variant_of"); g.fgroup = kvs(kv, "font_group"); g.gfont = kvs(kv, "font_file");
    g.lang_exp.clear(); { prof::Ini ui_; if (prof::ini_load(dir + "/user.ini", ui_) && ui_.count("game") && ui_["game"].count("lang")) { std::string le = ui_["game"]["lang"]; if (le == "ko" || le == "jp") g.lang_exp = le; } }   /* t44a (#494): 사용자 지정 언어는 user.ini [game] lang= 만 본다(game.ini·DB 값과 구분) */
    std::string o = kvs(kv, "order"); if (!o.empty()) g.order_n = atoi(o.c_str());
}
/* 항목 목록 읽기(game.ini + user.ini). 정의가 없으면 「자동 판정」 한 가지 */
inline void load_ents(ui::UiGame & g) {
    g.ents.clear(); g.defent.clear(); if (g.dir.empty()) return;
    std::string def; std::vector<prof::EntInfo> es = prof::list_entries(g.dir, &def); g.defent = def;
    for (auto & e : es) {
        ui::UiEnt u; u.name = e.name; u.label = e.label; u.hidden = e.hidden; u.tag = e.tag == "game.ini" ? "game.ini" : e.tag;
        prof::entry_kv(g.dir, e.name, u.kv); std::string lg = kvs(u.kv, "lang"); if (lg != "ko" && lg != "jp") lg = kvs(u.kv, "font_kind") == "jp" ? "jp" : g.lang; u.lang = lg;
        g.ents.push_back(u);
    }
    if (g.ents.empty()) { ui::UiEnt u; u.pseudo = true; u.label = g.lang == "jp" ? phr_("원판 일본어") : phr_("기본"); u.tag = "자동"; u.lang = g.lang; g.ents.push_back(u); }
}
/* 변형 연결: variant_of=<id> 가 있거나, 「_jp」 로 끝나고 같은 이름 앞부분의 폴더가 있으면 그 폴더의 변형. 변형의 항목은 기반 작품 목록에 한 줄로 붙는다 */
inline void link_variants(std::vector<ui::UiGame> & games, const std::map<std::string, std::string> * vlabel = NULL) {
    std::map<std::string, int> byid; for (size_t i = 0; i < games.size(); i++) byid[games[i].id] = (int)i;
    for (auto & g : games) g.base = -1;
    for (size_t i = 0; i < games.size(); i++) {
        ui::UiGame & v = games[i]; std::string b = v.variant_of;
        if (b.empty() && v.id.size() > 3 && v.id.compare(v.id.size() - 3, 3, "_jp") == 0) { std::string cand = v.id.substr(0, v.id.size() - 3); if (byid.count(cand)) b = cand; }
        if (b.empty() || !byid.count(b) || b == v.id) { if (!b.empty() && !byid.count(b)) v.variant_of.clear(); continue; }
        v.variant_of = b; v.base = byid[b];
    }
    for (size_t i = 0; i < games.size(); i++) {   /* 변형 항목을 기반 작품의 ents 뒤에 붙인다 */
        ui::UiGame & v = games[i]; if (v.base < 0) continue; ui::UiGame & bg = games[v.base];
        for (auto it = bg.ents.begin(); it != bg.ents.end();) { if (it->vgi == (int)i) it = bg.ents.erase(it); else ++it; }
        ui::UiEnt u; u.vgi = (int)i; u.tag = phr_("변형"); u.lang = v.lang; std::string lb = vlabel && vlabel->count(v.id) ? vlabel->at(v.id) : std::string(); u.label = lb.empty() ? (v.lang == "jp" ? std::string(phr_("원판 일본어")) : std::string(phr_("변형"))) : lb;
        bg.ents.push_back(u);
    }
}
inline void load_all(std::vector<ui::UiGame> & games, const std::map<std::string, std::string> * vlabel = NULL) {
    for (auto & g : games) { g.ents.clear(); load_ents(g); }
    link_variants(games, vlabel);
}
/* 저장 뒤 한 작품만 다시 읽기(변형 항목은 유지) */
inline void reload_one(std::vector<ui::UiGame> & games, int gi, const std::map<std::string, std::string> * vlabel = NULL) {
    std::vector<ui::UiEnt> keep; for (auto & e : games[gi].ents) if (e.vgi >= 0) keep.push_back(e);
    load_ents(games[gi]); for (auto & e : keep) games[gi].ents.push_back(e);
    (void)vlabel;
}
} // namespace ulist
