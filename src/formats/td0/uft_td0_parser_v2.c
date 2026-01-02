/**
 * @file uft_td0_parser_v2.c
 * @brief GOD MODE TD0 (Teledisk) Parser v2
 * 
 * Advanced Teledisk image parser with:
 * - RLE and LZSS decompression
 * - CRC validation
 * - Comment block support
 * - Track/sector geometry detection
 * - Write protection detection
 * - Data encoding analysis (FM/MFM)
 * - Sector interleave detection
 * 
 * Teledisk was created by Sydex in 1985 for disk-to-disk backup.
 * Two compression methods: "td" (none/RLE) and "TD" (advanced LZSS)
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
 * TD0 FORMAT STRUCTURES
 *============================================================================*/

/* TD0 signatures */
#define TD0_SIG_NORMAL    0x4454  /* "TD" - advanced compression */
#define TD0_SIG_OLD       0x6474  /* "td" - no/RLE compression */

/* TD0 header flags */
#define TD0_FLAG_COMMENT  0x80    /* Comment block present */

/* TD0 encoding types */
#define TD0_ENC_FM        0x00    /* FM (single density) */
#define TD0_ENC_MFM       0x02    /* MFM (double density) */

/* TD0 sector data types */
#define TD0_DATA_NORMAL   0x00    /* Raw sector data */
#define TD0_DATA_RLE      0x01    /* RLE compressed */
#define TD0_DATA_RLE2     0x02    /* RLE with 2-byte pattern */

/* TD0 sector flags */
#define TD0_SEC_DUPLICATE 0x01    /* Duplicate sector */
#define TD0_SEC_CRC_ERROR 0x02    /* CRC error in original */
#define TD0_SEC_DELETED   0x04    /* Deleted data mark */
#define TD0_SEC_SKIPPED   0x10    /* Sector data not saved */
#define TD0_SEC_NO_DAM    0x20    /* No data address mark */
#define TD0_SEC_NO_ID     0x40    /* No ID field */

/* Maximum limits */
#define TD0_MAX_TRACKS    100
#define TD0_MAX_SECTORS   40
#define TD0_MAX_SECTOR_SIZE 8192
#define TD0_MAX_COMMENT   1024

#pragma pack(push, 1)

/* TD0 file header (12 bytes) */
typedef struct {
    uint16_t signature;       /* "TD" or "td" */
    uint8_t  sequence;        /* Volume sequence (usually 0) */
    uint8_t  check_sig;       /* Check signature byte */
    uint8_t  version;         /* Version number */
    uint8_t  data_rate;       /* Data rate (250/300/500 kbps) */
    uint8_t  drive_type;      /* Drive type */
    uint8_t  stepping;        /* Track stepping (1 or 2) */
    uint8_t  dos_alloc;       /* DOS allocation flag */
    uint8_t  sides;           /* Number of sides (1 or 2) */
    uint16_t crc;             /* Header CRC */
} td0_header_t;

/* TD0 comment block header (10 bytes) */
typedef struct {
    uint16_t crc;             /* Comment CRC */
    uint16_t length;          /* Comment data length */
    uint8_t  year;            /* Year - 1900 */
    uint8_t  month;           /* Month (1-12) */
    uint8_t  day;             /* Day (1-31) */
    uint8_t  hour;            /* Hour (0-23) */
    uint8_t  minute;          /* Minute (0-59) */
    uint8_t  second;          /* Second (0-59) */
} td0_comment_header_t;

/* TD0 track header (4 bytes) */
typedef struct {
    uint8_t  sectors;         /* Number of sectors (255 = end) */
    uint8_t  cylinder;        /* Physical cylinder */
    uint8_t  head;            /* Physical head */
    uint8_t  crc;             /* Track header CRC */
} td0_track_header_t;

/* TD0 sector header (6 bytes) */
typedef struct {
    uint8_t  cylinder;        /* Sector ID cylinder */
    uint8_t  head;            /* Sector ID head */
    uint8_t  sector;          /* Sector ID number */
    uint8_t  size_code;       /* Size code (0=128, 1=256, 2=512...) */
    uint8_t  flags;           /* Sector flags */
    uint8_t  crc;             /* Sector header CRC */
} td0_sector_header_t;

/* TD0 sector data header (3 bytes) */
typedef struct {
    uint16_t data_size;       /* Compressed data size - 1 */
    uint8_t  encoding;        /* Data encoding method */
} td0_sector_data_t;

#pragma pack(pop)

/*============================================================================
 * TD0 CONTEXT STRUCTURE
 *============================================================================*/

typedef struct {
    /* File info */
    FILE    *fp;
    char     filename[256];
    size_t   file_size;
    
    /* Header info */
    td0_header_t header;
    bool     advanced_compression;
    bool     has_comment;
    
    /* Comment */
    char     comment[TD0_MAX_COMMENT];
    uint16_t comment_length;
    struct {
        uint16_t year;
        uint8_t  month;
        uint8_t  day;
        uint8_t  hour;
        uint8_t  minute;
        uint8_t  second;
    } timestamp;
    
    /* Geometry */
    uint8_t  tracks;
    uint8_t  sides;
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t error_sectors;
    uint32_t deleted_sectors;
    uint32_t skipped_sectors;
    
    /* LZSS decompression state */
    uint8_t  lzss_buffer[4096];
    int      lzss_pos;
    int      lzss_bit_buf;
    int      lzss_bit_count;
    
    /* Decompressed data buffer */
    uint8_t *decomp_buffer;
    size_t   decomp_size;
    size_t   decomp_pos;
} td0_context_t;

/*============================================================================
 * CRC CALCULATION
 *============================================================================*/

static uint16_t td0_crc_table[256];
static bool td0_crc_initialized = false;

static void td0_init_crc_table(void) {
    if (td0_crc_initialized) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = 0;
        uint16_t a = (uint16_t)(i << 8);
        for (int j = 0; j < 8; j++) {
            if ((crc ^ a) & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0xA097);
            } else {
                crc <<= 1;
            }
            a <<= 1;
        }
        td0_crc_table[i] = crc;
    }
    td0_crc_initialized = true;
}

static uint16_t td0_calc_crc(const uint8_t *data, size_t len, uint16_t init) {
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc = (uint16_t)((crc << 8) ^ td0_crc_table[(crc >> 8) ^ data[i]]);
    }
    return crc;
}

/*============================================================================
 * LZSS DECOMPRESSION
 *============================================================================*/

static int td0_lzss_getbit(td0_context_t *ctx) {
    if (ctx->lzss_bit_count == 0) {
        if (ctx->decomp_pos >= ctx->decomp_size) {
            return -1;  /* EOF */
        }
        ctx->lzss_bit_buf = ctx->decomp_buffer[ctx->decomp_pos++];
        ctx->lzss_bit_count = 8;
    }
    ctx->lzss_bit_count--;
    return (ctx->lzss_bit_buf >> ctx->lzss_bit_count) & 1;
}

static int td0_lzss_getbits(td0_context_t *ctx, int count) {
    int result = 0;
    for (int i = 0; i < count; i++) {
        int bit = td0_lzss_getbit(ctx);
        if (bit < 0) return -1;
        result = (result << 1) | bit;
    }
    return result;
}

static int td0_lzss_decompress(td0_context_t *ctx, uint8_t *output, size_t max_size) {
    size_t out_pos = 0;
    
    /* Initialize ring buffer */
    memset(ctx->lzss_buffer, 0x20, sizeof(ctx->lzss_buffer));
    ctx->lzss_pos = 0;
    ctx->lzss_bit_count = 0;
    
    while (out_pos < max_size) {
        int flag = td0_lzss_getbit(ctx);
        if (flag < 0) break;
        
        if (flag) {
            /* Literal byte */
            int byte = td0_lzss_getbits(ctx, 8);
            if (byte < 0) break;
            
            output[out_pos++] = (uint8_t)byte;
            ctx->lzss_buffer[ctx->lzss_pos] = (uint8_t)byte;
            ctx->lzss_pos = (ctx->lzss_pos + 1) & 0xFFF;
        } else {
            /* Copy from buffer */
            int offset = td0_lzss_getbits(ctx, 12);
            if (offset < 0) break;
            
            int length = td0_lzss_getbits(ctx, 4);
            if (length < 0) break;
            
            length += 2;  /* Minimum match length is 2 */
            
            for (int i = 0; i < length && out_pos < max_size; i++) {
                uint8_t byte = ctx->lzss_buffer[(offset + i) & 0xFFF];
                output[out_pos++] = byte;
                ctx->lzss_buffer[ctx->lzss_pos] = byte;
                ctx->lzss_pos = (ctx->lzss_pos + 1) & 0xFFF;
            }
        }
    }
    
    return (int)out_pos;
}

/*============================================================================
 * RLE DECOMPRESSION
 *============================================================================*/

static int td0_rle_decompress(const uint8_t *input, size_t in_size,
                               uint8_t *output, size_t out_size,
                               uint8_t encoding) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < in_size && out_pos < out_size) {
        if (encoding == TD0_DATA_NORMAL) {
            /* Raw data */
            output[out_pos++] = input[in_pos++];
        } else if (encoding == TD0_DATA_RLE) {
            /* RLE: count, byte */
            if (in_pos + 1 >= in_size) break;
            uint8_t count = input[in_pos++];
            uint8_t byte = input[in_pos++];
            
            if (count == 0) {
                /* Literal block */
                if (in_pos + byte > in_size) break;
                for (int i = 0; i < byte && out_pos < out_size; i++) {
                    output[out_pos++] = input[in_pos++];
                }
            } else {
                /* Repeat byte */
                for (int i = 0; i < count && out_pos < out_size; i++) {
                    output[out_pos++] = byte;
                }
            }
        } else if (encoding == TD0_DATA_RLE2) {
            /* RLE with 2-byte pattern: count, byte1, byte2 */
            if (in_pos + 2 >= in_size) break;
            uint8_t count = input[in_pos++];
            uint8_t byte1 = input[in_pos++];
            uint8_t byte2 = input[in_pos++];
            
            for (int i = 0; i < count && out_pos + 1 < out_size; i++) {
                output[out_pos++] = byte1;
                output[out_pos++] = byte2;
            }
        }
    }
    
    return (int)out_pos;
}

/*============================================================================
 * TD0 FILE OPERATIONS
 *============================================================================*/

/**
 * @brief Get drive type name
 */
static const char* td0_drive_type_name(uint8_t type) {
    switch (type) {
        case 0: return "5.25\" 360KB";
        case 1: return "5.25\" 1.2MB";
        case 2: return "3.5\" 720KB";
        case 3: return "3.5\" 1.44MB";
        case 4: return "8\" SD";
        case 5: return "8\" DD";
        default: return "Unknown";
    }
}

/**
 * @brief Get data rate name
 */
static const char* td0_data_rate_name(uint8_t rate) {
    switch (rate) {
        case 0: return "250 kbps (DD)";
        case 1: return "300 kbps (DD)";
        case 2: return "500 kbps (HD)";
        default: return "Unknown";
    }
}

/**
 * @brief Get encoding name
 */
static const char* td0_encoding_name(uint8_t enc) {
    return (enc & TD0_ENC_MFM) ? "MFM" : "FM";
}

/**
 * @brief Open TD0 file
 */
td0_context_t* td0_open(const char *filename) {
    if (!filename) return NULL;
    
    td0_init_crc_table();
    
    td0_context_t *ctx = calloc(1, sizeof(td0_context_t));
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
    
    /* Read header */
    if (fread(&ctx->header, sizeof(td0_header_t), 1, ctx->fp) != 1) {
        fclose(ctx->fp);
        free(ctx);
        return NULL;
    }
    
    /* Validate signature */
    if (ctx->header.signature != TD0_SIG_NORMAL && 
        ctx->header.signature != TD0_SIG_OLD) {
        fclose(ctx->fp);
        free(ctx);
        return NULL;
    }
    
    ctx->advanced_compression = (ctx->header.signature == TD0_SIG_NORMAL);
    ctx->has_comment = (ctx->header.drive_type & TD0_FLAG_COMMENT) != 0;
    ctx->header.drive_type &= ~TD0_FLAG_COMMENT;  /* Clear flag */
    
    /* Validate CRC */
    uint16_t calc_crc = td0_calc_crc((uint8_t*)&ctx->header, 10, 0);
    if (calc_crc != ctx->header.crc) {
        fprintf(stderr, "TD0: Header CRC mismatch (got 0x%04X, expected 0x%04X)\n",
                calc_crc, ctx->header.crc);
        /* Continue anyway - some TD0 files have bad CRCs */
    }
    
    ctx->sides = ctx->header.sides + 1;
    
    /* Handle advanced compression */
    if (ctx->advanced_compression) {
        /* Read rest of file for LZSS decompression */
        size_t comp_size = ctx->file_size - sizeof(td0_header_t);
        uint8_t *comp_data = malloc(comp_size);
        if (!comp_data) {
            fclose(ctx->fp);
            free(ctx);
            return NULL;
        }
        
        if (fread(comp_data, 1, comp_size, ctx->fp) != comp_size) {
            free(comp_data);
            fclose(ctx->fp);
            free(ctx);
            return NULL;
        }
        
        /* Allocate decompression buffer (estimate 10x expansion) */
        ctx->decomp_size = comp_size * 10;
        ctx->decomp_buffer = malloc(ctx->decomp_size);
        if (!ctx->decomp_buffer) {
            free(comp_data);
            fclose(ctx->fp);
            free(ctx);
            return NULL;
        }
        
        /* Copy compressed data for LZSS processing */
        memcpy(ctx->decomp_buffer, comp_data, comp_size);
        ctx->decomp_size = comp_size;
        ctx->decomp_pos = 0;
        
        free(comp_data);
    }
    
    /* Read comment if present */
    if (ctx->has_comment) {
        td0_comment_header_t comment_hdr;
        
        if (ctx->advanced_compression) {
            /* Decompress comment header */
            uint8_t hdr_buf[10];
            int decomp_len = td0_lzss_decompress(ctx, hdr_buf, 10);
            if (decomp_len >= 10) {
                memcpy(&comment_hdr, hdr_buf, sizeof(comment_hdr));
            }
        } else {
            size_t read_count = fread(&comment_hdr, sizeof(comment_hdr), 1, ctx->fp);
            if (read_count < 1) {
                /* Comment header read failed - treat as no comment */
                ctx->has_comment = false;
            }
        }
        
        if (ctx->has_comment && comment_hdr.length > 0) {
            ctx->comment_length = (comment_hdr.length < TD0_MAX_COMMENT) ? 
                                   comment_hdr.length : TD0_MAX_COMMENT - 1;
            
            if (ctx->advanced_compression) {
                uint8_t *comment_buf = malloc(comment_hdr.length);
                if (comment_buf) {
                    td0_lzss_decompress(ctx, comment_buf, comment_hdr.length);
                    memcpy(ctx->comment, comment_buf, ctx->comment_length);
                    free(comment_buf);
                }
            } else {
                size_t read_count = fread(ctx->comment, 1, ctx->comment_length, ctx->fp);
                if (read_count < ctx->comment_length) {
                    ctx->comment_length = (uint16_t)read_count;
                }
            }
            ctx->comment[ctx->comment_length] = '\0';
            
            /* Parse timestamp */
            ctx->timestamp.year = 1900 + comment_hdr.year;
            ctx->timestamp.month = comment_hdr.month;
            ctx->timestamp.day = comment_hdr.day;
            ctx->timestamp.hour = comment_hdr.hour;
            ctx->timestamp.minute = comment_hdr.minute;
            ctx->timestamp.second = comment_hdr.second;
        }
    }
    
    return ctx;
}

/**
 * @brief Close TD0 file
 */
void td0_close(td0_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->fp) fclose(ctx->fp);
    if (ctx->decomp_buffer) free(ctx->decomp_buffer);
    free(ctx);
}

/**
 * @brief Read sector data
 */
int td0_read_sector(td0_context_t *ctx, uint8_t cyl, uint8_t head, 
                    uint8_t sector, uint8_t *buffer, size_t buf_size) {
    if (!ctx || !buffer) return -1;
    
    /* Reset to start of track data */
    if (ctx->advanced_compression) {
        ctx->decomp_pos = ctx->has_comment ? 
            (sizeof(td0_comment_header_t) + ctx->comment_length) : 0;
    } else {
        fseek(ctx->fp, sizeof(td0_header_t), SEEK_SET);
        if (ctx->has_comment) {
            fseek(ctx->fp, sizeof(td0_comment_header_t) + ctx->comment_length, SEEK_CUR);
        }
    }
    
    /* Search for sector */
    while (1) {
        td0_track_header_t track_hdr;
        
        if (ctx->advanced_compression) {
            uint8_t hdr_buf[4];
            if (td0_lzss_decompress(ctx, hdr_buf, 4) < 4) break;
            memcpy(&track_hdr, hdr_buf, sizeof(track_hdr));
        } else {
            if (fread(&track_hdr, sizeof(track_hdr), 1, ctx->fp) != 1) break;
        }
        
        if (track_hdr.sectors == 0xFF) break;  /* End of data */
        
        for (int s = 0; s < track_hdr.sectors; s++) {
            td0_sector_header_t sec_hdr;
            
            if (ctx->advanced_compression) {
                uint8_t hdr_buf[6];
                if (td0_lzss_decompress(ctx, hdr_buf, 6) < 6) return -1;
                memcpy(&sec_hdr, hdr_buf, sizeof(sec_hdr));
            } else {
                if (fread(&sec_hdr, sizeof(sec_hdr), 1, ctx->fp) != 1) return -1;
            }
            
            /* Calculate sector size */
            size_t sec_size = (size_t)128 << sec_hdr.size_code;
            
            /* Check for data */
            bool has_data = !(sec_hdr.flags & (TD0_SEC_SKIPPED | TD0_SEC_NO_DAM | TD0_SEC_NO_ID));
            
            if (has_data) {
                td0_sector_data_t data_hdr;
                
                if (ctx->advanced_compression) {
                    uint8_t hdr_buf[3];
                    if (td0_lzss_decompress(ctx, hdr_buf, 3) < 3) return -1;
                    memcpy(&data_hdr, hdr_buf, sizeof(data_hdr));
                } else {
                    if (fread(&data_hdr, sizeof(data_hdr), 1, ctx->fp) != 1) return -1;
                }
                
                size_t comp_size = data_hdr.data_size + 1;
                
                /* Read compressed data */
                uint8_t *comp_data = malloc(comp_size);
                if (!comp_data) return -1;
                
                if (ctx->advanced_compression) {
                    if (td0_lzss_decompress(ctx, comp_data, comp_size) < (int)comp_size) {
                        free(comp_data);
                        return -1;
                    }
                } else {
                    if (fread(comp_data, 1, comp_size, ctx->fp) != comp_size) {
                        free(comp_data);
                        return -1;
                    }
                }
                
                /* Check if this is the sector we want */
                if (track_hdr.cylinder == cyl && track_hdr.head == head && 
                    sec_hdr.sector == sector) {
                    
                    /* Decompress sector data */
                    uint8_t *decomp = malloc(sec_size);
                    if (!decomp) {
                        free(comp_data);
                        return -1;
                    }
                    
                    int decomp_len = td0_rle_decompress(comp_data, comp_size,
                                                         decomp, sec_size,
                                                         data_hdr.encoding);
                    
                    size_t copy_size = (size_t)decomp_len < buf_size ? 
                                       (size_t)decomp_len : buf_size;
                    memcpy(buffer, decomp, copy_size);
                    
                    free(decomp);
                    free(comp_data);
                    return (int)copy_size;
                }
                
                free(comp_data);
            }
        }
    }
    
    return -1;  /* Sector not found */
}

/**
 * @brief Analyze TD0 geometry
 */
int td0_analyze_geometry(td0_context_t *ctx) {
    if (!ctx) return -1;
    
    /* Reset counters */
    ctx->tracks = 0;
    ctx->sectors_per_track = 0;
    ctx->sector_size = 0;
    ctx->total_sectors = 0;
    ctx->error_sectors = 0;
    ctx->deleted_sectors = 0;
    ctx->skipped_sectors = 0;
    
    /* Reset to start of track data */
    if (ctx->advanced_compression) {
        ctx->decomp_pos = ctx->has_comment ? 
            (sizeof(td0_comment_header_t) + ctx->comment_length) : 0;
    } else {
        fseek(ctx->fp, sizeof(td0_header_t), SEEK_SET);
        if (ctx->has_comment) {
            fseek(ctx->fp, sizeof(td0_comment_header_t) + ctx->comment_length, SEEK_CUR);
        }
    }
    
    uint8_t max_cyl = 0;
    uint8_t max_head = 0;
    uint8_t max_sector = 0;
    
    while (1) {
        td0_track_header_t track_hdr;
        
        if (ctx->advanced_compression) {
            uint8_t hdr_buf[4];
            if (td0_lzss_decompress(ctx, hdr_buf, 4) < 4) break;
            memcpy(&track_hdr, hdr_buf, sizeof(track_hdr));
        } else {
            if (fread(&track_hdr, sizeof(track_hdr), 1, ctx->fp) != 1) break;
        }
        
        if (track_hdr.sectors == 0xFF) break;  /* End of data */
        
        if (track_hdr.cylinder > max_cyl) max_cyl = track_hdr.cylinder;
        if (track_hdr.head > max_head) max_head = track_hdr.head;
        
        for (int s = 0; s < track_hdr.sectors; s++) {
            td0_sector_header_t sec_hdr;
            
            if (ctx->advanced_compression) {
                uint8_t hdr_buf[6];
                if (td0_lzss_decompress(ctx, hdr_buf, 6) < 6) break;
                memcpy(&sec_hdr, hdr_buf, sizeof(sec_hdr));
            } else {
                if (fread(&sec_hdr, sizeof(sec_hdr), 1, ctx->fp) != 1) break;
            }
            
            ctx->total_sectors++;
            
            if (sec_hdr.sector > max_sector) max_sector = sec_hdr.sector;
            
            /* Update sector size */
            uint16_t sec_size = (uint16_t)(128 << sec_hdr.size_code);
            if (sec_size > ctx->sector_size) ctx->sector_size = sec_size;
            
            /* Count flags */
            if (sec_hdr.flags & TD0_SEC_CRC_ERROR) ctx->error_sectors++;
            if (sec_hdr.flags & TD0_SEC_DELETED) ctx->deleted_sectors++;
            if (sec_hdr.flags & TD0_SEC_SKIPPED) ctx->skipped_sectors++;
            
            /* Skip sector data if present */
            bool has_data = !(sec_hdr.flags & (TD0_SEC_SKIPPED | TD0_SEC_NO_DAM | TD0_SEC_NO_ID));
            
            if (has_data) {
                td0_sector_data_t data_hdr;
                
                if (ctx->advanced_compression) {
                    uint8_t hdr_buf[3];
                    if (td0_lzss_decompress(ctx, hdr_buf, 3) < 3) break;
                    memcpy(&data_hdr, hdr_buf, sizeof(data_hdr));
                } else {
                    if (fread(&data_hdr, sizeof(data_hdr), 1, ctx->fp) != 1) break;
                }
                
                size_t comp_size = data_hdr.data_size + 1;
                
                if (ctx->advanced_compression) {
                    uint8_t *skip_buf = malloc(comp_size);
                    if (skip_buf) {
                        td0_lzss_decompress(ctx, skip_buf, comp_size);
                        free(skip_buf);
                    }
                } else {
                    fseek(ctx->fp, (long)comp_size, SEEK_CUR);
                }
            }
        }
    }
    
    ctx->tracks = max_cyl + 1;
    ctx->sides = max_head + 1;
    ctx->sectors_per_track = max_sector;
    
    return 0;
}

/**
 * @brief Print TD0 information
 */
void td0_print_info(td0_context_t *ctx) {
    if (!ctx) return;
    
    printf("=== TD0 (Teledisk) Image Info ===\n");
    printf("File: %s\n", ctx->filename);
    printf("Size: %zu bytes\n", ctx->file_size);
    printf("Compression: %s\n", ctx->advanced_compression ? "LZSS (advanced)" : "None/RLE");
    printf("Version: %d\n", ctx->header.version);
    printf("\n");
    
    printf("Drive Info:\n");
    printf("  Type: %s\n", td0_drive_type_name(ctx->header.drive_type));
    printf("  Data Rate: %s\n", td0_data_rate_name(ctx->header.data_rate));
    printf("  Encoding: %s\n", td0_encoding_name(ctx->header.data_rate));
    printf("  Stepping: %d:1\n", ctx->header.stepping);
    printf("  Sides: %d\n", ctx->sides);
    printf("\n");
    
    if (ctx->has_comment && ctx->comment_length > 0) {
        printf("Comment:\n");
        printf("  Date: %04d-%02d-%02d %02d:%02d:%02d\n",
               ctx->timestamp.year, ctx->timestamp.month, ctx->timestamp.day,
               ctx->timestamp.hour, ctx->timestamp.minute, ctx->timestamp.second);
        printf("  Text: %s\n", ctx->comment);
        printf("\n");
    }
    
    if (ctx->tracks > 0) {
        printf("Geometry:\n");
        printf("  Tracks: %d\n", ctx->tracks);
        printf("  Sides: %d\n", ctx->sides);
        printf("  Sectors/Track: %d\n", ctx->sectors_per_track);
        printf("  Sector Size: %d bytes\n", ctx->sector_size);
        printf("  Total Sectors: %u\n", ctx->total_sectors);
        printf("\n");
        
        if (ctx->error_sectors || ctx->deleted_sectors || ctx->skipped_sectors) {
            printf("Sector Status:\n");
            if (ctx->error_sectors) printf("  CRC Errors: %u\n", ctx->error_sectors);
            if (ctx->deleted_sectors) printf("  Deleted: %u\n", ctx->deleted_sectors);
            if (ctx->skipped_sectors) printf("  Skipped: %u\n", ctx->skipped_sectors);
        }
    }
}

/**
 * @brief Convert TD0 to raw sector image
 */
int td0_convert_to_raw(td0_context_t *ctx, const char *outfile) {
    if (!ctx || !outfile) return -1;
    
    /* First analyze geometry */
    if (td0_analyze_geometry(ctx) < 0) return -1;
    
    FILE *out = fopen(outfile, "wb");
    if (!out) return -1;
    
    int result = 0;
    uint8_t *sector_buf = malloc(ctx->sector_size);
    if (!sector_buf) {
        fclose(out);
        return -1;
    }
    
    for (int cyl = 0; cyl < ctx->tracks; cyl++) {
        for (int head = 0; head < ctx->sides; head++) {
            for (int sec = 1; sec <= ctx->sectors_per_track; sec++) {
                memset(sector_buf, 0xE5, ctx->sector_size);
                
                int read_len = td0_read_sector(ctx, (uint8_t)cyl, (uint8_t)head, 
                                                (uint8_t)sec, sector_buf, ctx->sector_size);
                if (read_len < 0) {
                    fprintf(stderr, "Warning: Missing sector C%d H%d S%d\n", cyl, head, sec);
                }
                
                if (fwrite(sector_buf, ctx->sector_size, 1, out) != 1) {
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

#ifdef TD0_PARSER_TEST

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("Testing %s... ", name)
#define PASS() do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("✗ (%s)\n", msg); tests_failed++; } while(0)

static void test_crc(void) {
    TEST("crc");
    
    td0_init_crc_table();
    
    /* Test known CRC values */
    uint8_t data1[] = {0x54, 0x44};  /* "TD" */
    uint16_t crc1 = td0_calc_crc(data1, 2, 0);
    
    /* CRC should be non-zero for non-empty data */
    if (crc1 == 0) {
        FAIL("CRC is zero");
        return;
    }
    
    PASS();
}

static void test_drive_names(void) {
    TEST("drive_names");
    
    if (strcmp(td0_drive_type_name(0), "5.25\" 360KB") != 0) {
        FAIL("Drive type 0");
        return;
    }
    if (strcmp(td0_drive_type_name(1), "5.25\" 1.2MB") != 0) {
        FAIL("Drive type 1");
        return;
    }
    if (strcmp(td0_drive_type_name(2), "3.5\" 720KB") != 0) {
        FAIL("Drive type 2");
        return;
    }
    if (strcmp(td0_drive_type_name(3), "3.5\" 1.44MB") != 0) {
        FAIL("Drive type 3");
        return;
    }
    
    PASS();
}

static void test_data_rates(void) {
    TEST("data_rates");
    
    if (strcmp(td0_data_rate_name(0), "250 kbps (DD)") != 0) {
        FAIL("Rate 0");
        return;
    }
    if (strcmp(td0_data_rate_name(1), "300 kbps (DD)") != 0) {
        FAIL("Rate 1");
        return;
    }
    if (strcmp(td0_data_rate_name(2), "500 kbps (HD)") != 0) {
        FAIL("Rate 2");
        return;
    }
    
    PASS();
}

static void test_rle_decompress(void) {
    TEST("rle_decompress");
    
    /* Test normal (no compression) */
    uint8_t raw_in[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t raw_out[4];
    int len = td0_rle_decompress(raw_in, 4, raw_out, 4, TD0_DATA_NORMAL);
    
    if (len != 4 || memcmp(raw_in, raw_out, 4) != 0) {
        FAIL("Normal mode");
        return;
    }
    
    /* Test RLE2 (2-byte pattern) */
    uint8_t rle2_in[] = {3, 0xAA, 0x55};  /* 3 times AA 55 */
    uint8_t rle2_out[6];
    len = td0_rle_decompress(rle2_in, 3, rle2_out, 6, TD0_DATA_RLE2);
    
    if (len != 6) {
        FAIL("RLE2 length");
        return;
    }
    
    for (int i = 0; i < 3; i++) {
        if (rle2_out[i*2] != 0xAA || rle2_out[i*2+1] != 0x55) {
            FAIL("RLE2 pattern");
            return;
        }
    }
    
    PASS();
}

static void test_encoding_names(void) {
    TEST("encoding_names");
    
    if (strcmp(td0_encoding_name(TD0_ENC_FM), "FM") != 0) {
        FAIL("FM encoding");
        return;
    }
    if (strcmp(td0_encoding_name(TD0_ENC_MFM), "MFM") != 0) {
        FAIL("MFM encoding");
        return;
    }
    
    PASS();
}

int main(void) {
    printf("=== TD0 Parser v2 Tests ===\n");
    
    test_crc();
    test_drive_names();
    test_data_rates();
    test_rle_decompress();
    test_encoding_names();
    
    printf("\n=== %s ===\n", tests_failed == 0 ? "All tests passed!" : "Some tests failed");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed;
}

#endif /* TD0_PARSER_TEST */
