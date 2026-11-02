#!/usr/bin/perl
# r011 패치(믹서 장치·계수) + r017 배속 소리. 사용: perl patch_mixer.pl mixer_P.cpp gensei_boostmix.inc "C:/dbx_build/r017/gensei_boostaud.h"
use strict; local $/; my ($f, $incf, $hdr) = @ARGV;
open F, "<", $f or die; binmode F; my $s = <F>; close F; open I, "<", $incf or die; binmode I; my $inc = <I>; close I;
my %c;
$c{cb}   = ($s =~ s/(static void SDLCALL MIXER_CallBack\(void \* userdata, Uint8 \*stream, int len\) \{)/unsigned gensei_mix_cb=0, gensei_mix_under=0, gensei_mix_drop=0; int gensei_audio_obt_freq=0, gensei_audio_obt_samples=0; const char * gensei_audio_dev=NULL; static bool gensei_mix_played=false;\nint gensei_ba_mode=0, gensei_ba_xq=100, gensei_ba_gain=256; unsigned gensei_ba_calls=0, gensei_ba_under=0, gensei_ba_drops=0; double gensei_ba_rho=0, gensei_ba_sumsq=0; unsigned long long gensei_ba_n=0; double gensei_out_sumsq=0; unsigned long long gensei_out_n=0;\n$1\n    gensei_mix_cb++;/);
$c{played} = ($s =~ s/(mixer\.last_dac\[0\] = \(output-2\)\[0\];)/$1\n            gensei_mix_played = true;\n            { const int16_t * q_ = (const int16_t *)stream; for (; q_ < output; q_++) gensei_out_sumsq += (double)*q_ * (double)*q_; gensei_out_n += (unsigned long long)(output - (int16_t *)stream); }/);
$c{under}  = ($s =~ s/    if \(need > 0\)\r?\n        mixer\.prebuffer_wait = true;/    if (need > 0) {\n        if (!mixer.prebuffer_wait && !mixer.mute && gensei_mix_played) gensei_mix_under++;\n        mixer.prebuffer_wait = true;\n    }/);
$c{drop}   = ($s =~ s/(        unsigned int drop;\r?\n)/$1        gensei_mix_drop++;\n/);
$c{dev}    = ($s =~ s/SDL_OpenAudioDevice\(NULL, 0, &spec/SDL_OpenAudioDevice(gensei_audio_dev, 0, &spec/);
$c{obt}    = ($s =~ s/(        mixer\.blocksize=obtained\.samples;)/$1\n        gensei_audio_obt_freq=obtained.freq; gensei_audio_obt_samples=obtained.samples;/);
# r017: 배속 소리 블록은 mute 처리 다음, prebuffer 처리 앞
$c{ba}     = ($s =~ s/(\r?\n    if \(mixer\.prebuffer_wait\) \{\r?\n        remains = )/\n$inc$1/);
$c{hdr}    = ($s =~ s/\A/#include "$hdr"\n/);
print "mixer patch counts: ", join(" ", map { "$_=$c{$_}" } sort keys %c), " (기대 전부 1)\n";
open O, ">", $f or die; binmode O; print O $s; close O;
