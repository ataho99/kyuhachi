/* gensei_boostaud.h -- #239 3번: 배속 중 소리. DOSBox 에 의존하지 않는 순수 DSP (믹서 콜백/단위 시험 공용).
 * 입력 = 믹서가 에뮬 시간으로 만든 샘플(배속 x 이면 실시간의 x 배 속도로 쌓임). 출력 = 실시간 속도 샘플.
 *   mode 1 "빠르게"        : 구간 평균 리샘플 → 음높이가 x 배로 올라간다(테이프 빨리감기).
 *   mode 2 "음높이 유지"   : WSOLA 타임스트레치 → 음높이는 그대로, 길이만 1/x.
 * 실제 배속 비율은 x 에 못 미칠 수 있으므로(PC 성능) 입력이 들어오는 속도를 재서 따라간다. */
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>

namespace ba {

static const int CAP = 1 << 17;      /* 입력 FIFO 프레임 수 (약 3초) */
static const int OCAP = 1 << 14;     /* 출력 FIFO */
static const int H = 882;            /* 합성 hop (20 ms @44.1k) */
static const int N = 2 * H;          /* 창 길이 */
static const int DELTA = 300;        /* 정렬 탐색 ±(약 6.8 ms) */
static const int MEVAL = 441;        /* 상관 비교 길이 */

struct Eng {
    int32_t il[CAP], ir[CAP]; int in_n;
    int32_t ol[OCAP], orr[OCAP]; int out_n;
    double pos;                       /* mode 1: FIFO 안 소수 위치 */
    long long base;                   /* il[0] 의 절대 번호 (mode 2) */
    double nom;                       /* mode 2: 다음 분석 위치(절대) */
    long long nat;                    /* 직전 구간의 자연 연속 시작(절대), -1 = 없음 */
    float tl[H], tr[H];               /* 겹침 꼬리 */
    float win[N];
    double rate_ema; bool rate_init;
    int lead;                         /* 직전 render 후 FIFO 에 남은 프레임 */
    int cur_mode;
    unsigned steps, under, drops;     /* 계수(로그용) */
    double last_rho;

    Eng() { for (int i = 0; i < N; i++) win[i] = (float)(0.5 - 0.5 * cos(2.0 * 3.14159265358979323846 * (i + 0.5) / N)); reset(); }
    void reset() {
        in_n = 0; out_n = 0; pos = 0; base = 0; nom = 0; nat = -1; memset(tl, 0, sizeof tl); memset(tr, 0, sizeof tr);
        rate_ema = 0; rate_init = false; lead = 0; cur_mode = 0; steps = under = drops = 0; last_rho = 0;
    }
    /* 믹서 링에서 읽은 프레임을 넣는다(스칼라 단위로 반복 호출). FIFO 가 넘치면 오래된 것부터 버린다. */
    void feed1(int32_t a, int32_t b) {
        if (in_n >= CAP) { int cut = CAP / 4; memmove(il, il + cut, (size_t)(in_n - cut) * 4); memmove(ir, ir + cut, (size_t)(in_n - cut) * 4); in_n -= cut; base += cut; pos -= cut; if (pos < 0) pos = 0; drops++; }
        il[in_n] = a; ir[in_n] = b; in_n++;
    }
    void drop_front(int cut) {
        if (cut <= 0) return; if (cut > in_n) cut = in_n;
        memmove(il, il + cut, (size_t)(in_n - cut) * 4); memmove(ir, ir + cut, (size_t)(in_n - cut) * 4); in_n -= cut; base += cut; pos -= cut;
    }
    /* 이번 콜백에서 fed 프레임이 들어왔고 need 프레임을 내보내야 한다 → 입력/출력 비율 추정 갱신 */
    void note(int fed, int need, double nominal) {
        if (!rate_init) { rate_ema = nominal; rate_init = true; }
        if (need > 0 && fed > 0) { double s = (double)fed / need; if (s < 1.0) s = 1.0; if (s > 12.0) s = 12.0; rate_ema += 0.12 * (s - rate_ema); }
    }
    double rho_now(int need, int lead_target) const {
        double r = rate_ema; if (r < 1.0) r = 1.0;
        double err = (double)(lead - lead_target) / (need * r + 1.0);
        double c = 0.25 * err; if (c > 0.2) c = 0.2; if (c < -0.2) c = -0.2;
        double v = r * (1.0 + c); if (v < 1.0) v = 1.0; if (v > 12.0) v = 12.0; return v;
    }

    /* ---- mode 1: 구간 평균 리샘플 */
    int render1(double rho, int need, int32_t * outl, int32_t * outr) {
        int made = 0;
        while (made < need) {
            double e = pos + rho; int i0 = (int)floor(pos), i1 = (int)floor(e);
            if (i1 + 1 >= in_n) break;
            double sl = 0, sr = 0, w;
            for (int i = i0; i <= i1; i++) {
                double a = i > i0 ? (double)i : pos, b = (i < i1) ? (double)(i + 1) : e; w = b - a; if (w <= 0) continue;
                sl += il[i] * w; sr += ir[i] * w;
            }
            outl[made] = (int32_t)(sl / rho); outr[made] = (int32_t)(sr / rho); made++; pos = e;
        }
        int cut = (int)floor(pos) - 1; if (cut > 0) { drop_front(cut); }
        return made;
    }

    /* ---- mode 2: WSOLA */
    static float mono(const int32_t * a, const int32_t * b, long long i) { return (float)((double)a[i] + (double)b[i]) * 0.5f; }
    bool step(double rho) {
        const double Ha = H * rho;
        long long S; /* 선택한 구간의 절대 시작 */
        if (nat < 0) {   /* 첫 구간: 현재 FIFO 앞에서 시작 */
            nom = (double)base; S = base;
            if (in_n < N + 8) return false;
        } else {
            long long nm = (long long)floor(nom + 0.5);
            long long lo = nm - DELTA, hi = nm + DELTA;
            if (lo < base) lo = base;
            long long need_hi = (hi > nm ? hi : nm) + N, tmpl_hi = nat + MEVAL;
            long long last = base + in_n;
            if (need_hi > last || tmpl_hi > last) return false;
            if (hi < lo) hi = lo;
            /* 템플릿 = x[nat .. nat+MEVAL) , 후보 = x[c .. c+MEVAL) 의 정규화 상관 */
            double best = -1e30; long long bc = nm < lo ? lo : (nm > hi ? hi : nm);
            const int ST = 2;
            for (long long c = lo; c <= hi; c += 2) {
                double xy = 0, yy = 1e-9;
                for (int k = 0; k < MEVAL; k += ST) {
                    float t = mono(il, ir, nat - base + k), y = mono(il, ir, c - base + k);
                    xy += (double)t * y; yy += (double)y * y;
                }
                double sc = xy / sqrt(yy);
                if (sc > best) { best = sc; bc = c; }
            }
            S = bc;
        }
        /* 합성: 출력 블록 = 꼬리 + 새 구간 앞 절반 */
        if (out_n + H > OCAP) return false;
        long long o = S - base;
        for (int i = 0; i < H; i++) {
            float wl = win[i], wr = win[i + H];
            float a = (float)il[o + i] * wl, b = (float)ir[o + i] * wl;
            ol[out_n + i] = (int32_t)(tl[i] + a); orr[out_n + i] = (int32_t)(tr[i] + b);
            tl[i] = (float)il[o + H + i] * wr; tr[i] = (float)ir[o + H + i] * wr;
        }
        out_n += H; steps++;
        nat = S + H;
        if (nat == S + H && nom < (double)S) nom = (double)S;   /* 첫 구간 보정 */
        nom += Ha; last_rho = rho;
        { long long keep = (nat < (long long)floor(nom) - DELTA ? nat : (long long)floor(nom) - DELTA); if (keep > base) drop_front((int)(keep - base)); }
        return true;
    }
    int render2(double rho, int need, int32_t * outl, int32_t * outr) {
        while (out_n < need + H) { if (!step(rho)) break; }
        int n = out_n < need ? out_n : need;
        memcpy(outl, ol, (size_t)n * 4); memcpy(outr, orr, (size_t)n * 4);
        memmove(ol, ol + n, (size_t)(out_n - n) * 4); memmove(orr, orr + n, (size_t)(out_n - n) * 4); out_n -= n;
        return n;
    }

    /* mode: 1 빠르게, 2 음높이 유지. nominal = 설정 배율. 반환 = 만든 프레임 수(need 보다 적으면 호출자가 채움) */
    int render(int mode, double nominal, int need, int32_t * outl, int32_t * outr) {
        if (mode != cur_mode) { /* 모드 전환: 상태 초기화(FIFO 는 유지) */ nat = -1; pos = 0; out_n = 0; memset(tl, 0, sizeof tl); memset(tr, 0, sizeof tr); cur_mode = mode; }
        if (!rate_init) { rate_ema = nominal; rate_init = true; }
        int made;
        if (mode == 1) { double rho = rho_now(need, 512); last_rho = rho; made = render1(rho, need, outl, outr); lead = in_n - (int)floor(pos); }
        else {
            double r = rate_ema < 1.0 ? 1.0 : rate_ema; double e2 = (double)(out_n - H) / (double)(H + 1);   /* 출력 FIFO 가 목표(H 프레임)보다 많으면 입력을 더 빨리 먹는다 */
            double c = 0.3 * e2; if (c > 0.2) c = 0.2; if (c < -0.2) c = -0.2; double rho = r * (1.0 + c); if (rho < 1.0) rho = 1.0; if (rho > 12.0) rho = 12.0;
            made = render2(rho, need, outl, outr); lead = in_n;
        }
        if (made < need) under++;
        return made;
    }
};

} /* namespace ba */
