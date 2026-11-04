#!/bin/bash
# 사용: cmake/build.sh <빌드명> <빌드번호>   — WSL 에서 BuildTools 의 Windows cmake·ninja·cl 을 부른다. 출력 build\<빌드명>\ (이미 있으면 거부: 덮어쓰지 않음)
# 선택 환경변수: KH_SRC_L/KH_SRC_W = 소스 폴더(WSL/Windows 표기, 기본 저장소) · KH_OUT = 출력 폴더 이름(기본 = 빌드명; 재현 시험용)
# S2: 소스에 tools/apply_core_patches.sh 가 있으면 먼저 실행해 build\<출력>\core_gen\ 에 bios_B.cpp·mixer_P.cpp 를 만든다.
set -u
NAME=${1:?빌드명}; VB=${2:?빌드번호}
P=/mnt/h/RETRO_SET/PC98_PLAYER; OUTN=${KH_OUT:-$NAME}; OUTW="H:/RETRO_SET/PC98_PLAYER/build/$OUTN"
SRCL=${KH_SRC_L:-$P/kyuhachi}; SRCW=${KH_SRC_W:-H:/RETRO_SET/PC98_PLAYER/kyuhachi}
[ -e "$P/build/$OUTN" ] && { echo "거부: build\\$OUTN 이미 있음"; exit 2; }
if [ -f "$SRCL/tools/apply_core_patches.sh" ]; then bash "$SRCL/tools/apply_core_patches.sh" "$P/build/$OUTN/core_gen" || { echo "거부: 코어 패치 적용 실패"; exit 3; }; fi
BT="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools"; MS="$BT/VC/Tools/MSVC/14.44.35207"; SK="C:/Program Files (x86)/Windows Kits/10"; SV=10.0.26100.0
CMW="/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
NJ="$BT/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
export INCLUDE="$MS/include;$SK/Include/$SV/ucrt;$SK/Include/$SV/um;$SK/Include/$SV/shared"
export LIB="$MS/lib/x64;$SK/Lib/$SV/ucrt/x64;$SK/Lib/$SV/um/x64"
export WSLENV=INCLUDE:LIB
"$CMW" -S "$SRCW" -B "$OUTW" -G Ninja -DCMAKE_MAKE_PROGRAM="$NJ" -DCMAKE_CXX_COMPILER="$MS/bin/Hostx64/x64/cl.exe" -DCMAKE_LINKER="$MS/bin/Hostx64/x64/link.exe" -DCMAKE_RC_COMPILER="$SK/bin/$SV/x64/rc.exe" -DCMAKE_MT="$SK/bin/$SV/x64/mt.exe" -DCMAKE_BUILD_TYPE=Release -DKH_BUILD="$NAME" -DKH_VER_BUILD="$VB" 2>&1 | tr -d '\r'
echo "== configure rc=${PIPESTATUS[0]}"
"$CMW" --build "$OUTW" 2>&1 | tr -d '\r'
echo "== build rc=${PIPESTATUS[0]}"
ls -l "$P/build/$OUTN/Kyuhachi.exe" 2>&1
mkdir -p "$P/build/$OUTN" && cp "$SRCL/res/strings/"strings_*.txt "$P/build/$OUTN/" 2>/dev/null; echo "== strings copied from res/strings: $(ls "$P/build/$OUTN"/strings_*.txt 2>/dev/null | wc -l) files"
[ -s "$P/build/$OUTN/Kyuhachi.exe" ]   # exit status = exe exists (the strings copy above must not hide a failed build)
