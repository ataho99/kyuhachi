#!/bin/bash
# 지시 #139: 리듬 음원 안내 — 문서만 바뀐 zip t52i2(exe 동일) + 라이브에 안내 txt + todo_next.md 추가
P=/mnt/h/RETRO_SET/PC98_PLAYER; Z1=$P/release/Kyuhachi-v0.5.0-t52i.zip; Z2=$P/release/Kyuhachi-v0.5.0-t52i2.zip; L=$P/live_v050/Kyuhachi; TS=$(date +%H%M%S); X=$P/_x_t52i2_$TS
[ -e "$Z2" ] && { mkdir -p $P/release/_old; mv "$Z2" "$P/release/_old/Kyuhachi-v0.5.0-t52i2_try$TS.zip"; echo "이전 t52i2 -> _old"; }
echo "t52i zip sha $(sha256sum "$Z1" | cut -c1-64)"
mkdir -p "$X"; unzip -o -q "$Z1" -d "$X" 2>/dev/null; K=$X/Kyuhachi; ls "$K" | tr '\n' ' '; echo
EXE1=$(sha256sum "$K/Kyuhachi.exe" | cut -c1-64)
mkdir -p "$K/res/rhythm"
{ printf '\xEF\xBB\xBF'
  printf 'Put the six YM2608 rhythm (drum) sounds here:\r\n  2608_bd.wav  2608_sd.wav  2608_top.wav  2608_hh.wav  2608_tom.wav  2608_rim.wav\r\nNot bundled because of copyright. Without them only the drums of the FM music are silent; everything else works normally. Do not delete this folder.\r\n\r\n'
  printf 'YM2608 리듬(드럼) 음원 6개를 여기에 넣으세요:\r\n  2608_bd.wav  2608_sd.wav  2608_top.wav  2608_hh.wav  2608_tom.wav  2608_rim.wav\r\n저작권 때문에 동봉하지 않습니다. 없으면 FM 음악의 드럼 소리만 빠지고 나머지는 정상입니다. 이 폴더는 지우지 마세요.\r\n'
} > "$K/res/rhythm/PUT_RHYTHM_HERE.txt"
perl -0777 -i -pe '
  s/(  res\/fonts\/       put font files here[^\r\n]*\r\n)/$1  res\/rhythm\/      put the 6 YM2608 rhythm WAVs here (drums; not bundled, see below)\r\n/ or die "layout anchor";
  my $sec = "=== RHYTHM SOUND (drums) \/ 리듬(드럼) 음원 ===\r\nPut the six YM2608 rhythm sounds 2608_bd.wav, 2608_sd.wav, 2608_top.wav, 2608_hh.wav, 2608_tom.wav, 2608_rim.wav in res\/rhythm\/ . They are not bundled (copyright). Without them only the drums of the FM music are silent; everything else works normally.\r\nYM2608 리듬 음원 6개(2608_bd\/sd\/top\/hh\/tom\/rim.wav)를 res\/rhythm\/ 에 넣으세요. 저작권 때문에 동봉하지 않습니다. 없으면 FM 음악의 드럼 소리만 빠지고 나머지는 정상입니다.\r\n\r\n";
  s/(=== KEYS \/ )/$sec$1/ or die "keys anchor";
' "$K/README.txt"
grep -a -c "RHYTHM SOUND" "$K/README.txt"
EXE2=$(sha256sum "$K/Kyuhachi.exe" | cut -c1-64)
( cd "$X" && if command -v zip >/dev/null 2>&1; then zip -r -X -q "$Z2" Kyuhachi; else python3 -c "
import zipfile,os,sys
z=zipfile.ZipFile(sys.argv[1],'w',zipfile.ZIP_DEFLATED)
for r,d,f in os.walk('Kyuhachi'):
    z.write(r)
    for n in f: z.write(os.path.join(r,n))
z.close()" "$Z2"; fi )
echo "t52i2 zip: $(stat -c %s "$Z2") B sha256 $(sha256sum "$Z2" | cut -c1-64)"
V=$P/_v_t52i2_$TS; mkdir -p "$V"; unzip -o -q "$Z2" -d "$V" 2>/dev/null
echo "항목:"; unzip -l "$Z2" | awk 'NR>3{print $4}' | grep -v '^$'
echo "zip 안 exe sha $(sha256sum "$V/Kyuhachi/Kyuhachi.exe" | cut -c1-64) | t52i zip 안 exe $EXE1 | 동일: $([ "$EXE1" = "$(sha256sum "$V/Kyuhachi/Kyuhachi.exe" | cut -c1-64)" ] && echo 예 || echo 아니오)"
echo "한글 이름: $(unzip -l "$Z2" | awk 'NR>3{print $4}' | grep -cP '[^\x00-\x7F]') | 잔여물: $(unzip -l "$Z2" | awk 'NR>3{print $4}' | grep -ciE '\.(bak|old|tmp)|_old')"
echo "README 리듬 줄:"; grep -a -n -i "rhythm" "$V/Kyuhachi/README.txt" | cut -c1-200
echo "PUT_RHYTHM_HERE.txt:"; cat "$V/Kyuhachi/res/rhythm/PUT_RHYTHM_HERE.txt" | tr -d '\r'
echo "== 라이브: res\\rhythm 현황"; ls -l "$L/res/rhythm" 2>/dev/null | awk '{print $5, $9}' || echo "(폴더 없음)"
mkdir -p "$L/res/rhythm"; [ -e "$L/res/rhythm/PUT_RHYTHM_HERE.txt" ] && echo "이미 있음(덮어쓰지 않음)" || cp "$K/res/rhythm/PUT_RHYTHM_HERE.txt" "$L/res/rhythm/PUT_RHYTHM_HERE.txt"
echo "라이브 exe sha $(sha256sum "$L/Kyuhachi.exe" | cut -c1-64) | settings.ini sha $(sha256sum "$L/settings.ini" | cut -c1-64)"; ls -l "$L/res/rhythm" | awk '{print $5, $9}'
echo "== todo_next.md 추가"
cat >> "$P/docs/todo_next.md" <<'EOT'
- [슨상 11-03 · #139] 리듬(드럼) 음원 안내: ① exe 는 이미 `<exe>\res\rhythm\` 한 곳만 읽는다(2608_bd/sd/top/hh/tom/rim.wav, 그 폴더를 현재 폴더로 지정) → 폴더 이동 코드 변경은 필요 없음. ② 앱 안 경고: 지금 경고(「리듬음 파일 없음 — 넣는 곳」 로그·구석 표시)는 작품 profile 의 `sound=` 값에 rhythm/2608/86 이 있을 때만 검사한다. 환세희담은 이 조건에 안 걸려 파일이 없어도 로그에 경고가 안 남는다(FM 쪽은 6개를 요청함). → 조건을 풀어, FM 리듬을 요청하는 작품이면 파일 6개가 다 없을 때 시작 때 한 번 구석에 「드럼 음원 없음 — res/rhythm 안내 참고」 를 잠깐 표시(설정에서 끌 수 있게). 문구는 ko/en/ja 3개 파일에 같이.
EOT
tail -n 3 "$P/docs/todo_next.md" | cut -c1-160
rm -rf "$X" "$V"; echo DONE
