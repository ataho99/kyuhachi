#!/bin/bash
# T11/R-1 의존 방향: src/<모듈>/ 파일의 #include "x" 가 허용된 모듈만 가리키는지. 모듈 폴더가 없으면(평면 src) 통과(정보만).
# 허용(모듈 계획 §2-4): app→ui,input,settings,text,platform,core / ui→text,settings,input,platform / input→settings,platform,core(E1) / settings→platform / text→platform / platform→(없음)
R=$(cd "$(dirname "$0")/.." && pwd); D=${1:-$R/src}
declare -A OK=( [app]="ui input settings text platform core" [ui]="text settings input platform" [input]="settings platform core" [settings]="platform" [text]="platform" [platform]="" )
bad=0; mods=0
for m in app ui input settings text platform; do [ -d "$D/$m" ] || continue; mods=$((mods+1))
  for f in $(find $D/$m -name '*.h' -o -name '*.cpp'); do
    for inc in $(grep -hoE '^#include "[^"]+"' $f | sed 's/#include "//; s/"//'); do
      t=""; for m2 in app ui input settings text platform; do [ -e "$D/$m2/$inc" ] || [ -n "$(find $D/$m2 -name "$(basename $inc)" 2>/dev/null | head -1)" ] && { t=$m2; break; }; done
      [ -z "$t" ] || [ "$t" = "$m" ] && continue
      case " ${OK[$m]} " in *" $t "*) ;; *) echo "  위반: $f ($m) → $inc ($t)"; bad=$((bad+1));; esac; done; done; done
echo "deps modules=$mods violations=$bad"; [ $bad = 0 ]
