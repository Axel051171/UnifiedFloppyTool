/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_scp_ablage.h
 * @brief Which SCP table slot holds (cylinder, head)? — including the
 *        legacy single-sided layout (P3-676, MF-1524).
 *
 * The SCP track table (TLUT) normally holds cylinder c, head h at slot
 * c * 2 + h, also for a single-sided image (the other side's slots stay 0).
 * Some tools wrote single-sided images into CONSECUTIVE slots instead.
 *
 * REFERENCE: greaseweazle (keirf/greaseweazle @ 26690f8, Unlicense),
 * src/greaseweazle/image/scp.py:246-254, behaviour, own implementation:
 *
 *   "Some tools produce (or used to produce) single-sided images using
 *    consecutive entries in the TLUT. This needs fixing up."
 *   if single_sided and s[0] and s[1]: slot n -> track n*2 + single_sided-1
 *
 * where single_sided is the header's heads field (byte 0x0A: 1 = side 0
 * only, 2 = side 1 only) and s[0]/s[1] count the occupied even/odd slots.
 * A single-sided image with both even AND odd slots occupied can only be
 * the consecutive layout.
 *
 * NOT covered, and named in P3-676: greaseweazle's second fix-up just above
 * it (scp.py:239-244, C64 images with halftracks from the Supercard Pro).
 */
#ifndef UFT_FLUX_SCP_ABLAGE_H
#define UFT_FLUX_SCP_ABLAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool    fortlaufend;   /**< consecutive single-sided layout */
    uint8_t seite;         /**< the one side it holds (0 or 1) */
} uft_scp_ablage_t;

/** Decide the layout from the heads field and the occupied table slots. */
static inline uft_scp_ablage_t uft_scp_ablage_bestimmen(uint8_t heads_feld,
                                                        const uint32_t *tlut,
                                                        int eintraege)
{
    uft_scp_ablage_t a = { false, 0 };
    if ((heads_feld != 1 && heads_feld != 2) || !tlut) return a;
    int gerade = 0, ungerade = 0;
    for (int i = 0; i < eintraege; i++) {
        if (!tlut[i]) continue;
        if (i & 1) ungerade++; else gerade++;
    }
    if (gerade && ungerade) {
        a.fortlaufend = true;
        a.seite = (uint8_t)(heads_feld - 1);
    }
    return a;
}

/** Table slot of (cylinder, head); -1 if the image holds no such side. */
static inline int uft_scp_ablage_platz(uft_scp_ablage_t a, int zylinder, int kopf)
{
    if (!a.fortlaufend) return zylinder * 2 + kopf;
    return (kopf == (int)a.seite) ? zylinder : -1;
}

#endif /* UFT_FLUX_SCP_ABLAGE_H */
