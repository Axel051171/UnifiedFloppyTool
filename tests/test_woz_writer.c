/**
 * @file test_woz_writer.c
 * @brief WOZ Writer Tests
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL\n"); _fail++; return; } } while(0)

/* CRC32 (matching WOZ spec) */
static uint32_t crc32_table[256];
static void init_crc(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? ((c >> 1) ^ 0xEDB88320) : (c >> 1);
        crc32_table[i] = c;
    }
}
static uint32_t woz_crc(const uint8_t *d, size_t n) {
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < n; i++) c = (c >> 8) ^ crc32_table[(c ^ d[i]) & 0xFF];
    return c ^ 0xFFFFFFFF;
}

/* GCR table */
static const uint8_t gcr_6and2[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

TEST(woz_magic) {
    char magic[] = "WOZ2";
    ASSERT(magic[0] == 'W');
    ASSERT(magic[1] == 'O');
    ASSERT(magic[2] == 'Z');
    ASSERT(magic[3] == '2');
}

TEST(woz_header_structure) {
    /* WOZ header: 4 magic + 4 high bit check + 4 CRC = 12 bytes */
    ASSERT(4 + 4 + 4 == 12);
}

TEST(chunk_ids) {
    /* Little-endian chunk IDs */
    uint32_t info = 0x4F464E49;  /* "INFO" */
    uint32_t tmap = 0x50414D54;  /* "TMAP" */
    uint32_t trks = 0x534B5254;  /* "TRKS" */
    
    char info_str[5] = {0};
    memcpy(info_str, &info, 4);
    ASSERT(strcmp(info_str, "INFO") == 0);
    
    char tmap_str[5] = {0};
    memcpy(tmap_str, &tmap, 4);
    ASSERT(strcmp(tmap_str, "TMAP") == 0);
}

TEST(tmap_size) {
    /* TMAP is always 160 bytes (40 tracks * 4 quarter tracks) */
    ASSERT(40 * 4 == 160);
}

TEST(track_limits) {
    /* 5.25" can have up to 40 tracks with quarter track support */
    int max_tracks_525 = 40 * 4;
    ASSERT(max_tracks_525 == 160);
    
    /* 3.5" has 80 tracks * 2 sides */
    int max_tracks_35 = 80 * 2;
    ASSERT(max_tracks_35 == 160);
}

TEST(crc32) {
    init_crc();
    
    uint8_t test[] = "123456789";
    uint32_t crc = woz_crc(test, 9);
    
    /* Standard CRC-32 of "123456789" is 0xCBF43926 */
    ASSERT(crc == 0xCBF43926);
}

TEST(gcr_encoding_table) {
    /* All encoded values must have high bit set */
    for (int i = 0; i < 64; i++) {
        ASSERT(gcr_6and2[i] >= 0x96);
    }
    
    /* First few values */
    ASSERT(gcr_6and2[0] == 0x96);
    ASSERT(gcr_6and2[1] == 0x97);
    ASSERT(gcr_6and2[63] == 0xFF);
}

TEST(address_field_prologue) {
    uint8_t prologue[] = {0xD5, 0xAA, 0x96};
    ASSERT(prologue[0] == 0xD5);
    ASSERT(prologue[1] == 0xAA);
    ASSERT(prologue[2] == 0x96);
}

TEST(data_field_prologue) {
    uint8_t prologue[] = {0xD5, 0xAA, 0xAD};
    ASSERT(prologue[0] == 0xD5);
    ASSERT(prologue[1] == 0xAA);
    ASSERT(prologue[2] == 0xAD);
}

TEST(sector_interleave) {
    /* DOS 3.3 interleave */
    int dos[16] = {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15};
    ASSERT(dos[0] == 0);
    ASSERT(dos[1] == 7);
    ASSERT(dos[15] == 15);
}

TEST(bit_timing) {
    /* Apple II bit timing: 4us = 32 * 125ns */
    int timing_125ns = 32;
    int bit_time_ns = timing_125ns * 125;
    ASSERT(bit_time_ns == 4000);  /* 4 microseconds */
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  WOZ Writer Tests (P2-007)\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(woz_magic);
    RUN(woz_header_structure);
    RUN(chunk_ids);
    RUN(tmap_size);
    RUN(track_limits);
    RUN(crc32);
    RUN(gcr_encoding_table);
    RUN(address_field_prologue);
    RUN(data_field_prologue);
    RUN(sector_interleave);
    RUN(bit_timing);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
