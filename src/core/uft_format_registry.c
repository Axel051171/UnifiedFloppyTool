/**
 * @file uft_format_registry.c
 * @brief Unified Format Registry Implementation (P2-ARCH-007)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_format_registry.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Magic Bytes
 *===========================================================================*/

static const uint8_t MAGIC_SCP[] = { 'S', 'C', 'P' };
static const uint8_t MAGIC_HFE[] = { 'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E' };
static const uint8_t MAGIC_IPF[] = { 'C', 'A', 'P', 'S' };
static const uint8_t MAGIC_IMD[] = { 'I', 'M', 'D', ' ' };
static const uint8_t MAGIC_TD0[] = { 'T', 'D' };
static const uint8_t MAGIC_TD0_ADV[] = { 't', 'd' };
static const uint8_t MAGIC_ATR[] = { 0x96, 0x02 };
static const uint8_t MAGIC_ATX[] = { 'A', 'T', '8', 'X' };
static const uint8_t MAGIC_WOZ[] = { 'W', 'O', 'Z', '1' };
static const uint8_t MAGIC_WOZ2[] = { 'W', 'O', 'Z', '2' };
static const uint8_t MAGIC_A2R[] = { 'A', '2', 'R', '2' };
static const uint8_t MAGIC_2IMG[] = { '2', 'I', 'M', 'G' };
static const uint8_t MAGIC_DC42[] = { 0x00, 0x00 }; /* + check byte 82 */
static const uint8_t MAGIC_EDSK[] = { 'E', 'X', 'T', 'E', 'N', 'D', 'E', 'D' };
static const uint8_t MAGIC_DSK_CPC[] = { 'M', 'V', ' ', '-' };
static const uint8_t MAGIC_DMS[] = { 'D', 'M', 'S', '!' };
static const uint8_t MAGIC_ZIP[] = { 'P', 'K', 0x03, 0x04 };
static const uint8_t MAGIC_GZIP[] = { 0x1F, 0x8B };
static const uint8_t MAGIC_UFT_IR[] = { 'U', 'F', 'T', 'I', 'R' };
static const uint8_t MAGIC_D88[] = { 0x00 }; /* Check structure */
static const uint8_t MAGIC_FDI[] = { 'F', 'D', 'I' };
static const uint8_t MAGIC_NFD[] = { 'N', 'F', 'D' };

/*===========================================================================
 * Format Database
 *===========================================================================*/

static const uft_format_info_t g_format_db[] = {
    /* Unknown */
    { UFT_FMT_UNKNOWN, "Unknown", "Unknown format", "",
      UFT_FCAT_UNKNOWN, UFT_FCAP_NONE, NULL, 0, 0, 0, 0, 0, "" },
    
    /* Sector Images - Amiga */
    { UFT_FMT_ADF, "ADF", "Amiga Disk File", "adf",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 901120, 901120, 901120, "Amiga" },
    { UFT_FMT_ADZ, "ADZ", "Compressed ADF", "adz",
      UFT_FCAT_SECTOR, UFT_FCAP_READ | UFT_FCAP_SECTOR | UFT_FCAP_COMPRESSION,
      MAGIC_GZIP, 2, 0, 100, 0, 400000, "Amiga" },
    { UFT_FMT_DMS, "DMS", "DiskMasher", "dms",
      UFT_FCAT_SECTOR, UFT_FCAP_READ | UFT_FCAP_SECTOR | UFT_FCAP_COMPRESSION,
      MAGIC_DMS, 4, 0, 100, 0, 500000, "Amiga" },
    
    /* Sector Images - Commodore */
    { UFT_FMT_D64, "D64", "C64 1541 Disk", "d64",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 174848, 196608, 174848, "C64" },
    { UFT_FMT_D71, "D71", "C128 1571 Disk", "d71",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 349696, 349696, 349696, "C128" },
    { UFT_FMT_D81, "D81", "C128 1581 Disk", "d81",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 819200, 819200, 819200, "C128" },
    { UFT_FMT_G64, "G64", "C64 GCR Bitstream", "g64",
      UFT_FCAT_BITSTREAM, UFT_FCAP_FULL | UFT_FCAP_BITSTREAM,
      (const uint8_t*)"GCR-1541", 8, 0, 200000, 0, 400000, "C64" },
    { UFT_FMT_NIB, "NIB", "NIBTOOLS Format", "nib,nbz",
      UFT_FCAT_BITSTREAM, UFT_FCAP_READ | UFT_FCAP_BITSTREAM,
      NULL, 0, 0, 300000, 0, 400000, "C64" },
    
    /* Sector Images - Atari */
    { UFT_FMT_ATR, "ATR", "Atari 8-bit Disk", "atr",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      MAGIC_ATR, 2, 0, 92176, 1048576, 92176, "Atari 8-bit" },
    { UFT_FMT_ATX, "ATX", "Atari Extended", "atx",
      UFT_FCAT_BITSTREAM, UFT_FCAP_READ | UFT_FCAP_BITSTREAM | UFT_FCAP_PROTECTION,
      MAGIC_ATX, 4, 0, 50000, 0, 200000, "Atari 8-bit" },
    { UFT_FMT_ST, "ST", "Atari ST Disk", "st",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR,
      NULL, 0, 0, 360*1024, 1474560, 737280, "Atari ST" },
    { UFT_FMT_STX, "STX", "Atari ST Extended", "stx",
      UFT_FCAT_BITSTREAM, UFT_FCAP_READ | UFT_FCAP_BITSTREAM | UFT_FCAP_PROTECTION,
      NULL, 0, 0, 50000, 0, 2000000, "Atari ST" },
    { UFT_FMT_MSA, "MSA", "Magic Shadow Archiver", "msa",
      UFT_FCAT_SECTOR, UFT_FCAP_READ | UFT_FCAP_SECTOR | UFT_FCAP_COMPRESSION,
      (const uint8_t*)"\x0E\x0F", 2, 0, 100, 0, 800000, "Atari ST" },
    
    /* Sector Images - Apple */
    { UFT_FMT_DSK_APPLE, "DSK", "Apple II DOS Order", "dsk,do",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 143360, 143360, 143360, "Apple II" },
    { UFT_FMT_PO, "PO", "Apple ProDOS Order", "po",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 143360, 819200, 143360, "Apple II" },
    { UFT_FMT_WOZ, "WOZ", "WOZ Flux Image", "woz",
      UFT_FCAT_FLUX, UFT_FCAP_RW | UFT_FCAP_FLUX | UFT_FCAP_WEAK_BITS,
      MAGIC_WOZ, 4, 0, 50000, 0, 500000, "Apple II" },
    { UFT_FMT_A2R, "A2R", "Applesauce A2R", "a2r",
      UFT_FCAT_FLUX, UFT_FCAP_READ | UFT_FCAP_FLUX | UFT_FCAP_MULTI_REV,
      MAGIC_A2R, 4, 0, 50000, 0, 2000000, "Apple II" },
    { UFT_FMT_2IMG, "2IMG", "2IMG Universal", "2mg,2img",
      UFT_FCAT_SECTOR, UFT_FCAP_RW | UFT_FCAP_SECTOR | UFT_FCAP_METADATA,
      MAGIC_2IMG, 4, 0, 143360, 819200, 143360, "Apple II,Mac" },
    { UFT_FMT_DC42, "DC42", "DiskCopy 4.2", "dc42,image",
      UFT_FCAT_SECTOR, UFT_FCAP_RW | UFT_FCAP_SECTOR,
      NULL, 0, 0, 400*1024, 1474560, 819200, "Macintosh" },
    
    /* Sector Images - PC/IBM */
    { UFT_FMT_IMD, "IMD", "ImageDisk", "imd",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_METADATA,
      MAGIC_IMD, 4, 0, 1000, 0, 2000000, "IBM PC" },
    { UFT_FMT_TD0, "TD0", "Teledisk", "td0",
      UFT_FCAT_SECTOR, UFT_FCAP_READ | UFT_FCAP_SECTOR | UFT_FCAP_COMPRESSION,
      MAGIC_TD0, 2, 0, 1000, 0, 2000000, "IBM PC" },
    { UFT_FMT_D88, "D88", "D88 Image", "d88,d77,1dd",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR,
      NULL, 0, 0, 300000, 0, 1500000, "PC-98,X68000,FM-7" },
    { UFT_FMT_FDI, "FDI", "FDI Image", "fdi",
      UFT_FCAT_SECTOR, UFT_FCAP_RW | UFT_FCAP_SECTOR,
      MAGIC_FDI, 3, 0, 100000, 0, 2000000, "PC-98" },
    
    /* Sector Images - British */
    { UFT_FMT_SSD, "SSD", "BBC Single-Sided", "ssd",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 102400, 204800, 102400, "BBC Micro" },
    { UFT_FMT_DSD, "DSD", "BBC Double-Sided", "dsd",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 204800, 409600, 204800, "BBC Micro" },
    { UFT_FMT_EDSK, "EDSK", "Extended DSK", "dsk",
      UFT_FCAT_BITSTREAM, UFT_FCAP_FULL | UFT_FCAP_BITSTREAM | UFT_FCAP_PROTECTION,
      MAGIC_EDSK, 8, 0, 1000, 0, 1000000, "Amstrad CPC,Spectrum" },
    { UFT_FMT_TRD, "TRD", "TR-DOS Image", "trd",
      UFT_FCAT_SECTOR, UFT_FCAP_FULL | UFT_FCAP_SECTOR | UFT_FCAP_FILESYSTEM,
      NULL, 0, 0, 640*1024, 640*1024, 655360, "ZX Spectrum" },
    
    /* Flux Images */
    { UFT_FMT_SCP, "SCP", "SuperCard Pro", "scp",
      UFT_FCAT_FLUX, UFT_FCAP_FULL | UFT_FCAP_FLUX | UFT_FCAP_MULTI_REV | UFT_FCAP_TIMING,
      MAGIC_SCP, 3, 0, 50000, 0, 50000000, "Universal" },
    { UFT_FMT_KF, "KF", "KryoFlux Stream", "raw",
      UFT_FCAT_FLUX, UFT_FCAP_READ | UFT_FCAP_FLUX | UFT_FCAP_TIMING,
      NULL, 0, 0, 10000, 0, 10000000, "Universal" },
    { UFT_FMT_IPF, "IPF", "Interchangeable", "ipf",
      UFT_FCAT_FLUX, UFT_FCAP_READ | UFT_FCAP_FLUX | UFT_FCAP_WEAK_BITS | UFT_FCAP_PROTECTION,
      MAGIC_IPF, 4, 0, 50000, 0, 10000000, "Universal" },
    
    /* Bitstream Images */
    { UFT_FMT_HFE, "HFE", "UFT HFE Format", "hfe",
      UFT_FCAT_BITSTREAM, UFT_FCAP_FULL | UFT_FCAP_BITSTREAM,
      MAGIC_HFE, 8, 0, 50000, 0, 5000000, "Universal" },
    { UFT_FMT_DMK, "DMK", "DMK Format", "dmk",
      UFT_FCAT_BITSTREAM, UFT_FCAP_FULL | UFT_FCAP_BITSTREAM,
      NULL, 0, 0, 50000, 0, 1000000, "TRS-80" },
    
    /* Archives */
    { UFT_FMT_ZIP, "ZIP", "ZIP Archive", "zip",
      UFT_FCAT_ARCHIVE, UFT_FCAP_READ | UFT_FCAP_COMPRESSION,
      MAGIC_ZIP, 4, 0, 22, 0, 0, "" },
    { UFT_FMT_GZIP, "GZIP", "GZIP Compressed", "gz",
      UFT_FCAT_ARCHIVE, UFT_FCAP_READ | UFT_FCAP_COMPRESSION,
      MAGIC_GZIP, 2, 0, 10, 0, 0, "" },
    
    /* UFT Native */
    { UFT_FMT_UFT_IR, "UFT-IR", "UFT Intermediate", "uir",
      UFT_FCAT_NATIVE, UFT_FCAP_FULL | UFT_FCAP_FLUX | UFT_FCAP_MULTI_REV | UFT_FCAP_METADATA,
      MAGIC_UFT_IR, 5, 0, 100, 0, 100000000, "UFT" },
};

#define FORMAT_DB_SIZE (sizeof(g_format_db) / sizeof(g_format_db[0]))

/*===========================================================================
 * Lookup Functions
 *===========================================================================*/

const uft_format_info_t* uft_format_get_info(uft_format_id_t id)
{
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        if (g_format_db[i].id == id) {
            return &g_format_db[i];
        }
    }
    return &g_format_db[0];  /* Unknown */
}

const uft_format_info_t* uft_format_get_by_name(const char *name)
{
    if (!name) return NULL;
    
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        if (strcasecmp(g_format_db[i].name, name) == 0) {
            return &g_format_db[i];
        }
    }
    return NULL;
}

const uft_format_info_t* uft_format_get_by_ext(const char *ext)
{
    if (!ext) return NULL;
    
    /* Skip leading dot */
    if (*ext == '.') ext++;
    
    for (size_t i = 0; i < FORMAT_DB_SIZE; i++) {
        const char *exts = g_format_db[i].extensions;
        if (!exts || !*exts) continue;
        
        /* Check each extension in comma-separated list */
        char buf[256];
        snprintf(buf, sizeof(buf), "%s", exts);
        char *token = strtok(buf, ",");
        while (token) {
            while (*token == ' ') token++;
            if (strcasecmp(token, ext) == 0) {
                return &g_format_db[i];
            }
            token = strtok(NULL, ",");
        }
    }
    return NULL;
}

const char* uft_format_name(uft_format_id_t id)
{
    const uft_format_info_t *info = uft_format_get_info(id);
    return info ? info->name : "Unknown";
}

const char* uft_format_desc(uft_format_id_t id)
{
    const uft_format_info_t *info = uft_format_get_info(id);
    return info ? info->description : "Unknown format";
}

const char* uft_format_extensions(uft_format_id_t id)
{
    const uft_format_info_t *info = uft_format_get_info(id);
    return info ? info->extensions : "";
}

bool uft_format_has_cap(uft_format_id_t id, uint32_t cap)
{
    const uft_format_info_t *info = uft_format_get_info(id);
    return info && (info->capabilities & cap) == cap;
}

const char* uft_format_category_name(uft_format_category_t cat)
{
    switch (cat) {
        case UFT_FCAT_SECTOR:    return "Sector Image";
        case UFT_FCAT_BITSTREAM: return "Bitstream Image";
        case UFT_FCAT_FLUX:      return "Flux Capture";
        case UFT_FCAT_ARCHIVE:   return "Archive";
        case UFT_FCAT_NATIVE:    return "UFT Native";
        default:                 return "Unknown";
    }
}

size_t uft_format_count(void)
{
    return FORMAT_DB_SIZE;
}

const uft_format_info_t* uft_format_iterate(size_t index)
{
    if (index >= FORMAT_DB_SIZE) return NULL;
    return &g_format_db[index];
}

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

uft_format_id_t uft_format_detect_ext(const char *ext)
{
    const uft_format_info_t *info = uft_format_get_by_ext(ext);
    return info ? info->id : UFT_FMT_UNKNOWN;
}

int uft_format_detect_buffer(const uint8_t *data, size_t size,
                             const char *ext,
                             uft_format_detect_result_t *result)
{
    if (!result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format = UFT_FMT_UNKNOWN;
    result->confidence = 0.0f;
    
    /* Try magic detection first */
    for (size_t i = 1; i < FORMAT_DB_SIZE; i++) {
        const uft_format_info_t *fmt = &g_format_db[i];
        
        if (!fmt->magic || fmt->magic_len == 0) continue;
        if (size < fmt->magic_offset + fmt->magic_len) continue;
        
        if (memcmp(data + fmt->magic_offset, fmt->magic, fmt->magic_len) == 0) {
            result->format = fmt->id;
            result->confidence = 0.9f;
            snprintf(result->message, sizeof(result->message),
                     "Detected by magic bytes");
            
            /* Check size constraints */
            if (fmt->min_size && size < fmt->min_size) {
                result->confidence *= 0.7f;
            }
            if (fmt->max_size && size > fmt->max_size) {
                result->confidence *= 0.8f;
            }
            
            return 0;
        }
    }
    
    /* Fall back to extension */
    if (ext) {
        uft_format_id_t ext_fmt = uft_format_detect_ext(ext);
        if (ext_fmt != UFT_FMT_UNKNOWN) {
            const uft_format_info_t *fmt = uft_format_get_info(ext_fmt);
            
            /* Check size for confidence */
            float conf = 0.5f;
            if (fmt->typical_size && size == fmt->typical_size) {
                conf = 0.8f;
            } else if (fmt->min_size && fmt->max_size &&
                       size >= fmt->min_size && size <= fmt->max_size) {
                conf = 0.6f;
            }
            
            result->format = ext_fmt;
            result->confidence = conf;
            snprintf(result->message, sizeof(result->message),
                     "Detected by extension .%s", ext);
            return 0;
        }
    }
    
    snprintf(result->message, sizeof(result->message), "Unknown format");
    return -1;
}

int uft_format_detect_file(const char *path,
                           uft_format_detect_result_t *result)
{
    if (!path || !result) return -1;
    
    /* Get extension */
    const char *ext = strrchr(path, '.');
    if (ext) ext++;
    
    /* Read header */
    FILE *f = fopen(path, "rb");
    if (!f) {
        result->format = UFT_FMT_UNKNOWN;
        result->confidence = 0.0f;
        snprintf(result->message, sizeof(result->message), "Cannot open file");
        return -1;
    }
    
    uint8_t header[256];
    size_t read = fread(header, 1, sizeof(header), f);
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = (size_t)ftell(f);
    fclose(f);
    
    return uft_format_detect_buffer(header, read > file_size ? file_size : read, 
                                     ext, result);
}

size_t uft_format_list_by_category(uft_format_category_t cat,
                                    uft_format_id_t *out, size_t max)
{
    if (!out || max == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < FORMAT_DB_SIZE && count < max; i++) {
        if (g_format_db[i].category == cat) {
            out[count++] = g_format_db[i].id;
        }
    }
    return count;
}

size_t uft_format_list_by_platform(const char *platform,
                                    uft_format_id_t *out, size_t max)
{
    if (!platform || !out || max == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < FORMAT_DB_SIZE && count < max; i++) {
        if (g_format_db[i].platforms && 
            strstr(g_format_db[i].platforms, platform)) {
            out[count++] = g_format_db[i].id;
        }
    }
    return count;
}

int uft_format_probe(const uint8_t *data, size_t size,
                     uft_format_id_t hint,
                     uft_format_detect_result_t *result)
{
    /* Basic detect first */
    int ret = uft_format_detect_buffer(data, size, NULL, result);
    
    /* If hint matches and confidence is low, boost it */
    if (hint != UFT_FMT_UNKNOWN && result->format == hint) {
        result->confidence = result->confidence * 0.5f + 0.5f;
    }
    
    return ret;
}
