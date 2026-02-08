/**
 * @file uft_format_registry.c
 * @brief Unified Format Registry Implementation
 * 
 * Contains all format profiles based on FluxEngine's supported formats.
 */

#include "uft_format_registry.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * MAGIC BYTES FOR FORMAT DETECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Amiga bootblock magic */
static const uint8_t MAGIC_AMIGA_DOS[] = { 'D', 'O', 'S', 0 };
static const uint8_t MAGIC_AMIGA_FFS[] = { 'D', 'O', 'S', 1 };

/* Apple */
static const uint8_t MAGIC_PRODOS[] = { 0x01, 0x38, 0xB0, 0x03 };

/* Atari ST */
static const uint8_t MAGIC_ATARI_ST[] = { 0x60, 0x1E }; /* BRA.S */

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT PROFILES TABLE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_format_profile_t g_format_profiles[] = {
    /* ═══════════════════════════════════════════════════════════════════════════
     * IBM PC FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_IBM_PC,
        .name = "ibm",
        .description = "IBM PC (generic)",
        .category = UFT_CAT_IBM_PC,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 18, 512, 300, 500, false, 1 },
        .image_ext = ".img",
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 2.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "fatfs",
    },
    {
        .id = UFT_FMT_IBM_360,
        .name = "ibm360",
        .description = "IBM PC 360KB 5.25\" DSDD",
        .category = UFT_CAT_IBM_PC,
        .encoding = UFT_ENC_MFM,
        .geometry = { 40, 2, 9, 512, 300, 250, false, 1 },
        .image_ext = ".img",
        .file_sizes = { 368640, 0 },
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "fatfs",
    },
    {
        .id = UFT_FMT_IBM_720,
        .name = "ibm720",
        .description = "IBM PC 720KB 3.5\" DSDD",
        .category = UFT_CAT_IBM_PC,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 9, 512, 300, 250, false, 1 },
        .image_ext = ".img",
        .file_sizes = { 737280, 0 },
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "fatfs",
    },
    {
        .id = UFT_FMT_IBM_1440,
        .name = "ibm1440",
        .description = "IBM PC 1.44MB 3.5\" DSHD",
        .category = UFT_CAT_IBM_PC,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 18, 512, 300, 500, false, 1 },
        .image_ext = ".img",
        .file_sizes = { 1474560, 0 },
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 2.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "fatfs",
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * COMMODORE FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_C64_1541,
        .name = "commodore",
        .description = "Commodore 1541 (170KB)",
        .category = UFT_CAT_COMMODORE,
        .encoding = UFT_ENC_GCR_C64,
        .geometry = { 35, 1, 0, 256, 300, 0, true, 0 },
        .image_ext = ".d64",
        .file_sizes = { 174848, 175531, 196608, 197376, 0 },
        .sync_pattern = 0x52AAAAAA,  /* GCR sync */
        .sync_bits = 40,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "cbmfs",
    },
    {
        .id = UFT_FMT_C64_1571,
        .name = "commodore1571",
        .description = "Commodore 1571 (340KB)",
        .category = UFT_CAT_COMMODORE,
        .encoding = UFT_ENC_GCR_C64,
        .geometry = { 35, 2, 0, 256, 300, 0, true, 0 },
        .image_ext = ".d71",
        .file_sizes = { 349696, 0 },
        .sync_pattern = 0x52AAAAAA,
        .sync_bits = 40,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "cbmfs",
    },
    {
        .id = UFT_FMT_C64_1581,
        .name = "commodore1581",
        .description = "Commodore 1581 (800KB)",
        .category = UFT_CAT_COMMODORE,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 10, 512, 300, 250, false, 1 },
        .image_ext = ".d81",
        .file_sizes = { 819200, 0 },
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "cbmfs",
    },
    {
        .id = UFT_FMT_AMIGA_DD,
        .name = "amiga",
        .description = "Amiga DD (880KB)",
        .category = UFT_CAT_COMMODORE,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 11, 512, 300, 250, false, 0 },
        .image_ext = ".adf",
        .magic_bytes = MAGIC_AMIGA_DOS,
        .magic_len = 4,
        .magic_offset = 0,
        .file_sizes = { 901120, 0 },
        .sync_pattern = 0x44894489,
        .sync_bits = 32,
        .clock_period_us = 2.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "amigaffs",
    },
    {
        .id = UFT_FMT_AMIGA_HD,
        .name = "amigahd",
        .description = "Amiga HD (1.76MB)",
        .category = UFT_CAT_COMMODORE,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 22, 512, 300, 500, false, 0 },
        .image_ext = ".adf",
        .file_sizes = { 1802240, 0 },
        .sync_pattern = 0x44894489,
        .sync_bits = 32,
        .clock_period_us = 1.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "amigaffs",
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * APPLE FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_APPLE2_DOS33,
        .name = "apple2",
        .description = "Apple II DOS 3.3 (140KB)",
        .category = UFT_CAT_APPLE,
        .encoding = UFT_ENC_GCR_APPLE,
        .geometry = { 35, 1, 16, 256, 300, 0, false, 0 },
        .image_ext = ".dsk",
        .file_sizes = { 143360, 0 },
        .sync_pattern = 0xD5AA96,
        .sync_bits = 24,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "appledos",
    },
    {
        .id = UFT_FMT_APPLE2_PRODOS,
        .name = "apple2prodos",
        .description = "Apple II ProDOS",
        .category = UFT_CAT_APPLE,
        .encoding = UFT_ENC_GCR_APPLE,
        .geometry = { 35, 1, 16, 256, 300, 0, false, 0 },
        .image_ext = ".po",
        .magic_bytes = MAGIC_PRODOS,
        .magic_len = 4,
        .magic_offset = 0,
        .file_sizes = { 143360, 0 },
        .sync_pattern = 0xD5AA96,
        .sync_bits = 24,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "prodos",
    },
    {
        .id = UFT_FMT_MAC_400,
        .name = "mac400",
        .description = "Macintosh 400KB GCR",
        .category = UFT_CAT_APPLE,
        .encoding = UFT_ENC_GCR_APPLE,
        .geometry = { 80, 1, 0, 512, 0, 0, true, 0 }, /* Variable speed */
        .image_ext = ".dsk",
        .file_sizes = { 409600, 0 },
        .sync_pattern = 0xD5AA96,
        .sync_bits = 24,
        .clock_period_us = 2.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "machfs",
    },
    {
        .id = UFT_FMT_MAC_800,
        .name = "mac",
        .description = "Macintosh 800KB GCR",
        .category = UFT_CAT_APPLE,
        .encoding = UFT_ENC_GCR_APPLE,
        .geometry = { 80, 2, 0, 512, 0, 0, true, 0 }, /* Variable speed */
        .image_ext = ".dsk",
        .file_sizes = { 819200, 0 },
        .sync_pattern = 0xD5AA96,
        .sync_bits = 24,
        .clock_period_us = 2.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "machfs",
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * ATARI FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_ATARI_ST,
        .name = "atarist",
        .description = "Atari ST",
        .category = UFT_CAT_ATARI,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 9, 512, 300, 250, false, 1 },
        .image_ext = ".st",
        .magic_bytes = MAGIC_ATARI_ST,
        .magic_len = 2,
        .magic_offset = 0,
        .file_sizes = { 737280, 819200, 901120, 0 },
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "fatfs",
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * ACORN FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_ACORN_DFS,
        .name = "acorndfs",
        .description = "Acorn DFS (BBC Micro)",
        .category = UFT_CAT_ACORN,
        .encoding = UFT_ENC_FM,
        .geometry = { 40, 1, 10, 256, 300, 125, false, 0 },
        .image_ext = ".ssd",
        .file_sizes = { 102400, 204800, 0 },
        .sync_pattern = 0xFE,
        .sync_bits = 8,
        .clock_period_us = 8.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = false,
        .has_filesystem = true,
        .filesystem_name = "acorndfs",
    },
    {
        .id = UFT_FMT_ACORN_ADFS,
        .name = "acornadfs",
        .description = "Acorn ADFS",
        .category = UFT_CAT_ACORN,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 16, 256, 300, 250, false, 0 },
        .image_ext = ".adf",
        .file_sizes = { 655360, 819200, 0 },
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = false,
        .has_filesystem = false,
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * CP/M FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_CPM_GENERIC,
        .name = "cpm",
        .description = "CP/M (generic)",
        .category = UFT_CAT_CPM,
        .encoding = UFT_ENC_MFM,
        .geometry = { 77, 2, 26, 128, 300, 250, false, 1 },
        .image_ext = ".cpm",
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = false,
        .has_filesystem = true,
        .filesystem_name = "cpmfs",
    },
    {
        .id = UFT_FMT_CPM_AMPRO,
        .name = "ampro",
        .description = "Ampro Little Board CP/M",
        .category = UFT_CAT_CPM,
        .encoding = UFT_ENC_MFM,
        .geometry = { 40, 2, 10, 512, 300, 250, false, 1 },
        .image_ext = ".img",
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = false,
        .has_filesystem = true,
        .filesystem_name = "cpmfs",
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * JAPANESE FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_PC98,
        .name = "n88basic",
        .description = "NEC PC-98 / PC-88",
        .category = UFT_CAT_JAPANESE,
        .encoding = UFT_ENC_MFM,
        .geometry = { 77, 2, 26, 256, 360, 500, false, 1 },
        .image_ext = ".d88",
        .sync_pattern = 0x4489,
        .sync_bits = 16,
        .clock_period_us = 2.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = false,
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * WORD PROCESSOR FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_BROTHER_120,
        .name = "brother",
        .description = "Brother 120KB Word Processor",
        .category = UFT_CAT_WORD_PROC,
        .encoding = UFT_ENC_GCR_BROTHER,
        .geometry = { 39, 1, 12, 256, 300, 0, false, 0 },
        .image_ext = ".img",
        .file_sizes = { 122880, 0 },
        .clock_period_us = 4.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "brother120fs",
    },
    {
        .id = UFT_FMT_BROTHER_240,
        .name = "brother240",
        .description = "Brother 240KB Word Processor",
        .category = UFT_CAT_WORD_PROC,
        .encoding = UFT_ENC_GCR_BROTHER,
        .geometry = { 78, 1, 12, 256, 300, 0, false, 0 },
        .image_ext = ".img",
        .file_sizes = { 245760, 0 },
        .clock_period_us = 4.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "fatfs",
    },
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * OTHER FORMATS
     * ═══════════════════════════════════════════════════════════════════════════ */
    {
        .id = UFT_FMT_VICTOR_9000,
        .name = "victor9k",
        .description = "Victor 9000 / Sirius One",
        .category = UFT_CAT_OTHER,
        .encoding = UFT_ENC_GCR_VICTOR,
        .geometry = { 80, 2, 0, 512, 0, 0, true, 0 }, /* Variable */
        .image_ext = ".img",
        .file_sizes = { 1224736, 0 },
        .clock_period_us = 2.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = false,
    },
    {
        .id = UFT_FMT_MICROPOLIS,
        .name = "micropolis",
        .description = "Micropolis MetaFloppy",
        .category = UFT_CAT_OTHER,
        .encoding = UFT_ENC_MFM,
        .geometry = { 77, 2, 16, 256, 300, 250, false, 0 },
        .image_ext = ".img",
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = false,
    },
    {
        .id = UFT_FMT_NORTHSTAR,
        .name = "northstar",
        .description = "Northstar Hard Sector",
        .category = UFT_CAT_OTHER,
        .encoding = UFT_ENC_FM,
        .geometry = { 35, 1, 10, 256, 300, 125, false, 0 },
        .image_ext = ".nsi",
        .clock_period_us = 8.0,
        .clock_tolerance = 0.3,
        .can_read = true,
        .can_write = true,
        .has_filesystem = false,
    },
    {
        .id = UFT_FMT_HP_LIF,
        .name = "hplif",
        .description = "HP LIF",
        .category = UFT_CAT_INDUSTRIAL,
        .encoding = UFT_ENC_MFM,
        .geometry = { 77, 2, 16, 256, 360, 500, false, 0 },
        .image_ext = ".lif",
        .clock_period_us = 2.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "lif",
    },
    {
        .id = UFT_FMT_ROLAND_D20,
        .name = "rolandd20",
        .description = "Roland D20 Synthesizer",
        .category = UFT_CAT_INDUSTRIAL,
        .encoding = UFT_ENC_MFM,
        .geometry = { 80, 2, 9, 512, 300, 250, false, 1 },
        .image_ext = ".img",
        .clock_period_us = 4.0,
        .clock_tolerance = 0.2,
        .can_read = true,
        .can_write = true,
        .has_filesystem = true,
        .filesystem_name = "roland",
    },
    
    /* Terminator */
    { .id = UFT_FMT_UNKNOWN }
};

static bool g_registry_initialized = false;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_format_registry_init(void)
{
    g_registry_initialized = true;
}

const uft_format_profile_t* uft_format_get_profile(uft_format_id_t id)
{
    for (const uft_format_profile_t *p = g_format_profiles; p->id != UFT_FMT_UNKNOWN; p++) {
        if (p->id == id) {
            return p;
        }
    }
    return NULL;
}

const uft_format_profile_t* uft_format_get_by_name(const char *name)
{
    if (!name) return NULL;
    
    for (const uft_format_profile_t *p = g_format_profiles; p->id != UFT_FMT_UNKNOWN; p++) {
        if (p->name && strcmp(p->name, name) == 0) {
            return p;
        }
    }
    return NULL;
}

int uft_format_get_by_category(uft_format_category_t category,
                                uft_format_id_t *out_ids, int max_count)
{
    int count = 0;
    for (const uft_format_profile_t *p = g_format_profiles; 
         p->id != UFT_FMT_UNKNOWN && count < max_count; p++) {
        if (p->category == category) {
            out_ids[count++] = p->id;
        }
    }
    return count;
}

int uft_format_detect_file(const char *filename, uft_detection_results_t *results)
{
    if (!filename || !results) return 0;
    
    memset(results, 0, sizeof(*results));
    
    /* Open file and get size */
    FILE *f = fopen(filename, "rb");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) return -1;
    
    /* Read header for magic detection */
    uint8_t header[512];
    size_t header_len = fread(header, 1, sizeof(header), f);
    fclose(f);
    
    int count = 0;
    
    /* Check each format */
    for (const uft_format_profile_t *p = g_format_profiles; 
         p->id != UFT_FMT_UNKNOWN && count < 10; p++) {
        int score = 0;
        const char *reason = NULL;
        
        /* Check file size match */
        for (size_t i = 0; p->file_sizes[i] != 0 && i < 4; i++) {
            if (file_size == p->file_sizes[i]) {
                score += 40;
                reason = "File size match";
                break;
            }
        }
        
        /* Check magic bytes */
        if (p->magic_bytes && p->magic_len > 0 && 
            p->magic_offset + p->magic_len <= header_len) {
            if (memcmp(header + p->magic_offset, p->magic_bytes, p->magic_len) == 0) {
                score += 50;
                reason = "Magic bytes match";
            }
        }
        
        /* Check file extension */
        if (p->image_ext) {
            const char *ext = strrchr(filename, '.');
            if (ext && strcasecmp(ext, p->image_ext) == 0) {
                score += 20;
                if (!reason) reason = "Extension match";
            }
        }
        
        /* Add to results if score > 0 */
        if (score > 0) {
            results->results[count].format = p->id;
            results->results[count].confidence = score > 100 ? 100 : score;
            results->results[count].reason = reason ? reason : "Heuristic";
            results->results[count].geometry = p->geometry;
            count++;
        }
    }
    
    results->count = count;
    
    /* Sort by confidence (simple bubble sort) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (results->results[j].confidence < results->results[j+1].confidence) {
                uft_detection_result_t tmp = results->results[j];
                results->results[j] = results->results[j+1];
                results->results[j+1] = tmp;
            }
        }
    }
    
    return count;
}

const char* uft_format_get_fluxengine_profile(uft_format_id_t id)
{
    const uft_format_profile_t *p = uft_format_get_profile(id);
    return p ? p->name : NULL;
}

int uft_format_list_all(uft_format_id_t *out_ids, int max_count)
{
    int count = 0;
    for (const uft_format_profile_t *p = g_format_profiles; 
         p->id != UFT_FMT_UNKNOWN && count < max_count; p++) {
        out_ids[count++] = p->id;
    }
    return count;
}

bool uft_format_can_read(uft_format_id_t id)
{
    const uft_format_profile_t *p = uft_format_get_profile(id);
    return p ? p->can_read : false;
}

bool uft_format_can_write(uft_format_id_t id)
{
    const uft_format_profile_t *p = uft_format_get_profile(id);
    return p ? p->can_write : false;
}

bool uft_format_has_filesystem(uft_format_id_t id)
{
    const uft_format_profile_t *p = uft_format_get_profile(id);
    return p ? p->has_filesystem : false;
}

/* Pattern detection implementations */
int uft_format_detect_flux(const uint8_t *flux_data, size_t flux_len,
                           uft_detection_results_t *results)
{
    if (!flux_data || flux_len == 0) return 0;
    if (results) {
        memset(results, 0, sizeof(*results));
    }
    if (!results) return 0;
    
    int count = 0;
    
    /* Scan for MFM sync marks (A1 A1 A1 = 0x4489 pattern) */
    int mfm_sync_count = 0;
    for (size_t i = 0; i + 2 < flux_len; i++) {
        if (flux_data[i] == 0xA1 && flux_data[i+1] == 0xA1 && flux_data[i+2] == 0xA1) {
            mfm_sync_count++;
            i += 2;
        }
    }
    
    /* Scan for FM sync marks (0xF57E pattern) */
    int fm_sync_count = 0;
    for (size_t i = 0; i + 1 < flux_len; i++) {
        if (flux_data[i] == 0xF5 && flux_data[i+1] == 0x7E) {
            fm_sync_count++;
            i += 1;
        }
    }
    
    /* Scan for GCR sync (0xFF 0xFF = C64 GCR sync) */
    int gcr_sync_count = 0;
    for (size_t i = 0; i + 4 < flux_len; i++) {
        if (flux_data[i] == 0xFF && flux_data[i+1] == 0xFF) {
            gcr_sync_count++;
            i += 1;
        }
    }
    
    /* Score based on sync pattern frequencies */
    if (mfm_sync_count > 5) {
        results->results[count].confidence = (mfm_sync_count > 15) ? 85 : 60;
        results->results[count].reason = "MFM sync patterns detected";
        count++;
    }
    if (fm_sync_count > 5 && count < 10) {
        results->results[count].confidence = (fm_sync_count > 10) ? 80 : 55;
        results->results[count].reason = "FM sync patterns detected";
        count++;
    }
    if (gcr_sync_count > 20 && count < 10) {
        results->results[count].confidence = 70;
        results->results[count].reason = "GCR sync patterns detected";
        count++;
    }
    
    results->count = count;
    return count;
}

int uft_format_detect_sectors(const uint8_t *sector_data, size_t sector_len,
                              uft_detection_results_t *results)
{
    if (!sector_data || sector_len == 0) return 0;
    if (results) {
        memset(results, 0, sizeof(*results));
    }
    if (!results) return 0;
    
    int count = 0;
    
    /* Detect format by sector data content and size */
    
    /* Amiga: check for DOS\0 or DOS\1 at boot block */
    if (sector_len >= 4 && sector_data[0] == 'D' && sector_data[1] == 'O' &&
        sector_data[2] == 'S' && (sector_data[3] <= 7)) {
        results->results[count].confidence = 90;
        results->results[count].reason = "AmigaDOS boot block signature";
        count++;
    }
    
    /* FAT: check for boot sector signature */
    if (sector_len >= 512 && sector_data[510] == 0x55 && sector_data[511] == 0xAA) {
        uint16_t bps = sector_data[0x0B] | ((uint16_t)sector_data[0x0C] << 8);
        if (bps == 512 || bps == 1024 || bps == 256) {
            results->results[count].confidence = 80;
            results->results[count].reason = "FAT boot sector detected";
            count++;
        }
    }
    
    /* C64: D64 has 683 sectors × 256 bytes = 174848 bytes */
    if (sector_len == 174848 || sector_len == 175531) {
        results->results[count].confidence = 85;
        results->results[count].reason = "D64 image size match";
        count++;
    }
    
    /* Atari ST: BRA.S instruction at offset 0 */
    if (sector_len >= 2 && sector_data[0] == 0x60 && sector_data[1] >= 0x1C) {
        results->results[count].confidence = 60;
        results->results[count].reason = "Atari ST boot sector (BRA.S)";
        count++;
    }
    
    results->count = count;
    return count;
}
