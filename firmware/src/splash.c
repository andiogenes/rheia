/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot splash (Rheia: its name as text; SLOOP's logo image, tools/gen_logo.py, is no longer drawn). */
#ifndef UI_SMALL                 /* (2.7) compiled for size: not the audio path (gfx.c UI_SMALL); the host tests: nothing */
#define UI_SMALL
#endif
/* (Rheia) the boot screen: RHEIA in the font's large letters made three times as big (R white, H E I A the four
 * tracks' colours), the version, where it comes from. SLOOP's own logo above stays SLOOP's (not drawn). */
UI_SMALL static void rheia_logo_draw(uint32_t y0)
{
    static const char L[5] = {'R', 'H', 'E', 'I', 'A'};
    uint32_t i, w = text_w(&FONT_L, "RHEIA"), k = w * 3u <= 232u ? 3u : 2u, h = 32u, off, y, x;
    int32_t px = 0;
    char c[2] = {0, 0};
    cv_begin(240, h * k, C_BLACK);
    for (i = 0; i < 5u; i++) {
        c[0] = L[i];
        px = cv_text(px, 0, &FONT_L, c, i ? TE_COL_DEF[i - 1u] : C_WHITE);
    }
    off = (240u - w * k) / 2u;
    for (y = h * k; y-- > 0;)                           /* k times as big, in place (from the end) */
        for (x = w * k; x-- > 0;)
            cv_px[y * 240u + off + x] = cv_px[(y / k) * 240u + x / k];
    for (y = 0; y < h * k; y++)
        for (x = 0; x < off; x++)
            cv_px[y * 240u + x] = swap16(C_BLACK);
    cv_blit(0, y0);
}
UI_SMALL static void sloop_splash(void)
{
    const char *v = FELUCCA_VERSION;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    rheia_logo_draw(60);
    if (v[0] == 'R' && v[1] == 'H' && v[5] == ' ')
        v += 6;                                         /* "RHEIA DEV" -> "DEV" under the name */
    cv_begin(240, 36, C_BLACK);
    cv_text(120 - text_w(&FONT_S, v) / 2, 0, &FONT_S, v, RGB(196, 196, 204));
    cv_text(120 - text_w(&FONT_S, "based on sloop + felucca") / 2, 18, &FONT_S, "based on sloop + felucca", RGB(96, 96, 104));
    cv_blit(0, 202);
}
