/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_gcr_tafeln.c
 * @brief Die GCR-Tafeln gegen ihre BEDINGUNG und gegeneinander — und
 *        deshalb braucht `encoding.c` keine Ableitung (MF-1126, Stufe 1)
 *
 * ── Der Auftrag ───────────────────────────────────────────────────────────
 *
 * Die Eigentuemer-Weisung stellt `encoding.c` auf Stufe 1 —
 * „normbestimmt, sofort ableitbar (auch in Fall A)" — mit der Referenz
 * *Inside Commodore DOS* (GCR) und IBM/WD177x (FM/MFM), und der
 * bindenden Reihenfolge „die Norm zuerst, der Fremdbestand als
 * Vergleich".
 *
 * ── Warum hier keine Tabelle abgeschrieben steht ──────────────────────────
 *
 * Eine Tafel aus dem Gedaechtnis hinzuschreiben und den Fremdbestand
 * dagegen zu halten waere eine Pruefung gegen eine **erinnerte**
 * Erwartung — genau der Fehler, den dieser Baum als
 * `recalled_ist_keine_pruefung` fuehrt. *Inside Commodore DOS* liegt
 * nicht im Baum. Geprueft wird deshalb, was ohne die gedruckte Quelle
 * objektiv feststellbar ist:
 *
 *   1. die **Bedingung**, die GCR ueberhaupt ausmacht (sie IST die Norm
 *      und braucht keine Tabelle), und
 *   2. die **Identitaet** der beiden unabhaengigen Umsetzungen.
 *
 * ── 1) Die Bedingung: GCR ist selbsttaktend ───────────────────────────────
 *
 * Ein GCR-Strom muss ohne getrennte Taktspur lesbar sein. Dafuer darf
 * zwischen zwei Flusswechseln nie zu viel Zeit liegen — bei der
 * 4-zu-5-Umsetzung des 1541 heisst das: **nie mehr als zwei Nullen in
 * Folge**, und zwar auch dann nicht, wenn zwei Kodes aneinanderstossen.
 * Das ist keine Konvention, sondern die Bedingung, aus der die Tafel
 * gewaehlt wurde; eine falsche Zeile reisst sie.
 *
 * Geprueft wird deshalb ueber **alle 16 x 16 = 256 Paare**. Gemessen
 * (2026-09-14): laengster Nulllauf ueber alle Paare **genau 2**, kein
 * einziges Paar darueber. Eine einzige falsche Tafelzeile faellt hier
 * auf, ohne dass irgendwo ein Sollwert steht.
 *
 * ── 2) Die Identitaet: zwei unabhaengige Haende ───────────────────────────
 *
 * `gcr_get_encode_table()` (UFT, `src/formats/c64/uft_gcr_ops.c`) ist
 * eine eigenstaendige Umsetzung nach der nibtools-Dokumentation, mit
 * Aehnlichkeitsaudit — nibtools steht unter Apache-2.0 und ist mit
 * GPL-2 unvereinbar (MF-1008), ein Port waere also gesperrt.
 * `dtc_gcr_cbm_4to5` ist die Tafel des Fremdbestands, hier
 * **ausgefuehrt, nicht uebernommen**. Gemessen sind beide in allen 16
 * Werten gleich.
 *
 * ── Das Ergebnis: keine Ableitung ─────────────────────────────────────────
 *
 * Die CBM-4-zu-5-Tafel liegt in diesem Baum **neunfach** (gemessen ueber
 * `git grep`: uft_gcr_viterbi.c, uft_gcr_viterbi_v2.c, brother.c,
 * uft_d64_g64.c, uft_gcr_ops.c, uft_d64_parser_v3.c, uft_g64.c,
 * uft_g64_parser_v2.c, uft_g64_parser_v3.c), die Apple-6-und-2-Tafel
 * dreifach. Eine Ableitung von `encoding.c` waere die **zehnte** bzw.
 * **vierte** Kopie desselben Inhalts — die Duplikatsklasse, die dieser
 * Baum wiederholt als Defekt gemessen hat. Der MFM-Encoder existiert
 * ebenfalls (`src/core/uft_mfm_encoder.c`, seit 2026-04-18 in
 * Produktion, mit Rundlauftest).
 *
 * Offen bleibt aus `encoding.c` genau **eine** Sache, und sie steht in
 * `src/dtc_components/UEBERNAHME.md`: der FM-Encoder auf **Spur**ebene
 * (Adressmarken mit fehlenden Taktbits, Luecken, CRC-Lage). Die
 * Zellregel ist vier Zeilen und normbestimmt; der Spur-Encoder ist es
 * nicht, und er fehlt seit MF-864/P3-218.
 *
 * ── Ein Befund am Rand, und er sitzt in UFTs eigenem Kopf ─────────────────
 *
 * `src/formats/apple/uft_apple_gcr.c` sagt ueber seine 64 Diskettenbytes:
 * „Jedes hat mindestens zwei benachbarte gesetzte Bits und **nie mehr
 * als eine Null in Folge** … *Beneath Apple DOS*, Kapitel 3."
 *
 * Gemessen ist der laengste Nulllauf in dieser Tafel **2**, nicht 1 —
 * schon der erste Wert `0x96` ist `10010110`. Die **Tafel** ist richtig
 * (MF-715 hat sie gegen das Oracle `to_woz2` gehalten, 5488 Datenbytes,
 * 0 fremde Nibbles); falsch ist die **angegebene Bedingung**. Wer aus
 * ihr ableitet, erzeugt 33 Werte statt 64 — nachgerechnet. Der Satz ist
 * mit MF-1126 berichtigt.
 *
 * **Was dieser Test fuer Apple deshalb NICHT tut:** er behauptet keine
 * hinreichende Regel. Mit `>= 0x80`, „zwei Einsen in Folge" und
 * „Nulllauf <= 2" kommen **74** Werte heraus, die Tafel hat 64 — welche
 * weitere Bedingung die zehn ausschliesst, ist von hier aus nicht
 * feststellbar, und sie zu erraten waere dasselbe Fehlverhalten wie eine
 * erinnerte Tabelle. Geprueft werden die **notwendigen** Bedingungen,
 * ausdruecklich als solche.
 */

#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "uft/core/uft_zellregel.h"
#include "uft/formats/c64/uft_gcr_ops.h"

/* Fremdbestand — nur AUSGEFUEHRT, nicht uebernommen */
extern const uint8_t dtc_gcr_cbm_4to5[16];
extern const uint8_t dtc_gcr_apple_6and2[64];

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    printf("  %-4s %s%s%s\n", ok ? "ok" : "ROT", was,
           detail && *detail ? " - " : "", detail ? detail : "");
    if (ok) gruen++; else rot++;
}

/**
 * @brief Laengster Nulllauf in den unteren `breite` Bits von `w`.
 *
 * MF-1128: hier stand eine eigene Schleife. Sie war die **dritte** Kopie
 * derselben Regel im Baum (neben `test_ipf_zellstrom.c::zellregel()` und
 * `test_hfe_spurende.c`), und drei Kopien sind die Stelle, an der Drift
 * anfaengt. Gemessen wird jetzt von `uft_zellregel_messen()`; diese
 * Huelle legt nur die Bits MSB-zuerst in einen Puffer, weil die
 * Tafelkodes 5 und 10 Bit breit sind und nicht an einer Bytegrenze
 * enden.
 */
static unsigned laengster_nulllauf(uint32_t w, unsigned breite)
{
    uint8_t puf[4] = { 0, 0, 0, 0 };
    for (unsigned i = 0; i < breite; i++)
        if ((w >> (breite - 1u - i)) & 1u)
            puf[i >> 3] |= (uint8_t)(0x80u >> (i & 7u));

    uft_zellregel_t r;
    if (!uft_zellregel_messen(puf, breite, UFT_ZELL_MSB_ZUERST,
                              UFT_ZELL_UNBEGRENZT, UFT_ZELL_UNBEGRENZT, &r))
        return UINT_MAX;      /* faellt in jeder Zusage auf */
    return r.max_null;
}

/** Hat `w` zwei benachbarte gesetzte Bits in `breite` Bits? */
static int hat_zwei_einsen_in_folge(uint32_t w, unsigned breite)
{
    for (unsigned i = breite; i-- > 1;)
        if (((w >> i) & 1u) && ((w >> (i - 1)) & 1u))
            return 1;
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("GCR-Tafeln gegen ihre Bedingung (MF-1126, Stufe 1)\n");
    printf("==================================================\n");

    const uint8_t *baum = gcr_get_encode_table();
    char det[256];

    printf("\n1) CBM 4-zu-5: die Bedingung, aus der die Tafel gewaehlt ist\n");

    unsigned max_einzeln = 0;
    int alle_klein = 1, alle_verschieden = 1;
    for (int i = 0; i < 16; i++) {
        const unsigned m = laengster_nulllauf(dtc_gcr_cbm_4to5[i], 5);
        if (m > max_einzeln) max_einzeln = m;
        if (dtc_gcr_cbm_4to5[i] > 31) alle_klein = 0;
        for (int j = i + 1; j < 16; j++)
            if (dtc_gcr_cbm_4to5[i] == dtc_gcr_cbm_4to5[j])
                alle_verschieden = 0;
    }
    pruefe("16 verschiedene Kodes, jeder in 5 Bit",
           alle_verschieden && alle_klein,
           (alle_verschieden && alle_klein) ? "" : "Tafel ist keine Bijektion");
    snprintf(det, sizeof det, "laengster Nulllauf %u", max_einzeln);
    pruefe("kein Kode traegt mehr als zwei Nullen in Folge",
           max_einzeln <= 2, det);

    /* Die eigentliche Zusage: auch ueber die NAHT zweier Kodes. */
    unsigned max_paar = 0;
    int schlimm = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++) {
            const uint32_t w = ((uint32_t)dtc_gcr_cbm_4to5[i] << 5)
                               | (uint32_t)dtc_gcr_cbm_4to5[j];
            const unsigned m = laengster_nulllauf(w, 10);
            if (m > max_paar) max_paar = m;
            if (m > 2) schlimm++;
        }
    snprintf(det, sizeof det,
             "256 Paare, laengster Nulllauf %u, darueber %d",
             max_paar, schlimm);
    pruefe("auch ueber die Naht zweier Kodes nie mehr als zwei Nullen - "
           "das ist die Selbsttaktbedingung",
           max_paar <= 2 && schlimm == 0, det);

    printf("\n2) CBM 4-zu-5: zwei unabhaengige Haende, dieselbe Tafel\n");
    int abweichend = 0, erste = -1;
    for (int i = 0; i < 16; i++)
        if (baum[i] != dtc_gcr_cbm_4to5[i]) {
            abweichend++;
            if (erste < 0) erste = i;
        }
    if (abweichend)
        snprintf(det, sizeof det,
                 "%d abweichend, erste bei %d: Baum 0x%02X, fremd 0x%02X",
                 abweichend, erste, baum[erste], dtc_gcr_cbm_4to5[erste]);
    else
        snprintf(det, sizeof det, "16 von 16 gleich");
    pruefe("gcr_get_encode_table() und dtc_gcr_cbm_4to5 sind identisch",
           abweichend == 0, det);

    printf("\n3) Apple 6-und-2: die NOTWENDIGEN Bedingungen (keine "
           "hinreichende Regel behauptet)\n");
    int hoch = 1, zwei = 1, aufsteigend = 1, verschieden = 1;
    unsigned max_a = 0;
    for (int i = 0; i < 64; i++) {
        const uint8_t b = dtc_gcr_apple_6and2[i];
        if (b < 0x80) hoch = 0;
        if (!hat_zwei_einsen_in_folge(b, 8)) zwei = 0;
        const unsigned m = laengster_nulllauf(b, 8);
        if (m > max_a) max_a = m;
        if (i && dtc_gcr_apple_6and2[i - 1] >= b) aufsteigend = 0;
        for (int j = i + 1; j < 64; j++)
            if (b == dtc_gcr_apple_6and2[j]) verschieden = 0;
    }
    pruefe("64 verschiedene Werte, streng aufsteigend",
           verschieden && aufsteigend, "");
    pruefe("jeder Wert hat das hohe Bit gesetzt", hoch, "");
    pruefe("jeder Wert hat zwei benachbarte Einsen", zwei, "");
    snprintf(det, sizeof det,
             "laengster Nulllauf %u - der Kopf von uft_apple_gcr.c sagte "
             "bis MF-1126 \"nie mehr als eine\"", max_a);
    pruefe("der laengste Nulllauf ist 2, nicht 1", max_a == 2, det);

    int d5 = 0, aa = 0;
    for (int i = 0; i < 64; i++) {
        if (dtc_gcr_apple_6and2[i] == 0xD5) d5 = 1;
        if (dtc_gcr_apple_6and2[i] == 0xAA) aa = 1;
    }
    pruefe("die Vorspannmarken 0xD5 und 0xAA stehen NICHT in der "
           "Datentafel", !d5 && !aa, "");

    printf("\n%d gruen, %d rot\n", gruen, rot);
    if (!rot)
        printf("Ergebnis: die Tafeln des Fremdbestands halten die "
               "Selbsttaktbedingung und sind mit denen des Baums "
               "identisch - eine Ableitung waere die zehnte bzw. vierte "
               "Kopie.\n");
    return rot ? 1 : 0;
}
