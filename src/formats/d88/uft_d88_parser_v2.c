/**
 * @file uft_d88_parser_v2.c
 * @brief GOD MODE D88/D77 Parser v2
 * 
 * Advanced D88 parser for Japanese retro computers:
 * - NEC PC-88/PC-98 series
 * - Sharp X1/X68000
 * - Fujitsu FM Towns
 * - MSX computers
 * 
 * Features:
 * - D88 and D77 format support
 * - Multi-disk archive support
 * - Track offset table parsing
 * - Sector density detection (FM/MFM)
 * - Deleted/error sector handling
 * - Write protection detection
 * - Media type identification
 * 
 * @author GOD MODE v5.3.8
 * @date 2026-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * D88 FORMAT CONSTANTS
 *============================================================================*/

/* D88 header size */
#define D88_HEADER_SIZE     0x2B0   /* 688 bytes */
#define D88_NAME_SIZE       17      /* Disk name field */
#define D88_TRACK_TABLE     164     /* Track offset table entries */

/* D88 media types */
#define D88_MEDIA_2D        0x00    /* 2D (320KB) */
#define D88_MEDIA_2DD       0x10    /* 2DD (640KB/720KB) */
#define D88_MEDIA_2HD       0x20    /* 2HD (1.2MB/1.44MB) */
#define D88_MEDIA_1D        0x30    /* 1D (160KB) */
#define D88_MEDIA_1DD       0x40    /* 1DD (320KB/360KB) */

/* D88 write protect flag */
#define D88_WP_OFF          0x00    /* Not write protected */
#define D88_WP_ON           0x10    /* Write protected */

/* D88 sector density */
#define D88_DENS_DOUBLE     0x00    /* MFM (double density) */
#define D88_DENS_SINGLE     0x40    /* FM (single density) */
#define D88_DENS_HIGH       0x01    /* High density (1.2MB/1.44MB) */

/* D88 sector status */
#define D88_STAT_NORMAL     0x00    /* Normal sector */
#define D88_STAT_DELETED    0x10    /* Deleted data mark */
#define D88_STAT_CRC_ERROR  0xA0    /* CRC error in ID */
#define D88_STAT_DATA_ERROR 0xB0    /* CRC error in data */
#define D88_STAT_ADDR_ERROR 0xE0    /* Address mark error */
#define D88_STAT_NO_DAM     0xF0    /* No data address mark */

/* Maximum limits */
#define D88_MAX_TRACKS      164
#define D88_MAX_SECTORS     64
#define D88_MAX_SECTOR_SIZE 16384
#define D88_MAX_DISKS       16

#pragma pack(push, 1)

/* D88 file header (688 bytes) */
typedef struct {
    char     name[D88_NAME_SIZE];   /* Disk name (null-terminated) */
    uint8_t  reserved[9];           /* Reserved bytes */
    uint8_t  write_protect;         /* Write protect flag */
    uint8_t  media_type;            /* Media type */
    uint32_t disk_size;             /* Total disk size in bytes */
    uint32_t track_offset[D88_TRACK_TABLE];  /* Track offset table */
} d88_header_t;

/* D88 sector header (16 bytes) */
typedef struct {
    uint8_t  cylinder;              /* Sector ID cylinder */
    uint8_t  head;                  /* Sector ID head */
    uint8_t  sector;                /* Sector ID number */
    uint8_t  size_code;             /* Size code (0=128, 1=256...) */
    uint16_t sectors;               /* Number of sectors on track */
    uint8_t  density;               /* Density flags */
    uint8_t  deleted;               /* Deleted data mark */
    uint8_t  status;                /* FDC status */
    uint8_t  reserved[5];           /* Reserved */
    uint16_t data_size;             /* Actual data size */
} d88_sector_header_t;

#pragma pack(pop)

/*============================================================================
 * D88 INTERNAL STRUCTURES
 *============================================================================*/

/* Sector info */
typedef struct {
    d88_sector_header_t header;
    long     file_offset;           /* Offset to sector data */
    bool     valid;
} d88_sector_info_t;

/* Track info */
typedef struct {
    uint32_t offset;                /* File offset */
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector_count;
    uint16_t sector_size;
    bool     fm_encoding;           /* FM (single density) */
    d88_sector_info_t sectors[D88_MAX_SECTORS];
} d88_track_info_t;

/* Disk info */
typedef struct {
    d88_header_t header;
    long     file_offset;           /* Start of this disk in file */
    d88_track_info_t tracks[D88_MAX_TRACKS];
    int      track_count;
} d88_disk_info_t;

/* D88 context */
typedef struct {
    /* File info */
    FILE    *fp;
    char     filename[256];
    size_t   file_size;
    
    /* Disk info */
    d88_disk_info_t disks[D88_MAX_DISKS];
    int      disk_count;
    int      current_disk;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t error_sectors;
    uint32_t deleted_sectors;
} d88_context_t;

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get media type name
 */
static const char* d88_media_name(uint8_t type) {
    switch (type) {
        case D88_MEDIA_2D:  return "2D (320KB)";
        case D88_MEDIA_2DD: return "2DD (640KB/720KB)";
        case D88_MEDIA_2HD: return "2HD (1.2MB/1.44MB)";
        case D88_MEDIA_1D:  return "1D (160KB)";
        case D88_MEDIA_1DD: return "1DD (320KB/360KB)";
        default: return "Unknown";
    }
}

/**
 * @brief Get sector status name
 */
static const char* d88_status_name(uint8_t status) {
    switch (status) {
        case D88_STAT_NORMAL:     return "Normal";
        case D88_STAT_DELETED:    return "Deleted";
        case D88_STAT_CRC_ERROR:  return "ID CRC Error";
        case D88_STAT_DATA_ERROR: return "Data CRC Error";
        case D88_STAT_ADDR_ERROR: return "Address Error";
        case D88_STAT_NO_DAM:     return "No DAM";
        default: return "Unknown";
    }
}

/**
 * @brief Calculate sector size from size code
 */
static uint16_t d88_sector_size(uint8_t size_code) {
    if (size_code > 8) return 0;
    return (uint16_t)(128 << size_code);
}

/**
 * @brief Check if density is FM (single)
 */
static bool d88_is_fm(uint8_t density) {
    return (density & D88_DENS_SINGLE) != 0;
}

/**
 * @brief Check if density is high density
 */
static bool d88_is_hd(uint8_t density) {
    return (density & D88_DENS_HIGH) != 0;
}

/**
 * @brief Get expected geometry for media type
 */
static void d88_get_geometry(uint8_t media_type, int *tracks, int *sides, 
                              int *sectors, int *sector_size) {
    switch (media_type) {
        case D88_MEDIA_2D:
            *tracks = 40; *sides = 2; *sectors = 16; *sector_size = 256;
            break;
        case D88_MEDIA_2DD:
            *tracks = 80; *sides = 2; *sectors = 16; *sector_size = 256;
            break;
        case D88_MEDIA_2HD:
            *tracks = 77; *sides = 2; *sectors = 26; *sector_size = 256;  /* PC-98 */
            break;
        case D88_MEDIA_1D:
            *tracks = 40; *sides = 1; *sectors = 16; *sector_size = 256;
            break;
        case D88_MEDIA_1DD:
            *tracks = 40; *sides = 2; *sectors = 16; *sector_size = 256;
            break;
        default:
            *tracks = 80; *sides = 2; *sectors = 16; *sector_size = 256;
    }
}

/*============================================================================
 * D88 FILE OPERATIONS
 *============================================================================*/

/**
 * @brief Parse a single disk from D88 file
 */
static int d88_parse_disk(d88_context_t *ctx, int disk_idx, long start_offset) {
    d88_disk_info_t *disk = &ctx->disks[disk_idx];
    disk->file_offset = start_offset;
    
    fseek(ctx->fp, start_offset, SEEK_SET);
    
    /* Read header */
    if (fread(&disk->header, sizeof(d88_header_t), 1, ctx->fp) != 1) {
        return -1;
    }
    
    /* Validate disk size */
    if (disk->header.disk_size == 0 || 
        disk->header.disk_size > ctx->file_size - (size_t)start_offset) {
        return -1;
    }
    
    /* Count valid tracks */
    disk->track_count = 0;
    for (int t = 0; t < D88_TRACK_TABLE; t++) {
        if (disk->header.track_offset[t] != 0) {
            disk->track_count = t + 1;
        }
    }
    
    /* Parse each track */
    for (int t = 0; t < disk->track_count; t++) {
        if (disk->header.track_offset[t] == 0) continue;
        
        d88_track_info_t *track = &disk->tracks[t];
        track->offset = disk->header.track_offset[t];
        track->cylinder = (uint8_t)(t / 2);
        track->head = (uint8_t)(t % 2);
        
        long track_offset = start_offset + track->offset;
        fseek(ctx->fp, track_offset, SEEK_SET);
        
        /* Read first sector header to get sector count */
        d88_sector_header_t first_sec;
        if (fread(&first_sec, sizeof(first_sec), 1, ctx->fp) != 1) {
            continue;
        }
        
        track->sector_count = (uint8_t)first_sec.sectors;
        track->sector_size = d88_sector_size(first_sec.size_code);
        track->fm_encoding = d88_is_fm(first_sec.density);
        
        if (track->sector_count > D88_MAX_SECTORS) {
            track->sector_count = D88_MAX_SECTORS;
        }
        
        /* Seek back and read all sectors */
        fseek(ctx->fp, track_offset, SEEK_SET);
        
        for (int s = 0; s < track->sector_count; s++) {
            d88_sector_info_t *sec = &track->sectors[s];
            
            if (fread(&sec->header, sizeof(d88_sector_header_t), 1, ctx->fp) != 1) {
                break;
            }
            
            sec->file_offset = ftell(ctx->fp);
            sec->valid = true;
            
            /* Update statistics */
            ctx->total_sectors++;
            if (sec->header.status != D88_STAT_NORMAL) {
                ctx->error_sectors++;
            }
            if (sec->header.deleted) {
                ctx->deleted_sectors++;
            }
            
            /* Skip sector data */
            fseek(ctx->fp, sec->header.data_size, SEEK_CUR);
        }
    }
    
    return 0;
}

/**
 * @brief Open D88 file
 */
d88_context_t* d88_open(const char *filename) {
    if (!filename) return NULL;
    
    d88_context_t *ctx = calloc(1, sizeof(d88_context_t));
    if (!ctx) return NULL;
    
    strncpy(ctx->filename, filename, sizeof(ctx->filename) - 1);
    
    ctx->fp = fopen(filename, "rb");
    if (!ctx->fp) {
        free(ctx);
        return NULL;
    }
    
    /* Get file size */
    fseek(ctx->fp, 0, SEEK_END);
    ctx->file_size = (size_t)ftell(ctx->fp);
    fseek(ctx->fp, 0, SEEK_SET);
    
    if (ctx->file_size < sizeof(d88_header_t)) {
        fclose(ctx->fp);
        free(ctx);
        return NULL;
    }
    
    /* Parse disk(s) - D88 files can contain multiple disks */
    long offset = 0;
    ctx->disk_count = 0;
    
    while (offset < (long)ctx->file_size && ctx->disk_count < D88_MAX_DISKS) {
        /* Try to parse a disk at this offset */
        fseek(ctx->fp, offset, SEEK_SET);
        
        /* Peek at disk size */
        uint32_t disk_size;
        fseek(ctx->fp, offset + 0x1C, SEEK_SET);
        if (fread(&disk_size, 4, 1, ctx->fp) != 1) break;
        
        if (disk_size == 0 || disk_size > ctx->file_size - (size_t)offset) {
            break;
        }
        
        if (d88_parse_disk(ctx, ctx->disk_count, offset) == 0) {
            ctx->disk_count++;
            offset += disk_size;
        } else {
            break;
        }
    }
    
    if (ctx->disk_count == 0) {
        fclose(ctx->fp);
        free(ctx);
        return NULL;
    }
    
    ctx->current_disk = 0;
    
    return ctx;
}

/**
 * @brief Close D88 file
 */
void d88_close(d88_context_t *ctx) {
    if (!ctx) return;
    if (ctx->fp) fclose(ctx->fp);
    free(ctx);
}

/**
 * @brief Select disk in multi-disk archive
 */
int d88_select_disk(d88_context_t *ctx, int disk) {
    if (!ctx || disk < 0 || disk >= ctx->disk_count) return -1;
    ctx->current_disk = disk;
    return 0;
}

/**
 * @brief Find track in current disk
 */
static d88_track_info_t* d88_find_track(d88_context_t *ctx, uint8_t cyl, uint8_t head) {
    d88_disk_info_t *disk = &ctx->disks[ctx->current_disk];
    int track_idx = cyl * 2 + head;
    
    if (track_idx >= D88_TRACK_TABLE) return NULL;
    if (disk->header.track_offset[track_idx] == 0) return NULL;
    
    return &disk->tracks[track_idx];
}

/**
 * @brief Read sector data
 */
int d88_read_sector(d88_context_t *ctx, uint8_t cyl, uint8_t head,
                    uint8_t sector, uint8_t *buffer, size_t buf_size) {
    if (!ctx || !buffer) return -1;
    
    d88_disk_info_t *disk = &ctx->disks[ctx->current_disk];
    d88_track_info_t *track = d88_find_track(ctx, cyl, head);
    if (!track) return -1;
    
    /* Find sector */
    for (int s = 0; s < track->sector_count; s++) {
        d88_sector_info_t *sec = &track->sectors[s];
        
        if (sec->header.sector == sector &&
            sec->header.cylinder == cyl &&
            sec->header.head == head) {
            
            /* Seek to sector data */
            fseek(ctx->fp, disk->file_offset + track->offset + 
                  sizeof(d88_sector_header_t) * (s + 1) - 
                  sizeof(d88_sector_header_t) + sizeof(d88_sector_header_t), 
                  SEEK_SET);
            
            /* Actually, use stored offset */
            fseek(ctx->fp, sec->file_offset, SEEK_SET);
            
            size_t read_size = sec->header.data_size;
            if (read_size > buf_size) read_size = buf_size;
            
            if (fread(buffer, 1, read_size, ctx->fp) != read_size) {
                return -1;
            }
            
            return (int)read_size;
        }
    }
    
    return -1;  /* Sector not found */
}

/**
 * @brief Print D88 information
 */
void d88_print_info(d88_context_t *ctx) {
    if (!ctx) return;
    
    printf("=== D88 Image Info ===\n");
    printf("File: %s\n", ctx->filename);
    printf("Size: %zu bytes\n", ctx->file_size);
    printf("Disks: %d\n", ctx->disk_count);
    printf("\n");
    
    for (int d = 0; d < ctx->disk_count; d++) {
        d88_disk_info_t *disk = &ctx->disks[d];
        
        printf("Disk %d:\n", d + 1);
        printf("  Name: %.*s\n", D88_NAME_SIZE, disk->header.name);
        printf("  Size: %u bytes\n", disk->header.disk_size);
        printf("  Media: %s\n", d88_media_name(disk->header.media_type));
        printf("  Write Protect: %s\n", 
               disk->header.write_protect ? "Yes" : "No");
        printf("  Tracks: %d\n", disk->track_count);
        
        /* Analyze geometry */
        int max_cyl = 0, max_head = 0, max_sec = 0;
        uint16_t sec_size = 0;
        bool has_fm = false, has_mfm = false;
        
        for (int t = 0; t < disk->track_count; t++) {
            d88_track_info_t *track = &disk->tracks[t];
            if (track->sector_count == 0) continue;
            
            if (track->cylinder > max_cyl) max_cyl = track->cylinder;
            if (track->head > max_head) max_head = track->head;
            if (track->sector_count > max_sec) max_sec = track->sector_count;
            if (track->sector_size > sec_size) sec_size = track->sector_size;
            if (track->fm_encoding) has_fm = true;
            else has_mfm = true;
        }
        
        printf("  Cylinders: %d\n", max_cyl + 1);
        printf("  Sides: %d\n", max_head + 1);
        printf("  Sectors/Track: %d\n", max_sec);
        printf("  Sector Size: %d bytes\n", sec_size);
        printf("  Encoding: %s%s%s\n", 
               has_fm ? "FM" : "", 
               (has_fm && has_mfm) ? "/" : "",
               has_mfm ? "MFM" : "");
        printf("\n");
    }
    
    printf("Statistics:\n");
    printf("  Total Sectors: %u\n", ctx->total_sectors);
    printf("  Error Sectors: %u\n", ctx->error_sectors);
    printf("  Deleted Sectors: %u\n", ctx->deleted_sectors);
}

/**
 * @brief Convert D88 to raw sector image
 */
int d88_convert_to_raw(d88_context_t *ctx, const char *outfile) {
    if (!ctx || !outfile) return -1;
    
    d88_disk_info_t *disk = &ctx->disks[ctx->current_disk];
    
    FILE *out = fopen(outfile, "wb");
    if (!out) return -1;
    
    int result = 0;
    
    /* Determine geometry from track table */
    int exp_tracks, exp_sides, exp_sectors, exp_size;
    d88_get_geometry(disk->header.media_type, &exp_tracks, &exp_sides, 
                     &exp_sectors, &exp_size);
    
    uint8_t *sector_buf = malloc(D88_MAX_SECTOR_SIZE);
    if (!sector_buf) {
        fclose(out);
        return -1;
    }
    
    for (int cyl = 0; cyl < exp_tracks; cyl++) {
        for (int head = 0; head < exp_sides; head++) {
            d88_track_info_t *track = d88_find_track(ctx, (uint8_t)cyl, (uint8_t)head);
            int sectors = track ? track->sector_count : exp_sectors;
            int sec_size = track ? track->sector_size : exp_size;
            
            for (int sec = 1; sec <= sectors; sec++) {
                memset(sector_buf, 0xE5, sec_size);
                
                if (track) {
                    d88_read_sector(ctx, (uint8_t)cyl, (uint8_t)head,
                                   (uint8_t)sec, sector_buf, sec_size);
                }
                
                if (fwrite(sector_buf, sec_size, 1, out) != 1) {
                    result = -1;
                    goto cleanup;
                }
            }
        }
    }
    
cleanup:
    free(sector_buf);
    fclose(out);
    return result;
}

/*============================================================================
 * TEST SUITE
 *============================================================================*/

#ifdef D88_PARSER_TEST

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("Testing %s... ", name)
#define PASS() do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("✗ (%s)\n", msg); tests_failed++; } while(0)

static void test_media_names(void) {
    TEST("media_names");
    
    if (strcmp(d88_media_name(D88_MEDIA_2D), "2D (320KB)") != 0) {
        FAIL("2D");
        return;
    }
    if (strcmp(d88_media_name(D88_MEDIA_2DD), "2DD (640KB/720KB)") != 0) {
        FAIL("2DD");
        return;
    }
    if (strcmp(d88_media_name(D88_MEDIA_2HD), "2HD (1.2MB/1.44MB)") != 0) {
        FAIL("2HD");
        return;
    }
    
    PASS();
}

static void test_status_names(void) {
    TEST("status_names");
    
    if (strcmp(d88_status_name(D88_STAT_NORMAL), "Normal") != 0) {
        FAIL("Normal");
        return;
    }
    if (strcmp(d88_status_name(D88_STAT_DATA_ERROR), "Data CRC Error") != 0) {
        FAIL("Data Error");
        return;
    }
    
    PASS();
}

static void test_sector_sizes(void) {
    TEST("sector_sizes");
    
    if (d88_sector_size(0) != 128) { FAIL("Size 0"); return; }
    if (d88_sector_size(1) != 256) { FAIL("Size 1"); return; }
    if (d88_sector_size(2) != 512) { FAIL("Size 2"); return; }
    if (d88_sector_size(3) != 1024) { FAIL("Size 3"); return; }
    
    PASS();
}

static void test_density_detection(void) {
    TEST("density_detection");
    
    if (!d88_is_fm(D88_DENS_SINGLE)) {
        FAIL("FM not detected");
        return;
    }
    if (d88_is_fm(D88_DENS_DOUBLE)) {
        FAIL("MFM detected as FM");
        return;
    }
    if (!d88_is_hd(D88_DENS_HIGH)) {
        FAIL("HD not detected");
        return;
    }
    
    PASS();
}

static void test_geometry(void) {
    TEST("geometry");
    
    int tracks, sides, sectors, size;
    
    d88_get_geometry(D88_MEDIA_2D, &tracks, &sides, &sectors, &size);
    if (tracks != 40 || sides != 2 || sectors != 16 || size != 256) {
        FAIL("2D geometry");
        return;
    }
    
    d88_get_geometry(D88_MEDIA_2HD, &tracks, &sides, &sectors, &size);
    if (tracks != 77 || sides != 2 || sectors != 26) {
        FAIL("2HD geometry");
        return;
    }
    
    PASS();
}

int main(void) {
    printf("=== D88 Parser v2 Tests ===\n");
    
    test_media_names();
    test_status_names();
    test_sector_sizes();
    test_density_detection();
    test_geometry();
    
    printf("\n=== %s ===\n", tests_failed == 0 ? "All tests passed!" : "Some tests failed");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed;
}

#endif /* D88_PARSER_TEST */
