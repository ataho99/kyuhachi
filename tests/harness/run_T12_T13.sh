#!/bin/bash
# T12(세이브 호환) · T13(설정·DB 호환) 하니스 — 사용: run_T12_T13.sh <새 빌드 zip> <라벨> [기준 zip(t52i)]  출력 = work/h12/out_<라벨>.txt
# 라이브 saves·설정은 읽기만(사본 폴더에서 시험). 합격 기준은 module_plan_v1_1.md T12·T13 행.
P=/mnt/h/RETRO_SET/PC98_PLAYER; W=$P/build/r020/work/h12; ZN=${1:?zip}; LAB=${2:?label}; ZI=${3:-$P/release/Kyuhachi-v0.5.0-t52i.zip}
LV=$P/live_v050/Kyuhachi; BK=$(ls -d $P/백업_saves_* | head -1); KS=$P/build/r020/work/t52i/uikeys_t52i.txt
mkdir -p $W; exec > $W/out_$LAB.txt 2>&1
T=$P/시험_h12_${LAB}_$(date +%H%M%S); mkdir -p "$T"; w() { wslpath -w "$1"; }; fail=0
echo "== 시작 $(date -Is) | 새 zip $(sha256sum $ZN | cut -c1-16) | 기준 zip $(sha256sum $ZI | cut -c1-16) | 사본 = $BK"
ex() { rm -rf "$2"; mkdir -p "$2"; unzip -o -q "$1" -d "$2" 2>/dev/null; [ -s "$2/Kyuhachi/Kyuhachi.exe" ] || { echo "unzip 실패 $1"; exit 1; }; }
mk() { ex "$2" "$T/$1"; k=$T/$1/Kyuhachi; mkdir -p "$k/saves" "$k/games"
  cp "$LV/settings.ini" "$LV/gamedb.ini" "$k/" 2>/dev/null; [ -e "$LV/gamedb_nk.tsv" ] && cp "$LV/gamedb_nk.tsv" "$k/"
  for d in "$LV"/games/*/; do g=$(basename "$d"); [ "$g" = "_중복" ] && continue; mkdir -p "$k/games/$g"; cp "$d"game.ini "$k/games/$g/" 2>/dev/null; [ -e "$d"user.ini ] && cp "$d"user.ini "$k/games/$g/"; done
  cp "$LV"/res/fonts/* "$k/res/fonts/" 2>/dev/null; for f in font.bmp font.fnt font_sbcs.fnt; do :; done
  cp "$LV/res/fonts/pc98player_font.bmp" "$k/res/fonts/kyuhachi_font.bmp"; cp "$LV/res/fonts/pc98player_font.fnt" "$k/res/fonts/kyuhachi_font.fnt"; cp "$LV/res/fonts/pc98player_font_sbcs.fnt" "$k/res/fonts/kyuhachi_font_sbcs.fnt"; }
mk N "$ZN"; mk I "$ZI"; N=$T/N/Kyuhachi; I=$T/I/Kyuhachi
echo "exe sha16: 새=$(sha256sum $N/Kyuhachi.exe | cut -c1-16) 기준=$(sha256sum $I/Kyuhachi.exe | cut -c1-16)"
echo "== T12 세이브 호환 (사본 3개 × 별도 프로세스 3회, --sec 10, 불러온 뒤 5초 이상 진행)"
for g in daimadou95 doukyuusei2 kitan; do cp "$BK/$g/slot01.g98s" "$N/L_$g.g98s"; echo "사본 $g sha16 $(sha256sum $N/L_$g.g98s | cut -c1-16)"
  for L in 3.0 4.0 5.0; do o=$T/N/t12_${g}_$L; mkdir -p $o
    rc=$( cd "$N" && timeout 80 "$N/Kyuhachi.exe" --windowed --hidewin --noboot --out "$(w $o)" --sec 11 --load $L --shot 10 --state "$(w $N/L_$g.g98s)" $g >/dev/null 2>&1; echo $? )
    lg=$o/kyuhachi_log.txt; ok=$(grep -a -c -E "LOAD .* <- OK" $lg); bad=$(grep -a -c -E "CRASH code|access violation|LOAD .* <- FAIL" $lg)
    big=$(ls -l $o/*.png 2>/dev/null | awk '{print $5}' | sort -n | tail -1); n=$(ls $o/*.png 2>/dev/null | wc -l)
    res=PASS; { [ "$rc" = 0 ] && [ "$ok" -ge 1 ] && [ "$bad" = 0 ] && [ "${big:-0}" -gt 2000 ]; } || { res=FAIL; fail=1; }
    echo "T12 $g load@$L rc=$rc LOAD_OK=$ok BAD=$bad png=$n max=${big:-0}B -> $res"; grep -a -h "LOAD .* <-" $lg | sed 's#H:\\RETRO_SET\\PC98_PLAYER\\##' | cut -c1-160 | head -1; done; done
echo "== T13 설정·DB 호환 (라이브 settings.ini·gamedb.ini·game.ini×N·user.ini×N 사본을 새 빌드와 기준 빌드가 읽고 저장)"
for d in $N $I; do cp "$LV/settings.ini" "$LV/gamedb.ini" "$d/"; done   # T12 가 N 의 settings.ini 에 플레이 시간을 쌓으므로 T13 은 라이브 사본으로 되돌리고 시작
sha_all() { ( cd "$1" && sha256sum settings.ini gamedb.ini games/*/game.ini games/*/user.ini 2>/dev/null | sort -k2 ); }
sha_all $N > $T/n_before.txt; sha_all $I > $T/i_before.txt; echo "읽기 전 파일 수: $(wc -l < $T/n_before.txt)  (새·기준 동일: $(diff -q $T/n_before.txt $T/i_before.txt >/dev/null && echo 예 || echo 아니오))"
for r in N:0 I:0 N:1 I:1 N:2 I:2; do d=${r%%:*}; L=${r##*:}; k=$T/$d/Kyuhachi; o=$T/$d/out_keys$L; mkdir -p $o
  ( cd "$k" && timeout 150 "$k/Kyuhachi.exe" --hidewin --uitest "$(w $o)" --uikeys "$(w $KS)" --out "$(w $o)" --lang $L >/dev/null 2>&1; echo "$d lang=$L rc=$?" ); done
for L in 0 1 2; do
  a=$(grep -a -h "T52I VALDUMP" $T/N/out_keys$L/*log*.txt | sed -E 's/^ *[0-9]+ [A-Z]+ //' ); b=$(grep -a -h "T52I VALDUMP" $T/I/out_keys$L/*log*.txt | sed -E 's/^ *[0-9]+ [A-Z]+ //')
  na=$(echo "$a" | grep -c VALDUMP); [ "$a" = "$b" ] && [ "$na" -ge 3 ] && v=PASS || { v=FAIL; fail=1; }
  echo "T13 VALDUMP lang=$L 줄수=$na 새=기준 동일 -> $v"; echo "$a" | cut -c1-140 | head -4
  sn=$(tr -d '\r' < $T/N/out_keys$L/settings_uikeys.ini 2>/dev/null | grep -v "^\s*\$" | grep -v "^;" | sort); si=$(tr -d '\r' < $T/I/out_keys$L/settings_uikeys.ini 2>/dev/null | grep -v "^\s*\$" | grep -v "^;" | sort)
  [ -n "$sn" ] && [ "$sn" = "$si" ] && v=PASS || { v=FAIL; fail=1; }; echo "T13 저장 ini 의미 동일(키=값 정렬 비교) lang=$L -> $v (줄수 새=$(echo "$sn" | wc -l) 기준=$(echo "$si" | wc -l))"
  cmp -s $T/N/out_keys$L/settings_uikeys.ini $T/I/out_keys$L/settings_uikeys.ini && echo "  (순서까지 diff 0 = T7 조건 충족)" || echo "  (순서·바이트는 다름 — T7 대상)"; done
sha_all $N > $T/n_after.txt; sha_all $I > $T/i_after.txt
diff -q $T/n_before.txt $T/n_after.txt >/dev/null && echo "T13 새 빌드가 읽기만 한 파일(game/user/gamedb/settings) 불변 -> PASS" || { echo "T13 새 빌드가 입력 ini 를 바꿈 -> 확인 필요:"; diff $T/n_before.txt $T/n_after.txt | head -6; }
diff -q $T/i_before.txt $T/i_after.txt >/dev/null && echo "  (기준 t52i 도 불변)" || { echo "  (기준 t52i 는 바꿈:)"; diff $T/i_before.txt $T/i_after.txt | head -6; }
echo "== 라이브 불변: $(for g in daimadou95 doukyuusei2 kitan; do echo -n "$g $(sha256sum $LV/saves/$g/slot01.g98s 2>/dev/null | cut -c1-8)/$(sha256sum $BK/$g/slot01.g98s | cut -c1-8) "; done)"
echo "T=$T"; echo "== 종합: $([ $fail = 0 ] && echo 합격 || echo 불합격) | $(date -Is)"
