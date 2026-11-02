# 큐하치 안드로이드(APK) 조사 — 2026-11-03 (대장방 위임 조사, 웹 자료만, 빌드 없음)

결론: DOSBox-X 로 PC-98 을 돌리는 Android 빌드는 공식·포크·libretro·Termux 어디에도 없음. 같은 소스에 SDL2 android-project 템플릿 + CMake 를 붙여 APK 를 직접 빌드하는 길이 기본. 최대 변수 = arm64 CPU 속도.

## 확인된 것
- 공식 Android 빌드 없음. libretro 코어 요청(#893)은 2018년부터 미해결(2025-01 메인테이너: 진행 없음). Termux+SDL2 컴파일 사례(#2658)는 APK 아님.
- SDL1 은 개조된 내장 SDL1 필요 → Android 는 SDL2 로 간다. 빌드 체계 autotools 가 가장 큰 걸림돌.
- 다른 Android DOSBox(Magic DOSBox·DosBox Turbo·dosbox-pure)는 원본 계열, PC-98 없음. PC-98 대안 = NP2kai(libretro, 노바에서 쓰는 것).
- arm64 동적 코어 백엔드(risc_armv8le.h) 소스에 있음. 2020 M1 에서 동작 보고 / 2022 arm64 리눅스에서는 일반 코어로 떨어짐 보고. Android 에서 동작 여부·일반 코어 실측 속도 = 미확인(실기 측정 필요).
- APK: SDL2 android-project(compile/target 35, min 21), cmake 모드, arm64-v8a 만. 코드는 공유라이브러리(main)로. config.h 를 Android 용으로 고정, PC-98 에 필요한 src 파일만 CMake 에 나열.
- 저장소(Android 13): MANAGE_EXTERNAL_STORAGE(사이드로드면 문제없음) → /sdcard/Kyuhachi/{games,fonts} 직접 fopen. SAF 비추천.

## 구조 제안 (소스 하나 → exe + apk)
core/(DOSBox-X PC-98 부분) · ui/(SDL2 UI 공통, 입력·해상도만 분기) · platform/(경로·권한·로그·파일선택 — Windows/Android 별도). #ifdef __ANDROID__ 는 platform/ 에만. CMakeLists.txt 하나로 MinGW·NDK 둘 다.
공수 추정: CMake 전환 2~4일 · 첫 APK 기동 2~3일 · 입력·화면 1주 · 성능 미지수.

## 권장 5단계
1. Windows 빌드를 CMake 로 옮기고 platform/ 분리 — Windows 결과물이 지금과 같은지 확인.
2. WSL2 에 NDK·Gradle, SDL2 android-project 를 cmake 모드·arm64 만으로.
3. Android config.h 고정, dynrec 끈 일반 코어로 노바에서 화면·소리 확인.
4. MANAGE_EXTERNAL_STORAGE 로 games·fonts 붙이고 환세 1편 실행.
5. dynrec 켜고 일반 코어와 속도 비교 → 어느 쪽으로 갈지 결정.

## 위험 Top 3
1. arm64 dynrec 불안정 → 일반 코어만으로 486급 게임 속도 부족 가능.
2. autotools→CMake 전환 중 Windows 빌드 회귀(동시개발이 양쪽 같이 깨짐).
3. 터치·패드 ↔ PC-98 키보드 매핑 공백.

Sources: github.com/joncampbell123/dosbox-x/issues/893 · /issues/2658 · dosbox-x configure.ac · BUILD.md · vogons.org t=31881 · magicbox.imejl.sk ?p=7233 · ?p=11202 · dosbox-staging discussions/3773 · github.com/meesokim/dosbox-svn-android · github.com/AZO234/NP2kai · SDL2 docs/README-android.md · SDL2 android-project build.gradle · developer.android.com/training/data-storage/manage-all-files
