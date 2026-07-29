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

#include <stdbool.h>
#include <stdlib.h>

/* Non-static symbols from src/formats/hfe/uft_hfe.c */
extern size_t uft_hfe_v3_count_weak_opcodes(const uint8_t *stream, size_t len);
extern size_t hfe_v3_decode(const uint8_t *in, size_t in_len,
                            uint8_t **out_bits, bool **out_weak,
                            size_t *rand_count);

static int getbit(const uint8_t *b, size_t i) { return (b[i >> 3] >> (7 - (i & 7))) & 1; }

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

/* ── Full decode: opcode stream -> bitstream + per-bit weak_mask (MF-362) ── */

TEST(decode_bits_and_weak_mask) {
    /* data 0xB4 | RAND | NOP | SKIPBITS 3 | data 0x2D(skip3) | SETBITRATE | data 0x5A */
    const uint8_t in[] = { 0xB4, RAND, NOP, SKIPBITS, 0x03, 0x2D, SETBITRATE, 0x10, 0x5A };
    uint8_t *bits = NULL; bool *weak = NULL; size_t rc = 0;
    size_t n = hfe_v3_decode(in, sizeof(in), &bits, &weak, &rc);

    /* 8 (data) + 8 (RAND) + 5 (0x2D bits[3..7]) + 8 (data) = 29 bits, 1 RAND */
    ASSERT(n == 29);
    ASSERT(rc == 1);
    ASSERT(bits && weak);

    /* 0xB4 = 1011 0100, weak-free */
    ASSERT(getbit(bits,0)==1 && getbit(bits,2)==1 && getbit(bits,3)==1 && getbit(bits,4)==0);
    for (int i = 0; i < 8; i++) ASSERT(weak[i] == false);
    /* RAND region: 8 weak bits, value placeholder 0 */
    for (int i = 8; i < 16; i++) { ASSERT(weak[i] == true); ASSERT(getbit(bits,i) == 0); }
    /* 0x2D = 0010 1101, bits [3..7] = 0,1,1,0,1 emitted at 16..20, weak-free */
    ASSERT(getbit(bits,16)==0 && getbit(bits,17)==1 && getbit(bits,18)==1 &&
           getbit(bits,19)==0 && getbit(bits,20)==1);
    for (int i = 16; i < 21; i++) ASSERT(weak[i] == false);
    /* 0x5A = 0101 1010 emitted at 21..28, weak-free */
    ASSERT(getbit(bits,21)==0 && getbit(bits,22)==1 && getbit(bits,28)==0);
    for (int i = 21; i < 29; i++) ASSERT(weak[i] == false);

    free(bits); free(weak);
}

TEST(decode_data_only_no_weak) {
    const uint8_t in[] = { 0x4E, 0xA1, 0x00, 0x55 };
    uint8_t *bits = NULL; bool *weak = NULL; size_t rc = 0;
    size_t n = hfe_v3_decode(in, sizeof(in), &bits, &weak, &rc);
    ASSERT(n == 32 && rc == 0);
    for (int i = 0; i < 32; i++) ASSERT(weak[i] == false);
    ASSERT(bits[0]==0x4E && bits[1]==0xA1 && bits[2]==0x00 && bits[3]==0x55);
    free(bits); free(weak);
}

int main(void) {
    printf("=== HFE v3 weak-bit RAND opcode detection + decode (MF-354/362) ===\n");
    RUN(data_only_has_no_weak);
    RUN(single_rand_counts_one);
    RUN(multiple_rand);
    RUN(rand_as_skipbits_param_not_counted);
    RUN(rand_as_setbitrate_param_not_counted);
    RUN(mixed_opcodes);
    RUN(empty_stream);
    RUN(trailing_two_byte_opcode_no_overrun);
    RUN(decode_bits_and_weak_mask);
    RUN(decode_data_only_no_weak);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
