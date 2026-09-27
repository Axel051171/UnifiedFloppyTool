/**
 * @file test_fdc_gap2_aus_profil.c
 * @brief The track budget takes gap2 from the PROFILE, and the ED profile
 *        carries the ED gap2 of 41 (MF-1388).
 *
 * Found by checking the owner's uft-limits package against the tree:
 * `fdc_satzlaenge()` in src/formats/uft_fdc_gaps.c counted gap2 as a fixed
 * 22 (MFM) / 11 (FM), while every profile carries its own `gaps.gap2`
 * field — the same quantity in two places (MF-1177). As long as all
 * profiles said 22/11 the two agreed; the ED profile shows why that is
 * not enough.
 *
 * ED gap2 is 41, from two independent sources:
 *   - greaseweazle src/greaseweazle/codec/ibm/ibm.py:738-742 (Unlicense,
 *     read at 26690f8): "At ED rate the default GAP2 is 41 bytes."
 *   - MAME src/lib/formats/pc_dsk.cpp:90-92 (BSD-3-Clause, read from
 *     neue-ideen/formats1.zip): 2880K entry, gap_2 = 41.
 * MAME marks that entry "gaps unverified"; both agree on gap2, neither
 * gives a verified gap3 — the profile stays not fully belegt.
 */
#include "uft/formats/uft_fdc_gaps.h"

#include <stdio.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { gruen++; }
    else    { printf("  [ROT] %s -- %s\n", was, hinweis); rot++; }
}

/* gap_space must shrink by exactly sectors x delta when gap2 grows. */
static void gap2_wirkt(const uft_fdc_format_t *vorlage, int delta,
                       const char *name)
{
    uft_fdc_format_t f = *vorlage;
    const uint32_t vorher = uft_fdc_gap_space(&f);
    f.gaps.gap2 = (uint8_t)(f.gaps.gap2 + delta);
    const uint32_t nachher = uft_fdc_gap_space(&f);
    const uint32_t erwartet = (uint32_t)f.sectors * (uint32_t)delta;
    char h[160];
    snprintf(h, sizeof h, "%s: gap_space %u -> %u, erwartet -%u",
             name, vorher, nachher, erwartet);
    pruefe("uft_fdc_gap_space() liest gap2 aus dem PROFIL",
           vorher > nachher && vorher - nachher == erwartet, h);
}

int main(void)
{
    puts("=== FDC-Spurhaushalt: gap2 aus dem Profil, ED gap2 = 41 ===");

    gap2_wirkt(&UFT_FDC_PC_1440K, 5, "PC 1.44M (MFM)");
    gap2_wirkt(&UFT_FDC_FM_SD, 3, "FM Single Density");

    char h[120];
    snprintf(h, sizeof h, "gap2 = %u", (unsigned)UFT_FDC_PC_2880K.gaps.gap2);
    pruefe("PC 2.88M (ED) traegt gap2 = 41 (gw ibm.py:738-742, MAME "
           "pc_dsk.cpp:92)", UFT_FDC_PC_2880K.gaps.gap2 == 41, h);

    /* The switch to the profile field changes nothing for the other
     * profiles: every MFM profile says 22, every FM profile 11 — measured,
     * held here so a new profile with a different value is a decision. */
    int abweichend = 0;
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *f = UFT_FDC_FORMATS[i];
        if (f == &UFT_FDC_PC_2880K) continue;
        const unsigned soll = f->mfm ? 22u : 11u;
        if (f->gaps.gap2 != soll) {
            printf("  %s: gap2 %u statt %u\n", f->name,
                   (unsigned)f->gaps.gap2, soll);
            abweichend++;
        }
    }
    pruefe("alle uebrigen Profile tragen gap2 22 (MFM) / 11 (FM)",
           abweichend == 0, "ein Profil weicht ab — Absicht belegen");

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
