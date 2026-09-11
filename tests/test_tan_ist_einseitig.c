/**
 * @file test_tan_ist_einseitig.c
 * @brief TAN hat eine Seite — auch bei 80 Spuren (MF-1026)
 *
 * ── Das Orakel ──────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/trs80_dsk.cpp`, **BSD-3-Clause**, Dirk Best.
 * `jv1_format::formats[]` (Z. 98-113) fuehrt drei Eintraege:
 *
 *     4000, 10, 35, 1, 256, {}, 0, {}, 14, 11, 12
 *     4000, 10, 40, 1, 256, {}, 0, {}, 14, 11, 12
 *     4000, 10, 80, 1, 256, {}, 0, {}, 14, 11, 12
 *
 * Nach MAMEs `wd177x_format::format` sind die Spalten
 * `cell_size, sector_count, track_count, head_count, sector_base_size,
 *  per_sector_size, sector_base_id, per_sector_id, gap_1, gap_2, gap_3`.
 * Also: **10 Sektoren, 35/40/80 Spuren, 1 Kopf, 256 Byte,
 * sector_base_id 0**.
 *
 * Zweite, unabhaengige Hand: Tim Mann, „Common File Formats for
 * Emulated TRS-80 Floppy Disks" (https://www.tim-mann.org/trs80/
 * dskspec.html) — „numbered 0 through 9, and only one side".
 *
 * ── Warum dieser Test existiert ─────────────────────────────────────
 *
 * MF-1016 hat genau diese zwei Fehler an `src/formats/jv1/uft_jv1.c`
 * behoben. `src/formats/tan/uft_tan.c` liest dieselbe Anordnung und
 * hatte sie **beide noch**:
 *
 *   1. `if (tracks <= 40) heads = 1; else { cyl = tracks/2; heads = 2; }`
 *      — bei 204 800 Byte kamen **40 Zylinder / 2 Koepfe** heraus, und
 *      die Spuren 40..79 lagen auf einer Seite, die es nicht gibt.
 *   2. Sektornummern **1..10** statt 0..9.
 *
 * Das ist die Gestalt von MF-519/MF-529 (dort behoben, hier nicht),
 * nur zwischen zwei DATEIEN statt zwei Funktionen.
 *
 * Damit ein falscher Versatz nicht durchgeht, ist jeder Sektor
 * **selbstbeschreibend**: Byte 0 nennt die Spur, Byte 1 den Sektor.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_tan;

#define TAN_SPT 10
#define TAN_SS  256

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

/* Eine TAN-Datei mit `spuren` Spuren, jeder Sektor nennt sich selbst. */
static uint8_t *baue(int spuren, size_t *groesse)
{
    size_t n = (size_t)spuren * TAN_SPT * TAN_SS;
    uint8_t *b = (uint8_t *)malloc(n);
    int t, s;
    assert(b != NULL);
    for (t = 0; t < spuren; t++) {
        for (s = 0; s < TAN_SPT; s++) {
            uint8_t *z = b + ((size_t)t * TAN_SPT + (size_t)s) * TAN_SS;
            memset(z, 0xA5, TAN_SS);
            z[0] = (uint8_t)t;
            z[1] = (uint8_t)s;
        }
    }
    *groesse = n;
    return b;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_tan;
    const char *tmp = getenv("TEMP");
    char pfad[512];
    uint8_t *b;
    size_t n;
    int t, s;

    printf("TAN gegen MAME trs80_dsk.cpp (jv1-Tafel, BSD-3-Clause)\n");
    printf("=======================================================\n");

    /* ── 80 Spuren: MAMEs dritter Eintrag ───────────────────────────── */
    b = baue(80, &n);
    {
        char d[100];
        snprintf(d, sizeof(d), "%zu Byte", n);
        pruefe("80 x 10 x 256 = 204800 Byte (MAMEs dritter Eintrag)",
               n == 204800u, d);
    }
    snprintf(pfad, sizeof(pfad), "%s/uft_tan80.dsk", tmp ? tmp : ".");
    assert(schreibe(pfad, b, n));

    {
        int conf = -1;
        bool ja = p->probe(b, 4096, n, &conf);
        char d[100];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        /* MF-729: 30..49 heisst „nur die Groesse" — fuer einen
         * kopflosen Abzug die richtige Stufe. */
        pruefe("Sonde nimmt an, Konfidenz im Band 30..49 (nur die Groesse)",
               ja && conf >= 30 && conf < 50, d);
    }

    {
        uft_disk_t disk;
        uft_error_t rc;
        memset(&disk, 0, sizeof(disk));
        rc = p->open(&disk, pfad, true);
        {
            char d[160];
            snprintf(d, sizeof(d), "rc=%d, %u Zyl, %u Koepfe, %u Sektoren",
                     (int)rc, disk.geometry.cylinders, disk.geometry.heads,
                     disk.geometry.total_sectors);
            pruefe("open: 80 Zylinder, EIN Kopf, 800 Sektoren "
                   "(nicht 40 x 2)",
                   rc == UFT_OK && disk.geometry.cylinders == 80
                   && disk.geometry.heads == 1
                   && disk.geometry.total_sectors == 800u, d);
        }
        if (rc != UFT_OK) {
            printf("\n%d gruen, %d rot\n", gruen, rot);
            free(b);
            return 1;
        }

        /* ── jede Spur, jeder Sektor: ID 0-basiert, Inhalt eigen ────── */
        {
            int falsche_id = 0, fremd = 0, unlesbar = 0;
            char erstes[200] = "";
            for (t = 0; t < 80; t++) {
                uft_track_t tr;
                memset(&tr, 0, sizeof(tr));
                if (p->read_track(&disk, t, 0, &tr) != UFT_OK
                    || (int)tr.sector_count != TAN_SPT) {
                    unlesbar++;
                    free(tr.sectors);
                    free(tr.raw_data);
                    continue;
                }
                for (s = 0; s < TAN_SPT; s++) {
                    const uint8_t *dd = tr.sectors[s].data;
                    if (tr.sectors[s].id.sector != (uint8_t)s) {
                        falsche_id++;
                        if (!erstes[0])
                            snprintf(erstes, sizeof(erstes),
                                     "Spur %d Sektor %d hat ID %u "
                                     "(MAME: sector_base_id = 0)", t, s,
                                     (unsigned)tr.sectors[s].id.sector);
                    }
                    if (!dd || dd[0] != (uint8_t)t || dd[1] != (uint8_t)s) {
                        fremd++;
                        if (!erstes[0] && dd)
                            snprintf(erstes, sizeof(erstes),
                                     "Spur %d Sektor %d liefert die Bytes "
                                     "von Spur %u Sektor %u", t, s,
                                     (unsigned)dd[0], (unsigned)dd[1]);
                    }
                }
                free(tr.sectors);
                free(tr.raw_data);
            }
            {
                char d[260];
                snprintf(d, sizeof(d), "%d unlesbar, %d falsche IDs, "
                         "%d fremde Sektoren; erster: %s", unlesbar,
                         falsche_id, fremd, erstes[0] ? erstes : "-");
                pruefe("alle 80 Spuren lesbar, IDs 0..9, jeder Sektor "
                       "liefert seine EIGENEN Bytes",
                       unlesbar == 0 && falsche_id == 0 && fremd == 0, d);
            }
        }

        /* ── Kopf 1 gibt es nicht, und das ist kein Erfolg ──────────── */
        {
            uft_track_t tr;
            uft_error_t rc2;
            char d[140];
            memset(&tr, 0, sizeof(tr));
            rc2 = p->read_track(&disk, 0, 1, &tr);
            snprintf(d, sizeof(d), "rc=%d, %u Sektoren", (int)rc2,
                     (unsigned)tr.sector_count);
            pruefe("Kopf 1 wird ABGEWIESEN — TAN hat nur eine Seite",
                   rc2 != UFT_OK, d);
            free(tr.sectors);
            free(tr.raw_data);
        }

        /* ── Spur 80 gibt es nicht ──────────────────────────────────── */
        {
            uft_track_t tr;
            uft_error_t rc2;
            char d[140];
            memset(&tr, 0, sizeof(tr));
            rc2 = p->read_track(&disk, 80, 0, &tr);
            snprintf(d, sizeof(d), "rc=%d, %u Sektoren", (int)rc2,
                     (unsigned)tr.sector_count);
            pruefe("Spur 80 wird ABGEWIESEN (0..79 ist alles)",
                   rc2 != UFT_OK, d);
            free(tr.sectors);
            free(tr.raw_data);
        }

        p->close(&disk);
    }
    remove(pfad);
    free(b);

    /* ── 35 Spuren: MAMEs erster Eintrag ───────────────────────────── */
    b = baue(35, &n);
    snprintf(pfad, sizeof(pfad), "%s/uft_tan35.dsk", tmp ? tmp : ".");
    assert(schreibe(pfad, b, n));
    {
        uft_disk_t disk;
        uft_error_t rc;
        char d[160];
        memset(&disk, 0, sizeof(disk));
        rc = p->open(&disk, pfad, true);
        snprintf(d, sizeof(d), "%zu Byte -> rc=%d, %u Zyl, %u Koepfe", n,
                 (int)rc, disk.geometry.cylinders, disk.geometry.heads);
        pruefe("35 x 10 x 256 = 89600 Byte -> 35 Zylinder, ein Kopf",
               rc == UFT_OK && n == 89600u && disk.geometry.cylinders == 35
               && disk.geometry.heads == 1, d);
        if (rc == UFT_OK) p->close(&disk);
    }
    remove(pfad);
    free(b);

    /* ── 70 Spuren: die Groesse, die MAME NICHT kennt ──────────────── */
    b = baue(70, &n);
    snprintf(pfad, sizeof(pfad), "%s/uft_tan70.dsk", tmp ? tmp : ".");
    assert(schreibe(pfad, b, n));
    {
        uft_disk_t disk;
        uft_error_t rc;
        char d[200];
        memset(&disk, 0, sizeof(disk));
        rc = p->open(&disk, pfad, true);
        snprintf(d, sizeof(d), "%zu Byte -> rc=%d, %u Zyl, %u Koepfe", n,
                 (int)rc, disk.geometry.cylinders, disk.geometry.heads);
        /* Diese Groesse steht in MAMEs Tafel nicht; UFT nimmt sie an,
         * liest sie aber als 70 EINSEITIGE Spuren — nicht als 35 x 2.
         * Nachsichtig bei der Spurzahl, nicht bei der Seitigkeit. */
        pruefe("179200 Byte -> 70 Zylinder, ein Kopf (MAMEs Tafel kennt "
               "diese Groesse nicht; 35 x 2 waere erfunden)",
               rc == UFT_OK && n == 179200u && disk.geometry.cylinders == 70
               && disk.geometry.heads == 1, d);
        if (rc == UFT_OK) p->close(&disk);
    }
    remove(pfad);
    free(b);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
