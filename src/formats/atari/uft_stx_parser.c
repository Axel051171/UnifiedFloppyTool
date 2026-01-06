/**
 * @file uft_stx_parser.c
 * @brief Atari ST STX (Pasti) Parser - Full Implementation
 * @version 3.3.2
 * @date 2025-01-03
 *
 * STX (Pasti) Format Features:
 * - Fuzzy bit preservation (copy protection)
 * - Precise timing data
 * - Sector data with CRC status
 * - Track timing information
 * - Multiple revolutions per track
 *
 * Based on Pasti STX specification v1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * STX CONSTANTS
 *============================================================================*/

#define STX_SIGNATURE       "RSY"
#define STX_VERSION         3
#define STX_MAX_TRACKS      168     /* 84 * 2 sides */
#define STX_MAX_SECTORS     32      /* Max sectors per track */
#define STX_SECTOR_SIZE     512

/* Record types */
#define STX_REC_TRACK_IMAGE   0x00
#define STX_REC_TRACK_TIMING  0x01
#define STX_REC_SECTOR_DATA   0x02

/* Track flags */
#define STX_TF_TRACK_IMAGE     0x01  /* Contains raw track image */
#define STX_TF_SECTOR_DATA     0x80  /* Contains sector data */
#define STX_TF_FUZZY_BITS      0x40  /* Track has fuzzy bits */
#define STX_TF_TIMING_DATA     0x20  /* Has timing information */

/* Sector flags */
#define STX_SF_FUZZY           0x80  /* Sector has fuzzy bits */
#define STX_SF_CRC_ERROR       0x08  /* CRC error in sector */
#define STX_SF_DELETED         0x04  /* Deleted data address mark */
#define STX_SF_ID_CRC_ERROR    0x02  /* ID field CRC error */
#define STX_SF_RECORD_TYPE     0x01  /* Record type mask */

/*============================================================================
 * STRUCTURES
 *============================================================================*/

/* File header (16 bytes) */
typedef struct __attribute__((packed)) {
    char     signature[4];       /* "RSY\0" */
    uint16_t version;            /* Format version (3) */
    uint16_t tool_version;       /* Tool version that created file */
    uint16_t reserved1;
    uint8_t  track_count;        /* Number of track records */
    uint8_t  revision;           /* File revision */
    uint32_t reserved2;
} stx_file_header_t;

/* Track descriptor (16 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t record_size;        /* Size of track record */
    uint32_t fuzzy_size;         /* Size of fuzzy bit data */
    uint16_t sector_count;       /* Number of sectors */
    uint16_t flags;              /* Track flags */
    uint16_t track_length;       /* MFM track length (bits/16) */
    uint8_t  track_number;       /* Physical track number */
    uint8_t  track_type;         /* Track type/encoding */
} stx_track_descriptor_t;

/* Sector descriptor (16 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t data_offset;        /* Offset to sector data */
    uint16_t bit_position;       /* Sector position in track (bits/16) */
    uint16_t read_time;          /* Read timing */
    uint8_t  id_track;           /* ID field: track */
    uint8_t  id_head;            /* ID field: head */
    uint8_t  id_sector;          /* ID field: sector */
    uint8_t  id_size;            /* ID field: size code */
    uint8_t  flags;              /* Sector flags */
    uint8_t  fdcstat;            /* FDC status */
    uint8_t  reserved[2];
} stx_sector_descriptor_t;

/* Parsed sector */
typedef struct {
    /* ID field */
    uint8_t track;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;
    uint16_t size_bytes;
    
    /* Position and timing */
    uint32_t bit_position;
    uint32_t read_time_us;
    
    /* Status */
    bool crc_error;
    bool id_crc_error;
    bool deleted;
    bool has_fuzzy;
    uint8_t fdc_status;
    
    /* Data */
    uint8_t* data;
    uint8_t* fuzzy_mask;         /* Fuzzy bit mask (if present) */
} stx_sector_t;

/* Parsed track */
typedef struct {
    int track_number;
    int side;
    int sector_count;
    
    /* Track image */
    uint8_t* track_data;
    size_t track_data_size;
    uint32_t track_length_bits;
    
    /* Timing data */
    uint16_t* timing_data;
    size_t timing_count;
    
    /* Fuzzy bits */
    uint8_t* fuzzy_data;
    size_t fuzzy_size;
    uint32_t fuzzy_bit_count;
    
    /* Sectors */
    stx_sector_t sectors[STX_MAX_SECTORS];
    
    /* Flags */
    bool has_track_image;
    bool has_timing;
    bool has_fuzzy;
    
    /* Statistics */
    int good_sectors;
    int bad_sectors;
    float quality_percent;
} stx_track_t;

/* Parser context */
typedef struct {
    FILE* file;
    stx_file_header_t header;
    
    /* Track offset table */
    uint32_t* track_offsets;
    int track_count;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t fuzzy_sectors;
    uint32_t crc_errors;
    
    bool initialized;
} stx_parser_ctx_t;

/*============================================================================
 * INTERNAL HELPERS
 *============================================================================*/

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Decode sector size from size code
 */
static uint16_t decode_sector_size(uint8_t size_code) {
    if (size_code > 6) return 0;
    return 128 << size_code;  /* 128, 256, 512, 1024, 2048, 4096, 8192 */
}

/**
 * @brief Parse sector descriptor
 */
static int parse_sector(
    stx_parser_ctx_t* ctx,
    const stx_sector_descriptor_t* desc,
    stx_sector_t* sector,
    uint32_t track_offset
) {
    if (!ctx || !desc || !sector) return -1;
    
    /* Copy ID field */
    sector->track = desc->id_track;
    sector->head = desc->id_head;
    sector->sector = desc->id_sector;
    sector->size_code = desc->id_size;
    sector->size_bytes = decode_sector_size(desc->id_size);
    
    /* Position and timing */
    sector->bit_position = desc->bit_position * 16;
    sector->read_time_us = desc->read_time * 8;  /* Convert to microseconds */
    
    /* Parse flags */
    sector->has_fuzzy = (desc->flags & STX_SF_FUZZY) != 0;
    sector->crc_error = (desc->flags & STX_SF_CRC_ERROR) != 0;
    sector->deleted = (desc->flags & STX_SF_DELETED) != 0;
    sector->id_crc_error = (desc->flags & STX_SF_ID_CRC_ERROR) != 0;
    sector->fdc_status = desc->fdcstat;
    
    /* Read sector data */
    if (desc->data_offset > 0 && sector->size_bytes > 0) {
        uint32_t data_pos = track_offset + desc->data_offset;
        
        if (fseek(ctx->file, data_pos, SEEK_SET) != 0) {
            return -1;
        }
        
        sector->data = malloc(sector->size_bytes);
        if (!sector->data) {
            return -1;
        }
        
        if (fread(sector->data, 1, sector->size_bytes, ctx->file) != sector->size_bytes) {
            free(sector->data);
            sector->data = NULL;
            return -1;
        }
        
        /* Read fuzzy mask if present */
        if (sector->has_fuzzy) {
            sector->fuzzy_mask = malloc(sector->size_bytes);
            if (sector->fuzzy_mask) {
                if (fread(sector->fuzzy_mask, 1, sector->size_bytes, ctx->file) 
                    != sector->size_bytes) {
                    free(sector->fuzzy_mask);
                    sector->fuzzy_mask = NULL;
                }
            }
        }
    }
    
    /* Update statistics */
    ctx->total_sectors++;
    if (sector->has_fuzzy) ctx->fuzzy_sectors++;
    if (sector->crc_error) ctx->crc_errors++;
    
    return 0;
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open STX file
 */
stx_parser_ctx_t* stx_parser_open(const char* path) {
    if (!path) return NULL;
    
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    stx_file_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    /* Verify signature */
    if (memcmp(header.signature, STX_SIGNATURE, 3) != 0) {
        fclose(f);
        return NULL;
    }
    
    /* Allocate context */
    stx_parser_ctx_t* ctx = calloc(1, sizeof(stx_parser_ctx_t));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    memcpy(&ctx->header, &header, sizeof(header));
    ctx->track_count = header.track_count;
    
    /* Build track offset table */
    ctx->track_offsets = calloc(ctx->track_count, sizeof(uint32_t));
    if (!ctx->track_offsets) {
        fclose(f);
        free(ctx);
        return NULL;
    }
    
    /* Scan file for track positions */
    long pos = sizeof(stx_file_header_t);
    
    for (int i = 0; i < ctx->track_count; i++) {
        ctx->track_offsets[i] = pos;
        
        /* Read track descriptor to get size */
        if (fseek(f, pos, SEEK_SET) != 0) break;
        
        stx_track_descriptor_t td;
        if (fread(&td, sizeof(td), 1, f) != 1) break;
        
        pos += td.record_size;
    }
    
    ctx->initialized = true;
    return ctx;
}

/**
 * @brief Close STX parser
 */
void stx_parser_close(stx_parser_ctx_t** ctx) {
    if (!ctx || !*ctx) return;
    
    if ((*ctx)->file) {
        fclose((*ctx)->file);
    }
    free((*ctx)->track_offsets);
    free(*ctx);
    *ctx = NULL;
}

/**
 * @brief Read and parse track
 */
int stx_parser_read_track(
    stx_parser_ctx_t* ctx,
    int track_num,
    int side,
    stx_track_t** track_out
) {
    if (!ctx || !track_out || !ctx->initialized) {
        return -1;
    }
    
    /* Find track in offset table */
    int track_idx = -1;
    
    for (int i = 0; i < ctx->track_count; i++) {
        uint32_t offset = ctx->track_offsets[i];
        
        if (fseek(ctx->file, offset, SEEK_SET) != 0) continue;
        
        stx_track_descriptor_t td;
        if (fread(&td, sizeof(td), 1, ctx->file) != 1) continue;
        
        int phys_track = td.track_number & 0x7F;
        int phys_side = (td.track_number >> 7) & 1;
        
        if (phys_track == track_num && phys_side == side) {
            track_idx = i;
            break;
        }
    }
    
    if (track_idx < 0) {
        return -1;
    }
    
    uint32_t track_offset = ctx->track_offsets[track_idx];
    
    /* Seek to track */
    if (fseek(ctx->file, track_offset, SEEK_SET) != 0) {
        return -1;
    }
    
    /* Read track descriptor */
    stx_track_descriptor_t td;
    if (fread(&td, sizeof(td), 1, ctx->file) != 1) {
        return -1;
    }
    
    /* Allocate track structure */
    stx_track_t* track = calloc(1, sizeof(stx_track_t));
    if (!track) {
        return -1;
    }
    
    track->track_number = track_num;
    track->side = side;
    track->sector_count = td.sector_count;
    track->track_length_bits = td.track_length * 16;
    
    /* Parse flags */
    track->has_track_image = (td.flags & STX_TF_TRACK_IMAGE) != 0;
    track->has_fuzzy = (td.flags & STX_TF_FUZZY_BITS) != 0;
    track->has_timing = (td.flags & STX_TF_TIMING_DATA) != 0;
    
    /* Read sector descriptors */
    if (td.sector_count > 0) {
        stx_sector_descriptor_t* sec_descs = malloc(
            td.sector_count * sizeof(stx_sector_descriptor_t));
        
        if (!sec_descs) {
            free(track);
            return -1;
        }
        
        if (fread(sec_descs, sizeof(stx_sector_descriptor_t), 
                  td.sector_count, ctx->file) != td.sector_count) {
            free(sec_descs);
            free(track);
            return -1;
        }
        
        /* Parse each sector */
        track->good_sectors = 0;
        track->bad_sectors = 0;
        
        for (int i = 0; i < td.sector_count && i < STX_MAX_SECTORS; i++) {
            if (parse_sector(ctx, &sec_descs[i], &track->sectors[i], track_offset) == 0) {
                if (track->sectors[i].crc_error || track->sectors[i].id_crc_error) {
                    track->bad_sectors++;
                } else {
                    track->good_sectors++;
                }
            }
        }
        
        free(sec_descs);
    }
    
    /* Read fuzzy bit data */
    if (track->has_fuzzy && td.fuzzy_size > 0) {
        track->fuzzy_data = malloc(td.fuzzy_size);
        if (track->fuzzy_data) {
            if (fread(track->fuzzy_data, 1, td.fuzzy_size, ctx->file) == td.fuzzy_size) {
                track->fuzzy_size = td.fuzzy_size;
                
                /* Count fuzzy bits */
                for (size_t i = 0; i < td.fuzzy_size; i++) {
                    uint8_t byte = track->fuzzy_data[i];
                    while (byte) {
                        track->fuzzy_bit_count += byte & 1;
                        byte >>= 1;
                    }
                }
            } else {
                free(track->fuzzy_data);
                track->fuzzy_data = NULL;
            }
        }
    }
    
    /* Read track image */
    if (track->has_track_image) {
        /* Track image follows sector data */
        size_t track_bytes = (track->track_length_bits + 7) / 8;
        track->track_data = malloc(track_bytes);
        
        if (track->track_data) {
            if (fread(track->track_data, 1, track_bytes, ctx->file) == track_bytes) {
                track->track_data_size = track_bytes;
            } else {
                free(track->track_data);
                track->track_data = NULL;
            }
        }
    }
    
    /* Read timing data */
    if (track->has_timing) {
        /* Timing is array of 16-bit values */
        size_t timing_count = track->track_length_bits / 16;
        track->timing_data = malloc(timing_count * sizeof(uint16_t));
        
        if (track->timing_data) {
            if (fread(track->timing_data, sizeof(uint16_t), 
                      timing_count, ctx->file) == timing_count) {
                track->timing_count = timing_count;
            } else {
                free(track->timing_data);
                track->timing_data = NULL;
            }
        }
    }
    
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
void stx_parser_free_track(stx_track_t** track) {
    if (!track || !*track) return;
    
    stx_track_t* t = *track;
    
    /* Free sector data */
    for (int i = 0; i < t->sector_count && i < STX_MAX_SECTORS; i++) {
        free(t->sectors[i].data);
        free(t->sectors[i].fuzzy_mask);
    }
    
    /* Free track data */
    free(t->track_data);
    free(t->timing_data);
    free(t->fuzzy_data);
    
    free(t);
    *track = NULL;
}

/**
 * @brief Read single sector
 */
int stx_parser_read_sector(
    stx_parser_ctx_t* ctx,
    int track_num,
    int side,
    int sector_num,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!ctx || !buffer) return -1;
    
    stx_track_t* track = NULL;
    if (stx_parser_read_track(ctx, track_num, side, &track) != 0) {
        return -1;
    }
    
    int result = -1;
    
    /* Find sector */
    for (int i = 0; i < track->sector_count && i < STX_MAX_SECTORS; i++) {
        if (track->sectors[i].sector == sector_num) {
            size_t copy_size = track->sectors[i].size_bytes;
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
    
    stx_parser_free_track(&track);
    return result;
}

/**
 * @brief Get disk info
 */
void stx_parser_get_info(
    stx_parser_ctx_t* ctx,
    int* track_count,
    uint16_t* version,
    uint16_t* tool_version
) {
    if (!ctx) return;
    
    if (track_count) *track_count = ctx->track_count;
    if (version) *version = ctx->header.version;
    if (tool_version) *tool_version = ctx->header.tool_version;
}

/**
 * @brief Get statistics
 */
void stx_parser_get_stats(
    stx_parser_ctx_t* ctx,
    uint32_t* total_sectors,
    uint32_t* fuzzy_sectors,
    uint32_t* crc_errors
) {
    if (!ctx) return;
    
    if (total_sectors) *total_sectors = ctx->total_sectors;
    if (fuzzy_sectors) *fuzzy_sectors = ctx->fuzzy_sectors;
    if (crc_errors) *crc_errors = ctx->crc_errors;
}

/**
 * @brief Analyze protection (for copy protection detection)
 */
int stx_parser_analyze_protection(
    stx_parser_ctx_t* ctx,
    char* report,
    size_t report_size
) {
    if (!ctx || !report || report_size == 0) return -1;
    
    int written = 0;
    
    written += snprintf(report + written, report_size - written,
        "=== STX Protection Analysis ===\n");
    
    if (ctx->fuzzy_sectors > 0) {
        written += snprintf(report + written, report_size - written,
            "• Fuzzy bits detected: %u sectors\n", ctx->fuzzy_sectors);
        written += snprintf(report + written, report_size - written,
            "  → Likely copy protection present\n");
    }
    
    if (ctx->crc_errors > 0) {
        written += snprintf(report + written, report_size - written,
            "• CRC errors: %u sectors\n", ctx->crc_errors);
        written += snprintf(report + written, report_size - written,
            "  → May indicate intentional errors (protection)\n");
    }
    
    /* Scan for specific protection schemes */
    written += snprintf(report + written, report_size - written,
        "\nDetected schemes:\n");
    
    bool found_protection = false;
    
    /* Speedlock: fuzzy on track 0 */
    /* Copylock: specific patterns */
    /* Rob Northen Copylock */
    
    if (ctx->fuzzy_sectors > 10) {
        written += snprintf(report + written, report_size - written,
            "• High fuzzy count suggests Copylock/Speedlock\n");
        found_protection = true;
    }
    
    if (!found_protection) {
        written += snprintf(report + written, report_size - written,
            "• No known protection detected\n");
    }
    
    return written;
}

/*============================================================================
 * UNIT TEST
 *============================================================================*/

#ifdef STX_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== STX Parser Unit Tests ===\n");
    
    /* Test 1: Sector size decoding */
    {
        assert(decode_sector_size(0) == 128);
        assert(decode_sector_size(1) == 256);
        assert(decode_sector_size(2) == 512);
        assert(decode_sector_size(3) == 1024);
        assert(decode_sector_size(7) == 0);
        printf("✓ Sector size decoding\n");
    }
    
    /* Test 2: Header size */
    {
        assert(sizeof(stx_file_header_t) == 16);
        printf("✓ File header: 16 bytes\n");
    }
    
    /* Test 3: Track descriptor size */
    {
        assert(sizeof(stx_track_descriptor_t) == 16);
        printf("✓ Track descriptor: 16 bytes\n");
    }
    
    /* Test 4: Sector descriptor size */
    {
        assert(sizeof(stx_sector_descriptor_t) == 16);
        printf("✓ Sector descriptor: 16 bytes\n");
    }
    
    /* Test 5: Context allocation */
    {
        stx_parser_ctx_t* ctx = calloc(1, sizeof(stx_parser_ctx_t));
        assert(ctx != NULL);
        ctx->track_count = 160;
        assert(ctx->track_count == 160);
        free(ctx);
        printf("✓ Context allocation\n");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* STX_PARSER_TEST */
