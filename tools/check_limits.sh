#!/bin/bash
# T11/R-6: 파일 ≤800줄·≤60KB, 한 줄 ≤140자. 예외는 tools/limits_exceptions.txt (한 줄에 파일명 이유). 출력: limits files=N over_lines=N over_bytes=N long_lines=N unlisted=N (unlisted>0 이면 실패)
R=$(cd "$(dirname "$0")/.." && pwd); D=${1:-$R/src}; EX=$R/tools/limits_exceptions.txt
ol=0; ob=0; ll=0; ul=0; n=0
for f in $(ls $D/*.h $D/*.cpp $D/*/*.h $D/*/*.cpp $D/*/*/*.h $D/*/*/*.cpp 2>/dev/null); do b=$(basename $f); case $b in *embed*|*.inc) continue;; esac; n=$((n+1))
  L=$(wc -l < $f); B=$(wc -c < $f); W=$(perl -CSD -ne 'chomp; $c++ if length($_)>140; END{print $c+0}' $f); bad=0
  [ $L -gt 800 ] && { ol=$((ol+1)); bad=1; }; [ $B -gt 61440 ] && { ob=$((ob+1)); bad=1; }; [ $W -gt 0 ] && { ll=$((ll+W)); bad=1; }
  if [ $bad = 1 ] && ! grep -q "^$b " "$EX" 2>/dev/null; then ul=$((ul+1)); echo "  미등록 초과: $b lines=$L bytes=$B long=$W"; fi; done
echo "limits files=$n over_lines=$ol over_bytes=$ob long_lines=$ll unlisted=$ul"; [ $ul = 0 ]
