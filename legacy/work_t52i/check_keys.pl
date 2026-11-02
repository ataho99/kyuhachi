#!/usr/bin/perl
# 소스의 phr_("…") 문구가 strings_ko.txt 의 키에 모두 있는지 / 파일의 키가 소스에 모두 있는지 본다.  usage: perl check_keys.pl strings_ko.txt src...
use strict; use warnings; my ($kf, @src) = @ARGV; local $/;
sub slurp { my $f = shift; open(my $h, "<:raw", $f) or die "open $f"; my $s = <$h>; close $h; $s }
my %src; for my $f (@src) { my $t = slurp($f); while ($t =~ /phr_\("((?:[^"\\]|\\.)*)"\)/g) { my $l = $1;
  $l =~ s/\\x([0-9A-Fa-f]{2})/chr(hex($1))/ge; $l =~ s/\\t/\t/g; $l =~ s/\\"/"/g;
  (my $k = $l) =~ s/=/\\=/g; $src{$k} = 1; } }
my $kt = slurp($kf); $kt =~ s/^\xEF\xBB\xBF//; my %key; for my $ln (split /\n/, $kt) { $ln =~ s/\r$//; next if $ln =~ /^#/ || $ln eq ''; my ($k) = $ln =~ /^((?:[^=\\]|\\.)*)=/s; $key{$k} = 1 if defined $k; }
my @miss = grep { !$key{$_} } sort keys %src; my @extra = grep { !$src{$_} } sort keys %key;
printf "소스 문구 %d · 파일 키 %d · 소스에만 있음(번역 없음) %d · 파일에만 있음(안 쓰임) %d\n", scalar(keys %src), scalar(keys %key), scalar(@miss), scalar(@extra);
print "NOKEY: $_\n" for @miss; print "UNUSED: $_\n" for @extra;
