/**
 * @file uft_format_autodetect.c
 * @brief Score-based Format Auto-Detection Implementation
 * 
 * P1-008: Format Auto-Detection Engine
 */

#include "uft/uft_format_autodetect.h"
#include "uft/uft_safe_io.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Magic Byte Database
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uint8_t MAGIC_SCP[] = {0x53, 0x43, 0x50};  /* "SCP" */
static const uint8_t MAGIC_HFE[] = {0x48, 0x58, 0x43, 0x50, 0x49, 0x43, 0x46, 0x45};  /* "HXCPICFE" */
static const uint8_t MAGIC_IPF[] = {0x43, 0x41, 0x50, 0x53};  /* "CAPS" */
static const uint8_t MAGIC_TD0[] = {0x54, 0x44};  /* "TD" */
static const uint8_t MAGIC_TD0_ADV[] = {0x74, 0x64};  /* "td" (advanced) */
static const uint8_t MAGIC_IMD[] = {0x49, 0x4D, 0x44, 0x20};  /* "IMD " */
static const uint8_t MAGIC_WOZ1[] = {0x57, 0x4F, 0x5A, 0x31};  /* "WOZ1" */
static const uint8_t MAGIC_WOZ2[] = {0x57, 0x4F, 0x5A, 0x32};  /* "WOZ2" */
static const uint8_t MAGIC_A2R[] = {0x41, 0x32, 0x52, 0x32};  /* "A2R2" */
static const uint8_t MAGIC_STX[] = {0x52, 0x53, 0x59, 0x00};  /* Pasti STX */
static const uint8_t MAGIC_DMS[] = {0x44, 0x4D, 0x53, 0x21};  /* "DMS!" */
static const uint8_t MAGIC_ADZ[] = {0x1F, 0x8B};  /* gzip */
static const uint8_t MAGIC_FDI[] = {0x46, 0x6F, 0x72, 0x6D};  /* "Form" */
static const uint8_t MAGIC_CQM[] = {0x43, 0x51};  /* "CQ" */
static const uint8_t MAGIC_DSK_EDSK[] = {0x45, 0x58, 0x54, 0x45};  /* "EXTE" (Extended) */
static const uint8_t MAGIC_DSK_STD[] = {0x4D, 0x56, 0x20, 0x2D};  /* "MV -" */
static const uint8_t MAGIC_D88[] = {0x00};  /* Check by size/structure */
static const uint8_t MAGIC_NIB[] = {0xD5, 0xAA, 0x96};  /* Apple sync */
static const uint8_t MAGIC_DC42[] = {0x00, 0x00, 0x01, 0x00};  /* DiskCopy 4.2 */

static const uft_magic_entry_t magic_table[] = {
    /* Flux formats (high priority) */
    {UFT_FORMAT_SCP, MAGIC_SCP, 3, 0, 50, "SuperCard Pro flux image"},
    {UFT_FORMAT_HFE, MAGIC_HFE, 8, 0, 50, "HxC Floppy Emulator image"},
    {UFT_FORMAT_IPF, MAGIC_IPF, 4, 0, 50, "Interchangeable Preservation Format"},
    {UFT_FORMAT_WOZ, MAGIC_WOZ1, 4, 0, 50, "WOZ 1.0 Apple II flux"},
    {UFT_FORMAT_WOZ, MAGIC_WOZ2, 4, 0, 50, "WOZ 2.0 Apple II flux"},
    {UFT_FORMAT_A2R, MAGIC_A2R, 4, 0, 50, "Applesauce A2R flux"},
    
    /* Archive/compressed formats */
    {UFT_FORMAT_TD0, MAGIC_TD0, 2, 0, 45, "Teledisk image"},
    {UFT_FORMAT_TD0, MAGIC_TD0_ADV, 2, 0, 45, "Teledisk advanced compression"},
    {UFT_FORMAT_IMD, MAGIC_IMD, 4, 0, 45, "ImageDisk format"},
    {UFT_FORMAT_STX, MAGIC_STX, 4, 0, 40, "Pasti/STX Atari format"},
    {UFT_FORMAT_CQM, MAGIC_CQM, 2, 0, 40, "CopyQM image"},
    {UFT_FORMAT_FDI, MAGIC_FDI, 4, 0, 40, "Formatted Disk Image"},
    
    /* Compressed Amiga */
    {UFT_FORMAT_MSA, MAGIC_ADZ, 2, 0, 30, "ADZ (gzip compressed ADF)"},
    
    /* Amstrad/Spectrum */
    {UFT_FORMAT_EDSK, MAGIC_DSK_EDSK, 4, 0, 45, "Extended DSK (Amstrad)"},
    {UFT_FORMAT_DSK_CPC, MAGIC_DSK_STD, 4, 0, 40, "Standard DSK (Amstrad)"},
    
    /* Apple formats */
    {UFT_FORMAT_NIB, MAGIC_NIB, 3, 0, 35, "Apple II Nibble format"},
    {UFT_FORMAT_DC42, MAGIC_DC42, 4, 0, 35, "DiskCopy 4.2 (Mac)"},
    
    {UFT_FORMAT_UNKNOWN, NULL, 0, 0, 0, NULL}  /* Terminator */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Size Table (for sector images)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_format_t format;
    size_t size;
    int score;
    const char *desc;
} size_entry_t;

static const size_entry_t size_table[] = {
    /* Amiga */
    {UFT_FORMAT_ADF, 901120, 40, "Amiga DD (880KB)"},
    {UFT_FORMAT_ADF, 1802240, 40, "Amiga HD (1760KB)"},
    
    /* C64/C128 */
    {UFT_FORMAT_D64, 174848, 45, "D64 (35 tracks, no errors)"},
    {UFT_FORMAT_D64, 175531, 45, "D64 (35 tracks + errors)"},
    {UFT_FORMAT_D64, 196608, 40, "D64 (40 tracks)"},
    {UFT_FORMAT_D71, 349696, 45, "D71 (70 tracks)"},
    {UFT_FORMAT_D81, 819200, 45, "D81 (80 tracks)"},
    {UFT_FORMAT_G64, 0, 0, "G64 (variable)"},  /* Checked by magic */
    
    /* Atari 8-bit */
    {UFT_FORMAT_ATR, 92176, 40, "ATR Single Density"},
    {UFT_FORMAT_ATR, 184336, 40, "ATR Enhanced Density"},
    {UFT_FORMAT_ATR, 183952, 40, "ATR Double Density"},
    {UFT_FORMAT_XFD, 92160, 40, "XFD Single Density"},
    {UFT_FORMAT_XFD, 133120, 40, "XFD Medium Density"},
    
    /* Atari ST */
    {UFT_FORMAT_ST, 368640, 40, "ST Single-sided (360KB)"},
    {UFT_FORMAT_ST, 737280, 40, "ST Double-sided (720KB)"},
    
    /* PC */
    {UFT_FORMAT_IMG, 163840, 35, "160KB PC"},
    {UFT_FORMAT_IMG, 184320, 35, "180KB PC"},
    {UFT_FORMAT_IMG, 327680, 35, "320KB PC"},
    {UFT_FORMAT_IMG, 368640, 35, "360KB PC"},
    {UFT_FORMAT_IMG, 737280, 35, "720KB PC"},
    {UFT_FORMAT_IMG, 1228800, 35, "1.2MB PC"},
    {UFT_FORMAT_IMG, 1474560, 35, "1.44MB PC"},
    {UFT_FORMAT_IMG, 2949120, 30, "2.88MB PC"},
    
    /* BBC Micro */
    {UFT_FORMAT_SSD, 102400, 40, "SSD (40T SS)"},
    {UFT_FORMAT_SSD, 204800, 40, "SSD (80T SS)"},
    {UFT_FORMAT_DSD, 204800, 40, "DSD (40T DS)"},
    {UFT_FORMAT_DSD, 409600, 40, "DSD (80T DS)"},
    
    /* Apple */
    {UFT_FORMAT_DO, 143360, 40, "Apple DOS Order (140KB)"},
    {UFT_FORMAT_PO, 143360, 40, "Apple ProDOS Order (140KB)"},
    {UFT_FORMAT_2MG, 0, 0, "2IMG (variable)"},
    
    /* Spectrum */
    {UFT_FORMAT_TRD, 655360, 40, "TR-DOS"},
    {UFT_FORMAT_SCL, 0, 0, "SCL (variable)"},
    
    {UFT_FORMAT_UNKNOWN, 0, 0, NULL}
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extension Table
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *ext;
    uft_format_t format;
    int score;
} ext_entry_t;

static const ext_entry_t ext_table[] = {
    /* Flux */
    {"scp", UFT_FORMAT_SCP, 30},
    {"hfe", UFT_FORMAT_HFE, 30},
    {"ipf", UFT_FORMAT_IPF, 30},
    {"raw", UFT_FORMAT_UFT_KF_RAW, 20},
    {"woz", UFT_FORMAT_WOZ, 30},
    {"a2r", UFT_FORMAT_A2R, 30},
    
    /* Sector images */
    {"adf", UFT_FORMAT_ADF, 30},
    {"adz", UFT_FORMAT_MSA, 25},  /* Actually compressed ADF */
    {"dms", UFT_FORMAT_MSA, 25},
    {"d64", UFT_FORMAT_D64, 35},
    {"g64", UFT_FORMAT_G64, 30},
    {"d71", UFT_FORMAT_D71, 35},
    {"d81", UFT_FORMAT_D81, 35},
    {"atr", UFT_FORMAT_ATR, 30},
    {"xfd", UFT_FORMAT_XFD, 30},
    {"st", UFT_FORMAT_ST, 25},
    {"msa", UFT_FORMAT_MSA, 30},
    {"stx", UFT_FORMAT_STX, 30},
    
    /* Archive/special */
    {"td0", UFT_FORMAT_TD0, 30},
    {"imd", UFT_FORMAT_IMD, 30},
    {"dmk", UFT_FORMAT_DMK, 30},
    {"fdi", UFT_FORMAT_FDI, 30},
    {"cqm", UFT_FORMAT_CQM, 30},
    
    /* Amstrad/Spectrum */
    {"dsk", UFT_FORMAT_DSK_CPC, 20},  /* Ambiguous */
    {"edsk", UFT_FORMAT_EDSK, 30},
    {"trd", UFT_FORMAT_TRD, 30},
    {"scl", UFT_FORMAT_SCL, 30},
    
    /* Apple */
    {"nib", UFT_FORMAT_NIB, 30},
    {"do", UFT_FORMAT_DO, 30},
    {"po", UFT_FORMAT_PO, 30},
    {"2mg", UFT_FORMAT_2MG, 30},
    {"dc42", UFT_FORMAT_DC42, 30},
    
    /* PC */
    {"img", UFT_FORMAT_IMG, 20},
    {"ima", UFT_FORMAT_IMG, 25},
    {"vfd", UFT_FORMAT_IMG, 25},
    {"flp", UFT_FORMAT_IMG, 25},
    
    /* BBC */
    {"ssd", UFT_FORMAT_SSD, 30},
    {"dsd", UFT_FORMAT_DSD, 30},
    
    /* Japanese */
    {"d88", UFT_FORMAT_D88, 30},
    {"hdm", UFT_FORMAT_HDM, 30},
    {"nfd", UFT_FORMAT_NFD, 30},
    
    {NULL, UFT_FORMAT_UNKNOWN, 0}
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static const char* get_extension(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : NULL;
}

static int strcasecmp_local(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static void add_candidate(uft_detect_result_t *r, uft_format_t fmt, 
                          int score, uint32_t heuristics,
                          const char *name, const char *desc) {
    if (r->candidate_count >= UFT_DETECT_MAX_CANDIDATES) return;
    
    /* Check if format already exists, update score if higher */
    for (int i = 0; i < r->candidate_count; i++) {
        if (r->candidates[i].format == fmt) {
            if (score > r->candidates[i].score) {
                r->candidates[i].score = score;
                r->candidates[i].heuristics_matched |= heuristics;
            }
            return;
        }
    }
    
    uft_detect_candidate_t *c = &r->candidates[r->candidate_count++];
    c->format = fmt;
    c->score = score > 100 ? 100 : score;
    c->heuristics_matched = heuristics;
    c->format_name = name;
    c->format_desc = desc;
}

static void add_warning(uft_detect_result_t *r, int severity, const char *fmt, ...) {
    if (r->warning_count >= UFT_DETECT_MAX_WARNINGS) return;
    
    uft_detect_warning_t *w = &r->warnings[r->warning_count++];
    w->severity = severity;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(w->text, UFT_DETECT_WARNING_LEN, fmt, args);
    va_end(args);
}

static void sort_candidates(uft_detect_result_t *r) {
    /* Simple bubble sort by score (descending) */
    for (int i = 0; i < r->candidate_count - 1; i++) {
        for (int j = 0; j < r->candidate_count - i - 1; j++) {
            if (r->candidates[j].score < r->candidates[j+1].score) {
                uft_detect_candidate_t tmp = r->candidates[j];
                r->candidates[j] = r->candidates[j+1];
                r->candidates[j+1] = tmp;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Heuristics
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void detect_by_magic(const uint8_t *data, size_t size, uft_detect_result_t *r) {
    for (const uft_magic_entry_t *m = magic_table; m->magic; m++) {
        if (m->offset + m->magic_len > size) continue;
        
        if (memcmp(data + m->offset, m->magic, m->magic_len) == 0) {
            add_candidate(r, m->format, m->score_boost, 
                         UFT_HEURISTIC_MAGIC_BYTES, 
                         m->description, m->description);
        }
    }
}

static void detect_by_size(size_t size, uft_detect_result_t *r) {
    for (const size_entry_t *s = size_table; s->desc; s++) {
        if (s->size == size && s->size > 0) {
            add_candidate(r, s->format, s->score,
                         UFT_HEURISTIC_FILE_SIZE,
                         s->desc, s->desc);
        }
    }
}

static void detect_by_extension(const char *ext, uft_detect_result_t *r) {
    if (!ext) return;
    
    for (const ext_entry_t *e = ext_table; e->ext; e++) {
        if (strcasecmp_local(ext, e->ext) == 0) {
            add_candidate(r, e->format, e->score,
                         UFT_HEURISTIC_EXTENSION,
                         NULL, NULL);
        }
    }
}

static void detect_boot_sector(const uint8_t *data, size_t size, uft_detect_result_t *r) {
    if (size < 512) return;
    
    /* Check for DOS boot sector (0x55 0xAA at offset 510) */
    if (data[510] == 0x55 && data[511] == 0xAA) {
        /* Check BPB (BIOS Parameter Block) */
        uint16_t bytes_per_sector = data[11] | (data[12] << 8);
        uint8_t sectors_per_cluster = data[13];
        
        if (bytes_per_sector == 512 && sectors_per_cluster > 0 && sectors_per_cluster <= 8) {
            add_candidate(r, UFT_FORMAT_IMG, 25,
                         UFT_HEURISTIC_BOOT_SECTOR | UFT_HEURISTIC_FILESYSTEM,
                         "DOS/FAT boot sector", "FAT12/16 filesystem detected");
        }
    }
    
    /* Check for Amiga boot block */
    if (size >= 1024) {
        if (memcmp(data, "DOS", 3) == 0) {
            uint8_t fs_type = data[3];
            const char *fs_name = "Amiga OFS";
            if (fs_type & 0x01) fs_name = "Amiga FFS";
            if (fs_type & 0x02) fs_name = "Amiga Int. Mode";
            if (fs_type & 0x04) fs_name = "Amiga Dir. Cache";
            
            add_candidate(r, UFT_FORMAT_ADF, 35,
                         UFT_HEURISTIC_BOOT_SECTOR | UFT_HEURISTIC_FILESYSTEM,
                         fs_name, "Amiga filesystem detected");
        }
    }
    
    /* Check for Atari ST boot sector */
    if (size >= 512 && data[0] == 0x60) {  /* BRA.S instruction */
        uint16_t bps = data[11] | (data[12] << 8);
        if (bps == 512) {
            add_candidate(r, UFT_FORMAT_ST, 20,
                         UFT_HEURISTIC_BOOT_SECTOR,
                         "Atari ST boot sector", "Possible Atari ST disk");
        }
    }
    
    /* Check for C64 D64 (BAM at track 18) */
    /* D64 doesn't have traditional boot sector, check BAM signature */
    if (size >= 174848) {
        /* Track 18, sector 0 is the BAM - offset depends on track layout */
        size_t bam_offset = 0x16500;  /* Approximate */
        if (bam_offset + 256 <= size) {
            /* Check for typical BAM values */
            if (data[bam_offset] == 0x12) {  /* Track 18 */
                add_candidate(r, UFT_FORMAT_D64, 30,
                             UFT_HEURISTIC_FILESYSTEM,
                             "C64 D64 with BAM", "Commodore DOS BAM detected");
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_detect_result_init(uft_detect_result_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(uft_detect_result_t));
    r->best_format = UFT_FORMAT_UNKNOWN;
    r->detected_encoding = UFT_ENC_UNKNOWN;
}

void uft_detect_result_free(uft_detect_result_t *r) {
    (void)r;  /* Currently no dynamic allocation */
}

uft_error_t uft_detect_format_buffer(const uint8_t *data, size_t size,
                                      const uft_detect_options_t *options,
                                      uft_detect_result_t *result) {
    if (!data || !result) return UFT_ERR_INVALID_PARAM;
    
    double start_time = get_time_ms();
    uft_detect_result_init(result);
    result->file_size = size;
    
    uft_detect_options_t opts = UFT_DETECT_OPTIONS_DEFAULT;
    if (options) opts = *options;
    
    result->heuristics_used = opts.heuristics;
    
    /* Run heuristics */
    if (opts.heuristics & UFT_HEURISTIC_MAGIC_BYTES) {
        detect_by_magic(data, size, result);
    }
    
    if (opts.heuristics & UFT_HEURISTIC_FILE_SIZE) {
        detect_by_size(size, result);
    }
    
    if (opts.heuristics & UFT_HEURISTIC_EXTENSION) {
        detect_by_extension(opts.hint_extension, result);
    }
    
    if (opts.heuristics & UFT_HEURISTIC_BOOT_SECTOR) {
        detect_boot_sector(data, size, result);
    }
    
    /* Sort by score */
    sort_candidates(result);
    
    /* Set best match */
    if (result->candidate_count > 0) {
        result->best_format = result->candidates[0].format;
        result->best_score = result->candidates[0].score;
        result->best_name = result->candidates[0].format_name;
    }
    
    /* Add warning if low confidence */
    if (result->best_score < UFT_DETECT_SCORE_MEDIUM) {
        add_warning(result, 1, "Low confidence detection (%d%%), manual verification recommended",
                   result->best_score);
    }
    
    /* Add warning if multiple high-scoring candidates */
    if (result->candidate_count >= 2 && 
        result->candidates[1].score >= UFT_DETECT_SCORE_MEDIUM) {
        add_warning(result, 0, "Multiple possible formats: %s, %s",
                   result->candidates[0].format_name ? result->candidates[0].format_name : "Unknown",
                   result->candidates[1].format_name ? result->candidates[1].format_name : "Unknown");
    }
    
    result->detection_time_ms = get_time_ms() - start_time;
    
    return UFT_OK;
}

uft_error_t uft_detect_format_file(const char *path,
                                    const uft_detect_options_t *options,
                                    uft_detect_result_t *result) {
    if (!path || !result) return UFT_ERR_INVALID_PARAM;
    
    /* Read file header (first 8KB should be enough for most formats) */
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Read header */
    size_t read_size = file_size > 8192 ? 8192 : file_size;
    uint8_t *buffer = malloc(read_size);
    if (!buffer) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    size_t read = fread(buffer, 1, read_size, f);
    fclose(f);
    
    if (read != read_size) {
        free(buffer);
        return UFT_ERR_IO;
    }
    
    /* Create options with extension hint */
    uft_detect_options_t opts = UFT_DETECT_OPTIONS_DEFAULT;
    if (options) opts = *options;
    opts.hint_extension = get_extension(path);
    
    /* Detect */
    uft_error_t err = uft_detect_format_buffer(buffer, file_size, &opts, result);
    
    free(buffer);
    return err;
}

uft_error_t uft_detect_format_quick(const uint8_t *header, size_t header_size,
                                     size_t file_size, const char *extension,
                                     uft_detect_result_t *result) {
    uft_detect_options_t opts = UFT_DETECT_OPTIONS_DEFAULT;
    opts.hint_extension = extension;
    opts.heuristics = UFT_HEURISTIC_MAGIC_BYTES | UFT_HEURISTIC_EXTENSION | 
                      UFT_HEURISTIC_FILE_SIZE;
    (void)opts; /* Quick path: bypasses full options pipeline */
    
    uft_detect_result_init(result);
    result->file_size = file_size;
    
    detect_by_magic(header, header_size, result);
    detect_by_size(file_size, result);
    detect_by_extension(extension, result);
    
    sort_candidates(result);
    
    if (result->candidate_count > 0) {
        result->best_format = result->candidates[0].format;
        result->best_score = result->candidates[0].score;
        result->best_name = result->candidates[0].format_name;
    }
    
    return UFT_OK;
}

const char* uft_detect_confidence_str(int score) {
    if (score >= UFT_DETECT_SCORE_HIGH) return "High";
    if (score >= UFT_DETECT_SCORE_MEDIUM) return "Medium";
    if (score >= UFT_DETECT_SCORE_LOW) return "Low";
    if (score >= UFT_DETECT_SCORE_UNCERTAIN) return "Uncertain";
    return "Unknown";
}

const uft_magic_entry_t* uft_get_magic_entries(size_t *count) {
    if (count) {
        size_t n = 0;
        for (const uft_magic_entry_t *m = magic_table; m->magic; m++) n++;
        *count = n;
    }
    return magic_table;
}

bool uft_format_is_flux(uft_format_t format) {
    switch (format) {
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_UFT_KF_STREAM:
        case UFT_FORMAT_UFT_KF_RAW:
        case UFT_FORMAT_HFE:
        case UFT_FORMAT_IPF:
        case UFT_FORMAT_CT_RAW:
        case UFT_FORMAT_A2R:
        case UFT_FORMAT_WOZ:
        case UFT_FORMAT_G64:
        case UFT_FORMAT_G71:
            return true;
        default:
            return false;
    }
}

bool uft_format_is_sector(uft_format_t format) {
    switch (format) {
        case UFT_FORMAT_ADF:
        case UFT_FORMAT_D64:
        case UFT_FORMAT_D71:
        case UFT_FORMAT_D81:
        case UFT_FORMAT_ST:
        case UFT_FORMAT_IMG:
        case UFT_FORMAT_ATR:
        case UFT_FORMAT_XFD:
        case UFT_FORMAT_SSD:
        case UFT_FORMAT_DSD:
        case UFT_FORMAT_DO:
        case UFT_FORMAT_PO:
            return true;
        default:
            return false;
    }
}

size_t uft_format_expected_size(uft_format_t format) {
    for (const size_entry_t *s = size_table; s->desc; s++) {
        if (s->format == format && s->size > 0) {
            return s->size;
        }
    }
    return 0;  /* Variable or unknown */
}
