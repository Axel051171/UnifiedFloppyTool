/* SPDX-License-Identifier: MIT */
/**
 * @file test_nib_ring_und_blindzone.c
 * @brief Der NIB-Leser verliert Sektoren am Spurumbruch (MF-948, P3-234)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * Eine Apple-II-Spur ist ein RING. Sie hat keine Naht: der Kopf laeuft
 * im Kreis, und ein Sektor kann an jeder Stelle beginnen — auch so,
 * dass seine Felder ueber das Ende der Aufzeichnung hinausragen und am
 * Anfang weitergehen.
 *
 * Die oracle-gepruefte Einheit `src/formats/apple/uft_apple_gcr.c` sagt
 * das in ihrem Vertrag ausdruecklich zu:
 *
 *     Die Spur wird als **Ring** gelesen: ein Feld, das ueber das Ende
 *     hinausragt, wird am Anfang fortgesetzt. Eine echte Spur hat keine
 *     Naht, und ein Sektor, der zufaellig auf dem Umbruch liegt, waere
 *     sonst verloren.
 *
 * `src/formats/nib/uft_nib.c` fuehrt eine EIGENE Zweitfassung dieses
 * Abtasters — eigene 64-Byte-Tabelle, eigenes `find_addr`,
 * `find_data`, `decode_sector` — und diese Fassung
 *
 *   1. laeuft NICHT um (`find_addr` sucht nur bis `i + 14 < len`), und
 *   2. hat eine BLINDZONE: `while (pos < NIB_TRACK_SIZE - 400)` bricht
 *      die Suche 400 Bytes vor dem Spurende ab.
 *
 * Beides zusammen heisst: ein Sektor, der in den letzten 400 Bytes
 * beginnt, wird nie gesucht — und einer, der ueber den Umbruch reicht,
 * koennte auch dann nicht gelesen werden. Der Verlust ist **still**:
 * `read_track` meldet `UFT_OK` und liefert einen Sektor weniger.
 *
 * Das ist genau die Lage aus P3-234: die geprüfte Einheit liegt
 * daneben, der Produktionspfad fuehrt eine private Zweitfassung.
 *
 * ── DER AUFBAU ───────────────────────────────────────────────────────
 *
 * Gebaut wird eine vollstaendige 16-Sektor-Spur mit den dokumentierten
 * Feldern (`Beneath Apple DOS`, Kap. 3), aber um eine halbe Teilung
 * GEDREHT, damit der letzte Sektor auf dem Umbruch liegt:
 *
 *     Teilung  6656 / 16 = 416 Bytes je Sektor
 *     Sektor k beginnt bei (208 + k * 416) mod 6656
 *     Sektor 15 beginnt bei 6448, sein Datenfeld endet bei 6816
 *              -> 160 Bytes hinter dem Spurende, also im Ring
 *
 * Die Wahrheit dieses Tests ist NICHT der Inhalt der Sektoren (den baut
 * er selbst und waere insofern zirkulaer), sondern ihre ANZAHL: es
 * wurden 16 geschrieben, also muessen 16 gelesen werden. Ein Leser, der
 * 15 zurueckgibt und `UFT_OK` meldet, hat einen Sektor verloren.
 *
 * Der Inhalt wird zusaetzlich verglichen, weil ein Leser, der die
 * richtige ANZAHL aus dem falschen Versatz holt, sonst durchkaeme.
 *
 * ── WAS DIESER TEST NICHT DECKT ──────────────────────────────────────
 *
 * Die Mutationsmatrix hat es gemessen: `alt_encoding` — das Feld, mit
 * dem die gepruefte Einheit ein Datenfeld in einer Kodierung meldet,
 * die sie NICHT beherrscht — wird hier nicht ausgeloest. Es feuert nur
 * beim DOS-3.2-Bootsektor (5-and-3, Spur 0 Sektor 0, MF-721); die hier
 * gebaute Spur ist durchgehend 16-sektorig (6-and-2). Entfernt man die
 * `alt_encoding`-Pruefung aus `nib_read_track`, bleibt dieser Test
 * gruen.
 *
 * Der Waechter selbst ist nicht ungeprueft: `test_apple_gcr_6and2.c`
 * Abschnitt 4b prueft ihn dort, wo er sitzt. Was fehlt, ist die
 * KOMBINATION „13-Sektor-NIB durch den nib-Leser" — dafuer braeuchte
 * es eine 5-and-3-Spur im NIB-Behaelter, und `nib_open()` schreibt die
 * Geometrie heute fest auf 16 Sektoren. Verzeichnet als offener Punkt.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_nib;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define SPUR      6656
#define SPUREN    35
#define DATEI     (SPUR * SPUREN)
#define SEKTOREN  16
#define TEILUNG   (SPUR / SEKTOREN)      /* 416 */
#define DREHUNG   (TEILUNG / 2)          /* 208 — Sektor 15 landet im Ring */

/* Die 64 Diskettenbytes der 6-and-2-Kodierung. Dass sie stimmen, sagt
 * nicht dieser Test, sondern die Messung gegen `to_woz2` (MF-715). */
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

/* Die Zweibit-Gruppen der Hilfsbytes stehen VERTAUSCHT (b0<->b1).
 * Gleiche Quelle und gleiche Stelle wie in tests/test_apple_gcr_6and2.c. */
static const uint8_t SWAP2[4] = { 0, 2, 1, 3 };

/* 256 Nutzbytes -> 343 Diskettenbytes (342 Daten + Pruefsumme). */
static void kodiere_6_2(const uint8_t in[256], uint8_t nib[343])
{
    uint8_t pri[256], aux[86];
    memset(aux, 0, sizeof(aux));
    for (size_t i = 0; i < 256; i++) pri[i] = (uint8_t)(in[i] >> 2);
    for (size_t i = 0; i < 256; i++) {
        size_t j = i % 86, k = i / 86;
        uint8_t low = (uint8_t)(in[i] & 3u);
        aux[j] = (uint8_t)(aux[j] | (uint8_t)(SWAP2[low] << (2u * k)));
    }
    uint8_t chk = 0;
    for (size_t i = 0; i < 86; i++)  { nib[i] = TAB[aux[i] ^ chk]; chk = aux[i]; }
    for (size_t i = 0; i < 256; i++) { nib[86 + i] = TAB[pri[i] ^ chk]; chk = pri[i]; }
    nib[342] = TAB[chk];
}

static void kodiere_4_4(uint8_t v, uint8_t *hi, uint8_t *lo)
{
    *hi = (uint8_t)((v >> 1) | 0xAAu);
    *lo = (uint8_t)(v | 0xAAu);
}

/* Ein Byte an die Ringstelle schreiben. */
static void ring(uint8_t *spur, size_t pos, uint8_t v)
{
    spur[pos % SPUR] = v;
}

/* Nutzinhalt von Sektor s auf Spur t — deterministisch und je Sektor
 * unterscheidbar, damit ein Versatzfehler auffiele. */
static void inhalt(uint8_t t, uint8_t s, uint8_t out[256])
{
    for (int i = 0; i < 256; i++)
        out[i] = (uint8_t)(i * 7u + s * 29u + t * 3u + 11u);
}

/* Baut eine 16-Sektor-Spur, um `DREHUNG` verschoben.
 *
 * `fremd` laesst EINEN Sektor eine falsche Spurnummer tragen. Das ist
 * kein Kunstgriff: eine Spur, die zur Nachbarspur uebersprochen hat,
 * traegt genau dieses Bild — und ein Leser, der die Spurnummer aus dem
 * Adressfeld nicht prueft, mischt fremde Daten unter. Ohne diesen Fall
 * blieb die Mutation "Spurnummer nicht mehr geprueft" gruen (gemessen). */
static void baue_spur_n(uint8_t t, uint8_t spur[SPUR], int fremd)
{
    memset(spur, 0xFF, SPUR);            /* Luecken sind Sync-Bytes */

    for (uint8_t s = 0; s < SEKTOREN; s++) {
        size_t p = (size_t)DREHUNG + (size_t)s * TEILUNG;
        uint8_t hi, lo;
        uint8_t tf = (fremd >= 0 && s == (uint8_t)fremd)
                   ? (uint8_t)(t + 1u) : t;

        /* Adressfeld */
        ring(spur, p++, 0xD5); ring(spur, p++, 0xAA); ring(spur, p++, 0x96);
        kodiere_4_4(254, &hi, &lo); ring(spur, p++, hi); ring(spur, p++, lo);
        kodiere_4_4(tf,  &hi, &lo); ring(spur, p++, hi); ring(spur, p++, lo);
        kodiere_4_4(s,   &hi, &lo); ring(spur, p++, hi); ring(spur, p++, lo);
        kodiere_4_4((uint8_t)(254u ^ tf ^ s), &hi, &lo);
        ring(spur, p++, hi); ring(spur, p++, lo);
        ring(spur, p++, 0xDE); ring(spur, p++, 0xAA); ring(spur, p++, 0xEB);

        p += 5;                          /* Luecke bleibt 0xFF */

        /* Datenfeld */
        uint8_t klar[256], nib[343];
        inhalt(tf, s, klar);
        kodiere_6_2(klar, nib);
        ring(spur, p++, 0xD5); ring(spur, p++, 0xAA); ring(spur, p++, 0xAD);
        for (int i = 0; i < 343; i++) ring(spur, p++, nib[i]);
        ring(spur, p++, 0xDE); ring(spur, p++, 0xAA); ring(spur, p++, 0xEB);
    }
}

static void baue_spur(uint8_t t, uint8_t spur[SPUR])
{
    baue_spur_n(t, spur, -1);
}

static void temp_pfad(char *p, size_t n)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_nib_ring_%d.nib", d, rand() % 100000);
}

static int schreibe_nib(const char *pfad)
{
    uint8_t *bild = malloc(DATEI);
    if (!bild) return 0;
    for (uint8_t t = 0; t < SPUREN; t++)
        baue_spur(t, bild + (size_t)t * SPUR);

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(bild); return 0; }
    size_t w = fwrite(bild, 1, DATEI, f);
    fclose(f);
    free(bild);
    return w == (size_t)DATEI;
}

static void sektoren_frei(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Alle 16 geschriebenen Sektoren muessen zurueckkommen.
 * ───────────────────────────────────────────────────────────────────── */
TEST(alle_sechzehn_sektoren_kommen_zurueck)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(schreibe_nib(pfad));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_nib.open(&disk, pfad, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_nib.read_track(&disk, 3, 0, &t) == UFT_OK);

    /* DIE ZEILE. Vor MF-948 kommen 15: Sektor 15 beginnt bei 6448 und
     * liegt damit sowohl hinter der Blindzonen-Schranke (6656-400) als
     * auch ueber dem Spurumbruch. Der Verlust wird nicht gemeldet. */
    if (t.sector_count != SEKTOREN)
        printf("\n       gelesen: %u von %d Sektoren\n",
               (unsigned)t.sector_count, SEKTOREN);
    ASSERT(t.sector_count == SEKTOREN);

    sektoren_frei(&t);
    uft_format_plugin_nib.close(&disk);
    remove(pfad);
}

/* Und der Inhalt jedes Sektors muss der geschriebene sein — sonst kaeme
 * ein Leser durch, der die richtige ANZAHL aus dem falschen Versatz
 * holt. */
TEST(jeder_sektor_traegt_seinen_eigenen_inhalt)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(schreibe_nib(pfad));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_nib.open(&disk, pfad, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_nib.read_track(&disk, 3, 0, &t) == UFT_OK);

    int gesehen[SEKTOREN];
    memset(gesehen, 0, sizeof(gesehen));

    for (size_t i = 0; i < t.sector_count; i++) {
        uint8_t nr = (uint8_t)t.sectors[i].id.sector;
        ASSERT(nr < SEKTOREN);
        ASSERT(t.sectors[i].data != NULL);
        ASSERT(t.sectors[i].data_size == 256);
        uint8_t soll[256];
        inhalt(3, nr, soll);
        if (memcmp(t.sectors[i].data, soll, 256) != 0)
            printf("\n       Sektor %u weicht ab\n", nr);
        ASSERT(memcmp(t.sectors[i].data, soll, 256) == 0);
        gesehen[nr] = 1;
    }
    for (int s = 0; s < SEKTOREN; s++) {
        if (!gesehen[s]) printf("\n       Sektor %d fehlt\n", s);
        ASSERT(gesehen[s]);
    }

    sektoren_frei(&t);
    uft_format_plugin_nib.close(&disk);
    remove(pfad);
}

/* Ein Sektor mit FREMDER Spurnummer darf nicht mitgelesen werden.
 *
 * Uebersprechen von der Nachbarspur erzeugt genau dieses Bild. Wer die
 * Spurnummer aus dem Adressfeld nicht prueft, mischt fremde Daten unter
 * — und meldet `UFT_OK`. Die Mutationsmatrix hat gezeigt, dass ohne
 * diesen Fall das Entfernen der Pruefung gruen bleibt. */
TEST(fremd_beschrifteter_sektor_bleibt_draussen)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));

    uint8_t *bild = malloc(DATEI);
    ASSERT(bild != NULL);
    for (uint8_t s = 0; s < SPUREN; s++)
        baue_spur_n(s, bild + (size_t)s * SPUR, (s == 3) ? 7 : -1);
    FILE *f = fopen(pfad, "wb");
    ASSERT(f != NULL);
    size_t w = fwrite(bild, 1, DATEI, f);
    fclose(f);
    free(bild);
    ASSERT(w == (size_t)DATEI);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_nib.open(&disk, pfad, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_nib.read_track(&disk, 3, 0, &tr) == UFT_OK);

    /* 15 statt 16: der fremd beschriftete Sektor 7 gehoert nicht hierher. */
    if (tr.sector_count != SEKTOREN - 1)
        printf("\n       gelesen: %u, erwartet %d\n",
               (unsigned)tr.sector_count, SEKTOREN - 1);
    ASSERT(tr.sector_count == SEKTOREN - 1);

    /* Und nirgends steht der Inhalt, den Spur 4 haette. */
    uint8_t fremd[256];
    inhalt(4, 7, fremd);
    for (size_t i = 0; i < tr.sector_count; i++) {
        ASSERT(tr.sectors[i].id.sector != 7);
        ASSERT(memcmp(tr.sectors[i].data, fremd, 256) != 0);
    }

    sektoren_frei(&tr);
    uft_format_plugin_nib.close(&disk);
    remove(pfad);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== NIB: Ring und Blindzone (MF-948) ===\n");
    RUN(alle_sechzehn_sektoren_kommen_zurueck);
    RUN(jeder_sektor_traegt_seinen_eigenen_inhalt);
    RUN(fremd_beschrifteter_sektor_bleibt_draussen);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
