/*
 * test_sector_parser.c - Unit tests for UFT Sector Parser
 */

#include "uft/uft_sector_parser.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test CRC16 calculation */
static void test_crc16(void) {
    printf("Testing CRC16... ");
    
    /* Known test vector: "123456789" should give 0x29B1 */
    const uint8_t test[] = "123456789";
    uint16_t crc = uft_sector_crc16(test, 9, 0xFFFF);
    assert(crc == 0x29B1);
    
    printf("OK\n");
}

/* Test length calculation */
static void test_length_from_n(void) {
    printf("Testing length_from_n... ");
    
    assert(uft_sector_length_from_n(0) == 128);
    assert(uft_sector_length_from_n(1) == 256);
    assert(uft_sector_length_from_n(2) == 512);
    assert(uft_sector_length_from_n(3) == 1024);
    assert(uft_sector_length_from_n(4) == 2048);
    assert(uft_sector_length_from_n(5) == 4096);
    assert(uft_sector_length_from_n(6) == 8192);
    assert(uft_sector_length_from_n(7) == 16384);
    assert(uft_sector_length_from_n(8) == 0);  /* Invalid */
    
    printf("OK\n");
}

/* Test MFM track parsing with synthetic data */
static void test_parse_mfm_track(void) {
    printf("Testing MFM track parsing... ");
    
    /* Create synthetic MFM track with one sector:
     * Sync: A1 A1 A1
     * IDAM: FE
     * ID: C=0, H=0, R=1, N=2 (512 bytes)
     * CRC: xx xx
     * Gap
     * Sync: A1 A1 A1
     * DAM: FB
     * Data: 512 bytes
     * CRC: xx xx
     */
    
    uint8_t track[1024];
    memset(track, 0x4E, sizeof(track));  /* Gap fill */
    
    size_t pos = 10;
    
    /* ID record */
    track[pos++] = 0xA1;
    track[pos++] = 0xA1;
    track[pos++] = 0xA1;
    track[pos++] = 0xFE;  /* IDAM */
    track[pos++] = 0;     /* C */
    track[pos++] = 0;     /* H */
    track[pos++] = 1;     /* R */
    track[pos++] = 2;     /* N (512 bytes) */
    
    /* Calculate ID CRC */
    uint8_t id_for_crc[8] = {0xA1, 0xA1, 0xA1, 0xFE, 0, 0, 1, 2};
    uint16_t id_crc = uft_sector_crc16(id_for_crc, 8, 0xFFFF);
    track[pos++] = (uint8_t)(id_crc >> 8);
    track[pos++] = (uint8_t)(id_crc & 0xFF);
    
    /* Gap */
    pos += 22;
    
    /* Data record */
    size_t data_sync = pos;
    track[pos++] = 0xA1;
    track[pos++] = 0xA1;
    track[pos++] = 0xA1;
    track[pos++] = 0xFB;  /* DAM */
    
    /* Data (512 bytes of 0xE5) */
    size_t data_start = pos;
    for (int i = 0; i < 512; i++) {
        track[pos++] = 0xE5;
    }
    
    /* Calculate data CRC */
    uint16_t data_crc = 0xFFFF;
    data_crc = uft_sector_crc16(&track[data_sync], 3, data_crc);
    data_crc = uft_sector_crc16(&track[data_sync + 3], 1, data_crc);
    data_crc = uft_sector_crc16(&track[data_start], 512, data_crc);
    track[pos++] = (uint8_t)(data_crc >> 8);
    track[pos++] = (uint8_t)(data_crc & 0xFF);
    
    /* Parse */
    uft_sector_config_t cfg = {
        .encoding = UFT_ENC_MFM,
        .max_sectors = 32,
        .max_search_gap = 100,
    };
    
    uint8_t sector_data[512];
    uft_parsed_sector_t sectors[1];
    sectors[0].data = sector_data;
    sectors[0].data_capacity = 512;
    
    uft_sector_result_t result;
    int ret = uft_sector_parse_track(&cfg, track, pos, sectors, 1, &result);
    
    assert(ret == 0);
    assert(result.sectors_found == 1);
    assert(result.ids_found == 1);
    assert(result.data_records_found == 1);
    assert(result.sectors_with_data == 1);
    assert(result.warnings == 0);
    
    /* Check ID */
    assert(sectors[0].id_rec.id.cyl == 0);
    assert(sectors[0].id_rec.id.head == 0);
    assert(sectors[0].id_rec.id.sec == 1);
    assert(sectors[0].id_rec.id.size_n == 2);
    assert(sectors[0].id_rec.status == UFT_SECTOR_OK);
    
    /* Check data */
    assert(sectors[0].data_rec.dam == 0xFB);
    assert(sectors[0].data_rec.data_len == 512);
    assert(sectors[0].data_rec.status == UFT_SECTOR_OK);
    
    printf("OK\n");
}

/* Test status string */
static void test_status_str(void) {
    printf("Testing status strings... ");
    
    assert(strcmp(uft_sector_status_str(UFT_SECTOR_OK), "OK") == 0);
    assert(strcmp(uft_sector_status_str(UFT_SECTOR_CRC_ID_BAD), "CRC Error (ID)") == 0);
    assert(strcmp(uft_sector_status_str(UFT_SECTOR_CRC_DATA_BAD), "CRC Error (Data)") == 0);
    assert(strcmp(uft_sector_status_str(UFT_SECTOR_MISSING_DATA), "Missing Data") == 0);
    
    printf("OK\n");
}

int main(void) {
    printf("\n=== UFT Sector Parser Tests ===\n\n");
    
    test_crc16();
    test_length_from_n();
    test_parse_mfm_track();
    test_status_str();
    
    printf("\n=== All tests passed! ===\n\n");
    return 0;
}
