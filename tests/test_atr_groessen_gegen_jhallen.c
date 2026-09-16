/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_atr_groessen_gegen_jhallen.c
 * @brief Die drei ATR-Groessen gegen eine fremde, benannte Quelle —
 *        und die DD-Zahl ist die aufschlussreiche (MF-1178)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────────
 *
 * `jhallen/atari-tools`, Commit 835d5a6fc1258921949fe92400adb789c398b9c3
 * (2021-10-22), GPL v1 oder spaeter, (c) 2011 Joseph H. Allen.
 * `readme.md`, Abschnitt „Image formats" — die drei Groessen mit
 * BEGRUENDUNG, und genau die Begruendung ist der Beleg:
 *
 *   SD  92 176 = 16 + 40 x 18 x 128
 *   ED 133 136 = 16 + 40 x 26 x 128
 *   DD 183 952 = 16 + 40 x 18 x 256 **- 384**, „because first three
 *                sectors are short"
 *
 * Dazu zwei Schwellen fuer die Erkennung: alles unter 131 088 gilt als
 * Single Density, alles unter 183 952 als Enhanced Density.
 *
 * Kanal *Spec* nach MF-695: gelesen wurde die Dokumentation, nicht der
 * Code. Warum aus diesem Werkzeug KEIN Differenzlauf wurde, steht unten.
 *
 * ── Warum die DD-Zahl mehr wert ist als die anderen zwei ────────────────
 *
 * 40 x 18 x 256 sind 184 320, plus Kopf 184 336. Die echte Zahl ist
 * 183 952 — **384 Byte weniger**, weil die ersten drei Sektoren einer
 * Atari-Double-Density-Diskette 128 Byte behalten. Wer das nicht weiss,
 * rechnet jeden Sektor ab dem vierten um 384 Byte falsch, und der Fehler
 * ist STILL: die Datei geht auf, die Geometrie sieht richtig aus, und nur
 * der Inhalt sitzt an der falschen Stelle. Genau die Gestalt von MF-1016,
 * MF-1026 und MF-1037.
 *
 * UFT hat das Wissen in `atr_boot_span()`. Dieser Test prueft, dass es
 * benutzt wird — und der Rotbeweis unten rechnet aus, was ohne es
 * herauskaeme.
 *
 * ── Was NICHT geht, und warum (GESTOPPT) ────────────────────────────────
 *
 * Der Auftrag verlangte einen Differenzlauf ATR -> `atr2imd` -> IMD gegen
 * UFTs eigenen Weg. Gemessen ist er NICHT durchfuehrbar, aus zwei
 * unabhaengigen Gruenden:
 *
 *   1. `atr2imd` schreibt keinen ImageDisk-Kopf. Eine echte IMD beginnt
 *      mit der Kennung `IMD ` (im Korpus: `IMD 1.17: 10/08/2026 22:...`);
 *      `atr2imd` schreibt `ATR2IMD 1.0: <Datum>`. UFTs Leser verlangt die
 *      Kennung (`uft_imd_plugin.c:14,34,89`) und antwortete auf die
 *      erzeugte Datei gemessen mit `UFT_ERROR_FORMAT_INVALID`.
 *   2. Jedes Datenbyte ist KOMPLEMENTIERT. `atr2imd.c:284,290,297`
 *      schreibt `~atr->data[...]`, und nur der eigene Partner
 *      `imd2atr.c:311` dreht es mit `buf[y] ^= 0xFF` zurueck. Gemessen:
 *      der Rundlauf des Werkzeugpaars ist byteidentisch (0 von 92 176
 *      abweichend), die Zwischendatei aber fuer jeden anderen IMD-Leser
 *      unbrauchbar. Aus `UFT-ATR S0001` wird `AA B9 AB D2 ...`.
 *
 * Keine der beiden Abweichungen steht in der Dokumentation. Ein Werkzeug,
 * dessen Zwischenformat nur es selbst lesen kann, ist kein Orakel fuer
 * dieses Format — dieselbe Entscheidung wie bei floptools `esq16`
 * (MF-1085), nur aus einem anderen Grund.
 *
 * Was `atari-tools` trotzdem beitraegt und was im Baum landet: die drei
 * Groessen hier, und seine drei **Interleave-Tafeln** (`atr2imd.c:56-65`,
 * Kommentar „Interleave map for 90K/130K/180K disks") — 18 Sektoren
 * `1,3,5,...,17,2,4,...,18`, 26 Sektoren `1,3,...,25,2,4,...,26`. Das ist
 * Interleave 2, und eine solche Aussage hat UFT sonst nirgends. Siehe
 * P3-432.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_atr;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* Die drei Groessen aus jhallens readme, als Literale und NICHT aus UFT
 * gelesen — sonst befragte der Test dieselbe Quelle wie der Pruefling
 * (Tor 64, MF-1000). */
#define ATR_KOPF        16u
#define SD_BYTES    92176u
#define ED_BYTES   133136u
#define DD_BYTES   183952u
/* Und die zwei Erkennungsschwellen derselben Quelle. */
#define SCHWELLE_ED 131088u
#define SCHWELLE_DD 183952u

typedef struct {
    const char *name;
    unsigned sektoren;
    unsigned sgroesse;
    unsigned kurz;        /* Zahl der Sektoren, die 128 Byte behalten */
    unsigned bytes;       /* erwartete Dateigroesse */
} atr_art_t;

static const atr_art_t ARTEN[] = {
    { "SD",  720u, 128u, 0u, SD_BYTES },
    { "ED", 1040u, 128u, 0u, ED_BYTES },
    { "DD",  720u, 256u, 3u, DD_BYTES },
};

/** Baut eine ATR, deren Sektoren sich SELBST BENENNEN. Der Korpus-ATR
 *  hat gemessen EINEN nicht einheitlichen Sektor und 94 Byte ungleich
 *  null von 92 160 — ein Vergleich daran waere eine Gleichheit ohne
 *  Aussage (MF-1021/MF-1039). */
static uint8_t *baue(const atr_art_t *a, size_t *out_len)
{
    const size_t nutz = (size_t)a->sektoren * a->sgroesse
                      - (size_t)a->kurz * (a->sgroesse - 128u);
    const size_t len = ATR_KOPF + nutz;
    uint8_t *d = calloc(1, len);
    if (!d) return NULL;

    const unsigned absaetze = (unsigned)(nutz / 16u);
    d[0] = 0x96; d[1] = 0x02;                   /* 0x0296 little endian */
    d[2] = (uint8_t)(absaetze & 0xFFu);
    d[3] = (uint8_t)((absaetze >> 8) & 0xFFu);
    d[4] = (uint8_t)(a->sgroesse & 0xFFu);
    d[5] = (uint8_t)((a->sgroesse >> 8) & 0xFFu);
    d[6] = (uint8_t)((absaetze >> 16) & 0xFFu);

    size_t off = ATR_KOPF;
    for (unsigned n = 1u; n <= a->sektoren; n++) {
        const unsigned laenge = (n <= a->kurz) ? 128u : a->sgroesse;
        memset(d + off, 0xE5, laenge);
        char marke[16];
        snprintf(marke, sizeof(marke), "UFT-ATR S%04u", n);
        memcpy(d + off, marke, 13);
        off += laenge;
    }
    if (out_len) *out_len = len;
    return d;
}

static bool schreibe(const char *pfad, const uint8_t *d, size_t len)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return false;
    const bool ok = fwrite(d, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

/* ── Die drei Groessen treffen die fremde Quelle ─────────────────────── */

TEST(die_drei_groessen_stimmen_mit_jhallens_rechnung)
{
    /* Erst die Arithmetik der Quelle nachrechnen, dann die Datei bauen.
     * Ohne diesen Schritt waere „die Datei ist so gross, wie ich sie
     * gebaut habe" die einzige Aussage. */
    ASSERT(ATR_KOPF + 40u * 18u * 128u == SD_BYTES);
    ASSERT(ATR_KOPF + 40u * 26u * 128u == ED_BYTES);
    /* Und die DD-Zahl NUR mit den 384 Byte fuer die drei kurzen. */
    ASSERT(ATR_KOPF + 40u * 18u * 256u - 384u == DD_BYTES);
    ASSERT(ATR_KOPF + 40u * 18u * 256u != DD_BYTES);   /* 184 336, falsch */

    /* Die Schwellen sind geordnet und trennen die drei Groessen. */
    ASSERT(SD_BYTES < SCHWELLE_ED);
    ASSERT(ED_BYTES > SCHWELLE_ED);
    ASSERT(ED_BYTES < SCHWELLE_DD);
    ASSERT(DD_BYTES == SCHWELLE_DD);

    for (unsigned i = 0; i < sizeof ARTEN / sizeof ARTEN[0]; i++) {
        size_t len = 0;
        uint8_t *d = baue(&ARTEN[i], &len);
        ASSERT(d != NULL);
        if (len != ARTEN[i].bytes) {
            printf("\n    %s: gebaut %zu Byte, jhallen sagt %u\n",
                   ARTEN[i].name, len, ARTEN[i].bytes);
        }
        ASSERT(len == ARTEN[i].bytes);
        free(d);
    }
}

/* ── UFT liest alle drei an der richtigen Stelle ─────────────────────── */

TEST(uft_liest_alle_drei_an_der_richtigen_stelle)
{
    for (unsigned i = 0; i < sizeof ARTEN / sizeof ARTEN[0]; i++) {
        const atr_art_t *a = &ARTEN[i];
        size_t len = 0;
        uint8_t *d = baue(a, &len);
        ASSERT(d != NULL);

        char pfad[64];
        snprintf(pfad, sizeof(pfad), "uft_jhallen_%s.atr", a->name);
        ASSERT(schreibe(pfad, d, len));
        free(d);

        uft_disk_t disk;
        memset(&disk, 0, sizeof(disk));
        disk.read_only = true;
        const uft_error_t e = uft_format_plugin_atr.open(&disk, pfad, true);
        if (e != UFT_OK) printf("\n    %s: open = %d\n", a->name, (int)e);
        ASSERT(e == UFT_OK);

        const unsigned spt = a->sektoren / 40u;
        ASSERT(disk.geometry.cylinders == 40u);
        ASSERT(disk.geometry.heads == 1u);
        ASSERT(disk.geometry.sectors == spt);
        ASSERT(disk.geometry.sector_size == a->sgroesse);
        ASSERT(disk.geometry.total_sectors == a->sektoren);

        /* Jede Spur, jeder Sektor: die Marke muss die logische Nummer
         * nennen, die an dieser Stelle stehen MUSS. Das ist der
         * Unterschied zu „irgendetwas kam zurueck" (MF-1020). */
        unsigned geprueft = 0u, falsch = 0u;
        for (unsigned cyl = 0; cyl < 40u; cyl++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            ASSERT(uft_format_plugin_atr.read_track(&disk, (int)cyl, 0, &t)
                   == UFT_OK);
            ASSERT(t.sector_count == spt);
            for (unsigned s = 0; s < spt; s++) {
                const unsigned soll = cyl * spt + s + 1u;
                char erwartet[16];
                snprintf(erwartet, sizeof(erwartet), "UFT-ATR S%04u", soll);
                if (!t.sectors[s].data
                    || memcmp(t.sectors[s].data, erwartet, 13) != 0) {
                    if (falsch < 3u) {
                        printf("\n    %s Zyl %u Sekt %u: erwartet \"%s\", "
                               "gelesen \"%.13s\"\n", a->name, cyl, s + 1u,
                               erwartet,
                               t.sectors[s].data
                                   ? (const char *)t.sectors[s].data
                                   : "(NULL)");
                    }
                    falsch++;
                }
                geprueft++;
            }
            for (size_t s = 0; s < t.sector_count; s++) free(t.sectors[s].data);
            free(t.sectors);
            free(t.raw_data);
        }
        uft_format_plugin_atr.close(&disk);
        remove(pfad);

        if (falsch) printf("\n    %s: %u von %u Sektoren an falscher Stelle\n",
                           a->name, falsch, geprueft);
        ASSERT(falsch == 0u);
        ASSERT(geprueft == a->sektoren);
    }
}

/* ── ROT-PROBE (D1): ohne die drei kurzen Bootsektoren ───────────────── */

TEST(rot_probe_ohne_die_drei_kurzen_bootsektoren)
{
    /* Die Falle, vor der jhallens Begruendung warnt: wer bei Double
     * Density jeden Sektor mit 256 Byte rechnet, liegt ab dem vierten um
     * 384 Byte daneben. Hier wird ausgerechnet, WAS dann herauskaeme —
     * am selben Puffer, den der Test gebaut hat, ohne UFT zu fragen. */
    const atr_art_t *dd = &ARTEN[2];
    ASSERT(dd->kurz == 3u && dd->sgroesse == 256u);

    size_t len = 0;
    uint8_t *d = baue(dd, &len);
    ASSERT(d != NULL);

    /* Richtig: Sektor 19 (Spur 1, erster Sektor) beginnt hinter drei
     * kurzen und fuenfzehn vollen Sektoren. */
    const size_t richtig = ATR_KOPF + 3u * 128u + 15u * 256u;   /* 4240 */
    ASSERT(richtig == 4240u);
    ASSERT(memcmp(d + richtig, "UFT-ATR S0019", 13) == 0);

    /* Naiv: 18 Sektoren a 256 Byte. */
    const size_t naiv = ATR_KOPF + 18u * 256u;                  /* 4624 */
    ASSERT(naiv == 4624u);
    ASSERT(naiv - richtig == 384u);

    /* Und die Probe: an der naiven Stelle steht NICHT die Marke von
     * Sektor 19. Ohne diese Zeile waere der Test wertlos, weil er dann
     * auch bestanden waere, wenn beide Stellen dasselbe traegen. */
    if (memcmp(d + naiv, "UFT-ATR S0019", 13) == 0) {
        printf("\n    ROT-PROBE verfehlt: die naive Stelle traegt "
               "ZUFAELLIG dieselbe Marke\n");
    }
    ASSERT(memcmp(d + naiv, "UFT-ATR S0019", 13) != 0);

    /* Was dort wirklich steht, gehoert in den Bericht: die naive Stelle
     * liegt MITTEN in einem Sektor, nicht an dessen Anfang. */
    ASSERT((naiv - ATR_KOPF - 3u * 128u) % 256u == 128u);

    free(d);
}

int main(void)
{
    printf("=== ATR-Groessen gegen jhallen/atari-tools (MF-1178) ===\n");
    RUN(die_drei_groessen_stimmen_mit_jhallens_rechnung);
    RUN(uft_liest_alle_drei_an_der_richtigen_stelle);
    printf("--- ROT-PROBE ---\n");
    RUN(rot_probe_ohne_die_drei_kurzen_bootsektoren);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
