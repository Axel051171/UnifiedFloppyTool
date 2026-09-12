/**
 * @file test_posix_gegen_libdsk.c
 * @brief POSIX: drei Spielarten, und die `.geom` ist UFT-eigen (MF-1034)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────
 *
 * libdsk **1.5.12** (John Elliott, **LGPL-2+**; Quelltext **nur
 * gelesen**, Kanal *Spec* nach MF-695, `dsktrans` **ausgefuehrt**):
 *
 *   `lib/drvposix.c` Z. 23-24  „the most basic of the drivers … a flat
 *                              file with the tracks laid out in the
 *                              SIDES_ALT order"
 *                    Z. 38-98  **drei** Treiberklassen:
 *                              `raw`/`rawalt`, `rawoo`, `rawob`
 *                    Z. 232-254 `posix_offset()` — und der Kommentar
 *                              grenzt gegen `logical` ab: *„Work out
 *                              the offset based on the sidedness of the
 *                              disk image (not the sidedness of the
 *                              geometry)"*
 *   `lib/dsklphys.c` Z. 91-118 `dg_pt2lt()` — die vier Gesetze, in
 *                              MF-1032 abgenommen
 *
 * ── Was der Vorzustand war ──────────────────────────────────────────
 *
 * **Q1** Die Attribution trug nicht: der Dateikopf nannte libdsks
 * `drvposix.c` als geprueft, und die `.geom`-Nachbardatei kommt im
 * **ganzen** libdsk-Baum nicht vor. Sie ist UFTs eigene Konvention.
 *
 * **Q2** Von den drei Spielarten war nur `raw` umgesetzt — der Leser
 * legte die Spuren immer linear ab. Bei `acorn640` (80 x 2, OUTOUT)
 * liegen damit **158 von 160** Spuren an anderer Stelle.
 *
 * **Q3** Ohne `.geom` wurde eine Geometrie **erfunden**: Rueckfall
 * 80 x 2 x 9 x 512 und `cylinders = (groesse / 4608) / 2`. Eine
 * 174 848 Byte grosse D64 wurde damit 18 x 2 x 9 x 512 — und 8960 Byte
 * fielen weg, still.
 *
 * **Q4** Der Schreiber konnte nur `alt` und nannte die Anordnung nicht.
 *
 * ── T1b: libdsk hat die Abbilder GESCHRIEBEN ────────────────────────
 *
 *     dsktrans -itype raw -format acorn640 -otype rawoo
 *         -> 655360 Byte, SIDES_OUTOUT
 *     dsktrans -itype raw -format ibm720  -otype rawob
 *         -> 737280 Byte, SIDES_OUTBACK, dg_secbase 1
 *
 * Und **gemessen** dazu: beide sind byteidentisch mit den
 * `logical`-Pruefdateien aus MF-1032. Das ist kein Zufall, sondern
 * genau die Abgrenzung, die libdsk selbst benennt — dort kommt die
 * Sidedness aus der Geometrie, hier aus dem Dateityp; wenn beide
 * dasselbe sagen, sind die Dateien gleich. Der Test haelt das fest.
 *
 * Die `.geom`-Nachbardateien daneben sind **UFT-eigen** und als solche
 * im Manifest geführt.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/uft_posix.h"

extern const uft_format_plugin_t uft_format_plugin_posix;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define F_OO  "libdsk_acorn640.rawoo"
#define F_OB  "libdsk_ibm720.rawob"
#define F_LOG "libdsk_acorn640_outout.logical"

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

static int schreib_text(const char *pfad, const char *text)
{
    FILE *f = fopen(pfad, "w");
    if (!f) return 0;
    fputs(text, f);
    fclose(f);
    return 1;
}

static int kopiere(const char *von, const char *nach)
{
    size_t n = 0;
    uint8_t *b = lies(von, &n);
    FILE *f;
    if (!b) return 0;
    f = fopen(nach, "wb");
    if (!f) { free(b); return 0; }
    if (fwrite(b, 1, n, f) != n) { fclose(f); free(b); return 0; }
    fclose(f);
    free(b);
    return 1;
}

/** Alle Sektoren gegen ihren eigenen Namen halten. */
static void pruefe_alle(const char *bild, const posix_geometry_t *g,
                        const char *wie)
{
    uft_disk_image_t *img = NULL;
    posix_read_result_t r;
    posix_read_options_t o;
    char erw[32], d1[300], gefunden[32];
    int falsch = 0, gesamt = 0, fc = -1, fh = -1, fs = -1;
    uft_error_t e;
    uint16_t c, h;
    size_t s;

    memset(gefunden, 0, sizeof(gefunden));
    uft_posix_read_options_init(&o);
    e = uft_posix_read(bild, &img, &o, &r);
    if (e != UFT_OK || !img) {
        snprintf(d1, sizeof(d1), "uft_posix_read = %d (%s)", (int)e,
                 r.error_detail ? r.error_detail : "-");
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
                            snprintf(gefunden, sizeof(gefunden),
                                     "(keine Daten)");
                        }
                    }
                }
            }
        }
    }
    uft_disk_free(img);
    snprintf(d1, sizeof(d1), "%d Sektoren, %d falsch; erster Fehler "
             "C%d H%d S%d -> \"%s\"; `.geom` gefunden: %d",
             gesamt, falsch, fc, fh, fs, gefunden, (int)r.geom_found);
    pruefe(wie, gesamt == (int)g->cylinders * g->heads * g->sectors
           && falsch == 0 && r.geom_found, d1);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_posix;
    const char *tmp = getenv("TEMP");
    char qoo[600], qob[600], qlog[600], d1[300];
    char boo[700], bob[700], goo[700], gob[700];
    posix_geometry_t g_oo, g_ob;

    printf("POSIX gegen libdsk drvposix.c (LGPL-2+, nur gelesen)\n");
    printf("====================================================\n");

    if (!tmp) tmp = ".";
    snprintf(qoo, sizeof(qoo), "%s/%s", UFT_CORPUS_DIR, F_OO);
    snprintf(qob, sizeof(qob), "%s/%s", UFT_CORPUS_DIR, F_OB);
    snprintf(qlog, sizeof(qlog), "%s/%s", UFT_CORPUS_DIR, F_LOG);

    /* Die Abbilder in den Temp-Bereich legen, damit die `.geom` daneben
     * geschrieben werden kann — der Korpus bleibt unangetastet. */
    snprintf(boo, sizeof(boo), "%s/uft_posix_oo.raw", tmp);
    snprintf(bob, sizeof(bob), "%s/uft_posix_ob.raw", tmp);
    snprintf(goo, sizeof(goo), "%s.geom", boo);
    snprintf(gob, sizeof(gob), "%s.geom", bob);

    if (!kopiere(qoo, boo) || !kopiere(qob, bob)) {
        printf("  [SKIP] Pruefdateien fehlen im Korpus (%s / %s)\n",
               F_OO, F_OB);
        printf("\n%d gruen, %d rot, 1 uebersprungen\n", gruen, rot);
        return 0;
    }

    /* `acorn640`: OUTOUT, 80 x 2 x 16 x 256, dg_secbase 0 */
    memset(&g_oo, 0, sizeof(g_oo));
    g_oo.cylinders = 80; g_oo.heads = 2; g_oo.sectors = 16;
    g_oo.sector_size = 256; g_oo.first_sector = 0;
    g_oo.encoding = UFT_ENC_MFM; g_oo.sides = UFT_LOGI_SIDES_OUTOUT;
    /* `ibm720`: OUTBACK, 80 x 2 x 9 x 512, dg_secbase 1 */
    memset(&g_ob, 0, sizeof(g_ob));
    g_ob.cylinders = 80; g_ob.heads = 2; g_ob.sectors = 9;
    g_ob.sector_size = 512; g_ob.first_sector = 1;
    g_ob.encoding = UFT_ENC_MFM; g_ob.sides = UFT_LOGI_SIDES_OUTBACK;

    /* ── 1. Die `.geom` traegt die Anordnung, und zwar rund ────────── */
    {
        posix_geometry_t zurueck;
        uft_error_t w = uft_posix_write_geometry(goo, &g_oo);
        uft_error_t r = uft_posix_read_geometry(goo, &zurueck);
        snprintf(d1, sizeof(d1), "write=%d read=%d; %u/%u/%u/%u erster %u "
                 "Anordnung %s", (int)w, (int)r, zurueck.cylinders,
                 zurueck.heads, zurueck.sectors, zurueck.sector_size,
                 zurueck.first_sector, uft_posix_sides_name(zurueck.sides));
        pruefe("die `.geom` traegt die Anordnung und kommt unveraendert "
               "zurueck (UFT-eigene Konvention, MF-1034)",
               w == UFT_OK && r == UFT_OK
               && zurueck.cylinders == 80 && zurueck.heads == 2
               && zurueck.sectors == 16 && zurueck.sector_size == 256
               && zurueck.first_sector == 0
               && zurueck.sides == UFT_LOGI_SIDES_OUTOUT, d1);
    }

    /* ── 2. Eine alte `.geom` mit VIER Werten gilt weiter als `alt` ── */
    {
        posix_geometry_t alt;
        uft_error_t r;
        schreib_text(goo, "80 2 9 512\n");
        r = uft_posix_read_geometry(goo, &alt);
        snprintf(d1, sizeof(d1), "read=%d, Anordnung %s, erster %u",
                 (int)r, uft_posix_sides_name(alt.sides), alt.first_sector);
        pruefe("eine `.geom` ohne Anordnungsfeld gilt unveraendert als "
               "`alt` — keine bisher geschriebene Datei wird ungueltig",
               r == UFT_OK && alt.sides == UFT_LOGI_SIDES_ALT
               && alt.first_sector == 1, d1);
    }

    /* ── 3. Ein unbekannter Anordnungsname wird ABGEWIESEN ────────── */
    {
        posix_geometry_t x;
        uft_error_t r;
        schreib_text(goo, "80 2 16 256 0 outoot\n");
        r = uft_posix_read_geometry(goo, &x);
        snprintf(d1, sizeof(d1), "read=%d", (int)r);
        pruefe("Gegenprobe: ein Tippfehler im Anordnungsnamen wird "
               "abgewiesen statt auf `alt` zurechtgebogen — sonst waere "
               "er eine stille Falschlesung", r != UFT_OK, d1);
    }

    /* ── 4. OUTOUT: alle 2560 Sektoren ────────────────────────────── */
    uft_posix_write_geometry(goo, &g_oo);
    pruefe_alle(boo, &g_oo,
                "rawoo (acorn640): alle 2560 Sektoren benennen sich selbst "
                "richtig, IDs ab 0 (dg_secbase = 0)");

    /* ── 5. OUTBACK: alle 1440 Sektoren, dg_secbase 1 ─────────────── */
    uft_posix_write_geometry(gob, &g_ob);
    pruefe_alle(bob, &g_ob,
                "rawob (ibm720): alle 1440 Sektoren benennen sich selbst "
                "richtig, IDs ab 1 (dg_secbase = 1)");

    /* ── 6. Gegenprobe zu Q2: dieselbe Datei als `alt` gelesen ─────
     *
     * Vorher rechnete der Leser IMMER `alt`. Wenn `alt` hier
     * durchgehen wuerde, waere die Anordnung gleichgueltig — und genau
     * das war der Befund. */
    {
        posix_geometry_t falsch = g_oo;
        uft_disk_image_t *img = NULL;
        posix_read_options_t o;
        char n[24];
        int ok = 0;
        uft_error_t e;
        memset(n, 0, sizeof(n));
        falsch.sides = UFT_LOGI_SIDES_ALT;
        uft_posix_write_geometry(goo, &falsch);
        uft_posix_read_options_init(&o);
        e = uft_posix_read(boo, &img, &o, NULL);
        if (e == UFT_OK && img) {
            uft_track_t *t = img->track_data[(size_t)40 * 2 + 0];
            if (t && t->sector_count && t->sectors[0].data) {
                memcpy(n, t->sectors[0].data, 17);
                ok = (memcmp(n, "UFT-K C00 H1 S00 ", 17) == 0);
            }
            uft_disk_free(img);
        }
        snprintf(d1, sizeof(d1), "read=%d, C40 H0 liefert \"%s\" "
                 "(erwartet den Sektor von C00 H1)", (int)e, n);
        pruefe("Gegenprobe zu Q2: mit `.geom`-Anordnung `alt` gelesen "
               "liefert Zylinder 40 Kopf 0 den Sektor von Zylinder 0 "
               "Kopf 1 — die Anordnung ist also wirklich wirksam", ok, d1);
        uft_posix_write_geometry(goo, &g_oo);
    }

    /* ── 7. Q3: ohne `.geom` wird abgesagt, nicht geraten ─────────── */
    {
        uft_disk_image_t *img = NULL;
        posix_read_result_t r;
        posix_read_options_t o;
        uft_error_t e;
        remove(goo);
        uft_posix_read_options_init(&o);
        e = uft_posix_read(boo, &img, &o, &r);
        snprintf(d1, sizeof(d1), "read=%d (%s), require_geom=%d", (int)e,
                 r.error_detail ? r.error_detail : "-", (int)o.require_geom);
        pruefe("Q3: ohne `.geom` sagt der Leser AB — die Vorgabe ist "
               "`require_geom = true`, vorher wurde 80x2x9x512 "
               "angenommen und die Zylinderzahl aus der Groesse gerechnet",
               e != UFT_OK && img == NULL
               && o.require_geom == true, d1);
        if (img) uft_disk_free(img);
    }

    /* ── 8. Q3b: ein Rueckfall gilt nur, wenn er restlos aufgeht ──── */
    {
        uft_disk_image_t *img = NULL;
        posix_read_options_t o;
        uft_error_t e1, e2;
        uft_logical_sides_t sides_merk;
        uft_error_t e3;
        uft_posix_read_options_init(&o);
        o.require_geom = false;

        /* (a) ZU GROSS: 80x2x9x512 = 737280, die Datei hat 655360 —
         *     das faengt die Schranke `size < brauche`. */
        e1 = uft_posix_read(boo, &img, &o, NULL);
        if (img) { uft_disk_free(img); img = NULL; }

        /* (b) ZU KLEIN: 40x2x16x256 = 327680, also genau die Haelfte.
         *     Hier greift NUR die Restlos-Pruefung — ohne sie wuerde die
         *     halbe Diskette gelesen und der Rest fiele still weg. Das
         *     ist der Vorzustand Q3 in Reinform, denn dort rechnete der
         *     Leser die Zylinderzahl aus der Groesse und verwarf den
         *     Rest. */
        sides_merk = g_oo.sides;
        o.fallback = g_oo;
        o.fallback.cylinders = 40;
        o.fallback.sides = sides_merk;
        e2 = uft_posix_read(boo, &img, &o, NULL);
        if (img) { uft_disk_free(img); img = NULL; }

        /* (c) GENAU: die richtige Geometrie geht auf. */
        o.fallback = g_oo;
        e3 = uft_posix_read(boo, &img, &o, NULL);
        snprintf(d1, sizeof(d1), "zu gross -> %d, zu klein (halbe "
                 "Diskette) -> %d, genau -> %d", (int)e1, (int)e2, (int)e3);
        pruefe("Q3b: ein ausdruecklicher Rueckfall wird nur angenommen, "
               "wenn er die Dateigroesse RESTLOS erklaert — zu gross UND "
               "zu klein fallen, und die halbe Diskette ist der "
               "Vorzustand Q3 in Reinform",
               e1 != UFT_OK && e2 != UFT_OK && e3 == UFT_OK, d1);
        if (img) uft_disk_free(img);
    }

    /* ── 9. Die Sonde und der Weg ueber `open()` ──────────────────── */
    {
        int conf = -1;
        bool inhalt, pfadweg;
        uft_disk_t disk;
        uft_error_t e;
        uint8_t kopf[64];
        memset(kopf, 0, sizeof(kopf));
        inhalt = p->probe(kopf, sizeof(kopf), 655360u, &conf);
        uft_posix_write_geometry(goo, &g_oo);
        pfadweg = uft_posix_probe(boo, NULL);
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, boo, true);
        snprintf(d1, sizeof(d1), "Inhaltssonde=%d (Konfidenz %d), "
                 "Pfadsonde=%d, open=%d, %ux%ux%ux%u", (int)inhalt, conf,
                 (int)pfadweg, (int)e, disk.geometry.cylinders,
                 disk.geometry.heads, disk.geometry.sectors,
                 disk.geometry.sector_size);
        pruefe("die Inhaltssonde stimmt nie zu (MF-546), die Pfadsonde "
               "schon — und `open()` sieht den Pfad, ist also erreichbar "
               "(anders als `logical`, P3-337)",
               !inhalt && conf == 0 && pfadweg && e == UFT_OK
               && disk.geometry.cylinders == 80 && disk.geometry.heads == 2
               && disk.geometry.sectors == 16
               && disk.geometry.sector_size == 256, d1);
        if (e == UFT_OK) p->close(&disk);
    }

    /* ── 10. Rundlauf: lesen, schreiben, byteidentisch ────────────── */
    {
        uft_disk_image_t *img = NULL;
        posix_read_options_t o;
        char ziel[700], zgeom[700];
        uft_error_t e;
        int ok = 0;
        size_t na = 0, nb = 0;
        uint8_t *a = NULL, *b = NULL;
        posix_geometry_t zurueck;

        snprintf(ziel, sizeof(ziel), "%s/uft_posix_rt.raw", tmp);
        snprintf(zgeom, sizeof(zgeom), "%s.geom", ziel);
        uft_posix_write_geometry(goo, &g_oo);
        uft_posix_read_options_init(&o);
        e = uft_posix_read(boo, &img, &o, NULL);
        if (e == UFT_OK && img) {
            e = uft_posix_write(img, &g_oo, ziel);
            if (e == UFT_OK) {
                a = lies(boo, &na);
                b = lies(ziel, &nb);
                ok = (a && b && na == nb && memcmp(a, b, na) == 0
                      && uft_posix_read_geometry(zgeom, &zurueck) == UFT_OK
                      && zurueck.sides == UFT_LOGI_SIDES_OUTOUT);
            }
            uft_disk_free(img);
        }
        snprintf(d1, sizeof(d1), "write=%d, %zu gegen %zu Byte", (int)e,
                 na, nb);
        pruefe("Rundlauf: gelesen und wieder geschrieben ergibt das Abbild "
               "BYTEIDENTISCH, und die neue `.geom` nennt `outout` "
               "(vorher konnte der Schreiber nur `alt`)", ok, d1);
        free(a); free(b);
        remove(ziel); remove(zgeom);
    }

    /* ── 11. Zwei libdsk-Treiber, dieselben Bytes ────────────────── */
    {
        size_t na = 0, nb = 0;
        uint8_t *a = lies(qoo, &na);
        uint8_t *b = lies(qlog, &nb);
        int ok = (a && b && na == nb && memcmp(a, b, na) == 0);
        snprintf(d1, sizeof(d1), "%s %zu Byte, %s %zu Byte", F_OO, na,
                 F_LOG, nb);
        pruefe("`rawoo` und `logical` mit derselben Geometrie ergeben "
               "DIESELBEN Bytes — libdsk grenzt beide nur darin ab, WOHER "
               "die Sidedness kommt (Treiber gegen Geometrie)", ok, d1);
        free(a); free(b);
    }

    remove(goo); remove(gob); remove(boo); remove(bob);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
