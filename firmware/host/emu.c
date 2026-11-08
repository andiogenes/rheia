/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 the Rheia authors. Based on SLOOP's browser preview (firmware/host/preview.c) and its UI test
 * harness (tests/ui_pages_test.c). */
/* Rheia emulator: the whole firmware that runs on the FM-1's screen and panel, compiled for the browser
 * (WebAssembly, tools/build_emu.py). The sound, the sequencer and every screen are the firmware's own sources; the
 * hardware is replaced by RAM:
 *   the screen    a 240 x 240 RGB565 framebuffer (emu_screen), drawn by gfx.c as on the FM-1
 *   the panel     the 14 buttons, the 7 knobs and the 27 keys, set by the page (emu_button, emu_enc, emu_key)
 *   the audio     emu_render: one block of CTL stereo frames, float, as the audio ISR mixes them
 *   the flash     none: projects, presets and settings live until the page is closed
 * MIDI, USB and the update path are not built. */
#include <stdint.h>
#include <string.h>
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "../src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include "felucca_tables.h"
static uint8_t user_flash[4][0x14000];
#define SMP_USER_XIP(k) (user_flash[(k) & 3u])
static struct { volatile uint32_t notes, buttons; } fm1_in;
static volatile uint32_t fm1_remote_btn;
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
#define FELUCCA_ARRANGER 1
#include "../src/core.h"
#include "../src/engines.c"
#include "../src/drums.c"
#include "../src/params.c"
#include "../src/voice.c"
#include "../src/slicer.c"
#include "../src/cpu1.h"                        /* no second core in the browser: every unit renders here */
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn; (void)arg; return 0; }
static int cpu1_wait(void) { return 0; }
static int cpu1_on(void) { return 0; }
static uint32_t cpu1_ticks(void) { return 0; }
#include "../src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../src/usb.c"
#include "../src/midi_uart.c"
#include "../src/arranger.c"
#include "../src/seq.c"

/* ---- the screen */
static uint16_t screen[240 * 240];                /* RGB565 as the LCD takes it (byte-swapped: gfx.c swap16) */
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    if (x + w > 240u || y + h > 240u)
        return;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            screen[(y + j) * 240u + x + i] = p[j * w + i];
}
#include "../src/gfx.c"
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    if (x + w > 240u || y + h > 240u)
        return;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            screen[(y + j) * 240u + x + i] = swap16(c);
}

/* ---- the panel */
static int32_t encs[7];                           /* detents waiting, by knob id (panel.enc) */
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t fm1_enc_take(uint32_t e) { int32_t s = e < 7u ? encs[e] : 0; if (e < 7u) encs[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
/* the light matrix (ui_input.c led_pos): here simply id = (row - 1) x 16 + column, ids 0..40 (14 buttons, 27 keys) */
#define EK(r, c) ((r) * 16 + (c) < 41 ? (int8_t)((r) * 16 + (c)) : (int8_t)-1)
#define EROW(r) {EK(r, 0), EK(r, 1), EK(r, 2), EK(r, 3), EK(r, 4), EK(r, 5), EK(r, 6), EK(r, 7), EK(r, 8), EK(r, 9), \
                 EK(r, 10), EK(r, 11), EK(r, 12), EK(r, 13), EK(r, 14), EK(r, 15)}
static const int8_t FM1_KEYMAP[5][16] = {{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
                                         EROW(0), EROW(1), EROW(2), EROW(3)};
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static void fm1_wdt_feed(void) {}
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../src/panel.c"
#include "../src/ui.c"
#include "../src/beatgen.c"

/* ---- storage: none (one project in RAM; the SAVE pages say so) */
static uint8_t sec_used;                          /* sections A..D stored (bits): the song's own copies */
static track_t sec_trk[4][NTRK];
static int project_used(uint32_t i) { return i < 4u && ((sec_used >> i) & 1u); }
static void project_save(uint32_t i) { (void)i; ui_message("NO FLASH IN THE EMULATOR"); }
static void project_load(uint32_t i) { (void)i; }
static void project_erase(uint32_t i) { if (i < 4u) sec_used &= (uint8_t)~(1u << i); ui_message("ERASED"); }
static void arrangement_save(void) {}
static uint32_t arrangement_ready(void) { return sec_used; }
static void arrangement_apply(uint32_t s) { (void)s; }
static uint32_t section_bars(uint32_t s) { (void)s; return 1u; }
static void song_backup(void) {}
static void song_restore(void) {}
static void section_store(uint32_t s)
{
    if (s >= 4u)
        return;
    memcpy(sec_trk[s], trk, sizeof trk);
    sec_used |= (uint8_t)(1u << s);
    live_sec = (int8_t)s;
}
static void section_load(uint32_t s)
{
    uint32_t k;
    if (s >= 4u || !((sec_used >> s) & 1u))
        return;
    fm1_irq_off();
    for (k = 0; k < NTRK; k++) {                  /* the patterns and lengths; the sounds stay */
        memcpy(trk[k].step, sec_trk[s][k].step, sizeof trk[k].step);
        memcpy(trk[k].micro, sec_trk[s][k].micro, sizeof trk[k].micro);
        memcpy(trk[k].fill, sec_trk[s][k].fill, sizeof trk[k].fill);
        trk[k].p[P_SLEN] = sec_trk[s][k].p[P_SLEN];
        trk[k].p[P_SDIV] = sec_trk[s][k].p[P_SDIV];
    }
    fm1_irq_on();
    live_sec = (int8_t)s;
}
static int up_used(uint32_t k) { (void)k; return 0; }
static int up_load(uint32_t k) { (void)k; return 0; }
static int up_load_to(track_t *t, uint32_t k) { (void)t; (void)k; return 0; }
static int up_arp_of(uint32_t k, int16_t *a) { (void)k; (void)a; return 0; }
static uint32_t up_count(void) { return 0; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t s) { return s; }
static void up_name(uint32_t k, char *b) { (void)k; str_cpy(b, "", 13); }
static void up_slot_label(char *b, uint32_t k) { fmt_int(b, (int32_t)k + 1); }
static void up_ui(uint32_t op, uint32_t k) { (void)op; (void)k; ui_message("NO FLASH IN THE EMULATOR"); }
static void settings_save(void) {}
#include "../src/ui_song.c"
#include "../src/ui_studio.c"
#include "../src/icons.c"
#include "../src/ui_draw.c"
#include "../src/ui_vis.c"
#include "../src/ui_layers.c"
#include "../src/ui_menu.c"
#include "../src/ui_input.c"
#include "../src/splash.c"
static int st_read(uint32_t off, void *dst, uint32_t n) { (void)off, (void)dst, (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { (void)off, (void)src, (void)n; return -1; }
#include "../src/storage.c"

/* ---- the page's side */
static int32_t block[CTL * 2];
static float output[CTL * 2];
static uint32_t ms_acc;                           /* audio samples since the last UI frame */

/* power-on, as main.c felucca_init without the flash: the three default sounds, the 808 kit, empty patterns */
void emu_init(void)
{
    uint32_t i;
    memset(user_flash, 0xFF, sizeof user_flash);
    panel = PANEL_DEFAULT;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    fm6_init();
    dsu_defaults();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    song.sel = 0;
    song.master_q12 = 2048;
    song.g[G_SYNC] = 0;
    layers_init();
    settings_init();                              /* (Rheia) the palette: emu_palette's, or a new FM-1's (ASH) */
    sloop_splash();
    go_home();
    ui.force = 1;
}
/* the controls, by role (panel.c B_*, EN_*; keys 0..26 from the lowest) */
void emu_button(uint32_t b, uint32_t down)
{
    uint32_t bit;
    if (b >= NB)
        return;
    bit = 1u << panel.btn[b];
    if (down) {
        if (!(fm1_in.buttons & bit))
            edges_btn |= bit;
        fm1_in.buttons |= bit;
    } else {
        fm1_in.buttons &= ~bit;
    }
}
void emu_enc(uint32_t e, int32_t detents)
{
    if (e < NE)
        encs[panel.enc[e]] += detents * panel.dir[e];
}
void emu_key(uint32_t k, uint32_t down)
{
    if (k < 27u)
        fm1_in.notes = down ? fm1_in.notes | 1u << k : fm1_in.notes & ~(1u << k);
}
/* one UI pass (input, lights, screen), as main.c's loop; ms: the time since the last one */
void emu_frame(uint32_t ms)
{
    fm1_ms += ms;
    ui_input();
    ui_leds();
    ui_draw();
}
uint16_t *emu_screen(void) { return screen; }
/* (Rheia) the screen's palette (gfx.c PALETTES; the page's theme picker): before emu_init, the boot screen's too */
void emu_palette(uint32_t i)
{
    if (i >= NPALETTES)
        return;
    settings.magic = SETTINGS_MAGIC;
    settings.palette = (uint8_t)i;
    theme_on = 0;
    palette_set(i);
    ui.force = 1;
}
/* a light by matrix id (buttons: emu_btn_id; key k: 14 + k): 0 off, 1 background, 2 dim, 3 lit */
uint32_t emu_led(uint32_t id)
{
    uint32_t c, r;
    if (id >= 41u)
        return 0;
    c = id % 16u, r = id / 16u + 1u;
    return (fm1_led[c] >> r) & 1u ? 3u : (fm1_led_dim[c] >> r) & 1u ? 2u : (fm1_led_bg[c] >> r) & 1u ? 1u : 0u;
}
uint32_t emu_frames(void) { return CTL; }
uint32_t emu_rate(void) { return FS; }
uint32_t emu_btn_id(uint32_t b) { return b < NB ? panel.btn[b] : 0xFFu; }
/* one audio block: CTL stereo frames, interleaved, -1..1 */
float *emu_render(void)
{
    uint32_t i;
    fm6_poll();
    mix_block(block, CTL);
    for (i = 1; i < CTL; i += 2u) {               /* the scope, as audio.c fills it (the visualiser) */
        scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
    }
    for (i = 0; i < CTL * 2u; i++)
        output[i] = (float)clamp(block[i], -32768, 32767) / 32768.0f;
    ms_acc += CTL;
    return output;
}
