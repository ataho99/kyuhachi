/* gensei_fontreg.h -- t39 (#291): res\fonts 의 bmp 글꼴 자동 등록·검사·배치 묶음 판정 (UI·호스트·하니스 공용) */
#pragma once
#include "gensei_ov.h"
#include <string>
#include <vector>
#include <math.h>
#include <algorithm>

namespace fontreg {
struct Info { std::string name, path, group, why; bool ok = false; };
/* 묶음: ks2350 = 한글 KS 2350자를 JIS 16~40구에 얹은 배치(한자 칸은 일본어 한자와 같음) · jis = 일본어 JIS 표준 · custom = 패치 전용 배치(교체 불가) */
inline const char * group_label(const std::string & g) { if (g == "ks2350") return "\xED\x95\x9C\xEA\xB8\x80 (KS 2350 \xEB\xB0\xB0\xEC\xB9\x98)"; if (g == "jis") return "\xEC\x9D\xBC\xEB\xB3\xB8\xEC\x96\xB4 (JIS)"; return "\xEC\xA0\x84\xEC\x9A\xA9 \xEB\xB0\xB0\xEC\xB9\x98"; }   /* 한글 (KS 2350 배치) / 일본어 (JIS) / 전용 배치 */
inline void feat(const ov::BmpFont & f, int ku, int t, double * v) { for (int gy = 0; gy < 4; gy++) for (int gx = 0; gx < 4; gx++) { int c = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (f.ink(ku * 16 + gx * 4 + x, (t + 32) * 16 + gy * 4 + y)) c++; v[gy * 4 + gx] = c; } }
inline double ncc(const double * a, const double * b) { double ma = 0, mb = 0; for (int i = 0; i < 16; i++) { ma += a[i]; mb += b[i]; } ma /= 16; mb /= 16; double sa = 0, sb = 0, sab = 0; for (int i = 0; i < 16; i++) { sa += (a[i] - ma) * (a[i] - ma); sb += (b[i] - mb) * (b[i] - mb); sab += (a[i] - ma) * (b[i] - mb); } return sa > 0 && sb > 0 ? sab / sqrt(sa * sb) : 0; }
inline bool same_rows(const ov::BmpFont & f, const ov::BmpFont & g, int k0, int k1) { for (int ku = k0; ku <= k1; ku++) for (int t = 1; t <= 94; t++) for (int y = 0; y < 16; y += 2) for (int x = 0; x < 16; x++) if (f.ink(ku * 16 + x, (t + 32) * 16 + y) != g.ink(ku * 16 + x, (t + 32) * 16 + y)) return false; return true; }
/* ref = 기준(번들 pc98player_font.bmp, ks2350). 반환 = ks2350 | jis | custom */
inline std::string classify(const ov::BmpFont & f, const ov::BmpFont & ref) {
    double s = 0; int n = 0, i;
    for (int ku = 16; ku <= 40; ku++) for (int t = 1; t <= 94; t += 3) { double a[16], b[16]; feat(f, ku, t, a); feat(ref, ku, t, b); s += ncc(a, b); n++; }
    (void)i; double c = s / n; bool hanja = same_rows(f, ref, 48, 52);
    if (c >= 0.42 && hanja) return "ks2350"; if (!hanja) return "jis"; return "custom";
}
inline bool ends_bmp(const std::string & n) { return n.size() > 4 && (n.compare(n.size() - 4, 4, ".bmp") == 0 || n.compare(n.size() - 4, 4, ".BMP") == 0); }
/* files = res\fonts 안의 파일 이름들. dir = 그 폴더. 이름 = 확장자 뺀 파일 이름(fonts.ini 등록 불필요) */
inline std::vector<Info> scan(const std::string & dir, const std::vector<std::string> & files, const std::string & sep = "/") {
    std::vector<Info> out; ov::BmpFont ref; bool hr = ref.load(dir + sep + "pc98player_font.bmp");
    for (auto & fn : files) {
        if (!ends_bmp(fn)) continue; Info i; i.name = fn.substr(0, fn.size() - 4); i.path = dir + sep + fn;
        ov::BmpFont f; i.ok = f.load(i.path, &i.why);
        if (i.ok) i.group = hr ? classify(f, ref) : "custom";
        out.push_back(i);
    }
    std::sort(out.begin(), out.end(), [](const Info & a, const Info & b) { return a.name < b.name; });
    return out;
}
} // namespace fontreg
