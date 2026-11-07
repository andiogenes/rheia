/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 the Rheia authors */
/* WALL (fx.c ins_wall, the INSERT types BUZZ SLAB SHRED) on test signals through the INSERT's own entry (ins_run):
 *   bounded      a loud saw at full DRIVE stays inside a voice's range, no DC
 *   level        each voicing's output within a range of the input's (level-matched, as the drives)
 *   character    BUZZ brighter than SLAB (energy above 2 kHz), SLAB's even harmonics, SHRED's octave up
 *   gate         a note then a noise floor: GATE silences the floor in a few ms, GATE 0 lets it through, the note
 *                itself passes; it opens again on the next note
 *   silence      silence in, silence out (no bias left by SLAB's offset)
 *   dry          MIX 0 or TYPE OFF: the input bit for bit
 *   cost         time a sample, relative to FUZZ (printed; a note, not a failure, as the regression suite's timed CPU)
 * Writes build/host/wall-{buzz,slab,shred}.wav: a power-chord riff with a stop, each voicing, to listen to.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o build/host/wall_test tests/wall_test.c -lm */
#include <stdint.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(int ok, const char *what)
{
    printf("wall: %-84s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
#define N (FS / 2u)                                       /* half a second */
static int32_t in[N], out[N];
static int16_t prm[P_COUNT];
static ins_t st;

static void run(uint32_t type, int a, int b, int c, int mix, const int32_t *x, int32_t *y, uint32_t n)
{
    uint32_t i;
    memset(&st, 0, sizeof st);
    prm[P_ITYPE] = (int16_t)type, prm[P_IA] = (int16_t)a, prm[P_IB] = (int16_t)b, prm[P_IC] = (int16_t)c;
    prm[P_IMIX] = (int16_t)mix;
    for (i = 0; i < n; i += CTL) {
        uint32_t m = n - i < CTL ? n - i : CTL;
        memcpy(y + i, x + i, m * 4u);
        ins_run(&st, prm, y + i, m);
    }
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double e = 0;
    for (uint32_t i = a; i < b; i++) e += (double)x[i] * x[i];
    return sqrt(e / (double)(b - a));
}
static double tone(const int32_t *x, uint32_t a, uint32_t b, double hz)   /* Goertzel power */
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s1 = 0, s2 = 0, s0;
    for (uint32_t i = a; i < b; i++) s0 = x[i] + c * s1 - s2, s2 = s1, s1 = s0;
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
static double high_share(const int32_t *x, uint32_t a, uint32_t b)       /* energy above ~2 kHz / all */
{
    double e = 0, h = 0, lp = 0;
    for (uint32_t i = a; i < b; i++) {
        lp += (x[i] - lp) * 0.25;                       /* one-pole low-pass at ~2 kHz */
        e += (double)x[i] * x[i];
        h += (x[i] - lp) * (x[i] - lp);
    }
    return e > 0 ? h / e : 0;
}
static void sine(int32_t *x, uint32_t n, double hz, double amp)
{
    for (uint32_t i = 0; i < n; i++) x[i] = (int32_t)(amp * sin(2 * M_PI * hz * i / FS));
}
static void saw(int32_t *x, uint32_t n, double hz, double amp)
{
    for (uint32_t i = 0; i < n; i++) { double p = fmod(hz * i / FS, 1.0); x[i] = (int32_t)(amp * (2 * p - 1)); }
}
static void wav(const char *path, const int32_t *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    wav_hdr(f, n);
    for (uint32_t i = 0; i < n; i++) wav_put(f, x[i] >> 1, x[i] >> 1);
    fclose(f);
}

int main(int argc, char **argv)
{
    static const char *const NAME[3] = {"BUZZ", "SLAB", "SHRED"};
    const char *dir = argc > 1 ? argv[1] : "build/host";
    double hi[3], even[3], oct[3];
    uint32_t t, i;
    char what[160];
    for (i = 0; i < P_COUNT; i++) prm[i] = TP[i].def;

    /* bounded, no DC, level-matched: a loud 110 Hz saw at full DRIVE, and a 110 Hz sine at the default */
    for (t = 0; t < 3u; t++) {
        int32_t peak = 0;
        double mean = 0, rin, rout;
        saw(in, N, 110, 30000);
        run(IT_BUZZ + t, 127, 127, 0, 127, in, out, N);
        for (i = N / 4u; i < N; i++) { int32_t a = out[i] < 0 ? -out[i] : out[i]; if (a > peak) peak = a; mean += out[i]; }
        mean /= (double)(N - N / 4u);
        snprintf(what, sizeof what, "%s: a loud saw at full DRIVE stays in range (peak %d) and has no DC (%.0f)", NAME[t], peak, mean);
        check(peak < 32768 && fabs(mean) < 300, what);
        sine(in, N, 110, 12000);
        run(IT_BUZZ + t, 80, 92, 0, 127, in, out, N);
        rin = rms(in, N / 4u, N), rout = rms(out, N / 4u, N);
        snprintf(what, sizeof what, "%s: level-matched at the default DRIVE (out / in %.2f)", NAME[t], rout / rin);
        check(rout / rin > 0.4 && rout / rin < 1.6, what);
        {   /* the brightness on a saw (a sine has nothing above the pre-EQ's corner for it to push) */
            static int32_t sw[N];
            saw(in, N, 110, 12000);
            run(IT_BUZZ + t, 80, 92, 0, 127, in, sw, N);
            hi[t] = high_share(sw, N / 4u, N);
            sine(in, N, 110, 12000);
        }
        even[t] = tone(out, N / 4u, N, 220) / tone(out, N / 4u, N, 110);
        oct[t] = tone(out, N / 4u, N, 220);
    }
    snprintf(what, sizeof what, "BUZZ brighter than SLAB on a saw (energy above ~2 kHz: BUZZ %.3f, SLAB %.3f; SHRED %.3f)", hi[0], hi[1], hi[2]);
    check(hi[0] > hi[1] * 2, what);
    snprintf(what, sizeof what, "SLAB has even harmonics, BUZZ almost none (2nd / 1st: SLAB %.4f, BUZZ %.4f)", even[1], even[0]);
    check(even[1] > 0.01 && even[1] > even[0] * 4, what);
    snprintf(what, sizeof what, "SHRED: the octave up (220 Hz) strongest of the three (%.2g / %.2g / %.2g)", oct[2], oct[0], oct[1]);
    check(oct[2] > oct[0] && oct[2] > oct[1], what);

    /* the gate: 100 ms of a note, then 300 ms of a noise floor (amplitude 300), then the note again */
    for (t = 0; t < 3u; t++) {
        uint32_t s1 = FS / 10u, s2 = s1 + FS * 3u / 10u;
        uint32_t rnd = 12345;
        double floor_on, floor_off, note2;
        for (i = 0; i < N; i++) {
            rnd = rnd * 1103515245u + 12345u;
            in[i] = i < s1 || i >= s2 ? (int32_t)(12000 * sin(2 * M_PI * 110 * i / FS)) : (int32_t)((rnd >> 16) % 600u) - 300;
        }
        run(IT_BUZZ + t, 100, 92, 64, 127, in, out, N);
        floor_on = rms(out, s1 + FS / 20u, s2);         /* (from 50 ms after the stop) */
        note2 = rms(out, s2 + FS / 100u, N);
        run(IT_BUZZ + t, 100, 92, 0, 127, in, out, N);
        floor_off = rms(out, s1 + FS / 20u, s2);
        snprintf(what, sizeof what, "%s: GATE silences the floor 50 ms after a stop (%.1f; GATE 0: %.0f), the next note passes (%.0f)",
                 NAME[t], floor_on, floor_off, note2);
        check(floor_on < 5 && floor_off > 200 && note2 > 2000, what);
    }

    /* silence in, silence out; MIX 0 and TYPE OFF: bit for bit */
    for (t = 0; t < 3u; t++) {
        int ok = 1;
        memset(in, 0, sizeof in);
        run(IT_BUZZ + t, 127, 127, 0, 127, in, out, N);
        for (i = 0; i < N; i++) ok &= out[i] == 0;
        snprintf(what, sizeof what, "%s: silence in, silence out", NAME[t]);
        check(ok, what);
        saw(in, N, 82.4, 15000);
        run(IT_BUZZ + t, 127, 127, 64, 0, in, out, N);
        ok = !memcmp(in, out, sizeof in);
        snprintf(what, sizeof what, "%s: MIX 0, the input bit for bit", NAME[t]);
        check(ok, what);
    }
    saw(in, N, 82.4, 15000);
    run(IT_OFF, 127, 127, 64, 127, in, out, N);
    check(!memcmp(in, out, sizeof in), "TYPE OFF: the input bit for bit");
    {   /* a type change while running fades the old one out and the new one in: no jump bigger than the signal's */
        int32_t big = 0;
        saw(in, N, 110, 15000);
        memset(&st, 0, sizeof st);
        prm[P_IA] = 100, prm[P_IB] = 92, prm[P_IC] = 0, prm[P_IMIX] = 127;
        for (i = 0; i < N; i += CTL) {
            uint32_t m = N - i < CTL ? N - i : CTL;
            prm[P_ITYPE] = (int16_t)(i < N / 2u ? IT_BUZZ : IT_SLAB);
            memcpy(out + i, in + i, m * 4u);
            ins_run(&st, prm, out + i, m);
        }
        for (i = 1; i < N; i++) { int32_t d = out[i] - out[i - 1]; if (d < 0) d = -d; if (d > big) big = d; }
        snprintf(what, sizeof what, "BUZZ -> SLAB while running: a fade, no jump past a voice's range (largest step %d)", big);
        check(big < 40000, what);
    }

    {   /* the cost: a WALL voicing against FUZZ, the same signal (timed: a note) */
        clock_t c0;
        double tf, tw;
        saw(in, N, 110, 20000);
        c0 = clock(); for (i = 0; i < 20u; i++) run(IT_FUZZ, 100, 92, 96, 127, in, out, N); tf = (double)(clock() - c0);
        c0 = clock(); for (i = 0; i < 20u; i++) run(IT_SLAB, 100, 92, 40, 127, in, out, N); tw = (double)(clock() - c0);
        printf("wall: cost: SLAB %.2f x FUZZ (timed on this computer; a note)\n", tf > 0 ? tw / tf : 0);
    }

    {   /* demos: a power-chord riff (E5 E5 G5 F#5, a stop, E5) on a saw pair, each voicing */
        static const double ROOT[6] = {82.41, 82.41, 98.0, 92.5, 0, 82.41};
        static int32_t riff[FS * 3u], y[FS * 3u];
        uint32_t n = FS * 3u, step = n / 6u;
        char path[512];
        for (i = 0; i < n; i++) {
            double r = ROOT[i / step], ph1 = fmod(r * i / FS, 1.0), ph2 = fmod(r * 1.4983 * i / FS, 1.0);
            riff[i] = r > 0 ? (int32_t)(7000 * (2 * ph1 - 1) + 6000 * (2 * ph2 - 1)) : (int32_t)((i * 2654435761u >> 22) % 300u) - 150;
        }
        for (t = 0; t < 3u; t++) {
            run(IT_BUZZ + t, 100, 96, 48, 127, riff, y, n);
            snprintf(path, sizeof path, "%s/wall-%s.wav", dir, t == 0 ? "buzz" : t == 1 ? "slab" : "shred");
            wav(path, y, n);
        }
        snprintf(path, sizeof path, "%s/wall-dry.wav", dir);
        wav(path, riff, n);
        printf("wall: demos in %s/wall-*.wav\n", dir);
    }
    printf(fails ? "wall: %d FAILED\n" : "wall: all passed\n", fails);
    return fails;
}
