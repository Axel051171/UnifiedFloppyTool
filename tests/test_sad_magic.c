/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_sad_magic.c
 * @brief SAD suchte nach einer Kennung, die es nicht gibt (MF-787).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * `src/formats/sad/uft_sad.c` prüfte auf die Signatur **`"SAD!"`**. Die
 * steht in keiner SAD-Datei. Gemessen an einer von **SAMdisk 4.0**
 * erzeugten Datei lautet der Kopf:
 *
 *     41 6c 65 79 27 73 20 64 69 73 6b 20 62 61 63 6b 75 70   "Aley's disk backup"
 *     02                                                      Köpfe    = 2
 *     50                                                      Zylinder = 80
 *     0a                                                      Sektoren = 10
 *     08                                                      Sektorgröße / 64 = 512
 *
 * 18 Byte Kennung plus vier Byte Geometrie — zusammen genau die 22, die
 * das Plugin ohnehin einliest. Der Kopf ist damit **selbsterklärend**:
 * seine vier Zahlen sagen 80 × 2 × 10 × 512 = 819 200, und das ist
 * exakt die Nutzdatenmenge der Datei (819 222 − 22).
 *
 * ── Was das kostete ──────────────────────────────────────────────────────
 *
 * Beides zugleich, und beides still:
 *
 *   * `sad_probe()` fand die falsche Kennung nicht und fiel auf die
 *     Größenprüfung `file_size == 819200` zurück — die eine echte
 *     SAD-Datei **nie** erfüllt, weil ihr Kopf 22 Byte hinzufügt. Eine
 *     echte SAD-Datei wurde also **gar nicht als SAD erkannt**.
 *   * `sad_open()` schloss aus der fehlenden Kennung auf „kopflos" und
 *     las die 22 Kopfbytes **als Nutzdaten** — Sektor 0 begann damit
 *     mitten in „Aley's disk backup".
 *
 * ── Wie es gefunden wurde ────────────────────────────────────────────────
 *
 * Nicht durch Lesen, sondern durch einen **Differenzlauf**. Ein
 * SAMdisk-erzeugtes Abbild fiel im Korpus-Test mit „Sektor 0 ohne
 * Kennung" durch; die Nutzdaten hinter Offset 22 waren aber
 * nachweislich richtig (`55 46 54 00` = `UFT\0`, Sektornummer 0). Damit
 * lag der Fehler auf UFTs Seite, und ein Blick auf die ersten 22 Byte
 * zeigte, welcher.
 *
 * Das ist dieselbe Klasse wie die fünf fabrizierten Parser (FMT-2/3/10/
 * 11/12): eine **erfundene** statt gemessene Signatur, grün getestet
 * gegen selbst erzeugte Eingaben.
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * SAMdisk 4.0 ALPHA (2022-07-25), MIT, im Oracle-Register als `samdisk`.
 * Die Kennung ist zusätzlich **selbstbelegend**: die vier Geometriebytes
 * dahinter rechnen genau auf die Dateigröße auf.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_sad;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-36s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

static const char *pfad(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/samdisk_sad.sad", UFT_CORPUS_DIR);
    return p;
}

static size_t kopf_lesen(uint8_t *out, size_t n, long *dateigroesse)
{
    FILE *f = fopen(pfad(), "rb");
    if (!f) return 0;
    size_t gelesen = fread(out, 1, n, f);
    fseek(f, 0, SEEK_END);
    *dateigroesse = ftell(f);
    fclose(f);
    return gelesen;
}

/* Die Kennung, wörtlich — und die vier Zahlen dahinter. */
TEST(kopf_traegt_aleys_kennung_und_geometrie)
{
    uint8_t k[64];
    long fs = 0;
    ASSERT(kopf_lesen(k, sizeof(k), &fs) == sizeof(k));

    ASSERT(memcmp(k, "Aley's disk backup", 18) == 0);
    ASSERT(k[18] == 2);     /* Koepfe */
    ASSERT(k[19] == 80);    /* Zylinder */
    ASSERT(k[20] == 10);    /* Sektoren je Spur */
    ASSERT(k[21] == 8);     /* Sektorgroesse / 64 -> 512 */

    /* Der Kopf belegt sich selbst: seine Zahlen rechnen auf die Datei auf. */
    long nutz = (long)k[19] * k[18] * k[20] * ((long)k[21] * 64);
    ASSERT(nutz == 819200L);
    ASSERT(fs == nutz + 22);
}

/* DER ROTBEWEIS. Vor MF-787 schlug dieser Fall fehl: die Sonde suchte
 * „SAD!", fand nichts, und fiel auf `file_size == 819200` zurueck — was
 * eine echte SAD-Datei nie erfuellt, weil ihr Kopf 22 Byte hinzufuegt. */
TEST(sonde_erkennt_eine_echte_sad_datei)
{
    uint8_t k[512];
    long fs = 0;
    ASSERT(kopf_lesen(k, sizeof(k), &fs) == sizeof(k));

    int conf = 0;
    ASSERT(uft_format_plugin_sad.probe(k, sizeof(k), (size_t)fs, &conf));
    /* Eine getroffene Kennung ist ein MERKMAL, nicht bloss eine Groesse
     * — Band 80..100 nach MF-729. */
    ASSERT(conf >= 80);
}

/* Die zweite Haelfte desselben Fehlers: ohne erkannte Kennung schloss
 * `sad_open()` auf „kopflos" und las die 22 Kopfbytes als NUTZDATEN. */
TEST(oeffnen_ueberspringt_den_kopf)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_sad.open(&disk, pfad(), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 80);
    ASSERT(disk.geometry.heads == 2);
    ASSERT(disk.geometry.sectors == 10);
    ASSERT(disk.geometry.sector_size == 512);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    if (uft_format_plugin_sad.read_track(&disk, 0, 0, &t) != UFT_OK) {
        uft_format_plugin_sad.close(&disk);
        ASSERT(0);
    }
    ASSERT(t.sector_count == 10);
    ASSERT(t.sectors[0].data && t.sectors[0].data_len == 512);

    /* Begaenne Sektor 0 im Kopf, staende hier „ley's disk backup". */
    ASSERT(memcmp(t.sectors[0].data, "UFT\x00", 4) == 0);
    ASSERT(t.sectors[0].data[4] == 0 && t.sectors[0].data[5] == 0);

    for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
    free(t.sectors);
    uft_format_plugin_sad.close(&disk);
}

/* Gegenprobe: ein KOPFLOSES Abbild der richtigen Groesse muss weiterhin
 * angenommen werden — die alte Groessenregel bleibt gueltig, sie war nur
 * nicht die einzige. */
TEST(kopfloses_abbild_bleibt_erkannt)
{
    uint8_t leer[512];
    memset(leer, 0, sizeof(leer));
    int conf = 0;
    ASSERT(uft_format_plugin_sad.probe(leer, sizeof(leer), 819200u, &conf));
    /* Ohne Merkmal darf die Sonde nur die Groesse beanspruchen: 30..49. */
    ASSERT(conf >= 30 && conf < 50);
}


/* ── DER ZWEITE BEFUND: SAD ist KOPF-DUR (MF-794) ───────────────────────
 *
 * Der Test oben liest Spur (0,0) — und das ist die EINZIGE Spur, die
 * unter beiden denkbaren Anordnungen an derselben Stelle liegt. Genau
 * deshalb blieb der zweite Fehler eine Woche unbemerkt.
 *
 * SAD legt die Spuren KOPF-DUR ab: alle Zylinder von Seite 0, dann alle
 * von Seite 1. Der Spurindex ist also `kopf * zylinder + zylinder_nr`,
 * nicht `zylinder_nr * koepfe + kopf`.
 *
 * Gemessen, zwei unabhaengige Haende (MF-785 sperrt keine davon fuer SAD):
 *
 *   * **SAMdisk 4.0** schreibt aus einem zylinder-dur Rohabbild eine SAD,
 *     in der Spurblock 1 den Sektor 20 traegt (= Zyl. 1 / Kopf 0) und
 *     Spurblock 80 den Sektor 10 (= Zyl. 0 / Kopf 1).
 *   * **hxcfe 2.16.13** liest dieselbe Datei und gibt ein Rohabbild
 *     zurueck, das mit der urspruenglichen Quelle **byteidentisch** ist.
 *     Der Rundlauf img -> sad -> img schliesst sich also, und zwar nur
 *     unter kopf-dur.
 *
 * Die Korpus-Datei belegt es aus sich selbst — die Zusicherungen unten
 * lesen sie roh, ohne UFTs Plugin zu befragen.
 *
 * WAS DER FEHLER KOSTETE: von 160 Spuren las UFT **158 an der falschen
 * Stelle**. Richtig lagen nur (0,0) und (79,1) — Anfang und Ende, wo
 * beide Formeln denselben Index ergeben. Ein Benutzer haette ein
 * vollstaendig durchmischtes Abbild bekommen, ohne eine einzige
 * Fehlermeldung.
 */

/* Erst die Datei selbst, ohne UFT: welche Anordnung liegt vor? */
TEST(die_datei_selbst_ist_kopf_dur)
{
    FILE *f = fopen(pfad(), "rb");
    ASSERT(f != NULL);
    /* Spurblock 1 = zweiter 10-Sektor-Block. Unter kopf-dur ist das
     * Zylinder 1 / Kopf 0, dessen Sektor 0 die Marke 20 traegt. */
    uint8_t b[8];
    ASSERT(fseek(f, 22 + 1 * 10 * 512, SEEK_SET) == 0);
    ASSERT(fread(b, 1, 6, f) == 6);
    ASSERT(memcmp(b, "UFT\x00", 4) == 0);
    ASSERT((b[4] | (b[5] << 8)) == 20);

    /* Spurblock 80 = Zylinder 0 / Kopf 1 -> Marke 10. */
    ASSERT(fseek(f, 22 + 80 * 10 * 512, SEEK_SET) == 0);
    ASSERT(fread(b, 1, 6, f) == 6);
    ASSERT(memcmp(b, "UFT\x00", 4) == 0);
    ASSERT((b[4] | (b[5] << 8)) == 10);
    fclose(f);
}

/* DER ROTBEWEIS. Vor MF-794 lieferte read_track(0,1) den Sektor 20 —
 * also Zylinder 1 / Kopf 0. */
TEST(spur_0_1_liefert_kopf_1_nicht_zylinder_1)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_sad.open(&disk, pfad(), true) == UFT_OK);

    struct { int cyl, head, erwartet; } faelle[] = {
        { 0,  0,   0 },   /* lag schon vorher richtig */
        { 0,  1,  10 },   /* <- der Rotbeweis */
        { 1,  0,  20 },
        { 1,  1,  30 },
        { 79, 1, 1590 },  /* lag schon vorher richtig */
    };
    for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
        uft_track_t t;
        memset(&t, 0, sizeof(t));
        if (uft_format_plugin_sad.read_track(&disk, faelle[i].cyl,
                                             faelle[i].head, &t) != UFT_OK) {
            uft_format_plugin_sad.close(&disk);
            ASSERT(0);
        }
        int nr = -1;
        if (t.sector_count == 10 && t.sectors[0].data &&
            memcmp(t.sectors[0].data, "UFT\x00", 4) == 0)
            nr = t.sectors[0].data[4] | (t.sectors[0].data[5] << 8);
        for (size_t k = 0; k < t.sector_count; k++) free(t.sectors[k].data);
        free(t.sectors);
        if (nr != faelle[i].erwartet) {
            printf("FAIL: Spur (%d,%d) -> Sektor %d, erwartet %d\n",
                   faelle[i].cyl, faelle[i].head, nr, faelle[i].erwartet);
            _fail++;
            uft_format_plugin_sad.close(&disk);
            return;
        }
    }
    uft_format_plugin_sad.close(&disk);
}

/* Die Formel steht an ZWEI Stellen — read_track und write_track. Der
 * Rotbeweis oben belegt nur die erste.
 *
 * WAS DIESER FALL BELEGT UND WAS NICHT: er zeigt, dass beide Formeln
 * DIESELBE sind. Waere nur eine geaendert worden, landete jede Spur an
 * einer anderen Stelle, als sie herkam, und der Vergleich schluege fehl.
 * Dass die Formel RICHTIG ist, belegt er nicht — das leistet der Fall
 * darueber, der gegen die rohe Datei prueft. Erst beide zusammen
 * tragen.
 *
 * Das Ziel wird mit 0xE5 vorbelegt und nicht etwa kopiert: eine Kopie
 * waere schon vor dem ersten Schreibzugriff identisch und der Vergleich
 * wertlos. */
TEST(lese_und_schreibpfad_verwenden_dieselbe_anordnung)
{
    uft_disk_t q;
    memset(&q, 0, sizeof(q));
    ASSERT(uft_format_plugin_sad.open(&q, pfad(), true) == UFT_OK);

    char ziel[512];
    snprintf(ziel, sizeof(ziel), "%s/_sad_rundlauf.sad", UFT_CORPUS_DIR);
    {   /* Kopf woertlich uebernehmen, Nutzdaten auf 0xE5 setzen. */
        FILE *o = fopen(pfad(), "rb"), *n = fopen(ziel, "wb");
        if (!o || !n) { if (o) fclose(o); if (n) fclose(n);
                        uft_format_plugin_sad.close(&q); ASSERT(0); }
        uint8_t k[22];
        size_t g = fread(k, 1, 22, o);
        fclose(o);
        if (g != 22 || fwrite(k, 1, 22, n) != 22) {
            fclose(n); uft_format_plugin_sad.close(&q); ASSERT(0);
        }
        uint8_t fuell[512];
        memset(fuell, 0xE5, sizeof(fuell));
        for (int i = 0; i < 1600; i++)
            if (fwrite(fuell, 1, 512, n) != 512) {
                fclose(n); uft_format_plugin_sad.close(&q); ASSERT(0);
            }
        fclose(n);
    }

    uft_disk_t z;
    memset(&z, 0, sizeof(z));
    if (uft_format_plugin_sad.open(&z, ziel, false) != UFT_OK) {
        uft_format_plugin_sad.close(&q);
        ASSERT(0);
    }

    int fehler = 0;
    for (int c = 0; c < q.geometry.cylinders && !fehler; c++)
        for (int h = 0; h < q.geometry.heads && !fehler; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_sad.read_track(&q, c, h, &t) != UFT_OK ||
                uft_format_plugin_sad.write_track(&z, c, h, &t) != UFT_OK)
                fehler = 1;
            for (size_t k = 0; k < t.sector_count; k++) free(t.sectors[k].data);
            free(t.sectors);
        }
    uft_format_plugin_sad.close(&z);
    uft_format_plugin_sad.close(&q);
    ASSERT(!fehler);

    /* Byteweiser Vergleich gegen das Original. */
    FILE *a = fopen(pfad(), "rb"), *b = fopen(ziel, "rb");
    ASSERT(a && b);
    long gleich = 1, n = 0;
    for (;;) {
        int x = fgetc(a), y = fgetc(b);
        if (x == EOF && y == EOF) break;
        if (x != y) { gleich = 0; break; }
        n++;
    }
    fclose(a); fclose(b);
    remove(ziel);
    if (!gleich) printf("FAIL: erste Abweichung bei Offset %ld\n", n);
    ASSERT(gleich);
    ASSERT(n == 819222L);
}

int main(void)
{
    printf("test_sad_magic (MF-787)\n");
    RUN(kopf_traegt_aleys_kennung_und_geometrie);
    RUN(sonde_erkennt_eine_echte_sad_datei);
    RUN(oeffnen_ueberspringt_den_kopf);
    RUN(kopfloses_abbild_bleibt_erkannt);
    RUN(die_datei_selbst_ist_kopf_dur);
    RUN(spur_0_1_liefert_kopf_1_nicht_zylinder_1);
    RUN(lese_und_schreibpfad_verwenden_dieselbe_anordnung);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
