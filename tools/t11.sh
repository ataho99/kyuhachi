#!/bin/bash
# T11 구조 검사 묶음. 사용: tools/t11.sh [--write-baseline]   (strings 위치: 환경변수 KH_STRINGS_DIR, 기본 kyuhachi\res\strings)
# 합격 = check_keys(개수 일치·0) · check_deps 위반 0 · limits 미등록 0 · literals 가 기준선보다 늘지 않음(S4 에서 줄이기 시작).
R=$(cd "$(dirname "$0")/.." && pwd); S=${KH_STRINGS_DIR:-$R/res/strings}; BL=$R/tests/t11_baseline.txt; rc=0
k=$(perl $R/tools/check_keys.pl $S/strings_ko.txt $S/strings_en.txt $S/strings_ja.txt $R/src); krc=$?; echo "$k"; [ $krc = 0 ] || rc=1
d=$(bash $R/tools/check_deps.sh); drc=$?; echo "$d"; [ $drc = 0 ] || rc=1
l=$(bash $R/tools/check_limits.sh); lrc=$?; echo "$l"; [ $lrc = 0 ] || rc=1
t=$(bash $R/tools/check_literals.sh $R/src); echo "$t"
now=$(echo "$t" | sed -E 's/.*outside_T=([0-9]+).*/\1/'); keys=$(echo "$k" | sed -E 's/keys ko=([0-9]+).*/\1/')
if [ "${1:-}" = "--write-baseline" ]; then printf 'keys %s\nliterals_outside_T %s\n' "$keys" "$now" > $BL; echo "기준선 기록: $(tr '\n' ' ' < $BL)"
elif [ -f $BL ]; then bk=$(awk '$1=="keys"{print $2}' $BL); bt=$(awk '$1=="literals_outside_T"{print $2}' $BL)
  [ "$keys" = "$bk" ] || { echo "키 개수 변경: 기준 $bk → $keys (구조 빌드는 불변이어야 함)"; rc=1; }
  [ "$now" -le "$bt" ] || { echo "한글 리터럴(T 밖) 증가: 기준 $bt → $now"; rc=1; }
fi
echo "T11 $([ $rc = 0 ] && echo PASS || echo FAIL)"; exit $rc
