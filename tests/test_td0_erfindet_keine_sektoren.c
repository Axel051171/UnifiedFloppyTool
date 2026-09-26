/* SPDX-License-Identifier: MIT */
/**
 * @file test_td0_erfindet_keine_sektoren.c
 * @brief TD0: was die Datei nicht traegt, ist kein gelesener Sektor (MF-981)
 *
 * ── WARUM DIESER TEST NACH MF-980 NOETIG WAR ─────────────────────────
 *
 * MF-980 hat 23 Leser umgestellt, die einen kurzen `fread` mit 0xE5
 * fuellten und den Sektor als „gelesen, CRC gueltig" anlegten. **Tor 62**
 * haelt die Klasse — es sucht ein `memset` im Fehlerzweig eines `fread`.
 *
 * TD0 entzieht sich dem auf zwei Wegen, und beide wurden erst beim
 * Nachmessen der TORGRENZE gefunden:
 *
 *   (1) Die Fuellung kommt aus einem FORMAT-FLAG, nicht aus einem kurzen
 *       Lesevorgang. `sec_flags & 0x30` heisst im Quelltext woertlich
 *       „No data for this sector"; der Leser legt dann ueber
 *       `uft_format_add_empty_sector(..., 0xE5, ...)` einen Sektor an.
 *       Kein `fread`, kein `memset` — Tor 62 sieht nichts.
 *
 *   (2) Der Dekodierpuffer ist `calloc(1, sec_size)`. Erzeugt der
 *       RLE-Dekoder weniger Bytes als `sec_size`, bleibt der Rest NULL —
 *       und `uft_format_add_sector()` bekommt trotzdem die volle
 *       `sec_size` uebergeben. Bei einem UNBEKANNTEN Verfahrensbyte
 *       erzeugt er gar nichts: der Sektor besteht dann aus lauter
 *       erfundenen Nullen.
 *
 * Gemessen wurde die Torgrenze, nicht geraten: von drei denkbaren
 * Umgehungen (Delegation an einen Helfer, Format-Flag, Fuellung ohne
 * `memset`) ist die erste im ganzen Baum **null**-mal vorhanden, und die
 * anderen beiden liegen **beide in dieser einen Datei**.
 *
 * ── DAS ABBILD ───────────────────────────────────────────────────────
 *
 * Ein unkomprimiertes TD0 („TD"), kein Kommentarblock (Bit 7 von
 * `bTrackDensity` ist 0), eine Spur mit drei Sektoren a 128 Byte:
 *
 *   Sektor 1   Flags 0x00, Verfahren 0 (roh), 128 Byte 0xAA   -> echt
 *   Sektor 2   Flags 0x10 („keine Daten")                     -> leer
 *   Sektor 3   Flags 0x00, Verfahren 9 (unbekannt)            -> nichts
 *
 * ── ROTBEWEIS ────────────────────────────────────────────────────────
 *
 * Vor MF-981 kommen alle drei mit `status == UFT_SECTOR_OK` und
 * `crc_ok == true` zurueck. Der Test faellt an Sektor 2.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
/* MF-1332: fuer die IMD-Zusage unten — Stromleser und Sektortypen. */
#include "uft/formats/uft_td0.h"
#include "uft/formats/uft_imd.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_td0;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS  128u

/* Die Bits, die einen FEHLER bedeuten — im Unterschied zu
 * `UFT_SECTOR_CRC_CHECKED`, das nur sagt, dass nachgerechnet wurde. */
#define FEHLERBITS (UFT_SECTOR_CRC_ERROR | UFT_SECTOR_ID_CRC_ERROR \
                  | UFT_SECTOR_MISSING)

static void sektoren_frei(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static void temp_pfad(char *p, size_t n) {
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_td0_%d.td0", d, rand() % 100000);
}

static int baue_td0(const char *pfad)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;

    /* Kopf, 12 Byte. Bit 7 von Byte 7 bleibt 0 -> kein Kommentarblock
     * (MF-971: der Block haengt am FLAG, nicht an der Version). */
    uint8_t kopf[12] = {0};
    kopf[0] = 'T'; kopf[1] = 'D';   /* unkomprimiert */
    kopf[4] = 0x15;                 /* Version */
    kopf[7] = 0x00;                 /* bTrackDensity, Bit 7 = 0 */
    kopf[9] = 1;                    /* Seiten */
    if (fwrite(kopf, 1, sizeof(kopf), f) != sizeof(kopf)) { fclose(f); return 0; }

    /* Spurkopf: 3 Sektoren, Zylinder 0, Kopf 0, CRC */
    uint8_t sk[4] = {3, 0, 0, 0};
    fwrite(sk, 1, sizeof(sk), f);

    /* Sektor 1: echte Daten, Verfahren 0 (roh).
     *
     * MF-1296: das sechste Byte ist die Pruefsumme ueber die Daten und
     * stand auf 0. Fuer 128 Byte 0xAA ist sie 0x92FE, unteres Byte
     * **0xFE** — unabhaengig mit einer Python-Umsetzung gerechnet, nicht
     * mit der des Pruefdatei-Erzeugers. */
    uint8_t s1[6] = {0, 0, 1, 0, 0x00, 0xFE};
    fwrite(s1, 1, sizeof(s1), f);
    uint8_t len[2] = {(uint8_t)((SS + 1) & 0xFF), (uint8_t)((SS + 1) >> 8)};
    fwrite(len, 1, 2, f);
    uint8_t verfahren = 0;
    fwrite(&verfahren, 1, 1, f);
    uint8_t daten[SS];
    memset(daten, 0xAA, SS);
    fwrite(daten, 1, SS, f);

    /* Sektor 2: Flag 0x10 — „keine Daten fuer diesen Sektor".
     * Es folgt KEIN Laengenfeld. */
    uint8_t s2[6] = {0, 0, 2, 0, 0x10, 0};
    fwrite(s2, 1, sizeof(s2), f);

    /* Sektor 3: Verfahren 9 — unbekannt. Der Dekoder erzeugt nichts. */
    uint8_t s3[6] = {0, 0, 3, 0, 0x00, 0};
    fwrite(s3, 1, sizeof(s3), f);
    uint8_t len3[2] = {2, 0};
    fwrite(len3, 1, 2, f);
    uint8_t roh3[2] = {9, 0x00};
    fwrite(roh3, 1, 2, f);

    /* Endemarke */
    uint8_t ende = 0xFF;
    fwrite(&ende, 1, 1, f);

    fclose(f);
    return 1;
}

TEST(was_die_datei_nicht_traegt_gilt_nicht_als_gelesen)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_td0(pfad));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_td0.open(&disk, pfad, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_td0.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 3);

    /* Sektor 1 stand wirklich in der Datei. */
    ASSERT(t.sectors[0].data != NULL);
    ASSERT(t.sectors[0].data[0] == 0xAA);
    /* MF-1296: NICHT `status == UFT_SECTOR_OK`. `status` ist ein
     * FLAGGENFELD, und seit MF-1296 traegt es mit
     * `UFT_SECTOR_CRC_CHECKED` eine Flagge, die KEIN Fehler ist — sie
     * sagt nur, dass nachgerechnet wurde. Gleichheit mit 0 faellt
     * deshalb bei jeder additiven Flagge, ohne dass etwas kaputt waere.
     * Gefragt ist `kein FEHLERbit`. */
    ASSERT((t.sectors[0].status & FEHLERBITS) == 0);
    ASSERT((t.sectors[0].status & UFT_SECTOR_CRC_CHECKED) != 0);
    ASSERT(t.sectors[0].crc_ok == true);

    /* DIE ZEILE (1): Flag 0x10 heisst „keine Daten fuer diesen Sektor". */
    if ((t.sectors[1].status & FEHLERBITS) == 0 && t.sectors[1].crc_ok) {
        printf("FEHLER: Sektor 2 traegt Flag 0x10 (keine Daten), gilt aber "
               "als gelesen (status=0x%02X, crc_ok=%d, data[0]=0x%02X)\n",
               (unsigned)t.sectors[1].status, (int)t.sectors[1].crc_ok,
               t.sectors[1].data ? t.sectors[1].data[0] : 0);
        _fail++;
        sektoren_frei(&t);
        uft_format_plugin_td0.close(&disk);
        remove(pfad);
        return;
    }
    ASSERT((t.sectors[1].status & UFT_SECTOR_MISSING) != 0);

    /* DIE ZEILE (2): unbekanntes Verfahren -> der Dekoder erzeugte nichts,
     * der Puffer ist der genullte calloc. */
    if ((t.sectors[2].status & FEHLERBITS) == 0 && t.sectors[2].crc_ok) {
        printf("FEHLER: Sektor 3 hat ein unbekanntes Verfahrensbyte, der "
               "Inhalt ist genullter calloc — gilt aber als gelesen "
               "(status=0x%02X, crc_ok=%d)\n",
               (unsigned)t.sectors[2].status, (int)t.sectors[2].crc_ok);
        _fail++;
        sektoren_frei(&t);
        uft_format_plugin_td0.close(&disk);
        remove(pfad);
        return;
    }
    ASSERT((t.sectors[2].status & UFT_SECTOR_MISSING) != 0);

    sektoren_frei(&t);
    uft_format_plugin_td0.close(&disk);
    remove(pfad);
}

/* MF-1332: dieselbe Aussage EINE SCHICHT WEITER — die Ehrlichkeit des
 * fehlenden Sektors muss die Wandlung nach IMD ueberleben.
 *
 * MF-1287 hat `imd_stype_aus_sektor()` so gefasst, dass
 * `UFT_SECTOR_MISSING` zu `UFT_IMD_SEC_UNAVAIL` (0x00) wird — die
 * vorsichtige Richtung, „lieber sagen nicht gelesen als Fuellbytes als
 * Daten ausgeben". `docs/OPEN_ITEMS.md` P3-523 verlangt dafuer
 * ausdruecklich eine Zusage, und gemessen gab es sie nicht: `UNAVAIL`
 * kam in KEINEM der sieben `tests/test_td0*.c` vor. Der einzige Nenner
 * im Baum war `test_convert_imd_img_belegt.c`, also die GEGENrichtung.
 *
 * Die Zusage haengt hier und nicht in einer eigenen Datei, weil das
 * Abbild schon hier steht: eine zweite `baue_td0()` waere die Bauform
 * aus MF-1177.
 *
 * Erwartet, aus dem Abbild abgeleitet und nicht geraten:
 *   Sektor 1  Flags 0x00, echte Daten          -> NICHT unavail
 *   Sektor 2  Flags 0x10 („keine Daten")       -> UNAVAIL
 *   Sektor 3  unbekanntes Verfahrensbyte       -> UNAVAIL (MF-981)
 */
TEST(fehlender_sektor_wird_in_imd_nicht_zu_daten)
{
    char pfad[512];
    temp_pfad(pfad, sizeof pfad);
    ASSERT(baue_td0(pfad));

    /* Die Datei als Bytes einlesen — `uft_td0_strom_aus_bytes()` ist der
     * Weg, den `uft_td0_to_imd()` erwartet. */
    FILE *f = fopen(pfad, "rb");
    ASSERT(f != NULL);
    ASSERT(fseek(f, 0, SEEK_END) == 0);
    long groesse = ftell(f);
    ASSERT(groesse > 0);
    ASSERT(fseek(f, 0, SEEK_SET) == 0);
    uint8_t *roh = (uint8_t *)malloc((size_t)groesse);
    ASSERT(roh != NULL);
    size_t gelesen = fread(roh, 1, (size_t)groesse, f);
    fclose(f);
    if (gelesen != (size_t)groesse) { free(roh); remove(pfad); ASSERT(0); }

    uft_td0_strom_t strom;
    memset(&strom, 0, sizeof strom);
    if (uft_td0_strom_aus_bytes(roh, (size_t)groesse, &strom) != UFT_OK) {
        free(roh); remove(pfad); ASSERT(0);
    }

    struct uft_imd_image_t imd;
    memset(&imd, 0, sizeof imd);
    int rc = uft_td0_to_imd(&strom, &imd);

    if (rc != UFT_OK || imd.num_tracks < 1 || imd.tracks == NULL) {
        uft_imd_free(&imd);
        uft_td0_strom_frei(&strom); free(roh); remove(pfad);
        printf("FEHLER: uft_td0_to_imd rc=%d, Spuren=%u\n",
               rc, (unsigned)imd.num_tracks);
        ASSERT(0);
    }

    /* DIE ZEILE: ein Sektor, den die Datei nicht traegt, darf in der IMD
     * kein Datensatz sein. */
    ASSERT(imd.tracks[0].header.nsectors == 3);
    ASSERT(imd.tracks[0].stype[1] == UFT_IMD_SEC_UNAVAIL);
    ASSERT(imd.tracks[0].stype[2] == UFT_IMD_SEC_UNAVAIL);

    /* Gegenprobe im selben Lauf: der ECHTE Sektor darf nicht
     * mitverschwinden. Ohne sie waere ein Wandler gruen, der alles auf
     * UNAVAIL setzt. */
    ASSERT(imd.tracks[0].stype[0] != UFT_IMD_SEC_UNAVAIL);

    /* Und die Zaehlung muss dasselbe sagen wie die Typen. */
    ASSERT(imd.unavail_sectors == 2);

    /* Die IMD, die uft_td0_to_imd() angelegt hat, blieb hier
     * liegen. uft_imd_free() ist die passende Freigabe: der
     * Wandler ruft sie auf seinem eigenen Fehlerweg, und sie gibt
     * die Struktur selbst nicht frei (sie liegt auf dem Stapel). */
    uft_imd_free(&imd);
    uft_td0_strom_frei(&strom);
    free(roh);
    remove(pfad);
}

int main(void)
{
    printf("=== TD0 erfindet keine Sektoren (MF-981) ===\n");
    RUN(was_die_datei_nicht_traegt_gilt_nicht_als_gelesen);
    RUN(fehlender_sektor_wird_in_imd_nicht_zu_daten);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
