/**
 * @file test_hfe_v3_weak.c
 * @brief HFE v3 weak/fuzzy-bit RAND-opcode detection (MF-354).
 *
 * Unit-tests uft_hfe_v3_count_weak_opcodes() — the opcode-stream walker that
 * counts RAND opcodes (0xF4 = weak/fuzzy-bit region) in a bit-reversed HFE v3
 * track. Opcode set + semantics verified against the HxC HFEv3 loader
 * (jfdelnero/HxCFloppyEmulator .../hfev3_loader.c): every 0xFx byte is an
 * opcode, SETBITRATE (0xF2) and SKIPBITS (0xF3) carry a parameter byte, and
 * literal data bytes are never 0xFx in a valid stream.
 *
 * The crux the walker must get right: a 0xF4 that is only the PARAMETER of a
 * 2-byte opcode must NOT be miscounted as a RAND region. Testing the pure
 * function directly avoids synthesising a full HFE container.
 *
 * Liability-mode note: HFE only APPROXIMATES the original disk (per the HxC
 * author). This test proves the detection logic; a bit-exact per-bit weak_mask
 * is a documented follow-up, not claimed here.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* Non-static symbol from src/formats/hfe/uft_hfe.c */
extern size_t uft_hfe_v3_count_weak_opcodes(const uint8_t *stream, size_t len);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define NOP        0xF0
#define SETINDEX   0xF1
#define SETBITRATE 0xF2
#define SKIPBITS   0xF3
#define RAND       0xF4

TEST(data_only_has_no_weak) {
    /* Ordinary MFM-ish data bytes, none with a 0xFx top nibble. */
    const uint8_t s[] = { 0x4E, 0x4E, 0xA1, 0xA1, 0xFE, 0x00, 0x01, 0xC5 };
    /* 0xFE has top nibble 0xF -> treated as (unknown) opcode, 1 byte, not RAND. */
    ASSERT(uft_hfe_v3_count_weak_opcodes(s, sizeof(s)) == 0);
}

TEST(single_rand_counts_one) {
    const uint8_t s[] = { 0x4E, RAND, 0x4E, 0x4E };
    ASSERT(uft_hfe_v3_count_weak_opcodes(s, sizeof(s)) == 1);
}

TEST(multiple_rand) {
    const uint8_t s[] = { RAND, 0x22, RAND, 0x33, RAND };
    ASSERT(uft_hfe_v3_count_weak_opcodes(s, sizeof(s)) == 3);
}

TEST(rand_as_skipbits_param_not_counted) {
    /* SKIPBITS consumes its next byte as a parameter; a 0xF4 there is data,
     * not a RAND opcode. Walker must skip it. */
    const uint8_t s[] = { SKIPBITS, RAND, 0x4E };
    ASSERT(uft_hfe_v3_count_weak_opcodes(s, sizeof(s)) == 0);
}

TEST(rand_as_setbitrate_param_not_counted) {
    const uint8_t s[] = { SETBITRATE, RAND, 0x4E };
    ASSERT(uft_hfe_v3_count_weak_opcodes(s, sizeof(s)) == 0);
}

TEST(mixed_opcodes) {
    /* NOP, SETINDEX (1 byte), SETBITRATE+param (2), RAND (weak), SKIPBITS+param
     * (2, param happens to be 0xF4), RAND (weak), data. Expect exactly 2. */
    const uint8_t s[] = { NOP, SETINDEX, SETBITRATE, 0x02, RAND,
                          SKIPBITS, RAND, RAND, 0x4E };
    ASSERT(uft_hfe_v3_count_weak_opcodes(s, sizeof(s)) == 2);
}

TEST(empty_stream) {
    ASSERT(uft_hfe_v3_count_weak_opcodes((const uint8_t*)"", 0) == 0);
}

TEST(trailing_two_byte_opcode_no_overrun) {
    /* SKIPBITS as the very last byte (its param is off the end) must not crash
     * or count — the loop's i+=2 simply terminates. */
    const uint8_t s[] = { 0x4E, SKIPBITS };
    ASSERT(uft_hfe_v3_count_weak_opcodes(s, sizeof(s)) == 0);
}

int main(void) {
    printf("=== HFE v3 weak-bit RAND opcode detection (MF-354) ===\n");
    RUN(data_only_has_no_weak);
    RUN(single_rand_counts_one);
    RUN(multiple_rand);
    RUN(rand_as_skipbits_param_not_counted);
    RUN(rand_as_setbitrate_param_not_counted);
    RUN(mixed_opcodes);
    RUN(empty_stream);
    RUN(trailing_two_byte_opcode_no_overrun);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
