/**
 * @file test_woz_writer.c
 * @brief WOZ CRC-32, tested against the SHIPPED implementation (MF-396).
 *
 * This file used to define init_crc() and woz_crc() locally and assert against
 * those. The real woz_crc32() in src/formats/apple/uft_woz.c was never called.
 *
 * The CRC guards the WOZ chunk payload: Applesauce writes it into the header
 * and a reader is supposed to reject an image whose data does not match. A
 * wrong CRC therefore either rejects good dumps or accepts corrupted ones —
 * both unacceptable for a preservation tool, and both invisible to a test that
 * checks a private copy of the algorithm.
 *
 * The expectations are the standard CRC-32 (IEEE 802.3, reflected, init and
 * final XOR 0xFFFFFFFF) known-answer vectors, not values read back out of this
 * implementation.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Declared here because uft_woz.c exposes it without a public header entry. */
uint32_t woz_crc32(uint32_t crc, const uint8_t *data, size_t size);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(matches_the_standard_crc32_check_value) {
    /* The canonical CRC-32 check value: "123456789" -> 0xCBF43926.
     * This is the published constant for the algorithm, so it tests the
     * implementation rather than describing it. */
    const uint8_t v[] = "123456789";
    ASSERT(woz_crc32(0, v, 9) == 0xCBF43926u);
}

TEST(empty_input_yields_the_identity_value) {
    ASSERT(woz_crc32(0, (const uint8_t *)"", 0) == 0u);
}

TEST(single_byte_vectors_match_the_standard) {
    const uint8_t a = 'a';
    ASSERT(woz_crc32(0, &a, 1) == 0xE8B7BE43u);   /* CRC-32("a") */
    const uint8_t zero = 0x00;
    ASSERT(woz_crc32(0, &zero, 1) == 0xD202EF8Du); /* CRC-32("\0") */
}

TEST(a_single_flipped_bit_changes_the_result) {
    /* The whole point of the checksum: a one-bit difference must not slip
     * through. Checked across every bit of a small payload. */
    uint8_t buf[8];
    memset(buf, 0x5A, sizeof(buf));
    uint32_t base = woz_crc32(0, buf, sizeof(buf));

    for (size_t byte = 0; byte < sizeof(buf); byte++) {
        for (int bit = 0; bit < 8; bit++) {
            buf[byte] ^= (uint8_t)(1u << bit);
            ASSERT(woz_crc32(0, buf, sizeof(buf)) != base);
            buf[byte] ^= (uint8_t)(1u << bit);   /* restore */
        }
    }
}

TEST(order_matters) {
    const uint8_t ab[] = { 'A', 'B' };
    const uint8_t ba[] = { 'B', 'A' };
    ASSERT(woz_crc32(0, ab, 2) != woz_crc32(0, ba, 2));
}

TEST(length_matters_for_trailing_zeros) {
    /* A CRC that ignored trailing zero bytes would accept a truncated chunk. */
    uint8_t buf[8];
    memset(buf, 0, sizeof(buf));
    ASSERT(woz_crc32(0, buf, 4) != woz_crc32(0, buf, 8));
}

int main(void) {
    printf("=== WOZ CRC-32, real implementation (MF-396) ===\n");
    RUN(matches_the_standard_crc32_check_value);
    RUN(empty_input_yields_the_identity_value);
    RUN(single_byte_vectors_match_the_standard);
    RUN(a_single_flipped_bit_changes_the_result);
    RUN(order_matters);
    RUN(length_matters_for_trailing_zeros);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
