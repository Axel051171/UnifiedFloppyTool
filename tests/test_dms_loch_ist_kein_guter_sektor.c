/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_dms_loch_ist_kein_guter_sektor.c
 * @brief MF-1135 hat das ENDE gekennzeichnet, nicht das LOCH — und ein
 *        Loch in der Mitte kam als guter Sektor heraus (A-026)
 *
 * ── Was MF-1135 behoben hat, und was offen blieb ──────────────────────────
 *
 * `dms_unpack()` gibt in `written` zurueck, wie weit der Entpacker
 * gekommen ist. MF-1135 hat gemessen, dass `open()` diese Zahl in eine
 * Warnung schrieb und danach verwarf: an einer GEDRITTELTEN Datei kamen
 * 292 864 von 901 120 Byte zurueck, und Spur 70/1 — die es in der Datei
 * nicht mehr gibt — meldete **elf gute Sektoren** mit erfundener
 * 0xE5-Fuellung. Seither behaelt das Plugin die Grenze in `p->gelesen`
 * und kennzeichnet jeden Sektor dahinter.
 *
 * Das faengt die ABGESCHNITTENE Datei. Es faengt nicht das LOCH.
 *
 * ── Gemessen am Vorzustand ────────────────────────────────────────────────
 *
 * Eine DMS, in der genau EIN Byte in den gepackten Daten des Spursatzes
 * 40 gekippt ist — alle 80 Spurkoepfe unveraendert, keine Laenge, keine
 * Pruefsumme angefasst, also genau die Lage einer an einer Stelle
 * beschaedigten Diskette:
 *
 *   strenger Lauf       : bricht ab mit „Track data CRC error"
 *   toleranter Lauf     : 901 120 von 901 120 Byte — `written == adf_size`
 *   Sektoren gesamt     : 1760
 *   Status UFT_SECTOR_OK: **1760**
 *   gekennzeichnet      : **0**
 *   Selbstbenennung     : **1759** von 1760 treffen
 *
 * Ein Sektor traegt verfaelschte Bytes und meldet sich als guter Sektor
 * mit gueltigen CRC-Flags. Der Grund ist arithmetisch: im toleranten
 * Lauf laufen im Entpacker ALLE drei Pruefungen durch (Daten-CRC,
 * Entpackfehler, Pruefsumme) und die Spur wird trotzdem geschrieben —
 * `out_pos` waechst mit. Bei einem Loch in der Mitte liegt die Grenze
 * zwischen GELESEN und GEFUELLT damit am DATEIENDE, und `p->gelesen`
 * deckt das Loch mit ab.
 *
 * Das ist die Klasse, die dieser Baum sechsmal behoben hat: MF-1001
 * („gefuellt, nicht gelesen"), MF-1022 (`sap`s Fuellsektor als guter
 * Sektor mit gueltiger CRC), MF-1038 (`fds`, 36 erfundene Byte je Seite
 * als `UFT_SECTOR_OK`), MF-980 („das Format sagt 0xE5" und „hier wurde
 * 0xE5 gelesen" sind zwei Aussagen), MF-1040 (`cas`) und MF-1135 selbst.
 * Und der Dateikopf von `uft_dms_plugin.c` sagt ueber seinen Vorgaenger
 * woertlich: *„der Verlust wurde als Datum ausgegeben"* — fuer das Loch
 * galt der Satz weiter.
 *
 * ── Der Mechanismus lag die ganze Zeit im Baum, ungerufen ─────────────────
 *
 * `dms_unpack()` nimmt einen Spur-Callback, und `dms_track_info_t`
 * traegt `crc_ok` und `checksum_ok` JE SPURSATZ. Gemessen an derselben
 * Datei meldet der Callback genau `Spursatz 40: crc_ok=0 checksum_ok=0`
 * bei 80 gemeldeten Saetzen — waehrend `dms_unpack` 0 zurueckgibt.
 * Das Plugin uebergab an dieser Stelle `NULL`. Klasse MF-930/P3-204:
 * ein arbeitender Mechanismus ohne Aufrufer.
 *
 * Dritter Fall derselben Datei: `dms_is_dms()`, `dms_disk_type_name()`
 * und `dms_comp_mode_name()` haben in `src/` je 0 Aufrufer ausserhalb
 * ihrer eigenen Definition — nur Tests rufen sie.
 *
 * ── Was dieser Test NICHT behauptet ───────────────────────────────────────
 *
 * Er sagt nichts darueber, WELCHE Bytes richtig waeren — ein
 * beschaedigter Spursatz ist nicht wiederherstellbar, und ihn zu raten
 * waere die Fabrikation, gegen die der ganze Baum geschrieben ist. Er
 * sagt nur: er darf sich nicht als guter Sektor ausgeben.
 *
 * Und er prueft EINEN Modus — das Korpusstueck traegt gemessen 80
 * Spursaetze, alle `cmode = 1` (SIMPLE/RLE), `flags = 0` durchgehend,
 * keine Sondernummern. Die sechs uebrigen Betriebsarten und jedes
 * gesetzte Flag-Bit sind unbelegt; `adf2dms` kann sie gemessen nicht
 * herstellen (`main.py:39` wirft `NotImplementedError`).
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

/* Am Erzeugnis gemessen, nicht angenommen (siehe Dateikopf). */
#define GROESSE   40376u
#define SAETZE       80u
#define ZYL          80
#define KOPF          2
#define SPT          11
#define SEK         512u
#define SEK_GESAMT (ZYL * KOPF * SPT)   /* 1760 */
#define UNPK_JE_SATZ 11264u             /* ein DMS-Satz ist ein ZYLINDER */
#define FAUL_SATZ    40                 /* der Satz, den dieser Test kippt */

/* DMS-Dateiaufbau, aus xDMS 1.3.2 `pfile.c` (Public Domain, gelesen):
 * HEADLEN 56, THLEN 20, `pklen1` im Spurkopf bei Byte 6-7 (big endian). */
#define DMS_KOPF_LEN 56u
#define DMS_TH_LEN   20u

extern const uft_format_plugin_t uft_format_plugin_dms;

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }               \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }               \
        assert(bed);                                                        \
    } while (0)

/* ── Korpusstueck finden ───────────────────────────────────────────────── */

static char g_pfad[1024];

static bool pfad_finden(void)
{
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

static uint8_t *datei_lesen(const char *pfad, size_t *laenge)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *d = (uint8_t *)malloc((size_t)n);
    if (!d) { fclose(f); return NULL; }
    size_t gelesen = fread(d, 1, (size_t)n, f);
    fclose(f);
    if (gelesen != (size_t)n) { free(d); return NULL; }
    *laenge = (size_t)n;
    return d;
}

/* Throw-away path from TMPDIR/TMP/TEMP, falling back to ".", like the
 * other file tests in this tree. Not tmpnam(): MinGW's tmpnam() names a
 * file in the ROOT of the current drive (measured "\spcc."), which an
 * ordinary user cannot create, so the test never ran on a local Windows
 * build (MF-1340). */
static void wegwerf_pfad(char *buf, size_t n, const char *name)
{
    static unsigned lauf = 0;
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(buf, n, "%s/uft_%s_%u.tmp", d, name, lauf++);
}

static char *schreiben(const uint8_t *d, size_t n)
{
    static char pfad[1024];
    wegwerf_pfad(pfad, sizeof pfad, "dms_loch");
    FILE *f = fopen(pfad, "wb");
    if (!f) return NULL;
    size_t w = fwrite(d, 1, n, f);
    fclose(f);
    return (w == n) ? pfad : NULL;
}

/* ── Die Verfaelschungsregel steht HIER, nicht in einem Skript ──────────── */

/* Laeuft die Spurkopf-Kette ab und kippt ein Byte MITTEN in der Nutzlast
 * des Satzes mit der Nummer `wunsch`. Aendert keine Laenge und keine
 * Pruefsumme; gibt die Zahl der gefundenen Spurkoepfe zurueck, oder 0,
 * wenn die Kette nicht aufgeht. */
static unsigned loch_schlagen(uint8_t *d, size_t n, int wunsch,
                              size_t *stelle_aus)
{
    size_t off = DMS_KOPF_LEN;
    unsigned saetze = 0;
    size_t stelle = 0;
    while (off + DMS_TH_LEN <= n) {
        if (d[off] != 'T' || d[off + 1] != 'R') break;
        int nummer      = (d[off + 2] << 8) | d[off + 3];
        unsigned pklen1 = (unsigned)((d[off + 6] << 8) | d[off + 7]);
        if (nummer == wunsch && pklen1 >= 2)
            stelle = off + DMS_TH_LEN + pklen1 / 2u;
        saetze++;
        off += DMS_TH_LEN + pklen1;
    }
    if (stelle == 0 || stelle >= n) return 0;
    d[stelle] ^= 0xFF;
    *stelle_aus = stelle;
    return saetze;
}

/* ── Bilanz einer geoeffneten Diskette ─────────────────────────────────── */

typedef struct {
    unsigned sektoren;
    unsigned status_ok;
    unsigned gekennzeichnet;
    unsigned benennung_trifft;
    unsigned ok_im_faulen_zyl;
    unsigned mark_im_faulen_zyl;
} bilanz_t;

static void bilanzieren(uft_disk_t *disk, bilanz_t *b)
{
    memset(b, 0, sizeof *b);
    for (int c = 0; c < ZYL; c++) {
        for (int h = 0; h < KOPF; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_dms.read_track(disk, c, h, &t) != UFT_OK) { uft_track_cleanup(&t); continue; }
            for (unsigned s = 0; s < t.sector_count; s++) {
                char erw[32];
                int ln = snprintf(erw, sizeof erw, "UFT-K C%02d H%d S%02d ",
                                  c, h, (int)s);
                const uint8_t *dat = t.sectors[s].data;
                if (dat && memcmp(dat, erw, (size_t)ln) == 0)
                    b->benennung_trifft++;
                int ok = (t.sectors[s].status == UFT_SECTOR_OK);
                if (ok) b->status_ok++; else b->gekennzeichnet++;
                if (c == FAUL_SATZ) { if (ok) b->ok_im_faulen_zyl++;
                                      else    b->mark_im_faulen_zyl++; }
                b->sektoren++;
            }
            uft_track_cleanup(&t);
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 1) Nulllinie — das unveraenderte Korpusstueck ist restlos in Ordnung
 *
 * Ohne diese Zusage koennte der Test aus dem falschen Grund gruen sein:
 * ein Plugin, das JEDEN Sektor kennzeichnet, wuerde 2) und 3) bestehen.
 * ══════════════════════════════════════════════════════════════════════ */
static void nulllinie(void)
{
    printf("\n1) Nulllinie: das unveraenderte Fremderzeugnis\n");

    size_t n = 0;
    uint8_t *d = datei_lesen(g_pfad, &n);
    ZUSAGE(d != NULL, "das Korpusstueck ist lesbar");
    if (!d) return;
    ZUSAGE(n == GROESSE,
           "40 376 Byte — RLE-gepackt aus 901 120, keine Durchreichung");

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t e = uft_format_plugin_dms.open(&disk, g_pfad, true);
    ZUSAGE(e == UFT_OK, "open() nimmt das Fremderzeugnis an");
    if (e != UFT_OK) { free(d); return; }

    bilanz_t b;
    bilanzieren(&disk, &b);
    printf("      %u Sektoren, %u OK, %u gekennzeichnet, %u benannt\n",
           b.sektoren, b.status_ok, b.gekennzeichnet, b.benennung_trifft);

    ZUSAGE(b.sektoren == SEK_GESAMT,
           "1760 Sektoren — 80 Zylinder x 2 Koepfe x 11");
    ZUSAGE(b.status_ok == SEK_GESAMT,
           "alle 1760 melden UFT_SECTOR_OK, weil die Datei in Ordnung ist");
    ZUSAGE(b.gekennzeichnet == 0,
           "kein Sektor ist gekennzeichnet — das Plugin kennzeichnet nicht "
           "blind");
    ZUSAGE(b.benennung_trifft == SEK_GESAMT,
           "alle 1760 Selbstbenennungen treffen ihre eigene Ortsmarke");

    uft_format_plugin_dms.close(&disk);
    free(d);
}

/* ══════════════════════════════════════════════════════════════════════
 * 2) Ein Loch in der Mitte — und es darf sich nicht als guter Sektor
 *    ausgeben
 * ══════════════════════════════════════════════════════════════════════ */
static void loch(void)
{
    printf("\n2) Ein gekipptes Byte im Spursatz %d\n", FAUL_SATZ);

    size_t n = 0;
    uint8_t *d = datei_lesen(g_pfad, &n);
    if (!d) { rot++; printf("   [ROT] Korpusstueck nicht lesbar\n"); return; }

    size_t stelle = 0;
    unsigned saetze = loch_schlagen(d, n, FAUL_SATZ, &stelle);
    printf("      Spurkopf-Kette: %u Saetze, gekippt an Versatz %zu\n",
           saetze, stelle);
    ZUSAGE(saetze == SAETZE,
           "die Spurkopf-Kette geht mit 80 Saetzen auf — es ist EIN Byte "
           "der Nutzlast gekippt, keine Laenge und keine Pruefsumme");

    char *pfad = schreiben(d, n);
    ZUSAGE(pfad != NULL, "die verfaelschte Fassung ist schreibbar");
    if (!pfad) { free(d); return; }

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t e = uft_format_plugin_dms.open(&disk, pfad, true);

    /* Ein Befund darf den Zugriff nicht verstellen (MF-830). */
    ZUSAGE(e == UFT_OK,
           "open() liefert die Diskette trotz des Befundes — ein Befund "
           "verstellt den Zugriff nicht (MF-830)");
    if (e != UFT_OK) { free(d); remove(pfad); return; }

    bilanz_t b;
    bilanzieren(&disk, &b);
    printf("      %u Sektoren, %u OK, %u gekennzeichnet, %u benannt\n",
           b.sektoren, b.status_ok, b.gekennzeichnet, b.benennung_trifft);
    printf("      Zylinder %d: %u OK, %u gekennzeichnet\n",
           FAUL_SATZ, b.ok_im_faulen_zyl, b.mark_im_faulen_zyl);

    /* ANTI-TAUTOLOGIE: das gekippte Byte MUSS die Daten wirklich
     * veraendern. Traefe die Selbstbenennung weiter 1760-mal, wuerde
     * dieser Test nichts messen — dann waere die Verfaelschung
     * folgenlos gewaehlt und alle Zusagen darunter waeren leer. */
    ZUSAGE(b.benennung_trifft < SEK_GESAMT,
           "ANTI-TAUTOLOGIE: das gekippte Byte veraendert die gelesenen "
           "Daten wirklich — mindestens eine Selbstbenennung trifft nicht "
           "mehr");

    /* Der Befund selbst. */
    ZUSAGE(b.mark_im_faulen_zyl > 0,
           "der Zylinder des faulen Spursatzes hat mindestens einen "
           "GEKENNZEICHNETEN Sektor — ein Loch ist kein guter Sektor");
    ZUSAGE(b.mark_im_faulen_zyl == (unsigned)(KOPF * SPT),
           "und zwar ALLE 22 seiner Sektoren: der Spursatz ist die "
           "kleinste Einheit, fuer die der Entpacker eine Aussage macht — "
           "welches Byte darin faul ist, sagt er nicht");
    ZUSAGE(b.status_ok == SEK_GESAMT - (unsigned)(KOPF * SPT),
           "und NUR diese 22 — die uebrigen 1738 bleiben UFT_SECTOR_OK, "
           "die Kennzeichnung ist keine Pauschale");

    uft_format_plugin_dms.close(&disk);
    free(d);
    remove(pfad);
}

/* ══════════════════════════════════════════════════════════════════════
 * 3) Die abgeschnittene Datei bleibt gekennzeichnet (MF-1135-Regression)
 *
 * Die neue Kennzeichnung ERSETZT die alte nicht. Ein abgeschnittenes
 * Archiv hat gar keine Spursaetze mehr, ueber die ein Callback etwas
 * sagen koennte — dort traegt allein `written` die Grenze.
 * ══════════════════════════════════════════════════════════════════════ */
static void abgeschnitten(void)
{
    printf("\n3) Regression MF-1135: die abgeschnittene Datei\n");

    size_t n = 0;
    uint8_t *d = datei_lesen(g_pfad, &n);
    if (!d) { rot++; printf("   [ROT] Korpusstueck nicht lesbar\n"); return; }

    const size_t kurz = n / 3u;      /* gedrittelt, wie in MF-1135 */
    char *pfad = schreiben(d, kurz);
    ZUSAGE(pfad != NULL, "die gedrittelte Fassung ist schreibbar");
    if (!pfad) { free(d); return; }

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t e = uft_format_plugin_dms.open(&disk, pfad, true);
    ZUSAGE(e == UFT_OK,
           "open() liefert, was wiederherstellbar ist");
    if (e != UFT_OK) { free(d); remove(pfad); return; }

    bilanz_t b;
    bilanzieren(&disk, &b);
    printf("      %u Sektoren, %u OK, %u gekennzeichnet, %u benannt\n",
           b.sektoren, b.status_ok, b.gekennzeichnet, b.benennung_trifft);

    ZUSAGE(b.gekennzeichnet > 0,
           "die Sektoren hinter dem Abbruch sind gekennzeichnet — MF-1135 "
           "gilt weiter");
    ZUSAGE(b.status_ok < SEK_GESAMT,
           "und nicht alle 1760 melden OK");
    ZUSAGE(b.benennung_trifft < SEK_GESAMT,
           "die Datei traegt wirklich weniger Daten — sonst waere die "
           "Zusage darueber leer");

    uft_format_plugin_dms.close(&disk);
    free(d);
    remove(pfad);
}

/* ══════════════════════════════════════════════════════════════════════
 * 4) Die Sonde sagt nicht mehr, als sie gelesen hat
 *
 * Vorzustand, gemessen: sie verglich VIER Byte und meldete 98 — eine
 * Datei, die nur aus „DMS!" besteht, bekam dieselbe Zahl wie ein
 * vollstaendiges, in sich stimmiges Archiv. Dazu verwarf sie
 * `file_size` (`(void)file_size`), obwohl der Kopf sie nachrechenbar
 * macht: die MF-1029-Falle.
 * ══════════════════════════════════════════════════════════════════════ */
static void sonde(void)
{
    printf("\n4) Die Sonde: abgeleitet statt vergeben (MF-1153)\n");

    size_t n = 0;
    uint8_t *d = datei_lesen(g_pfad, &n);
    if (!d) { rot++; printf("   [ROT] Korpusstueck nicht lesbar\n"); return; }

    /* (a) Das echte Archiv: alle vier Belege. */
    int k_echt = -1;
    bool p_echt = uft_format_plugin_dms.probe(d, n, n, &k_echt);
    printf("      echtes Archiv        -> %d, Konfidenz %d\n",
           (int)p_echt, k_echt);
    ZUSAGE(p_echt && k_echt == 100,
           "das vollstaendige Archiv bekommt 100: Kennung 50 + Struktur 15 "
           "(Kopf-CRC) + Selbstkonsistenz 25 (56 + pkfsize + 20 x Saetze "
           "== Dateigroesse) + Geometrie 10 (unpkfsize == Saetze x 11 264)");

    /* (b) Vier Byte. Die Kennung stimmt, sonst ist nichts gelesen. */
    int k_vier = -1;
    bool p_vier = uft_format_plugin_dms.probe(d, 4, 4, &k_vier);
    printf("      nur \"DMS!\" (4 Byte)  -> %d, Konfidenz %d\n",
           (int)p_vier, k_vier);
    ZUSAGE(p_vier, "vier Byte Kennung reichen fuer einen Anspruch");
    ZUSAGE(k_vier == 50,
           "aber nur fuer 50 — die Kennung allein, kein Kopf, keine "
           "Selbstkonsistenz, keine Geometrie. Vorher waren es 98");
    ZUSAGE(k_vier < 80,
           "und damit NICHT im Band 'Merkmal getroffen' (MF-729): aus vier "
           "Byte folgt kein vollstaendiges Archiv");

    /* (c) Vollstaendiger Kopf, aber der Kopf-CRC ist verfaelscht.
     *     Dann faellt die Struktur weg — und mit ihr die beiden
     *     Rechnungen, die `dms_read_info()` liefert. */
    uint8_t *kaputt = (uint8_t *)malloc(n);
    ZUSAGE(kaputt != NULL, "Arbeitskopie fuer den verfaelschten Kopf");
    if (kaputt) {
        memcpy(kaputt, d, n);
        kaputt[54] ^= 0xFF;            /* Kopf-CRC, Byte 54-55 */
        int k_krc = -1;
        bool p_krc = uft_format_plugin_dms.probe(kaputt, n, n, &k_krc);
        printf("      Kopf-CRC verfaelscht -> %d, Konfidenz %d\n",
               (int)p_krc, k_krc);
        ZUSAGE(p_krc && k_krc == 50,
               "ein falscher Kopf-CRC kostet Struktur, Selbstkonsistenz "
               "und Geometrie auf einmal — 50 bleibt, weil die Kennung "
               "wirklich dasteht");
        free(kaputt);
    }

    /* (d) ANTI-TAUTOLOGIE: die drei Zahlen muessen sich UNTERSCHEIDEN.
     *     Eine Sonde, die immer dasselbe meldet, bestuende (a) bis (c)
     *     nicht — aber sie bestuende sie, wenn alle Erwartungen gleich
     *     waeren. Deshalb steht der Abstand hier ausdruecklich. */
    ZUSAGE(k_echt > k_vier,
           "ANTI-TAUTOLOGIE: das vollstaendige Archiv bekommt MEHR als "
           "vier Byte — die Sonde unterscheidet wirklich");

    free(d);
}

/* ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== dms: ein Loch in der Mitte ist kein guter Sektor (A-026)\n");

    if (!pfad_finden()) {
        printf("   [SKIP] %s nicht gefunden — Korpus fehlt\n", DMS_DATEI);
        return 77;
    }
    printf("   Korpus: %s\n", g_pfad);

    nulllinie();
    loch();
    abgeschnitten();
    sonde();

    printf("\n=== %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
