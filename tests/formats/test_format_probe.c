/**
 * @file test_format_probe.c
 * @brief Tests für Format-Erkennung
 */

#include "../uft_test.h"
#include <string.h>

int test_format_probe(void) {
    TEST_BEGIN("Format Probe Tests");
    
    // G64 Magic
    char g64_magic[] = "GCR-1541";
    TEST_ASSERT(memcmp(g64_magic, "GCR-1541", 8) == 0, "G64 magic bytes correct");
    
    // SCP Magic
    char scp_magic[] = "SCP";
    TEST_ASSERT(memcmp(scp_magic, "SCP", 3) == 0, "SCP magic bytes correct");
    
    // HFE Magic
    char hfe_magic[] = "HXCPICFE";
    TEST_ASSERT(memcmp(hfe_magic, "HXCPICFE", 8) == 0, "HFE magic bytes correct");
    
    // IPF Magic (Big-Endian!)
    uint8_t ipf_magic[] = { 'C', 'A', 'P', 'S' };
    uint32_t ipf_val = (ipf_magic[0] << 24) | (ipf_magic[1] << 16) | 
                       (ipf_magic[2] << 8) | ipf_magic[3];
    TEST_ASSERT_EQ(ipf_val, 0x43415053, "IPF magic = CAPS (BE)");
    
    // TD0 Magic
    uint8_t td0_magic[] = { 'T', 'D' };
    uint16_t td0_val = td0_magic[0] | (td0_magic[1] << 8);
    TEST_ASSERT_EQ(td0_val, 0x4454, "TD0 magic = TD (LE)");
    
    // IMD Magic
    char imd_magic[] = "IMD ";
    TEST_ASSERT(memcmp(imd_magic, "IMD ", 4) == 0, "IMD magic correct");
    
    // DSK Magics
    char dsk_std_magic[] = "MV - CPC";
    char dsk_ext_magic[] = "EXTENDED";
    TEST_ASSERT(memcmp(dsk_std_magic, "MV - CPC", 8) == 0, "DSK standard magic");
    TEST_ASSERT(memcmp(dsk_ext_magic, "EXTENDED", 8) == 0, "DSK extended magic");
    
    // MSA Magic
    uint8_t msa_magic[] = { 0x0E, 0x0F };
    TEST_ASSERT(msa_magic[0] == 0x0E && msa_magic[1] == 0x0F, "MSA magic 0E 0F");
    
    // ATR Magic
    uint8_t atr_magic[] = { 0x96, 0x02 };
    uint16_t atr_val = atr_magic[0] | (atr_magic[1] << 8);
    TEST_ASSERT_EQ(atr_val, 0x0296, "ATR magic = 9602 (LE)");
    
    // WOZ1 Magic
    uint8_t woz1_magic[] = { 'W', 'O', 'Z', '1' };
    uint32_t woz1_val = woz1_magic[0] | (woz1_magic[1] << 8) | 
                        (woz1_magic[2] << 16) | (woz1_magic[3] << 24);
    TEST_ASSERT_EQ(woz1_val, 0x315A4F57, "WOZ1 magic (LE)");
    
    // FDI Magic
    char fdi_magic[] = "FDI";
    TEST_ASSERT(memcmp(fdi_magic, "FDI", 3) == 0, "FDI magic correct");
    
    // CQM Magic
    uint8_t cqm_magic[] = { 'C', 'Q', 0x14 };
    TEST_ASSERT(cqm_magic[0] == 'C' && cqm_magic[1] == 'Q' && cqm_magic[2] == 0x14,
                "CQM magic CQ+0x14");
    
    // SAD Magic
    char sad_magic[] = "SAD!";
    TEST_ASSERT(memcmp(sad_magic, "SAD!", 4) == 0, "SAD magic correct");
    
    // G71 Magic
    char g71_magic[] = "GCR-1571";
    TEST_ASSERT(memcmp(g71_magic, "GCR-1571", 8) == 0, "G71 magic correct");
    
    return 0;
}
