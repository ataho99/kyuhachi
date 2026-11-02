// pc98_sys_state.cpp — PC-98 system save component "pc98_sys" (draft, eye-checked, not compiled)
// Design: Fable_PC98_세이브설계_1023.md §1.1 표, §2.8~§2.12, §2.14, §3 표(Mouse·Pic 로드 꼬리 행),
//         §4.0, §4.1 B행, §4.2 11·12번, 부록.
// Sources (read-only S): keyboard.cpp, dma.cpp, bios.cpp, vga_memory.cpp, ints/pc98_lio.cpp, ide.cpp.
//
// Save order (getBytes/setBytes identical; sizes x86-64 est., see B_inventory.txt):
//   #  bytes  content
//   1  ~70B   8251 UART state (data/txdata/mode/cmd flags/busy flags/delays/recv queue)
//   2  3B     caps/kana/num
//   3  ~20B   SYS 8255 value members (latches/mode/masks/IBF/OBF/INTR/INTE)
//   4  ~20B   PRN 8255 value members (same; inPortB is computed, not stored)
//   5  2B     SHUT0/SHUT1
//   6  15B    mouse 8255 blob (existing PC98_Mouse_* format: 11 regs + rate)
//   7  8B     MSW[8]
//   8  1B     port 29h (+ derived DMA channel wraparound via setter)
//   9  4B     pc98_ide_select
//   10 32B    INT 1Bh progress (4 arrays x 2 drives)
//   11 ~36B   LIO work + draw values + palette mode (paint pointer excluded)
// Total ≈ 210B + mouse blob. Pointers never stored: 8255 name strings,
// LIO paint_work, IDE/FDC controller objects are all excluded by design.
//
// Load-order note (§1.1): "pc98_sys" is lowercase, so std::map ordering loads
// it after "Pic" (and after "Mouse"/"Vga"). The PIC queue is therefore already
// restored when setBytes runs; Rearm/RestoreTick drop any duplicated UART/mouse
// tick entries first (PIC_RemoveEvents) and schedule fresh ones from flags.
// UART/FDC/IDE PIC table entries themselves are E-run scope (E_events.txt);
// the three UART handlers are verified non-static (keyboard.cpp:2387-2401).

#include <iostream>
#include <cstdint>

#include "dosbox.h"

// ---- externs: non-static definitions verified in S, no .cpp.B copy needed ----
extern bool PC98_SHUT0, PC98_SHUT1;            // keyboard.cpp:2025
extern unsigned char pc98_mem_msw_m[8];         // vga_memory.cpp:1535
extern unsigned int pc98_ide_select;            // ide.cpp:4249 (non-static verified; ide.cpp untouched)

// ---- helpers added in .cpp.B copies (same TU as the static definitions) ----
extern void PC98_8251_SaveState(std::ostream& stream);          // keyboard.cpp.B
extern void PC98_8251_LoadState(std::istream& stream);          // keyboard.cpp.B
extern void PC98_Keyboard_State_SaveState(std::ostream& stream);// keyboard.cpp.B
extern void PC98_Keyboard_State_LoadState(std::istream& stream);// keyboard.cpp.B
extern void PC98_Sys8255_SaveState(std::ostream& stream);       // keyboard.cpp.B
extern void PC98_Sys8255_LoadState(std::istream& stream);       // keyboard.cpp.B
extern void PC98_Prn8255_SaveState(std::ostream& stream);       // keyboard.cpp.B
extern void PC98_Prn8255_LoadState(std::istream& stream);       // keyboard.cpp.B
extern void PC98_Sys_RearmEvents(void);                         // keyboard.cpp.B
extern void PC98_Mouse_SaveState(std::ostream& stream);         // keyboard.cpp:2669 (non-static, reused as-is)
extern void PC98_Mouse_LoadState(std::istream& stream);         // keyboard.cpp:2683 (non-static, reused as-is)
extern void PC98_Mouse_RestoreTick(void);                       // keyboard.cpp:2708 (non-static, reused as-is)
extern uint8_t PC98_GetPort29h(void);                           // dma.cpp.B
extern void PC98_SetPort29h(uint8_t v);                         // dma.cpp.B (also restores channel wraparound)
extern void PC98_BIOS_FDC_SaveProgress(std::ostream& stream);   // bios.cpp.B
extern void PC98_BIOS_FDC_LoadProgress(std::istream& stream);   // bios.cpp.B
extern void PC98_LIO_SaveState(std::ostream& stream);           // pc98_lio.cpp.B
extern void PC98_LIO_LoadState(std::istream& stream);           // pc98_lio.cpp.B

namespace {
class Pc98SysState : public SaveState::Component {
public:
    Pc98SysState() { SaveState::instance().registerComponent("pc98_sys", *this); }

private:
    void getBytes(std::ostream& stream) override
    {
        // §2.8 8251 UART + caps/kana/num (§4.2-12: recv queue is dropped on load)
        PC98_8251_SaveState(stream);
        PC98_Keyboard_State_SaveState(stream);
        // §2.9 8255 system/printer + SHUT0/1 (§2.9)
        PC98_Sys8255_SaveState(stream);
        PC98_Prn8255_SaveState(stream);
        WRITE_POD(&PC98_SHUT0, PC98_SHUT0);
        WRITE_POD(&PC98_SHUT1, PC98_SHUT1);
        // §2.10 mouse 8255 (existing blob reused; this component now owns it,
        // so mouse.obj/int33 removal no longer drops it — see B_deferred.txt)
        PC98_Mouse_SaveState(stream);
        // §2.14 memory switch MSW[8]
        WRITE_POD(&pc98_mem_msw_m, pc98_mem_msw_m);
        // §2.14 DMA port 29h (static in S; setter restores channel wraparound)
        {
            const uint8_t p29 = PC98_GetPort29h();
            WRITE_POD(&p29, p29);
        }
        // §2.12 PC-98 IDE select (value only; controller/device bodies are D2 scope)
        WRITE_POD(&pc98_ide_select, pc98_ide_select);
        // §2.11/§4.2-11 INT 1Bh multi-sector progress (function-local statics, via bios.cpp.B)
        PC98_BIOS_FDC_SaveProgress(stream);
        // §2.14 LIO work (paint buffer pointer excluded, re-derived on next LIO call)
        PC98_LIO_SaveState(stream);
    }

    void setBytes(std::istream& stream) override
    {
        // Read order mirrors getBytes exactly; a short read anywhere aborts
        // the whole load (savestate driver rolls back, §4.0 F).
        PC98_8251_LoadState(stream);
        PC98_Keyboard_State_LoadState(stream);
        PC98_Sys8255_LoadState(stream);
        PC98_Prn8255_LoadState(stream);
        READ_POD(&PC98_SHUT0, PC98_SHUT0);
        READ_POD(&PC98_SHUT1, PC98_SHUT1);
        PC98_Mouse_LoadState(stream);
        READ_POD(&pc98_mem_msw_m, pc98_mem_msw_m);
        {
            uint8_t p29 = 0;
            READ_POD(&p29, p29);
            PC98_SetPort29h(p29);
        }
        READ_POD(&pc98_ide_select, pc98_ide_select);
        PC98_BIOS_FDC_LoadProgress(stream);
        PC98_LIO_LoadState(stream);
        // Post-load (§4.1 B행, 부록 순서): UART re-arm first (rx/tx flags, with
        // type-ahead dropped per §4.2-12), then mouse tick. Both helpers start
        // with PIC_RemoveEvents so a restored queue (E-run table) cannot double
        // schedule. SHUT/speaker side effects are NOT replayed here: SHUT flags
        // are restored above, speaker inhibit stays with the Keyboard component.
        PC98_Sys_RearmEvents();
        PC98_Mouse_RestoreTick();
    }
};

static Pc98SysState pc98SysStateInstance;

} // namespace
