/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_fat_kopien_kette.c
 * @brief Die Clusterkette in BEIDEN FAT-Kopien — pro Datei (MF-874)
 *
 * -- Der Befund --------------------------------------------------------
 *
 * Der Baum hat drei FAT-Leser. Gemessen ueber `git ls-files`:
 *
 *   src/fs/uft_fat12.c                    nur Tests    VERGLEICHT (MF-829)
 *   src/formats/uft_fat12_legacy.c        explorertab.cpp:677,772   nein
 *   src/fileops/uft_file_ops_extended.c   explorertab.cpp:1172,1319 nein
 *
 * Der einzige, der die Kopien vergleicht, ist der einzige ohne
 * Produktionsaufrufer. Die beiden, die die Oberflaeche erreicht, lasen
 * unbedingt FAT 1.
 *
 * Das ist nicht gleichgueltig. `explorertab.cpp:1165` schickt auch
 * `.st`-Abbilder durch den dritten Leser, und fuer die gilt laut
 * Claus Brod, MSXPATCH (1994, ZOO-Archiv vom 23.11.1994; vom
 * Eigentuemer entpackt, zitiert in P3-56):
 *
 *   „MSXDOS disks have two FATs, but only the first of them contains
 *    valid data. On a PC, that's no problem since MS-DOS uses the
 *    second FAT only as a backup of the first FAT. TOS, however,
 *    always tries to look up its file allocation data in the second
 *    FAT."
 *
 * Weichen die Kopien ab, lieferte `fat12_extract_file()` also
 * moeglicherweise den falschen Inhalt — ohne ein Wort.
 *
 * -- Was MF-874 tut, und was NICHT --------------------------------------
 *
 * Gemeldet wird die BEOBACHTUNG, nicht ihre Deutung. WELCHE Kopie
 * massgeblich ist, haengt vom schreibenden System ab und ist im Baum
 * nicht belegbar — genau EINE Quelle, P3-56. Die Funktion folgt
 * weiterhin FAT 1 und sagt lediglich, dass es eine zweite Lesart gibt.
 * Der Rueckgabewert 1 heisst „gelesen, zweideutig", nicht „Fehler": ein
 * Befund darf den Zugriff nicht verstellen (MF-829).
 *
 * -- Warum PRO DATEI und nicht per memcmp ------------------------------
 *
 * Ein Vergleich der ganzen FAT waere einfacher und WAERE FALSCH. Zwei
 * Kopien duerfen sich in Bereichen unterscheiden, die diese Datei gar
 * nicht beruehrt — etwa in den Eintraegen geloeschter Dateien —, ohne
 * dass ihr Inhalt dadurch zweideutig waere. Ein memcmp meldete dort
 * einen Befund, den es nicht gibt.
 *
 * Der Fall `abweichung_ausserhalb_der_kette_meldet_nichts` ist die
 * Gegenprobe dazu. Ohne ihn waere ein memcmp ebenso „gruen".
 */
#include "uft/uft_file_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

/* 360 KB, 40 x 2 x 9 — die Geometrie, die `fat12_list_files()` ohne
 * Rueckfall aufloest. */
#define BPS        512u
#define SPC          2u
#define RESERVED     1u
#define FATS         2u
#define ROOT_ENT   112u
#define TOTAL      720u
#define SPF          2u

#define FAT_START   (RESERVED * BPS)                 /*   512 */
#define FAT_BYTES   (SPF * BPS)                      /*  1024 */
#define ROOT_BYTES  (ROOT_ENT * 32u)                 /*  3584 */
#define CLUSTER     (SPC * BPS)                      /*  1024 */
#define ABBILD      (TOTAL * BPS)                    /* 368640 */

/* Wurzelverzeichnis und Daten liegen HINTER allen FAT-Kopien — bei
 * einem Einzel-FAT-Medium also 1024 Byte frueher. Der Erbauer muss
 * dieselbe Rechnung machen wie der Leser, sonst prueft er ein Abbild,
 * das es so nicht gibt. */
#define ROOT_START(n)  (FAT_START + (n) * FAT_BYTES)
#define DATA_START(n)  (ROOT_START(n) + ROOT_BYTES)

/* Die Datei belegt die Cluster 2 -> 3 -> 4 (Ende). */
#define C0 2u
#define C1 3u
#define C2 4u
#define DATEIGROESSE (3u * CLUSTER)

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/** Einen 12-Bit-Eintrag in eine FAT schreiben. */
static void fat_set(uint8_t *fat, unsigned cluster, uint16_t wert)
{
    unsigned off = cluster + cluster / 2;
    if (cluster & 1u) {
        fat[off]     = (uint8_t)((fat[off] & 0x0F) | ((wert & 0x0F) << 4));
        fat[off + 1] = (uint8_t)(wert >> 4);
    } else {
        fat[off]     = (uint8_t)(wert & 0xFF);
        fat[off + 1] = (uint8_t)((fat[off + 1] & 0xF0) | ((wert >> 8) & 0x0F));
    }
}

/**
 * Ein FAT12-Abbild bauen.
 *
 * @param fat_anzahl      1 oder 2
 * @param kette_abweichen FAT 2 leitet Cluster C1 woandershin
 * @param fremd_abweichen FAT 2 weicht bei einem Cluster ab, den die
 *                        Datei NICHT benutzt
 */
static uint8_t *baue_abbild(unsigned fat_anzahl, bool kette_abweichen,
                            bool fremd_abweichen)
{
    uint8_t *img = calloc(1, ABBILD);
    if (!img) return NULL;

    img[0] = 0xEB; img[1] = 0x3C; img[2] = 0x90;
    memcpy(img + 3, "UFTTEST ", 8);
    put16(img + 0x0B, (uint16_t)BPS);
    img[0x0D] = (uint8_t)SPC;
    put16(img + 0x0E, (uint16_t)RESERVED);
    img[0x10] = (uint8_t)fat_anzahl;
    put16(img + 0x11, (uint16_t)ROOT_ENT);
    put16(img + 0x13, (uint16_t)TOTAL);
    img[0x15] = 0xFD;
    put16(img + 0x16, (uint16_t)SPF);
    put16(img + 0x18, 9);
    put16(img + 0x1A, 2);
    img[510] = 0x55; img[511] = 0xAA;

    /* FAT 1: 2 -> 3 -> 4 -> Ende */
    uint8_t *f1 = img + FAT_START;
    f1[0] = 0xFD; f1[1] = 0xFF; f1[2] = 0xFF;
    fat_set(f1, C0, (uint16_t)C1);
    fat_set(f1, C1, (uint16_t)C2);
    fat_set(f1, C2, 0xFFF);
    /* ein Cluster ausserhalb der Kette, damit es etwas zu variieren gibt */
    fat_set(f1, 9, 0x000);

    /* Das Verzeichnis: eine Datei ueber drei Cluster. */
    uint8_t *e = img + ROOT_START(fat_anzahl);
    memcpy(e, "DATEI   BIN", 11);
    e[11] = 0x20;                       /* Archiv */
    put16(e + 26, (uint16_t)C0);
    put32(e + 28, DATEIGROESSE);

    /* Die Nutzdaten: jeder Cluster traegt seine Nummer. */
    for (unsigned c = C0; c <= C2; c++)
        memset(img + DATA_START(fat_anzahl) + (c - 2) * CLUSTER,
               (int)c, CLUSTER);

    if (fat_anzahl >= 2) {
        uint8_t *f2 = img + FAT_START + FAT_BYTES;
        memcpy(f2, f1, FAT_BYTES);      /* zunaechst identisch */
        if (kette_abweichen) fat_set(f2, C1, 7);    /* mitten in der Kette */
        if (fremd_abweichen) fat_set(f2, 9, 0x123); /* ausserhalb          */
    }
    return img;
}

/** Prueft, dass der gelieferte Inhalt FAT 1 folgt: 2,3,4 je Cluster. */
static bool inhalt_folgt_fat1(const uint8_t *d, size_t n)
{
    if (n != DATEIGROESSE) return false;
    for (unsigned c = 0; c < 3; c++)
        for (size_t i = 0; i < CLUSTER; i++)
            if (d[c * CLUSTER + i] != (uint8_t)(C0 + c)) return false;
    return true;
}

TEST(zwei_gleiche_kopien_melden_nichts)
{
    uint8_t *img = baue_abbild(2, false, false);
    ASSERT(img != NULL);
    uint8_t *d = NULL; size_t n = 0;
    int rc = fat12_extract_file(img, ABBILD, "DATEI.BIN", &d, &n);
    if (rc != 0) { printf("\n      rc=%d, erwartet 0\n      ", rc); _fail++; }
    else if (!inhalt_folgt_fat1(d, n)) {
        printf("\n      Inhalt stimmt nicht (%zu Byte)\n      ", n);
        _fail++;
    }
    free(d); free(img);
}

TEST(eine_abweichende_kette_wird_gemeldet)
{
    /* DER ROTBEWEIS. Vor MF-874 gab dieser Fall 0 zurueck — der
     * Benutzer bekam Bytes, ohne zu erfahren, dass es eine zweite
     * Lesart gibt. */
    uint8_t *img = baue_abbild(2, true, false);
    ASSERT(img != NULL);
    uint8_t *d = NULL; size_t n = 0;
    int rc = fat12_extract_file(img, ABBILD, "DATEI.BIN", &d, &n);
    if (rc != 1) {
        printf("\n      rc=%d, erwartet 1 — die Abweichung wurde nicht "
               "gemeldet\n      ", rc);
        _fail++;
    } else if (!inhalt_folgt_fat1(d, n)) {
        /* Der Befund darf den Zugriff nicht verstellen UND nicht still
         * die Kopie wechseln. */
        printf("\n      Inhalt folgt nicht mehr FAT 1\n      ");
        _fail++;
    }
    free(d); free(img);
}

TEST(abweichung_ausserhalb_der_kette_meldet_nichts)
{
    /* DIE GEGENPROBE. Ein memcmp ueber die ganze FAT meldete hier einen
     * Befund, den es nicht gibt: Cluster 9 gehoert zu keiner Datei. */
    uint8_t *img = baue_abbild(2, false, true);
    ASSERT(img != NULL);
    uint8_t *d = NULL; size_t n = 0;
    int rc = fat12_extract_file(img, ABBILD, "DATEI.BIN", &d, &n);
    if (rc != 0) {
        printf("\n      rc=%d, erwartet 0 — die Abweichung liegt "
               "ausserhalb der Kette dieser Datei\n      ", rc);
        _fail++;
    }
    free(d); free(img);
}

TEST(eine_einzelne_fat_meldet_nichts)
{
    /* Einzel-FAT-Medien sind auf dem Atari eine BENANNTE, unterstuetzte
     * Eigenschaft, kein Defekt (Harun Scheutzow, FLOP_FIX.TXT 1992,
     * Fehler 2: Bit 1 der BPB-Flags an Offset $10). Hinter FAT 1 liegt
     * dann das Wurzelverzeichnis — wer dort „die zweite FAT" laese,
     * verglaeche Verzeichniseintraege mit Clustereintraegen. */
    uint8_t *img = baue_abbild(1, false, false);
    ASSERT(img != NULL);
    uint8_t *d = NULL; size_t n = 0;
    int rc = fat12_extract_file(img, ABBILD, "DATEI.BIN", &d, &n);
    if (rc != 0) { printf("\n      rc=%d, erwartet 0\n      ", rc); _fail++; }
    free(d); free(img);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== FAT: die Clusterkette in beiden Kopien (MF-874) ===\n");
    RUN(zwei_gleiche_kopien_melden_nichts);
    RUN(eine_abweichende_kette_wird_gemeldet);
    RUN(abweichung_ausserhalb_der_kette_meldet_nichts);
    RUN(eine_einzelne_fat_meldet_nichts);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
