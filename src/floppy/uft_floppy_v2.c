/**
 * @file uft_floppy_v2.c
 * @brief UFT Floppy Library v2 - Unified Disk Interface
 * 
 * EXT4-016: Modern unified floppy disk library
 * 
 * Features:
 * - Unified disk image API
 * - Multiple format support
 * - Automatic format detection
 * - Read/write/convert operations
 * - Sector-level access
 * - Track-level access
 */

#include "uft/floppy/uft_floppy_v2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_MAX_TRACKS      84
#define UFT_MAX_SIDES       2
#define UFT_MAX_SECTORS     36
#define UFT_MAX_SECTOR_SIZE 8192

/*===========================================================================
 * Format Registry
 *===========================================================================*/

typedef struct {
    uft_disk_format_t format;
    const char *name;
    const char *extension;
    int (*detect)(const uint8_t *data, size_t size);
    int (*open)(uft_disk_t *disk, const uint8_t *data, size_t size);
    int (*read_sector)(uft_disk_t *disk, int track, int side, int sector,
                       uint8_t *buffer, size_t size);
    int (*write_sector)(uft_disk_t *disk, int track, int side, int sector,
                        const uint8_t *buffer, size_t size);
    void (*close)(uft_disk_t *disk);
} uft_format_handler_t;

/* Forward declarations */
static int raw_detect(const uint8_t *data, size_t size);
static int raw_open(uft_disk_t *disk, const uint8_t *data, size_t size);
static int raw_read(uft_disk_t *disk, int track, int side, int sector,
                    uint8_t *buffer, size_t size);
static int raw_write(uft_disk_t *disk, int track, int side, int sector,
                     const uint8_t *buffer, size_t size);

/* Format handlers */
static const uft_format_handler_t format_handlers[] = {
    {UFT_FORMAT_RAW, "Raw Sector Image", "img,ima,dsk", 
     raw_detect, raw_open, raw_read, raw_write, NULL},
    /* More formats can be added here */
    {UFT_FORMAT_UNKNOWN, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

/*===========================================================================
 * Disk Context
 *===========================================================================*/

struct uft_disk_s {
    uft_disk_format_t format;
    uft_disk_geometry_t geometry;
    
    uint8_t *data;
    size_t data_size;
    bool owns_data;
    bool modified;
    
    const uft_format_handler_t *handler;
    void *format_ctx;
    
    char error_msg[256];
};

/*===========================================================================
 * Error Handling
 *===========================================================================*/

static void set_error(uft_disk_t *disk, const char *msg)
{
    if (disk && msg) {
        strncpy(disk->error_msg, msg, sizeof(disk->error_msg) - 1);
        disk->error_msg[sizeof(disk->error_msg) - 1] = '\0';
    }
}

const char *uft_disk_get_error(uft_disk_t *disk)
{
    return disk ? disk->error_msg : "Invalid disk";
}

/*===========================================================================
 * Raw Format Implementation
 *===========================================================================*/

static int raw_detect(const uint8_t *data, size_t size)
{
    /* Raw images are detected by size */
    static const size_t known_sizes[] = {
        163840,   /* 160KB (40x1x8x512) */
        184320,   /* 180KB (40x1x9x512) */
        327680,   /* 320KB (40x2x8x512) */
        368640,   /* 360KB (40x2x9x512) */
        655360,   /* 640KB (80x2x8x512) */
        737280,   /* 720KB (80x2x9x512) */
        1228800,  /* 1.2MB (80x2x15x512) */
        1474560,  /* 1.44MB (80x2x18x512) */
        2949120,  /* 2.88MB (80x2x36x512) */
        0
    };
    
    (void)data;
    
    for (int i = 0; known_sizes[i] != 0; i++) {
        if (size == known_sizes[i]) {
            return 100;  /* High confidence */
        }
    }
    
    /* Check if size is a multiple of 512 */
    if (size >= 512 && size % 512 == 0) {
        return 50;  /* Medium confidence */
    }
    
    return 0;
}

static int raw_open(uft_disk_t *disk, const uint8_t *data, size_t size)
{
    if (!disk || !data) return -1;
    
    /* Determine geometry from size */
    int tracks = 0, sides = 0, sectors = 0, sector_size = 512;
    
    switch (size) {
        case 163840:  tracks = 40; sides = 1; sectors = 8; break;
        case 184320:  tracks = 40; sides = 1; sectors = 9; break;
        case 327680:  tracks = 40; sides = 2; sectors = 8; break;
        case 368640:  tracks = 40; sides = 2; sectors = 9; break;
        case 655360:  tracks = 80; sides = 2; sectors = 8; break;
        case 737280:  tracks = 80; sides = 2; sectors = 9; break;
        case 1228800: tracks = 80; sides = 2; sectors = 15; break;
        case 1474560: tracks = 80; sides = 2; sectors = 18; break;
        case 2949120: tracks = 80; sides = 2; sectors = 36; break;
        default:
            /* Try to guess */
            sectors = 18;
            sides = 2;
            tracks = size / (sectors * sides * sector_size);
            if (tracks == 0) tracks = 80;
            break;
    }
    
    disk->geometry.tracks = tracks;
    disk->geometry.sides = sides;
    disk->geometry.sectors = sectors;
    disk->geometry.sector_size = sector_size;
    disk->geometry.total_size = size;
    
    return 0;
}

static int raw_read(uft_disk_t *disk, int track, int side, int sector,
                    uint8_t *buffer, size_t size)
{
    if (!disk || !buffer) return -1;
    
    int sec_size = disk->geometry.sector_size;
    if ((int)size < sec_size) return -1;
    
    size_t offset = ((track * disk->geometry.sides + side) * 
                     disk->geometry.sectors + (sector - 1)) * sec_size;
    
    if (offset + sec_size > disk->data_size) return -1;
    
    memcpy(buffer, disk->data + offset, sec_size);
    return sec_size;
}

static int raw_write(uft_disk_t *disk, int track, int side, int sector,
                     const uint8_t *buffer, size_t size)
{
    if (!disk || !buffer) return -1;
    
    int sec_size = disk->geometry.sector_size;
    if ((int)size < sec_size) return -1;
    
    size_t offset = ((track * disk->geometry.sides + side) * 
                     disk->geometry.sectors + (sector - 1)) * sec_size;
    
    if (offset + sec_size > disk->data_size) return -1;
    
    memcpy(disk->data + offset, buffer, sec_size);
    disk->modified = true;
    
    return sec_size;
}

/*===========================================================================
 * Public API - Disk Operations
 *===========================================================================*/

uft_disk_t *uft_disk_create(void)
{
    uft_disk_t *disk = calloc(1, sizeof(uft_disk_t));
    if (!disk) return NULL;
    
    disk->format = UFT_FORMAT_UNKNOWN;
    return disk;
}

void uft_disk_destroy(uft_disk_t *disk)
{
    if (!disk) return;
    
    if (disk->handler && disk->handler->close) {
        disk->handler->close(disk);
    }
    
    if (disk->owns_data) {
        free(disk->data);
    }
    
    free(disk->format_ctx);
    free(disk);
}

int uft_disk_open(uft_disk_t *disk, const uint8_t *data, size_t size)
{
    if (!disk || !data || size == 0) return -1;
    
    /* Auto-detect format */
    int best_score = 0;
    const uft_format_handler_t *best_handler = NULL;
    
    for (int i = 0; format_handlers[i].name != NULL; i++) {
        if (format_handlers[i].detect) {
            int score = format_handlers[i].detect(data, size);
            if (score > best_score) {
                best_score = score;
                best_handler = &format_handlers[i];
            }
        }
    }
    
    if (!best_handler) {
        set_error(disk, "Unknown disk format");
        return -1;
    }
    
    /* Copy data */
    disk->data = malloc(size);
    if (!disk->data) {
        set_error(disk, "Memory allocation failed");
        return -1;
    }
    
    memcpy(disk->data, data, size);
    disk->data_size = size;
    disk->owns_data = true;
    disk->format = best_handler->format;
    disk->handler = best_handler;
    
    /* Initialize format */
    if (best_handler->open) {
        if (best_handler->open(disk, data, size) != 0) {
            free(disk->data);
            disk->data = NULL;
            set_error(disk, "Failed to open disk");
            return -1;
        }
    }
    
    return 0;
}

int uft_disk_open_file(uft_disk_t *disk, const char *filename)
{
    if (!disk || !filename) return -1;
    
    FILE *f = fopen(filename, "rb");
    if (!f) {
        set_error(disk, "Cannot open file");
        return -1;
    }
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        set_error(disk, "Memory allocation failed");
        return -1;
    }
    
    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        set_error(disk, "Read error");
        return -1;
    }
    
    fclose(f);
    
    int result = uft_disk_open(disk, data, size);
    free(data);
    
    return result;
}

int uft_disk_save(uft_disk_t *disk, const char *filename)
{
    if (!disk || !filename || !disk->data) return -1;
    
    FILE *f = fopen(filename, "wb");
    if (!f) {
        set_error(disk, "Cannot create file");
        return -1;
    }
    
    if (fwrite(disk->data, 1, disk->data_size, f) != disk->data_size) {
        fclose(f);
        set_error(disk, "Write error");
        return -1;
    }
    
    fclose(f);
    disk->modified = false;
    
    return 0;
}

/*===========================================================================
 * Geometry
 *===========================================================================*/

int uft_disk_get_geometry(uft_disk_t *disk, uft_disk_geometry_t *geometry)
{
    if (!disk || !geometry) return -1;
    
    *geometry = disk->geometry;
    return 0;
}

int uft_disk_set_geometry(uft_disk_t *disk, const uft_disk_geometry_t *geometry)
{
    if (!disk || !geometry) return -1;
    
    disk->geometry = *geometry;
    return 0;
}

/*===========================================================================
 * Sector Operations
 *===========================================================================*/

int uft_disk_read_sector(uft_disk_t *disk, int track, int side, int sector,
                         uint8_t *buffer, size_t size)
{
    if (!disk || !buffer) return -1;
    
    if (disk->handler && disk->handler->read_sector) {
        return disk->handler->read_sector(disk, track, side, sector, buffer, size);
    }
    
    set_error(disk, "Read not supported");
    return -1;
}

int uft_disk_write_sector(uft_disk_t *disk, int track, int side, int sector,
                          const uint8_t *buffer, size_t size)
{
    if (!disk || !buffer) return -1;
    
    if (disk->handler && disk->handler->write_sector) {
        return disk->handler->write_sector(disk, track, side, sector, buffer, size);
    }
    
    set_error(disk, "Write not supported");
    return -1;
}

int uft_disk_read_track(uft_disk_t *disk, int track, int side,
                        uint8_t *buffer, size_t size)
{
    if (!disk || !buffer) return -1;
    
    int sec_size = disk->geometry.sector_size;
    int sectors = disk->geometry.sectors;
    size_t track_size = sectors * sec_size;
    
    if (size < track_size) return -1;
    
    for (int s = 1; s <= sectors; s++) {
        int result = uft_disk_read_sector(disk, track, side, s,
                                          buffer + (s - 1) * sec_size, sec_size);
        if (result < 0) return result;
    }
    
    return track_size;
}

/*===========================================================================
 * Disk Creation
 *===========================================================================*/

int uft_disk_create_blank(uft_disk_t *disk, const uft_disk_geometry_t *geometry)
{
    if (!disk || !geometry) return -1;
    
    size_t size = geometry->tracks * geometry->sides * 
                  geometry->sectors * geometry->sector_size;
    
    disk->data = calloc(1, size);
    if (!disk->data) {
        set_error(disk, "Memory allocation failed");
        return -1;
    }
    
    disk->data_size = size;
    disk->owns_data = true;
    disk->geometry = *geometry;
    disk->geometry.total_size = size;
    disk->format = UFT_FORMAT_RAW;
    disk->handler = &format_handlers[0];  /* Raw format */
    
    return 0;
}

int uft_disk_format(uft_disk_t *disk, uint8_t fill_byte)
{
    if (!disk || !disk->data) return -1;
    
    memset(disk->data, fill_byte, disk->data_size);
    disk->modified = true;
    
    return 0;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

uft_disk_format_t uft_disk_get_format(uft_disk_t *disk)
{
    return disk ? disk->format : UFT_FORMAT_UNKNOWN;
}

const char *uft_disk_format_name(uft_disk_format_t format)
{
    for (int i = 0; format_handlers[i].name != NULL; i++) {
        if (format_handlers[i].format == format) {
            return format_handlers[i].name;
        }
    }
    return "Unknown";
}

bool uft_disk_is_modified(uft_disk_t *disk)
{
    return disk ? disk->modified : false;
}

size_t uft_disk_get_size(uft_disk_t *disk)
{
    return disk ? disk->data_size : 0;
}

const uint8_t *uft_disk_get_data(uft_disk_t *disk)
{
    return disk ? disk->data : NULL;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_disk_info_json(uft_disk_t *disk, char *buffer, size_t size)
{
    if (!disk || !buffer) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"tracks\": %d,\n"
        "  \"sides\": %d,\n"
        "  \"sectors_per_track\": %d,\n"
        "  \"sector_size\": %d,\n"
        "  \"total_size\": %zu,\n"
        "  \"modified\": %s\n"
        "}",
        uft_disk_format_name(disk->format),
        disk->geometry.tracks,
        disk->geometry.sides,
        disk->geometry.sectors,
        disk->geometry.sector_size,
        disk->data_size,
        disk->modified ? "true" : "false"
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
