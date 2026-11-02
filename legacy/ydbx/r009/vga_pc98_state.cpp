// vga_pc98_state.cpp — PC-98 video save component "pc98_video" (draft, eye-checked, not compiled)
// Design: Fable_PC98_세이브설계_1023.md §2.1~§2.6, §4.0, §4.1 A행, §4.2 1·6·7·10, 부록.
// Sources (read-only S): vga_pc98_{gdc,crtc,egc,dac,cg}.cpp, vga{,_draw,_memory}.cpp, pc98_gdc.h, dosbox.h:371-454.

#include <iostream>
#include <cstdint>

#include "dosbox.h"
#include "pc98_gdc.h"

// ---- externs: definitions verified non-static in S, so no .cpp.A copy ----
// (pc98_gdc[2], tiles, vramop, modereg, tile_counter, srcmask/maskef/mask,
//  gdc_analog, pc98_text_palette[8] come from pc98_gdc.h already.)
extern bool gdc_proc_delay_set;                                 // vga_pc98_gdc.cpp:58
extern bool gdc_5mhz_mode, gdc_5mhz_mode_initial;               // vga_pc98_crtc.cpp:62,71
extern bool GDC_vsync_interrupt, pc98_256kb_boundary;           // vga_pc98_crtc.cpp:70,69
extern bool gdc_clock_1, gdc_clock_2, egc_enable_enable;        // vga_pc98_crtc.cpp:59,60,167
extern unsigned char sel_9a0;                                   // vga_pc98_crtc.cpp:309
extern uint8_t GDC_display_plane, GDC_display_plane_pending;     // vga_pc98_crtc.cpp:72-74
extern uint8_t GDC_display_plane_wait_for_vsync;                // (same)
extern bool pc98_crt_mode, pc98_allow_scanline_effect;          // vga.cpp:198,231
extern bool pc98_allow_4_display_partitions;                    // vga.cpp:232
extern bool pc98_graphics_hide_odd_raster_200line;              // vga.cpp:233
extern bool pc98_attr4_graphic, pc98_40col_text, pc98_31khz_mode;// vga.cpp:234,235,237
extern bool pc98_display_enable, pc98_monochrome_mode;          // vga_draw.cpp:273,274
extern unsigned char pc98_text_first_row_scanline_start, pc98_text_first_row_scanline_end; // vga_draw.cpp:3084,3085
extern unsigned char pc98_text_row_scanline_blank_at, pc98_text_row_scroll_lines; // vga_draw.cpp:3086,3087
extern unsigned char pc98_text_row_scroll_count_start, pc98_text_row_scroll_num_lines; // vga_draw.cpp:3088,3089
extern uint16_t pc98_egc_raw_values[8];                         // vga_pc98_egc.cpp:56
extern uint8_t pc98_egc_access, pc98_egc_fgc;                   // vga_pc98_egc.cpp:58,63
extern uint8_t pc98_egc_lead_plane, pc98_egc_compare_lead;      // vga_pc98_egc.cpp:64,65
extern uint8_t pc98_egc_lightsource, pc98_egc_shiftinput;       // vga_pc98_egc.cpp:66,67
extern uint8_t pc98_egc_regload, pc98_egc_rop;                  // vga_pc98_egc.cpp:68,69
extern uint8_t pc98_egc_foreground_color, pc98_egc_background_color; // vga_pc98_egc.cpp:70,71
extern bool pc98_egc_shift_descend;                             // vga_pc98_egc.cpp:73
extern uint8_t pc98_egc_shift_destbit, pc98_egc_shift_srcbit;   // vga_pc98_egc.cpp:74,75
extern uint16_t pc98_egc_shift_length;                          // vga_pc98_egc.cpp:76
extern egc_quad pc98_egc_src, pc98_egc_bgcm, pc98_egc_fgcm;     // vga_memory.cpp:1002-1004
extern egc_quad pc98_egc_data, pc98_egc_last_vram;              // vga_memory.cpp:1005,1006
extern unsigned char pc98_pegc_mmio[0x200];                      // vga_memory.cpp:39
extern uint32_t pc98_pegc_banks[2];                             // vga_memory.cpp:40
extern uint8_t pc98_pal_vga[256 * 3], pc98_pal_analog[256 * 3]; // vga_pc98_dac.cpp:53,54
extern uint8_t pc98_pal_digital[8], pc98_16col_analog_rgb_palette_index; // vga_pc98_dac.cpp:55,51
extern bool pc98_cg_kanji_dot_access_mode;                      // vga_pc98_cg.cpp:49
extern uint16_t a1_font_load_addr;                              // vga_pc98_cg.cpp:50
extern uint8_t a1_font_char_offset;                             // vga_pc98_cg.cpp:51

// ---- layout mirrors (draft only; final integration should move real structs to a header) ----
// pc98_egc_shifter is defined in vga_memory.cpp:743-783 with no header declaration.
// Methods/templates do not affect object layout; data order/types eye-matched to S.
struct Pc98EgcShiftPOD {
    bool     decrement;
    uint16_t remain, srcbit, dstbit, o_srcbit, o_dstbit;
    uint8_t  buffer[512];
    uint16_t bufi, bufo;
    uint8_t  shft8load, shft8bitr, shft8bitl;
};
extern Pc98EgcShiftPOD pc98_egc_shift;                          // vga_memory.cpp:1008

// Text_Draw_State is defined in vga_draw.cpp:3094-3158 with no header. Data-only mirror.
struct Pc98TextDrawPOD {
    unsigned int  scroll_vmem;
    unsigned char scroll_scanline_cg, row_scanline_cg, row_char, row_scroll_countdown;
};
extern Pc98TextDrawPOD pc98_text_draw;                          // vga_draw.cpp:3160

extern void pc98_update_page_ptrs(void), update_gdc_analog(void); // vga_pc98_gdc.cpp:95; vga_pc98_crtc.cpp:169
extern void pc98_update_palette(void), VGA_SetupHandlers(void); // vga_pc98_dac.cpp:57; vga_memory.cpp:2534
extern void VGA_SetupDrawing(Bitu val), gdc_proc_schedule_delay(void); // vga_draw.cpp:7273; vga_pc98_gdc.cpp:685

namespace {
class Pc98VideoState : public SaveState::Component {
public:
    Pc98VideoState() { SaveState::instance().registerComponent("pc98_video", *this); }

private:
    void getBytes(std::ostream& stream) override
    {
        // §2.1 GDC pair + delay flag (draw.base is a PhysPt offset, no pointers)
        WRITE_POD(&pc98_gdc, pc98_gdc);
        WRITE_POD(&gdc_proc_delay_set, gdc_proc_delay_set);
        // §2.1 CRTC/mode globals
        WRITE_POD(&gdc_5mhz_mode, gdc_5mhz_mode);
        WRITE_POD(&gdc_5mhz_mode_initial, gdc_5mhz_mode_initial);
        WRITE_POD(&GDC_vsync_interrupt, GDC_vsync_interrupt);
        WRITE_POD(&pc98_256kb_boundary, pc98_256kb_boundary);
        WRITE_POD(&gdc_clock_1, gdc_clock_1); WRITE_POD(&gdc_clock_2, gdc_clock_2);
        WRITE_POD(&egc_enable_enable, egc_enable_enable); WRITE_POD(&sel_9a0, sel_9a0);
        WRITE_POD(&pc98_gdc_vramop, pc98_gdc_vramop);
        WRITE_POD(&pc98_gdc_modereg, pc98_gdc_modereg);
        WRITE_POD(&GDC_display_plane, GDC_display_plane);
        WRITE_POD(&GDC_display_plane_pending, GDC_display_plane_pending);
        WRITE_POD(&GDC_display_plane_wait_for_vsync, GDC_display_plane_wait_for_vsync);
        WRITE_POD(&pc98_crt_mode, pc98_crt_mode);
        WRITE_POD(&pc98_allow_scanline_effect, pc98_allow_scanline_effect);
        WRITE_POD(&pc98_allow_4_display_partitions, pc98_allow_4_display_partitions);
        WRITE_POD(&pc98_graphics_hide_odd_raster_200line, pc98_graphics_hide_odd_raster_200line);
        WRITE_POD(&pc98_attr4_graphic, pc98_attr4_graphic);
        WRITE_POD(&pc98_40col_text, pc98_40col_text);
        WRITE_POD(&gdc_analog, gdc_analog); WRITE_POD(&pc98_31khz_mode, pc98_31khz_mode);
        WRITE_POD(&pc98_display_enable, pc98_display_enable);
        WRITE_POD(&pc98_monochrome_mode, pc98_monochrome_mode);
        WRITE_POD(&pc98_text_first_row_scanline_start, pc98_text_first_row_scanline_start);
        WRITE_POD(&pc98_text_first_row_scanline_end, pc98_text_first_row_scanline_end);
        WRITE_POD(&pc98_text_row_scanline_blank_at, pc98_text_row_scanline_blank_at);
        WRITE_POD(&pc98_text_row_scroll_lines, pc98_text_row_scroll_lines);
        WRITE_POD(&pc98_text_row_scroll_count_start, pc98_text_row_scroll_count_start);
        WRITE_POD(&pc98_text_row_scroll_num_lines, pc98_text_row_scroll_num_lines);
        WRITE_POD(&pc98_text_draw, pc98_text_draw);
        // §2.2 EGC registers + latches + shifter (all value types, no pointers)
        WRITE_POD(&pc98_egc_raw_values, pc98_egc_raw_values);
        WRITE_POD(&pc98_egc_access, pc98_egc_access);
        WRITE_POD(&pc98_egc_srcmask, pc98_egc_srcmask);
        WRITE_POD(&pc98_egc_maskef, pc98_egc_maskef);
        WRITE_POD(&pc98_egc_mask, pc98_egc_mask);
        WRITE_POD(&pc98_egc_fgc, pc98_egc_fgc);
        WRITE_POD(&pc98_egc_lead_plane, pc98_egc_lead_plane);
        WRITE_POD(&pc98_egc_compare_lead, pc98_egc_compare_lead);
        WRITE_POD(&pc98_egc_lightsource, pc98_egc_lightsource);
        WRITE_POD(&pc98_egc_shiftinput, pc98_egc_shiftinput);
        WRITE_POD(&pc98_egc_regload, pc98_egc_regload); WRITE_POD(&pc98_egc_rop, pc98_egc_rop);
        WRITE_POD(&pc98_egc_foreground_color, pc98_egc_foreground_color);
        WRITE_POD(&pc98_egc_background_color, pc98_egc_background_color);
        WRITE_POD(&pc98_egc_shift_descend, pc98_egc_shift_descend);
        WRITE_POD(&pc98_egc_shift_destbit, pc98_egc_shift_destbit);
        WRITE_POD(&pc98_egc_shift_srcbit, pc98_egc_shift_srcbit);
        WRITE_POD(&pc98_egc_shift_length, pc98_egc_shift_length);
        WRITE_POD(&pc98_egc_src, pc98_egc_src); WRITE_POD(&pc98_egc_bgcm, pc98_egc_bgcm);
        WRITE_POD(&pc98_egc_fgcm, pc98_egc_fgcm); WRITE_POD(&pc98_egc_data, pc98_egc_data);
        WRITE_POD(&pc98_egc_last_vram, pc98_egc_last_vram);
        WRITE_POD(&pc98_egc_shift, pc98_egc_shift);
        // §2.3 GRCG tile + counter (mode bit lives in vramop above)
        WRITE_POD(&pc98_gdc_tiles, pc98_gdc_tiles);
        WRITE_POD(&pc98_gdc_tile_counter, pc98_gdc_tile_counter);
        // §2.4 palette source regs + PEGC (vga.dac result itself is inside Vga)
        WRITE_POD(&pc98_text_palette, pc98_text_palette);
        WRITE_POD(&pc98_pal_vga, pc98_pal_vga);
        WRITE_POD(&pc98_pal_analog, pc98_pal_analog);
        WRITE_POD(&pc98_pal_digital, pc98_pal_digital);
        WRITE_POD(&pc98_16col_analog_rgb_palette_index, pc98_16col_analog_rgb_palette_index);
        WRITE_POD(&pc98_pegc_mmio, pc98_pegc_mmio);
        WRITE_POD(&pc98_pegc_banks, pc98_pegc_banks);
        // §2.6 CG font-access regs (font RAM itself is inside Vga)
        WRITE_POD(&pc98_cg_kanji_dot_access_mode, pc98_cg_kanji_dot_access_mode);
        WRITE_POD(&a1_font_load_addr, a1_font_load_addr);
        WRITE_POD(&a1_font_char_offset, a1_font_char_offset);
    }

    void setBytes(std::istream& stream) override
    {
        READ_POD(&pc98_gdc, pc98_gdc);
        READ_POD(&gdc_proc_delay_set, gdc_proc_delay_set);
        READ_POD(&gdc_5mhz_mode, gdc_5mhz_mode);
        READ_POD(&gdc_5mhz_mode_initial, gdc_5mhz_mode_initial);
        READ_POD(&GDC_vsync_interrupt, GDC_vsync_interrupt);
        READ_POD(&pc98_256kb_boundary, pc98_256kb_boundary);
        READ_POD(&gdc_clock_1, gdc_clock_1); READ_POD(&gdc_clock_2, gdc_clock_2);
        READ_POD(&egc_enable_enable, egc_enable_enable); READ_POD(&sel_9a0, sel_9a0);
        READ_POD(&pc98_gdc_vramop, pc98_gdc_vramop);
        READ_POD(&pc98_gdc_modereg, pc98_gdc_modereg);
        READ_POD(&GDC_display_plane, GDC_display_plane);
        READ_POD(&GDC_display_plane_pending, GDC_display_plane_pending);
        READ_POD(&GDC_display_plane_wait_for_vsync, GDC_display_plane_wait_for_vsync);
        READ_POD(&pc98_crt_mode, pc98_crt_mode);
        READ_POD(&pc98_allow_scanline_effect, pc98_allow_scanline_effect);
        READ_POD(&pc98_allow_4_display_partitions, pc98_allow_4_display_partitions);
        READ_POD(&pc98_graphics_hide_odd_raster_200line, pc98_graphics_hide_odd_raster_200line);
        READ_POD(&pc98_attr4_graphic, pc98_attr4_graphic);
        READ_POD(&pc98_40col_text, pc98_40col_text);
        READ_POD(&gdc_analog, gdc_analog); READ_POD(&pc98_31khz_mode, pc98_31khz_mode);
        READ_POD(&pc98_display_enable, pc98_display_enable);
        READ_POD(&pc98_monochrome_mode, pc98_monochrome_mode);
        READ_POD(&pc98_text_first_row_scanline_start, pc98_text_first_row_scanline_start);
        READ_POD(&pc98_text_first_row_scanline_end, pc98_text_first_row_scanline_end);
        READ_POD(&pc98_text_row_scanline_blank_at, pc98_text_row_scanline_blank_at);
        READ_POD(&pc98_text_row_scroll_lines, pc98_text_row_scroll_lines);
        READ_POD(&pc98_text_row_scroll_count_start, pc98_text_row_scroll_count_start);
        READ_POD(&pc98_text_row_scroll_num_lines, pc98_text_row_scroll_num_lines);
        READ_POD(&pc98_text_draw, pc98_text_draw);
        READ_POD(&pc98_egc_raw_values, pc98_egc_raw_values);
        READ_POD(&pc98_egc_access, pc98_egc_access);
        READ_POD(&pc98_egc_srcmask, pc98_egc_srcmask);
        READ_POD(&pc98_egc_maskef, pc98_egc_maskef);
        READ_POD(&pc98_egc_mask, pc98_egc_mask);
        READ_POD(&pc98_egc_fgc, pc98_egc_fgc);
        READ_POD(&pc98_egc_lead_plane, pc98_egc_lead_plane);
        READ_POD(&pc98_egc_compare_lead, pc98_egc_compare_lead);
        READ_POD(&pc98_egc_lightsource, pc98_egc_lightsource);
        READ_POD(&pc98_egc_shiftinput, pc98_egc_shiftinput);
        READ_POD(&pc98_egc_regload, pc98_egc_regload); READ_POD(&pc98_egc_rop, pc98_egc_rop);
        READ_POD(&pc98_egc_foreground_color, pc98_egc_foreground_color);
        READ_POD(&pc98_egc_background_color, pc98_egc_background_color);
        READ_POD(&pc98_egc_shift_descend, pc98_egc_shift_descend);
        READ_POD(&pc98_egc_shift_destbit, pc98_egc_shift_destbit);
        READ_POD(&pc98_egc_shift_srcbit, pc98_egc_shift_srcbit);
        READ_POD(&pc98_egc_shift_length, pc98_egc_shift_length);
        READ_POD(&pc98_egc_src, pc98_egc_src); READ_POD(&pc98_egc_bgcm, pc98_egc_bgcm);
        READ_POD(&pc98_egc_fgcm, pc98_egc_fgcm); READ_POD(&pc98_egc_data, pc98_egc_data);
        READ_POD(&pc98_egc_last_vram, pc98_egc_last_vram);
        READ_POD(&pc98_egc_shift, pc98_egc_shift);
        READ_POD(&pc98_gdc_tiles, pc98_gdc_tiles);
        READ_POD(&pc98_gdc_tile_counter, pc98_gdc_tile_counter);
        READ_POD(&pc98_text_palette, pc98_text_palette);
        READ_POD(&pc98_pal_vga, pc98_pal_vga);
        READ_POD(&pc98_pal_analog, pc98_pal_analog);
        READ_POD(&pc98_pal_digital, pc98_pal_digital);
        READ_POD(&pc98_16col_analog_rgb_palette_index, pc98_16col_analog_rgb_palette_index);
        READ_POD(&pc98_pegc_mmio, pc98_pegc_mmio);
        READ_POD(&pc98_pegc_banks, pc98_pegc_banks);
        READ_POD(&pc98_cg_kanji_dot_access_mode, pc98_cg_kanji_dot_access_mode);
        READ_POD(&a1_font_load_addr, a1_font_load_addr);
        READ_POD(&a1_font_char_offset, a1_font_char_offset);
        // Post-load (§4.1 A행, 부록 순서): page ptrs first (handlers need VRAM base),
        // analog before palette (palette branch depends on it), palette before drawing (DAC first),
        // handlers before drawing (CPU mapping before renderer), FIFO last (needs valid GDC state).
        pc98_update_page_ptrs();
        update_gdc_analog();
        pc98_update_palette();
        VGA_SetupHandlers();
        VGA_SetupDrawing(0);
        if (!pc98_gdc[GDC_MASTER].fifo_empty() || !pc98_gdc[GDC_SLAVE].fifo_empty())
            gdc_proc_schedule_delay();
    }
};

static Pc98VideoState pc98VideoStateInstance;

} // namespace
