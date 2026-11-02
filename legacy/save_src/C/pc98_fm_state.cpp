// pc98_fm_state.cpp — PC-98 sound save component "pc98_fm" (draft, eye-checked, not compiled)
// Design: Fable_PC98_세이브설계_1023.md §1.1 표, §2.13, §4.0, §4.1 C행, §4.2 2·3·4·9, §5 T1·T6, 부록.
// Sources (read-only S): snd_pc98/sound/{fmboard.h:15-26, opngen.h, psggen.h, fmtimer.h, rhythm.h,
//   sound.h:57-73, adpcm.h, pcm86.h}, fmboard.c:300-330, pc98_fm.cpp:176-199,203-239.

#include <iostream>
#include <cstdint>

#include "dosbox.h"
#include "fmboard.h"
#include "opngen.h"
#include "psggen.h"
#include "fmtimer.h"
#include "rhythm.h"
#include "adpcm.h"
#include "pcm86.h"
#include "tms3631.h"
#include "sound.h" // PMIXTRK/PMIXDAT layout for rhythm trk (sound.h:57-73)

// ---- S globals used here (all non-static externs via fmboard.h; lines are S declaration sites) ----
// extern UINT32 usesound;        // fmboard.h:49 — board type set by fmboard_reset (fmboard.c:122)
// extern OPN_T opn;              // fmboard.h:50 — addr/addr2/data/data2/base/mask/channels + reg[0x400]
// extern _OPNGEN opngen;         // fmboard.h:56 — playchannels/playing/feedback2-4/outdl/dc/dr (opngen.h:136-147)
// extern OPNCH opnch[12];        // fmboard.h:57 — OPNCH_MAX=12 (opngen.h:6); slot ptrs + connect1-4
// extern _PSGGEN __psg[3];       // fmboard.h:58 — tone pvol ptr per ch (psggen.h:40-52)
// extern _FMTIMER fmtimer;       // fmboard.h:55 — 8B timer state (fmtimer.h:2-10)
// extern _RHYTHM rhythm;         // fmboard.h:59 — hdr + trk[6] (rhythm.h:2-7; trk layout sound.h:67-73)
// extern _ADPCM adpcm;           // fmboard.h:60 — 256KB incl. buf[0x40000] (adpcm.h:27-50)
// extern _PCM86 pcm86;           // fmboard.h:61 — 64KB incl. buffer[0x10000] (pcm86.h:23-58)
// extern _TMS3631 tms3631;       // fmboard.h:54 — ch[8]+enable (tms3631.h:7-10)
// extern AMD98 amd98;            // fmboard.h:51 — port/psg3reg/rhythm (fmboard.h:28-32)
// extern MUSICGEN musicgen;      // fmboard.h:52 — porta/b/c/mask/key[8]/sync/ch (fmboard.h:34-43)
// Public restore/setreg APIs used: opngen_setreg (opngen.h:174), psggen_setreg (psggen.h:72),
//   rhythm_setreg (rhythm.h:22), fmboard_rhyrestore (fmboard.h:86), fmtimer_setreg (fmtimer.h:21).
// PIC timer events fmport_a/b_pic_event are already in pic_state_event_table[] (pc98_fm.cpp:187-188).
// Mixer path pc98_mix_CallBack (pc98_fm.cpp:203-239) + MixTemp are NOT saved (§4.2 9).
// T1 (BGM 이어짐) needs opn/opnch/fmtimer; T6 (리듬+ADPCM 이어짐) needs rhythm offsets + adpcm.buf.

// ---- board presence: pc98_fm.cpp:41 MixerChannel *pc98_mixer (global, non-static).
// pc98fm_init (pc98_fm.cpp:241) is static so it cannot be used here.
// board=="off" returns early without creating the mixer, so NULL means absent.
class MixerChannel;
extern MixerChannel *pc98_mixer;

// ---- lrclock accessors (deferred): static uint8_t lrclock in cbus/pcm86io.c:153.
// No getter exists in S; integration must add pcm86io.c.C with these two.
extern "C" uint8_t pcm86io_get_lrclock(void);
extern "C" void pcm86io_set_lrclock(uint8_t v);

// ---- FM pointer restore: keyon-excluded variant of fmboard_fmrestore.
// Uses only public opngen_setreg (opngen.h:174), so no .C copy is needed.
extern "C" void pc98_fm_restore_ptrs(void)
{
    const struct { uint8_t chbase; unsigned bank; } pairs[2] = { {0, 0}, {3, 1} };
    for (unsigned p = 0; p < 2; p++) {
        const uint8_t *reg = opn.reg + (pairs[p].bank * 0x100);
        for (int i = 0x30; i < 0xA0; i++)
            opngen_setreg(pairs[p].chbase, (unsigned)i, reg[i]);
        // 0xB4-0xB7 before 0xB0-0xB3: set_algorithm picks outd from pan/stereo.
        // Descending 0xB7->0xA0 matches fmboard.c:308-310 for this reason.
        for (int i = 0xB7; i >= 0xA0; i--)
            opngen_setreg(pairs[p].chbase, (unsigned)i, reg[i]);
    }
    // NOTE: no opngen_keyon here: keyon would re-trigger every channel from attack.
}

namespace {
class Pc98FmState : public SaveState::Component {
public:
    Pc98FmState() { SaveState::instance().registerComponent("pc98_fm", *this); }

private:
    void getBytes(std::ostream& stream) override
    {
        // Order mirrors setBytes below; presence flag first (§4.0: absent board = no-op).
        uint8_t present = (pc98_mixer != NULL) ? 1 : 0;
        stream.write((const char*)&present, 1);
        if (!present) return; // board absent (pc98_fm.cpp:406 off path): nothing to do
        // §2.13 row 1: board type + OPN base. usesound decides bank count at restore.
        WRITE_POD(&usesound, usesound); // board type (fmboard.c:122)
        WRITE_POD(&opn, opn);           // OPN_T reg[0x400] is the restore source
        // §2.13 rows 2-3: opngen value block, then opnch raw (ptrs fixed on load).
        WRITE_POD(&opngen, opngen);     // value-only; feedback/outd are targets
        // opnch holds 24x12 table ptrs + connect 4x12: raw here, fixed by restore_ptrs on load
        WRITE_POD(&opnch, opnch);
        // §2.13 row 4 + §4.2 3: PSG raw; pvol ptrs fixed by regs 8-10 on load (never restore()).
        WRITE_POD(&__psg, __psg);       // pvol ptrs fixed by regs 8-10 on load
        // §2.13 row 5: fmtimer raw; nevent flags re-armed by reg 0x27 on load (§4.2 2nd half).
        WRITE_POD(&fmtimer, fmtimer);   // nevent flags re-armed by reg 0x27 on load
        // §2.13 row 6 + §4.2 4: rhythm raw + per-track WAV offsets.
        WRITE_POD(&rhythm, rhythm);     // trk[i].pcm fixed by offsets below
        for (int i = 0; i < 6; i++) {   // rhythm WAV-buffer offsets (-1 if none)
            int32_t off = -1;           // NULL-checked; sample==NULL when WAV not loaded
            if (rhythm.trk[i].pcm != NULL && rhythm.trk[i].data.sample != NULL)
                off = (int32_t)(rhythm.trk[i].pcm - rhythm.trk[i].data.sample);
            stream.write((const char*)&off, sizeof(off));
        }
        // §2.13 rows 7-8: ADPCM 256KB + PCM86 64KB raw (§4.2 5). lrclock is 1B extra.
        WRITE_POD(&adpcm, adpcm);       // 256KB incl. buf[0x40000]
        WRITE_POD(&pcm86, pcm86);       // 64KB incl. buffer[0x10000]
        { uint8_t lr = pcm86io_get_lrclock(); stream.write((const char*)&lr, 1); }
        // §2.13 row 9: tms3631/amd98/musicgen exist (fmboard_reset zeroes them) so raw.
        WRITE_POD(&tms3631, tms3631);
        WRITE_POD(&amd98, amd98);
        WRITE_POD(&musicgen, musicgen);
        // Mixer buffers/channel positions not saved (§4.2 9): few ms silence after load is allowed.
    }

    void setBytes(std::istream& stream) override
    {
        // Read order must match getBytes exactly (SaveState has no per-field tags).
        uint8_t present = 0;
        stream.read((char*)&present, 1);
        if (!present) return;
        READ_POD(&usesound, usesound);
        READ_POD(&opn, opn);
        READ_POD(&opngen, opngen);
        READ_POD(&opnch, opnch);
        READ_POD(&__psg, __psg);
        READ_POD(&fmtimer, fmtimer);
        READ_POD(&rhythm, rhythm);
        int32_t off[6];
        for (int i = 0; i < 6; i++)
            stream.read((char*)&off[i], sizeof(off[i]));
        READ_POD(&adpcm, adpcm);
        READ_POD(&pcm86, pcm86);
        { uint8_t lr = 0; stream.read((char*)&lr, 1); pcm86io_set_lrclock(lr); }
        READ_POD(&tms3631, tms3631);
        READ_POD(&amd98, amd98);
        READ_POD(&musicgen, musicgen);
        // Post-load (§4.1 C행 + 부록): raw first, then pointer/flag reconstruction.
        // Order: FM ptrs -> PSG pvol -> rhythm ptrs+levels -> fmtimer re-arm.
        pc98_fm_restore_ptrs(); // keyon excluded; B4-B7 before B0-B3 (see above)
        for (int i = 0; i < 3; i++) // PSG pvol only; reg13 would reset envelope (psggenc.c:159)
            for (unsigned r = 8; r <= 10; r++)
                psggen_setreg(&__psg[i], r, ((const uint8_t*)&__psg[i].reg)[r]);
        for (int i = 0; i < 6; i++) { // rhythm sample ptrs; NULL-checked (§4.2 4)
            if (off[i] < 0 || rhythm.trk[i].data.sample == NULL) {
                rhythm.trk[i].pcm = NULL; rhythm.trk[i].remain = 0;
            } else {
                rhythm.trk[i].pcm = rhythm.trk[i].data.sample + off[i];
                // remain was saved raw above; keep it (rhythm_setreg would restart it).
            }
        }
        fmboard_rhyrestore(&rhythm, 0); // level/pan only, no keyon (fmboard.c:317-330)
        fmtimer_setreg(0x27, fmtimer.reg); // re-arms nevent_is_set via public setter path
    }
};

static Pc98FmState pc98FmStateInstance;

} // namespace
