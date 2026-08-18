/**
 * @file test_platform.c
 * @brief Byte-order and platform facts, tested against the SHIPPED helpers.
 *
 * This file used to reimplement read_le16/read_be32/write_le32 and friends
 * locally and assert against those copies. It therefore could not have caught
 * the TD0 byte-order bug (FMT-13, MF-389), where a constant was swapped
 * relative to what uft_read_le16() actually returns — the very function this
 * test claimed to cover.
 *
 * It now exercises include/uft/uft_endian.h directly: all twelve read/write
 * helpers, with known byte vectors, round trips, and an explicit cross-check
 * that LE and BE readers disagree on asymmetric input. That last one is the
 * property whose absence let FMT-13 through.
 *
 * The old RUN macro printed "OK" and counted a pass unconditionally, so a
 * failing case still looked green in the log (only the exit code was right).
 * Fixed here as well.
 */

#include "uft/uft_endian.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* One asymmetric vector reused everywhere: every byte differs, so a swapped
 * reader cannot accidentally produce the right answer. */
static const uint8_t VEC[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

TEST(read_le_helpers_match_known_vectors) {
    ASSERT(uft_read_le16(VEC) == 0x2211u);
    ASSERT(uft_read_le32(VEC) == 0x44332211u);
    ASSERT(uft_read_le64(VEC) == 0x8877665544332211ull);
}

TEST(read_be_helpers_match_known_vectors) {
    ASSERT(uft_read_be16(VEC) == 0x1122u);
    ASSERT(uft_read_be32(VEC) == 0x11223344u);
    ASSERT(uft_read_be64(VEC) == 0x1122334455667788ull);
}

TEST(le_and_be_readers_disagree_on_asymmetric_input) {
    /* The missing property behind FMT-13: a byte-swapped constant compared
     * against the wrong reader looks plausible until something checks that the
     * two readers are genuinely different functions. */
    ASSERT(uft_read_le16(VEC) != uft_read_be16(VEC));
    ASSERT(uft_read_le32(VEC) != uft_read_be32(VEC));
    ASSERT(uft_read_le64(VEC) != uft_read_be64(VEC));

    /* and they are exact byte reversals of one another */
    ASSERT(uft_read_le16(VEC) == (uint16_t)((uft_read_be16(VEC) >> 8) |
                                            (uft_read_be16(VEC) << 8)));
}

TEST(le_write_then_read_round_trips) {
    uint8_t buf[8];
    memset(buf, 0, sizeof(buf));

    uft_write_le16(buf, 0xBEEFu);
    ASSERT(uft_read_le16(buf) == 0xBEEFu);
    ASSERT(buf[0] == 0xEF && buf[1] == 0xBE);      /* low byte first */

    uft_write_le32(buf, 0xDEADBEEFu);
    ASSERT(uft_read_le32(buf) == 0xDEADBEEFu);
    ASSERT(buf[0] == 0xEF && buf[3] == 0xDE);

    uft_write_le64(buf, 0x0123456789ABCDEFull);
    ASSERT(uft_read_le64(buf) == 0x0123456789ABCDEFull);
    ASSERT(buf[0] == 0xEF && buf[7] == 0x01);
}

TEST(be_write_then_read_round_trips) {
    uint8_t buf[8];
    memset(buf, 0, sizeof(buf));

    uft_write_be16(buf, 0xBEEFu);
    ASSERT(uft_read_be16(buf) == 0xBEEFu);
    ASSERT(buf[0] == 0xBE && buf[1] == 0xEF);      /* high byte first */

    uft_write_be32(buf, 0xDEADBEEFu);
    ASSERT(uft_read_be32(buf) == 0xDEADBEEFu);
    ASSERT(buf[0] == 0xDE && buf[3] == 0xEF);

    uft_write_be64(buf, 0x0123456789ABCDEFull);
    ASSERT(uft_read_be64(buf) == 0x0123456789ABCDEFull);
    ASSERT(buf[0] == 0x01 && buf[7] == 0xEF);
}

TEST(writers_touch_exactly_their_own_width) {
    /* A writer that runs past its width would silently corrupt neighbouring
     * header fields — the kind of damage a forensic tool must never do. */
    uint8_t buf[8];

    memset(buf, 0xCC, sizeof(buf));
    uft_write_le16(buf, 0x0000u);
    ASSERT(buf[2] == 0xCC && buf[7] == 0xCC);

    memset(buf, 0xCC, sizeof(buf));
    uft_write_be32(buf, 0x00000000u);
    ASSERT(buf[4] == 0xCC && buf[7] == 0xCC);
}

TEST(helpers_work_on_unaligned_addresses) {
    /* Disk headers are packed; readers are routinely handed odd offsets. */
    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));
    memcpy(buf + 1, VEC, sizeof(VEC));

    ASSERT(uft_read_le16(buf + 1) == 0x2211u);
    ASSERT(uft_read_be32(buf + 1) == 0x11223344u);

    uft_write_le32(buf + 3, 0xCAFEBABEu);
    ASSERT(uft_read_le32(buf + 3) == 0xCAFEBABEu);
}

TEST(fixed_width_types_have_the_promised_sizes) {
    /* The on-disk structures depend on these; a surprise here would invalidate
     * every offset in every parser. */
    ASSERT(sizeof(uint8_t) == 1);
    ASSERT(sizeof(uint16_t) == 2);
    ASSERT(sizeof(uint32_t) == 4);
    ASSERT(sizeof(uint64_t) == 8);
    ASSERT(sizeof(size_t) >= 4);
}

int main(void) {
    printf("=== Byte order via the shipped helpers (MF-394) ===\n");
    RUN(read_le_helpers_match_known_vectors);
    RUN(read_be_helpers_match_known_vectors);
    RUN(le_and_be_readers_disagree_on_asymmetric_input);
    RUN(le_write_then_read_round_trips);
    RUN(be_write_then_read_round_trips);
    RUN(writers_touch_exactly_their_own_width);
    RUN(helpers_work_on_unaligned_addresses);
    RUN(fixed_width_types_have_the_promised_sizes);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
