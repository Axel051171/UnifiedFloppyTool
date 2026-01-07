/**
 * @file test_crc.c
 * @brief CRC Unit Tests
 */

#include "uft_test.h"
#include <stdint.h>

/* CRC-16 CCITT (used by many floppy formats) */
static uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

UFT_TEST(test_crc16_empty) {
    uint8_t data[] = {};
    uint16_t crc = crc16_ccitt(data, 0);
    UFT_ASSERT_EQ(crc, 0xFFFF);
}

UFT_TEST(test_crc16_single_byte) {
    uint8_t data[] = {0x00};
    uint16_t crc = crc16_ccitt(data, 1);
    UFT_ASSERT_EQ(crc, 0xE1F0);
}

UFT_TEST(test_crc16_known_value) {
    uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint16_t crc = crc16_ccitt(data, 9);
    UFT_ASSERT_EQ(crc, 0x29B1);  /* Known CRC-16 CCITT for "123456789" */
}

UFT_TEST(test_crc16_sector_header) {
    /* Typical MFM sector header: FE C H R N */
    uint8_t header[] = {0xFE, 0x00, 0x00, 0x01, 0x02};
    uint16_t crc = crc16_ccitt(header, 5);
    /* Just verify it produces a valid result */
    UFT_ASSERT(crc != 0);
}

UFT_TEST_MAIN_BEGIN()
    printf("CRC Tests:\n");
    UFT_RUN_TEST(test_crc16_empty);
    UFT_RUN_TEST(test_crc16_single_byte);
    UFT_RUN_TEST(test_crc16_known_value);
    UFT_RUN_TEST(test_crc16_sector_header);
UFT_TEST_MAIN_END()
