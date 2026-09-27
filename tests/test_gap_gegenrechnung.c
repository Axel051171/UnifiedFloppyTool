/**
 * @file test_gap_gegenrechnung.c
 * @brief The track budget, computed twice — the tree's ONE calculation
 *        (`uft_fdc_gap_space()`, MF-1177) against an independent
 *        cross-calculation, at all FDC profiles (A-037 step 1, MF-1377).
 *
 * MF-1177 says: one quantity, one calculation — and a second copy in a
 * gate or test is the dangerous kind, because one believes it. A SECOND
 * calculation is allowed here for exactly one purpose: to GUARD the first.
 * It is linked only into this test (tests/gegenrechnung/, never src/).
 *
 * The cross-calculation is `uft_gap_policy.c` from the owner's package
 * UFT_AdaptiveCopyCore v1.0.0, UNCHANGED (GPL-2.0-or-later). It is NOT a
 * foreign hand: same owner, not an executed third-party tool, so it is not
 * an Oracle in the sense of docs/ORACLES.md. What makes it independent is
 * its STRUCTURE — gap4a + gap1 [+ IAM] + n x (enc + gap2 + gap3) + gap4b,
 * gap2 taken from the profile instead of a fixed 22/11 — and its CONSTANTS,
 * which this test does NOT take from the tree:
 *
 *   greaseweazle src/greaseweazle/codec/ibm/ibm.py (Unlicense, read at
 *   26690f8), its own track-length calculation, :715-725 with :269-272 and
 *   :692-698:
 *     MFM: presync 12, synclen 4 ("A1 A1 A1 Mark")
 *     FM:  presync 6,  synclen 1 ("Mark")
 *     index area: presync + synclen (+ gap1)            -> IAM 16 / 7
 *     per sector: presync + synclen + 4 (ID) + 2 (CRC)
 *               + presync + synclen + data + 2 (CRC)    -> 40 / 22 + data
 *
 * Promises, per profile:
 *   (a) the cross-calculation's format sum equals track_bytes exactly when
 *       uft_fdc_gaps_schliessen() says the profile closes;
 *   (b) the tail it derives (uft_gap_calculate_gap4b) equals
 *       uft_fdc_gap_space() - gap3_fmt * sectors wherever the latter is
 *       defined — same gap4b from two structures;
 *   (c) it refuses gap3_fmt < gap3_rw (BAD_ORDER) exactly where the rule
 *       of scripts/audit_fdc_gaps.py would;
 *   (d) its IAM branch (untested in its own package, mutation M18) adds
 *       exactly the IAM bytes on the profiles that write an IAM.
 * Counter-proofs: a profile with gap4b + 1 must break (a) in BOTH
 * calculations; a profile with swapped gap3 values must be BAD_ORDER.
 */
#include "uft/formats/uft_fdc_gaps.h"
#include "uft/copy/uft_gap_policy.h"

#include <stdio.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { gruen++; }
    else    { printf("  [ROT] %s -- %s\n", was, hinweis); rot++; }
}

/* Constants from gw ibm.py, NOT from the tree (see file head). */
static size_t gw_enc_je_sektor(const uft_fdc_format_t *f)
{
    const size_t presync = f->mfm ? 12u : 6u;
    const size_t synclen = f->mfm ? 4u : 1u;
    return (presync + synclen + 4u + 2u) + (presync + synclen + 2u)
           + (size_t)f->sector_size;
}

static size_t gw_iam(const uft_fdc_format_t *f)
{
    return f->mfm ? (12u + 4u) : (6u + 1u);
}

static uft_gap_layout_t layout(const uft_fdc_format_t *f)
{
    uft_gap_layout_t g;
    memset(&g, 0, sizeof g);
    g.gap4a = f->gaps.gap4a;
    g.gap1 = f->gaps.gap1;
    g.gap2 = f->gaps.gap2;
    g.gap3_read_write = f->gaps.gap3_rw;
    g.gap3_format = f->gaps.gap3_fmt;
    g.gap4b = f->gaps.gap4b;
    g.has_index_address_mark = f->iam;
    return g;
}

static uft_gap_budget_t budget(const uft_fdc_format_t *f)
{
    uft_gap_budget_t b;
    b.track_bytes = f->track_bytes;
    b.sector_count = f->sectors;
    b.encoded_bytes_per_sector = gw_enc_je_sektor(f);
    b.index_address_mark_bytes = gw_iam(f);
    return b;
}

/* (a): both calculations agree on "the profile closes". */
static int parity(const uft_fdc_format_t *f, int *baum_out, int *orakel_out)
{
    uint32_t space = 0, used = 0;
    const int baum = uft_fdc_gaps_schliessen(f, &space, &used);
    const uft_gap_layout_t g = layout(f);
    const uft_gap_budget_t b = budget(f);
    size_t fmt = 0, rw = 0;
    const int rc = uft_gap_validate(&g, &b, &fmt, &rw);
    const int orakel = (rc == UFT_GAP_OK || rc == UFT_GAP_DOES_NOT_FIT)
                       && fmt == f->track_bytes;
    if (baum_out) *baum_out = baum;
    if (orakel_out) *orakel_out = orakel;
    return baum == orakel;
}

int main(void)
{
    puts("=== Spurhaushalt zweimal gerechnet: uft_fdc_gap_space() gegen die "
         "Gegenrechnung (gw-Konstanten) ===");
    char h[240];
    int profile = 0, schliessen = 0, iam_profile = 0, geprueft_b = 0;

    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *f = UFT_FDC_FORMATS[i];
        profile++;

        /* (a) */
        int baum = 0, orakel = 0;
        const int gleich = parity(f, &baum, &orakel);
        snprintf(h, sizeof h, "%s: Baum schliesst=%d, Gegenrechnung=%d",
                 f->name, baum, orakel);
        pruefe("(a) Summen-Paritaet", gleich, h);
        schliessen += baum;

        /* (b) */
        const uint32_t space = uft_fdc_gap_space(f);
        if (space != 0u && space >= (uint32_t)f->gaps.gap3_fmt * f->sectors) {
            uft_gap_layout_t g = layout(f);
            const uft_gap_budget_t b = budget(f);
            const int rc = uft_gap_calculate_gap4b(&g, &b, 0);
            const uint32_t baum_gap4b = space - (uint32_t)f->gaps.gap3_fmt * f->sectors;
            snprintf(h, sizeof h, "%s: gap4b Baum %u, Gegenrechnung %u (rc %d)",
                     f->name, (unsigned)baum_gap4b, (unsigned)g.gap4b, rc);
            pruefe("(b) dieselbe gap4b aus zwei Strukturen",
                   rc == UFT_GAP_OK && g.gap4b == baum_gap4b, h);
            geprueft_b++;
        }

        /* (c) */
        {
            const uft_gap_layout_t g = layout(f);
            const uft_gap_budget_t b = budget(f);
            const int rc = uft_gap_validate(&g, &b, NULL, NULL);
            const int regel = f->gaps.gap3_fmt >= f->gaps.gap3_rw;
            snprintf(h, sizeof h, "%s: gap3_fmt %u, gap3_rw %u, rc %d",
                     f->name, f->gaps.gap3_fmt, f->gaps.gap3_rw, rc);
            pruefe("(c) Ordnung wie audit_fdc_gaps.py",
                   regel == (rc != UFT_GAP_BAD_ORDER), h);
        }

        /* (d) */
        if (f->iam) {
            iam_profile++;
            uft_gap_layout_t mit = layout(f), ohne = layout(f);
            ohne.has_index_address_mark = false;
            const uft_gap_budget_t b = budget(f);
            size_t fm = 0, fo = 0;
            (void)uft_gap_validate(&mit, &b, &fm, NULL);
            (void)uft_gap_validate(&ohne, &b, &fo, NULL);
            snprintf(h, sizeof h, "%s: mit IAM %zu, ohne %zu, IAM %zu",
                     f->name, fm, fo, gw_iam(f));
            pruefe("(d) IAM-Zweig zaehlt genau die IAM", fm == fo + gw_iam(f), h);
        }
    }

    /* Counter-proof 1: gap4b + 1 on a closing profile breaks BOTH. */
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        uint32_t s = 0, u = 0;
        if (!uft_fdc_gaps_schliessen(UFT_FDC_FORMATS[i], &s, &u)) continue;
        uft_fdc_format_t kaputt = *UFT_FDC_FORMATS[i];
        kaputt.gaps.gap4b = (uint16_t)(kaputt.gaps.gap4b + 1u);
        int baum = 1, orakel = 1;
        (void)parity(&kaputt, &baum, &orakel);
        snprintf(h, sizeof h, "%s mit gap4b+1: Baum %d, Gegenrechnung %d",
                 kaputt.name, baum, orakel);
        pruefe("Gegenprobe: verfaelschtes gap4b faellt in BEIDEN Rechnungen",
               baum == 0 && orakel == 0, h);
        break;
    }

    /* Counter-proof 2: swapped gap3 values are BAD_ORDER. */
    {
        uft_fdc_format_t v = *UFT_FDC_FORMATS[0];
        if (v.gaps.gap3_fmt != v.gaps.gap3_rw) {
            const uint8_t t = v.gaps.gap3_fmt;
            v.gaps.gap3_fmt = v.gaps.gap3_rw;
            v.gaps.gap3_rw = t;
        } else {
            v.gaps.gap3_rw = (uint8_t)(v.gaps.gap3_fmt + 1u);
        }
        const uft_gap_layout_t g = layout(&v);
        const uft_gap_budget_t b = budget(&v);
        pruefe("Gegenprobe: vertauschte gap3-Werte sind BAD_ORDER",
               uft_gap_validate(&g, &b, NULL, NULL) == UFT_GAP_BAD_ORDER,
               "die Ordnungsregel greift nicht");
    }

    printf("  %d Profile, davon %d schliessen, %d mit gap4b-Vergleich, "
           "%d mit IAM\n", profile, schliessen, geprueft_b, iam_profile);
    pruefe("alle Profile der Tafel gesehen (UFT_FDC_FORMAT_COUNT)",
           profile == UFT_FDC_FORMAT_COUNT, "Tafel und Zaehler weichen ab");
    pruefe("Gegenprobe 1 hatte ein schliessendes Profil", schliessen > 0,
           "kein Profil schliesst — Gegenprobe 1 lief nicht");
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
