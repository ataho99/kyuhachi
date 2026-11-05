#pragma once
/* kh_share.h -- build 5b-2: run-method share code KHX1-... = game title + one run method; disks are pointed at by sha1.
   Pure functions (no UI, no file access): the host passes in "name -> sha1" and "sha1 -> name" lookups. */
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <ctype.h>
#include <string.h>
#include <zlib.h>
#include "settings/kh_ent.h"

namespace share {
struct Ref { std::string key, sha, name; };   /* key: disk_a..d, cd, demo, or swap (one Ref per swap entry, in order) */
struct Decoded { int status = 0; std::string title; ent::Kv kv; std::vector<Ref> refs; };
/* status: 0 ok, 1 unreadable, 2 check value, 3 newer */
typedef std::function<std::string(const std::string &)> StrFn;
static const char B32[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   /* no 0 O 1 I */

inline bool is_disk_key(const std::string & k) {
    return k == "disk_a" || k == "disk_b" || k == "disk_c" || k == "disk_d" || k == "cd" || k == "demo"; }
inline const std::vector<std::string> & keys() {   /* only these travel: no screen/sound/pad settings, no host paths */
    static const std::vector<std::string> k = { "label", "tpl", "boot", "disk_a", "disk_b", "disk_c", "disk_d", "cd", "demo", "swap",
        "autoexec", "bootkeys", "font_kind", "font", "mouse", "install", "hdd_mb", "next", "dosexe", "dosargs", "pointer" };
    return k; }
inline bool known_key(const std::string & k) { for (auto & x : keys()) if (x == k) return true; return false; }
inline std::string flat(const std::string & s) { std::string o; for (char c : s) o += (c == '\n' || c == '\r') ? ' ' : c; return o; }
inline std::string one_ref(const std::string & v, const StrFn & sha_of) {   /* "<sha1 first 12>|<file name>" ("-" when unknown) */
    const bool hx = ent::is_hex40(v); std::string sha = hx ? ent::lower(v) : sha_of(v);
    std::string name = hx ? std::string() : ent::base_name(v);
    for (auto & c : name) if (c == '|' || c == ',' || c == '@') c = '_';
    return (sha.size() < 12 ? std::string("-") : sha.substr(0, 12)) + "|" + name; }
inline std::string b32(const std::vector<unsigned char> & b) {
    std::string o; unsigned acc = 0; int n = 0;
    for (unsigned char x : b) { acc = (acc << 8) | x; n += 8; 
        while (n >= 5) { o += B32[(acc >> (n - 5)) & 31]; n -= 5; }
        acc &= (1u << n) - 1; }
    if (n > 0) o += B32[(acc << (5 - n)) & 31];
    return o; }
inline std::string encode(const std::string & title, const ent::Kv & kv, const StrFn & sha_of) {
    std::string p = "v=1\ntitle=" + flat(title) + "\n";
    for (auto & k : keys()) {
        auto it = kv.find(k); if (it == kv.end() || it->second.empty() || it->second == "-") continue;
        std::string v = flat(it->second);
        if (is_disk_key(k)) v = "@" + one_ref(v, sha_of);
        else if (k == "swap") {
            std::string o, s = v; size_t q = 0;
            while (q <= s.size()) { size_t e = s.find(',', q); if (e == std::string::npos) e = s.size();
                if (e > q) { if (!o.empty()) o += ","; o += "@" + one_ref(s.substr(q, e - q), sha_of); } q = e + 1; }
            v = o; }
        p += k + "=" + v + "\n"; }
    uLongf cl = compressBound((uLong)p.size()); std::vector<unsigned char> b(cl + 4);
    if (compress2(b.data(), &cl, (const Bytef *)p.data(), (uLong)p.size(), 9) != Z_OK) return std::string();
    const uLong crc = crc32(0, b.data(), (uInt)cl);
    for (int i = 3; i >= 0; i--) b[cl + (3 - i)] = (unsigned char)((crc >> (8 * i)) & 255);
    b.resize(cl + 4);
    const std::string s = b32(b); std::string o = "KHX1";
    for (size_t i = 0; i < s.size(); i++) { if (i % 4 == 0) o += '-'; o += s[i]; }
    return o; }
inline Decoded decode(const std::string & code) {
    Decoded d; std::string s;
    for (char c : code) { if (isspace((unsigned char)c) || c == '-') continue; s += (char)toupper((unsigned char)c); }
    if (s.size() < 8 || s.compare(0, 3, "KHX") != 0 || !isdigit((unsigned char)s[3])) { d.status = 1; return d; }
    if (s[3] != '1') { d.status = 3; return d; }
    std::vector<unsigned char> b; unsigned acc = 0; int n = 0;
    for (size_t i = 4; i < s.size(); i++) {
        const char * q = strchr(B32, s[i]); if (!q || !s[i]) { d.status = 1; return d; }
        acc = (acc << 5) | (unsigned)(q - B32); n += 5;
        if (n >= 8) { b.push_back((unsigned char)((acc >> (n - 8)) & 255)); n -= 8; acc &= (1u << n) - 1; } }
    if (b.size() < 6) { d.status = 1; return d; }
    const size_t cl = b.size() - 4; uLong want = 0; for (int i = 0; i < 4; i++) want = (want << 8) | b[cl + i];
    if (crc32(0, b.data(), (uInt)cl) != want) { d.status = 2; return d; }
    std::vector<unsigned char> out; bool ok = false;
    for (uLongf cap = 1 << 14; cap <= (1u << 20) && !ok; cap <<= 2) {
        out.assign(cap, 0); uLongf dl = cap; int r = uncompress(out.data(), &dl, b.data(), (uLong)cl);
        if (r == Z_OK) { out.resize(dl); ok = true; } else if (r != Z_BUF_ERROR) break; }
    if (!ok) { d.status = 1; return d; }
    const std::string t((const char *)out.data(), out.size()); size_t pos = 0;
    while (pos < t.size()) {
        size_t e = t.find('\n', pos); if (e == std::string::npos) e = t.size(); const std::string ln = t.substr(pos, e - pos); pos = e + 1;
        const size_t eq = ln.find('='); if (eq == std::string::npos) continue;
        const std::string k = ln.substr(0, eq), v = ln.substr(eq + 1);
        if (k == "title") { d.title = v; continue; }
        auto ref = [&](const std::string & key, const std::string & x) { const size_t bar = x.find('|');
            Ref r; r.key = key; const bool nb = bar == std::string::npos;
            r.sha = nb ? x.substr(1) : x.substr(1, bar - 1); r.name = nb ? std::string() : x.substr(bar + 1);
            d.refs.push_back(r); };
        if (is_disk_key(k) && !v.empty() && v[0] == '@') ref(k, v);
        else if (k == "swap") { size_t q = 0; while (q <= v.size()) { size_t c = v.find(',', q); if (c == std::string::npos) c = v.size();
            if (c > q && v[q] == '@') ref("swap", v.substr(q, c - q)); q = c + 1; } }
        else if (known_key(k) && !is_disk_key(k)) d.kv[k] = v; }
    return d; }
inline int resolve(Decoded & d, const StrFn & find, std::vector<std::string> & missing) {   /* returns how many disks were found */
    int found = 0; std::string sw;
    for (auto & r : d.refs) {
        const std::string f = r.sha == "-" ? std::string() : find(r.sha);
        if (!f.empty()) { found++; if (r.key == "swap") { if (!sw.empty()) sw += ","; sw += f; } else d.kv[r.key] = f; }
        else missing.push_back(r.name.empty() ? r.sha : r.name); }
    if (!sw.empty()) d.kv["swap"] = sw;
    return found; }
}   /* namespace share */
