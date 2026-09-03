/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_adf_teilabzug.c
 * @brief Ein spurgenauer ADF-Teilabzug ist kein Muell (MF-840).
 *
 * ── Der Anlass ───────────────────────────────────────────────────────────
 *
 * `TransDisk` (Amiga, ueber `trackdisk.device`) kann Spurbereiche
 * abziehen. Das ist das dokumentierte Beispiel seines Autors:
 *
 *     transdisk >RAM:df1.adf.1 -d trackdisk 1 -s 0 -e 39
 *       "transfers the first half of the floppy in DF1: into a file"
 *
 * Ergebnis: 40 x 11 x 512 = **225 280 Byte**, und die Endung `.1` sagt,
 * dass eine `.2` folgt. Auf Amigas mit knapper RAM-Disk oder bei
 * serieller Uebertragung war das der Normalfall.
 *
 * `adf_plugin_probe()` und `adf_open()` verlangten **exakte** Gleichheit
 * mit 901 120 oder 1 802 240 Byte. Ein solcher Teilabzug war damit kein
 * ADF — nicht „ADF, unvollstaendig", sondern gar nichts. Die Erkennung
 * fiel durch und die Datei ging an den naechsten Kandidaten, der sie mit
 * Sicherheit falsch deutet.
 *
 * Fuer ein Werkzeug mit dem Grundsatz „Kein Bit verloren" ist das die
 * falsche Reaktion: die Datei enthaelt 40 **vollstaendige, gueltige**
 * Spuren. Sie zu verwerfen, weil 40 weitere fehlen, verliert alles statt
 * nichts.
 *
 * ── Und die Falle daneben ────────────────────────────────────────────────
 *
 * Die Groessenpruefung bloss zu lockern waere schlimmer gewesen. Im
 * Lesepfad stand
 *
 *     if (fread(...) != ADF_SECTOR_SIZE) memset(buf, 0xE5, ...);
 *
 * — ein kurzer Lesevorgang jenseits des Dateiendes liefert `0xE5`-Fuellung
 * und `UFT_OK`. Genau die Klasse, die MF-837 aus dem DMS-Plugin entfernt
 * hat: der Verlust wird als Datum ausgegeben. Die fehlenden 40 Spuren
 * waeren als „leere, formatierte Diskette" erschienen.
 *
 * Deshalb hat der Patch zwei Haelften: Teilabzuege ANNEHMEN, und fehlende
 * Spuren als FEHLEND melden statt sie zu erfinden.
 *
 * ── Die drei Kurzfaelle sind unterscheidbar ──────────────────────────────
 *
 *   Vielfaches der Spurlaenge (5632 / 11264)  spurgenauer Teilabzug
 *   Vielfaches von 512, nicht spurgenau       an Blockgrenze abgebrochen
 *   kein Vielfaches von 512                   mitten im Block abgebrochen
 *
 * Zwei Modulo-Rechnungen, drei verschiedene Aussagen fuer den Bericht.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_adf;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define SEK        512u
#define SPT_DD      11u
#define SPUR_DD    (SPT_DD * SEK)      /*  5632 */
#define SPUREN     160u                /* 80 Zyl. x 2 Seiten */
#define VOLL       (SPUREN * SPUR_DD)  /* 901120 */
#define TEIL_SPUREN 80u                /* -s 0 -e 39 => 40 Zyl. = 80 Spuren */
#define TEIL       (TEIL_SPUREN * SPUR_DD)

#define MARKE_ERSTE 0x5Au
#define MARKE_LETZTE 0xA7u

static void temp_pfad(char *p, size_t n, const char *stamm) {
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_adf_%s_%d.adf", d, stamm, rand() % 100000);
}

/* Schreibt `laenge` Byte: Bootblock "DOS\0", Marke im ersten und im
 * letzten VORHANDENEN Sektor. */
static int schreibe(const char *pfad, size_t laenge)
{
    uint8_t *b = (uint8_t *)calloc(1, laenge);
    if (!b) return 0;
    if (laenge >= 4) memcpy(b, "DOS\0", 4);
    if (laenge >= 8) b[4] = MARKE_ERSTE;
    if (laenge >= SEK) b[laenge - 1] = MARKE_LETZTE;
    FILE *f = fopen(pfad, "wb");
    if (!f) { free(b); return 0; }
    size_t w = fwrite(b, 1, laenge, f);
    fclose(f); free(b);
    return w == laenge;
}

static void spuren_frei(uft_track_t *t) {
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors);
    t->sectors = NULL; t->sector_count = 0;
}

TEST(vollstaendiges_adf_bleibt_wie_bisher)
{
    /* Gegenprobe. Ohne sie koennte der Patch die Erkennung generell
     * verwaessert haben. */
    char p[300]; temp_pfad(p, sizeof p, "voll");
    ASSERT(schreibe(p, VOLL));

    uint8_t kopf[8] = {0};
    FILE *f = fopen(p, "rb"); ASSERT(f); (void)!fread(kopf, 1, 8, f); fclose(f);

    int konf = -1;
    ASSERT(uft_format_plugin_adf.probe(kopf, sizeof kopf, VOLL, &konf));
    ASSERT(konf == 95);                    /* Bootblock getroffen */

    uft_disk_t d; memset(&d, 0, sizeof d); d.read_only = true;
    ASSERT(uft_format_plugin_adf.open(&d, p, true) == UFT_OK);
    ASSERT(d.geometry.cylinders == 80);
    ASSERT(d.geometry.sectors == SPT_DD);
    if (uft_format_plugin_adf.close) uft_format_plugin_adf.close(&d);
    remove(p);
}

TEST(spurgenauer_teilabzug_wird_angenommen)
{
    /* ROTBEWEIS 1. 225 280 Byte — das dokumentierte TransDisk-Beispiel. */
    char p[300]; temp_pfad(p, sizeof p, "teil");
    ASSERT(schreibe(p, TEIL));

    uint8_t kopf[8] = {0};
    FILE *f = fopen(p, "rb"); ASSERT(f); (void)!fread(kopf, 1, 8, f); fclose(f);

    int konf = -1;
    ASSERT(uft_format_plugin_adf.probe(kopf, sizeof kopf, TEIL, &konf));
    /* Bootblock vorhanden, aber unvollstaendige Datei: nach der Skala
     * aus MF-729 ein Merkmalstreffer mit Vorbehalt, nicht die volle 95. */
    ASSERT(konf >= 50 && konf < 95);

    uft_disk_t d; memset(&d, 0, sizeof d); d.read_only = true;
    ASSERT(uft_format_plugin_adf.open(&d, p, true) == UFT_OK);
    /* Die GEOMETRIE bleibt die der ganzen Diskette — die Datei ist ein
     * Ausschnitt daraus, nicht eine kleinere Diskette. */
    ASSERT(d.geometry.cylinders == 80);
    ASSERT(d.geometry.heads == 2);
    if (uft_format_plugin_adf.close) uft_format_plugin_adf.close(&d);
    remove(p);
}

TEST(vorhandene_spuren_des_teilabzugs_kommen_an)
{
    char p[300]; temp_pfad(p, sizeof p, "vorh");
    ASSERT(schreibe(p, TEIL));

    uft_disk_t d; memset(&d, 0, sizeof d); d.read_only = true;
    ASSERT(uft_format_plugin_adf.open(&d, p, true) == UFT_OK);

    uft_track_t t; memset(&t, 0, sizeof t);
    ASSERT(uft_format_plugin_adf.read_track(&d, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT_DD);
    ASSERT(t.sectors[0].data[0] == 'D');
    ASSERT(t.sectors[0].data[4] == MARKE_ERSTE);
    spuren_frei(&t);

    /* Letzte VORHANDENE Spur: Zylinder 39, Kopf 1 (Spurindex 79). */
    memset(&t, 0, sizeof t);
    ASSERT(uft_format_plugin_adf.read_track(&d, 39, 1, &t) == UFT_OK);
    ASSERT(t.sector_count == SPT_DD);
    ASSERT(t.sectors[SPT_DD - 1].data[SEK - 1] == MARKE_LETZTE);
    spuren_frei(&t);

    if (uft_format_plugin_adf.close) uft_format_plugin_adf.close(&d);
    remove(p);
}

TEST(fehlende_spuren_werden_nicht_erfunden)
{
    /* ROTBEWEIS 2 — der wichtigere. Eine Spur jenseits des Dateiendes
     * darf NICHT mit UFT_OK und 0xE5-Fuellung zurueckkommen. Das ist
     * dieselbe Klasse, die MF-837 aus dem DMS-Plugin entfernt hat:
     * 0xE5 ist die AmigaDOS-Formatfuellung, ein Fehlschlag waere von
     * einer leeren formatierten Spur nicht zu unterscheiden. */
    char p[300]; temp_pfad(p, sizeof p, "fehlt");
    ASSERT(schreibe(p, TEIL));

    uft_disk_t d; memset(&d, 0, sizeof d); d.read_only = true;
    ASSERT(uft_format_plugin_adf.open(&d, p, true) == UFT_OK);

    uft_track_t t; memset(&t, 0, sizeof t);
    uft_error_t rc = uft_format_plugin_adf.read_track(&d, 45, 0, &t);

    bool erfundene_leere = (rc == UFT_OK && t.sector_count == SPT_DD
                            && t.sectors[0].data
                            && t.sectors[0].data[0] == 0xE5);
    ASSERT(!erfundene_leere);
    ASSERT(rc != UFT_OK);          /* fehlend ist ein Fehler, kein Inhalt */

    spuren_frei(&t);
    if (uft_format_plugin_adf.close) uft_format_plugin_adf.close(&d);
    remove(p);
}

TEST(nicht_spurgenaue_kuerzung_ist_etwas_anderes)
{
    /* An einer Blockgrenze abgebrochen, aber nicht an einer Spurgrenze.
     * Darf angenommen werden, aber mit deutlich schwaecherem Anspruch —
     * hier ist die Datei beschaedigt, nicht bloss unvollstaendig. */
    char p[300]; temp_pfad(p, sizeof p, "block");
    size_t len = TEIL + 3u * SEK;          /* 3 Sektoren ueber der Spur */
    ASSERT(schreibe(p, len));

    uint8_t kopf[8] = {0};
    FILE *f = fopen(p, "rb"); ASSERT(f); (void)!fread(kopf, 1, 8, f); fclose(f);

    int konf = -1;
    bool ja = uft_format_plugin_adf.probe(kopf, sizeof kopf, len, &konf);
    if (ja) ASSERT(konf < 50);     /* kein Merkmalsanspruch */
    remove(p);
}

TEST(unsinnige_groessen_bleiben_abgewiesen)
{
    uint8_t kopf[8] = { 'D','O','S',0, 0,0,0,0 };
    int konf = -1;
    /* Kein Vielfaches von 512 und zu klein fuer eine Spur. */
    ASSERT(!uft_format_plugin_adf.probe(kopf, sizeof kopf, 1000, &konf));
    ASSERT(!uft_format_plugin_adf.probe(kopf, sizeof kopf, 0, &konf));
    /* Groesser als eine HD-Diskette. */
    ASSERT(!uft_format_plugin_adf.probe(kopf, sizeof kopf, 4000000, &konf));
}

int main(void)
{
    printf("=== ADF-Teilabzug (MF-840) ===\n");
    RUN(vollstaendiges_adf_bleibt_wie_bisher);
    RUN(spurgenauer_teilabzug_wird_angenommen);
    RUN(vorhandene_spuren_des_teilabzugs_kommen_an);
    RUN(fehlende_spuren_werden_nicht_erfunden);
    RUN(nicht_spurgenaue_kuerzung_ist_etwas_anderes);
    RUN(unsinnige_groessen_bleiben_abgewiesen);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
