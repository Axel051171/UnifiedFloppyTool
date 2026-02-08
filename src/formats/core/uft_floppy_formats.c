/**
 * @file uft_floppy_formats.c
 * @brief Floppy Disk Format Database Implementation
 */

#include "uft_floppy_formats.h"
#include "uft/core/uft_error_compat.h"
#include <string.h>

/*===========================================================================
 * Apple 6-and-2 GCR Encoding Tables
 *===========================================================================*/

/**
 * Apple 6-and-2 encoding: 6 data bits -> 8 GCR bits
 * Valid bytes have MSB=1 and at most 2 consecutive 0s
 */
const uint8_t UFT_GCR_APPLE_6AND2_ENC[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/**
 * Apple 6-and-2 decoding: 8 GCR bits -> 6 data bits
 * 0xFF indicates invalid GCR byte
 */
const uint8_t UFT_GCR_APPLE_6AND2_DEC[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 08-0F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 10-17 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 18-1F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 20-27 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 28-2F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 30-37 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 38-3F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 40-47 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 48-4F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 50-57 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 58-5F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 60-67 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 68-6F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 70-77 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 78-7F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 80-87 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 88-8F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,  /* 90-97 */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,  /* 98-9F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,  /* A0-A7 */
    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,  /* A8-AF */
    0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,  /* B0-B7 */
    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,  /* B8-BF */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* C0-C7 */
    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,  /* C8-CF */
    0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,  /* D0-D7 */
    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,  /* D8-DF */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,  /* E0-E7 */
    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,  /* E8-EF */
    0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,  /* F0-F7 */
    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F   /* F8-FF */
};

/*===========================================================================
 * Commodore GCR Encoding Tables
 *===========================================================================*/

/**
 * Commodore GCR encoding: 4 data bits -> 5 GCR bits
 */
const uint8_t UFT_GCR_C64_ENC[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/**
 * Commodore GCR decoding: 5 GCR bits -> 4 data bits
 * 0xFF indicates invalid GCR value
 */
const uint8_t UFT_GCR_C64_DEC[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

/*===========================================================================
 * Format Database
 *===========================================================================*/

static const uft_format_desc_t format_database[] = {
    /* IBM PC Formats */
    {
        .id = UFT_FMT_PC_160K,
        .geometry = {
            .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 1, .tracks = 40, .sectors = 8, .sector_size = 512,
            .rpm = 300, .data_rate = 250, .tpi = 48,
            .formatted_capacity = 163840, .name = "PC 160K", .platform = "IBM PC"
        },
        .filesystem = "FAT12", .dir_entries = 64, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 50, .gap2 = 22, .gap3 = 80
    },
    {
        .id = UFT_FMT_PC_360K,
        .geometry = {
            .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 40, .sectors = 9, .sector_size = 512,
            .rpm = 300, .data_rate = 250, .tpi = 48,
            .formatted_capacity = 368640, .name = "PC 360K", .platform = "IBM PC"
        },
        .filesystem = "FAT12", .dir_entries = 112, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 50, .gap2 = 22, .gap3 = 54
    },
    {
        .id = UFT_FMT_PC_720K,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 9, .sector_size = 512,
            .rpm = 300, .data_rate = 250, .tpi = 135,
            .formatted_capacity = 737280, .name = "PC 720K", .platform = "IBM PC"
        },
        .filesystem = "FAT12", .dir_entries = 112, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 50, .gap2 = 22, .gap3 = 54
    },
    {
        .id = UFT_FMT_PC_1200K,
        .geometry = {
            .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_HIGH,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 15, .sector_size = 512,
            .rpm = 360, .data_rate = 500, .tpi = 96,
            .formatted_capacity = 1228800, .name = "PC 1.2M", .platform = "IBM PC AT"
        },
        .filesystem = "FAT12", .dir_entries = 224, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 50, .gap2 = 22, .gap3 = 54
    },
    {
        .id = UFT_FMT_PC_1440K,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_HIGH,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 18, .sector_size = 512,
            .rpm = 300, .data_rate = 500, .tpi = 135,
            .formatted_capacity = 1474560, .name = "PC 1.44M", .platform = "IBM PC"
        },
        .filesystem = "FAT12", .dir_entries = 224, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 80, .gap2 = 22, .gap3 = 84
    },
    {
        .id = UFT_FMT_PC_2880K,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_EXTENDED,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 36, .sector_size = 512,
            .rpm = 300, .data_rate = 1000, .tpi = 135,
            .formatted_capacity = 2949120, .name = "PC 2.88M", .platform = "IBM PC"
        },
        .filesystem = "FAT12", .dir_entries = 240, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 80, .gap2 = 22, .gap3 = 41
    },
    
    /* Amiga Formats */
    {
        .id = UFT_FMT_AMIGA_DD,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 11, .sector_size = 512,
            .rpm = 300, .data_rate = 250, .tpi = 135,
            .formatted_capacity = 901120, .name = "Amiga DD", .platform = "Amiga"
        },
        .filesystem = "OFS/FFS", .dir_entries = 0, .reserved_tracks = 2,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 0,
        .gap1 = 0, .gap2 = 0, .gap3 = 0  /* Amiga has no inter-sector gaps */
    },
    {
        .id = UFT_FMT_AMIGA_HD,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_HIGH,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 22, .sector_size = 512,
            .rpm = 300, .data_rate = 500, .tpi = 135,
            .formatted_capacity = 1802240, .name = "Amiga HD", .platform = "Amiga"
        },
        .filesystem = "OFS/FFS", .dir_entries = 0, .reserved_tracks = 2,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 0,
        .gap1 = 0, .gap2 = 0, .gap3 = 0
    },
    
    /* Atari ST Formats */
    {
        .id = UFT_FMT_ATARI_ST_DS,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 9, .sector_size = 512,
            .rpm = 300, .data_rate = 250, .tpi = 135,
            .formatted_capacity = 737280, .name = "Atari ST DS", .platform = "Atari ST"
        },
        .filesystem = "FAT12", .dir_entries = 112, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 60, .gap2 = 22, .gap3 = 40
    },
    
    /* Commodore Formats */
    {
        .id = UFT_FMT_C64_1541,
        .geometry = {
            .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE,
            .encoding = UFT_ENCODING_GCR_C64, .sectoring = UFT_SECTOR_SOFT,
            .sides = 1, .tracks = 35, .sectors = 0, .sector_size = 256,
            .rpm = 300, .data_rate = 250, .tpi = 48,
            .formatted_capacity = 174848, .name = "C64 1541", .platform = "Commodore 64"
        },
        .filesystem = "CBM DOS", .dir_entries = 144, .reserved_tracks = 1,
        .interleave = 10, .head_skew = 0, .track_skew = 0, .first_sector = 0,
        .gap1 = 0, .gap2 = 0, .gap3 = 0
    },
    {
        .id = UFT_FMT_C128_1581,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 10, .sector_size = 512,
            .rpm = 300, .data_rate = 250, .tpi = 135,
            .formatted_capacity = 819200, .name = "C128 1581", .platform = "Commodore 128"
        },
        .filesystem = "CBM DOS", .dir_entries = 296, .reserved_tracks = 2,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 0,
        .gap1 = 0, .gap2 = 0, .gap3 = 0
    },
    
    /* Atari 8-bit Formats */
    {
        .id = UFT_FMT_ATARI_810,
        .geometry = {
            .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE,
            .encoding = UFT_ENCODING_FM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 1, .tracks = 40, .sectors = 18, .sector_size = 128,
            .rpm = 288, .data_rate = 125, .tpi = 48,
            .formatted_capacity = 92160, .name = "Atari 810", .platform = "Atari 8-bit"
        },
        .filesystem = "Atari DOS", .dir_entries = 64, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 0, .gap2 = 0, .gap3 = 0
    },
    {
        .id = UFT_FMT_ATARI_1050,
        .geometry = {
            .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE,
            .encoding = UFT_ENCODING_FM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 1, .tracks = 40, .sectors = 26, .sector_size = 128,
            .rpm = 288, .data_rate = 125, .tpi = 48,
            .formatted_capacity = 133120, .name = "Atari 1050", .platform = "Atari 8-bit"
        },
        .filesystem = "Atari DOS 2.5", .dir_entries = 64, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 1,
        .gap1 = 0, .gap2 = 0, .gap3 = 0
    },
    
    /* Macintosh Formats */
    {
        .id = UFT_FMT_MAC_800K,
        .geometry = {
            .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_GCR, .sectoring = UFT_SECTOR_SOFT,
            .sides = 2, .tracks = 80, .sectors = 0, .sector_size = 512,
            .rpm = 394, .data_rate = 250, .tpi = 135,
            .formatted_capacity = 819200, .name = "Mac 800K", .platform = "Macintosh"
        },
        .filesystem = "HFS", .dir_entries = 0, .reserved_tracks = 0,
        .interleave = 2, .head_skew = 0, .track_skew = 0, .first_sector = 0,
        .gap1 = 0, .gap2 = 0, .gap3 = 0
    },
    
    /* BBC Micro Formats */
    {
        .id = UFT_FMT_BBC_DFS_SS80,
        .geometry = {
            .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE,
            .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT,
            .sides = 1, .tracks = 80, .sectors = 10, .sector_size = 256,
            .rpm = 300, .data_rate = 250, .tpi = 96,
            .formatted_capacity = 204800, .name = "BBC DFS SS/80", .platform = "BBC Micro"
        },
        .filesystem = "DFS", .dir_entries = 31, .reserved_tracks = 0,
        .interleave = 1, .head_skew = 0, .track_skew = 0, .first_sector = 0,
        .gap1 = 0, .gap2 = 0, .gap3 = 0
    },
    
    /* Terminator */
    { .id = UFT_FMT_UNKNOWN }
};

#define FORMAT_DB_SIZE (sizeof(format_database) / sizeof(format_database[0]) - 1)

/*===========================================================================
 * Format Database Functions
 *===========================================================================*/

const uft_format_desc_t *uft_format_get(uft_format_id_t id)
{
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        if (format_database[i].id == id) {
            return &format_database[i];
        }
    }
    return NULL;
}

const uft_format_desc_t *uft_format_find_by_name(const char *name)
{
    if (!name) return NULL;
    
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        if (format_database[i].geometry.name &&
            strcmp(format_database[i].geometry.name, name) == 0) {
            return &format_database[i];
        }
    }
    return NULL;
}

int uft_format_count(void)
{
    return (int)FORMAT_DB_SIZE;
}

void uft_format_foreach(uft_format_callback_t callback, void *ctx)
{
    if (!callback) return;
    
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        callback(&format_database[i], ctx);
    }
}

uft_format_id_t uft_format_detect(uint32_t size, uint8_t sides,
                                   uint8_t tracks, uint8_t sectors,
                                   uint16_t sector_size)
{
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        const uft_floppy_geometry_t *g = &format_database[i].geometry;
        
        if (g->sides == sides && g->tracks == tracks &&
            g->sectors == sectors && g->sector_size == sector_size) {
            return format_database[i].id;
        }
    }
    
    /* Try by size alone */
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        if (format_database[i].geometry.formatted_capacity == size) {
            return format_database[i].id;
        }
    }
    
    return UFT_FMT_UNKNOWN;
}

int uft_format_candidates(size_t size, uft_format_id_t *formats, int max_formats)
{
    int count = 0;
    
    for (size_t i = 0; i < FORMAT_DB_SIZE && count < max_formats; i++) {
        if (format_database[i].geometry.formatted_capacity == size) {
            formats[count++] = format_database[i].id;
        }
    }
    
    return count;
}

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static const char *uft_floppy_encoding_name(uft_encoding_t encoding)
{
    switch (encoding) {
        case UFT_ENCODING_FM:        return "FM";
        case UFT_ENCODING_MFM:       return "MFM";
        case UFT_ENCODING_M2FM:      return "M2FM";
        case UFT_ENCODING_GCR:       return "GCR";
        case UFT_ENCODING_GCR_APPLE: return "Apple GCR";
        case UFT_ENCODING_GCR_C64:   return "C64 GCR";
        default:                     return "Unknown";
    }
}

const char *uft_density_name(uft_density_t density)
{
    switch (density) {
        case UFT_DENSITY_SINGLE:   return "Single Density";
        case UFT_DENSITY_DOUBLE:   return "Double Density";
        case UFT_DENSITY_QUAD:     return "Quad Density";
        case UFT_DENSITY_HIGH:     return "High Density";
        case UFT_DENSITY_EXTENDED: return "Extended Density";
        default:                   return "Unknown";
    }
}
