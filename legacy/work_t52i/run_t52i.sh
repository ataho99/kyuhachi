#!/bin/bash
# 지시 #136 t52i 빌드: t52h 소스 + B1(작품 목록 ○ 시작, 시작 팝업 시스템 디스크만) · B2 조작 안내 한 장 표 · B3 배속 표, 전체화면 줄 · 동작 환경 × = 뒤로. 새 폴더 work\t52i · out_t52i
P=/mnt/h/RETRO_SET/PC98_PLAYER; B=$P/build/r020; A=$B/work/t52h; W=$B/work/t52i
mkdir -p "$W"
TS=$(date +%H%M%S); Z=$P/release/Kyuhachi-v0.5.0-t52i.zip; if [ -e "$Z" ]; then mkdir -p $P/release/_old; mv "$Z" "$P/release/_old/Kyuhachi-v0.5.0-t52i_try$TS.zip"; echo "이전 시도 zip -> release/_old/Kyuhachi-v0.5.0-t52i_try$TS.zip"; fi
if [ -e "$B/out_t52i" ]; then mv "$B/out_t52i" "$B/out_t52i_try$TS"; echo "이전 산출 폴더 -> out_t52i_try$TS"; fi
echo "== 소스 복사(t52h → t52i)"
for f in gensei_host_v12_t39ui.cpp gensei_prof_t39.h gensei_ov_t39.h gensei_ent_t39.h gensei_uilist_t39.h gensei_input12_t48e.h gensei_harness_t49a.h OFL_neodgm.txt gensei_ui_t39.h kh_version.h gensei_kbov_t52f.h gensei_phrase_t52.h check_keys.pl dbx_host_t52h.sh shinonome16_jp.fnt shinonome16_LICENSE.txt; do
  if [ -e "$A/$f" ]; then cp -f "$A/$f" "$W/$f"; else echo "work\\t52h 에 $f 없음 -> stop"; exit 0; fi
done
for f in strings_ko.txt strings_en.txt strings_ja.txt patch_t52i.pl mod_build_t52i.pl; do [ -e "$W/$f" ] || { echo "work\\t52i 에 $f 없음 -> stop"; exit 0; }; done
echo "== 패치"
perl "$W/patch_t52i.pl" "$W/gensei_host_v12_t39ui.cpp" "$W/gensei_ui_t39.h" "$W/gensei_ov_t39.h" "$W/kh_version.h" || { echo "PATCH FAIL -> stop"; exit 0; }
echo "== 문구 키 대조 (소스 phr_ 문구 ↔ strings_ko/en/ja.txt 키)"
for L in ko en ja; do echo "[$L]"; perl "$W/check_keys.pl" "$W/strings_$L.txt" "$W/gensei_host_v12_t39ui.cpp" "$W/gensei_ui_t39.h" "$W/gensei_ov_t39.h" "$W/gensei_prof_t39.h" "$W/gensei_ent_t39.h" "$W/gensei_uilist_t39.h" "$W/gensei_input12_t48e.h" "$W/gensei_harness_t49a.h" | cut -c1-200; done
echo "== 문구 파일 줄 수: ko=$(grep -a -c . "$W/strings_ko.txt") en=$(grep -a -c . "$W/strings_en.txt") ja=$(grep -a -c . "$W/strings_ja.txt")"
echo "== 빌드 스크립트 생성"
perl "$W/mod_build_t52i.pl" "$W/dbx_host_t52h.sh" "$W/dbx_host_t52i.sh" || { echo "MOD FAIL -> stop"; exit 0; }
bash "$W/dbx_host_t52i.sh"
echo "== build_log (tail)"; tail -40 "$B/out_t52i/build_log.txt"
ls -l "$B/out_t52i" | awk '{print $5, $9}' | head -30
echo DONE
