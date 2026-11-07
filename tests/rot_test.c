/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 the Rheia authors */
/* ROT (Rheia, fx.c rot_process: tape wear on the master) on test tones, and its place in a project:
 *   off        ROT 0: the mix bit for bit, nothing runs
 *   wow        ROT 127: a 1 kHz tone's pitch sways (period-to-period), more than at ROT 40; both more than none
 *   loss       ROT 127: an 8 kHz tone loses more than a 1 kHz one
 *   dropouts   ROT 127 over 20 s: the level sinks now and then; ROT 20: far less often
 *   bounded    a loud tone stays in range
 *   fade       ROT back to 0: the dry mix bit for bit again within 100 ms
 *   project    ROT kept with TEXTURE's MOVE (the bits above it), MOVE unchanged; a project before it: ROT 0
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o build/host/rot_test tests/rot_test.c -lm */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? i : 0; }
#include "../firmware/src/project.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("rot: %-92s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
#define SECS 20u
static int32_t L[FS * SECS], R[FS * SECS];
static void tone(double hz, int32_t amp, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) L[i] = R[i] = (int32_t)(amp * sin(2 * M_PI * hz * i / FS));
}
static void run(int rotv, uint32_t n)
{
    song.g[G_ROT] = (int16_t)rotv;
    for (uint32_t i = 0; i < n; i += CTL)
        rot_process(L + i, R + i, n - i < CTL ? n - i : CTL);
}
static double pitch_spread(uint32_t a, uint32_t b)            /* the zero-crossing periods' spread, relative */
{
    double last = -1, sum = 0, sum2 = 0;
    uint32_t cnt = 0;
    for (uint32_t i = a + 1; i < b; i++)
        if (L[i - 1] < 0 && L[i] >= 0) {
            double t = i - 1 + (double)-L[i - 1] / (L[i] - L[i - 1]);
            if (last >= 0) { double p = t - last; sum += p; sum2 += p * p; cnt++; }
            last = t;
        }
    if (cnt < 2) return 0;
    double m = sum / cnt;
    return sqrt(sum2 / cnt - m * m) / m;
}
static double rms(uint32_t a, uint32_t b)
{
    double e = 0;
    for (uint32_t i = a; i < b; i++) e += (double)L[i] * L[i];
    return sqrt(e / (b - a));
}
static uint32_t dips(uint32_t a, uint32_t b, double ref)      /* 10 ms windows under 75 % of ref, counted once a dip */
{
    uint32_t n = 0, in = 0, w = FS / 100u;
    for (uint32_t i = a; i + w <= b; i += w) {
        int low = rms(i, i + w) < 0.75 * ref;
        if (low && !in) n++;
        in = low;
    }
    return n;
}
int main(void)
{
    uint32_t n = FS * 2u, i;
    double s0, s40, s127, r1, r8, ref;
    char what[200];
    host_tracks_init();

    tone(1000, 20000, n);
    {
        static int32_t keep[FS * 2u];
        memcpy(keep, L, n * 4u);
        run(0, n);
        check(!memcmp(keep, L, n * 4u), "ROT 0: the mix bit for bit");
    }
    tone(1000, 20000, n); s0 = pitch_spread(FS / 2u, n);
    tone(1000, 20000, n); run(40, n); s40 = pitch_spread(FS / 2u, n); run(0, FS / 10u);
    memset(&rot, 0, sizeof rot); rot.rnd = 0x51A3C0DE; rot.g = 32767;
    tone(1000, 20000, n); run(127, n); s127 = pitch_spread(FS / 2u, n);
    snprintf(what, sizeof what, "wow and flutter: the period's spread %.4f at 127, %.4f at 40, %.4f dry", s127, s40, s0);
    check(s127 > s40 * 1.5 && s40 > s0 * 5 && s127 > 0.002, what);

    memset(&rot, 0, sizeof rot); rot.rnd = 0x51A3C0DE; rot.g = 32767;
    song.g[G_ROT] = 0;
    tone(1000, 20000, n); run(127, n); r1 = rms(FS / 2u, n);
    memset(&rot, 0, sizeof rot); rot.rnd = 0x51A3C0DE; rot.g = 32767;
    tone(8000, 20000, n); run(127, n); r8 = rms(FS / 2u, n);
    snprintf(what, sizeof what, "tape loss: 8 kHz down %.1f dB, 1 kHz %.1f dB", 20 * log10(r8 / (20000 / sqrt(2))),
             20 * log10(r1 / (20000 / sqrt(2))));
    check(r8 < r1 * 0.6, what);

    {   /* dropouts */
        uint32_t d127, d20, all = FS * SECS;
        memset(&rot, 0, sizeof rot); rot.rnd = 0x51A3C0DE; rot.g = 32767;
        tone(300, 16000, all); ref = 16000 / sqrt(2);
        run(127, all); d127 = dips(FS / 10u, all, ref);
        memset(&rot, 0, sizeof rot); rot.rnd = 0x51A3C0DE; rot.g = 32767;
        tone(300, 16000, all);
        run(20, all); d20 = dips(FS / 10u, all, ref);
        snprintf(what, sizeof what, "dropouts in %u s: %u at ROT 127, %u at ROT 20", SECS, d127, d20);
        check(d127 >= 5 && d20 * 4 <= d127, what);
    }

    {   /* bounded */
        int32_t pk = 0;
        memset(&rot, 0, sizeof rot); rot.rnd = 0x51A3C0DE; rot.g = 32767;
        tone(200, 120000, n); run(127, n);
        for (i = 0; i < n; i++) { int32_t a = L[i] < 0 ? -L[i] : L[i]; if (a > pk) pk = a; }
        snprintf(what, sizeof what, "a loud tone (120000) stays in range (peak %d)", pk);
        check(pk <= 131072, what);
    }

    {   /* fade back to dry */
        static int32_t keep[FS * 2u];
        uint32_t first_exact = n;
        memset(&rot, 0, sizeof rot); rot.rnd = 0x51A3C0DE; rot.g = 32767;
        tone(1000, 20000, n); run(127, FS / 2u);
        tone(1000, 20000, n); memcpy(keep, L, n * 4u);
        run(0, n);
        for (i = n; i-- > 0;) if (L[i] != keep[i]) { first_exact = i + 1; break; }
        snprintf(what, sizeof what, "ROT back to 0: dry bit for bit after %.0f ms", 1000.0 * first_exact / FS);
        check(first_exact < FS / 10u && !rot.mix, what);
    }

    {   /* the project */
        static project_t pj;
        song.g[G_TXMOVE] = 77; song.g[G_ROT] = 99; song.g[G_TXSRC] = 2;
        proj_tx_put(&pj);
        song.g[G_TXMOVE] = 0; song.g[G_ROT] = 0;
        proj_tx_get(&pj);
        check(song.g[G_ROT] == 99 && song.g[G_TXMOVE] == 77, "project: ROT kept above TEXTURE's MOVE, MOVE as it was");
        pj.rsv[0] = 0; pj.g[G_INFO] = 50;                     /* (a project from before TEXTURE) */
        song.g[G_ROT] = 64;
        proj_tx_get(&pj);
        check(song.g[G_ROT] == 0, "project: an older one (no TEXTURE mark): ROT 0");
        pj.rsv[0] = 0xC0; pj.g[G_INFO] = 40;                  /* (2.7's: MOVE alone) */
        proj_tx_get(&pj);
        check(song.g[G_ROT] == 0 && song.g[G_TXMOVE] == 40, "project: SLOOP 2.7's (MOVE alone): ROT 0, MOVE 40");
    }
    printf(fails ? "rot: %d FAILED\n" : "rot: all passed\n", fails);
    return fails;
}
