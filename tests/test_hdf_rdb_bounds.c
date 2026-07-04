/**
 * @file test_hdf_rdb_bounds.c
 * @brief RDB parser must reject a too-small SummedLongs (heap-overflow guard).
 *
 * Links the real Amiga RDB parser (src/formats/uft_hdf_parser.c). The parser
 * mallocs size*4 bytes (size = SummedLongs from the file) then zeroes the
 * checksum field at temp[8..11]. If size < 3 that write lands past the
 * allocation — a heap buffer overflow triggerable by a malformed HDF with a
 * valid "RDSK" magic and size 0/1/2 (MF-326). This test feeds exactly those
 * and asserts the parser rejects them (returns -1) instead of taking the
 * overflowing path, while a well-formed SummedLongs still parses.
 */

#include "uft_hdf_parser.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

TEST(rejects_tiny_summedlongs) {
    uint8_t buf[256];
    uft_rdb_info_t rdb;
    for (uint32_t sz = 0; sz < 3; sz++) {
        memset(buf, 0, sizeof(buf));
        put_be32(buf, UFT_RDB_MAGIC);   /* valid magic */
        put_be32(buf + 4, sz);          /* malformed SummedLongs 0/1/2 */
        /* Must be rejected, not run the temp[8..11] overflow path. */
        ASSERT(uft_hdf_parse_rdb(buf, sizeof(buf), &rdb) == -1);
    }
}

TEST(accepts_wellformed_summedlongs) {
    uint8_t buf[256];
    uft_rdb_info_t rdb;
    memset(buf, 0, sizeof(buf));
    put_be32(buf, UFT_RDB_MAGIC);
    put_be32(buf + 4, 64);              /* standard RDB SummedLongs */
    ASSERT(uft_hdf_parse_rdb(buf, sizeof(buf), &rdb) == 0);
    ASSERT(rdb.valid == true);
}

TEST(rejects_oversized_and_short) {
    uint8_t buf[256];
    uft_rdb_info_t rdb;
    memset(buf, 0, sizeof(buf));
    put_be32(buf, UFT_RDB_MAGIC);
    put_be32(buf + 4, 1000);            /* size > len/4 -> reject */
    ASSERT(uft_hdf_parse_rdb(buf, sizeof(buf), &rdb) == -1);
    /* short buffer rejected up front */
    ASSERT(uft_hdf_parse_rdb(buf, 100, &rdb) == -1);
}

int main(void) {
    printf("=== HDF/RDB parser bounds tests ===\n");
    RUN(rejects_tiny_summedlongs);
    RUN(accepts_wellformed_summedlongs);
    RUN(rejects_oversized_and_short);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
