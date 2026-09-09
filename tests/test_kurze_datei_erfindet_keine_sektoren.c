/* SPDX-License-Identifier: MIT */
/**
 * @file test_kurze_datei_erfindet_keine_sektoren.c
 * @brief Was nicht in der Datei stand, darf nicht als gelesen gelten (MF-980)
 *
 * ── DER BEFUND ───────────────────────────────────────────────────────
 *
 * Neunzehn Sektor-Leser im Baum haben dieselbe Zeile:
 *
 *     if (fread(buf, 1, SS, p->file) != SS) {
 *         memset(buf, 0xE5, SS);
 *     }
 *     uft_format_add_sector(track, s, buf, SS, cyl, head);
 *
 * Und `uft_format_add_sector_with_id()` setzt fuer JEDEN Sektor, den es
 * anlegt, unbedingt:
 *
 *     sector.status = UFT_SECTOR_OK;
 *     uft_sector_set_crc(&sector, true);      // "good sector"
 *     uft_sector_set_id_crc(&sector, true);
 *
 * Ein abgeschnittenes oder kurzes Abbild liefert damit Sektoren, die
 * voller **erfundener** 0xE5 stehen und als „gelesen, CRC gueltig"
 * markiert sind. Sie sind von echten 0xE5-Daten nicht zu unterscheiden.
 *
 * `uft_atr.c` nennt es im Kommentar sogar „forensic fill on read error".
 * Eine Fuellung, die nicht GEKENNZEICHNET ist, ist das Gegenteil von
 * forensisch — sie ist die dritte Verletzung des Mottos: „Kein Bit
 * verloren. Keine stille Veraenderung. **Keine erfundenen Daten**."
 *
 * Der Baum hat die Kennzeichnung laengst: `UFT_SECTOR_MISSING`
 * (`uft_types.h`, `1 << 2`). Sie wurde an diesen Stellen nur nie gesetzt.
 *
 * ── WARUM TRD ────────────────────────────────────────────────────────
 *
 * `trd_open()` nimmt JEDE Dateigroesse an: alles ausser 655360 und
 * 327680 wird zu „40 Spuren, 1 Seite" (`uft_trd.c`). Eine Datei mit
 * 1024 Byte oeffnet also als 40-Spur-Diskette mit 640 Sektoren, von
 * denen 4 in der Datei stehen. Kein Kunstgriff noetig — der Fall
 * entsteht bei jedem abgebrochenen Abzug.
 *
 * ── ROTBEWEIS ────────────────────────────────────────────────────────
 *
 * Vor MF-980 meldet `trd_read_track(0,0)` sechzehn Sektoren, alle mit
 * `status == UFT_SECTOR_OK` und `crc_ok == true` — auch die zwoelf, die
 * nie in der Datei standen. Der Test faellt in der Zeile
 * „Sektor %d stand nicht in der Datei".
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_trd;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS   256u          /* TRD_SEC_SIZE */
#define SPT   16u          /* TRD_SPT      */
#define ECHT   4u          /* so viele Sektoren stehen wirklich in der Datei */

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
    snprintf(p, n, "%s/uft_kurz_%d.trd", d, rand() % 100000);
}

/* Eine TRD-Datei mit nur ECHT Sektoren. Jeder traegt sein eigenes
 * Muster, damit „echt" und „erfunden" unterscheidbar sind. */
static int baue_kurze_trd(const char *pfad) {
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    for (unsigned s = 0; s < ECHT; s++) {
        uint8_t buf[SS];
        memset(buf, (int)(0x10 + s), SS);
        if (fwrite(buf, 1, SS, f) != SS) { fclose(f); return 0; }
    }
    fclose(f);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Was nicht in der Datei stand, darf nicht als gelesen gelten.
 * ───────────────────────────────────────────────────────────────────── */
TEST(fehlende_sektoren_sind_gekennzeichnet)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_kurze_trd(pfad));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_trd.open(&disk, pfad, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_trd.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT);

    /* Die ersten ECHT standen in der Datei und muessen sauber sein. */
    for (unsigned s = 0; s < ECHT; s++) {
        ASSERT(t.sectors[s].data != NULL);
        if (t.sectors[s].data[0] != (uint8_t)(0x10 + s)) {
            printf("FEHLER: Sektor %u traegt 0x%02X, erwartet 0x%02X\n",
                   s, t.sectors[s].data[0], (unsigned)(0x10 + s));
            _fail++;
            sektoren_frei(&t);
            uft_format_plugin_trd.close(&disk);
            remove(pfad);
            return;
        }
        ASSERT(t.sectors[s].status == UFT_SECTOR_OK);
    }

    /* DIE ZEILE: die uebrigen standen NICHT in der Datei. Sie duerfen
     * nicht als gelesener, CRC-gueltiger Sektor durchgehen. */
    for (unsigned s = ECHT; s < SPT; s++) {
        if (t.sectors[s].status == UFT_SECTOR_OK && t.sectors[s].crc_ok) {
            printf("FEHLER: Sektor %u stand nicht in der Datei, gilt aber "
                   "als gelesen (status=0x%02X, crc_ok=%d, data[0]=0x%02X)\n",
                   s, (unsigned)t.sectors[s].status,
                   (int)t.sectors[s].crc_ok,
                   t.sectors[s].data ? t.sectors[s].data[0] : 0);
            _fail++;
            sektoren_frei(&t);
            uft_format_plugin_trd.close(&disk);
            remove(pfad);
            return;
        }
        /* Gekennzeichnet werden MUSS als fehlend. */
        ASSERT((t.sectors[s].status & UFT_SECTOR_MISSING) != 0);
    }

    sektoren_frei(&t);
    uft_format_plugin_trd.close(&disk);
    remove(pfad);
}

/* Eine vollstaendige Spur darf davon NICHT betroffen sein — sonst
 * wuerde der Fix jeden gesunden Sektor verdaechtigen. */
TEST(vollstaendige_spur_bleibt_sauber)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));

    FILE *f = fopen(pfad, "wb");
    ASSERT(f != NULL);
    for (unsigned s = 0; s < SPT; s++) {
        uint8_t buf[SS];
        memset(buf, (int)(0x20 + s), SS);
        fwrite(buf, 1, SS, f);
    }
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_trd.open(&disk, pfad, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_trd.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT);
    for (unsigned s = 0; s < SPT; s++) {
        ASSERT(t.sectors[s].status == UFT_SECTOR_OK);
        ASSERT(t.sectors[s].crc_ok == true);
        ASSERT(t.sectors[s].data[0] == (uint8_t)(0x20 + s));
    }

    sektoren_frei(&t);
    uft_format_plugin_trd.close(&disk);
    remove(pfad);
}

int main(void)
{
    printf("=== Kurze Datei erfindet keine Sektoren (MF-980) ===\n");
    RUN(vollstaendige_spur_bleibt_sauber);
    RUN(fehlende_sektoren_sind_gekennzeichnet);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
