/**
 * @file uft_hxc_formats.c
 * @brief HxC Format Detection Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * Source: HxCFloppyEmulator format loaders
 */

#include <string.h>
#include <ctype.h>

#include "uft/formats/uft_hxc_formats.h"

/*============================================================================
 * Format Signatures
 *============================================================================*/

typedef struct {
    uft_format_type_t type;
    const char* name;
    const uint8_t* signature;
    size_t sig_len;
    size_t sig_offset;
} format_signature_t;

/* Format signatures */
static const uint8_t sig_woz[] = { 'W', 'O', 'Z' };
static const uint8_t sig_scp[] = { 'S', 'C', 'P' };
static const uint8_t sig_ipf[] = { 'C', 'A', 'P', 'S' };
static const uint8_t sig_hfe_v1[] = { 'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E' };
static const uint8_t sig_hfe_v3[] = { 'H', 'X', 'C', 'H', 'F', 'E', 'V', '3' };
static const uint8_t sig_stx[] = { 'R', 'S', 'Y', 0 };
static const uint8_t sig_imd[] = { 'I', 'M', 'D', ' ' };
static const uint8_t sig_td0[] = { 'T', 'D' };
static const uint8_t sig_td0_adv[] = { 't', 'd' };
static const uint8_t sig_dms[] = { 'D', 'M', 'S', '!' };
static const uint8_t sig_a2r[] = { 'A', '2', 'R' };
static const uint8_t sig_2mg[] = { '2', 'I', 'M', 'G' };
static const uint8_t sig_afi[] = { 'A', 'F', 'I', 26 };
static const uint8_t sig_mfm[] = { 'M', 'F', 'M', 0 };

static const format_signature_t signatures[] = {
    /* Preservation formats */
    { UFT_FORMAT_WOZ,       "WOZ",              sig_woz,    3, 0 },
    { UFT_FORMAT_SCP,       "SCP",              sig_scp,    3, 0 },
    { UFT_FORMAT_IPF,       "IPF",              sig_ipf,    4, 0 },
    { UFT_FORMAT_A2R,       "A2R",              sig_a2r,    3, 0 },
    
    /* HxC formats */
    { UFT_FORMAT_HFE,       "HFE v1",           sig_hfe_v1, 8, 0 },
    { UFT_FORMAT_HFE_V3,    "HFE v3",           sig_hfe_v3, 8, 0 },
    { UFT_FORMAT_AFI,       "AFI",              sig_afi,    4, 0 },
    { UFT_FORMAT_MFM,       "MFM",              sig_mfm,    4, 0 },
    
    /* Atari */
    { UFT_FORMAT_STX,       "STX (Pasti)",      sig_stx,    4, 0 },
    
    /* Amiga */
    { UFT_FORMAT_DMS,       "DMS",              sig_dms,    4, 0 },
    
    /* PC/IBM */
    { UFT_FORMAT_IMD,       "IMD",              sig_imd,    4, 0 },
    { UFT_FORMAT_TD0,       "TD0",              sig_td0,    2, 0 },
    { UFT_FORMAT_TD0,       "TD0 (ADV)",        sig_td0_adv,2, 0 },
    
    /* Apple */
    { UFT_FORMAT_2MG,       "2MG",              sig_2mg,    4, 0 },
    
    /* End marker */
    { UFT_FORMAT_UNKNOWN,   NULL,               NULL,       0, 0 }
};

/*============================================================================
 * Format Names
 *============================================================================*/

static const struct {
    uft_format_type_t type;
    const char* name;
} format_names[] = {
    /* Apple */
    { UFT_FORMAT_WOZ,       "WOZ" },
    { UFT_FORMAT_WOZ_V1,    "WOZ v1" },
    { UFT_FORMAT_WOZ_V2,    "WOZ v2" },
    { UFT_FORMAT_WOZ_V3,    "WOZ v3" },
    { UFT_FORMAT_NIB,       "NIB" },
    { UFT_FORMAT_DO,        "DOS Order" },
    { UFT_FORMAT_PO,        "ProDOS Order" },
    { UFT_FORMAT_2MG,       "2MG" },
    
    /* Preservation */
    { UFT_FORMAT_SCP,       "SuperCard Pro" },
    { UFT_FORMAT_IPF,       "IPF (CAPS/SPS)" },
    { UFT_FORMAT_KRYOFLUX,  "KryoFlux Stream" },
    { UFT_FORMAT_A2R,       "Applesauce" },
    
    /* Commodore */
    { UFT_FORMAT_D64,       "D64" },
    { UFT_FORMAT_G64,       "G64" },
    { UFT_FORMAT_D81,       "D81" },
    { UFT_FORMAT_D71,       "D71" },
    { UFT_FORMAT_D80,       "D80" },
    { UFT_FORMAT_D82,       "D82" },
    
    /* Amiga */
    { UFT_FORMAT_ADF,       "ADF" },
    { UFT_FORMAT_ADZ,       "ADZ (gzipped ADF)" },
    { UFT_FORMAT_DMS,       "DMS" },
    { UFT_FORMAT_FDI,       "FDI" },
    
    /* Atari */
    { UFT_FORMAT_STX,       "STX (Pasti)" },
    { UFT_FORMAT_ST,        "ST" },
    { UFT_FORMAT_MSA,       "MSA" },
    
    /* TRS-80 */
    { UFT_FORMAT_DMK,       "DMK" },
    { UFT_FORMAT_JV1,       "JV1" },
    { UFT_FORMAT_JV3,       "JV3" },
    
    /* PC/IBM */
    { UFT_FORMAT_IMD,       "ImageDisk" },
    { UFT_FORMAT_IMG,       "IMG" },
    { UFT_FORMAT_TD0,       "TeleDisk" },
    { UFT_FORMAT_DSK,       "DSK" },
    
    /* HxC */
    { UFT_FORMAT_HFE,       "HFE" },
    { UFT_FORMAT_HFE_V3,    "HFE v3" },
    { UFT_FORMAT_MFM,       "MFM" },
    { UFT_FORMAT_AFI,       "AFI" },
    
    /* Other */
    { UFT_FORMAT_RAW,       "RAW" },
    { UFT_FORMAT_FLUX,      "Flux" },
    { UFT_FORMAT_UNKNOWN,   "Unknown" },
    
    { 0, NULL }
};

/*============================================================================
 * Size-Based Detection
 *============================================================================*/

/**
 * @brief Detect format by file size
 */
static uft_format_type_t detect_by_size(size_t file_size)
{
    /* Common disk image sizes */
    switch (file_size) {
        /* Commodore */
        case 174848:    return UFT_FORMAT_D64;  /* 35 tracks */
        case 175531:    return UFT_FORMAT_D64;  /* 35 tracks + errors */
        case 196608:    return UFT_FORMAT_D64;  /* 40 tracks */
        case 197376:    return UFT_FORMAT_D64;  /* 40 tracks + errors */
        case 349696:    return UFT_FORMAT_D71;
        case 819200:    return UFT_FORMAT_D81;
        case 533248:    return UFT_FORMAT_D80;
        case 1066496:   return UFT_FORMAT_D82;
        
        /* Amiga */
        case 901120:    return UFT_FORMAT_ADF;  /* DD */
        case 1802240:   return UFT_FORMAT_ADF;  /* HD */
        
        /* Atari ST */
        case 737280:    return UFT_FORMAT_ST;   /* SS/DD 80 tracks */
        case 368640:    return UFT_FORMAT_ST;   /* SS/DD 40 tracks */
        
        /* PC */
        case 163840:    return UFT_FORMAT_IMG;  /* 160KB SS/DD */
        case 184320:    return UFT_FORMAT_IMG;  /* 180KB SS/DD */
        case 327680:    return UFT_FORMAT_IMG;  /* 320KB DS/DD */
        case 360448:    return UFT_FORMAT_IMG;  /* 360KB DS/DD */
        case 655360:    return UFT_FORMAT_IMG;  /* 640KB */
        case 737280:    return UFT_FORMAT_IMG;  /* 720KB */
        case 1228800:   return UFT_FORMAT_IMG;  /* 1.2MB */
        case 1474560:   return UFT_FORMAT_IMG;  /* 1.44MB */
        case 2949120:   return UFT_FORMAT_IMG;  /* 2.88MB ED */
        
        default:
            return UFT_FORMAT_UNKNOWN;
    }
}

/*============================================================================
 * API Implementation
 *============================================================================*/

bool uft_hxc_detect_format(const uint8_t* data, size_t size,
                           uft_format_detect_t* result)
{
    if (!data || !result || size < 4) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    result->type = UFT_FORMAT_UNKNOWN;
    result->confidence = 0;
    strcpy(result->name, "Unknown");
    
    /* Check signatures */
    for (const format_signature_t* sig = signatures; sig->name; sig++) {
        if (sig->sig_offset + sig->sig_len <= size) {
            if (memcmp(data + sig->sig_offset, sig->signature, sig->sig_len) == 0) {
                result->type = sig->type;
                result->confidence = 100;
                strncpy(result->name, sig->name, 31);
                result->name[31] = '\0';
                memcpy(&result->magic, data, sizeof(uint32_t));
                
                /* Get version for WOZ */
                if (sig->type == UFT_FORMAT_WOZ && size >= 4) {
                    result->version = data[3] - '0';
                    if (result->version == 1) result->type = UFT_FORMAT_WOZ_V1;
                    else if (result->version == 2) result->type = UFT_FORMAT_WOZ_V2;
                    else if (result->version == 3) result->type = UFT_FORMAT_WOZ_V3;
                }
                
                return true;
            }
        }
    }
    
    /* Check DMK header (no signature, but specific header structure) */
    if (size >= 16) {
        uint8_t wp = data[0];
        uint8_t tracks = data[1];
        uint16_t track_len = data[2] | (data[3] << 8);
        uint8_t flags = data[4];
        
        /* DMK heuristics */
        if ((wp == 0x00 || wp == 0xFF) &&
            (tracks > 0 && tracks <= 96) &&
            (track_len >= 0x1900 && track_len <= 0x4E00) &&
            (flags & 0x0F) == 0) {
            result->type = UFT_FORMAT_DMK;
            result->confidence = 70;
            strcpy(result->name, "DMK");
            return true;
        }
    }
    
    /* Check G64 by size and content */
    if (size >= 12) {
        if (data[0] == 'G' && data[1] == 'C' && data[2] == 'R' && data[3] == '-') {
            result->type = UFT_FORMAT_G64;
            result->confidence = 100;
            strcpy(result->name, "G64");
            return true;
        }
    }
    
    /* Check KryoFlux stream by filename pattern (would need filename) */
    /* For now, mark as unknown */
    
    /* Low confidence - check common sizes */
    uft_format_type_t size_type = detect_by_size(size);
    if (size_type != UFT_FORMAT_UNKNOWN) {
        result->type = size_type;
        result->confidence = 50;  /* Lower confidence for size-only detection */
        strncpy(result->name, uft_hxc_format_name(size_type), 31);
        result->name[31] = '\0';
        return true;
    }
    
    return false;
}

const char* uft_hxc_format_name(uft_format_type_t type)
{
    for (int i = 0; format_names[i].name; i++) {
        if (format_names[i].type == type) {
            return format_names[i].name;
        }
    }
    return "Unknown";
}

bool uft_hxc_is_flux_format(uft_format_type_t type)
{
    switch (type) {
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_KRYOFLUX:
        case UFT_FORMAT_A2R:
        case UFT_FORMAT_HFE_V3:
        case UFT_FORMAT_FLUX:
        case UFT_FORMAT_WOZ_V3:
            return true;
        default:
            return false;
    }
}

bool uft_hxc_is_preservation_format(uft_format_type_t type)
{
    switch (type) {
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_IPF:
        case UFT_FORMAT_KRYOFLUX:
        case UFT_FORMAT_A2R:
        case UFT_FORMAT_WOZ:
        case UFT_FORMAT_WOZ_V1:
        case UFT_FORMAT_WOZ_V2:
        case UFT_FORMAT_WOZ_V3:
        case UFT_FORMAT_G64:
        case UFT_FORMAT_STX:
        case UFT_FORMAT_HFE_V3:
            return true;
        default:
            return false;
    }
}
