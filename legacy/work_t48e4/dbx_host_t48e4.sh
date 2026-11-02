#!/bin/bash

# 산출 = build\r020\out_t48e4, exe = pc98_r024t48e4_test.exe (새 이름). 소스 = work\t48e4\ .
WS=${WS:-/mnt/y/dbx_ws}; export WS
set -u
D=/mnt/h/RETRO_SET/PC98_PLAYER
HS="$D/src/host_src"; SRC="$D/build/r020/work/src20"
OUT="$D/build/r020/out_t48e4"; mkdir -p "$OUT"; LOG="$OUT/build_log.txt"
B=${WS}/r024t48e4; mkdir -p "$B"
R9=${WS}/r009; R11=${WS}/r011
MSVC="/mnt/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207"
CL="$MSVC/bin/Hostx64/x64/cl.exe"; LINK="$MSVC/bin/Hostx64/x64/link.exe"; ML="$MSVC/bin/Hostx64/x64/ml64.exe"
WMSVC='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207'
WSDK='C:\Program Files (x86)\Windows Kits\10'; SDKV=10.0.26100.0
export LC_ALL=C.UTF-8
LIBP=("/LIBPATH:${WMSVC}"'\lib\x64' "/LIBPATH:${WSDK}"'\Lib\'"${SDKV}"'\ucrt\x64' "/LIBPATH:${WSDK}"'\Lib\'"${SDKV}"'\um\x64')
fresh() { [ -e "$1" ] && mv "$1" "$1.old_$(date +%H%M%S)"; mkdir -p "$1"; }   # 지우지 않고 옛것은 이름만 바꿔 둔다 (#243)
wp() { echo "$1" | sed 's#^/mnt/h#H:#; s#^/mnt/c#C:#; s#^/mnt/y#Y:#; s#/#\\#g'; }
findlib() { find ${WS}/r004b/src/obj ${WS}/ana/src/obj -iname "$1" 2>/dev/null | head -1; }
{
echo "== start $(date -Is)"
ls -l "$SRC" | awk '{print $5, $9}'
# ---------- 소스 배치
cp "$SRC/gensei_host_v12_t35.cpp" "$B/gensei_host.cpp"
for f in gensei_input12.h gensei_ov.h gensei_ui.h gensei_prof.h gensei_nkdb.h gensei_zip.h gensei_boostaud.h gensei_boostmix.inc; do cp "$SRC/$f" "$B/"; done; cp "$D/build/r020/work/t48e4/gensei_addwork_t40.h" "$B/gensei_addwork.h"; cp "$D/build/r020/work/t48e4/gensei_input12_t48e.h" "$B/gensei_input12.h"; cp "$D/build/r020/work/t48e4/gensei_padnames_t48e.h" "$B/gensei_padnames_t48e.h"; cp "$SRC/gensei_ui_t35.h" "$B/gensei_ui.h"
T39="$D/build/r020/work/t48e4"
for p in "gensei_host_v12_t39ui.cpp:gensei_host.cpp" "gensei_prof_t39.h:gensei_prof.h" "gensei_ov_t39.h:gensei_ov.h" "gensei_ui_t39.h:gensei_ui.h" "gensei_ent_t39.h:gensei_ent.h" "gensei_fontreg_t39.h:gensei_fontreg.h" "gensei_uilist_t39.h:gensei_uilist.h"; do cp "$T39/${p%%:*}" "$B/${p##*:}" || echo "MISSING SRC ${p%%:*}"; done
cp "$T39/gensei_hangul_t48.h" "$B/" || echo "MISSING SRC gensei_hangul_t48.h"
ls -l "$B"/gensei_host.cpp "$B"/gensei_prof.h "$B"/gensei_ov.h "$B"/gensei_ui.h "$B"/gensei_ent.h "$B"/gensei_fontreg.h "$B"/gensei_uilist.h | awk '{print $5, $9}'
for f in gensei_menu_names.inc pc98_savestate.h pc98_savestate.cpp; do cp "$R11/$f" "$B/"; done
sed 's/r011/r024t48e4/g' "$R11/cl_host.rsp" > "$B/cl_host.rsp"; echo '/utf-8' >> "$B/cl_host.rsp"; echo '/D GH_DOSKERNEL' >> "$B/cl_host.rsp"; mkdir -p "$B/rt"
sed 's/r011/r024t48e4/g' "$R11/cl9.rsp" > "$B/cl9.rsp"; echo '/Fd"Y:\dbx_ws\r024t48e4\vc9.pdb"' >> "$B/cl9.rsp"
printf '#define GENSEI_BUILD_ID "r024t48e4"\n' > "$B/gensei_build_id.h"
cp "$D/build/r020/work/t48e4/gensei_kbtable_t46.h" "$B/gensei_kbtable.h"; cp "$D/build/r020/work/t48e4/gensei_kbov_t46.h" "$B/gensei_kbov.h"
IMG="$D/build/구번들/bt_sdl2_r004b"; SHD="$D/build/r011/산출/shaders"
perl "$SRC/gen_embed.pl" "$B/gensei_embed.h" "$IMG/gensei16_neodgm_sbcs3.fnt" "$IMG/gensei16_neodgm.fnt" "$SHD/pc98_sharp.glsl" "$D/build/r020/work/t48e4/pc98_crt_t48c.glsl"   # #246-3: 글꼴·셰이더 내장
# ---------- 패치 사본: bios(스플래시 생략) · mixer(배속 소리)
SS="$D/src/save_src"
cp "$SS/B/bios.cpp.B" "$B/bios_B.cpp"
sed -i -E 's/(reinterpret_cast<const char\*>\(pc98_bios_fdc_[a-z]+), sizeof/\1), sizeof/' "$B/bios_B.cpp"
perl "$SRC/patch_bios.pl" "$B/bios_B.cpp"
cp "$SS/ref/mixer.cpp.orig" "$B/mixer_P.cpp"
perl "$SRC/patch_mixer_t35.pl" "$B/mixer_P.cpp" "$B/gensei_boostmix.inc" "Y:/dbx_ws/r024t48e4/gensei_boostaud.h"
# ---------- 호스트 컴파일
echo "== compile host"
rm -f "$B/gensei_host.obj"
"$CL" @'Y:\dbx_ws\r024t48e4\cl_host.rsp' /FI'Y:\dbx_ws\r024t48e4\gensei_build_id.h' /Fo'Y:\dbx_ws\r024t48e4\gensei_host.obj' 'Y:\dbx_ws\r024t48e4\gensei_host.cpp' 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > "$OUT/cl_host_out.txt"
echo "cl errors: $(grep -c 'error C' "$OUT/cl_host_out.txt")"; grep "error C" "$OUT/cl_host_out.txt" | head -40
cc17() { local nm=$1 sf=$2; shift 2
  "$CL" @'Y:\dbx_ws\r024t48e4\cl9.rsp' "$@" /Fo"Y:\\dbx_ws\\r024t48e4\\$nm.obj" "Y:\\dbx_ws\\r024t48e4\\$sf" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > "$OUT/cl_$nm.txt"
  echo "cc $nm: errors=$(grep -c 'error C' "$OUT/cl_$nm.txt") obj=$([ -f "$B/$nm.obj" ] && echo yes || echo NO)"; grep 'error C' "$OUT/cl_$nm.txt" | head -12; }
rm -f "$B"/s17_*.obj
cc17 s17_bios bios_B.cpp
cc17 s17_mixer mixer_P.cpp
cc17 s17_savestate pc98_savestate.cpp /FI"Y:\\dbx_ws\\r024t48e4\\gensei_build_id.h"
for n in s17_bios s17_mixer s17_savestate; do [ -f "$B/$n.obj" ] || { echo "MISSING $n.obj -> stop"; echo "== end $(date -Is)"; exit 0; }; done
[ -f "$B/gensei_host.obj" ] || { echo "NO host obj -> stop"; echo "== end $(date -Is)"; exit 0; }
echo "== menu.cpp"
cp "$R9/gensei_menu.obj" "$B/gensei_menu.obj" 2>/dev/null
MENUO='Y:\dbx_ws\r024t48e4\gensei_menu.obj'
NEWO=(); for n in s9_pic s9_video s9_sys s9_kbd s9_dma s9_lio s9_fm s9_floppy s9_fdc s9_fmtimer s9_pcm86io; do NEWO+=("Y:\\dbx_ws\\r009\\$n.obj"); done
NEWO+=("Y:\\dbx_ws\\r024t48e4\\s17_bios.obj" "Y:\\dbx_ws\\r024t48e4\\s17_savestate.obj" "Y:\\dbx_ws\\r011\\s11_paging.obj" "Y:\\dbx_ws\\r024t48e4\\s17_mixer.obj")
AN='Y:\dbx_ws\ana\src\obj\dosbox-x\x64\Release SDL2'
NEWO+=("$AN\\imagedisk_d88.obj" "$AN\\imagedisk_nfd.obj" "$AN\\qcow2_disk.obj" "$AN\\bios_vhd.obj" "$AN\\imagedisk_vfd.obj" "$AN\\imagedisk_teledisk.obj")
for o in cdrom cdrom_image dos_mscdex drive_iso dos dos_classes dos_devices dos_files dos_ioctl dos_memory dos_misc dos_tables dos_execute drives drive_cache drive_fat drive_local drive_virtual shell shell_batch shell_cmds shell_misc dos_programs dos_keyboard_layout ems xms; do NEWO+=("$AN\\$o.obj"); done
FORCE=()   # t38cd: /FORCE:MULTIPLE 제거 — 중복 정의가 있으면 링크가 실패해야 한다 (#285-1)
EXTRA="${WS}/lists/extra_objs_t37.txt"; if [ -f "$EXTRA" ]; then while IFS= read -r xo; do [ -n "$xo" ] && NEWO+=("$xo"); done < <(tr -d "\r" < "$EXTRA"); echo "추가 obj: $(grep -c . "$EXTRA")개"; fi
cp "${WS}/lists/r011_objs.rsp" "$OUT/r024t48e4_objs.rsp"
SDLLIB=$(findlib sdl2.lib); ZLIB=$(findlib zlib.lib); PNGLIB=$(findlib libpng.lib)
LIBS=("$(wp "$SDLLIB")" "$(wp "$ZLIB")" "$(wp "$PNGLIB")" opengl32.lib kernel32.lib user32.lib advapi32.lib shell32.lib ole32.lib oleaut"32".lib winmm.lib ws2_32.lib gdi32.lib imm32.lib setupapi.lib version.lib iphlpapi.lib uuid.lib comdlg32.lib dbghelp.lib shlwapi.lib)
RSP="$(wp "$OUT/r024t48e4_objs.rsp")"
EXE='Y:\dbx_ws\r024t48e4\gensei_host_r024t48e4.exe'
cd "$OUT"
"$LINK" /nologo "${LIBP[@]}" /MACHINE:X64 "${FORCE[@]}" /DEBUG /SUBSYSTEM:CONSOLE /OUT:"$EXE" @"$RSP" 'Y:\dbx_ws\r024t48e4\gensei_host.obj' "$MENUO" "${NEWO[@]}" "${LIBS[@]}" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > lo_0.txt
echo "round 0: LNK2001/2019=$(grep -c 'LNK2001\|LNK2019' lo_0.txt)"; grep -h "LNK4006\|LNK2005" lo_0.txt | head -20
SETUNRES=$(grep -E "opngen_setvol|psggen_setvol|rhythm_setvol|rhythm_update|adpcm_setvol|adpcm_update|pcm86gen_setvol|[ @_?]rhythm[@ ]|[ @_?]adpcm[@ ]" lo_0.txt | sort -u | head -12)
if [ -n "$SETUNRES" ]; then echo "SETTER_UNRESOLVED -> 멈춤(지시 #270 1a): $SETUNRES"; echo "== end $(date -Is)"; exit 0; fi
echo "setter 링크 확인: lo_0 에 볼륨 setter 미해결 없음"
PREV=""
for round in 1 2 3 4 5 6; do
  perl "$HS/stubgen_d.pl" lo_$((round-1)).txt stubs_$round $PREV
  PREV=stubs_$round.lst
  cp stubs_$round.asm stubs_${round}_names.cpp stubs_$round.altrsp "$B/"
  ( cd "$B" && "$ML" /nologo /c /Fostubs_$round.obj stubs_$round.asm 2>&1 | tr -d '\r' | head -20 )
  ( cd "$B" && "$CL" /nologo /c /MT /EHsc /Fostubs_${round}_names.obj stubs_${round}_names.cpp 2>&1 | tr -d '\r' | head -5 )
  "$LINK" /nologo "${LIBP[@]}" /MACHINE:X64 "${FORCE[@]}" /DEBUG /SUBSYSTEM:CONSOLE /OUT:"$EXE" @"$RSP" 'Y:\dbx_ws\r024t48e4\gensei_host.obj' "$MENUO" "${NEWO[@]}" "Y:\\dbx_ws\\r024t48e4\\stubs_$round.obj" "Y:\\dbx_ws\\r024t48e4\\stubs_${round}_names.obj" "@Y:\\dbx_ws\\r024t48e4\\stubs_$round.altrsp" "${LIBS[@]}" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > lo_$round.txt
  echo "link round $round: LNK2001/2019=$(grep -c 'LNK2001\|LNK2019' lo_$round.txt) other=$(grep 'error LNK' lo_$round.txt | grep -vc 'LNK2001\|LNK2019')"
  [ "$(grep -c 'LNK2001\|LNK2019' lo_$round.txt)" = "0" ] && break
done
grep "error LNK" lo_$round.txt | grep -v "LNK2001\|LNK2019" | head -15
grep -h "LNK4006\|LNK2005" lo_$round.txt | sort -u | head -20
[ -f "$B/gensei_host_r024t48e4.exe" ] || { echo "NO exe -> stop"; echo "== end $(date -Is)"; exit 0; }
EXEWIN='Y:\dbx_ws\r024t48e4\PC-98_PLAYER_win.exe'
"$LINK" /nologo "${LIBP[@]}" /MACHINE:X64 "${FORCE[@]}" /DEBUG /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OUT:"$EXEWIN" @"$RSP" 'Y:\dbx_ws\r024t48e4\gensei_host.obj' "$MENUO" "${NEWO[@]}" "Y:\\dbx_ws\\r024t48e4\\stubs_$round.obj" "Y:\\dbx_ws\\r024t48e4\\stubs_${round}_names.obj" "@Y:\\dbx_ws\\r024t48e4\\stubs_$round.altrsp" "${LIBS[@]}" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > lo_win.txt
echo "win link: $(grep -c 'error LNK' lo_win.txt) errors"
[ -f "$B/PC-98_PLAYER_win.exe" ] || { echo "NO win exe -> stop"; echo "== end $(date -Is)"; exit 0; }
echo "== mf build done: $(sha256sum "$B/PC-98_PLAYER_win.exe" | cut -c1-16)"
FB=${WS}/r020/rt/fb; [ -e "$FB/pc98_r024t48e4_test.exe" ] && { echo "이미 있음 - 덮어쓰지 않음"; echo "== end $(date -Is)"; exit 0; }; cp "$B/PC-98_PLAYER_win.exe" "$FB/pc98_r024t48e4_test.exe" && echo "시험용 창 exe: $FB/pc98_r024t48e4_test.exe"
echo "== end $(date -Is)"
} > "$LOG" 2>&1
