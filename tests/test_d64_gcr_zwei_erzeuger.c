/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d64_gcr_zwei_erzeuger.c
 * @brief Zwei GCR-Spurerzeuger, ein Ergebnis? (MF-862)
 *
 * ── Die Frage ────────────────────────────────────────────────────────
 *
 * Der Baum hat ZWEI Erzeuger fuer GCR-Spuren aus D64-Sektoren:
 *
 *   `uft_d64_g64.c::build_gcr_track()`   angeschlossen; der ANGEBOTENE
 *                                        Wandlungspfad D64→G64 laeuft
 *                                        hierueber (MF-532/533: verlustfrei
 *                                        gemessen)
 *   `uft_d64_writer.c::d64_write_track_gcr()`
 *                                        KEIN Aufrufer ausserhalb des
 *                                        eigenen Moduls (P3-116)
 *
 * Der unbenutzte ist dabei der REICHERE: Gap- und Sync-Laengen sind je
 * Spur einstellbar, eine eigene Sektorordnung ist moeglich, und er
 * liefert einen Ergebnisdatensatz je Spur. Das ist genau, was
 * Kopierschutz braucht (lange Spuren, eigene Syncs) und was der
 * angeschlossene nicht kann.
 *
 * ── Warum dieser Test VOR jeder Verdrahtung kommt ────────────────────
 *
 * Den angebotenen Pfad auf den reicheren Erzeuger umzustellen heisst,
 * eine **gemessen verlustfreie** Wandlung anzufassen. Das darf nur, wer
 * vorher zeigt, dass dabei dieselben Bytes herauskommen.
 *
 * Beide legen dieselbe Spurstruktur an — 5×`0xFF` Sync, Kopf, 9×`0x55`
 * Gap, 5×`0xFF` Sync, Daten, 9×`0x55` Gap —, aber nur `build_gcr_track`
 * fuellt die Spur danach auf die Zonenkapazitaet auf. Ob der Inhalt
 * davor Byte fuer Byte gleich ist, ist eine Messung, keine Annahme.
 */
#include "uft/uft_d64_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Aus src/formats/c64/uft_d64_g64.c — nicht statisch. */
size_t build_gcr_track(const uint8_t **sectors, int num_sectors,
                       uint8_t *gcr_output, int track,
                       const uint8_t *disk_id, uint8_t gap_fill);

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define SPUR      1
#define SEKTOREN 21
#define PUFFER   (16u * 1024u)

/** Erkennbare Sektordaten: Sektor s ist mit (0x40 + s) gefuellt. */
static void sektoren_bauen(uint8_t *flach)
{
    for (int s = 0; s < SEKTOREN; s++)
        memset(flach + (size_t)s * 256, (uint8_t)(0x40 + s), 256);
}

TEST(beide_erzeuger_liefern_denselben_inhalt)
{
    static const uint8_t DISK_ID[2] = { 0x30, 0x30 };

    uint8_t *flach = (uint8_t *)calloc(SEKTOREN, 256);
    ASSERT(flach != NULL);
    sektoren_bauen(flach);

    const uint8_t *zeiger[SEKTOREN];
    for (int s = 0; s < SEKTOREN; s++) zeiger[s] = flach + (size_t)s * 256;

    /* --- der angeschlossene Erzeuger --- */
    uint8_t *a = (uint8_t *)calloc(1, PUFFER);
    ASSERT(a != NULL);
    size_t a_len = build_gcr_track(zeiger, SEKTOREN, a, SPUR, DISK_ID, 0x55);
    ASSERT(a_len > 0);

    /* --- der unbenutzte, reichere --- */
    d64_writer_config_t cfg = (d64_writer_config_t)D64_WRITER_CONFIG_DEFAULT;
    cfg.disk_id[0] = DISK_ID[0];
    cfg.disk_id[1] = DISK_ID[1];
    d64_writer_t *w = d64_writer_create(&cfg);
    ASSERT(w != NULL);

    uint8_t *b = (uint8_t *)calloc(1, PUFFER);
    ASSERT(b != NULL);
    size_t b_len = 0;
    d64_track_result_t r;
    memset(&r, 0, sizeof r);
    int rc = d64_write_track_gcr(w, SPUR, flach, SEKTOREN, b, &b_len, &r);

    if (rc != 0) {
        printf("\n      d64_write_track_gcr gab %d: %s\n      ",
               rc, r.error_msg);
        _fail++;
        goto ende;
    }

    /* Der reichere fuellt NICHT auf Zonenkapazitaet auf — er liefert die
     * belegte Laenge. Verglichen wird deshalb sein Bereich. */
    if (b_len > a_len) {
        printf("\n      Erzeuger B liefert %zu Byte, A nur %zu\n      ",
               b_len, a_len);
        _fail++;
        goto ende;
    }

    size_t erste_abweichung = (size_t)-1;
    for (size_t i = 0; i < b_len; i++) {
        if (a[i] != b[i]) { erste_abweichung = i; break; }
    }

    if (erste_abweichung != (size_t)-1) {
        size_t i = erste_abweichung;
        printf("\n      erste Abweichung bei Byte %zu von %zu\n"
               "        angeschlossen: %02X %02X %02X %02X\n"
               "        unbenutzt    : %02X %02X %02X %02X\n      ",
               i, b_len,
               a[i], a[i + 1], a[i + 2], a[i + 3],
               b[i], b[i + 1], b[i + 2], b[i + 3]);
        _fail++;
    }

ende:
    d64_writer_destroy(w);
    free(a); free(b); free(flach);
}

TEST(der_angeschlossene_fuellt_auf_zonenkapazitaet)
{
    /* Gegenprobe: der Unterschied in der LAENGE ist gewollt und muss
     * bleiben — eine G64-Spur traegt ihre volle Kapazitaet. Wer den
     * reicheren Erzeuger verdrahtet, muss die Auffuellung ergaenzen. */
    static const uint8_t DISK_ID[2] = { 0x30, 0x30 };
    uint8_t *flach = (uint8_t *)calloc(SEKTOREN, 256);
    ASSERT(flach != NULL);
    sektoren_bauen(flach);

    const uint8_t *zeiger[SEKTOREN];
    for (int s = 0; s < SEKTOREN; s++) zeiger[s] = flach + (size_t)s * 256;

    uint8_t *a = (uint8_t *)calloc(1, PUFFER);
    ASSERT(a != NULL);
    size_t a_len = build_gcr_track(zeiger, SEKTOREN, a, SPUR, DISK_ID, 0x55);

    /* Zone 0 traegt 7692 Byte. Die genaue Zahl steht in der
     * Kapazitaetstabelle; hier genuegt: deutlich mehr als die Nutzdaten. */
    ASSERT(a_len > (size_t)SEKTOREN * 325);

    free(a); free(flach);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== D64: zwei GCR-Erzeuger im Vergleich (MF-862) ===\n");
    RUN(beide_erzeuger_liefern_denselben_inhalt);
    RUN(der_angeschlossene_fuellt_auf_zonenkapazitaet);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
