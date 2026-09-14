/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_dtc_nenner.c
 * @brief Der NENNER der fremden dtc-Testreihe, gemessen statt kommentiert
 *        (MF-1126)
 *
 * ── Warum es diesen Test gibt ─────────────────────────────────────────────
 *
 * `src/dtc_components/tests/test_main.c` endet mit `puts("all tests
 * passed")` — **ohne Zahl**. Eine Null ohne Nenner ist keine Auskunft
 * (Klasse MF-1000), und dasselbe gilt fuer ein „passed" ohne Nenner: es
 * sagt nicht, WIE VIEL geprueft wurde. Wuerden zwoelf der fuenfzehn
 * Zusagen entfernt, meldete die Reihe woertlich dasselbe.
 *
 * Die fremde Datei wird dafuer **nicht umgeschrieben** — wer sie
 * aendert, macht aus einem Beleg eine Ableitung. Der Nenner steht
 * deshalb hier, daneben, und wird aus der Datei GELESEN statt in ihr
 * gepflegt.
 *
 * ── Der Nenner war selbst uneindeutig, und das ist der eigentliche Fund ──
 *
 * Er stand bisher in einem CMake-Kommentar, und zwei Fassungen desselben
 * Satzes zaehlen verschieden:
 *
 *   `tests/CMakeLists.txt`  „**sieben** Pruefgruppen (CRC-32 und
 *                           CRC-16/CCITT gegen `"123456789"`, MFM-,
 *                           GCR-Rundlauf, Bitpuffer, Flussstatistik,
 *                           Formaterkennung, CT-Raw)" — die beiden CRCs
 *                           als EINE Gruppe gezaehlt
 *   Eigentuemer-Weisung     „**sieben** Pruefgruppen (CRC-32,
 *                           CRC-16/CCITT, MFM, GCR, Bitpuffer,
 *                           Flussstatistik, Formaterkennung, CT-Raw)" —
 *                           **acht** Posten, mit „sieben" beschriftet
 *
 * Ein Kommentar altert, und dieser hatte schon gedriftet, bevor ihn
 * jemand geprueft hat. Gezaehlt wird deshalb, was objektiv in der Datei
 * steht.
 *
 * ── Was gemessen ist (git-Objekt, 2026-09-14) ─────────────────────────────
 *
 *   `assert(`-Aufrufe         **15**
 *   `&&` darin                **7**
 *   -> geprueifte Bedingungen **22**
 *   Gruppen (Leitsymbol)      **8** — je genau einmal genannt
 *   Zeilen                    25
 *
 * Die acht Leitsymbole sind `dtc_crc32`, `dtc_crc16_ccitt`,
 * `dtc_mfm_encode`, `dtc_gcr_encode`, `dtc_bits_init`,
 * `dtc_flux_measure`, `dtc_detect_buffer`, `dtc_ctraw_write`. Jedes
 * steht fuer eine Pruefgruppe, und jedes kommt genau einmal vor — damit
 * ist „acht Gruppen" nicht gezaehlt, sondern belegt.
 *
 * ── Abgrenzung: was dieser Test NICHT ist ─────────────────────────────────
 *
 * Er prueft **nicht**, ob der Fremdbestand richtig rechnet — das tun
 * `test_dtc_ungeprueft` (MF-1109) und, fuer die normbestimmten Teile,
 * der Abgleich gegen Koopman/RevEng. Er prueft **auch nicht** die
 * Unveraendertheit der Datei — das tut das Tor
 * `scripts/audit_dtc_unveraendert.py` (MF-1126) ueber ihre SHA-256.
 * Er prueft genau eine Sache: **dass der Nenner benannt ist und
 * stimmt.** Faellt er, hat sich die fremde Reihe geaendert, und dann
 * gehoert die Zahl hier neu gemessen — nicht angepasst.
 *
 * Der Pfad kommt aus `DTC_TESTQUELLE` (CMake). Ein Pfad als `argv[1]`
 * ueberschreibt ihn; das ist der Weg fuer den Rotbeweis, ohne die
 * echte Datei anzufassen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DTC_TESTQUELLE
#define DTC_TESTQUELLE ""
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    printf("  %-4s %s%s%s\n", ok ? "ok" : "ROT", was,
           detail && *detail ? " - " : "", detail ? detail : "");
    if (ok) gruen++; else rot++;
}

/** Zaehlt, wie oft `muster` in `text` vorkommt. */
static size_t zaehle(const char *text, const char *muster)
{
    const size_t m = strlen(muster);
    size_t n = 0;
    for (const char *p = text; (p = strstr(p, muster)) != NULL; p += m)
        n++;
    return n;
}

/* Die gemessenen Zahlen. Sie stehen hier, weil sie GEMESSEN sind; wer
 * sie aendert, muss neu messen (MF-1077: eine Zahl aendert sich nur,
 * weil eine Messung vorliegt). */
#define SOLL_ASSERTS     15u
#define SOLL_UND          7u
#define SOLL_BEDINGUNGEN (SOLL_ASSERTS + SOLL_UND)   /* 22 */

static const char *LEITSYMBOL[] = {
    "dtc_crc32",          /* CRC-32 gegen den Normpruefwert          */
    "dtc_crc16_ccitt",    /* CRC-16/CCITT gegen den Normpruefwert    */
    "dtc_mfm_encode",     /* MFM-Rundlauf                            */
    "dtc_gcr_encode",     /* Commodore-GCR-Rundlauf                  */
    "dtc_bits_init",      /* Bitpuffer                               */
    "dtc_flux_measure",   /* Flussstatistik                          */
    "dtc_detect_buffer",  /* Formaterkennung                         */
    "dtc_ctraw_write",    /* CT-Raw-Rundlauf                         */
};
#define SOLL_GRUPPEN (sizeof LEITSYMBOL / sizeof LEITSYMBOL[0])   /* 8 */

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *pfad = (argc > 1 && argv[1][0]) ? argv[1] : DTC_TESTQUELLE;

    printf("Der Nenner der fremden dtc-Testreihe (MF-1126)\n");
    printf("==============================================\n");

    if (!pfad || !*pfad) {
        printf("SKIP (DTC_TESTQUELLE nicht gesetzt)\n");
        return 77;
    }

    FILE *f = fopen(pfad, "rb");
    if (!f) {
        printf("SKIP (nicht lesbar: %s)\n", pfad);
        return 77;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        printf("SKIP (seek)\n");
        return 77;
    }
    const long gross = ftell(f);
    if (gross <= 0 || gross > (1L << 20)) {
        fclose(f);
        printf("SKIP (unerwartete Groesse %ld)\n", gross);
        return 77;
    }
    rewind(f);
    char *text = malloc((size_t)gross + 1u);
    if (!text) {
        fclose(f);
        printf("SKIP (Speicher)\n");
        return 77;
    }
    const size_t gelesen = fread(text, 1, (size_t)gross, f);
    fclose(f);
    text[gelesen] = '\0';

    const size_t a = zaehle(text, "assert(");
    const size_t u = zaehle(text, "&&");
    char det[256];

    snprintf(det, sizeof det, "%zu Aufrufe, erwartet %u", a, SOLL_ASSERTS);
    pruefe("die Reihe traegt genau so viele assert-Aufrufe wie gemessen",
           a == SOLL_ASSERTS, det);

    snprintf(det, sizeof det, "%zu Verknuepfungen, erwartet %u", u, SOLL_UND);
    pruefe("und genau so viele &&-Verknuepfungen darin",
           u == SOLL_UND, det);

    snprintf(det, sizeof det, "%zu Bedingungen, erwartet %u",
             a + u, SOLL_BEDINGUNGEN);
    pruefe("macht zusammen den benannten Nenner",
           a + u == SOLL_BEDINGUNGEN, det);

    size_t gefunden = 0, fehlend = 0;
    char fehlt[256];
    fehlt[0] = '\0';
    for (size_t i = 0; i < SOLL_GRUPPEN; i++) {
        if (zaehle(text, LEITSYMBOL[i]) >= 1) {
            gefunden++;
        } else {
            fehlend++;
            if (strlen(fehlt) + strlen(LEITSYMBOL[i]) + 2u < sizeof fehlt) {
                if (fehlt[0]) strcat(fehlt, ", ");
                strcat(fehlt, LEITSYMBOL[i]);
            }
        }
    }
    snprintf(det, sizeof det, "%zu von %zu%s%s", gefunden, SOLL_GRUPPEN,
             fehlend ? ", fehlt: " : "", fehlend ? fehlt : "");
    pruefe("jede der acht Pruefgruppen ist an ihrem Leitsymbol belegt",
           gefunden == SOLL_GRUPPEN, det);

    /* Und die Aussage, um die es eigentlich geht: die fremde Reihe
     * meldet ihren Erfolg ohne Zahl. Das ist hier festgehalten, damit
     * niemand spaeter „all tests passed" fuer eine Auskunft haelt. */
    pruefe("die fremde Reihe meldet ihren Erfolg OHNE Nenner - deshalb "
           "steht er hier",
           zaehle(text, "all tests passed") == 1
           && zaehle(text, "%d") == 0 && zaehle(text, "%zu") == 0,
           "kein Formatplatzhalter in ihrer Erfolgsmeldung");

    free(text);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
