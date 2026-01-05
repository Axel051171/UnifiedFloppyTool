/**
 * @file uft_unified_api.c
 * @brief UFT Unified High-Level API Implementation
 * 
 * @version 5.28.0 GOD MODE
 */

#include "uft/uft_unified_api.h"
#include "uft/uft_integration.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

struct uft_context {
    char last_error[512];
    uft_progress_cb progress_cb;
    void *progress_user_data;
    uft_log_cb log_cb;
    void *log_user_data;
    
    /* Options */
    bool strict_mode;
    bool verbose;
    int max_retries;
};

struct uft_image {
    uft_context_t *ctx;
    char *path;
    uint8_t *data;
    size_t data_size;
    uft_image_type_t type;
    
    /* Parsed info */
    char format_name[32];
    char platform_name[32];
    size_t tracks;
    size_t heads;
    size_t sectors_per_track;
    size_t sector_size;
    
    /* Filesystem */
    uft_filesystem_t *fs;
    const uft_fs_driver_t *fs_driver;
    
    /* Flux decoder (if flux image) */
    uft_flux_decoder_t *flux_dec;
    uft_bitstream_decoder_t *bs_dec;
};

struct uft_file {
    uft_image_t *image;
    char path[256];
    uint8_t *data;
    size_t size;
    size_t pos;
};

struct uft_dir {
    uft_image_t *image;
    char path[256];
    uft_dirent_t *entries;
    size_t entry_count;
    size_t current;
};

/* ============================================================================
 * Format Detection Tables
 * ============================================================================ */

typedef struct {
    const char *extension;
    const char *format;
    const char *platform;
    uft_image_type_t type;
} format_entry_t;

static const format_entry_t format_table[] = {
    /* Amiga */
    {".adf", "ADF", "Amiga", UFT_IMAGE_SECTOR},
    {".adz", "ADZ", "Amiga", UFT_IMAGE_ARCHIVE},
    {".dms", "DMS", "Amiga", UFT_IMAGE_ARCHIVE},
    
    /* Commodore */
    {".d64", "D64", "C64", UFT_IMAGE_SECTOR},
    {".d71", "D71", "C128", UFT_IMAGE_SECTOR},
    {".d81", "D81", "C128", UFT_IMAGE_SECTOR},
    {".g64", "G64", "C64", UFT_IMAGE_BITSTREAM},
    {".nib", "NIB", "C64", UFT_IMAGE_BITSTREAM},
    
    /* Apple */
    {".dsk", "DSK", "Apple II", UFT_IMAGE_SECTOR},
    {".do", "DSK", "Apple II", UFT_IMAGE_SECTOR},
    {".po", "ProDOS", "Apple II", UFT_IMAGE_SECTOR},
    {".2mg", "2MG", "Apple II", UFT_IMAGE_SECTOR},
    {".woz", "WOZ", "Apple II", UFT_IMAGE_FLUX},
    
    /* IBM PC */
    {".img", "IMG", "IBM PC", UFT_IMAGE_SECTOR},
    {".ima", "IMA", "IBM PC", UFT_IMAGE_SECTOR},
    {".vfd", "VFD", "IBM PC", UFT_IMAGE_SECTOR},
    {".360", "IMG", "IBM PC", UFT_IMAGE_SECTOR},
    {".720", "IMG", "IBM PC", UFT_IMAGE_SECTOR},
    {".144", "IMG", "IBM PC", UFT_IMAGE_SECTOR},
    
    /* Atari */
    {".st", "ST", "Atari ST", UFT_IMAGE_SECTOR},
    {".msa", "MSA", "Atari ST", UFT_IMAGE_ARCHIVE},
    {".atr", "ATR", "Atari 8-bit", UFT_IMAGE_SECTOR},
    {".xfd", "XFD", "Atari 8-bit", UFT_IMAGE_SECTOR},
    
    /* TRS-80 */
    {".dmk", "DMK", "TRS-80", UFT_IMAGE_BITSTREAM},
    {".jv3", "JV3", "TRS-80", UFT_IMAGE_SECTOR},
    
    /* Amstrad CPC */
    {".dsk", "EDSK", "Amstrad CPC", UFT_IMAGE_SECTOR},
    
    /* MSX */
    {".dsk", "MSX", "MSX", UFT_IMAGE_SECTOR},
    
    /* Flux formats */
    {".scp", "SCP", "Universal", UFT_IMAGE_FLUX},
    {".a2r", "A2R", "Apple II", UFT_IMAGE_FLUX},
    {".kf", "KryoFlux", "Universal", UFT_IMAGE_FLUX},
    {".raw", "KryoFlux", "Universal", UFT_IMAGE_FLUX},
    {".hfe", "HFE", "Universal", UFT_IMAGE_BITSTREAM},
    {".ipf", "IPF", "Universal", UFT_IMAGE_SECTOR},
    
    /* IMD/TD0 */
    {".imd", "IMD", "Universal", UFT_IMAGE_SECTOR},
    {".td0", "TD0", "Universal", UFT_IMAGE_ARCHIVE},
    
    {NULL, NULL, NULL, 0}
};

/* ============================================================================
 * Error Handling
 * ============================================================================ */

const char* uft_strerror(uft_status_t status)
{
    switch (status) {
        case UFT_SUCCESS:       return "Success";
        case UFT_ERR_INVALID_ARG: return "Invalid argument";
        case UFT_ERR_NO_MEMORY: return "Out of memory";
        case UFT_ERR_IO:        return "I/O error";
        case UFT_ERR_NOT_FOUND: return "Not found";
        case UFT_ERR_FORMAT:    return "Invalid format";
        case UFT_ERR_UNSUPPORTED: return "Not supported";
        case UFT_ERR_CRC:       return "CRC error";
        case UFT_ERR_CORRUPT:   return "Data corrupt";
        case UFT_ERR_PERMISSION: return "Permission denied";
        case UFT_ERR_INTERNAL:  return "Internal error";
        default:                return "Unknown error";
    }
}

const char* uft_get_last_error(uft_context_t *ctx)
{
    return ctx ? ctx->last_error : "No context";
}

static void set_error(uft_context_t *ctx, const char *fmt, ...)
{
    if (!ctx) return;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->last_error, sizeof(ctx->last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * Context Management
 * ============================================================================ */

uft_context_t* uft_create(void)
{
    uft_context_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    ctx->max_retries = 3;
    
    /* Initialize integration layer */
    uft_integration_init();
    
    return ctx;
}

void uft_destroy(uft_context_t *ctx)
{
    if (ctx) {
        uft_integration_cleanup();
        free(ctx);
    }
}

uft_status_t uft_set_option(uft_context_t *ctx, const char *key, const char *value)
{
    if (!ctx || !key) return UFT_ERR_INVALID_ARG;
    
    if (strcmp(key, "strict") == 0) {
        ctx->strict_mode = (value && strcmp(value, "true") == 0);
    } else if (strcmp(key, "verbose") == 0) {
        ctx->verbose = (value && strcmp(value, "true") == 0);
    } else if (strcmp(key, "retries") == 0) {
        ctx->max_retries = value ? atoi(value) : 3;
    }
    
    return UFT_SUCCESS;
}

const char* uft_get_option(uft_context_t *ctx, const char *key)
{
    return NULL;  /* TODO */
}

/* ============================================================================
 * Format Detection
 * ============================================================================ */

static const char* get_extension(const char *path)
{
    const char *ext = strrchr(path, '.');
    return ext ? ext : "";
}

static const format_entry_t* detect_by_extension(const char *path)
{
    const char *ext = get_extension(path);
    if (!ext || !*ext) return NULL;
    
    for (const format_entry_t *e = format_table; e->extension; e++) {
        if (strcasecmp(ext, e->extension) == 0) {
            return e;
        }
    }
    
    return NULL;
}

static uft_status_t detect_by_content(const uint8_t *data, size_t size,
                                      const format_entry_t **entry)
{
    /* Check magic bytes */
    
    /* SCP: "SCP" at offset 0 */
    if (size >= 3 && memcmp(data, "SCP", 3) == 0) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "SCP") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    /* A2R: "A2R" at offset 0 */
    if (size >= 4 && memcmp(data, "A2R", 3) == 0) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "A2R") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    /* WOZ: "WOZ" at offset 0 */
    if (size >= 4 && memcmp(data, "WOZ", 3) == 0) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "WOZ") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    /* HFE: "HXCPICFE" at offset 0 */
    if (size >= 8 && memcmp(data, "HXCPICFE", 8) == 0) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "HFE") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    /* IPF: "CAPS" at offset 0 */
    if (size >= 4 && memcmp(data, "CAPS", 4) == 0) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "IPF") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    /* DMS: "DMS!" at offset 0 */
    if (size >= 4 && memcmp(data, "DMS!", 4) == 0) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "DMS") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    /* ADF: Check for DOS signature at sector 0 */
    if (size >= 4 && memcmp(data, "DOS", 3) == 0) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "ADF") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    /* D64: Check size (174848 or 175531 bytes) */
    if (size == 174848 || size == 175531) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcmp(e->format, "D64") == 0) {
                *entry = e;
                return UFT_SUCCESS;
            }
        }
    }
    
    return UFT_ERR_FORMAT;
}

uft_status_t uft_detect_format(const char *path, 
                               char *format, size_t format_size,
                               uint8_t *confidence)
{
    const format_entry_t *entry = detect_by_extension(path);
    
    if (entry) {
        strncpy(format, entry->format, format_size - 1);
        format[format_size - 1] = '\0';
        if (confidence) *confidence = 80;
        return UFT_SUCCESS;
    }
    
    return UFT_ERR_FORMAT;
}

/* ============================================================================
 * Image Loading
 * ============================================================================ */

uft_status_t uft_load(uft_context_t *ctx, const char *path, uft_image_t **image)
{
    if (!ctx || !path || !image) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Open file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error(ctx, "Cannot open file: %s", path);
        return UFT_ERR_IO;
    }
    
    /* Get size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size == 0 || size > 100*1024*1024) {  /* Max 100MB */
        fclose(f);
        set_error(ctx, "Invalid file size: %zu", size);
        return UFT_ERR_FORMAT;
    }
    
    /* Read data */
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return UFT_ERR_NO_MEMORY;
    }
    
    if (fread(data, 1, size, f) != size) {
        fclose(f);
        free(data);
        set_error(ctx, "Read error");
        return UFT_ERR_IO;
    }
    fclose(f);
    
    /* Load from memory */
    uft_status_t status = uft_load_memory(ctx, data, size, 
                                          get_extension(path), image);
    
    if (status == UFT_SUCCESS && *image) {
        (*image)->path = strdup(path);
        (*image)->data = data;  /* Transfer ownership */
    } else {
        free(data);
    }
    
    return status;
}

uft_status_t uft_load_memory(uft_context_t *ctx, const uint8_t *data, 
                             size_t size, const char *format_hint,
                             uft_image_t **image)
{
    if (!ctx || !data || !size || !image) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Allocate image */
    uft_image_t *img = calloc(1, sizeof(*img));
    if (!img) return UFT_ERR_NO_MEMORY;
    
    img->ctx = ctx;
    img->data_size = size;
    
    /* Detect format */
    const format_entry_t *entry = NULL;
    
    if (format_hint) {
        for (const format_entry_t *e = format_table; e->extension; e++) {
            if (strcasecmp(format_hint, e->extension) == 0 ||
                strcasecmp(format_hint, e->format) == 0) {
                entry = e;
                break;
            }
        }
    }
    
    if (!entry) {
        detect_by_content(data, size, &entry);
    }
    
    if (entry) {
        strncpy(img->format_name, entry->format, sizeof(img->format_name) - 1);
        strncpy(img->platform_name, entry->platform, sizeof(img->platform_name) - 1);
        img->type = entry->type;
    } else {
        strcpy(img->format_name, "Unknown");
        strcpy(img->platform_name, "Unknown");
        img->type = UFT_IMAGE_UNKNOWN;
    }
    
    /* Parse format-specific structure */
    if (strcmp(img->format_name, "ADF") == 0) {
        /* Amiga ADF: 880KB = 1760 sectors of 512 bytes */
        img->tracks = 80;
        img->heads = 2;
        img->sectors_per_track = 11;
        img->sector_size = 512;
    } else if (strcmp(img->format_name, "D64") == 0) {
        /* C64 D64: Variable sectors per track */
        img->tracks = 35;
        img->heads = 1;
        img->sectors_per_track = 0;  /* Variable */
        img->sector_size = 256;
    } else if (strcmp(img->format_name, "IMG") == 0) {
        /* PC IMG: Detect from size */
        if (size == 1474560) {  /* 1.44MB */
            img->tracks = 80;
            img->heads = 2;
            img->sectors_per_track = 18;
            img->sector_size = 512;
        } else if (size == 737280) {  /* 720KB */
            img->tracks = 80;
            img->heads = 2;
            img->sectors_per_track = 9;
            img->sector_size = 512;
        } else if (size == 368640) {  /* 360KB */
            img->tracks = 40;
            img->heads = 2;
            img->sectors_per_track = 9;
            img->sector_size = 512;
        }
    }
    
    /* Try to mount filesystem */
    /* Create temporary disk structure for filesystem probing */
    uft_disk_t disk_view = {0};
    disk_view.data = img->data;
    disk_view.size = img->data_size;
    disk_view.tracks = img->tracks;
    disk_view.heads = img->heads;
    disk_view.sectors_per_track = img->sectors_per_track;
    disk_view.sector_size = img->sector_size;
    disk_view.read_only = true;
    
    /* Try automatic filesystem detection */
    const uft_fs_driver_t *driver = NULL;
    uft_filesystem_t *fs = NULL;
    
    if (uft_fs_mount_auto(&disk_view, &fs, &driver) == UFT_OK) {
        img->fs = fs;
        img->fs_driver = driver;
    }
    /* Filesystem mount failure is not an error - image might be raw/unformatted */
    
    *image = img;
    return UFT_SUCCESS;
}

void uft_close(uft_image_t *image)
{
    if (image) {
        /* Unmount filesystem if mounted */
        if (image->fs && image->fs_driver && image->fs_driver->unmount) {
            image->fs_driver->unmount(image->fs);
        }
        
        free(image->path);
        free(image->data);
        uft_flux_decoder_free(image->flux_dec);
        uft_bitstream_decoder_free(image->bs_dec);
        free(image);
    }
}

uft_status_t uft_get_info(uft_image_t *image, uft_image_info_t *info)
{
    if (!image || !info) return UFT_ERR_INVALID_ARG;
    
    memset(info, 0, sizeof(*info));
    
    info->type = image->type;
    info->format_name = image->format_name;
    info->platform_name = image->platform_name;
    info->tracks = image->tracks;
    info->heads = image->heads;
    info->sectors_per_track = image->sectors_per_track;
    info->sector_size = image->sector_size;
    info->total_size = image->data_size;
    
    return UFT_SUCCESS;
}

/* ============================================================================
 * Raw Access
 * ============================================================================ */

uft_status_t uft_read_sector(uft_image_t *image, 
                             size_t track, size_t head, size_t sector,
                             uint8_t *buffer, size_t buffer_size)
{
    if (!image || !buffer) return UFT_ERR_INVALID_ARG;
    
    if (image->type != UFT_IMAGE_SECTOR) {
        return UFT_ERR_UNSUPPORTED;
    }
    
    /* Calculate offset based on format */
    size_t offset = 0;
    size_t sec_size = image->sector_size;
    
    if (strcmp(image->format_name, "ADF") == 0) {
        /* Amiga ADF: sequential sectors */
        offset = ((track * image->heads + head) * image->sectors_per_track + sector) * sec_size;
    } else if (strcmp(image->format_name, "D64") == 0) {
        /* D64: Variable sectors per track */
        static const uint8_t d64_sectors[] = {
            21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
            19, 19, 19, 19, 19, 19, 19,
            18, 18, 18, 18, 18, 18,
            17, 17, 17, 17, 17
        };
        
        for (size_t t = 0; t < track && t < 35; t++) {
            offset += d64_sectors[t] * 256;
        }
        offset += sector * 256;
        sec_size = 256;
    } else if (strcmp(image->format_name, "IMG") == 0) {
        /* PC IMG: sequential sectors */
        offset = ((track * image->heads + head) * image->sectors_per_track + sector) * sec_size;
    }
    
    if (offset + sec_size > image->data_size) {
        return UFT_ERR_INVALID_ARG;
    }
    
    if (buffer_size < sec_size) {
        return UFT_ERR_INVALID_ARG;
    }
    
    memcpy(buffer, image->data + offset, sec_size);
    return UFT_SUCCESS;
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

uft_status_t uft_read_file(uft_image_t *image, const char *path,
                           uint8_t **data, size_t *size)
{
    if (!image || !path || !data || !size) return UFT_ERR_INVALID_ARG;
    
    *data = NULL;
    *size = 0;
    
    /* Check if filesystem is mounted */
    if (!image->fs || !image->fs_driver || !image->fs_driver->read) {
        return UFT_ERR_UNSUPPORTED;
    }
    
    /* First get file size via stat if available */
    uft_file_entry_t entry = {0};
    if (image->fs_driver->stat) {
        uft_error_t err = image->fs_driver->stat(image->fs, path, &entry);
        if (err != UFT_OK) {
            return UFT_ERR_NOT_FOUND;
        }
    }
    
    /* Allocate buffer - use stat size or default max */
    size_t buf_size = entry.size > 0 ? entry.size : (16 * 1024 * 1024);
    uint8_t *buffer = malloc(buf_size);
    if (!buffer) return UFT_ERR_NO_MEMORY;
    
    /* Read file */
    size_t bytes_read = 0;
    uft_error_t err = image->fs_driver->read(image->fs, path, buffer, buf_size, &bytes_read);
    if (err != UFT_OK) {
        free(buffer);
        return UFT_ERR_IO;
    }
    
    /* Shrink buffer if we read less than allocated */
    if (bytes_read < buf_size) {
        uint8_t *shrunk = realloc(buffer, bytes_read > 0 ? bytes_read : 1);
        if (shrunk) buffer = shrunk;
    }
    
    *data = buffer;
    *size = bytes_read;
    return UFT_SUCCESS;
}

void uft_free_data(uint8_t *data)
{
    free(data);
}

/* ============================================================================
 * Version Info
 * ============================================================================ */

const char* uft_version(void)
{
    return "5.28.0";
}

void uft_version_info(int *major, int *minor, int *patch)
{
    if (major) *major = 5;
    if (minor) *minor = 28;
    if (patch) *patch = 0;
}

const char* uft_build_info(void)
{
    return "UFT 5.28.0 GOD MODE - Built with 382 format parsers, "
           "27 track decoders, 11 filesystems";
}

/* ============================================================================
 * Format Lists
 * ============================================================================ */

const char** uft_get_input_formats(size_t *count)
{
    static const char* formats[] = {
        "ADF", "ADZ", "DMS",
        "D64", "D71", "D81", "G64", "NIB",
        "DSK", "DO", "PO", "2MG", "WOZ",
        "IMG", "IMA", "VFD",
        "ST", "MSA", "ATR", "XFD",
        "DMK", "JV3",
        "SCP", "A2R", "HFE", "IPF",
        "IMD", "TD0",
        NULL
    };
    
    if (count) {
        *count = 0;
        while (formats[*count]) (*count)++;
    }
    
    return formats;
}

const char** uft_get_output_formats(size_t *count)
{
    static const char* formats[] = {
        "ADF", "D64", "IMG", "ST", "DSK", "HFE",
        NULL
    };
    
    if (count) {
        *count = 0;
        while (formats[*count]) (*count)++;
    }
    
    return formats;
}

void uft_set_progress_callback(uft_context_t *ctx, 
                               uft_progress_cb callback,
                               void *user_data)
{
    if (ctx) {
        ctx->progress_cb = callback;
        ctx->progress_user_data = user_data;
    }
}

void uft_set_log_callback(uft_context_t *ctx,
                          uft_log_cb callback,
                          void *user_data)
{
    if (ctx) {
        ctx->log_cb = callback;
        ctx->log_user_data = user_data;
    }
}
