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
 *   1. der Feldname selbst — `write_allowed`, 0xFF = wahr = erlaubt
 *   2. include/uft/flux/uft_hfe.h:108      "0xFF = writable"
 *   3. include/uft/uft_hfe_format.h:99     "0xFF = write allowed"
 *   4. include/uft/uft_hfe_format.h:197    setzt 0xFF mit "Writeable"
 *   5. src/samdisk/hfe.cpp:274             schreibt 0xff fuer ein ganz
 *                                          gewoehnliches Abbild
 *   6. src/formats/hfe/uft_hfe_parser_v2.c:313
 *                                          `write_allowed ? "Yes" : "No"`
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

#define BLOCK 512u

static const char *korpus_hfe(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/gw_amigados.hfe", UFT_CORPUS_DIR);
    return p;
}

/* Minimale gueltige HFE v1, eine Spur, eine Seite — nur der Kopfwert
 * `write_allowed` ist hier von Belang. Aufbau wie in
 * `test_hfe_track0_encoding.c` beschrieben. */
static int schreibe_hfe(const char *pfad, uint8_t write_allowed)
{
    uint8_t datei[3 * BLOCK];
    memset(datei, 0, sizeof(datei));

    memcpy(datei + 0, "HXCPICFE", 8);
    datei[8]  = 0;
    datei[9]  = 1;              /* number_of_tracks */
    datei[10] = 1;              /* number_of_sides  */
    datei[11] = 0x00;           /* track_encoding: ISO MFM */
    datei[12] = 250; datei[13] = 0;
    datei[14] = 44;  datei[15] = 1;
    datei[16] = 0x07;
    datei[17] = 0x01;
    datei[18] = 1;   datei[19] = 0;    /* track_list_offset = Block 1 */
    datei[20] = write_allowed;
    datei[21] = 0xFF;                  /* single_step */
    datei[22] = 0xFF; datei[23] = 0xFF;  /* kein Spur-0-Ersatz (MF-897) */
    datei[24] = 0xFF; datei[25] = 0xFF;

    uint8_t *lut = datei + BLOCK;
    lut[0] = 2; lut[1] = 0;
    lut[2] = (uint8_t)(BLOCK & 0xFF); lut[3] = (uint8_t)(BLOCK >> 8);
    memset(datei + 2 * BLOCK, 0xA5, BLOCK);

    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    size_t n = fwrite(datei, 1, sizeof(datei), f);
    fclose(f);
    return n == sizeof(datei);
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
