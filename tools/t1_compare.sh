#!/bin/bash
# usage: t1_compare.sh <golden폴더> <새폴더>  — 같은 이름 PNG 의 y=16..399 영역 화소 비교
G=$1; N=$2; bad=0; n=0
for f in "$G"/*.png; do b=$(basename "$f"); [ -f "$N/$b" ] || { echo "MISSING $b"; bad=1; continue; }
  n=$((n+1))
  if command -v magick >/dev/null; then d=$(magick compare -metric AE -crop 640x384+0+16 "$f" "$N/$b" null: 2>&1 | head -1)
  elif command -v compare >/dev/null; then d=$(compare -metric AE "$f[640x384+0+16]" "$N/$b[640x384+0+16]" null: 2>&1 | head -1)
  else echo "NO image tool"; exit 9; fi
  [ "${d%% *}" = "0" ] || { echo "DIFF $b ($d)"; bad=1; }
done; echo "compared=$n bad=$bad"; exit $bad
