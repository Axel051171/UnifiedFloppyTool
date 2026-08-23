/**
 * @file test_disk_write_fuzz.c
 * @brief Der Schreibpfad, auf einer Kopie, mit unsinnigen Koordinaten (MF-522)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `tests/test_disk_open_fuzz.c` (MF-512) deckt `probe`, `open` und
 * `read_track` ab und hat dort sieben Speicherfehler gefunden. `write_track`
 * hat es nie angefasst — und beim Beheben von MF-520 stellte sich heraus,
 * dass dieselbe fehlende Schranke auch dort stand:
 *
 *     d71_read_track :  falsche Sektorzahl -> falsche DATEN gelesen
 *     d71_write_track:  falsche Sektorzahl -> SCHREIBEN an Stellen, die
 *                       zu einer anderen Spur gehoeren
 *
 * In einem Werkzeug, dessen erster Grundsatz "Keine stille Veraenderung"
 * lautet, ist der zweite Fall der schlimmere. Er wurde durch Lesen
 * gefunden, nicht durch Messen — dieser Test schliesst die Luecke.
 *
 * ── Wie geprueft wird ────────────────────────────────────────────────────
 *
 * Immer auf einer KOPIE. Das Korpus unter tests/corpus_free/ ist
 * Referenzmaterial; kein Test veraendert es.
 *
 *      Korpusdatei -> Kopie
 *      -> uft_disk_open(kopie, read_only=false)
 *      -> read_track(0,0)              eine ECHTE Spur besorgen
 *      -> write_track(cyl, head, ...)  an gueltige UND unsinnige Stellen
 *      -> uft_disk_close()
 *
 * Geschrieben wird eine Spur, die das Plugin selbst geliefert hat. Damit
 * prueft der Test die KOORDINATEN, nicht den Inhalt — ein Plugin, das
 * seine eigene Spur an ihre eigene Stelle nicht zurueckschreiben kann,
 * hat ein anderes Problem, und das faellt hier ebenfalls auf.
 *
 * ── Was als Fehler gilt ──────────────────────────────────────────────────
 *
 * Ein Absturz oder ein Speicherfehler unter ASan/UBSan. **Nicht** als
 * Fehler gilt ein Rueckgabewert != UFT_OK: eine unsinnige Koordinate
 * abzulehnen ist richtig.
 *
 * Und ausdruecklich geprueft: nach einem ABGELEHNTEN Schreiben muss die
 * Datei **unveraendert** sein. Ein Plugin, das erst schreibt und dann
 * einen Fehler meldet, hat still veraendert — genau das, was
 * DESIGN_PRINCIPLES verbietet.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

static const char *tmp_path = "uft_write_fuzz_tmp.img";

static const char *const CORPUS[] = {
    "vice_c1541_35trk.d64", "vice_c1541_70trk.d71", "vice_c1541_80trk.d81",
    "vice_c1541_2040.d67",  "vice_c1541_8050.d80",  "vice_c1541_8250.d82",
    "atrcopy_dos2sd.atr",   "atrcopy_dos2sd.xfd",   "xdftool_dd_ofs.adf",
    NULL
};

/* Gueltige und unsinnige Koordinaten. Die unsinnigen gehoeren dazu: ein
 * Plugin darf nicht darauf bauen, dass der Aufrufer sich an die Geometrie
 * haelt, die es selbst gemeldet hat. */
static const int CYL[]  = { 0, 1, 17, 34, 35, 39, 79, 80, 1000, -1 };
static const int HEAD[] = { 0, 1, 2, -1 };

static long n_write_ok, n_write_rejected, n_silent_change, n_out_of_bounds;
static int failures;

#define MAX_IMG (4 * 1024 * 1024)
static uint8_t buf_a[MAX_IMG];
static uint8_t buf_b[MAX_IMG];

static size_t slurp(const char *path, uint8_t *dst)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, MAX_IMG, f);
    fclose(f);
    return n;
}

static bool copy_file(const char *src, const char *dst)
{
    size_t n = slurp(src, buf_a);
    if (!n) return false;
    FILE *f = fopen(dst, "wb");
    if (!f) return false;
    size_t w = fwrite(buf_a, 1, n, f);
    fclose(f);
    return w == n;
}

static void run_one(const char *name)
{
    char src[1024];
    snprintf(src, sizeof(src), "%s/%s", UFT_CORPUS_DIR, name);
    if (!copy_file(src, tmp_path)) {
        printf("  %-24s (fehlt oder nicht kopierbar)\n", name);
        return;
    }
    size_t base_len = slurp(tmp_path, buf_a);

    uft_disk_t *disk = uft_disk_open(tmp_path, false /* schreibbar */);
    if (!disk) {
        printf("  %-24s nicht schreibbar geoeffnet — uebersprungen\n", name);
        return;
    }
    const uft_format_plugin_t *p = disk->plugin;
    if (!p || !p->write_track || !p->read_track) {
        printf("  %-24s Plugin %s kann nicht schreiben — uebersprungen\n",
               name, p && p->name ? p->name : "?");
        uft_disk_close(disk);
        return;
    }

    /* Eine ECHTE Spur besorgen: die des Plugins selbst. */
    uft_track_t trk;
    memset(&trk, 0, sizeof(trk));
    if (p->read_track(disk, 0, 0, &trk) != UFT_OK) {
        printf("  %-24s read_track(0,0) schlug fehl — uebersprungen\n", name);
        uft_disk_close(disk);
        return;
    }

    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    (void)uft_disk_get_geometry(disk, &geo);

    printf("  %-24s Plugin %-10s (%u x %u)", name, p->name ? p->name : "?",
           (unsigned)geo.cylinders, (unsigned)geo.heads);
    for (size_t c = 0; c < sizeof(CYL) / sizeof(CYL[0]); c++) {
        for (size_t h = 0; h < sizeof(HEAD) / sizeof(HEAD[0]); h++) {
            size_t before = slurp(tmp_path, buf_b);

            uft_error_t e = p->write_track(disk, CYL[c], HEAD[h], &trk);
            if (p->flush) (void)p->flush(disk);

            if (e == UFT_OK) {
                n_write_ok++;
                /* Angenommen — aber lag die Koordinate ueberhaupt auf der
                 * Diskette, die das Plugin selbst gemeldet hat?
                 *
                 * Ein Schreiben ausserhalb der eigenen Geometrie ist keine
                 * Kleinigkeit: es landet irgendwo in der Datei, und der
                 * Aufrufer bekommt UFT_OK. Das ist eine stille
                 * Veraenderung mit Erfolgsmeldung. */
                if (CYL[c] >= (int)geo.cylinders || HEAD[h] >= (int)geo.heads) {
                    printf("\n  FAIL  %s (%s): write_track(%d,%d) ANGENOMMEN, "
                           "die Geometrie meldet aber nur %u x %u\n",
                           name, p->name ? p->name : "?", CYL[c], HEAD[h],
                           (unsigned)geo.cylinders, (unsigned)geo.heads);
                    n_out_of_bounds++;
                    failures++;
                }
                continue;
            }
            n_write_rejected++;

            /* Abgelehnt — dann darf sich nichts geaendert haben. */
            size_t after = slurp(tmp_path, buf_a);
            if (after != before || (before && memcmp(buf_a, buf_b, before) != 0)) {
                printf("\n  FAIL  %s: write_track(%d,%d) meldete Fehler %d, "
                       "hat die Datei aber VERAENDERT\n",
                       name, CYL[c], HEAD[h], (int)e);
                n_silent_change++;
                failures++;
                /* Ausgangszustand wiederherstellen, damit der naechste
                 * Durchgang nicht auf Truemmern misst. */
                FILE *f = fopen(tmp_path, "wb");
                if (f) { fwrite(buf_b, 1, before, f); fclose(f); }
            }
        }
    }
    printf("  fertig\n");

    /* MF-525: die gelesene Spur gehoert freigegeben — sonst leckt
     * jeder Durchgang das sectors-Feld samt Sektordaten. */
    uft_track_cleanup(&trk);
    uft_disk_close(disk);
    (void)base_len;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("=== Schreibpfad auf einer Kopie, mit unsinnigen Koordinaten "
           "(MF-522) ===\n");
    for (int i = 0; CORPUS[i]; i++)
        run_one(CORPUS[i]);

    remove(tmp_path);

    printf("\nwrite_track: %ld angenommen, %ld abgelehnt\n",
           n_write_ok, n_write_rejected);
    printf("stille Veraenderung nach Ablehnung: %ld\n", n_silent_change);

    /* NICHT GEPRUEFT: ob ein ANGENOMMENES Schreiben das Richtige tut.
     * Das verlangt einen Rundlauf gegen eine benannte Referenz und
     * gehoert in die Format-Hebung (VERIFICATION_PLAN.md), nicht hierher.
     * Dieser Test prueft, dass der Schreibpfad nicht abstuerzt und nach
     * einer Ablehnung nichts veraendert hat. */

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
