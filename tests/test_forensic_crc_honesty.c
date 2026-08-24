/**
 * @file test_forensic_crc_honesty.c
 * @brief `crc_valid` war per Konstruktion wahr — eine Tautologie statt
 *        einer Pruefung.
 *
 * ── Der Fehler ───────────────────────────────────────────────────────────
 *
 * `uft_forensic_recover_sector()` schloss die CRC-Pruefung so ab
 * (src/recovery/uft_forensic_recovery.c, vor diesem Fix Zeile 876 ff.):
 *
 *     out_sector->crc_computed = crc16_ccitt(out_sector->data, sector_size);
 *     // In real implementation, crc_stored would come from sector header
 *     out_sector->crc_stored = out_sector->crc_computed;  // Placeholder
 *     out_sector->crc_valid = (out_sector->crc_computed == out_sector->crc_stored);
 *
 * Ein Wert, der eben erst auf sich selbst kopiert wurde, ist immer gleich
 * sich selbst: `crc_valid` war fuer JEDEN Sektor wahr, egal was in den
 * Bitstroemen stand. Folgen im selben Funktionskoerper:
 *
 *   - jeder Sektor bekam UFT_FSEC_FLAG_DATA_OK | UFT_FSEC_FLAG_CRC_OK;
 *   - der Korrekturpfad (`if (!crc_valid)`) war toter Code;
 *   - `total_sectors_partial` und `total_sectors_failed` waren
 *     unerreichbar, `total_sectors_perfect` zaehlte jeden Sektor mit
 *     hoher Konfidenz;
 *   - eine Ebene hoeher zaehlte `uft_forensic_recover_track()` jeden
 *     gefundenen Sektor als wiederhergestellt.
 *
 * ── Warum der Fix "unbekannt" heisst und nicht "Header-CRC lesen" ────────
 *
 * Die Funktion bekommt laut Signatur nur rohe, undekodierte Bitstroeme —
 * keinen Sektorkopf, keine Rahmung, kein Format. Die auf der Diskette
 * gespeicherte CRC erreicht sie schlicht nie. Sie aus einer angenommenen
 * Bit-Position hinter den Daten zu "lesen" hiesse, eine Rahmung zu
 * erfinden (EINFRIER-REGEL: kein Decoder-Verhalten ohne benannte
 * Referenz). Die ehrliche Aussage ist daher:
 *
 *     crc_computed = echte Messung ueber die dekodierte Nutzlast
 *     crc_stored   = 0      (Sentinel: nicht verfuegbar)
 *     crc_valid    = false  (Bedeutung: NICHT VERIFIZIERT)
 *
 * und KEIN Aufruf von uft_forensic_correct_sector(): der korrigiert Bits
 * in Richtung `crc_stored` — mit fabriziertem Ziel waere das aktive
 * Datenerfindung (Prinzip 1).
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Drei identische Lesedurchgaenge ohne jede Header-Information duerfen
 * NICHT als CRC-verifiziert gelten. Vor dem Fix ist jede dieser
 * Zusicherungen rot; `crc_computed` muss dabei eine echte Messung
 * bleiben (Gegenprobe gegen eine unabhaengige CRC-Implementierung).
 */

#include "uft/recovery/uft_forensic_types.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-56s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* CRC-16/CCITT-FALSE (Init 0xFFFF, Polynom 0x1021, MSB zuerst) —
 * unabhaengige Referenz-Implementierung desselben Algorithmus wie
 * crc16_ccitt() in src/recovery/uft_forensic_recovery.c. */
static uint16_t crc16_ccitt_ref(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
    }
    return crc;
}

/* recover_sector verlangt sector_size*8 + 32 Bits pro Durchgang
 * (uft_forensic_recovery.c: needed_bits). 128*8 + 32 = 1056 Bits
 * = 132 Bytes. */
/* MF-542: `FCH_SEC_SIZE` heisst in tests/test_cqm_layout.c 512 und war hier
 * 128 — zwei Bedeutungen unter einem Namen. Heute sieht keine
 * Uebersetzungseinheit beide, aber das Tor `enum vs macro conflicts`
 * verbietet die Bauart zu Recht: ein spaeterer gemeinsamer Header
 * entscheidet die Bedeutung still. Der Name ist hier ohnehin
 * praeziser, wenn er sagt, WESSEN Groesse gemeint ist. */
enum { FCH_SEC_SIZE = 128, PASS_BYTES = 132, PASS_BITS = 1056 };

static void sector_dispose(uft_forensic_sector_t *sec)
{
    free(sec->data);
    free(sec->confidence_map);
    free(sec->source_pass);
    free(sec->errors);
    memset(sec, 0, sizeof(*sec));
}

static int recover_three_identical_passes(uft_forensic_session_t *s,
                                          uft_forensic_sector_t *sec,
                                          uint8_t *buf /* PASS_BYTES */)
{
    for (size_t i = 0; i < PASS_BYTES; i++)
        buf[i] = (uint8_t)(0xA5 ^ i);

    const uint8_t *passes[3]    = { buf, buf, buf };
    const size_t   bit_counts[3] = { PASS_BITS, PASS_BITS, PASS_BITS };

    memset(s, 0, sizeof(*s));
    uft_forensic_config_t cfg = uft_forensic_config_default();
    if (uft_forensic_session_init(s, &cfg) != 0) return -1;
    s->config.verbosity = 0;  /* Testlauf still halten */

    memset(sec, 0, sizeof(*sec));
    return uft_forensic_recover_sector(passes, bit_counts, 3,
                                       0, 0, 1, FCH_SEC_SIZE, s, sec);
}

TEST(no_header_crc_means_no_crc_verdict)
{
    uft_forensic_session_t s;
    uft_forensic_sector_t sec;
    uint8_t buf[PASS_BYTES];

    ASSERT(recover_three_identical_passes(&s, &sec, buf) == 0);

    /* Kernaussage: ohne gespeicherte CRC gibt es kein "CRC OK".
     * Vor dem Fix: crc_stored = crc_computed -> crc_valid immer true. */
    if (sec.crc_valid)
        printf("\n        crc_valid ist true, obwohl nie eine Header-CRC"
               " uebergeben wurde (Tautologie)\n");
    ASSERT(sec.crc_valid == false);
    ASSERT((sec.flags & UFT_FSEC_FLAG_CRC_OK)  == 0);
    ASSERT((sec.flags & UFT_FSEC_FLAG_DATA_OK) == 0);
    ASSERT(sec.quality.checksum == 0.0f);

    /* Zaehler: ein unverifizierter Sektor ist niemals "perfect"
     * (uft_forensic_types.h: perfect = CRC OK). Mit einstimmigen
     * Durchgaengen (Konfidenz 1.0 > 0.5) gehoert er zu "partial". */
    ASSERT(s.total_sectors_found == 1);
    ASSERT(s.total_sectors_perfect == 0);
    ASSERT(s.total_sectors_partial == 1);

    /* Gegenprobe gegen den naiven Alternativ-Fix (crc_valid=false setzen,
     * aber den Korrekturpfad weiterlaufen lassen): Korrektur in Richtung
     * eines fabrizierten crc_stored=0 waere Datenerfindung. */
    ASSERT(sec.quality.corrections_applied == 0);
    ASSERT(s.total_corrections == 0);
    ASSERT((sec.flags & UFT_FSEC_FLAG_RECOVERED) == 0);
    ASSERT(s.total_sectors_recovered == 0);

    sector_dispose(&sec);
    uft_forensic_session_finish(&s);
}

TEST(crc_computed_stays_a_real_measurement)
{
    uft_forensic_session_t s;
    uft_forensic_sector_t sec;
    uint8_t buf[PASS_BYTES];

    ASSERT(recover_three_identical_passes(&s, &sec, buf) == 0);

    /* Drei identische Durchgaenge, MSB-zuerst-Dekodierung: die Nutzlast
     * muss den ersten FCH_SEC_SIZE Eingabebytes entsprechen, crc_computed
     * der unabhaengig gerechneten CRC darueber. crc_stored bleibt der
     * 0-Sentinel "nicht verfuegbar". */
    ASSERT(sec.data != NULL);
    ASSERT(memcmp(sec.data, buf, FCH_SEC_SIZE) == 0);
    ASSERT(sec.crc_computed == crc16_ccitt_ref(buf, FCH_SEC_SIZE));
    ASSERT(sec.crc_stored == 0);

    sector_dispose(&sec);
    uft_forensic_session_finish(&s);
}

int main(void)
{
    printf("=== crc_valid ist eine Pruefung, keine Tautologie ===\n");
    RUN(no_header_crc_means_no_crc_verdict);
    RUN(crc_computed_stays_a_real_measurement);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
