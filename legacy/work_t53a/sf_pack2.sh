#!/bin/bash
# t53a 포장: 시험한 exe 는 그대로 두고, t52i2 에서 들어간 리듬 폴더 안내(res/rhythm/PUT_RHYTHM_HERE.txt + README 줄)를 같은 내용으로 얹어 release 규칙을 맞춘다.
P=/mnt/h/RETRO_SET/PC98_PLAYER; W=$P/build/r020/work/t53a; OUT=$P/build/r020/out_t53a; exec > $W/pack_out2.txt 2>&1
Z0=$OUT/Kyuhachi-v0.5.0-t53a.zip; ZR=$P/release/Kyuhachi-v0.5.0-t52i2.zip; DST=$P/release/Kyuhachi-v0.5.0-t53a.zip
echo "== pack start $(date -Is)"; [ -e "$DST" ] && { echo "release 에 이미 있음 - 중단"; exit 0; }
D=$OUT/pack_$(date +%H%M%S); mkdir -p "$D"; cd "$D"; unzip -q "$Z0"; [ -s Kyuhachi/Kyuhachi.exe ] || { echo "unzip 실패(exe 없음)"; exit 1; }
mkdir -p Kyuhachi/res/rhythm; unzip -p "$ZR" Kyuhachi/res/rhythm/PUT_RHYTHM_HERE.txt > Kyuhachi/res/rhythm/PUT_RHYTHM_HERE.txt
unzip -p "$ZR" Kyuhachi/README.txt > "$D/ref_README.txt"
perl -e 'local $/; open R,"<:raw",$ARGV[0]; $r=<R>; close R; open O,"<:raw",$ARGV[1]; $o=<O>; close O; @r=split(/(?<=\n)/,$r); @o=split(/(?<=\n)/,$o);
 ($i)=grep{$r[$_]=~/^  res\/rhythm\//}0..$#r; die "ref rhythm line 없음" unless defined $i; $p1=$r[$i-1]; ($j)=grep{$o[$_] eq $p1}0..$#o; die "ours prev line 없음: $p1" unless defined $j; splice(@o,$j+1,0,$r[$i]);
 ($s)=grep{$r[$_]=~/^=== RHYTHM SOUND/}0..$#r; die "ref section 없음" unless defined $s; $b=$s-1; $e=$s; $e++ while $e+1<=$#r && $r[$e+1]!~/^===/; @blk=@r[$b..$e]; $p2=$r[$b-1]; ($k)=grep{$o[$_] eq $p2}0..$#o; die "ours section anchor 없음: $p2" unless defined $k; splice(@o,$k+1,0,@blk);
 open W,">:raw",$ARGV[1]; print W join("",@o); close W; print "README 줄 삽입: layout 1줄 + 섹션 ".scalar(@blk)."줄\n";' "$D/ref_README.txt" Kyuhachi/README.txt || { echo "README 패치 실패 - 중단"; exit 1; }
echo "-- 새 README vs t52i2 README 차이(빌드 줄·버전만 나와야 함)"; diff <(tr -d '\r' < Kyuhachi/README.txt) <(tr -d '\r' < "$D/ref_README.txt") | cut -c1-160
rm -f "$D/ref_README.txt"
if command -v zip >/dev/null 2>&1; then zip -qr "$D/pack.zip" Kyuhachi; else python3 -m zipfile -c "$D/pack.zip" Kyuhachi; fi
cp "$D/pack.zip" "$DST" && echo "RELEASE_ZIP: $DST sha256=$(sha256sum "$DST" | cut -c1-64) size=$(stat -c %s "$DST")"
echo "zip 안 exe sha16: $(unzip -p "$DST" Kyuhachi/Kyuhachi.exe | sha256sum | cut -c1-16) (시험한 exe: dd0465d0f7a135a5)"; unzip -Z1 "$DST" | sort
echo "== pack done $(date -Is)"
