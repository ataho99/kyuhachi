    /* ---- r017 #239 3번: 배속 중 소리 (gensei_ba_mode 1 = 빠르게(음높이 상승), 2 = 음높이 유지(타임스트레치)) ---- */
    {
        static ba::Eng * ba_eng = NULL; static bool ba_was = false; static int32_t ba_l[8192], ba_r[8192];
        bool ba_on = gensei_ba_mode != 0 && gensei_ba_xq > 105 && !mixer.mute && need > 0;
        if (!ba_on) { if (ba_was && ba_eng) ba_eng->reset(); ba_was = false; }
        else {
            if (!ba_eng) ba_eng = new ba::Eng;
            if (!ba_was) { ba_eng->reset(); ba_was = true; }
            Bitu wi = mixer.work_in; int fed = 0; int32_t * in0 = &mixer.work[mixer.work_out][0];
            while (mixer.work_out != wi) {
                const int32_t * q = &mixer.work[mixer.work_out][0];
                ba_eng->feed1(q[0], q[1]); fed++;
                mixer.work_out++; if (mixer.work_out >= mixer.work_wrap) mixer.work_out = 0;
            }
            (void)in0;
            int want = (int)need; if (want > 8192) want = 8192;
            ba_eng->note(fed, want, gensei_ba_xq / 100.0);
            int made = ba_eng->render(gensei_ba_mode, gensei_ba_xq / 100.0, want, ba_l, ba_r);
            int64_t gain = (int64_t)gensei_ba_gain;   /* 256 = 1.0 */
            for (int i = 0; i < made; i++) {
                int32_t a = MIXER_CLIP(((((int64_t)ba_l[i]) * (int64_t)volscale1) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT)) * gain >> 8);
                int32_t b = MIXER_CLIP(((((int64_t)ba_r[i]) * (int64_t)volscale2) >> (MIXER_VOLSHIFT + MIXER_VOLSHIFT)) * gain >> 8);
                *output++ = (int16_t)a; *output++ = (int16_t)b; gensei_ba_sumsq += (double)a * a + (double)b * b; gensei_ba_n += 2;
            }
            if (made > 0) { mixer.last_dac[0] = (output - 2)[0]; mixer.last_dac[1] = (output - 2)[1]; }
            need -= (Bitu)made;
            gensei_ba_calls++; if (made < want) gensei_ba_under++; gensei_ba_rho = ba_eng->last_rho; gensei_ba_drops = ba_eng->drops;
            while (need > 0) { *output++ = (int16_t)mixer.last_dac[0]; *output++ = (int16_t)mixer.last_dac[1]; need--; }
            return;
        }
    }
