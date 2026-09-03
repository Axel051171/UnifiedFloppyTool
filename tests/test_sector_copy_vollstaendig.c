/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_sector_copy_vollstaendig.c
 * @brief Eine Kopie, die Felder verliert, ist eine stille Aenderung (MF-832).
 *
 * ── Woher der Verdacht kam ───────────────────────────────────────────────
 *
 * Bei P3-59 wurde gemessen, dass die sechs Positionsfelder auf
 * `uft_sector_t` von keinem Erzeuger gefuellt werden. Beim Nachsehen, wer
 * sie wenigstens WEITERREICHT, fiel `uft_sector_copy()` auf: sie zaehlt
 * **neun Skalarfelder von Hand auf**
 *
 *     id, crc_stored, crc_calculated, crc_ok, error,
 *     retry_count, bit_offset, byte_offset, confidence
 *
 * und kopiert dann die vier eigenen Puffer. Alles andere faellt weg.
 *
 * Das ist die Aufzaehlung-statt-Messung, die dieser Baum schon mehrfach
 * teuer bezahlt hat — nur diesmal an Nutzdaten statt an einer Kennzahl:
 * jedes Feld, das nach dem Schreiben dieser Funktion hinzukam, wird beim
 * Kopieren still verworfen. Betroffen sind unter anderem
 *
 *     angular_position / has_angular_position   MF-474
 *     id_crc_ok                                 ID-CRC getrennt vom Daten-CRC
 *     gap_before                                Zwischenraum vor dem Sektor
 *
 * `uft_sector_copy()` ist nicht tot: `uft_track_copy()`
 * (`uft_unified_types.c:358`) ruft sie fuer JEDEN Sektor auf. Eine kopierte
 * Spur verliert also die Winkelposition eines ATX-Sektors und meldet
 * anschliessend `has_angular_position == false` — „nicht gemessen", wo
 * gemessen wurde. Fuer ein Werkzeug mit dem Grundsatz „Keine stille
 * Veraenderung" ist das ein Befund, kein Schoenheitsfehler.
 *
 * ── Warum der Test so gebaut ist ─────────────────────────────────────────
 *
 * Er prueft NICHT eine Liste von Feldnamen — das waere derselbe Fehler
 * eine Ebene hoeher. Er fuellt die Struktur byteweise mit einem Muster,
 * kopiert, und vergleicht anschliessend ALLES ausser den vier
 * Zeigerfeldern und den von ihnen abhaengigen Laengen. Ein kuenftig
 * hinzugefuegtes Feld ist damit automatisch mit abgedeckt.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_types.h"

static int _pass = 0, _fail = 0;
#define RUN(n)  do { printf("  [TEST] %-44s ... ", #n); test_##n(); \
                     printf("\n"); } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)
#define DONE()  do { printf("OK"); _pass++; } while (0)

/* Die vier Zeigerfelder und die zwei Laengen, die von ihnen abhaengen,
 * duerfen sich unterscheiden — sie werden bewusst neu angelegt. */
static bool ist_zeigerfeld(size_t off)
{
    uft_sector_t s;
    const size_t p[] = {
        offsetof(uft_sector_t, data),
        offsetof(uft_sector_t, confidence_map),
        offsetof(uft_sector_t, weak_mask),
        offsetof(uft_sector_t, timing_ns),
    };
    (void)s;
    for (size_t i = 0; i < sizeof(p) / sizeof(p[0]); i++)
        if (off >= p[i] && off < p[i] + sizeof(void *)) return true;
    return false;
}

TEST(kopie_verliert_die_winkelposition)
{
    uft_sector_t a, b;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);

    /* MF-474: gemessen, und das Flag sagt es. */
    a.angular_position     = 0.375;
    a.has_angular_position = true;
    a.id_crc_ok            = true;
    a.gap_before           = 4242u;
    a.bit_offset           = 99u;      /* Kontrolle: wird kopiert */

    ASSERT(uft_sector_copy(&b, &a) == 0);

    ASSERT(b.bit_offset == 99u);              /* Kontrolle haelt */
    ASSERT(b.has_angular_position == true);   /* das Flag ueberlebt */
    ASSERT(b.angular_position == 0.375);      /* der Wert ueberlebt */
    ASSERT(b.id_crc_ok  == true);
    ASSERT(b.gap_before == 4242u);
    DONE();
}

TEST(kopie_haelt_JEDES_feld_ausser_den_puffern)
{
    /* Byteweises Muster statt einer Feldliste: was kuenftig hinzukommt,
     * ist automatisch mit gedeckt. */
    uft_sector_t a, b;
    unsigned char *pa = (unsigned char *)&a;
    for (size_t i = 0; i < sizeof a; i++)
        pa[i] = (unsigned char)(0x40u + (i % 0x30u));

    /* Zeiger auf NULL: sonst wuerde die Kopierfunktion Muellzeiger
     * dereferenzieren. Die zugehoerigen Laengen ebenfalls auf 0. */
    a.data = NULL; a.confidence_map = NULL;
    a.weak_mask = NULL; a.timing_ns = NULL;
    a.data_len = 0; a.timing_count = 0;

    memset(&b, 0, sizeof b);
    ASSERT(uft_sector_copy(&b, &a) == 0);

    const unsigned char *pb = (const unsigned char *)&b;
    size_t abweichend = 0, erste = (size_t)-1;
    for (size_t i = 0; i < sizeof a; i++) {
        if (ist_zeigerfeld(i)) continue;
        if (pa[i] != pb[i]) {
            if (erste == (size_t)-1) erste = i;
            abweichend++;
        }
    }
    if (abweichend) {
        printf("FAIL: %zu Byte verloren, erstes bei Offset %zu "
               "(Struktur %zu Byte)\n", abweichend, erste, sizeof a);
        _fail++;
        return;
    }
    DONE();
}

int main(void)
{
    printf("test_sector_copy_vollstaendig (MF-832)\n");
    RUN(kopie_verliert_die_winkelposition);
    RUN(kopie_haelt_JEDES_feld_ausser_den_puffern);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
