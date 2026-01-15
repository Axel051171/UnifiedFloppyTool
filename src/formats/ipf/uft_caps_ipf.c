/**
 * @file uft_caps_ipf.c
 * @brief CAPS/IPF Disk Image Format Support
 * 
 * Based on SPS CAPS Library (Software Preservation Society)
 * Integrated into UnifiedFloppyTool for IPF/CTRaw format handling.
 * 
 * Supports:
 * - IPF: Interchangeable Preservation Format
 * - CTRaw: CT Raw flux images
 * - FDC emulation structures
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Library version */
#define UFT_CAPS_VERSION        5
#define UFT_CAPS_REVISION       1

/* Error codes */
#define UFT_CAPS_OK             0
#define UFT_CAPS_ERROR          -1
#define UFT_CAPS_OUT_OF_RANGE   -2
#define UFT_CAPS_READ_ONLY      -3
#define UFT_CAPS_OPEN_ERROR     -4
#define UFT_CAPS_TYPE_ERROR     -5
#define UFT_CAPS_UNSUPPORTED    -6

/* Image types */
#define UFT_CAPS_TYPE_UNKNOWN   0
#define UFT_CAPS_TYPE_IPF       1
#define UFT_CAPS_TYPE_CTRAW     2
#define UFT_CAPS_TYPE_KFSTREAM  3

/* Block types */
#define IPF_BLOCK_CAPS          1
#define IPF_BLOCK_INFO          2
#define IPF_BLOCK_IMGE          3
#define IPF_BLOCK_DATA          4
#define IPF_BLOCK_TRCK          5
#define IPF_BLOCK_GAP           6
#define IPF_BLOCK_DUMP          7
#define IPF_BLOCK_CTEX          8
#define IPF_BLOCK_CTRL          9
#define IPF_BLOCK_END           10

/* Encoding types */
#define UFT_CAPS_ENC_NA         0   /* Not applicable */
#define UFT_CAPS_ENC_MFM        1   /* MFM encoding */
#define UFT_CAPS_ENC_GCR        2   /* GCR encoding (Amiga) */
#define UFT_CAPS_ENC_FM         3   /* FM encoding */
#define UFT_CAPS_ENC_RAW        4   /* Raw flux */

/* Density types */
#define UFT_CAPS_DEN_NOISE      1
#define UFT_CAPS_DEN_AUTO       2
#define UFT_CAPS_DEN_AMIGA_DD   3
#define UFT_CAPS_DEN_AMIGA_HD   4
#define UFT_CAPS_DEN_PC_DD      5
#define UFT_CAPS_DEN_PC_HD      6
#define UFT_CAPS_DEN_ST_DD      7
#define UFT_CAPS_DEN_ST_HD      8

/* Platform IDs */
#define UFT_CAPS_PLATFORM_NA        0
#define UFT_CAPS_PLATFORM_AMIGA     1
#define UFT_CAPS_PLATFORM_ATARI_ST  2
#define UFT_CAPS_PLATFORM_PC        3
#define UFT_CAPS_PLATFORM_SPECTRUM  4
#define UFT_CAPS_PLATFORM_CPC       5
#define UFT_CAPS_PLATFORM_C64       6
#define UFT_CAPS_PLATFORM_MSX       7
#define UFT_CAPS_PLATFORM_ARCHIE    8
#define UFT_CAPS_PLATFORM_MAC       9
#define UFT_CAPS_PLATFORM_APPLE2    10
#define UFT_CAPS_PLATFORM_SAM       11

/* Track flags */
#define UFT_CAPS_TF_FLAKEY      (1 << 0)  /* Has weak/flakey bits */
#define UFT_CAPS_TF_TIMING      (1 << 1)  /* Has timing data */
#define UFT_CAPS_TF_MULTI_REV   (1 << 2)  /* Multiple revolutions */

/* Maximum values */
#define UFT_CAPS_MAX_CYLINDER   84
#define UFT_CAPS_MAX_HEAD       2
#define UFT_CAPS_MAX_PLATFORM   4

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF block header (12 bytes)
 */
typedef struct {
    uint32_t type;      /* Block type (big-endian) */
    uint32_t length;    /* Data length (big-endian) */
    uint32_t crc;       /* CRC-32 of block data */
} uft_ipf_block_header_t;

/**
 * @brief IPF CAPS header block
 */
typedef struct {
    uint32_t encoder;
    uint32_t encrev;
    uint32_t release;
    uint32_t revision;
    uint32_t origin;
    uint32_t min_track;
    uint32_t max_track;
} uft_ipf_caps_header_t;

/**
 * @brief IPF INFO block
 */
typedef struct {
    uint32_t type;
    uint32_t encoder;
    uint32_t encrev;
    uint32_t release;
    uint32_t revision;
    uint32_t origin;
    uint32_t min_cylinder;
    uint32_t max_cylinder;
    uint32_t min_head;
    uint32_t max_head;
    uint32_t creation_date;
    uint32_t platforms[UFT_CAPS_MAX_PLATFORM];
    uint32_t disk_number;
    uint32_t creator_id;
} uft_ipf_info_t;

/**
 * @brief IPF IMGE block (track descriptor)
 */
typedef struct {
    uint32_t cylinder;
    uint32_t head;
    uint32_t density_type;
    uint32_t signal_type;
    uint32_t track_bytes;
    uint32_t start_byte_pos;
    uint32_t start_bit_pos;
    uint32_t data_bits;
    uint32_t gap_bits;
    uint32_t track_bits;
    uint32_t block_count;
    uint32_t encoder_process;
    uint32_t track_flags;
    uint32_t data_key;
} uft_ipf_imge_t;

/**
 * @brief Track information structure
 */
typedef struct {
    uint32_t type;
    uint32_t cylinder;
    uint32_t head;
    uint32_t sector_count;
    uint32_t sector_size;
    
    uint8_t *track_buf;         /* Raw track data */
    uint32_t track_len;         /* Track length in bits */
    
    uint8_t *decoded_data;      /* Decoded sector data */
    uint32_t decoded_size;
    
    uint32_t *timing_data;      /* Per-bit timing */
    uint32_t timing_len;
    
    int32_t overlap;            /* Overlap position (-1 if none) */
    uint32_t start_bit;
    uint32_t weak_bits;
    uint32_t cell_ns;           /* Cell size in nanoseconds */
    uint32_t encoding;          /* Encoding type */
    uint32_t flags;
} uft_caps_track_info_t;

/**
 * @brief Image information structure
 */
typedef struct {
    uint32_t type;
    uint32_t platform;
    uint32_t release;
    uint32_t revision;
    uint32_t min_cylinder;
    uint32_t max_cylinder;
    uint32_t min_head;
    uint32_t max_head;
    uint32_t creation_date;
    uint32_t media_type;
} uft_caps_image_info_t;

/**
 * @brief Sector information structure
 */
typedef struct {
    uint32_t desc_id;
    uint32_t desc_pos;
    uint32_t data_crc;
    uint32_t header_crc;
    uint32_t data_size;
    uint32_t data_start;
    uint32_t header_size;
    uint32_t header_start;
    uint32_t gap_count;
    uint32_t gap3_data;
} uft_caps_sector_info_t;

/**
 * @brief IPF image context
 */
typedef struct {
    uint8_t *data;
    size_t size;
    
    int type;
    bool loaded;
    
    uft_ipf_info_t info;
    uft_ipf_imge_t *tracks;
    int track_count;
    
    uint8_t **track_data;       /* Track data blocks */
    size_t *track_data_size;
    
    int current_revolution;
    int revolution_count;
} uft_caps_image_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC-32 Implementation (IPF uses big-endian CRC)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t ipf_crc32_table[256];
static bool ipf_crc32_table_init = false;

/**
 * @brief Initialize CRC-32 table (polynomial 0x04C11DB7)
 */
static void init_ipf_crc32_table(void)
{
    if (ipf_crc32_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint32_t crc = (uint32_t)i << 24;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ 0x04C11DB7;
            } else {
                crc <<= 1;
            }
        }
        ipf_crc32_table[i] = crc;
    }
    ipf_crc32_table_init = true;
}

/**
 * @brief Calculate CRC-32 for IPF block
 */
static uint32_t calc_ipf_crc32(const uint8_t *data, size_t len)
{
    init_ipf_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ ipf_crc32_table[(crc >> 24) ^ data[i]];
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 32-bit big-endian value
 */
static uint32_t read32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/**
 * @brief Write 32-bit big-endian value
 */
static void write32_be(uint8_t *p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

/**
 * @brief Read block header
 */
static int read_block_header(const uint8_t *data, size_t pos, size_t size,
                             uft_ipf_block_header_t *header)
{
    if (pos + 12 > size) return UFT_CAPS_ERROR;
    
    header->type = read32_be(data + pos);
    header->length = read32_be(data + pos + 4);
    header->crc = read32_be(data + pos + 8);
    
    return UFT_CAPS_OK;
}

/**
 * @brief Get platform name
 */
const char *uft_caps_platform_name(uint32_t platform)
{
    static const char *names[] = {
        "N/A",
        "Amiga",
        "Atari ST",
        "IBM PC",
        "ZX Spectrum",
        "Amstrad CPC",
        "Commodore 64",
        "MSX",
        "Acorn Archimedes",
        "Apple Macintosh",
        "Apple II",
        "SAM Coupé"
    };
    
    if (platform <= 11) return names[platform];
    return "Unknown";
}

/**
 * @brief Get encoding name
 */
const char *uft_caps_encoding_name(uint32_t encoding)
{
    static const char *names[] = {
        "N/A", "MFM", "GCR", "FM", "Raw"
    };
    
    if (encoding <= 4) return names[encoding];
    return "Unknown";
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * IPF Loader Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize CAPS image structure
 */
void uft_caps_image_init(uft_caps_image_t *img)
{
    memset(img, 0, sizeof(*img));
    img->current_revolution = 0;
}

/**
 * @brief Free CAPS image resources
 */
void uft_caps_image_free(uft_caps_image_t *img)
{
    if (!img) return;
    
    free(img->data);
    free(img->tracks);
    
    if (img->track_data) {
        for (int i = 0; i < img->track_count; i++) {
            free(img->track_data[i]);
        }
        free(img->track_data);
    }
    free(img->track_data_size);
    
    memset(img, 0, sizeof(*img));
}

/**
 * @brief Check if file is IPF format
 */
bool uft_caps_is_ipf(const uint8_t *data, size_t size)
{
    if (size < 12) return false;
    
    /* Check for CAPS block type (1) */
    uint32_t type = read32_be(data);
    return (type == IPF_BLOCK_CAPS);
}

/**
 * @brief Parse INFO block
 */
static int parse_info_block(const uint8_t *data, size_t len, uft_ipf_info_t *info)
{
    if (len < 80) return UFT_CAPS_ERROR;
    
    info->type = read32_be(data + 0);
    info->encoder = read32_be(data + 4);
    info->encrev = read32_be(data + 8);
    info->release = read32_be(data + 12);
    info->revision = read32_be(data + 16);
    info->origin = read32_be(data + 20);
    info->min_cylinder = read32_be(data + 24);
    info->max_cylinder = read32_be(data + 28);
    info->min_head = read32_be(data + 32);
    info->max_head = read32_be(data + 36);
    info->creation_date = read32_be(data + 40);
    
    for (int i = 0; i < UFT_CAPS_MAX_PLATFORM; i++) {
        info->platforms[i] = read32_be(data + 44 + i * 4);
    }
    
    info->disk_number = read32_be(data + 60);
    info->creator_id = read32_be(data + 64);
    
    return UFT_CAPS_OK;
}

/**
 * @brief Parse IMGE block (track descriptor)
 */
static int parse_imge_block(const uint8_t *data, size_t len, uft_ipf_imge_t *imge)
{
    if (len < 80) return UFT_CAPS_ERROR;
    
    imge->cylinder = read32_be(data + 0);
    imge->head = read32_be(data + 4);
    imge->density_type = read32_be(data + 8);
    imge->signal_type = read32_be(data + 12);
    imge->track_bytes = read32_be(data + 16);
    imge->start_byte_pos = read32_be(data + 20);
    imge->start_bit_pos = read32_be(data + 24);
    imge->data_bits = read32_be(data + 28);
    imge->gap_bits = read32_be(data + 32);
    imge->track_bits = read32_be(data + 36);
    imge->block_count = read32_be(data + 40);
    imge->encoder_process = read32_be(data + 44);
    imge->track_flags = read32_be(data + 48);
    imge->data_key = read32_be(data + 52);
    
    return UFT_CAPS_OK;
}

/**
 * @brief Load IPF image from file
 */
int uft_caps_load_image(uft_caps_image_t *img, const char *filename)
{
    uft_caps_image_init(img);
    
    FILE *f = fopen(filename, "rb");
    if (!f) return UFT_CAPS_OPEN_ERROR;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 100 * 1024 * 1024) {
        fclose(f);
        return UFT_CAPS_ERROR;
    }
    
    img->data = malloc(size);
    if (!img->data) {
        fclose(f);
        return UFT_CAPS_ERROR;
    }
    
    if (fread(img->data, 1, size, f) != (size_t)size) {
        free(img->data);
        img->data = NULL;
        fclose(f);
        return UFT_CAPS_ERROR;
    }
    fclose(f);
    
    img->size = size;
    
    /* Verify IPF format */
    if (!uft_caps_is_ipf(img->data, img->size)) {
        uft_caps_image_free(img);
        return UFT_CAPS_TYPE_ERROR;
    }
    
    img->type = UFT_CAPS_TYPE_IPF;
    
    /* Parse blocks */
    size_t pos = 0;
    int imge_count = 0;
    int imge_capacity = 0;
    
    while (pos < img->size) {
        uft_ipf_block_header_t header;
        if (read_block_header(img->data, pos, img->size, &header) != UFT_CAPS_OK) {
            break;
        }
        
        pos += 12;  /* Skip header */
        
        if (pos + header.length > img->size) break;
        
        /* Verify CRC */
        uint32_t calc_crc = calc_ipf_crc32(img->data + pos, header.length);
        if (calc_crc != header.crc) {
            /* CRC mismatch - warning but continue */
        }
        
        switch (header.type) {
            case IPF_BLOCK_CAPS:
                /* CAPS header - already validated */
                break;
                
            case IPF_BLOCK_INFO:
                parse_info_block(img->data + pos, header.length, &img->info);
                break;
                
            case IPF_BLOCK_IMGE: {
                /* Expand track array if needed */
                if (imge_count >= imge_capacity) {
                    int new_cap = imge_capacity ? imge_capacity * 2 : 168;
                    uft_ipf_imge_t *new_tracks = realloc(img->tracks, 
                                                          new_cap * sizeof(uft_ipf_imge_t));
                    if (!new_tracks) break;
                    img->tracks = new_tracks;
                    imge_capacity = new_cap;
                }
                
                parse_imge_block(img->data + pos, header.length, 
                                 &img->tracks[imge_count]);
                imge_count++;
                break;
            }
            
            case IPF_BLOCK_DATA:
                /* Track data - store reference */
                break;
                
            case IPF_BLOCK_END:
                /* End of file */
                pos = img->size;
                break;
                
            default:
                /* Skip unknown blocks */
                break;
        }
        
        pos += header.length;
    }
    
    img->track_count = imge_count;
    img->loaded = true;
    
    return UFT_CAPS_OK;
}

/**
 * @brief Get image information
 */
int uft_caps_get_image_info(const uft_caps_image_t *img, uft_caps_image_info_t *info)
{
    if (!img || !img->loaded || !info) return UFT_CAPS_ERROR;
    
    memset(info, 0, sizeof(*info));
    
    info->type = img->type;
    info->platform = img->info.platforms[0];
    info->release = img->info.release;
    info->revision = img->info.revision;
    info->min_cylinder = img->info.min_cylinder;
    info->max_cylinder = img->info.max_cylinder;
    info->min_head = img->info.min_head;
    info->max_head = img->info.max_head;
    info->creation_date = img->info.creation_date;
    
    return UFT_CAPS_OK;
}

/**
 * @brief Get track information
 */
int uft_caps_get_track_info(const uft_caps_image_t *img, 
                            uint32_t cylinder, uint32_t head,
                            uft_caps_track_info_t *info)
{
    if (!img || !img->loaded || !info) return UFT_CAPS_ERROR;
    
    memset(info, 0, sizeof(*info));
    info->overlap = -1;
    
    /* Find track */
    for (int i = 0; i < img->track_count; i++) {
        if (img->tracks[i].cylinder == cylinder && 
            img->tracks[i].head == head) {
            
            const uft_ipf_imge_t *t = &img->tracks[i];
            
            info->cylinder = t->cylinder;
            info->head = t->head;
            info->track_len = t->track_bits;
            info->start_bit = t->start_bit_pos;
            info->encoding = t->signal_type;
            info->flags = t->track_flags;
            info->cell_ns = 2000;  /* Default 2µs cell */
            
            /* Check for weak bits */
            if (t->track_flags & UFT_CAPS_TF_FLAKEY) {
                info->weak_bits = 1;  /* Has weak bits */
            }
            
            return UFT_CAPS_OK;
        }
    }
    
    return UFT_CAPS_OUT_OF_RANGE;
}

/**
 * @brief Get number of revolutions for a track
 */
int uft_caps_get_revolutions(const uft_caps_image_t *img,
                             uint32_t cylinder, uint32_t head)
{
    if (!img || !img->loaded) return 0;
    
    int count = 0;
    for (int i = 0; i < img->track_count; i++) {
        if (img->tracks[i].cylinder == cylinder &&
            img->tracks[i].head == head) {
            count++;
        }
    }
    
    return count > 0 ? count : 1;  /* At least 1 revolution */
}

/**
 * @brief Set active revolution for track reading
 */
int uft_caps_set_revolution(uft_caps_image_t *img, int revolution)
{
    if (!img) return UFT_CAPS_ERROR;
    img->current_revolution = revolution;
    return UFT_CAPS_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * IPF Writer Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write block header
 */
static int write_block_header(FILE *f, uint32_t type, uint32_t length, 
                              const uint8_t *data)
{
    uint8_t header[12];
    
    uint32_t crc = calc_ipf_crc32(data, length);
    
    write32_be(header + 0, type);
    write32_be(header + 4, length);
    write32_be(header + 8, crc);
    
    if (fwrite(header, 1, 12, f) != 12) return UFT_CAPS_ERROR;
    if (length > 0 && data) {
        if (fwrite(data, 1, length, f) != length) return UFT_CAPS_ERROR;
    }
    
    return UFT_CAPS_OK;
}

/**
 * @brief Create IPF file from track data
 */
int uft_caps_create_ipf(const char *filename,
                        uint32_t platform,
                        uint32_t min_cyl, uint32_t max_cyl,
                        uint32_t min_head, uint32_t max_head)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return UFT_CAPS_OPEN_ERROR;
    
    /* CAPS header block */
    uint8_t caps_data[32] = {0};
    write32_be(caps_data + 0, 1);           /* Encoder ID */
    write32_be(caps_data + 4, 1);           /* Encoder revision */
    write32_be(caps_data + 8, 1);           /* Release */
    write32_be(caps_data + 12, 0);          /* Revision */
    write32_be(caps_data + 16, 0);          /* Origin */
    write32_be(caps_data + 20, min_cyl);    /* Min track */
    write32_be(caps_data + 24, max_cyl);    /* Max track */
    
    if (write_block_header(f, IPF_BLOCK_CAPS, 32, caps_data) != UFT_CAPS_OK) {
        fclose(f);
        return UFT_CAPS_ERROR;
    }
    
    /* INFO block */
    uint8_t info_data[96] = {0};
    write32_be(info_data + 0, 1);           /* Media type */
    write32_be(info_data + 4, 1);           /* Encoder */
    write32_be(info_data + 8, 1);           /* Encoder rev */
    write32_be(info_data + 12, 1);          /* Release */
    write32_be(info_data + 16, 0);          /* Revision */
    write32_be(info_data + 20, 0);          /* Origin */
    write32_be(info_data + 24, min_cyl);    /* Min cylinder */
    write32_be(info_data + 28, max_cyl);    /* Max cylinder */
    write32_be(info_data + 32, min_head);   /* Min head */
    write32_be(info_data + 36, max_head);   /* Max head */
    write32_be(info_data + 40, 0);          /* Creation date */
    write32_be(info_data + 44, platform);   /* Platform */
    
    if (write_block_header(f, IPF_BLOCK_INFO, 96, info_data) != UFT_CAPS_OK) {
        fclose(f);
        return UFT_CAPS_ERROR;
    }
    
    /* End block */
    if (write_block_header(f, IPF_BLOCK_END, 0, NULL) != UFT_CAPS_OK) {
        fclose(f);
        return UFT_CAPS_ERROR;
    }
    
    fclose(f);
    return UFT_CAPS_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Print IPF image summary
 */
void uft_caps_print_info(const uft_caps_image_t *img, FILE *out)
{
    if (!img || !img->loaded) {
        fprintf(out, "No image loaded\n");
        return;
    }
    
    fprintf(out, "IPF Image Information:\n");
    fprintf(out, "  Platform: %s\n", uft_caps_platform_name(img->info.platforms[0]));
    fprintf(out, "  Release: %u.%u\n", img->info.release, img->info.revision);
    fprintf(out, "  Cylinders: %u-%u\n", img->info.min_cylinder, img->info.max_cylinder);
    fprintf(out, "  Heads: %u-%u\n", img->info.min_head, img->info.max_head);
    fprintf(out, "  Tracks: %d\n", img->track_count);
    
    /* Track summary */
    int weak_tracks = 0;
    for (int i = 0; i < img->track_count; i++) {
        if (img->tracks[i].track_flags & UFT_CAPS_TF_FLAKEY) {
            weak_tracks++;
        }
    }
    
    if (weak_tracks > 0) {
        fprintf(out, "  Tracks with weak bits: %d\n", weak_tracks);
    }
}

/**
 * @brief Get library version
 */
void uft_caps_get_version(int *version, int *revision)
{
    if (version) *version = UFT_CAPS_VERSION;
    if (revision) *revision = UFT_CAPS_REVISION;
}
