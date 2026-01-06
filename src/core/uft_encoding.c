/**
 * @file uft_encoding.c
 * @brief Unified Disk Encoding Implementation (P2-ARCH-003)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_encoding.h"
#include <string.h>

/*===========================================================================
 * Encoding Database
 *===========================================================================*/

static const uft_encoding_info_t g_encoding_db[] = {
    /* Unknown/Raw */
    { UFT_DISK_ENC_UNKNOWN, UFT_ENCCAT_UNKNOWN, "Unknown", "Unknown encoding",
      0, 0, 0, false, true, 0, "" },
    { UFT_DISK_ENC_RAW, UFT_ENCCAT_RAW, "Raw", "Raw flux/bitstream",
      0, 0, 0, false, true, 0, "" },
    
    /* FM */
    { UFT_DISK_ENC_FM, UFT_ENCCAT_FM, "FM", "FM Single Density",
      4000, 125, 1, false, true, 6, "IBM, DEC, HP" },
    { UFT_DISK_ENC_FM_IBM, UFT_ENCCAT_FM, "FM-IBM", "FM IBM 3740",
      4000, 125, 1, false, true, 6, "IBM 3740" },
    { UFT_DISK_ENC_FM_INTEL, UFT_ENCCAT_FM, "FM-Intel", "FM Intel MCS-80",
      4000, 125, 1, false, true, 6, "Intel MCS-80" },
    
    /* MFM */
    { UFT_DISK_ENC_MFM, UFT_ENCCAT_MFM, "MFM", "MFM Double Density",
      2000, 250, 1, false, true, 12, "IBM PC, most systems" },
    { UFT_DISK_ENC_MFM_IBM, UFT_ENCCAT_MFM, "MFM-IBM", "MFM IBM System/34",
      2000, 250, 1, false, true, 12, "IBM System/34" },
    { UFT_DISK_ENC_MFM_HD, UFT_ENCCAT_MFM, "MFM-HD", "MFM High Density",
      1000, 500, 1, false, true, 12, "IBM PC HD, Amiga HD" },
    { UFT_DISK_ENC_MFM_ED, UFT_ENCCAT_MFM, "MFM-ED", "MFM Extra Density",
      500, 1000, 1, false, true, 12, "IBM PC ED (2.88MB)" },
    
    /* Amiga MFM */
    { UFT_DISK_ENC_MFM_AMIGA, UFT_ENCCAT_MFM, "Amiga-MFM", "Amiga MFM",
      2000, 250, 1, false, true, 32, "Amiga 500/1000/2000" },
    { UFT_DISK_ENC_MFM_AMIGA_HD, UFT_ENCCAT_MFM, "Amiga-HD", "Amiga HD MFM",
      1000, 500, 1, false, true, 32, "Amiga 3000/4000" },
    
    /* M2FM */
    { UFT_DISK_ENC_M2FM, UFT_ENCCAT_M2FM, "M2FM", "M2FM Intel iSBC",
      2000, 250, 1, false, true, 8, "Intel iSBC" },
    { UFT_DISK_ENC_M2FM_HP, UFT_ENCCAT_M2FM, "M2FM-HP", "M2FM HP 9895",
      2000, 250, 1, false, true, 8, "HP 9895" },
    
    /* GCR Commodore */
    { UFT_DISK_ENC_GCR_C64, UFT_ENCCAT_GCR, "GCR-C64", "Commodore 64 GCR",
      3200, 156, 0, true, true, 10, "C64, 1541" },
    { UFT_DISK_ENC_GCR_C128, UFT_ENCCAT_GCR, "GCR-C128", "Commodore 128 GCR",
      3200, 156, 0, true, true, 10, "C128, 1571" },
    { UFT_DISK_ENC_GCR_VIC20, UFT_ENCCAT_GCR, "GCR-VIC20", "VIC-20 GCR",
      3200, 156, 0, true, true, 10, "VIC-20" },
    { UFT_DISK_ENC_GCR_1571, UFT_ENCCAT_GCR, "GCR-1571", "1571 GCR",
      3200, 156, 0, true, true, 10, "1571 drive" },
    { UFT_DISK_ENC_GCR_1581, UFT_ENCCAT_MFM, "1581-MFM", "1581 MFM",
      2000, 250, 1, false, true, 12, "1581 drive" },
    
    /* GCR Apple */
    { UFT_DISK_ENC_GCR_APPLE_525, UFT_ENCCAT_GCR, "Apple-GCR", "Apple II 5.25\"",
      4000, 125, 0, false, true, 40, "Apple II" },
    { UFT_DISK_ENC_GCR_APPLE_DOS, UFT_ENCCAT_GCR, "Apple-DOS", "Apple DOS 3.x",
      4000, 125, 0, false, true, 40, "Apple DOS 3.2/3.3" },
    { UFT_DISK_ENC_GCR_APPLE_PRO, UFT_ENCCAT_GCR, "Apple-Pro", "Apple ProDOS",
      4000, 125, 0, false, true, 40, "Apple ProDOS" },
    { UFT_DISK_ENC_GCR_APPLE_35, UFT_ENCCAT_GCR, "Apple-3.5", "Apple 3.5\"",
      2000, 250, 0, true, true, 40, "Apple IIGS, Mac 400K" },
    { UFT_DISK_ENC_GCR_MAC, UFT_ENCCAT_GCR, "Mac-GCR", "Macintosh GCR",
      2000, 250, 0, true, true, 40, "Macintosh 400K/800K" },
    { UFT_DISK_ENC_GCR_MAC_HD, UFT_ENCCAT_MFM, "Mac-HD", "Macintosh HD",
      1000, 500, 1, false, true, 12, "Macintosh 1.44MB" },
    
    /* GCR Other */
    { UFT_DISK_ENC_GCR_VICTOR, UFT_ENCCAT_GCR, "Victor-GCR", "Victor 9000 GCR",
      3000, 166, 0, true, true, 10, "Victor 9000" },
    { UFT_DISK_ENC_GCR_NORTHSTAR, UFT_ENCCAT_GCR, "NorthStar", "NorthStar GCR",
      4000, 125, 0, false, false, 8, "NorthStar" },
    
    /* Japanese */
    { UFT_DISK_ENC_MFM_PC98, UFT_ENCCAT_MFM, "PC-98", "NEC PC-98 MFM",
      2000, 250, 1, false, true, 12, "NEC PC-98" },
    { UFT_DISK_ENC_MFM_X68K, UFT_ENCCAT_MFM, "X68000", "Sharp X68000 MFM",
      2000, 250, 1, false, true, 12, "Sharp X68000" },
    { UFT_DISK_ENC_MFM_FM7, UFT_ENCCAT_MFM, "FM-7", "Fujitsu FM-7 MFM",
      2000, 250, 1, false, true, 12, "Fujitsu FM-7/FM-77" },
    { UFT_DISK_ENC_MFM_MSX, UFT_ENCCAT_MFM, "MSX", "MSX MFM",
      2000, 250, 1, false, true, 12, "MSX, MSX2" },
    
    /* European */
    { UFT_DISK_ENC_MFM_AMSTRAD, UFT_ENCCAT_MFM, "Amstrad", "Amstrad CPC MFM",
      2000, 250, 1, false, true, 12, "Amstrad CPC" },
    { UFT_DISK_ENC_MFM_SPECTRUM, UFT_ENCCAT_MFM, "Spectrum+3", "ZX Spectrum +3",
      2000, 250, 1, false, true, 12, "ZX Spectrum +3" },
    { UFT_DISK_ENC_MFM_SAM, UFT_ENCCAT_MFM, "SAM", "SAM Coupé MFM",
      2000, 250, 1, false, true, 12, "SAM Coupé" },
    { UFT_DISK_ENC_FM_BBC, UFT_ENCCAT_FM, "BBC-FM", "BBC Micro FM",
      4000, 125, 1, false, true, 6, "BBC Micro" },
    { UFT_DISK_ENC_MFM_BBC, UFT_ENCCAT_MFM, "BBC-MFM", "BBC Micro MFM",
      2000, 250, 1, false, true, 12, "BBC Master" },
    { UFT_DISK_ENC_FM_ACORN, UFT_ENCCAT_FM, "Acorn-DFS", "Acorn DFS FM",
      4000, 125, 1, false, true, 6, "Acorn DFS" },
    { UFT_DISK_ENC_MFM_ACORN, UFT_ENCCAT_MFM, "Acorn-ADFS", "Acorn ADFS MFM",
      2000, 250, 1, false, true, 12, "Acorn ADFS" },
    
    /* US */
    { UFT_DISK_ENC_FM_TRS80, UFT_ENCCAT_FM, "TRS-80-FM", "TRS-80 FM",
      4000, 125, 1, false, true, 6, "TRS-80 Model I" },
    { UFT_DISK_ENC_MFM_TRS80, UFT_ENCCAT_MFM, "TRS-80-MFM", "TRS-80 MFM",
      2000, 250, 1, false, true, 12, "TRS-80 Model III/4" },
    { UFT_DISK_ENC_FM_ATARI8, UFT_ENCCAT_FM, "Atari8-FM", "Atari 8-bit FM",
      4000, 125, 1, false, true, 6, "Atari 400/800" },
    { UFT_DISK_ENC_MFM_ATARI8, UFT_ENCCAT_MFM, "Atari8-MFM", "Atari 8-bit MFM",
      2000, 250, 1, false, true, 12, "Atari XL/XE" },
    { UFT_DISK_ENC_MFM_ATARIST, UFT_ENCCAT_MFM, "Atari-ST", "Atari ST MFM",
      2000, 250, 1, false, true, 12, "Atari ST/STE/TT" },
    
    /* Hard Sector */
    { UFT_DISK_ENC_HARDSEC_5, UFT_ENCCAT_HARDSEC, "HardSec-5", "5-sector hard",
      4000, 125, 1, false, false, 0, "Micropolis" },
    { UFT_DISK_ENC_HARDSEC_10, UFT_ENCCAT_HARDSEC, "HardSec-10", "10-sector hard",
      4000, 125, 1, false, false, 0, "NorthStar" },
    { UFT_DISK_ENC_HARDSEC_16, UFT_ENCCAT_HARDSEC, "HardSec-16", "16-sector hard",
      4000, 125, 1, false, false, 0, "Various" },
    
    /* Special */
    { UFT_DISK_ENC_FLUXSTREAM, UFT_ENCCAT_RAW, "Flux", "Raw flux stream",
      0, 0, 0, false, true, 0, "SCP, A2R, KryoFlux" },
    { UFT_DISK_ENC_BITSTREAM, UFT_ENCCAT_RAW, "Bitstream", "Decoded bitstream",
      0, 0, 0, false, true, 0, "HFE" },
    { UFT_DISK_ENC_CUSTOM, UFT_ENCCAT_UNKNOWN, "Custom", "Custom encoding",
      0, 0, 0, false, true, 0, "" },
};

#define ENCODING_DB_SIZE (sizeof(g_encoding_db) / sizeof(g_encoding_db[0]))

/*===========================================================================
 * Lookup Functions
 *===========================================================================*/

static const uft_encoding_info_t* find_encoding(uft_disk_encoding_t enc)
{
    for (size_t i = 0; i < ENCODING_DB_SIZE; i++) {
        if (g_encoding_db[i].encoding == enc) {
            return &g_encoding_db[i];
        }
    }
    return &g_encoding_db[0];  /* Return Unknown */
}

const uft_encoding_info_t* uft_encoding_get_info(uft_disk_encoding_t enc)
{
    return find_encoding(enc);
}

const char* uft_encoding_name(uft_disk_encoding_t enc)
{
    const uft_encoding_info_t *info = find_encoding(enc);
    return info ? info->name : "Unknown";
}

uft_encoding_category_t uft_encoding_category(uft_disk_encoding_t enc)
{
    const uft_encoding_info_t *info = find_encoding(enc);
    return info ? info->category : UFT_ENCCAT_UNKNOWN;
}

const char* uft_encoding_category_name(uft_encoding_category_t cat)
{
    switch (cat) {
        case UFT_ENCCAT_FM:      return "FM";
        case UFT_ENCCAT_MFM:     return "MFM";
        case UFT_ENCCAT_M2FM:    return "M2FM";
        case UFT_ENCCAT_GCR:     return "GCR";
        case UFT_ENCCAT_HARDSEC: return "Hard-Sectored";
        case UFT_ENCCAT_RAW:     return "Raw";
        default:                 return "Unknown";
    }
}

uint32_t uft_encoding_bitcell_ns(uft_disk_encoding_t enc)
{
    const uft_encoding_info_t *info = find_encoding(enc);
    return info ? info->bitcell_ns : 0;
}

uint32_t uft_encoding_data_rate(uft_disk_encoding_t enc)
{
    const uft_encoding_info_t *info = find_encoding(enc);
    return info ? info->data_rate_kbps : 0;
}

/*===========================================================================
 * Legacy Conversion
 *===========================================================================*/

uft_disk_encoding_t uft_encoding_from_decoder(int legacy_enc)
{
    /* Map from uft_encoding_t (uft_unified_decoder.h) */
    switch (legacy_enc) {
        case 0:  return UFT_DISK_ENC_FM;
        case 1:  return UFT_DISK_ENC_MFM;
        case 2:  return UFT_DISK_ENC_MFM_HD;
        case 3:  return UFT_DISK_ENC_MFM_AMIGA;
        case 4:  return UFT_DISK_ENC_GCR_APPLE_525;
        case 5:  return UFT_DISK_ENC_GCR_APPLE_35;
        case 6:  return UFT_DISK_ENC_GCR_C64;
        case 7:  return UFT_DISK_ENC_GCR_C128;
        case 8:  return UFT_DISK_ENC_GCR_VICTOR;
        case 9:  return UFT_DISK_ENC_GCR_MAC;
        case 10: return UFT_DISK_ENC_M2FM;
        case 11: return UFT_DISK_ENC_HARDSEC_10;
        default: return UFT_DISK_ENC_UNKNOWN;
    }
}

int uft_encoding_to_decoder(uft_disk_encoding_t enc)
{
    switch (enc) {
        case UFT_DISK_ENC_FM:
        case UFT_DISK_ENC_FM_IBM:
        case UFT_DISK_ENC_FM_INTEL:
            return 0;
        case UFT_DISK_ENC_MFM:
        case UFT_DISK_ENC_MFM_IBM:
            return 1;
        case UFT_DISK_ENC_MFM_HD:
        case UFT_DISK_ENC_MFM_ED:
            return 2;
        case UFT_DISK_ENC_MFM_AMIGA:
        case UFT_DISK_ENC_MFM_AMIGA_HD:
            return 3;
        case UFT_DISK_ENC_GCR_APPLE_525:
        case UFT_DISK_ENC_GCR_APPLE_DOS:
        case UFT_DISK_ENC_GCR_APPLE_PRO:
            return 4;
        case UFT_DISK_ENC_GCR_APPLE_35:
            return 5;
        case UFT_DISK_ENC_GCR_C64:
        case UFT_DISK_ENC_GCR_VIC20:
            return 6;
        case UFT_DISK_ENC_GCR_C128:
        case UFT_DISK_ENC_GCR_1571:
            return 7;
        case UFT_DISK_ENC_GCR_VICTOR:
            return 8;
        case UFT_DISK_ENC_GCR_MAC:
            return 9;
        case UFT_DISK_ENC_M2FM:
        case UFT_DISK_ENC_M2FM_HP:
            return 10;
        case UFT_DISK_ENC_HARDSEC_10:
            return 11;
        default:
            return -1;
    }
}

uft_disk_encoding_t uft_encoding_from_ir(int ir_enc)
{
    /* Map from uft_ir_encoding_t */
    switch (ir_enc) {
        case 0:  return UFT_DISK_ENC_UNKNOWN;
        case 1:  return UFT_DISK_ENC_FM;
        case 2:  return UFT_DISK_ENC_MFM;
        case 3:  return UFT_DISK_ENC_MFM_HD;
        case 4:  return UFT_DISK_ENC_GCR_C64;
        case 5:  return UFT_DISK_ENC_GCR_APPLE_525;
        case 6:  return UFT_DISK_ENC_GCR_VICTOR;
        case 7:  return UFT_DISK_ENC_MFM_AMIGA;
        case 8:  return UFT_DISK_ENC_GCR_MAC;
        default: return UFT_DISK_ENC_UNKNOWN;
    }
}

int uft_encoding_to_ir(uft_disk_encoding_t enc)
{
    switch (enc) {
        case UFT_DISK_ENC_UNKNOWN:        return 0;
        case UFT_DISK_ENC_FM:
        case UFT_DISK_ENC_FM_IBM:         return 1;
        case UFT_DISK_ENC_MFM:
        case UFT_DISK_ENC_MFM_IBM:        return 2;
        case UFT_DISK_ENC_MFM_HD:         return 3;
        case UFT_DISK_ENC_GCR_C64:
        case UFT_DISK_ENC_GCR_C128:       return 4;
        case UFT_DISK_ENC_GCR_APPLE_525:
        case UFT_DISK_ENC_GCR_APPLE_35:   return 5;
        case UFT_DISK_ENC_GCR_VICTOR:     return 6;
        case UFT_DISK_ENC_MFM_AMIGA:      return 7;
        case UFT_DISK_ENC_GCR_MAC:        return 8;
        default:                          return 0;
    }
}

/*===========================================================================
 * Encoding Detection
 *===========================================================================*/

uft_disk_encoding_t uft_encoding_detect(const uint8_t *bitstream, 
                                         size_t bits, 
                                         float *confidence)
{
    if (!bitstream || bits < 128) {
        if (confidence) *confidence = 0.0f;
        return UFT_DISK_ENC_UNKNOWN;
    }
    
    /* Simple heuristics based on sync patterns */
    size_t bytes = bits / 8;
    
    /* Count potential MFM sync patterns (0xA1 with missing clock) */
    int mfm_sync_count = 0;
    for (size_t i = 0; i + 2 < bytes; i++) {
        if (bitstream[i] == 0x44 && bitstream[i+1] == 0x89) {
            mfm_sync_count++;
        }
    }
    
    /* Count potential FM patterns */
    int fm_pattern_count = 0;
    for (size_t i = 0; i + 1 < bytes; i++) {
        if ((bitstream[i] & 0xAA) == 0xAA) {  /* FM has clocks on every bit */
            fm_pattern_count++;
        }
    }
    
    /* Count potential GCR patterns (no 0x00 or 0xFF typically) */
    int gcr_pattern_count = 0;
    for (size_t i = 0; i < bytes; i++) {
        if (bitstream[i] != 0x00 && bitstream[i] != 0xFF) {
            gcr_pattern_count++;
        }
    }
    
    /* Make decision */
    float conf = 0.5f;
    uft_disk_encoding_t detected = UFT_DISK_ENC_UNKNOWN;
    
    if (mfm_sync_count > 5) {
        detected = UFT_DISK_ENC_MFM;
        conf = 0.7f + 0.1f * (mfm_sync_count > 10 ? 1.0f : 0.0f);
    } else if (fm_pattern_count > (int)(bytes / 2)) {
        detected = UFT_DISK_ENC_FM;
        conf = 0.6f;
    } else if (gcr_pattern_count > (int)(bytes * 0.9)) {
        detected = UFT_DISK_ENC_GCR_C64;  /* Default GCR guess */
        conf = 0.5f;
    }
    
    if (confidence) *confidence = conf;
    return detected;
}
