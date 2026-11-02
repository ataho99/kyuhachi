#!/usr/bin/perl
# T1 화소 비교: t1_compare.pl <골든폴더> <새폴더>  — PNG(8bit RGB, 비인터레이스) y=16..399 영역(윗줄 16px 가림) 화소 비교. 외부 도구 없음(Compress::Zlib 만).
use strict; use warnings; use Compress::Zlib;
my ($G,$N)=@ARGV; my $Y0=16;
sub rd { my $f=shift; open my $h,'<:raw',$f or return; local $/; my $d=<$h>; close $h; return $d; }
sub dec { my $d=shift; return unless substr($d,0,8) eq "\x89PNG\r\n\x1a\n"; my ($p,$w,$ht,$bd,$ct,$il)=(8); my $z='';
  while($p<length $d){ my $l=unpack('N',substr($d,$p,4)); my $t=substr($d,$p+4,4); my $c=substr($d,$p+8,$l);
    if($t eq 'IHDR'){ ($w,$ht,$bd,$ct,undef,undef,$il)=unpack('NNCCCCC',$c);} elsif($t eq 'IDAT'){$z.=$c;} elsif($t eq 'IEND'){last;} $p+=12+$l; }
  return unless $bd==8 && $il==0 && ($ct==2||$ct==6); my $bpp=$ct==2?3:4; my $raw=uncompress($z); return unless defined $raw;
  my $st=$w*$bpp; my @rows; my $prev="\0" x $st; my $o=0;
  for my $y (0..$ht-1){ my $f=ord(substr($raw,$o++,1)); my @c=unpack('C*',substr($raw,$o,$st)); $o+=$st; my @p=unpack('C*',$prev); my @r=@c;
    for my $i (0..$st-1){ my $a=$i>=$bpp?$r[$i-$bpp]:0; my $b=$p[$i]; my $cc=$i>=$bpp?$p[$i-$bpp]:0; my $x=$c[$i];
      if($f==1){$x+=$a}elsif($f==2){$x+=$b}elsif($f==3){$x+=int(($a+$b)/2)}elsif($f==4){my $pa=abs($b-$cc);my $pb=abs($a-$cc);my $pc=abs($a+$b-2*$cc);my $pr=($pa<=$pb&&$pa<=$pc)?$a:($pb<=$pc?$b:$cc);$x+=$pr}
      $r[$i]=$x&255; }
    $prev=pack('C*',@r); push @rows,$prev; }
  return ($w,$ht,\@rows); }
my ($bad,$n)=(0,0);
opendir my $dh,$G or die; my @f=sort grep{/\.png$/}readdir $dh;
for my $b (@f){ my $a=rd("$G/$b"); my $c=rd("$N/$b"); if(!defined $c){print "MISSING $b\n";$bad=1;next;} $n++;
  my ($w1,$h1,$r1)=dec($a); my ($w2,$h2,$r2)=dec($c); if(!$r1||!$r2){print "DECODE-FAIL $b\n";$bad=1;next;}
  if($w1!=$w2||$h1!=$h2){print "SIZE $b ${w1}x$h1 vs ${w2}x$h2\n";$bad=1;next;}
  my $d=0; for my $y ($Y0..$h1-1){ $d++ if $r1->[$y] ne $r2->[$y]; } if($d){print "DIFF $b rows=$d\n";$bad=1;} }
print "compared=$n bad=$bad\n"; exit $bad;
