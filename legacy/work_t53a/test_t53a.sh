#!/bin/bash
# 지시 #136 2항 시험: 새로 푼 폴더(시험_t53a_<시각>). live 폴더 안에서는 부팅하지 않는다(읽기 복사만).
P=/mnt/h/RETRO_SET/PC98_PLAYER; ZB=/mnt/h/RETRO_SET/PC98_PLAYER/build/r020/out_t53a/Kyuhachi-v0.5.0-t53a.zip; ZH=$P/release/Kyuhachi-v0.5.0-t52h.zip; T=$P/시험_t53a_$(date +%H%M%S)
FD=$P/live_v050/Kyuhachi/res/fonts; PS=/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe; KS=/mnt/h/RETRO_SET/PC98_PLAYER/build/r020/work/t52i/uikeys_t52i.txt
mkdir -p "$T"
ex() { rm -rf "$2"; mkdir -p "$2"; if command -v unzip >/dev/null 2>&1; then unzip -o -q "$1" -d "$2" 2>/dev/null; else "$PS" -NoProfile -Command "Expand-Archive -LiteralPath '$(wslpath -w "$1")' -DestinationPath '$(wslpath -w "$2")' -Force"; fi; }
mk() { z=${3:-$ZB}; d=$T/$1; ex "$z" "$d"; k=$d/Kyuhachi
  mkdir -p "$k/games/kitan"; cp "$P/live_v050/Kyuhachi/games/kitan/game.ini" "$k/games/kitan/game.ini"; [ -e "$P/live_v050/Kyuhachi/games/kitan/user.ini" ] && cp "$P/live_v050/Kyuhachi/games/kitan/user.ini" "$k/games/kitan/user.ini"
  if [ "$2" = menu ]; then cp "$FD/pc98player_font.bmp" "$k/res/fonts/kyuhachi_font.bmp"; cp "$FD/pc98player_font.fnt" "$k/res/fonts/kyuhachi_font.fnt"; cp "$FD/pc98player_font_sbcs.fnt" "$k/res/fonts/kyuhachi_font_sbcs.fnt"; fi
  echo "[$1] 글꼴 종류=$2 | res/fonts: $(ls "$k/res/fonts" | tr '\n' ' ') | exe sha256: $(sha256sum "$k/Kyuhachi.exe" | cut -c1-64)"; }
run() { k=$T/$1/Kyuhachi; o=$T/$1/out; rm -rf "$o"; mkdir -p "$o"
  ( cd "$k" && timeout 90 "$k/Kyuhachi.exe" --windowed --hidewin --noboot --sec 9 --shot 7 --out "$(wslpath -w "$o")" kitan; echo "rc=$?" ) 2>&1 | tail -3
  grep -a -n -E 'ERROR|CRASH|not found|neither|run time reached|글꼴 =' "$o/kyuhachi_log.txt" 2>/dev/null | grep -v 'shader' | cut -c1-230 | head -8; ls "$o" | head -8; }
keys() { k=$T/$1/Kyuhachi; o=$T/$1/out_keys$2; rm -rf "$o"; mkdir -p "$o"
  ( cd "$k" && timeout 150 "$k/Kyuhachi.exe" --hidewin --uitest "$(wslpath -w "$o")" --uikeys "$(wslpath -w "$KS")" --out "$(wslpath -w "$o")" --lang $2; echo "L=$2 rc=$?" ) 2>&1 | tail -2
  grep -a -n -E 'T52I|uikeys: 설정 저장|UITEST done|FAIL' "$o"/*log*.txt 2>/dev/null | cut -c1-230 | head -12
  echo "저장된 settings_uikeys.ini: $(grep -a -E '^(bgm|se)=' "$o/settings_uikeys.ini" 2>/dev/null | tr '\n' ' ')"; ls "$o" | grep -E 't52i_' | tr '\n' ' '; echo; }
echo "== ① 글꼴 0개"; mk zero none; run zero
echo "== ② 글꼴 1개(메뉴 글꼴만) · 작품 1개 부팅"; mk f1 menu; run f1
echo "== ③④ B1~B3 화면 + 동작 환경 ×  (ko / en / ja)"
keys f1 0; keys f1 1; keys f1 2
echo "== zip 안 트리 · 한글 이름 · 잔여물"
if command -v unzip >/dev/null 2>&1; then unzip -l "$ZB" | awk 'NR>3{print $4}' | grep -v '^$' > "$T/ziplist.txt"; else "$PS" -NoProfile -Command "Add-Type -A System.IO.Compression.FileSystem; [IO.Compression.ZipFile]::OpenRead('$(wslpath -w "$ZB")').Entries | ForEach-Object { \$_.FullName }" | tr -d '\r' > "$T/ziplist.txt"; fi
echo "항목 수: $(wc -l < "$T/ziplist.txt") | 한글 이름: $(grep -cP '[^\x00-\x7F]' "$T/ziplist.txt") | .bak/.old/.tmp: $(grep -ciE '\.(bak|old|tmp)|_old' "$T/ziplist.txt")"
cat "$T/ziplist.txt" | head -30
kz=$T/zipcheck; ex "$ZB" "$kz"
echo "README 'to be filled': $(grep -a -c 'to be filled' "$kz/Kyuhachi/README.txt" 2>/dev/null) | 소스 줄: $(grep -a -c -i 'written offer\|source' "$kz/Kyuhachi/README.txt") | 버전: $(grep -a -m1 'build t53' "$kz/Kyuhachi/README.txt" | tr -d '\r')"
echo "README 조작 절:"; grep -a -n -A6 'KEYS' "$kz/Kyuhachi/README.txt" | cut -c1-300
echo "zip 안 strings_*.txt: $(ls "$kz/Kyuhachi" | grep -c '^strings_') | strings.txt(없어야): $(ls "$kz/Kyuhachi" | grep -c '^strings.txt$') | settings.ini/logs: $(ls "$kz/Kyuhachi" | grep -ciE 'settings.ini|logs')"
echo "zip 안 문구 줄 수: ko=$(grep -a -c . "$kz/Kyuhachi/strings_ko.txt") en=$(grep -a -c . "$kz/Kyuhachi/strings_en.txt") ja=$(grep -a -c . "$kz/Kyuhachi/strings_ja.txt")"
echo "zip 안 res/fonts: $(ls -l "$kz/Kyuhachi/res/fonts" | awk 'NR>1{print $5, $9}' | tr '\n' ';')"
echo "shinonome16_jp.fnt zip 안 sha256: $(sha256sum "$kz/Kyuhachi/res/fonts/shinonome16_jp.fnt" | cut -c1-64) | work 쪽: $(sha256sum "$P/build/r020/work/t52i/shinonome16_jp.fnt" | cut -c1-64)"
echo "zip: $(stat -c %s "$ZB") sha256 $(sha256sum "$ZB" | cut -c1-64)"
echo "T=$T"
echo DONE
