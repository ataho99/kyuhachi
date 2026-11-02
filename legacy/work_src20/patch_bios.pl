#!/usr/bin/perl
# r017 1번: DOSBox-X BIOS 스플래시(텍스트·로고·1초 대기) 건너뛰기. 사용: perl patch_bios.pl bios_B.cpp
use strict; local $/; my $f = shift; open F, "<", $f or die; binmode F; my $s = <F>; close F;
my $n1 = ($s =~ s{^(#include "dosbox\.h"\r?\n)}{${1}int gensei_nosplash = 1;   // r017: 1 = BIOS 스플래시 생략 (호스트 --splash 가 0 으로)\n}m);
my $inj = <<'INJ';

        if (gensei_nosplash && machine == MCH_PC98) {   // r017: 스플래시 없이 끝 장면의 상태(텍스트 지움·그래픽 숨김)만 만든다
            reg_eax = 0x0C00; CALLBACK_RunRealInt(0x18);    // 텍스트 층 켜기
            reg_eax = 0x1100; CALLBACK_RunRealInt(0x18);    // 커서 표시
            reg_eax = 0x1300; reg_edx = 0x0000; CALLBACK_RunRealInt(0x18);
            reg_eax = 0x4100; CALLBACK_RunRealInt(0x18);    // 그래픽 층 숨김
            for (unsigned int i=0;i < (80*400);i++) {
                mem_writeb(0xA8000+i,0); mem_writeb(0xB0000+i,0); mem_writeb(0xB8000+i,0); mem_writeb(0xE0000+i,0);
            }
            IO_Write(0x6A,0x00);                            // 8색 모드
            reg_eax = 0x4200; reg_ecx = 0x8000; CALLBACK_RunRealInt(0x18);
            reg_eax = 0x1600; reg_edx = 0xE100; CALLBACK_RunRealInt(0x18);   // 텍스트 VRAM 지움
            setupStopClock = setupStopClock_PC98; setupGetDateTime = setupGetDateTime_PC98;
            setupSetDateTime = setupSetDateTime_PC98; setupPollClockUpdated = setupPollClockUpdated_PC98;
            return CBRET_NONE;
        }
INJ
my $n2 = ($s =~ s/(\r?\n\ttextsplash = true;\r?\n)/$1$inj/);
print "bios patch: nosplash_decl=$n1 inject=$n2 (기대 1,1)\n";
open O, ">", $f or die; binmode O; print O $s; close O;
