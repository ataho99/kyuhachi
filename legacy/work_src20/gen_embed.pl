#!/usr/bin/perl
# gen_embed.pl <출력.h> <sbcs.fnt> <db.fnt> <sharp.glsl> <crt.glsl>  -- 글꼴·셰이더를 exe 안에 넣는 헤더 생성 (#246-3)
use strict; use warnings;
my ($out, @in) = @ARGV; die "usage\n" unless @in == 4;
my @nm = qw(emb_font_sb emb_font_db emb_sh_sharp emb_sh_crt);
open(my $o, '>:raw', $out) or die "$out: $!";
print $o "// 자동 생성 (gen_embed.pl) -- 편집 금지\n#pragma once\n#include <stddef.h>\n";
for my $i (0..3) {
  open(my $f, '<:raw', $in[$i]) or die "$in[$i]: $!"; local $/; my $d = <$f>; close $f;
  print $o "static const unsigned char $nm[$i]\[] = {\n"; my $n = length $d;
  for (my $p = 0; $p < $n; $p += 24) { print $o join(',', unpack('C*', substr($d, $p, 24))), ",\n"; }
  print $o "0};\nstatic const size_t $nm[$i]_n = $n;\n";
}
close $o; print "embed ok\n";
