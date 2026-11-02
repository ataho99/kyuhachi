#ifndef PC98_SAVESTATE_H
#define PC98_SAVESTATE_H
/* pc98_savestate.h — GENSEI 미니 에뮬레이터 세이브스테이트 드라이버 인터페이스 (포맷 G98S v2, r011).
 * 파일 입출력 경로는 호스트가 UTF-8 로 준다(Windows 변환은 호스트 몫). */

#include <cstdint>
#include <string>
#include <vector>

struct Pc98StateDisk {
    std::string name;   /* 이미지 파일명(호스트가 넘긴 식별자) */
    uint64_t    size;   /* 바이트 */
    uint32_t    crc32;  /* 이미지 내용 CRC32(호스트 계산) */
};

/* 파일 왕복. 헤더(빌드ID·기종·메모리 페이지·디스크 목록) 불일치 시 거부(false + err 사유). v1 파일은 거부.
 * thumb = 저장 순간 원본 640x400 화면 PNG(셰이더·확대 전). 헤더 바로 뒤 THMB 항목으로 들어간다. NULL/빈 = 없음. */
bool PC98_SaveState_ToFile(const char *path, const std::vector<Pc98StateDisk> &disks, std::string &err, const std::vector<uint8_t> *thumb = 0);
bool PC98_SaveState_FromFile(const char *path, const std::vector<Pc98StateDisk> &disks, std::string &err);

/* 슬롯 목록용: 상태 파일 앞부분만 읽어 THMB PNG 바이트를 꺼낸다(없으면 png 비어 있고 true). */
bool PC98_SaveState_ReadThumb(const char *path, std::vector<uint8_t> &png, std::string &err);

/* 메모리 왕복. 롤백용 스냅샷(로드 전 현 상태 보관)·빠른 저장 겸용. */
bool PC98_SaveState_ToMemory(std::vector<uint8_t> &out, const std::vector<Pc98StateDisk> &disks, std::string &err, const std::vector<uint8_t> *thumb = 0);
bool PC98_SaveState_FromMemory(const std::vector<uint8_t> &in, const std::vector<Pc98StateDisk> &disks, std::string &err);

#endif /* PC98_SAVESTATE_H */
