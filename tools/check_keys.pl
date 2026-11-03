#!/usr/bin/perl
# T11/T4: 문구 키 대조. 사용: perl check_keys.pl <strings_ko.txt> <strings_en.txt> <strings_ja.txt> [소스 폴더...]
# 출력(한 줄): keys ko=N en=N ja=N missing_en=N missing_ja=N extra_en=N extra_ja=N empty=N src_unknown=N  — 합격 = 개수 일치 · 나머지 0 (src_unknown 은 정보: 소스의 T("…") 가 ko 에 없는 것)
use strict; use warnings; use open qw(:std :utf8);
my ($fk,$fe,$fj,@src)=@ARGV; die "사용법: check_keys.pl ko en ja [src...]\n" unless $fj;
sub rd { my $f=shift; open(my $h,'<',$f) or die "열기 실패 $f\n"; my %m; my $empty=0;
  while(<$h>){ chomp; s/\r$//; next if /^\s*#/ || !/=/; my($k,$v)=split(/=/,$_,2); $m{$k}=$v; $empty++ if $v eq ''; } return (\%m,$empty); }
my($k,$ek)=rd($fk); my($e,$ee)=rd($fe); my($j,$ej)=rd($fj);
my $me=grep{!exists $e->{$_}}keys %$k; my $mj=grep{!exists $j->{$_}}keys %$k;
my $xe=grep{!exists $k->{$_}}keys %$e; my $xj=grep{!exists $k->{$_}}keys %$j;
my $unk=0; my %seen;
for my $d (@src){ for my $f (glob("$d/*.h $d/*.cpp $d/*/*.h $d/*/*.cpp")){ next if $f=~/embed/; open(my $h,'<:encoding(UTF-8)',$f) or next; local $/; my $t=<$h>;
  while($t=~/\bT\(\s*"((?:[^"\\]|\\.)*)"/g){ my $s=$1; $s=~s/\\n/\n/g; $s=~s/\\"/"/g; $s=~s/\\\\/\\/g; next if $seen{$s}++; $unk++ unless exists $k->{$s}; } } }
printf "keys ko=%d en=%d ja=%d missing_en=%d missing_ja=%d extra_en=%d extra_ja=%d empty=%d src_unknown=%d\n",scalar(keys %$k),scalar(keys %$e),scalar(keys %$j),$me,$mj,$xe,$xj,$ek+$ee+$ej,$unk;
exit(($me||$mj||$xe||$xj||scalar(keys %$k)!=scalar(keys %$e)||scalar(keys %$k)!=scalar(keys %$j))?1:0);
