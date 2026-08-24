/**
 * @file test_sector_recovery_honesty.c
 * @brief Ein Mitteln, das die CRC nicht repariert, darf den Sektor nicht
 *        RECOVERED nennen — und die Rohlesung nicht ueberschreiben.
 *
 * ── Der Fehler ───────────────────────────────────────────────────────────
 *
 * `uft_sector_recover_average()` entschied so
 * (src/recovery/uft_sector_recovery.c, vor diesem Fix Zeile 285 f.):
 *
 *     if (new_crc == sector->data_crc ||
 *         sector->status == UFT_SECTOR_CRC_ERROR) {
 *         memcpy(sector->data, averaged, sector->data_len);
 *         sector->status = UFT_SECTOR_RECOVERED;
 *
 * Die zweite Haelfte des `||` hebt die erste auf: gerufen wird die
 * Funktion genau fuer Sektoren mit CRC-Fehler, die Bedingung feuert
 * also IMMER — auch wenn der Mehrheitsentscheid die CRC nicht getroffen
 * hat. Zwei Verstoesse in einem Zug:
 *
 *   1. Ein weiterhin CRC-falscher Sektor wird als RECOVERED gemeldet
 *      (erfundener Erfolg, Rueckgabewert 0).
 *   2. Die Rohlesung in sector->data wird durch den — nachweislich
 *      nicht verifizierten — Mittelwert ersetzt (stille Veraenderung
 *      von Originaldaten, Prinzip 1).
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Fall 1: Der Mehrheitsentscheid trifft die gespeicherte CRC NICHT.
 *         Erwartet: Rueckgabe != 0, Status bleibt CRC_ERROR, das erste
 *         Byte der Rohlesung bleibt 0xAA. Vor dem Fix: alle drei rot.
 * Fall 2 (Gegenprobe): Der Mehrheitsentscheid trifft die CRC. Erwartet:
 *         RECOVERED, Daten = Mittelwert, read_count = 3. Muss vor wie
 *         nach dem Fix gruen sein — schuetzt davor, den Erfolgsfall
 *         beim Reparieren mit zu zerschneiden.
 */

#include "uft_sector_recovery.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-56s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* CRC-16/CCITT-FALSE (Init 0xFFFF, Polynom 0x1021, MSB zuerst) —
 * unabhaengige Referenz-Implementierung desselben Algorithmus wie das
 * statische crc16() in src/recovery/uft_sector_recovery.c. */
static uint16_t crc16_ref(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
    }
    return crc;
}

enum { LEN = 16 };

/* Rohlesung: Byte 0 = 0xAA. Zwei Zusatzlesungen: Byte 0 = 0x55.
 * Mehrheitsentscheid fuer Byte 0 ist damit 0x55 (2:1), Rest identisch
 * 0x11 — der Mittelwert ist deterministisch gleich `alt`. */
static void make_reads(uint8_t *orig, uint8_t *alt)
{
    memset(orig, 0x11, LEN);
    memset(alt,  0x11, LEN);
    orig[0] = 0xAA;
    alt[0]  = 0x55;
}

TEST(unrepaired_average_is_not_a_recovery)
{
    uint8_t orig[LEN], alt[LEN], data[LEN];
    make_reads(orig, alt);
    memcpy(data, orig, LEN);

    const uint8_t *extra[2] = { alt, alt };

    uft_sector_t sec;
    memset(&sec, 0, sizeof(sec));
    sec.data     = data;
    sec.data_len = LEN;
    sec.status   = UFT_SECTOR_CRC_ERROR;

    /* Gespeicherte CRC absichtlich so waehlen, dass sie WEDER zur
     * Rohlesung NOCH zum Mittelwert passt — das Mitteln kann diesen
     * Sektor nicht reparieren. */
    sec.data_crc = (uint16_t)(crc16_ref(alt, LEN) ^ 0x5A5A);
    ASSERT(sec.data_crc != crc16_ref(orig, LEN));  /* Vorbedingung */
    ASSERT(sec.data_crc != crc16_ref(alt,  LEN));  /* Vorbedingung */

    int ret = uft_sector_recover_average(&sec, extra, 2);

    if (sec.status == UFT_SECTOR_RECOVERED)
        printf("\n        Sektor als RECOVERED gemeldet, obwohl die CRC"
               " des Mittelwerts weiterhin nicht stimmt\n");
    ASSERT(ret != 0);
    ASSERT(sec.status == UFT_SECTOR_CRC_ERROR);

    if (sec.data[0] != 0xAA)
        printf("\n        Rohlesung ueberschrieben: data[0] = 0x%02X"
               " statt 0xAA\n", sec.data[0]);
    ASSERT(sec.data[0] == 0xAA);                 /* Rohlesung erhalten */
    ASSERT(memcmp(sec.data, orig, LEN) == 0);
}

TEST(repaired_average_still_recovers)
{
    uint8_t orig[LEN], alt[LEN], data[LEN];
    make_reads(orig, alt);
    memcpy(data, orig, LEN);

    const uint8_t *extra[2] = { alt, alt };

    uft_sector_t sec;
    memset(&sec, 0, sizeof(sec));
    sec.data     = data;
    sec.data_len = LEN;
    sec.status   = UFT_SECTOR_CRC_ERROR;

    /* Diesmal passt die gespeicherte CRC zum Mehrheitsentscheid: die
     * Rohlesung hatte einen Ein-Byte-Fehler, zwei Zusatzlesungen sind
     * sauber — das ist der Fall, fuer den die Funktion existiert. */
    sec.data_crc = crc16_ref(alt, LEN);

    int ret = uft_sector_recover_average(&sec, extra, 2);

    ASSERT(ret == 0);
    ASSERT(sec.status == UFT_SECTOR_RECOVERED);
    ASSERT(memcmp(sec.data, alt, LEN) == 0);     /* Mittelwert uebernommen */
    ASSERT(sec.read_count == 3);
    ASSERT(sec.confidence > 0);
}

int main(void)
{
    printf("=== RECOVERED heisst: die CRC stimmt wieder ===\n");
    RUN(unrepaired_average_is_not_a_recovery);
    RUN(repaired_average_still_recovers);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
