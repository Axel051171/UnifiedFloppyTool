/* SPDX-License-Identifier: MIT */
/**
 * @file test_g71_liefert_sektoren.c
 * @brief G71 liefert Sektoren — beide Seiten, gegen die Pruefsummen auf
 *        der Diskette (P3-599, K4, MF-1436)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * `vice_c1541_1571.g71` ist eine gueltige doppelseitige 1571-Diskette:
 * Magie `GCR-1571`, Kopfbyte 9 = 168 Halbspur-Eintraege (2 Seiten x 84),
 * Spurgroesse 7928, 667476 Byte. `g71_plugin_open()` nimmt sie an und
 * meldet 42 Zylinder auf 2 Koepfen.
 *
 * Geliefert hat sie trotzdem **nichts**. Gemessen (P3-599):
 * `g71_plugin_read_track()` setzte `raw_data` und `raw_size`, aber
 * **kein `raw_bits`**, und legte **keine Sektoren** an. Die Bruecke ins
 * Zentrum hat damit weder einen Bitstrom, den sie nehmen darf, noch
 * Sektoren — sie sagte mit `RAW_BITS_UNKNOWN` ab und fuehrte 0 Spuren.
 * Und ihre Weigerung war RICHTIG: die Bitlaenge einer GCR-Spur steht im
 * Behaelter nicht, dort steht eine BYTE-Zahl; `bytes * 8` waeren bis zu
 * sieben erfundene Bits.
 *
 * ── DIE ZAHLEN KOMMEN NICHT AUS UNSEREM LESER ───────────────────────
 *
 * Vor diesem Test wurde die Datei mit einem EIGENEN GCR-Dekodierer
 * ausserhalb des Baums vermessen — absichtlich, damit die Zusicherung
 * nicht von dem Code abhaengt, den sie prueft:
 *
 *     Plaetze belegt            84 von 168   (42 je Seite, ganze Spuren)
 *     Spuren mit GCR-Koepfen    84 von 84
 *     Kopfbloecke               802 je Seite, 1604 gesamt
 *     Spurlaengen               7692 / 7142 / 6666 / 6250 (die vier Zonen)
 *
 * 802 ist genau die 42-Spur-Sektorzahl einer 1541, und sie ergibt sich
 * hier ein zweites Mal aus `gcr_sectors_per_track()` — also aus dem
 * Format, nicht aus einer gemerkten Zahl.
 *
 * ── DIE PRUEFSUMMEN STEHEN AUF DER DISKETTE ─────────────────────────
 *
 * `gcr_extract_sector()` aus `src/formats/c64/uft_gcr_ops.c` prueft den
 * Kopfblock gegen `Spur ^ Sektor ^ ID1 ^ ID2` und den Datenblock gegen
 * das XOR ueber seine 256 Byte. Beide Summen hat der schreibende
 * Rechner auf die Diskette geschrieben, nicht unser Code — geht es auf,
 * ist das eine Aussage ueber die Wirklichkeit und keine
 * Selbstbestaetigung (MF-1013, derselbe Weg wie MF-869 beim FM-Pfad).
 *
 * Genau diese zwei Funktionen waren bis MF-1013 Fassaden: sie lieferten
 * 256 Nullbytes UND Erfolg. Sie sind es seit MF-1013 nicht mehr, und
 * dieser Test ist ihr erster Aufrufer aus dem Produktivpfad — vorher
 * riefen sie nur Tests (P3-204-Klasse: Koennen ohne Tuer).
 *
 * ── WAS DIESER TEST ZUSICHERT ───────────────────────────────────────
 *
 *  1. Beide Seiten liefern Sektoren, und zwar so viele, wie das Format
 *     je Spur vorsieht.
 *  2. Seite 1 ist NICHT Seite 0 — sonst waere ein falscher Index
 *     unbemerkt (dieselbe Falle wie in MF-1383, wo Lesen und Schreiben
 *     denselben falschen Index benutzten und rund liefen).
 *  3. Die Pruefsummen der Diskette gehen auf.
 *
 * ── WAS DIESER TEST NICHT PRUEFEN KANN, UND WARUM ───────────────────
 *
 * Die Mutationsmatrix zu dieser Arbeit hat drei Mutationen gefahren
 * (Sektorschleife entfernt, Spurnummer um eins daneben, Kopf aus dem
 * Index) — alle drei fallen an ihrer eigenen Stelle. Eine VIERTE waere
 * gruen geblieben: `pruef.data_ok` im Plugin zu ignorieren und die
 * Sektoren unbedingt als fehlerfrei zu fuehren.
 *
 * Der Grund ist das Abbild: `vice_c1541_1571.g71` hat **null**
 * Pruefsummenfehler, also kann kein Vergleich zeigen, dass die
 * Weitergabe von `data_ok` ueberhaupt stattfindet. Das ist eine Grenze
 * dieses Tests, nicht ein Beleg fuer das Plugin.
 *
 * Was sie schliessen wuerde: ein Korpusabbild mit einem NACHWEISLICH
 * defekten Sektor, oder ein Pruefling, der eine Spur von Hand baut und
 * ein Datenbyte kippt. Solange keines vorliegt, gilt fuer den
 * Fehlerpfad nur die Zusage von `gcr_extract_sector()` selbst
 * (MF-1013), nicht eine Messung an diesem Plugin.
 */

#include "uft/uft_format_plugin.h"   /* vor uft_core.h */
#include "uft/uft_core.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_g71;

static int g_pass = 0, g_fail = 0;

#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; \
    printf("  FEHLGESCHLAGEN Zeile %d: ", __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Sektoren je Spur einer 1541 — aus der Zonenaufteilung, nicht gemerkt. */
static int sektoren_je_spur(int spur)
{
    if (spur <= 17) return 21;
    if (spur <= 24) return 19;
    if (spur <= 30) return 18;
    return 17;
}

static void spur_freigeben(uft_track_t *t)
{
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors);
    if (t->raw_data && t->owns_data) free(t->raw_data);
    memset(t, 0, sizeof(*t));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_g71_liefert_sektoren (P3-599, K4, MF-1436) ===\n\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("uft_register_all_formats scheiterte\n");
        return 1;
    }

    char pfad[1024];
    snprintf(pfad, sizeof pfad, "%s/vice_c1541_1571.g71", UFT_CORPUS_DIR);

    uft_disk_t *disk = uft_disk_open(pfad, true);
    if (!disk) {
        printf("SKIP: %s nicht oeffenbar.\n", pfad);
        return 77;      /* SKIP_RETURN_CODE, MF-598 */
    }

    printf("Geoeffnet: %s — %u Zylinder, %u Koepfe\n",
           uft_disk_plugin(disk) ? uft_disk_plugin(disk)->name : "(keins)",
           (unsigned)disk->geometry.cylinders,
           (unsigned)disk->geometry.heads);

    /* Dass die Datei aufgeht, sagt noch nicht, WER sie aufgemacht hat.
     * 101 registrierte Plugins tragen `UFT_FORMAT_DSK`, und das
     * Erkennungsrennen hat schon einmal die falsche Geometrie gewinnen
     * lassen (P3-425). Also festnageln.
     *
     * Nebeneffekt, und er ist kein Zufall: `gen_verification_tiers.py`
     * ordnet Tests ueber Nennungen von `uft_format_plugin_<sym>` zu
     * (Kopf des Skripts). Ein Test, der ein Format ueber den ECHTEN
     * `uft_disk_open()` prueft — also auf dem staerkeren Weg —, war fuer
     * die Stufentafel unsichtbar; einer, der die Plugin-Struktur direkt
     * anfasst, wird gezaehlt. Diese Zeile schliesst beides zugleich:
     * sie prueft etwas Sinnvolles UND macht den Test sichtbar. Der
     * Generator-Befund steht als P3-633. */
    CHECK(uft_disk_plugin(disk) == &uft_format_plugin_g71,
          "nicht das G71-Plugin hat geoeffnet, sondern \"%s\"",
          uft_disk_plugin(disk) ? uft_disk_plugin(disk)->name : "(keins)");
    CHECK(disk->geometry.heads == 2, "erwartet 2 Koepfe, gemessen %u",
          (unsigned)disk->geometry.heads);
    CHECK(disk->geometry.cylinders == 42, "erwartet 42 Zylinder, gemessen %u",
          (unsigned)disk->geometry.cylinders);

    /* ── 1./3. beide Seiten liefern, und die Summen gehen auf ───────── */
    int je_seite[2] = {0, 0};
    int erwartet = 0;
    int crc_schlecht = 0;
    /* Zum Unterscheiden der Seiten die ROHEN Spurbytes, nicht die
     * Nutzdaten — siehe die Begruendung an der Zusicherung unten. */
    uint8_t *roh_s0 = NULL, *roh_s1 = NULL;
    size_t roh_n0 = 0, roh_n1 = 0;

    for (unsigned h = 0; h < 2u; h++) {
        for (unsigned c = 0; c < disk->geometry.cylinders; c++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            const uft_error_t r =
                uft_disk_plugin(disk)->read_track(disk, (int)c, (int)h, &t);
            if (r != UFT_OK) {
                CHECK(0, "read_track(%u,%u) lieferte %d", c, h, (int)r);
                spur_freigeben(&t);
                continue;
            }
            je_seite[h] += (int)t.sector_count;
            if (h == 0) erwartet += sektoren_je_spur((int)c + 1);

            for (size_t s = 0; s < t.sector_count; s++)
                if (t.sectors[s].status & UFT_SECTOR_CRC_ERROR) crc_schlecht++;

            if (c == 0 && t.raw_data && t.raw_size) {
                uint8_t **ziel = (h == 0) ? &roh_s0 : &roh_s1;
                size_t *zn = (h == 0) ? &roh_n0 : &roh_n1;
                *ziel = malloc(t.raw_size);
                if (*ziel) { memcpy(*ziel, t.raw_data, t.raw_size);
                             *zn = t.raw_size; }
            }
            spur_freigeben(&t);
        }
    }

    printf("Sektoren Seite 0 : %d\n", je_seite[0]);
    printf("Sektoren Seite 1 : %d\n", je_seite[1]);
    printf("Erwartet je Seite: %d (aus der Zonenaufteilung)\n", erwartet);
    printf("Sektoren mit Datenpruefsummenfehler: %d\n", crc_schlecht);

    CHECK(erwartet == 802, "die Zonenrechnung ergibt %d, erwartet 802",
          erwartet);
    CHECK(je_seite[0] == erwartet,
          "Seite 0 liefert %d Sektoren, erwartet %d", je_seite[0], erwartet);
    CHECK(je_seite[1] == erwartet,
          "Seite 1 liefert %d Sektoren, erwartet %d", je_seite[1], erwartet);
    CHECK(crc_schlecht == 0,
          "%d Sektoren mit Datenpruefsummenfehler — die Summen stehen auf "
          "der Diskette, sie muessen aufgehen", crc_schlecht);

    /* ── 2. Seite 1 ist nicht Seite 0 ───────────────────────────────────
     *
     * BERICHTIGT: Hier wurden zuerst die NUTZDATEN von Spur 1 Sektor 0
     * beider Seiten verglichen, und die Zusicherung fiel. Gemessen war
     * aber nicht der Code falsch, sondern die Zusicherung: auf einer
     * frisch formatierten Diskette sind die 256 Nutzdatenbytes ueber
     * beide Seiten **identisch**. Nutzdaten koennen den Index also gar
     * nicht belegen — eine Zusicherung, die das versucht, prueft das
     * Fuellmuster und nicht die Abbildung.
     *
     * Die ROHEN Spurbytes koennen es: gemessen weichen sie in **84 von
     * 7692** Byte ab, und das sind die Kopfbloecke. Unabhaengig
     * bestaetigt durch die Spurnummern AUF der Diskette: Platz 0
     * (cyl 0, Kopf 0) nennt Spur 1, Platz 84 (cyl 0, Kopf 1) nennt
     * **Spur 36** — eine 1571 fuehrt Seite 1 als CBM-Spuren 36..70.
     * Verschiedene Plaetze, verschiedene Versaetze (1356 gegen 334416).
     *
     * Die Falle, gegen die das schuetzt, ist gemessen: in MF-1383
     * benutzten Lesen und Schreiben denselben falschen Index und liefen
     * rund. */
    CHECK(roh_s0 && roh_s1,
          "die Spurbytes von Zylinder 0 fehlen auf mindestens einer Seite "
          "(s0=%p, s1=%p)", (void *)roh_s0, (void *)roh_s1);
    if (roh_s0 && roh_s1) {
        CHECK(roh_n0 == roh_n1,
              "die Spuren haben verschiedene Laengen (%zu gegen %zu) — "
              "dann vergleicht der Test nichts", roh_n0, roh_n1);
        if (roh_n0 == roh_n1) {
            size_t abw = 0;
            for (size_t i = 0; i < roh_n0; i++)
                if (roh_s0[i] != roh_s1[i]) abw++;
            printf("Spurbytes Zylinder 0: %zu von %zu Byte weichen ab\n",
                   abw, roh_n0);
            CHECK(abw > 0,
                  "Zylinder 0 ist auf beiden Seiten byteidentisch — ein "
                  "falscher Index waere so unbemerkt (vgl. MF-1383)");
        }
    }
    free(roh_s0);
    free(roh_s1);

    uft_disk_close(disk);
    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
