/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 the Rheia authors */
/* BEAT GEN (firmware/src/beatgen.c): the styles' grooves, the KICK / CYM / CRATE / FILL / ACCENT / HUMAN options,
 * the lengths, the same seed giving the same pattern, nothing written past the pattern, every value in range.
 *   cc -O1 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/hal -o build/host/beatgen_test tests/beatgen_test.c
 * VERBOSE=1: prints each style's first bar as a grid. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* the types it writes (core.h: dstep_t, NSTEP, LV_*), without the rest of the firmware */
#define NSTEP 64
enum { LV_NORM, LV_GHOST, LV_SOFT, LV_HARD };
typedef struct {
    uint8_t on[2];
    uint8_t lvl[4];
    uint8_t rat[4];
} dstep_t;
#define BEATGEN_PURE 1
#include "beatgen.c"

#define NELEM_(a) (sizeof(a) / sizeof((a)[0]))
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t lvl_of(const dstep_t *s, uint32_t l) { return (s->lvl[(l >> 2) & 3u] >> ((l & 3u) * 2u)) & 3u; }
static uint32_t rat_of(const dstep_t *s, uint32_t l) { return (s->rat[(l >> 2) & 3u] >> ((l & 3u) * 2u)) & 3u; }
static uint32_t mask_of(const dstep_t *s) { return (uint32_t)s->on[0] | (uint32_t)s->on[1] << 8; }
static uint32_t count(const bg_pat_t *p, uint32_t lane, uint32_t from, uint32_t to)
{
    uint32_t i, n = 0;
    for (i = from; i < to; i++)
        n += (uint32_t)bg_has(&p->st[i], lane);
    return n;
}
static bg_cfg_t plain(uint32_t style)          /* the style alone: no accent, no humanising, no fill */
{
    bg_cfg_t c = BG_DEFAULT;
    c.style = (uint8_t)style;
    c.accent = 0;
    c.human = 0;
    return c;
}
static void show(const char *name, const bg_pat_t *p)
{
    static const uint32_t lanes[] = {BG_L_CRASH, BG_L_RIDE, BG_L_OPEN, BG_L_HAT, BG_L_TOM_H, BG_L_TOM_L, BG_L_SNARE, BG_L_KICK};
    static const char *const ln[] = {"crash", "ride ", "open ", "hat  ", "tom h", "tom l", "snare", "kick "};
    uint32_t k, i;
    printf("%s\n", name);
    for (k = 0; k < NELEM_(lanes); k++) {
        printf("  %s |", ln[k]);
        for (i = 0; i < BG_BAR; i++) {
            const dstep_t *s = &p->st[i];
            char c = '.';
            if (bg_has(s, lanes[k]))
                c = rat_of(s, lanes[k]) ? 'r' : lvl_of(s, lanes[k]) == LV_HARD ? 'X' :
                    lvl_of(s, lanes[k]) == LV_GHOST ? 'g' : lvl_of(s, lanes[k]) == LV_SOFT ? 'o' : 'x';
            printf("%c%s", c, i % 4u == 3u ? "|" : "");
        }
        printf("\n");
    }
}
int main(void)
{
    bg_pat_t p, q;
    bg_cfg_t c;
    uint32_t st, i, b;
    int verbose = getenv("VERBOSE") != NULL;

    /* each style: 16 steps of 1/16, something on the downbeat, nothing past the pattern */
    for (st = 0; st < BG_NSTYLE; st++) {
        c = plain(st);
        CHECK(beatgen_make(&c, &p) == 16u, "%s: 1 bar is 16 steps", BG_STYLE_NAME[st]);
        CHECK(p.len == 16u && p.div == BG_DIV, "%s: len / div", BG_STYLE_NAME[st]);
        CHECK(mask_of(&p.st[0]) != 0u, "%s: the downbeat plays", BG_STYLE_NAME[st]);
        for (i = 16; i < NSTEP; i++)
            CHECK(mask_of(&p.st[i]) == 0u && p.micro[i] == 0, "%s: step %u past the pattern is empty", BG_STYLE_NAME[st], i);
        for (i = 0; i < 16u; i++)
            CHECK(p.micro[i] == 0, "%s: no nudge without HUMAN", BG_STYLE_NAME[st]);
        if (verbose)
            show(BG_STYLE_NAME[st], &p);
    }

    /* the grooves themselves */
    c = plain(BG_TRAD);
    beatgen_make(&c, &p);
    for (i = 0; i < 16u; i++) {
        CHECK(bg_has(&p.st[i], BG_L_KICK) == !(i & 1u), "TRAD: kick on the even 16ths (%u)", i);
        CHECK(bg_has(&p.st[i], BG_L_SNARE) == (int)(i & 1u), "TRAD: snare on the odd 16ths (%u)", i);
        CHECK(!(bg_has(&p.st[i], BG_L_KICK) && bg_has(&p.st[i], BG_L_SNARE)), "TRAD: kick and snare alternate (%u)", i);
    }
    CHECK(count(&p, BG_L_RIDE, 0, 16) == 8u, "TRAD: the ride with the kick (AUTO)");
    c = plain(BG_HAMMER);
    beatgen_make(&c, &p);
    for (i = 0; i < 16u; i++)
        CHECK((bg_has(&p.st[i], BG_L_KICK) && bg_has(&p.st[i], BG_L_SNARE)) == !(i & 1u), "HAMMER: unison on the 8ths (%u)", i);
    c = plain(BG_BOMB);
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_KICK, 0, 16) == 16u && count(&p, BG_L_SNARE, 0, 16) == 8u, "BOMB: 16th kicks, 8th snares");
    c = plain(BG_SKANK);
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_KICK, 0, 16) == 4u && count(&p, BG_L_SNARE, 0, 16) == 4u, "SKANK: kick on the beat, snare between");
    CHECK(bg_has(&p.st[2], BG_L_SNARE) && !bg_has(&p.st[2], BG_L_KICK), "SKANK: the snare on the 'and'");
    c = plain(BG_DBEAT);
    beatgen_make(&c, &p);
    CHECK(bg_has(&p.st[0], BG_L_KICK) && bg_has(&p.st[4], BG_L_SNARE) && bg_has(&p.st[6], BG_L_KICK) &&
          bg_has(&p.st[8], BG_L_KICK) && bg_has(&p.st[12], BG_L_SNARE), "DBEAT: K S K K S");
    CHECK(count(&p, BG_L_CRASH, 0, 16) == 8u, "DBEAT: the crash on the 8ths (AUTO)");
    c = plain(BG_STOMP);
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_SNARE, 0, 16) == 1u && bg_has(&p.st[8], BG_L_SNARE), "STOMP: one snare, on 3 (half time)");
    CHECK(count(&p, BG_L_HAT, 0, 16) == 8u, "STOMP: closed hats on the 8ths");
    c = plain(BG_CRAWL);
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_RIDE, 0, 16) == 4u, "CRAWL: the ride on the quarters");
    c = plain(BG_WAR);
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_TOM_L, 0, 16) == 4u && count(&p, BG_L_TOM_H, 0, 16) == 3u, "WAR: toms on the beat, pickups");
    CHECK(rat_of(&p.st[15], BG_L_SNARE) == 1u, "WAR: the roll ends in 32nds");
    CHECK(count(&p, BG_L_HAT, 0, 16) + count(&p, BG_L_RIDE, 0, 16) + count(&p, BG_L_CRASH, 0, 16) == 0u, "WAR: no cymbal (AUTO)");

    /* bars: 2 and 4 repeat the bar; anything else is 1 */
    for (b = 0; b <= 5u; b++) {
        uint32_t want = b == 2u || b == 4u ? b * 16u : 16u;
        c = plain(BG_DBEAT);
        c.bars = (uint8_t)b;
        CHECK(beatgen_make(&c, &p) == want, "bars %u: %u steps", b, want);
        for (i = 0; i < want; i++)
            CHECK(memcmp(&p.st[i], &p.st[i % 16u], sizeof(dstep_t)) == 0, "bars %u: step %u repeats the bar", b, i);
    }

    /* KICK */
    c = plain(BG_STOMP);
    c.kick = BG_KICK_DOUBLE;
    beatgen_make(&c, &p);
    for (i = 0; i < 16u; i++)
        if (bg_has(&p.st[i], BG_L_KICK))
            CHECK(rat_of(&p.st[i], BG_L_KICK) == 1u, "KICK DOUBLE: a 32nd pair on step %u", i);
    c.kick = BG_KICK_FULL;
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_KICK, 0, 16) == 16u, "KICK FULL: every 16th");

    /* CYM and CRATE */
    c = plain(BG_TRAD);
    c.cym = BG_CYM_CRASH;
    c.crate = BG_CRATE_4;
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_CRASH, 0, 16) == 4u && count(&p, BG_L_RIDE, 0, 16) == 0u, "CYM CRASH at 1/4");
    c.cym = BG_CYM_HAT;
    c.crate = BG_CRATE_16;
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_HAT, 0, 16) == 16u, "CYM HAT at 1/16");
    c.cym = BG_CYM_OFF;
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_HAT, 0, 16) + count(&p, BG_L_RIDE, 0, 16) + count(&p, BG_L_CRASH, 0, 16) +
          count(&p, BG_L_OPEN, 0, 16) == 0u, "CYM OFF");

    /* FILL: the last beat only, the groove before it untouched */
    for (b = BG_FILL_SNARE; b < BG_NFILL; b++) {
        c = plain(BG_DBEAT);
        c.bars = 2;
        beatgen_make(&c, &q);
        c.fill = (uint8_t)b;
        beatgen_make(&c, &p);
        for (i = 0; i < 28u; i++)
            CHECK(memcmp(&p.st[i], &q.st[i], sizeof(dstep_t)) == 0, "FILL %s: step %u keeps the groove", BG_FILL_NAME[b], i);
        CHECK(count(&p, BG_L_HAT, 28, 32) + count(&p, BG_L_CRASH, 28, 32) == 0u, "FILL %s: the cymbals stop", BG_FILL_NAME[b]);
        CHECK(mask_of(&p.st[31]) != 0u, "FILL %s: plays to the end", BG_FILL_NAME[b]);
    }
    c.fill = BG_FILL_SNARE;
    beatgen_make(&c, &p);
    CHECK(count(&p, BG_L_SNARE, 28, 32) == 4u && rat_of(&p.st[31], BG_L_SNARE) == 1u && lvl_of(&p.st[31], BG_L_SNARE) == LV_HARD,
          "FILL SNARE: four snares, into 32nds, louder");
    c.fill = BG_FILL_TOMS;
    beatgen_make(&c, &p);
    CHECK(bg_has(&p.st[28], BG_L_TOM_H) && bg_has(&p.st[31], BG_L_TOM_L), "FILL TOMS: high to low");

    /* ACCENT */
    c = plain(BG_SKANK);
    c.accent = 1;
    beatgen_make(&c, &p);
    CHECK(bg_has(&p.st[0], BG_L_CRASH) && lvl_of(&p.st[0], BG_L_CRASH) == LV_HARD && lvl_of(&p.st[0], BG_L_KICK) == LV_HARD,
          "ACCENT: a hard crash and kick on step 1");
    CHECK(count(&p, BG_L_CRASH, 1, 16) == 8u - 1u, "ACCENT: only the first step changes");

    /* HUMAN: nudges in range, the beats on the grid; the same seed the same pattern, another seed another */
    for (st = 0; st < BG_NSTYLE; st++)
        for (b = 0; b <= BG_HUMAN_MAX + 2u; b++) {
            uint32_t h = b > BG_HUMAN_MAX ? BG_HUMAN_MAX : b, moved = 0;
            c = plain(st);
            c.bars = 4;
            c.human = (uint8_t)b;
            c.seed = 1234;
            beatgen_make(&c, &p);
            beatgen_make(&c, &q);
            CHECK(memcmp(&p, &q, sizeof p) == 0, "HUMAN %u %s: deterministic", b, BG_STYLE_NAME[st]);
            for (i = 0; i < 64u; i++) {
                CHECK(p.micro[i] >= -(int32_t)h && p.micro[i] <= (int32_t)h, "HUMAN %u: nudge %d in range", b, p.micro[i]);
                if (i % 4u == 0u)
                    CHECK(p.micro[i] == 0, "HUMAN %u: beat %u on the grid", b, i);
                moved += p.micro[i] != 0;
                CHECK((mask_of(&p.st[i]) & ~0x1E35u) == 0u, "only its lanes (step %u)", i);
            }
            if (h)
                CHECK(moved > 0u, "HUMAN %u %s: something moves", b, BG_STYLE_NAME[st]);
        }
    c = plain(BG_STOMP);
    c.bars = 4;
    c.human = 4;
    c.seed = 1;
    beatgen_make(&c, &p);
    c.seed = 2;
    beatgen_make(&c, &q);
    CHECK(memcmp(&p, &q, sizeof p) != 0, "HUMAN: another seed, another pattern");
    CHECK(count(&p, BG_L_SNARE, 0, 64) >= 4u, "STOMP: its snares stay with HUMAN");

    /* out-of-range settings stay safe */
    memset(&c, 0xFF, sizeof c);
    CHECK(beatgen_make(&c, &p) == 16u, "every setting 0xFF: a pattern of 16 steps");

    printf(fails ? "beatgen: %d FAILED\n" : "beatgen: all passed\n", fails);
    return fails != 0;
}
