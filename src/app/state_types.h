/* app/state_types.h -- S9d: AppState 멤버로 쓰이는 타입 정의만 먼저 옮김 (본문 불변, 옛 위치: frame.h · opts_log.h · video_sink.h) */
struct FtThumb { std::vector<uint32_t> px; std::string label; };
struct GProfile {
    const char * id = "";
    const char * disk[4] = { NULL, NULL, NULL, NULL };   /* index 0=A 1=B 2=C 3=D (bios_disk index) */
    char bootletter = 'A';       /* 'A'..'D' */
    std::vector<GKey> keys;      /* ms after guest boot */
    std::vector<GKey> bootkeys;  /* t36cd: 부팅 후 키·명령 주입(--autokeys 없이도 동작). bootkeys=3000:type=dir~,9000:enter */
    std::string cd, autoexec;    /* t36cd: CD 슬롯(이미지 경로 또는 이름) · 내장 DOS 의 autoexec 줄(| 로 구분) */
    std::string title, engine, src, reason, imgdir, cycles, font, extra, never_map;
    std::map<std::string, std::string> ov, pad, kv;
    std::vector<std::string> swap; bool install_required = false;
};
struct GMouse { unsigned at_ms; float dx, dy; int act; };   /* act: 0 이동만, 1 좌 누름, 2 좌 뗌, 3 우 누름, 4 우 뗌 */
enum { FIT_43 = 0, FIT_1610 = 1, FIT_INT = 2 };
enum { SH_NEAREST = 0, SH_SHARP = 1, SH_CRT = 2, SH_N = 3 };
struct GProg { unsigned int p; int uTex, uSrc, uOut, uCurve, uScan, uMask, uBleed, uGlow, uNear, uBeam, uBreath,
    uTime, uGlowP, uGTex; bool ok, linear; };
