#!/bin/bash
# t52i 시험 보충: 동작 환경 값 바꾸고 ×
P=/mnt/h/RETRO_SET/PC98_PLAYER; T=$P/시험_t52i_061057; KS=$P/build/r020/work/t52i/uikeys_t52i.txt
keys() { k=$T/$1/Kyuhachi; o=$T/$1/out_keys$2; rm -rf "$o"; mkdir -p "$o"
  ( cd "$k" && timeout 150 "$k/Kyuhachi.exe" --hidewin --uitest "$(wslpath -w "$o")" --uikeys "$(wslpath -w "$KS")" --out "$(wslpath -w "$o")" --lang $2; echo "L=$2 rc=$?" ) 2>&1 | tail -2
  grep -a -n -E 'T52I|uikeys: 설정 저장|UITEST done|FAIL' "$o"/*log*.txt 2>/dev/null | cut -c1-230 | head -12
  echo "저장된 settings_uikeys.ini: $(grep -a -E '^(bgm|se)=' "$o/settings_uikeys.ini" 2>/dev/null | tr '\n' ' ')"; ls "$o" | grep -E 't52i_' | tr '\n' ' '; echo; }
keys f1 0
cat "$T/f1/out_keys0/settings_uikeys.ini" | head -30
DONE
