/**
 * @file uft_imd_parser_v2.c
 * @brief GOD MODE IMD (ImageDisk) Parser v2
 * 
 * Advanced ImageDisk parser with:
 * - Full header/comment parsing
 * - Track mode detection (FM/MFM)
 * - Data rate analysis
 * - Sector map extraction
 * - Cylinder/head map support
 * - Compression detection (unavailable/normal/compressed/deleted)
 * - Raw sector image conversion
 * 
 * ImageDisk was created by Dave Dunfield for CP/M disk preservation.
 * It stores full track geometry including interleave and skew.
 * 
 * @author GOD MODE v5.3.8
 * @date 2026-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/*============================================================================
 * IMD FORMAT CONSTANTS
 *============================================================================*/

/* IMD signature */
#define IMD_SIGNATURE      "IMD "

/* IMD version */
#define IMD_VERSION_1_17   0x0111  /* 1.17 */
#define IMD_VERSION_1_18   0x0112  /* 1.18 */

/* IMD track modes (bits 0-2: mode, bits 3-4: data rate indicator) */
#define IMD_MODE_FM_500    0x00    /* FM, 500 kbps */
#define IMD_MODE_FM_300    0x01    /* FM, 300 kbps */
#define IMD_MODE_FM_250    0x02    /* FM, 250 kbps */
#define IMD_MODE_MFM_500   0x03    /* MFM, 500 kbps */
#define IMD_MODE_MFM_300   0x04    /* MFM, 300 kbps */
#define IMD_MODE_MFM_250   0x05    /* MFM, 250 kbps */

/* IMD sector data types */
#define IMD_SEC_UNAVAIL    0x00    /* Sector data unavailable */
#define IMD_SEC_NORMAL     0x01    /* Normal data */
#define IMD_SEC_COMPRESS   0x02    /* Compressed (fill byte) */
#define IMD_SEC_DEL_NORM   0x03    /* Deleted, normal */
#define IMD_SEC_DEL_COMP   0x04    /* Deleted, compressed */
#define IMD_SEC_ERR_NORM   0x05    /* Error, normal */
#define IMD_SEC_ERR_COMP   0x06    /* Error, compressed */
#define IMD_SEC_DEL_ERR_N  0x07    /* Deleted+Error, normal */
#define IMD_SEC_DEL_ERR_C  0x08    /* Deleted+Error, compressed */

/* Flags in track header */
#define IMD_FLAG_CYL_MAP   0x80    /* Cylinder map present */
#define IMD_FLAG_HEAD_MAP  0x40    /* Head map present */

/* Maximum limits */
#define IMD_MAX_COMMENT    8192
#define IMD_MAX_TRACKS     166     /* 83 tracks * 2 sides */
#define IMD_MAX_SECTORS    64
#define IMD_MAX_SECTOR_SIZE 8192

/*============================================================================
 * IMD STRUCTURES
 *============================================================================*/

/* IMD track header */
typedef struct {
    uint8_t  mode;            /* Track mode (encoding + data rate) */
    uint8_t  cylinder;        /* Physical cylinder */
    uint8_t  head;            /* Physical head (+ flags in upper bits) */
    uint8_t  sectors;         /* Number of sectors */
    uint8_t  size_code;       /* Sector size code (0=128, 1=256...) */
} imd_track_header_t;

/* IMD sector info (internal) */
typedef struct {
    uint8_t  number;          /* Sector number */
    uint8_t  cylinder;        /* Sector ID cylinder (if cyl map) */
    uint8_t  head;            /* Sector ID head (if head map) */
    uint8_t  type;            /* Data type */
    uint16_t size;            /* Actual data size */
    bool     deleted;         /* Deleted data mark */
    bool     error;           /* Data error */
    bool     compressed;      /* Data is compressed */
    uint8_t  fill_byte;       /* Fill byte if compressed */
} imd_sector_info_t;

/* IMD track info (internal) */
typedef struct {
    imd_track_header_t header;
    bool     has_cyl_map;
    bool     has_head_map;
    uint16_t sector_size;
    imd_sector_info_t sectors[IMD_MAX_SECTORS];
    long     file_offset;     /* Offset in file */
} imd_track_info_t;

/* IMD context */
typedef struct {
    /* File info */
    FILE    *fp;
    char     filename[256];
    size_t   file_size;
    
    /* Header info */
    uint8_t  version_major;
    uint8_t  version_minor;
    struct tm creation_time;
    char     comment[IMD_MAX_COMMENT];
    size_t   comment_length;
    long     data_offset;     /* Start of track data */
    
    /* Geometry */
    uint8_t  tracks;
    uint8_t  sides;
    uint8_t  max_sectors;
    uint16_t max_sector_size;
    
    /* Track info */
    imd_track_info_t track_info[IMD_MAX_TRACKS];
    int      track_count;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t unavail_sectors;
    uint32_t error_sectors;
    uint32_t deleted_sectors;
    uint32_t compressed_sectors;
} imd_context_t;

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get mode name
 */
static const char* imd_mode_name(uint8_t mode) {
    switch (mode & 0x07) {
        case IMD_MODE_FM_500:  return "FM 500 kbps";
        case IMD_MODE_FM_300:  return "FM 300 kbps";
        case IMD_MODE_FM_250:  return "FM 250 kbps";
        case IMD_MODE_MFM_500: return "MFM 500 kbps";
        case IMD_MODE_MFM_300: return "MFM 300 kbps";
        case IMD_MODE_MFM_250: return "MFM 250 kbps";
        default: return "Unknown";
    }
}

/**
 * @brief Check if mode is MFM
 */
static bool imd_is_mfm(uint8_t mode) {
    return (mode & 0x07) >= IMD_MODE_MFM_500;
}

/**
 * @brief Get data rate in kbps
 */
static int imd_data_rate(uint8_t mode) {
    switch (mode & 0x07) {
        case IMD_MODE_FM_500:
        case IMD_MODE_MFM_500: return 500;
        case IMD_MODE_FM_300:
        case IMD_MODE_MFM_300: return 300;
        case IMD_MODE_FM_250:
        case IMD_MODE_MFM_250: return 250;
        default: return 0;
    }
}

/**
 * @brief Get sector type name
 */
static const char* imd_sector_type_name(uint8_t type) {
    switch (type) {
        case IMD_SEC_UNAVAIL:   return "Unavailable";
        case IMD_SEC_NORMAL:    return "Normal";
        case IMD_SEC_COMPRESS:  return "Compressed";
        case IMD_SEC_DEL_NORM:  return "Deleted";
        case IMD_SEC_DEL_COMP:  return "Deleted+Compressed";
        case IMD_SEC_ERR_NORM:  return "Error";
        case IMD_SEC_ERR_COMP:  return "Error+Compressed";
        case IMD_SEC_DEL_ERR_N: return "Deleted+Error";
        case IMD_SEC_DEL_ERR_C: return "Deleted+Error+Compressed";
        default: return "Unknown";
    }
}

/**
 * @brief Calculate sector size from size code
 */
static uint16_t imd_sector_size(uint8_t size_code) {
    if (size_code > 6) return 0;
    return (uint16_t)(128 << size_code);
}

/*============================================================================
 * IMD FILE OPERATIONS
 *============================================================================*/

/**
 * @brief Parse IMD header
 */
static int imd_parse_header(imd_context_t *ctx) {
    char header[32];
    
    /* Read signature line */
    if (fread(header, 1, 4, ctx->fp) != 4) return -1;
    
    if (memcmp(header, IMD_SIGNATURE, 4) != 0) {
        return -1;  /* Invalid signature */
    }
    
    /* Parse version and date from first line */
    /* Format: "IMD 1.18: DD/MM/YYYY HH:MM:SS" */
    char line[64];
    rewind(ctx->fp);
    if (!fgets(line, sizeof(line), ctx->fp)) return -1;
    
    int major = 0, minor = 0;
    int day = 0, month = 0, year = 0;
    int hour = 0, minute = 0, second = 0;
    
    if (sscanf(line, "IMD %d.%d: %d/%d/%d %d:%d:%d",
               &major, &minor, &day, &month, &year,
               &hour, &minute, &second) >= 2) {
        ctx->version_major = (uint8_t)major;
        ctx->version_minor = (uint8_t)minor;
        ctx->creation_time.tm_mday = day;
        ctx->creation_time.tm_mon = month - 1;
        ctx->creation_time.tm_year = year - 1900;
        ctx->creation_time.tm_hour = hour;
        ctx->creation_time.tm_min = minute;
        ctx->creation_time.tm_sec = second;
    }
    
    /* Read comment (until 0x1A terminator) */
    size_t comment_pos = 0;
    int c;
    while ((c = fgetc(ctx->fp)) != EOF && c != 0x1A) {
        if (comment_pos < IMD_MAX_COMMENT - 1) {
            ctx->comment[comment_pos++] = (char)c;
        }
    }
    ctx->comment[comment_pos] = '\0';
    ctx->comment_length = comment_pos;
    
    /* Record data start position */
    ctx->data_offset = ftell(ctx->fp);
    
    return 0;
}

/**
 * @brief Parse all tracks
 */
static int imd_parse_tracks(imd_context_t *ctx) {
    if (fseek(ctx->fp, ctx->data_offset, SEEK_SET) != 0) { /* seek error */ }
    ctx->track_count = 0;
    ctx->tracks = 0;
    ctx->sides = 0;
    ctx->max_sectors = 0;
    ctx->max_sector_size = 0;
    ctx->total_sectors = 0;
    ctx->unavail_sectors = 0;
    ctx->error_sectors = 0;
    ctx->deleted_sectors = 0;
    ctx->compressed_sectors = 0;
    
    while (ctx->track_count < IMD_MAX_TRACKS) {
        imd_track_info_t *track = &ctx->track_info[ctx->track_count];
        track->file_offset = ftell(ctx->fp);
        
        /* Read track header */
        if (fread(&track->header, sizeof(imd_track_header_t), 1, ctx->fp) != 1) {
            break;  /* End of file */
        }
        
        /* Parse flags */
        track->has_cyl_map = (track->header.head & IMD_FLAG_CYL_MAP) != 0;
        track->has_head_map = (track->header.head & IMD_FLAG_HEAD_MAP) != 0;
        track->header.head &= 0x0F;  /* Clear flags */
        
        track->sector_size = imd_sector_size(track->header.size_code);
        
        /* Update geometry stats */
        if (track->header.cylinder >= ctx->tracks) {
            ctx->tracks = track->header.cylinder + 1;
        }
        if (track->header.head >= ctx->sides) {
            ctx->sides = track->header.head + 1;
        }
        if (track->header.sectors > ctx->max_sectors) {
            ctx->max_sectors = track->header.sectors;
        }
        if (track->sector_size > ctx->max_sector_size) {
            ctx->max_sector_size = track->sector_size;
        }
        
        /* Read sector numbering map */
        uint8_t sec_map[IMD_MAX_SECTORS];
        if (fread(sec_map, 1, track->header.sectors, ctx->fp) != track->header.sectors) {
            return -1;
        }
        
        /* Read optional cylinder map */
        uint8_t cyl_map[IMD_MAX_SECTORS] = {0};
        if (track->has_cyl_map) {
            if (fread(cyl_map, 1, track->header.sectors, ctx->fp) != track->header.sectors) {
                return -1;
            }
        }
        
        /* Read optional head map */
        uint8_t head_map[IMD_MAX_SECTORS] = {0};
        if (track->has_head_map) {
            if (fread(head_map, 1, track->header.sectors, ctx->fp) != track->header.sectors) {
                return -1;
            }
        }
        
        /* Read sector data */
        for (int s = 0; s < track->header.sectors; s++) {
            imd_sector_info_t *sec = &track->sectors[s];
            
            sec->number = sec_map[s];
            sec->cylinder = track->has_cyl_map ? cyl_map[s] : track->header.cylinder;
            sec->head = track->has_head_map ? head_map[s] : track->header.head;
            sec->size = track->sector_size;
            
            /* Read sector type byte */
            uint8_t type;
            if (fread(&type, 1, 1, ctx->fp) != 1) return -1;
            sec->type = type;
            
            /* Parse type flags */
            sec->deleted = (type == IMD_SEC_DEL_NORM || type == IMD_SEC_DEL_COMP ||
                           type == IMD_SEC_DEL_ERR_N || type == IMD_SEC_DEL_ERR_C);
            sec->error = (type == IMD_SEC_ERR_NORM || type == IMD_SEC_ERR_COMP ||
                         type == IMD_SEC_DEL_ERR_N || type == IMD_SEC_DEL_ERR_C);
            sec->compressed = (type == IMD_SEC_COMPRESS || type == IMD_SEC_DEL_COMP ||
                              type == IMD_SEC_ERR_COMP || type == IMD_SEC_DEL_ERR_C);
            
            /* Update statistics */
            ctx->total_sectors++;
            if (type == IMD_SEC_UNAVAIL) ctx->unavail_sectors++;
            if (sec->error) ctx->error_sectors++;
            if (sec->deleted) ctx->deleted_sectors++;
            if (sec->compressed) ctx->compressed_sectors++;
            
            /* Skip sector data */
            if (type == IMD_SEC_UNAVAIL) {
                /* No data */
            } else if (sec->compressed) {
                /* Just fill byte */
                if (fread(&sec->fill_byte, 1, 1, ctx->fp) != 1) return -1;
            } else {
                /* Full sector data */
                if (fseek(ctx->fp, track->sector_size, SEEK_CUR) != 0) { /* seek error */ }
            }
        }
        
        ctx->track_count++;
    }
    
    return 0;
}

/**
 * @brief Open IMD file
 */
imd_context_t* imd_open(const char *filename) {
    if (!filename) return NULL;
    
    imd_context_t *ctx = calloc(1, sizeof(imd_context_t));
    if (!ctx) return NULL;
    
    strncpy(ctx->filename, filename, sizeof(ctx->filename) - 1);
    
    ctx->fp = fopen(filename, "rb");
    if (!ctx->fp) {
        free(ctx);
        return NULL;
    }
    
    /* Get file size */
    if (fseek(ctx->fp, 0, SEEK_END) != 0) { /* seek error */ }
    ctx->file_size = (size_t)ftell(ctx->fp);
    if (fseek(ctx->fp, 0, SEEK_SET) != 0) { /* seek error */ }
    /* Parse header */
    if (imd_parse_header(ctx) < 0) {
        fclose(ctx->fp);
        free(ctx);
        return NULL;
    }
    
    /* Parse tracks */
    if (imd_parse_tracks(ctx) < 0) {
        fclose(ctx->fp);
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

/**
 * @brief Close IMD file
 */
void imd_close(imd_context_t *ctx) {
    if (!ctx) return;
    if (ctx->fp) fclose(ctx->fp);
    free(ctx);
}

/**
 * @brief Find track by cylinder and head
 */
static imd_track_info_t* imd_find_track(imd_context_t *ctx, uint8_t cyl, uint8_t head) {
    for (int t = 0; t < ctx->track_count; t++) {
        imd_track_info_t *track = &ctx->track_info[t];
        if (track->header.cylinder == cyl && track->header.head == head) {
            return track;
        }
    }
    return NULL;
}

/**
 * @brief Read sector data
 */
int imd_read_sector(imd_context_t *ctx, uint8_t cyl, uint8_t head,
                    uint8_t sector, uint8_t *buffer, size_t buf_size) {
    if (!ctx || !buffer) return -1;
    
    imd_track_info_t *track = imd_find_track(ctx, cyl, head);
    if (!track) return -1;
    
    /* Seek to track start */
    if (fseek(ctx->fp, track->file_offset + sizeof(imd_track_header_t), SEEK_SET) != 0) { /* seek error */ }
    /* Skip sector map */
    if (fseek(ctx->fp, track->header.sectors, SEEK_CUR) != 0) { /* seek error */ }
    /* Skip optional maps */
    if (track->has_cyl_map) fseek(ctx->fp, track->header.sectors, SEEK_CUR);
    if (track->has_head_map) fseek(ctx->fp, track->header.sectors, SEEK_CUR);
    
    /* Find sector */
    for (int s = 0; s < track->header.sectors; s++) {
        imd_sector_info_t *sec = &track->sectors[s];
        
        /* Read type byte */
        uint8_t type;
        if (fread(&type, 1, 1, ctx->fp) != 1) return -1;
        
        if (sec->number == sector) {
            /* Found it */
            if (type == IMD_SEC_UNAVAIL) {
                return 0;  /* No data available */
            }
            
            size_t copy_size = track->sector_size < buf_size ? 
                              track->sector_size : buf_size;
            
            if (sec->compressed) {
                /* Fill with byte */
                uint8_t fill;
                if (fread(&fill, 1, 1, ctx->fp) != 1) return -1;
                memset(buffer, fill, copy_size);
            } else {
                /* Read data */
                if (fread(buffer, 1, copy_size, ctx->fp) != copy_size) {
                    return -1;
                }
            }
            
            return (int)copy_size;
        }
        
        /* Skip this sector's data */
        if (type == IMD_SEC_UNAVAIL) {
            /* No data */
        } else if (type == IMD_SEC_COMPRESS || type == IMD_SEC_DEL_COMP ||
                   type == IMD_SEC_ERR_COMP || type == IMD_SEC_DEL_ERR_C) {
            fseek(ctx->fp, 1, SEEK_CUR);  /* Fill byte only */
        } else {
            if (fseek(ctx->fp, track->sector_size, SEEK_CUR) != 0) { /* seek error */ }
        }
    }
    
    return -1;  /* Sector not found */
}

/**
 * @brief Detect interleave pattern
 */
int imd_detect_interleave(imd_context_t *ctx, uint8_t cyl, uint8_t head) {
    imd_track_info_t *track = imd_find_track(ctx, cyl, head);
    if (!track || track->header.sectors < 2) return 1;
    
    /* Calculate interleave from sector numbering */
    int first = track->sectors[0].number;
    int second = track->sectors[1].number;
    
    int interleave = second - first;
    if (interleave <= 0) {
        interleave += track->header.sectors;
    }
    
    return interleave;
}

/**
 * @brief Print IMD information
 */
void imd_print_info(imd_context_t *ctx) {
    if (!ctx) return;
    
    printf("=== IMD (ImageDisk) Image Info ===\n");
    printf("File: %s\n", ctx->filename);
    printf("Size: %zu bytes\n", ctx->file_size);
    printf("Version: %d.%d\n", ctx->version_major, ctx->version_minor);
    
    if (ctx->creation_time.tm_year > 0) {
        printf("Created: %04d-%02d-%02d %02d:%02d:%02d\n",
               ctx->creation_time.tm_year + 1900,
               ctx->creation_time.tm_mon + 1,
               ctx->creation_time.tm_mday,
               ctx->creation_time.tm_hour,
               ctx->creation_time.tm_min,
               ctx->creation_time.tm_sec);
    }
    printf("\n");
    
    if (ctx->comment_length > 0) {
        printf("Comment:\n%s\n\n", ctx->comment);
    }
    
    printf("Geometry:\n");
    printf("  Tracks: %d\n", ctx->tracks);
    printf("  Sides: %d\n", ctx->sides);
    printf("  Max Sectors/Track: %d\n", ctx->max_sectors);
    printf("  Max Sector Size: %d bytes\n", ctx->max_sector_size);
    printf("  Total Sectors: %u\n", ctx->total_sectors);
    printf("\n");
    
    printf("Sector Statistics:\n");
    printf("  Unavailable: %u\n", ctx->unavail_sectors);
    printf("  With Errors: %u\n", ctx->error_sectors);
    printf("  Deleted: %u\n", ctx->deleted_sectors);
    printf("  Compressed: %u\n", ctx->compressed_sectors);
    printf("\n");
    
    /* Show track modes */
    printf("Track Modes:\n");
    bool modes_shown[6] = {false};
    for (int t = 0; t < ctx->track_count; t++) {
        uint8_t mode = ctx->track_info[t].header.mode & 0x07;
        if (!modes_shown[mode]) {
            printf("  %s\n", imd_mode_name(mode));
            modes_shown[mode] = true;
        }
    }
}

/**
 * @brief Convert IMD to raw sector image
 */
int imd_convert_to_raw(imd_context_t *ctx, const char *outfile) {
    if (!ctx || !outfile) return -1;
    
    FILE *out = fopen(outfile, "wb");
    if (!out) return -1;
    
    int result = 0;
    uint8_t *sector_buf = malloc(ctx->max_sector_size);
    if (!sector_buf) {
        fclose(out);
        return -1;
    }
    
    /* Determine sectors per track (use first track) */
    uint8_t sectors_per_track = ctx->max_sectors;
    
    for (int cyl = 0; cyl < ctx->tracks; cyl++) {
        for (int head = 0; head < ctx->sides; head++) {
            imd_track_info_t *track = imd_find_track(ctx, (uint8_t)cyl, (uint8_t)head);
            uint16_t sec_size = track ? track->sector_size : ctx->max_sector_size;
            
            for (int sec = 1; sec <= sectors_per_track; sec++) {
                memset(sector_buf, 0xE5, sec_size);
                
                if (track) {
                    int read_len = imd_read_sector(ctx, (uint8_t)cyl, (uint8_t)head,
                                                    (uint8_t)sec, sector_buf, sec_size);
                    if (read_len < 0) {
                        fprintf(stderr, "Warning: Missing sector C%d H%d S%d\n", cyl, head, sec);
                    }
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

#ifdef IMD_PARSER_TEST

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("Testing %s... ", name)
#define PASS() do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("✗ (%s)\n", msg); tests_failed++; } while(0)

static void test_mode_names(void) {
    TEST("mode_names");
    
    if (strcmp(imd_mode_name(IMD_MODE_FM_500), "FM 500 kbps") != 0) {
        FAIL("FM 500");
        return;
    }
    if (strcmp(imd_mode_name(IMD_MODE_MFM_250), "MFM 250 kbps") != 0) {
        FAIL("MFM 250");
        return;
    }
    
    PASS();
}

static void test_data_rates(void) {
    TEST("data_rates");
    
    if (imd_data_rate(IMD_MODE_FM_500) != 500) {
        FAIL("FM 500 rate");
        return;
    }
    if (imd_data_rate(IMD_MODE_MFM_300) != 300) {
        FAIL("MFM 300 rate");
        return;
    }
    if (imd_data_rate(IMD_MODE_FM_250) != 250) {
        FAIL("FM 250 rate");
        return;
    }
    
    PASS();
}

static void test_sector_sizes(void) {
    TEST("sector_sizes");
    
    if (imd_sector_size(0) != 128) { FAIL("Size 0"); return; }
    if (imd_sector_size(1) != 256) { FAIL("Size 1"); return; }
    if (imd_sector_size(2) != 512) { FAIL("Size 2"); return; }
    if (imd_sector_size(3) != 1024) { FAIL("Size 3"); return; }
    if (imd_sector_size(4) != 2048) { FAIL("Size 4"); return; }
    if (imd_sector_size(5) != 4096) { FAIL("Size 5"); return; }
    if (imd_sector_size(6) != 8192) { FAIL("Size 6"); return; }
    
    PASS();
}

static void test_sector_types(void) {
    TEST("sector_types");
    
    if (strcmp(imd_sector_type_name(IMD_SEC_UNAVAIL), "Unavailable") != 0) {
        FAIL("Unavailable");
        return;
    }
    if (strcmp(imd_sector_type_name(IMD_SEC_NORMAL), "Normal") != 0) {
        FAIL("Normal");
        return;
    }
    if (strcmp(imd_sector_type_name(IMD_SEC_DEL_ERR_C), "Deleted+Error+Compressed") != 0) {
        FAIL("Del+Err+Comp");
        return;
    }
    
    PASS();
}

static void test_mfm_detection(void) {
    TEST("mfm_detection");
    
    if (imd_is_mfm(IMD_MODE_FM_500)) { FAIL("FM detected as MFM"); return; }
    if (imd_is_mfm(IMD_MODE_FM_300)) { FAIL("FM detected as MFM"); return; }
    if (imd_is_mfm(IMD_MODE_FM_250)) { FAIL("FM detected as MFM"); return; }
    if (!imd_is_mfm(IMD_MODE_MFM_500)) { FAIL("MFM not detected"); return; }
    if (!imd_is_mfm(IMD_MODE_MFM_300)) { FAIL("MFM not detected"); return; }
    if (!imd_is_mfm(IMD_MODE_MFM_250)) { FAIL("MFM not detected"); return; }
    
    PASS();
}

int main(void) {
    printf("=== IMD Parser v2 Tests ===\n");
    
    test_mode_names();
    test_data_rates();
    test_sector_sizes();
    test_sector_types();
    test_mfm_detection();
    
    printf("\n=== %s ===\n", tests_failed == 0 ? "All tests passed!" : "Some tests failed");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed;
}

#endif /* IMD_PARSER_TEST */
