/**
 * @file test_jv1_ist_einseitig.c
 * @brief JV1 gegen MAME und die Formatdokumentation (MF-1016).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * `jv1` stand auf **T3**. Zwei Befunde, beide gemessen.
 *
 * **Befund 1 — >40 Spuren wurden zweiseitig gelesen.** Der alte
 * `jv1_detect_geometry()` nahm bei mehr als 40 Spuren eine zweite Seite
 * an. Gemessen an einer Datei von 204 800 Byte (80 x 10 x 256), jede
 * Spur mit ihrer eigenen Nummer gefuellt:
 *
 *     Geometrie  : 40 Zyl, 2 Koepfe        (Orakel: 80 Zyl, 1 Kopf)
 *     Zyl 40/K 0 : nicht lesbar
 *
 * Die Spuren 40..79 lagen auf Kopf 1 statt auf den Zylindern 40..79.
 *
 * **Befund 2 — die Sektornummern waren 1..10 statt 0..9.** Der alte
 * Rumpf rief `uft_format_add_sector()`, und die Funktion nimmt laut
 * ihrem eigenen Kopf einen **0-basierten Laufindex und addiert 1** —
 * richtig fuer IBM-PC, falsch fuer alles, was ab 0 zaehlt.
 *
 * ── Die Referenzen ───────────────────────────────────────────────────
 *
 * **Tim Mann, „Common File Formats for Emulated TRS-80 Floppy Disks"**
 * (https://www.tim-mann.org/trs80/dskspec.html, abgerufen 2026-09-10),
 * woertlich:
 *
 *   „There are 10 sectors per track (i.e., single density), numbered
 *    **0 through 9**, and **only one side**."
 *
 * **MAME `formats/trs80_dsk.cpp`** (BSD-3-Clause, Dirk Best): drei
 * Geometrien, 35/40/80 Spuren, **alle mit `head_count = 1`**, und
 * `sector_base_id = 0`.
 *
 * Zwei unabhaengige Haende, dieselbe Aussage — und die eine ist die
 * Formatbeschreibung selbst.
 *
 * ── Und ein gruener Test, der den Fehler bewacht hat ─────────────────
 *
 * `tests/test_plugin_probe_real.c` verlangte woertlich:
 *
 *     // 41..79 tracks are only valid when even (two heads)
 *     ASSERT(!probe_sized(&uft_format_plugin_jv1, ..., 41 * TRACK, ...));
 *
 * Diese Zusage folgte aus der zweiten Seite, die es nicht gibt. Sie ist
 * mit MF-1016 berichtigt. Dieselbe Gestalt wie MF-992, wo zehn gruene
 * Tests durch dieselbe falsche Struktur zurueckgelesen haben: **ein
 * Test kann einen Fehler festhalten statt ihn zu fangen.**
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_jv1;

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

#define SPT     10
#define SEK     256
#define SPUR    (SPT * SEK)         /* 2560 */

/* Eine JV1 mit `spuren` Spuren; jede Spur ist mit ihrer Nummer
 * gefuellt, jeder Sektor traegt in Byte 1 seine Sektornummer. So
 * verraet der Inhalt, WELCHE Spur und WELCHER Sektor geliefert wurde. */
static int baue_jv1(const char *pfad, int spuren)
{
    uint8_t *b = (uint8_t *)calloc(1, (size_t)spuren * SPUR);
    if (!b) return 0;
    for (int t = 0; t < spuren; t++)
        for (int s = 0; s < SPT; s++) {
            uint8_t *sek = b + (size_t)t * SPUR + (size_t)s * SEK;
            memset(sek, (uint8_t)t, SEK);
            sek[1] = (uint8_t)s;
        }
    FILE *f = fopen(pfad, "wb");
    if (!f) { free(b); return 0; }
    int ok = (fwrite(b, 1, (size_t)spuren * SPUR, f) == (size_t)spuren * SPUR);
    fclose(f);
    free(b);
    return ok;
}

static void frei(uft_track_t *t)
{
    if (!t) return;
    free(t->sectors);
    memset(t, 0, sizeof(*t));
}

int main(void)
{
    printf("=== JV1 ist einseitig (MF-1016) ===\n");

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1016.jv1", tmp);

    char h[220];

    /* ── 80 Spuren: der Befund ───────────────────────────────────────── */
    if (!baue_jv1(pfad, 80)) {
        printf("  [ROT]  Pruefdatei liess sich nicht schreiben\n");
        return 1;
    }

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_error_t rc = uft_format_plugin_jv1.open(&disk, pfad, true);
    snprintf(h, sizeof(h), "open=%d, %u Zyl, %u Koepfe, %u Sek, %u gesamt",
             (int)rc, disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.total_sectors);
    pruefe("204800 Byte sind 80 Zylinder auf EINEM Kopf",
           rc == UFT_OK && disk.geometry.cylinders == 80
           && disk.geometry.heads == 1 && disk.geometry.sectors == SPT
           && disk.geometry.total_sectors == 800, h);

    if (rc == UFT_OK) {
        /* Zylinder 40 gab es vorher nicht (bei 40 Zylindern ist er
         * ausserhalb) — und sein Inhalt sagt, welche Spur es ist. */
        uft_track_t t;
        memset(&t, 0, sizeof(t));
        uft_error_t r = uft_format_plugin_jv1.read_track(&disk, 40, 0, &t);
        snprintf(h, sizeof(h), "rc=%d, %zu Sektoren, Fuellbyte %02X "
                 "(erwartet 28 = Spur 40)", (int)r, (size_t)t.sector_count,
                 t.sector_count ? t.sectors[0].data[0] : 0);
        pruefe("Zylinder 40 ist lesbar und traegt Spur 40",
               r == UFT_OK && t.sector_count == SPT
               && t.sectors[0].data[0] == 40, h);

        /* Sektornummern 0..9, wie auf der Diskette. */
        int nummern_ok = (t.sector_count == SPT);
        for (size_t s = 0; s < t.sector_count && nummern_ok; s++)
            if (t.sectors[s].id.sector != (uint8_t)s) nummern_ok = 0;
        snprintf(h, sizeof(h), "erste IDs: %u %u %u ... (erwartet 0 1 2)",
                 t.sector_count > 0 ? t.sectors[0].id.sector : 255,
                 t.sector_count > 1 ? t.sectors[1].id.sector : 255,
                 t.sector_count > 2 ? t.sectors[2].id.sector : 255);
        pruefe("die Sektornummern sind 0..9, nicht 1..10", nummern_ok, h);

        /* Und der Inhalt gehoert zur richtigen Sektornummer: Byte 1
         * traegt die Sektornummer aus der Pruefdatei. */
        int inhalt_ok = (t.sector_count == SPT);
        for (size_t s = 0; s < t.sector_count && inhalt_ok; s++)
            if (t.sectors[s].data[1] != (uint8_t)s) inhalt_ok = 0;
        pruefe("und der Inhalt passt zur Nummer (Byte 1 = Sektornummer)",
               inhalt_ok, NULL);
        frei(&t);

        /* Die letzte Spur. */
        memset(&t, 0, sizeof(t));
        r = uft_format_plugin_jv1.read_track(&disk, 79, 0, &t);
        snprintf(h, sizeof(h), "rc=%d, Fuellbyte %02X (erwartet 4F)",
                 (int)r, t.sector_count ? t.sectors[0].data[0] : 0);
        pruefe("Zylinder 79 ist die letzte Spur",
               r == UFT_OK && t.sector_count == SPT
               && t.sectors[0].data[0] == 79, h);
        frei(&t);

        /* Kopf 1 gibt es nicht. */
        memset(&t, 0, sizeof(t));
        r = uft_format_plugin_jv1.read_track(&disk, 0, 1, &t);
        snprintf(h, sizeof(h), "rc=%d", (int)r);
        pruefe("Kopf 1 wird abgewiesen — JV1 hat keine zweite Seite",
               r != UFT_OK, h);
        frei(&t);

        memset(&t, 0, sizeof(t));
        r = uft_format_plugin_jv1.read_track(&disk, 80, 0, &t);
        pruefe("Zylinder 80 wird abgewiesen", r != UFT_OK, NULL);
        frei(&t);

        memset(&t, 0, sizeof(t));
        r = uft_format_plugin_jv1.read_track(&disk, -1, 0, &t);
        pruefe("Zylinder -1 wird abgewiesen (MF-519)", r != UFT_OK, NULL);
        frei(&t);

        uft_format_plugin_jv1.close(&disk);
    }
    remove(pfad);

    /* ── 41 Spuren: der Fall, den ein gruener Test abgewiesen hat ────── */
    {
        baue_jv1(pfad, 41);
        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t r = uft_format_plugin_jv1.open(&d, pfad, true);
        snprintf(h, sizeof(h), "open=%d, %u Zyl, %u Koepfe", (int)r,
                 d.geometry.cylinders, d.geometry.heads);
        pruefe("41 Spuren gehen durch — die alte Regel \"nur gerade\" "
               "kam von der zweiten Seite",
               r == UFT_OK && d.geometry.cylinders == 41
               && d.geometry.heads == 1, h);
        if (r == UFT_OK) uft_format_plugin_jv1.close(&d);
        remove(pfad);
    }

    /* ── Gegenproben an der Sonde ───────────────────────────────────── */
    {
        uint8_t kopf[32];
        memset(kopf, 0, sizeof(kopf));
        int conf = -1;
        struct { size_t groesse; int erwartet; const char *was; } f[] = {
            { SPUR,            1, "eine Spur geht durch" },
            { 35 * SPUR,       1, "35 Spuren gehen durch (MAME SSSD)" },
            { 40 * SPUR,       1, "40 Spuren gehen durch (MAME SSSD)" },
            { 80 * SPUR,       1, "80 Spuren gehen durch (MAME SSQD)" },
            { 41 * SPUR,       1, "41 Spuren gehen durch" },
            { 81 * SPUR,       0, "81 Spuren nicht — 80 ist die groesste "
                                  "Geometrie des Orakels" },
            { SPUR + 1,        0, "keine volle Spur wird abgewiesen" },
            { 0,               0, "eine leere Datei wird abgewiesen" },
        };
        for (size_t i = 0; i < sizeof(f)/sizeof(f[0]); i++) {
            conf = -1;
            bool p = uft_format_plugin_jv1.probe(kopf, sizeof(kopf),
                                                 f[i].groesse, &conf);
            snprintf(h, sizeof(h), "%zu Byte -> %s (Konfidenz %d)",
                     f[i].groesse, p ? "ja" : "nein", conf);
            pruefe(f[i].was, (p ? 1 : 0) == f[i].erwartet, h);
        }

        conf = -1;
        uft_format_plugin_jv1.probe(kopf, sizeof(kopf), 35 * SPUR, &conf);
        snprintf(h, sizeof(h), "Konfidenz %d", conf);
        pruefe("und die Sonde beansprucht nur die Groesse (MF-729: 30..49)",
               conf >= 30 && conf < 50, h);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
