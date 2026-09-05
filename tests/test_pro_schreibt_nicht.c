/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_pro_schreibt_nicht.c
 * @brief Ein `write_track`, das OK meldet, muss die Datei geaendert haben
 *        (MF-880)
 *
 * -- Der Befund --------------------------------------------------------
 *
 * `src/formats/atari/uft_pro_plugin.c` ist ein REGISTRIERTES Plugin, und
 * seine Merkmalstabelle fuehrte `{ "Write", UFT_FEATURE_SUPPORTED }`.
 * Gemessen:
 *
 *   `pro_open()`        liest die Datei mit `uft_read_file()` vollstaendig
 *                       in den Speicher und behaelt WEDER Pfad NOCH `FILE*`
 *   `pro_write_track()` schreibt in `p->file_data` — die Speicherkopie —
 *                       und meldet `UFT_OK`
 *   `pro_close()`       macht `free(p->file_data)`
 *
 * Im ganzen File steht **kein einziges** `fwrite`, `fopen` oder `fseek`
 * (gezaehlt: 0), und die Plugin-Struktur hat kein `.flush`.
 *
 * Jeder Schreibvorgang auf ein PRO-Abbild wurde also still verworfen, und
 * der Aufrufer bekam Erfolg gemeldet. Das ist woertlich die Klasse, die
 * MF-522 an D64/D81 behoben hat — dort steht es im Kommentar:
 * „Eine Erfolgsmeldung ohne Tat ist in einem forensischen Werkzeug
 * schlimmer als ein Fehler."
 *
 * -- Warum NICHT ein echter Schreiber gebaut wurde ---------------------
 *
 * Die EINFRIER-REGEL (MF-363/498) verlangt fuer neuen Format-Code eine
 * benannte Referenz, jede Zahl gemessen, die Referenz im Header. Fuer PRO
 * ist im Baum nichts davon da:
 *
 *   - keine Quellenangabe in `uft_pro_plugin.c`
 *   - kein PRO-Abbild im Korpus
 *   - `PRO_MAX_TRACKS` ist 77 hier und 80 in
 *     `src/formats/atari/uft_pro_parser_v2.c` — ein Widerspruch, den
 *     niemand aufgeloest hat
 *   - der Kopfkommentar sagt „Tracks and SPT from header bytes 4-5",
 *     der Code liest `raw[7]` und `raw[6]`
 *
 * Einen Schreiber gegen diese Lage zu bauen waere genau die Wette der
 * fuenf fabrizierten Parser. Die Zusage wahr zu machen ist der kleinere
 * und richtige Schritt.
 *
 * -- Was hier geprueft wird --------------------------------------------
 *
 * Der VERTRAG, nicht das Format: **ein `write_track`, das `UFT_OK`
 * meldet, muss die Datei geaendert haben.** Diese Bedingung ist
 * formatunabhaengig und faellt gegen den Vorzustand um — dort meldete
 * PRO `UFT_OK` bei unveraenderter Datei.
 *
 * Das Pruefabbild ist synthetisch und stammt aus derselben Hand wie der
 * Leser. Fuer diese Frage genuegt das: geprueft wird nicht, ob PRO
 * richtig gelesen wird, sondern ob eine Erfolgsmeldung gedeckt ist.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_pro;

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define KOPF     16
#define SEKGR   128
#define SPUREN    2
#define SPT       2
#define SEKTOREN (SPUREN * SPT)
#define GROESSE  (KOPF + SEKTOREN * 4 + SEKTOREN * SEKGR)

static void pfad(char *p, size_t n)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_pro_%d.pro", d, rand() % 100000);
}

/** Ein Abbild, das `pro_open()` annimmt. Kopf: "APRO", spt in Byte 6,
 *  Spurzahl in Byte 7 — so, wie der Leser es tatsaechlich liest. */
static bool baue(const char *p)
{
    uint8_t *d = calloc(1, GROESSE);
    if (!d) return false;
    memcpy(d, "APRO", 4);
    d[6] = SPT;
    d[7] = SPUREN;
    /* Nutzdaten erkennbar fuellen, damit eine Aenderung auffiele. */
    for (int i = 0; i < SEKTOREN; i++)
        memset(d + KOPF + SEKTOREN * 4 + i * SEKGR, 0xA0 + i, SEKGR);

    FILE *f = fopen(p, "wb");
    if (!f) { free(d); return false; }
    bool ok = fwrite(d, 1, GROESSE, f) == GROESSE;
    fclose(f);
    free(d);
    return ok;
}

static bool lies(const char *p, uint8_t *out)
{
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    bool ok = fread(out, 1, GROESSE, f) == GROESSE;
    fclose(f);
    return ok;
}

TEST(ein_ok_beim_schreiben_muss_die_datei_aendern)
{
    /* DER ROTBEWEIS. Vor MF-880 meldete pro_write_track UFT_OK, ohne dass
     * ein Byte die Platte erreichte. */
    char p[512];
    pfad(p, sizeof p);
    ASSERT(baue(p));

    uint8_t vorher[GROESSE], nachher[GROESSE];
    ASSERT(lies(p, vorher));

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.read_only = false;
    ASSERT(uft_format_plugin_pro.open(&disk, p, false) == UFT_OK);

    /* Eine Spur mit einem Sektor, dessen Inhalt sich vom Abbild
     * unterscheidet — waere er gleich, sagte ein unveraendertes Abbild
     * nichts. */
    uint8_t nutz[SEKGR];
    memset(nutz, 0x5A, sizeof nutz);

    uft_sector_t sek;
    memset(&sek, 0, sizeof sek);
    sek.id.sector  = 0;
    sek.data       = nutz;
    sek.data_len   = SEKGR;

    uft_track_t spur;
    memset(&spur, 0, sizeof spur);
    spur.sectors      = &sek;
    spur.sector_count = 1;

    const uft_error_t rc =
        uft_format_plugin_pro.write_track(&disk, 0, 0, &spur);
    uft_format_plugin_pro.close(&disk);

    ASSERT(lies(p, nachher));
    remove(p);

    const bool geaendert = memcmp(vorher, nachher, GROESSE) != 0;

    if (rc == UFT_OK && !geaendert) {
        printf("\n      write_track meldete UFT_OK, die Datei ist "
               "unveraendert — eine Erfolgsmeldung ohne Tat\n      ");
        _fail++;
        return;
    }
    if (rc != UFT_OK && geaendert) {
        printf("\n      write_track meldete Fehler %d, hat die Datei aber "
               "veraendert\n      ", (int)rc);
        _fail++;
        return;
    }
}

TEST(die_merkmalstabelle_sagt_dasselbe_wie_der_code)
{
    /* Eine Tabelle, die „Write: SUPPORTED" sagt, waehrend write_track
     * ablehnt, ist dieselbe Luege an anderer Stelle. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    char p[512];
    pfad(p, sizeof p);
    ASSERT(baue(p));
    ASSERT(uft_format_plugin_pro.open(&disk, p, false) == UFT_OK);

    uft_track_t leer;
    memset(&leer, 0, sizeof leer);
    const uft_error_t rc =
        uft_format_plugin_pro.write_track(&disk, 0, 0, &leer);
    uft_format_plugin_pro.close(&disk);
    remove(p);

    const bool code_schreibt = (rc == UFT_OK);

    bool tabelle_schreibt = false;
    bool gefunden = false;
    for (size_t i = 0; i < uft_format_plugin_pro.feature_count; i++) {
        const uft_plugin_feature_t *f = &uft_format_plugin_pro.features[i];
        if (f->name && strcmp(f->name, "Write") == 0) {
            gefunden = true;
            tabelle_schreibt = (f->status == UFT_FEATURE_SUPPORTED);
        }
    }
    ASSERT(gefunden);

    if (tabelle_schreibt != code_schreibt) {
        printf("\n      Tabelle sagt Write=%s, der Code %s\n      ",
               tabelle_schreibt ? "SUPPORTED" : "UNSUPPORTED",
               code_schreibt ? "schreibt" : "lehnt ab");
        _fail++;
    }
}

TEST(lesen_bleibt_unberuehrt)
{
    /* Gegenprobe: die Ablehnung des Schreibens darf das Lesen nicht
     * beschaedigen. Ohne diesen Fall waere ein Plugin, das gar nichts
     * mehr kann, ebenso „bestanden". */
    char p[512];
    pfad(p, sizeof p);
    ASSERT(baue(p));

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    ASSERT(uft_format_plugin_pro.open(&disk, p, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == SPUREN);
    ASSERT(disk.geometry.sectors == SPT);

    uft_track_t spur;
    memset(&spur, 0, sizeof spur);
    const uft_error_t rc = uft_format_plugin_pro.read_track(&disk, 0, 0, &spur);
    ASSERT(rc == UFT_OK);
    ASSERT(spur.sector_count == SPT);
    ASSERT(spur.sectors != NULL);
    ASSERT(spur.sectors[0].data != NULL);
    ASSERT(spur.sectors[0].data[0] == 0xA0);   /* wie gebaut */

    for (size_t i = 0; i < spur.sector_count; i++) free(spur.sectors[i].data);
    free(spur.sectors);
    uft_format_plugin_pro.close(&disk);
    remove(p);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== PRO: eine Erfolgsmeldung braucht eine Tat (MF-880) ===\n");
    RUN(ein_ok_beim_schreiben_muss_die_datei_aendern);
    RUN(die_merkmalstabelle_sagt_dasselbe_wie_der_code);
    RUN(lesen_bleibt_unberuehrt);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
