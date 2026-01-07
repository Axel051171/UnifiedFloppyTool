/**
 * @file uft_format_detect.c
 * @brief Multi-stage format detection implementation
 */

#include "uft_format_detect.h"
#include <string.h>
#include <strings.h>  /* For strcasecmp on POSIX */
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Built-in Format Probes
 * ============================================================================ */

/* D64 - Commodore 64 */
static int probe_d64(const uint8_t *h, size_t len, size_t file_size) {
    (void)h; (void)len;
    /* D64 has no magic, but fixed sizes */
    if (file_size == 174848) return 80;  /* 35 tracks */
    if (file_size == 175531) return 80;  /* 35 tracks + errors */
    if (file_size == 196608) return 70;  /* 40 tracks */
    if (file_size == 197376) return 70;  /* 40 tracks + errors */
    return 0;
}

/* G64 - Commodore GCR */
static const uint8_t g64_magic[] = "GCR-1541";
static int probe_g64(const uint8_t *h, size_t len, size_t file_size) {
    (void)file_size;
    if (len < 8) return 0;
    if (memcmp(h, g64_magic, 8) == 0) return 100;
    return 0;
}

/* ADF - Amiga */
static int probe_adf(const uint8_t *h, size_t len, size_t file_size) {
    if (len < 4) return 0;
    int score = 0;
    
    /* Check file size */
    if (file_size == 901120) score += 40;   /* DD */
    if (file_size == 1802240) score += 40;  /* HD */
    
    /* Check for DOS magic in bootblock */
    if (h[0] == 'D' && h[1] == 'O' && h[2] == 'S') score += 40;
    if (memcmp(h, "KICK", 4) == 0) score += 30;
    
    return score;
}

/* DMK - TRS-80 */
static int probe_dmk(const uint8_t *h, size_t len, size_t file_size) {
    (void)file_size;
    if (len < 16) return 0;
    
    /* DMK header structure */
    uint8_t wp = h[0];           /* Write protect: 0x00 or 0xFF */
    uint8_t tracks = h[1];       /* Track count */
    uint16_t track_len = h[2] | (h[3] << 8);  /* Track length LE */
    
    /* Validate header */
    if (wp != 0x00 && wp != 0xFF) return 0;
    if (tracks == 0 || tracks > 86) return 0;
    if (track_len < 0x1900 || track_len > 0x3400) return 0;
    
    return 90;
}

/* HFE - UFT HFE Format */
static const uint8_t hfe_magic[] = "HXCPICFE";
static int probe_hfe(const uint8_t *h, size_t len, size_t file_size) {
    (void)file_size;
    if (len < 8) return 0;
    if (memcmp(h, hfe_magic, 8) == 0) return 100;
    return 0;
}

/* IPF - SPS Interchangeable Preservation Format */
static const uint8_t ipf_magic[] = "CAPS";
static int probe_ipf(const uint8_t *h, size_t len, size_t file_size) {
    (void)file_size;
    if (len < 4) return 0;
    if (memcmp(h, ipf_magic, 4) == 0) return 100;
    return 0;
}

/* SCP - SuperCard Pro */
static const uint8_t scp_magic[] = "SCP";
static int probe_scp(const uint8_t *h, size_t len, size_t file_size) {
    (void)file_size;
    if (len < 3) return 0;
    if (memcmp(h, scp_magic, 3) == 0) return 100;
    return 0;
}

/* IMG/IMA - Raw sector image */
static int probe_img(const uint8_t *h, size_t len, size_t file_size) {
    (void)h; (void)len;
    
    /* Common floppy sizes */
    static const size_t valid_sizes[] = {
        163840,   /* 160K SS */
        184320,   /* 180K SS */
        327680,   /* 320K DS */
        368640,   /* 360K DS */
        737280,   /* 720K */
        1228800,  /* 1.2M */
        1474560,  /* 1.44M */
        2949120,  /* 2.88M */
    };
    
    for (size_t i = 0; i < sizeof(valid_sizes)/sizeof(valid_sizes[0]); i++) {
        if (file_size == valid_sizes[i]) return 60;
    }
    
    /* Check if divisible by 512 (sector size) */
    if (file_size > 0 && file_size % 512 == 0) return 30;
    
    return 0;
}

/* MFM - fdc_bitstream native format */
static const uint8_t mfm_magic[] = "MFM_IMG ";
static int probe_mfm(const uint8_t *h, size_t len, size_t file_size) {
    (void)file_size;
    if (len < 8) return 0;
    if (memcmp(h, mfm_magic, 8) == 0) return 100;
    return 0;
}

/* FDI - Floppy Disk Image (Amiga) */
static const uint8_t fdi_magic[] = "Formatted Disk Image";
static int probe_fdi(const uint8_t *h, size_t len, size_t file_size) {
    (void)file_size;
    if (len < 20) return 0;
    if (memcmp(h, fdi_magic, 20) == 0) return 100;
    return 0;
}

/* ============================================================================
 * Built-in Formats Table
 * ============================================================================ */

static const uft_format_descriptor_t builtin_formats[] = {
    {
        .name = "D64",
        .extension = "d64",
        .description = "Commodore 64 disk image",
        .category = UFT_FORMAT_CAT_STRUCTURED,
        .platform = UFT_PLATFORM_C64,
        .probe = probe_d64,
        .min_size = 174848,
        .max_size = 197376,
    },
    {
        .name = "G64",
        .extension = "g64",
        .description = "Commodore GCR image",
        .category = UFT_FORMAT_CAT_BITSTREAM,
        .platform = UFT_PLATFORM_C64,
        .magic = g64_magic,
        .magic_len = 8,
        .probe = probe_g64,
    },
    {
        .name = "ADF",
        .extension = "adf",
        .description = "Amiga Disk File",
        .category = UFT_FORMAT_CAT_RAW_SECTOR,
        .platform = UFT_PLATFORM_AMIGA,
        .probe = probe_adf,
        .min_size = 901120,
        .max_size = 1802240,
    },
    {
        .name = "DMK",
        .extension = "dmk",
        .description = "TRS-80 disk image",
        .category = UFT_FORMAT_CAT_STRUCTURED,
        .platform = UFT_PLATFORM_TRS80,
        .probe = probe_dmk,
    },
    {
        .name = "HFE",
        .extension = "hfe",
        .description = "UFT HFE Format",
        .category = UFT_FORMAT_CAT_EMULATOR,
        .platform = UFT_PLATFORM_MULTI,
        .magic = hfe_magic,
        .magic_len = 8,
        .probe = probe_hfe,
    },
    {
        .name = "IPF",
        .extension = "ipf",
        .description = "SPS Preservation Format",
        .category = UFT_FORMAT_CAT_EMULATOR,
        .platform = UFT_PLATFORM_MULTI,
        .magic = ipf_magic,
        .magic_len = 4,
        .probe = probe_ipf,
    },
    {
        .name = "SCP",
        .extension = "scp",
        .description = "SuperCard Pro flux",
        .category = UFT_FORMAT_CAT_BITSTREAM,
        .platform = UFT_PLATFORM_MULTI,
        .magic = scp_magic,
        .magic_len = 3,
        .probe = probe_scp,
    },
    {
        .name = "IMG",
        .extension = "img",
        .description = "Raw sector image",
        .category = UFT_FORMAT_CAT_RAW_SECTOR,
        .platform = UFT_PLATFORM_IBM_PC,
        .probe = probe_img,
    },
    {
        .name = "IMA",
        .extension = "ima",
        .description = "Raw sector image",
        .category = UFT_FORMAT_CAT_RAW_SECTOR,
        .platform = UFT_PLATFORM_IBM_PC,
        .probe = probe_img,
    },
    {
        .name = "MFM",
        .extension = "mfm",
        .description = "Native MFM bitstream",
        .category = UFT_FORMAT_CAT_BITSTREAM,
        .platform = UFT_PLATFORM_MULTI,
        .magic = mfm_magic,
        .magic_len = 8,
        .probe = probe_mfm,
    },
    {
        .name = "FDI",
        .extension = "fdi",
        .description = "Formatted Disk Image",
        .category = UFT_FORMAT_CAT_STRUCTURED,
        .platform = UFT_PLATFORM_AMIGA,
        .magic = fdi_magic,
        .magic_len = 20,
        .probe = probe_fdi,
    },
};

#define BUILTIN_FORMAT_COUNT (sizeof(builtin_formats) / sizeof(builtin_formats[0]))

/* ============================================================================
 * Registry Management
 * ============================================================================ */

void uft_format_registry_init(uft_format_registry_t *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
    
    /* Add built-in formats */
    for (size_t i = 0; i < BUILTIN_FORMAT_COUNT; i++) {
        if (reg->count < UFT_FORMAT_MAX) {
            reg->formats[reg->count++] = builtin_formats[i];
        }
    }
}

int uft_format_registry_add(uft_format_registry_t *reg,
                            const uft_format_descriptor_t *format) {
    if (!reg || !format) return -1;
    if (reg->count >= UFT_FORMAT_MAX) return -2;
    
    reg->formats[reg->count++] = *format;
    return 0;
}

const uft_format_descriptor_t* uft_format_registry_find(
    const uft_format_registry_t *reg,
    const char *name) {
    if (!reg || !name) return NULL;
    
    for (size_t i = 0; i < reg->count; i++) {
        if (strcasecmp(reg->formats[i].name, name) == 0) {
            return &reg->formats[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Format Detection
 * ============================================================================ */

const char* uft_format_get_extension(const char *filename) {
    if (!filename) return NULL;
    const char *dot = strrchr(filename, '.');
    return dot ? dot + 1 : NULL;
}

bool uft_format_extension_match(const char *filename, const char *ext) {
    const char *file_ext = uft_format_get_extension(filename);
    if (!file_ext || !ext) return false;
    return strcasecmp(file_ext, ext) == 0;
}

static int score_format(const uft_format_descriptor_t *fmt,
                        const uint8_t *data,
                        size_t len,
                        size_t file_size,
                        const char *filename,
                        uft_format_info_t *info) {
    int score = 0;
    
    info->name = fmt->name;
    info->extension = fmt->extension;
    info->description = fmt->description;
    info->category = fmt->category;
    info->platform = fmt->platform;
    
    /* Magic byte matching */
    if (fmt->magic && fmt->magic_len > 0 && len >= fmt->magic_len + fmt->magic_offset) {
        if (memcmp(data + fmt->magic_offset, fmt->magic, fmt->magic_len) == 0) {
            info->magic_score = 50;
            score += 50;
        }
    }
    
    /* Extension matching */
    if (filename && fmt->extension) {
        if (uft_format_extension_match(filename, fmt->extension)) {
            info->extension_score = 20;
            score += 20;
        }
    }
    
    /* Size validation */
    if (fmt->min_size > 0 && file_size >= fmt->min_size) {
        info->size_score += 5;
        score += 5;
    }
    if (fmt->max_size > 0 && file_size <= fmt->max_size) {
        info->size_score += 5;
        score += 5;
    }
    for (size_t i = 0; i < fmt->fixed_size_count; i++) {
        if (file_size == fmt->fixed_sizes[i]) {
            info->size_score += 20;
            score += 20;
            break;
        }
    }
    
    /* Custom probe function */
    if (fmt->probe) {
        int probe_score = fmt->probe(data, len, file_size);
        info->structure_score = probe_score;
        score += probe_score;
    }
    
    info->score = score;
    info->confidence = (score > 100) ? 100 : (uint8_t)score;
    
    return score;
}

uft_format_info_t uft_format_detect(const uft_format_registry_t *reg,
                                    const uint8_t *data,
                                    size_t len,
                                    size_t file_size,
                                    const char *filename) {
    uft_format_candidates_t candidates;
    uft_format_detect_all(reg, data, len, file_size, filename, &candidates);
    
    if (candidates.best) {
        return *candidates.best;
    }
    
    uft_format_info_t unknown = {0};
    unknown.name = "Unknown";
    return unknown;
}

void uft_format_detect_all(const uft_format_registry_t *reg,
                           const uint8_t *data,
                           size_t len,
                           size_t file_size,
                           const char *filename,
                           uft_format_candidates_t *candidates) {
    if (!candidates) return;
    memset(candidates, 0, sizeof(*candidates));
    
    candidates->filename = filename;
    candidates->file_size = file_size;
    
    if (!reg || !data || len == 0) return;
    
    /* Score each format */
    for (size_t i = 0; i < reg->count && candidates->count < 16; i++) {
        uft_format_info_t info = {0};
        int score = score_format(&reg->formats[i], data, len, file_size, filename, &info);
        
        if (score > 0) {
            candidates->results[candidates->count++] = info;
        }
    }
    
    /* Find best */
    if (candidates->count > 0) {
        candidates->best = &candidates->results[0];
        for (size_t i = 1; i < candidates->count; i++) {
            if (candidates->results[i].score > candidates->best->score) {
                candidates->best = &candidates->results[i];
            }
        }
    }
}

int uft_format_validate(const uft_format_registry_t *reg,
                        const char *format_name,
                        const uint8_t *data,
                        size_t len) {
    if (!reg || !format_name || !data) return 0;
    
    const uft_format_descriptor_t *fmt = uft_format_registry_find(reg, format_name);
    if (!fmt) return 0;
    
    if (fmt->validate) {
        return fmt->validate(data, len, len);
    }
    
    /* Default validation: check magic and probe */
    int score = 0;
    if (fmt->magic && len >= fmt->magic_len) {
        if (memcmp(data, fmt->magic, fmt->magic_len) == 0) {
            score += 50;
        }
    }
    if (fmt->probe) {
        score += fmt->probe(data, len, len);
    }
    
    return (score > 100) ? 100 : score;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* uft_format_category_name(uft_format_category_t cat) {
    switch (cat) {
        case UFT_FORMAT_CAT_RAW_SECTOR: return "Raw Sector";
        case UFT_FORMAT_CAT_BITSTREAM:  return "Bitstream";
        case UFT_FORMAT_CAT_STRUCTURED: return "Structured";
        case UFT_FORMAT_CAT_ARCHIVE:    return "Archive";
        case UFT_FORMAT_CAT_EMULATOR:   return "Emulator";
        default:                        return "Unknown";
    }
}

const char* uft_format_platform_name(uft_platform_t plat) {
    switch (plat) {
        case UFT_PLATFORM_IBM_PC:   return "IBM PC";
        case UFT_PLATFORM_AMIGA:    return "Amiga";
        case UFT_PLATFORM_ATARI_ST: return "Atari ST";
        case UFT_PLATFORM_C64:      return "Commodore 64";
        case UFT_PLATFORM_APPLE2:   return "Apple II";
        case UFT_PLATFORM_MAC:      return "Macintosh";
        case UFT_PLATFORM_MSX:      return "MSX";
        case UFT_PLATFORM_BBC:      return "BBC Micro";
        case UFT_PLATFORM_CPC:      return "Amstrad CPC";
        case UFT_PLATFORM_TRS80:    return "TRS-80";
        case UFT_PLATFORM_PC98:     return "PC-98";
        case UFT_PLATFORM_MULTI:    return "Multi-platform";
        default:                    return "Generic";
    }
}

void uft_format_dump_candidates(const uft_format_candidates_t *candidates) {
    if (!candidates) {
        printf("Format Detection: NULL\n");
        return;
    }
    
    printf("=== Format Detection Results ===\n");
    if (candidates->filename) {
        printf("File: %s\n", candidates->filename);
    }
    printf("Size: %zu bytes\n\n", candidates->file_size);
    
    printf("Candidates:\n");
    for (size_t i = 0; i < candidates->count; i++) {
        const uft_format_info_t *f = &candidates->results[i];
        printf("  %s: score=%d (magic=%d struct=%d size=%d ext=%d)",
               f->name, f->score,
               f->magic_score, f->structure_score,
               f->size_score, f->extension_score);
        if (f == candidates->best) {
            printf(" [BEST]");
        }
        printf("\n");
    }
    
    if (candidates->best) {
        printf("\nDetected: %s (%s, %s)\n",
               candidates->best->name,
               uft_format_category_name(candidates->best->category),
               uft_format_platform_name(candidates->best->platform));
    }
}
