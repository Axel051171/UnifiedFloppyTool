/**
 * @file test_recovery.c
 * @brief CRC-16 and single-bit CRC correction, against the SHIPPED code (MF-397).
 *
 * This file used to define calc_crc16(), vote_bit() and fix_crc_single_bit()
 * locally and assert against those copies. The recovery code in src/ was never
 * called by it — so a divergence between test and production was invisible,
 * the same pattern that hid the TD0 byte-order bug (FMT-13).
 *
 * The expectations come from the published CRC-16/CCITT-FALSE check value
 * (init 0xFFFF, polynomial 0x1021, MSB-first, no final XOR: "123456789" gives
 * 0x29B1), i.e. from outside this codebase.
 *
 * Bit voting across revolutions is deliberately NOT covered here: the pipeline
 * exposes no callable voting function today, and its per-sector voting is an
 * open TODO (src/recovery/uft_multiread_pipeline.c:436). Writing a test
 * against a local copy of it would recreate exactly the problem this change
 * removes.
 */

#include "uft/uft_god_mode.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Defined in src/algorithms/advanced/uft_crc_correction_v2.c */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(crc16_matches_the_published_ccitt_false_check_value) {
    const uint8_t v[] = "123456789";
    ASSERT(crc16_ccitt(v, 9) == 0x29B1u);
}

TEST(crc16_of_empty_input_is_the_init_value) {
    ASSERT(crc16_ccitt((const uint8_t *)"", 0) == 0xFFFFu);
}

TEST(crc16_detects_every_single_bit_error) {
    /* The property a sector CRC exists for. Checked over all 128 bit
     * positions of a 16-byte payload, not one sample. */
    uint8_t buf[16];
    memset(buf, 0xA5, sizeof(buf));
    uint16_t base = crc16_ccitt(buf, sizeof(buf));

    for (size_t byte = 0; byte < sizeof(buf); byte++) {
        for (int bit = 0; bit < 8; bit++) {
            buf[byte] ^= (uint8_t)(1u << bit);
            ASSERT(crc16_ccitt(buf, sizeof(buf)) != base);
            buf[byte] ^= (uint8_t)(1u << bit);
        }
    }
}

TEST(crc16_is_order_and_length_sensitive) {
    const uint8_t ab[] = { 0x12, 0x34 };
    const uint8_t ba[] = { 0x34, 0x12 };
    ASSERT(crc16_ccitt(ab, 2) != crc16_ccitt(ba, 2));

    uint8_t zeros[8];
    memset(zeros, 0, sizeof(zeros));
    ASSERT(crc16_ccitt(zeros, 4) != crc16_ccitt(zeros, 8));
}

TEST(intact_block_is_accepted_without_being_altered) {
    /* A block whose trailing CRC already matches must be reported as fine and
     * must come back byte-identical. Silent "repair" of good data would be a
     * forensic-integrity violation, not a convenience. */
    uint8_t blk[18];
    for (size_t i = 0; i < 16; i++) blk[i] = (uint8_t)(i * 3 + 1);
    uint16_t crc = crc16_ccitt(blk, 16);
    blk[16] = (uint8_t)(crc >> 8);
    blk[17] = (uint8_t)(crc & 0xFF);

    uint8_t before[18];
    memcpy(before, blk, sizeof(blk));

    uft_crc_correction_t r;
    memset(&r, 0, sizeof(r));
    ASSERT(uft_crc_correct(blk, sizeof(blk), 0, &r));
    ASSERT(r.corrected);
    ASSERT(r.bit_position == -1);          /* nothing needed changing */
    ASSERT(memcmp(before, blk, sizeof(blk)) == 0);
}

TEST(single_bit_error_is_located_and_repaired) {
    uint8_t blk[18];
    for (size_t i = 0; i < 16; i++) blk[i] = (uint8_t)(i * 7 + 5);
    uint16_t crc = crc16_ccitt(blk, 16);
    blk[16] = (uint8_t)(crc >> 8);
    blk[17] = (uint8_t)(crc & 0xFF);

    uint8_t original[18];
    memcpy(original, blk, sizeof(blk));

    blk[5] ^= 0x08;                         /* flip one known bit */

    uft_crc_correction_t r;
    memset(&r, 0, sizeof(r));
    ASSERT(uft_crc_correct(blk, sizeof(blk), 0, &r));
    ASSERT(r.corrected);
    ASSERT(r.bit_position >= 0);            /* a position was identified */
    ASSERT(memcmp(original, blk, sizeof(blk)) == 0);  /* and it was restored */
}

TEST(rejects_degenerate_arguments) {
    uint8_t blk[18];
    memset(blk, 0, sizeof(blk));
    uft_crc_correction_t r;

    ASSERT(!uft_crc_correct(NULL, sizeof(blk), 0, &r));
    ASSERT(!uft_crc_correct(blk, 2, 0, &r));      /* smaller than the CRC itself */
    ASSERT(!uft_crc_correct(blk, sizeof(blk), 0, NULL));
}

int main(void) {
    printf("=== CRC-16 + single-bit correction, real code (MF-397) ===\n");
    RUN(crc16_matches_the_published_ccitt_false_check_value);
    RUN(crc16_of_empty_input_is_the_init_value);
    RUN(crc16_detects_every_single_bit_error);
    RUN(crc16_is_order_and_length_sensitive);
    RUN(intact_block_is_accepted_without_being_altered);
    RUN(single_bit_error_is_located_and_repaired);
    RUN(rejects_degenerate_arguments);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
