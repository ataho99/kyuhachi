#!/bin/bash
# 지시 #137 ① 현황 실측(읽기만). 출력 = work\t137\measure_out.txt
P=/mnt/h/RETRO_SET/PC98_PLAYER; B=$P/build/r020; W=$B/work; O=$W/t137; mkdir -p "$O"
{
echo "## 디렉터리 개요"; for d in "$P/src" "$P/src/host_src" "$P/src/save_src" "$B/work/src20" "$B/work/t52i"; do echo "-- $d"; ls -l "$d" 2>/dev/null | awk 'NR>1{print $5, $9}' | head -80; done
echo; echo "## 우리 소스 줄 수 (work\\t52i, 없으면 work\\t48e4)"
cd "$W/t52i"; for f in gensei_host_v12_t39ui.cpp gensei_ui_t39.h gensei_ov_t39.h gensei_prof_t39.h gensei_ent_t39.h gensei_uilist_t39.h gensei_input12_t48e.h gensei_harness_t49a.h gensei_kbov_t52f.h gensei_phrase_t52.h kh_version.h; do [ -f "$f" ] && echo "$(wc -l < $f) $(wc -c < $f) $f (t52i)"; done
cd "$W/t48e4"; for f in gensei_fontreg_t39.h gensei_hangul_t48.h gensei_addwork_t40.h gensei_kbtable_t46.h; do [ -f "$f" ] && echo "$(wc -l < $f) $(wc -c < $f) $f (t48e4)"; done
cd "$W/src20" 2>/dev/null && for f in gensei_nkdb.h gensei_zip.h gensei_boostaud.h gensei_boostmix.inc gensei_input12.h gen_embed.pl patch_bios.pl patch_mixer_t35.pl patch_gamedb.pl; do [ -f "$f" ] && echo "$(wc -l < $f) $(wc -c < $f) $f (src20)"; done
echo; echo "## 호스트 안 전역(g_ 접두) 정의 수 / 파일 내 함수 수(대략)"
cd "$W/t52i"; for f in gensei_host_v12_t39ui.cpp gensei_ui_t39.h gensei_ov_t39.h gensei_prof_t39.h gensei_ent_t39.h gensei_uilist_t39.h gensei_input12_t48e.h gensei_kbov_t52f.h; do
  g=$(grep -cE '^(static |extern |inline )?[A-Za-z_][A-Za-z0-9_:<>,\*& ]* \**g_[A-Za-z0-9_]+( *=[^=]|;|\[)' "$f"); fn=$(grep -cE '^(static |inline )?[A-Za-z_][A-Za-z0-9_:<>,\*& ]+ [\*&]?[A-Za-z_][A-Za-z0-9_:]*\(.*\) *(const)? *\{' "$f"); echo "$f  전역(g_)=$g  함수(줄머리 정의)=$fn"; done
echo; echo "## #include 관계"; for f in gensei_host_v12_t39ui.cpp gensei_ui_t39.h gensei_ov_t39.h gensei_prof_t39.h gensei_ent_t39.h gensei_uilist_t39.h gensei_input12_t48e.h gensei_kbov_t52f.h gensei_harness_t49a.h gensei_phrase_t52.h; do echo "$f -> $(grep -hE '^#include "' "$f" | sed 's/#include //' | tr -d '"' | awk '{print $1}' | tr '\n' ' ')"; done
echo; echo "## 네임스페이스·접두 사용 횟수 (행=쓰는 파일, 열=쓰이는 것)"
for f in gensei_host_v12_t39ui.cpp gensei_ui_t39.h gensei_ov_t39.h gensei_prof_t39.h gensei_ent_t39.h gensei_uilist_t39.h gensei_input12_t48e.h gensei_kbov_t52f.h; do echo "$f: ui::=$(grep -o 'ui::' $f | wc -l) ov::=$(grep -o 'ov::' $f | wc -l) prof::=$(grep -o 'prof::' $f | wc -l) ent::=$(grep -o 'ent::' $f | wc -l) kbov::=$(grep -o 'kbov::' $f | wc -l) aw::=$(grep -o 'aw::' $f | wc -l) hg::=$(grep -o 'hg::' $f | wc -l) fr::=$(grep -o 'fr::' $f | wc -l) phr_=$(grep -o 'phr_(' $f | wc -l) gh_=$(grep -o 'gh_[a-z_]*(' $f | wc -l)"; done
echo; echo "## namespace 선언"; grep -nE '^namespace |^} *// *namespace|^struct [A-Za-z]+ \{' gensei_*.h gensei_host_v12_t39ui.cpp 2>/dev/null | cut -c1-120 | head -60
echo; echo "## #ifdef/_WIN32/Windows API 사용 (플랫폼 의존 위치)"; for f in gensei_host_v12_t39ui.cpp gensei_ui_t39.h gensei_ov_t39.h gensei_prof_t39.h gensei_ent_t39.h gensei_uilist_t39.h gensei_input12_t48e.h; do echo "$f: #ifdef/#if=$(grep -cE '^#if' $f)  WinAPI(A() 호출)=$(grep -oE '\b(GetFileAttributesA|CreateFileA|FindFirstFileA|SetCurrentDirectoryA|DeleteFileA|CreateDirectoryA|GetModuleFileNameA|MultiByteToWideChar|WideCharToMultiByte|GetTickCount|QueryPerformanceCounter|SHGetFolderPath|ShellExecute|MessageBox|CreateProcess|LoadLibrary|GetProcAddress)\b' $f | wc -l)"; done
echo; echo "## DOSBox-X 쪽 우리가 고친 파일 (빌드 스크립트가 쓰는 것)"
for f in "$P/src/host_src" "$P/src/save_src"; do [ -d "$f" ] && find "$f" -type f | head -60 | while read x; do echo "$(wc -l < "$x") $x"; done; done
echo; echo "## 원본 대조 가능한 것 (.orig / ref)"; find "$P/src" -name '*.orig' -o -name '*.B' -o -path '*ref*' -type f 2>/dev/null | head -30 | while read x; do echo "$(wc -l < "$x") $x"; done
echo; echo "## 패치 스크립트가 바꾸는 줄 수 (perl 패치 파일 크기)"; for f in "$W/src20"/patch_*.pl "$W/src20"/merge_settings.pl "$W/src20"/scan_mouse.pl; do [ -f "$f" ] && echo "$(wc -l < "$f") $f"; done
echo; echo "## 빌드 스크립트 크기/횟수"; wc -l "$W/t52i/dbx_host_t52i.sh"; ls "$W" | wc -l; echo "work 폴더 수: $(ls -d $W/*/ | wc -l)"; echo "work\\*\\ 안 소스 복사본 용량(대략): $(du -sh $W 2>/dev/null | cut -f1)"; echo "out_* 폴더 수: $(ls -d $B/out_*/ | wc -l)  test_* 폴더 수: $(ls -d $B/test_*/ | wc -l)"
echo; echo "## git 유무"; ls -d "$P/.git" "$P/../.git" 2>&1 | head -2; which git cmake gcc x86_64-w64-mingw32-g++ 2>&1 | head -5; ls /mnt/y/dbx_ws 2>/dev/null | head -30
} > "$O/measure_out.txt" 2>&1
wc -l "$O/measure_out.txt"; echo DONE
