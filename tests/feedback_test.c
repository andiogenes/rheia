/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 the Rheia authors */
/* NOISE MODE FDBK (Rheia, eng_noise.c): a band-pass fed back into itself through a soft clip, through the real mixer:
 *   howl       FDBK 127: a steady tone at the filter's frequency (most of the energy in a few semitones), bounded
 *   ringing    FDBK 0: resonant noise, not a howl (far less of it at one pitch)
 *   keys       TRK 127: a note an octave up, the howl an octave up
 *   release    the note let go: silence, every voice free
 *   bite       BITE 127: more harmonics than BITE 0
 *   others     MODE 0..3 render as before (the golden renders cover them; here: the menu still has 5 modes)
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o build/host/feedback_test tests/feedback_test.c -lm */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(int ok, const char *what)
{
    printf("fdbk: %-92s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
#define NB (FS * 2u / CTL)                                     /* 2 s */
static int32_t outl[NB * CTL];
static void setup(int fdbk, int bite, int ktrk, int freq)
{
    uint32_t i;
    host_tracks_init();
    host_preset(&trk[0], ENGI_NOISE, 0);                       /* (the engine, then the values below) */
    for (i = 0; i < 8u; i++) trk[0].p[P_E0 + i] = ENGINES[ENGI_NOISE]->edit[i].def;
    trk[0].p[P_E0] = 4;                                        /* FDBK */
    trk[0].p[P_E1] = 30, trk[0].p[P_E2] = (int16_t)freq, trk[0].p[P_E3] = 110, trk[0].p[P_E4] = (int16_t)ktrk;
    trk[0].p[P_E5] = (int16_t)fdbk, trk[0].p[P_E6] = 0, trk[0].p[P_E7] = (int16_t)bite;
    trk[0].p[P_ATK] = 0, trk[0].p[P_SUS] = 127, trk[0].p[P_REL] = 20;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}
static void render(uint32_t from, uint32_t nb)
{
    static int32_t o[2u * CTL];
    for (uint32_t b = 0; b < nb; b++) {
        mix_block(o, CTL);
        for (uint32_t i = 0; i < CTL; i++) outl[(from + b) * CTL + i] = o[2 * i];
    }
}
static double tone(uint32_t a, uint32_t b, double hz)
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s1 = 0, s2 = 0, s0;
    for (uint32_t i = a; i < b; i++) s0 = outl[i] + c * s1 - s2, s2 = s1, s1 = s0;
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double)(b - a);
}
static double energy(uint32_t a, uint32_t b)
{
    double e = 0;
    for (uint32_t i = a; i < b; i++) e += (double)outl[i] * outl[i];
    return e / (double)(b - a);
}
static double peak_hz(uint32_t a, uint32_t b, double *share)   /* the strongest pitch, 1/4 semitone steps 40 Hz .. 8 kHz */
{
    double best = 0, bh = 0, tot = 0;
    for (double st = 0; st < 12 * 7.6; st += 0.25) {
        double hz = 40 * pow(2, st / 12), p = tone(a, b, hz);
        tot += p;
        if (p > best) best = p, bh = hz;
    }
    *share = tot > 0 ? best * 48 / tot : 0;                    /* (48 bins an octave... ~ peak / mean of a semitone's) */
    return bh;
}
int main(void)
{
    uint32_t a = NB * CTL / 2u, b = NB * CTL, i;
    int32_t pk = 0;
    double sh_howl, sh_ring, hz1, hz2, e1, e2, sh, h_bite[2];
    char what[200];

    check(ENG_NOISE.edit[0].max == 4 && !strcmp(N_NOISE_MODE[4], "FDBK"), "MODE: ANLG DUST LFSR META FDBK");

    setup(127, 0, 0, 70);
    input_on(&trk[0], 60, 100);
    render(0, NB);
    for (i = a; i < b; i++) { int32_t x = outl[i] < 0 ? -outl[i] : outl[i]; if (x > pk) pk = x; }
    hz1 = peak_hz(a, b, &sh_howl);
    e1 = energy(a, a + (b - a) / 2), e2 = energy(a + (b - a) / 2, b);
    snprintf(what, sizeof what, "FDBK 127: a howl at %.0f Hz (peak share %.1f), steady (%.1f dB between halves), bounded (%d)",
             hz1, sh_howl, 10 * log10(e2 / e1), pk);
    check(sh_howl > 8 && fabs(10 * log10(e2 / e1)) < 2 && pk < 32768 && e1 > 1e6, what);
    input_off(&trk[0], 60);
    render(0, NB);
    {
        int idle = 1;
        for (i = 0; i < NVOICE; i++) idle &= !trk[0].v[i].active;
        snprintf(what, sizeof what, "let go: silence after the release (%.1f), every voice free", sqrt(energy(a, b)));
        check(energy(a, b) < 4 && idle, what);
    }

    setup(0, 0, 0, 70);
    input_on(&trk[0], 60, 100);
    render(0, NB);
    peak_hz(a, b, &sh_ring);
    snprintf(what, sizeof what, "FDBK 0: ringing noise, not a howl (peak share %.1f, the howl's %.1f)", sh_ring, sh_howl);
    check(sh_ring * 3 < sh_howl, what);

    setup(127, 0, 127, 60);
    input_on(&trk[0], 48, 100);
    render(0, NB);
    hz1 = peak_hz(a, b, &sh);
    input_off(&trk[0], 48);
    render(0, NB / 2u);
    setup(127, 0, 127, 60);
    input_on(&trk[0], 60, 100);
    render(0, NB);
    hz2 = peak_hz(a, b, &sh);
    snprintf(what, sizeof what, "TRK 127: C3 howls at %.0f Hz, C4 at %.0f Hz (x%.2f, want about 2)", hz1, hz2, hz2 / hz1);
    check(hz2 / hz1 > 1.8 && hz2 / hz1 < 2.2, what);

    for (int k = 0; k < 2; k++) {                              /* BITE: the howl's harmonics */
        setup(127, k ? 127 : 0, 0, 55);
        input_on(&trk[0], 60, 100);
        render(0, NB);
        double f0 = peak_hz(a, b, &sh);
        h_bite[k] = (tone(a, b, f0 * 3) + tone(a, b, f0 * 5)) / tone(a, b, f0);
    }
    snprintf(what, sizeof what, "BITE 127: more harmonics (3rd + 5th / 1st: %.4f, BITE 0 %.4f)", h_bite[1], h_bite[0]);
    check(h_bite[1] > h_bite[0] * 2, what);

    printf(fails ? "fdbk: %d FAILED\n" : "fdbk: all passed\n", fails);
    return fails;
}
