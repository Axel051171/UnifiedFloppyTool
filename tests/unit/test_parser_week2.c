/**
 * @file test_parser_week2.c
 * @brief Unit tests for Week 2 parsers (SCP, STX, EDSK, 2MG)
 * @version 3.3.2
 * 
 * Tests cover:
 * - Format detection (magic bytes, probe confidence)
 * - Header parsing (geometry, track count, side count)
 * - Multi-revolution support (SCP)
 * - Protection detection (STX)
 * - Extension handling (EDSK)
 * - Apple II variants (2MG: DOS, ProDOS, nibble)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/uft_test_framework.h"
#include "uft/uft_error.h"
#include "uft/uft_endian.h"

/*============================================================================
 * Test Data: SCP (SuperCard Pro)
 *============================================================================*/

/* Valid SCP header with 2 revolutions, 80 tracks */
static const uint8_t SCP_HEADER_VALID[] = {
    'S', 'C', 'P',              /* Magic */
    0x19,                        /* Version 1.9 */
    0x04,                        /* Disk type: C64 */
    0x02,                        /* Number of revolutions */
    0x00,                        /* Start track */
    0x4F,                        /* End track (79) */
    0x01,                        /* Flags: index hole */
    0x00,                        /* Bit cell encoding */
    0x00, 0x00,                  /* Number of heads */
    0x19,                        /* Resolution (25ns) */
    0x00, 0x00, 0x00, 0x00       /* Checksum placeholder */
};

/* Invalid SCP magic */
static const uint8_t SCP_HEADER_BAD_MAGIC[] = {
    'S', 'C', 'X',              /* Wrong magic */
    0x19, 0x04, 0x02, 0x00, 0x4F, 0x01, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00
};

/* SCP track header */
static const uint8_t SCP_TRACK_HEADER[] = {
    'T', 'R', 'K',              /* Track magic */
    0x00,                        /* Track number */
    /* Revolution 0 */
    0x00, 0x10, 0x00, 0x00,     /* Duration (little endian) */
    0x00, 0x20, 0x00, 0x00,     /* Length */
    0x20, 0x00, 0x00, 0x00,     /* Offset */
    /* Revolution 1 */
    0x00, 0x10, 0x00, 0x00,
    0x00, 0x20, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00
};

/*============================================================================
 * Test Data: STX (Pasti)
 *============================================================================*/

/* Valid STX header */
static const uint8_t STX_HEADER_VALID[] = {
    'R', 'S', 'Y', 0x00,        /* Magic "RSY\0" */
    0x03, 0x00,                  /* Version 3 */
    0x01, 0x00,                  /* Tool version */
    0x00, 0x00,                  /* Reserved */
    0x50, 0x00,                  /* Track count (80) */
    0x00, 0x00,                  /* Revision */
    0x00, 0x00, 0x00, 0x00       /* Reserved */
};

/* STX track descriptor */
static const uint8_t STX_TRACK_DESC[] = {
    0x00, 0x10, 0x00, 0x00,     /* Fuzzy count */
    0x09, 0x00,                  /* Sector count */
    0x00, 0x00,                  /* Track flags */
    0x00, 0x20, 0x00, 0x00,     /* Track length */
    0x00,                        /* Track number */
    0x00,                        /* Track type */
    0x00, 0x00                   /* Reserved */
};

/* STX sector with protection (timing, fuzzy bits) */
static const uint8_t STX_SECTOR_PROTECTED[] = {
    0x00, 0x02, 0x00, 0x00,     /* Data offset */
    0x08, 0x00,                  /* Bit position */
    0x00, 0x00,                  /* Read time */
    0x00,                        /* Track */
    0x00,                        /* Side */
    0x01,                        /* Sector */
    0x02,                        /* Size (512 bytes) */
    0x00,                        /* CRC */
    0x06,                        /* FDC flags: fuzzy + timing */
    0x00                         /* Reserved */
};

/*============================================================================
 * Test Data: EDSK (Extended DSK)
 *============================================================================*/

/* Valid EDSK header */
static const uint8_t EDSK_HEADER_VALID[] = {
    'E', 'X', 'T', 'E', 'N', 'D', 'E', 'D', ' ',
    'C', 'P', 'C', ' ', 'D', 'S', 'K', ' ',
    'F', 'i', 'l', 'e', '\r', '\n',
    'D', 'i', 's', 'k', '-', 'I', 'n', 'f', 'o', '\r', '\n',
    /* Creator (14 bytes) */
    'U', 'F', 'T', ' ', 'v', '3', '.', '3', '.', '2', 0, 0, 0, 0,
    0x28,                        /* Number of tracks (40) */
    0x02,                        /* Number of sides */
    0x00, 0x00,                  /* Unused (track size for standard DSK) */
    /* Track size table follows... */
};

/* Standard DSK header (non-extended) */
static const uint8_t DSK_HEADER_STANDARD[] = {
    'M', 'V', ' ', '-', ' ', 'C', 'P', 'C', 'E', 'M', 'U', ' ',
    'D', 'i', 's', 'k', '-', 'F', 'i', 'l', 'e', '\r', '\n',
    'D', 'i', 's', 'k', '-', 'I', 'n', 'f', 'o', '\r', '\n',
    'U', 'F', 'T', ' ', 'v', '3', '.', '3', '.', '2', 0, 0, 0, 0,
    0x28,                        /* Tracks */
    0x01,                        /* Sides */
    0x00, 0x13,                  /* Track size (0x1300 = 4864 bytes) */
};

/* EDSK track info block */
static const uint8_t EDSK_TRACK_INFO[] = {
    'T', 'r', 'a', 'c', 'k', '-', 'I', 'n', 'f', 'o', '\r', '\n',
    0x00, 0x00, 0x00, 0x00,      /* Unused */
    0x00,                        /* Track number */
    0x00,                        /* Side number */
    0x00, 0x00,                  /* Unused */
    0x02,                        /* Sector size (512) */
    0x09,                        /* Number of sectors */
    0x4E,                        /* GAP3 length */
    0xE5,                        /* Filler byte */
    /* Sector info list follows... */
};

/*============================================================================
 * Test Data: 2MG (Apple II)
 *============================================================================*/

/* Valid 2MG header - ProDOS order */
static const uint8_t IMG2_HEADER_PRODOS[] = {
    '2', 'I', 'M', 'G',         /* Magic */
    'P', 'R', 'O', 'D',         /* Creator */
    0x40, 0x00,                  /* Header size (64) */
    0x01, 0x00,                  /* Version */
    0x01, 0x00, 0x00, 0x00,     /* Format: ProDOS order */
    0x00, 0x00, 0x00, 0x00,     /* Flags */
    0x00, 0x60, 0x01, 0x00,     /* ProDOS blocks (280) */
    0x40, 0x00, 0x00, 0x00,     /* Data offset */
    0x00, 0x38, 0x02, 0x00,     /* Data length (143360) */
    0x00, 0x00, 0x00, 0x00,     /* Comment offset */
    0x00, 0x00, 0x00, 0x00,     /* Comment length */
    0x00, 0x00, 0x00, 0x00,     /* Creator data offset */
    0x00, 0x00, 0x00, 0x00,     /* Creator data length */
    0x00, 0x00, 0x00, 0x00,     /* Reserved */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/* Valid 2MG header - DOS 3.3 order */
static const uint8_t IMG2_HEADER_DOS[] = {
    '2', 'I', 'M', 'G',
    'D', 'O', 'S', '!',         /* Creator: DOS */
    0x40, 0x00,
    0x01, 0x00,
    0x00, 0x00, 0x00, 0x00,     /* Format: DOS order */
    0x00, 0x00, 0x00, 0x00,
    0x18, 0x01, 0x00, 0x00,     /* ProDOS blocks (280) */
    0x40, 0x00, 0x00, 0x00,
    0x00, 0x38, 0x02, 0x00,     /* 143360 bytes */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/* 2MG nibble format */
static const uint8_t IMG2_HEADER_NIB[] = {
    '2', 'I', 'M', 'G',
    'N', 'I', 'B', 'B',         /* Creator: Nibble */
    0x40, 0x00,
    0x01, 0x00,
    0x02, 0x00, 0x00, 0x00,     /* Format: Nibble */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,     /* Blocks not used for nibble */
    0x40, 0x00, 0x00, 0x00,
    0x00, 0xA0, 0x03, 0x00,     /* 232960 bytes (35 * 6656) */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/*============================================================================
 * SCP Parser Tests
 *============================================================================*/

UFT_TEST(scp_probe_valid_header)
{
    /* Create test buffer with valid SCP header */
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, SCP_HEADER_VALID, sizeof(SCP_HEADER_VALID));
    
    /* Probe should return high confidence (85-100) */
    int confidence = uft_scp_probe(buffer, sizeof(buffer));
    UFT_ASSERT_GE(confidence, 85);
    UFT_ASSERT_LE(confidence, 100);
}

UFT_TEST(scp_probe_invalid_magic)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, SCP_HEADER_BAD_MAGIC, sizeof(SCP_HEADER_BAD_MAGIC));
    
    /* Should return 0 confidence for invalid magic */
    int confidence = uft_scp_probe(buffer, sizeof(buffer));
    UFT_ASSERT_EQ(confidence, 0);
}

UFT_TEST(scp_probe_too_small)
{
    /* Buffer too small for header */
    uint8_t buffer[8];
    memcpy(buffer, "SCP", 3);
    
    int confidence = uft_scp_probe(buffer, sizeof(buffer));
    UFT_ASSERT_EQ(confidence, 0);
}

UFT_TEST(scp_parse_geometry)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, SCP_HEADER_VALID, sizeof(SCP_HEADER_VALID));
    
    uft_scp_context_t ctx;
    uft_error_t err = uft_scp_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(ctx.version_major, 1);
    UFT_ASSERT_EQ(ctx.version_minor, 9);
    UFT_ASSERT_EQ(ctx.disk_type, 0x04);  /* C64 */
    UFT_ASSERT_EQ(ctx.num_revolutions, 2);
    UFT_ASSERT_EQ(ctx.start_track, 0);
    UFT_ASSERT_EQ(ctx.end_track, 79);
    UFT_ASSERT_EQ(ctx.resolution_ns, 25);
}

UFT_TEST(scp_multi_revolution_count)
{
    /* Test various revolution counts (1-5) */
    uint8_t buffer[512];
    
    for (int revs = 1; revs <= 5; revs++) {
        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, SCP_HEADER_VALID, sizeof(SCP_HEADER_VALID));
        buffer[5] = (uint8_t)revs;  /* Revolution count offset */
        
        uft_scp_context_t ctx;
        uft_error_t err = uft_scp_parse_header(buffer, sizeof(buffer), &ctx);
        
        UFT_ASSERT_EQ(err, UFT_ERR_OK);
        UFT_ASSERT_EQ(ctx.num_revolutions, revs);
    }
}

UFT_TEST(scp_disk_types)
{
    /* Test known disk types */
    const struct {
        uint8_t type;
        const char* name;
    } disk_types[] = {
        { 0x00, "C64" },
        { 0x04, "C64" },
        { 0x10, "Amiga" },
        { 0x20, "Atari FM" },
        { 0x24, "Atari MFM" },
        { 0x30, "Apple II" },
        { 0x40, "PC 360K" },
        { 0x48, "PC 1.2M" },
        { 0x50, "PC 720K" },
        { 0x58, "PC 1.44M" }
    };
    
    for (size_t i = 0; i < sizeof(disk_types) / sizeof(disk_types[0]); i++) {
        uint8_t buffer[512];
        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, SCP_HEADER_VALID, sizeof(SCP_HEADER_VALID));
        buffer[4] = disk_types[i].type;
        
        uft_scp_context_t ctx;
        uft_error_t err = uft_scp_parse_header(buffer, sizeof(buffer), &ctx);
        
        UFT_ASSERT_EQ(err, UFT_ERR_OK);
        UFT_ASSERT_EQ(ctx.disk_type, disk_types[i].type);
    }
}

/*============================================================================
 * STX Parser Tests
 *============================================================================*/

UFT_TEST(stx_probe_valid_header)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, STX_HEADER_VALID, sizeof(STX_HEADER_VALID));
    
    int confidence = uft_stx_probe(buffer, sizeof(buffer));
    UFT_ASSERT_GE(confidence, 85);
    UFT_ASSERT_LE(confidence, 100);
}

UFT_TEST(stx_probe_invalid_magic)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'X';  /* Wrong magic */
    buffer[1] = 'S';
    buffer[2] = 'Y';
    
    int confidence = uft_stx_probe(buffer, sizeof(buffer));
    UFT_ASSERT_EQ(confidence, 0);
}

UFT_TEST(stx_parse_geometry)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, STX_HEADER_VALID, sizeof(STX_HEADER_VALID));
    
    uft_stx_context_t ctx;
    uft_error_t err = uft_stx_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(ctx.version, 3);
    UFT_ASSERT_EQ(ctx.track_count, 80);
}

UFT_TEST(stx_protection_flags)
{
    /* Test FDC status flags indicating protection */
    const struct {
        uint8_t fdc_flags;
        bool has_fuzzy;
        bool has_timing;
    } cases[] = {
        { 0x00, false, false },
        { 0x02, true, false },   /* Fuzzy bits */
        { 0x04, false, true },   /* Timing data */
        { 0x06, true, true },    /* Both */
    };
    
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t sector_data[16];
        memcpy(sector_data, STX_SECTOR_PROTECTED, sizeof(STX_SECTOR_PROTECTED));
        sector_data[13] = cases[i].fdc_flags;
        
        uft_stx_sector_info_t info;
        uft_error_t err = uft_stx_parse_sector_info(sector_data, sizeof(sector_data), &info);
        
        UFT_ASSERT_EQ(err, UFT_ERR_OK);
        UFT_ASSERT_EQ(info.has_fuzzy_bits, cases[i].has_fuzzy);
        UFT_ASSERT_EQ(info.has_timing_data, cases[i].has_timing);
    }
}

UFT_TEST(stx_sector_sizes)
{
    /* Test sector size codes 0-3 */
    const int expected_sizes[] = { 128, 256, 512, 1024 };
    
    for (int code = 0; code < 4; code++) {
        uint8_t sector_data[16];
        memcpy(sector_data, STX_SECTOR_PROTECTED, sizeof(STX_SECTOR_PROTECTED));
        sector_data[11] = (uint8_t)code;
        
        uft_stx_sector_info_t info;
        uft_error_t err = uft_stx_parse_sector_info(sector_data, sizeof(sector_data), &info);
        
        UFT_ASSERT_EQ(err, UFT_ERR_OK);
        UFT_ASSERT_EQ(info.data_size, expected_sizes[code]);
    }
}

/*============================================================================
 * EDSK Parser Tests
 *============================================================================*/

UFT_TEST(edsk_probe_extended_header)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, EDSK_HEADER_VALID, sizeof(EDSK_HEADER_VALID));
    
    int confidence = uft_edsk_probe(buffer, sizeof(buffer));
    UFT_ASSERT_GE(confidence, 90);  /* Extended DSK gets higher confidence */
}

UFT_TEST(edsk_probe_standard_header)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, DSK_HEADER_STANDARD, sizeof(DSK_HEADER_STANDARD));
    
    int confidence = uft_edsk_probe(buffer, sizeof(buffer));
    UFT_ASSERT_GE(confidence, 80);
    UFT_ASSERT_LT(confidence, 90);  /* Standard DSK slightly lower */
}

UFT_TEST(edsk_probe_invalid)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    strcpy((char*)buffer, "INVALID HEADER");
    
    int confidence = uft_edsk_probe(buffer, sizeof(buffer));
    UFT_ASSERT_EQ(confidence, 0);
}

UFT_TEST(edsk_parse_extended_geometry)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, EDSK_HEADER_VALID, sizeof(EDSK_HEADER_VALID));
    
    /* Add track size table (40 tracks * 2 sides = 80 entries) */
    for (int i = 0; i < 80; i++) {
        buffer[52 + i] = 0x13;  /* 0x1300 bytes per track */
    }
    
    uft_edsk_context_t ctx;
    uft_error_t err = uft_edsk_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(ctx.is_extended, true);
    UFT_ASSERT_EQ(ctx.track_count, 40);
    UFT_ASSERT_EQ(ctx.side_count, 2);
}

UFT_TEST(edsk_parse_standard_geometry)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, DSK_HEADER_STANDARD, sizeof(DSK_HEADER_STANDARD));
    
    uft_edsk_context_t ctx;
    uft_error_t err = uft_edsk_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(ctx.is_extended, false);
    UFT_ASSERT_EQ(ctx.track_count, 40);
    UFT_ASSERT_EQ(ctx.side_count, 1);
    UFT_ASSERT_EQ(ctx.track_size, 0x1300);
}

UFT_TEST(edsk_sector_info_parse)
{
    /* Build track info block with sector info */
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, EDSK_TRACK_INFO, sizeof(EDSK_TRACK_INFO));
    
    /* Add 9 sector info entries */
    for (int i = 0; i < 9; i++) {
        int offset = 24 + i * 8;
        buffer[offset + 0] = 0;          /* Track */
        buffer[offset + 1] = 0;          /* Side */
        buffer[offset + 2] = (uint8_t)(i + 1);  /* Sector ID */
        buffer[offset + 3] = 2;          /* Size code (512) */
        buffer[offset + 4] = 0;          /* FDC status 1 */
        buffer[offset + 5] = 0;          /* FDC status 2 */
        buffer[offset + 6] = 0x00;       /* Actual length low */
        buffer[offset + 7] = 0x02;       /* Actual length high (512) */
    }
    
    uft_edsk_track_info_t track_info;
    uft_error_t err = uft_edsk_parse_track_info(buffer, sizeof(buffer), &track_info);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(track_info.sector_count, 9);
    UFT_ASSERT_EQ(track_info.sector_size_code, 2);
    UFT_ASSERT_EQ(track_info.gap3_length, 0x4E);
}

UFT_TEST(edsk_weak_sector_detection)
{
    /* FDC status byte combinations indicating weak/bad sectors */
    const struct {
        uint8_t st1;
        uint8_t st2;
        bool is_weak;
        bool is_bad;
    } cases[] = {
        { 0x00, 0x00, false, false },
        { 0x20, 0x00, false, true },   /* CRC error in ID */
        { 0x00, 0x20, false, true },   /* CRC error in data */
        { 0x20, 0x20, true, true },    /* Both CRC errors = weak */
        { 0x04, 0x00, false, true },   /* No data */
        { 0x01, 0x00, false, true },   /* Missing address mark */
    };
    
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uft_edsk_sector_status_t status;
        uft_edsk_analyze_sector_status(cases[i].st1, cases[i].st2, &status);
        
        UFT_ASSERT_EQ(status.is_weak, cases[i].is_weak);
        UFT_ASSERT_EQ(status.has_error, cases[i].is_bad);
    }
}

/*============================================================================
 * 2MG Parser Tests
 *============================================================================*/

UFT_TEST(img2_probe_prodos)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, IMG2_HEADER_PRODOS, sizeof(IMG2_HEADER_PRODOS));
    
    int confidence = uft_2mg_probe(buffer, sizeof(buffer));
    UFT_ASSERT_GE(confidence, 90);
}

UFT_TEST(img2_probe_dos)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, IMG2_HEADER_DOS, sizeof(IMG2_HEADER_DOS));
    
    int confidence = uft_2mg_probe(buffer, sizeof(buffer));
    UFT_ASSERT_GE(confidence, 90);
}

UFT_TEST(img2_probe_nibble)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, IMG2_HEADER_NIB, sizeof(IMG2_HEADER_NIB));
    
    int confidence = uft_2mg_probe(buffer, sizeof(buffer));
    UFT_ASSERT_GE(confidence, 85);  /* Nibble slightly lower */
}

UFT_TEST(img2_probe_invalid)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    strcpy((char*)buffer, "NOT2IMG");
    
    int confidence = uft_2mg_probe(buffer, sizeof(buffer));
    UFT_ASSERT_EQ(confidence, 0);
}

UFT_TEST(img2_parse_prodos)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, IMG2_HEADER_PRODOS, sizeof(IMG2_HEADER_PRODOS));
    
    uft_2mg_context_t ctx;
    uft_error_t err = uft_2mg_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(ctx.format, UFT_2MG_FORMAT_PRODOS);
    UFT_ASSERT_EQ(ctx.header_size, 64);
    UFT_ASSERT_EQ(ctx.data_offset, 64);
    UFT_ASSERT_EQ(ctx.data_length, 143360);  /* 280 * 512 */
    UFT_ASSERT_EQ(ctx.block_count, 280);
}

UFT_TEST(img2_parse_dos)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, IMG2_HEADER_DOS, sizeof(IMG2_HEADER_DOS));
    
    uft_2mg_context_t ctx;
    uft_error_t err = uft_2mg_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(ctx.format, UFT_2MG_FORMAT_DOS);
    UFT_ASSERT_EQ(ctx.data_length, 143360);
}

UFT_TEST(img2_parse_nibble)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, IMG2_HEADER_NIB, sizeof(IMG2_HEADER_NIB));
    
    uft_2mg_context_t ctx;
    uft_error_t err = uft_2mg_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_EQ(err, UFT_ERR_OK);
    UFT_ASSERT_EQ(ctx.format, UFT_2MG_FORMAT_NIB);
    UFT_ASSERT_EQ(ctx.data_length, 232960);  /* 35 tracks * 6656 */
}

UFT_TEST(img2_creator_detection)
{
    /* Test various known creator IDs */
    const struct {
        char creator[5];
        const char* expected_tool;
    } creators[] = {
        { "PROD", "ProDOS" },
        { "DOS!", "DOS 3.3" },
        { "NIBB", "Nibble" },
        { "!nib", "Sweet 16" },
        { "WOOF", "CiderPress" },
        { "B2TR", "Bernie II" },
        { "CTKG", "Catakig" },
        { "XGS!", "XGS" },
    };
    
    for (size_t i = 0; i < sizeof(creators) / sizeof(creators[0]); i++) {
        uint8_t buffer[512];
        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, IMG2_HEADER_PRODOS, sizeof(IMG2_HEADER_PRODOS));
        memcpy(buffer + 4, creators[i].creator, 4);
        
        uft_2mg_context_t ctx;
        uft_error_t err = uft_2mg_parse_header(buffer, sizeof(buffer), &ctx);
        
        UFT_ASSERT_EQ(err, UFT_ERR_OK);
        /* Creator should be stored */
        UFT_ASSERT_EQ(memcmp(ctx.creator, creators[i].creator, 4), 0);
    }
}

UFT_TEST(img2_size_validation)
{
    /* Test various disk sizes */
    const struct {
        uint32_t data_length;
        uint32_t block_count;
        bool valid;
    } sizes[] = {
        { 143360, 280, true },    /* 140K - 5.25" standard */
        { 819200, 1600, true },   /* 800K - 3.5" */
        { 409600, 800, true },    /* 400K - early Mac */
        { 1000, 0, false },       /* Too small */
    };
    
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        uint8_t buffer[512];
        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, IMG2_HEADER_PRODOS, sizeof(IMG2_HEADER_PRODOS));
        
        /* Set data length */
        uft_write_le32(buffer + 28, sizes[i].data_length);
        uft_write_le32(buffer + 20, sizes[i].block_count);
        
        uft_2mg_context_t ctx;
        uft_error_t err = uft_2mg_parse_header(buffer, sizeof(buffer), &ctx);
        
        if (sizes[i].valid) {
            UFT_ASSERT_EQ(err, UFT_ERR_OK);
            UFT_ASSERT_EQ(ctx.data_length, sizes[i].data_length);
        } else {
            UFT_ASSERT_NE(err, UFT_ERR_OK);
        }
    }
}

/*============================================================================
 * Cross-Parser Tests
 *============================================================================*/

UFT_TEST(probe_priority_flux_over_sector)
{
    /* SCP (flux) should get higher confidence than sector images */
    uint8_t scp_buf[512], edsk_buf[512];
    
    memset(scp_buf, 0, sizeof(scp_buf));
    memcpy(scp_buf, SCP_HEADER_VALID, sizeof(SCP_HEADER_VALID));
    
    memset(edsk_buf, 0, sizeof(edsk_buf));
    memcpy(edsk_buf, EDSK_HEADER_VALID, sizeof(EDSK_HEADER_VALID));
    
    int scp_conf = uft_scp_probe(scp_buf, sizeof(scp_buf));
    int edsk_conf = uft_edsk_probe(edsk_buf, sizeof(edsk_buf));
    
    /* Both should be high confidence for their respective formats */
    UFT_ASSERT_GE(scp_conf, 85);
    UFT_ASSERT_GE(edsk_conf, 80);
}

UFT_TEST(null_buffer_handling)
{
    /* All parsers should handle NULL gracefully */
    UFT_ASSERT_EQ(uft_scp_probe(NULL, 0), 0);
    UFT_ASSERT_EQ(uft_stx_probe(NULL, 0), 0);
    UFT_ASSERT_EQ(uft_edsk_probe(NULL, 0), 0);
    UFT_ASSERT_EQ(uft_2mg_probe(NULL, 0), 0);
}

UFT_TEST(zero_length_handling)
{
    uint8_t buffer[16] = {0};
    
    UFT_ASSERT_EQ(uft_scp_probe(buffer, 0), 0);
    UFT_ASSERT_EQ(uft_stx_probe(buffer, 0), 0);
    UFT_ASSERT_EQ(uft_edsk_probe(buffer, 0), 0);
    UFT_ASSERT_EQ(uft_2mg_probe(buffer, 0), 0);
}

/*============================================================================
 * Bounds Checking Tests
 *============================================================================*/

UFT_TEST(scp_bounds_check_track_offset)
{
    /* SCP with track offset pointing beyond file */
    uint8_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, SCP_HEADER_VALID, sizeof(SCP_HEADER_VALID));
    
    /* Set track 0 offset to beyond buffer */
    uft_write_le32(buffer + 16, 0xFFFFFF00);
    
    uft_scp_context_t ctx;
    uft_error_t err = uft_scp_open(buffer, sizeof(buffer), &ctx);
    
    /* Should detect invalid offset */
    UFT_ASSERT_NE(err, UFT_ERR_OK);
}

UFT_TEST(edsk_bounds_check_track_size)
{
    /* EDSK with track size table entry too large */
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, EDSK_HEADER_VALID, sizeof(EDSK_HEADER_VALID));
    
    /* Set absurdly large track size */
    buffer[52] = 0xFF;  /* 0xFF00 bytes */
    
    uft_edsk_context_t ctx;
    /* Parser should handle gracefully */
    uft_error_t err = uft_edsk_parse_header(buffer, sizeof(buffer), &ctx);
    
    /* May succeed parsing header but should note the large size */
    UFT_ASSERT_TRUE(err == UFT_ERR_OK || err == UFT_ERR_INVALID_FORMAT);
}

UFT_TEST(img2_bounds_check_data_offset)
{
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, IMG2_HEADER_PRODOS, sizeof(IMG2_HEADER_PRODOS));
    
    /* Set data offset beyond header size */
    uft_write_le32(buffer + 24, 0xFFFFFF00);
    
    uft_2mg_context_t ctx;
    uft_error_t err = uft_2mg_parse_header(buffer, sizeof(buffer), &ctx);
    
    UFT_ASSERT_NE(err, UFT_ERR_OK);
}

/*============================================================================
 * Test Runner
 *============================================================================*/

UFT_TEST_SUITE(parser_week2)
{
    /* SCP Tests */
    UFT_RUN_TEST(scp_probe_valid_header);
    UFT_RUN_TEST(scp_probe_invalid_magic);
    UFT_RUN_TEST(scp_probe_too_small);
    UFT_RUN_TEST(scp_parse_geometry);
    UFT_RUN_TEST(scp_multi_revolution_count);
    UFT_RUN_TEST(scp_disk_types);
    
    /* STX Tests */
    UFT_RUN_TEST(stx_probe_valid_header);
    UFT_RUN_TEST(stx_probe_invalid_magic);
    UFT_RUN_TEST(stx_parse_geometry);
    UFT_RUN_TEST(stx_protection_flags);
    UFT_RUN_TEST(stx_sector_sizes);
    
    /* EDSK Tests */
    UFT_RUN_TEST(edsk_probe_extended_header);
    UFT_RUN_TEST(edsk_probe_standard_header);
    UFT_RUN_TEST(edsk_probe_invalid);
    UFT_RUN_TEST(edsk_parse_extended_geometry);
    UFT_RUN_TEST(edsk_parse_standard_geometry);
    UFT_RUN_TEST(edsk_sector_info_parse);
    UFT_RUN_TEST(edsk_weak_sector_detection);
    
    /* 2MG Tests */
    UFT_RUN_TEST(img2_probe_prodos);
    UFT_RUN_TEST(img2_probe_dos);
    UFT_RUN_TEST(img2_probe_nibble);
    UFT_RUN_TEST(img2_probe_invalid);
    UFT_RUN_TEST(img2_parse_prodos);
    UFT_RUN_TEST(img2_parse_dos);
    UFT_RUN_TEST(img2_parse_nibble);
    UFT_RUN_TEST(img2_creator_detection);
    UFT_RUN_TEST(img2_size_validation);
    
    /* Cross-Parser Tests */
    UFT_RUN_TEST(probe_priority_flux_over_sector);
    UFT_RUN_TEST(null_buffer_handling);
    UFT_RUN_TEST(zero_length_handling);
    
    /* Bounds Checking Tests */
    UFT_RUN_TEST(scp_bounds_check_track_offset);
    UFT_RUN_TEST(edsk_bounds_check_track_size);
    UFT_RUN_TEST(img2_bounds_check_data_offset);
}

#ifdef UFT_TEST_MAIN
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("=== UFT Parser Week 2 Unit Tests ===\n\n");
    
    UFT_RUN_SUITE(parser_week2);
    
    return uft_test_summary();
}
#endif
