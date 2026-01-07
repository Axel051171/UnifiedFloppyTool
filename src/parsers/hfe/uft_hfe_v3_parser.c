/**
 * @file uft_hfe_v3_parser.c
 * @brief UFT HFE v3 Parser - UFT HFE Format Format with HDDD A2 Support
 * @version 3.3.7
 * @date 2026-01-03
 *
 * FEATURES:
 * - HFE v1, v2, v3 format support
 * - HDDD A2 (Apple II) GCR encoding
 * - Opcode-based track encoding (HFE v3)
 * - Variable bitrate support
 * - Weak/fuzzy bits via RAND opcode
 * - Index pulse markers
 * - Read/Write operations
 *
 * HFE FILE STRUCTURE:
 * - Header (512 bytes)
 * - Track offset table (variable)
 * - Track data blocks (512-byte aligned)
 *
 * HFE v3 OPCODES:
 * - 0xF0: NOP (no operation)
 * - 0xF1: SETINDEX (mark index pulse)
 * - 0xF2: SETBITRATE (change bitrate)
 * - 0xF3: SKIPBITS (skip N bits)
 * - 0xF4: RAND (random/weak bits)
 *
 * @author UFT Team
 * @copyright UFT Project 2025-2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * HFE CONSTANTS
 *============================================================================*/

/* Signatures */
#define HFE_V1_SIGNATURE    "HXCPICFE"
#define HFE_V3_SIGNATURE    "HXCHFEV3"
#define HFE_SIGNATURE_LEN   8

/* Header sizes */
#define HFE_HEADER_SIZE     512
#define HFE_BLOCK_SIZE      512

/* Maximum values */
#define HFE_MAX_TRACKS      84
#define HFE_MAX_SIDES       2

/* HFE v3 Opcodes */
#define HFE_OPCODE_MASK     0xF0
#define HFE_NOP_OPCODE      0xF0
#define HFE_SETINDEX_OPCODE 0xF1
#define HFE_SETBITRATE_OPCODE 0xF2
#define HFE_SKIPBITS_OPCODE 0xF3
#define HFE_RAND_OPCODE     0xF4

/* Floppyemu frequency for bitrate calculation */
#define HFE_FLOPPYEMU_FREQ  72000000

/* Track encoding types */
#define HFE_ENCODING_ISOIBM_MFM     0
#define HFE_ENCODING_AMIGA_MFM      1
#define HFE_ENCODING_ISOIBM_FM      2
#define HFE_ENCODING_EMU_FM         3
#define HFE_ENCODING_UNKNOWN        4

/* HDDD A2 specific encodings */
#define HFE_ENCODING_APPLE_GCR1     7
#define HFE_ENCODING_APPLE_GCR2     8
#define HFE_ENCODING_HDDD_A2_GCR1   0x87
#define HFE_ENCODING_HDDD_A2_GCR2   0x88

/* Interface modes */
#define HFE_IFMODE_IBMPC_DD         0
#define HFE_IFMODE_IBMPC_HD         1
#define HFE_IFMODE_ATARIST_DD       2
#define HFE_IFMODE_ATARIST_HD       3
#define HFE_IFMODE_AMIGA_DD         4
#define HFE_IFMODE_AMIGA_HD         5
#define HFE_IFMODE_CPC_DD           6
#define HFE_IFMODE_SHUGART_DD       7
#define HFE_IFMODE_IBMPC_ED         8
#define HFE_IFMODE_MSX2_DD          9
#define HFE_IFMODE_C64_DD           10
#define HFE_IFMODE_EMU_SHUGART      11

/*============================================================================
 * ERROR CODES
 *============================================================================*/

typedef enum {
    HFE_OK = 0,
    HFE_ERR_NULL_PARAM,
    HFE_ERR_FILE_OPEN,
    HFE_ERR_FILE_READ,
    HFE_ERR_FILE_WRITE,
    HFE_ERR_BAD_SIGNATURE,
    HFE_ERR_BAD_VERSION,
    HFE_ERR_TRUNCATED,
    HFE_ERR_BAD_TRACK,
    HFE_ERR_ALLOC,
    HFE_ERR_INVALID_DATA,
    HFE_ERR_NOT_SUPPORTED,
    HFE_ERR_COUNT
} hfe_error_t;

static const char* hfe_error_names[] = {
    [HFE_OK] = "OK",
    [HFE_ERR_NULL_PARAM] = "Null parameter",
    [HFE_ERR_FILE_OPEN] = "Cannot open file",
    [HFE_ERR_FILE_READ] = "File read error",
    [HFE_ERR_FILE_WRITE] = "File write error",
    [HFE_ERR_BAD_SIGNATURE] = "Invalid HFE signature",
    [HFE_ERR_BAD_VERSION] = "Unsupported HFE version",
    [HFE_ERR_TRUNCATED] = "File is truncated",
    [HFE_ERR_BAD_TRACK] = "Invalid track data",
    [HFE_ERR_ALLOC] = "Memory allocation failed",
    [HFE_ERR_INVALID_DATA] = "Invalid data",
    [HFE_ERR_NOT_SUPPORTED] = "Feature not supported"
};

/*============================================================================
 * STRUCTURES
 *============================================================================*/

/* HFE File Header (512 bytes) */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  signature[8];         /* "HXCPICFE" or "HXCHFEV3" */
    uint8_t  format_revision;      /* Format revision */
    uint8_t  number_of_tracks;     /* Number of tracks */
    uint8_t  number_of_sides;      /* Number of sides (1 or 2) */
    uint8_t  track_encoding;       /* Track encoding type */
    uint16_t bitrate;              /* Bitrate in kbps (for fixed rate) */
    uint16_t uft_floppy_rpm;           /* Floppy RPM (usually 0 = auto) */
    uint8_t  uft_floppy_interface_mode;/* Interface mode */
    uint8_t  reserved1;            /* Reserved (must be 1) */
    uint16_t track_list_offset;    /* Offset to track list (in 512-byte blocks) */
    uint8_t  write_allowed;        /* Write allowed flag */
    uint8_t  single_step;          /* Single step mode */
    uint8_t  track0s0_altencoding; /* Track 0 side 0 alternate encoding */
    uint8_t  track0s0_encoding;    /* Track 0 side 0 encoding */
    uint8_t  track0s1_altencoding; /* Track 0 side 1 alternate encoding */
    uint8_t  track0s1_encoding;    /* Track 0 side 1 encoding */
    uint8_t  reserved2[478];       /* Padding to 512 bytes */
} hfe_header_t;
UFT_PACK_END

/* Track offset entry */
UFT_PACK_BEGIN
typedef struct {
    uint16_t offset;               /* Offset in 512-byte blocks */
    uint16_t track_len;            /* Track length in bytes */
} hfe_track_entry_t;
UFT_PACK_END

/* Decoded track side data */
typedef struct {
    uint8_t* data;                 /* Track data */
    uint32_t data_len;             /* Data length in bits */
    uint8_t* flakybitmap;          /* Weak/flaky bit map */
    uint8_t* indexbitmap;          /* Index pulse bitmap */
    uint32_t* timing;              /* Per-byte timing (bitrate) */
    uint32_t tracklen_bytes;       /* Length in bytes */
    int encoding;                  /* Track encoding type */
} hfe_track_side_t;

/* Complete track with both sides */
typedef struct {
    int track_number;
    int number_of_sides;
    hfe_track_side_t sides[2];
    uint16_t rpm;                  /* Track RPM */
    bool valid;
} hfe_track_t;

/* HFE Disk context */
typedef struct {
    FILE* file;
    hfe_header_t header;
    hfe_track_entry_t* track_list;
    
    /* File info */
    char filename[256];
    int version;                   /* 1, 2, or 3 */
    bool is_hddd_a2;               /* HDDD A2 variant */
    
    /* Statistics */
    uint32_t tracks_read;
    uint32_t weak_bits_found;
    uint32_t index_marks_found;
    
    bool initialized;
} hfe_ctx_t;

/*============================================================================
 * LUT TABLES
 *============================================================================*/

/* Bit reversal LUT (for HFE interleaved format) */
static const uint8_t bit_reverse_lut[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
    0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
    0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
    0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
    0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
    0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
    0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
    0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
    0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
    0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write_le16(uint8_t* p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/**
 * @brief Get error string
 */
const char* hfe_error_string(hfe_error_t err) {
    if (err >= HFE_ERR_COUNT) return "Unknown error";
    return hfe_error_names[err];
}

/**
 * @brief Get encoding name
 */
const char* hfe_encoding_name(int encoding) {
    switch (encoding) {
        case HFE_ENCODING_ISOIBM_MFM:   return "IBM MFM";
        case HFE_ENCODING_AMIGA_MFM:    return "Amiga MFM";
        case HFE_ENCODING_ISOIBM_FM:    return "IBM FM";
        case HFE_ENCODING_EMU_FM:       return "EMU FM";
        case HFE_ENCODING_APPLE_GCR1:   return "Apple GCR 5-3";
        case HFE_ENCODING_APPLE_GCR2:   return "Apple GCR 6-2";
        case HFE_ENCODING_HDDD_A2_GCR1: return "HDDD A2 GCR 5-3";
        case HFE_ENCODING_HDDD_A2_GCR2: return "HDDD A2 GCR 6-2";
        default:                         return "Unknown";
    }
}

/**
 * @brief Get interface mode name
 */
const char* hfe_interface_name(int mode) {
    static const char* names[] = {
        "IBM PC DD", "IBM PC HD", "Atari ST DD", "Atari ST HD",
        "Amiga DD", "Amiga HD", "CPC DD", "Shugart DD",
        "IBM PC ED", "MSX2 DD", "C64 DD", "EMU Shugart"
    };
    if (mode < 0 || mode > 11) return "Unknown";
    return names[mode];
}

/**
 * @brief Check if encoding is HDDD A2 (Apple II specific)
 */
static bool is_hddd_a2_encoding(int encoding) {
    return (encoding == HFE_ENCODING_HDDD_A2_GCR1 ||
            encoding == HFE_ENCODING_HDDD_A2_GCR2);
}

/*============================================================================
 * HFE v3 OPCODE DECODER
 *============================================================================*/

/**
 * @brief Decode HFE v3 track with opcodes
 *
 * HFE v3 uses in-band opcodes for:
 * - Index pulse markers
 * - Bitrate changes
 * - Weak/random bits
 * - Bit skipping (for precise timing)
 */
static int decode_hfev3_track(
    const uint8_t* raw_data,
    uint32_t raw_len,
    hfe_track_side_t* side,
    uint32_t base_bitrate,
    hfe_ctx_t* ctx
) {
    if (!raw_data || !side || raw_len == 0) {
        return HFE_ERR_NULL_PARAM;
    }
    
    /* Allocate output buffers (max same size as input) */
    side->data = calloc(1, raw_len);
    side->flakybitmap = calloc(1, raw_len);
    side->indexbitmap = calloc(1, raw_len);
    side->timing = calloc(raw_len, sizeof(uint32_t));
    
    if (!side->data || !side->flakybitmap || 
        !side->indexbitmap || !side->timing) {
        free(side->data);
        free(side->flakybitmap);
        free(side->indexbitmap);
        free(side->timing);
        return HFE_ERR_ALLOC;
    }
    
    uint32_t bitrate = base_bitrate;
    uint32_t bit_offset_out = 0;
    uint32_t byte_idx_out = 0;
    int skip_bits = 0;
    
    for (uint32_t i = 0; i < raw_len; i++) {
        uint8_t byte = raw_data[i];
        
        /* Check for opcode */
        if ((byte & HFE_OPCODE_MASK) == HFE_OPCODE_MASK) {
            switch (byte) {
                case HFE_NOP_OPCODE:
                    /* No operation - skip this byte */
                    break;
                    
                case HFE_SETINDEX_OPCODE:
                    /* Mark index pulse at current position */
                    if (byte_idx_out < raw_len) {
                        /* Set 256 bytes of index marker */
                        uint32_t mark_len = (byte_idx_out + 256 <= raw_len) ? 
                                           256 : (raw_len - byte_idx_out);
                        memset(side->indexbitmap + byte_idx_out, 0xFF, mark_len);
                        if (ctx) ctx->index_marks_found++;
                    }
                    break;
                    
                case HFE_SETBITRATE_OPCODE:
                    /* Next byte is bitrate divisor */
                    if (i + 1 < raw_len) {
                        uint8_t divisor = raw_data[++i];
                        if (divisor > 0) {
                            bitrate = HFE_FLOPPYEMU_FREQ / (divisor * 2);
                        }
                    }
                    break;
                    
                case HFE_SKIPBITS_OPCODE:
                    /* Next byte is number of bits to skip */
                    if (i + 1 < raw_len) {
                        skip_bits = raw_data[++i] & 0x07;
                    }
                    break;
                    
                case HFE_RAND_OPCODE:
                    /* Random/weak bits - mark as flaky */
                    if (byte_idx_out < raw_len) {
                        /* Generate pseudo-random data */
                        uint8_t rand_byte = (uint8_t)(rand() & 0x54);
                        
                        /* Apply skip bits */
                        int bits_to_write = 8 - skip_bits;
                        
                        side->data[byte_idx_out] = rand_byte;
                        side->flakybitmap[byte_idx_out] = 0xFF;
                        side->timing[byte_idx_out] = bitrate;
                        
                        byte_idx_out++;
                        bit_offset_out += bits_to_write;
                        skip_bits = 0;
                        
                        if (ctx) ctx->weak_bits_found++;
                    }
                    break;
                    
                default:
                    /* Unknown opcode - treat as data */
                    break;
            }
        } else {
            /* Regular data byte */
            if (byte_idx_out < raw_len) {
                /* Apply bit reversal (HFE format quirk) */
                side->data[byte_idx_out] = bit_reverse_lut[byte];
                side->timing[byte_idx_out] = bitrate;
                
                byte_idx_out++;
                bit_offset_out += (8 - skip_bits);
                skip_bits = 0;
            }
        }
    }
    
    side->tracklen_bytes = byte_idx_out;
    side->data_len = bit_offset_out;
    
    return HFE_OK;
}

/**
 * @brief Decode HFE v1/v2 track (no opcodes)
 */
static int decode_hfe_v1_track(
    const uint8_t* raw_data,
    uint32_t raw_len,
    hfe_track_side_t* side,
    uint32_t bitrate
) {
    if (!raw_data || !side || raw_len == 0) {
        return HFE_ERR_NULL_PARAM;
    }
    
    side->data = malloc(raw_len);
    side->timing = calloc(raw_len, sizeof(uint32_t));
    
    if (!side->data || !side->timing) {
        free(side->data);
        free(side->timing);
        return HFE_ERR_ALLOC;
    }
    
    /* Simple bit reversal for v1/v2 */
    for (uint32_t i = 0; i < raw_len; i++) {
        side->data[i] = bit_reverse_lut[raw_data[i]];
        side->timing[i] = bitrate;
    }
    
    side->tracklen_bytes = raw_len;
    side->data_len = raw_len * 8;
    
    /* No flaky bits or index in v1/v2 */
    side->flakybitmap = NULL;
    side->indexbitmap = NULL;
    
    return HFE_OK;
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open HFE file for reading
 */
hfe_ctx_t* hfe_open(const char* path) {
    if (!path) return NULL;
    
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Allocate context */
    hfe_ctx_t* ctx = calloc(1, sizeof(hfe_ctx_t));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    strncpy(ctx->filename, path, sizeof(ctx->filename) - 1);
    
    /* Read header */
    if (fread(&ctx->header, sizeof(hfe_header_t), 1, f) != 1) {
        fclose(f);
        free(ctx);
        return NULL;
    }
    
    /* Check signature */
    if (memcmp(ctx->header.signature, HFE_V3_SIGNATURE, 8) == 0) {
        ctx->version = 3;
    } else if (memcmp(ctx->header.signature, HFE_V1_SIGNATURE, 8) == 0) {
        ctx->version = 1;
    } else {
        fclose(f);
        free(ctx);
        return NULL;
    }
    
    /* Check for HDDD A2 encoding */
    ctx->is_hddd_a2 = is_hddd_a2_encoding(ctx->header.track_encoding);
    
    /* Read track list */
    uint32_t track_count = ctx->header.number_of_tracks;
    if (track_count > HFE_MAX_TRACKS) {
        track_count = HFE_MAX_TRACKS;
    }
    
    ctx->track_list = calloc(track_count, sizeof(hfe_track_entry_t));
    if (!ctx->track_list) {
        fclose(f);
        free(ctx);
        return NULL;
    }
    
    /* Seek to track list */
    if (fseek(f, ctx->header.track_list_offset * HFE_BLOCK_SIZE, SEEK_SET) != 0) { /* seek error */ }
    if (fread(ctx->track_list, sizeof(hfe_track_entry_t), 
              track_count, f) != track_count) {
        free(ctx->track_list);
        fclose(f);
        free(ctx);
        return NULL;
    }
    
    ctx->initialized = true;
    return ctx;
}

/**
 * @brief Close HFE context
 */
void hfe_close(hfe_ctx_t** ctx) {
    if (!ctx || !*ctx) return;
    
    if ((*ctx)->file) {
        fclose((*ctx)->file);
    }
    free((*ctx)->track_list);
    free(*ctx);
    *ctx = NULL;
}

/**
 * @brief Read track from HFE file
 */
int hfe_read_track(hfe_ctx_t* ctx, int track_num, hfe_track_t** track_out) {
    if (!ctx || !track_out || !ctx->initialized) {
        return HFE_ERR_NULL_PARAM;
    }
    
    if (track_num < 0 || track_num >= ctx->header.number_of_tracks) {
        return HFE_ERR_BAD_TRACK;
    }
    
    /* Get track entry */
    hfe_track_entry_t* entry = &ctx->track_list[track_num];
    
    if (entry->offset == 0 || entry->track_len == 0) {
        return HFE_ERR_BAD_TRACK;
    }
    
    /* Calculate track size (padded to 512) */
    uint32_t track_size = entry->track_len;
    if (track_size & 0x1FF) {
        track_size = (track_size & ~0x1FF) + 0x200;
    }
    
    /* Read raw track data */
    uint8_t* raw_track = malloc(track_size);
    if (!raw_track) {
        return HFE_ERR_ALLOC;
    }
    
    if (fseek(ctx->file, entry->offset * HFE_BLOCK_SIZE, SEEK_SET) != 0) { /* seek error */ }
    if (fread(raw_track, 1, track_size, ctx->file) != track_size) {
        free(raw_track);
        return HFE_ERR_FILE_READ;
    }
    
    /* Allocate track structure */
    hfe_track_t* track = calloc(1, sizeof(hfe_track_t));
    if (!track) {
        free(raw_track);
        return HFE_ERR_ALLOC;
    }
    
    track->track_number = track_num;
    track->number_of_sides = ctx->header.number_of_sides;
    track->rpm = ctx->header.uft_floppy_rpm;
    
    /* De-interleave track data (HFE stores sides interleaved in 256-byte blocks) */
    uint32_t side_len = track_size / 2;
    uint8_t* side0_data = malloc(side_len);
    uint8_t* side1_data = malloc(side_len);
    
    if (!side0_data || !side1_data) {
        free(raw_track);
        free(track);
        free(side0_data);
        free(side1_data);
        return HFE_ERR_ALLOC;
    }
    
    /* De-interleave: 256 bytes side0, 256 bytes side1, repeat */
    uint32_t blocks = track_size / 512;
    for (uint32_t b = 0; b < blocks; b++) {
        memcpy(side0_data + b * 256, raw_track + b * 512, 256);
        memcpy(side1_data + b * 256, raw_track + b * 512 + 256, 256);
    }
    
    free(raw_track);
    
    /* Calculate base bitrate */
    uint32_t bitrate = ctx->header.bitrate * 1000;
    if (bitrate == 0) {
        bitrate = 250000;  /* Default to DD */
    }
    
    /* Determine encoding for this track */
    int encoding = ctx->header.track_encoding;
    if (track_num == 0) {
        if (!ctx->header.track0s0_altencoding) {
            encoding = ctx->header.track0s0_encoding;
        }
    }
    
    /* Decode sides */
    int err;
    
    if (ctx->version == 3) {
        err = decode_hfev3_track(side0_data, side_len, &track->sides[0], bitrate, ctx);
    } else {
        err = decode_hfe_v1_track(side0_data, side_len, &track->sides[0], bitrate);
    }
    
    track->sides[0].encoding = encoding;
    
    if (err != HFE_OK) {
        free(side0_data);
        free(side1_data);
        free(track);
        return err;
    }
    
    if (track->number_of_sides > 1) {
        /* Handle track 0 side 1 alternate encoding */
        int encoding1 = ctx->header.track_encoding;
        if (track_num == 0) {
            if (!ctx->header.track0s1_altencoding) {
                encoding1 = ctx->header.track0s1_encoding;
            }
        }
        
        if (ctx->version == 3) {
            err = decode_hfev3_track(side1_data, side_len, &track->sides[1], bitrate, ctx);
        } else {
            err = decode_hfe_v1_track(side1_data, side_len, &track->sides[1], bitrate);
        }
        
        track->sides[1].encoding = encoding1;
    }
    
    free(side0_data);
    free(side1_data);
    
    if (err != HFE_OK) {
        hfe_free_track(&track);
        return err;
    }
    
    track->valid = true;
    ctx->tracks_read++;
    
    *track_out = track;
    return HFE_OK;
}

/**
 * @brief Free track data
 */
void hfe_free_track(hfe_track_t** track) {
    if (!track || !*track) return;
    
    hfe_track_t* t = *track;
    
    for (int s = 0; s < 2; s++) {
        free(t->sides[s].data);
        free(t->sides[s].flakybitmap);
        free(t->sides[s].indexbitmap);
        free(t->sides[s].timing);
    }
    
    free(t);
    *track = NULL;
}

/**
 * @brief Get disk info
 */
void hfe_get_info(
    hfe_ctx_t* ctx,
    int* tracks,
    int* sides,
    int* encoding,
    int* interface_mode,
    int* version,
    bool* is_hddd_a2
) {
    if (!ctx) return;
    
    if (tracks) *tracks = ctx->header.number_of_tracks;
    if (sides) *sides = ctx->header.number_of_sides;
    if (encoding) *encoding = ctx->header.track_encoding;
    if (interface_mode) *interface_mode = ctx->header.uft_floppy_interface_mode;
    if (version) *version = ctx->version;
    if (is_hddd_a2) *is_hddd_a2 = ctx->is_hddd_a2;
}

/**
 * @brief Get statistics
 */
void hfe_get_stats(
    hfe_ctx_t* ctx,
    uint32_t* tracks_read,
    uint32_t* weak_bits,
    uint32_t* index_marks
) {
    if (!ctx) return;
    
    if (tracks_read) *tracks_read = ctx->tracks_read;
    if (weak_bits) *weak_bits = ctx->weak_bits_found;
    if (index_marks) *index_marks = ctx->index_marks_found;
}

/**
 * @brief Check if file is valid HFE
 */
bool hfe_is_valid_file(const char* path) {
    if (!path) return false;
    
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    
    uint8_t sig[8];
    bool valid = false;
    
    if (fread(sig, 1, 8, f) == 8) {
        valid = (memcmp(sig, HFE_V1_SIGNATURE, 8) == 0) ||
                (memcmp(sig, HFE_V3_SIGNATURE, 8) == 0);
    }
    
    fclose(f);
    return valid;
}

/**
 * @brief Get HFE version from file
 */
int hfe_get_file_version(const char* path) {
    if (!path) return 0;
    
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    
    uint8_t sig[8];
    int version = 0;
    
    if (fread(sig, 1, 8, f) == 8) {
        if (memcmp(sig, HFE_V3_SIGNATURE, 8) == 0) {
            version = 3;
        } else if (memcmp(sig, HFE_V1_SIGNATURE, 8) == 0) {
            version = 1;
        }
    }
    
    fclose(f);
    return version;
}

/*============================================================================
 * HDDD A2 SPECIFIC FUNCTIONS
 *============================================================================*/

/**
 * @brief Decode HDDD A2 GCR track to Apple II nibbles
 *
 * HDDD A2 stores Apple II GCR data with FM clock bits inserted.
 * This function extracts the original nibble data.
 */
int hfe_decode_hddd_a2_track(
    const hfe_track_side_t* side,
    uint8_t* nibbles_out,
    uint32_t* nibbles_len
) {
    if (!side || !nibbles_out || !nibbles_len) {
        return HFE_ERR_NULL_PARAM;
    }
    
    if (!side->data || side->tracklen_bytes == 0) {
        return HFE_ERR_INVALID_DATA;
    }
    
    /* HDDD A2 format: each nibble is expanded to 2 bytes with FM clocks
     * Original nibble = extract even bits from the 16-bit pair
     */
    uint32_t out_idx = 0;
    
    for (uint32_t i = 0; i + 1 < side->tracklen_bytes; i += 2) {
        /* Combine two bytes into 16-bit word */
        uint16_t word = ((uint16_t)side->data[i] << 8) | side->data[i + 1];
        
        /* Extract even bits (data bits, skip clock bits) */
        uint8_t nibble = 0;
        nibble |= ((word >> 14) & 1) << 7;
        nibble |= ((word >> 12) & 1) << 6;
        nibble |= ((word >> 10) & 1) << 5;
        nibble |= ((word >> 8) & 1) << 4;
        nibble |= ((word >> 6) & 1) << 3;
        nibble |= ((word >> 4) & 1) << 2;
        nibble |= ((word >> 2) & 1) << 1;
        nibble |= (word & 1);
        
        nibbles_out[out_idx++] = nibble;
    }
    
    *nibbles_len = out_idx;
    return HFE_OK;
}

/*============================================================================
 * UNIT TEST
 *============================================================================*/

#ifdef HFE_V3_PARSER_TEST

#include <assert.h>
#include "uft/uft_compiler.h"

int main(void) {
    printf("=== HFE v3 Parser Unit Tests ===\n");
    
    /* Test 1: Header size */
    {
        assert(sizeof(hfe_header_t) == 512);
        printf("✓ Header size: 512 bytes\n");
    }
    
    /* Test 2: Track entry size */
    {
        assert(sizeof(hfe_track_entry_t) == 4);
        printf("✓ Track entry: 4 bytes\n");
    }
    
    /* Test 3: Bit reversal LUT */
    {
        assert(bit_reverse_lut[0x00] == 0x00);
        assert(bit_reverse_lut[0xFF] == 0xFF);
        assert(bit_reverse_lut[0x80] == 0x01);
        assert(bit_reverse_lut[0x01] == 0x80);
        assert(bit_reverse_lut[0xAA] == 0x55);
        printf("✓ Bit reversal LUT\n");
    }
    
    /* Test 4: Encoding names */
    {
        assert(strcmp(hfe_encoding_name(HFE_ENCODING_ISOIBM_MFM), "IBM MFM") == 0);
        assert(strcmp(hfe_encoding_name(HFE_ENCODING_HDDD_A2_GCR2), "HDDD A2 GCR 6-2") == 0);
        printf("✓ Encoding names\n");
    }
    
    /* Test 5: HDDD A2 detection */
    {
        assert(is_hddd_a2_encoding(HFE_ENCODING_HDDD_A2_GCR1) == true);
        assert(is_hddd_a2_encoding(HFE_ENCODING_HDDD_A2_GCR2) == true);
        assert(is_hddd_a2_encoding(HFE_ENCODING_ISOIBM_MFM) == false);
        printf("✓ HDDD A2 detection\n");
    }
    
    /* Test 6: Error strings */
    {
        assert(strcmp(hfe_error_string(HFE_OK), "OK") == 0);
        assert(strcmp(hfe_error_string(HFE_ERR_BAD_SIGNATURE), "Invalid HFE signature") == 0);
        printf("✓ Error strings\n");
    }
    
    /* Test 7: LE16 read/write */
    {
        uint8_t buf[2];
        write_le16(buf, 0x1234);
        assert(buf[0] == 0x34);
        assert(buf[1] == 0x12);
        assert(read_le16(buf) == 0x1234);
        printf("✓ LE16 read/write\n");
    }
    
    /* Test 8: File version detection (no file) */
    {
        assert(hfe_get_file_version(NULL) == 0);
        assert(hfe_get_file_version("/nonexistent/file.hfe") == 0);
        printf("✓ File version detection (null/missing)\n");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* HFE_V3_PARSER_TEST */
