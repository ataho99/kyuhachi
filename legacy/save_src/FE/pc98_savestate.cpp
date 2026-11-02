/* pc98_savestate.cpp — GENSEI 미니 에뮬레이터 세이브스테이트 드라이버 F.
 * S/src/misc/savestates.cpp 대체. savestates.obj 대신 본체 링크에 들어간다.
 * 원본의 GUI·셸·설정 의존(tinyfd, GUI_Shortcut, systemmessagebox, RunningProgram,
 * control->GetSection, MAPPER, flagged_backup/restore)은 전부 제거.
 * 컨테이너는 minizip 대신 단순 TLV: 매직 "G98S"+포맷버전, 헤더, 컴포넌트 순회,
 * 끝에 CRC32(전체). 컴포넌트 순회·로드 순서는 원본과 같이 std::map 이름 순.
 * 파일 입출력은 FILE*/fread/fwrite. 경로는 호스트가 UTF-8 로 준다.
 * 링크 주의: pic.cpp.E 가 pic.obj 를 대체 링크할 때 g_pic_state_unknown_events 를 정의한다.
 */
#include <cstdio>
#include <cstring>
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
const uint32_t kFormatVersion = 1;

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

bool BuildImage(std::vector<uint8_t> &out, const std::vector<Pc98StateDisk> &disks) {
    out.clear();
    out.insert(out.end(), kMagic, kMagic + 4);
    Put32(out, kFormatVersion);
    PutStr(out, GENSEI_BUILD_ID);
    Put32(out, (uint32_t)MCH_PC98);
    Put32(out, (uint32_t)MEM_TotalPages());
    Put32(out, (uint32_t)disks.size());
    for (size_t i = 0; i < disks.size(); i++) {
        PutStr(out, disks[i].name);
        Put64(out, disks[i].size);
        Put32(out, disks[i].crc32);
    }
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

bool ApplyImage(const uint8_t *buf, size_t len, const std::vector<Pc98StateDisk> &disks, std::string &err) {
    if (len < 12) { err = "image too short"; return false; }
    if (std::memcmp(buf, kMagic, 4) != 0) { err = "bad magic (not a GENSEI state)"; return false; }
    uint32_t stored_crc = 0;
    for (int i = 0; i < 4; i++) stored_crc |= ((uint32_t)buf[len - 4 + i] << (i * 8));
    if (CrcOf(buf, len - 4) != stored_crc) { err = "CRC32 mismatch (corrupt image)"; return false; }
    Reader r(buf + 4, len - 8);
    if (r.U32() != kFormatVersion) { err = "format version mismatch"; return false; }
    std::string build = r.Str();
    if (r.fail) { err = "truncated header"; return false; }
    if (build != GENSEI_BUILD_ID) { err = std::string("build ID mismatch: saved='") + build + "' current='" + GENSEI_BUILD_ID + "'"; return false; }
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
    for (CompMap::iterator it = m.begin(); it != m.end(); ++it) {
        std::map<std::string, std::string>::iterator f = entries.find(it->first);
        if (f == entries.end()) {
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

bool PC98_SaveState_ToMemory(std::vector<uint8_t> &out, const std::vector<Pc98StateDisk> &disks, std::string &err) {
    (void)err;
    return BuildImage(out, disks);
}

bool PC98_SaveState_FromMemory(const std::vector<uint8_t> &in, const std::vector<Pc
...[truncated 2085 chars]