/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_dms_gegen_adf2dms.c
 * @brief `dms` von T3 auf T1b — der Erzeuger, den P3-347 als nicht
 *        existent gemessen hatte (MF-1135)
 *
 * ── Warum `dms` auf T3 stand ──────────────────────────────────────────────
 *
 * Nicht weil der Leser schlecht war. P3-347 und P3-373 hatten gemessen,
 * dass **kein Werkzeug im Baum eine DMS ERZEUGT**: hxcfe, xdms und
 * amigadx lesen nur, und der originale Packer laeuft auf dem Amiga und
 * ist proprietaer. Jeder DMS-Test war damit ein geschlossener Kreis —
 * UFT gegen UFT, die Lage von `apridisk` (MF-1009) und `qrst`
 * (MF-1028).
 *
 * ── Der Erzeuger ──────────────────────────────────────────────────────────
 *
 * `dlitz/adf2dms` (MIT, LICENSE.txt woertlich gelesen; Klon unter
 * `tools/uft-scout/work/adf2dms`) ist ein ADF-nach-DMS-PACKER in
 * reinem Python. Seine Schreiber-Linie ist unabhaengig von xDMS —
 * und aus xDMS stammen sowohl UFTs Leser als auch der von hxcfe. Der
 * geschlossene Kreis ist damit offen.
 *
 * Gemessen vor der Uebernahme:
 *
 *   * der Packer ist deterministisch (zwei Laeufe, gleiche SHA-256);
 *   * er erzeugt NOCOMP (cmode 0) UND echtes RLE (cmode 1) — in allen
 *     80 Spursaetzen am Kopffeld nachgezaehlt; die RLE-Fassung ist
 *     **40 376 Byte** aus 901 120, also keine Durchreichung;
 *   * **hxcfe als ZWEITE fremde Hand** liest beide Erzeugnisse zurueck:
 *     0 von 901 120 Byte abweichend gegen die Eingabe, gleiche SHA-256.
 *
 * ── Die Eingabe benennt sich selbst ───────────────────────────────────────
 *
 * MF-1021 hat gemessen, was ein Abbild ohne nachgewiesenen INHALT wert
 * ist: dort erzeugte hxcfe eine Datei richtiger Groesse, meldete
 * Erfolg, und sie bestand zu 100 % aus dem Fuellbyte 0xF6 — null
 * Aussage. Die ADF hinter dieser DMS traegt deshalb je Sektor
 *
 *     "UFT-K C%02d H%d S%02d "     (17 Byte) + 495 Nullbyte
 *
 * und die Regel ist an **allen 1760** Sektoren der Quelle geprueft.
 * Ein Leseergebnis sagt damit nicht nur, DASS etwas kam, sondern ob
 * die RICHTIGE Stelle getroffen wurde — eine vertauschte Spur, ein
 * verschobener Kopf oder eine falsche Sektornummer fallen auf.
 *
 * Deshalb liegt auch nur die **DMS** im Baum und keine ADF daneben: die
 * Erwartung kommt aus der Regel, nicht aus einer zweiten Datei. 40 KB
 * statt 940 KB, und der Vergleich wird dabei nicht schwaecher, sondern
 * strenger — eine gespeicherte Vergleichsdatei koennte selbst falsch
 * sein.
 *
 * ── Was hier NICHT belegt ist ─────────────────────────────────────────────
 *
 * `adf2dms` kennt die Modi quick/medium/deep/heavy. Belegt sind NOCOMP
 * und RLE; die uebrigen vier Kompressionsverfahren hat keine fremde Hand
 * geprueft. Und Weg (a) aus P3-347 — eine ECHTE `.dms` aus der Zeit mit
 * geklaerter Herkunft, die T1 ergaebe — bleibt daneben offen. T1b sagt,
 * woher der Beleg kommt, nicht dass der Leser fertig ist.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"

#ifndef UFT_CORPUS_FREE_DIR
#define UFT_CORPUS_FREE_DIR "tests/corpus_free"
#endif

#define DMS_DATEI "adf2dms_uftk_rle_880k.dms"

/* Aus dem Erzeugnis gemessen, nicht angenommen. */
#define ZYL  80
#define KOPF  2
#define SPT  11
#define SEK 512u

extern const uft_format_plugin_t uft_format_plugin_dms;

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }               \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }               \
        assert(bed);                                                        \
    } while (0)

/* Die Erwartung, aus der Regel — nicht aus einer Datei. */
static void erwartet_bauen(uint8_t *aus, int c, int h, int s) {
    char name[24];
    const int n = snprintf(name, sizeof name, "UFT-K C%02d H%d S%02d ",
                           c, h, s);
    memset(aus, 0, SEK);
    memcpy(aus, name, (size_t)n);
}

static char g_pfad[1024];

static bool pfad_finden(void) {
    static const char *orte[] = {
        UFT_CORPUS_FREE_DIR "/" DMS_DATEI,
        "tests/corpus_free/" DMS_DATEI,
        "../tests/corpus_free/" DMS_DATEI,
        "../../tests/corpus_free/" DMS_DATEI,
    };
    for (size_t i = 0; i < sizeof orte / sizeof orte[0]; i++) {
        FILE *f = fopen(orte[i], "rb");
        if (f) {
            fclose(f);
            snprintf(g_pfad, sizeof g_pfad, "%s", orte[i]);
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════════
 * 1) Die Sonde erkennt das Fremderzeugnis
 * ══════════════════════════════════════════════════════════════════════ */
static void sonde(void) {
    FILE *f = fopen(g_pfad, "rb");
    if (!f) { rot++; printf("   [ROT] Datei nicht lesbar\n"); return; }
    uint8_t kopf[8192];
    const size_t kn = fread(kopf, 1, sizeof kopf, f);
    fseek(f, 0, SEEK_END);
    const long groesse = ftell(f);
    fclose(f);

    printf("   %s: %ld Byte\n", DMS_DATEI, groesse);
    ZUSAGE(groesse == 40376,
           "die Datei ist 40 376 Byte gross — RLE-komprimiert aus "
           "901 120, also keine Durchreichung");

    int konf = -1;
    const int p = uft_format_plugin_dms.probe
        ? uft_format_plugin_dms.probe(kopf, kn, (size_t)groesse, &konf)
        : -1;
    printf("   Sonde -> %d, Konfidenz %d\n", p, konf);
    ZUSAGE(p == 1, "die Sonde nimmt das Fremderzeugnis an");
    ZUSAGE(konf >= 80,
           "mit einer Konfidenz im Band 'Merkmal getroffen' (MF-729) — "
           "DMS hat eine echte Kennung, keine Groessenheuristik");
}

/* ══════════════════════════════════════════════════════════════════════
 * 2) Jeder Sektor an seiner eigenen Ortsmarke
 * ══════════════════════════════════════════════════════════════════════ */
static void alle_sektoren(void) {
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);

    const uft_error_t rc = uft_format_plugin_dms.open(&disk, g_pfad, true);
    ZUSAGE(rc == UFT_OK, "open() gelingt");
    if (rc != UFT_OK) return;

    printf("   Geometrie laut Plugin: %d x %d x %d\n",
           disk.geometry.cylinders, disk.geometry.heads,
           disk.geometry.sectors);
    ZUSAGE(disk.geometry.cylinders == ZYL && disk.geometry.heads == KOPF &&
           disk.geometry.sectors == SPT,
           "80 x 2 x 11 — die AmigaDOS-DD-Geometrie aus dem Abbild");

    size_t gesamt = 0, gleich = 0, abweichend = 0;
    size_t spuren = 0, spuren_fehler = 0;
    char erste[200] = {0};

    for (int c = 0; c < ZYL; c++) {
        for (int h = 0; h < KOPF; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_dms.read_track(&disk, c, h, &t) != UFT_OK) {
                spuren_fehler++;
                uft_track_cleanup(&t);
                continue;
            }
            spuren++;
            for (size_t s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sc = &t.sectors[s];
                if (!sc->data || sc->data_len < SEK) continue;
                gesamt++;
                uint8_t soll[SEK];
                erwartet_bauen(soll, c, h, (int)s);
                if (memcmp(sc->data, soll, SEK) == 0) {
                    gleich++;
                } else {
                    abweichend++;
                    if (!erste[0]) {
                        char ist[24] = {0}, sl[24] = {0};
                        memcpy(ist, sc->data, 17);
                        memcpy(sl, soll, 17);
                        snprintf(erste, sizeof erste,
                                 "C%02d H%d S%02zu: ist \"%s\" soll \"%s\"",
                                 c, h, s, ist, sl);
                    }
                }
            }
            uft_track_cleanup(&t);
        }
    }
    if (uft_format_plugin_dms.close) uft_format_plugin_dms.close(&disk);

    printf("   Spuren %zu gelesen, %zu nicht lesbar\n",
           spuren, spuren_fehler);
    printf("   Sektoren %zu: %zu an ihrer Ortsmarke, %zu abweichend\n",
           gesamt, gleich, abweichend);
    if (erste[0]) printf("   erste Abweichung: %s\n", erste);

    ZUSAGE(spuren == (size_t)(ZYL * KOPF) && spuren_fehler == 0,
           "alle 160 Spuren lesbar");
    ZUSAGE(gesamt == (size_t)(ZYL * KOPF * SPT),
           "1760 Sektoren — 80 x 2 x 11, keiner fehlt");
    ZUSAGE(abweichend == 0,
           "JEDER Sektor traegt seine eigene Ortsmarke: 1760 von 1760 "
           "byteidentisch gegen die Regel");
}

/* ══════════════════════════════════════════════════════════════════════
 * 3) Negativtest — eine abgeschnittene Datei wird abgewiesen
 * ══════════════════════════════════════════════════════════════════════
 *
 * Ohne diesen Abschnitt belegt der Test nur, dass der Leser eine GUTE
 * Datei liest. Ein Leser, der alles annimmt, ist kein Leser
 * (MF-919/MF-1039: eine Sonde, die nie „nein" sagt).
 */
static void abgeschnitten(void) {
    FILE *q = fopen(g_pfad, "rb");
    if (!q) { rot++; printf("   [ROT] Quelle nicht lesbar\n"); return; }
    uint8_t *puf = malloc(40376);
    if (!puf) { fclose(q); rot++; return; }
    const size_t n = fread(puf, 1, 40376, q);
    fclose(q);

    const char *kurz = "mf1135_abgeschnitten.dms";
    FILE *z = fopen(kurz, "wb");
    if (!z) { free(puf); rot++; return; }
    /* Kopf vollstaendig, Nutzlast auf ein Drittel gekuerzt. */
    const size_t behalten = n / 3;
    fwrite(puf, 1, behalten, z);
    fclose(z);
    free(puf);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    const uft_error_t rc = uft_format_plugin_dms.open(&disk, kurz, true);

    /* Die erste Fassung dieses Abschnitts verlangte eine ABSAGE — und
     * das war die falsche Frage. Gemessen tut der Leser naemlich das
     * Richtige: er versucht es streng, scheitert, wiederholt mit
     * Fehlertoleranz, stellt 292 864 von 901 120 Byte wieder her und
     * SAGT es („Integritaet NICHT bestaetigt — der Rest bleibt 0xE5").
     * Ein Befund darf den Zugriff nicht verstellen (MF-830).
     *
     * Die richtige Frage ist, ob die Fuellsektoren als FEHLEND
     * gekennzeichnet sind. Vor MF-1135 waren sie es nicht: die Warnung
     * erreichte den Bediener, die Datenstruktur nicht — `sectors[]`
     * zeigte elf gute Sektoren mit erfundenen 0xE5-Bytes. Das ist die
     * Klasse MF-1001/MF-1022/MF-1038. */
    int vorn_gut = 0, vorn_fehlend = 0;
    int hinten_gut = 0, hinten_fehlend = 0;
    if (rc == UFT_OK) {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        if (uft_format_plugin_dms.read_track(&disk, 0, 0, &t) == UFT_OK) {
            for (size_t s = 0; s < t.sector_count; s++) {
                if (t.sectors[s].status & UFT_SECTOR_MISSING) vorn_fehlend++;
                else vorn_gut++;
            }
        }
        uft_track_cleanup(&t);

        memset(&t, 0, sizeof t);
        if (uft_format_plugin_dms.read_track(&disk, 70, 1, &t) == UFT_OK) {
            for (size_t s = 0; s < t.sector_count; s++) {
                if (t.sectors[s].status & UFT_SECTOR_MISSING) hinten_fehlend++;
                else hinten_gut++;
            }
        }
        uft_track_cleanup(&t);
        if (uft_format_plugin_dms.close) uft_format_plugin_dms.close(&disk);
    }
    remove(kurz);

    printf("   gedrittelte Datei (%zu von %zu Byte): open -> %d\n",
           behalten, n, (int)rc);
    printf("   Spur 0/0 (im wiederhergestellten Teil): %d gut, "
           "%d fehlend\n", vorn_gut, vorn_fehlend);
    printf("   Spur 70/1 (dahinter):                   %d gut, "
           "%d fehlend\n", hinten_gut, hinten_fehlend);

    ZUSAGE(hinten_gut == 0 && hinten_fehlend > 0,
           "hinter der Wiederherstellungsgrenze ist JEDER Sektor als "
           "fehlend gekennzeichnet — 0xE5-Fuellung gilt nicht als "
           "gelesenes Datum");
    ZUSAGE(vorn_gut > 0 && vorn_fehlend == 0,
           "und im wiederhergestellten Teil ist KEINER gekennzeichnet — "
           "die Kennzeichnung trifft die Grenze, nicht alles");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("MF-1135 — dms gegen ein Erzeugnis von adf2dms (MIT)\n");

    if (!pfad_finden()) {
        printf("   [uebersprungen] %s nicht gefunden — der Beleg ist "
               "damit UNGEMESSEN, nicht gruen (MF-598)\n", DMS_DATEI);
        return 77;
    }

    printf("\n1) Sonde\n");
    sonde();

    printf("\n2) Alle 1760 Sektoren an ihrer Ortsmarke\n");
    alle_sektoren();

    printf("\n3) Negativtest: abgeschnittene Datei\n");
    abgeschnitten();

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
