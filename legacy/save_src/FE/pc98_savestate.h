#ifndef PC98_SAVESTATE_H
#define PC98_SAVESTATE_H
/* pc98_savestate.h — GENSEI 미니 에뮬레이터 세이브스테이트 드라이버 F 인터페이스.
 * S/src/misc/savestates.cpp 대체. 본체 링크에서 savestates.obj 대신 pc98_savestate.obj 가 들어간다.
 * 파일 입출력 경로는 호스트가 UTF-8 로 준다(Windows 변환은 호스트 몫). */

#include <cstdint>
#include <string>
#include <vector>

struct Pc98StateDisk {
    std::string name;   /* 이미지 파일명(호스트가 넘긴 식별자) */
    uint64_t    size;   /* 바이트 */
    uint32_t    crc32;  /* 이미지 내용 CRC32(호스트 계산) */
};

/* 파일 왕복. 헤더(빌드ID·기종·메모리 페이지·디스크 목록) 불일치 시 거부(false + err 사유). */
bool PC98_SaveState_ToFile(const char *path, const std::vector<Pc98StateDisk> &disks, std::string &err);
bool PC98_SaveState_FromFile(const char *path, const std::vector<Pc98StateDisk> &disks, std::string &err);

/* 메모리 왕복. 롤백용 스냅샷(로드 전 현 상태 보관)·빠른 저장 겸용. 빈 컴포넌트에서도 왕복 성립. */
bool PC98_SaveState_ToMemory(std::vector<uint8_t> &out, const std::vector<Pc98StateDisk> &disks, std::string &err);
bool PC98_SaveState_FromMemory(const std::vector<uint8_t> &in, const std::vector<Pc98StateDisk> &disks, std::string &err);

#endif /* PC98_SAVESTATE_H */
