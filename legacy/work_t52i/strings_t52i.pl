#!/usr/bin/perl
# t52i: strings_{ko,en,ja}.txt 갱신 — usage: perl strings_t52i.pl <in_dir> <out_dir>
use strict; use warnings; use utf8; binmode(STDOUT, ":utf8");
my ($in, $out) = @ARGV;
my @del = ("        △:이 메뉴  START:Enter  십자키/왼스틱:방향키", "        R2 누르는 동안:배속  L3:배속 켜고 끄기", "키보드  F12:메뉴  F11 누름:배속  ScrollLock/Alt+F11:배속 토글", "        CAPS:빨리 넘김  그 밖의 키는 PC-98 키로 그대로 전달", "        Alt+위/아래:곡면 세기  (바꿀 때마다 화면 구석에 표시)", "↑↓:선택, ←→:값, ○:결정, ×:취소, △:출하 시 설정");
my @add = (
 ["        △:메뉴(F12)  START:Enter  십자키/왼스틱:방향키", "        △:Menu (F12)  START:Enter  D-pad/left stick:Arrows", "        △:メニュー(F12)  START:Enter  十字キー/左スティック:矢印キー"],
 ["배속    입력 → 동작", "Boost   Input -> Action", "倍速    入力 → 動作"],
 ["        누르는 동안   R2 / F11", "        While held    R2 / F11", "        押している間  R2 / F11"],
 ["        켜고 끄기     L3 / ScrollLock / Alt+F11", "        On / off      L3 / ScrollLock / Alt+F11", "        切り替え      L3 / ScrollLock / Alt+F11"],
 ["        빨리 넘김     □ / CAPS  (글 넘기기)", "        Fast text     □ / CAPS", "        文送り        □ / CAPS"],
 ["        Alt+위/아래:곡면 세기  Alt+[ ]:창 배율", "        Alt+Up/Down:Curve  Alt+[ ]:Window size", "        Alt+上/下:湾曲  Alt+[ ]:ウィンドウ倍率"],
 ["키보드  그 밖의 키는 PC-98 키로 그대로 전달", "Keys    Other keys go to the PC-98 as they are", "キー    その他のキーはPC-98にそのまま渡す"],
 ["전체화면: 창과 전체화면을 오갑니다. Alt+Enter 로도 됩니다.", "Fullscreen: switch between window and fullscreen. Alt+Enter works too.", "全画面: ウィンドウと全画面を切り替えます。Alt+Enterでも切り替えられます。"],
 ["↑↓:선택, ←→:값, ○:결정, ×:뒤로, △:출하 시 설정", "↑↓:Select, ←→:Value, ○:OK, ×:Back, △:Factory", "↑↓:選択 ←→:値 ○:決定 ×:戻る △:初期値"],
);
my %dk = map { $_ => 1 } @del;
my $li = 0;
for my $L (qw(ko en ja)) {
  my $col = $L eq 'ko' ? 0 : $L eq 'en' ? 1 : 2;
  open(my $h, "<:raw:encoding(UTF-8)", "$in/strings_$L.txt") or die; my @ln = <$h>; close $h;
  my ($removed, @keep) = (0);
  for my $l (@ln) { my $t = $l; $t =~ s/\r?\n$//; my ($k) = split(/=/, $t, 2); if (exists $dk{$k}) { $removed++; next } push @keep, $l; }
  die "$L removed $removed != 6" if $removed != 6;
  push @keep, "\n" if @keep && $keep[-1] !~ /\n$/;
  for my $r (@add) { push @keep, $r->[0] . "=" . $r->[$col] . "\n"; }
  open(my $o, ">:raw:encoding(UTF-8)", "$out/strings_$L.txt") or die; print $o @keep; close $o;
  print "$L ok removed=$removed added=" . scalar(@add) . "\n";
}
