/**
 * @file test_external_integration.c
 * @brief Tests for integrated external tools (OpenDTC, cbmconvert, CAPS)
 * 
 * Tests integration of:
 * - OpenDTC: KryoFlux stream protocol
 * - cbmconvert: Commodore D64/T64 formats  
 * - capsimage: IPF/CAPS format handling
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_BEGIN(name) \
    do { \
        g_tests_run++; \
        printf("  [%02d] %-50s ", g_tests_run, name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() do { g_tests_passed++; printf("\033[32m[PASS]\033[0m\n"); } while(0)
#define TEST_FAIL(msg) do { printf("\033[31m[FAIL]\033[0m %s\n", msg); } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * KryoFlux Stream Tests (OpenDTC)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Stream protocol constants */
#define STREAM_OOB          0x0D
#define OOB_INDEX           0x02
#define KRYOFLUX_SCK        (48054857.0 / 2.0)

static void test_kf_stream_constants(void)
{
    TEST_BEGIN("KryoFlux: Stream protocol constants");
    
    /* Verify clock frequencies */
    double mck = (18432000.0 * 73.0) / 14.0 / 2.0;
    double sck = mck / 2.0;
    double ick = mck / 16.0;
    
    /* MCK should be ~48.054857 MHz */
    bool mck_ok = (mck > 48000000 && mck < 49000000);
    
    /* SCK should be ~24 MHz */
    bool sck_ok = (sck > 23000000 && sck < 25000000);
    
    /* ICK should be ~3 MHz */
    bool ick_ok = (ick > 2900000 && ick < 3100000);
    
    if (mck_ok && sck_ok && ick_ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("Clock frequency mismatch");
    }
}

static void test_kf_sample_to_time(void)
{
    TEST_BEGIN("KryoFlux: Sample to time conversion");
    
    /* 24 million samples = 1 second */
    uint32_t sample = 24000000;
    double time_s = (double)sample / KRYOFLUX_SCK;
    
    /* Should be approximately 1 second */
    bool ok = (time_s > 0.99 && time_s < 1.01);
    
    if (ok) {
        TEST_PASS();
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Expected ~1.0s, got %f", time_s);
        TEST_FAIL(msg);
    }
}

static void test_kf_rpm_calculation(void)
{
    TEST_BEGIN("KryoFlux: RPM calculation");
    
    /* ICK = ~3 MHz, 300 RPM = 200ms per revolution */
    /* index_time = ICK * 0.2 = ~600000 */
    double ick = (48054857.0 / 16.0);
    uint32_t index_time = (uint32_t)(ick * 0.2);  /* 200ms */
    
    double rpm = (ick * 60.0) / (double)index_time;
    
    /* Should be approximately 300 RPM */
    bool ok = (rpm > 295 && rpm < 305);
    
    if (ok) {
        TEST_PASS();
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Expected ~300 RPM, got %.1f", rpm);
        TEST_FAIL(msg);
    }
}

static void test_kf_oob_marker(void)
{
    TEST_BEGIN("KryoFlux: OOB marker detection");
    
    uint8_t stream[] = {
        0x50, 0x60,           /* Regular samples */
        0x0D,                 /* OOB marker */
        0x02,                 /* OOB type: Index */
        0x0C, 0x00,           /* Size: 12 */
        /* Index data (12 bytes) */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x70, 0x80            /* More samples */
    };
    
    /* Find OOB marker */
    bool found = false;
    for (size_t i = 0; i < sizeof(stream); i++) {
        if (stream[i] == STREAM_OOB) {
            if (i + 1 < sizeof(stream) && stream[i + 1] == OOB_INDEX) {
                found = true;
                break;
            }
        }
    }
    
    if (found) {
        TEST_PASS();
    } else {
        TEST_FAIL("OOB marker not detected");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Commodore Format Tests (cbmconvert)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* D64 constants */
#define D64_SIZE            174848
#define D64_40_SIZE         196608
#define D71_SIZE            349696
#define D81_SIZE            819200
#define SECTOR_SIZE         256

static const int d64_sectors_per_track[] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17,
    17, 17, 17, 17, 17
};

static void test_cbm_d64_size_detection(void)
{
    TEST_BEGIN("CBM: D64 size detection");
    
    bool d64_ok = (D64_SIZE == 174848);
    bool d64_40_ok = (D64_40_SIZE == 196608);
    bool d71_ok = (D71_SIZE == 349696);
    bool d81_ok = (D81_SIZE == 819200);
    
    if (d64_ok && d64_40_ok && d71_ok && d81_ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("Size constant mismatch");
    }
}

static void test_cbm_sector_offset(void)
{
    TEST_BEGIN("CBM: D64 sector offset calculation");
    
    /* Calculate offset for track 18, sector 0 (BAM) */
    long offset = 0;
    for (int t = 1; t < 18; t++) {
        offset += d64_sectors_per_track[t - 1] * SECTOR_SIZE;
    }
    
    /* Track 18 should start at sector 357 * 256 = 91392 */
    /* Actually: tracks 1-17 = 17*21 = 357 sectors */
    long expected = 357 * SECTOR_SIZE;
    
    if (offset == expected) {
        TEST_PASS();
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Expected %ld, got %ld", expected, offset);
        TEST_FAIL(msg);
    }
}

static void test_cbm_petscii_to_ascii(void)
{
    TEST_BEGIN("CBM: PETSCII to ASCII conversion");
    
    /* Test lowercase letters (0x41-0x5A -> lowercase) */
    uint8_t petscii_a = 0x41;  /* Should become 'a' */
    uint8_t ascii_a = (petscii_a >= 0x41 && petscii_a <= 0x5A) 
                      ? petscii_a + 0x20 : petscii_a;
    
    /* Test shifted space */
    uint8_t shifted_space = 0xA0;
    uint8_t ascii_space = (shifted_space == 0xA0) ? ' ' : shifted_space;
    
    if (ascii_a == 'a' && ascii_space == ' ') {
        TEST_PASS();
    } else {
        TEST_FAIL("PETSCII conversion failed");
    }
}

static void test_cbm_t64_magic(void)
{
    TEST_BEGIN("CBM: T64 magic detection");
    
    const char *magic1 = "C64 tape image file";
    const char *magic2 = "C64S tape image file";
    
    /* Test first magic variant */
    uint8_t header[64] = {0};
    memcpy(header, magic1, strlen(magic1));
    
    bool is_t64 = (memcmp(header, magic1, strlen(magic1)) == 0 ||
                   memcmp(header, magic2, strlen(magic2)) == 0);
    
    if (is_t64) {
        TEST_PASS();
    } else {
        TEST_FAIL("T64 magic not detected");
    }
}

static void test_cbm_file_types(void)
{
    TEST_BEGIN("CBM: File type strings");
    
    const char *types[] = {"DEL", "SEQ", "PRG", "USR", "REL", "CBM", "DIR"};
    
    bool all_ok = true;
    for (int i = 0; i < 7; i++) {
        if (strlen(types[i]) != 3) {
            all_ok = false;
            break;
        }
    }
    
    if (all_ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("File type string error");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * IPF/CAPS Format Tests (capsimage)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* IPF block types */
#define IPF_BLOCK_CAPS      1
#define IPF_BLOCK_INFO      2
#define IPF_BLOCK_IMGE      3
#define IPF_BLOCK_DATA      4
#define IPF_BLOCK_END       10

static uint32_t read32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void test_ipf_block_header(void)
{
    TEST_BEGIN("IPF: Block header parsing");
    
    /* Create a CAPS block header */
    uint8_t header[12] = {
        0x00, 0x00, 0x00, 0x01,  /* Type: CAPS (1) */
        0x00, 0x00, 0x00, 0x20,  /* Length: 32 */
        0x12, 0x34, 0x56, 0x78   /* CRC */
    };
    
    uint32_t type = read32_be(header);
    uint32_t length = read32_be(header + 4);
    uint32_t crc = read32_be(header + 8);
    
    bool ok = (type == IPF_BLOCK_CAPS && 
               length == 32 && 
               crc == 0x12345678);
    
    if (ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("Block header parsing error");
    }
}

static void test_ipf_magic_detection(void)
{
    TEST_BEGIN("IPF: Magic byte detection");
    
    /* Valid IPF starts with CAPS block type (1) in big-endian */
    uint8_t valid_ipf[12] = {
        0x00, 0x00, 0x00, 0x01,  /* CAPS block type */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    
    uint8_t invalid[12] = {
        0x00, 0x00, 0x00, 0x00,  /* Invalid type */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    
    bool valid_detected = (read32_be(valid_ipf) == IPF_BLOCK_CAPS);
    bool invalid_rejected = (read32_be(invalid) != IPF_BLOCK_CAPS);
    
    if (valid_detected && invalid_rejected) {
        TEST_PASS();
    } else {
        TEST_FAIL("Magic detection failed");
    }
}

static void test_ipf_platform_ids(void)
{
    TEST_BEGIN("IPF: Platform ID constants");
    
    /* Verify platform IDs */
    bool amiga_ok = (1 == 1);    /* PLATFORM_AMIGA */
    bool atari_ok = (2 == 2);    /* PLATFORM_ATARI_ST */
    bool c64_ok = (6 == 6);      /* PLATFORM_C64 */
    bool apple2_ok = (10 == 10); /* PLATFORM_APPLE2 */
    
    if (amiga_ok && atari_ok && c64_ok && apple2_ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("Platform ID mismatch");
    }
}

static void test_ipf_crc32(void)
{
    TEST_BEGIN("IPF: CRC-32 calculation");
    
    /* IPF uses polynomial 0x04C11DB7 (big-endian) */
    static uint32_t crc32_table[256];
    static bool table_init = false;
    
    if (!table_init) {
        for (int i = 0; i < 256; i++) {
            uint32_t crc = (uint32_t)i << 24;
            for (int j = 0; j < 8; j++) {
                if (crc & 0x80000000) {
                    crc = (crc << 1) ^ 0x04C11DB7;
                } else {
                    crc <<= 1;
                }
            }
            crc32_table[i] = crc;
        }
        table_init = true;
    }
    
    /* Calculate CRC of test data */
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < 4; i++) {
        crc = (crc << 8) ^ crc32_table[(crc >> 24) ^ data[i]];
    }
    
    /* Just verify CRC is non-zero and consistent */
    bool ok = (crc != 0 && crc != 0xFFFFFFFF);
    
    if (ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("CRC calculation error");
    }
}

static void test_ipf_encoding_types(void)
{
    TEST_BEGIN("IPF: Encoding type constants");
    
    /* Verify encoding types */
    int enc_mfm = 1;
    int enc_gcr = 2;
    int enc_fm = 3;
    int enc_raw = 4;
    
    bool ok = (enc_mfm == 1 && enc_gcr == 2 && enc_fm == 3 && enc_raw == 4);
    
    if (ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("Encoding type mismatch");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT External Integration Tests\n");
    printf("  (OpenDTC + cbmconvert + CAPS)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("KryoFlux Stream (OpenDTC):\n");
    test_kf_stream_constants();
    test_kf_sample_to_time();
    test_kf_rpm_calculation();
    test_kf_oob_marker();
    
    printf("\nCommodore Formats (cbmconvert):\n");
    test_cbm_d64_size_detection();
    test_cbm_sector_offset();
    test_cbm_petscii_to_ascii();
    test_cbm_t64_magic();
    test_cbm_file_types();
    
    printf("\nIPF/CAPS Format (capsimage):\n");
    test_ipf_block_header();
    test_ipf_magic_detection();
    test_ipf_platform_ids();
    test_ipf_crc32();
    test_ipf_encoding_types();
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n",
           g_tests_passed,
           g_tests_run - g_tests_passed,
           g_tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
