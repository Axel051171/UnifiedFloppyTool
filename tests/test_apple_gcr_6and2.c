/**
 * @file test_apple_gcr_6and2.c
 * @brief Apple-II-GCR: 6-and-2, 4-and-4 und der Spur-Abtaster (MF-715)
 *
 * Der Baum konnte bis MF-715 keinen Apple-Sektor aus einem Bitstrom
 * zurueckholen. Dieser Test deckt die neue Einheit
 * `src/formats/apple/uft_apple_gcr.c` ab.
 *
 * ── Was hier geprueft wird, und was woanders ────────────────────────────
 *
 * Dieser Test baut seine Eingaben SELBST und ist damit fuer sich
 * genommen zirkulaer — er prueft die Rechenwege (laufende Pruefsumme,
 * Bit-Zusammensetzung, Feldsuche), nicht die Wirklichkeit.
 *
 * Der Beweis gegen die Wirklichkeit ist gefahren, aber er braucht ein
 * Fremdwerkzeug und laeuft darum nicht in CI. Gemessen (MF-715) mit dem
 * registrierten Oracle `to_woz2` (`docs/ORACLES.md`):
 *
 *     Eingabe : 143 360-Byte-DSK, deterministisches Fuellmuster
 *     Weg     : to_woz2 -> WOZ 2.0 -> woz_get_track_525() ->
 *               uft_apple_gcr_scan_track() -> Vergleich gegen die DSK
 *     Ergebnis: 560 von 560 Sektoren gefunden, Adress-Pruefsumme 560/560,
 *               Datenfeld dekodiert und **byteidentisch** 560/560
 *
 * Nebenbei fiel die Zuordnung physisch->logisch heraus, gemessen statt
 * angenommen:
 *
 *     0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
 *
 * Das ist genau die Umkehrung der DOS-3.3-Tabelle aus `a8rawconv`
 * (`diska2.cpp:3-5`) — eine unabhaengige Bestaetigung, die niemand
 * eingebaut hat.
 */

#include "uft/formats/apple/uft_apple_gcr.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Dieselben 64 Diskettenbytes wie im Kodierer — hier, um eine Eingabe
 * zu BAUEN. Dass sie stimmen, sagt nicht dieser Test, sondern die
 * Messung gegen `to_woz2` (5488 Datenbytes, 0 fremde Nibbles). */
static const uint8_t TAB[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
static const uint8_t SWAP2[4] = { 0x0, 0x2, 0x1, 0x3 };

/* 256 Nutzbytes -> 343 Diskettenbytes. Der Gegenweg zum Dekoder. */
static void kodiere_6_2(const uint8_t in[256], uint8_t nib[343])
{
    uint8_t aux[86], pri[256];
    memset(aux, 0, sizeof(aux));
    for (size_t i = 0; i < 256; i++) {
        pri[i] = (uint8_t)(in[i] >> 2);
        size_t j = i % 86, k = i / 86;
        uint8_t low = (uint8_t)(in[i] & 3u);
        /* Beim Dekodieren wird SWAP2 angewandt; SWAP2 ist zu sich selbst
         * invers, also hier dasselbe. */
        aux[j] = (uint8_t)(aux[j] | (uint8_t)(SWAP2[low] << (2u * k)));
    }
    uint8_t chk = 0;
    for (size_t i = 0; i < 86; i++)  { nib[i] = TAB[aux[i] ^ chk]; chk = aux[i]; }
    for (size_t i = 0; i < 256; i++) { nib[86 + i] = TAB[pri[i] ^ chk]; chk = pri[i]; }
    nib[342] = TAB[chk];
}

/* Ein Nutzbyte 4-and-4 kodieren: ungerade Bits, dann gerade. */
static void kodiere_4_4(uint8_t v, uint8_t *hi, uint8_t *lo)
{
    *hi = (uint8_t)((v >> 1) | 0xAAu);
    *lo = (uint8_t)(v | 0xAAu);
}

/* Bitschreiber, MSB zuerst — dieselbe Anordnung wie WOZ. */
typedef struct { uint8_t *b; uint32_t pos; } schreiber_t;

static void schreib_nibble(schreiber_t *s, uint8_t v)
{
    for (int k = 7; k >= 0; k--, s->pos++)
        if ((v >> k) & 1u) s->b[s->pos >> 3] |= (uint8_t)(0x80u >> (s->pos & 7u));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Apple-II-GCR: 6-and-2, 4-and-4, Spur-Abtaster (MF-715)\n\n");

    /* ── 1 · 6-and-2 hin und zurueck ─────────────────────────────────── */
    uint8_t klar[256], nib[343], zurueck[256];
    for (size_t i = 0; i < 256; i++) klar[i] = (uint8_t)(i * 7u + 13u);
    kodiere_6_2(klar, nib);

    memset(zurueck, 0xCC, sizeof(zurueck));
    bool ok = uft_apple_gcr_denibblize_6_2(nib, zurueck);
    printf("  6-and-2 hin und zurueck        : %s\n", ok ? "ok" : "FEHLER");
    PRUEFE(ok, "das selbst kodierte Datenfeld wird nicht angenommen");
    PRUEFE(memcmp(klar, zurueck, 256) == 0,
           "256 Nutzbytes kommen nicht unveraendert zurueck");

    /* Alle 256 Bytewerte, damit kein Wert durchrutscht. */
    for (int v = 0; v < 256; v++) {
        uint8_t e[256], n2[343], d[256];
        memset(e, (uint8_t)v, sizeof(e));
        kodiere_6_2(e, n2);
        if (!uft_apple_gcr_denibblize_6_2(n2, d) || memcmp(e, d, 256) != 0) {
            PRUEFE(false, "Bytewert 0x%02X ueberlebt den Rundlauf nicht", v);
            break;
        }
    }
    printf("  alle 256 Bytewerte             : geprueft\n");

    /* ── 2 · Was NICHT angenommen werden darf ────────────────────────── */
    uint8_t kaputt[343];
    memcpy(kaputt, nib, sizeof(kaputt));
    kaputt[100] = 0x00;             /* steht nicht in der Tabelle */
    memset(zurueck, 0xCC, sizeof(zurueck));
    PRUEFE(!uft_apple_gcr_denibblize_6_2(kaputt, zurueck),
           "ein Diskettenbyte ausserhalb der Tabelle wird angenommen");
    bool unberuehrt = true;
    for (size_t i = 0; i < 256; i++) if (zurueck[i] != 0xCC) unberuehrt = false;
    PRUEFE(unberuehrt,
           "bei abgewiesenem Feld wurde das Ziel doch beschrieben — ein "
           "halb dekodierter Sektor ist eine stille Veraenderung");

    memcpy(kaputt, nib, sizeof(kaputt));
    kaputt[342] = TAB[(size_t)((kaputt[342] == TAB[0]) ? 1 : 0)];
    PRUEFE(!uft_apple_gcr_denibblize_6_2(kaputt, zurueck),
           "eine falsche Pruefsumme wird angenommen");
    printf("  fremdes Nibble / falsche Summe : beide abgewiesen\n");

    /* ── 3 · 4-and-4 ─────────────────────────────────────────────────── */
    int vier_ok = 1;
    for (int v = 0; v < 256; v++) {
        uint8_t h, l;
        kodiere_4_4((uint8_t)v, &h, &l);
        if (uft_apple_gcr_decode_4_4(h, l) != (uint8_t)v) { vier_ok = 0; break; }
    }
    PRUEFE(vier_ok, "4-and-4 ueberlebt den Rundlauf nicht fuer alle 256 Werte");
    printf("  4-and-4, alle 256 Werte        : %s\n", vier_ok ? "ok" : "FEHLER");

    /* ── 4 · Der Spur-Abtaster auf einer gebauten Spur ───────────────── */
    const uint32_t bits = 50624u;               /* wie eine echte Spur   */
    uint8_t *spur = calloc(1, bits / 8u + 8u);
    if (!spur) { printf("kein Speicher\n"); return 2; }
    schreiber_t s = { spur, 0 };

    for (int i = 0; i < 40; i++) schreib_nibble(&s, 0xFF);   /* Vorlauf   */
    schreib_nibble(&s, 0xD5); schreib_nibble(&s, 0xAA);
    schreib_nibble(&s, 0x96);
    uint8_t h, l;
    kodiere_4_4(0xFE, &h, &l); schreib_nibble(&s, h); schreib_nibble(&s, l);
    kodiere_4_4(0x11, &h, &l); schreib_nibble(&s, h); schreib_nibble(&s, l);
    kodiere_4_4(0x0D, &h, &l); schreib_nibble(&s, h); schreib_nibble(&s, l);
    kodiere_4_4((uint8_t)(0xFE ^ 0x11 ^ 0x0D), &h, &l);
    schreib_nibble(&s, h); schreib_nibble(&s, l);
    schreib_nibble(&s, 0xDE); schreib_nibble(&s, 0xAA); schreib_nibble(&s, 0xEB);
    for (int i = 0; i < 6; i++) schreib_nibble(&s, 0xFF);
    schreib_nibble(&s, 0xD5); schreib_nibble(&s, 0xAA);
    schreib_nibble(&s, 0xAD);
    for (size_t i = 0; i < 343; i++) schreib_nibble(&s, nib[i]);
    schreib_nibble(&s, 0xDE); schreib_nibble(&s, 0xAA); schreib_nibble(&s, 0xEB);

    uft_a2_sector_t sek[8];
    int n = uft_apple_gcr_scan_track(spur, bits, sek, 8);
    printf("\n  Spur-Abtaster: %d Sektor(en)\n", n);
    PRUEFE(n == 1, "erwartet war genau EIN Sektor, gefunden %d", n);
    if (n == 1) {
        printf("     Datentraeger %d  Spur %d  Sektor %d  "
               "AdrSumme %s  DatSumme %s\n",
               sek[0].volume, sek[0].track, sek[0].sector,
               sek[0].addr_checksum_ok ? "ok" : "FEHLER",
               sek[0].data_checksum_ok ? "ok" : "FEHLER");
        PRUEFE(sek[0].volume == 0xFE, "Datentraeger falsch");
        PRUEFE(sek[0].track == 0x11, "Spur falsch");
        PRUEFE(sek[0].sector == 0x0D, "Sektor falsch");
        PRUEFE(sek[0].addr_checksum_ok, "Adress-Pruefsumme abgelehnt");
        PRUEFE(sek[0].has_data && sek[0].data_checksum_ok,
               "Datenfeld nicht dekodiert");
        PRUEFE(memcmp(sek[0].data, klar, 256) == 0,
               "die Nutzbytes stimmen nicht mit dem Geschriebenen ueberein");
    }

    /* Eine Spur ohne jedes gesetzte Bit darf nicht haengen. */
    memset(spur, 0, bits / 8u);
    int leer = uft_apple_gcr_scan_track(spur, bits, sek, 8);
    printf("  Spur aus lauter Nullbits       : %d Sektoren\n", leer);
    PRUEFE(leer == 0, "aus einer leeren Spur kamen %d Sektoren", leer);

    PRUEFE(uft_apple_gcr_scan_track(NULL, bits, sek, 8) == -1,
           "NULL-Bitstrom wird nicht abgewiesen");
    PRUEFE(uft_apple_gcr_scan_track(spur, bits, sek, 0) == -1,
           "Platz 0 wird nicht abgewiesen");

    free(spur);
    printf("\n  Was die gruene Ampel NICHT heisst: dass echte Disketten "
           "gelesen werden.\n"
           "  Dieser Test baut seine Eingaben selbst. Der Beweis gegen "
           "eine fremde Hand\n"
           "  steht im Kopf: 560 von 560 Sektoren byteidentisch gegen "
           "to_woz2 (MF-715).\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
