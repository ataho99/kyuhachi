#!/bin/bash
# 지시 #138: t52i 라이브 반영. run\ 불변. 사본 → (실행 중이면 옆에 두기 / 꺼져 있으면 교체) → 파일 넣기 → strings.txt 이동 → 밖 사본 시험.
P=/mnt/h/RETRO_SET/PC98_PLAYER; L=$P/live_v050/Kyuhachi; Z=$P/release/Kyuhachi-v0.5.0-t52i.zip; TS=$(date +%Y%m%d-%H%M%S); B=$P/live_v050/_old/backup_t52i_$TS
WANT=0c1a391e4760b922a077d67c1ed9fe2ec042030138759bef366859273b185db1; PS=/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
ZS=$(sha256sum "$Z" | cut -c1-64); echo "zip sha $ZS $( [ "$ZS" = "$WANT" ] && echo 일치 || echo 불일치)"; [ "$ZS" = "$WANT" ] || { echo "STOP"; exit 0; }
echo "== 라이브 현황"; ls -l "$L" | awk '{print $5, $9}'; ls -l "$L/res/fonts" | awk '{print $5, $9}'
SB=$(sha256sum "$L/Kyuhachi.exe" | cut -c1-64); echo "교체 전 exe sha $SB"
for f in settings.ini gamedb.ini; do [ -e "$L/$f" ] && echo "$f 교체 전 sha $(sha256sum "$L/$f" | cut -c1-64)"; done
SETB=$(sha256sum "$L/settings.ini" 2>/dev/null | cut -c1-64)
echo "== 1. 사본 -> $B"; mkdir -p "$B"
for f in settings.ini gamedb.ini strings.txt README.txt LICENSE.txt Kyuhachi.exe strings_ko.txt strings_en.txt strings_ja.txt; do [ -e "$L/$f" ] && cp -p "$L/$f" "$B/$f"; done
for d in "$L"/games/*/; do n=$(basename "$d"); for f in game.ini user.ini; do [ -e "$d$f" ] && { mkdir -p "$B/games/$n"; cp -p "$d$f" "$B/games/$n/$f"; }; done; done
[ -d "$L/res/fonts" ] && { mkdir -p "$B/res_fonts_list"; ls -l "$L/res/fonts" > "$B/res_fonts_list/ls.txt"; }
echo "사본 파일: $(cd "$B" && find . -type f | wc -l)개"; (cd "$B" && find . -type f | sort)
X=$P/_x_t52i_$TS; mkdir -p "$X"; unzip -o -q "$Z" -d "$X"; K=$X/Kyuhachi; echo "zip 풀기: $(ls $K | tr '\n' ' ')"
NEWSHA=$(sha256sum "$K/Kyuhachi.exe" | cut -c1-64); echo "새 exe sha $NEWSHA"
echo "== 2. 실행 중 확인"
RUN=$("$PS" -NoProfile -Command "Get-Process Kyuhachi -ErrorAction SilentlyContinue | Where-Object { \$_.Path -like '*live_v050*' } | ForEach-Object { \$_.Id }" 2>/dev/null | tr -d '\r' | tr '\n' ' ')
ALLP=$("$PS" -NoProfile -Command "Get-Process Kyuhachi -ErrorAction SilentlyContinue | ForEach-Object { \$_.Path }" 2>/dev/null | tr -d '\r' | tr '\n' ';')
echo "라이브 실행 중 PID: [${RUN}] | 전체 Kyuhachi 프로세스 경로: [${ALLP}]"
if [ -n "$(echo $RUN | tr -d ' ')" ]; then
  cp -f "$K/Kyuhachi.exe" "$L/Kyuhachi_t52i_new.exe"; echo "실행 중 -> 새 exe 를 옆에 둠: $L/Kyuhachi_t52i_new.exe (교체 안 함)"; REPL=0
else
  cp -f "$K/Kyuhachi.exe" "$L/Kyuhachi.exe"; echo "꺼져 있음 -> exe 교체"; REPL=1
fi
echo "== 3. 넣기"
cp -f "$K/README.txt" "$L/README.txt"; cp -f "$K/LICENSE.txt" "$L/LICENSE.txt"
for f in strings_ko.txt strings_en.txt strings_ja.txt; do cp -f "$K/$f" "$L/$f"; done
mkdir -p "$L/res/fonts"; for f in shinonome16_jp.fnt shinonome16_LICENSE.txt PUT_FONTS_HERE.txt; do cp -f "$K/res/fonts/$f" "$L/res/fonts/$f"; done
echo "== 4. strings.txt 이동"
if [ -e "$L/strings.txt" ]; then mv "$L/strings.txt" "$B/strings.txt.moved"; echo "strings.txt -> $B/strings.txt.moved"; else echo "라이브에 strings.txt 없음"; fi
echo "== 확인: 교체 후"
echo "exe sha $(sha256sum "$L/Kyuhachi.exe" | cut -c1-64) (교체 $REPL, 새 exe $NEWSHA)"
echo "settings.ini sha 전 $SETB 후 $(sha256sum "$L/settings.ini" 2>/dev/null | cut -c1-64)"
for f in README.txt LICENSE.txt strings_ko.txt strings_en.txt strings_ja.txt; do [ "$(sha256sum "$L/$f" | cut -c1-64)" = "$(sha256sum "$K/$f" | cut -c1-64)" ] && echo "$f 일치" || echo "$f 불일치"; done
echo "라이브 현재:"; ls -l "$L" | awk '{print $5, $9}'; ls -l "$L/res/fonts" | awk '{print $5, $9}'
echo "== 6. 밖 사본에서 시험 (같은 exe)"
T=$P/시험_live_t52i_$TS; rm -rf "$T"; mkdir -p "$T"; cp -r "$K" "$T/"; k=$T/Kyuhachi
[ "$REPL" = 1 ] && cp -f "$L/Kyuhachi.exe" "$k/Kyuhachi.exe" || cp -f "$L/Kyuhachi_t52i_new.exe" "$k/Kyuhachi.exe"
echo "시험 exe sha $(sha256sum "$k/Kyuhachi.exe" | cut -c1-64)"
mkdir -p "$k/games/kitan"; cp "$L/games/kitan/game.ini" "$k/games/kitan/"; [ -e "$L/games/kitan/user.ini" ] && cp "$L/games/kitan/user.ini" "$k/games/kitan/"
for f in pc98player_font.bmp pc98player_font.fnt pc98player_font_sbcs.fnt; do [ -e "$L/res/fonts/$f" ] && cp "$L/res/fonts/$f" "$k/res/fonts/$f"; done
cp "$L/res/fonts/pc98player_font.bmp" "$k/res/fonts/kyuhachi_font.bmp" 2>/dev/null; cp "$L/res/fonts/pc98player_font.fnt" "$k/res/fonts/kyuhachi_font.fnt" 2>/dev/null; cp "$L/res/fonts/pc98player_font_sbcs.fnt" "$k/res/fonts/kyuhachi_font_sbcs.fnt" 2>/dev/null
o=$T/out; mkdir -p "$o"; ( cd "$k" && timeout 90 "$k/Kyuhachi.exe" --windowed --hidewin --noboot --sec 9 --shot 7 --out "$(wslpath -w "$o")" kitan; echo "부팅 rc=$?" ) 2>&1 | tail -2
grep -a -n -E 'ERROR|CRASH|run time reached|글꼴 =' "$o/kyuhachi_log.txt" | cut -c1-200 | head -5; ls "$o"
o2=$T/out_ko; mkdir -p "$o2"; ( cd "$k" && timeout 60 "$k/Kyuhachi.exe" --windowed --hidewin --noboot --menushot --winsize 1280x720 --lang 0 --out "$(wslpath -w "$o2")"; echo "메뉴캡처 rc=$?" ) 2>&1 | tail -1; ls "$o2" | grep -i png
rm -rf "$X"
echo "T=$T"; echo "B=$B"; echo DONE
