/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MF-1484 (P3-656) — Ein A2R2 mit Disk Type 2 traegt Kopf UND Zylinder
 * in der Location, und UFT hat die Seite verworfen.
 *
 * ── Die Referenz, jetzt gelesen ──────────────────────────────────────
 *
 * Die „A2R 2.x Reference" (applesaucefdc.com/a2r2-reference/) wurde bis
 * MF-1483 im ganzen Baum NICHT gelesen; `parse_strm_chunk()` bekannte
 * sich dazu im Kommentar und liess `side = 0` als „NICHT belegt" stehen.
 * Genau das war P3-656. Sie sagt woertlich:
 *
 *   INFO, Feld Disk Type bei +33, ein Byte:  „1 = 5.25, 2 = 3.5"
 *
 *   STRM, Feld Location:
 *     5.25"  „For 5.25 disks, this value is in halfphases or quarter
 *             tracks. For example track 0.00 is halfphase 0 and track
 *             1.00 is halfphase 4."
 *     3.5"   „For 3.5 disks, this value indicates track number as well
 *             as side. The formula ((track << 1) + side) can be used
 *             (0 = Track 0 Side 0, 1 = Track 0 Side 1, 2 = Track 1
 *             Side 0)."
 *
 * Damit ist entschieden, was vorher eine Annahme war: fuer Disk Type 1
 * ist die rohe Location richtig, fuer Disk Type 2 MUSS zerlegt werden.
 *
 * ── Warum die Datei hier gebaut und nicht beschafft wird ─────────────
 *
 * Der Korpus hat kein zweiseitiges A2R2. Gemessen ueber alle 50
 * Aufnahmen des Fahey-Satzes (MF-1483): 50 von 50 sind Disk Type 1, wo
 * roh und richtig zusammenfallen. Ein Fixture waere also zu beschaffen,
 * und solange es fehlt, prueft niemand die andere Haelfte der Regel.
 *
 * Deshalb baut dieser Test die kleinste Datei, die die REGEL ausloest —
 * nicht eine Diskette, sondern einen Behaelter mit zwei Locations, deren
 * richtige Deutung die Referenz woertlich angibt: Location 2 = Spur 1
 * Seite 0, Location 3 = Spur 1 Seite 1. Die Flussbytes darin sind
 * bedeutungslos und sollen es sein; geprueft wird die Zerlegung, nicht
 * das Dekodieren. Erfunden ist hier keine Formatangabe — jede Zahl im
 * Kopf steht in der Referenz.
 *
 * ── Rotbeweis ────────────────────────────────────────────────────────
 *
 * Vor der Korrektur: `parse_strm_chunk()` wies `track_number = location`
 * roh zu und `side = 0`. Aus den Locations 2 und 3 wurden damit die
 * „Zylinder" 2 und 3 auf Kopf 0 — also 4x1 statt 2x2, und die Seite 1
 * war verloren. Diese Datei faellt vor der Korrektur und steht danach.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"   /* vor uft_core.h */
#include "uft/uft_core.h"
#include "uft/parsers/uft_a2r_parser.h"

extern const uft_format_plugin_t uft_format_plugin_a2r;

static int g_pass, g_fail;

#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; \
    printf("  FEHLGESCHLAGEN Zeile %d: ", __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static void u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

/* Baut ein A2R2 mit Disk Type `typ` und den angegebenen Locations.
 * Aufbau streng nach der A2R-2.x-Referenz:
 *   Kopf   "A2R2" 0xFF 0x0A 0x0D 0x0A
 *   Block  <Kennung:4> <Laenge:4 LE> <Nutzlast>
 *   INFO   version(1) creator(32) disk_type(1) write_protected(1)
 *          synchronized(1)  = 36 Byte
 *   STRM   je Eintrag: location(1) capture_type(1) data_length(4)
 *          estimated_loop_point(4) <Daten> … dann 0xFF als Ende-Marke
 */
static int a2r2_bauen(const char *pfad, unsigned char typ,
                      const unsigned char *orte, size_t n_orte)
{
    const size_t FLUSS = 8;            /* bedeutungslos, aber vorhanden */
    unsigned char info[36];
    memset(info, 0, sizeof info);
    info[0] = 1;                       /* INFO version 1 */
    memcpy(info + 1, "UFT red proof MF-1484", 21);
    info[33] = typ;                    /* Disk Type: 1 = 5.25, 2 = 3.5 */
    info[34] = 0;                      /* write protected */
    info[35] = 1;                      /* synchronized */

    size_t strm_len = n_orte * (10 + FLUSS) + 1;
    unsigned char *strm = (unsigned char *)calloc(1, strm_len);
    if (!strm) return 0;
    size_t o = 0;
    for (size_t i = 0; i < n_orte; i++) {
        strm[o++] = orte[i];           /* location */
        strm[o++] = 1;                 /* capture type: timing */
        u32(strm + o, (unsigned long)FLUSS); o += 4;
        u32(strm + o, 0);              o += 4;   /* loop point */
        for (size_t k = 0; k < FLUSS; k++) strm[o++] = 0x40;
    }
    strm[o++] = 0xFF;                  /* Ende-Marke */

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(strm); return 0; }
    unsigned char kopf[8] = { 'A', '2', 'R', '2', 0xFF, 0x0A, 0x0D, 0x0A };
    unsigned char bk[8];
    int ok = fwrite(kopf, 1, 8, f) == 8;
    memcpy(bk, "INFO", 4); u32(bk + 4, (unsigned long)sizeof info);
    ok = ok && fwrite(bk, 1, 8, f) == 8 &&
         fwrite(info, 1, sizeof info, f) == sizeof info;
    memcpy(bk, "STRM", 4); u32(bk + 4, (unsigned long)o);
    ok = ok && fwrite(bk, 1, 8, f) == 8 && fwrite(strm, 1, o, f) == o;
    fclose(f);
    free(strm);
    return ok;
}

static const char *wegwerf_verzeichnis(void)
{
    const char *k[] = { "TMPDIR", "TMP", "TEMP" };
    for (size_t i = 0; i < sizeof k / sizeof k[0]; i++) {
        const char *v = getenv(k[i]);
        if (v && *v) return v;
    }
    return ".";
}

int main(void)
{
    uft_register_all_formats();
    char pfad[1024];

    /* ── Disk Type 2 (3.5"): die Seite steckt in der Location ───────── */
    snprintf(pfad, sizeof pfad, "%s/uft_mf1484_typ2.a2r",
             wegwerf_verzeichnis());
    /* Location 2 = Spur 1 Seite 0, Location 3 = Spur 1 Seite 1
     * (woertlich aus der Referenz). */
    const unsigned char orte2[] = { 2, 3 };
    if (!a2r2_bauen(pfad, 2, orte2, 2)) {
        printf("SKIP: Wegwerf-Datei nicht anzulegen (%s)\n", pfad);
        return 77;
    }

    uft_disk_t *disk = uft_disk_open(pfad, true);
    CHECK(disk != NULL, "Disk Type 2: uft_disk_open lieferte NULL");
    if (disk) {
        const uft_format_plugin_t *p = uft_disk_plugin(disk);
        printf("  Disk Type 2 (3.5\"), Locations 2 und 3 -> Plugin \"%s\", "
               "%ux%u\n", p ? p->name : "(keins)",
               (unsigned)disk->geometry.cylinders,
               (unsigned)disk->geometry.heads);
        CHECK(p == &uft_format_plugin_a2r,
              "Disk Type 2 wurde von \"%s\" geoeffnet",
              p ? p->name : "(keins)");

        /* Die Referenz: (track << 1) + side. Location 2 und 3 sind damit
         * Spur 1 auf Seite 0 und Seite 1 — also EIN Zylinder, ZWEI
         * Koepfe. Roh gelesen waeren es die „Zylinder" 2 und 3 auf einem
         * Kopf, und die Seite 1 waere verloren. */
        CHECK(disk->geometry.heads == 2,
              "Disk Type 2 meldet %u Kopf/Koepfe. Location 3 ist nach der "
              "A2R-2.x-Referenz Spur 1 SEITE 1 — wer `side = 0` setzt, "
              "verwirft die Seite", (unsigned)disk->geometry.heads);
        CHECK(disk->geometry.cylinders == 2,
              "Disk Type 2 meldet %u Zylinder. ((track << 1) + side) "
              "macht aus 2 und 3 die Spur 1 auf beiden Seiten, also "
              "Zylinder 0..1 — %u heisst, die Location wurde roh "
              "uebernommen", (unsigned)disk->geometry.cylinders,
              (unsigned)disk->geometry.cylinders);
        uft_disk_close(disk);
    }
    remove(pfad);

    /* ── Disk Type 1 (5.25"): die Location BLEIBT eine Viertelspur ──── */
    snprintf(pfad, sizeof pfad, "%s/uft_mf1484_typ1.a2r",
             wegwerf_verzeichnis());
    const unsigned char orte1[] = { 0, 4, 8 };   /* Spur 0.00, 1.00, 2.00 */
    if (!a2r2_bauen(pfad, 1, orte1, 3)) {
        printf("FEHLGESCHLAGEN: zweite Wegwerf-Datei nicht anzulegen\n");
        g_fail++;                       /* MF-1340: kein stiller Skip */
    } else {
        uft_disk_t *d1 = uft_disk_open(pfad, true);
        CHECK(d1 != NULL, "Disk Type 1: uft_disk_open lieferte NULL");
        if (d1) {
            printf("  Disk Type 1 (5.25\"), Locations 0/4/8 -> %ux%u\n",
                   (unsigned)d1->geometry.cylinders,
                   (unsigned)d1->geometry.heads);
            /* Die INFO-Felder selbst — sie wiegen schwerer als die
             * Geometrie, weil eines davon eine forensische Aussage ist.
             * Der v2-Zweig war um ein Byte verschoben und las
             * `write_protected` aus dem Disk Type: damit galt JEDE
             * A2R2-Datei als schreibgeschuetzt. Hier steht, was in die
             * Datei geschrieben wurde. */
            a2r_context_t *ctx = a2r_open(pfad);
            CHECK(ctx != NULL, "a2r_open lieferte NULL fuer die "
                               "Typ-1-Pruefdatei");
            if (ctx) {
                printf("    INFO: Erzeuger \"%s\", disk_type %u, "
                       "write_protected %d, synchronized %d\n",
                       ctx->info.creator, (unsigned)ctx->info.disk_type,
                       (int)ctx->info.write_protected,
                       (int)ctx->info.synchronized);
                CHECK(ctx->info.disk_type == 1,
                      "disk_type ist %u, die Datei sagt 1 — die Referenz "
                      "nennt das Feld bei INFO+33",
                      (unsigned)ctx->info.disk_type);
                CHECK(ctx->info.write_protected == false,
                      "write_protected ist wahr, die Datei sagt 0. Ein "
                      "Leser, der jede Datei fuer schreibgeschuetzt "
                      "haelt, trifft eine forensische Aussage ohne Grund");
                CHECK(ctx->info.synchronized == true,
                      "synchronized ist falsch, die Datei sagt 1");
                CHECK(strncmp(ctx->info.creator, "UFT red proof", 13) == 0,
                      "Erzeuger ist \"%s\" — erwartet ist der Text ab "
                      "INFO+1, ohne das Versionsbyte davor",
                      ctx->info.creator);
                a2r_close(ctx);
            }
            /* „track 0.00 ist halfphase 0 und track 1.00 ist halfphase 4":
             * die groesste Location ist 8, also 9 Positionen, ein Kopf.
             * Wer hier zerlegt, macht aus 8 die Spur 4 Seite 0. */
            CHECK(d1->geometry.heads == 1,
                  "Disk Type 1 meldet %u Koepfe — bei 5.25\" ist die "
                  "Location eine Viertelspur, die Seite steckt NICHT "
                  "darin", (unsigned)d1->geometry.heads);
            CHECK(d1->geometry.cylinders == 9,
                  "Disk Type 1 meldet %u Positionen statt 9. Die groesste "
                  "Location ist 8; wer sie zerlegt, kommt auf 5",
                  (unsigned)d1->geometry.cylinders);
            uft_disk_close(d1);
        }
        remove(pfad);
    }

    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
