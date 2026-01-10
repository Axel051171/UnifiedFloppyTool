/**
 * @file test_format_adapters.c
 * @brief Comprehensive Format Adapter Tests
 * 
 * Tests for ADF, D64, IMG, and TRD format adapters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/adapters/uft_adf_adapter.h"
#include "uft/adapters/uft_d64_adapter.h"
#include "uft/adapters/uft_img_adapter.h"
#include "uft/adapters/uft_trd_adapter.h"

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test) do { \
    printf("  %-40s", #test "..."); \
    tests_run++; \
    test(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Data Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint8_t *create_adf_data(void) {
    /* Create minimal ADF (DD: 901120 bytes) */
    uint8_t *data = calloc(1, 901120);
    if (!data) return NULL;
    
    /* DOS bootblock */
    data[0] = 'D'; data[1] = 'O'; data[2] = 'S'; data[3] = 0;
    
    /* Calculate and set bootblock checksum */
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        if (i != 4) {
            uint32_t val = ((uint32_t)data[i] << 24) | 
                          ((uint32_t)data[i+1] << 16) | 
                          ((uint32_t)data[i+2] << 8) | 
                          (uint32_t)data[i+3];
            uint32_t old = sum;
            sum += val;
            if (sum < old) sum++;
        }
    }
    sum = ~sum;
    data[4] = (sum >> 24) & 0xFF;
    data[5] = (sum >> 16) & 0xFF;
    data[6] = (sum >> 8) & 0xFF;
    data[7] = sum & 0xFF;
    
    /* Root block at block 880 (offset 450560) */
    size_t root_offset = 880 * 512;
    data[root_offset + 0x1B0] = 4;  /* Name length */
    data[root_offset + 0x1B1] = 'T';
    data[root_offset + 0x1B2] = 'E';
    data[root_offset + 0x1B3] = 'S';
    data[root_offset + 0x1B4] = 'T';
    
    return data;
}

static uint8_t *create_d64_data(void) {
    /* Create minimal D64 (35 tracks: 174848 bytes) */
    uint8_t *data = calloc(1, 174848);
    if (!data) return NULL;
    
    /* BAM at track 18, sector 0 (offset 0x16500) */
    size_t bam_offset = 0x16500;
    data[bam_offset + 0] = 18;  /* Directory track */
    data[bam_offset + 1] = 1;   /* Directory sector */
    data[bam_offset + 2] = 0x41; /* DOS type 'A' */
    
    /* Disk name at BAM + 0x90 */
    data[bam_offset + 0x90] = 'T';
    data[bam_offset + 0x91] = 'E';
    data[bam_offset + 0x92] = 'S';
    data[bam_offset + 0x93] = 'T';
    for (int i = 4; i < 16; i++) {
        data[bam_offset + 0x90 + i] = 0xA0;  /* Padding */
    }
    
    /* Disk ID */
    data[bam_offset + 0xA2] = '0';
    data[bam_offset + 0xA3] = '1';
    
    return data;
}

static uint8_t *create_img_data(void) {
    /* Create minimal IMG (1.44M: 1474560 bytes) */
    uint8_t *data = calloc(1, 1474560);
    if (!data) return NULL;
    
    /* Boot sector */
    data[0] = 0xEB;  /* JMP short */
    data[1] = 0x3C;
    data[2] = 0x90;  /* NOP */
    
    /* OEM name */
    memcpy(data + 3, "MSDOS5.0", 8);
    
    /* BPB */
    data[11] = 0x00; data[12] = 0x02;  /* Bytes per sector: 512 */
    data[13] = 1;                       /* Sectors per cluster */
    data[14] = 1; data[15] = 0;         /* Reserved sectors */
    data[16] = 2;                       /* Number of FATs */
    data[17] = 0xE0; data[18] = 0x00;   /* Root entries: 224 */
    data[19] = 0x40; data[20] = 0x0B;   /* Total sectors: 2880 */
    data[21] = 0xF0;                    /* Media descriptor */
    data[22] = 9; data[23] = 0;         /* Sectors per FAT */
    data[24] = 18; data[25] = 0;        /* Sectors per track */
    data[26] = 2; data[27] = 0;         /* Number of heads */
    
    /* Boot signature */
    data[510] = 0x55;
    data[511] = 0xAA;
    
    return data;
}

static uint8_t *create_trd_data(void) {
    /* Create minimal TRD (640K: 655360 bytes) */
    uint8_t *data = calloc(1, 655360);
    if (!data) return NULL;
    
    /* System sector at track 0, sector 9 (offset 0x800) */
    size_t sys_offset = 8 * 256;  /* Sector 9 (0-based: 8) */
    
    data[sys_offset + 0] = 1;     /* First free sector */
    data[sys_offset + 1] = 1;     /* First free track */
    data[sys_offset + 2] = 0x16;  /* Disk type: 80T DS */
    data[sys_offset + 3] = 0;     /* File count */
    data[sys_offset + 4] = 0x60;  /* Free sectors LSB */
    data[sys_offset + 5] = 0x09;  /* Free sectors MSB (2400) */
    data[sys_offset + 6] = 0x10;  /* TR-DOS ID */
    
    /* Disk label */
    memcpy(data + sys_offset + 20, "TEST    ", 8);
    
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF Adapter Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_adf_probe(void) {
    uint8_t *data = create_adf_data();
    assert(data != NULL);
    
    uft_format_score_t score = uft_adf_adapter.probe(data, 901120, "test.adf");
    
    assert(score.overall > 0.5f);
    assert(score.valid == true);
    
    free(data);
}

static void test_adf_open_close(void) {
    uint8_t *data = create_adf_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    
    uft_error_t err = uft_adf_adapter.open(&ctx, data, 901120);
    assert(err == UFT_SUCCESS);
    assert(ctx.format_data != NULL);
    
    uft_adf_adapter.close(&ctx);
    assert(ctx.format_data == NULL);
    
    free(data);
}

static void test_adf_geometry(void) {
    uint8_t *data = create_adf_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_adf_adapter.open(&ctx, data, 901120);
    
    uint16_t tracks;
    uint8_t sides, sectors;
    uint16_t sector_size;
    
    uft_adf_adapter.get_geometry(&ctx, &tracks, &sides, &sectors, &sector_size);
    
    assert(tracks == 80);
    assert(sides == 2);
    assert(sectors == 11);
    assert(sector_size == 512);
    
    uft_adf_adapter.close(&ctx);
    free(data);
}

static void test_adf_read_track(void) {
    uint8_t *data = create_adf_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_adf_adapter.open(&ctx, data, 901120);
    
    uft_track_data_t track;
    uft_error_t err = uft_adf_adapter.read_track(&ctx, 0, 0, &track);
    
    assert(err == UFT_SUCCESS);
    assert(track.track_num == 0);
    assert(track.side == 0);
    assert(track.sector_count == 11);
    assert(track.raw_size == 11 * 512);
    
    uft_track_data_free(&track);
    uft_adf_adapter.close(&ctx);
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D64 Adapter Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_d64_probe(void) {
    uint8_t *data = create_d64_data();
    assert(data != NULL);
    
    uft_format_score_t score = uft_d64_adapter.probe(data, 174848, "game.d64");
    
    assert(score.overall > 0.3f);
    assert(score.detail.c64.tracks == 35);
    
    free(data);
}

static void test_d64_open_close(void) {
    uint8_t *data = create_d64_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    
    uft_error_t err = uft_d64_adapter.open(&ctx, data, 174848);
    assert(err == UFT_SUCCESS);
    assert(ctx.format_data != NULL);
    
    uft_d64_adapter.close(&ctx);
    assert(ctx.format_data == NULL);
    
    free(data);
}

static void test_d64_geometry(void) {
    uint8_t *data = create_d64_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_d64_adapter.open(&ctx, data, 174848);
    
    uint16_t tracks;
    uint8_t sides, sectors;
    uint16_t sector_size;
    
    uft_d64_adapter.get_geometry(&ctx, &tracks, &sides, &sectors, &sector_size);
    
    assert(tracks == 35);
    assert(sides == 1);
    assert(sectors == 21);  /* Max sectors */
    assert(sector_size == 256);
    
    uft_d64_adapter.close(&ctx);
    free(data);
}

static void test_d64_read_track(void) {
    uint8_t *data = create_d64_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_d64_adapter.open(&ctx, data, 174848);
    
    uft_track_data_t track;
    uft_error_t err = uft_d64_adapter.read_track(&ctx, 1, 0, &track);  /* Track 1 (1-based) */
    
    assert(err == UFT_SUCCESS);
    assert(track.sector_count == 21);  /* Track 1 has 21 sectors */
    
    uft_track_data_free(&track);
    uft_d64_adapter.close(&ctx);
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMG Adapter Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_img_probe(void) {
    uint8_t *data = create_img_data();
    assert(data != NULL);
    
    uft_format_score_t score = uft_img_adapter.probe(data, 1474560, "disk.img");
    
    assert(score.overall > 0.5f);
    
    free(data);
}

static void test_img_open_close(void) {
    uint8_t *data = create_img_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    
    uft_error_t err = uft_img_adapter.open(&ctx, data, 1474560);
    assert(err == UFT_SUCCESS);
    assert(ctx.format_data != NULL);
    
    uft_img_adapter.close(&ctx);
    assert(ctx.format_data == NULL);
    
    free(data);
}

static void test_img_geometry(void) {
    uint8_t *data = create_img_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_img_adapter.open(&ctx, data, 1474560);
    
    uint16_t tracks;
    uint8_t sides, sectors;
    uint16_t sector_size;
    
    uft_img_adapter.get_geometry(&ctx, &tracks, &sides, &sectors, &sector_size);
    
    assert(tracks == 80);
    assert(sides == 2);
    assert(sectors == 18);
    assert(sector_size == 512);
    
    uft_img_adapter.close(&ctx);
    free(data);
}

static void test_img_read_track(void) {
    uint8_t *data = create_img_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_img_adapter.open(&ctx, data, 1474560);
    
    uft_track_data_t track;
    uft_error_t err = uft_img_adapter.read_track(&ctx, 0, 0, &track);
    
    assert(err == UFT_SUCCESS);
    assert(track.track_num == 0);
    assert(track.side == 0);
    assert(track.sector_count == 18);
    
    uft_track_data_free(&track);
    uft_img_adapter.close(&ctx);
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TRD Adapter Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_trd_probe(void) {
    uint8_t *data = create_trd_data();
    assert(data != NULL);
    
    uft_format_score_t score = uft_trd_adapter.probe(data, 655360, "game.trd");
    
    assert(score.overall > 0.3f);
    
    free(data);
}

static void test_trd_open_close(void) {
    uint8_t *data = create_trd_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    
    uft_error_t err = uft_trd_adapter.open(&ctx, data, 655360);
    assert(err == UFT_SUCCESS);
    assert(ctx.format_data != NULL);
    
    uft_trd_adapter.close(&ctx);
    assert(ctx.format_data == NULL);
    
    free(data);
}

static void test_trd_geometry(void) {
    uint8_t *data = create_trd_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_trd_adapter.open(&ctx, data, 655360);
    
    uint16_t tracks;
    uint8_t sides, sectors;
    uint16_t sector_size;
    
    uft_trd_adapter.get_geometry(&ctx, &tracks, &sides, &sectors, &sector_size);
    
    assert(tracks == 80);
    assert(sides == 2);
    assert(sectors == 16);
    assert(sector_size == 256);
    
    uft_trd_adapter.close(&ctx);
    free(data);
}

static void test_trd_read_track(void) {
    uint8_t *data = create_trd_data();
    assert(data != NULL);
    
    struct uft_xdf_context ctx = {0};
    uft_trd_adapter.open(&ctx, data, 655360);
    
    uft_track_data_t track;
    uft_error_t err = uft_trd_adapter.read_track(&ctx, 0, 0, &track);
    
    assert(err == UFT_SUCCESS);
    assert(track.track_num == 0);
    assert(track.side == 0);
    assert(track.sector_count == 16);
    
    uft_track_data_free(&track);
    uft_trd_adapter.close(&ctx);
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Integration Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_adapter_registration(void) {
    /* Register all adapters */
    uft_adf_adapter_init();
    uft_d64_adapter_init();
    uft_img_adapter_init();
    uft_trd_adapter_init();
    
    /* Find by ID */
    const uft_format_adapter_t *adf = uft_adapter_find_by_id(UFT_FORMAT_ID_ADF);
    assert(adf != NULL);
    assert(strcmp(adf->name, "ADF") == 0);
    
    const uft_format_adapter_t *d64 = uft_adapter_find_by_id(UFT_FORMAT_ID_D64);
    assert(d64 != NULL);
    assert(strcmp(d64->name, "D64") == 0);
}

static void test_auto_detection(void) {
    /* Test ADF detection */
    uint8_t *adf_data = create_adf_data();
    uft_format_score_t score;
    const uft_format_adapter_t *adapter = uft_adapter_detect(adf_data, 901120, "test.adf", &score);
    assert(adapter != NULL);
    assert(adapter->format_id == UFT_FORMAT_ID_ADF);
    free(adf_data);
    
    /* Test D64 detection */
    uint8_t *d64_data = create_d64_data();
    adapter = uft_adapter_detect(d64_data, 174848, "game.d64", &score);
    assert(adapter != NULL);
    assert(adapter->format_id == UFT_FORMAT_ID_D64);
    free(d64_data);
    
    /* Test IMG detection */
    uint8_t *img_data = create_img_data();
    adapter = uft_adapter_detect(img_data, 1474560, "disk.img", &score);
    assert(adapter != NULL);
    assert(adapter->format_id == UFT_FORMAT_ID_IMG);
    free(img_data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" Format Adapter Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    /* Register adapters first */
    uft_adf_adapter_init();
    uft_d64_adapter_init();
    uft_img_adapter_init();
    uft_trd_adapter_init();
    
    printf("ADF Adapter Tests:\n");
    RUN_TEST(test_adf_probe);
    RUN_TEST(test_adf_open_close);
    RUN_TEST(test_adf_geometry);
    RUN_TEST(test_adf_read_track);
    
    printf("\nD64 Adapter Tests:\n");
    RUN_TEST(test_d64_probe);
    RUN_TEST(test_d64_open_close);
    RUN_TEST(test_d64_geometry);
    RUN_TEST(test_d64_read_track);
    
    printf("\nIMG Adapter Tests:\n");
    RUN_TEST(test_img_probe);
    RUN_TEST(test_img_open_close);
    RUN_TEST(test_img_geometry);
    RUN_TEST(test_img_read_track);
    
    printf("\nTRD Adapter Tests:\n");
    RUN_TEST(test_trd_probe);
    RUN_TEST(test_trd_open_close);
    RUN_TEST(test_trd_geometry);
    RUN_TEST(test_trd_read_track);
    
    printf("\nIntegration Tests:\n");
    RUN_TEST(test_adapter_registration);
    RUN_TEST(test_auto_detection);
    
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf(" ✓ All Format Adapter tests passed! (%d/%d)\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}
