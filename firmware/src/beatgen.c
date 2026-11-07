/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 the Rheia authors. Rheia is based on SLOOP (isod89) and Felucca (Leo Kuroshita,
 * Hügelton Instruments). */
/* ---------------------------------------------------------- BEAT GEN ---
 * Rheia: writes a drum pattern of a style into the drum track (16 lanes, drums.c LANE_*), as plain steps:
 * nothing new plays it, the sequencer plays what it wrote and every step stays editable (SEQ, EDIT, undo).
 *
 * The grid is 1/16 (DIV 2), 16 steps a bar, 1, 2 or 4 bars (16, 32, 64 steps). The tempo is the song's:
 * blasts want 180..240 BPM, the half-time styles 60..110.
 *
 *   TRAD    traditional blast: kick and snare alternating on the 16ths, the cymbal with the kick
 *   HAMMER  hammer blast: kick, snare and cymbal together on the 8ths
 *   BOMB    bomb blast: kick on every 16th, snare and cymbal on the 8ths
 *   SKANK   skank beat (thrash / hardcore polka): kick on the beat, snare on the "and"
 *   DBEAT   D-beat: kick 1, snare 2, kick &2 and 3, snare 4, the cymbal on the 8ths
 *   STOMP   half-time industrial stomp: a heavy kick, the snare on 3, closed hats on the 8ths
 *   CRAWL   sludge crawl: kick, a late kick, the snare on 3, the ride on the quarters
 *   WAR     war march: low tom on the beat, high tom pickups, a snare roll on 4
 *
 * KICK  STYLE: as the style writes it; DOUBLE: every kick a 32nd pair (ratchet x2); FULL: a kick on every 16th.
 * CYM   AUTO (the style's), HAT, OPEN, RIDE, CRASH (a wash), OFF; CRATE AUTO, 1/4, 1/8, 1/16.
 * FILL  on the last beat of the pattern: OFF, SNARE (a roll into 32nds), TOMS (high to low), BLAST (kick and snare
 *       in 32nds). ACCENT: a crash and a hard kick on the first step.
 * HUMAN 0..4: cymbal dynamics (normal / soft), ghost snares on the half-time styles, a step nudge of up to
 *       +-HUMAN / 64 of a step. SEED: another draw of the same settings (the same seed, the same pattern).
 */

enum { BG_TRAD, BG_HAMMER, BG_BOMB, BG_SKANK, BG_DBEAT, BG_STOMP, BG_CRAWL, BG_WAR, BG_NSTYLE };
enum { BG_KICK_STYLE, BG_KICK_DOUBLE, BG_KICK_FULL, BG_NKICK };
enum { BG_CYM_AUTO, BG_CYM_HAT, BG_CYM_OPEN, BG_CYM_RIDE, BG_CYM_CRASH, BG_CYM_OFF, BG_NCYM };
enum { BG_CRATE_AUTO, BG_CRATE_4, BG_CRATE_8, BG_CRATE_16, BG_NCRATE };
enum { BG_FILL_OFF, BG_FILL_SNARE, BG_FILL_TOMS, BG_FILL_BLAST, BG_NFILL };

static const char *const BG_STYLE_NAME[BG_NSTYLE] = {"TRAD", "HAMMER", "BOMB", "SKANK", "DBEAT", "STOMP", "CRAWL", "WAR"};
static const char *const BG_KICK_NAME[BG_NKICK] = {"STYLE", "DOUBLE", "FULL"};
static const char *const BG_CYM_NAME[BG_NCYM] = {"AUTO", "HAT", "OPEN", "RIDE", "CRASH", "OFF"};
static const char *const BG_CRATE_NAME[BG_NCRATE] = {"AUTO", "1/4", "1/8", "1/16"};
static const char *const BG_FILL_NAME[BG_NFILL] = {"OFF", "SNARE", "TOMS", "BLAST"};

/* the lanes it writes (drums.c LANE_NOTE order) */
#define BG_L_KICK 0u
#define BG_L_SNARE 2u
#define BG_L_HAT 4u
#define BG_L_OPEN 5u
#define BG_L_TOM_L 9u
#define BG_L_TOM_H 10u
#define BG_L_CRASH 11u
#define BG_L_RIDE 12u

#define BG_DIV 2u                        /* N_SDIV "1/16" */
#define BG_BAR 16u                       /* steps a bar on that grid */
#define BG_HUMAN_MAX 4u

typedef struct {
    uint8_t style, bars;                 /* BG_*; 1, 2 or 4 (others: 1) */
    uint8_t kick, cym, crate, fill;      /* BG_KICK_*, BG_CYM_*, BG_CRATE_*, BG_FILL_* */
    uint8_t accent, human;               /* 0 / 1; 0..BG_HUMAN_MAX */
    uint16_t seed;
} bg_cfg_t;

typedef struct {                         /* what it writes into the drum track */
    dstep_t st[NSTEP];
    int8_t micro[NSTEP];
    uint8_t len, div;
} bg_pat_t;

static const bg_cfg_t BG_DEFAULT = {BG_TRAD, 1, BG_KICK_STYLE, BG_CYM_AUTO, BG_CRATE_AUTO, BG_FILL_OFF, 1, 1, 1};

/* a lane on in a step with its level and ratchet (as drums.c dstep_set, kept here so the generator stands alone) */
static void bg_hit(dstep_t *s, uint32_t l, uint32_t lvl, uint32_t rat)
{
    uint32_t sh = (l & 3u) * 2u, b = (l >> 2) & 3u;
    s->on[(l >> 3) & 1u] |= (uint8_t)(1u << (l & 7u));
    s->lvl[b] = (uint8_t)((s->lvl[b] & ~(3u << sh)) | (lvl & 3u) << sh);
    s->rat[b] = (uint8_t)((s->rat[b] & ~(3u << sh)) | (rat & 3u) << sh);
}
static int bg_has(const dstep_t *s, uint32_t l) { return (s->on[(l >> 3) & 1u] >> (l & 7u)) & 1u; }
static void bg_clear(dstep_t *s) { s->on[0] = s->on[1] = 0; s->lvl[0] = s->lvl[1] = s->lvl[2] = s->lvl[3] = 0;
                                   s->rat[0] = s->rat[1] = s->rat[2] = s->rat[3] = 0; }

static uint32_t bg_rand(uint32_t *x)     /* xorshift32: the same seed, the same pattern */
{
    uint32_t v = *x ? *x : 0x9E3779B9u;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    return *x = v;
}

/* the style's own hits on step s of a bar (0..15): kick, snare (level), cymbal; the low / high toms for WAR */
typedef struct { uint8_t kick, snare, cym, tom_l, tom_h, snare_rat; } bg_hits_t;
static bg_hits_t bg_style_hits(uint32_t style, uint32_t s)
{
    bg_hits_t h = {0, 0, 0, 0, 0, 0};
    switch (style) {
    case BG_TRAD:   h.kick = !(s & 1u); h.snare = s & 1u; h.cym = !(s & 1u); break;
    case BG_HAMMER: h.kick = h.snare = h.cym = !(s & 1u); break;
    case BG_BOMB:   h.kick = 1; h.snare = h.cym = !(s & 1u); break;
    case BG_SKANK:  h.kick = s % 4u == 0u; h.snare = s % 4u == 2u; h.cym = !(s & 1u); break;
    case BG_DBEAT:  h.kick = s == 0u || s == 6u || s == 8u; h.snare = s == 4u || s == 12u; h.cym = !(s & 1u); break;
    case BG_STOMP:  h.kick = s == 0u || s == 6u || s == 10u; h.snare = s == 8u; h.cym = !(s & 1u); break;
    case BG_CRAWL:  h.kick = s == 0u || s == 3u; h.snare = s == 8u; h.cym = s % 4u == 0u; break;
    case BG_WAR:    h.tom_l = s % 4u == 0u; h.tom_h = s == 3u || s == 7u || s == 11u; h.snare = s >= 12u;
                    h.snare_rat = s == 14u || s == 15u; h.kick = s == 0u || s == 8u; break;
    default: break;
    }
    return h;
}
static uint32_t bg_auto_cym(uint32_t style)
{
    switch (style) {
    case BG_TRAD: case BG_HAMMER: case BG_BOMB: return BG_CYM_RIDE;    /* (a crash wash: CYM CRASH) */
    case BG_SKANK: case BG_DBEAT: return BG_CYM_CRASH;
    case BG_CRAWL: return BG_CYM_RIDE;
    case BG_WAR: return BG_CYM_OFF;
    default: return BG_CYM_HAT;                                         /* STOMP */
    }
}
static uint32_t bg_cym_lane(uint32_t cym)
{
    return cym == BG_CYM_OPEN ? BG_L_OPEN : cym == BG_CYM_RIDE ? BG_L_RIDE : cym == BG_CYM_CRASH ? BG_L_CRASH : BG_L_HAT;
}
static int bg_cym_on(uint32_t crate, uint32_t s, int style_says)
{
    switch (crate) {
    case BG_CRATE_4: return s % 4u == 0u;
    case BG_CRATE_8: return s % 2u == 0u;
    case BG_CRATE_16: return 1;
    default: return style_says;
    }
}

/* the fill on the last beat (the last 4 steps), over the groove there */
static void bg_fill(dstep_t *st, uint32_t len, uint32_t fill)
{
    uint32_t i;
    if (fill == BG_FILL_OFF || len < 4u)
        return;
    for (i = 0; i < 4u; i++) {
        dstep_t *s = &st[len - 4u + i];
        bg_clear(s);
        switch (fill) {
        case BG_FILL_SNARE:                              /* 16ths, then 32nds, louder to the end */
            bg_hit(s, BG_L_SNARE, i < 2u ? LV_NORM : LV_HARD, i < 2u ? 0u : 1u);
            break;
        case BG_FILL_TOMS:                               /* high, high, low, low (the last in 32nds) */
            bg_hit(s, i < 2u ? BG_L_TOM_H : BG_L_TOM_L, i == 3u ? LV_HARD : LV_NORM, i == 3u ? 1u : 0u);
            bg_hit(s, BG_L_KICK, LV_NORM, 0);
            break;
        case BG_FILL_BLAST:                              /* kick and snare together in 32nds */
            bg_hit(s, BG_L_KICK, LV_HARD, 1);
            bg_hit(s, BG_L_SNARE, LV_HARD, 1);
            break;
        default:
            break;
        }
    }
}

/* the pattern of cfg into *out: its steps, nudges, length and DIV. Returns the length in steps. */
static uint32_t beatgen_make(const bg_cfg_t *cfg, bg_pat_t *out)
{
    uint32_t bars = cfg->bars == 2u || cfg->bars == 4u ? cfg->bars : 1u;
    uint32_t len = bars * BG_BAR, style = cfg->style % BG_NSTYLE, human = cfg->human > BG_HUMAN_MAX ? BG_HUMAN_MAX : cfg->human;
    uint32_t cym = cfg->cym % BG_NCYM, crate = cfg->crate % BG_NCRATE, kick = cfg->kick % BG_NKICK;
    uint32_t rnd = 0x5EED0000u ^ cfg->seed ^ (style << 8) ^ (bars << 4), i;
    if (cym == BG_CYM_AUTO)
        cym = bg_auto_cym(style);
    for (i = 0; i < NSTEP; i++) {
        bg_clear(&out->st[i]);
        out->micro[i] = 0;
    }
    for (i = 0; i < len; i++) {
        uint32_t s = i % BG_BAR;
        dstep_t *st = &out->st[i];
        bg_hits_t h = bg_style_hits(style, s);
        if (kick == BG_KICK_FULL)
            h.kick = 1;
        if (h.kick)
            bg_hit(st, BG_L_KICK, LV_NORM, kick == BG_KICK_DOUBLE ? 1u : 0u);
        if (h.snare)
            bg_hit(st, BG_L_SNARE, style == BG_WAR && s < 14u ? LV_SOFT : LV_NORM, h.snare_rat);
        if (h.tom_l)
            bg_hit(st, BG_L_TOM_L, s == 0u ? LV_HARD : LV_NORM, 0);
        if (h.tom_h)
            bg_hit(st, BG_L_TOM_H, LV_NORM, 0);
        if (cym != BG_CYM_OFF && bg_cym_on(crate, s, h.cym)) {
            uint32_t lvl = LV_NORM;
            if (human && bg_rand(&rnd) % 8u < human)     /* HUMAN: some cymbals softer */
                lvl = LV_SOFT;
            bg_hit(st, bg_cym_lane(cym), lvl, 0);
        }
        /* HUMAN on the half-time styles: a ghost snare on an empty off-16th now and then */
        if (human && (style == BG_STOMP || style == BG_CRAWL || style == BG_DBEAT) && (s & 1u) && !h.snare &&
            bg_rand(&rnd) % 16u < human)
            bg_hit(st, BG_L_SNARE, LV_GHOST, 0);
    }
    bg_fill(out->st, len, cfg->fill % BG_NFILL);
    if (cfg->accent) {                                   /* the first step: a crash and a hard kick */
        bg_hit(&out->st[0], BG_L_CRASH, LV_HARD, 0);
        bg_hit(&out->st[0], BG_L_KICK, LV_HARD, bg_has(&out->st[0], BG_L_KICK) && kick == BG_KICK_DOUBLE ? 1u : 0u);
    }
    /* HUMAN: nudge the steps off the grid a little (never the first, the bar's downbeats stay on the grid) */
    if (human)
        for (i = 1; i < len; i++)
            if (i % 4u)
                out->micro[i] = (int8_t)((int32_t)(bg_rand(&rnd) % (2u * human + 1u)) - (int32_t)human);
    out->len = (uint8_t)len;
    out->div = BG_DIV;
    return len;
}

#ifndef BEATGEN_PURE
/* the pattern into the drum track (one undo step): its steps, nudges, length and DIV; the old locks and fill
 * conditions go (they were the old pattern's). Swing, the kit and the levels are left as they are. */
UI_SMALL static void beatgen_apply(const bg_cfg_t *cfg)
{
    static bg_pat_t pat;                                 /* (1 KB: not on the UI's stack) */
    track_t *t = TDRUM;
    uint32_t i;
    beatgen_make(cfg, &pat);
    undo_mark(t, (undo_sess += 4u) | 3u);
    fm1_irq_off();
    for (i = 0; i < NSTEP; i++) {
        t->dstep[i] = pat.st[i];
        t->micro[i] = pat.micro[i];
        lock_del(t, i, P_COUNT);
        step_fill_set(t, i, FC_NORM);
    }
    t->p[P_SLEN] = (int16_t)pat.len;
    t->p[P_SDIV] = (int16_t)pat.div;
    t->seq_active = 1;
    fm1_irq_on();
    sync_reload = 1;
}
#endif
