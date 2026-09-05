/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_hfe_write_allowed.c
 * @brief HFE: `write_allowed` hatte die Polaritaet verdreht (MF-898)
 *
 * ── Der Befund, an einer echten fremden Datei gemessen ────────────────────
 *
 * `hfe_open()` setzte:
 *
 *     disk->read_only = read_only || (header.write_allowed == 0xFF);
 *
 * und `read_metadata("write_protected")` antwortete nach derselben Regel.
 * Gemessen an `tests/corpus_free/gw_amigados.hfe` — einer voellig
 * gewoehnlichen, von **greaseweazle 1.23** geschriebenen Datei:
 *
 *     write_allowed                      = 0xFF
 *     disk.read_only nach dem Oeffnen    = JA
 *     read_metadata("write_protected")   = "yes"
 *
 * An dieser Diskette ist nichts schreibgeschuetzt. Das Werkzeug sagte es
 * trotzdem — und sperrte den Schreibpfad gleich mit.
 *
 * ── Sechs Belege gegen einen ──────────────────────────────────────────────
 *
 * Fuer "0xFF heisst SCHREIBEN ERLAUBT" sprechen, alle im Baum nachlesbar:
 *
 * (MF-903: hier standen Zeilennummern; vier waren binnen zweier
 * Commits falsch. Verweise nennen jetzt das SYMBOL.)
 *
 *   1. der Feldname selbst — `write_allowed`, 0xFF = wahr = erlaubt
 *   2. include/uft/flux/uft_hfe.h      `write_allowed`: "0xFF = writable"
 *   3. include/uft/uft_hfe_format.h    `write_allowed`: "0xFF = write allowed"
 *   4. include/uft/uft_hfe_format.h    `uft_hfe_header_init()` setzt 0xFF
 *   5. src/samdisk/hfe.cpp             der Schreiber setzt 0xff fuer ein
 *                                      ganz gewoehnliches Abbild
 *   6. src/formats/hfe/uft_hfe_parser_v2.c
 *                                      `hfe_info_to_text()`:
 *                                      `write_allowed ? "Yes" : "No"`
 *
 * Dazu die Messung: greaseweazle schreibt 0xFF. **Waere 0xFF
 * Schreibschutz, dann waere jede jemals von SAMdisk oder greaseweazle
 * geschriebene HFE schreibgeschuetzt.**
 *
 * Dagegen stand allein `src/formats/hfe/uft_hfe.c` — an drei Stellen,
 * und ausgerechnet die, die handelt.
 *
 * ── Und UFT widersprach sich dabei selbst ─────────────────────────────────
 *
 * `hfe_create()` schrieb `header.write_allowed = 0x00;` mit dem Kommentar
 * "Schreiben erlaubt" — waehrend `uft_hfe_header_init()` in
 * `include/uft/uft_hfe_format.h` fuer denselben Zweck 0xFF setzt. Zwei
 * Schreiber im selben Baum, entgegengesetzte Werte, dieselbe Absicht.
 * Dieselbe Klasse wie MF-897 eine Zeile weiter unten im selben Kopf.
 *
 * ── Was bewusst NICHT behauptet wird ──────────────────────────────────────
 *
 * Dass 0x00 in freier Wildbahn "schreibgeschuetzt" bedeutet, ist **nicht
 * belegt** — kein bekannter Schreiber setzt je etwas anderes als 0xFF,
 * und `src/samdisk/hfe.cpp` liest das Feld ueberhaupt nie. UFT liest es
 * nach dem Feldnamen: 0 = nicht erlaubt, alles andere = erlaubt. Das ist
 * die konservative Richtung, weil sie den Benutzer nicht ungefragt
 * aussperrt; der Vermerk darueber steht im Quelltext.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include "fixtures/hfe_v1_fixture.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_hfe;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR muss vom Bau gesetzt werden (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

static const char *korpus_hfe(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/gw_amigados.hfe", UFT_CORPUS_DIR);
    return p;
}

/*
 * MF-903: derselbe HFE-v1-Kopfbauer wie in
 * `test_hfe_track0_encoding.c` lag hier ein zweites Mal - rund 40
 * Zeilen doppelt, vom Code-Review als Duplicated Code benannt. Er liegt
 * jetzt in `tests/fixtures/hfe_v1_fixture.h`, samt der Beschreibung des
 * Aufbaus. Hier ist nur `write_allowed` von Belang; die Spur-0-Felder
 * stehen auf 0xFF, also "kein Ersatz" (MF-897).
 */
static int schreibe_hfe(const char *pfad, uint8_t write_allowed)
{
    const uft_test_hfe_v1_t o = {
        /* tracks */ 1, /* sides */ 1,
        /* track_encoding */ 0x00,          /* ISO MFM */
        write_allowed,
        0xFF, 0xFF, 0xFF, 0xFF,
    };
    return uft_test_write_hfe_v1(pfad, &o);
}

/* Oeffnet und liefert Schreibschutz-Fahne und Metadaten-Antwort. */
static int oeffne(const char *pfad, bool aufrufer_will_ro,
                  bool *ro_out, char *meta_out, size_t meta_len)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = aufrufer_will_ro;
    if (uft_format_plugin_hfe.open(&disk, pfad, aufrufer_will_ro) != UFT_OK)
        return 0;
    *ro_out = disk.read_only;
    meta_out[0] = '\0';
    if (uft_format_plugin_hfe.read_metadata)
        uft_format_plugin_hfe.read_metadata(&disk, "write_protected",
                                            meta_out, meta_len);
    uft_format_plugin_hfe.close(&disk);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  1. Eine gewoehnliche greaseweazle-HFE ist NICHT schreibgeschuetzt.
 *
 *  Der schaerfste Pruefstein, weil die Datei von einer fremden Hand
 *  stammt und niemand sie je geschuetzt hat.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(fremde_hfe_ist_nicht_geschuetzt)
{
    bool ro = false; char meta[32];
    ASSERT(oeffne(korpus_hfe(), false, &ro, meta, sizeof(meta)));
    ASSERT(ro == false);
    ASSERT(strcmp(meta, "no") == 0);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  2. WAECHTER: der Wunsch des Aufrufers bleibt unangetastet.
 *
 *  Wer nur lesen will, bekommt read_only — unabhaengig vom Kopfwert.
 *  Diese Pruefung steht vorher wie nachher gruen; sie haelt fest, dass
 *  die Korrektur die Sperre des AUFRUFERS nicht mit wegnimmt.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(waechter_aufruferwunsch_bleibt)
{
    bool ro = false; char meta[32];
    ASSERT(oeffne(korpus_hfe(), true, &ro, meta, sizeof(meta)));
    ASSERT(ro == true);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  3. write_allowed == 0 heisst "nicht erlaubt".
 *
 *  Die Lesart des Feldnamens. Vorher meldete UFT hier "no", weil es nur
 *  auf 0xFF prueft — genau verkehrt herum.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(null_heisst_nicht_erlaubt)
{
    const char *p = "uft_hfe_wa_null.hfe";
    ASSERT(schreibe_hfe(p, 0x00));
    bool ro = false; char meta[32];
    ASSERT(oeffne(p, false, &ro, meta, sizeof(meta)));
    ASSERT(ro == true);
    ASSERT(strcmp(meta, "yes") == 0);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  4. Jeder andere Wert heisst "erlaubt" — 0xFF ebenso wie 0x01.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(ungleich_null_heisst_erlaubt)
{
    const char *p = "uft_hfe_wa_ff.hfe";
    bool ro = true; char meta[32];

    ASSERT(schreibe_hfe(p, 0xFF));
    ASSERT(oeffne(p, false, &ro, meta, sizeof(meta)));
    ASSERT(ro == false);
    ASSERT(strcmp(meta, "no") == 0);

    ASSERT(schreibe_hfe(p, 0x01));
    ASSERT(oeffne(p, false, &ro, meta, sizeof(meta)));
    ASSERT(ro == false);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  5. Der eigene Schreiber setzt 0xFF — wie SAMdisk und greaseweazle.
 *
 *  `hfe_create()` schrieb 0x00 mit dem Kommentar "Schreiben erlaubt",
 *  waehrend `uft_hfe_header_init()` im selben Baum 0xFF fuer denselben
 *  Zweck setzt. Zwei Schreiber, entgegengesetzte Werte, eine Absicht.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(eigener_schreiber_setzt_ff)
{
    const char *p = "uft_hfe_wa_created.hfe";
    remove(p);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    geo.cylinders = 2; geo.heads = 2; geo.sectors = 9; geo.sector_size = 512;

    ASSERT(uft_format_plugin_hfe.create != NULL);
    ASSERT(uft_format_plugin_hfe.create(&disk, p, &geo) == UFT_OK);
    uft_format_plugin_hfe.close(&disk);

    FILE *f = fopen(p, "rb");
    ASSERT(f != NULL);
    uint8_t kopf[32];
    size_t gelesen = fread(kopf, 1, sizeof(kopf), f);
    fclose(f);
    ASSERT(gelesen == sizeof(kopf));
    ASSERT(kopf[20] == 0xFF);   /* write_allowed */
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  6. Rundlauf: was UFT schreibt, liest UFT nicht als geschuetzt.
 *
 *  Die Probe darauf, dass Schreiber und Leser dieselbe Sprache sprechen.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(rundlauf_eigene_datei_nicht_geschuetzt)
{
    const char *p = "uft_hfe_wa_rundlauf.hfe";
    remove(p);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    geo.cylinders = 2; geo.heads = 2; geo.sectors = 9; geo.sector_size = 512;
    ASSERT(uft_format_plugin_hfe.create(&disk, p, &geo) == UFT_OK);
    uft_format_plugin_hfe.close(&disk);

    bool ro = true; char meta[32];
    ASSERT(oeffne(p, false, &ro, meta, sizeof(meta)));
    ASSERT(ro == false);
    ASSERT(strcmp(meta, "no") == 0);
    remove(p);
}

int main(void)
{
    printf("HFE: Polaritaet von write_allowed (MF-898)\n");
    RUN(fremde_hfe_ist_nicht_geschuetzt);
    RUN(waechter_aufruferwunsch_bleibt);
    RUN(null_heisst_nicht_erlaubt);
    RUN(ungleich_null_heisst_erlaubt);
    RUN(eigener_schreiber_setzt_ff);
    RUN(rundlauf_eigene_datei_nicht_geschuetzt);
    printf("\n  %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
