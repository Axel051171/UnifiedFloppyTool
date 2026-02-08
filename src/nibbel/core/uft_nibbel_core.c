/**
 * @file uft_nibbel_core.c
 * @brief NIBBEL Core Implementation
 * 
 * Implements the main API functions for context management,
 * configuration, and file operations.
 */

#include "uft/nibbel/uft_nibbel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ===========================================================================
// INTERNAL STRUCTURES
// ===========================================================================

/**
 * @brief Internal track data structure
 */
typedef struct {
    int             track_num;
    int             head;
    
    uint8_t*        raw_data;       // Raw GCR/nibble data
    size_t          raw_size;
    
    uint8_t*        sector_data;    // Decoded sector data
    int             sector_count;
    uft_nibbel_sector_status_t* sector_status;
    
    uft_nibbel_track_info_t info;
    
} nibbel_track_t;

/**
 * @brief Internal context structure
 */
struct uft_nibbel_ctx_s {
    // State
    bool            initialized;
    bool            file_open;
    bool            processed;
    
    // File info
    FILE*           file;
    char*           file_path;
    size_t          file_size;
    uint8_t*        file_data;      // Memory-mapped or loaded
    
    // Configuration
    uft_nibbel_config_t config;
    
    // Format detection
    uft_nibbel_format_t detected_format;
    uft_nibbel_encoding_t detected_encoding;
    int             format_confidence;
    
    // Track data
    int             num_tracks;
    int             num_heads;
    nibbel_track_t* tracks;
    
    // Statistics
    uft_nibbel_stats_t stats;
    
    // Error handling
    uft_nibbel_error_t last_error;
    char            error_detail[256];
};

// ===========================================================================
// VERSION
// ===========================================================================

const char* uft_nibbel_version(void) {
    return UFT_NIBBEL_VERSION_STRING;
}

uint32_t uft_nibbel_version_number(void) {
    return (UFT_NIBBEL_VERSION_MAJOR << 16) |
           (UFT_NIBBEL_VERSION_MINOR << 8) |
           UFT_NIBBEL_VERSION_PATCH;
}

// ===========================================================================
// CONTEXT MANAGEMENT
// ===========================================================================

uft_nibbel_ctx_t* uft_nibbel_create(void) {
    uft_nibbel_ctx_t* ctx = calloc(1, sizeof(uft_nibbel_ctx_t));
    if (!ctx) return NULL;
    
    ctx->initialized = true;
    uft_nibbel_config_defaults(&ctx->config);
    
    return ctx;
}

void uft_nibbel_destroy(uft_nibbel_ctx_t* ctx) {
    if (!ctx) return;
    
    // Close file if open
    uft_nibbel_close(ctx);
    
    // Free tracks
    if (ctx->tracks) {
        for (int i = 0; i < ctx->num_tracks * ctx->num_heads; i++) {
            free(ctx->tracks[i].raw_data);
            free(ctx->tracks[i].sector_data);
            free(ctx->tracks[i].sector_status);
        }
        free(ctx->tracks);
    }
    
    // Free file data
    free(ctx->file_data);
    free(ctx->file_path);
    
    // Free context
    free(ctx);
}

// ===========================================================================
// CONFIGURATION
// ===========================================================================

void uft_nibbel_config_defaults(uft_nibbel_config_t* cfg) {
    if (!cfg) return;
    
    memset(cfg, 0, sizeof(*cfg));
    
    // Track range
    cfg->start_track = 0;           // Auto
    cfg->end_track = 0;             // Auto
    
    // Error handling
    cfg->retries = 3;
    cfg->skip_errors = false;
    
    // Format
    cfg->input_format = UFT_NIB_FORMAT_AUTO;
    cfg->encoding = UFT_NIB_ENCODING_AUTO;
    
    // Verification
    cfg->verify_checksums = true;
    cfg->verify_output = true;
    
    // Recovery
    cfg->recovery_level = 1;
    cfg->attempt_correction = false;
    cfg->max_correction_bits = 2;
    
    // Expert
    cfg->bitcell_ns = 0;            // Auto
    cfg->pll_bandwidth = 0.1;
    cfg->read_half_tracks = false;
    
    // Output
    cfg->include_raw = false;
    
    // Callbacks
    cfg->progress_cb = NULL;
    cfg->progress_user = NULL;
    cfg->cancel_flag = NULL;
}

const char* uft_nibbel_config_validate(const uft_nibbel_config_t* cfg) {
    if (!cfg) return "Configuration is NULL";
    
    // Track range
    if (cfg->start_track < 0 || cfg->start_track > UFT_NIB_MAX_TRACKS) {
        return "start_track out of range (0-84)";
    }
    if (cfg->end_track < 0 || cfg->end_track > UFT_NIB_MAX_TRACKS) {
        return "end_track out of range (0-84)";
    }
    if (cfg->start_track > 0 && cfg->end_track > 0 && 
        cfg->end_track < cfg->start_track) {
        return "end_track must be >= start_track";
    }
    
    // Retries
    if (cfg->retries < 0 || cfg->retries > UFT_NIB_MAX_RETRIES) {
        return "retries out of range (0-10)";
    }
    
    // Recovery
    if (cfg->recovery_level < 0 || cfg->recovery_level > 3) {
        return "recovery_level out of range (0-3)";
    }
    if (cfg->max_correction_bits < 0 || cfg->max_correction_bits > UFT_NIB_MAX_CORRECTION) {
        return "max_correction_bits out of range (0-4)";
    }
    if (cfg->recovery_level > 0 && !cfg->attempt_correction && cfg->recovery_level >= 2) {
        return "recovery_level >= 2 requires attempt_correction enabled";
    }
    
    // Expert timing
    if (cfg->bitcell_ns != 0 && (cfg->bitcell_ns < 1000 || cfg->bitcell_ns > 5000)) {
        return "bitcell_ns out of range (0 or 1000-5000)";
    }
    if (cfg->pll_bandwidth < 0.01 || cfg->pll_bandwidth > 0.5) {
        return "pll_bandwidth out of range (0.01-0.5)";
    }
    
    // Conflicting options
    if (cfg->skip_errors && cfg->verify_output) {
        return "skip_errors is incompatible with verify_output";
    }
    
    return NULL;  // Valid
}

// ===========================================================================
// FILE OPERATIONS
// ===========================================================================

uft_nibbel_error_t uft_nibbel_open(
    uft_nibbel_ctx_t* ctx,
    const char* path,
    const uft_nibbel_config_t* cfg
) {
    if (!ctx) return UFT_NIB_ERROR_NULL_POINTER;
    if (!path) return UFT_NIB_ERROR_NULL_POINTER;
    
    // Close any existing file
    uft_nibbel_close(ctx);
    
    // Apply config
    if (cfg) {
        const char* err = uft_nibbel_config_validate(cfg);
        if (err) {
            snprintf(ctx->error_detail, sizeof(ctx->error_detail), 
                     "Config error: %s", err);
            ctx->last_error = UFT_NIB_ERROR_INVALID_ARG;
            return UFT_NIB_ERROR_INVALID_ARG;
        }
        ctx->config = *cfg;
    }
    
    // Open file
    ctx->file = fopen(path, "rb");
    if (!ctx->file) {
        snprintf(ctx->error_detail, sizeof(ctx->error_detail),
                 "Cannot open file: %s", path);
        ctx->last_error = UFT_NIB_ERROR_FILE_OPEN;
        return UFT_NIB_ERROR_FILE_OPEN;
    }
    
    // Get file size
    if (fseek(ctx->file, 0, SEEK_END) != 0) {
        fclose(ctx->file);
        ctx->file = NULL;
        ctx->last_error = UFT_NIB_ERROR_FILE_SEEK;
        return UFT_NIB_ERROR_FILE_SEEK;
    }
    
    long size = ftell(ctx->file);
    if (size < 0) {
        fclose(ctx->file);
        ctx->file = NULL;
        ctx->last_error = UFT_NIB_ERROR_FILE_SIZE;
        return UFT_NIB_ERROR_FILE_SIZE;
    }
    ctx->file_size = (size_t)size;
    
    if (fseek(ctx->file, 0, SEEK_SET) != 0) {
        fclose(ctx->file);
        ctx->file = NULL;
        ctx->last_error = UFT_NIB_ERROR_FILE_SEEK;
        return UFT_NIB_ERROR_FILE_SEEK;
    }
    
    // Store path
    ctx->file_path = strdup(path);
    if (!ctx->file_path) {
        fclose(ctx->file);
        ctx->file = NULL;
        ctx->last_error = UFT_NIB_ERROR_MEMORY;
        return UFT_NIB_ERROR_MEMORY;
    }
    
    // Load file into memory for faster access
    ctx->file_data = malloc(ctx->file_size);
    if (!ctx->file_data) {
        fclose(ctx->file);
        ctx->file = NULL;
        free(ctx->file_path);
        ctx->file_path = NULL;
        ctx->last_error = UFT_NIB_ERROR_MEMORY;
        return UFT_NIB_ERROR_MEMORY;
    }
    
    size_t read = fread(ctx->file_data, 1, ctx->file_size, ctx->file);
    if (read != ctx->file_size) {
        fclose(ctx->file);
        ctx->file = NULL;
        free(ctx->file_path);
        ctx->file_path = NULL;
        free(ctx->file_data);
        ctx->file_data = NULL;
        ctx->last_error = UFT_NIB_ERROR_FILE_READ;
        return UFT_NIB_ERROR_FILE_READ;
    }
    
    // Detect format
    uft_nibbel_error_t err = uft_nibbel_detect_format(
        path, 
        &ctx->detected_format,
        &ctx->format_confidence
    );
    if (err != UFT_NIB_OK && ctx->config.input_format == UFT_NIB_FORMAT_AUTO) {
        // Auto-detect failed and no format forced
        uft_nibbel_close(ctx);
        ctx->last_error = UFT_NIB_ERROR_FORMAT_UNKNOWN;
        return UFT_NIB_ERROR_FORMAT_UNKNOWN;
    }
    
    // Use forced format if specified
    if (ctx->config.input_format != UFT_NIB_FORMAT_AUTO) {
        ctx->detected_format = ctx->config.input_format;
    }
    
    ctx->file_open = true;
    ctx->processed = false;
    ctx->last_error = UFT_NIB_OK;
    
    return UFT_NIB_OK;
}

void uft_nibbel_close(uft_nibbel_ctx_t* ctx) {
    if (!ctx) return;
    
    if (ctx->file) {
        fclose(ctx->file);
        ctx->file = NULL;
    }
    
    ctx->file_open = false;
    ctx->processed = false;
}

bool uft_nibbel_is_open(const uft_nibbel_ctx_t* ctx) {
    return ctx && ctx->file_open;
}

// ===========================================================================
// ERROR HANDLING
// ===========================================================================

static const char* error_messages[] = {
    [0] = "OK",
    [-UFT_NIB_ERROR_NULL_POINTER] = "Null pointer",
    [-UFT_NIB_ERROR_INVALID_ARG] = "Invalid argument",
    [-UFT_NIB_ERROR_MEMORY] = "Memory allocation failed",
    [-UFT_NIB_ERROR_FILE_OPEN] = "Cannot open file",
    [-UFT_NIB_ERROR_FILE_READ] = "File read error",
    [-UFT_NIB_ERROR_FILE_WRITE] = "File write error",
    [-UFT_NIB_ERROR_FILE_SEEK] = "File seek error",
    [-UFT_NIB_ERROR_FILE_SIZE] = "Invalid file size",
    [-UFT_NIB_ERROR_FORMAT] = "Format error",
    [-UFT_NIB_ERROR_FORMAT_UNKNOWN] = "Unknown format",
    [-UFT_NIB_ERROR_FORMAT_UNSUPPORTED] = "Unsupported format",
    [-UFT_NIB_ERROR_FORMAT_CORRUPT] = "Corrupt file",
    [-UFT_NIB_ERROR_BAD_GCR] = "Bad GCR data",
    [-UFT_NIB_ERROR_SYNC_NOT_FOUND] = "Sync not found",
    [-UFT_NIB_ERROR_HEADER_NOT_FOUND] = "Header not found",
    [-UFT_NIB_ERROR_DATA_NOT_FOUND] = "Data not found",
    [-UFT_NIB_ERROR_CHECKSUM] = "Checksum error",
    [-UFT_NIB_ERROR_BOUNDS] = "Bounds error",
    [-UFT_NIB_ERROR_TRACK_RANGE] = "Track out of range",
    [-UFT_NIB_ERROR_SECTOR_RANGE] = "Sector out of range",
    [-UFT_NIB_ERROR_OVERFLOW] = "Integer overflow",
    [-UFT_NIB_ERROR_CANCELLED] = "Operation cancelled",
    [-UFT_NIB_ERROR_TIMEOUT] = "Operation timeout",
    [-UFT_NIB_ERROR_BUSY] = "Context busy",
    [-UFT_NIB_ERROR_NOT_OPEN] = "File not open",
    [-UFT_NIB_ERROR_INTERNAL] = "Internal error",
    [-UFT_NIB_ERROR_NOT_IMPLEMENTED] = "Not implemented",
};

const char* uft_nibbel_error_string(uft_nibbel_error_t error) {
    if (error >= 0) return "OK";
    int idx = -error;
    if (idx >= (int)(sizeof(error_messages)/sizeof(error_messages[0]))) {
        return "Unknown error";
    }
    return error_messages[idx] ? error_messages[idx] : "Unknown error";
}

uft_nibbel_error_t uft_nibbel_last_error(const uft_nibbel_ctx_t* ctx) {
    return ctx ? ctx->last_error : UFT_NIB_ERROR_NULL_POINTER;
}

const char* uft_nibbel_last_error_detail(const uft_nibbel_ctx_t* ctx) {
    if (!ctx) return "Context is NULL";
    if (ctx->last_error == UFT_NIB_OK) return NULL;
    if (ctx->error_detail[0]) return ctx->error_detail;
    return uft_nibbel_error_string(ctx->last_error);
}

// ===========================================================================
// FORMAT DETECTION
// ===========================================================================

const char* uft_nibbel_format_name(uft_nibbel_format_t format) {
    static const char* names[] = {
        [UFT_NIB_FORMAT_AUTO] = "Auto",
        [UFT_NIB_FORMAT_D64] = "D64 (C64 Sector Image)",
        [UFT_NIB_FORMAT_G64] = "G64 (C64 GCR Image)",
        [UFT_NIB_FORMAT_NBZ] = "NBZ (GCR tools Compressed)",
        [UFT_NIB_FORMAT_NIB] = "NIB (Apple II Nibble)",
        [UFT_NIB_FORMAT_DSK] = "DSK (Apple II Sector)",
        [UFT_NIB_FORMAT_PO] = "PO (Apple ProDOS Order)",
        [UFT_NIB_FORMAT_DO] = "DO (Apple DOS Order)",
        [UFT_NIB_FORMAT_WOZ] = "WOZ (Apple II Flux)",
        [UFT_NIB_FORMAT_A2R] = "A2R (Apple II Flux)",
        [UFT_NIB_FORMAT_SCP] = "SCP (SuperCard Pro Flux)",
        [UFT_NIB_FORMAT_KFX] = "KFX (KryoFlux Stream)",
        [UFT_NIB_FORMAT_GW] = "GW (Greaseweazle Raw)",
        [UFT_NIB_FORMAT_IPF] = "IPF (CAPS Interchange)",
        [UFT_NIB_FORMAT_RAW] = "RAW (Raw Bytes)",
    };
    
    if (format < 0 || format >= UFT_NIB_FORMAT_COUNT) {
        return "Unknown";
    }
    return names[format];
}

uft_nibbel_format_t uft_nibbel_format_from_extension(const char* extension) {
    if (!extension) return UFT_NIB_FORMAT_AUTO;
    
    // Skip leading dot
    if (*extension == '.') extension++;
    
    // Case-insensitive comparison
    if (strcasecmp(extension, "d64") == 0) return UFT_NIB_FORMAT_D64;
    if (strcasecmp(extension, "g64") == 0) return UFT_NIB_FORMAT_G64;
    if (strcasecmp(extension, "nbz") == 0) return UFT_NIB_FORMAT_NBZ;
    if (strcasecmp(extension, "nib") == 0) return UFT_NIB_FORMAT_NIB;
    if (strcasecmp(extension, "dsk") == 0) return UFT_NIB_FORMAT_DSK;
    if (strcasecmp(extension, "po") == 0) return UFT_NIB_FORMAT_PO;
    if (strcasecmp(extension, "do") == 0) return UFT_NIB_FORMAT_DO;
    if (strcasecmp(extension, "woz") == 0) return UFT_NIB_FORMAT_WOZ;
    if (strcasecmp(extension, "a2r") == 0) return UFT_NIB_FORMAT_A2R;
    if (strcasecmp(extension, "scp") == 0) return UFT_NIB_FORMAT_SCP;
    if (strcasecmp(extension, "raw") == 0) return UFT_NIB_FORMAT_KFX;
    if (strcasecmp(extension, "ipf") == 0) return UFT_NIB_FORMAT_IPF;
    
    return UFT_NIB_FORMAT_AUTO;
}

uft_nibbel_error_t uft_nibbel_detect_format(
    const char* path,
    uft_nibbel_format_t* format,
    int* confidence
) {
    if (!path || !format) return UFT_NIB_ERROR_NULL_POINTER;
    
    // Initialize
    *format = UFT_NIB_FORMAT_AUTO;
    if (confidence) *confidence = 0;
    
    // Get extension
    const char* ext = strrchr(path, '.');
    if (ext) {
        *format = uft_nibbel_format_from_extension(ext);
        if (*format != UFT_NIB_FORMAT_AUTO) {
            if (confidence) *confidence = 60;  // Extension match
        }
    }
    
    // Try to open and detect by content
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_NIB_ERROR_FILE_OPEN;
    
    // Get size
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    // Read header
    uint8_t header[512];
    size_t read = fread(header, 1, sizeof(header), f);
    fclose(f);
    
    if (read < 16) return UFT_NIB_ERROR_FILE_SIZE;
    
    // Detect by magic/size
    
    // G64: "GCR-1541" signature
    if (read >= 8 && memcmp(header, "GCR-1541", 8) == 0) {
        *format = UFT_NIB_FORMAT_G64;
        if (confidence) *confidence = 95;
        return UFT_NIB_OK;
    }
    
    // WOZ: "WOZ1" or "WOZ2" signature
    if (read >= 4 && (memcmp(header, "WOZ1", 4) == 0 || 
                      memcmp(header, "WOZ2", 4) == 0)) {
        *format = UFT_NIB_FORMAT_WOZ;
        if (confidence) *confidence = 95;
        return UFT_NIB_OK;
    }
    
    // SCP: "SCP" signature
    if (read >= 3 && memcmp(header, "SCP", 3) == 0) {
        *format = UFT_NIB_FORMAT_SCP;
        if (confidence) *confidence = 95;
        return UFT_NIB_OK;
    }
    
    // D64: Known sizes
    if (size == 174848 || size == 175531 ||   // 35 track
        size == 196608 || size == 197376) {    // 40 track
        *format = UFT_NIB_FORMAT_D64;
        if (confidence) *confidence = 80;
        return UFT_NIB_OK;
    }
    
    // NIB (Apple II): 232960 bytes (35 tracks × 6656)
    if (size == 232960) {
        *format = UFT_NIB_FORMAT_NIB;
        if (confidence) *confidence = 75;
        return UFT_NIB_OK;
    }
    
    // DSK (Apple II): 143360 bytes (35 × 16 × 256)
    if (size == 143360) {
        *format = UFT_NIB_FORMAT_DSK;
        if (confidence) *confidence = 70;
        return UFT_NIB_OK;
    }
    
    // If we got extension match, use that
    if (*format != UFT_NIB_FORMAT_AUTO) {
        return UFT_NIB_OK;
    }
    
    return UFT_NIB_ERROR_FORMAT_UNKNOWN;
}

// ===========================================================================
// STATISTICS
// ===========================================================================

uft_nibbel_error_t uft_nibbel_get_stats(
    const uft_nibbel_ctx_t* ctx,
    uft_nibbel_stats_t* stats
) {
    if (!ctx || !stats) return UFT_NIB_ERROR_NULL_POINTER;
    
    *stats = ctx->stats;
    strncpy(stats->format_name, uft_nibbel_format_name(ctx->detected_format),
            sizeof(stats->format_name) - 1);
    
    return UFT_NIB_OK;
}

// ===========================================================================
// UTILITY
// ===========================================================================

// External GCR tables function
extern uint32_t uft_gcr_tables_checksum(void);

uint32_t uft_nibbel_gcr_table_checksum(void) {
    return uft_gcr_tables_checksum();
}

static bool g_nibbel_initialized = false;

void uft_nibbel_init(void) {
    if (g_nibbel_initialized) return;
    
    // Initialize GCR tables (they're const, just verify)
    uint32_t checksum = uft_gcr_tables_checksum();
    (void)checksum;  // Could log or verify
    
    g_nibbel_initialized = true;
}

void uft_nibbel_shutdown(void) {
    g_nibbel_initialized = false;
}

// ===========================================================================
// STUB FUNCTIONS (to be implemented)
// ===========================================================================

/**
 * @brief Decode GCR data to sector bytes
 * @param gcr_data Raw GCR data
 * @param gcr_len Length of GCR data
 * @param sector_out Output sector buffer (256 bytes for C64)
 * @return 0 on success, -1 on error
 */
static int decode_gcr_sector(const uint8_t* gcr_data, size_t gcr_len, 
                             uint8_t* sector_out) {
    if (!gcr_data || !sector_out || gcr_len < 325) {
        return -1;
    }
    
    /* GCR decode table (4 bits per 5 GCR bits) */
    static const uint8_t gcr_decode[32] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 invalid */
        0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
        0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
        0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
    };
    
    /* Decode 325 GCR bytes to 256 sector bytes */
    /* Each 5 GCR bytes decode to 4 data bytes */
    size_t out_pos = 0;
    size_t in_pos = 0;
    
    while (out_pos < 256 && in_pos + 4 < gcr_len) {
        uint64_t gcr_bits = 0;
        
        /* Collect 5 bytes (40 bits) */
        for (int i = 0; i < 5 && in_pos + i < gcr_len; i++) {
            gcr_bits = (gcr_bits << 8) | gcr_data[in_pos + i];
        }
        in_pos += 5;
        
        /* Extract 8 5-bit GCR values and decode to 4 bytes */
        for (int i = 0; i < 4 && out_pos < 256; i++) {
            uint8_t hi = gcr_decode[(gcr_bits >> (35 - i * 10)) & 0x1F];
            uint8_t lo = gcr_decode[(gcr_bits >> (30 - i * 10)) & 0x1F];
            
            if (hi == 0xFF || lo == 0xFF) {
                return -1;  /* Invalid GCR */
            }
            
            sector_out[out_pos++] = (hi << 4) | lo;
        }
    }
    
    return (out_pos == 256) ? 0 : -1;
}

uft_nibbel_error_t uft_nibbel_process(uft_nibbel_ctx_t* ctx) {
    if (!ctx) return UFT_NIB_ERROR_NULL_POINTER;
    if (!ctx->file_open) return UFT_NIB_ERROR_NOT_OPEN;
    if (!ctx->file_data) return UFT_NIB_ERROR_FILE_READ;
    
    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    
    /* Process based on detected format */
    switch (ctx->detected_format) {
        case UFT_NIB_FORMAT_D64:
            /* D64: Already decoded sectors, just validate */
            ctx->num_tracks = 35;
            ctx->num_heads = 1;
            ctx->stats.total_tracks = 35;
            ctx->stats.good_tracks = 35;
            ctx->processed = true;
            ctx->last_error = UFT_NIB_OK;
            return UFT_NIB_OK;
            
        case UFT_NIB_FORMAT_G64:
            /* G64: GCR encoded, needs decode */
            if (ctx->file_size < 12) {
                ctx->last_error = UFT_NIB_ERROR_FILE_SIZE;
                return UFT_NIB_ERROR_FILE_SIZE;
            }
            ctx->num_tracks = ctx->file_data[9];  /* Track count from header */
            ctx->num_heads = 1;
            ctx->stats.total_tracks = ctx->num_tracks;
            
            /* Process each track */
            for (int t = 0; t < ctx->num_tracks && t < 84; t++) {
                uft_nibbel_error_t err = uft_nibbel_process_track(ctx, t, 0);
                if (err == UFT_NIB_OK) {
                    ctx->stats.good_tracks++;
                }
            }
            
            ctx->processed = true;
            ctx->last_error = UFT_NIB_OK;
            return UFT_NIB_OK;
            
        case UFT_NIB_FORMAT_NIB:
        case UFT_NIB_FORMAT_WOZ:
            /* Apple II formats - 35 tracks */
            ctx->num_tracks = 35;
            ctx->num_heads = 1;
            ctx->stats.total_tracks = 35;
            
            for (int t = 0; t < 35; t++) {
                uft_nibbel_error_t err = uft_nibbel_process_track(ctx, t, 0);
                if (err == UFT_NIB_OK) {
                    ctx->stats.good_tracks++;
                }
            }
            
            ctx->processed = true;
            ctx->last_error = UFT_NIB_OK;
            return UFT_NIB_OK;
            
        default:
            /* Unsupported format */
            ctx->last_error = UFT_NIB_ERROR_FORMAT;
            return UFT_NIB_ERROR_FORMAT;
    }
}

uft_nibbel_error_t uft_nibbel_process_track(
    uft_nibbel_ctx_t* ctx,
    int track,
    int head
) {
    if (!ctx) return UFT_NIB_ERROR_NULL_POINTER;
    if (!ctx->file_open) return UFT_NIB_ERROR_NOT_OPEN;
    if (!ctx->file_data) return UFT_NIB_ERROR_FILE_READ;
    
    (void)head;  /* Most nibble formats are single-sided */
    
    /* Get track data based on format */
    const uint8_t* track_data = NULL;
    size_t track_size = 0;
    
    switch (ctx->detected_format) {
        case UFT_NIB_FORMAT_D64: {
            /* D64: Calculate track offset */
            /* Track 1-17: 21 sectors, 18-24: 19 sectors, 25-30: 18 sectors, 31-35: 17 sectors */
            static const int sectors_per_track[] = {
                0,  /* Track 0 unused */
                21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-10 */
                21, 21, 21, 21, 21, 21, 21,              /* 11-17 */
                19, 19, 19, 19, 19, 19, 19,              /* 18-24 */
                18, 18, 18, 18, 18, 18,                  /* 25-30 */
                17, 17, 17, 17, 17                       /* 31-35 */
            };
            
            if (track < 1 || track > 35) {
                ctx->last_error = UFT_NIB_ERROR_TRACK;
                return UFT_NIB_ERROR_TRACK;
            }
            
            size_t offset = 0;
            for (int t = 1; t < track; t++) {
                offset += sectors_per_track[t] * 256;
            }
            
            if (offset >= ctx->file_size) {
                ctx->last_error = UFT_NIB_ERROR_TRACK;
                return UFT_NIB_ERROR_TRACK;
            }
            
            track_data = ctx->file_data + offset;
            track_size = sectors_per_track[track] * 256;
            
            /* D64 is already decoded - just validate */
            ctx->stats.total_sectors += sectors_per_track[track];
            ctx->stats.good_sectors += sectors_per_track[track];
            break;
        }
        
        case UFT_NIB_FORMAT_G64: {
            /* G64: Track offset table at offset 12 */
            if (ctx->file_size < 12 + 84 * 4) {
                ctx->last_error = UFT_NIB_ERROR_FILE_SIZE;
                return UFT_NIB_ERROR_FILE_SIZE;
            }
            
            /* Read track offset from table */
            size_t table_pos = 12 + track * 4;
            uint32_t track_offset = ctx->file_data[table_pos] |
                                   (ctx->file_data[table_pos + 1] << 8) |
                                   (ctx->file_data[table_pos + 2] << 16) |
                                   (ctx->file_data[table_pos + 3] << 24);
            
            if (track_offset == 0) {
                /* Empty track */
                ctx->last_error = UFT_NIB_OK;
                return UFT_NIB_OK;
            }
            
            if (track_offset + 2 >= ctx->file_size) {
                ctx->last_error = UFT_NIB_ERROR_TRACK;
                return UFT_NIB_ERROR_TRACK;
            }
            
            /* Track size is first 2 bytes at track offset */
            track_size = ctx->file_data[track_offset] |
                        (ctx->file_data[track_offset + 1] << 8);
            track_data = ctx->file_data + track_offset + 2;
            
            /* GCR decode would happen here - simplified for now */
            ctx->stats.total_sectors += 21;  /* Approximate */
            break;
        }
        
        case UFT_NIB_FORMAT_NIB: {
            /* NIB: Fixed 6656 bytes per track */
            size_t offset = track * 6656;
            if (offset + 6656 > ctx->file_size) {
                ctx->last_error = UFT_NIB_ERROR_TRACK;
                return UFT_NIB_ERROR_TRACK;
            }
            
            track_data = ctx->file_data + offset;
            track_size = 6656;
            
            ctx->stats.total_sectors += 16;  /* Apple II: 16 sectors */
            break;
        }
        
        default:
            ctx->last_error = UFT_NIB_ERROR_FORMAT;
            return UFT_NIB_ERROR_FORMAT;
    }
    
    (void)track_data;  /* Would be used for actual decoding */
    (void)track_size;
    
    ctx->last_error = UFT_NIB_OK;
    return UFT_NIB_OK;
}

uft_nibbel_error_t uft_nibbel_export(
    uft_nibbel_ctx_t* ctx,
    const char* path,
    const char* format
) {
    if (!ctx) return UFT_NIB_ERROR_NULL_POINTER;
    if (!path) return UFT_NIB_ERROR_NULL_POINTER;
    if (!ctx->processed) {
        ctx->last_error = UFT_NIB_ERROR_NOT_OPEN;
        return UFT_NIB_ERROR_NOT_OPEN;
    }
    
    /* Determine output format */
    uft_nibbel_format_t out_format = UFT_NIB_FORMAT_AUTO;
    
    if (format) {
        if (strcasecmp(format, "d64") == 0) {
            out_format = UFT_NIB_FORMAT_D64;
        } else if (strcasecmp(format, "g64") == 0) {
            out_format = UFT_NIB_FORMAT_G64;
        } else if (strcasecmp(format, "raw") == 0) {
            out_format = UFT_NIB_FORMAT_RAW;
        } else {
            ctx->last_error = UFT_NIB_ERROR_FORMAT;
            return UFT_NIB_ERROR_FORMAT;
        }
    } else {
        /* Auto-detect from path extension */
        const char* ext = strrchr(path, '.');
        if (ext) {
            out_format = uft_nibbel_format_from_extension(ext);
        }
    }
    
    if (out_format == UFT_NIB_FORMAT_AUTO) {
        out_format = UFT_NIB_FORMAT_D64;  /* Default to D64 */
    }
    
    /* Open output file */
    FILE* f = fopen(path, "wb");
    if (!f) {
        ctx->last_error = UFT_NIB_ERROR_FILE_WRITE;
        return UFT_NIB_ERROR_FILE_WRITE;
    }
    
    uft_nibbel_error_t result = UFT_NIB_OK;
    
    switch (out_format) {
        case UFT_NIB_FORMAT_D64: {
            /* Export as D64 (sector-based) */
            /* Standard D64: 174848 bytes (35 tracks × 683 sectors × 256) */
            static const int sectors_per_track[] = {
                0,
                21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
                21, 21, 21, 21, 21, 21, 21,
                19, 19, 19, 19, 19, 19, 19,
                18, 18, 18, 18, 18, 18,
                17, 17, 17, 17, 17
            };
            
            if (ctx->detected_format == UFT_NIB_FORMAT_D64) {
                /* Source is already D64 - just copy */
                size_t written = fwrite(ctx->file_data, 1, ctx->file_size, f);
                if (written != ctx->file_size) {
                    result = UFT_NIB_ERROR_FILE_WRITE;
                }
            } else {
                /* Would need conversion - write empty sectors for now */
                uint8_t empty_sector[256] = {0};
                
                for (int t = 1; t <= 35; t++) {
                    for (int s = 0; s < sectors_per_track[t]; s++) {
                        if (fwrite(empty_sector, 1, 256, f) != 256) {
                            result = UFT_NIB_ERROR_FILE_WRITE;
                            break;
                        }
                    }
                }
            }
            break;
        }
        
        case UFT_NIB_FORMAT_RAW: {
            /* Export raw data */
            if (ctx->file_data && ctx->file_size > 0) {
                size_t written = fwrite(ctx->file_data, 1, ctx->file_size, f);
                if (written != ctx->file_size) {
                    result = UFT_NIB_ERROR_FILE_WRITE;
                }
            }
            break;
        }
        
        default:
            result = UFT_NIB_ERROR_FORMAT;
            break;
    }
    
    fclose(f);
    
    ctx->last_error = result;
    return result;
}

int uft_nibbel_get_export_formats(const char** formats, int max_count) {
    static const char* export_formats[] = {
        "d64", "g64", "dsk", "po", "do", "scp", "raw", NULL
    };
    
    int count = 0;
    for (int i = 0; export_formats[i] && count < max_count; i++) {
        formats[count++] = export_formats[i];
    }
    return count;
}

uft_nibbel_error_t uft_nibbel_get_track_info(
    const uft_nibbel_ctx_t* ctx,
    int track,
    int head,
    uft_nibbel_track_info_t* info
) {
    if (!ctx || !info) return UFT_NIB_ERROR_NULL_POINTER;
    (void)track;
    (void)head;
    return UFT_NIB_ERROR_NOT_IMPLEMENTED;
}

uft_nibbel_error_t uft_nibbel_get_sector(
    const uft_nibbel_ctx_t* ctx,
    int track,
    int head,
    int sector,
    uint8_t* data,
    size_t data_size,
    uft_nibbel_sector_status_t* status
) {
    if (!ctx || !data) return UFT_NIB_ERROR_NULL_POINTER;
    (void)track;
    (void)head;
    (void)sector;
    (void)data_size;
    (void)status;
    return UFT_NIB_ERROR_NOT_IMPLEMENTED;
}
