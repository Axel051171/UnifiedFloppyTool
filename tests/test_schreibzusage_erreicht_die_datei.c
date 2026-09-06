/* SPDX-License-Identifier: MIT */
/**
 * @file test_schreibzusage_erreicht_die_datei.c
 * @brief `write_track` meldet keinen Erfolg, den die Datei nicht traegt (MF-930)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * MF-883 hat neun Plugins gefunden, die beim Schreiben ihre SPEICHERKOPIE
 * aenderten, `UFT_OK` meldeten und sie beim Schliessen freigaben. Kein
 * Byte erreichte die Platte. Tor 57 (`scripts/audit_schreibzusage.py`)
 * haelt diese Klasse seither fest — aber nur ihre SCHARFE Haelfte: ein
 * Plugin mit `CAP_WRITE`, in dessen Datei **keine** Schreiboperation
 * steht.
 *
 * P3-154 hat die unscharfe Haelfte offen gelassen und acht Verdaechtige
 * AUFGEZAEHLT. Gemessen sind es **elf**, und drei davon standen auf
 * keiner Liste (`logical`, `myz80`, `nanowasp`, `opus`):
 *
 *     apridisk  cfi  hardsector  logical  mgt
 *     myz80  nanowasp  opus  posix  qrst  rcpmfs
 *
 * Alle elf haben einen echten Dateischreiber `uft_<fmt>_write()` IM
 * HAUS — deshalb laesst Tor 57 sie durch. Nur fuehrt kein Weg dorthin:
 * die Plugin-Tafel hat kein `.flush`, `close` gibt den Puffer frei ohne
 * zu schreiben, und `write_track` fasst nur den Speicher an. Bei
 * `apridisk` steht es woertlich im Quelltext:
 *
 *     In-memory write: modifies the cached disk image.
 *     Call flush/close to persist changes via uft_apridisk_write().
 *
 * `plugin->flush` wird im ganzen Baum von **niemandem** gerufen
 * (MF-883), und `uft_disk_close()` ruft nur `close`. Der Satz beschreibt
 * einen Rueckweg, den es nicht gibt.
 *
 * ── WAS DIESER TEST ZUSICHERT ────────────────────────────────────────
 *
 * Format-unabhaengig und AM ERGEBNIS gemessen, nicht am Zeiger — genau
 * die Form, die P3-154 verlangt:
 *
 *   Meldet `write_track` `UFT_OK`, dann muss die Aenderung nach
 *   `close()` und ERNEUTEM OEFFNEN in der Datei stehen.
 *   Kann das Plugin das nicht, muss es `UFT_ERROR_NOT_SUPPORTED`
 *   antworten. Was es NICHT darf, ist Erfolg melden und nichts tun.
 *
 * Gefahren wird er an `opus`, weil dort ein geprueftes Abbild
 * herstellbar ist: `baue_opd()` aus `tests/test_opd_geometrie.c`
 * (MF-905, Format auf T2 gehoben gegen das Oracle `src/samdisk/opd.cpp`).
 * Fuer die uebrigen zehn fehlt heute ein Pruefabbild — sie werden von
 * Tor 57 in seiner geschaerften Fassung strukturell gehalten, nicht von
 * diesem Test. Das steht hier, weil eine Zusicherung ueber elf Formate,
 * die einen misst, eine Ueberzeichnung waere.
 *
 * ── ROTBEWEIS ────────────────────────────────────────────────────────
 *
 * Vor MF-930 meldet `opus_write_track()` `UFT_OK`, `opus_close()` gibt
 * den Puffer frei, und das erneute Oeffnen liefert den ALTEN Wert.
 * Dieser Test faellt dann in der Zeile „der geschriebene Wert steht in
 * der Datei".
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_opus;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* read_track fuellt eine Spur, die dem Aufrufer gehoert (hier auf dem
 * Stapel): die Sektoren freigeben, NICHT die Struktur selbst —
 * `uft_track_free()` ruft `free(track)`. */
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
    snprintf(p, n, "%s/uft_opd_wr_%d.opd", d, rand() % 100000);
}

/* Ein gueltiges OPD, gebaut wie in tests/test_opd_geometrie.c (MF-905).
 * Bootsektor: jr, Fuellbyte, Zylinder, Sektoren, Flags. */
#define OP_JR 0x18
static int baue_opd(const char *pfad, uint8_t cyls, uint8_t sektoren_je_spur)
{
    const size_t sektorgroesse = 256;           /* Groessencode 1 */
    const size_t n = (size_t)cyls * sektoren_je_spur * sektorgroesse;
    uint8_t *p = calloc(1, n);
    if (!p) return 0;

    p[0] = OP_JR;
    p[1] = 0x28;
    p[2] = cyls;
    p[3] = sektoren_je_spur;
    p[4] = (uint8_t)(1u << 6);                  /* 256 B, einseitig */

    /* Jeden Sektor unterscheidbar fuellen. */
    for (size_t s = 1; s < n / sektorgroesse; s++)
        memset(p + s * sektorgroesse, (int)(s & 0xFF), sektorgroesse);

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(p); return 0; }
    size_t w = fwrite(p, 1, n, f);
    fclose(f);
    free(p);
    return w == n;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Erfolg melden heisst: die Datei traegt es.
 * ───────────────────────────────────────────────────────────────────── */
TEST(erfolg_bedeutet_die_datei_traegt_es)
{
    /* Den Zeiger PRUEFEN, bevor er gerufen wird. Ohne diese Zeile
     * stuerzt der Test bei einem NULL-write_track ab, statt zu melden —
     * gemessen in der Mutationsmatrix (M3), dieselbe Reihenfolge-Falle
     * wie in MF-922. */
    ASSERT(uft_format_plugin_opus.write_track != NULL);

    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_opd(pfad, 40, 18));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_opus.read_track(&disk, 1, 0, &t) == UFT_OK);
    ASSERT(t.sector_count > 0);
    ASSERT(t.sectors[0].data != NULL);

    const uint8_t vorher = t.sectors[0].data[0];
    const uint8_t neu    = (uint8_t)(vorher ^ 0xFF);
    t.sectors[0].data[0] = neu;

    uft_error_t r = uft_format_plugin_opus.write_track(&disk, 1, 0, &t);
    sektoren_frei(&t);
    uft_format_plugin_opus.close(&disk);

    if (r == UFT_ERROR_NOT_SUPPORTED) {
        /* Ehrliche Absage — dann muss die Datei unberuehrt sein. */
        uft_disk_t d2;
        memset(&d2, 0, sizeof(d2));
        ASSERT(uft_format_plugin_opus.open(&d2, pfad, true) == UFT_OK);
        uft_track_t t2;
        memset(&t2, 0, sizeof(t2));
        ASSERT(uft_format_plugin_opus.read_track(&d2, 1, 0, &t2) == UFT_OK);
        ASSERT(t2.sectors[0].data[0] == vorher);
        sektoren_frei(&t2);
        uft_format_plugin_opus.close(&d2);
        remove(pfad);
        return;
    }

    /* Erfolg gemeldet — dann MUSS die Aenderung in der Datei stehen. */
    ASSERT(r == UFT_OK);

    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    ASSERT(uft_format_plugin_opus.open(&d2, pfad, true) == UFT_OK);
    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_opus.read_track(&d2, 1, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[0].data != NULL);

    /* DIE ZEILE. Vor MF-930 steht hier der alte Wert: write_track hat die
     * Speicherkopie geaendert, close hat sie freigegeben, und
     * `uft_opus_write()` — der echte Dateischreiber in derselben Datei —
     * wurde nie gerufen. */
    ASSERT(t2.sectors[0].data[0] == neu);

    sektoren_frei(&t2);
    uft_format_plugin_opus.close(&d2);
    remove(pfad);
}

/* Die Merkmalstafel darf nichts zusagen, was `write_track` nicht haelt. */
TEST(die_zusage_und_die_tat_stimmen_ueberein)
{
    /* write_track bleibt GESETZT — ein Nullzeiger gaebe dem Aufrufer
     * keine Begruendung (MF-883). */
    ASSERT(uft_format_plugin_opus.write_track != NULL);

    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_opd(pfad, 40, 18));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, false) == UFT_OK);
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_opus.read_track(&disk, 1, 0, &t) == UFT_OK);
    uft_error_t r = uft_format_plugin_opus.write_track(&disk, 1, 0, &t);
    sektoren_frei(&t);
    uft_format_plugin_opus.close(&disk);
    remove(pfad);

    const int sagt_write = (uft_format_plugin_opus.capabilities
                            & UFT_FORMAT_CAP_WRITE) != 0;
    if (sagt_write) {
        /* Wer CAP_WRITE fuehrt, darf nicht absagen. */
        ASSERT(r != UFT_ERROR_NOT_SUPPORTED);
    } else {
        ASSERT(r == UFT_ERROR_NOT_SUPPORTED);
    }
}

int main(void)
{
    printf("=== Schreibzusage erreicht die Datei (MF-930) ===\n");
    /* Reihenfolge ist Absicht: der Zeigertest zuerst, sonst ruft der
     * Rundlauf einen NULL-write_track und der Prozess stirbt, bevor
     * irgendeine Zusicherung etwas sagen kann. */
    RUN(die_zusage_und_die_tat_stimmen_ueberein);
    RUN(erfolg_bedeutet_die_datei_traegt_es);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
