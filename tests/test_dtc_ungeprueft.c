/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_dtc_ungeprueft.c
 * @brief Die vier dtc_components-Module ohne eigene Abnahme — was sie
 *        gemessen tun, und warum keines an einen Produktpfad darf
 *        (MF-1109)
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * Der Eigentümer hat gebeten, die Fähigkeiten aus `code-schnipsel.zip`
 * **im GUI zu- und abschaltbar** zu machen. MF-1100 hat das Paket unter
 * `src/dtc_components/` aufgenommen, hinter dem Bau-Schalter
 * `UFT_WITH_DTC_COMPONENTS` (Vorgabe OFF), und dabei gemessen: von zehn
 * Modulen berührt die **mitgelieferte Testreihe sechs**. Vier nicht:
 * `ipf.c`, `match.c`, `protection.c`, `track.c`.
 *
 * `docs/orphan_baseline.txt` hält dazu die Regel fest, die dieser Test
 * jetzt einlöst: *„Ohne eigene Abnahme wird keines davon an einen
 * Produktpfad gehaengt — genau das waere die Wette der fuenf
 * fabrizierten Parser."* Ein GUI-Schalter IST ein Produktpfad. Also
 * kommt die Abnahme zuerst.
 *
 * **Sie fällt für alle vier negativ aus.** Das ist das Ergebnis, nicht
 * ein Zwischenstand — und dieser Test hält es fest, damit die Frage
 * nicht in sechs Monaten erneut aus dem Gedächtnis beantwortet wird.
 *
 * ── Was gemessen wurde ───────────────────────────────────────────────
 *
 * **(1) `track.c` — `header_ok` kann an keinem spezifikationsgerechten
 * Sektor 1 werden.** Der Scanner rechnet
 * `dtc_crc16_ccitt(b+i+3, 7, 0xffff) == 0`, also **ab dem `FE`**. IBM-MFM
 * nimmt die drei `A1` in die CRC auf; UFTs eigener Encoder sagt das
 * wörtlich (`src/core/uft_mfm_encoder.c:22`: *„running CRC starts at the
 * first 0xA1 of each address mark"*) und tut es auch (Z. 141-148).
 * Gemessen an einem Adressfeld, das nach dieser Regel gebaut ist:
 * Restprüfung über `A1 A1 A1 FE C H R N CRC` = **0x0000**, Restprüfung
 * ab `FE` = **0x90DC**. Der Scanner findet den Sektor mit der richtigen
 * C/H/S — und meldet `header_ok = 0`. Klasse MF-1000: ein Prüfbit, das
 * zuverlässig null meldet.
 *
 * **(1b) Und `data_crc` trägt keine Daten-CRC.** Das Feld bekommt
 * `dtc_crc16_ccitt(b+i+3, 5, 0xffff)` — einen Zwischenwert über
 * `FE C H R N`. Gemessen ist das 0x5858, während die Kopf-CRC in der
 * Datei 0x60B6 lautet. Weder das eine noch das andere, unter einem
 * Namen, der das dritte sagt.
 *
 * **(2) `match.c` — Erfolg gemeldet, drei Felder nie beschrieben.**
 * `dtc_track_match()` setzt `r->compared_bits` **nur** im Zweig
 * `if(d<best)`. Bei `bit_count == 0` läuft die Schleife nie. Gemessen
 * mit einem Giftmuster `0xCD` im Ergebnis: Rückgabe **0 (Erfolg)**,
 * `differing_bits = SIZE_MAX`, `compared_bits` unverändert
 * **0xCDCDCDCDCDCDCDCD**, und daraus gerechnet `similarity = -0.24` —
 * eine Ähnlichkeit außerhalb von [0,1].
 *
 * **(3) `protection.c` — die Aufzählung ist um drei Werte breiter als
 * ihr Erzeuger.** Der Header nennt fünf Arten (`LONG_TRACK`,
 * `WEAK_BITS`, `ILLEGAL_MFM`, `VORPAL`, `VMAX`);
 * `dtc_detect_protection()` setzt **zwei**. Gemessen über 4096
 * Zufallsspuren mit variiertem `nominal`: gesehene Flaggen `0x05`.
 * `WEAK_BITS`, `VORPAL` und `VMAX` sind über **jede** Eingabe
 * unerreichbar. Das ist nicht dasselbe wie „fehlt": die
 * Vorpal-/V-Max-Tafeln liegen in `encoding.c`, und `dtc_weak_regions()`
 * liegt in `match.c` — **es fehlt die Verbindung**, und genau die
 * verspricht der Aufzählungstyp.
 *
 * **(4) `ipf.c` — die CRC wird gelesen, nicht geprüft.** Zwei Blöcke mit
 * `crc = 0xDEADBEEF` gehen unbeanstandet durch `dtc_ipf_list_chunks()`;
 * `id` ist in jedem Block 0, weil die Funktion es unbedingt nullt.
 * Dazu der Zusammenhang, der hier entscheidet: **UFT hat seit MF-1079
 * einen IPF-Leser auf T1**, an zwei echten SPS-Erhaltungsabbildern
 * abgenommen, mit Zellsummen, die `datasize` in 1618 von 1618 Blöcken
 * treffen. Dieses Modul ist also nicht nur ungeprüft, sondern ersetzt
 * nichts.
 *
 * ── Was dieser Test ausdrücklich NICHT tut ───────────────────────────
 *
 * Er **repariert nichts**. `tests/CMakeLists.txt` sagt bei
 * `UFT_WITH_DTC_COMPONENTS` wörtlich: *„Die Dateien unter
 * `src/dtc_components/` werden nicht angefasst — wer sie aendert, macht
 * aus einem Beleg eine Ableitung."* Der Bestand bleibt, wie er kam; er
 * ist der Beleg dafür, WAS zugesagt war. Dieselbe Haltung wie MF-699
 * (beschriftet statt repariert) und die Form von
 * `tests/test_rcpmfs_ist_kein_dateiformat.c` (MF-1035).
 *
 * Er sagt auch **nicht**, dass die Vorpal-Tafel falsch ist. Gemessen ist
 * nur: `dtc_gcr_vorpal_4to5` ist eine **Permutation derselben 16
 * Codewörter** wie `dtc_gcr_cbm_4to5` (beide Mengen sortiert:
 * 09 0a 0b 0d 0e 0f 12 13 15 16 17 19 1a 1b 1d 1e) — was für einen
 * GCR-Schutz plausibel ist, der dieselben gültigen Codewörter anders
 * zuordnet. Plausibel und ungeprüft sind zwei verschiedene Dinge; eine
 * benannte Vorpal-Referenz liegt nicht vor.
 *
 * Und er baut die vier Module **selbst**, statt an
 * `UFT_WITH_DTC_COMPONENTS` zu hängen. Sonst wäre er ein Test, der in
 * CI nie läuft und damit nicht rot werden kann — MF-1000 / Tor 64, die
 * Falle, die dieser Baum bei `test_libdsk_formats.c` schon bezahlt hat.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "dtc_components.h"

static int g_fail = 0;
static int g_zusagen = 0;

#define PRUEFE(bed, text)                                               \
    do {                                                                \
        ++g_zusagen;                                                    \
        if (!(bed)) {                                                   \
            printf("  FAIL @ %d: %s\n", __LINE__, (text));              \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

/* ── 1. track.c: die Kopf-CRC-Spanne ───────────────────────────────── */

static void track_crc_spanne(void)
{
    /* Ein IBM-MFM-Adressfeld in dekodierten Bytes, gebaut nach der Regel,
     * die UFTs eigener Encoder umsetzt: 12x 0x00, 3x A1, FE, C, H, R, N,
     * dann die CRC-16-CCITT (init 0xFFFF) ueber A1 A1 A1 FE C H R N. */
    uint8_t s[64];
    size_t n = 0;
    for (int k = 0; k < 12; k++) s[n++] = 0x00;
    const size_t am = n;
    s[n++] = 0xA1; s[n++] = 0xA1; s[n++] = 0xA1;
    s[n++] = 0xFE;
    s[n++] = 7; s[n++] = 1; s[n++] = 5; s[n++] = 2;   /* C H R N */
    const uint16_t crc = dtc_crc16_ccitt(s + am, 8, 0xFFFF);
    s[n++] = (uint8_t)(crc >> 8);
    s[n++] = (uint8_t)(crc & 0xFF);
    for (int k = 0; k < 22; k++) s[n++] = 0x4E;

    /* Der Strom ist spezifikationsgerecht — das ist die Voraussetzung,
     * ohne die der Rest nichts sagte. Rest ueber die volle Spanne = 0. */
    PRUEFE(dtc_crc16_ccitt(s + am, 10, 0xFFFF) == 0,
           "der gebaute Strom muss die IBM-MFM-CRC-Regel erfuellen "
           "(Rest ueber A1 A1 A1 FE C H R N CRC == 0)");

    /* Und die Gegenprobe, ohne die die erste zu wenig sagt: ab FE
     * gerechnet geht sie NICHT auf. Genau das rechnet track.c. */
    PRUEFE(dtc_crc16_ccitt(s + am + 3, 7, 0xFFFF) != 0,
           "ab FE gerechnet darf der Rest NICHT aufgehen — sonst waere "
           "der Befund keiner");

    dtc_sector out[8];
    memset(out, 0, sizeof out);
    const size_t k = dtc_scan_mfm_ibm(s, n, out, 8);

    PRUEFE(k == 1, "der Scanner muss genau ein Adressfeld finden");
    if (k >= 1) {
        /* Was er richtig macht, gehoert mitgeprueft — sonst liest sich
         * der Befund breiter, als er ist. */
        PRUEFE(out[0].cylinder == 7 && out[0].head == 1 && out[0].sector == 5,
               "C/H/S werden richtig gelesen");

        /* Der Befund. */
        PRUEFE(out[0].header_ok == 0,
               "header_ok meldet 0 an einem spezifikationsgerechten "
               "Sektor — die drei A1 fehlen in der CRC-Spanne (MF-1109)");

        /* Und das Feld, das etwas anderes heisst, als es traegt. */
        PRUEFE(out[0].data_crc != crc,
               "data_crc traegt NICHT die Kopf-CRC der Datei");
        PRUEFE(out[0].data_crc == dtc_crc16_ccitt(s + am + 3, 5, 0xFFFF),
               "data_crc ist der Zwischenwert ueber FE C H R N");
        PRUEFE(out[0].data_ok == 0 && out[0].bit_length == 0,
               "der Datensatz wird gar nicht gesucht — data_ok und "
               "bit_length bleiben 0");
    }
}

/* ── 2. match.c: Erfolg ohne Messung ───────────────────────────────── */

static void match_meldet_erfolg_ohne_messung(void)
{
    uint8_t leer_a[1] = { 0 }, leer_b[1] = { 0 };
    dtc_bits a, b;
    dtc_bits_init(&a, leer_a, 0, 0);
    dtc_bits_init(&b, leer_b, 0, 0);

    /* Giftmuster: was die Funktion nicht schreibt, bleibt sichtbar. */
    dtc_match r;
    memset(&r, 0xCD, sizeof r);
    const size_t gift = r.compared_bits;

    const int rc = dtc_track_match(&a, &b, 8, &r);

    PRUEFE(rc == 0,
           "die Funktion meldet Erfolg — das ist der Kern des Befunds");
    PRUEFE(r.differing_bits == (size_t)-1,
           "differing_bits bleibt SIZE_MAX, also der Startwert und keine "
           "Messung");
    PRUEFE(r.compared_bits == gift,
           "compared_bits wird NIE geschrieben (Giftmuster unveraendert) "
           "— der Aufrufer rechnet mit uninitialisiertem Speicher");

    /* Gegenprobe: bei nichtleeren Feldern arbeitet die Funktion. Ohne
     * sie waere der Befund ein Pauschalurteil ueber die ganze Funktion. */
    uint8_t va[8], vb[8];
    memset(va, 0xA5, sizeof va);
    memset(vb, 0xA5, sizeof vb);
    dtc_bits c, d;
    dtc_bits_init(&c, va, sizeof(va) * 8, 0);
    dtc_bits_init(&d, vb, sizeof(vb) * 8, 0);
    dtc_match r2;
    memset(&r2, 0xCD, sizeof r2);
    PRUEFE(dtc_track_match(&c, &d, 4, &r2) == 0, "gleiche Spuren: rc 0");
    PRUEFE(r2.differing_bits == 0 && r2.best_shift == 0,
           "gleiche Spuren: 0 Unterschiede bei Verschiebung 0");
    PRUEFE(r2.compared_bits == sizeof(va) * 8,
           "gleiche Spuren: compared_bits wird hier sehr wohl gesetzt");
}

/* ── 3. protection.c: drei Flaggen, die nie kommen ─────────────────── */

static void protection_kann_drei_flaggen_nie_setzen(void)
{
    unsigned gesehen = 0;
    uint8_t muster[64];

    /* Deterministisch statt zufaellig — ein Tor, dessen Ergebnis vom
     * Zufall abhaengt, ist kein Tor. */
    for (unsigned welle = 0; welle < 256; welle++) {
        for (size_t i = 0; i < sizeof muster; i++)
            muster[i] = (uint8_t)((welle * 31u + i * 17u) & 0xFF);
        dtc_bits t;
        dtc_bits_init(&t, muster, sizeof(muster) * 8, 0);
        /* nominal klein => LONG_TRACK erreichbar; nominal gross => nicht */
        gesehen |= dtc_detect_protection(&t, 8, 3, 3);
        gesehen |= dtc_detect_protection(&t, 4096, 3, 3);
        gesehen |= dtc_detect_protection(&t, 0, 1, 1);
    }

    PRUEFE((gesehen & DTC_PROT_LONG_TRACK) != 0,
           "LONG_TRACK ist erreichbar");
    PRUEFE((gesehen & DTC_PROT_ILLEGAL_MFM) != 0,
           "ILLEGAL_MFM ist erreichbar");

    PRUEFE((gesehen & DTC_PROT_WEAK_BITS) == 0,
           "WEAK_BITS wird ueber KEINE Eingabe gesetzt — die Funktion "
           "kennt die Flagge nicht (MF-1109)");
    PRUEFE((gesehen & DTC_PROT_VORPAL) == 0,
           "VORPAL wird ueber KEINE Eingabe gesetzt");
    PRUEFE((gesehen & DTC_PROT_VMAX) == 0,
           "VMAX wird ueber KEINE Eingabe gesetzt");

    /* Und die Tafeln, die dazu gehoeren wuerden, liegen da — gemessen,
     * damit "fehlt" und "nicht verbunden" nicht verwechselt werden. */
    uint32_t cbm_menge = 0, vorpal_menge = 0;
    for (int i = 0; i < 16; i++) {
        cbm_menge    |= 1u << (dtc_gcr_cbm_4to5[i]    & 31);
        vorpal_menge |= 1u << (dtc_gcr_vorpal_4to5[i] & 31);
    }
    PRUEFE(cbm_menge == vorpal_menge,
           "die Vorpal-Tafel ist eine Permutation DERSELBEN 16 "
           "GCR-Codewoerter wie die CBM-Tafel — plausibel, aber ohne "
           "benannte Referenz nicht abgenommen");
}

/* ── 4. ipf.c: CRC gelesen, nicht geprueft ─────────────────────────── */

static void ipf_prueft_die_crc_nicht(void)
{
    const char *pfad = "uft_mf1109_ipf.bin";
    FILE *f = fopen(pfad, "wb");
    if (!f) {
        printf("  (uebersprungen: kein Schreibrecht im Arbeitsverzeichnis)\n");
        return;
    }
    /* Zwei Bloecke, Groesse stimmt, CRC ist absichtlich Unsinn. */
    for (int k = 0; k < 2; k++) {
        const uint8_t h[12] = { 'C','A','P','S', 0,0,0,16,
                                0xDE,0xAD,0xBE,0xEF };
        const uint8_t rest[4] = { 1, 2, 3, 4 };
        fwrite(h, 1, sizeof h, f);
        fwrite(rest, 1, sizeof rest, f);
    }
    fclose(f);

    f = fopen(pfad, "rb");
    if (!f) { printf("  (uebersprungen: Datei nicht lesbar)\n"); return; }

    PRUEFE(dtc_ipf_probe(f) == 1, "die CAPS-Kennung wird erkannt");

    dtc_ipf_chunk ch[8];
    memset(ch, 0, sizeof ch);
    const size_t n = dtc_ipf_list_chunks(f, ch, 8);

    PRUEFE(n == 2, "beide Bloecke werden gelistet — OBWOHL ihre CRC "
                   "0xDEADBEEF lautet: geprueft wird sie nicht (MF-1109)");
    if (n >= 1) {
        PRUEFE(ch[0].crc == 0xDEADBEEFu,
               "die falsche CRC kommt unveraendert zurueck");
        PRUEFE(ch[0].id == 0 && (n < 2 || ch[1].id == 0),
               "id ist in jedem Block 0 — die Funktion nullt es "
               "unbedingt, sie liest es nicht");
    }
    fclose(f);
    remove(pfad);
}

int main(void)
{
    printf("=== dtc_components: die vier ohne Abnahme (MF-1109) ===\n");

    track_crc_spanne();
    match_meldet_erfolg_ohne_messung();
    protection_kann_drei_flaggen_nie_setzen();
    ipf_prueft_die_crc_nicht();

    if (g_fail == 0)
        printf("test_dtc_ungeprueft: %d Zusagen, 0 Fehler\n", g_zusagen);
    else
        printf("test_dtc_ungeprueft: %d Zusagen, %d Fehler\n",
               g_zusagen, g_fail);

    printf("\nWas dieser Test BEDEUTET: die vier Module halten ihre eigene\n"
           "Zusage nicht. Keines davon darf an einen Produktpfad — auch\n"
           "nicht hinter einem GUI-Schalter. Was sie tun SOLLTEN, kann\n"
           "UFT an drei von vier Stellen bereits belegt: IBM-MFM-Koepfe\n"
           "ueber uft_mfm_encoder.c (MF-938), IPF ueber den T1-Leser aus\n"
           "MF-1079, Schutz-Signale ueber src/protection (MF-508).\n"
           "NICHT geprueft: ob die Vorpal-/V-Max-ZUORDNUNG stimmt — dafuer\n"
           "fehlt eine benannte Referenz.\n");
    return g_fail == 0 ? 0 : 1;
}
