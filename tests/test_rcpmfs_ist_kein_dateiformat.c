/**
 * @file test_rcpmfs_ist_kein_dateiformat.c
 * @brief RCPMFS: die benannte Referenz beschreibt etwas anderes (MF-1035)
 *
 * ── Was gemessen wurde ──────────────────────────────────────────────
 *
 * `include/uft/formats/uft_rcpmfs.h` beschrieb bis MF-1035 ein
 * „network-accessible CP/M file system format used by some CP/M
 * emulators and servers" mit der Kennung `"RCPM"`, einem **64-Byte-Kopf**,
 * mehreren Disketten in einem Behaelter, Benutzerbereichen,
 * Dateiattributen und optionaler Kompression — und nannte als Referenz
 * libdsks `drvrcpm.c`.
 *
 * **Diese Referenz beschreibt einen VERZEICHNISTREIBER.** Ihr eigener
 * Kopfkommentar (`lib/drvrcpm.c` Z. 24-27):
 *
 *     „This driver is probably the weirdest I've written so far. It
 *      simulates a CP/M disc using a collection of separate files; the
 *      idea being to make a host directory appear as a CP/M filesystem."
 *
 * libdsks Handbuch (`doc/libdsk.txt` Z. 407-409) sagt dasselbe:
 * *„Reverse CP/M filesystem. A directory is made to appear as a CP/M
 * disk."* Und der Beweis steht in einer Zeile von `rcpmfs_open()`
 * (Z. 1239-1240):
 *
 *     if (stat(passed, &st)) return DSK_ERR_NOTME;
 *     if (!S_ISDIR(st.st_mode)) return DSK_ERR_NOTME;
 *
 * — **nur ein Verzeichnis**, Dateien werden ausdruecklich abgewiesen.
 * Die Einstellungen liegen in einer `.libdsk.ini` **im** Verzeichnis
 * (`CONFIGFILE`, Z. 72), nicht in einem Dateikopf.
 *
 * Fuer das beschriebene Behaelterformat gibt es in diesem Baum **keinen
 * Beleg**. Klasse FMT-2/3/10/11/12 — und der erste Fall in dieser
 * Runde, bei dem nicht die Kennung, sondern das **ganze Format samt
 * Zweck** erfunden war.
 *
 * ── Was dieser Test festhaelt ───────────────────────────────────────
 *
 * Dass die Grenzen absagen — und zwar **auch fuer eine Datei, die genau
 * die erfundene Kennung traegt**. Ohne diese Zusage koennte die alte
 * Sonde jederzeit zurueckkommen: sie hat nie etwas anderes getroffen als
 * Dateien, die UFTs eigener Schreiber erzeugt hat, und **ein
 * geschlossener Kreis fiel diesem Baum schon zweimal auf** — bei
 * `apridisk` (MF-1009) und bei `qrst` (MF-1028) war je ein
 * Rundlauftest gruen, weil Packer und Entpacker Spiegelbilder derselben
 * Erfindung waren.
 *
 * Die Stufe bleibt **T3**, und das ist die richtige Antwort: ein Format,
 * fuer das es keine Referenz gibt, ist nicht „ungeprueft" im Sinne von
 * „noch nicht geprueft", sondern unbelegt. Die beiden Wege daraus stehen
 * als **P3-338**.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_rcpmfs.h"

extern const uft_format_plugin_t uft_format_plugin_rcpmfs;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

/** Ein Puffer, der genau die ERFUNDENE Kennung traegt. */
static void baue_erfundenen_kopf(uint8_t *b, size_t n)
{
    memset(b, 0, n);
    memcpy(b, RCPMFS_MAGIC, RCPMFS_MAGIC_LEN);
    /* Version 1, eine Diskette — so, wie der alte Schreiber es setzte. */
    b[4] = RCPMFS_VERSION;
    b[8] = 1;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_rcpmfs;
    uint8_t kopf[RCPMFS_HEADER_SIZE * 2];
    char d[260];

    printf("RCPMFS: die Referenz beschreibt einen Verzeichnistreiber\n");
    printf("========================================================\n");

    baue_erfundenen_kopf(kopf, sizeof(kopf));

    /* ── 1. Die Kennung steht noch im Baum — und wird nicht mehr
     *        anerkannt ────────────────────────────────────────────── */
    {
        int conf = -1;
        bool ja = uft_rcpmfs_probe(kopf, sizeof(kopf), &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d; die ersten vier "
                 "Bytes sind \"%c%c%c%c\"", (int)ja, conf,
                 kopf[0], kopf[1], kopf[2], kopf[3]);
        pruefe("die Sonde stimmt NICHT zu, obwohl der Puffer genau die "
               "erfundene Kennung \"RCPM\" traegt — vorher meldete sie "
               "Konfidenz 95", !ja && conf == 0, d);
    }

    /* ── 2. Und die Plugin-Sonde ebenso ───────────────────────────── */
    {
        int conf = -1;
        bool ja = p->probe(kopf, sizeof(kopf), sizeof(kopf), &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", (int)ja, conf);
        pruefe("die Plugin-Sonde stimmt NICHT zu (Konfidenz 0)",
               !ja && conf == 0, d);
    }

    /* ── 3. Es gibt keinen Einstieg — `open` ist NULL ──────────────
     *
     * Nicht „open sagt ab", sondern **es gibt kein open**. Das ist die
     * staerkere Aussage, und `test_capability_manifest` (MF-658)
     * verlangt sie: „Read =
     * UNSUPPORTED" darf nicht neben einem vorhandenen `open`/
     * `read_track` stehen. Alle Aufrufstellen im Kern pruefen den
     * Nullzeiger (`uft_core_stubs.c:122`, `uft_disk_convert.c:101`,
     * `uft_smart_open.c:247`).
     *
     * `write_track` bleibt gesetzt und sagt ab — ein Nullzeiger gaebe
     * dem Aufrufer keine Begruendung (Regel aus MF-930). */
    {
        snprintf(d, sizeof(d), "open=%p, read_track=%p, close=%p, "
                 "write_track=%p, capabilities=0x%X",
                 (const void *)p->open, (const void *)p->read_track,
                 (const void *)p->close, (const void *)p->write_track,
                 (unsigned)p->capabilities);
        pruefe("es gibt keinen Einstieg: `open` und `read_track` sind "
               "NULL und `capabilities` ist leer — das Plugin kann nicht "
               "geoeffnet werden, statt es zu versuchen und abzusagen",
               p->open == NULL && p->read_track == NULL
               && p->capabilities == 0 && p->write_track != NULL, d);
    }

    /* ── 4. Der Schreiber erzeugt kein unbelegtes Format ──────────── */
    {
        const char *tmp = getenv("TEMP");
        char pfad[600];
        uft_disk_image_t *img = uft_disk_alloc(2, 1);
        uft_error_t e = UFT_ERR_INVALID_PARAM;

        if (!tmp) tmp = ".";
        snprintf(pfad, sizeof(pfad), "%s/uft_rcpmfs_schreib.rcpmfs", tmp);
        if (img) {
            img->sectors_per_track = 9;
            img->bytes_per_sector = 512;
            e = uft_rcpmfs_write(img, pfad, "TEST", NULL, NULL);
            uft_disk_free(img);
        }
        snprintf(d, sizeof(d), "write=%d (erwartet %d)", (int)e,
                 (int)UFT_ERROR_NOT_SUPPORTED);
        pruefe("`uft_rcpmfs_write()` sagt ab — UFT erzeugt keine Datei in "
               "einem Format, fuer das es keine Referenz gibt (die Lehre "
               "aus MF-1009 und MF-1028)",
               e == UFT_ERROR_NOT_SUPPORTED, d);
        remove(pfad);
    }

    /* ── 5. Die Merkmalstafel sagt es auch ────────────────────────── */
    {
        int i, read_unsupported = 0, write_unsupported = 0;
        const char *grund = NULL;
        for (i = 0; i < (int)p->feature_count; i++) {
            if (strcmp(p->features[i].name, "Read") == 0) {
                read_unsupported =
                    (p->features[i].status == UFT_FEATURE_UNSUPPORTED);
                grund = p->features[i].note;
            }
            if (strcmp(p->features[i].name, "Write") == 0) {
                write_unsupported =
                    (p->features[i].status == UFT_FEATURE_UNSUPPORTED);
            }
        }
        snprintf(d, sizeof(d), "Read unsupported=%d, Write unsupported=%d, "
                 "Grund vorhanden=%d", read_unsupported, write_unsupported,
                 (int)(grund != NULL));
        pruefe("die Merkmalstafel meldet Read UND Write als nicht "
               "unterstuetzt, mit Grund — „Read: SUPPORTED\" bei einem "
               "`open()`, das immer absagt, waere die Zusage ohne Tat aus "
               "MF-883/MF-961",
               read_unsupported && write_unsupported && grund != NULL, d);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
