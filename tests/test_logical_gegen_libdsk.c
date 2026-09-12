/**
 * @file test_logical_gegen_libdsk.c
 * @brief Logical: kein Kopf, und die Anordnung IST das Format (MF-1032)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────
 *
 * libdsk (John Elliott, **LGPL-2+**; **nur gelesen**, Kanal *Spec* nach
 * MF-695 — und zusaetzlich **ausgefuehrt** zur Abnahme). Vier Stellen
 * tragen diesen Test:
 *
 *   `lib/drvlogi.c`   Z. 24-26  „a flat file, like drvposix, but with
 *                               the sides laid out in the order
 *                               specified by the disk geometry"
 *                     Z. 67-85  `logical_open()` prueft **NICHTS**
 *                     Z. 124-147 `logical_read()`: `dg_ps2ls()` mal
 *                               `dg_secsize`
 *                     Z. 150-165 Loecher werden mit **0xE5** gefuellt
 *   `lib/dsklphys.c`  Z. 91-118 `dg_pt2lt()` — die **vier Gesetze**
 *                     Z. 28-48  `dg_ps2ls()` — `spur * sektoren +
 *                               (sektor - dg_secbase)`
 *   `lib/dsksgeom.c`  Z. 53-55  `acorn160/320/640` mit
 *                               **`dg_secbase = 0`**
 *                     Z. 49     `ibm720` = **SIDES_OUTBACK**
 *
 * ── Was der Vorzustand war ──────────────────────────────────────────
 *
 * **UFT konnte keine echte Logical-Datei lesen, und die Anordnung, die
 * das Format ausmacht, war gar nicht umgesetzt.** Verlangt wurden eine
 * Kennung `"LGD\0"` und ein **32-Byte-Kopf** mit Geometriefeldern —
 * beides gibt es nicht. Und der Leser lief `for (c) for (h)` linear,
 * also **immer SIDES_ALT**; damit war „logical" byteweise dasselbe wie
 * ein gewoehnliches flaches Abbild.
 *
 * Dazu: `if (first_sector == 0) first_sector = 1;` — bei den drei
 * Acorn-Formaten mit `dg_secbase = 0` war damit jede Sektornummer um
 * eins zu hoch.
 *
 * ── Warum es keine Sonde gibt, und das GEMESSEN ist ─────────────────
 *
 * In libdsks **eigener** Geometrietafel teilt **jede** der acht
 * nicht-ALT-Geometrien ihre Dateigroesse mit mindestens einer
 * ALT-Geometrie (ibm720/pcw720, ibm1200/pcw1200, ibm1440/pcw1440,
 * acorn160/ibm160, acorn320/ibm320, acorn640/trdos640, mgt800/pcw800,
 * pcpm320/ibm320). Acht von acht — die Groesse kann die Anordnung in
 * **keinem** Fall entscheiden.
 *
 * ── T1b: libdsk hat die Pruefdateien GESCHRIEBEN ────────────────────
 *
 * Nicht UFT. Die Eingabe ist ein flaches ALT-Abbild mit
 * selbstbenennenden Sektoren; `dsktrans` hat es mit libdsks **eigener**
 * Geometrie in die `logical`-Anordnung gewandelt:
 *
 *     dsktrans -itype raw -format acorn640 -otype logical
 *         -> 655360 Byte, SIDES_OUTOUT
 *     dsktrans -itype raw -format ibm720  -otype logical
 *         -> 737280 Byte, SIDES_OUTBACK, dg_secbase 1
 *
 * Beide sind **byteidentisch** mit der unabhaengig aus `dg_pt2lt()` und
 * `dg_ps2ls()` nachgerechneten Fassung, und der Rueckweg
 * (`-otype raw`) liefert wieder genau die Eingabe. Die **Umordnung** hat
 * also eine fremde Hand gerechnet, und die Datei kommt aus ihrer Feder.
 *
 * **Damit ist P3-333 berichtigt.** Dort stand, libdsk erzeuge keine
 * Fixtures, weil „sein `raw`-Treiber die Geometrie eines kopflosen
 * Abbilds nicht bekommt". Der Kanal dafuer ist `-format <name>`, und er
 * war die ganze Zeit da.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/uft_logical.h"

extern const uft_format_plugin_t uft_format_plugin_logical;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define F_OO "libdsk_acorn640_outout.logical"
#define F_OB "libdsk_ibm720_outback.logical"

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)len);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *n = (size_t)len;
    return b;
}

/* `acorn640`: SIDES_OUTOUT, 80 x 2 x 16 x 256, dg_secbase = 0 */
static const uft_logical_geometry_t G_ACORN640 = {
    80, 2, 16, 256, 0, UFT_LOGI_SIDES_OUTOUT, UFT_ENC_MFM
};
/* dieselbe Geometrie, aber mit dem FALSCHEN Gesetz — die Gegenprobe */
static const uft_logical_geometry_t G_ACORN640_ALT = {
    80, 2, 16, 256, 0, UFT_LOGI_SIDES_ALT, UFT_ENC_MFM
};
/* `ibm720`: SIDES_OUTBACK, 80 x 2 x 9 x 512, dg_secbase = 1 */
static const uft_logical_geometry_t G_IBM720 = {
    80, 2, 9, 512, 1, UFT_LOGI_SIDES_OUTBACK, UFT_ENC_MFM
};

/** Alle Sektoren gegen ihren eigenen Namen halten. */
static void pruefe_alle(const uft_logical_geometry_t *g,
                        const uint8_t *datei, size_t n,
                        const char *wie)
{
    uft_disk_image_t *img = NULL;
    logical_read_result_t r;
    char erw[32], d1[280];
    int falsch = 0, gesamt = 0;
    int fc = -1, fh = -1, fs = -1;
    char gefunden[32];
    uft_error_t e;
    uint16_t c, h;
    size_t s;

    memset(gefunden, 0, sizeof(gefunden));
    e = uft_logical_read_mem(datei, n, g, &img, &r);
    if (e != UFT_OK || !img) {
        snprintf(d1, sizeof(d1), "read_mem = %d", (int)e);
        pruefe(wie, 0, d1);
        return;
    }

    for (c = 0; c < g->cylinders; c++) {
        for (h = 0; h < g->heads; h++) {
            uft_track_t *t = img->track_data[(size_t)c * g->heads + h];
            if (!t) { falsch += g->sectors; continue; }
            for (s = 0; s < t->sector_count; s++) {
                const uft_sector_t *sect = &t->sectors[s];
                int nummer = (int)g->first_sector + (int)s;
                gesamt++;
                snprintf(erw, sizeof(erw), "UFT-K C%02d H%d S%02d ",
                         (int)c, (int)h, nummer);
                if (!sect->data || sect->id.sector != (uint8_t)nummer
                    || memcmp(sect->data, erw, strlen(erw)) != 0) {
                    falsch++;
                    if (fc < 0) {
                        fc = c; fh = h; fs = nummer;
                        if (sect->data) {
                            memcpy(gefunden, sect->data, 17);
                            gefunden[17] = 0;
                        } else {
                            snprintf(gefunden, sizeof(gefunden), "(keine Daten)");
                        }
                    }
                }
            }
        }
    }
    uft_disk_free(img);

    snprintf(d1, sizeof(d1), "%d Sektoren, %d falsch; erster Fehler "
             "C%d H%d S%d -> \"%s\"", gesamt, falsch, fc, fh, fs, gefunden);
    pruefe(wie, gesamt == (int)g->cylinders * g->heads * g->sectors
           && falsch == 0, d1);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_logical;
    char pfad[600], d1[300];
    uint8_t *foo = NULL, *fob = NULL;
    size_t noo = 0, nob = 0;

    printf("Logical gegen libdsk drvlogi.c + dsklphys.c (LGPL-2+, nur gelesen)\n");
    printf("=================================================================\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_OO);
    foo = lies(pfad, &noo);
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_OB);
    fob = lies(pfad, &nob);
    if (!foo || !fob) {
        printf("  [SKIP] Pruefdateien fehlen im Korpus (%s / %s)\n", F_OO, F_OB);
        free(foo); free(fob);
        printf("\n%d gruen, %d rot, 1 uebersprungen\n", gruen, rot);
        return 0;
    }
    printf("  %s: %zu Byte, %s: %zu Byte\n\n", F_OO, noo, F_OB, nob);

    /* ── 1. Die vier Gesetze, an ihren Raendern ────────────────────── */
    {
        long a0  = uft_logical_track_index(0, 0, &G_ACORN640_ALT);
        long a1  = uft_logical_track_index(0, 1, &G_ACORN640_ALT);
        long o0  = uft_logical_track_index(0, 0, &G_ACORN640);
        long o1  = uft_logical_track_index(0, 1, &G_ACORN640);
        long o79 = uft_logical_track_index(79, 1, &G_ACORN640);
        long b0  = uft_logical_track_index(0, 0, &G_IBM720);
        long b1  = uft_logical_track_index(0, 1, &G_IBM720);
        long b79 = uft_logical_track_index(79, 1, &G_IBM720);
        snprintf(d1, sizeof(d1), "ALT C0H0=%ld C0H1=%ld | OUTOUT C0H0=%ld "
                 "C0H1=%ld C79H1=%ld | OUTBACK C0H0=%ld C0H1=%ld C79H1=%ld",
                 a0, a1, o0, o1, o79, b0, b1, b79);
        pruefe("dg_pt2lt(): ALT 0/1 · OUTOUT 0/80/159 · OUTBACK 0/159/80 "
               "— Kopf 1 laeuft bei OUTBACK RUECKWAERTS",
               a0 == 0 && a1 == 1
               && o0 == 0 && o1 == 80 && o79 == 159
               && b0 == 0 && b1 == 159 && b79 == 80, d1);
    }

    /* ── 2. Der Versatz, samt dg_secbase ──────────────────────────── */
    {
        long x0 = uft_logical_offset(0, 0, 0, &G_ACORN640);
        long x1 = uft_logical_offset(0, 0, 1, &G_ACORN640);
        long xh = uft_logical_offset(0, 1, 0, &G_ACORN640);
        long y1 = uft_logical_offset(0, 0, 1, &G_IBM720);   /* secbase 1! */
        long y0 = uft_logical_offset(0, 0, 0, &G_IBM720);   /* < secbase */
        snprintf(d1, sizeof(d1), "acorn640 S0=%ld S1=%ld C0H1S0=%ld | "
                 "ibm720 S1=%ld S0(=unter secbase)=%ld", x0, x1, xh, y1, y0);
        pruefe("dg_ps2ls(): acorn640 0 / 256 / 327680 (Kopf 1 hinter der "
               "ganzen Seite 0), ibm720 Sektor 1 bei 0 — und Sektor 0 "
               "liegt UNTER dg_secbase und wird abgewiesen",
               x0 == 0 && x1 == 256 && xh == 80L * 16 * 256
               && y1 == 0 && y0 == -1, d1);
    }

    /* ── 3. Es gibt keinen Kopf ───────────────────────────────────── */
    {
        char n[24];
        memcpy(n, foo, 17); n[17] = 0;
        snprintf(d1, sizeof(d1), "%zu Byte; bei Versatz 0 steht \"%s\"",
                 noo, n);
        pruefe("Pruefdatei: genau 655360 Byte, und bei Versatz 0 steht "
               "SOFORT ein Sektor (C00 H0 S00) — kein Kopf, keine Kennung",
               noo == 655360u
               && memcmp(foo, "UFT-K C00 H0 S00 ", 17) == 0, d1);
    }

    /* ── 4. OUTOUT: alle 2560 Sektoren ────────────────────────────── */
    pruefe_alle(&G_ACORN640, foo, noo,
                "OUTOUT (acorn640): alle 2560 Sektoren benennen sich selbst "
                "richtig, und die Sektor-IDs laufen ab 0 (dg_secbase = 0)");

    /* ── 5. Gegenprobe: dieselbe Datei mit dem FALSCHEN Gesetz ─────
     *
     * Das ist die Zusage, die L2 isoliert. Vorher rechnete der Leser
     * IMMER ALT; wenn ALT hier durchgehen wuerde, waere das Gesetz
     * gleichgueltig — und das Format sinnlos. */
    {
        uft_disk_image_t *img = NULL;
        uft_error_t e = uft_logical_read_mem(foo, noo, &G_ACORN640_ALT,
                                            &img, NULL);
        char n[24];
        int ok = 0;
        memset(n, 0, sizeof(n));
        if (e == UFT_OK && img) {
            /* Bei ALT liegt am Anfang der zweiten Haelfte Zylinder 40,
             * Kopf 0 — in der OUTOUT-Datei steht dort aber Kopf 1,
             * Zylinder 0. Der ALT-Leser muss also an Zylinder 40 den
             * Sektor von Kopf 1 Zylinder 0 ausliefern. */
            uft_track_t *t = img->track_data[(size_t)40 * 2 + 0];
            if (t && t->sector_count && t->sectors[0].data) {
                memcpy(n, t->sectors[0].data, 17);
                ok = (memcmp(n, "UFT-K C00 H1 S00 ", 17) == 0);
            }
            uft_disk_free(img);
        }
        snprintf(d1, sizeof(d1), "read_mem=%d, C40 H0 liefert \"%s\" "
                 "(erwartet den Sektor von C00 H1)", (int)e, n);
        pruefe("Gegenprobe zu L2: mit dem GESETZ ALT gelesen liefert "
               "Zylinder 40 Kopf 0 den Sektor von Zylinder 0 Kopf 1 — die "
               "Anordnung ist also wirklich wirksam", ok, d1);
    }

    /* ── 6. OUTBACK: alle 1440 Sektoren, mit dg_secbase = 1 ───────── */
    pruefe_alle(&G_IBM720, fob, nob,
                "OUTBACK (ibm720): alle 1440 Sektoren benennen sich selbst "
                "richtig, IDs ab 1 (dg_secbase = 1)");

    /* ── 7. Die OUTBACK-Marke in der Datei selbst ─────────────────── */
    {
        size_t letzte = (size_t)(2 * 80 - 1) * 9 * 512;
        char n[24];
        memcpy(n, fob + letzte, 17); n[17] = 0;
        snprintf(d1, sizeof(d1), "%zu Byte; letzte logische Spur bei %zu "
                 "traegt \"%s\"", nob, letzte, n);
        pruefe("OUTBACK am Objekt: die LETZTE logische Spur traegt "
               "Zylinder 0 Kopf 1 — genau das meint „out and back\"",
               nob == 737280u
               && memcmp(fob + letzte, "UFT-K C00 H1 S01 ", 17) == 0, d1);
    }

    /* ── 8. Geometrie-Schranken (MF-543) ─────────────────────────── */
    {
        uft_logical_geometry_t g = G_ACORN640;
        int a, b, c, d;
        g.heads = 5;                  a = uft_logical_geometry_ok(&g);
        g = G_ACORN640; g.sector_size = 333; b = uft_logical_geometry_ok(&g);
        g = G_ACORN640; g.cylinders = 257;   c = uft_logical_geometry_ok(&g);
        /* libdsk selbst: `if (self->dg_heads > 2) return DSK_ERR_BADPARM;`
         * im OUTBACK-Zweig (dsklphys.c Z. 107). */
        g = G_IBM720;   g.heads = 3;         d = uft_logical_geometry_ok(&g);
        snprintf(d1, sizeof(d1), "heads=5 -> %d, secsize=333 -> %d, "
                 "cyl=257 -> %d, OUTBACK mit 3 Koepfen -> %d", a, b, c, d);
        pruefe("Schranken: 5 Koepfe, Sektorgroesse 333 und 257 Zylinder "
               "werden abgewiesen — und OUTBACK mit drei Koepfen auch, weil "
               "libdsk das selbst tut",
               !a && !b && !c && !d, d1);
    }

    /* ── 9. Eine zu kleine Datei erfindet keine Sektoren ──────────── */
    {
        uft_disk_image_t *img = NULL;
        uft_error_t e = uft_logical_read_mem(foo, noo - 1, &G_ACORN640,
                                            &img, NULL);
        snprintf(d1, sizeof(d1), "read_mem = %d", (int)e);
        pruefe("Gegenprobe: 655359 Byte reichen fuer diese Geometrie nicht "
               "und werden abgewiesen statt mit 0xE5 aufgefuellt",
               e != UFT_OK && img == NULL, d1);
        if (img) uft_disk_free(img);
    }

    /* ── 10. Die Sonde kann nicht zustimmen, und sagt das ─────────── */
    {
        int conf = -1;
        bool ja = p->probe(foo, 4096, noo, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Die Sonde stimmt NIE zu (Konfidenz 0): die Datei hat keinen "
               "Kopf, und in libdsks eigener Tafel teilt jede der acht "
               "nicht-ALT-Geometrien ihre Groesse mit einer ALT-Geometrie",
               !ja && conf == 0, d1);
    }

    /* ── 11. Und `open` sagt ab statt zu raten ────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        memset(&disk, 0, sizeof(disk));
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_OO);
        e = p->open(&disk, pfad, true);
        snprintf(d1, sizeof(d1), "open = %d (erwartet %d)", (int)e,
                 (int)UFT_ERROR_NOT_SUPPORTED);
        pruefe("open() sagt ab (NOT_SUPPORTED), weil die "
               "Plugin-Schnittstelle keinen Kanal fuer eine "
               "benutzergesetzte Geometrie hat (P3-337) — raten waere "
               "erfundene Daten mit richtiger Dateigroesse",
               e == UFT_ERROR_NOT_SUPPORTED, d1);
    }

    /* ── 12. Rundlauf: schreiben ohne Kopf ───────────────────────── */
    {
        const char *tmp = getenv("TEMP");
        char ziel[700];
        uft_disk_image_t *img = NULL;
        uft_error_t e;
        int ok = 0;
        size_t nrt = 0;
        uint8_t *rt = NULL;

        if (!tmp) tmp = ".";
        snprintf(ziel, sizeof(ziel), "%s/uft_logi_rt.logical", tmp);
        e = uft_logical_read_mem(foo, noo, &G_ACORN640, &img, NULL);
        if (e == UFT_OK && img) {
            e = uft_logical_write(img, &G_ACORN640, ziel);
            if (e == UFT_OK) {
                rt = lies(ziel, &nrt);
                ok = (rt && nrt == noo && memcmp(rt, foo, noo) == 0);
            }
            uft_disk_free(img);
        }
        snprintf(d1, sizeof(d1), "write=%d, %zu Byte zurueck (Soll %zu)",
                 (int)e, nrt, noo);
        pruefe("Rundlauf: gelesen und wieder geschrieben ergibt die Datei "
               "BYTEIDENTISCH — ohne Kopf und in derselben Anordnung "
               "(vorher schrieb er einen erfundenen 32-Byte-Kopf)", ok, d1);
        free(rt);
        remove(ziel);
    }

    free(foo); free(fob);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
