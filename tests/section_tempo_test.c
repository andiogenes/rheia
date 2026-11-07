/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 the Rheia authors */
/* Section tempo (Rheia: arranger.h arr_tempo, arranger.c sec_tempo_*), with the real sequencer and scene loader:
 *   song       A x1 for 2 bars, B x2 for 2 bars, A again: 120, 240 when B starts, 120 back on A; the song's bars take
 *              their time at each tempo (B's two bars half as long); STOP: 120
 *   half       B x1/2 at 120: 60, its bars twice as long
 *   song mode  STOP brings the loop back as before PLAY (SLOOP's restore), its BPM too
 *   live       SAVE + key (no restore): B x2 on the next bar; a BPM turned during it is the new base after STOP;
 *              x2 of 150 stops at 240, and STOP still brings 150 back
 *   storage    each section's code in arrangement.reserved[0], 2 bits; zero (SLOOP's) is x1 everywhere
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o build/host/section_tempo_test tests/section_tempo_test.c -lm */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? i : 0; }
#include "../firmware/src/project.c"
static struct { uint8_t force; } ui;
static uint8_t sync_reload;
#include "../firmware/src/arranger_scene.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("tempo: %-90s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static uint32_t frames_now;
static void blocks(uint32_t n)
{
    static int32_t out[CTL * 2];
    while (n--) {
        mix_block(out, CTL);
        frames_now += CTL;
    }
}
/* run until the song reaches entry `index` (or stops): the frame it happened at */
static uint32_t until_entry(uint32_t index, uint32_t limit)
{
    uint32_t start = frames_now;
    while (frames_now - start < limit && song.playing && arrangement_clock.index != index)
        blocks(1);
    return frames_now;
}
static void song_setup(uint32_t tempo_b)
{
    uint32_t i, k;
    host_tracks_init();
    song.g[G_BPM] = 120;
    for (i = 0; i < NTRK; i++) {
        trk[i].p[P_SLEN] = 16;
        trk[i].p[P_SDIV] = 2;
    }
    for (k = 0; k < 16; k++) {
        uint8_t drum[] = {k % 4u ? 42 : 36};
        put_step(TDRUM, k, 1, drum, ST_NOTE, 0);
    }
    proj_capture(&proj_slot[0]);
    proj_capture(&proj_slot[1]);
    arr_defaults(&arrangement);
    arrangement.count = 3;
    arrangement.entry[0] = (arr_entry_t){0, 2};
    arrangement.entry[1] = (arr_entry_t){1, 2};
    arrangement.entry[2] = (arr_entry_t){0, 1};
    arrangement.loop = 0;
    arr_tempo_set(&arrangement, 1, tempo_b);
    arrangement_enabled = 1;
    sec_mul2 = 2;
    frames_now = 0;
}

int main(void)
{
    uint32_t t_b, t_a2, bar120 = FS * 2u;              /* a 4/4 bar at 120: 2 s */
    char what[200];

    /* storage */
    arr_defaults(&arrangement);
    check(arrangement.reserved[0] == 0 && arr_tempo(&arrangement, 0) == 2u && arr_tempo(&arrangement, 3) == 2u,
          "defaults (and SLOOP's zero byte): every section x1");
    arr_tempo_set(&arrangement, 2, ARR_TDOUBLE);
    arr_tempo_set(&arrangement, 3, ARR_THALF);
    check(arr_tempo(&arrangement, 2) == 4u && arr_tempo(&arrangement, 3) == 1u && arr_tempo(&arrangement, 1) == 2u &&
          arrangement.reserved[0] == (ARR_TDOUBLE << 4 | ARR_THALF << 6), "2 bits a section: C x2, D x1/2, the others x1");
    arrangement.reserved[0] = 0xFF;
    check(arr_tempo(&arrangement, 0) == 2u && arr_tempo_code(&arrangement, 0) == ARR_T1, "code 3 (not written): x1");
    check(arr_valid(&arrangement, 15u) == arr_valid(&(arr_config_t){4, 0, {0, 0}, {{0, 4}, {1, 4}, {2, 4}, {3, 4}}}, 15u),
          "the tempo byte does not change whether a song is valid");

    /* song: A x1, B x2, A */
    song_setup(ARR_TDOUBLE);
    transport_req = 1;
    blocks(2);
    check(song.playing && song.g[G_BPM] == 120, "song starts on A x1: 120");
    t_b = until_entry(1, FS * 10u);
    blocks(2);
    snprintf(what, sizeof what, "B x2 starts after A's 2 bars at 120 (%.2f s, want 4.00): 240", (double)t_b / FS);
    check(song.g[G_BPM] == 240 && t_b > 2u * bar120 - FS / 10u && t_b < 2u * bar120 + FS / 10u, what);
    t_a2 = until_entry(2, FS * 10u);
    blocks(2);
    snprintf(what, sizeof what, "B's 2 bars at 240 take half the time (%.2f s, want 2.00); A again: 120", (double)(t_a2 - t_b) / FS);
    check(song.g[G_BPM] == 120 && t_a2 - t_b > bar120 - FS / 10u && t_a2 - t_b < bar120 + FS / 10u, what);
    transport_req = 2;
    blocks(2);
    check(!song.playing && song.g[G_BPM] == 120, "STOP: 120");

    /* half */
    song_setup(ARR_THALF);
    transport_req = 1;
    blocks(2);
    t_b = until_entry(1, FS * 10u);
    blocks(2);
    check(song.g[G_BPM] == 60, "B x1/2 at 120: 60");
    t_a2 = until_entry(2, FS * 20u);
    snprintf(what, sizeof what, "B's 2 bars at 60 take twice as long (%.2f s, want 8.00)", (double)(t_a2 - t_b) / FS);
    check(t_a2 - t_b > 4u * bar120 - FS / 10u && t_a2 - t_b < 4u * bar120 + FS / 10u, what);
    transport_req = 2;
    blocks(2);
    check(song.g[G_BPM] == 120, "STOP after a x1/2 section: 120");

    /* song mode: STOP brings the loop back as it was before PLAY (SLOOP), its BPM too, whatever was turned */
    song_setup(ARR_TDOUBLE);
    transport_req = 1;
    blocks(2);
    until_entry(1, FS * 10u);
    blocks(2);
    song.g[G_BPM] = 200;
    blocks(4);
    transport_req = 2;
    blocks(2);
    check(song.g[G_BPM] == 120, "song mode: the BPM turned during B x2, STOP: the loop's own 120 (SLOOP's restore)");

    /* live sections (loop mode, SAVE + key: no restore): B x2, the BPM turned to 200 meanwhile: the base is 100 after */
    song_setup(ARR_TDOUBLE);
    arrangement_enabled = 0;
    transport_req = 1;
    blocks(2);
    live_req = 1;                                         /* (SAVE + key 2: B on the next bar) */
    blocks(FS * 3u / CTL);
    check(song.g[G_BPM] == 240, "live: B x2 on the next bar: 240");
    song.g[G_BPM] = 200;
    blocks(4);
    transport_req = 2;
    blocks(2);
    check(song.g[G_BPM] == 100, "live: the BPM turned to 200 during B x2, STOP: the base is 100");

    /* clamp, live: x2 of 150 stops at 240, the base stays 150 */
    song_setup(ARR_TDOUBLE);
    arrangement_enabled = 0;
    song.g[G_BPM] = 150;
    transport_req = 1;
    blocks(2);
    live_req = 1;
    blocks(FS * 3u / CTL);
    check(song.g[G_BPM] == 240, "live: x2 of 150: 240 (the top of BPM)");
    transport_req = 2;
    blocks(2);
    check(song.g[G_BPM] == 150, "live: STOP: 150 again, not 120");

    printf(fails ? "tempo: %d FAILED\n" : "tempo: all passed\n", fails);
    return fails;
}
