/* SPDX-License-Identifier: GPL-3.0-only */
/* Firmware adapter. Scene snapshots are already resident in RAM. No flash
 * operation, UI draw or allocation is allowed from these audio callbacks. */
#include "arranger.h"
static arr_config_t arrangement;
static arr_clock_t arrangement_clock;
static uint8_t arrangement_enabled;
static uint32_t arrangement_ready(void);              /* project.c */
static void arrangement_apply(uint32_t scene);         /* project.c */

static int arrangement_start(void)
{
    int scene;
    if (!arrangement_enabled) return 1;
    scene = arr_begin(&arrangement_clock, &arrangement, arrangement_ready());
    if (scene < 0) return 0;
    arrangement_apply((uint32_t)scene);
    song.rec = 0;                                    /* song playback does not overwrite patterns */
    return 1;
}

/* (Rheia) a section's tempo (arranger.h arr_tempo): while it plays the song's BPM is the base times it; stopping brings
 * the base back. A BPM turned meanwhile becomes the new base (the section's factor taken out). In the audio ISR (a
 * section starting on its bar) and at the stop */
static uint8_t sec_mul2 = 2;                      /* the factor in force, x2: 1 (x1/2), 2, 4 (x2) */
static int16_t sec_base;                          /* the BPM it multiplies (valid while sec_mul2 != 2) */
static int16_t sec_bpm(int32_t base, uint32_t mul2)
{
    return (int16_t)clamp(base * (int32_t)mul2 / 2, GP[G_BPM].min, GP[G_BPM].max);
}
static void sec_tempo_base(void)                 /* the base now (a BPM turned while a factor ran: from it) */
{
    if (sec_mul2 == 2u)
        sec_base = song.g[G_BPM];
    else if (song.g[G_BPM] != sec_bpm(sec_base, sec_mul2))
        sec_base = (int16_t)clamp(song.g[G_BPM] * 2 / (int32_t)sec_mul2, GP[G_BPM].min, GP[G_BPM].max);
}
static void sec_tempo_enter(uint32_t scene)
{
    uint32_t m = arr_tempo(&arrangement, scene);
    sec_tempo_base();
    song.g[G_BPM] = sec_bpm(sec_base, m);
    sec_mul2 = (uint8_t)m;
}
static void sec_tempo_leave(void)
{
    if (sec_mul2 == 2u)
        return;
    sec_tempo_base();
    song.g[G_BPM] = sec_base;
    sec_mul2 = 2;
}
