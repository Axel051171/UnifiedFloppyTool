/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_fdc_gaps_1440k.c
 * @brief Die beiden Gap-3-Werte von PC 1.44M waren vertauscht (MF-838).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * Die DDPT des PC fuehrt **zwei** Gap-3-Werte getrennt — FreeDOS FORMAT
 * 0.92 (Brian E. Reifsnyder u. a., GPL-2), `floppy.c:402` (`ddptPrinter`)
 * zeigt sie nebeneinander als `gap3_length_rw` und `gap3_length_xmat`.
 * Werte nach Ch. Hochstaetter, FDFORMAT/88 1.8, zitiert in FreeDOS
 * FORMAT `floppy.c:952`:
 *
 *    9 Sekt.: Format-Gap 0x50 = 80    (BIOS-R/W-Gap 0x2A = 42)
 *   15 Sekt.: Format-Gap 0x54 = 84
 *   18 Sekt.: Format-Gap 0x6C = 108   (BIOS-R/W-Gap 0x1B = 27)
 *
 * `UFT_FDC_PC_1440K` trug `gap3_rw = 108` — das ist der FORMAT-Gap — und
 * `gap3_fmt = 84`, den Wert von 1.2M. Derselbe 84er stand in **vier**
 * Eintraegen mit **drei verschiedenen Sektorzahlen** (15, 18, 18, 36):
 * das Muster eines kopierten Blocks.
 *
 * ── Lizenz ───────────────────────────────────────────────────────────────
 *
 * FreeDOS FORMAT steht unter **GPL-2-only** und ist damit mit GPL-3 nicht
 * vereinbar. Aus jenem Baum geht **kein Code** in UFT — uebernommen sind
 * nur ZAHLEN und die Fundstellen, und Zahlen sind Fakten.
 *
 * ── Was hier nicht steht ─────────────────────────────────────────────────
 *
 * Der Rundum-Sweep ueber alle Eintraege liegt in
 * `scripts/audit_fdc_gaps.py`, weil er den Header PARST und damit auch
 * kuenftige Eintraege deckt. Diese Datei nagelt nur die drei Zahlen fest,
 * fuer die eine benannte Quelle vorliegt — sie ist der Anker, der eine
 * Umbenennung oder ein Refactoring ueberlebt.
 */
#include "uft/formats/uft_fdc_gaps.h"

#include <stdio.h>
#include <stdint.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* Bytes je Sektor OHNE gap3 — Sync, Marken, CHRN, CRC, gap2, Daten.
 * Identisch zu `satzlaenge()` in scripts/audit_fdc_gaps.py. */
static uint32_t satzlaenge(const uft_fdc_format_t *f)
{
    return f->mfm
        ? 12u + 3u + 1u + 4u + 2u + f->gaps.gap2 + 12u + 3u + 1u
          + f->sector_size + 2u
        : 6u + 1u + 4u + 2u + f->gaps.gap2 + 6u + 1u + f->sector_size + 2u;
}

static uint32_t belegt(const uft_fdc_format_t *f)
{
    return f->gaps.gap4a + f->gaps.gap1
         + (uint32_t)f->sectors * (satzlaenge(f) + f->gaps.gap3_fmt)
         + f->gaps.gap4b;
}

TEST(pc_1440k_traegt_die_belegten_gap3_werte)
{
    const uft_fdc_format_t *f = &UFT_FDC_PC_1440K;
    ASSERT(f->sectors == 18);
    ASSERT(f->sector_size == 512);

    /* 0x1B = 27, BIOS-Lese/Schreib-Gap. Vorher stand hier 108. */
    ASSERT(f->gaps.gap3_rw == 27);
    /* 0x6C = 108, Hochstaetters Format-Gap. Vorher stand hier 84 —
     * der Wert von 1.2M. */
    ASSERT(f->gaps.gap3_fmt == 108);
}

TEST(der_format_gap_ist_nie_kleiner_als_der_lese_schreib_gap)
{
    /* Die Invariante, an der der Fehler aufgefallen ist. Belegt an drei
     * Wertepaaren bei Hochstaetter: 80/42, 84/—, 108/27 — der Format-Gap
     * liegt immer darueber, weil beim Formatieren Drehzahlschwankung
     * aufgefangen werden muss.
     *
     * Geprueft an den fuenf PC-Eintraegen; den Rundumlauf ueber ALLE
     * Eintraege macht `scripts/audit_fdc_gaps.py` durch Parsen des
     * Headers. */
    const uft_fdc_format_t *pc[] = {
        &UFT_FDC_PC_360K, &UFT_FDC_PC_720K, &UFT_FDC_PC_1200K,
        &UFT_FDC_PC_1440K, &UFT_FDC_PC_2880K
    };
    for (size_t i = 0; i < sizeof pc / sizeof pc[0]; i++)
        ASSERT(pc[i]->gaps.gap3_fmt >= pc[i]->gaps.gap3_rw);
}

TEST(die_1440k_spur_passt_mit_dem_belegten_format_gap)
{
    /* Mit `gap3_fmt = 108` belegt die Spur
     *   80 + 50 + 18 x (574 + 108) + 78 = 12484
     * von 12500 Byte. Die alten 400 Byte `gap4b` haetten NICHT gepasst
     * (12806 > 12500) — deshalb ist `gap4b` auf 78 ABGELEITET, und
     * deshalb steht diese Rechnung hier. */
    const uft_fdc_format_t *f = &UFT_FDC_PC_1440K;
    ASSERT(satzlaenge(f) == 574);
    ASSERT(belegt(f) == 12484);
    ASSERT(belegt(f) <= f->track_bytes);

    /* Gegenprobe: mit den alten 400 Byte haette es nicht gepasst. */
    uint32_t mit_400 = belegt(f) - f->gaps.gap4b + 400u;
    ASSERT(mit_400 > f->track_bytes);
}

TEST(der_1200k_wert_84_bleibt_dort_wo_er_hingehoert)
{
    /* 84 ist der Format-Gap von 15 Sektoren (0x54). Dass er in 1.2M
     * steht, ist richtig — falsch war nur, dass er zusaetzlich in drei
     * andere Eintraege kopiert war. */
    ASSERT(UFT_FDC_PC_1200K.sectors == 15);
    ASSERT(UFT_FDC_PC_1200K.gaps.gap3_fmt == 84);
    /* Und in 1.44M steht er jetzt NICHT mehr. */
    ASSERT(UFT_FDC_PC_1440K.gaps.gap3_fmt != 84);
}

int main(void)
{
    printf("=== PC 1.44M: die zwei Gap-3-Werte (MF-838) ===\n");
    RUN(pc_1440k_traegt_die_belegten_gap3_werte);
    RUN(der_format_gap_ist_nie_kleiner_als_der_lese_schreib_gap);
    RUN(die_1440k_spur_passt_mit_dem_belegten_format_gap);
    RUN(der_1200k_wert_84_bleibt_dort_wo_er_hingehoert);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
