/**
 * @file test_mgt_schreibt_in_die_datei.c
 * @brief `mgt_write_track()` muss die Datei erreichen (MF-1006).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * **MF-930** fand elf Plugins mit einem vollstaendigen, aber
 * unerreichbaren Dateischreiber: `write_track` machte ein `memcpy` in
 * die Speicherkopie und meldete `UFT_OK`, `close()` gab den Puffer
 * frei, `plugin->flush` wird im ganzen Baum von niemandem gerufen.
 *
 * `mgt` ist der **dritte**, der verdrahtet wird — nach `opus` (MF-931)
 * und `cfi` (MF-1004). Verzeichnet als **P3-204**.
 *
 * ── Warum er an die Reihe kam ───────────────────────────────────────
 *
 * **Weil seine Leseseite zuerst gehoben wurde.** MF-1006 hat sie Feld
 * fuer Feld gegen MAMEs `src/lib/formats/coupedsk.cpp` gehalten
 * (`mgt_format::load()`, BSD-3-Clause) — Geometrie 80 x 2 x 10 x 512,
 * Sektor-IDs 1..10, Spurversatz `(track*2 + head) * track_size`. Der
 * Abgleich fand **keinen** Fehler; `tests/test_mgt_gegen_mame.c` nagelt
 * die Uebereinstimmung fest, Mutationsmatrix 3 von 3.
 *
 * Ein Rundlauf durch einen UNGEPRUEFTEN Leser beweist nur, dass unser
 * Leser unseren Schreiber versteht — die Selbstbestaetigung aus MF-992.
 *
 * ── Was dieser Test belegt ──────────────────────────────────────────
 *
 * Gemessen wird am ERGEBNIS: schreiben, `close()`, NEU oeffnen,
 * zuruecklesen — nicht am Funktionszeiger. Diese Form verlangt P3-154
 * seit MF-880.
 *
 * Und hier reicht es weiter als bei `cfi`: **MGT ist ein kopfloses
 * Sektorabbild**. Die erzeugte Datei ist damit strukturell kanonisch —
 * 80 x 2 x 10 x 512 an genau den Versaetzen, die MF-1006 gegen das
 * Orakel gemessen hat. Bei `cfi` packt und liest derselbe Baum, dort
 * blieb die Aussage auf „die Bytes erreichen die Datei" beschraenkt.
 * Fall 5 prueft deshalb den ROHEN Dateiinhalt am errechneten Versatz.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_mgt.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

extern const uft_format_plugin_t uft_format_plugin_mgt;

#define ZYLINDER      80
#define KOEPFE        2
#define SEKTOREN      10
#define SEKTORGROESSE 512
#define SPURGROESSE   (SEKTOREN * SEKTORGROESSE)      /* 5120   */
#define DATEIGROESSE  (ZYLINDER * KOEPFE * SPURGROESSE) /* 819200 */

#define NEUER_WERT    0x3C
#define PRUEF_ZYL     7
#define PRUEF_KOPF    1

static uint8_t marke(int c, int h)
{
    return (uint8_t)((c * 2 + h) & 0xFF);
}

static int baue_mgt(const char *pfad)
{
    uint8_t *p = (uint8_t *)malloc(DATEIGROESSE);
    if (!p) return 0;
    for (int c = 0; c < ZYLINDER; c++)
        for (int h = 0; h < KOEPFE; h++)
            memset(p + ((size_t)c * 2 + h) * SPURGROESSE,
                   marke(c, h), SPURGROESSE);
    FILE *f = fopen(pfad, "wb");
    int ok = 0;
    if (f) {
        ok = (fwrite(p, 1, DATEIGROESSE, f) == DATEIGROESSE);
        ok = (fclose(f) == 0) && ok;
    }
    free(p);
    return ok;
}

static void setze_pfad(uft_disk_t *d, const char *pfad)
{
    snprintf(d->path_buf, sizeof(d->path_buf), "%s", pfad);
    d->path = d->path_buf;
}

static void spur_frei(uft_track_t *tr)
{
    if (!tr) return;
    for (uint8_t s = 0; s < tr->sector_count; s++)
        free(tr->sectors[s].data);
    free(tr->sectors);
    memset(tr, 0, sizeof(*tr));
}

int main(void)
{
    printf("=== mgt_write_track erreicht die Datei (MF-1006) ===\n");

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1006_mgt_rundlauf.mgt", tmp);

    if (!baue_mgt(pfad)) {
        printf("  [ROT]  Pruefdatei liess sich nicht schreiben\n");
        return 1;
    }

    pruefe("das Plugin hat ein write_track",
           uft_format_plugin_mgt.write_track != NULL, NULL);
    if (!uft_format_plugin_mgt.write_track) {
        remove(pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    /* ── schreiben ─────────────────────────────────────────────────── */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    setze_pfad(&disk, pfad);

    uft_error_t rc = uft_format_plugin_mgt.open(&disk, pfad, false);
    pruefe("die Pruefdatei laesst sich oeffnen", rc == UFT_OK, NULL);
    if (rc != UFT_OK) {
        remove(pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    rc = uft_format_plugin_mgt.read_track(&disk, PRUEF_ZYL, PRUEF_KOPF, &t);
    if (rc == UFT_OK && t.sector_count > 0 && t.sectors[0].data)
        memset(t.sectors[0].data, NEUER_WERT, t.sectors[0].data_size);

    uft_error_t wrc =
        uft_format_plugin_mgt.write_track(&disk, PRUEF_ZYL, PRUEF_KOPF, &t);
    char h[190];
    snprintf(h, sizeof(h),
             "write_track meldete %d -- vor MF-1006 stand hier "
             "UFT_ERROR_NOT_SUPPORTED, davor UFT_OK ohne Tat", (int)wrc);
    pruefe("write_track meldet Erfolg", wrc == UFT_OK, h);

    spur_frei(&t);
    uft_format_plugin_mgt.close(&disk);

    /* ── zuruecklesen ─────────────────────────────────────────────── */
    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    setze_pfad(&d2, pfad);
    rc = uft_format_plugin_mgt.open(&d2, pfad, true);
    pruefe("die geschriebene Datei laesst sich neu oeffnen", rc == UFT_OK,
           "der Schreiber hat die Datei unbrauchbar gemacht");

    if (rc == UFT_OK) {
        uft_track_t t2;
        memset(&t2, 0, sizeof(t2));
        uft_error_t r2 =
            uft_format_plugin_mgt.read_track(&d2, PRUEF_ZYL, PRUEF_KOPF, &t2);
        int traegt = (r2 == UFT_OK && t2.sector_count > 0
                      && t2.sectors[0].data
                      && t2.sectors[0].data[0] == NEUER_WERT);
        snprintf(h, sizeof(h),
                 "Sektor (%d,%d,1) traegt 0x%02X statt 0x%02X",
                 PRUEF_ZYL, PRUEF_KOPF,
                 (r2 == UFT_OK && t2.sector_count && t2.sectors[0].data)
                     ? (unsigned)t2.sectors[0].data[0] : 0u,
                 (unsigned)NEUER_WERT);
        pruefe("die Aenderung steht in der DATEI", traegt, h);
        spur_frei(&t2);

        /* Gegenprobe (MF-931): die NACHBARSPUR darf sich nicht geaendert
         * haben. Lesen und Schreiben mit demselben falschen Index liefen
         * sonst rund und deckten einander. */
        uft_track_t t0;
        memset(&t0, 0, sizeof(t0));
        uft_error_t r0 =
            uft_format_plugin_mgt.read_track(&d2, PRUEF_ZYL, 0, &t0);
        int nachbar_heil = (r0 == UFT_OK && t0.sector_count > 0
                            && t0.sectors[0].data
                            && t0.sectors[0].data[0] == marke(PRUEF_ZYL, 0));
        snprintf(h, sizeof(h),
                 "Spur (%d,0) traegt 0x%02X statt 0x%02X -- der "
                 "Schreibindex trifft die falsche Spur",
                 PRUEF_ZYL,
                 (r0 == UFT_OK && t0.sector_count && t0.sectors[0].data)
                     ? (unsigned)t0.sectors[0].data[0] : 0u,
                 (unsigned)marke(PRUEF_ZYL, 0));
        pruefe("die Nachbarspur (gleicher Zylinder, Kopf 0) ist heil",
               nachbar_heil, h);
        spur_frei(&t0);

        uft_format_plugin_mgt.close(&d2);
    }

    /* ── der ROHE Dateiinhalt am errechneten Versatz ───────────────── */
    {
        FILE *f = fopen(pfad, "rb");
        int roh_ok = 0;
        long groesse = -1;
        uint8_t b = 0;
        if (f) {
            fseek(f, 0, SEEK_END);
            groesse = ftell(f);
            /* Versatz nach der gegen MAME gemessenen Formel */
            long versatz = ((long)PRUEF_ZYL * 2 + PRUEF_KOPF) * SPURGROESSE;
            if (fseek(f, versatz, SEEK_SET) == 0 && fread(&b, 1, 1, f) == 1)
                roh_ok = (b == NEUER_WERT);
            fclose(f);
        }
        snprintf(h, sizeof(h),
                 "Datei %ld Byte, Byte an ((%d*2+%d)*5120) = 0x%02X statt "
                 "0x%02X", groesse, PRUEF_ZYL, PRUEF_KOPF, (unsigned)b,
                 (unsigned)NEUER_WERT);
        pruefe("der ROHE Dateiinhalt steht am Versatz aus dem Orakel",
               groesse == DATEIGROESSE && roh_ok, h);
    }

    remove(pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
