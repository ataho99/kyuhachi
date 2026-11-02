#!/usr/bin/perl
# stubgen_d: 링크 출력(utf8 변환본)에서 미해결 심볼을 뽑아 (a) 호출 카운터가 달린 함수 스텁 gensei_stub_<i> + ALTERNATENAME 목록
# (b) 0 초기화 데이터 버퍼 (c) 스텁 이름표 names.cpp 를 만든다.  usage: stubgen_d.pl <link_out_utf8> <out_prefix> [prev_lst]
use strict; use warnings;
my ($lo,$pre,$prevlist)=@ARGV;
my (%fn,%dat,%skip);
open(my $l,"<",$lo) or die "open $lo";
while(<$l>){
  next unless /LNK2001|LNK2019/;
  my $line=$_; my $m;
  if($line=~/LNK2001: .*?기호 (.+)$/){ my $t=$1; $t=~s/\s+$//; if($t=~/\((\?[^\s()]+)\)\s*$/){$m=$1}else{($m)=$t=~/^"?(\S+?)"?$/} }
  elsif($line=~/LNK2019: (.+?) 함수에서/){ my $t=$1; if($t=~/^"[^"]*" \((\?[^\s()]+)\)/){$m=$1}elsif($t=~/^(\S+)/){$m=$1} }
  next unless defined $m; $m=~s{".*}{}s; next unless length $m;
  next if $m=~/^gensei_/;
  if($m=~/^__imp_/ || $m=~/^\?\?_[0-9]/ && $m!~/^\?\?_7/){ $skip{$m}=1; next }
  if($m=~/@@[0-9]/ || $m=~/^\?\?_7/){ $dat{$m}=1 } else { $fn{$m}=1 }
}
close $l;
if($prevlist && -f $prevlist){ open(my $p,"<",$prevlist); while(<$p>){chomp; my($k,$s)=split /\t/; next unless defined $s; if($k eq 'F'){$fn{$s}=1}elsif($k eq 'D'){$dat{$s}=1}} close $p }
my @f = grep { !$dat{$_} } sort keys %fn;
my $n = scalar @f;
open(my $a,">","$pre.asm") or die;
print $a "OPTION CASEMAP:NONE\n_TEXT SEGMENT\n";
for my $i (0..$n-1){ print $a "PUBLIC gensei_stub_$i\ngensei_stub_$i PROC\n lea rax,gensei_stub_counts\n lock inc DWORD PTR [rax+".(4*$i)."]\n xor eax,eax\n ret\ngensei_stub_$i ENDP\n"; }
print $a "PUBLIC gensei_stub0\ngensei_stub0 PROC\n xor eax,eax\n ret\ngensei_stub0 ENDP\n_TEXT ENDS\n";
print $a "_DATA SEGMENT ALIGN(16)\nPUBLIC gensei_blob\ngensei_blob BYTE 65536 DUP(0)\nPUBLIC gensei_stub_counts\ngensei_stub_counts DWORD ".($n+1)." DUP(0)\nPUBLIC gensei_stub_n\ngensei_stub_n DWORD $n\n";
open(my $al,">","$pre.altrsp") or die;
for my $i (0..$n-1){ print $al "/ALTERNATENAME:$f[$i]=gensei_stub_$i\n"; }
for my $s (sort keys %dat){ if(length($s)>120){ print $al "/ALTERNATENAME:$s=gensei_blob\n"; next } print $a "PUBLIC $s\n$s BYTE 4096 DUP(0)\n"; }
close $al;
print $a "_DATA ENDS\nEND\n"; close $a;
open(my $c,">","${pre}_names.cpp") or die;
print $c "extern \"C\" const char * const gensei_stub_names[] = {\n";
for my $i (0..$n-1){ my $s=$f[$i]; $s=~s/\\/\\\\/g; $s=~s/"/\\"/g; print $c "\"$s\",\n"; }
print $c "0};\n"; close $c;
open(my $r,">","$pre.lst") or die;
print $r "F\t$_\n" for @f; print $r "D\t$_\n" for sort keys %dat; print $r "S\t$_\n" for sort keys %skip; close $r;
print "funcs=$n data=",scalar(keys %dat)," skipped=",scalar(keys %skip),"\n";
