/* pc98_savestate.cpp — GENSEI 미니 에뮬레이터 세이브스테이트 드라이버 F.
 * S/src/misc/savestates.cpp 대체. savestates.obj 대신 본체 링크에 들어간다.
 * 원본의 GUI·셸·설정 의존(tinyfd, GUI_Shortcut, systemmessagebox, RunningProgram,
 * control->GetSection, MAPPER, flagged_backup/restore)은 전부 제거.
 * 컨테이너는 minizip 대신 단순 TLV: 매직 "G98S"+포맷버전, 헤더, 컴포넌트 순회,
 * 끝에 CRC32(전체). 컴포넌트 순회·로드 순서는 원본과 같이 std::map 이름 순.
 * 파일 입출력은 FILE fread fwrite. 경로는 호스트가 UTF-8 로 준다.
 * 링크 주의: pic.cpp.E 가 pic.obj 를 대체 링크할 때 g_pic_state_unknown_events 를 정의한다.
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "dosbox.h"   /* SaveState, MachineType, MCH_PC98, extern machine */
#include "mem.h"      /* MEM_TotalPages (memory.obj, 서브셋 내) */
#include "logging.h"  /* LOG_MSG */

#include "pc98_savestate.h"

#ifndef GENSEI_BUILD_ID
#define GENSEI_BUILD_ID "unknown"
#endif

/* T10 진단 카운터. 정의: pic.cpp.E (E 가 pic.obj 를 대체할 때만 존재). */
extern unsigned g_pic_state_unknown_events;

namespace {

const uint8_t kMagic[4] = { 'G', '9', '8', 'S' };
const uint32_t kFormatVersion = 3;   /* t53a(SF/F1): v3 = v2 + 빌드ID 문자열 바로 뒤 상태 형식 번호(kStateAbi). 쓰기는 v3, 읽기는 v2·v3. v1 은 거부.
                                      * v2(r011): 헤더 뒤 THMB 항목(저장 순간 640x400 PNG) 추가, TLB(호스트 포인터) 제외는 paging 사본 몫 */
const uint32_t kStateAbi = 1;        /* 컴포넌트 구성·바이트 배치가 바뀌는 빌드에서만 올린다. 빌드ID 는 정보용(불러오기 판정에 쓰지 않음) */

bool OptionalComp(const std::string &n) { return n == "pc98_video"; }   /* #634: absent in state files written by Android builds before t661 */
uint32_t g_crc_tab[256];
bool g_crc_init = false;

void CrcInit() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_crc_tab[i] = c;
    }
    g_crc_init = true;
}

uint32_t CrcOf(const uint8_t *p, size_t n) {
    if (!g_crc_init) CrcInit();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = g_crc_tab[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void Put16(std::vector<uint8_t> &o, uint16_t v) {
    o.push_back((uint8_t)(v & 0xFFu)); o.push_back((uint8_t)((v >> 8) & 0xFFu));
}

void Put32(std::vector<uint8_t> &o, uint32_t v) {
    for (int i = 0; i < 4; i++) o.push_back((uint8_t)((v >> (i * 8)) & 0xFFu));
}

void Put64(std::vector<uint8_t> &o, uint64_t v) {
    for (int i = 0; i < 8; i++) o.push_back((uint8_t)((v >> (i * 8)) & 0xFFu));
}

void PutStr(std::vector<uint8_t> &o, const std::string &s) {
    Put16(o, (uint16_t)s.size());
    o.insert(o.end(), s.begin(), s.end());
}

/* private 멤버 components 에 손대지 않기 위해 registerComponent 가 채우는 거울. map 순회=이름 순. */
typedef std::map<std::string, SaveState::Component *> CompMap;
CompMap &Registry() { static CompMap m; return m; }

struct Reader {
    const uint8_t *p;
    const uint8_t *end;
    bool fail;
    Reader(const uint8_t *b, size_t n) : p(b), end(b + n), fail(false) {}
    uint16_t U16() {
        if (fail || (size_t)(end - p) < 2) { fail = true; return 0; }
        uint16_t v = (uint16_t)(p[0] | (p[1] << 8)); p += 2; return v;
    }
    uint32_t U32() {
        if (fail || (size_t)(end - p) < 4) { fail = true; return 0; }
        uint32_t v = 0; for (int i = 0; i < 4; i++) v |= ((uint32_t)p[i] << (i * 8)); p += 4; return v;
    }
    uint64_t U64() {
        if (fail || (size_t)(end - p) < 8) { fail = true; return 0; }
        uint64_t v = 0; for (int i = 0; i < 8; i++) v |= ((uint64_t)p[i] << (i * 8)); p += 8; return v;
    }
    std::string Str() {
        uint16_t n = U16();
        if (fail || (size_t)(end - p) < n) { fail = true; return std::string(); }
        std::string s((const char *)p, n); p += n; return s;
    }
};

/* t53a/F5 도우미. Win32 는 헤더 충돌을 피하려고 직접 선언(kernel32). 좁은 문자열 API 는 fopen 과 같은 규약이라 경로 해석이 일치한다. */
#if defined(_WIN32)
extern "C" {
__declspec(dllimport) int __stdcall CopyFileA(const char *, const char *, int);
__declspec(dllimport) int __stdcall MoveFileExA(const char *, const char *, unsigned long);
__declspec(dllimport) int __stdcall DeleteFileA(const char *);
int __cdecl _commit(int);
int __cdecl _fileno(FILE *);
}
#endif

bool ReadWhole(const char *path, std::vector<uint8_t> &img) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    img.clear();
    static uint8_t tmpbuf[65536];
    for (;;) { size_t n = fread(tmpbuf, 1, sizeof(tmpbuf), f); if (!n) break; img.insert(img.end(), tmpbuf, tmpbuf + n); }
    fclose(f);
    return true;
}

bool FileExists(const std::string &p) { FILE *f = fopen(p.c_str(), "rb"); if (!f) return false; fclose(f); return true; }

bool BuildImage(std::vector<uint8_t> &out, const std::vector<Pc98StateDisk> &disks, const std::vector<uint8_t> *thumb) {
    out.clear();
    out.insert(out.end(), kMagic, kMagic + 4);
    Put32(out, kFormatVersion);
    PutStr(out, GENSEI_BUILD_ID);
    Put32(out, kStateAbi);
    Put32(out, (uint32_t)MCH_PC98);
    Put32(out, (uint32_t)MEM_TotalPages());
    Put32(out, (uint32_t)disks.size());
    for (size_t i = 0; i < disks.size(); i++) {
        PutStr(out, disks[i].name);
        Put64(out, disks[i].size);
        Put32(out, disks[i].crc32);
    }
    /* THMB: 길이(u32, 0=없음) + PNG 바이트. 헤더 바로 뒤라 앞부분만 읽어 미리보기 가능 */
    if (thumb && !thumb->empty()) { Put32(out, (uint32_t)thumb->size()); out.insert(out.end(), thumb->begin(), thumb->end()); } else Put32(out, 0);
    CompMap &m = Registry();
    Put32(out, (uint32_t)m.size());
    for (CompMap::iterator it = m.begin(); it != m.end(); ++it) {
        std::ostringstream ss(std::ios::binary);
        it->second->getBytes(ss);
        std::string bytes = ss.str();
        PutStr(out, it->first);
        Put64(out, (uint64_t)bytes.size());
        out.insert(out.end(), bytes.begin(), bytes.end());
        LOG_MSG("PC98_SaveState: saved component '%s' (%u bytes)", it->first.c_str(), (unsigned)bytes.size());
    }
    Put32(out, CrcOf(&out[0], out.size()));
    return true;
}

void Rollback(const std::map<std::string, std::string> &snap) {
    CompMap &m = Registry();
    for (CompMap::iterator it = m.begin(); it != m.end(); ++it) {
        std::map<std::string, std::string>::const_iterator f = snap.find(it->first);
        if (f == snap.end()) continue;
        std::istringstream ss(f->second, std::ios::binary);
        it->second->setBytes(ss); /* 최선 복구. 실패 무시. */
    }
}

/* t53a/F1: 헤더 검사 공용(불러오기·기존 파일 분류). 빌드ID 는 판정에서 뺀다(정보용). 형식 번호(v3) 와 기종·메모리·디스크만 본다.
 * 통과하면 r 은 THMB 길이 필드 앞에 서 있다. */
bool ParseHeader(const uint8_t *buf, size_t len, Reader &r, const std::vector<Pc98StateDisk> &disks, std::string &build, uint32_t &ver, std::string &err) {
    if (len < 12) { err = "image too short"; return false; }
    if (std::memcmp(buf, kMagic, 4) != 0) { err = "bad magic (not a GENSEI state)"; return false; }
    uint32_t stored_crc = 0;
    for (int i = 0; i < 4; i++) stored_crc |= ((uint32_t)buf[len - 4 + i] << (i * 8));
    if (CrcOf(buf, len - 4) != stored_crc) { err = "CRC32 mismatch (corrupt image)"; return false; }
    r = Reader(buf + 4, len - 8);
    ver = r.U32();
    if (r.fail || (ver != 2 && ver != kFormatVersion)) { err = "format version mismatch"; return false; }
    build = r.Str();
    if (r.fail) { err = "truncated header"; return false; }
    if (ver >= 3) {
        uint32_t abi = r.U32();
        if (r.fail) { err = "truncated header"; return false; }
        if (abi != kStateAbi) {
            std::ostringstream o; o << "state ABI mismatch: saved=" << abi << " current=" << kStateAbi << " (saved by build '" << build << "')";
            err = o.str(); return false;
        }
    }
    if (r.U32() != (uint32_t)machine) { err = "machine type mismatch"; return false; }
    if (r.U32() != (uint32_t)MEM_TotalPages()) { err = "memory page count mismatch"; return false; }
    uint32_t ndisk = r.U32();
    if (r.fail || ndisk != disks.size()) { err = "disk list count mismatch"; return false; }
    for (size_t i = 0; i < disks.size(); i++) {
        std::string nm = r.Str();
        uint64_t sz = r.U64();
        uint32_t cc = r.U32();
        if (r.fail) { err = "truncated disk header"; return false; }
        if (nm != disks[i].name || sz != disks[i].size || cc != disks[i].crc32) {
            std::ostringstream idx; idx << i;
            err = std::string("disk image mismatch at index ") + idx.str() + " ('" + disks[i].name + "')";
            return false;
        }
    }
    return true;
}

bool ApplyImage(const uint8_t *buf, size_t len, const std::vector<Pc98StateDisk> &disks, std::string &err) {
    Reader r(buf, 0);
    std::string build; uint32_t ver = 0;
    if (!ParseHeader(buf, len, r, disks, build, ver, err)) return false;
    { uint32_t tl = r.U32(); if (r.fail || (uint64_t)(r.end - r.p) < tl) { err = "truncated THMB"; return false; } r.p += tl; }
    uint32_t ncomp = r.U32();
    if (r.fail) { err = "truncated component table"; return false; }
    std::map<std::string, std::string> entries;
    for (uint32_t i = 0; i < ncomp; i++) {
        std::string nm = r.Str();
        uint64_t dl = r.U64();
        if (r.fail || (uint64_t)(r.end - r.p) < dl) { err = "truncated component data"; return false; }
        entries[nm] = std::string((const char *)r.p, (size_t)dl);
        r.p += (size_t)dl;
    }
    if (r.fail) { err = "truncated component table"; return false; }
    CompMap &m = Registry();
    std::map<std::string, std::string> snap; /* §4.2 7번: 로드 전 현 상태를 메모리에 저장 */
    for (CompMap::iterator it = m.begin(); it != m.end(); ++it) {
        std::ostringstream ss(std::ios::binary);
        it->second->getBytes(ss);
        snap[it->first] = ss.str();
    }
    /* t53a/F1: 컴포넌트 길이 표 비교 — 하나라도 어긋나면 아무것도 건드리기 전에 거부(상태 형식이 다른 저장). */
    for (CompMap::iterator it = m.begin(); it != m.end(); ++it) {
        std::map<std::string, std::string>::iterator f = entries.find(it->first);
        if (f == entries.end()) { if (OptionalComp(it->first)) continue; err = std::string("missing component entry: '") + it->first + "'"; return false; }
        /* 'Dos' is variable-length by design (open-file table), so equal length is not a format signal; the consumed-length check below still applies. */
        if (it->first != "Dos" && f->second.size() != snap[it->first].size()) {
            std::ostringstream o; o << "component length mismatch: '" << it->first << "' saved=" << f->second.size() << " current=" << snap[it->first].size() << " (saved by build '" << build << "')";
            err = o.str(); return false;
        }
    }
    if (build != GENSEI_BUILD_ID) LOG_MSG("PC98_SaveState: loading state saved by build '%s' into '%s' (format v%u, same component table)", build.c_str(), GENSEI_BUILD_ID, (unsigned)ver);
    for (CompMap::iterator it = m.begin(); it != m.end(); ++it) {
        std::map<std::string, std::string>::iterator f = entries.find(it->first);
        if (f == entries.end()) {
            if (OptionalComp(it->first)) { LOG_MSG("PC98_SaveState: component '%s' not in file (older state), kept as is", it->first.c_str()); continue; }
            Rollback(snap);
            err = std::string("missing component entry: '") + it->first + "' (rolled back)";
            return false;
        }
        std::istringstream ss(f->second, std::ios::binary);
        it->second->setBytes(ss);
        std::streamoff got = ss.tellg();
        if (ss.fail() || got != (std::streamoff)f->second.size()) {
            Rollback(snap);
            err = std::string("component load failed: '") + it->first + "' (rolled back)";
            return false;
        }
        LOG_MSG("PC98_SaveState: loaded component '%s' (%u bytes)", it->first.c_str(), (unsigned)f->second.size());
    }
    LOG_MSG("PC98_SaveState: load done, PIC unknown events: %u", g_pic_state_unknown_events);
    return true;
}

} /* namespace */

bool PC98_SaveState_ToMemory(std::vector<uint8_t> &out, const std::vector<Pc98StateDisk> &disks, std::string &err, const std::vector<uint8_t> *thumb) {
    (void)err;
    return BuildImage(out, disks, thumb);
}

/* ---- 이하 방(팀장)이 직접 작성: 뮤즈 산출물이 209행 FromMemory 선언 도중에서 잘려 있었다(파일 끝에 "...[truncated N chars]" 문구가 박힘). ---- */
bool PC98_SaveState_FromMemory(const std::vector<uint8_t> &in, const std::vector<Pc98StateDisk> &disks, std::string &err) {
    if (in.empty()) { err = "empty image"; return false; }
    return ApplyImage(&in[0], in.size(), disks, err);
}

bool PC98_SaveState_ToFile(const char *path, const std::vector<Pc98StateDisk> &disks, std::string &err, const std::vector<uint8_t> *thumb) {
    std::vector<uint8_t> img;
    if (!BuildImage(img, disks, thumb)) { err = "build failed"; return false; }
#if !defined(_WIN32)
    /* Non-Windows build (Android): plain write; the host keeps its own backup logic. */
    { FILE *pf = fopen(path, "wb"); if (!pf) { err = std::string("cannot open for write: ") + path; return false; }
      size_t pn = fwrite(&img[0], 1, img.size(), pf); fclose(pf);
      if (pn != img.size()) { err = "short write"; return false; } return true; }
#else
    /* t53a/F5: 임시 파일에 쓰고 검증한 뒤 교체. 기존 파일은 지우지 않고 .bak(읽을 수 있던 것) / .unreadable(못 읽던 것)로 보존. */
    const std::string spath(path), tmp = spath + ".tmp";
    const char *stop = getenv("GENSEI_SS_TEST_STOP");   /* 시험 전용: "tmp"=임시 파일 검증 뒤, "bak"=백업 뒤 정지(강제 종료 흉내). 기본 미설정 */
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) { err = std::string("cannot open for write: ") + tmp; return false; }
    size_t n = fwrite(&img[0], 1, img.size(), f);
    bool flushed = (fflush(f) == 0) && (_commit(_fileno(f)) == 0);
    fclose(f);
    if (n != img.size() || !flushed) { DeleteFileA(tmp.c_str()); err = "short write (temp file)"; return false; }
    {   /* 읽어 되짚기: 크기·전체 CRC */
        std::vector<uint8_t> back; std::string e2;
        if (!ReadWhole(tmp.c_str(), back) || back.size() != img.size() || CrcOf(&back[0], back.size() - 4) != CrcOf(&img[0], img.size() - 4)) {
            DeleteFileA(tmp.c_str()); err = "verify failed (temp file)"; return false;
        }
    }
    if (stop && !std::strcmp(stop, "tmp")) { err = "test stop after temp"; return false; }
    {   /* 기존 파일 분류 → 백업 */
        std::vector<uint8_t> old;
        if (ReadWhole(spath.c_str(), old)) {
            std::string e3; Reader rr(0, 0); std::string bld; uint32_t ver = 0;
            bool readable = ParseHeader(old.empty() ? (const uint8_t *)"" : &old[0], old.size(), rr, disks, bld, ver, e3);
            std::string dest;
            if (readable) dest = spath + ".bak";
            else { dest = spath + ".unreadable"; if (FileExists(dest)) dest = spath + ".unreadable2"; }
            if (!CopyFileA(spath.c_str(), dest.c_str(), 0)) { DeleteFileA(tmp.c_str()); err = "cannot back up previous file: " + dest; return false; }
            LOG_MSG("PC98_SaveState: previous file kept as %s (%s)", dest.c_str(), readable ? "readable" : e3.c_str());
        }
    }
    if (stop && !std::strcmp(stop, "bak")) { err = "test stop after backup"; return false; }
    if (!MoveFileExA(tmp.c_str(), spath.c_str(), 1 /*REPLACE_EXISTING*/ | 8 /*WRITE_THROUGH*/)) { DeleteFileA(tmp.c_str()); err = "cannot replace save file"; return false; }
    LOG_MSG("PC98_SaveState: wrote %u bytes to %s", (unsigned)img.size(), path);
    return true;
#endif
}

bool PC98_SaveState_FromFile(const char *path, const std::vector<Pc98StateDisk> &disks, std::string &err) {
    FILE *f = fopen(path, "rb");
    if (!f) { err = std::string("cannot open for read: ") + path; return false; }
    std::vector<uint8_t> img;
    uint8_t tmp[65536];
    for (;;) { size_t n = fread(tmp, 1, sizeof(tmp), f); if (!n) break; img.insert(img.end(), tmp, tmp + n); }
    fclose(f);
    return PC98_SaveState_FromMemory(img, disks, err);
}

/* ---- SaveState 클래스 멤버 정의(원본 savestates.cpp 대체; F_required_symbols.txt 1~8) ---- */
SaveState& SaveState::instance() {
    static SaveState singleton;
    return singleton;
}

void SaveState::registerComponent(const std::string& uniqueName, Component& comp) {
    components.insert(std::make_pair(uniqueName, CompData(comp)));
    Registry()[uniqueName] = &comp;   /* 이름 순 순회용 거울 */
}

void SaveState::save(size_t) {}
void SaveState::load(size_t) const {}
bool SaveState::isEmpty(size_t) const { return true; }
void SaveState::removeState(size_t) const {}
std::string SaveState::getName(size_t, bool) const { return std::string(); }

void AddSaveStateMapper(void) {}

/* ---- r011: 슬롯 미리보기용. 파일 앞부분만 읽어 THMB(PNG)를 꺼낸다(전체 CRC 는 검사하지 않음 — 목록 표시용). ---- */
static bool ThumbRd(FILE *f, void *dst, size_t n) { return fread(dst, 1, n, f) == n; }
static bool ThumbU32(FILE *f, uint32_t &v) { uint8_t b[4]; if (!ThumbRd(f, b, 4)) return false; v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); return true; }
static bool ThumbStr(FILE *f) { uint8_t b[2]; if (!ThumbRd(f, b, 2)) return false; unsigned n = b[0] | (b[1] << 8); return fseek(f, (long)n, SEEK_CUR) == 0; }
bool PC98_SaveState_ReadThumb(const char *path, std::vector<uint8_t> &png, std::string &err) {
    png.clear();
    FILE *f = fopen(path, "rb");
    if (!f) { err = std::string("cannot open: ") + path; return false; }
    bool ok = false; uint8_t mg[4]; uint32_t ver = 0, tmp = 0, nd = 0, tl = 0;
    do {
        if (!ThumbRd(f, mg, 4) || std::memcmp(mg, kMagic, 4) != 0) { err = "bad magic"; break; }
        if (!ThumbU32(f, ver) || (ver != 2 && ver != kFormatVersion)) { err = "format version mismatch"; break; }
        if (!ThumbStr(f) || (ver >= 3 && !ThumbU32(f, tmp)) || !ThumbU32(f, tmp) || !ThumbU32(f, tmp) || !ThumbU32(f, nd)) { err = "truncated header"; break; }
        bool bad = false;
        for (uint32_t i = 0; i < nd && !bad; i++) { uint32_t c; uint8_t s8[8]; if (!ThumbStr(f) || !ThumbRd(f, s8, 8) || !ThumbU32(f, c)) bad = true; }
        if (bad || !ThumbU32(f, tl)) { err = "truncated disk list"; break; }
        if (tl > (64u << 20)) { err = "THMB too large"; break; }
        png.resize(tl);
        if (tl && !ThumbRd(f, &png[0], tl)) { err = "truncated THMB"; png.clear(); break; }
        ok = true;
    } while (0);
    fclose(f);
    return ok;
}
