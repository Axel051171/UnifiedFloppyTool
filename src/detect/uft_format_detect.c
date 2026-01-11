/**
 * @file uft_format_detect.c
 * @brief Score-based Format Auto-Detection Engine Implementation
 * 
 * Implements ADR-0004: Score-basierte Format Auto-Detection
 */

#include "uft/detect/uft_format_detect.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * MAGIC SIGNATURES
 *============================================================================*/

/* Commodore */
static const uint8_t D64_BAM_TRACK = 18;
static const uint8_t D64_BAM_SECTOR = 0;

/* Container formats */
static const uint8_t HFE_SIGNATURE[] = "HXCPICFE";
static const uint8_t SCP_SIGNATURE[] = "SCP";
static const uint8_t WOZ_SIGNATURE[] = "WOZ1";
static const uint8_t WOZ2_SIGNATURE[] = "WOZ2";
static const uint8_t IMD_SIGNATURE[] = "IMD ";
static const uint8_t TD0_SIGNATURE[] = { 'T', 'D' };
static const uint8_t TD0_ADV_SIGNATURE[] = { 't', 'd' };
static const uint8_t IPF_SIGNATURE[] = "CAPS";
static const uint8_t MFI_SIGNATURE[] = "MFI\x00";
static const uint8_t DC42_SIGNATURE = 0x01;  /* At offset 0x52 */

/* Japanese formats */
static const uint8_t D88_SIGNATURE[] = { 0x00 };  /* First byte often 0 */
static const uint8_t FDI_SIGNATURE[] = { 0x00 };

/* Spectrum */
static const uint8_t SCL_SIGNATURE[] = "SINCLAIR";
static const uint8_t TRD_SIGNATURE = 0x10;  /* Disk type at 0x8E3 */

/* CPC/Spectrum DSK */
static const uint8_t DSK_SIGNATURE[] = "MV - CPC";
static const uint8_t EDSK_SIGNATURE[] = "EXTENDED";

/* Atari */
static const uint8_t ATR_SIGNATURE[] = { 0x96, 0x02 };  /* NICKATARI */
static const uint8_t STX_SIGNATURE[] = "RSY\x00";

/* Apple 2IMG */
static const uint8_t IMG2_SIGNATURE[] = "2IMG";

/* FAT boot signature */
static const uint8_t FAT_BOOT_SIG[] = { 0x55, 0xAA };

/*============================================================================
 * PROBE FUNCTIONS
 *============================================================================*/

/* --- D64: Commodore 1541 --- */
static int probe_d64(const uint8_t *data, size_t len) {
    /* D64 sizes: 174848 (35 tracks), 175531 (35 + errors), 
                  196608 (40 tracks), 197376 (40 + errors) */
    if (len == 174848 || len == 175531) {
        /* Check BAM signature at track 18, sector 0 (offset 0x16500) */
        if (len >= 0x16500 + 256) {
            const uint8_t *bam = data + 0x16500;
            /* First byte of BAM should be track 18 (0x12) */
            if (bam[0] == 0x12 && bam[1] == 0x01) {
                return 95;  /* Very high confidence */
            }
            /* Check disk name area has printable chars */
            int printable = 0;
            for (int i = 0x90; i < 0xA0; i++) {
                if (bam[i] >= 0x20 && bam[i] <= 0x7E) printable++;
            }
            if (printable > 8) return 80;
        }
        return 70;  /* Size matches exactly */
    }
    if (len == 196608 || len == 197376) {
        return 65;  /* 40-track variant */
    }
    return 0;
}

/* --- G64: GCR-encoded 1541 --- */
static int probe_g64(const uint8_t *data, size_t len) {
    if (len < 12) return 0;
    if (memcmp(data, "GCR-1541", 8) == 0) {
        return 98;  /* Definitive signature */
    }
    return 0;
}

/* --- ADF: Amiga Disk File --- */
static int probe_adf(const uint8_t *data, size_t len) {
    /* Standard ADF: 901120 bytes (DD), 1802240 (HD) */
    if (len == 901120 || len == 1802240) {
        /* Check for "DOS" signature at start (bootblock) */
        if (len >= 4 && data[0] == 'D' && data[1] == 'O' && data[2] == 'S') {
            return 92;
        }
        /* Check for NDOS (non-bootable) */
        if (len >= 4 && data[0] == 'N' && data[1] == 'D' && 
            data[2] == 'O' && data[3] == 'S') {
            return 90;
        }
        /* Even without boot signature, size is very specific */
        return 75;
    }
    return 0;
}

/* --- HFE: HxC Floppy Emulator --- */
static int probe_hfe(const uint8_t *data, size_t len) {
    if (len < 512) return 0;
    if (memcmp(data, HFE_SIGNATURE, 8) == 0) {
        return 99;  /* Definitive signature */
    }
    return 0;
}

/* --- SCP: SuperCard Pro --- */
static int probe_scp(const uint8_t *data, size_t len) {
    if (len < 16) return 0;
    if (memcmp(data, SCP_SIGNATURE, 3) == 0) {
        return 99;
    }
    return 0;
}

/* --- WOZ: Apple II flux --- */
static int probe_woz(const uint8_t *data, size_t len) {
    if (len < 12) return 0;
    if (memcmp(data, WOZ_SIGNATURE, 4) == 0 || 
        memcmp(data, WOZ2_SIGNATURE, 4) == 0) {
        /* Check for INFO chunk */
        if (len >= 12 && memcmp(data + 8, "INFO", 4) == 0) {
            return 99;
        }
        return 95;
    }
    return 0;
}

/* --- IMD: ImageDisk --- */
static int probe_imd(const uint8_t *data, size_t len) {
    if (len < 32) return 0;
    if (memcmp(data, IMD_SIGNATURE, 4) == 0) {
        return 98;
    }
    return 0;
}

/* --- TD0: Teledisk --- */
static int probe_td0(const uint8_t *data, size_t len) {
    if (len < 12) return 0;
    if (memcmp(data, TD0_SIGNATURE, 2) == 0 ||
        memcmp(data, TD0_ADV_SIGNATURE, 2) == 0) {
        return 95;
    }
    return 0;
}

/* --- DSK/EDSK: CPC/Spectrum --- */
static int probe_dsk(const uint8_t *data, size_t len) {
    if (len < 256) return 0;
    if (memcmp(data, DSK_SIGNATURE, 8) == 0) {
        return 95;
    }
    if (memcmp(data, EDSK_SIGNATURE, 8) == 0) {
        return 98;  /* EDSK more specific */
    }
    return 0;
}

/* --- ATR: Atari 8-bit --- */
static int probe_atr(const uint8_t *data, size_t len) {
    if (len < 16) return 0;
    if (memcmp(data, ATR_SIGNATURE, 2) == 0) {
        return 95;
    }
    return 0;
}

/* --- STX: Atari ST Extended --- */
static int probe_stx(const uint8_t *data, size_t len) {
    if (len < 16) return 0;
    if (memcmp(data, STX_SIGNATURE, 4) == 0) {
        return 95;
    }
    return 0;
}

/* --- ST: Atari ST raw --- */
static int probe_st(const uint8_t *data, size_t len) {
    /* Common ST sizes */
    if (len == 368640 || len == 737280 || len == 819200 || len == 1474560) {
        /* Check for FAT boot sector */
        if (len >= 512 && data[510] == 0x55 && data[511] == 0xAA) {
            /* Could be ST or generic FAT - check media descriptor */
            if (len >= 22 && (data[21] == 0xF8 || data[21] == 0xF9 ||
                              data[21] == 0xFA || data[21] == 0xFB)) {
                return 50;  /* Possible ST, but also could be PC */
            }
        }
        return 30;  /* Size matches but no signature */
    }
    return 0;
}

/* --- D88: Japanese PC-88/98 --- */
static int probe_d88(const uint8_t *data, size_t len) {
    if (len < 688) return 0;
    
    /* Check header structure */
    /* Bytes 0-16: Disk name (null-terminated) */
    /* Byte 0x1A: Write protect flag (0 or 0x10) */
    /* Byte 0x1B: Media type */
    /* Bytes 0x1C-0x1F: Disk size (little-endian) */
    
    uint32_t disk_size = data[0x1C] | (data[0x1D] << 8) | 
                         (data[0x1E] << 16) | (data[0x1F] << 24);
    
    if (disk_size > 0 && disk_size <= len && disk_size >= 688) {
        /* Check media type is valid */
        uint8_t media = data[0x1B];
        if (media == 0x00 || media == 0x10 || media == 0x20 || media == 0x30) {
            return 85;
        }
        return 60;
    }
    return 0;
}

/* --- D77: Japanese FM-7 --- */
static int probe_d77(const uint8_t *data, size_t len) {
    /* D77 is very similar to D88 */
    if (len < 688) return 0;
    
    uint32_t disk_size = data[0x1C] | (data[0x1D] << 8) | 
                         (data[0x1E] << 16) | (data[0x1F] << 24);
    
    /* D77 typically smaller than D88 */
    if (disk_size > 0 && disk_size <= len && disk_size < 400000) {
        return 70;
    }
    return 0;
}

/* --- SCL: Sinclair --- */
static int probe_scl(const uint8_t *data, size_t len) {
    if (len < 9) return 0;
    if (memcmp(data, SCL_SIGNATURE, 8) == 0) {
        return 98;
    }
    return 0;
}

/* --- TRD: TR-DOS --- */
static int probe_trd(const uint8_t *data, size_t len) {
    /* TRD: 640K standard */
    if (len == 655360 || len == 655360 - 256) {
        /* Check disk type at 0x8E3 */
        if (len > 0x8E3 && data[0x8E3] == 0x10) {
            return 85;
        }
        return 60;
    }
    return 0;
}

/* --- DC42: DiskCopy 4.2 --- */
static int probe_dc42(const uint8_t *data, size_t len) {
    if (len < 84) return 0;
    /* Check signature byte at 0x52 */
    if (data[0x52] == DC42_SIGNATURE && data[0x53] == 0x00) {
        /* Validate disk name length */
        if (data[0] > 0 && data[0] < 64) {
            return 92;
        }
        return 70;
    }
    return 0;
}

/* --- IPF: SPS Interchangeable --- */
static int probe_ipf(const uint8_t *data, size_t len) {
    if (len < 12) return 0;
    if (memcmp(data, IPF_SIGNATURE, 4) == 0) {
        return 99;
    }
    return 0;
}

/* --- MFI: MAME Floppy Image --- */
static int probe_mfi(const uint8_t *data, size_t len) {
    if (len < 16) return 0;
    if (data[0] == 'M' && data[1] == 'F' && data[2] == 'I') {
        return 95;
    }
    return 0;
}

/* --- 2IMG: Apple container --- */
static int probe_2img(const uint8_t *data, size_t len) {
    if (len < 64) return 0;
    if (memcmp(data, IMG2_SIGNATURE, 4) == 0) {
        return 98;
    }
    return 0;
}

/* --- FAT: Generic FAT filesystem --- */
static int probe_fat(const uint8_t *data, size_t len) {
    if (len < 512) return 0;
    
    /* Check boot signature */
    if (data[510] != 0x55 || data[511] != 0xAA) {
        return 0;
    }
    
    int score = 20;  /* Boot signature present */
    
    /* Check BPB fields */
    uint16_t bytes_per_sector = data[11] | (data[12] << 8);
    if (bytes_per_sector == 512 || bytes_per_sector == 1024 ||
        bytes_per_sector == 2048 || bytes_per_sector == 4096) {
        score += 10;
    }
    
    uint8_t sectors_per_cluster = data[13];
    if (sectors_per_cluster >= 1 && sectors_per_cluster <= 128 &&
        (sectors_per_cluster & (sectors_per_cluster - 1)) == 0) {
        score += 10;  /* Power of 2 */
    }
    
    uint8_t num_fats = data[16];
    if (num_fats == 1 || num_fats == 2) {
        score += 10;
    }
    
    uint8_t media_desc = data[21];
    if (media_desc >= 0xF0) {
        score += 10;  /* Valid media descriptor */
    }
    
    /* Check for common floppy sizes */
    if (len == 368640 || len == 737280 || len == 1474560 ||
        len == 1228800 || len == 2949120) {
        score += 15;
    }
    
    /* Cap at 75 - FAT is generic and shouldn't override specific formats */
    return (score > 75) ? 75 : score;
}

/*============================================================================
 * DETECTOR REGISTRY
 *============================================================================*/

static const uft_format_detector_t builtin_detectors[] = {
    /* High-specificity formats (clear signatures) - highest priority */
    { UFT_FORMAT_HFE,     "HFE",           ".hfe",          probe_hfe,  100, 512 },
    { UFT_FORMAT_SCP,     "SuperCard Pro", ".scp",          probe_scp,  100, 16 },
    { UFT_FORMAT_WOZ,     "WOZ",           ".woz",          probe_woz,  100, 12 },
    { UFT_FORMAT_G64,     "G64",           ".g64",          probe_g64,  100, 12 },
    { UFT_FORMAT_IPF,     "IPF",           ".ipf",          probe_ipf,  100, 12 },
    { UFT_FORMAT_IMD,     "ImageDisk",     ".imd",          probe_imd,  100, 32 },
    { UFT_FORMAT_EDSK,    "Extended DSK",  ".dsk",          probe_dsk,   99, 256 },
    { UFT_FORMAT_DSK,     "CPC DSK",       ".dsk",          probe_dsk,   98, 256 },
    { UFT_FORMAT_SCL,     "Sinclair SCL",  ".scl",          probe_scl,   98, 9 },
    { UFT_FORMAT_TD0,     "Teledisk",      ".td0",          probe_td0,   95, 12 },
    { UFT_FORMAT_ATR,     "Atari ATR",     ".atr",          probe_atr,   95, 16 },
    { UFT_FORMAT_STX,     "Atari STX",     ".stx",          probe_stx,   95, 16 },
    { UFT_FORMAT_DC42,    "DiskCopy 4.2",  ".dc42;.image",  probe_dc42,  95, 84 },
    { UFT_FORMAT_2IMG,    "Apple 2IMG",    ".2img;.2mg",    probe_2img,  95, 64 },
    { UFT_FORMAT_MFI,     "MAME MFI",      ".mfi",          probe_mfi,   95, 16 },
    
    /* Platform-specific formats */
    { UFT_FORMAT_D64,     "D64",           ".d64",          probe_d64,   90, 174848 },
    { UFT_FORMAT_ADF,     "Amiga ADF",     ".adf",          probe_adf,   90, 512 },
    { UFT_FORMAT_D88,     "D88",           ".d88;.88d",     probe_d88,   85, 688 },
    { UFT_FORMAT_D77,     "D77",           ".d77;.77d",     probe_d77,   80, 688 },
    { UFT_FORMAT_TRD,     "TR-DOS",        ".trd",          probe_trd,   80, 512 },
    
    /* Low-specificity formats (generic) - lowest priority */
    { UFT_FORMAT_ST,      "Atari ST",      ".st",           probe_st,    40, 512 },
    { UFT_FORMAT_FAT12,   "FAT12",         ".img;.ima",     probe_fat,   30, 512 },
    
    /* Sentinel */
    { UFT_FORMAT_UNKNOWN, NULL, NULL, NULL, 0, 0 }
};

/* Custom detector registry */
#define MAX_CUSTOM_DETECTORS 32
static uft_format_detector_t custom_detectors[MAX_CUSTOM_DETECTORS];
static size_t custom_detector_count = 0;

/*============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

int uft_detect_format(const uint8_t *data, size_t len, 
                      uft_detect_result_t *result) {
    if (!data || !result) {
        return -1;
    }
    
    memset(result, 0, sizeof(*result));
    result->format = UFT_FORMAT_UNKNOWN;
    result->format_name = "Unknown";
    result->extensions = "";
    result->confidence = 0;
    
    int best_score = 0;
    int second_score = 0;
    const uft_format_detector_t *best = NULL;
    const uft_format_detector_t *second = NULL;
    
    /* Check builtin detectors */
    for (const uft_format_detector_t *d = builtin_detectors; d->name; d++) {
        if (len < d->min_size) continue;
        
        int raw_score = d->probe(data, len);
        if (raw_score == 0) continue;
        
        /* Apply priority weighting */
        int weighted_score = (raw_score * d->priority) / 100;
        
        if (weighted_score > best_score) {
            second_score = best_score;
            second = best;
            best_score = weighted_score;
            best = d;
        } else if (weighted_score > second_score) {
            second_score = weighted_score;
            second = d;
        }
        
        /* Early exit on perfect score */
        if (raw_score >= 99 && d->priority >= 95) {
            break;
        }
    }
    
    /* Check custom detectors */
    for (size_t i = 0; i < custom_detector_count; i++) {
        const uft_format_detector_t *d = &custom_detectors[i];
        if (!d->probe || len < d->min_size) continue;
        
        int raw_score = d->probe(data, len);
        if (raw_score == 0) continue;
        
        int weighted_score = (raw_score * d->priority) / 100;
        
        if (weighted_score > best_score) {
            second_score = best_score;
            second = best;
            best_score = weighted_score;
            best = d;
        }
    }
    
    /* Populate result */
    if (best) {
        result->format = best->format;
        result->format_name = best->name;
        result->extensions = best->extensions;
        result->confidence = best_score;
        result->probe_score = best->probe(data, len);
        
        snprintf(result->reason, sizeof(result->reason),
                 "Matched %s with score %d (priority %d)",
                 best->name, result->probe_score, best->priority);
    }
    
    if (second) {
        result->alt_format = second->format;
        result->alt_confidence = second_score;
    }
    
    return 0;
}

const char* uft_format_name(uft_format_t format) {
    for (const uft_format_detector_t *d = builtin_detectors; d->name; d++) {
        if (d->format == format) {
            return d->name;
        }
    }
    return "Unknown";
}

const char* uft_format_extensions(uft_format_t format) {
    for (const uft_format_detector_t *d = builtin_detectors; d->name; d++) {
        if (d->format == format) {
            return d->extensions;
        }
    }
    return "";
}

bool uft_format_is_flux(uft_format_t format) {
    return format == UFT_FORMAT_SCP ||
           format == UFT_FORMAT_KFX ||
           format == UFT_FORMAT_WOZ ||
           format == UFT_FORMAT_G64 ||
           format == UFT_FORMAT_MFI;
}

bool uft_format_is_container(uft_format_t format) {
    return format == UFT_FORMAT_HFE ||
           format == UFT_FORMAT_IMD ||
           format == UFT_FORMAT_TD0 ||
           format == UFT_FORMAT_DSK ||
           format == UFT_FORMAT_EDSK ||
           format == UFT_FORMAT_IPF ||
           format == UFT_FORMAT_2IMG;
}

const uft_format_detector_t* uft_get_detectors(size_t *count) {
    if (count) {
        *count = 0;
        for (const uft_format_detector_t *d = builtin_detectors; d->name; d++) {
            (*count)++;
        }
    }
    return builtin_detectors;
}

int uft_register_detector(const uft_format_detector_t *detector) {
    if (!detector || !detector->probe || !detector->name) {
        return -1;
    }
    if (custom_detector_count >= MAX_CUSTOM_DETECTORS) {
        return -1;
    }
    custom_detectors[custom_detector_count++] = *detector;
    return 0;
}
