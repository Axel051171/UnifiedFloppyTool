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

    /* Sektor 1: echte Daten, Verfahren 0 (roh) */
    uint8_t s1[6] = {0, 0, 1, 0, 0x00, 0};
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
    ASSERT(t.sectors[0].status == UFT_SECTOR_OK);
    ASSERT(t.sectors[0].crc_ok == true);

    /* DIE ZEILE (1): Flag 0x10 heisst „keine Daten fuer diesen Sektor". */
    if (t.sectors[1].status == UFT_SECTOR_OK && t.sectors[1].crc_ok) {
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
    if (t.sectors[2].status == UFT_SECTOR_OK && t.sectors[2].crc_ok) {
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

int main(void)
{
    printf("=== TD0 erfindet keine Sektoren (MF-981) ===\n");
    RUN(was_die_datei_nicht_traegt_gilt_nicht_als_gelesen);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
