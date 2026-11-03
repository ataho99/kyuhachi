/* kh_zip.h -- r018 (#240 ③): zip 읽기. 중앙 디렉터리 해석(ZIP64 포함) + 자체 inflate(RFC1951) + CRC32 검사.
 * 외부 라이브러리 없음. 호스트(Windows)·시험 도구(Linux) 공용. 경로는 "시스템 문자열"(Windows 에서는 ANSI). */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "platform/plat_fs.h" /* S3: platform layer */

namespace zp {

struct Entry { std::string name; uint32_t crc = 0; uint64_t csize = 0, usize = 0, lho = 0; uint16_t method = 0, flags = 0; bool isdir = false; };
struct Zip { std::string path; std::vector<Entry> e; uint64_t fsize = 0; };

inline uint32_t crc32_update(uint32_t crc, const uint8_t * p, size_t n) {
    static uint32_t T[256]; static bool init = false;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; T[i] = c; } init = true; }
    crc = ~crc; for (size_t i = 0; i < n; i++) crc = T[(crc ^ p[i]) & 255] ^ (crc >> 8); return ~crc;
}

/* ---- 64비트 파일 위치 */
inline int seek64(FILE * f, uint64_t off) {
    return plat::seek64(f, off);
}
inline uint16_t r16(const uint8_t * p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t r32(const uint8_t * p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
inline uint64_t r64(const uint8_t * p) { return (uint64_t)r32(p) | ((uint64_t)r32(p + 4) << 32); }

/* 중앙 디렉터리를 읽는다 (압축은 풀지 않는다 — 이름·크기·CRC32 만) */
inline bool open(const std::string & path, Zip & z, std::string * err = NULL) {
    z = Zip(); z.path = path; FILE * f = fopen(path.c_str(), "rb"); if (!f) { if (err) *err = "열 수 없음"; return false; }
    fseek(f, 0, SEEK_END);
    uint64_t fs = plat::tell64(f);
    z.fsize = fs; if (fs < 22) { fclose(f); if (err) *err = "zip 이 아님"; return false; }
    size_t tail = (size_t)(fs < 70000 ? fs : 70000); std::vector<uint8_t> t(tail); seek64(f, fs - tail);
    if (fread(t.data(), 1, tail, f) != tail) { fclose(f); if (err) *err = "읽기 실패"; return false; }
    long eocd = -1; for (long i = (long)tail - 22; i >= 0; i--) if (r32(&t[i]) == 0x06054b50u) { eocd = i; break; }
    if (eocd < 0) { fclose(f); if (err) *err = "zip 이 아님"; return false; }
    uint64_t total = r16(&t[eocd + 10]), cdsz = r32(&t[eocd + 12]), cdoff = r32(&t[eocd + 16]);
    if (total == 0xFFFF || cdsz == 0xFFFFFFFFu || cdoff == 0xFFFFFFFFu) {   /* ZIP64 */
        long loc = eocd - 20; if (loc >= 0 && r32(&t[loc]) == 0x07064b50u) {
            uint64_t e64 = r64(&t[loc + 8]); uint8_t h[56]; seek64(f, e64);
            if (fread(h, 1, 56, f) == 56 && r32(h) == 0x06064b50u) { total = r64(h + 32); cdsz = r64(h + 40); cdoff = r64(h + 48); }
        }
    }
    if (cdoff + cdsz > fs || cdsz > (256u << 20)) { fclose(f); if (err) *err = "중앙 디렉터리 이상"; return false; }
    std::vector<uint8_t> cd((size_t)cdsz); seek64(f, cdoff); if (cdsz && fread(cd.data(), 1, (size_t)cdsz, f) != (size_t)cdsz) { fclose(f); if (err) *err = "읽기 실패"; return false; }
    fclose(f);
    size_t p = 0;
    for (uint64_t i = 0; i < total && p + 46 <= cd.size(); i++) {
        if (r32(&cd[p]) != 0x02014b50u) break;
        Entry e; e.flags = r16(&cd[p + 8]); e.method = r16(&cd[p + 10]); e.crc = r32(&cd[p + 16]); e.csize = r32(&cd[p + 20]); e.usize = r32(&cd[p + 24]);
        size_t nl = r16(&cd[p + 28]), xl = r16(&cd[p + 30]), cl = r16(&cd[p + 32]); e.lho = r32(&cd[p + 42]);
        if (p + 46 + nl + xl + cl > cd.size()) break;
        e.name.assign((const char *)&cd[p + 46], nl);
        for (auto & ch : e.name) if (ch == '\\') ch = '/';
        e.isdir = !e.name.empty() && e.name.back() == '/';
        size_t xp = p + 46 + nl, xe = xp + xl;
        while (xp + 4 <= xe) {   /* ZIP64 확장 필드 */
            uint16_t id = r16(&cd[xp]), sz = r16(&cd[xp + 2]); size_t q = xp + 4;
            if (id == 1) { if (e.usize == 0xFFFFFFFFu && q + 8 <= xe) { e.usize = r64(&cd[q]); q += 8; } if (e.csize == 0xFFFFFFFFu && q + 8 <= xe) { e.csize = r64(&cd[q]); q += 8; } if (e.lho == 0xFFFFFFFFu && q + 8 <= xe) { e.lho = r64(&cd[q]); q += 8; } }
            xp += 4 + sz;
        }
        z.e.push_back(e); p += 46 + nl + xl + cl;
    }
    return true;
}

/* ---- inflate (RFC1951) : tinf 방식의 작은 구현 */
struct Tree { uint16_t counts[16]; uint16_t symbols[320]; };
struct Inf {
    const uint8_t * s; size_t n, p; uint32_t tag; int bits; std::vector<uint8_t> * o; bool err;
    int getbit() { if (!bits--) { if (p >= n) { err = true; return 0; } tag = s[p++]; bits = 7; } int b = (int)(tag & 1); tag >>= 1; return b; }
    uint32_t readbits(int cnt, uint32_t base) { uint32_t v = 0; if (cnt) { uint32_t lim = 1u << cnt; for (uint32_t m = 1; m < lim; m <<= 1) if (getbit()) v += m; } return v + base; }
    static void build(Tree & t, const uint8_t * len, unsigned num) {
        uint16_t offs[16]; for (int i = 0; i < 16; i++) t.counts[i] = 0; for (unsigned i = 0; i < num; i++) t.counts[len[i]]++;
        t.counts[0] = 0; unsigned sum = 0; for (int i = 0; i < 16; i++) { offs[i] = (uint16_t)sum; sum += t.counts[i]; }
        for (unsigned i = 0; i < num; i++) if (len[i]) t.symbols[offs[len[i]]++] = (uint16_t)i;
    }
    int decode(const Tree & t) {
        int sum = 0, cur = 0, len = 0;
        do { cur = 2 * cur + getbit(); ++len; if (len > 15) { err = true; return 0; } sum += t.counts[len]; cur -= t.counts[len]; } while (cur >= 0 && !err);
        if (sum + cur >= 320 || sum + cur < 0) { err = true; return 0; } return t.symbols[sum + cur];
    }
    void codes(const Tree & lt, const Tree & dt) {
        static const uint16_t lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
        static const uint8_t lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
        static const uint16_t dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
        static const uint8_t dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
        while (!err) {
            int sym = decode(lt); if (err) return;
            if (sym == 256) return;
            if (sym < 256) { o->push_back((uint8_t)sym); continue; }
            sym -= 257; if (sym >= 29) { err = true; return; }
            uint32_t len = readbits(lext[sym], lbase[sym]); int ds = decode(dt); if (err || ds >= 30) { err = true; return; }
            uint32_t dist = readbits(dext[ds], dbase[ds]); if (dist > o->size()) { err = true; return; }
            size_t at = o->size(); o->resize(at + len); uint8_t * b = o->data(); for (uint32_t i = 0; i < len; i++) b[at + i] = b[at + i - dist];
        }
    }
    bool run() {
        bits = 0; tag = 0; err = false; int fin;
        do {
            fin = getbit(); uint32_t bt = readbits(2, 0); if (err) return false;
            if (bt == 0) {
                if (p + 4 > n) return false; uint32_t len = (uint32_t)(s[p] | (s[p + 1] << 8)); p += 4; if (p + len > n) return false;
                o->insert(o->end(), s + p, s + p + len); p += len; bits = 0;
            } else if (bt == 1) {
                static Tree fl, fd; static bool init = false;
                if (!init) { uint8_t l[288]; for (int i = 0; i < 144; i++) l[i] = 8; for (int i = 144; i < 256; i++) l[i] = 9; for (int i = 256; i < 280; i++) l[i] = 7; for (int i = 280; i < 288; i++) l[i] = 8; build(fl, l, 288); uint8_t d[30]; for (int i = 0; i < 30; i++) d[i] = 5; build(fd, d, 30); init = true; }
                codes(fl, fd);
            } else if (bt == 2) {
                Tree lt, dt, ct; uint8_t lengths[320]; memset(lengths, 0, sizeof lengths);
                unsigned hlit = readbits(5, 257), hdist = readbits(5, 1), hclen = readbits(4, 4); if (hlit > 286 || hdist > 30) return false;
                static const uint8_t ci[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
                for (unsigned i = 0; i < hclen; i++) lengths[ci[i]] = (uint8_t)readbits(3, 0);
                build(ct, lengths, 19);
                unsigned num = 0; while (num < hlit + hdist && !err) {
                    int sym = decode(ct); if (err) return false;
                    if (sym < 16) lengths[num++] = (uint8_t)sym;
                    else {
                        uint8_t prev = 0; uint32_t rep = 0;
                        if (sym == 16) { if (num == 0) return false; prev = lengths[num - 1]; rep = readbits(2, 3); }
                        else if (sym == 17) rep = readbits(3, 3); else rep = readbits(7, 11);
                        if (num + rep > hlit + hdist) return false; while (rep--) lengths[num++] = prev;
                    }
                }
                if (err) return false;
                uint8_t l2[320]; memset(l2, 0, sizeof l2); memcpy(l2, lengths, hlit); build(lt, l2, hlit);
                uint8_t d2[32]; memset(d2, 0, sizeof d2); memcpy(d2, lengths + hlit, hdist); build(dt, d2, hdist);
                codes(lt, dt);
            } else return false;
            if (err) return false;
        } while (!fin);
        return true;
    }
};
inline bool inflate_raw(const uint8_t * src, size_t n, std::vector<uint8_t> & out, uint64_t expect) {
    out.clear(); if (expect < (1ull << 30)) out.reserve((size_t)expect);
    Inf d; d.s = src; d.n = n; d.p = 0; d.o = &out; d.tag = 0; d.bits = 0; d.err = false; return d.run();
}

/* 한 항목을 풀어 out 에 담는다. CRC32·크기를 확인한다. 실패하면 err 에 사유. */
inline bool extract(const Zip & z, const Entry & e, std::vector<uint8_t> & out, std::string & err, uint64_t maxsize = (768ull << 20)) {
    if (e.usize > maxsize || e.csize > maxsize) { err = "항목이 너무 큼"; return false; }
    if (e.flags & 1) { err = "암호가 걸린 zip"; return false; }
    FILE * f = fopen(z.path.c_str(), "rb"); if (!f) { err = "열 수 없음"; return false; }
    uint8_t h[30]; seek64(f, e.lho);
    if (fread(h, 1, 30, f) != 30 || r32(h) != 0x04034b50u) { fclose(f); err = "지역 머리 이상"; return false; }
    uint64_t data = e.lho + 30 + r16(h + 26) + r16(h + 28); seek64(f, data);
    std::vector<uint8_t> raw((size_t)e.csize); if (e.csize && fread(raw.data(), 1, (size_t)e.csize, f) != (size_t)e.csize) { fclose(f); err = "읽기 실패"; return false; }
    fclose(f);
    if (e.method == 0) out = raw;
    else if (e.method == 8) { if (!inflate_raw(raw.data(), raw.size(), out, e.usize)) { err = "압축 해제 실패"; return false; } }
    else { err = "지원하지 않는 zip 압축 방식(" + std::to_string(e.method) + ")"; return false; }
    if (out.size() != e.usize) { err = "크기 불일치"; return false; }
    if (crc32_update(0, out.data(), out.size()) != e.crc) { err = "CRC32 불일치"; return false; }
    return true;
}

} /* namespace zp */
