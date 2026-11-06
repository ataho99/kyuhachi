#pragma once
/* screens/indiv.h -- build 9: which own-value bit (Ui::ovm) a settings line carries; used for the per-line "back to global" key. */
inline unsigned Ui::indiv_bit(Scr s, int id) {
    static const unsigned sc[12] = { 0u, 1u << 0, 1u << 1, 1u << 5, 1u << 6, 1u << 3, 1u << 4, 1u << 2, 0u, 0u, 1u << 16, 0u };
    static const unsigned cf[15] = { 0u, 1u << 7, 1u << 8, 0u, (1u << 9) | (15u << 17), 1u << 10, 1u << 11, 1u << 14,
                                     1u << 12, 1u << 13, 1u << 15, 0u, 0u, 0u, 0u };
    if (s == S_SCREEN) return (id >= 0 && id < 12) ? sc[id] : 0u;
    if (s == S_CONF) return (id >= 0 && id < 15) ? cf[id] : 0u;
    return 0u;
}
