/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot splash (Rheia: Bruegel's The Misanthrope as line art; SLOOP's logo image, tools/gen_logo.py, is no longer drawn). */
#ifndef UI_SMALL                 /* (2.7) compiled for size: not the audio path (gfx.c UI_SMALL); the host tests: nothing */
#define UI_SMALL
#endif
/* (Rheia) the boot screen: Pieter Bruegel the Elder's The Misanthrope (1568, public domain) as line art
 * (misanthrope.h, tools/gen_misanthrope.py), RHEIA where the painting has its inscription (Om dat de werelt is soe
 * ongetru / Daer om gaen ik in de rouw), the version in the corner below -- all in the chosen theme's colours
 * (settings_init sets it first): the man in the values' colour, the thief in track 1's, his globe in the accent, the
 * purse in track 4's, the land and the frame in the labels'. SLOOP's own logo stays SLOOP's (not drawn). */
#include "misanthrope.h"
UI_SMALL static uint16_t mz_colour(uint32_t cls)
{
    switch (cls) {
    case MZ_MAN: return C_HI;
    case MZ_THIEF: return TE_COL[0];
    case MZ_GLOBE: return C_WHITE;
    case MZ_PURSE: return TE_COL[3];
    case MZ_FLOCK: return C_AMB;
    default: return C_GRAY;                             /* the frame, the land */
    }
}
#define MZ_BAND 120u                                    /* the screen in two bands of the canvas (240 x 124 at most) */
UI_SMALL static void misanthrope_draw(void)
{
    uint32_t band, i, k;
    for (band = 0; band < 2u; band++) {
        cv_begin(240, MZ_BAND, C_BLACK);
        cv_oy = -(int32_t)(band * MZ_BAND);
        for (i = 0; MISANTHROPE[i] != MZ_END && i < sizeof MISANTHROPE;) {
            const uint8_t *p = &MISANTHROPE[i];
            uint16_t c = mz_colour(p[0]);
            uint32_t n = p[1];
            for (k = 1; k < n; k++)
                cv_line(p[2u * k], p[2u * k + 1u], p[2u * k + 2u], p[2u * k + 3u], c);
            i += 2u + 2u * n;
        }
        cv_oy = 0;
        if (band) {
            static const char L[5] = {'R', 'H', 'E', 'I', 'A'};
            const char *v = FELUCCA_VERSION;
            char ch[2] = {0, 0};
            int32_t x = 120 - text_w(&FONT_S, "RHEIA") / 2;
            for (k = 0; k < 5u; k++) {                  /* R the accent, H E I A the four tracks' colours */
                ch[0] = L[k];
                x = cv_text(x, 207 - (int32_t)MZ_BAND, &FONT_S, ch, k ? TE_COL[k - 1u] : C_WHITE);
            }
            if (v[0] == 'R' && v[1] == 'H' && v[5] == ' ')
                v += 6;                                 /* "RHEIA DEV" -> "DEV" */
            cv_text(238 - text_w(&FONT_S, v), 224 - (int32_t)MZ_BAND, &FONT_S, v, C_AMB);
        }
        cv_blit(0, band * MZ_BAND);
    }
}
UI_SMALL static void sloop_splash(void)
{
    misanthrope_draw();
}
