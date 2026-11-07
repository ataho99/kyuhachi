// kh_prof.h — 원본 파일 구조 기반 자동 프로필 + game.ini 덮어쓰기 + 게임 DB(sha1 → 작품). (#230/#237)
// 헤더만으로 형식을 판별한다(확장자 무시). 호스트(Windows)·시험 도구(Linux) 공용. 원본은 읽기만 한다.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <set>
#include "settings/kh_tr.h"

namespace prof {

/* ---------------------------------------------------------------- SHA-1 */
struct Sha1 {
    uint32_t h[5]; uint64_t len; uint8_t buf[64]; size_t bl;
    Sha1() { h[0] = 0x67452301; h[1] = 0xEFCDAB89; h[2] = 0x98BADCFE; h[3] = 0x10325476; h[4] = 0xC3D2E1F0; len = 0; bl = 0; }
    static uint32_t rol(uint32_t v, int s) { return (v << s) | (v >> (32 - s)); }
    void blk(const uint8_t * p) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; } else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; } else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i]; e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    void add(const uint8_t * p, size_t n) {
        len += n;
        while (n) { size_t t = std::min(n, (size_t)64 - bl); memcpy(buf + bl, p, t); bl += t; p += t; n -= t; if (bl == 64) { blk(buf); bl = 0; } }
    }
    std::string hex() {
        uint64_t bits = len * 8; uint8_t pad = 0x80; add(&pad, 1); uint8_t z = 0; while (bl != 56) add(&z, 1);
        uint8_t lb[8]; for (int i = 0; i < 8; i++) lb[i] = (uint8_t)(bits >> (56 - 8 * i)); add(lb, 8);
        char o[41]; for (int i = 0; i < 5; i++) snprintf(o + i * 8, 9, "%08x", h[i]); return std::string(o, 40);
    }
};
inline std::string sha1_file(const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return "";
    Sha1 s; std::vector<uint8_t> b(1 << 20); size_t n;
    while ((n = fread(&b[0], 1, b.size(), f)) > 0) s.add(&b[0], n);
    fclose(f); return s.hex();
}

/* ---------------------------------------------------------------- 형식 판별 */
enum Fmt { F_UNKNOWN = 0, F_FDI, F_HDI, F_NHD, F_D88, F_NFD0, F_NFD1, F_TD0, F_RAWFD, F_RAWHD, F_VHD, F_ISO, F_DIM, F_NOTIMG };
struct Support { Fmt f; const char * name; const char * exts; const char * how; };
/* DOSBox-X(PC-98) 가 읽는 형식 표 — how: 호스트 지원 상태(실측 근거: 링크 객체 bios_disk.obj 만 포함) */
static const Support g_table[] = {
    { F_FDI,   "FDI",       ".fdi",            "지원(imageDisk 헤더 해석: 0x20 헤더, 섹터크기/섹터/면/실린더)" },
    { F_HDI,   "HDI",       ".hdi",            "지원(imageDisk, HDD 헤더 0x1000)" },
    { F_NHD,   "NHD (T98HDDIMAGE.R0)", ".nhd", "지원(imageDisk)" },
    { F_RAWFD, "HDM/IMG/IMA/XDF/DSK(원시 FD)", ".hdm .img .ima .xdf .dsk", "지원(크기로 지오메트리: 1261568=2HD 77x2x8x1024, 737280=2DD, 1228800/1474560)" },
    { F_RAWHD, "HDD/IMG(원시 HD)", ".hdd .img", "지원(imageDisk, 크기로 추정 — 지오메트리 불명확하면 game.ini 로 지정)" },
    { F_D88,   "D88/D77/88D", ".d88 .d77 .88d .98d", "지원(r017 링크: imageDiskD88, 헤더로 판별 — 확장자 무관)" },
    { F_NFD0,  "NFD r0",    ".nfd",            "지원(r017 링크: imageDiskNFD revision 0)" },
    { F_NFD1,  "NFD r1",    ".nfd",            "지원(r017 링크: imageDiskNFD revision 1, DOSBox-X 도 부분 지원)" },
    { F_TD0,   "TD0 (TeleDisk)", ".td0",       "미링크(imagedisk_teledisk.cpp)" },
    { F_VHD,   "VHD",       ".vhd",            "미링크(bios_vhd.cpp)" },
    { F_ISO,   "ISO/CD (CD001)", ".iso .cue .ccd .img", "CD — 부팅 불가/설치 전용(지시 #237)" },
    { F_DIM,   "DIM(X68k)", ".dim",            "PC-98 용 아님(참고)" },
};
inline const char * fmt_name(Fmt f) { for (auto & s : g_table) if (s.f == f) return s.name; return f == F_NOTIMG ? "이미지 아님" : "알 수 없음"; }
inline const char * fmt_how(Fmt f) { for (auto & s : g_table) if (s.f == f) return s.how; return ""; }
inline bool fmt_supported(Fmt f) { return f == F_FDI || f == F_HDI || f == F_NHD || f == F_RAWFD || f == F_RAWHD || f == F_D88 || f == F_NFD0 || f == F_NFD1; }   /* r017: D88/NFD 링크 (imagedisk_d88/nfd.obj) */

struct ImgInfo {
    std::string path, base, sha1;
    uint64_t size = 0;
    Fmt fmt = F_UNKNOWN;
    bool hdd = false;               // 하드(IDE/SCSI) 로 붙일 것인가
    uint32_t cyl = 0, heads = 0, sect = 0, ssize = 0; // 알면 채움
    uint32_t hdr = 0;               // 헤더 바이트 수
    bool boot_ok = false;           // 첫 섹터 비어있지 않음(부팅 가능 후보)
    bool ipl = false;               // IPL1 시그니처(HDD)
    std::string label;              // 첫 섹터 속 텍스트(디스크 이름 추정)
    uint64_t payload = 0;           // r017: 헤더를 뺀 원시 데이터 크기(FDI/HDI/NHD), 그 밖에는 파일 크기
    bool nosys_boot = false;        // t43: 부트섹터 문구가 「No system files / data disk」 — DOS 시스템 없는 FAT 디스크(스스로 부팅 못 함)
    bool has_sys = false;           // t40: FD 의 FAT 루트에 IO.SYS/MSDOS.SYS 가 있음(DOS 시스템 디스크)
    bool sheet = false;             // t40: .ccd/.cue 같은 CD 시트(이미지가 아니라 CD 이미지를 가리키는 글)
    std::string vlabel, exes;       // r017: FAT 루트의 볼륨 라벨 · 실행 파일 이름들(콤마, 대문자) — 변형 판정 증거
    std::string note;
};

static inline uint32_t rd32(const uint8_t * p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static inline uint16_t rd16(const uint8_t * p) { return (uint16_t)(p[0] | (p[1] << 8)); }

inline bool pow2(uint32_t v) { return v && !(v & (v - 1)); }

inline ImgInfo probe_raw(const std::string & path) {
    ImgInfo r; r.path = path; r.payload = 0;
    size_t sl = path.find_last_of("/\\"); r.base = sl == std::string::npos ? path : path.substr(sl + 1);
    FILE * f = fopen(path.c_str(), "rb"); if (!f) { r.fmt = F_NOTIMG; r.note = "열 수 없음"; return r; }
    fseek(f, 0, SEEK_END); r.size = (uint64_t)ftell(f); fseek(f, 0, SEEK_SET);
    {   /* t40: .ccd/.cue = CD 시트. 머리를 읽지 않고 CD 로 표시 (같은 줄기의 .img/.bin 을 CD 로 묶는 근거) */
        std::string lb = r.base; for (auto & c : lb) c = (char)tolower((unsigned char)c); size_t dd = lb.find_last_of('.'); std::string ex = dd == std::string::npos ? std::string() : lb.substr(dd);
        if (ex == ".sub") { fclose(f); r.fmt = F_NOTIMG; r.note = "CD 서브채널(.sub) — 이미지 아님"; return r; }   /* t40: 크기가 512 배수면 HDD 로 오인하던 것 (s046) */
        if ((ex == ".ccd" || ex == ".cue") && r.size < (1u << 20)) { fclose(f); r.fmt = F_ISO; r.sheet = true; r.note = "CD 시트(" + ex + ")"; return r; }
    }
    std::vector<uint8_t> h(0x2000, 0); size_t hn = fread(&h[0], 1, h.size(), f); (void)hn;
    // ISO: "CD001" at 0x8001
    bool iso = false;
    if (r.size > 0x8006) { uint8_t t[6] = {0}; fseek(f, 0x8001, SEEK_SET); if (fread(t, 1, 5, f) == 5 && !memcmp(t, "CD001", 5)) iso = true; }
    // VHD footer "conectix"
    bool vhd = false;
    if (r.size >= 512) { uint8_t t[8] = {0}; fseek(f, (long)(r.size - 512), SEEK_SET); if (fread(t, 1, 8, f) == 8 && !memcmp(t, "conectix", 8)) vhd = true; }
    if (!memcmp(&h[0], "conectix", 8)) vhd = true;
    fclose(f);
    const uint8_t * p = &h[0];
    ImgInfo & o = r;
    if (!iso && r.size % 2352 == 0 && r.size >= 2352 * 300 && !memcmp(p, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00", 12)) { o.fmt = F_ISO; o.note = "원시 CD 2352 (동기 패턴)"; return o; }
    if (iso) { o.fmt = F_ISO; o.note = "CD001"; return o; }
    if (!memcmp(p, "T98HDDIMAGE.R0\0", 15)) {
        o.fmt = F_NHD; o.hdd = true; o.hdr = rd32(p + 0x110); o.cyl = rd32(p + 0x114); o.heads = rd16(p + 0x118); o.sect = rd16(p + 0x11A); o.ssize = rd16(p + 0x11C);
        return o;
    }
    if (!memcmp(p, "T98FDDIMAGE.R0\0", 15)) { o.fmt = F_NFD0; o.hdr = rd32(p + 0x110); return o; }
    if (!memcmp(p, "T98FDDIMAGE.R1\0", 15)) { o.fmt = F_NFD1; return o; }
    if (!memcmp(p, "TD", 2) || !memcmp(p, "td", 2)) { if (p[2] != 0 || p[3] <= 0x30 || true) { if (p[4] >= 10 && p[4] <= 21) { o.fmt = F_TD0; return o; } } }
    if (vhd) { o.fmt = F_VHD; o.hdd = true; return o; }
    if (!memcmp(p, "DIFC HEADER", 11)) { o.fmt = F_DIM; return o; }
    // D88: 0x2B0 헤더, +0x1C = 디스크 전체 크기, +0x1B = 매체(0x00 2D, 0x10 2DD, 0x20 2HD, 0x30 1.2M, 0x40 1.44M)
    if (r.size > 0x2B0) {
        uint32_t dsz = rd32(p + 0x1C); uint8_t media = p[0x1B]; uint32_t t0 = rd32(p + 0x20);
        if (dsz <= r.size && dsz >= 0x2B0 && (media == 0x00 || media == 0x10 || media == 0x20 || media == 0x30 || media == 0x40) && t0 >= 0x2A0 && t0 < dsz && p[0x1A] != 0xFF) {
            // 트랙 표 0x20..0x2AF 는 전부 0 또는 [0x2B0, dsz) 안
            bool ok = true; for (int i = 0; i < 164 && ok; i++) { uint32_t t = rd32(p + 0x20 + i * 4); if (t != 0 && (t < 0x2B0 || t >= dsz)) ok = false; }
            if (ok) { o.fmt = F_D88; o.hdr = 0x2B0; o.note = media == 0x20 ? "2HD" : media == 0x10 ? "2DD" : media == 0x00 ? "2D" : "기타"; return o; }
        }
    }
    // FDI / HDI: 0x20 헤더 {dummy, type, headersize, size, sectorsize, sectors, surfaces, cylinders}
    {
        uint32_t hs = rd32(p + 8), sz = rd32(p + 12), ss = rd32(p + 16), sec = rd32(p + 20), sur = rd32(p + 24), cyl = rd32(p + 28);
        bool plaus = pow2(ss) && ss >= 128 && ss <= 1024 && hs >= 0x20 && hs <= 0x2000 && (hs % ss == 0 || hs % 256 == 0) && sec > 0 && sec <= 255 && sur > 0 && sur <= 16 && cyl > 0 && cyl <= 4096
                     && (uint64_t)hs + (uint64_t)sz <= r.size + 4096ull && sz > 0;
        if (plaus && (uint64_t)sec * sur * cyl * ss == sz) {
            o.hdr = hs; o.ssize = ss; o.sect = sec; o.heads = sur; o.cyl = cyl;
            // HDD 인가 FD 인가: 용량 > 2.88MB 이거나 섹터/면/실린더가 FD 범위 밖이면 HDD
            bool fd = (uint64_t)sz <= 2949120ull && cyl <= 85 && sur <= 2;
            o.fmt = fd ? F_FDI : F_HDI; o.hdd = !fd;
            // 확장자와 다르면 메모(DOSBox 는 확장자로 해석하므로 호스트는 확장자를 맞춰 열어야 함)
            return o;
        }
    }
    // 원시: 크기로 판정
    static const uint64_t fdsz[] = { 1261568ull, 1228800ull, 1474560ull, 737280ull, 655360ull, 327680ull, 368640ull, 163840ull, 184320ull, 1310720ull, 1638400ull };
    for (uint64_t s : fdsz) if (r.size == s || r.size == s + 1024) { o.fmt = F_RAWFD; if (s == 1261568ull) { o.cyl = 77; o.heads = 2; o.sect = 8; o.ssize = 1024; } else if (s == 1228800ull) { o.cyl = 80; o.heads = 2; o.sect = 15; o.ssize = 512; } else if (s == 1474560ull) { o.cyl = 80; o.heads = 2; o.sect = 18; o.ssize = 512; } else if (s == 737280ull) { o.cyl = 80; o.heads = 2; o.sect = 9; o.ssize = 512; } return o; }
    if (r.size >= 5ull * 1024 * 1024 && (r.size % 512) == 0) { o.fmt = F_RAWHD; o.hdd = true; o.ssize = 512; o.note = "크기만으로 추정"; return o; }
    o.fmt = F_UNKNOWN; return o;
}

inline ImgInfo probe(const std::string & path) {
    ImgInfo r = probe_raw(path);
    r.payload = ((r.fmt == F_FDI || r.fmt == F_HDI || r.fmt == F_NHD) && r.hdr > 0 && r.hdr < r.size) ? r.size - r.hdr : r.size;
    return r;
}

/* t40: FD(FDI/원시)의 FAT 루트에 IO.SYS / MSDOS.SYS 가 있으면 has_sys. 못 읽으면 조용히 false. */
inline void probe_sys(ImgInfo & r) {
    r.has_sys = false;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return;
    uint8_t b[512]; fseek(f, (long)r.hdr, SEEK_SET); if (fread(b, 1, 512, f) != 512) { fclose(f); return; }
    uint32_t bps = rd16(b + 0x0B), spc = b[0x0D], rsv = rd16(b + 0x0E), nfat = b[0x10], nroot = rd16(b + 0x11), spf = rd16(b + 0x16);
    if ((bps != 256 && bps != 512 && bps != 1024) || !pow2(spc) || nfat < 1 || nfat > 2 || nroot < 16 || nroot > 2048 || spf == 0 || spf > 64 || rsv == 0 || rsv > 16) { fclose(f); return; }
    uint64_t off = (uint64_t)r.hdr + (uint64_t)(rsv + nfat * spf) * bps;
    std::vector<uint8_t> d((size_t)nroot * 32); fseek(f, (long)off, SEEK_SET); size_t n = fread(&d[0], 1, d.size(), f); fclose(f);
    bool io = false, ms = false;
    for (size_t i = 0; i + 32 <= n; i += 32) {
        const uint8_t * e = &d[i]; if (e[0] == 0) break; if (e[0] == 0xE5) continue; uint8_t at = e[11]; if (at == 0x0F || (at & 0x18)) continue;
        if (!memcmp(e, "IO      SYS", 11)) io = true; else if (!memcmp(e, "MSDOS   SYS", 11)) ms = true;
    }
    r.has_sys = io || ms;
}

/* t43s (#369): 보조 — 이름 줄기(대문자), 도구(인터프리터·편집기류) 판별, D88 평탄화, FAT12 접근기(하위폴더) */
inline std::string stem_up(const std::string & n) { std::string u; for (char c : n) u += (char)toupper((unsigned char)c); size_t d = u.find_last_of('.'); return d == std::string::npos ? u : u.substr(0, d); }
inline bool is_tool_stem(const std::string & s) {   /* 인터프리터(BASIC98·N88 계열·GWBASIC)·BASIC 도구(B98*)·단순 도구 — 자동 실행 후보에서 뺀다 */
    static const char * const px[] = { "BASIC", "N88", "GWBAS", "B98", "MOUSE" };
    for (auto * k : px) { size_t n = std::string(k).size(); if (s.size() >= n && s.compare(0, n, k) == 0) return true; }
    static const char * const ex[] = { "XREF", "DEBUG", "MSD", "CMD" };
    for (auto * k : ex) if (s == k) return true;
    return false;
}
/* D88/D77 → 논리 순서(C,H,R) 로 이어 붙인 원시 FD 이미지 임시 파일. 실패하면 원래 ImgInfo 를 그대로 돌려준다 */
inline ImgInfo flat_img(const ImgInfo & r) {
    if (r.fmt != F_D88) return r;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return r; fseek(f, 0, SEEK_END); long n = ftell(f);
    if (n <= 0x2B0 || n > (8L << 20)) { fclose(f); return r; }
    std::vector<uint8_t> all((size_t)n); fseek(f, 0, SEEK_SET); size_t got = fread(&all[0], 1, all.size(), f); fclose(f); if (got != all.size()) return r;
    struct S { uint32_t key; size_t off, len; }; std::vector<S> v;
    for (int t = 0; t < 164; t++) {
        size_t q = rd32(&all[0x20 + t * 4]); if (!q || q + 16 > all.size()) continue; int ns = all[q + 4] | (all[q + 5] << 8);
        for (int i = 0; i < ns && q + 16 <= all.size(); i++) { size_t len = (size_t)(all[q + 14] | (all[q + 15] << 8)); if (q + 16 + len > all.size()) break; S s; s.key = ((uint32_t)all[q] << 16) | ((uint32_t)all[q + 1] << 8) | all[q + 2]; s.off = q + 16; s.len = len; v.push_back(s); q += 16 + len; }
    }
    if (v.empty()) return r;
    std::stable_sort(v.begin(), v.end(), [](const S & a, const S & b) { return a.key < b.key; });
    std::string tmp; { const char * e = getenv("TEMP"); tmp = e ? e : "."; } tmp += "\\pc98p_d88_" + std::to_string((unsigned long long)std::hash<std::string>()(r.path) ^ (unsigned long long)n) + ".raw";
    FILE * o = fopen(tmp.c_str(), "wb"); if (!o) return r; uint32_t last = 0xFFFFFFFFu;
    for (auto & s : v) { if (s.key == last) continue; last = s.key; fwrite(&all[s.off], 1, s.len, o); }   /* 같은 (C,H,R) 중복은 첫 것만 */
    fclose(o); ImgInfo q = r; q.path = tmp; q.fmt = F_RAWFD; q.hdr = 0; return q;
}
/* t45x α: BPB 검사에 실패한 PC-98 2HD(1261568B) 이미지를 FAT 머리·루트가 그럴듯할 때만 기본 지오메트리(1024/1/1/2/192/2)로 읽는다. 검사를 통과한 디스크는 이 함수를 부르지 않는다(후보 0 한정). */
inline bool geo_fb_core(const uint8_t * fh, const uint8_t * root, uint32_t & bps, uint32_t & spc, uint32_t & rsv, uint32_t & nfat, uint32_t & nroot, uint32_t & spf) {
    if (!(fh[0] >= 0xF0 && fh[1] == 0xFF && fh[2] == 0xFF)) return false; int ok = 0;
    for (size_t i = 0; i + 32 <= 6144; i += 32) { const uint8_t * e = root + i; if (e[0] == 0) break; if (e[0] == 0xE5) continue; for (int k = 0; k < 11; k++) if (e[k] < 0x20) return false; if (e[11] & 0xC0) return false; uint32_t sz = (uint32_t)e[28] | ((uint32_t)e[29] << 8) | ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24); if (sz > 1261568u) return false; ok++; }
    if (!ok) return false; bps = 1024; spc = 1; rsv = 1; nfat = 2; nroot = 192; spf = 2; return true;
}
inline bool geo_fb(FILE * f, long hdr, uint32_t & bps, uint32_t & spc, uint32_t & rsv, uint32_t & nfat, uint32_t & nroot, uint32_t & spf) {
    if (fseek(f, 0, SEEK_END) != 0) return false; long n = ftell(f); if (n - hdr != 1261568L) return false;
    std::vector<uint8_t> v(10240); if (fseek(f, hdr + 1024, SEEK_SET) != 0 || fread(&v[0], 1, v.size(), f) != v.size()) return false;
    return geo_fb_core(&v[0], &v[4096], bps, spc, rsv, nfat, nroot, spf);
}
inline bool geo_fb_mem(const std::vector<uint8_t> & img, uint32_t & bps, uint32_t & spc, uint32_t & rsv, uint32_t & nfat, uint32_t & nroot, uint32_t & spf) {
    if (img.size() != 1261568u) return false; return geo_fb_core(&img[1024], &img[5120], bps, spc, rsv, nfat, nroot, spf);
}
struct DirEnt { std::string name; uint32_t cl = 0, size = 0; bool dir = false; };
struct Fat12 {
    std::vector<uint8_t> img; uint32_t bps = 0, spc = 0, rsv = 0, nfat = 0, nroot = 0, spf = 0; uint64_t fat0 = 0, root0 = 0, data0 = 0;
    bool open(const ImgInfo & r0) {
        ImgInfo r = flat_img(r0); if (r.fmt != F_FDI && r.fmt != F_RAWFD) return false;
        FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return false; fseek(f, 0, SEEK_END); long n = ftell(f);
        if (n <= (long)r.hdr + 1024 || n - (long)r.hdr > (4L << 20)) { fclose(f); return false; }
        img.resize((size_t)(n - (long)r.hdr)); fseek(f, (long)r.hdr, SEEK_SET); size_t g = fread(&img[0], 1, img.size(), f); fclose(f); if (g != img.size()) return false;
        const uint8_t * b = &img[0]; bps = rd16(b + 0x0B); spc = b[0x0D]; rsv = rd16(b + 0x0E); nfat = b[0x10]; nroot = rd16(b + 0x11); spf = rd16(b + 0x16);
        if (((bps != 256 && bps != 512 && bps != 1024) || !pow2(spc) || nfat < 1 || nfat > 2 || nroot < 16 || nroot > 2048 || spf == 0 || spf > 64 || rsv == 0 || rsv > 16) && !geo_fb_mem(img, bps, spc, rsv, nfat, nroot, spf)) return false;
        fat0 = (uint64_t)rsv * bps; root0 = fat0 + (uint64_t)nfat * spf * bps; data0 = root0 + (((uint64_t)nroot * 32 + bps - 1) / bps) * bps; return data0 < img.size();
    }
    uint32_t next(uint32_t c) const { size_t o = (size_t)fat0 + c + c / 2; if (o + 1 >= img.size()) return 0xFFF; uint32_t w = img[o] | (img[o + 1] << 8); return (c & 1) ? (w >> 4) : (w & 0xFFF); }
    std::vector<DirEnt> parse(const uint8_t * d, size_t n) const {
        std::vector<DirEnt> out;
        for (size_t i = 0; i + 32 <= n; i += 32) {
            const uint8_t * e = d + i; if (e[0] == 0) break; if (e[0] == 0xE5 || e[11] == 0x0F || (e[11] & 0x08)) continue;
            std::string nm((const char *)e, 8), xt((const char *)e + 8, 3);
            for (auto & c : nm) if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7E) c = ' '; for (auto & c : xt) if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7E) c = ' ';
            while (!nm.empty() && nm.back() == ' ') nm.pop_back(); while (!xt.empty() && xt.back() == ' ') xt.pop_back();
            if (nm.empty() || nm == "." || nm == ".." || nm.find(' ') != std::string::npos || xt.find(' ') != std::string::npos) continue;
            DirEnt x; x.name = xt.empty() ? nm : nm + "." + xt; x.cl = rd16(e + 26); x.size = (uint32_t)e[28] | ((uint32_t)e[29] << 8) | ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24); x.dir = (e[11] & 0x10) != 0; out.push_back(x);
        }
        return out;
    }
    std::vector<DirEnt> root() const { size_t n = (size_t)nroot * 32; if ((size_t)root0 + n > img.size()) n = img.size() - (size_t)root0; return parse(&img[(size_t)root0], n); }
    std::vector<DirEnt> dir(uint32_t cl) const {
        std::vector<uint8_t> d; size_t cs = (size_t)spc * bps;
        for (int g = 0; cl >= 2 && cl < 0xFF0 && g < 64; g++) { size_t o = (size_t)(data0 + (uint64_t)(cl - 2) * cs); if (o + cs > img.size()) break; d.insert(d.end(), img.begin() + o, img.begin() + o + cs); cl = next(cl); }
        return d.empty() ? std::vector<DirEnt>() : parse(&d[0], d.size());
    }
};
inline bool is_exec_name(const std::string & n) { std::string u; for (char c : n) u += (char)toupper((unsigned char)c); size_t d = u.find_last_of('.'); if (d == std::string::npos) return false; std::string x = u.substr(d + 1); return (x == "EXE" || x == "COM" || x == "BAT") && u != "COMMAND.COM"; }
/* t43: FD(FDI/원시) FAT 루트의 실행 파일(EXE/COM/BAT, COMMAND.COM 제외) 이름들. 못 읽으면 빈 목록. */
inline std::vector<std::string> dos_runlist(const ImgInfo & r0) { ImgInfo r = flat_img(r0);
    std::vector<std::string> out; if (r.fmt != F_FDI && r.fmt != F_RAWFD) return out;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return out;
    uint8_t b[512]; fseek(f, (long)r.hdr, SEEK_SET); if (fread(b, 1, 512, f) != 512) { fclose(f); return out; }
    uint32_t bps = rd16(b + 0x0B), spc = b[0x0D], rsv = rd16(b + 0x0E), nfat = b[0x10], nroot = rd16(b + 0x11), spf = rd16(b + 0x16);
    if (((bps != 256 && bps != 512 && bps != 1024) || !pow2(spc) || nfat < 1 || nfat > 2 || nroot < 16 || nroot > 2048 || spf == 0 || spf > 64 || rsv == 0 || rsv > 16) && !geo_fb(f, (long)r.hdr, bps, spc, rsv, nfat, nroot, spf)) { fclose(f); return out; }
    uint64_t off = (uint64_t)r.hdr + (uint64_t)(rsv + nfat * spf) * bps;
    std::vector<uint8_t> d((size_t)nroot * 32); fseek(f, (long)off, SEEK_SET); size_t n = fread(&d[0], 1, d.size(), f); fclose(f);
    for (size_t i = 0; i + 32 <= n; i += 32) {
        const uint8_t * e = &d[i]; if (e[0] == 0) break; if (e[0] == 0xE5) continue; uint8_t at = e[11]; if (at == 0x0F || (at & 0x18)) continue;
        std::string nm((const char *)e, 8), xt((const char *)e + 8, 3);
        for (auto & c : nm) if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7E) c = ' '; for (auto & c : xt) if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7E) c = ' ';
        while (!nm.empty() && nm.back() == ' ') nm.pop_back(); while (!xt.empty() && xt.back() == ' ') xt.pop_back();
        if (nm.empty() || nm.find(' ') != std::string::npos || xt.find(' ') != std::string::npos) continue;
        if (xt != "EXE" && xt != "COM" && xt != "BAT") continue; if (nm == "COMMAND" && xt == "COM") continue;
        out.push_back(nm + "." + xt);
    }
    std::sort(out.begin(), out.end()); out.erase(std::unique(out.begin(), out.end()), out.end()); return out;
}

/* t43d (#330 C1): FD(FDI/원시) FAT12 루트에서 name(8.3 대문자) 파일의 앞 8KB. 못 읽으면 빈 문자열. */
inline std::string dos_readfile(const ImgInfo & r0, const std::string & name) { ImgInfo r = flat_img(r0);
    std::string out; if (r.fmt != F_FDI && r.fmt != F_RAWFD) return out;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return out;
    uint8_t b[512]; fseek(f, (long)r.hdr, SEEK_SET); if (fread(b, 1, 512, f) != 512) { fclose(f); return out; }
    uint32_t bps = rd16(b + 0x0B), spc = b[0x0D], rsv = rd16(b + 0x0E), nfat = b[0x10], nroot = rd16(b + 0x11), spf = rd16(b + 0x16);
    if (((bps != 256 && bps != 512 && bps != 1024) || !pow2(spc) || nfat < 1 || nfat > 2 || nroot < 16 || nroot > 2048 || spf == 0 || spf > 64 || rsv == 0 || rsv > 16) && !geo_fb(f, (long)r.hdr, bps, spc, rsv, nfat, nroot, spf)) { fclose(f); return out; }
    uint64_t base = (uint64_t)r.hdr, fat0 = base + (uint64_t)rsv * bps, root0 = fat0 + (uint64_t)nfat * spf * bps, data0 = root0 + (uint64_t)nroot * 32;
    data0 = (data0 + bps - 1) / bps * bps; data0 = root0 + (((uint64_t)nroot * 32 + bps - 1) / bps) * bps;
    std::vector<uint8_t> fat((size_t)spf * bps); fseek(f, (long)fat0, SEEK_SET); if (fread(&fat[0], 1, fat.size(), f) != fat.size()) { fclose(f); return out; }
    std::vector<uint8_t> d((size_t)nroot * 32); fseek(f, (long)root0, SEEK_SET); size_t n = fread(&d[0], 1, d.size(), f);
    uint32_t cl = 0, sz = 0; std::string want = name;
    for (size_t i = 0; i + 32 <= n; i += 32) {
        const uint8_t * e = &d[i]; if (e[0] == 0) break; if (e[0] == 0xE5 || (e[11] & 0x18) || e[11] == 0x0F) continue;
        std::string nm((const char *)e, 8), xt((const char *)e + 8, 3); while (!nm.empty() && nm.back() == ' ') nm.pop_back(); while (!xt.empty() && xt.back() == ' ') xt.pop_back();
        if (nm + "." + xt == want) { cl = rd16(e + 26); sz = (uint32_t)e[28] | ((uint32_t)e[29] << 8) | ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24); break; }
    }
    for (int guard = 0; cl >= 2 && cl < 0xFF0 && guard < 32 && out.size() < 8192 && out.size() < sz; guard++) {
        std::vector<char> c((size_t)spc * bps); fseek(f, (long)(data0 + (uint64_t)(cl - 2) * spc * bps), SEEK_SET); size_t k = fread(&c[0], 1, c.size(), f); out.append(&c[0], k);
        size_t o = cl + cl / 2; if (o + 1 >= fat.size()) break; uint32_t w = fat[o] | (fat[o + 1] << 8); cl = (cl & 1) ? (w >> 4) : (w & 0xFFF);
    }
    fclose(f); if (out.size() > sz) out.resize(sz); return out;
}
/* t43c (#330 C1): 설치 프로그램 이름(확장자 뺀 대문자) — 정확히 이 이름들만. *INST* 전부는 쓰지 않는다(PCing 같은 모음 디스크 오탐 방지) */
inline std::string stem_up(const std::string & n);
inline bool is_tool_stem(const std::string & s);
inline int inst_rank(const std::string & fn) {
    std::string u; for (char c : fn) u += (char)toupper((unsigned char)c); size_t d = u.find_last_of('.'); if (d != std::string::npos) u = u.substr(0, d);
    static const char * const nm[] = { "HDINST", "HDDINST", "HDINSTAL", "HDINSTALL", "HDSETUP", "INSTALL", "INSTAL", "INSTALLER", "SETUP", "INST", "HINST", "HDINSTALLER", "HDD_INST" };
    for (size_t i = 0; i < sizeof nm / sizeof nm[0]; i++) if (u == nm[i]) return (int)i + 1; return 0;
}
inline std::string inst_pick(const std::vector<std::string> & l) { std::string b; int br = 999; int real = 0; for (auto & x : l) { std::string s = stem_up(x); if (!inst_rank(x) && !is_tool_stem(s) && s != "AUTOEXEC") real++; } for (auto & x : l) { int r = inst_rank(x); if (!r || (r >= 9 && real > 0)) continue; if (r < br) { br = r; b = x; } } return b; }   /* t43s: 약한 이름은 다른 실행 후보가 없을 때만 설치기로 */   /* 9 = SETUP · 10 = INST: 약한 이름 */
/* 설치 대화 자동 응답(부팅 후 ms:키). 기본 = 설치 드라이브 「C」 → 기본 디렉터리 → 확인 Y. 설치기별 보정은 여기 표에 */
inline std::string inst_keys(const std::string &) { return "8000:type=y~,14000:type=y~,21000:type=y~"; }   /* t43d: 설치 대상 기본 = A:(HDD) 라 드라이브 입력은 필요 없다 — 확인(Y) 대화만 응답 */

/* t43n (#343): 루트 실행 후보(이름+크기) — dos_runlist 와 같은 걸음, 크기(바이트)를 같이 돌려준다 */
struct RunEnt { std::string name; uint32_t size = 0; };
inline std::vector<RunEnt> dos_runlist_sz(const ImgInfo & r0) { ImgInfo r = flat_img(r0);
    std::vector<RunEnt> out; if (r.fmt != F_FDI && r.fmt != F_RAWFD) return out;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return out;
    uint8_t b[512]; fseek(f, (long)r.hdr, SEEK_SET); if (fread(b, 1, 512, f) != 512) { fclose(f); return out; }
    uint32_t bps = rd16(b + 0x0B), spc = b[0x0D], rsv = rd16(b + 0x0E), nfat = b[0x10], nroot = rd16(b + 0x11), spf = rd16(b + 0x16);
    if (((bps != 256 && bps != 512 && bps != 1024) || !pow2(spc) || nfat < 1 || nfat > 2 || nroot < 16 || nroot > 2048 || spf == 0 || spf > 64 || rsv == 0 || rsv > 16) && !geo_fb(f, (long)r.hdr, bps, spc, rsv, nfat, nroot, spf)) { fclose(f); return out; }
    uint64_t off = (uint64_t)r.hdr + (uint64_t)(rsv + nfat * spf) * bps;
    std::vector<uint8_t> d((size_t)nroot * 32); fseek(f, (long)off, SEEK_SET); size_t n = fread(&d[0], 1, d.size(), f); fclose(f);
    for (size_t i = 0; i + 32 <= n; i += 32) {
        const uint8_t * e = &d[i]; if (e[0] == 0) break; if (e[0] == 0xE5) continue; uint8_t at = e[11]; if (at == 0x0F || (at & 0x18)) continue;
        std::string nm((const char *)e, 8), xt((const char *)e + 8, 3);
        for (auto & c : nm) if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7E) c = ' '; for (auto & c : xt) if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7E) c = ' ';
        while (!nm.empty() && nm.back() == ' ') nm.pop_back(); while (!xt.empty() && xt.back() == ' ') xt.pop_back();
        if (nm.empty() || nm.find(' ') != std::string::npos || xt.find(' ') != std::string::npos) continue;
        if (xt != "EXE" && xt != "COM" && xt != "BAT") continue; if (nm == "COMMAND" && xt == "COM") continue;
        RunEnt x; x.name = nm + "." + xt; x.size = (uint32_t)e[28] | ((uint32_t)e[29] << 8) | ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24); out.push_back(x);
    }
    return out;
}
/* t43n (#343): 루트 실행 후보가 여럿일 때 실행 파일을 고른다. 우선순위 = 디스크의 AUTOEXEC.BAT(원래 부팅 절차) > 로더 이름 BAT(GAME·START·RUN·GO·PLAY·MAIN·BOOT·<라벨>) > 설치기·유틸이 아닌 BAT 가 딱 1개 > 로더 이름 EXE/COM(같은 이름들) > 가장 큰 EXE(없으면 가장 큰 COM). 설치기·유틸·드라이버는 후보에서 뺀다. 못 고르면 빈 문자열(= 목록). */
/* 2b: FD(FAT12) 이미지 안 폴더 보기. dir = "" 이면 루트, 아니면 "A\B". 목록 = 하위 폴더 + 실행 파일(EXE/COM/BAT, COMMAND.COM 제외). */
inline std::string up_s(std::string s) { for (auto & c : s) c = (char)toupper((unsigned char)c); return s; }
inline bool dos_walk(const Fat12 & fx, const std::string & dir, uint32_t & cl_out) {
    uint32_t cl = 0; std::string cur;
    for (size_t i = 0; i <= dir.size(); i++) {
        if (i == dir.size() || dir[i] == (char)92 || dir[i] == (char)47) {
            if (!cur.empty()) { std::vector<DirEnt> v = cl ? fx.dir(cl) : fx.root(); bool ok = false; for (auto & e : v) if (e.dir && up_s(e.name) == up_s(cur)) { cl = e.cl; ok = true; break; } if (!ok) return false; cur.clear(); }
        } else cur += dir[i];
    }
    cl_out = cl; return true;
}
inline bool dos_browse(const std::string & path, const std::string & dir, std::vector<DirEnt> & out) {
    out.clear(); Fat12 fx; if (!fx.open(probe(path))) return false; uint32_t cl = 0; if (!dos_walk(fx, dir, cl)) return false;
    std::vector<DirEnt> v = cl ? fx.dir(cl) : fx.root();
    for (auto & e : v) if (e.dir || (is_exec_name(e.name) && up_s(e.name) != "COMMAND.COM")) out.push_back(e);
    std::stable_sort(out.begin(), out.end(), [](const DirEnt & a, const DirEnt & b) { if (a.dir != b.dir) return a.dir; return up_s(a.name) < up_s(b.name); });
    return true;
}
/* file = "폴더\이름.EXE" (대소문자 무시). 이미지를 못 읽으면 false. */
inline bool dos_exists(const std::string & path, const std::string & file) {
    size_t p = file.find_last_of("\\/"); std::string d = p == std::string::npos ? std::string() : file.substr(0, p), n = p == std::string::npos ? file : file.substr(p + 1);
    std::vector<DirEnt> v; if (!dos_browse(path, d, v)) return false;
    for (auto & e : v) if (!e.dir && up_s(e.name) == up_s(n)) return true; return false;
}
/* t43p (#344-3): LHA 자기풀림(SFX) EXE 판별 — 앞부분에 LHa 문구 + -lh 헤더 */
inline bool is_lha_sfx(const std::string & t) { if (t.size() < 1000) return false; size_t n = t.size() < 200000 ? t.size() : 200000; std::string h = t.substr(0, n); bool lh = h.find("-lh") != std::string::npos; bool nm = h.find("SFX") != std::string::npos || h.find("LHa") != std::string::npos || h.find("LHA") != std::string::npos || h.find("lha") != std::string::npos; return (lh && nm) || h.find("LHa's SFX") != std::string::npos || h.find("LHA's SFX") != std::string::npos; }
inline std::string dos_pick(const std::vector<RunEnt> & in, const std::string & label) {
    auto up = [](std::string s) { for (auto & c : s) c = (char)toupper((unsigned char)c); return s; };
    auto stem = [&](const std::string & s) { size_t d = s.find_last_of('.'); return up(d == std::string::npos ? s : s.substr(0, d)); };
    auto ext = [&](const std::string & s) { size_t d = s.find_last_of('.'); return d == std::string::npos ? std::string() : up(s.substr(d + 1)); };
    static const char * const util[] = { "COMMAND", "FORMAT", "DISKCOPY", "CHKDSK", "XCOPY", "LHA", "LHARC", "LHX", "LHEXT", "LHZ", "ARC", "PKUNZIP", "PKZIP", "UNLHA", "MSCDEX", "EDIT", "DEBUG", "FDISK", "MOUSE", "FMDRV", "FILES", "README", "DOC", "READ", "HELP", "COPY", "DISKCOMP", "SYS", "UNINST", "UNINSTAL", "DELETE", "BACKUP", "RESTORE", "MENU" };
    static const char * const ldr[] = { "GAME", "START", "RUN", "GO", "PLAY", "MAIN", "BOOT" };
    std::vector<RunEnt> c; bool autoexec = false;
    for (auto & x : in) { std::string s = stem(x.name), e = ext(x.name); if (s == "AUTOEXEC" && e == "BAT") { autoexec = true; continue; } if (inst_rank(x.name)) continue; if (s.compare(0, 4, "INST") == 0 || s.compare(0, 5, "SETUP") == 0 || s.compare(0, 5, "HDINS") == 0 || s.compare(0, 6, "DOSINS") == 0) continue; bool u = false;   /* t43o: 설치기 계열 접두어(INST·SETUP·HDINS·DOSINS)도 후보에서 뺀다 */ for (auto * k : util) if (s == k) u = true; if (u || is_tool_stem(s)) continue; c.push_back(x); }   /* t43s: 인터프리터·도구 */
    if (autoexec) return "AUTOEXEC.BAT";
    std::string lb = up(label);
    auto isldr = [&](const std::string & s) { for (auto * k : ldr) if (s == k) return true; if (s == "SYSTEM" || s == "DISK" || s == "DATA" || s == "USER" || s == "SYS") return false; if (s.size() >= 4 && lb.find(s) != std::string::npos) return true; return false; };   /* t43o: 디스크 이름에 흔한 낱말(System disk 등)로는 라벨 일치 판정하지 않는다 */
    std::vector<RunEnt> bats; for (auto & x : c) if (ext(x.name) == "BAT") bats.push_back(x);
    for (auto & x : bats) if (isldr(stem(x.name))) return x.name;
    if (bats.size() == 1) return bats[0].name;
    for (auto & x : c) if (isldr(stem(x.name))) return x.name;
    const RunEnt * best = NULL; for (auto & x : c) if (ext(x.name) == "EXE" && (!best || x.size > best->size)) best = &x;
    if (!best) for (auto & x : c) if (ext(x.name) == "COM" && (!best || x.size > best->size)) best = &x;
    return best ? best->name : std::string();
}

/* 첫 섹터 읽기로 부팅 후보·이름 추정 (FD: 섹터크기 256/1024 가능, HD: IPL) */
inline void probe_boot(ImgInfo & r) {
    if (!(fmt_supported(r.fmt))) return;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return;
    if (r.fmt == F_NFD0 || r.fmt == F_NFD1) { fclose(f); r.boot_ok = true; return; }   // NFD: 섹터 표를 따라가야 첫 섹터가 나온다 — 부팅 가능으로 보고 라벨은 비움
    long off0 = (long)r.hdr;
    if (r.fmt == F_D88) { uint8_t t[4] = {0}; fseek(f, 0x20, SEEK_SET); if (fread(t, 1, 4, f) != 4) { fclose(f); return; } off0 = (long)rd32(t) + 16; }   // 첫 트랙 첫 섹터 데이터(섹터 머리 16바이트 뒤)
    std::vector<uint8_t> b(1024, 0); fseek(f, off0, SEEK_SET); size_t n = fread(&b[0], 1, b.size(), f); fclose(f);
    bool nz = false; for (size_t i = 0; i < n && i < 512; i++) if (b[i]) { nz = true; break; }
    bool ff256 = n >= 16; for (size_t i = 0; i < n && i < 256; i++) if (b[i] != 0xFF) { ff256 = false; break; }   /* t43i: 첫 섹터 앞 256바이트가 전부 FF = 포맷만 된 데이터 디스크(부팅 불가) */
    r.boot_ok = nz && !ff256;
    if (r.fmt == F_FDI || r.fmt == F_RAWFD || r.fmt == F_D88) {   /* t43: 부트섹터 문구(대소문자 무시) */
        std::string t; for (size_t i = 0; i < n && i < 512; i++) { char c = (char)b[i]; t += (c >= 0x20 && c < 0x7F) ? (char)tolower((unsigned char)c) : ' '; }
        r.nosys_boot = t.find("no system") != std::string::npos || t.find("non-system") != std::string::npos || t.find("data disk") != std::string::npos;
    }
    if (n >= 8 && !memcmp(&b[4], "IPL1", 4)) r.ipl = true;
    if (r.fmt == F_FDI || r.fmt == F_RAWFD) probe_sys(r);   /* t40: 시스템 디스크 판별(부팅 디스크 선택 근거) */
    // 라벨: 인쇄 가능한 ASCII 연속 구간 중 가장 긴 것
    std::string best, cur;
    for (size_t i = 0; i < n; i++) { if (b[i] >= 0x20 && b[i] < 0x7F) cur += (char)b[i]; else { if (cur.size() > best.size()) best = cur; cur.clear(); } }
    if (cur.size() > best.size()) best = cur;
    if (best.size() >= 4) r.label = best.substr(0, 40);
}

/* r017: 변형 판정의 증거 — 플로피(FDI/원시)의 FAT 루트에서 볼륨 라벨·실행 파일 이름을 읽는다. 못 읽으면 조용히 빈 값. */
inline void fat_sig(ImgInfo & r) {
    r.vlabel.clear(); r.exes.clear();
    if (r.fmt != F_FDI && r.fmt != F_RAWFD) return;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return;
    uint8_t b[512]; fseek(f, (long)r.hdr, SEEK_SET); if (fread(b, 1, 512, f) != 512) { fclose(f); return; }
    uint32_t bps = rd16(b + 0x0B), spc = b[0x0D], rsv = rd16(b + 0x0E), nfat = b[0x10], nroot = rd16(b + 0x11), spf = rd16(b + 0x16);
    if ((bps != 256 && bps != 512 && bps != 1024) || !pow2(spc) || nfat < 1 || nfat > 2 || nroot < 16 || nroot > 2048 || spf == 0 || spf > 64 || rsv == 0 || rsv > 16) { fclose(f); return; }
    uint64_t off = (uint64_t)r.hdr + (uint64_t)(rsv + nfat * spf) * bps;
    std::vector<uint8_t> d((size_t)nroot * 32); fseek(f, (long)off, SEEK_SET); size_t n = fread(&d[0], 1, d.size(), f); fclose(f);
    std::vector<std::string> ex;
    for (size_t i = 0; i + 32 <= n; i += 32) {
        const uint8_t * e = &d[i]; if (e[0] == 0) break; if (e[0] == 0xE5) continue; uint8_t at = e[11]; if (at == 0x0F) continue;
        std::string nm((const char *)e, 8), xt((const char *)e + 8, 3);
        for (auto & c : nm) if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) c = '?'; for (auto & c : xt) if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) c = '?';
        while (!nm.empty() && nm.back() == ' ') nm.pop_back(); while (!xt.empty() && xt.back() == ' ') xt.pop_back();
        if (at & 0x08) { r.vlabel = nm + xt; while (!r.vlabel.empty() && r.vlabel.back() == ' ') r.vlabel.pop_back(); continue; }
        if (at & 0x10) continue;
        std::string full = xt.empty() ? nm : nm + "." + xt;
        if (xt == "EXE" || xt == "COM" || xt == "BAT" || xt == "SYS") ex.push_back(full);
    }
    std::sort(ex.begin(), ex.end()); for (size_t i = 0; i < ex.size() && i < 24; i++) { if (i) r.exes += ","; r.exes += ex[i]; }
}

/* ---------------------------------------------------------------- 프로필 */
struct Profile {
    std::string id, title, engine, src;     // src = "자동" | "game.ini" | "DB"
    std::vector<std::string> disk;           // disk[0..3] = A,B,C,D 에 붙일 파일(상대 이름), 빈칸 = 없음
    char bootletter = 'A';
    std::vector<std::string> swap;           // 교체 목록
    std::map<std::string, std::string> kv;   // cycles, font, extra, bgm, se, shader, ratio, curve, scan, mask, pad.* , never_map ...
    std::string reason;                      // 자동 판정 근거(한 줄)
    std::vector<std::string> ignore;
    std::map<std::string, std::string> ov, pad;   // 전역 위에 얹는 작품별 값 / 패드 표
    bool install_required = false;
    std::string note;
    std::string grade, judge, metasrc, block;   // r017: 판정 등급(A/B/C/-) · 판정 문장 · 메타 출처 · 실행 불가 사유(빈칸 = 실행 가능)
    std::string dbhit_id, dbhit_title;          // r018: 게임 DB(sha1) 와 일치한 작품 (game.ini 로 직접 정했어도 같이 기록)
    Profile() : disk(4) {}
};

inline bool has_ci(const std::string & s, const char * k) { std::string a = s, b = k; for (auto & c : a) c = (char)tolower((unsigned char)c); for (auto & c : b) c = (char)tolower((unsigned char)c); return a.find(b) != std::string::npos; }

/* 디스크 중요도 점수: 시스템/부팅/Disk A/1 이 앞 */
inline int sys_score(const ImgInfo & i) {
    int s = 0; const std::string & n = i.base;
    if (has_ci(n, "system") || has_ci(n, "boot") || has_ci(n, "sys")) s += 50;
    if (has_ci(n, "disk a") || has_ci(n, "disk_a") || has_ci(n, "disk-a") || has_ci(n, "(disk a)") || has_ci(n, "disk 1") || has_ci(n, "disk1") || has_ci(n, "disk.1")) s += 40;
    if (has_ci(n, "alt 1")) s += 5;
    if (has_ci(n, "game")) s += 30;
    if (has_ci(n, "data") || has_ci(n, "user")) s -= 20;
    if (has_ci(n, "demo") || has_ci(n, "option")) s -= 25;
    if (has_ci(n, "alt 2") || has_ci(n, "alt 3")) s -= 5;
    if (i.boot_ok) s += 10; else s -= 60;   /* t43i: 첫 섹터가 비었거나 전부 FF 인 디스크는 A(부팅)로 뽑지 않는다 */
    if (has_ci(n, "400-line") || has_ci(n, "400 line")) s -= 15;   /* t43l (#334): 일반판(200줄) 우선, 400-line 변형은 후순위 */
    if (i.ipl) s += 20;
    if (i.has_sys) s += 45;   /* t40: DOS 시스템 파일이 든 디스크는 부팅 디스크 */
    return s;
}

/* t40: 같은 디스크의 중복(형식 중복 d88/nfd, [Alt N] 덤프)을 한 장으로 묶는다. 키 = 확장자·[Alt N] 을 뺀 소문자 이름. */
inline std::string lower_s(const std::string & s) { std::string o = s; for (auto & c : o) c = (char)tolower((unsigned char)c); return o; }
inline std::string ext_of(const std::string & base) { size_t d = base.find_last_of('.'); return d == std::string::npos ? std::string() : lower_s(base.substr(d)); }
inline std::string stem_s(const std::string & base) { size_t d = base.find_last_of('.'); return d == std::string::npos ? base : base.substr(0, d); }
/* [Alt N] 번호(없으면 0) 와 그것을 뺀 이름 */
inline std::string strip_alt(const std::string & stem, int * altno) {
    std::string l = lower_s(stem), o; if (altno) *altno = 0; size_t i = 0;
    while (i < stem.size()) {
        if (l.compare(i, 5, "[alt ") == 0) { size_t e = l.find(']', i); if (e != std::string::npos) { if (altno) *altno = atoi(l.c_str() + i + 5); i = e + 1; while (!o.empty() && o.back() == ' ' && (i >= stem.size() || stem[i] == ' ' || stem[i] == '.')) o.pop_back(); continue; } }
        o += l[i]; i++;
    }
    std::string r; bool sp = false; for (char c : o) { if (c == ' ') { if (!sp && !r.empty()) r += c; sp = true; } else { r += c; sp = false; } }
    while (!r.empty() && r.back() == ' ') r.pop_back(); return r;
}
inline int fmt_pri(Fmt f) { switch (f) { case F_RAWFD: return 0; case F_D88: return 1; case F_FDI: return 2; case F_NFD1: return 3; case F_NFD0: return 4; default: return 5; } }   /* 형식 우선순위: 원시 > D88 > FDI > NFD */
/* 「(Disk X)」 의 X — 같은 번호의 다른 디스크(설치 전/후 판 등)를 A 와 B 로 따로 잡지 않기 위한 표 */
inline std::string disk_no(const std::string & base) {
    std::string l = lower_s(base); size_t p = l.find("(disk "); if (p == std::string::npos) return "";
    size_t e = l.find(')', p); if (e == std::string::npos) return ""; std::string r = l.substr(p + 6, e - p - 6); size_t sp = r.find(' '); if (sp != std::string::npos) r = r.substr(0, sp);
    if (r.size() == 1 && r[0] >= 'a' && r[0] <= 'z') r = std::to_string(r[0] - 'a' + 1);   /* Disk A = Disk 1 */
    return r;
}
/* 같은 키·같은 내용을 한 장으로: 남길 것 = [Alt] 없는 것 > 번호 작은 Alt, 그다음 형식 우선순위, 이름순. dropped 에 버려진 개수. */
inline std::vector<ImgInfo *> collapse_dups(std::vector<ImgInfo *> v, int & dropped) {
    struct K { std::string key; int alt, pri; ImgInfo * p; };
    std::vector<K> ks; for (auto * p : v) { K k; int a = 0; k.key = strip_alt(stem_s(p->base), &a); k.alt = a; k.pri = fmt_pri(p->fmt); k.p = p; ks.push_back(k); }
    std::stable_sort(ks.begin(), ks.end(), [](const K & a, const K & b) { if (a.key != b.key) return a.key < b.key; if (a.alt != b.alt) return a.alt < b.alt; if (a.pri != b.pri) return a.pri < b.pri; return a.p->base < b.p->base; });
    std::vector<ImgInfo *> out; std::set<std::string> seen; dropped = 0;
    for (auto & k : ks) {
        if (seen.count(k.key)) { dropped++; continue; }
        std::string sh = k.p->sha1.empty() ? std::string() : k.p->sha1 + ":" + std::to_string(k.p->size);
        if (!sh.empty()) { bool dup = false; for (auto * o : out) if (!o->sha1.empty() && o->sha1 == k.p->sha1 && o->size == k.p->size) { dup = true; break; } if (dup) { dropped++; seen.insert(k.key); continue; } }
        seen.insert(k.key); out.push_back(k.p);
    }
    return out;
}
inline int cd_pri(const ImgInfo & i) { std::string e = ext_of(i.base); if (e == ".iso") return 0; if (e == ".cue") return 1; if (e == ".img") return 2; if (e == ".mdf") return 3; if (e == ".bin") return 4; if (e == ".ccd") return 5; return 6; }

/* 자동 프로필: 폴더(또는 파일 목록)의 이미지에서 결정.
   규칙(t40): 같은 디스크의 형식 중복·[Alt N] 은 한 장으로 묶는다(형식 우선순위 원시>D88>FDI>NFD).
   HDD 가 있으면 HDD(C) 부팅. 없으면 FD 중 부팅 점수 최고(DOS 시스템 파일 든 디스크 우대) = A, 같은 번호 디스크가 아닌 다음 것 = B, 나머지 = 교체 목록.
   .ccd/.cue 와 같은 줄기의 .img/.bin 은 CD. FD·HDD 없이 CD 만 있으면 cd= 로 잡는다(내장 DOS 에서 D:).
   미지원 형식만 있으면 reason 에 사유를 적고 disk 비움. */
inline Profile auto_profile(std::vector<ImgInfo> imgs, const std::string & id) {
    Profile p; p.id = id; p.src = "자동";
    {   /* t40: CD 시트(.ccd/.cue) 가 있으면 같은 줄기(.cue 는 모든 .bin/.img)의 데이터 이미지는 CD 로 */
        bool has_cue = false; std::set<std::string> ccd;
        for (auto & i : imgs) { std::string e = ext_of(i.base); if (e == ".cue" && i.sheet) has_cue = true; if (e == ".ccd" && i.sheet) ccd.insert(lower_s(stem_s(i.base))); }
        if (has_cue || !ccd.empty()) for (auto & i : imgs) {
            if (i.sheet) continue; std::string e = ext_of(i.base);
            if ((e == ".img" || e == ".bin" || e == ".iso" || e == ".mdf") && (has_cue || ccd.count(lower_s(stem_s(i.base))))) { i.fmt = F_ISO; i.hdd = false; if (i.note.empty()) i.note = "CD 데이터(시트와 같은 줄기)"; }
        }
    }
    std::vector<ImgInfo *> hd, fd, cd, bad;
    for (auto & i : imgs) {
        if (i.fmt == F_ISO) { cd.push_back(&i); continue; }
        if (!fmt_supported(i.fmt)) { bad.push_back(&i); continue; }
        (i.hdd ? hd : fd).push_back(&i);
    }
    int dh = 0, df = 0; hd = collapse_dups(hd, dh); fd = collapse_dups(fd, df);
    std::string dupnote = (dh + df) ? " · " + khtr::fmt(khtr::tr("중복 %d장 묶음"), dh + df) : std::string();
    if (!hd.empty()) {
        // 가장 큰 HDD 하나(여러 개면 이름순 첫 번째) 를 C 로, 둘째는 D
        std::sort(hd.begin(), hd.end(), [](ImgInfo * a, ImgInfo * b) { return a->base < b->base; });
        p.disk[2] = hd[0]->base; if (hd.size() > 1) p.disk[3] = hd[1]->base;
        p.bootletter = 'C';
        p.reason = std::string(khtr::tr("HDD 있음 → C 부팅")) + dupnote;
        if (!fd.empty()) {   /* 2c: HDD+FD auto bundle: best FD -> A only, all FDs -> swap list, boot stays C */
            auto sc = [](ImgInfo * a, ImgInfo * b) { int x = sys_score(*a), y = sys_score(*b); return x != y ? x > y : a->base < b->base; };
            std::sort(fd.begin(), fd.end(), sc); p.disk[0] = fd[0]->base;
            if (fd.size() > 1) for (auto * x : fd) p.swap.push_back(x->base);
            p.reason += " + " + khtr::fmt(fd.size() > 1 ? khtr::tr("FD %d개 (A 부착, 전부 교체 목록)") : khtr::tr("FD %d개 (A 부착)"), (int)fd.size());
        }
        return p;
    }
    if (!fd.empty()) {
        { bool anyboot = false; for (auto * x : fd) if (x->boot_ok) anyboot = true;   /* t43j (#323 C4): 첫 섹터가 비었거나 전부 FF 인 디스크뿐 = 추가 시나리오·데이터 디스크만 든 세트 — 부팅시키지 않고 사유를 보인다 */
          if (!anyboot) { p.reason = std::string(khtr::tr("부팅할 수 있는 디스크가 없음 — 첫 섹터가 비어 있거나 전부 FF 인 데이터 디스크뿐 (본편 디스크가 필요한 추가 디스크)")) + dupnote; return p; } }
        std::sort(fd.begin(), fd.end(), [](ImgInfo * a, ImgInfo * b) { int sa = sys_score(*a), sb = sys_score(*b); if (sa != sb) return sa > sb; return a->base < b->base; });
        /* B = A 와 같은 번호가 아닌 디스크 중 점수 최고. 같은 번호(설치 전/후 등)는 뒤로 */
        std::string an = disk_no(fd[0]->base);
        if (!an.empty() && fd.size() > 2) std::stable_partition(fd.begin() + 1, fd.end(), [&](ImgInfo * x) { return disk_no(x->base) != an; });
        if (fd.size() > 2 && !an.empty()) { int ai = atoi(an.c_str()); if (ai > 0) for (size_t k = 1; k < fd.size(); k++) if (atoi(disk_no(fd[k]->base).c_str()) == ai + 1) { std::rotate(fd.begin() + 1, fd.begin() + k, fd.begin() + k + 1); break; } }   /* t43d (#323 C2): 2번 디스크 = B */
        p.disk[0] = fd[0]->base; p.bootletter = 'A';
        if (fd.size() >= 2) p.disk[1] = fd[1]->base;
        for (size_t k = 2; k < fd.size(); k++) p.swap.push_back(fd[k]->base);
        p.reason = khtr::fmt(khtr::tr("FD %d개 → 부팅 점수 최고 = A"), (int)fd.size()) + (fd[0]->has_sys ? khtr::tr("(DOS 시스템 디스크)") : "") + (fd.size() >= 2 ? std::string(", ") + khtr::tr("다음 = B") : std::string()) + (fd.size() > 2 ? std::string(", ") + khtr::tr("나머지 교체 목록") : std::string()) + dupnote;
        if (fd[0]->nosys_boot) {   /* t43 (#315 A1): 부트섹터가 「시스템 없음」 — 이 디스크는 스스로 부팅하지 못한다 → 내장 DOS 에 A:/B: 로 장착하고 실행 파일을 찾는다 */
            std::vector<std::string> ra = dos_runlist(*fd[0]), rb; if (fd.size() >= 2) rb = dos_runlist(*fd[1]);
            if (ra.empty() && rb.empty() && fd.size() > 2) {   /* t45x β: A·B 후보 0 이면 교체 목록(swap)에서 실행 파일이 든 첫 디스크를 B 로 올린다(정렬 순서 첫째) */
                for (size_t k = 2; k < fd.size(); k++) { std::vector<std::string> rk = dos_runlist(*fd[k]); if (rk.empty()) continue; std::swap(fd[1], fd[k]); p.disk[1] = fd[1]->base; p.swap.clear(); for (size_t m = 2; m < fd.size(); m++) p.swap.push_back(fd[m]->base); rb = rk; p.reason += " → " + std::string(khtr::tr("swap 에서 실행 디스크 승격")) + ": " + fd[1]->base + "(t45x β)"; break; } }
            const std::vector<std::string> & use = !ra.empty() ? ra : rb; std::string j; for (auto & x : use) { if (!j.empty()) j += ","; j += x; }
            p.kv["dosfd"] = "1"; p.kv["dosdrv"] = fd.size() >= 2 && ra.empty() ? "b" : "a"; p.kv["dosrun"] = j;
            {   /* t43n (#343): 실행 후보가 여럿이면 선택 규칙으로 하나 고른다 — dosrunsz = 이름:크기 목록, dospick = 고른 이름 */
                const ImgInfo & ud = (!ra.empty() || fd.size() < 2) ? *fd[0] : *fd[1];   /* t43t: FD 1장이면 fd[1] 이 없다(범위 밖 접근 = 프로필 생성 중 크래시, rc=5) */ std::vector<RunEnt> ents = dos_runlist_sz(ud); std::string sz; for (auto & x : ents) { if (!sz.empty()) sz += ","; sz += x.name + ":" + std::to_string(x.size); }
                p.kv["dosrunsz"] = sz; if (ents.size() > 1) { std::string pk = dos_pick(ents, ud.base); if (!pk.empty()) p.kv["dospick"] = pk; } }
            if (ra.empty() && rb.empty()) {   /* t43s R2: 루트에 실행이 없고 하위폴더(3개 이하)가 하나만 후보를 가지면 그 폴더로 들어가 실행. 모음 디스크(폴더 4개 이상)는 목록 유지 */
                for (size_t k = 0; k < fd.size() && k < 2; k++) {
                    Fat12 fx; if (!fx.open(*fd[k])) continue; std::vector<DirEnt> rt = fx.root(), dirs; for (auto & e : rt) if (e.dir) dirs.push_back(e);
                    if (dirs.empty() || dirs.size() >= 4) continue; std::string bd, bp; int cnt = 0;
                    for (auto & d : dirs) { std::vector<RunEnt> es; for (auto & e : fx.dir(d.cl)) if (!e.dir && is_exec_name(e.name)) { RunEnt x; x.name = e.name; x.size = e.size; es.push_back(x); }
                        if (es.empty()) continue; std::string pk = dos_pick(es, fd[k]->base); if (pk.empty()) continue; cnt++; bd = d.name; bp = pk; }
                    if (cnt == 1) { p.kv["dosdrv"] = k == 0 ? "a" : "b"; p.kv["dosrun"] = bp; p.kv["dossub"] = bd; p.kv["dospick"] = bp; p.kv["dosrunsz"] = bd + "\\" + bp; j = bp; p.reason += " → " + khtr::fmt(khtr::tr("하위폴더 %s 자동 실행(t43s R2)"), (bd + "\\" + bp).c_str()); break; }
                }
                if (!p.kv.count("dossub")) { std::string ia0 = inst_pick(ra), ib0 = inst_pick(rb); if (ia0.empty() && ib0.empty() && !p.kv.count("doshdd")) p.kv["dosdata"] = "1"; }   /* t43s R5: 실행도 설치기도 없음 = 데이터 전용 가능성 */
            }
            {   /* t43p (#344-3): LHA 자기풀림 EXE 는 FD 에 쓰지 못해 "Write Error A:" 로 멈춘다 → 작품별 호스트 폴더(HDD)를 A: 로 두고 거기서 풀게 한다. 설치기가 있으면 설치기가 우선 */
                if (!p.kv.count("doshdd")) { std::vector<std::string> sx; int other = 0;   /* t43q: 후보가 전부 LHA 자기풀림 EXE 일 때만(모음 디스크·게임 EXE 섞인 디스크·LHA 도구는 제외) */
                    for (size_t k = 0; k < fd.size(); k++) { std::vector<RunEnt> es = dos_runlist_sz(*fd[k]); for (auto & e : es) { std::string u = e.name; for (auto & c : u) c = (char)toupper((unsigned char)c); if (u == "AUTOEXEC.BAT" || u == "CONFIG.SYS") continue; if (inst_rank(u) || u.compare(0, 3, "LHA") == 0 || u.compare(0, 5, "UNLHA") == 0 || u.compare(0, 3, "LHX") == 0 || u.compare(0, 4, "LHEX") == 0 || u.compare(0, 4, "INST") == 0 || u.compare(0, 5, "SETUP") == 0) continue;
                            if (u.size() > 4 && u.substr(u.size() - 4) == ".EXE" && is_lha_sfx(dos_readfile(*fd[k], u))) sx.push_back(k < 2 ? std::string(k == 0 ? "b" : "c") + ":\\" + u : "#" + std::to_string(k) + "|" + u); else if (k < 2) other++; } }
                    if (!sx.empty() && other == 0) { std::string j; for (auto & x : sx) { if (!j.empty()) j += ","; j += x; } p.kv["doshdd"] = "1"; p.kv["dossfx"] = j; p.kv.erase("dospick"); } } }
            {   /* t43c (#330 C1): 설치 프로그램이 있으면 C: = 작품별 호스트 폴더로 두고 설치기를 먼저 돌린다(설치 끝난 폴더는 실행 파일 탐지) */
                std::string ia = inst_pick(ra), ib = inst_pick(rb);
                if (!ia.empty() || !ib.empty()) { p.kv["doshdd"] = "1"; p.kv["dosinst"] = !ia.empty() ? ia : ib; p.kv["dosinstdrv"] = !ia.empty() ? "a" : "b"; p.kv["bootkeys"] = inst_keys(p.kv["dosinst"]);
                    {   /* t43d: BAT 설치기가 %1·%2 를 쓰면 인자를 준다 — 둘이면 「설치기 FD 드라이브 C」, 하나면 「C」 (설치 대상 = C:) */
                        const std::string & in = p.kv["dosinst"]; const ImgInfo & src = (!ia.empty() || fd.size() < 2) ? *fd[0] : *fd[1];
                        if (in.size() > 4 && (in.substr(in.size() - 4) == ".EXE" || in.substr(in.size() - 4) == ".COM") && in.compare(0, 7, "INSTALL") == 0) { p.kv["dosinstargs"] = "@H:"; p.kv["bootkeys"] = "9000:type=y~,16000:type=y~"; }   /* t43f: INSTALL.EXE 류는 「INSTALL A:」(설치 대상 드라이브)가 사용법 */
                        else if (in.size() > 4 && in.substr(in.size() - 4) == ".BAT") { std::string t = dos_readfile(src, in); std::string tn; for (char c : t) if (c != ' ') tn += c; bool a2 = (t.find("%2") != std::string::npos && t.find("\"%2\"") == std::string::npos) || tn.find("\"%2\"==\"\"") != std::string::npos, a1 = t.find("%1") != std::string::npos;
                            if (a2) { p.kv["dosinstargs"] = "@F @H"; p.kv["bootkeys"] = "9000:type=y~,16000:type=y~"; } else if (a1) { p.kv["dosinstargs"] = "@H:"; p.kv["bootkeys"] = "9000:type=y~,16000:type=y~"; } }
                    } }
            }
            p.reason += " → " + std::string(khtr::tr("부트섹터에 DOS 시스템 없음: 내장 DOS 에 A: 장착")) + (j.empty() ? khtr::tr("(실행 파일 못 찾음 — 목록만)") : (use.size() == 1 ? khtr::tr("(실행 1개: 자동 실행)") : khtr::tr("(실행 여럿: 목록)")));
        }
        return p;
    }
    if (!cd.empty()) {
        std::sort(cd.begin(), cd.end(), [](ImgInfo * a, ImgInfo * b) { int pa = cd_pri(*a), pb = cd_pri(*b); if (pa != pb) return pa < pb; return a->base < b->base; });
        p.kv["cd"] = cd[0]->base; p.install_required = false;
        p.reason = std::string(khtr::tr("CD 이미지 → cd= 로 잡음(내장 DOS 의 D:)")) + ": " + cd[0]->base;
        return p;
    }
    if (!bad.empty()) { p.reason = std::string(khtr::tr("미지원 형식만 있음")) + ": " + fmt_name(bad[0]->fmt); return p; }
    p.reason = khtr::tr("이미지 없음"); return p;
}

/* ---------------------------------------------------------------- INI (game.ini / gamedb.ini) */
typedef std::map<std::string, std::map<std::string, std::string> > Ini;   // 섹션 → 키 → 값 (키 소문자)
inline std::string trim(const std::string & s) { size_t a = 0, b = s.size(); while (a < b && (unsigned char)s[a] <= ' ') a++; while (b > a && (unsigned char)s[b - 1] <= ' ') b--; return s.substr(a, b - a); }
inline bool ini_load(const std::string & path, Ini & out, std::vector<std::string> * order = NULL) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
    std::string data; char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n); fclose(f);
    if (data.size() >= 3 && (uint8_t)data[0] == 0xEF && (uint8_t)data[1] == 0xBB && (uint8_t)data[2] == 0xBF) data.erase(0, 3);
    std::string sec; size_t pos = 0;
    while (pos <= data.size()) {
        size_t e = data.find('\n', pos); if (e == std::string::npos) e = data.size();
        std::string ln = trim(data.substr(pos, e - pos)); pos = e + 1;
        if (ln.empty() || ln[0] == ';' || ln[0] == '#') { if (pos > data.size()) break; continue; }
        if (ln[0] == '[' && ln.back() == ']') { sec = trim(ln.substr(1, ln.size() - 2)); if (order && out.find(sec) == out.end()) order->push_back(sec); out[sec]; }
        else { size_t q = ln.find('='); if (q != std::string::npos) { std::string k = trim(ln.substr(0, q)); for (auto & c : k) c = (char)tolower((unsigned char)c); out[sec][k] = trim(ln.substr(q + 1)); } }
        if (pos > data.size()) break;
    }
    return true;
}
inline std::vector<std::string> split_list(const std::string & s, char sep = ',') {
    std::vector<std::string> v; std::string cur;
    for (char c : s) { if (c == sep) { v.push_back(trim(cur)); cur.clear(); } else cur += c; }
    if (!trim(cur).empty() || !v.empty()) v.push_back(trim(cur));
    std::vector<std::string> r; for (auto & x : v) if (!x.empty()) r.push_back(x); return r;
}

/* game.ini 형식 (게임 폴더 또는 DB 의 작품 절 공통):
   [game]
   id=hukyou           title=환세풍광전
   engine=compile_gensei
   disk_a=hukyou_kr.fdi  disk_b=  disk_c=  disk_d=
   boot=A              (A|B|C|D)
   swap=a.fdi,b.fdi
   cycles=3000         font=gensei_np2kai_font.bmp   extra=<dosbox conf 한 줄들 ; 로 구분>
   keys=15000:enter,17000:enter     (게임 부팅 후 자동 입력)
   [override]          (전역 설정 위에 얹는 작품별 값 — 화면·소리·패드·배속·글꼴·cycles)
   ratio=0..2 shader=0..2 curve=0..10 scan=0..9 mask=0..3 bgm=0..8 se=0..8 pad=<표> boost=2..8 boostmode=0..2 font=… cycles=…
   [pad]               o=RETURN x=ESC sq=CAPS ...
   never_map=F10,STOP,HELP     (패드·단축키 배정 금지)
*/
/* t36cd: 슬롯 별칭 — fd1/fd2/hd1/hd2 = disk_a/b/c/d, cd1 = cd. 같은 절에 옛 이름이 이미 있으면 옛 이름이 이긴다 */
inline std::map<std::string, std::string> slot_alias(const std::map<std::string, std::string> & g0) {
    std::map<std::string, std::string> g = g0;
    static const char * const al[][2] = { { "fd1", "disk_a" }, { "fd2", "disk_b" }, { "hd1", "disk_c" }, { "hd2", "disk_d" }, { "cd1", "cd" } };
    for (auto & a : al) { auto it = g0.find(a[0]); if (it != g0.end() && g.find(a[1]) == g.end()) g[a[1]] = it->second; }
    return g;
}
static std::set<std::string> g_ex_keys;   /* t48a (#614 ③): 이번 층이 줄로 적은 키(층 끝에서 비운다) */
inline bool apply_ini_section(Profile & p, const std::map<std::string, std::string> & g_in) {
    const std::map<std::string, std::string> g = slot_alias(g_in);
    for (auto & kvx : g) g_ex_keys.insert(kvx.first);
    auto get = [&](const char * k) -> std::string { auto it = g.find(k); return it == g.end() ? std::string() : it->second; };
    if (!get("id").empty()) p.id = get("id");
    if (!get("title").empty()) p.title = get("title");
    if (!get("engine").empty()) p.engine = get("engine");
    const char * dk[4] = { "disk_a", "disk_b", "disk_c", "disk_d" };
    bool any = false;
    for (int i = 0; i < 4; i++) { auto it = g.find(dk[i]); if (it != g.end()) { p.disk[i] = it->second == "-" ? std::string() : it->second; any = true; } }   /* t39: 「-」 = 이 칸을 비운다 */
    if (!get("boot").empty()) { char c = (char)toupper((unsigned char)get("boot")[0]); if (c >= 'A' && c <= 'D') p.bootletter = c; }
    if (g.find("swap") != g.end()) p.swap = split_list(get("swap"));
    if (g.find("ignore") != g.end()) p.ignore = split_list(get("ignore"));
    if (!get("install_required").empty()) p.install_required = get("install_required") == "1";
    if (!get("note").empty()) p.note = get("note");
    for (const char * k : { "cd", "bootkeys", "autoexec", "entry", "next", "next_label", "dosmode", "cycles", "font", "font_kind", "extra", "keys", "never_map", "mouse", "joystick", "sound", "order", "label_a", "label_b", "label_c", "label_d", "container_sha1", "demo", "label_demo", "company", "year", "genre", "patch", "devices", "auto", "grade", "judge", "metasrc", "srcdir", "nk_gid", "block", "jshort", "nk_flags", "nk_media", "sig", "auto_note", "curated", "label", "hide", "tpl", "base", "lang", "variant_of", "variant_label", "install", "hdd_mb", "hdd_name", "font_file", "swap_first", "entry_order", "short", "font_group", "fav_note", "dosexe", "dosargs" }) if (g.find(k) != g.end()) p.kv[k] = get(k);
    { auto c = p.kv.find("cd"); if (c != p.kv.end() && c->second == "-") p.kv.erase(c); }
    return any;
}

/* ---------------------------------------------------------------- 게임 DB: sha1 → 작품 */
struct DbGame { std::string id; std::map<std::string, std::string> g, ov, pad; std::vector<std::pair<std::string, std::string> > media; /* (role, sha1) */ };
inline bool db_load(const std::string & path, std::vector<DbGame> & out) {
    Ini ini; std::vector<std::string> order; if (!ini_load(path, ini, &order)) return false;
    for (auto & s : order) {
        if (s.compare(0, 5, "game:") != 0) continue;
        DbGame d; d.id = s.substr(5); d.g = ini[s];
        auto o = ini.find("override:" + d.id); if (o != ini.end()) d.ov = o->second;
        auto pd = ini.find("pad:" + d.id); if (pd != ini.end()) d.pad = pd->second;
        for (auto & kv : d.g) if (kv.first.compare(0, 6, "media_") == 0) d.media.push_back(std::make_pair(kv.first.substr(6), kv.second));   // media_<역할>=<sha1>[,<sha1>...]
        out.push_back(d);
    }
    return true;
}
inline const DbGame * db_find(const std::vector<DbGame> & db, const std::vector<ImgInfo> & imgs) {
    for (auto & d : db) for (auto & m : d.media) for (auto & hs : split_list(m.second)) for (auto & i : imgs) if (!i.sha1.empty() && i.sha1 == hs) return &d;
    return NULL;
}

/* 빌드 3 (#547): 목록 구분 — lang=ko|jp(gamedb 의 sha1 일치 항목 또는 사용자 지정)가 있을 때만 그것, 나머지는 추측하지 않고 빈 값(미분류) */
inline std::string eff_lang(const Profile & p) {
    auto g = [&](const char * k) -> std::string { auto it = p.kv.find(k); return it == p.kv.end() ? std::string() : it->second; };
    std::string l = g("lang"); if (l == "ko" || l == "jp") return l;
    return std::string();
}
static std::string g_entry_req;   /* t36cd: --entry <이름> (호스트가 채움) */
static std::string g_userini_override;   /* t39: --userini 로 시험 실행에만 쓰는 user.ini 경로 */
/* t48a (#614 ③): 키별 출처 추적 — 층(자동·DB·game.ini·항목·user.ini·계산)이 끝날 때마다 직전과 값이 달라진 키, 또는 그 층이 줄로 적은 키의 출처를 그 층으로 적고,
   값이 바뀌면 덮인 옛 값(출처)을 쌓는다. 결과는 p.kv 의 __val.<키> · __src.<키> · __cov.<키> 로 호스트에 넘긴다(값이 같은 층은 줄로 적은 경우만 출처가 바뀐다). */
inline std::map<std::string, std::string> src_snap(const Profile & p) {
    std::map<std::string, std::string> m; static const char * const dk[4] = { "disk_a", "disk_b", "disk_c", "disk_d" };
    for (int i = 0; i < 4; i++) if (!p.disk[i].empty()) m[dk[i]] = p.disk[i];
    m["boot"] = std::string(1, p.bootletter);
    { std::string s; for (auto & x : p.swap) { if (!s.empty()) s += ","; s += x; } if (!s.empty()) m["swap"] = s; }
    if (!p.title.empty()) m["title"] = p.title;
    for (auto & kv : p.kv) { if (kv.first.compare(0, 2, "__") == 0) continue; if (!kv.second.empty()) m[kv.first] = kv.second; }
    for (auto & kv : p.ov) m["[override] " + kv.first] = kv.second;
    for (auto & kv : p.pad) m["[pad] " + kv.first] = kv.second;
    return m;
}
struct SrcTrack {
    std::map<std::string, std::string> last, src, cov;
    void step(const Profile & p, const std::string & tag) {
        std::map<std::string, std::string> now = src_snap(p); std::set<std::string> ks;
        for (auto & kv : now) ks.insert(kv.first); for (auto & kv : last) ks.insert(kv.first);
        for (auto & k : ks) {
            auto a = last.find(k), b = now.find(k); std::string o = a == last.end() ? std::string() : a->second, n = b == now.end() ? std::string() : b->second;
            if (o == n) continue;
            if (!o.empty()) { auto s = src.find(k); std::string c = o + " (" + (s == src.end() ? std::string("자동") : s->second) + ")"; auto cv = cov.find(k); cov[k] = cv == cov.end() ? c : c + " ← " + cv->second; }
            src[k] = tag;
        }
        for (auto & k : g_ex_keys) if (now.count(k)) src[k] = tag;
        g_ex_keys.clear(); last = now;
    }
    void emit(Profile & p) {
        std::set<std::string> ks; for (auto & kv : last) ks.insert(kv.first); for (auto & kv : cov) ks.insert(kv.first);
        for (auto & k : ks) { auto a = last.find(k); p.kv["__val." + k] = a == last.end() ? std::string() : a->second; auto s = src.find(k); p.kv["__src." + k] = s == src.end() ? std::string("자동") : s->second; auto c = cov.find(k); if (c != cov.end()) p.kv["__cov." + k] = c->second; }
    }
};
/* 프로필 결정 — 우선순위: user.ini > 게임 폴더 game.ini(+entry) > DB(sha1) > 자동. 반환 시 p.src 에 출처. */
inline Profile resolve(const std::string & dir, const std::vector<ImgInfo> & imgs, const std::vector<DbGame> & db, const std::string & id_hint) {
    Profile p = auto_profile(imgs, id_hint);
    SrcTrack st; g_ex_keys.clear(); st.step(p, "자동");   /* t48a (#614 ③) */
    const DbGame * d = db_find(db, imgs);
    if (d) {
        // DB 항목은 디스크 배치를 이미지 파일명 → 슬롯으로 풀어낸다(해시 일치한 이미지의 base 로 치환)
        Profile q = p; q.src = "DB"; apply_ini_section(q, d->g);
        for (auto & kv : d->ov) q.ov[kv.first] = kv.second;
        for (auto & kv : d->pad) q.pad[kv.first] = kv.second;
        q.reason = std::string(khtr::tr("DB 일치")) + ": " + d->id; p = q; st.step(p, "DB");
        p.dbhit_id = d->id; { auto tt = d->g.find("title"); p.dbhit_title = (tt == d->g.end() || tt->second.empty()) ? d->id : tt->second; }
    }
    Ini gi; if (ini_load(dir + "/game.ini", gi) && gi.count("game")) {
        { auto & g = gi["game"]; if (g.count("disk_a") || g.count("disk_b") || g.count("disk_c") || g.count("disk_d")) { for (auto & d : p.disk) d.clear(); p.swap.clear(); p.kv.erase("keys"); } }   // game.ini 가 디스크를 정하면 DB/자동 배치는 전부 버린다
        apply_ini_section(p, gi["game"]); p.src = "game.ini"; p.reason = "game.ini";
        if (gi.count("override")) for (auto & kv : gi["override"]) p.ov[kv.first] = kv.second;
        if (gi.count("pad")) for (auto & kv : gi["pad"]) p.pad[kv.first] = kv.second;
        st.step(p, "game.ini");
    }
    /* t39: 실행 항목 [entry:<이름>] — 이름은 한 번만 정한다: --entry/실행 요청(「-」 = 항목 없이 기본) > user.ini [game] entry > game.ini [game] entry > DB entry.
       같은 이름의 항목은 game.ini → user.ini 순으로 키 단위 병합(항목이 디스크 슬롯을 정하면 앞선 배치를 비우고 항목 것으로) */
    {
        Ini gi2, ui; bool hg = ini_load(dir + "/game.ini", gi2), hu = ini_load(g_userini_override.empty() ? dir + "/user.ini" : g_userini_override, ui);
        std::string en = g_entry_req;
        if (en.empty() && hu && ui.count("game") && ui["game"].count("entry")) en = ui["game"]["entry"];
        if (en.empty() && hg && gi2.count("game") && gi2["game"].count("entry")) en = gi2["game"]["entry"];
        if (en.empty() && p.kv.count("entry")) en = p.kv["entry"];
        if (en == "-") en.clear();
        p.kv.erase("entry"); p.kv.erase("entry_name");
        auto apply_entry = [&](Ini & ini, const char * from) {
            if (en.empty()) return;
            auto it = ini.find("entry:" + en); if (it == ini.end()) return;
            const std::map<std::string, std::string> e = slot_alias(it->second);
            if (e.count("disk_a") || e.count("disk_b") || e.count("disk_c") || e.count("disk_d") || e.count("cd")) { for (auto & d : p.disk) d.clear(); p.swap.clear(); p.kv.erase("keys"); p.kv.erase("cd"); }
            apply_ini_section(p, it->second); p.kv["entry_name"] = en; p.reason += std::string(" + entry ") + en + "(" + from + ")"; st.step(p, std::string("entry:") + en + " (" + from + ")");
        };
        if (hg) apply_entry(gi2, "game.ini");
        /* user.ini — 사용자 값이 game.ini·DB 보다 우선. [game] 과 [entry:<이름>] 만 읽는다(디스크 슬롯이 있으면 앞 배치를 비움) */
        if (hu) {
            if (ui.count("game")) {
                const std::map<std::string, std::string> u = slot_alias(ui["game"]);
                if (u.count("disk_a") || u.count("disk_b") || u.count("disk_c") || u.count("disk_d")) { for (auto & d : p.disk) d.clear(); p.swap.clear(); p.kv.erase("keys"); p.kv.erase("cd"); }
                apply_ini_section(p, ui["game"]); p.reason += " + user.ini"; st.step(p, "user.ini");
            }
            apply_entry(ui, "user.ini");
            if (ui.count("override")) for (auto & kv : ui["override"]) p.ov[kv.first] = kv.second;
            if (ui.count("pad")) for (auto & kv : ui["pad"]) p.pad[kv.first] = kv.second;
            st.step(p, "user.ini");
        }
        p.kv.erase("entry");   /* 「entry」 는 이름 지정용 키 — 결과에는 entry_name 만 남긴다 */
    }
    // disk/swap/ignore 값이 sha1(40 hex) 이면 해시가 같은 이미지의 파일 이름으로 바꾼다
    auto fix = [&](std::string & v) { if (v.size() == 40) for (auto & im : imgs) if (im.sha1 == v) { v = im.base; break; } };
    for (int i = 0; i < 4; i++) fix(p.disk[i]);
    for (auto & v : p.swap) fix(v);
    for (auto & v : p.ignore) fix(v);
    { auto it = p.kv.find("demo"); if (it != p.kv.end()) fix(it->second); }
    /* t39 (#290): 글꼴 기본값은 목록 구분을 따른다 — 항목의 font= > font_kind= > 목록 기본(한글판 = 번들 한글, 원판 = jp). lang 이 없으면 한글 증거(패치 표기·제목의 「한글」)로 판정 */
    {
        bool hf = p.kv.count("font") && !p.kv["font"].empty(), hk = p.kv.count("font_kind") && !p.kv["font_kind"].empty();
        std::string lg = eff_lang(p); p.kv["lang_eff"] = lg;
        /* t44a (#494): 사용자 「언어」(lang=ko/jp)는 저장된 font_kind 보다 먼저 본다 — font= 가 없을 때만. 자동 감지는 lang 이 비고 font_kind 도 비었을 때의 기본값 */
        if (!hf) { std::string l0 = p.kv.count("lang") ? p.kv["lang"] : std::string(); if (l0 == "jp") { p.kv["font_kind"] = "jp"; p.kv["font_kind_src"] = "user"; } else if (l0 == "ko") { p.kv["font_kind"] = "kr"; p.kv["font_kind_src"] = "user"; }
          else if (!hk) { if (lg == "ko") { p.kv["font_kind"] = "kr"; p.kv["font_kind_src"] = "list"; }
          else if (l0.empty() && (p.kv.count("dosfd") || p.src == "자동" || (p.kv.count("sha1s") && !p.kv.count("dbid")))) { p.kv["font_kind"] = "jp"; p.kv["font_kind_src"] = "auto"; } } }   /* t43c (#330-2): 자동·자동등록 + lang 미지정 + 한글 증거 없음 = 일본어 원판 → jp_default */   /* t39ui3: lang 이 명시되지 않은 항목은 옛 동작(번들 한글 글꼴) 유지 — 이관 전 데이터가 일본어 글꼴로 바뀌지 않게 */
    }
    st.step(p, "계산(언어·글꼴 기본, 해시→파일명)"); st.emit(p);   /* t48a (#614 ③) */
    return p;
}


/* ---------------------------------------------------------------- t39: 실행 방법(항목) 목록 · user.ini 쓰기 */
struct EntInfo { std::string name, label, tag; bool hidden = false, in_game = false, in_user = false; };
inline std::string ini_get(const Ini & ini, const std::string & sec, const char * k) { auto s = ini.find(sec); if (s == ini.end()) return std::string(); auto it = s->second.find(k); return it == s->second.end() ? std::string() : it->second; }
/* game.ini + user.ini 의 [entry:*] 를 모은다. 순서 = user [game] entry_order > game.ini entry_order > 파일 등장 순. def = 기본으로 지정된 항목 이름 */
inline std::vector<EntInfo> list_entries(const std::string & dir, std::string * def = NULL) {
    Ini gi, ui; std::vector<std::string> go, uo; bool hg = ini_load(dir + "/game.ini", gi, &go), hu = ini_load(dir + "/user.ini", ui, &uo);
    std::vector<std::string> names; auto add = [&](const std::string & n) { for (auto & x : names) if (x == n) return; names.push_back(n); };
    std::vector<std::string> ord = split_list(hu ? ini_get(ui, "game", "entry_order") : std::string()); if (ord.empty() && hg) ord = split_list(ini_get(gi, "game", "entry_order"));
    for (auto & n : ord) { if ((hg && gi.count("entry:" + n)) || (hu && ui.count("entry:" + n))) add(n); }
    for (auto & sct : go) if (sct.compare(0, 6, "entry:") == 0) add(sct.substr(6));
    for (auto & sct : uo) if (sct.compare(0, 6, "entry:") == 0) add(sct.substr(6));
    std::vector<EntInfo> r;
    for (auto & n : names) {
        EntInfo e; e.name = n; e.in_game = hg && gi.count("entry:" + n); e.in_user = hu && ui.count("entry:" + n);
        std::string lb = e.in_user ? ini_get(ui, "entry:" + n, "label") : std::string(); if (lb.empty() && e.in_game) lb = ini_get(gi, "entry:" + n, "label"); e.label = lb.empty() ? n : lb;
        std::string hd = e.in_user ? ini_get(ui, "entry:" + n, "hide") : std::string(); if (hd.empty() && e.in_game) hd = ini_get(gi, "entry:" + n, "hide"); e.hidden = hd == "1";
        e.tag = e.in_user && e.in_game ? "수정됨" : e.in_user ? "내 것" : "game.ini";
        r.push_back(e);
    }
    if (def) { std::string d = hu ? ini_get(ui, "game", "entry") : std::string(); if (d.empty() && hg) d = ini_get(gi, "game", "entry"); *def = d; }
    return r;
}
/* user.ini 쓰기: UTF-8, 섹션 순서 유지, 키는 자주 쓰는 순서 → 나머지 가나다. 첫 쓰기 전에 user.ini.bak 보관 */
inline bool ini_write(const std::string & path, const Ini & ini, const std::vector<std::string> & order, const char * header, bool backup = true) {
    if (backup) { FILE * t = fopen(path.c_str(), "rb"); if (t) { fclose(t); std::string bk = path + ".bak"; FILE * b = fopen(bk.c_str(), "rb"); if (b) fclose(b); else { FILE * in = fopen(path.c_str(), "rb"); FILE * out = fopen(bk.c_str(), "wb"); if (in && out) { char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out); } if (in) fclose(in); if (out) fclose(out); } } }
    std::string tmp = path + ".tmp"; FILE * f = fopen(tmp.c_str(), "wb"); if (!f) return false;
    if (header && *header) fprintf(f, "%s\n", header);
    static const char * const pk[] = { "label", "tpl", "base", "entry", "entry_order", "lang", "disk_a", "disk_b", "disk_c", "disk_d", "cd", "boot", "demo", "swap", "swap_first", "bootkeys", "autoexec", "install", "hdd_mb", "hdd_name", "font", "font_kind", "font_file", "mouse", "hide", "dosexe", "dosargs" };
    for (auto & sct : order) {
        auto it = ini.find(sct); if (it == ini.end()) continue;
        fprintf(f, "\n[%s]\n", sct.c_str()); std::set<std::string> done;
        for (const char * k : pk) { auto q = it->second.find(k); if (q != it->second.end()) { fprintf(f, "%s=%s\n", k, q->second.c_str()); done.insert(k); } }
        for (auto & kv : it->second) if (!done.count(kv.first)) fprintf(f, "%s=%s\n", kv.first.c_str(), kv.second.c_str());
    }
    fclose(f);
    remove(path.c_str()); return rename(tmp.c_str(), path.c_str()) == 0;
}

/* ---------------------------------------------------------------- t39: 항목 읽기/저장 (user.ini) */
static const char * const UINI_HEADER = "; " KH_FULLNAME " user.ini -- 이 파일은 큐하치가 씁니다(손으로 고쳐도 되나 주석은 저장 때 사라집니다). UTF-8.";
typedef std::map<std::string, std::string> Kv;
/* game.ini → user.ini 순으로 병합한 항목 값. 디스크 슬롯 값 「-」 는 비움. 없으면 false */
inline bool entry_kv(const std::string & dir, const std::string & name, Kv & out, bool * in_game = NULL, bool * in_user = NULL) {
    Ini gi, ui; bool hg = ini_load(dir + "/game.ini", gi), hu = ini_load(dir + "/user.ini", ui); out.clear();
    bool a = hg && gi.count("entry:" + name), b = hu && ui.count("entry:" + name); if (in_game) *in_game = a; if (in_user) *in_user = b;
    if (!a && !b) return false;
    auto put = [&](const Kv & src) {
        Kv u = slot_alias(src); bool slots = u.count("disk_a") || u.count("disk_b") || u.count("disk_c") || u.count("disk_d") || u.count("cd");
        if (slots) for (const char * k : { "disk_a", "disk_b", "disk_c", "disk_d", "cd", "swap", "bootkeys" }) if (!u.count(k)) out.erase(k);
        for (auto & kv : u) { if ((kv.first.compare(0, 5, "disk_") == 0 || kv.first == "cd") && kv.second == "-") out.erase(kv.first); else out[kv.first] = kv.second; }
    };
    if (a) put(gi["entry:" + name]); if (b) put(ui["entry:" + name]);
    return true;
}
/* 사용자 값 쓰기. kv = 편집기의 전체 값(빈 값은 「없음」). game.ini 가 같은 항목을 정의하면 지워진 칸은 「-」/빈 값으로 덮는다 */
inline bool save_entry(const std::string & dir, const std::string & name, const Kv & kv, const std::string & set_default = std::string(), const std::string & new_order = std::string(), bool order_set = false) {
    Ini gi, ui; std::vector<std::string> uo; ini_load(dir + "/game.ini", gi); ini_load(dir + "/user.ini", ui, &uo);
    Kv base; if (gi.count("entry:" + name)) base = slot_alias(gi["entry:" + name]);
    Kv w;
    for (auto & kv1 : kv) if (!kv1.second.empty()) w[kv1.first] = kv1.second;
    for (const char * k : { "disk_a", "disk_b", "disk_c", "disk_d", "cd" }) if (!w.count(k) && base.count(k) && !base[k].empty() && base[k] != "-") w[k] = "-";
    for (auto & b : base) if (!w.count(b.first) && b.first.compare(0, 5, "disk_") != 0 && b.first != "cd" && !b.second.empty()) w[b.first] = "";
    ui["entry:" + name] = w; if (std::find(uo.begin(), uo.end(), "entry:" + name) == uo.end()) uo.push_back("entry:" + name);
    if (!ui.count("game")) { ui["game"]; uo.insert(uo.begin(), "game"); }
    if (!set_default.empty()) ui["game"]["entry"] = set_default == "-" ? std::string() : set_default;
    if (order_set) { if (new_order.empty()) ui["game"].erase("entry_order"); else ui["game"]["entry_order"] = new_order; }
    return ini_write(dir + "/user.ini", ui, uo, UINI_HEADER);
}
/* 항목 지우기: 사용자 섹션 삭제. game.ini 에 있던 항목이면 hide=1 만 남긴다(원본 불변). reset=true 면 숨김 없이 섹션만 삭제(DB 기본값으로 되돌리기) */
inline bool delete_entry(const std::string & dir, const std::string & name, bool reset) {
    Ini gi, ui; std::vector<std::string> uo; ini_load(dir + "/game.ini", gi); if (!ini_load(dir + "/user.ini", ui, &uo)) { ui.clear(); uo.clear(); }
    bool in_game = gi.count("entry:" + name) > 0;
    ui.erase("entry:" + name); uo.erase(std::remove(uo.begin(), uo.end(), "entry:" + name), uo.end());
    if (!reset && in_game) { ui["entry:" + name]["hide"] = "1"; uo.push_back("entry:" + name); }
    if (ui.count("game")) { if (ui["game"]["entry"] == name && !reset) ui["game"].erase("entry"); auto o = split_list(ui["game"]["entry_order"]); o.erase(std::remove(o.begin(), o.end(), name), o.end()); std::string j; for (auto & x : o) { if (!j.empty()) j += ","; j += x; } if (j.empty()) ui["game"].erase("entry_order"); else ui["game"]["entry_order"] = j; }
    return ini_write(dir + "/user.ini", ui, uo, UINI_HEADER);
}
/* t48e (#646): user.ini [pad] 한 줄 쓰기(빈 값 = 줄 지움). 다른 구역·줄은 그대로 보존 */
inline bool set_pad_key(const std::string & dir, const std::string & key, const std::string & val) {
    Ini ui; std::vector<std::string> uo; if (!ini_load(dir + "/user.ini", ui, &uo)) { ui.clear(); uo.clear(); }
    if (!ui.count("pad")) { ui["pad"]; uo.push_back("pad"); }
    if (val.empty()) ui["pad"].erase(key); else ui["pad"][key] = val;
    if (ui["pad"].empty()) { ui.erase("pad"); uo.erase(std::remove(uo.begin(), uo.end(), "pad"), uo.end()); }
    return ini_write(dir + "/user.ini", ui, uo, UINI_HEADER);
}
/* t48e: user.ini [pad] 의 btn_* 줄만 읽기 */
inline void get_pad_keys(const std::string & dir, std::map<std::string, std::string> & out) {
    out.clear(); Ini ui; if (!ini_load(dir + "/user.ini", ui)) return; auto it = ui.find("pad"); if (it == ui.end()) return;
    for (auto & kv : it->second) if (kv.first.compare(0, 4, "btn_") == 0) out[kv.first] = kv.second;
}
inline bool set_game_key(const std::string & dir, const std::string & key, const std::string & val) {
    Ini ui; std::vector<std::string> uo; if (!ini_load(dir + "/user.ini", ui, &uo)) { ui.clear(); uo.clear(); }
    if (!ui.count("game")) { ui["game"]; uo.insert(uo.begin(), "game"); }
    if (val.empty()) ui["game"].erase(key); else ui["game"][key] = val;
    return ini_write(dir + "/user.ini", ui, uo, UINI_HEADER);
}

} // namespace prof
