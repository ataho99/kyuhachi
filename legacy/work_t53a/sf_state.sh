#!/bin/bash
# SF 상태 시험(지시 #144 2항). 라이브 saves 는 건드리지 않고 백업_saves 사본만 쓴다. 출력 = work/t53a/state_out.txt
P=/mnt/h/RETRO_SET/PC98_PLAYER; W=$P/build/r020/work/t53a; O=$W/state_out.txt; exec > "$O" 2>&1
ZN=$P/build/r020/out_t53a/Kyuhachi-v0.5.0-t53a.zip; ZI=$P/release/Kyuhachi-v0.5.0-t52i.zip; LV=$P/live_v050/Kyuhachi; BK=$(ls -d $P/백업_saves_* | head -1)
OLDF=$P/live_v050/_old/backup_t52i_20261103-064347/Kyuhachi.exe
T=$P/시험_t53a_state_$(date +%H%M%S); PS=/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
mkdir -p "$T"; w() { wslpath -w "$1"; }
echo "== 시작 $(date -Is) | 대상 zip sha $(sha256sum $ZN | cut -c1-16) | 백업 사본 = $BK"
ex() { rm -rf "$2"; mkdir -p "$2"; if command -v unzip >/dev/null 2>&1; then unzip -o -q "$1" -d "$2" 2>/dev/null; else "$PS" -NoProfile -Command "Expand-Archive -LiteralPath '$(w $1)' -DestinationPath '$(w $2)' -Force"; fi; }
mk() { ex "$2" "$T/$1"; k=$T/$1/Kyuhachi; mkdir -p "$k/saves"
  for g in kitan doukyuusei2 daimadou95; do mkdir -p "$k/games/$g"; cp "$LV/games/$g/game.ini" "$k/games/$g/" 2>/dev/null; [ -e "$LV/games/$g/user.ini" ] && cp "$LV/games/$g/user.ini" "$k/games/$g/"; done
  cp "$LV"/res/fonts/* "$k/res/fonts/" 2>/dev/null; cp "$LV/res/fonts/pc98player_font.bmp" "$k/res/fonts/kyuhachi_font.bmp" 2>/dev/null; cp "$LV/res/fonts/pc98player_font.fnt" "$k/res/fonts/kyuhachi_font.fnt" 2>/dev/null; cp "$LV/res/fonts/pc98player_font_sbcs.fnt" "$k/res/fonts/kyuhachi_font_sbcs.fnt" 2>/dev/null; }
run() { # run <폴더> <이름> <작품> <인자...>   (환경변수는 호출 앞에 export)
  k=$T/$1/Kyuhachi; o=$T/$1/$2; g=$3; shift 3; rm -rf "$o"; mkdir -p "$o"
  rc=$( cd "$k" && timeout 70 "$k/Kyuhachi.exe" --windowed --hidewin --noboot --out "$(w $o)" "$@" $g >/dev/null 2>&1; echo $? )
  echo -n "rc=$rc "; grep -a -h -E 'gensei_host r024.* start,|SAVE .* (OK|FAIL)|LOAD .* (OK|FAIL)|CRASH code|access violation|PC98_SaveState: (loading|previous)' "$o"/kyuhachi_log.txt 2>/dev/null | sed -E 's/^ *[0-9]+ HOST //; s#H:\\RETRO_SET\\PC98_PLAYER\\##' | cut -c1-200 | head -6 | tr '\n' '|'
  echo -n " png=$(ls -l $o/*.png 2>/dev/null | awk '{print $5}' | tr '\n' ',')"; echo; }
mk N "$ZN"; mk I "$ZI"; mk F "$ZI"; [ -f "$OLDF" ] && cp "$OLDF" "$T/F/Kyuhachi/Kyuhachi.exe"
echo "exe sha: N=$(sha256sum $T/N/Kyuhachi/Kyuhachi.exe | cut -c1-16) I(t52i)=$(sha256sum $T/I/Kyuhachi/Kyuhachi.exe | cut -c1-16) F(옛)=$(sha256sum $T/F/Kyuhachi/Kyuhachi.exe | cut -c1-16)"
N=$T/N/Kyuhachi
echo "== S1 같은 부팅 저장→불러오기"
echo -n "kitan: "; run N s1a kitan --sec 11 --save 6 --load 8 --state "$(w $N/s1a.g98s)"
echo -n "doukyuusei2: "; run N s1b doukyuusei2 --sec 11 --save 6 --load 8 --state "$(w $N/s1b.g98s)"
echo "== S2 저장(프로세스 1) → 다른 프로세스에서 불러오기 14회"
echo -n "저장: "; run N s2a kitan --sec 9 --save 6 --state "$(w $N/s2.g98s)"; ls -l $N/s2.g98s | awk '{print "s2 크기", $5}'
for L in 2.0 2.2 2.4 2.6 2.8 3.0 3.2 3.4 3.6 3.8 4.0 4.5 5.0 6.0; do echo -n "load@$L: "; run N s2b_$L kitan --sec 9 --load $L --state "$(w $N/s2.g98s)"; done
echo "== S3 라이브 세이브 사본(백업_saves) → t53a 로 불러오기 (대조: t52i 로 같은 사본)"
for g in daimadou95 doukyuusei2 kitan; do cp "$BK/$g/slot01.g98s" "$N/L_$g.g98s"; cp "$BK/$g/slot01.g98s" "$T/I/Kyuhachi/L_$g.g98s"; echo "사본 $g sha16 $(sha256sum $N/L_$g.g98s | cut -c1-16) 크기 $(stat -c %s $N/L_$g.g98s)"; done
for g in daimadou95 doukyuusei2 kitan; do for L in 3.0 6.0; do echo -n "t53a $g@$L: "; run N s3_${g}_$L $g --sec 10 --load $L --shot 9 --state "$(w $N/L_$g.g98s)"; done; echo -n "t52i $g@3.0 (대조): "; run I s3i_$g $g --sec 10 --load 3.0 --state "$(w $T/I/Kyuhachi/L_$g.g98s)"; done
for L in 2.0 2.4 4.0 5.0; do echo -n "t53a doukyuusei2 반복@$L: "; run N s3r_$L doukyuusei2 --sec 9 --load $L --state "$(w $N/L_doukyuusei2.g98s)"; done
echo "== S4 이어하기 경로(saves\\<작품>\\slot01.g98s, UI 이어하기와 같은 코드) — 사본으로"
for g in doukyuusei2 kitan daimadou95; do mkdir -p $N/saves_resume/$g; cp "$BK/$g/slot01.g98s" $N/saves_resume/$g/slot01.g98s; echo -n "resume $g: "; run N s4_$g $g --sec 10 --resume 1 --savedir "$(w $N/saves_resume)"; done
echo "== S5 다른 빌드가 저장한 상태 → t53a 로 불러오기"
echo -n "t52i 저장: "; run I s5a kitan --sec 9 --save 6 --state "$(w $T/I/Kyuhachi/s5i.g98s)"; cp $T/I/Kyuhachi/s5i.g98s $N/s5i.g98s
echo -n "t53a 로 불러오기(t52i 저장분): "; run N s5b kitan --sec 9 --load 3 --state "$(w $N/s5i.g98s)"
if [ -f "$OLDF" ]; then echo -n "옛 exe 저장: "; run F s5c kitan --sec 9 --save 6 --state "$(w $T/F/Kyuhachi/s5f.g98s)"; cp $T/F/Kyuhachi/s5f.g98s $N/s5f.g98s; echo -n "t53a 로 불러오기(옛 exe 저장분): "; run N s5d kitan --sec 9 --load 3 --state "$(w $N/s5f.g98s)"; fi
echo "== S6 잘린 파일 · 한 바이트 깨짐 · 없는 파일 (t53a 로 만든 s2)"
F=$N/s2.g98s; SZ=$(stat -c %s "$F")
head -c $((SZ/2)) "$F" > $N/s6a.g98s; head -c $((SZ-1)) "$F" > $N/s6b.g98s; head -c 100 "$F" > $N/s6c.g98s; : > $N/s6d.g98s
for n in a b c d; do echo -n "6$n($(stat -c %s $N/s6$n.g98s)B): "; run N s6$n kitan --sec 6 --load 3 --state "$(w $N/s6$n.g98s)"; done
cp "$F" $N/s6e.g98s; printf '\x55' | dd of=$N/s6e.g98s bs=1 seek=$((SZ/2)) conv=notrunc 2>/dev/null; echo -n "6e(한 바이트): "; run N s6e kitan --sec 6 --load 3 --state "$(w $N/s6e.g98s)"
echo -n "6f(없는 파일): "; run N s6f kitan --sec 6 --load 3 --state "$(w $N/없음.g98s)"
echo "== S7 형식이 정말 다른 파일(헤더·길이표를 고치고 CRC 다시 계산) — 거부돼야 하고 크래시는 없어야 함"
craft() { perl -MCompress::Zlib -e 'local $/; open F,"<:raw",$ARGV[0]; $d=<F>; close F; $m=$ARGV[2]; $b=substr($d,0,length($d)-4);
 if($m eq "abi"){ $sl=unpack("v",substr($b,8,2)); substr($b,10+$sl,4)=pack("V",99); }
 elsif($m eq "ver"){ substr($b,4,4)=pack("V",7); }
 elsif($m eq "len"){ $p=8; $sl=unpack("v",substr($b,$p,2)); $p+=2+$sl; $p+=4; $p+=8; $nd=unpack("V",substr($b,$p,4)); $p+=4;
   for(1..$nd){ $n=unpack("v",substr($b,$p,2)); $p+=2+$n+12; } $tl=unpack("V",substr($b,$p,4)); $p+=4+$tl; $p+=4;
   $nl=unpack("v",substr($b,$p,2)); $p+=2+$nl; ($lo,$hi)=unpack("VV",substr($b,$p,8)); substr($b,$p,8)=pack("VV",$lo+1,$hi); substr($b,$p+8+$lo,0)="\0"; }
 open O,">:raw",$ARGV[1]; print O $b, pack("V",crc32($b)); close O;' "$1" "$2" "$3"; }
craft $F $N/s7a.g98s abi; craft $F $N/s7b.g98s ver; craft $F $N/s7c.g98s len
for n in a b c; do echo -n "7$n: "; run N s7$n kitan --sec 6 --load 3 --state "$(w $N/s7$n.g98s)"; done
echo "== S8 F5 덮어쓰기 · 백업 · 저장 도중 정지 흉내"
cd $N
echo -n "8a 첫 저장: "; run N s8a kitan --sec 9 --save 6 --state "$(w $N/s8.g98s)"; echo "  파일: $(ls $N | grep '^s8\.' | tr '\n' ' ')"; S1=$(sha256sum s8.g98s | cut -c1-16)
echo -n "8b 덮어쓰기(읽을 수 있던 파일→.bak): "; run N s8b kitan --sec 9 --save 6 --state "$(w $N/s8.g98s)"; echo "  파일: $(ls $N | grep '^s8\.' | tr '\n' ' ') | .bak==첫 저장: $([ "$(sha256sum s8.g98s.bak 2>/dev/null | cut -c1-16)" = "$S1" ] && echo 예 || echo 아니오) | 새 저장 CRC 유효: $(echo -n)"
echo -n "8c 못 읽는 파일 덮어쓰기(→.unreadable): "; head -c 5000 s2.g98s > s8c.g98s; J=$(sha256sum s8c.g98s | cut -c1-16); run N s8c kitan --sec 9 --save 6 --state "$(w $N/s8c.g98s)"; echo "  파일: $(ls $N | grep '^s8c\.' | tr '\n' ' ') | .unreadable==원래 조각: $([ "$(sha256sum s8c.g98s.unreadable 2>/dev/null | cut -c1-16)" = "$J" ] && echo 예 || echo 아니오)"
echo -n "8d 또 못 읽는 파일(→.unreadable2, .bak 안 건드림): "; head -c 3000 s2.g98s > s8c.g98s; run N s8d kitan --sec 9 --save 6 --state "$(w $N/s8c.g98s)"; echo "  파일: $(ls $N | grep '^s8c\.' | tr '\n' ' ')"
cp s2.g98s s8e.g98s; E=$(sha256sum s8e.g98s | cut -c1-16)
export WSLENV=GENSEI_SS_TEST_STOP; export GENSEI_SS_TEST_STOP=tmp
echo -n "8e 임시 파일만 쓰고 정지(원본 s8e 유지?): "; run N s8e kitan --sec 9 --save 6 --state "$(w $N/s8e.g98s)"; echo "  파일: $(ls $N | grep '^s8e\.' | tr '\n' ' ') | 원본 불변: $([ "$(sha256sum s8e.g98s | cut -c1-16)" = "$E" ] && echo 예 || echo 아니오)"
export GENSEI_SS_TEST_STOP=bak
echo -n "8f 백업까지 하고 정지(원본 s8e 유지?): "; run N s8f kitan --sec 9 --save 6 --state "$(w $N/s8e.g98s)"; echo "  파일: $(ls $N | grep '^s8e\.' | tr '\n' ' ') | 원본 불변: $([ "$(sha256sum s8e.g98s | cut -c1-16)" = "$E" ] && echo 예 || echo 아니오)"
unset GENSEI_SS_TEST_STOP
echo -n "8g 남은 .tmp 가 있어도 정상 저장: "; run N s8g kitan --sec 9 --save 6 --state "$(w $N/s8e.g98s)"; echo "  파일: $(ls $N | grep '^s8e\.' | tr '\n' ' ') | 새 파일 != 옛 파일: $([ "$(sha256sum s8e.g98s | cut -c1-16)" != "$E" ] && echo 예 || echo 아니오)"
echo -n "8h 저장한 파일을 다시 불러오기: "; run N s8h kitan --sec 9 --load 3 --state "$(w $N/s8e.g98s)"
echo "== S9 라이브 saves 불변 확인"; for g in daimadou95 doukyuusei2 kitan; do echo "live $g: $(sha256sum $LV/saves/$g/slot01.g98s 2>/dev/null | cut -c1-16) (백업 $(sha256sum $BK/$g/slot01.g98s | cut -c1-16))"; done
echo "T=$T"; echo "== 끝 $(date -Is)"
