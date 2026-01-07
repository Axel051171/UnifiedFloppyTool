/**
 * @file uft_format_detect_complete.c
 * @brief Complete Format Detection with Version Recognition
 * 
 * Detects ALL disk image formats with:
 * - Magic byte verification
 * - Size-based heuristics
 * - Structure validation
 * - Version identification
 * - Confidence scoring
 * 
 * @version 5.31.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Format Definitions
 * ============================================================================ */

typedef enum {
    /* Unknown */
    UFT_FMT_UNKNOWN = 0,
    
    /* Amiga Formats */
    UFT_FMT_ADF,            /* Amiga Disk File */
    UFT_FMT_ADZ,            /* Compressed ADF (gzip) */
    UFT_FMT_DMS,            /* Disk Masher System */
    UFT_FMT_IPF,            /* Interchangeable Preservation Format */
    UFT_FMT_ADF_EXT,        /* Extended ADF (MFM) */
    
    /* Commodore Formats */
    UFT_FMT_D64,            /* 1541 Disk Image */
    UFT_FMT_D71,            /* 1571 Double-Sided */
    UFT_FMT_D81,            /* 1581 3.5" Disk */
    UFT_FMT_D80,            /* 8050 Disk */
    UFT_FMT_D82,            /* 8250 Disk */
    UFT_FMT_G64,            /* GCR-Level Image */
    UFT_FMT_G71,            /* GCR 1571 */
    UFT_FMT_NIB,            /* Nibbler Raw */
    UFT_FMT_NBZ,            /* Compressed NIB */
    UFT_FMT_P64,            /* NUFLI Image */
    UFT_FMT_X64,            /* Extended D64 */
    UFT_FMT_T64,            /* Tape Image */
    UFT_FMT_TAP_C64,        /* C64 Tape */
    
    /* Apple II Formats */
    UFT_FMT_DO,             /* DOS 3.3 Order */
    UFT_FMT_PO,             /* ProDOS Order */
    UFT_FMT_NIB_APPLE,      /* Apple Nibble */
    UFT_FMT_2MG,            /* Universal Disk Image */
    UFT_FMT_WOZ,            /* Applesauce Flux */
    UFT_FMT_A2R,            /* Applesauce Raw */
    UFT_FMT_DSK_APPLE,      /* Apple DSK */
    UFT_FMT_DC42,           /* Disk Copy 4.2 */
    
    /* Atari Formats */
    UFT_FMT_ATR,            /* Atari 8-Bit */
    UFT_FMT_ATX,            /* VAPI Protected */
    UFT_FMT_XFD,            /* XFormer Raw */
    UFT_FMT_DCM,            /* DiskComm */
    UFT_FMT_PRO,            /* APE Pro Image */
    UFT_FMT_ST,             /* Atari ST Raw */
    UFT_FMT_MSA,            /* Magic Shadow Archiver */
    UFT_FMT_STX,            /* Pasti */
    UFT_FMT_DIM,            /* DIM Image */
    
    /* PC Formats */
    UFT_FMT_IMG,            /* Raw Sector Image */
    UFT_FMT_IMA,            /* DOS Floppy Image */
    UFT_FMT_IMD,            /* ImageDisk */
    UFT_FMT_TD0,            /* Teledisk */
    UFT_FMT_FDI,            /* Formatted Disk Image */
    UFT_FMT_DSK_CPC,        /* CPC DSK */
    UFT_FMT_EDSK,           /* Extended DSK */
    UFT_FMT_MFM,            /* HxC MFM Stream */
    UFT_FMT_ISO,            /* ISO Image */
    UFT_FMT_VFD,            /* Virtual Floppy */
    
    /* Flux Formats */
    UFT_FMT_SCP,            /* SuperCard Pro */
    UFT_FMT_HFE,            /* UFT HFE Format */
    UFT_FMT_HFE_V3,         /* HxC v3 */
    UFT_FMT_DMK,            /* TRS-80 DMK */
    UFT_FMT_CT,             /* CatWeasel */
    UFT_FMT_RAW_FLUX,       /* Raw Flux Data */
    
    /* ZX Spectrum Formats */
    UFT_FMT_TRD,            /* TR-DOS */
    UFT_FMT_SCL,            /* Sinclair SCL */
    UFT_FMT_FDD,            /* Spectrum +3 */
    UFT_FMT_OPD,            /* Opus Discovery */
    UFT_FMT_MGT,            /* MGT +D */
    UFT_FMT_TAP,            /* Spectrum Tape */
    UFT_FMT_TZX,            /* Tape Extension */
    
    /* BBC/Acorn Formats */
    UFT_FMT_SSD,            /* Single-Sided DFS */
    UFT_FMT_DSD,            /* Double-Sided DFS */
    UFT_FMT_ADF_ACORN,      /* Acorn ADFS */
    UFT_FMT_ADL,            /* ADFS Large */
    UFT_FMT_UEF,            /* Unified Emulator Format */
    
    /* TRS-80 Formats */
    UFT_FMT_JV1,            /* JV1 */
    UFT_FMT_JV3,            /* JV3 */
    UFT_FMT_DMK_TRS,        /* DMK TRS-80 */
    
    /* Japanese PC Formats */
    UFT_FMT_D88,            /* PC-88/PC-98/X1 */
    UFT_FMT_NFD,            /* PC-98 NFD */
    UFT_FMT_FDI_PC98,       /* PC-98 FDI */
    UFT_FMT_HDM,            /* PC-98 HDM */
    UFT_FMT_XDF,            /* X68000 XDF */
    UFT_FMT_DIM_X68,        /* X68000 DIM */
    
    /* SAM Coupé */
    UFT_FMT_SAD,            /* SAM ADisk */
    UFT_FMT_SDF,            /* SAM Disk File */
    UFT_FMT_MGT_SAM,        /* SAM MGT */
    
    /* Other Systems */
    UFT_FMT_DSK_MSX,        /* MSX DSK */
    UFT_FMT_CAS,            /* MSX Cassette */
    UFT_FMT_DSK_CPC464,     /* CPC 464 DSK */
    UFT_FMT_TI99,           /* TI-99/4A */
    UFT_FMT_DSK_ORIC,       /* Oric DSK */
    UFT_FMT_DSK_DRAGON,     /* Dragon 32/64 */
    UFT_FMT_VDK,            /* Virtual Disk (Dragon) */
    UFT_FMT_OS9,            /* OS-9 */
    
    UFT_FMT_COUNT
} uft_format_t;

typedef struct {
    uft_format_t format;
    char         name[32];
    char         version[32];
    char         system[32];
    char         description[128];
    int          confidence;    /* 0-100 */
    
    /* Detected parameters */
    int          tracks;
    int          sides;
    int          sectors;
    int          sector_size;
    uint64_t     file_size;
    
    /* Flags */
    bool         has_errors;
    bool         write_protected;
    bool         compressed;
    bool         copy_protected;
} uft_format_info_t;

/* ============================================================================
 * Magic Bytes Definitions
 * ============================================================================ */

typedef struct {
    uft_format_t format;
    size_t       offset;
    size_t       length;
    uint8_t      magic[16];
    const char  *name;
    const char  *version;
} magic_entry_t;

static const magic_entry_t MAGIC_TABLE[] = {
    /* Amiga */
    { UFT_FMT_DMS,      0, 4, {'D','M','S','!'}, "DMS", "" },
    { UFT_FMT_IPF,      0, 4, {'C','A','P','S'}, "IPF", "CAPS" },
    
    /* Apple II */
    { UFT_FMT_2MG,      0, 4, {'2','I','M','G'}, "2IMG", "" },
    { UFT_FMT_WOZ,      0, 4, {'W','O','Z','1'}, "WOZ", "1.0" },
    { UFT_FMT_WOZ,      0, 4, {'W','O','Z','2'}, "WOZ", "2.0" },
    { UFT_FMT_A2R,      0, 4, {'A','2','R','2'}, "A2R", "2.0" },
    { UFT_FMT_A2R,      0, 4, {'A','2','R','3'}, "A2R", "3.0" },
    { UFT_FMT_DC42,     0, 4, {0x00,0x00,0x01,0x00}, "DC42", "4.2" },
    
    /* Commodore */
    { UFT_FMT_G64,      0, 8, {'G','C','R','-','1','5','4','1'}, "G64", "1541" },
    { UFT_FMT_G71,      0, 8, {'G','C','R','-','1','5','7','1'}, "G71", "1571" },
    { UFT_FMT_P64,      0, 4, {'P','6','4','-'}, "P64", "" },
    { UFT_FMT_X64,      0, 4, {'C','1','5','4'}, "X64", "C1541" },
    { UFT_FMT_T64,      0, 20, {'C','6','4',' ','t','a','p','e'}, "T64", "" },
    { UFT_FMT_TAP_C64,  0, 12, {'C','6','4','-','T','A','P','E'}, "TAP", "C64" },
    
    /* Atari */
    { UFT_FMT_ATR,      0, 2, {0x96,0x02}, "ATR", "NICKATARI" },
    { UFT_FMT_ATX,      0, 4, {'A','T','8','X'}, "ATX", "VAPI" },
    { UFT_FMT_DCM,      0, 3, {0xF9,0x41,0x00}, "DCM", "" },
    { UFT_FMT_PRO,      0, 16, {'P','R','O','\0'}, "PRO", "APE" },
    { UFT_FMT_MSA,      0, 2, {0x0E,0x0F}, "MSA", "" },
    { UFT_FMT_STX,      0, 4, {'R','S','Y',0x00}, "STX", "Pasti" },
    
    /* PC/CPC */
    { UFT_FMT_IMD,      0, 4, {'I','M','D',' '}, "IMD", "" },
    { UFT_FMT_TD0,      0, 2, {'T','D'}, "TD0", "Normal" },
    { UFT_FMT_TD0,      0, 2, {'t','d'}, "TD0", "Advanced" },
    { UFT_FMT_FDI,      0, 3, {'F','D','I'}, "FDI", "" },
    { UFT_FMT_DSK_CPC,  0, 8, {'M','V',' ','-',' ','C','P','C'}, "DSK", "MV-CPC" },
    { UFT_FMT_EDSK,     0, 8, {'E','X','T','E','N','D','E','D'}, "EDSK", "Extended" },
    { UFT_FMT_MFM,      0, 7, {'H','X','C','M','F','M','\0'}, "MFM", "HxC" },
    
    /* Flux */
    { UFT_FMT_SCP,      0, 3, {'S','C','P'}, "SCP", "" },
    { UFT_FMT_HFE,      0, 8, {'H','X','C','P','I','C','F','E'}, "HFE", "v1" },
    { UFT_FMT_HFE_V3,   0, 8, {'H','X','C','H','F','E','V','3'}, "HFE", "v3" },
    { UFT_FMT_DMK,      0, 1, {0x00}, "DMK", "" },  /* Needs size check */
    { UFT_FMT_FLX,      0, 4, {'F','L','U','X'}, "FLX", "" },
    
    /* Spectrum */
    { UFT_FMT_TZX,      0, 7, {'Z','X','T','a','p','e','!'}, "TZX", "" },
    { UFT_FMT_FDD,      0, 8, {'F','D','D','\0','\0','\0','\0','\0'}, "FDD", "+3" },
    { UFT_FMT_SCL,      0, 8, {'S','I','N','C','L','A','I','R'}, "SCL", "" },
    
    /* BBC/Acorn */
    { UFT_FMT_UEF,      0, 10, {'U','E','F',' ','F','i','l','e','!','\0'}, "UEF", "" },
    { UFT_FMT_ADF_ACORN,0, 4, {'H','u','g','o'}, "ADFS", "Hugo" },
    
    /* Japanese */
    { UFT_FMT_D88,      0, 1, {0x00}, "D88", "" },  /* Needs offset 0x1A check */
    { UFT_FMT_NFD,      0, 14, {'T','9','8','F','D','D','I','M','A','G','E','.','R','0'}, "NFD", "r0" },
    { UFT_FMT_NFD,      0, 14, {'T','9','8','F','D','D','I','M','A','G','E','.','R','1'}, "NFD", "r1" },
    { UFT_FMT_HDM,      0, 2, {0x00,0x00}, "HDM", "" },  /* Size check needed */
    { UFT_FMT_XDF,      0, 4, {'X','D','F','1'}, "XDF", "X68000" },
    
    /* MSX */
    { UFT_FMT_CAS,      0, 8, {0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74}, "CAS", "MSX" },
    
    /* End marker */
    { UFT_FMT_UNKNOWN, 0, 0, {0}, "", "" }
};

/* ============================================================================
 * Size-Based Detection
 * ============================================================================ */

typedef struct {
    uft_format_t format;
    uint64_t     size;
    const char  *name;
    const char  *version;
    int          confidence;
} size_entry_t;

static const size_entry_t SIZE_TABLE[] = {
    /* Amiga ADF */
    { UFT_FMT_ADF,       901120, "ADF", "DD 880K", 90 },
    { UFT_FMT_ADF,      1802240, "ADF", "HD 1760K", 90 },
    
    /* Commodore */
    { UFT_FMT_D64,       174848, "D64", "35 Track", 95 },
    { UFT_FMT_D64,       175531, "D64", "35+Errors", 95 },
    { UFT_FMT_D64,       196608, "D64", "40 Track", 90 },
    { UFT_FMT_D64,       197376, "D64", "40+Errors", 90 },
    { UFT_FMT_D71,       349696, "D71", "70 Track", 95 },
    { UFT_FMT_D71,       351062, "D71", "70+Errors", 95 },
    { UFT_FMT_D81,       819200, "D81", "80 Track", 95 },
    { UFT_FMT_D80,       533248, "D80", "8050", 90 },
    { UFT_FMT_D82,      1066496, "D82", "8250", 90 },
    { UFT_FMT_NIB,       286720, "NIB", "35 Track", 85 },
    { UFT_FMT_NIB,       573440, "NIB", "70 Half-Track", 85 },
    
    /* Apple II */
    { UFT_FMT_DO,        143360, "DO/DSK", "DOS 3.3", 70 },
    { UFT_FMT_PO,        143360, "PO", "ProDOS", 70 },
    { UFT_FMT_NIB_APPLE, 232960, "NIB", "Apple 35T", 80 },
    
    /* Atari 8-Bit */
    { UFT_FMT_ATR,        92176, "ATR", "SD 90K", 60 },
    { UFT_FMT_ATR,       133136, "ATR", "ED 130K", 60 },
    { UFT_FMT_ATR,       184336, "ATR", "DD 180K", 60 },
    { UFT_FMT_XFD,        92160, "XFD", "SD 90K", 70 },
    { UFT_FMT_XFD,       133120, "XFD", "ED 130K", 70 },
    { UFT_FMT_XFD,       184320, "XFD", "DD 180K", 70 },
    
    /* Atari ST */
    { UFT_FMT_ST,        368640, "ST", "SS 360K", 75 },
    { UFT_FMT_ST,        737280, "ST", "DS 720K", 75 },
    { UFT_FMT_ST,        819200, "ST", "DS 800K", 70 },
    
    /* PC */
    { UFT_FMT_IMG,       163840, "IMG", "160K SS/SD", 60 },
    { UFT_FMT_IMG,       184320, "IMG", "180K SS/SD", 60 },
    { UFT_FMT_IMG,       327680, "IMG", "320K DS/SD", 60 },
    { UFT_FMT_IMG,       368640, "IMG", "360K DS/DD", 70 },
    { UFT_FMT_IMG,       737280, "IMG", "720K 3.5\"", 70 },
    { UFT_FMT_IMG,      1228800, "IMG", "1.2M 5.25\"", 75 },
    { UFT_FMT_IMG,      1474560, "IMG", "1.44M 3.5\"", 80 },
    { UFT_FMT_IMG,      2949120, "IMG", "2.88M ED", 80 },
    
    /* Spectrum */
    { UFT_FMT_TRD,       655360, "TRD", "DS 640K", 85 },
    { UFT_FMT_TRD,       327680, "TRD", "SS 320K", 80 },
    { UFT_FMT_OPD,       184320, "OPD", "Opus 180K", 70 },
    
    /* BBC */
    { UFT_FMT_SSD,       102400, "SSD", "40T SS", 80 },
    { UFT_FMT_SSD,       204800, "SSD", "80T SS", 80 },
    { UFT_FMT_DSD,       204800, "DSD", "40T DS", 75 },
    { UFT_FMT_DSD,       409600, "DSD", "80T DS", 80 },
    
    /* TRS-80 */
    { UFT_FMT_JV1,        87040, "JV1", "SSSD 35T", 70 },
    { UFT_FMT_JV1,        89600, "JV1", "SSSD 35T/10s", 70 },
    
    /* SAM Coupé */
    { UFT_FMT_MGT_SAM,   819200, "MGT", "800K", 80 },
    
    /* Japanese */
    { UFT_FMT_D88,      1261568, "D88", "2HD 1.2M", 70 },
    { UFT_FMT_D88,       348160, "D88", "2DD 320K", 70 },
    
    /* End marker */
    { UFT_FMT_UNKNOWN, 0, "", "", 0 }
};

/* ============================================================================
 * Extension-Based Detection
 * ============================================================================ */

typedef struct {
    const char  *ext;
    uft_format_t format;
    const char  *name;
} ext_entry_t;

static const ext_entry_t EXT_TABLE[] = {
    /* Amiga */
    { "adf", UFT_FMT_ADF, "ADF" },
    { "adz", UFT_FMT_ADZ, "ADZ" },
    { "dms", UFT_FMT_DMS, "DMS" },
    { "ipf", UFT_FMT_IPF, "IPF" },
    
    /* Commodore */
    { "d64", UFT_FMT_D64, "D64" },
    { "d71", UFT_FMT_D71, "D71" },
    { "d81", UFT_FMT_D81, "D81" },
    { "d80", UFT_FMT_D80, "D80" },
    { "d82", UFT_FMT_D82, "D82" },
    { "g64", UFT_FMT_G64, "G64" },
    { "g71", UFT_FMT_G71, "G71" },
    { "nib", UFT_FMT_NIB, "NIB" },
    { "nbz", UFT_FMT_NBZ, "NBZ" },
    { "t64", UFT_FMT_T64, "T64" },
    { "tap", UFT_FMT_TAP_C64, "TAP" },
    
    /* Apple II */
    { "do", UFT_FMT_DO, "DO" },
    { "po", UFT_FMT_PO, "PO" },
    { "2mg", UFT_FMT_2MG, "2MG" },
    { "woz", UFT_FMT_WOZ, "WOZ" },
    { "a2r", UFT_FMT_A2R, "A2R" },
    { "dc", UFT_FMT_DC42, "DC42" },
    
    /* Atari */
    { "atr", UFT_FMT_ATR, "ATR" },
    { "atx", UFT_FMT_ATX, "ATX" },
    { "xfd", UFT_FMT_XFD, "XFD" },
    { "dcm", UFT_FMT_DCM, "DCM" },
    { "pro", UFT_FMT_PRO, "PRO" },
    { "st", UFT_FMT_ST, "ST" },
    { "msa", UFT_FMT_MSA, "MSA" },
    { "stx", UFT_FMT_STX, "STX" },
    { "dim", UFT_FMT_DIM, "DIM" },
    
    /* PC */
    { "img", UFT_FMT_IMG, "IMG" },
    { "ima", UFT_FMT_IMA, "IMA" },
    { "imd", UFT_FMT_IMD, "IMD" },
    { "td0", UFT_FMT_TD0, "TD0" },
    { "fdi", UFT_FMT_FDI, "FDI" },
    { "dsk", UFT_FMT_DSK_CPC, "DSK" },
    { "mfm", UFT_FMT_MFM, "MFM" },
    { "iso", UFT_FMT_ISO, "ISO" },
    { "vfd", UFT_FMT_VFD, "VFD" },
    
    /* Flux */
    { "scp", UFT_FMT_SCP, "SCP" },
    { "hfe", UFT_FMT_HFE, "HFE" },
    { "raw", UFT_FMT_KRYOFLUX, "KryoFlux" },
    { "dmk", UFT_FMT_DMK, "DMK" },
    { "flx", UFT_FMT_FLX, "FLX" },
    { "ct", UFT_FMT_CT, "CT" },
    
    /* Spectrum */
    { "trd", UFT_FMT_TRD, "TRD" },
    { "scl", UFT_FMT_SCL, "SCL" },
    { "fdd", UFT_FMT_FDD, "FDD" },
    { "opd", UFT_FMT_OPD, "OPD" },
    { "mgt", UFT_FMT_MGT, "MGT" },
    { "tzx", UFT_FMT_TZX, "TZX" },
    
    /* BBC */
    { "ssd", UFT_FMT_SSD, "SSD" },
    { "dsd", UFT_FMT_DSD, "DSD" },
    { "uef", UFT_FMT_UEF, "UEF" },
    { "adl", UFT_FMT_ADL, "ADL" },
    
    /* TRS-80 */
    { "jv1", UFT_FMT_JV1, "JV1" },
    { "jv3", UFT_FMT_JV3, "JV3" },
    
    /* Japanese */
    { "d88", UFT_FMT_D88, "D88" },
    { "88d", UFT_FMT_D88, "D88" },
    { "nfd", UFT_FMT_NFD, "NFD" },
    { "hdm", UFT_FMT_HDM, "HDM" },
    { "xdf", UFT_FMT_XDF, "XDF" },
    
    /* SAM */
    { "sad", UFT_FMT_SAD, "SAD" },
    { "sdf", UFT_FMT_SDF, "SDF" },
    
    /* MSX */
    { "cas", UFT_FMT_CAS, "CAS" },
    
    { NULL, UFT_FMT_UNKNOWN, "" }
};

/* ============================================================================
 * System Names
 * ============================================================================ */

static const char* format_system(uft_format_t fmt)
{
    switch (fmt) {
        case UFT_FMT_ADF:
        case UFT_FMT_ADZ:
        case UFT_FMT_DMS:
        case UFT_FMT_IPF:
        case UFT_FMT_ADF_EXT:
            return "Amiga";
            
        case UFT_FMT_D64:
        case UFT_FMT_D71:
        case UFT_FMT_D81:
        case UFT_FMT_D80:
        case UFT_FMT_D82:
        case UFT_FMT_G64:
        case UFT_FMT_G71:
        case UFT_FMT_NIB:
        case UFT_FMT_NBZ:
        case UFT_FMT_T64:
        case UFT_FMT_TAP_C64:
            return "Commodore 64/128";
            
        case UFT_FMT_DO:
        case UFT_FMT_PO:
        case UFT_FMT_NIB_APPLE:
        case UFT_FMT_2MG:
        case UFT_FMT_WOZ:
        case UFT_FMT_A2R:
        case UFT_FMT_DC42:
            return "Apple II";
            
        case UFT_FMT_ATR:
        case UFT_FMT_ATX:
        case UFT_FMT_XFD:
        case UFT_FMT_DCM:
        case UFT_FMT_PRO:
            return "Atari 8-Bit";
            
        case UFT_FMT_ST:
        case UFT_FMT_MSA:
        case UFT_FMT_STX:
        case UFT_FMT_DIM:
            return "Atari ST";
            
        case UFT_FMT_IMG:
        case UFT_FMT_IMA:
        case UFT_FMT_IMD:
        case UFT_FMT_TD0:
        case UFT_FMT_FDI:
        case UFT_FMT_MFM:
        case UFT_FMT_ISO:
            return "IBM PC";
            
        case UFT_FMT_DSK_CPC:
        case UFT_FMT_EDSK:
            return "Amstrad CPC";
            
        case UFT_FMT_SCP:
        case UFT_FMT_HFE:
        case UFT_FMT_HFE_V3:
        case UFT_FMT_KRYOFLUX:
        case UFT_FMT_DMK:
        case UFT_FMT_FLX:
            return "Flux (Universal)";
            
        case UFT_FMT_TRD:
        case UFT_FMT_SCL:
        case UFT_FMT_FDD:
        case UFT_FMT_OPD:
        case UFT_FMT_TAP:
        case UFT_FMT_TZX:
            return "ZX Spectrum";
            
        case UFT_FMT_SSD:
        case UFT_FMT_DSD:
        case UFT_FMT_ADF_ACORN:
        case UFT_FMT_ADL:
        case UFT_FMT_UEF:
            return "BBC Micro/Acorn";
            
        case UFT_FMT_JV1:
        case UFT_FMT_JV3:
        case UFT_FMT_DMK_TRS:
            return "TRS-80";
            
        case UFT_FMT_D88:
        case UFT_FMT_NFD:
        case UFT_FMT_FDI_PC98:
        case UFT_FMT_HDM:
            return "NEC PC-98";
            
        case UFT_FMT_XDF:
        case UFT_FMT_DIM_X68:
            return "Sharp X68000";
            
        case UFT_FMT_MGT:
        case UFT_FMT_SAD:
        case UFT_FMT_SDF:
        case UFT_FMT_MGT_SAM:
            return "SAM Coupé";
            
        case UFT_FMT_DSK_MSX:
        case UFT_FMT_CAS:
            return "MSX";
            
        default:
            return "Unknown";
    }
}

/* ============================================================================
 * Detection Functions
 * ============================================================================ */

/**
 * @brief Get file extension (lowercase)
 */
static void get_extension(const char *path, char *ext, size_t ext_size)
{
    ext[0] = '\0';
    if (!path) return;
    
    const char *dot = strrchr(path, '.');
    if (!dot || !dot[1]) return;
    
    size_t len = strlen(dot + 1);
    if (len >= ext_size) len = ext_size - 1;
    
    for (size_t i = 0; i < len; i++) {
        ext[i] = tolower((unsigned char)dot[1 + i]);
    }
    ext[len] = '\0';
}

/**
 * @brief Read file header
 */
static int read_header(const char *path, uint8_t *buf, size_t size)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    
    size_t read = fread(buf, 1, size, fp);
    fclose(fp);
    
    return (read == size) ? 0 : -1;
}

/**
 * @brief Get file size
 */
static uint64_t get_file_size(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(fp);
    fclose(fp);
    
    return (size > 0) ? (uint64_t)size : 0;
}

/**
 * @brief Check magic bytes
 */
static int check_magic(const uint8_t *header, uft_format_info_t *info)
{
    for (int i = 0; MAGIC_TABLE[i].format != UFT_FMT_UNKNOWN; i++) {
        const magic_entry_t *m = &MAGIC_TABLE[i];
        
        if (memcmp(header + m->offset, m->magic, m->length) == 0) {
            info->format = m->format;
            strncpy(info->name, m->name, sizeof(info->name) - 1);
            strncpy(info->version, m->version, sizeof(info->version) - 1);
            return 100;  /* Magic match = high confidence */
        }
    }
    
    return 0;
}

/**
 * @brief Check file size
 */
static int check_size(uint64_t size, uft_format_info_t *info)
{
    for (int i = 0; SIZE_TABLE[i].format != UFT_FMT_UNKNOWN; i++) {
        const size_entry_t *s = &SIZE_TABLE[i];
        
        if (size == s->size) {
            if (info->format == UFT_FMT_UNKNOWN) {
                info->format = s->format;
                strncpy(info->name, s->name, sizeof(info->name) - 1);
            }
            strncpy(info->version, s->version, sizeof(info->version) - 1);
            return s->confidence;
        }
    }
    
    return 0;
}

/**
 * @brief Check extension
 */
static int check_extension(const char *ext, uft_format_info_t *info)
{
    for (int i = 0; EXT_TABLE[i].ext != NULL; i++) {
        if (strcasecmp(ext, EXT_TABLE[i].ext) == 0) {
            if (info->format == UFT_FMT_UNKNOWN) {
                info->format = EXT_TABLE[i].format;
                strncpy(info->name, EXT_TABLE[i].name, sizeof(info->name) - 1);
            }
            return 50;  /* Extension match = medium confidence */
        }
    }
    
    return 0;
}

/**
 * @brief Validate specific format structures
 */
static int validate_structure(const char *path, const uint8_t *header,
                              uint64_t size, uft_format_info_t *info)
{
    int bonus = 0;
    
    switch (info->format) {
        case UFT_FMT_D64:
            /* Check for valid BAM at track 18 */
            if (size >= 0x16500) {
                FILE *fp = fopen(path, "rb");
                if (fp) {
                    uint8_t bam[256];
                    if (fseek(fp, 0x16500, SEEK_SET) != 0) { /* seek error */ }
                    if (fread(bam, 1, 256, fp) != 256) { /* I/O error */ }
                    fclose(fp);
                    
                    /* Check directory pointer */
                    if (bam[0] == 18 && bam[1] == 1) {
                        bonus = 20;
                        info->tracks = 35;
                        info->sectors = 683;
                        info->sector_size = 256;
                    }
                }
            }
            break;
            
        case UFT_FMT_ADF:
            /* Check for valid bootblock */
            if (header[0] == 'D' && header[1] == 'O' && header[2] == 'S') {
                bonus = 20;
                snprintf(info->version, sizeof(info->version), "OFS");
            } else if (memcmp(header, "DOS\0", 4) == 0 ||
                       memcmp(header, "DOS\1", 4) == 0) {
                bonus = 20;
                snprintf(info->version, sizeof(info->version), 
                         header[3] ? "FFS" : "OFS");
            }
            info->tracks = 80;
            info->sides = 2;
            info->sectors = 11;
            info->sector_size = 512;
            break;
            
        case UFT_FMT_SCP:
            /* Validate SCP header */
            if (header[0] == 'S' && header[1] == 'C' && header[2] == 'P') {
                info->tracks = header[6];
                info->sides = (header[4] & 0x01) ? 2 : 1;
                snprintf(info->version, sizeof(info->version), 
                         "v%d.%d", header[5] >> 4, header[5] & 0x0F);
                bonus = 15;
            }
            break;
            
        case UFT_FMT_HFE:
        case UFT_FMT_HFE_V3:
            /* Validate HFE header */
            if (memcmp(header, "HXCPICFE", 8) == 0 ||
                memcmp(header, "HXCHFEV3", 8) == 0) {
                info->tracks = header[9];
                info->sides = header[10];
                bonus = 15;
            }
            break;
            
        case UFT_FMT_WOZ:
            /* Check WOZ version */
            if (header[0] == 'W' && header[1] == 'O' && header[2] == 'Z') {
                snprintf(info->version, sizeof(info->version), 
                         "%c.0", header[3]);
                bonus = 10;
            }
            break;
            
        case UFT_FMT_D88:
            /* Check D88 header */
            {
                uint32_t d88_size = header[0x1C] | (header[0x1D] << 8) |
                                    (header[0x1E] << 16) | (header[0x1F] << 24);
                if (d88_size > 0 && d88_size <= size) {
                    info->write_protected = header[0x1A] != 0;
                    uint8_t media = header[0x1B];
                    
                    switch (media) {
                        case 0x00: snprintf(info->version, sizeof(info->version), "2D"); break;
                        case 0x10: snprintf(info->version, sizeof(info->version), "2DD"); break;
                        case 0x20: snprintf(info->version, sizeof(info->version), "2HD"); break;
                        default:   snprintf(info->version, sizeof(info->version), "Unknown"); break;
                    }
                    bonus = 25;
                }
            }
            break;
            
        default:
            break;
    }
    
    return bonus;
}

/* ============================================================================
 * Main Detection Function
 * ============================================================================ */

/**
 * @brief Detect disk image format
 * 
 * @param path      Path to disk image file
 * @param info      Output: detected format information
 * @return          0 on success, -1 on error
 */
int uft_detect_format(const char *path, uft_format_info_t *info)
{
    if (!path || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    info->format = UFT_FMT_UNKNOWN;
    strncpy(info->name, "Unknown", sizeof(info->name) - 1);
    
    /* Get file info */
    info->file_size = get_file_size(path);
    if (info->file_size == 0) return -1;
    
    /* Read header */
    uint8_t header[256];
    if (read_header(path, header, sizeof(header)) != 0) {
        memset(header, 0, sizeof(header));
    }
    
    /* Get extension */
    char ext[16];
    get_extension(path, ext, sizeof(ext));
    
    /* Check gzip compression */
    if (header[0] == 0x1F && header[1] == 0x8B) {
        info->compressed = true;
        if (strcasecmp(ext, "adz") == 0) {
            info->format = UFT_FMT_ADZ;
            strncpy(info->name, "ADZ", sizeof(info->name) - 1);
            strncpy(info->version, "gzip", sizeof(info->version) - 1);
            strncpy(info->system, "Amiga", sizeof(info->system) - 1);
            info->confidence = 95;
            return 0;
        }
    }
    
    /* Detection stages */
    int magic_conf = check_magic(header, info);
    int size_conf = check_size(info->file_size, info);
    int ext_conf = check_extension(ext, info);
    int struct_bonus = validate_structure(path, header, info->file_size, info);
    
    /* Calculate confidence */
    if (magic_conf > 0) {
        info->confidence = magic_conf;
    } else if (size_conf > 0 && ext_conf > 0) {
        info->confidence = (size_conf + ext_conf) / 2 + 10;
    } else if (size_conf > 0) {
        info->confidence = size_conf;
    } else if (ext_conf > 0) {
        info->confidence = ext_conf;
    }
    
    info->confidence += struct_bonus;
    if (info->confidence > 100) info->confidence = 100;
    
    /* Set system name */
    strncpy(info->system, format_system(info->format), sizeof(info->system) - 1);
    
    /* Generate description */
    snprintf(info->description, sizeof(info->description),
             "%s %s%s%s (%s)",
             info->name,
             info->version[0] ? info->version : "",
             info->version[0] ? " " : "",
             info->compressed ? "[compressed]" : "",
             info->system);
    
    return 0;
}

/**
 * @brief Detect format and print results
 */
void uft_detect_format_print(const char *path)
{
    uft_format_info_t info;
    
    if (uft_detect_format(path, &info) != 0) {
        printf("Error: Cannot open file\n");
        return;
    }
    
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ FORMAT DETECTION RESULT                                        ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ File:       %-50s ║\n", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    printf("║ Size:       %-50llu ║\n", (unsigned long long)info.file_size);
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Format:     %-50s ║\n", info.name);
    printf("║ Version:    %-50s ║\n", info.version[0] ? info.version : "N/A");
    printf("║ System:     %-50s ║\n", info.system);
    printf("║ Confidence: %d%%%-47s ║\n", info.confidence, "");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    
    if (info.tracks > 0) {
        printf("║ Tracks:     %-50d ║\n", info.tracks);
    }
    if (info.sides > 0) {
        printf("║ Sides:      %-50d ║\n", info.sides);
    }
    if (info.sectors > 0) {
        printf("║ Sectors:    %-50d ║\n", info.sectors);
    }
    if (info.sector_size > 0) {
        printf("║ Sect.Size:  %-50d ║\n", info.sector_size);
    }
    
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Compressed: %-50s ║\n", info.compressed ? "Yes" : "No");
    printf("║ Protected:  %-50s ║\n", info.write_protected ? "Yes" : "No");
    printf("║ Copy Prot:  %-50s ║\n", info.copy_protected ? "Yes" : "No");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Get format name string
 */
const char* uft_format_name(uft_format_t format)
{
    static const char* names[] = {
        [UFT_FMT_UNKNOWN] = "Unknown",
        [UFT_FMT_ADF] = "ADF",
        [UFT_FMT_ADZ] = "ADZ",
        [UFT_FMT_DMS] = "DMS",
        [UFT_FMT_IPF] = "IPF",
        [UFT_FMT_D64] = "D64",
        [UFT_FMT_D71] = "D71",
        [UFT_FMT_D81] = "D81",
        [UFT_FMT_G64] = "G64",
        [UFT_FMT_NIB] = "NIB",
        [UFT_FMT_DO] = "DO",
        [UFT_FMT_PO] = "PO",
        [UFT_FMT_2MG] = "2MG",
        [UFT_FMT_WOZ] = "WOZ",
        [UFT_FMT_ATR] = "ATR",
        [UFT_FMT_ATX] = "ATX",
        [UFT_FMT_XFD] = "XFD",
        [UFT_FMT_ST] = "ST",
        [UFT_FMT_MSA] = "MSA",
        [UFT_FMT_STX] = "STX",
        [UFT_FMT_IMG] = "IMG",
        [UFT_FMT_IMD] = "IMD",
        [UFT_FMT_TD0] = "TD0",
        [UFT_FMT_FDI] = "FDI",
        [UFT_FMT_DSK_CPC] = "DSK",
        [UFT_FMT_EDSK] = "EDSK",
        [UFT_FMT_SCP] = "SCP",
        [UFT_FMT_HFE] = "HFE",
        [UFT_FMT_KRYOFLUX] = "KryoFlux",
        [UFT_FMT_DMK] = "DMK",
        [UFT_FMT_TRD] = "TRD",
        [UFT_FMT_SSD] = "SSD",
        [UFT_FMT_DSD] = "DSD",
        [UFT_FMT_JV3] = "JV3",
        [UFT_FMT_D88] = "D88",
        [UFT_FMT_NFD] = "NFD",
        [UFT_FMT_MGT] = "MGT",
    };
    
    if (format < UFT_FMT_COUNT && names[format]) {
        return names[format];
    }
    return "Unknown";
}
