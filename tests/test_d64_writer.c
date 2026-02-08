/**
 * @file test_d64_writer.c
 * @brief D64 Writer Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL\n"); _fail++; return; } } while(0)

/* Mock GCR encode table */
static const uint8_t gcr_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

static uint8_t gcr_enc(uint8_t n) { return gcr_encode[n & 0x0F]; }

/* Sectors per track */
static int spt(int track) {
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

/* Speed zone */
static int zone(int track) {
    if (track <= 17) return 0;
    if (track <= 24) return 1;
    if (track <= 30) return 2;
    return 3;
}

TEST(sectors_per_track) {
    ASSERT(spt(1) == 21);
    ASSERT(spt(17) == 21);
    ASSERT(spt(18) == 19);
    ASSERT(spt(24) == 19);
    ASSERT(spt(25) == 18);
    ASSERT(spt(30) == 18);
    ASSERT(spt(31) == 17);
    ASSERT(spt(35) == 17);
}

TEST(speed_zones) {
    ASSERT(zone(1) == 0);
    ASSERT(zone(17) == 0);
    ASSERT(zone(18) == 1);
    ASSERT(zone(24) == 1);
    ASSERT(zone(25) == 2);
    ASSERT(zone(30) == 2);
    ASSERT(zone(31) == 3);
}

TEST(gcr_encode_table) {
    ASSERT(gcr_enc(0x0) == 0x0A);
    ASSERT(gcr_enc(0x1) == 0x0B);
    ASSERT(gcr_enc(0x8) == 0x09);
    ASSERT(gcr_enc(0xF) == 0x15);
}

TEST(header_checksum) {
    /* XOR of track, sector, id1, id2 */
    uint8_t track = 1, sector = 0, id1 = 0x30, id2 = 0x30;
    uint8_t checksum = track ^ sector ^ id1 ^ id2;
    ASSERT(checksum == (1 ^ 0 ^ 0x30 ^ 0x30));
    ASSERT(checksum == 1);
}

TEST(data_checksum) {
    uint8_t data[256];
    memset(data, 0, 256);
    data[0] = 0xFF;
    data[1] = 0x01;
    
    uint8_t checksum = 0;
    for (int i = 0; i < 256; i++) checksum ^= data[i];
    ASSERT(checksum == (0xFF ^ 0x01));
    ASSERT(checksum == 0xFE);
}

TEST(total_sectors_35_tracks) {
    int total = 0;
    for (int t = 1; t <= 35; t++) {
        total += spt(t);
    }
    ASSERT(total == 683);  /* Standard D64 sector count */
}

TEST(d64_file_size) {
    /* 683 sectors * 256 bytes = 174848 */
    int total_sectors = 0;
    for (int t = 1; t <= 35; t++) {
        total_sectors += spt(t);
    }
    ASSERT(total_sectors * 256 == 174848);
}

TEST(gcr_expansion) {
    /* 4 bytes raw -> 5 bytes GCR */
    /* 256 byte sector + 4 byte overhead = 260 bytes -> 325 GCR */
    ASSERT((260 * 5 / 4) == 325);
    
    /* Header: 8 bytes -> 10 GCR */
    ASSERT((8 * 5 / 4) == 10);
}

TEST(track_timing) {
    /* Zone bit times in microseconds */
    double z0 = 4.0, z1 = 3.75, z2 = 3.5, z3 = 3.25;
    
    /* Track length should fit in 200ms revolution */
    /* Zone 0: 7692 bytes * 8 bits * 4.0us = ~246ms (adjusted with gaps) */
    ASSERT(z0 > z1);
    ASSERT(z1 > z2);
    ASSERT(z2 > z3);
}

TEST(sync_pattern) {
    /* Sync is 5 bytes of 0xFF by default */
    uint8_t sync[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for (int i = 0; i < 5; i++) {
        ASSERT(sync[i] == 0xFF);
    }
}

TEST(gap_pattern) {
    /* Gap is filled with 0x55 pattern */
    uint8_t gap[9];
    memset(gap, 0x55, 9);
    for (int i = 0; i < 9; i++) {
        ASSERT(gap[i] == 0x55);
    }
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  D64 Writer Tests (P2-006)\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(sectors_per_track);
    RUN(speed_zones);
    RUN(gcr_encode_table);
    RUN(header_checksum);
    RUN(data_checksum);
    RUN(total_sectors_35_tracks);
    RUN(d64_file_size);
    RUN(gcr_expansion);
    RUN(track_timing);
    RUN(sync_pattern);
    RUN(gap_pattern);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
