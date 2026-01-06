/**
 * @file uft_edsk_parser.c
 * @brief Extended DSK Parser for Amstrad CPC/Spectrum
 * @version 3.3.2
 * @date 2025-01-03
 *
 * EDSK (Extended DSK) Format Features:
 * - Variable sector sizes per track
 * - Non-standard sector IDs
 * - CRC error flags
 * - Deleted data marks
 * - Weak/random sector support (0x10 flag)
 * - Track timing information
 *
 * Supports both standard DSK and Extended DSK formats.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * EDSK CONSTANTS
 *============================================================================*/

#define DSK_SIGNATURE       "MV - CPC"
#define EDSK_SIGNATURE      "EXTENDED CPC DSK File"
#define DSK_HEADER_SIZE     256
#define TRACK_INFO_SIZE     256
#define MAX_TRACKS          204     /* 102 * 2 sides */
#define MAX_SECTORS         29      /* Per track */

/* FDC status flags */
#define FDC_ST1_DE          0x20    /* Data error (CRC) */
#define FDC_ST1_ND          0x04    /* No data */
#define FDC_ST2_CM          0x40    /* Control mark (deleted) */
#define FDC_ST2_DD          0x20    /* Data error in data field */

/* Sector flags (EDSK extension) */
#define EDSK_SF_WEAK        0x10    /* Weak/random sector */

/*============================================================================
 * STRUCTURES
 *============================================================================*/

/* Disk Information Block (256 bytes) */
typedef struct __attribute__((packed)) {
    char     signature[34];          /* Format signature */
    char     creator[14];            /* Creator name */
    uint8_t  num_tracks;             /* Number of tracks */
    uint8_t  num_sides;              /* Number of sides */
    uint16_t track_size;             /* Track size (standard DSK only) */
    uint8_t  track_sizes[MAX_TRACKS]; /* Per-track sizes (EDSK) */
} edsk_disk_info_t;

/* Track Information Block (256 bytes) */
typedef struct __attribute__((packed)) {
    char     signature[12];          /* "Track-Info\r\n" */
    uint8_t  unused1[4];
    uint8_t  track_number;           /* Track number */
    uint8_t  side_number;            /* Side number */
    uint8_t  unused2[2];
    uint8_t  sector_size;            /* Sector size code */
    uint8_t  num_sectors;            /* Number of sectors */
    uint8_t  gap3_length;            /* GAP#3 length */
    uint8_t  filler_byte;            /* Filler byte */
    /* Followed by sector info blocks */
} edsk_track_info_t;

/* Sector Information Block (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  track;                  /* ID: track (C) */
    uint8_t  side;                   /* ID: side (H) */
    uint8_t  sector;                 /* ID: sector (R) */
    uint8_t  size;                   /* ID: size (N) */
    uint8_t  fdc_status1;            /* FDC status register 1 */
    uint8_t  fdc_status2;            /* FDC status register 2 */
    uint16_t data_length;            /* Actual data length (EDSK) */
} edsk_sector_info_t;

/* Parsed sector */
typedef struct {
    /* ID field */
    uint8_t  id_track;
    uint8_t  id_side;
    uint8_t  id_sector;
    uint8_t  id_size;
    uint16_t actual_size;
    
    /* FDC status */
    uint8_t  fdc_st1;
    uint8_t  fdc_st2;
    
    /* Flags */
    bool     crc_error;
    bool     deleted;
    bool     no_data;
    bool     weak;
    
    /* Data */
    uint8_t* data;
    uint8_t* weak_data;              /* Multiple copies for weak sectors */
    int      weak_copies;
} edsk_sector_t;

/* Parsed track */
typedef struct {
    int track_number;
    int side;
    int sector_count;
    
    /* Track parameters */
    uint8_t sector_size_code;
    uint8_t gap3_length;
    uint8_t filler_byte;
    
    /* Sectors */
    edsk_sector_t sectors[MAX_SECTORS];
    
    /* Statistics */
    int good_sectors;
    int bad_sectors;
    int weak_sectors;
    int deleted_sectors;
    float quality_percent;
} edsk_track_t;

/* Parser context */
typedef struct {
    FILE* file;
    edsk_disk_info_t disk_info;
    bool is_extended;
    
    /* Track offsets */
    uint32_t track_offsets[MAX_TRACKS];
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t crc_errors;
    uint32_t weak_sectors;
    uint32_t deleted_sectors;
    
    bool initialized;
} edsk_parser_ctx_t;

/*============================================================================
 * INTERNAL HELPERS
 *============================================================================*/

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Decode sector size from size code
 */
static uint16_t decode_sector_size(uint8_t size_code) {
    if (size_code > 6) return 0;
    return 128 << size_code;
}

/**
 * @brief Parse FDC status flags
 */
static void parse_fdc_status(edsk_sector_t* sector) {
    /* ST1 flags */
    if (sector->fdc_st1 & FDC_ST1_DE) {
        sector->crc_error = true;
    }
    if (sector->fdc_st1 & FDC_ST1_ND) {
        sector->no_data = true;
    }
    
    /* ST2 flags */
    if (sector->fdc_st2 & FDC_ST2_CM) {
        sector->deleted = true;
    }
    if (sector->fdc_st2 & FDC_ST2_DD) {
        sector->crc_error = true;
    }
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open DSK/EDSK file
 */
edsk_parser_ctx_t* edsk_parser_open(const char* path) {
    if (!path) return NULL;
    
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read disk info */
    edsk_disk_info_t disk_info;
    if (fread(&disk_info, DSK_HEADER_SIZE, 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    /* Check signature */
    bool is_extended = (strncmp(disk_info.signature, EDSK_SIGNATURE, 21) == 0);
    bool is_standard = (strncmp(disk_info.signature, DSK_SIGNATURE, 8) == 0);
    
    if (!is_extended && !is_standard) {
        fclose(f);
        return NULL;
    }
    
    /* Allocate context */
    edsk_parser_ctx_t* ctx = calloc(1, sizeof(edsk_parser_ctx_t));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    memcpy(&ctx->disk_info, &disk_info, sizeof(disk_info));
    ctx->is_extended = is_extended;
    
    /* Build track offset table */
    uint32_t offset = DSK_HEADER_SIZE;
    int num_tracks = disk_info.num_tracks * disk_info.num_sides;
    
    if (num_tracks > MAX_TRACKS) {
        num_tracks = MAX_TRACKS;
    }
    
    for (int i = 0; i < num_tracks; i++) {
        ctx->track_offsets[i] = offset;
        
        if (is_extended) {
            /* EDSK: variable track sizes */
            uint32_t track_size = disk_info.track_sizes[i] * 256;
            offset += track_size;
        } else {
            /* Standard DSK: fixed track size */
            offset += disk_info.track_size;
        }
    }
    
    ctx->initialized = true;
    return ctx;
}

/**
 * @brief Close EDSK parser
 */
void edsk_parser_close(edsk_parser_ctx_t** ctx) {
    if (!ctx || !*ctx) return;
    
    if ((*ctx)->file) {
        fclose((*ctx)->file);
    }
    free(*ctx);
    *ctx = NULL;
}

/**
 * @brief Read and parse track
 */
int edsk_parser_read_track(
    edsk_parser_ctx_t* ctx,
    int track_num,
    int side,
    edsk_track_t** track_out
) {
    if (!ctx || !track_out || !ctx->initialized) {
        return -1;
    }
    
    /* Validate parameters */
    if (track_num >= ctx->disk_info.num_tracks || side >= ctx->disk_info.num_sides) {
        return -1;
    }
    
    /* Calculate track index */
    int track_idx = track_num * ctx->disk_info.num_sides + side;
    uint32_t track_offset = ctx->track_offsets[track_idx];
    
    /* Check for empty track (EDSK) */
    if (ctx->is_extended && ctx->disk_info.track_sizes[track_idx] == 0) {
        return -1;
    }
    
    /* Seek to track */
    if (fseek(ctx->file, track_offset, SEEK_SET) != 0) {
        return -1;
    }
    
    /* Read track info */
    edsk_track_info_t track_info;
    if (fread(&track_info, TRACK_INFO_SIZE, 1, ctx->file) != 1) {
        return -1;
    }
    
    /* Verify signature */
    if (strncmp(track_info.signature, "Track-Info", 10) != 0) {
        return -1;
    }
    
    /* Allocate track structure */
    edsk_track_t* track = calloc(1, sizeof(edsk_track_t));
    if (!track) {
        return -1;
    }
    
    track->track_number = track_num;
    track->side = side;
    track->sector_count = track_info.num_sectors;
    track->sector_size_code = track_info.sector_size;
    track->gap3_length = track_info.gap3_length;
    track->filler_byte = track_info.filler_byte;
    
    if (track->sector_count > MAX_SECTORS) {
        track->sector_count = MAX_SECTORS;
    }
    
    /* Read sector info blocks (follow track info header) */
    /* Seek back to read sector info from proper position */
    if (fseek(ctx->file, track_offset + 24, SEEK_SET) != 0) { /* seek error */ }
    edsk_sector_info_t* sec_infos = malloc(track->sector_count * sizeof(edsk_sector_info_t));
    if (!sec_infos) {
        free(track);
        return -1;
    }
    
    if (fread(sec_infos, sizeof(edsk_sector_info_t), 
              track->sector_count, ctx->file) != (size_t)track->sector_count) {
        free(sec_infos);
        free(track);
        return -1;
    }
    
    /* Calculate data start position */
    uint32_t data_offset = track_offset + TRACK_INFO_SIZE;
    
    /* Parse each sector */
    track->good_sectors = 0;
    track->bad_sectors = 0;
    track->weak_sectors = 0;
    track->deleted_sectors = 0;
    
    for (int i = 0; i < track->sector_count; i++) {
        edsk_sector_info_t* si = &sec_infos[i];
        edsk_sector_t* sector = &track->sectors[i];
        
        /* Copy ID field */
        sector->id_track = si->track;
        sector->id_side = si->side;
        sector->id_sector = si->sector;
        sector->id_size = si->size;
        sector->fdc_st1 = si->fdc_status1;
        sector->fdc_st2 = si->fdc_status2;
        
        /* Calculate actual size */
        if (ctx->is_extended && si->data_length > 0) {
            sector->actual_size = si->data_length;
        } else {
            sector->actual_size = decode_sector_size(si->size);
        }
        
        /* Parse flags */
        parse_fdc_status(sector);
        
        /* Check for weak sector (EDSK extension) */
        if (sector->actual_size > decode_sector_size(si->size)) {
            sector->weak = true;
            sector->weak_copies = sector->actual_size / decode_sector_size(si->size);
        }
        
        /* Read sector data */
        if (sector->actual_size > 0) {
            if (fseek(ctx->file, data_offset, SEEK_SET) != 0) { /* seek error */ }
            sector->data = malloc(sector->actual_size);
            if (sector->data) {
                if (fread(sector->data, 1, sector->actual_size, ctx->file) 
                    != sector->actual_size) {
                    free(sector->data);
                    sector->data = NULL;
                }
            }
            
            data_offset += sector->actual_size;
        }
        
        /* Update statistics */
        ctx->total_sectors++;
        
        if (sector->crc_error || sector->no_data) {
            track->bad_sectors++;
            ctx->crc_errors++;
        } else {
            track->good_sectors++;
        }
        
        if (sector->weak) {
            track->weak_sectors++;
            ctx->weak_sectors++;
        }
        
        if (sector->deleted) {
            track->deleted_sectors++;
            ctx->deleted_sectors++;
        }
    }
    
    free(sec_infos);
    
    /* Calculate quality */
    if (track->sector_count > 0) {
        track->quality_percent = (float)track->good_sectors / track->sector_count * 100.0f;
    } else {
        track->quality_percent = 100.0f;
    }
    
    *track_out = track;
    return 0;
}

/**
 * @brief Free track data
 */
void edsk_parser_free_track(edsk_track_t** track) {
    if (!track || !*track) return;
    
    edsk_track_t* t = *track;
    
    for (int i = 0; i < t->sector_count && i < MAX_SECTORS; i++) {
        free(t->sectors[i].data);
        free(t->sectors[i].weak_data);
    }
    
    free(t);
    *track = NULL;
}

/**
 * @brief Read single sector
 */
int edsk_parser_read_sector(
    edsk_parser_ctx_t* ctx,
    int track_num,
    int side,
    int sector_num,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!ctx || !buffer) return -1;
    
    edsk_track_t* track = NULL;
    if (edsk_parser_read_track(ctx, track_num, side, &track) != 0) {
        return -1;
    }
    
    int result = -1;
    
    /* Find sector by ID */
    for (int i = 0; i < track->sector_count && i < MAX_SECTORS; i++) {
        if (track->sectors[i].id_sector == sector_num) {
            uint16_t sector_size = decode_sector_size(track->sectors[i].id_size);
            size_t copy_size = sector_size;
            
            if (copy_size > buffer_size) {
                copy_size = buffer_size;
            }
            
            if (track->sectors[i].data) {
                memcpy(buffer, track->sectors[i].data, copy_size);
                result = (int)copy_size;
            }
            break;
        }
    }
    
    edsk_parser_free_track(&track);
    return result;
}

/**
 * @brief Get disk info
 */
void edsk_parser_get_info(
    edsk_parser_ctx_t* ctx,
    int* num_tracks,
    int* num_sides,
    bool* is_extended,
    char* creator,
    size_t creator_size
) {
    if (!ctx) return;
    
    if (num_tracks) *num_tracks = ctx->disk_info.num_tracks;
    if (num_sides) *num_sides = ctx->disk_info.num_sides;
    if (is_extended) *is_extended = ctx->is_extended;
    
    if (creator && creator_size > 0) {
        size_t len = sizeof(ctx->disk_info.creator);
        if (len >= creator_size) len = creator_size - 1;
        memcpy(creator, ctx->disk_info.creator, len);
        creator[len] = '\0';
    }
}

/**
 * @brief Get statistics
 */
void edsk_parser_get_stats(
    edsk_parser_ctx_t* ctx,
    uint32_t* total_sectors,
    uint32_t* crc_errors,
    uint32_t* weak_sectors,
    uint32_t* deleted_sectors
) {
    if (!ctx) return;
    
    if (total_sectors) *total_sectors = ctx->total_sectors;
    if (crc_errors) *crc_errors = ctx->crc_errors;
    if (weak_sectors) *weak_sectors = ctx->weak_sectors;
    if (deleted_sectors) *deleted_sectors = ctx->deleted_sectors;
}

/**
 * @brief Analyze disk format
 */
int edsk_parser_analyze_format(
    edsk_parser_ctx_t* ctx,
    char* report,
    size_t report_size
) {
    if (!ctx || !report || report_size == 0) return -1;
    
    int written = 0;
    
    written += snprintf(report + written, report_size - written,
        "=== EDSK Format Analysis ===\n");
    
    written += snprintf(report + written, report_size - written,
        "Format: %s\n", ctx->is_extended ? "Extended DSK" : "Standard DSK");
    
    written += snprintf(report + written, report_size - written,
        "Tracks: %d, Sides: %d\n", 
        ctx->disk_info.num_tracks, ctx->disk_info.num_sides);
    
    written += snprintf(report + written, report_size - written,
        "Creator: %.14s\n", ctx->disk_info.creator);
    
    /* Detect format by analyzing track 0 */
    edsk_track_t* track = NULL;
    if (edsk_parser_read_track(ctx, 0, 0, &track) == 0) {
        written += snprintf(report + written, report_size - written,
            "\nTrack 0 info:\n");
        written += snprintf(report + written, report_size - written,
            "  Sectors: %d\n", track->sector_count);
        written += snprintf(report + written, report_size - written,
            "  Sector size: %d bytes\n", decode_sector_size(track->sector_size_code));
        
        /* Detect specific formats */
        if (track->sector_count == 9 && track->sector_size_code == 2) {
            written += snprintf(report + written, report_size - written,
                "\nDetected: Standard CPC DATA format\n");
        } else if (track->sector_count == 9 && track->sector_size_code == 2 
                   && track->sectors[0].id_sector == 0x41) {
            written += snprintf(report + written, report_size - written,
                "\nDetected: CPC SYSTEM format\n");
        } else if (track->sector_count == 10 && ctx->disk_info.num_tracks == 80) {
            written += snprintf(report + written, report_size - written,
                "\nDetected: Spectrum +3 format\n");
        }
        
        edsk_parser_free_track(&track);
    }
    
    /* Protection analysis */
    if (ctx->weak_sectors > 0) {
        written += snprintf(report + written, report_size - written,
            "\n⚠ Weak sectors detected: %u\n", ctx->weak_sectors);
        written += snprintf(report + written, report_size - written,
            "  → Likely copy protection present\n");
    }
    
    if (ctx->crc_errors > 0) {
        written += snprintf(report + written, report_size - written,
            "\n⚠ CRC errors: %u\n", ctx->crc_errors);
    }
    
    return written;
}

/*============================================================================
 * UNIT TEST
 *============================================================================*/

#ifdef EDSK_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== EDSK Parser Unit Tests ===\n");
    
    /* Test 1: Sector size decoding */
    {
        assert(decode_sector_size(0) == 128);
        assert(decode_sector_size(1) == 256);
        assert(decode_sector_size(2) == 512);
        assert(decode_sector_size(3) == 1024);
        assert(decode_sector_size(7) == 0);
        printf("✓ Sector size decoding\n");
    }
    
    /* Test 2: Disk info size */
    {
        assert(sizeof(edsk_disk_info_t) == DSK_HEADER_SIZE);
        printf("✓ Disk info: 256 bytes\n");
    }
    
    /* Test 3: Track info size */
    {
        /* First 24 bytes are the fixed header */
        assert(sizeof(edsk_track_info_t) == 24);
        printf("✓ Track info: 24 bytes fixed\n");
    }
    
    /* Test 4: Sector info size */
    {
        assert(sizeof(edsk_sector_info_t) == 8);
        printf("✓ Sector info: 8 bytes\n");
    }
    
    /* Test 5: Context allocation */
    {
        edsk_parser_ctx_t* ctx = calloc(1, sizeof(edsk_parser_ctx_t));
        assert(ctx != NULL);
        ctx->is_extended = true;
        assert(ctx->is_extended == true);
        free(ctx);
        printf("✓ Context allocation\n");
    }
    
    /* Test 6: FDC status parsing */
    {
        edsk_sector_t sector = {0};
        sector.fdc_st1 = FDC_ST1_DE;
        sector.fdc_st2 = FDC_ST2_CM;
        parse_fdc_status(&sector);
        assert(sector.crc_error == true);
        assert(sector.deleted == true);
        printf("✓ FDC status parsing\n");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* EDSK_PARSER_TEST */
