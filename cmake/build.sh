#!/bin/bash
# 사용: cmake/build.sh <빌드명> <빌드번호>   — WSL 에서 BuildTools 의 Windows cmake·ninja·cl 을 부른다. 출력 build\<빌드명>\ (이미 있으면 거부: 덮어쓰지 않음)
set -u
NAME=${1:?빌드명}; VB=${2:?빌드번호}
P=/mnt/h/RETRO_SET/PC98_PLAYER; OUTW="H:/RETRO_SET/PC98_PLAYER/build/$NAME"
[ -e "$P/build/$NAME" ] && { echo "거부: build\\$NAME 이미 있음"; exit 2; }
BT="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools"; MS="$BT/VC/Tools/MSVC/14.44.35207"; SK="C:/Program Files (x86)/Windows Kits/10"; SV=10.0.26100.0
CMW="/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
NJ="$BT/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
export INCLUDE="$MS/include;$SK/Include/$SV/ucrt;$SK/Include/$SV/um;$SK/Include/$SV/shared"
export LIB="$MS/lib/x64;$SK/Lib/$SV/ucrt/x64;$SK/Lib/$SV/um/x64"
export WSLENV=INCLUDE:LIB
"$CMW" -S 'H:/RETRO_SET/PC98_PLAYER/kyuhachi' -B "$OUTW" -G Ninja -DCMAKE_MAKE_PROGRAM="$NJ" -DCMAKE_CXX_COMPILER="$MS/bin/Hostx64/x64/cl.exe" -DCMAKE_LINKER="$MS/bin/Hostx64/x64/link.exe" -DCMAKE_RC_COMPILER="$SK/bin/$SV/x64/rc.exe" -DCMAKE_MT="$SK/bin/$SV/x64/mt.exe" -DCMAKE_BUILD_TYPE=Release -DKH_BUILD="$NAME" -DKH_VER_BUILD="$VB" 2>&1 | tr -d '\r'
echo "== configure rc=${PIPESTATUS[0]}"
"$CMW" --build "$OUTW" 2>&1 | tr -d '\r'
echo "== build rc=${PIPESTATUS[0]}"
ls -l "$P/build/$NAME/Kyuhachi.exe" 2>&1
