/**
 * @file uft_writer_backend.c
 * @brief Writer Backend Implementation
 * 
 * P0-002: Complete Writer Backend for Transaction System
 */

#include "uft/uft_writer_backend.h"
#include "uft/uft_safe_io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_writer_backend {
    uft_writer_options_t    options;
    uft_writer_stats_t      stats;
    
    /* State */
    bool                    is_open;
    char                    last_error[256];
    
    /* Image backend */
    FILE                    *image_file;
    uint8_t                 *image_buffer;
    size_t                  image_size;
    size_t                  track_size;
    int                     tracks_per_side;
    int                     heads;
    
    /* Memory backend */
    uint8_t                 *memory_buffer;
    size_t                  memory_size;
    
    /* Progress callback */
    uft_writer_progress_fn  progress_fn;
    void                    *progress_user;
    
    /* Timing */
    struct timespec         start_time;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static double get_elapsed_ms(struct timespec *start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) * 1000.0 +
           (now.tv_nsec - start->tv_nsec) / 1000000.0;
}

static size_t calc_track_offset(uft_writer_backend_t *b, uint8_t cyl, uint8_t head) {
    /* Standard layout: all tracks side 0, then all tracks side 1 */
    size_t track_num = (head == 0) ? cyl : (b->tracks_per_side + cyl);
    return track_num * b->track_size;
}

static void report_progress(uft_writer_backend_t *b, int cyl, int head, 
                           int percent, const char *status) {
    if (b->progress_fn) {
        b->progress_fn(cyl, head, percent, status, b->progress_user);
    }
}

static void set_error(uft_writer_backend_t *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(b->last_error, sizeof(b->last_error), fmt, args);
    va_end(args);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format-specific Parameters
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int     tracks;
    int     heads;
    size_t  track_size;
    size_t  total_size;
} format_params_t;

static format_params_t get_format_params(uft_format_t format) {
    format_params_t p = {0};
    
    switch (format) {
        case UFT_FORMAT_ADF:
            p.tracks = 80; p.heads = 2;
            p.track_size = 11 * 512;  /* 11 sectors * 512 bytes */
            p.total_size = 901120;    /* 880 KB */
            break;
            
        case UFT_FORMAT_D64:
            p.tracks = 35; p.heads = 1;
            p.track_size = 0;  /* Variable - handled specially */
            p.total_size = 174848;
            break;
            
        case UFT_FORMAT_ST:
            p.tracks = 80; p.heads = 2;
            p.track_size = 9 * 512;
            p.total_size = 737280;  /* 720 KB */
            break;
            
        case UFT_FORMAT_IMG:
        case UFT_FORMAT_RAW:
            p.tracks = 80; p.heads = 2;
            p.track_size = 18 * 512;
            p.total_size = 1474560;  /* 1.44 MB */
            break;
            
        case UFT_FORMAT_G64:
            p.tracks = 42; p.heads = 1;  /* Can have half-tracks */
            p.track_size = 7928;  /* Max track size */
            p.total_size = 0;  /* Variable */
            break;
            
        case UFT_FORMAT_SCP:
            p.tracks = 84; p.heads = 2;
            p.track_size = 0;  /* Variable flux data */
            p.total_size = 0;
            break;
            
        default:
            p.tracks = 80; p.heads = 2;
            p.track_size = 512 * 18;
            p.total_size = 1474560;
    }
    
    return p;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Backend Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_writer_backend_t* uft_writer_create(const uft_writer_options_t *options) {
    uft_writer_backend_t *b = calloc(1, sizeof(uft_writer_backend_t));
    if (!b) return NULL;
    
    if (options) {
        b->options = *options;
    } else {
        b->options = (uft_writer_options_t)UFT_WRITER_OPTIONS_DEFAULT;
    }
    
    /* Set format-specific parameters */
    format_params_t params = get_format_params(b->options.format);
    b->tracks_per_side = params.tracks;
    b->heads = params.heads;
    b->track_size = params.track_size;
    b->image_size = params.total_size;
    
    return b;
}

void uft_writer_destroy(uft_writer_backend_t *backend) {
    if (!backend) return;
    
    uft_writer_close(backend);
    
    free(backend->image_buffer);
    free(backend->memory_buffer);
    free(backend);
}

uft_error_t uft_writer_open(uft_writer_backend_t *b) {
    if (!b) return UFT_ERR_INVALID_PARAM;
    if (b->is_open) return UFT_OK;
    
    clock_gettime(CLOCK_MONOTONIC, &b->start_time);
    
    switch (b->options.type) {
        case UFT_BACKEND_IMAGE: {
            if (!b->options.image_path) {
                set_error(b, "No image path specified");
                return UFT_ERR_INVALID_PARAM;
            }
            
            const char *mode = b->options.create_new ? "wb+" : "rb+";
            b->image_file = fopen(b->options.image_path, mode);
            
            if (!b->image_file && !b->options.create_new) {
                /* Try creating new file */
                b->image_file = fopen(b->options.image_path, "wb+");
            }
            
            if (!b->image_file) {
                set_error(b, "Cannot open image: %s", b->options.image_path);
                return UFT_ERR_IO;
            }
            
            /* Allocate image buffer for read-modify-write operations */
            if (b->image_size > 0) {
                b->image_buffer = calloc(1, b->image_size);
                if (!b->image_buffer) {
                    fclose(b->image_file);
                    b->image_file = NULL;
                    set_error(b, "Cannot allocate image buffer");
                    return UFT_ERR_MEMORY;
                }
                
                /* Read existing content if not creating new */
                if (!b->options.create_new) {
                    if (fseek(b->image_file, 0, SEEK_SET) != 0) return UFT_ERR_IO;
                    size_t read = fread(b->image_buffer, 1, b->image_size, b->image_file);
                    if (read != b->image_size) {
                        set_error(b, "Short read loading image: %zu of %zu", read, b->image_size);
                        free(b->image_buffer);
                        b->image_buffer = NULL;
                        return UFT_ERR_IO;
                    }
                }
            }
            break;
        }
        
        case UFT_BACKEND_MEMORY: {
            size_t size = b->image_size > 0 ? b->image_size : 2 * 1024 * 1024;
            b->memory_buffer = calloc(1, size);
            if (!b->memory_buffer) {
                set_error(b, "Cannot allocate memory buffer");
                return UFT_ERR_MEMORY;
            }
            b->memory_size = size;
            break;
        }
        
        case UFT_BACKEND_HARDWARE: {
            /* Hardware backend would initialize device communication here */
            /* For now, placeholder - actual implementation depends on provider */
            set_error(b, "Hardware backend not yet implemented");
            return UFT_ERR_UNSUPPORTED;
        }
        
        case UFT_BACKEND_FLUX: {
            /* Flux backend for raw flux writing */
            if (!b->options.image_path) {
                set_error(b, "No output path specified for flux");
                return UFT_ERR_INVALID_PARAM;
            }
            b->image_file = fopen(b->options.image_path, "wb");
            if (!b->image_file) {
                set_error(b, "Cannot create flux file: %s", b->options.image_path);
                return UFT_ERR_IO;
            }
            break;
        }
        
        default:
            set_error(b, "Unknown backend type");
            return UFT_ERR_INVALID_PARAM;
    }
    
    b->is_open = true;
    return UFT_OK;
}

void uft_writer_close(uft_writer_backend_t *b) {
    if (!b || !b->is_open) return;
    
    /* Flush image buffer to file */
    if (b->image_file && b->image_buffer) {
        if (fseek(b->image_file, 0, SEEK_SET) != 0) {
            /* Seek failed, cannot flush */
        } else if (fwrite(b->image_buffer, 1, b->image_size, b->image_file) != b->image_size) {
            /* Write failed but we still close below */
        }
    }
    
    if (b->image_file) {
        fclose(b->image_file);
        b->image_file = NULL;
    }
    
    b->stats.elapsed_ms = get_elapsed_ms(&b->start_time);
    b->is_open = false;
}

bool uft_writer_is_ready(const uft_writer_backend_t *b) {
    return b && b->is_open;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations - Image Backend
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t image_write_track(uft_writer_backend_t *b,
                                      uint8_t cyl, uint8_t head,
                                      const uint8_t *data, size_t size) {
    if (!b->image_buffer && !b->image_file) {
        set_error(b, "Image not open");
        return UFT_ERR_STATE;
    }
    
    size_t offset = calc_track_offset(b, cyl, head);
    
    /* Bounds check */
    if (offset + size > b->image_size) {
        set_error(b, "Track offset %zu + size %zu exceeds image size %zu",
                  offset, size, b->image_size);
        return UFT_ERR_BOUNDS;
    }
    
    /* Write to buffer */
    if (b->image_buffer) {
        memcpy(b->image_buffer + offset, data, size);
    }
    
    /* Also write directly to file for safety */
    if (b->image_file) {
        if (fseek(b->image_file, offset, SEEK_SET) != 0) {
            set_error(b, "Seek failed to offset %zu", (size_t)offset);
            return UFT_ERR_IO;
        }
        size_t written = fwrite(data, 1, size, b->image_file);
        if (written != size) {
            set_error(b, "Short write: %zu of %zu bytes", written, size);
            return UFT_ERR_IO;
        }
        fflush(b->image_file);
    }
    
    b->stats.tracks_written++;
    b->stats.bytes_written += size;
    
    return UFT_OK;
}

static uft_error_t image_write_sector(uft_writer_backend_t *b,
                                       uint8_t cyl, uint8_t head, uint8_t sector,
                                       const uint8_t *data, size_t size) {
    if (!b->image_buffer && !b->image_file) {
        set_error(b, "Image not open");
        return UFT_ERR_STATE;
    }
    
    /* Calculate sector offset */
    size_t track_offset = calc_track_offset(b, cyl, head);
    size_t sector_offset = track_offset + (sector * size);
    
    /* Bounds check */
    if (sector_offset + size > b->image_size) {
        set_error(b, "Sector offset exceeds image size");
        return UFT_ERR_BOUNDS;
    }
    
    /* Write to buffer */
    if (b->image_buffer) {
        memcpy(b->image_buffer + sector_offset, data, size);
    }
    
    /* Write to file */
    if (b->image_file) {
        if (fseek(b->image_file, sector_offset, SEEK_SET) != 0) {
            set_error(b, "Seek failed to sector offset");
            return UFT_ERR_IO;
        }
        size_t written = fwrite(data, 1, size, b->image_file);
        if (written != size) {
            set_error(b, "Short sector write");
            return UFT_ERR_IO;
        }
    }
    
    b->stats.sectors_written++;
    b->stats.bytes_written += size;
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations - Memory Backend
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t memory_write_track(uft_writer_backend_t *b,
                                       uint8_t cyl, uint8_t head,
                                       const uint8_t *data, size_t size) {
    size_t offset = calc_track_offset(b, cyl, head);
    
    if (offset + size > b->memory_size) {
        set_error(b, "Track exceeds memory buffer");
        return UFT_ERR_BOUNDS;
    }
    
    memcpy(b->memory_buffer + offset, data, size);
    b->stats.tracks_written++;
    b->stats.bytes_written += size;
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations - Public API
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_writer_write_track(uft_writer_backend_t *b,
                                    uint8_t cylinder, uint8_t head,
                                    const uint8_t *data, size_t size) {
    if (!b || !data) return UFT_ERR_INVALID_PARAM;
    if (!b->is_open) return UFT_ERR_STATE;
    
    report_progress(b, cylinder, head, 0, "Writing track");
    
    uft_error_t err;
    
    switch (b->options.type) {
        case UFT_BACKEND_IMAGE:
            err = image_write_track(b, cylinder, head, data, size);
            break;
            
        case UFT_BACKEND_MEMORY:
            err = memory_write_track(b, cylinder, head, data, size);
            break;
            
        case UFT_BACKEND_HARDWARE:
            /* Would dispatch to hardware provider */
            set_error(b, "Hardware write not implemented");
            err = UFT_ERR_UNSUPPORTED;
            break;
            
        default:
            err = UFT_ERR_INVALID_PARAM;
    }
    
    if (err != UFT_OK) {
        b->stats.tracks_failed++;
    }
    
    return err;
}

uft_error_t uft_writer_write_sector(uft_writer_backend_t *b,
                                     uint8_t cylinder, uint8_t head, uint8_t sector,
                                     const uint8_t *data, size_t size) {
    if (!b || !data) return UFT_ERR_INVALID_PARAM;
    if (!b->is_open) return UFT_ERR_STATE;
    
    uft_error_t err;
    
    switch (b->options.type) {
        case UFT_BACKEND_IMAGE:
            err = image_write_sector(b, cylinder, head, sector, data, size);
            break;
            
        case UFT_BACKEND_MEMORY:
            /* Similar to image */
            err = image_write_sector(b, cylinder, head, sector, data, size);
            break;
            
        default:
            err = UFT_ERR_UNSUPPORTED;
    }
    
    if (err != UFT_OK) {
        b->stats.sectors_failed++;
    }
    
    return err;
}

uft_error_t uft_writer_write_flux(uft_writer_backend_t *b,
                                   uint8_t cylinder, uint8_t head,
                                   const double *flux_times, size_t count) {
    if (!b || !flux_times) return UFT_ERR_INVALID_PARAM;
    if (!b->is_open) return UFT_ERR_STATE;
    
    if (b->options.type != UFT_BACKEND_FLUX && 
        b->options.type != UFT_BACKEND_HARDWARE) {
        set_error(b, "Flux write requires FLUX or HARDWARE backend");
        return UFT_ERR_UNSUPPORTED;
    }
    
    /* For flux backend, write timing data */
    if (b->options.type == UFT_BACKEND_FLUX && b->image_file) {
        /* Simple format: track header + flux times */
        uint8_t header[4] = {cylinder, head, (count >> 8) & 0xFF, count & 0xFF};
        fwrite(header, 1, 4, b->image_file);
        
        /* Write flux times as 32-bit nanosecond values */
        for (size_t i = 0; i < count; i++) {
            uint32_t ns = (uint32_t)(flux_times[i]);
            fwrite(&ns, sizeof(ns), 1, b->image_file);
        }
        
        if (ferror(b->image_file)) {
            set_error(b, "Flux write failed");
            return UFT_ERR_IO;
        }
        
        b->stats.tracks_written++;
        b->stats.bytes_written += 4 + count * sizeof(uint32_t);
    }
    
    return UFT_OK;
}

uft_error_t uft_writer_format_track(uft_writer_backend_t *b,
                                     uint8_t cylinder, uint8_t head,
                                     int sectors_per_track, int sector_size) {
    if (!b) return UFT_ERR_INVALID_PARAM;
    if (!b->is_open) return UFT_ERR_STATE;
    
    /* Create formatted track with gaps and sector headers */
    size_t track_size = sectors_per_track * (sector_size + 64);  /* + overhead */
    uint8_t *track = calloc(1, track_size);
    if (!track) return UFT_ERR_MEMORY;
    
    /* Fill with format pattern */
    memset(track, b->options.fill_byte, track_size);
    
    /* Write formatted track */
    uft_error_t err = uft_writer_write_track(b, cylinder, head, track, track_size);
    
    free(track);
    return err;
}

uft_error_t uft_writer_erase_track(uft_writer_backend_t *b,
                                    uint8_t cylinder, uint8_t head) {
    if (!b) return UFT_ERR_INVALID_PARAM;
    if (!b->is_open) return UFT_ERR_STATE;
    
    /* Create empty track filled with erase pattern (0x00 or 0x4E) */
    size_t track_size = b->track_size > 0 ? b->track_size : 12500;  /* ~100ms at DD */
    uint8_t *track = calloc(1, track_size);
    if (!track) return UFT_ERR_MEMORY;
    
    /* For MFM, fill with 0x4E (gap byte); for FM, 0xFF */
    memset(track, 0x4E, track_size);
    
    uft_error_t err = uft_writer_write_track(b, cylinder, head, track, track_size);
    
    free(track);
    return err;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Verify Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_writer_verify_track(uft_writer_backend_t *b,
                                     uint8_t cylinder, uint8_t head,
                                     const uint8_t *expected, size_t size) {
    if (!b || !expected) return UFT_ERR_INVALID_PARAM;
    if (!b->is_open) return UFT_ERR_STATE;
    
    uint8_t *readback = malloc(size);
    if (!readback) return UFT_ERR_MEMORY;
    
    uft_error_t err = uft_writer_read_track(b, cylinder, head, readback, size);
    
    if (err == UFT_OK) {
        if (memcmp(expected, readback, size) != 0) {
            set_error(b, "Verify failed: data mismatch on cyl %d head %d",
                      cylinder, head);
            err = UFT_ERR_VERIFY;
            b->stats.verify_errors++;
        } else {
            b->stats.tracks_verified++;
        }
    }
    
    free(readback);
    return err;
}

uft_error_t uft_writer_verify_sector(uft_writer_backend_t *b,
                                      uint8_t cylinder, uint8_t head, uint8_t sector,
                                      const uint8_t *expected, size_t size) {
    if (!b || !expected) return UFT_ERR_INVALID_PARAM;
    
    /* Read back and compare sector */
    size_t track_offset = calc_track_offset(b, cylinder, head);
    size_t sector_offset = track_offset + (sector * size);
    
    if (b->image_buffer && sector_offset + size <= b->image_size) {
        if (memcmp(expected, b->image_buffer + sector_offset, size) != 0) {
            set_error(b, "Sector verify failed");
            b->stats.verify_errors++;
            return UFT_ERR_VERIFY;
        }
    }
    
    return UFT_OK;
}

uft_error_t uft_writer_read_track(uft_writer_backend_t *b,
                                   uint8_t cylinder, uint8_t head,
                                   uint8_t *buffer, size_t size) {
    if (!b || !buffer) return UFT_ERR_INVALID_PARAM;
    if (!b->is_open) return UFT_ERR_STATE;
    
    size_t offset = calc_track_offset(b, cylinder, head);
    
    if (b->image_buffer && offset + size <= b->image_size) {
        memcpy(buffer, b->image_buffer + offset, size);
        return UFT_OK;
    }
    
    if (b->image_file) {
        if (fseek(b->image_file, offset, SEEK_SET) != 0) return UFT_ERR_IO;
        size_t read = fread(buffer, 1, size, b->image_file);
        if (read != size) {
            set_error(b, "Short read: %zu of %zu", read, size);
            return UFT_ERR_IO;
        }
        return UFT_OK;
    }
    
    return UFT_ERR_STATE;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_writer_set_progress(uft_writer_backend_t *b,
                              uft_writer_progress_fn callback, void *user) {
    if (b) {
        b->progress_fn = callback;
        b->progress_user = user;
    }
}

void uft_writer_get_stats(const uft_writer_backend_t *b, uft_writer_stats_t *stats) {
    if (b && stats) {
        *stats = b->stats;
        if (b->is_open) {
            /* Update elapsed time */
            stats->elapsed_ms = get_elapsed_ms((struct timespec*)&b->start_time);
        }
    }
}

void uft_writer_reset_stats(uft_writer_backend_t *b) {
    if (b) {
        memset(&b->stats, 0, sizeof(b->stats));
    }
}

const char* uft_writer_get_error(const uft_writer_backend_t *b) {
    return b ? b->last_error : "NULL backend";
}

const char* uft_backend_type_name(uft_backend_type_t type) {
    switch (type) {
        case UFT_BACKEND_NONE: return "None";
        case UFT_BACKEND_IMAGE: return "Image";
        case UFT_BACKEND_HARDWARE: return "Hardware";
        case UFT_BACKEND_MEMORY: return "Memory";
        case UFT_BACKEND_FLUX: return "Flux";
        default: return "Unknown";
    }
}
