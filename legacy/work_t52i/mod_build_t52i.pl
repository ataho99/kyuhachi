#!/usr/bin/perl
# dbx_host_t52h.sh → t52i 빌드 스크립트: 이름 t52h→t52i(kbov 헤더 이름은 그대로 t52f), README 조작 절 갱신.
use strict; use warnings; my ($in, $out) = @ARGV; local $/; open(my $h, "<:raw", $in) or die; my $s = <$h>; close $h;
sub rep { my ($sr, $a, $b) = @_; my $n = () = $$sr =~ /\Q$a\E/g; die "anchor count $n for: $a" if $n != 1; my $i = index($$sr, $a); substr($$sr, $i, length($a)) = $b; }
my $n = () = $s =~ /t52h/g; die "no t52h" if !$n; $s =~ s/t52h/t52i/g;
rep(\$s, q~=== KEYS / 조작 ===\r\nF12: menu / 메뉴    F11 (hold): fast-forward / 누르는 동안 배속\r\nScrollLock or Alt+F11: fast-forward on/off / 배속 켜기·끄기    Alt+Enter: fullscreen / 전체화면\r\n~,
 q~=== KEYS / 조작 ===\r\nF12: menu / 메뉴    F11 (hold): fast-forward / 누르는 동안 배속\r\nScrollLock or Alt+F11: fast-forward on/off / 배속 켜기·끄기    Alt+Enter: fullscreen / 전체화면\r\nPad: R2 (hold) fast-forward, L3 on/off, square = fast text (CAPS) / 패드: R2 누르는 동안 배속, L3 켜고 끄기, □ 빨리 넘김\r\nGame list: OK (circle) starts the game (a save asks resume / from start / system disk only). Screen settings has a Fullscreen row. Sound/pad/boost settings are saved as soon as you change them; X just goes back. / 작품 목록: ○ 로 시작(저장이 있으면 이어하기·처음부터·시스템 디스크만 선택). 화면 설정에 「전체화면」 줄. 동작 환경(소리·패드·배속) 값은 바꾸는 즉시 저장되고 × 는 뒤로 가기입니다. 메뉴 → 조작 안내에 전체 키 표.\r\n~);
open(my $o, ">:raw", $out) or die; print $o $s; close $o; print "mod ok ($n)\n";
