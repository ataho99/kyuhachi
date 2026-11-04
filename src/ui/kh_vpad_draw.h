#pragma once
/* kh_vpad_draw.h -- draws the virtual pad into a window-sized premultiplied ARGB buffer (render thread). */
#include <string.h>
namespace vpad {
static inline uint32_t pm(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | (b * a / 255);
}
static void shape(uint32_t * buf, int bw, int bh, const Rect & r, bool round, int inset, uint32_t c) {
    int x0 = r.x + inset, y0 = r.y + inset, x1 = r.x + r.w - inset, y1 = r.y + r.h - inset;
    int cx2 = r.x * 2 + r.w, cy2 = r.y * 2 + r.h, rad = r.w / 2 - inset;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        if (x < 0 || y < 0 || x >= bw || y >= bh) continue;
        if (round) { int dx = x * 2 + 1 - cx2, dy = y * 2 + 1 - cy2; if ((dx * dx + dy * dy) / 4 > rad * rad) continue; }
        buf[(size_t)y * bw + x] = c;
    }
}
static void icon(uint32_t * buf, int bw, int bh, int b, const Rect & r, uint32_t ink) {
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2, q = r.w * 28 / 100, th = r.w / 14 > 2 ? r.w / 14 : 2;
    for (int y = cy - q; y <= cy + q; y++) for (int x = cx - q; x <= cx + q; x++) {
        if (x < 0 || y < 0 || x >= bw || y >= bh) continue;
        int px = x - cx, py = y - cy, ax = px < 0 ? -px : px, ay = py < 0 ? -py : py; bool on = false;
        if (b == B_UP) on = ax * 10 <= (py + q) * 6; else if (b == B_DOWN) on = ax * 10 <= (q - py) * 6;
        else if (b == B_LEFT) on = ay * 10 <= (px + q) * 6; else if (b == B_RIGHT) on = ay * 10 <= (q - px) * 6;
        else if (b == B_O) { int d2 = px * px + py * py; on = d2 <= (q + th) * (q + th) && d2 >= (q - th) * (q - th); }
        else if (b == B_X) { int d = ax - ay; on = (d < 0 ? -d : d) <= th; }
        if (on) buf[(size_t)y * bw + x] = ink;
    }
}
static void render(uint32_t * buf, int dw, int dh, const Geo & g, unsigned mask) {
    memset(buf, 0, (size_t)dw * dh * 4);
    ov::Canvas cv = { buf, dw, dh };
    for (int b = 0; b < NB; b++) {
        const Rect & r = g.r[b]; bool on = ((mask >> b) & 1u) != 0, round = b == B_O || b == B_X;
        uint32_t edge = on ? pm(235, 255, 255, 255) : pm(190, 200, 208, 230), face = on ? pm(235, 255, 192, 0) : pm(130, 32, 40, 64);
        uint32_t ink = on ? pm(255, 0, 0, 0) : pm(255, 255, 255, 255);
        shape(buf, dw, dh, r, round, 0, edge); shape(buf, dw, dh, r, round, r.w / 24 > 2 ? r.w / 24 : 2, face);
        if (b < B_SEL) icon(buf, dw, dh, b, r, ink);
        else {
            const char * t = b == B_SEL ? "SELECT" : "START"; int tw = ov::tw(t);
            ov::text(cv, r.x + (r.w - tw) / 2, r.y + (r.h - 16) / 2, t, ink);
        }
    }
}
}   /* namespace vpad */
