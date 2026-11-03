#!/bin/bash
# SF 회귀: t53a 를 t52i 와 같은 방법으로 시험(T1·T2·T3·T7 + 언어별 로그) 하고 golden 과 비교. 출력 = work/t53a/reg_out.txt
P=/mnt/h/RETRO_SET/PC98_PLAYER; K=$P/kyuhachi; W=$P/build/r020/work/t53a; O=$W/reg_out.txt; G=$K/tests/golden/t52i
exec > "$O" 2>&1
echo "== reg start $(date -Is)"
ZB=$P/build/r020/out_t53a/Kyuhachi-v0.5.0-t53a.zip; echo "대상 = t53a zip sha256 $(sha256sum "$ZB" | cut -c1-16) size $(stat -c %s "$ZB")"
sed -e "s#ZB=\$P/release/Kyuhachi-v0.5.0-t52i.zip#ZB=$ZB#" -e 's#시험_t52i_#시험_t53a_#' -e "s#KS=\$P/build/r020/work/t52i/uikeys_t52i.txt#KS=$P/build/r020/work/t52i/uikeys_t52i.txt#" -e "s#build t52'#build t53'#" $P/build/r020/work/t52i/test_t52i.sh > $W/test_t53a.sh
grep -n 'ZB=\|KS=\|시험_' $W/test_t53a.sh | head -4 | cut -c1-160
bash $W/test_t53a.sh > $W/reg_raw.txt 2>&1
T=$(grep -a '^T=' $W/reg_raw.txt | tail -1 | cut -d= -f2-); echo "시험 폴더: $T"
[ -d "$T" ] || { echo "FAIL: 시험 폴더 없음"; tail -5 $W/reg_raw.txt; exit 3; }
echo "-- raw 끝부분"; tail -4 $W/reg_raw.txt | cut -c1-200
echo "== T2 (글꼴 0개): 로그 줄 vs golden"; grep -a -E 'ERROR|CRASH|not found|neither|run time reached|글꼴 =' $T/zero/out/kyuhachi_log.txt | sed -E 's/^ *[0-9]+ //' > $W/n_zero.txt; sed -E 's/^ *[0-9]+ //' $G/boot_zero/log_key_lines.txt > $W/g_zero.txt; diff $W/g_zero.txt $W/n_zero.txt > $W/d_zero.txt && echo "T2 same" || { echo "T2 DIFF"; head -8 $W/d_zero.txt | cut -c1-200; }
echo "== T3 (부팅): 로그 줄 vs golden + 캡처"; grep -a -E 'ERROR|CRASH|not found|neither|run time reached|글꼴 =' $T/f1/out/kyuhachi_log.txt | sed -E 's/^ *[0-9]+ //' > $W/n_f1.txt; sed -E 's/^ *[0-9]+ //' $G/boot_f1/log_key_lines.txt > $W/g_f1.txt; diff $W/g_f1.txt $W/n_f1.txt > $W/d_f1.txt && echo "T3 log same" || { echo "T3 log DIFF"; head -8 $W/d_f1.txt | cut -c1-200; }
echo "캡처 장수/크기 새: $(ls -l $T/f1/out/*.png 2>/dev/null | awk '{print $5}' | tr '\n' ' ') | golden: $(ls -l $G/boot_f1/*.png 2>/dev/null | awk '{print $5}' | tr '\n' ' ')"
for L in 0 1 2; do
 echo "== T1/T7 uikeys lang$L"; d=$T/f1/out_keys$L
 perl $K/tools/t1_compare.pl $G/lang$L $d | tail -3
 diff <(cat $G/lang$L/settings_uikeys.ini) <(cat $d/settings_uikeys.ini) > /dev/null && echo "T7 settings_uikeys.ini diff 0" || echo "T7 DIFF lang$L"
 grep -a -E 'T52I|uikeys|UITEST|VALDUMP|FAIL' $d/*log*.txt | sed -E 's/^ *[0-9]+ //' > $W/n_k$L.txt; sed -E 's/^ *[0-9]+ //' $G/lang$L/log_key_lines.txt > $W/g_k$L.txt
 diff $W/g_k$L.txt $W/n_k$L.txt > $W/d_k$L.txt && echo "log key lines same ($(wc -l < $W/n_k$L.txt))" || { echo "log key lines DIFF"; head -6 $W/d_k$L.txt | cut -c1-200; }
done
echo "== T1 전체 57장 × 3 언어"
k=$T/f1/Kyuhachi
for L in 0 1 2; do o=$T/f1/out_full$L; rm -rf "$o"; mkdir -p "$o"
 ( cd "$k" && timeout 150 "$k/Kyuhachi.exe" --hidewin --uitest "$(wslpath -w "$o")" --out "$(wslpath -w "$o")" --lang $L >/dev/null 2>&1 ); echo "full$L png=$(ls $o/*.png | wc -l)"
 perl $K/tools/t1_compare.pl $G/full$L $o | tail -4
 grep -a -E 'FAIL' $o/*log*.txt | sed -E 's/^ *[0-9]+ //' | cut -c1-90 | sort | uniq -c
done
echo "T=$T"; echo "== reg done $(date -Is)"
