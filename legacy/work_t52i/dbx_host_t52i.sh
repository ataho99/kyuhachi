#!/bin/bash

# 산출 = build\r020\out_t52i, exe = Kyuhachi_r024t52i_test.exe (새 이름). 소스 = work\t48e4\ .
WS=${WS:-/mnt/y/dbx_ws}; export WS
set -u
D=/mnt/h/RETRO_SET/PC98_PLAYER; W49="$D/build/r020/work/t52i"
HS="$D/src/host_src"; SRC="$D/build/r020/work/src20"
OUT="$D/build/r020/out_t52i"; mkdir -p "$OUT"; LOG="$OUT/build_log.txt"
B=${WS}/r024t52i; mkdir -p "$B"
[ -f "$LOG" ] && mv "$LOG" "$LOG.old_$(date +%H%M%S)"   # 옛 로그는 이름만 바꿔 둔다
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
SRCSHA() { ( cd "$W49" && for f in gensei_host_v12_t39ui.cpp gensei_ui_t39.h gensei_prof_t39.h gensei_ov_t39.h gensei_input12_t48e.h gensei_uilist_t39.h; do sha256sum "$f"; done ) | sort -k2 | sha256sum | cut -c1-64; }
HSHA() { sha256sum "$W49/gensei_harness_t49a.h" | cut -c1-64; }
{
echo "SRCTREE product=$(SRCSHA) harness=$(HSHA) (제품 6파일 + 하네스 1파일 gensei_harness_t49a.h; 대상 소스 = work\\t52i, 안 바뀐 파일 = work\\t48e4)"
grep -rIn --include=*.cpp --include=*.h -e 'neither ANEX86' "${WS}/ana/src" 2>/dev/null | head -5 > "$OUT/font_src_grep.txt"; grep -rIn --include=*.cpp -e 'GetDbcsFont' "${WS}/ana/src" 2>/dev/null | head -12 >> "$OUT/font_src_grep.txt"
echo "== start $(date -Is)"
ls -l "$SRC" | awk '{print $5, $9}'
# ---------- 소스 배치
cp "$SRC/gensei_host_v12_t35.cpp" "$B/gensei_host.cpp"
for f in gensei_input12.h gensei_ov.h gensei_ui.h gensei_prof.h gensei_nkdb.h gensei_zip.h gensei_boostaud.h gensei_boostmix.inc; do cp "$SRC/$f" "$B/"; done; cp "$D/build/r020/work/t48e4/gensei_addwork_t40.h" "$B/gensei_addwork.h"; cp "$D/build/r020/work/t52i/gensei_input12_t48e.h" "$B/gensei_input12.h"; cp "$D/build/r020/work/t48e4/gensei_padnames_t48e.h" "$B/gensei_padnames_t48e.h"; cp "$SRC/gensei_ui_t35.h" "$B/gensei_ui.h"
T39="$D/build/r020/work/t48e4"; W49="$D/build/r020/work/t52i"   # 바뀐 파일은 t52i, 안 바뀐 파일은 t48e4 에서
for p in "gensei_host_v12_t39ui.cpp:gensei_host.cpp" "gensei_prof_t39.h:gensei_prof.h" "gensei_ov_t39.h:gensei_ov.h" "gensei_ui_t39.h:gensei_ui.h" "gensei_ent_t39.h:gensei_ent.h" "gensei_fontreg_t39.h:gensei_fontreg.h" "gensei_uilist_t39.h:gensei_uilist.h"; do sf="$W49/${p%%:*}"; [ -f "$sf" ] || sf="$T39/${p%%:*}"; cp "$sf" "$B/${p##*:}" || echo "MISSING SRC ${p%%:*}"; done; cp "$W49/gensei_harness_t49a.h" "$B/" || echo "MISSING SRC gensei_harness_t49a.h"; cp "$W49/gensei_phrase_t52.h" "$B/gensei_phrase.h" || echo "MISSING SRC gensei_phrase_t52.h"
cp "$T39/gensei_hangul_t48.h" "$B/" || echo "MISSING SRC gensei_hangul_t48.h"
ls -l "$B"/gensei_host.cpp "$B"/gensei_prof.h "$B"/gensei_ov.h "$B"/gensei_ui.h "$B"/gensei_ent.h "$B"/gensei_fontreg.h "$B"/gensei_uilist.h | awk '{print $5, $9}'
for f in gensei_menu_names.inc pc98_savestate.h pc98_savestate.cpp; do cp "$R11/$f" "$B/"; done
sed 's/r011/r024t52i/g' "$R11/cl_host.rsp" > "$B/cl_host.rsp"; echo '/utf-8' >> "$B/cl_host.rsp"; echo '/D GH_DOSKERNEL' >> "$B/cl_host.rsp"; mkdir -p "$B/rt"
sed 's/r011/r024t52i/g' "$R11/cl9.rsp" > "$B/cl9.rsp"; echo '/Fd"Y:\dbx_ws\r024t52i\vc9.pdb"' >> "$B/cl9.rsp"
printf '#define GENSEI_BUILD_ID "r024t52i"\n' > "$B/gensei_build_id.h"
cp "$W49/kh_version.h" "$B/kh_version.h" || echo "MISSING SRC kh_version.h"

# ---- t52i (#53): exe VERSIONINFO (rc.exe). 버전은 kh_version.h 한 곳에서만 읽는다.
cp "$D/icon/kyuhachi.ico" "$B/kyuhachi.ico" || echo "MISSING icon/kyuhachi.ico"
cat > "$B/kyuhachi.rc" <<'RCEOF'
#include <winver.h>
#include "kh_version.h"
1 ICON "kyuhachi.ico"
#define KH_STR2(x) #x
#define KH_STR(x) KH_STR2(x)
1 VERSIONINFO
 FILEVERSION KH_VER_MAJOR,KH_VER_MINOR,KH_VER_PATCH,KH_VER_BUILD
 PRODUCTVERSION KH_VER_MAJOR,KH_VER_MINOR,KH_VER_PATCH,KH_VER_BUILD
 FILEOS 0x00040004L
 FILETYPE 0x00000001L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "ProductName", "Kyuhachi"
      VALUE "FileDescription", "Kyuhachi - PC-98 BOOTLOADER"
      VALUE "ProductVersion", KH_VERSION ""
      VALUE "FileVersion", KH_VERSION ""
      VALUE "OriginalFilename", "Kyuhachi.exe"
      VALUE "InternalName", "Kyuhachi"
      VALUE "LegalCopyright", "GNU GPL v2 or later (includes DOSBox-X components)"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 1200
  END
END
RCEOF
RCEXE="/mnt/c/Program Files (x86)/Windows Kits/10/bin/${SDKV}/x64/rc.exe"
rm -f "$B/kyuhachi.res"
if [ -f "$RCEXE" ]; then ( cd "$B" && "$RCEXE" /nologo /I"${WSDK}\\Include\\${SDKV}\\um" /I"${WSDK}\\Include\\${SDKV}\\shared" /fo 'Y:\dbx_ws\r024t52i\kyuhachi.res' 'Y:\dbx_ws\r024t52i\kyuhachi.rc' 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > "$OUT/rc_out.txt" ); else echo "NO rc.exe -> VERSIONINFO 없이 진행: $RCEXE"; fi
RES=(); [ -f "$B/kyuhachi.res" ] && RES=('Y:\dbx_ws\r024t52i\kyuhachi.res'); echo "VERSIONINFO res: ${#RES[@]} (0 이면 VERSIONINFO 없음)"

cp "$D/build/r020/work/t48e4/gensei_kbtable_t46.h" "$B/gensei_kbtable.h"; cp "$D/build/r020/work/t52i/gensei_kbov_t52f.h" "$B/gensei_kbov.h"
IMG="$D/build/구번들/bt_sdl2_r004b"; SHD="$D/build/r011/산출/shaders"
perl "$SRC/gen_embed.pl" "$B/gensei_embed.h" "$IMG/gensei16_neodgm_sbcs3.fnt" "$IMG/gensei16_neodgm.fnt" "$SHD/pc98_sharp.glsl" "$D/build/r020/work/t48e4/pc98_crt_t48c.glsl"   # #246-3: 글꼴·셰이더 내장
# ---------- 패치 사본: bios(스플래시 생략) · mixer(배속 소리)
SS="$D/src/save_src"
cp "$SS/B/bios.cpp.B" "$B/bios_B.cpp"
sed -i -E 's/(reinterpret_cast<const char\*>\(pc98_bios_fdc_[a-z]+), sizeof/\1), sizeof/' "$B/bios_B.cpp"
perl "$SRC/patch_bios.pl" "$B/bios_B.cpp"
cp "$SS/ref/mixer.cpp.orig" "$B/mixer_P.cpp"
perl "$SRC/patch_mixer_t35.pl" "$B/mixer_P.cpp" "$B/gensei_boostmix.inc" "Y:/dbx_ws/r024t52i/gensei_boostaud.h"
# ---------- 호스트 컴파일
echo "== compile host"
rm -f "$B/gensei_host.obj"
"$CL" @'Y:\dbx_ws\r024t52i\cl_host.rsp' /FI'Y:\dbx_ws\r024t52i\gensei_build_id.h' /Fo'Y:\dbx_ws\r024t52i\gensei_host.obj' 'Y:\dbx_ws\r024t52i\gensei_host.cpp' 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > "$OUT/cl_host_out.txt"
echo "cl errors: $(grep -c 'error C' "$OUT/cl_host_out.txt")"; grep "error C" "$OUT/cl_host_out.txt" | head -40
cc17() { local nm=$1 sf=$2; shift 2
  "$CL" @'Y:\dbx_ws\r024t52i\cl9.rsp' "$@" /Fo"Y:\\dbx_ws\\r024t52i\\$nm.obj" "Y:\\dbx_ws\\r024t52i\\$sf" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > "$OUT/cl_$nm.txt"
  echo "cc $nm: errors=$(grep -c 'error C' "$OUT/cl_$nm.txt") obj=$([ -f "$B/$nm.obj" ] && echo yes || echo NO)"; grep 'error C' "$OUT/cl_$nm.txt" | head -12; }
rm -f "$B"/s17_*.obj
cc17 s17_bios bios_B.cpp
cc17 s17_mixer mixer_P.cpp
cc17 s17_savestate pc98_savestate.cpp /FI"Y:\\dbx_ws\\r024t52i\\gensei_build_id.h"
for n in s17_bios s17_mixer s17_savestate; do [ -f "$B/$n.obj" ] || { echo "MISSING $n.obj -> stop"; echo "== end $(date -Is)"; exit 0; }; done
[ -f "$B/gensei_host.obj" ] || { echo "NO host obj -> stop"; echo "== end $(date -Is)"; exit 0; }
echo "== menu.cpp"
cp "$R9/gensei_menu.obj" "$B/gensei_menu.obj" 2>/dev/null
MENUO='Y:\dbx_ws\r024t52i\gensei_menu.obj'
NEWO=(); for n in s9_pic s9_video s9_sys s9_kbd s9_dma s9_lio s9_fm s9_floppy s9_fdc s9_fmtimer s9_pcm86io; do NEWO+=("Y:\\dbx_ws\\r009\\$n.obj"); done
NEWO+=("Y:\\dbx_ws\\r024t52i\\s17_bios.obj" "Y:\\dbx_ws\\r024t52i\\s17_savestate.obj" "Y:\\dbx_ws\\r011\\s11_paging.obj" "Y:\\dbx_ws\\r024t52i\\s17_mixer.obj")
AN='Y:\dbx_ws\ana\src\obj\dosbox-x\x64\Release SDL2'
NEWO+=("$AN\\imagedisk_d88.obj" "$AN\\imagedisk_nfd.obj" "$AN\\qcow2_disk.obj" "$AN\\bios_vhd.obj" "$AN\\imagedisk_vfd.obj" "$AN\\imagedisk_teledisk.obj")
for o in cdrom cdrom_image dos_mscdex drive_iso dos dos_classes dos_devices dos_files dos_ioctl dos_memory dos_misc dos_tables dos_execute drives drive_cache drive_fat drive_local drive_virtual shell shell_batch shell_cmds shell_misc dos_programs dos_keyboard_layout ems xms; do NEWO+=("$AN\\$o.obj"); done
FORCE=()   # t38cd: /FORCE:MULTIPLE 제거 — 중복 정의가 있으면 링크가 실패해야 한다 (#285-1)
EXTRA="${WS}/lists/extra_objs_t37.txt"; if [ -f "$EXTRA" ]; then while IFS= read -r xo; do [ -n "$xo" ] && NEWO+=("$xo"); done < <(tr -d "\r" < "$EXTRA"); echo "추가 obj: $(grep -c . "$EXTRA")개"; fi
cp "${WS}/lists/r011_objs.rsp" "$OUT/r024t52i_objs.rsp"
SDLLIB=$(findlib sdl2.lib); ZLIB=$(findlib zlib.lib); PNGLIB=$(findlib libpng.lib)
LIBS=("$(wp "$SDLLIB")" "$(wp "$ZLIB")" "$(wp "$PNGLIB")" opengl32.lib kernel32.lib user32.lib advapi32.lib shell32.lib ole32.lib oleaut"32".lib winmm.lib ws2_32.lib gdi32.lib imm32.lib setupapi.lib version.lib iphlpapi.lib uuid.lib comdlg32.lib dbghelp.lib shlwapi.lib)
RSP="$(wp "$OUT/r024t52i_objs.rsp")"
EXE='Y:\dbx_ws\r024t52i\gensei_host_r024t52i.exe'
cd "$OUT"
"$LINK" /nologo "${LIBP[@]}" /MACHINE:X64 "${FORCE[@]}" /DEBUG /SUBSYSTEM:CONSOLE /OUT:"$EXE" @"$RSP" 'Y:\dbx_ws\r024t52i\gensei_host.obj' "$MENUO" "${NEWO[@]}" "${LIBS[@]}" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > lo_0.txt
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
  "$LINK" /nologo "${LIBP[@]}" /MACHINE:X64 "${FORCE[@]}" /DEBUG /SUBSYSTEM:CONSOLE /OUT:"$EXE" @"$RSP" 'Y:\dbx_ws\r024t52i\gensei_host.obj' "$MENUO" "${NEWO[@]}" "Y:\\dbx_ws\\r024t52i\\stubs_$round.obj" "Y:\\dbx_ws\\r024t52i\\stubs_${round}_names.obj" "@Y:\\dbx_ws\\r024t52i\\stubs_$round.altrsp" "${LIBS[@]}" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > lo_$round.txt
  echo "link round $round: LNK2001/2019=$(grep -c 'LNK2001\|LNK2019' lo_$round.txt) other=$(grep 'error LNK' lo_$round.txt | grep -vc 'LNK2001\|LNK2019')"
  [ "$(grep -c 'LNK2001\|LNK2019' lo_$round.txt)" = "0" ] && break
done
grep "error LNK" lo_$round.txt | grep -v "LNK2001\|LNK2019" | head -15
grep -h "LNK4006\|LNK2005" lo_$round.txt | sort -u | head -20
[ -f "$B/gensei_host_r024t52i.exe" ] || { echo "NO exe -> stop"; echo "== end $(date -Is)"; exit 0; }
EXEWIN='Y:\dbx_ws\r024t52i\Kyuhachi.exe'
"$LINK" /nologo "${LIBP[@]}" /MACHINE:X64 "${FORCE[@]}" /DEBUG /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /OUT:"$EXEWIN" @"$RSP" 'Y:\dbx_ws\r024t52i\gensei_host.obj' "$MENUO" "${NEWO[@]}" "Y:\\dbx_ws\\r024t52i\\stubs_$round.obj" "Y:\\dbx_ws\\r024t52i\\stubs_${round}_names.obj" "@Y:\\dbx_ws\\r024t52i\\stubs_$round.altrsp" "${LIBS[@]}" "${RES[@]}" 2>&1 | tr -d '\r' | iconv -f cp949 -t utf-8 -c > lo_win.txt
echo "win link: $(grep -c 'error LNK' lo_win.txt) errors"
[ -f "$B/Kyuhachi.exe" ] || { echo "NO win exe -> stop"; echo "== end $(date -Is)"; exit 0; }
echo "== mf build done: $(sha256sum "$B/Kyuhachi.exe" | cut -c1-16)"
# ---- t52i (#50/#52/#53): 배포 트리 (이름은 전부 영문/ASCII). 게임 이미지 · NEC BIOS · 글꼴 ROM · 재배포 불가 글꼴은 넣지 않는다.
KHVER=$(sed -n 's/^#define KH_VERSION "\(.*\)".*/\1/p' "$W49/kh_version.h"); [ -n "$KHVER" ] || KHVER=unknown
T="$OUT/Kyuhachi"; mkdir -p "$T/res/fonts" "$T/games" "$T/saves" "$T/src"
for L in ko en ja; do cp "$W49/strings_$L.txt" "$T/strings_$L.txt"; done; for F in shinonome16_jp.fnt shinonome16_LICENSE.txt; do cp "$W49/$F" "$T/res/fonts/$F"; done   # t52i: 언어별 문구 파일 3개(strings.txt 는 동봉하지 않는다 - 사용자가 만드는 한국어 덮어쓰기)
[ -e "$T/Kyuhachi.exe" ] || cp "$B/Kyuhachi.exe" "$T/Kyuhachi.exe"
BSHA8=$(sha256sum "$T/Kyuhachi.exe" | cut -c1-8)
echo "BUILD_SHA256 Kyuhachi.exe $(sha256sum "$T/Kyuhachi.exe" | cut -c1-64) size=$(stat -c %s "$T/Kyuhachi.exe") version=$KHVER"
{ printf '\xEF\xBB\xBF'; printf 'Put anex86-format font BMP files (524,350 bytes) here. TTF/OTF do NOT work.\r\nKorean patches / Korean editions need the Korean font ROM that fits the game. Font BMPs are not bundled (license). shinonome16_jp.fnt (Japanese menu font, public domain) is already here - do not delete it.\r\n'; printf '이 폴더에는 anex86 형식 BMP(524,350 B)만 됩니다. TTF/OTF 는 안 됩니다.\r\n한글패치·한국판은 그 작품에 맞는 한글 글꼴 롬이 필요합니다. 글꼴 BMP 는 라이선스 때문에 동봉하지 않았습니다. shinonome16_jp.fnt(일본어 메뉴 글꼴, 퍼블릭 도메인)는 들어 있으니 지우지 마세요.\r\n'; } > "$T/res/fonts/PUT_FONTS_HERE.txt"
printf 'Put each game folder here (one folder per game).\r\nGame images are NOT included. Bring your own legally obtained copies.\r\n' > "$T/games/PUT_GAMES_HERE.txt"
printf 'Save files are created here at run time.\r\n' > "$T/saves/SAVES_ARE_CREATED_HERE.txt"
printf 'Full corresponding source (GPL): see README.txt, section SOURCE.\r\n' > "$T/src/SOURCE_README.txt"
GPLF=$(find "${WS}/ana/src" -maxdepth 2 -iname 'COPYING' 2>/dev/null | head -1)
{ printf 'Kyuhachi %s - LICENSE\r\n\r\n' "$KHVER"
  printf 'Kyuhachi contains components of DOSBox-X (https://dosbox-x.com/), licensed under the GNU General Public License, version 2 or (at your option) any later version.\r\n'
  printf 'Kyuhachi as a whole is distributed under the same license. NO WARRANTY.\r\n'
  printf 'The corresponding source code (including the modified DOSBox-X files and the build scripts) is available as described in README.txt.\r\n\r\n'
  printf 'Other linked components: SDL2 (zlib license), zlib (zlib license), libpng (libpng license).\r\n'
  printf 'The Japanese menu font res/fonts/shinonome16_jp.fnt is converted from the Shinonome (Shinonome 16 dot Gothic, shnmk16.bdf, shinonome-0.9.11p1) fonts, which their authors provide as public domain (source: http://openlab.ring.gr.jp/efont/shinonome/). Original license wording and the conversion notes: res/fonts/shinonome16_LICENSE.txt.\r\n'
  printf 'The built-in bitmap font is derived from Neo Dunggeunmo (SIL Open Font License 1.1). Its copyright notice and license text follow.\r\n\r\n'
  printf -- '------------------------------------------------------------\r\nNeo Dunggeunmo - copyright notice and SIL OFL 1.1 (applies to the built-in font only)\r\n------------------------------------------------------------\r\n'
  if [ -f "$W49/OFL_neodgm.txt" ]; then sed 's/$/\r/' "$W49/OFL_neodgm.txt"; printf '\r\n'; else printf '(OFL_neodgm.txt missing at build time)\r\n'; fi
  printf -- '------------------------------------------------------------\r\nGNU GENERAL PUBLIC LICENSE text follows (COPYING from the DOSBox-X source tree)\r\n------------------------------------------------------------\r\n'
  if [ -n "$GPLF" ]; then sed 's/$/\r/' "$GPLF"; else printf '(COPYING not found at build time - add the full GPL text before release)\r\n'; fi
} > "$T/LICENSE.txt"
{ printf '\xEF\xBB\xBF'
  printf 'Kyuhachi v%s\r\nPC-98 BOOTLOADER\r\n\r\n' "$KHVER"
  printf 'FOLDER LAYOUT\r\n\r\nKyuhachi/\r\n  Kyuhachi.exe     the program (no installer, no runtime needed)\r\n  strings_ko.txt   screen text, Korean   (edit with Notepad, restart to apply)\r\n  strings_en.txt   screen text, English\r\n  strings_ja.txt   screen text, Japanese\r\n  strings.txt      optional, you create it: lines here override the Korean text\r\n  README.txt       this file\r\n  LICENSE.txt      license (GPL) and notices\r\n  res/fonts/       put font files here (shinonome16_jp.fnt = bundled Japanese menu font, keep it)\r\n  games/           put game folders here\r\n  saves/           save files (created at run time)\r\n  src/             source code information\r\n\r\n'
  printf 'NOTE: UI language = Korean / English / Japanese, set in the menu: Sound/Pad/Speed settings, last row (applies at once). In Japanese, kanji and kana are drawn with a Japanese font BMP in res/fonts if there is one (jp_default.bmp, or the one chosen in the settings), otherwise with the bundled Shinonome 16 (res/fonts/shinonome16_jp.fnt, public domain). Kanji are drawn the same way in Korean and English. strings_*.txt keys are the original on-screen phrases (original=replacement). The same phrase changes everywhere at once; if a later version changes a phrase, the old line no longer matches.\r\n\r\n'
  printf '=== SOURCE ===\r\nThe full corresponding source code of this exact build (including the modified DOSBox-X files and the build scripts) is provided on request / at the location below.\r\nSource location: written offer - for at least three years from this release, anyone may request the complete corresponding source of this build by opening an issue at https://github.com/ataho99/kyuhachi/issues . Upstream DOSBox-X: https://github.com/joncampbell123/dosbox-x\r\n\r\n'
  printf '=== 한국어 설명 ===\r\n- 설치가 필요 없습니다. 압축을 풀고 Kyuhachi.exe 를 실행하세요. 폴더 이름에 한글이 있어도 됩니다.\r\n- games 폴더에 게임 폴더를 넣으세요. 게임 이미지는 들어 있지 않습니다. 직접 구한 정품 사본을 쓰세요.\r\n- res/fonts 폴더에 글꼴 파일을 넣으세요. 글꼴 라이선스 때문에 동봉하지 않았습니다. 글꼴 파일이 없으면 작품이 시작되지 않고 안내가 나옵니다(메뉴 글자는 내장 글꼴).\r\n- 화면 언어(한국어 / English / にほんご)는 메뉴 「소리/패드/배속 설정」 맨 아래 「언어 / Language / 言語」 에서 고릅니다. 바로 적용되고 저장됩니다. 일본어 화면의 한자·가나는 res/fonts 의 일본어 글꼴 BMP(jp_default.bmp 또는 설정에서 고른 것)가 있으면 그것으로, 없으면 동봉한 東雲(Shinonome) 16(res/fonts/shinonome16_jp.fnt, 퍼블릭 도메인)으로 그립니다.\r\n- 화면 문구는 strings_ko.txt / strings_en.txt / strings_ja.txt 를 메모장으로 고쳐 저장한 뒤 다시 실행하면 바뀝니다. 한국어는 직접 만든 strings.txt 의 줄이 우선합니다. 파일이 없거나 줄이 틀리면 기본 문구를 씁니다.\r\n- strings_*.txt 의 키는 원문 문구입니다(원문=바꿀문구). 같은 원문은 한 번에 바뀌고, 프로그램 버전이 올라 원문이 바뀌면 예전 줄은 맞지 않을 수 있습니다.\r\n- saves 폴더에 저장 파일이 만들어집니다.\r\n- 게임 이미지 / NEC BIOS / 글꼴 ROM 은 이 배포판에 들어 있지 않습니다.\r\n\r\n'
  printf '=== FONTS / 글꼴 ===\r\nOnly anex86-format BMP (524,350 bytes) works in res/fonts. TTF/OTF do not work. The default font name is kyuhachi_font.bmp (the old name pc98player_font.bmp is still read).\r\nJapanese UI: kanji/kana use jp_default.bmp (or the Japanese font chosen in the settings) if present, otherwise the bundled shinonome16_jp.fnt (Shinonome 16, public domain; license in shinonome16_LICENSE.txt).\r\nKorean patches / Korean editions need the Korean font ROM that fits the game. With no font BMP at all, games do not start (the menu itself uses a built-in font).\r\nres/fonts 에는 anex86 형식 BMP(524,350 B)만 됩니다. TTF/OTF 는 안 됩니다.\r\n한글패치·한국판은 그 작품에 맞는 한글 글꼴 롬이 필요합니다. 글꼴 BMP 가 하나도 없으면 작품이 시작되지 않고 안내가 나옵니다(메뉴 글자는 내장 글꼴).\r\n\r\n'
  printf '=== KEYS / 조작 ===\r\nF12: menu / 메뉴    F11 (hold): fast-forward / 누르는 동안 배속\r\nScrollLock or Alt+F11: fast-forward on/off / 배속 켜기·끄기    Alt+Enter: fullscreen / 전체화면\r\nPad: R2 (hold) fast-forward, L3 on/off, square = fast text (CAPS) / 패드: R2 누르는 동안 배속, L3 켜고 끄기, □ 빨리 넘김\r\nGame list: OK (circle) starts the game (a save asks resume / from start / system disk only). Screen settings has a Fullscreen row. Sound/pad/boost settings are saved as soon as you change them; X just goes back. / 작품 목록: ○ 로 시작(저장이 있으면 이어하기·처음부터·시스템 디스크만 선택). 화면 설정에 「전체화면」 줄. 동작 환경(소리·패드·배속) 값은 바꾸는 즉시 저장되고 × 는 뒤로 가기입니다. 메뉴 → 조작 안내에 전체 키 표.\r\nOverlay buttons (bottom-left, during play): menu, quick save, quick load (hold 0.6 s), fast-forward on/off, on-screen keyboard / 화면 아래 버튼: 메뉴 · 빠른 저장 · 빠른 불러오기(길게) · 배속 · 화면 키보드 (설정에서 끔)\r\n\r\n'
  printf 'build t52i - sha %s\r\n' "$BSHA8"
} > "$T/README.txt"
rm -f "$OUT/Kyuhachi-v${KHVER}.zip"
if command -v zip >/dev/null 2>&1; then ( cd "$OUT" && zip -qr "Kyuhachi-v${KHVER}.zip" Kyuhachi ); else ( cd "$OUT" && python3 -m zipfile -c "Kyuhachi-v${KHVER}.zip" Kyuhachi ); fi
echo "ZIP: $OUT/Kyuhachi-v${KHVER}.zip $(stat -c %s "$OUT/Kyuhachi-v${KHVER}.zip" 2>/dev/null)"
mkdir -p "$D/release"; [ -e "$D/release/Kyuhachi-v${KHVER}.zip" ] && echo "release 에 같은 이름이 이미 있음 - 덮어쓰지 않음" || { cp "$OUT/Kyuhachi-v${KHVER}.zip" "$D/release/Kyuhachi-v${KHVER}.zip" && echo "RELEASE_ZIP: $D/release/Kyuhachi-v${KHVER}.zip sha256=$(sha256sum "$D/release/Kyuhachi-v${KHVER}.zip" | cut -c1-64) size=$(stat -c %s "$D/release/Kyuhachi-v${KHVER}.zip")"; }
echo "TREE_NONASCII: $( ( cd "$OUT" && find Kyuhachi | LC_ALL=C grep -c -P '[^\x00-\x7F]' ) )"
echo "TREE:"; ( cd "$OUT" && find Kyuhachi | sort )
FB=${WS}/r020/rt/fb; [ -e "$FB/Kyuhachi_r024t52i_test.exe" ] && { echo "이미 있음 - 덮어쓰지 않음"; echo "== end $(date -Is)"; exit 0; }; cp "$B/Kyuhachi.exe" "$FB/Kyuhachi_r024t52i_test.exe" && echo "시험용 창 exe: $FB/Kyuhachi_r024t52i_test.exe"
echo "== end $(date -Is)"
} > "$LOG" 2>&1
