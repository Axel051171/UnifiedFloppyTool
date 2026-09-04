/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_st_bootpruefsumme.c
 * @brief Die TOS-Bootpruefsumme, gegen zwei externe Quellen und eine
 *        Gegenspur (MF-873)
 *
 * -- Was hier NICHT behauptet wird -------------------------------------
 *
 * Die erste Fassung dieses Kopfes sagte, `tests/test_st_geometry.c`
 * koenne Big- von Little-Endian nicht unterscheiden, weil sein Erbauer
 * dieselbe Formel rechne wie der Pruefer. **Das ist gemessen falsch.**
 * `build_st()` ruft `st_boot_checksum()` nicht auf, sondern fuehrt eine
 * ZWEITE, unabhaengige Big-Endian-Schreibweise. Wird der Pruefer auf
 * Little-Endian umgestellt, bricht das Paar, und der bestehende Test
 * wird ebenfalls rot.
 *
 * Gemessen mit zwei Mutationen:
 *
 *   1) `st_boot_checksum()` little-endian gerechnet
 *      -> test_st_geometry ROT, dieser Test ROT.
 *         Der bestehende Test genuegt; dieser fuegt nichts hinzu.
 *
 *   2) Die Pruefung nimmt BE ODER LE („aufgeweicht")
 *      -> test_st_geometry GRUEN, dieser Test ROT.
 *
 * Nur der zweite Fall rechtfertigt diese Datei. Er ist keine Erfindung:
 * so entsteht eine nachsichtige Pruefung, wenn jemand einen
 * Fehlerbericht ueber „nicht erkannte Abbilder" bekommt und die
 * Bedingung lockert. Der bestehende Test kann das nicht sehen, weil er
 * nie einen Sektor vorlegt, der die FALSCHE Summe trifft.
 *
 * -- Die Referenz, jetzt dreifach --------------------------------------
 *
 * `docs/VERIFICATION_TIERS.md` fuehrte die Pruefsumme bis MF-873 als
 * „nur bei SAMdisk belegt, nicht doppelt gegengelesen". Zwei
 * unabhaengige Quellen schliessen das:
 *
 *   disktype, Dokumentation §3.3.1 „The GEMDOS File System"
 *     „the boot sector must have a checksum of $1234 (computed from
 *      16-bit words in big-endian byte order)"
 *     https://disktype.sourceforge.net/doc/ch03s03.html
 *     -> benennt Wert, Wortbreite UND Byte-Reihenfolge.
 *
 *   Richard Karsmakers, „The Ultimate Virus Killer Book",
 *   Anhang I „Atari Boot Flowchart", Schritt 9
 *     „the Operating System will load the bootsector off that
 *      particular floppy disk and execute it when the checksum is $1234"
 *     https://st-news.com/uvk-book/the-book/part-iii-appendices/
 *       i-atari-boot-flowchart
 *     -> benennt Wert und Folge, nicht die Mechanik.
 *
 *   src/samdisk/st.cpp:6 (die bisherige, einzige Quelle)
 *
 * disktype ist die tragende der drei, weil es als einzige die
 * Byte-Reihenfolge ausspricht. Das ist der eigentliche Ertrag von
 * MF-873 — nicht dieser Test, sondern dass die Zahl 0x1234 und ihre
 * Leserichtung nicht mehr an einer einzigen fremden Datei haengen.
 *
 * -- Die Gegenspur ------------------------------------------------------
 *
 * Zwei Bootsektoren, die sich NUR im Ausgleichswort an $1FE
 * unterscheiden:
 *
 *   a) Big-Endian-Summe 0x1234, Little-Endian-Summe 0x3611
 *   b) Little-Endian-Summe 0x1234, Big-Endian-Summe 0x3610
 *
 * (b) ist der Sektor, den der bestehende Test nie vorlegt.
 *
 * Dass das letzte Wort den Ausgleich traegt, ist keine Erfindung dieses
 * Tests: die UVK-Quelle nennt es „the evening out value".
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_st;

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-44s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define SEC          512
#define ZIEL      0x1234u
/* 80 x 2 x 9 — eine der sechs Altgroessen, damit der Groessenweg traegt
 * und sich allein die Pruefsumme unterscheidet. */
#define DATEIGROESSE (80u * 2u * 9u * SEC)

/** Grundsektor: 68000 BRA.S plus OEM-Feld, Rest null. */
static void grundsektor(uint8_t *b)
{
    memset(b, 0, SEC);
    b[0] = 0x60; b[1] = 0x1C;
    memcpy(b + 2, "UFTTEST ", 8);
}

/** Summe der 256 Woerter, Big-Endian gelesen. */
static uint16_t summe_be(const uint8_t *b, size_t n)
{
    uint16_t s = 0;
    for (size_t i = 0; i + 1 < n; i += 2)
        s = (uint16_t)(s + (((uint16_t)b[i] << 8) | b[i + 1]));
    return s;
}

/** Summe der 256 Woerter, Little-Endian gelesen. */
static uint16_t summe_le(const uint8_t *b, size_t n)
{
    uint16_t s = 0;
    for (size_t i = 0; i + 1 < n; i += 2)
        s = (uint16_t)(s + ((uint16_t)b[i] | ((uint16_t)b[i + 1] << 8)));
    return s;
}

/** Ausgleichswort an $1FE so setzen, dass die BE-Summe das Ziel trifft. */
static void ausgleich_be(uint8_t *b)
{
    uint16_t w = (uint16_t)(ZIEL - summe_be(b, SEC - 2));
    b[SEC - 2] = (uint8_t)(w >> 8);
    b[SEC - 1] = (uint8_t)w;
}

/** Dasselbe, aber die LE-Summe trifft das Ziel. */
static void ausgleich_le(uint8_t *b)
{
    uint16_t w = (uint16_t)(ZIEL - summe_le(b, SEC - 2));
    b[SEC - 2] = (uint8_t)w;
    b[SEC - 1] = (uint8_t)(w >> 8);
}

static int konfidenz(const uint8_t *b)
{
    int k = -1;
    if (!uft_format_plugin_st.probe(b, SEC, DATEIGROESSE, &k)) return -1;
    return k;
}

TEST(die_beiden_pruefspuren_unterscheiden_wirklich)
{
    /* Ohne diesen Fall koennte die ganze Messung an zwei Sektoren
     * haengen, die sich gar nicht unterscheiden — dann waere jedes
     * Ergebnis unten bedeutungslos. */
    uint8_t a[SEC], c[SEC];
    grundsektor(a); ausgleich_be(a);
    grundsektor(c); ausgleich_le(c);

    ASSERT(summe_be(a, SEC) == ZIEL);
    ASSERT(summe_le(a, SEC) != ZIEL);
    ASSERT(summe_le(c, SEC) == ZIEL);
    ASSERT(summe_be(c, SEC) != ZIEL);
    /* und sie unterscheiden sich AUSSCHLIESSLICH im Ausgleichswort */
    ASSERT(memcmp(a, c, SEC - 2) == 0);
}

TEST(eine_big_endian_summe_gilt)
{
    /* Referenz: disktype §3.3.1 — „16-bit words in big-endian byte
     * order". */
    uint8_t b[SEC];
    grundsektor(b);
    ausgleich_be(b);
    int k = konfidenz(b);
    if (k != 90) {
        printf("\n      BE-Summe 0x%04X: Konfidenz %d, erwartet 90\n      ",
               summe_be(b, SEC), k);
        _fail++;
    }
}

TEST(eine_little_endian_summe_gilt_nicht)
{
    /* DER FALL, DEN NUR DIESE DATEI HAT. Der bestehende Test legt nie
     * einen Sektor vor, der die FALSCHE Summe trifft — er kann deshalb
     * eine nachsichtige Pruefung („BE ODER LE") nicht sehen. Gemessen:
     * unter dieser Mutation bleibt test_st_geometry gruen und dieser
     * Fall wird rot. */
    uint8_t b[SEC];
    grundsektor(b);
    ausgleich_le(b);
    int k = konfidenz(b);
    if (k == 90) {
        printf("\n      LE-Summe 0x%04X (BE 0x%04X) ergab 90 — die "
               "Pruefsumme wird little-endian gerechnet\n      ",
               summe_le(b, SEC), summe_be(b, SEC));
        _fail++;
    }
}

TEST(das_ausgleichswort_entscheidet_allein)
{
    /* Die UVK-Quelle nennt das letzte Wort „the evening out value". Ein
     * einziges veraendertes Byte darin muss die Bootbarkeit kippen —
     * sonst haengt die 90 an etwas anderem als der Pruefsumme. */
    uint8_t b[SEC];
    grundsektor(b);
    ausgleich_be(b);
    ASSERT(konfidenz(b) == 90);

    b[SEC - 1] = (uint8_t)(b[SEC - 1] + 1);
    int k = konfidenz(b);
    if (k == 90) {
        printf("\n      ein Byte im Ausgleichswort geaendert, Summe "
               "0x%04X — trotzdem 90\n      ", summe_be(b, SEC));
        _fail++;
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Atari ST: die Bootpruefsumme ist big-endian (MF-873) ===\n");
    RUN(die_beiden_pruefspuren_unterscheiden_wirklich);
    RUN(eine_big_endian_summe_gilt);
    RUN(eine_little_endian_summe_gilt_nicht);
    RUN(das_ausgleichswort_entscheidet_allein);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
