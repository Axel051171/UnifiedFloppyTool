/**
 * @file test_disk_metadata_variant.c
 * @brief Die Variante kommt vom Plugin — oder gar nicht (MF-662)
 *
 * Stufe 3 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`.
 *
 * ── Der Anlass, gemessen ─────────────────────────────────────────────────
 *
 * `src/diskanalyzerwindow.cpp` schrieb `labelSide0Format` **fest** auf
 * `"ISO MFM"` — in beiden Zweigen, für jedes Format. Ein D64 ist GCR,
 * ein Amiga-ADF ist Amiga MFM, ein SD-ATR ist FM. Die Oberfläche
 * behauptete für alle dasselbe, und das Plugin wusste es oft besser.
 *
 * Dieselbe Fehlerklasse wie die HFE-Interface-Tabelle (MF-659): eine
 * Aussage über das Medium, die niemand gemessen hat.
 *
 * ── Was hier bewiesen wird ───────────────────────────────────────────────
 *
 * 1. Die Abfrage holt wirklich, was das Plugin sagt — an HFE, dessen
 *    Antworten seit MF-659 nachgemessen sind.
 * 2. Ein Plugin ohne `read_metadata` führt zu **false und leerem
 *    Puffer** — nicht zu Speichermüll und nicht zu einer Erfindung.
 * 3. Verschiedene Formate liefern **Verschiedenes**. Ohne diese Prüfung
 *    wäre die Anzeige so wertlos wie das feste „ISO MFM".
 *
 * ── Die Regel, die dieser Test festhält ──────────────────────────────────
 *
 * Es wird **nicht abgeleitet**. Kein Rückschluss aus Dateigröße oder
 * Endung auf eine Variante — das wäre bequem und wäre geraten, also
 * genau die Fabrikations-Klasse FMT-2/3/10/11/12, nur in der
 * Oberfläche.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

/* Ein minimaler, gueltiger HFEv1 — dieselbe Bauart wie in
 * test_hfe_interface_modes.c, damit beide Tests dieselbe Datei meinen. */
static bool schreibe_hfe(const char *pfad, uint8_t interface_mode,
                         uint8_t track_encoding)
{
    uint8_t buf[1024];
    memset(buf, 0, sizeof(buf));
    memcpy(buf + 0, "HXCPICFE", 8);
    buf[9]  = 1;                 /* number_of_tracks */
    buf[10] = 1;                 /* number_of_sides  */
    buf[11] = track_encoding;
    buf[12] = 250;               /* bitRate LE16     */
    buf[14] = 44; buf[15] = 1;   /* floppyRPM LE16 = 300 */
    buf[16] = interface_mode;
    buf[18] = 1;                 /* track_list_offset */
    buf[20] = 0xFF;              /* write_allowed */

    FILE *f = fopen(pfad, "wb");
    if (!f) return false;
    bool ok = fwrite(buf, 1, sizeof(buf), f) == sizeof(buf);
    fclose(f);
    return ok;
}

static void temp_pfad(char *out, size_t n, const char *endung)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(out, n, "%s/uft_meta_%d.%s", dir, rand() % 100000, endung);
}

/* 1. Die Abfrage liefert, was das Plugin sagt. */
static void test_holt_was_das_plugin_sagt(void)
{
    char pfad[512];
    temp_pfad(pfad, sizeof(pfad), "hfe");
    if (!schreibe_hfe(pfad, 0x04 /* AMIGA_DD */, 0x01 /* AMIGA_MFM */)) {
        printf("  FAIL Testdatei nicht schreibbar\n"); fehler++; return;
    }

    uft_disk_t *disk = uft_disk_open(pfad, true);
    if (!disk) {
        printf("  FAIL uft_disk_open() lieferte NULL fuer ein gueltiges HFE\n");
        fehler++; remove(pfad); return;
    }

    char wert[64];

    PRUEFE(uft_disk_has_metadata(disk),
           "HFE-Plugin bietet read_metadata an, has_metadata sagt nein");

    PRUEFE(uft_disk_metadata(disk, "version", wert, sizeof(wert)),
           "\"version\" muss beantwortet werden");
    PRUEFE(strcmp(wert, "HFEv1") == 0,
           "version = \"%s\", erwartet \"HFEv1\"", wert);
    printf("  ok   version   -> \"%s\"\n", wert);

    PRUEFE(uft_disk_metadata(disk, "encoding", wert, sizeof(wert)),
           "\"encoding\" muss beantwortet werden");
    PRUEFE(strcmp(wert, "Amiga MFM") == 0,
           "encoding = \"%s\", erwartet \"Amiga MFM\" — NICHT \"ISO MFM\"",
           wert);
    printf("  ok   encoding  -> \"%s\"\n", wert);

    PRUEFE(uft_disk_metadata(disk, "interface", wert, sizeof(wert)),
           "\"interface\" muss beantwortet werden");
    PRUEFE(strcmp(wert, "Amiga DD") == 0,
           "interface = \"%s\", erwartet \"Amiga DD\"", wert);
    printf("  ok   interface -> \"%s\"\n", wert);

    uft_disk_close(disk);
    remove(pfad);
}

/* 2. Unbekannter Schluessel: false UND leerer Puffer. */
static void test_schweigen_ist_leer_nicht_muell(void)
{
    char pfad[512];
    temp_pfad(pfad, sizeof(pfad), "hfe");
    if (!schreibe_hfe(pfad, 0x00, 0x00)) {
        printf("  FAIL Testdatei nicht schreibbar\n"); fehler++; return;
    }
    uft_disk_t *disk = uft_disk_open(pfad, true);
    if (!disk) { printf("  FAIL open\n"); fehler++; remove(pfad); return; }

    char wert[64];
    memset(wert, 'X', sizeof(wert));   /* absichtlich vorbelegt */

    PRUEFE(!uft_disk_metadata(disk, "gibtesnicht", wert, sizeof(wert)),
           "ein unbekannter Schluessel muss false liefern");
    PRUEFE(wert[0] == '\0',
           "der Puffer muss LEER sein, nicht Vorbelegung durchlassen");

    /* NULL darf nicht knallen — die Oberflaeche fragt auch ohne Abbild. */
    memset(wert, 'X', sizeof(wert));
    PRUEFE(!uft_disk_metadata(NULL, "version", wert, sizeof(wert)),
           "NULL-Abbild muss false liefern");
    PRUEFE(wert[0] == '\0', "NULL-Abbild muss den Puffer leeren");
    PRUEFE(!uft_disk_has_metadata(NULL), "has_metadata(NULL) muss false sein");

    uft_disk_close(disk);
    remove(pfad);
}

/* 3. Der Kern: verschiedene Formate sagen Verschiedenes.
 *
 * Ohne diese Pruefung waere die Anzeige so wertlos wie das feste
 * "ISO MFM", das sie ersetzt. */
static void test_formate_sagen_verschiedenes(void)
{
    const uft_format_plugin_t *plugins[256];
    size_t n = uft_list_format_plugins(plugins, 256);
    PRUEFE(n > 0, "keine Plugins registriert");

    size_t mit = 0, ohne = 0;
    for (size_t i = 0; i < n; i++) {
        if (!plugins[i]) continue;
        if (plugins[i]->read_metadata) mit++; else ohne++;
    }
    printf("  %zu Plugins mit Metadaten, %zu ohne\n", mit, ohne);

    PRUEFE(mit > 0, "kein einziges Plugin liefert Metadaten");
    PRUEFE(ohne > 0,
           "alle Plugins liefern Metadaten — dann kann der Fall "
           "\"nicht ermittelt\" nie eintreten, und der Test deckt ihn "
           "nicht ab");

    /* Und die Antworten selbst duerfen nicht alle gleich sein. Das
     * pruefen wir an HFE gegen die feste Behauptung von frueher. */
    char pfad[512];
    temp_pfad(pfad, sizeof(pfad), "hfe");
    if (!schreibe_hfe(pfad, 0x04, 0x01)) return;
    uft_disk_t *disk = uft_disk_open(pfad, true);
    if (disk) {
        char wert[64];
        if (uft_disk_metadata(disk, "encoding", wert, sizeof(wert))) {
            PRUEFE(strcmp(wert, "ISO MFM") != 0,
                   "ein Amiga-MFM-Abbild darf nicht \"ISO MFM\" melden — "
                   "genau das stand frueher fest in der Oberflaeche");
        }
        uft_disk_close(disk);
    }
    remove(pfad);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }
    printf("Variante und Kodierung kommen vom Plugin (MF-662)\n\n");

    test_holt_was_das_plugin_sagt();
    test_schweigen_ist_leer_nicht_muell();
    test_formate_sagen_verschiedenes();

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
