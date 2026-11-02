// gensei_prof.h — 원본 파일 구조 기반 자동 프로필 + game.ini 덮어쓰기 + 게임 DB(sha1 → 작품). (#230/#237)
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

/* 첫 섹터 읽기로 부팅 후보·이름 추정 (FD: 섹터크기 256/1024 가능, HD: IPL) */
inline void probe_boot(ImgInfo & r) {
    if (!(fmt_supported(r.fmt))) return;
    FILE * f = fopen(r.path.c_str(), "rb"); if (!f) return;
    if (r.fmt == F_NFD0 || r.fmt == F_NFD1) { fclose(f); r.boot_ok = true; return; }   // NFD: 섹터 표를 따라가야 첫 섹터가 나온다 — 부팅 가능으로 보고 라벨은 비움
    long off0 = (long)r.hdr;
    if (r.fmt == F_D88) { uint8_t t[4] = {0}; fseek(f, 0x20, SEEK_SET); if (fread(t, 1, 4, f) != 4) { fclose(f); return; } off0 = (long)rd32(t) + 16; }   // 첫 트랙 첫 섹터 데이터(섹터 머리 16바이트 뒤)
    std::vector<uint8_t> b(1024, 0); fseek(f, off0, SEEK_SET); size_t n = fread(&b[0], 1, b.size(), f); fclose(f);
    bool nz = false; for (size_t i = 0; i < n && i < 512; i++) if (b[i]) { nz = true; break; }
    r.boot_ok = nz;
    if (n >= 8 && !memcmp(&b[4], "IPL1", 4)) r.ipl = true;
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
    if (i.boot_ok) s += 10;
    if (i.ipl) s += 20;
    return s;
}

/* 자동 프로필: 폴더(또는 파일 목록)의 이미지에서 결정.
   규칙: HDD 가 있으면 HDD(C) 부팅. 없으면 FD 중 부팅 점수 최고 = A, 나머지 = B(1장) / 교체 목록(2장 이상).
   미지원 형식만 있으면 reason 에 사유를 적고 disk 비움. */
inline Profile auto_profile(std::vector<ImgInfo> imgs, const std::string & id) {
    Profile p; p.id = id; p.src = "자동";
    std::vector<ImgInfo *> hd, fd, cd, bad;
    for (auto & i : imgs) {
        if (i.fmt == F_ISO) { cd.push_back(&i); continue; }
        if (!fmt_supported(i.fmt)) { bad.push_back(&i); continue; }
        (i.hdd ? hd : fd).push_back(&i);
    }
    if (!hd.empty()) {
        // 가장 큰 HDD 하나(여러 개면 이름순 첫 번째) 를 C 로, 둘째는 D
        std::sort(hd.begin(), hd.end(), [](ImgInfo * a, ImgInfo * b) { return a->base < b->base; });
        p.disk[2] = hd[0]->base; if (hd.size() > 1) p.disk[3] = hd[1]->base;
        p.bootletter = 'C';
        p.reason = "HDD 있음 → C 부팅";
        if (!fd.empty()) p.reason += " (FD " + std::to_string(fd.size()) + "개는 무시/보조)";
        return p;
    }
    if (!fd.empty()) {
        std::sort(fd.begin(), fd.end(), [](ImgInfo * a, ImgInfo * b) { int sa = sys_score(*a), sb = sys_score(*b); if (sa != sb) return sa > sb; return a->base < b->base; });
        p.disk[0] = fd[0]->base; p.bootletter = 'A';
        if (fd.size() >= 2) p.disk[1] = fd[1]->base;
        for (size_t k = 2; k < fd.size(); k++) p.swap.push_back(fd[k]->base);
        p.reason = std::string("FD ") + std::to_string(fd.size()) + "개 → 부팅 점수 최고 = A" + (fd.size() >= 2 ? ", 다음 = B" : "") + (fd.size() > 2 ? ", 나머지 교체 목록" : "");
        return p;
    }
    if (!cd.empty()) { p.install_required = true; p.reason = "CD 이미지만 있음 → 부팅 불가(설치 전용)"; return p; }
    if (!bad.empty()) { p.reason = std::string("미지원 형식만 있음: ") + fmt_name(bad[0]->fmt); return p; }
    p.reason = "이미지 없음"; return p;
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
inline bool apply_ini_section(Profile & p, const std::map<std::string, std::string> & g) {
    auto get = [&](const char * k) -> std::string { auto it = g.find(k); return it == g.end() ? std::string() : it->second; };
    if (!get("id").empty()) p.id = get("id");
    if (!get("title").empty()) p.title = get("title");
    if (!get("engine").empty()) p.engine = get("engine");
    const char * dk[4] = { "disk_a", "disk_b", "disk_c", "disk_d" };
    bool any = false;
    for (int i = 0; i < 4; i++) { auto it = g.find(dk[i]); if (it != g.end()) { p.disk[i] = it->second; any = true; } }
    if (!get("boot").empty()) { char c = (char)toupper((unsigned char)get("boot")[0]); if (c >= 'A' && c <= 'D') p.bootletter = c; }
    if (g.find("swap") != g.end()) p.swap = split_list(get("swap"));
    if (g.find("ignore") != g.end()) p.ignore = split_list(get("ignore"));
    if (!get("install_required").empty()) p.install_required = get("install_required") == "1";
    if (!get("note").empty()) p.note = get("note");
    for (const char * k : { "cycles", "font", "font_kind", "extra", "keys", "never_map", "mouse", "joystick", "sound", "order", "label_a", "label_b", "label_c", "label_d", "container_sha1", "demo", "label_demo", "company", "year", "genre", "patch", "devices", "auto", "grade", "judge", "metasrc", "srcdir", "nk_gid", "block", "jshort", "nk_flags", "nk_media", "sig", "auto_note", "curated" }) if (g.find(k) != g.end()) p.kv[k] = get(k);
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

/* 프로필 결정 — 우선순위: 게임 폴더 game.ini > DB(sha1) > 자동. 반환 시 p.src 에 출처. */
inline Profile resolve(const std::string & dir, const std::vector<ImgInfo> & imgs, const std::vector<DbGame> & db, const std::string & id_hint) {
    Profile p = auto_profile(imgs, id_hint);
    const DbGame * d = db_find(db, imgs);
    if (d) {
        // DB 항목은 디스크 배치를 이미지 파일명 → 슬롯으로 풀어낸다(해시 일치한 이미지의 base 로 치환)
        Profile q = p; q.src = "DB"; apply_ini_section(q, d->g);
        for (auto & kv : d->ov) q.ov[kv.first] = kv.second;
        for (auto & kv : d->pad) q.pad[kv.first] = kv.second;
        q.reason = "DB 일치: " + d->id; p = q;
        p.dbhit_id = d->id; { auto tt = d->g.find("title"); p.dbhit_title = (tt == d->g.end() || tt->second.empty()) ? d->id : tt->second; }
    }
    Ini gi; if (ini_load(dir + "/game.ini", gi) && gi.count("game")) {
        { auto & g = gi["game"]; if (g.count("disk_a") || g.count("disk_b") || g.count("disk_c") || g.count("disk_d")) { for (auto & d : p.disk) d.clear(); p.swap.clear(); p.kv.erase("keys"); } }   // game.ini 가 디스크를 정하면 DB/자동 배치는 전부 버린다
        apply_ini_section(p, gi["game"]); p.src = "game.ini"; p.reason = "game.ini";
        if (gi.count("override")) for (auto & kv : gi["override"]) p.ov[kv.first] = kv.second;
        if (gi.count("pad")) for (auto & kv : gi["pad"]) p.pad[kv.first] = kv.second;
    }
    // disk/swap/ignore 값이 sha1(40 hex) 이면 해시가 같은 이미지의 파일 이름으로 바꾼다
    auto fix = [&](std::string & v) { if (v.size() == 40) for (auto & im : imgs) if (im.sha1 == v) { v = im.base; break; } };
    for (int i = 0; i < 4; i++) fix(p.disk[i]);
    for (auto & v : p.swap) fix(v);
    for (auto & v : p.ignore) fix(v);
    { auto it = p.kv.find("demo"); if (it != p.kv.end()) fix(it->second); }
    return p;
}

} // namespace prof
