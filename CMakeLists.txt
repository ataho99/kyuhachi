# Kyuhachi — S1/S2: 저장소에서 바로 빌드 (MSVC · 미리 빌드 obj). 옛 스크립트(legacy/work_*/dbx_host_*.sh)는 병행 보존.
# 사용: cmake/build.sh <빌드명> <빌드번호>   (예: cmake/build.sh t54a 62)  — 출력 = build\<빌드명>\
cmake_minimum_required(VERSION 3.20)
if(NOT KH_BUILD)
  message(FATAL_ERROR "KH_BUILD (빌드명, 예 t54a) 와 KH_VER_BUILD (번호) 를 -D 로 주세요")
endif()
project(kyuhachi LANGUAGES CXX)
set(CMAKE_CXX_FLAGS "")            # 옛 빌드와 같은 옵션만 쓴다(CMake 기본 /GR /D_WINDOWS 등 제외)
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
set(CMAKE_CXX_FLAGS_MINSIZEREL "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
include(${CMAKE_SOURCE_DIR}/cmake/prebuilt_objs.cmake)   # KH_PRE_A/MENU/S9/PAGING/D/LIBS (third_party/prebuilt.lock 과 같은 목록)

set(KH_MSVC   "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207" CACHE PATH "")
set(KH_SDK    "C:/Program Files (x86)/Windows Kits/10" CACHE PATH "")
set(KH_SDKV   "10.0.26100.0" CACHE STRING "")
set(KH_LINK   "${KH_MSVC}/bin/Hostx64/x64/link.exe")
set(KH_ML64   "${KH_MSVC}/bin/Hostx64/x64/ml64.exe")
set(KH_RC     "${KH_SDK}/bin/${KH_SDKV}/x64/rc.exe")
set(GEN "${CMAKE_BINARY_DIR}/gen")
set(SRC "${CMAKE_SOURCE_DIR}/src")
set(ANA "Y:/dbx_ws/ana/src/vs")
set(R4  "Y:/dbx_ws/r004b/src/src")

# ---- 빌드 이름에서 만드는 헤더 (옛 빌드: kh_version.h · gensei_build_id.h)
math(EXPR KH_VER_BUILD_N "${KH_VER_BUILD}")
file(WRITE ${GEN}/kh_version.h.tmp "/* kh_version.h -- single source of the version (ASCII only: also read by rc.exe).
 * Used by: window title, about screen, boot screen, top-right label, exe VERSIONINFO, README, zip name.
 * Not changeable from strings.txt. Display string = public version + \"-\" + internal build name. */
#ifndef KH_VERSION_H
#define KH_VERSION_H
#define KH_VER_MAJOR 0
#define KH_VER_MINOR 6
#define KH_VER_PATCH 2
#define KH_VER_BUILD ${KH_VER_BUILD_N}
#define KH_BUILD \"${KH_BUILD}\"
#define KH_VERSION \"0.6.5-${KH_BUILD}\"
#endif
")
file(WRITE ${GEN}/gensei_build_id.h.tmp "#define GENSEI_BUILD_ID \"r024${KH_BUILD}\"\n")
configure_file(${GEN}/kh_version.h.tmp ${GEN}/kh_version.h COPYONLY)
configure_file(${GEN}/gensei_build_id.h.tmp ${GEN}/gensei_build_id.h COPYONLY)

# ---- 컴파일 옵션 (cl_host.rsp · cl9.rsp 와 같은 것. MSVC 표준 include 는 환경변수 INCLUDE 로 받는다)
set(KH_CL /nologo /W3 /EHsc /MT /O2 /Zi /FS /Zc:wchar_t /Zc:forScope /Zc:inline /fp:precise /GS /Gd /FC /utf-8
          /DWIN32 /D__WIN32__ /D_CRT_SECURE_NO_WARNINGS /D_FILE_OFFSET_BITS=64 /DC_SDL2=1 /D_MBCS)
set(KH_INC_BASE
  ${GEN} ${SRC}
  ${ANA}/pcap ${ANA} ${ANA}/.. ${ANA}/../include ${ANA}/../src ${ANA}/../src/aviwriter
  ${ANA}/../src/hardware/snd_pc98/cbus ${ANA}/../src/hardware/snd_pc98/common ${ANA}/../src/hardware/snd_pc98/generic
  ${ANA}/../src/hardware/snd_pc98/sound ${ANA}/../src/hardware/snd_pc98/x11 ${ANA}/../src/hardware/snd_pc98/sound/getsnd
  ${ANA}/../src/libs/mt32 ${ANA}/libpdcurses ${ANA}/zlib ${ANA}/libpng ${ANA}/freetype/include ${ANA}/sdl2/include ${ANA}/sdlnet)
set(KH_INC_CORE ${R4}/agent ${R4}/aviwriter ${R4}/builtin ${R4}/cpu ${R4}/debug ${R4}/dos ${R4}/fpu ${R4}/gamelink ${R4}/gui
  ${R4}/hardware ${R4}/ints ${R4}/libs ${R4}/misc ${R4}/output ${R4}/platform ${R4}/shell ${R4}/tool)
set(KH_FI /FI${GEN}/gensei_build_id.h)

# S2: bios·mixer 는 DOSBox-X 원본 + core_patches 로 build\<이름>\core_gen\ 에 만들어진 것을 쓴다(없으면 S1 방식: src/ 의 사본)
set(KH_CORE_GEN "${CMAKE_BINARY_DIR}/core_gen")
if(EXISTS "${CMAKE_SOURCE_DIR}/DOSBOX_X.lock")
  foreach(f bios_B.cpp mixer_P.cpp)
    if(NOT EXISTS "${KH_CORE_GEN}/${f}")
      message(FATAL_ERROR "core_gen/${f} 없음 — cmake/build.sh 로 빌드하세요(tools/apply_core_patches.sh 먼저 실행)")
    endif()
  endforeach()
  set(KH_BIOS_SRC "${KH_CORE_GEN}/bios_B.cpp")
  set(KH_MIXER_SRC "${KH_CORE_GEN}/mixer_P.cpp")
else()
  set(KH_BIOS_SRC "${SRC}/bios_B.cpp")
  set(KH_MIXER_SRC "${SRC}/mixer_P.cpp")
endif()

function(kh_obj name file)   # 한 소스 = 한 OBJECT 타깃(링크 순서를 이름으로 고정)
  add_library(${name} OBJECT ${file})
  target_compile_options(${name} PRIVATE ${KH_CL} ${ARGN})
endfunction()
kh_obj(kh_host      ${SRC}/gensei_host.cpp     /DGH_DOSKERNEL ${KH_FI})
kh_obj(kh_bios      ${KH_BIOS_SRC})
kh_obj(kh_savestate ${SRC}/pc98_savestate.cpp  ${KH_FI})
kh_obj(kh_fmstate   ${SRC}/pc98_fm_state.cpp)
kh_obj(kh_mixer     ${KH_MIXER_SRC})
foreach(t kh_host)
  target_include_directories(${t} PRIVATE ${KH_INC_BASE})
endforeach()
foreach(t kh_bios kh_savestate kh_fmstate kh_mixer)
  target_include_directories(${t} PRIVATE ${KH_INC_BASE} ${KH_INC_CORE})
endforeach()
add_library(kh_stubnames OBJECT ${SRC}/stubs_1_names.cpp)
target_compile_options(kh_stubnames PRIVATE /nologo /MT /EHsc)

# ---- 어셈블러 스텁(고정 stubs_1) · 리소스(VERSIONINFO)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/stubs_1.obj
  COMMAND "${KH_ML64}" /nologo /c /Fo${CMAKE_BINARY_DIR}/stubs_1.obj ${SRC}/stubs_1.asm
  DEPENDS ${SRC}/stubs_1.asm VERBATIM)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/kyuhachi.res
  COMMAND "${KH_RC}" /nologo /I${KH_SDK}/Include/${KH_SDKV}/um /I${KH_SDK}/Include/${KH_SDKV}/shared /I${GEN} /I${SRC} /fo ${CMAKE_BINARY_DIR}/kyuhachi.res ${SRC}/kyuhachi.rc
  DEPENDS ${SRC}/kyuhachi.rc ${SRC}/kyuhachi.ico ${GEN}/kh_version.h VERBATIM)

# ---- 링크 (옛 스크립트와 같은 순서: 미리 빌드 목록 → host → menu → s9 → s17(bios·savestate·fmstate) → paging → mixer → 나머지 → stubs → 라이브러리 → res)
file(READ ${SRC}/stubs_1.altrsp KH_ALT)
set(KH_LIBS_SYS opengl32.lib kernel32.lib user32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib winmm.lib ws2_32.lib gdi32.lib imm32.lib setupapi.lib version.lib iphlpapi.lib uuid.lib comdlg32.lib dbghelp.lib shlwapi.lib)
set(KH_RSP "/nologo\n\"/LIBPATH:${KH_MSVC}/lib/x64\"\n\"/LIBPATH:${KH_SDK}/Lib/${KH_SDKV}/ucrt/x64\"\n\"/LIBPATH:${KH_SDK}/Lib/${KH_SDKV}/um/x64\"\n/MACHINE:X64\n/DEBUG\n/SUBSYSTEM:WINDOWS\n/ENTRY:mainCRTStartup\n\"/OUT:${CMAKE_BINARY_DIR}/KyuhachiX.exe\"\n")
foreach(o ${KH_PRE_A})
  string(APPEND KH_RSP "\"${o}\"\n")
endforeach()
string(APPEND KH_RSP "\"$<TARGET_OBJECTS:kh_host>\"\n")
foreach(o ${KH_PRE_MENU} ${KH_PRE_S9})
  string(APPEND KH_RSP "\"${o}\"\n")
endforeach()
string(APPEND KH_RSP "\"$<TARGET_OBJECTS:kh_bios>\"\n\"$<TARGET_OBJECTS:kh_savestate>\"\n\"$<TARGET_OBJECTS:kh_fmstate>\"\n")
foreach(o ${KH_PRE_PAGING})
  string(APPEND KH_RSP "\"${o}\"\n")
endforeach()
string(APPEND KH_RSP "\"$<TARGET_OBJECTS:kh_mixer>\"\n")
foreach(o ${KH_PRE_D})
  string(APPEND KH_RSP "\"${o}\"\n")
endforeach()
string(APPEND KH_RSP "\"${CMAKE_BINARY_DIR}/stubs_1.obj\"\n\"$<TARGET_OBJECTS:kh_stubnames>\"\n")
string(APPEND KH_RSP "${KH_ALT}\n")
foreach(o ${KH_PRE_LIBS})
  string(APPEND KH_RSP "\"${o}\"\n")
endforeach()
foreach(o ${KH_LIBS_SYS})
  string(APPEND KH_RSP "${o}\n")
endforeach()
string(APPEND KH_RSP "\"${CMAKE_BINARY_DIR}/kyuhachi.res\"\n")
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/link.rsp CONTENT "${KH_RSP}")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/KyuhachiX.exe
  COMMAND "${KH_LINK}" @${CMAKE_BINARY_DIR}/link.rsp
  DEPENDS kh_host kh_bios kh_savestate kh_fmstate kh_mixer kh_stubnames ${CMAKE_BINARY_DIR}/stubs_1.obj ${CMAKE_BINARY_DIR}/kyuhachi.res ${CMAKE_BINARY_DIR}/link.rsp
  VERBATIM)
add_custom_target(kyuhachi ALL DEPENDS ${CMAKE_BINARY_DIR}/KyuhachiX.exe)
