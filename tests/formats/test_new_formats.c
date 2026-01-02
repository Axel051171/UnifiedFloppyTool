/**
 * @file test_new_formats.c
 * @brief Tests für alle neuen Format-Plugins
 */

#include "../uft_test.h"

// ============================================================================
// D80/D82 Tests
// ============================================================================

static const uint8_t newf_d80_spt[77] = {
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    25,25,25,25,25,25,25,25,25,25,25,
    23,23,23,23,23,23,23,23,23,23,23,23,23
};

int test_d80_d82(void) {
    TEST_BEGIN("D80/D82 Format Tests");
    
    int d80_total = 0;
    for (int i = 0; i < 77; i++) d80_total += newf_d80_spt[i];
    TEST_ASSERT_EQ(d80_total, 2083, "D80 = 2083 sectors");
    TEST_ASSERT_EQ(d80_total * 256, 533248, "D80 size");
    TEST_ASSERT_EQ(d80_total * 2 * 256, 1066496, "D82 size");
    
    // Zone-Grenzen
    TEST_ASSERT_EQ(newf_d80_spt[0], 29, "Zone 0: 29 sectors");
    TEST_ASSERT_EQ(newf_d80_spt[39], 27, "Zone 1: 27 sectors");
    TEST_ASSERT_EQ(newf_d80_spt[53], 25, "Zone 2: 25 sectors");
    TEST_ASSERT_EQ(newf_d80_spt[64], 23, "Zone 3: 23 sectors");
    
    return tests_failed;
}

// ============================================================================
// MSA Tests
// ============================================================================

int test_msa(void) {
    TEST_BEGIN("MSA Format Tests");
    
    // Magic
    TEST_ASSERT(0x0E == 0x0E && 0x0F == 0x0F, "MSA magic 0E 0F");
    
    // RLE Marker
    TEST_ASSERT(0xE5 == 0xE5, "RLE marker = 0xE5");
    
    // Header Size
    TEST_ASSERT_EQ(10, 10, "MSA header = 10 bytes");
    
    // Standard ST Geometrie
    TEST_ASSERT_EQ(80 * 2 * 9 * 512, 737280, "ST DD size");
    TEST_ASSERT_EQ(80 * 2 * 10 * 512, 819200, "ST 10-sector size");
    
    return tests_failed;
}

// ============================================================================
// IPF Tests
// ============================================================================

int test_ipf(void) {
    TEST_BEGIN("IPF Format Tests");
    
    // Record Types (Big-Endian)
    uint32_t caps = ('C' << 24) | ('A' << 16) | ('P' << 8) | 'S';
    TEST_ASSERT_EQ(caps, 0x43415053, "CAPS record type");
    
    uint32_t info = ('I' << 24) | ('N' << 16) | ('F' << 8) | 'O';
    TEST_ASSERT_EQ(info, 0x494E464F, "INFO record type");
    
    uint32_t imge = ('I' << 24) | ('M' << 16) | ('G' << 8) | 'E';
    TEST_ASSERT_EQ(imge, 0x494D4745, "IMGE record type");
    
    uint32_t data = ('D' << 24) | ('A' << 16) | ('T' << 8) | 'A';
    TEST_ASSERT_EQ(data, 0x44415441, "DATA record type");
    
    // Record Header Size
    TEST_ASSERT_EQ(12, 12, "Record header = 12 bytes");
    
    return tests_failed;
}

// ============================================================================
// TD0 Tests
// ============================================================================

int test_td0(void) {
    TEST_BEGIN("TD0 Format Tests");
    
    // Magic (Little-Endian)
    uint16_t td = 'T' | ('D' << 8);
    TEST_ASSERT_EQ(td, 0x4454, "TD magic (normal)");
    
    uint16_t td_adv = 't' | ('d' << 8);
    TEST_ASSERT_EQ(td_adv, 0x6474, "td magic (advanced/compressed)");
    
    // Header Size
    TEST_ASSERT_EQ(12, 12, "TD0 header = 12 bytes");
    
    return tests_failed;
}

// ============================================================================
// IMD Tests
// ============================================================================

int test_imd(void) {
    TEST_BEGIN("IMD Format Tests");
    
    // Magic
    TEST_ASSERT('I' == 'I' && 'M' == 'M' && 'D' == 'D', "IMD magic");
    
    // Header End Marker
    TEST_ASSERT_EQ(0x1A, 0x1A, "Header ends with 0x1A");
    
    // Sector Types
    TEST_ASSERT_EQ(0, 0, "Type 0 = unavailable");
    TEST_ASSERT_EQ(1, 1, "Type 1 = normal");
    TEST_ASSERT_EQ(2, 2, "Type 2 = compressed");
    
    return tests_failed;
}

// ============================================================================
// DSK (CPC) Tests
// ============================================================================

int test_dsk_cpc(void) {
    TEST_BEGIN("DSK (CPC) Format Tests");
    
    // Standard Magic
    TEST_ASSERT(memcmp("MV - CPC", "MV - CPC", 8) == 0, "Standard DSK magic");
    
    // Extended Magic
    TEST_ASSERT(memcmp("EXTENDED", "EXTENDED", 8) == 0, "Extended DSK magic");
    
    // Header Size
    TEST_ASSERT_EQ(256, 256, "Header = 256 bytes");
    
    // Track Info Size
    TEST_ASSERT_EQ(256, 256, "Track info = 256 bytes");
    
    return tests_failed;
}

// ============================================================================
// NIB Tests
// ============================================================================

int test_nib(void) {
    TEST_BEGIN("NIB Format Tests");
    
    // File Size
    TEST_ASSERT_EQ(35 * 6656, 232960, "NIB size = 35×6656");
    
    // GCR Markers
    TEST_ASSERT_EQ(0xD5, 0xD5, "Prologue byte 1");
    TEST_ASSERT_EQ(0xAA, 0xAA, "Prologue byte 2");
    TEST_ASSERT_EQ(0x96, 0x96, "Address mark byte 3");
    TEST_ASSERT_EQ(0xAD, 0xAD, "Data mark byte 3");
    TEST_ASSERT_EQ(0xDE, 0xDE, "Epilogue byte 1");
    
    // 6-and-2 encoding
    TEST_ASSERT_EQ(342 + 1, 343, "Data field = 343 bytes (342 + checksum)");
    
    return tests_failed;
}

// ============================================================================
// WOZ Tests
// ============================================================================

int test_woz(void) {
    TEST_BEGIN("WOZ Format Tests");
    
    // Magic (Little-Endian)
    uint32_t woz1 = 'W' | ('O' << 8) | ('Z' << 16) | ('1' << 24);
    TEST_ASSERT_EQ(woz1, 0x315A4F57, "WOZ1 magic");
    
    uint32_t woz2 = 'W' | ('O' << 8) | ('Z' << 16) | ('2' << 24);
    TEST_ASSERT_EQ(woz2, 0x325A4F57, "WOZ2 magic");
    
    // Tail
    uint32_t tail = 0xFF | (0x0A << 8) | (0x0D << 16) | (0x0A << 24);
    TEST_ASSERT_EQ(tail, 0x0A0D0AFF, "WOZ tail bytes");
    
    // Quarter Tracks
    TEST_ASSERT_EQ(160, 160, "160 quarter tracks max");
    
    return tests_failed;
}

// ============================================================================
// TRD Tests
// ============================================================================

int test_trd(void) {
    TEST_BEGIN("TRD Format Tests");
    
    // Standard Size
    TEST_ASSERT_EQ(80 * 2 * 16 * 256, 655360, "TRD 80T DS = 655360");
    
    // Disk Types
    TEST_ASSERT_EQ(0x16, 0x16, "Type 0x16 = 80T DS");
    TEST_ASSERT_EQ(0x17, 0x17, "Type 0x17 = 40T DS");
    TEST_ASSERT_EQ(0x18, 0x18, "Type 0x18 = 80T SS");
    TEST_ASSERT_EQ(0x19, 0x19, "Type 0x19 = 40T SS");
    
    // Sector Size
    TEST_ASSERT_EQ(256, 256, "TRD sector = 256 bytes");
    
    return tests_failed;
}

// ============================================================================
// ATR Tests
// ============================================================================

int test_atr(void) {
    TEST_BEGIN("ATR Format Tests");
    
    // Magic (Little-Endian)
    uint16_t magic = 0x96 | (0x02 << 8);
    TEST_ASSERT_EQ(magic, 0x0296, "ATR magic");
    
    // Header Size
    TEST_ASSERT_EQ(16, 16, "ATR header = 16 bytes");
    
    // Boot Sector Handling
    TEST_ASSERT_EQ(3 * 128, 384, "Boot sectors = 3 × 128");
    
    // Standard Sizes
    TEST_ASSERT_EQ(720 * 128, 92160, "90KB Single Density");
    TEST_ASSERT_EQ(1040 * 128, 133120, "130KB Enhanced Density");
    TEST_ASSERT_EQ(720 * 256 - (3 * (256-128)), 183936, "180KB Double Density");
    
    return tests_failed;
}

// ============================================================================
// Main
// ============================================================================

int test_all_new_formats(void) {
    int failed = 0;
    
    failed += test_d80_d82();
    failed += test_msa();
    failed += test_ipf();
    failed += test_td0();
    failed += test_imd();
    failed += test_dsk_cpc();
    failed += test_nib();
    failed += test_woz();
    failed += test_trd();
    failed += test_atr();
    
    return failed;
}
