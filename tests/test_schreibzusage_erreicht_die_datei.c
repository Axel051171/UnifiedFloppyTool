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
static int baue_opd_n(const char *pfad, uint8_t cyls, uint8_t sektoren_je_spur,
                      int zwei_seiten)
{
    const size_t sektorgroesse = 256;           /* Groessencode 1 */
    const uint8_t heads = zwei_seiten ? 2 : 1;
    const size_t n = (size_t)cyls * heads * sektoren_je_spur * sektorgroesse;
    uint8_t *p = calloc(1, n);
    if (!p) return 0;

    p[0] = OP_JR;
    p[1] = 0x28;
    p[2] = cyls;
    p[3] = sektoren_je_spur;
    /* Flags: Groessencode in Bit 6/7, zwei Seiten in Bit 4 — nach
     * src/samdisk/opd.cpp (ReadOPD/WriteOPD), MF-905. */
    p[4] = (uint8_t)((1u << 6) | (zwei_seiten ? 0x10u : 0x00u));

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

static int baue_opd(const char *pfad, uint8_t cyls, uint8_t sektoren_je_spur)
{
    return baue_opd_n(pfad, cyls, sektoren_je_spur, 0);
}

/* Spiegelt, was `uft_disk_open()` tut, bevor es das Plugin ruft:
 * Pfad in `path_buf`, `path` darauf zeigen lassen
 * (`src/core/uft_core_stubs.c`, gemessen MF-931). Ohne diesen Schritt
 * hat das Plugin keinen Ort, an den es schreiben koennte — und muss
 * das dann auch sagen, statt Erfolg zu melden. */
static void setze_pfad(uft_disk_t *d, const char *pfad)
{
    snprintf(d->path_buf, sizeof(d->path_buf), "%s", pfad);
    d->path = d->path_buf;
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

    if (r != UFT_OK) {
        /* Ehrliche Absage — dann muss die Datei unberuehrt sein.
         *
         * MF-931: hier stand `r == UFT_ERROR_NOT_SUPPORTED`. Das war zu
         * eng: es gibt einen DRITTEN ehrlichen Ausgang neben „kann ich
         * nicht" und „erledigt" — „ich kann nicht, und zwar aus diesem
         * Grund" (hier `UFT_ERR_INVALID_STATE`, wenn kein Pfad gesetzt
         * ist). Die Zusicherung dieses Tests ist NICHT, welcher
         * Fehlercode kommt, sondern: was nicht `UFT_OK` meldet, darf die
         * Datei nicht angefasst haben. */
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

/* ─────────────────────────────────────────────────────────────────────
 *  MF-931: opus schreibt bis in die Datei.
 *
 *  Das Gegenstueck zu MF-930. Dort wurde die Zusage ZURUECKGENOMMEN,
 *  weil elf Formate keinen Weg zu ihrem eigenen Schreiber hatten. Fuer
 *  `opus` ist der Weg jetzt gebaut — und zwar so, dass KEINE neue
 *  Layout-Rechnung entsteht: `write_track` aendert die Speicherkopie
 *  und ruft danach `uft_opus_write()`, denselben Schreiber, den MF-905
 *  gegen `src/samdisk/opd.cpp` belegt hat.
 *
 *  Warum nicht ueber `close()`: das ist `void`. Ein dort scheiternder
 *  Schreibvorgang waere eine STILLE Veraenderung — genau das, was
 *  DESIGN_PRINCIPLES verbietet. `write_track` hat einen Fehlerkanal.
 * ───────────────────────────────────────────────────────────────────── */
TEST(opus_schreibt_bis_in_die_datei)
{
    ASSERT(uft_format_plugin_opus.write_track != NULL);

    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_opd(pfad, 40, 18));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    setze_pfad(&disk, pfad);
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_opus.read_track(&disk, 5, 0, &t) == UFT_OK);
    ASSERT(t.sector_count > 0 && t.sectors[0].data != NULL);
    const uint8_t vorher = t.sectors[0].data[0];
    const uint8_t neu    = (uint8_t)(vorher ^ 0xFF);
    t.sectors[0].data[0] = neu;

    /* DIE ZEILE: keine Absage mehr. */
    ASSERT(uft_format_plugin_opus.write_track(&disk, 5, 0, &t) == UFT_OK);
    sektoren_frei(&t);
    uft_format_plugin_opus.close(&disk);

    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    ASSERT(uft_format_plugin_opus.open(&d2, pfad, true) == UFT_OK);
    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_opus.read_track(&d2, 5, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[0].data != NULL);
    ASSERT(t2.sectors[0].data[0] == neu);
    sektoren_frei(&t2);
    uft_format_plugin_opus.close(&d2);
    remove(pfad);
}

/* Ohne Pfad kann niemand schreiben — dann muss das Plugin es SAGEN.
 * Ein `write_track`, das ohne Ziel `UFT_OK` meldet, waere MF-930 von
 * vorn. */
TEST(ohne_pfad_wird_abgesagt_statt_gelogen)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_opd(pfad, 40, 18));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    /* KEIN setze_pfad() — genau der Unterschied. */
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_opus.read_track(&disk, 5, 0, &t) == UFT_OK);
    uft_error_t r = uft_format_plugin_opus.write_track(&disk, 5, 0, &t);
    sektoren_frei(&t);
    uft_format_plugin_opus.close(&disk);
    remove(pfad);

    ASSERT(r != UFT_OK);
}

/* Seite 1 einer doppelseitigen OPD.
 *
 * MF-905 hat `read_track` von `track_data[cyl]` auf
 * `[cyl * heads + head]` gezogen und `head != 0` entfernt — auf der
 * SCHREIBSEITE blieb beides stehen. Dieselbe Halbierung wie MF-519 vs.
 * MF-529: die Leseseite repariert, die Schreibseite uebersehen. */
TEST(opus_schreibt_auch_auf_seite_1)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_opd_n(pfad, 40, 18, /*zwei_seiten*/1));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    setze_pfad(&disk, pfad);
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, false) == UFT_OK);
    ASSERT(disk.geometry.heads == 2);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_opus.read_track(&disk, 5, 1, &t) == UFT_OK);
    ASSERT(t.sector_count > 0 && t.sectors[0].data != NULL);
    const uint8_t neu = (uint8_t)(t.sectors[0].data[0] ^ 0xFF);
    t.sectors[0].data[0] = neu;

    ASSERT(uft_format_plugin_opus.write_track(&disk, 5, 1, &t) == UFT_OK);
    sektoren_frei(&t);
    uft_format_plugin_opus.close(&disk);

    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    ASSERT(uft_format_plugin_opus.open(&d2, pfad, true) == UFT_OK);

    /* Seite 1 traegt den neuen Wert ... */
    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_opus.read_track(&d2, 5, 1, &t2) == UFT_OK);
    ASSERT(t2.sectors[0].data[0] == neu);
    sektoren_frei(&t2);

    /* ... und Seite 0 derselben Spur wurde NICHT mitgeschrieben.
     * Ohne diese Zusicherung wuerde ein falscher Index (track_data[cyl]
     * statt [cyl*heads+head]) unbemerkt durchgehen, weil Lesen und
     * Schreiben denselben Fehler machen wuerden. */
    uft_track_t t0;
    memset(&t0, 0, sizeof(t0));
    ASSERT(uft_format_plugin_opus.read_track(&d2, 5, 0, &t0) == UFT_OK);
    ASSERT(t0.sectors[0].data[0] != neu);
    sektoren_frei(&t0);

    uft_format_plugin_opus.close(&d2);
    remove(pfad);
}

/* Der Schreiber scheitert — und das MUSS beim Aufrufer ankommen.
 *
 * Diese Luecke fand die Mutationsmatrix (M4): `(void)werr;` statt
 * `if (werr != UFT_OK) return werr;` blieb GRUEN, weil kein Fall den
 * Schreiber ueberhaupt scheitern liess — die Pfadpruefung griff vorher.
 * Ein verschluckter Schreibfehler ist exakt die MF-930-Klasse.
 *
 * Der Aufbau: regulaer oeffnen (das Abbild liegt danach im Speicher),
 * dann das Ziel auf einen Pfad umbiegen, den `fopen(..., "wb")` nicht
 * anlegen kann — ein Verzeichnis, das es nicht gibt. */
TEST(scheitert_der_schreiber_erfaehrt_es_der_aufrufer)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_opd(pfad, 40, 18));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    setze_pfad(&disk, pfad);
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_opus.read_track(&disk, 5, 0, &t) == UFT_OK);
    ASSERT(t.sector_count > 0 && t.sectors[0].data != NULL);
    t.sectors[0].data[0] ^= 0xFF;

    char kaputt[500];
    snprintf(kaputt, sizeof(kaputt),
             "%s_gibt_es_nicht/darin/x.opd", pfad);
    setze_pfad(&disk, kaputt);

    /* Nicht welcher Code — nur: NICHT UFT_OK. */
    uft_error_t r = uft_format_plugin_opus.write_track(&disk, 5, 0, &t);
    sektoren_frei(&t);
    uft_format_plugin_opus.close(&disk);

    ASSERT(r != UFT_OK);

    /* Und die urspruengliche Datei ist unberuehrt geblieben. */
    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    ASSERT(uft_format_plugin_opus.open(&d2, pfad, true) == UFT_OK);
    uft_format_plugin_opus.close(&d2);
    remove(pfad);
}

int main(void)
{
    printf("=== Schreibzusage erreicht die Datei (MF-930) ===\n");
    /* Reihenfolge ist Absicht: der Zeigertest zuerst, sonst ruft der
     * Rundlauf einen NULL-write_track und der Prozess stirbt, bevor
     * irgendeine Zusicherung etwas sagen kann. */
    RUN(die_zusage_und_die_tat_stimmen_ueberein);
    RUN(erfolg_bedeutet_die_datei_traegt_es);
    RUN(opus_schreibt_bis_in_die_datei);
    RUN(ohne_pfad_wird_abgesagt_statt_gelogen);
    RUN(opus_schreibt_auch_auf_seite_1);
    RUN(scheitert_der_schreiber_erfaehrt_es_der_aufrufer);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
