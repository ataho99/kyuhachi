// pc98_fdc_state.cpp — PC-98 FDC save component "pc98_fdc" (draft, eye-checked, not compiled)
// Design: Fable_PC98_세이브설계_1023.md §2.11, §4.0, §4.1 D1행, §4.2 1·8번.
// Sources (read-only S): S/src/hardware/floppy.cpp:45-62,106-166,224,290.
// Field list: W/save_src/D1/D1_inventory.txt. Accessors live in the
// floppy.cpp.D1 copy (same TU as floppycontroller[]), integrated later
// into the real floppy.cpp; this file only calls them extern.

#include <iostream>
#include <cstdint>

#include "dosbox.h"

// Defined at the bottom of floppy.cpp (.D1 copy; integration target: real floppy.cpp).
// FloppyController/FloppyDevice members touched there are all public and the
// static floppycontroller[] array is visible in that TU, so no friend line.
extern void FDC_SaveState_Write(std::ostream& stream);
extern void FDC_SaveState_Read(std::istream& stream);

namespace {
class Pc98FdcState : public SaveState::Component {
public:
    Pc98FdcState() { SaveState::instance().registerComponent("pc98_fdc", *this); }

private:
    void getBytes(std::ostream& stream) override
    {
        // Layout: u8 ctl_present + controller value fields + 4x (u8 dev_present + device fields).
        // Pointers/config excluded (host re-attaches image + DMA at boot, FDC_Init re-applies config).
        FDC_SaveState_Write(stream);
    }

    void setBytes(std::istream& stream) override
    {
        FDC_SaveState_Read(stream);
        // No PIC re-arm here: pending FDC_MotorStep / FDC_ProcessReadID events are restored
        // via the pic.cpp.E table (GENSEI_SAVE_PIC_FDC, see FE/E_events.txt). The pending flags
        // (busy_status/motor_steps/data_register_ready, out_res_state) travel in this same blob,
        // so "event restored but flag off" cannot happen (checked in D1_inventory.txt).
        // No handler re-install needed: IO handlers are static functions, DMA/device pointers
        // are excluded from the blob and stay valid on the live object.
        // Disk image contents + write-backs are NOT stored (§4.2 8 — host UI notice).
    }
};

static Pc98FdcState pc98FdcStateInstance;

} // namespace
