/**
 * @file test_whd_crc16.c
 * @brief Tests for uft_crc16_ansi (CRC-16/IBM reflected, restored from v3.7.0).
 *
 * Parameters: poly 0x8005 (reflected 0xA001), init 0x0000,
 * refin true, refout true, xorout 0x0000. Same as CRC-16/ARC.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "uft/whdload/whd_crc16.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(empty_input_is_zero) {
    ASSERT(uft_crc16_ansi("", 0) == 0x0000);
}

TEST(standard_check_string) {
    /* CRC-16/ARC check value for "123456789" is 0xBB3D. */
    ASSERT(uft_crc16_ansi("123456789", 9) == 0xBB3D);
}

TEST(single_zero_byte) {
    uint8_t z = 0;
    ASSERT(uft_crc16_ansi(&z, 1) == 0x0000);
}

TEST(single_ff_byte) {
    uint8_t f = 0xFF;
    /* CRC-16/ARC of {0xFF} is 0x4040 */
    ASSERT(uft_crc16_ansi(&f, 1) == 0x4040);
}

TEST(differs_per_byte) {
    uint8_t a = 0x01, b = 0x02;
    ASSERT(uft_crc16_ansi(&a, 1) != uft_crc16_ansi(&b, 1));
}

TEST(order_sensitive) {
    ASSERT(uft_crc16_ansi("ab", 2) != uft_crc16_ansi("ba", 2));
}

int main(void) {
    printf("=== whd_crc16 tests ===\n");
    RUN(empty_input_is_zero);
    RUN(standard_check_string);
    RUN(single_zero_byte);
    RUN(single_ff_byte);
    RUN(differs_per_byte);
    RUN(order_sensitive);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
