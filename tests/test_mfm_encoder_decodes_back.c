/**
 * @file test_mfm_encoder_decodes_back.c
 * @brief Kodiert der MFM-Encoder etwas, das der Dekoder wieder liest? (MF-539)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `src/core/uft_mfm_encoder.c` ist ein vollstaendiger IBM-System-34-Encoder:
 * Zell-Ausgabe ueber `put_cell()`, echte `0x4489`-Synchronmarken, CRC16-CCITT
 * ueber Adressmarke und Nutzdaten. Er hatte bis heute **keinen einzigen
 * Test** — gemessen mit `ls tests/ | grep -i mfm_enc`, Ergebnis leer.
 *
 * Aufgefallen ist das auf dem Umweg ueber MF-538. `ADF -> HFE` erzeugt eine
 * Datei, aus der `HFE -> ADF` nichts zurueckgewinnt. Der Vergleich mit einer
 * echten Greaseweazle-Aufnahme (tests/corpus_free/gw_amigados.hfe) zeigte,
 * woran es liegt:
 *
 *                              echte HFE      unsere ADF->HFE
 *      Sync 0x4489 je Seite         22                     0
 *      haeufigstes Byte           0x55 (12498)    0x4E (6712)
 *      rohe Nullbytes                0 von 12792   5969 von 12800
 *
 * `0x55` ist MFM-kodiertes `0x00`. `0x4E` ist das IBM-Fuellbyte **im
 * Klartext**. Und mehr als drei Nullbits am Stueck kann ein gueltiger
 * MFM-Strom gar nicht enthalten — 5969 Nullbytes sind also kein falscher
 * Dialekt, sondern gar kein MFM.
 *
 * Der Grund steht in `uftc_convert_sectors_to_hfe()`
 * (src/formats/uft_format_convert_bitstream.c:757 ff.): dort werden
 * `0x4E`, `0x00`, `0xC2`, `0xA1`, `0xFE`, `0xFB` als **rohe Bytes** in den
 * Spurpuffer geschrieben. Es gibt keinen Kodierschritt. Die CRC-Felder
 * bleiben `0x00 0x00` — sie werden nie berechnet. Trotzdem laufen
 * `sectors_converted++` und `tracks_converted++` bedingungslos durch.
 *
 * Es existiert also eine handgeschriebene, kaputte Zweitfassung neben einem
 * richtigen Encoder, den niemand ruft.
 *
 * ── Was dieser Test beweist ──────────────────────────────────────────────
 *
 * Nicht "der Encoder sieht richtig aus", sondern: **schreibt er etwas, das
 * der vorhandene Dekoder wieder liest?**
 *
 *      uft_mfm_encode_track()   -> Zellenstrom
 *      uft_mfm_decode_track()   -> Sektoren zurueck
 *      Vergleich                -> CHRN, CRC-Flags, Nutzdaten byteweise
 *
 * Der Dekoder ist die benannte Referenz: er ist derselbe, der reale
 * Aufnahmen liest, und er wurde nicht fuer diesen Test veraendert. Kaeme
 * hier etwas anderes heraus als hineingegeben wurde, waere entweder der
 * Encoder falsch oder der Dekoder — beides ein Befund, keiner davon
 * hinnehmbar.
 *
 * Der Test prueft ausserdem die Eigenschaft, an der die kaputte Zweitfassung
 * gescheitert ist: der Ausgabestrom muss `0x4489`-Marken enthalten, und
 * zwar drei je Adressmarke, also sechs je Sektor. Findet er null, ist der
 * Strom nicht kodiert — genau der Zustand, in dem die HFE-Ausgabe war.
 */

#include "uft/uft_types.h"
#include "uft/uft_mfm_encoder.h"
#include "uft/flux/uft_mfm_sector_parser.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SPT        18
#define SECSZ      512
#define TRACK_CAP  32768

static int failures;

static void fail(const char *fmt, ...)
{
    va_list ap;
    printf("  FAIL ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    failures++;
}

/* Zaehlt 0x4489 im Zellenstrom (MSB-first, so gibt der Encoder aus). */
static int count_sync(const uint8_t *buf, size_t bytes)
{
    uint16_t v = 0;
    int n = 0;
    for (size_t i = 0; i < bytes; i++) {
        for (int b = 7; b >= 0; b--) {
            v = (uint16_t)((v << 1) | ((buf[i] >> b) & 1));
            if (v == 0x4489) n++;
        }
    }
    return n;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Kodiert der MFM-Encoder etwas Lesbares? (MF-539) ===\n");

    /* Quelldaten: je Sektor ein eigenes Muster, damit eine Verwechslung
     * zweier Sektoren auffaellt und nicht als Erfolg durchgeht. */
    static uint8_t payload[SPT][SECSZ];
    for (int s = 0; s < SPT; s++)
        for (int i = 0; i < SECSZ; i++)
            payload[s][i] = (uint8_t)(s * 7 + i * 3 + (i >> 5));

    static uft_sector_t secs[SPT];
    memset(secs, 0, sizeof(secs));
    for (int s = 0; s < SPT; s++) {
        secs[s].id.cylinder  = 5;
        secs[s].id.head      = 1;
        secs[s].id.sector    = (uint8_t)(s + 1);
        secs[s].id.size_code = 2;          /* 2 = 512 Byte */
        secs[s].data         = payload[s];
        secs[s].data_len     = SECSZ;
        secs[s].data_size    = SECSZ;
    }

    static uint8_t cells[TRACK_CAP];
    uft_mfm_encode_params_t p = UFT_MFM_PARAMS_DEFAULT_HD;
    size_t n = uft_mfm_encode_track(secs, SPT, 5, 1, &p, cells, sizeof(cells));

    printf("  kodiert: %zu Byte Zellenstrom\n", n);
    if (n == 0) {
        fail("uft_mfm_encode_track() lieferte 0 — nichts zu pruefen");
        printf("\nFEHLGESCHLAGEN (%d Abweichungen)\n", failures);
        return 1;
    }

    /* 1. Ist es ueberhaupt ein kodierter Strom?
     *
     * Drei A1-Synchronmarken je Adressmarke, zwei Adressmarken je Sektor
     * (IDAM + DAM) = 6 * 18 = 108. Genau diese Zahl ist bei der kaputten
     * HFE-Fassung null. */
    int sync = count_sync(cells, n);
    printf("  Sync 0x4489: %d (erwartet %d)\n", sync, 6 * SPT);
    if (sync < 6 * SPT)
        fail("zu wenige Synchronmarken — der Strom ist nicht MFM-kodiert");

    /* 2. Kein Nullbyte. MFM erlaubt nie mehr als drei Nullbits am Stueck,
     *    ein volles Nullbyte kann also nicht vorkommen. */
    size_t zeros = 0;
    for (size_t i = 0; i < n; i++) if (cells[i] == 0) zeros++;
    printf("  Nullbytes: %zu von %zu\n", zeros, n);
    if (zeros)
        fail("%zu Nullbytes im Zellenstrom — MFM kann das nicht erzeugen", zeros);

    /* 3. Der eigentliche Beweis: zurueck durch den vorhandenen Dekoder. */
    static uint8_t pool[SPT * SECSZ * 2];
    static uft_mfm_sector_t got[64];
    size_t found = uft_mfm_decode_track(cells, n * 8, pool, sizeof(pool),
                                        got, 64, NULL);
    printf("  dekodiert: %zu Sektoren (erwartet %d)\n", found, SPT);
    if (found != SPT)
        fail("Sektorzahl weicht ab");

    int good = 0, id_bad = 0, data_bad = 0, content_bad = 0;
    for (size_t i = 0; i < found && i < SPT; i++) {
        const uft_mfm_sector_t *g = &got[i];
        if (!g->id_crc_ok)   { id_bad++;   continue; }
        if (!g->data_crc_ok) { data_bad++; continue; }
        if (g->cylinder != 5 || g->head != 1 || g->size_code != 2) {
            fail("Sektor %zu: CHRN falsch (C=%u H=%u N=%u)",
                 i, g->cylinder, g->head, g->size_code);
            continue;
        }
        int idx = g->sector - 1;
        if (idx < 0 || idx >= SPT) {
            fail("Sektor %zu: R=%u ausserhalb 1..%d", i, g->sector, SPT);
            continue;
        }
        if (g->data_len != SECSZ ||
            memcmp(pool + g->data_offset, payload[idx], SECSZ) != 0) {
            content_bad++;
            continue;
        }
        good++;
    }

    printf("  davon mit gueltiger ID-CRC, Daten-CRC und gleichem Inhalt: %d\n",
           good);
    if (id_bad)      fail("%d Sektoren mit falscher ID-CRC", id_bad);
    if (data_bad)    fail("%d Sektoren mit falscher Daten-CRC", data_bad);
    if (content_bad) fail("%d Sektoren mit abweichendem Inhalt", content_bad);
    if (good != SPT) fail("nur %d von %d Sektoren vollstaendig korrekt",
                          good, SPT);

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
