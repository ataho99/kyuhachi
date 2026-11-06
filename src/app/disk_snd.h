#pragma once
/* app/disk_snd.h -- #641/#642: floppy drive noise (seek / read / spin / insert / eject) on its own mixer channel "DISKNOISE".
   No core change: the mixer handler polls the file position of the floppy image objects (imageDiskList[0..1]); any move of
   the position is a disk access (DOS, INT 1Bh and FDC paths all end in that file), the cylinder is estimated from it.
   Loading speed is untouched: nothing is delayed, the sound is only played from the mixer handler. Rules (all enforced in try_start / gen):
   - one transient voice at a time (seek, read, insert, eject never overlap, never restart while playing);
   - a voice of the same kind starts only GAP_MS[kind] after the previous one of that kind ended, any voice only
     ANY_GAP_MS after the previous voice ended;
   - seek requests that arrive while a voice plays are merged into ONE later seek (distance summed, capped);
     read requests that arrive while a voice plays are dropped;
   - the spin sound is one continuous loop while the motor is on (fades in / out), never restarted.
   Samples are FILES (no synthesis): folder run\res\disk-noises\ with fixed names fdd_seek1.wav fdd_seek2.wav
   fdd_seek3.wav (short / medium / long seek) fdd_read.wav fdd_insert.wav
   fdd_eject.wav fdd_spin.wav (16-bit PCM mono/stereo, any rate; fdd_spin.wav must loop seamlessly). A missing file means
   that sound is simply not played. Sources and licenses: CREDITS.txt in that folder.
   HDD noise: placeholder only (not played). */
#include <vector>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
namespace dsn {
static const int RATE = 22050;
enum Kind { K_SEEK = 0, K_READ = 1, K_INS = 2, K_EJE = 3, K_N = 4, K_SPIN = 4 };
static const char * const KNAME[5] = { "seek", "read", "insert", "eject", "spin" };
static const char * const FNAME[5] = { "fdd_seek.wav", "fdd_read.wav", "fdd_insert.wav", "fdd_eject.wav", "fdd_spin.wav" };
static const int GAP_MS[K_N] = { 60, 120, 400, 400 };   /* min quiet time between two voices of the same kind */
static const int ANY_GAP_MS = 30;                        /* min quiet time between any two voices */
static const int MOTOR_HOLD_MS = 1500;                   /* motor stays on this long after the last access */
static const int SEEK_CAP = 80;
struct St {
    int mode = 0, vol = 0;            /* mode 0 off 1 seek only 2 on; vol 0..8 */
    void * ch = NULL; bool boost = false; bool inited = false;
    uint64_t t = 0;                   /* sample clock of this channel */
    uint64_t last_act[2] = { 0, 0 }; bool act[2] = { false, false }; int cyl[2] = { 0, 0 };
    int pend_seek = 0, pend_read = 0, pend_ins = 0, pend_eje = 0; int last_kind = -1;
    bool v_on = false; int v_kind = 0; size_t v_pos = 0; uint64_t v_start = 0; const std::vector<int16_t> * v_buf = NULL;
    uint64_t end_t[K_N] = { 0, 0, 0, 0 }; bool ended[K_N] = { false, false, false, false };
    uint64_t any_end = 0; bool any_ended = false;
    float spin_g = 0.0f; size_t spin_pos = 0; uint64_t spin_start = 0; bool spin_run = false;
    std::vector<int16_t> wav[5]; bool have[5] = { false, false, false, false, false };
    std::vector<int16_t> seekv[3]; bool hseek[3] = { false, false, false };   /* fdd_seek1..3.wav: short / medium / long seek */
    unsigned ev[7] = { 0, 0, 0, 0, 0, 0, 0 }; unsigned started[5] = { 0, 0, 0, 0, 0 }; unsigned merged = 0, dropped = 0;
    unsigned calls = 0; unsigned long long nz = 0;
    unsigned pk = 0, pk_all = 0; double sumsq = 0.0; unsigned long long ns = 0;
    unsigned lpk[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   /* file peaks after norm: 0 seek1 1 read 2 ins 3 eje 4 spin 5 seek2 6 seek3 */
    FILE * lg = NULL; std::vector<int16_t> dump; bool dumping = false; std::string wavpath;
    int test = 0; unsigned tc = 0;
    const void * ip[2] = { NULL, NULL }; long long lpos[2] = { -1, -1 }; unsigned polled = 0, chg = 0;   /* image poll */
};
static St S;
static inline int16_t to16(float v) { v *= 32767.0f; return (int16_t)(v > 32767.0f ? 32767 : v < -32767.0f ? -32767 : v); }
static inline int ms(int m) { return m * RATE / 1000; }
static inline uint32_t le32(const unsigned char * p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
/* 16-bit PCM wav (mono or stereo, any rate) -> mono 22050 */
static bool load_wav(const std::string & path, std::vector<int16_t> & out) {
    FILE * f = fopen(path.c_str(), "rb"); if (!f) return false;
    std::vector<unsigned char> d; unsigned char tmp[4096]; size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) d.insert(d.end(), tmp, tmp + n);
    fclose(f);
    if (d.size() < 44 || memcmp(&d[0], "RIFF", 4) || memcmp(&d[8], "WAVE", 4)) return false;
    int ch = 0, rate = 0, bits = 0, fmt = 0; size_t doff = 0, dlen = 0;
    for (size_t p = 12; p + 8 <= d.size();) {
        const uint32_t sz = le32(&d[p + 4]);
        if (!memcmp(&d[p], "fmt ", 4) && p + 24 <= d.size()) {
            fmt = d[p + 8] | (d[p + 9] << 8); ch = d[p + 10] | (d[p + 11] << 8); rate = (int)le32(&d[p + 12]);
            bits = d[p + 22] | (d[p + 23] << 8);
        } else if (!memcmp(&d[p], "data", 4)) {
            doff = p + 8; dlen = sz; if (doff + dlen > d.size()) dlen = d.size() - doff; break;
        }
        p += 8 + (size_t)sz + (sz & 1);
    }
    if (fmt != 1 || bits != 16 || (ch != 1 && ch != 2) || rate < 4000 || rate > 192000 || dlen < 4) return false;
    const size_t fr = dlen / (2 * (size_t)ch); std::vector<float> m(fr);
    for (size_t i = 0; i < fr; i++) {
        const unsigned char * q = &d[doff + i * 2 * (size_t)ch]; int a = (int16_t)(q[0] | (q[1] << 8)); int c = a;
        if (ch == 2) { const int b2 = (int16_t)(q[2] | (q[3] << 8)); c = (a + b2) / 2; }
        m[i] = (float)c;
    }
    const size_t on = (size_t)((double)fr * RATE / rate); out.assign(on, 0);
    for (size_t i = 0; i < on; i++) {
        const double p = (double)i * rate / RATE; const size_t i0 = (size_t)p; const double fr0 = p - (double)i0;
        const float a = m[i0 < fr ? i0 : fr - 1], b = m[i0 + 1 < fr ? i0 + 1 : fr - 1];
        out[i] = (int16_t)(a + (b - a) * fr0);
    }
    return true;
}
/* scale a buffer so that its peak is `target` (0..1 of full scale); returns the peak (0..32767) after scaling */
static unsigned norm(std::vector<int16_t> & b, float target) {
    int pk = 0; for (size_t i = 0; i < b.size(); i++) { const int a = b[i] < 0 ? -(int)b[i] : (int)b[i]; if (a > pk) pk = a; }
    if (pk < 16) return (unsigned)pk;
    const float g = target * 32767.0f / (float)pk;
    for (size_t i = 0; i < b.size(); i++) b[i] = to16((float)b[i] * g / 32767.0f);
    return (unsigned)(pk * g);
}
static inline void logv(int kind, uint64_t a, uint64_t b) {
    if (S.lg) fprintf(S.lg, "V %s %llu %llu\n", KNAME[kind], (unsigned long long)a, (unsigned long long)b);
}
static inline bool gap_ok(int kind, uint64_t now) {
    if (S.ended[kind] && now < S.end_t[kind] + (uint64_t)ms(GAP_MS[kind])) return false;
    if (S.any_ended && now < S.any_end + (uint64_t)ms(ANY_GAP_MS)) return false;
    return true;
}
static void start_voice(int kind, uint64_t now, int dist = 0) {
    S.v_buf = &S.wav[kind];
    if (kind == K_SEEK) {   /* pick by distance: 1-2 tracks -> seek1, up to 12 -> seek2, more -> seek3; fall back to what exists */
        const int want = dist <= 2 ? 0 : dist <= 12 ? 1 : 2; int use = -1;
        for (int k = want; k >= 0 && use < 0; k--) if (S.hseek[k]) use = k;
        for (int k = want + 1; k < 3 && use < 0; k++) if (S.hseek[k]) use = k;
        if (use >= 0) S.v_buf = &S.seekv[use];
    }
    S.v_on = !S.v_buf->empty(); S.v_kind = kind; S.last_kind = kind;
    S.v_pos = 0; S.v_start = now; if (S.v_on) S.started[kind]++;
}
static void try_start(uint64_t now) {
    if (S.v_on || S.mode < 1) return;
    if (S.mode == 2 && S.pend_eje && gap_ok(K_EJE, now)) { S.pend_eje = 0; start_voice(K_EJE, now); return; }
    if (S.mode == 2 && S.pend_ins && !S.pend_eje && gap_ok(K_INS, now)) { S.pend_ins = 0; start_voice(K_INS, now); return; }
    const bool sk = S.pend_seek > 0 && gap_ok(K_SEEK, now);
    const bool rd = S.mode == 2 && S.pend_read > 0 && gap_ok(K_READ, now);
    if (sk && (!rd || S.last_kind != K_SEEK)) { const int d = S.pend_seek; S.pend_seek = 0; start_voice(K_SEEK, now, d); }
    else if (rd) { S.pend_read = 0; start_voice(K_READ, now); }
}
static void do_seek(int d, int c) {
    if (!S.have[K_SEEK]) { S.cyl[d] = c; return; }
    int dl = c - S.cyl[d]; if (dl < 0) dl = -dl; S.cyl[d] = c;
    if (dl > 0) { if (S.pend_seek > 0 || S.v_on) S.merged++; S.pend_seek += dl; if (S.pend_seek > SEEK_CAP) S.pend_seek = SEEK_CAP; }
}
static void hook(int ev, int drive, int cyl, int nbytes) {
    if (S.mode == 0 || !S.ch || S.boost) return;
    const int d = drive & 1; if (ev >= 1 && ev <= 6) S.ev[ev]++;
    if (S.mode == 2) { S.last_act[d] = S.t; S.act[d] = true; }
    const bool rd = S.mode == 2 && S.have[K_READ];
    switch (ev) {
        case 1: do_seek(d, cyl); break;
        case 2: case 3: do_seek(d, cyl); if (rd) { if (S.v_on) S.dropped++; else S.pend_read += nbytes > 0 ? nbytes : 512; } break;
        case 4: do_seek(d, 0); break;
        case 5: if (rd && !S.v_on) S.pend_read += 256; break;
        case 6: do_seek(d, cyl); if (rd && !S.v_on) S.pend_read += 1024; break;
        default: break;
    }
}
static void media(int drive) {   /* disk swapped: eject then insert (gh_attach with replace) */
    if (S.mode != 2 || !S.ch) return;
    if (!S.pend_ins && !(S.v_on && S.v_kind == K_EJE)) S.pend_eje = S.have[K_EJE] ? 1 : 0;   /* 2nd swap while queued: 1 eject + 1 insert */
    S.pend_ins = S.have[K_INS] ? 1 : 0;
    S.last_act[drive & 1] = S.t; S.act[drive & 1] = true;
}
static void test_step() {   /* --dsntest: scripted request storm (1 step per mixer call, ~1 ms of emulated time) */
    const unsigned c = S.tc++; const unsigned long long ms_ = (unsigned long long)c;
    if (ms_ >= 1000 && ms_ < 2000) hook(1, 0, (int)((c * 37u + 11u) % 78u), 0);            /* seek storm */
    else if (ms_ >= 2500 && ms_ < 3500) hook(2, 0, (int)(((c / 50u) * 5u) % 78u), 1024);   /* read storm */
    else if (ms_ == 4000 || ms_ == 4100) media(0);                                          /* swap, twice quickly */
    else if (ms_ >= 5000 && ms_ < 6500) {
        if (c & 1) hook(1, 0, (int)((c * 13u) % 78u), 0); else hook(2, 0, (int)((c * 13u) % 78u), 512);
    }
}
static void poll();
static void gen(int16_t * out, int n) {
    S.calls++;
    if (S.mode > 0 && !S.test) poll();
    if (S.test) test_step();
    if (S.boost) { for (int i = 0; i < n; i++) out[i] = 0; S.t += (uint64_t)n; return; }
    const uint64_t now = S.t; const float gain = S.vol / 4.0f;   /* vol 4 = unity on the normalised files, 8 = +6 dB */
    bool motor = false;
    if (S.mode == 2 && S.have[K_SPIN])
        for (int d = 0; d < 2; d++) if (S.act[d] && now < S.last_act[d] + (uint64_t)ms(MOTOR_HOLD_MS)) motor = true;
    if (!S.v_on) try_start(now);
    const float up = 1.0f / (0.30f * RATE), dn = 1.0f / (0.50f * RATE);
    for (int i = 0; i < n; i++) {
        float o = 0.0f;
        if (motor) { S.spin_g += up; if (S.spin_g > 1.0f) S.spin_g = 1.0f; } else { S.spin_g -= dn; if (S.spin_g < 0.0f) S.spin_g = 0.0f; }
        if (S.spin_g > 0.0f) {
            if (!S.spin_run) { S.spin_run = true; S.spin_start = now + (uint64_t)i; S.started[K_SPIN]++; }
            const std::vector<int16_t> & sl = S.wav[K_SPIN];
            if (!sl.empty()) { o += (float)sl[S.spin_pos % sl.size()] * (1.0f / 32768.0f) * S.spin_g; S.spin_pos++; }
        } else if (S.spin_run) {
            S.spin_run = false;
            if (S.lg) fprintf(S.lg, "V spin %llu %llu\n", (unsigned long long)S.spin_start, (unsigned long long)(now + (uint64_t)i));
        }
        if (S.v_on) {
            o += (float)(*S.v_buf)[S.v_pos] * (1.0f / 32768.0f); S.v_pos++;
            if (S.v_pos >= S.v_buf->size()) {
                const uint64_t e = now + (uint64_t)i + 1; S.end_t[S.v_kind] = e; S.ended[S.v_kind] = true;
                S.any_end = e; S.any_ended = true;
                logv(S.v_kind, S.v_start, e); S.v_on = false; S.pend_read = 0;
            }
        }
        float y = o * gain; const float ay = y < 0.0f ? -y : y;   /* soft knee above 0.8 */
        if (ay > 0.8f) { const float c = 0.8f + 0.2f * tanhf((ay - 0.8f) * 5.0f); y = y < 0.0f ? -c : c; }
        const int16_t v = to16(y); out[i] = v; if (v) S.nz++;
        const unsigned av = (unsigned)(v < 0 ? -(int)v : (int)v); if (av > S.pk) S.pk = av;
        S.sumsq += (double)v * (double)v; S.ns++;
    }
    if (S.pk > S.pk_all) S.pk_all = S.pk;
    if (S.dumping) S.dump.insert(S.dump.end(), out, out + n);
    S.t += (uint64_t)n;
}
/* level of the window since the last call (peak 0..32767, rms), then reset */
static void lvl_take(unsigned & pk, double & rms) {
    pk = S.pk; rms = S.ns ? sqrt(S.sumsq / (double)S.ns) : 0.0; S.pk = 0; S.sumsq = 0.0; S.ns = 0;
}
static void flush() {
    if (S.lg) {
        fprintf(S.lg, "C calls=%u t=%llu nz=%llu ev=%u,%u,%u,%u,%u,%u started=%u,%u,%u,%u,%u merged=%u dropped=%u have=%d%d%d%d%d\n",
            S.calls, (unsigned long long)S.t, S.nz, S.ev[1], S.ev[2], S.ev[3], S.ev[4], S.ev[5], S.ev[6],
            S.started[0], S.started[1], S.started[2], S.started[3], S.started[4], S.merged, S.dropped,
            (int)S.have[0], (int)S.have[1], (int)S.have[2], (int)S.have[3], (int)S.have[4]);
        fflush(S.lg);
    }
    if (S.dumping && !S.wavpath.empty() && !S.dump.empty()) {
        FILE * f = fopen(S.wavpath.c_str(), "wb"); if (!f) return;
        const uint32_t dl = (uint32_t)(S.dump.size() * 2), rt = RATE, br = RATE * 2, rl = 36 + dl, f16 = 16;
        const uint16_t one = 1, bl = 2, bt = 16;
        fwrite("RIFF", 1, 4, f); fwrite(&rl, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&f16, 4, 1, f);
        fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f); fwrite(&rt, 4, 1, f); fwrite(&br, 4, 1, f);
        fwrite(&bl, 2, 1, f); fwrite(&bt, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&dl, 4, 1, f);
        fwrite(&S.dump[0], 2, S.dump.size(), f); fclose(f);
    }
}
static void reset_voices() {
    S.v_on = false; S.pend_seek = S.pend_read = S.pend_ins = S.pend_eje = 0; S.act[0] = S.act[1] = false;
}
static void load_all(const std::string & resdir) {
    static const char * const sk[3] = { "fdd_seek1.wav", "fdd_seek2.wav", "fdd_seek3.wav" };
    for (int i = 1; i < 5; i++) S.have[i] = !resdir.empty() && load_wav(resdir + "\\" + FNAME[i], S.wav[i]);
    for (int k = 0; k < 3; k++) S.hseek[k] = !resdir.empty() && load_wav(resdir + "\\" + sk[k], S.seekv[k]);
    S.have[K_SEEK] = S.hseek[0] || S.hseek[1] || S.hseek[2];
    /* The recordings are quiet (clicks -6 .. -9 dBFS, spin -14): normalise so that vol 4 is clearly audible next to the game's
       FM/SSG mix. One-shots peak at 0.7 full scale, the continuous spin loop at 0.25. */
    for (int i = 1; i < 5; i++) if (S.have[i]) S.lpk[i] = norm(S.wav[i], i == K_SPIN ? 0.25f : 0.7f);
    for (int k = 0; k < 3; k++) if (S.hseek[k]) S.lpk[k == 0 ? 0 : 4 + k] = norm(S.seekv[k], 0.7f);
}
#ifndef DSN_NO_MIXER
/* disk activity = the file position of the floppy image moved since the last mixer call (needs bios_disk.h) */
static void poll() {
    for (int d = 0; d < 2; d++) {
        imageDisk * im = imageDiskList[d];
        if ((const void *)im != S.ip[d]) {
            const bool had = S.ip[d] != NULL; S.ip[d] = im; S.lpos[d] = -1; if (had && im) media(d); continue;
        }
        if (!im || !im->diskimg) continue;
        S.polled++;
        const long long p = (long long)ftello64(im->diskimg);
        if (S.lpos[d] < 0) { S.lpos[d] = p; continue; }
        if (p == S.lpos[d]) continue;
        S.lpos[d] = p; S.chg++;
        const uint64_t bpc = (uint64_t)im->heads * im->sectors * im->sector_size; int cyl = 0;
        if (bpc && p >= (long long)im->image_base) {
            cyl = (int)(((uint64_t)p - im->image_base) / bpc); if (im->cylinders && cyl >= (int)im->cylinders) cyl = (int)im->cylinders - 1;
        }
        hook(2, d, cyl, 512);
    }
}
#else
static void poll() {}
#endif
#ifndef DSN_NO_MIXER
/* ---- glue to the DOSBox mixer (needs mixer.h, included before this file) ---- */
static void handler(Bitu len) {
    if (!S.ch) return;
    S.boost = gApp.boost_on;
    int16_t buf[2048];
    while (len > 0) {
        const int n = len > 2048 ? 2048 : (int)len; gen(buf, n);
        ((MixerChannel *)S.ch)->AddSamples_m16((Bitu)n, buf); len -= (Bitu)n;
    }
}
static void init(const std::string & resdir, const std::string & logpath, const std::string & wavpath, int test) {
    if (S.inited) return;
    S.inited = true; S.test = test; load_all(resdir);
    if (!logpath.empty()) S.lg = fopen(logpath.c_str(), "w");
    if (!wavpath.empty()) { S.wavpath = wavpath; S.dumping = true; }
    S.ch = MIXER_AddChannel(handler, RATE, "DISKNOISE");
}
/* mode 0 off / 1 seek only / 2 on ; vol 0..8 (0 = off). Called from gh_apply_settings. */
static void apply(int mode, int vol, int override_mode, const std::string & resdir, const std::string & logpath,
                  const std::string & wavpath, int test) {
    if (override_mode >= 0) mode = override_mode;
    if (mode < 0) mode = 0; if (mode > 2) mode = 2; if (vol <= 0) mode = 0;
    if (mode == S.mode && vol == S.vol) return;
    const bool was = S.mode != 0; S.mode = mode; S.vol = vol;
    if (mode > 0) {
        init(resdir, logpath, wavpath, test);
        if (!was && S.ch) { reset_voices(); ((MixerChannel *)S.ch)->Enable(true); }
    } else {
        reset_voices();
        if (S.ch) ((MixerChannel *)S.ch)->Enable(false);
    }
}
#endif   /* DSN_NO_MIXER */
}   /* namespace dsn */
