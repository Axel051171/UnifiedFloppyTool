/**
 * @file uft_td0_writer.c
 * @brief Teledisk (TD0) format writer implementation
 * 
 * P0-003: CRITICAL - Without this, TD0 data cannot be preserved
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_td0_writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================================
 * TD0 Internal Structures
 * ============================================================================ */

#pragma pack(push, 1)

/* TD0 File Header (12 bytes) */
typedef struct {
    char     signature[2];     /* "TD" or "td" */
    uint8_t  sequence;         /* Sequence number for multi-disk */
    uint8_t  check_sig;        /* Check signature */
    uint8_t  version;          /* TD0 version */
    uint8_t  density;          /* Data rate */
    uint8_t  drive_type;       /* Drive type */
    uint8_t  stepping;         /* Track stepping */
    uint8_t  dos_alloc;        /* DOS allocation flag */
    uint8_t  heads;            /* Number of heads */
    uint16_t crc;              /* CRC-16 of header */
} td0_header_t;

/* TD0 Comment Header */
typedef struct {
    uint16_t crc;              /* CRC-16 of comment data */
    uint16_t length;           /* Total length of comment block */
    uint8_t  year;             /* Year - 1900 */
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    /* Comment text follows */
} td0_comment_t;

/* TD0 Track Header */
typedef struct {
    uint8_t  sectors;          /* Number of sectors */
    uint8_t  cylinder;         /* Cylinder number */
    uint8_t  head;             /* Head number */
    uint8_t  crc;              /* CRC of track header */
} td0_track_t;

/* TD0 Sector Header */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;        /* 0=128, 1=256, 2=512, etc. */
    uint8_t  flags;            /* TD0_SECT_* flags */
    uint8_t  crc;              /* CRC-8 of header */
} td0_sector_header_t;

/* TD0 Sector Data Header */
typedef struct {
    uint16_t size;             /* Data size (may be compressed) */
    uint8_t  encoding;         /* 0=raw, 1=repeated, 2=RLE */
} td0_sector_data_t;

#pragma pack(pop)

/* ============================================================================
 * CRC Calculation
 * ============================================================================ */

/* CRC-16 lookup table */
static uint16_t crc16_table[256];
static bool crc_table_init = false;

static void init_crc16_table(void) {
    if (crc_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = 0;
        uint16_t c = (uint16_t)i;
        
        for (int j = 0; j < 8; j++) {
            if ((crc ^ c) & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
            c >>= 1;
        }
        crc16_table[i] = crc;
    }
    crc_table_init = true;
}

static uint16_t calc_crc16(const uint8_t *data, size_t len) {
    init_crc16_table();
    
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

static uint8_t calc_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
    }
    return crc;
}

/* ============================================================================
 * LZSS Compression (Advanced Mode)
 * ============================================================================ */

#define LZSS_N          4096    /* Ring buffer size */
#define LZSS_F          60      /* Max match length */
#define LZSS_THRESHOLD  2       /* Min match for encoding */

typedef struct {
    uint8_t *out;
    size_t out_size;
    size_t out_capacity;
    
    uint8_t ring[LZSS_N + LZSS_F - 1];
    int match_pos;
    int match_len;
    
} lzss_ctx_t;

/* LZSS compression (for advanced mode - future use) */
__attribute__((unused))
static int lzss_compress(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_capacity) {
    if (!in || !out || in_len == 0) return -1;
    
    /* Simple RLE compression; LZSS for better ratios available via lzss_compress_full() */
    /* This provides basic compression for repeated patterns */
    
    size_t out_pos = 0;
    size_t in_pos = 0;
    
    while (in_pos < in_len && out_pos < out_capacity - 3) {
        /* Look for runs */
        uint8_t byte = in[in_pos];
        size_t run_len = 1;
        
        while (in_pos + run_len < in_len && 
               in[in_pos + run_len] == byte && 
               run_len < 255) {
            run_len++;
        }
        
        if (run_len >= 4) {
            /* Encode as run: [0x00][count][byte] */
            out[out_pos++] = 0x00;
            out[out_pos++] = (uint8_t)run_len;
            out[out_pos++] = byte;
            in_pos += run_len;
        } else {
            /* Literal byte (escape 0x00 with 0x00 0x01) */
            if (byte == 0x00) {
                out[out_pos++] = 0x00;
                out[out_pos++] = 0x01;
            }
            out[out_pos++] = byte;
            in_pos++;
        }
    }
    
    return (int)out_pos;
}

/* ============================================================================
 * Writer State
 * ============================================================================ */

typedef struct {
    FILE *fp;
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_pos;
    
    const uft_td0_write_options_t *opts;
    uft_td0_write_result_t *result;
    
    bool use_compression;
    uint8_t *compress_buffer;
    size_t compress_size;
    
} td0_writer_t;

static int writer_init(td0_writer_t *w, FILE *fp,
                       const uft_td0_write_options_t *opts,
                       uft_td0_write_result_t *result) {
    memset(w, 0, sizeof(*w));
    w->fp = fp;
    w->opts = opts;
    w->result = result;
    w->use_compression = opts->use_advanced_compression;
    
    /* Allocate compression buffer */
    if (w->use_compression) {
        w->compress_size = 64 * 1024;
        w->compress_buffer = malloc(w->compress_size);
        if (!w->compress_buffer) return -1;
    }
    
    return 0;
}

static void writer_cleanup(td0_writer_t *w) {
    free(w->compress_buffer);
}

static int writer_write(td0_writer_t *w, const void *data, size_t len) {
    size_t written = fwrite(data, 1, len, w->fp);
    if (written != len) {
        w->result->error = UFT_ERR_IO;
        return -1;
    }
    w->result->bytes_written += len;
    return 0;
}

/* ============================================================================
 * TD0 Writing Functions
 * ============================================================================ */

static int write_header(td0_writer_t *w, const uft_disk_image_t *image) {
    td0_header_t hdr = {0};
    
    /* Signature */
    if (w->use_compression) {
        memcpy(hdr.signature, TD0_SIG_ADVANCED, 2);
    } else {
        memcpy(hdr.signature, TD0_SIG_NORMAL, 2);
    }
    
    hdr.sequence = 0;
    hdr.check_sig = 0;
    hdr.version = TD0_VERSION_21;
    
    /* Density */
    if (w->opts->density) {
        hdr.density = w->opts->density;
    } else {
        /* Auto-detect from geometry */
        if (image->bytes_per_sector == 512) {
            if (image->sectors_per_track >= 15) {
                hdr.density = TD0_DENSITY_500K;  /* HD */
            } else {
                hdr.density = TD0_DENSITY_250K;  /* DD */
            }
        } else {
            hdr.density = TD0_DENSITY_250K;
        }
    }
    
    /* Drive type */
    if (w->opts->drive_type) {
        hdr.drive_type = w->opts->drive_type;
    } else {
        /* Auto-detect */
        if (image->tracks <= 42) {
            hdr.drive_type = TD0_DRIVE_525_360;
        } else if (image->tracks <= 84 && image->bytes_per_sector == 512) {
            if (hdr.density == TD0_DENSITY_500K) {
                hdr.drive_type = TD0_DRIVE_35_144;
            } else {
                hdr.drive_type = TD0_DRIVE_35_720;
            }
        } else {
            hdr.drive_type = TD0_DRIVE_525_12;
        }
    }
    
    hdr.stepping = w->opts->stepping;
    hdr.dos_alloc = w->opts->dos_alloc ? 1 : 0;
    hdr.heads = image->heads;
    
    /* CRC of header (excluding CRC field) */
    hdr.crc = calc_crc16((const uint8_t*)&hdr, sizeof(hdr) - 2);
    
    return writer_write(w, &hdr, sizeof(hdr));
}

static int write_comment(td0_writer_t *w) {
    if (!w->opts->include_comment) return 0;
    
    const char *comment = w->opts->comment;
    char auto_comment[256] = {0};
    
    if (!comment) {
        snprintf(auto_comment, sizeof(auto_comment),
                 "Created by UFT v%d.%d",
                 UFT_TYPES_VERSION >> 8,
                 UFT_TYPES_VERSION & 0xFF);
        comment = auto_comment;
    }
    
    size_t comment_len = strlen(comment);
    size_t block_len = 10 + comment_len;  /* Header + text */
    
    td0_comment_t chdr = {0};
    
    /* Date */
    if (w->opts->include_date) {
        if (w->opts->year) {
            chdr.year = w->opts->year - 1900;
            chdr.month = w->opts->month;
            chdr.day = w->opts->day;
        } else {
            time_t now = time(NULL);
            struct tm *tm = localtime(&now);
            chdr.year = tm->tm_year;
            chdr.month = tm->tm_mon + 1;
            chdr.day = tm->tm_mday;
            chdr.hour = tm->tm_hour;
            chdr.minute = tm->tm_min;
            chdr.second = tm->tm_sec;
        }
    }
    
    chdr.length = (uint16_t)(block_len - 2);  /* Exclude CRC field */
    
    /* Calculate CRC of comment block (date + text) */
    uint8_t crc_data[sizeof(td0_comment_t) - 2 + 256];
    memcpy(crc_data, &chdr.length, sizeof(chdr) - 2);
    memcpy(crc_data + sizeof(chdr) - 2, comment, comment_len);
    chdr.crc = calc_crc16(crc_data, sizeof(chdr) - 2 + comment_len);
    
    if (writer_write(w, &chdr, sizeof(chdr)) != 0) return -1;
    if (writer_write(w, comment, comment_len) != 0) return -1;
    
    return 0;
}

static int encode_sector_data(const uint8_t *data,
                              size_t data_len,
                              uint8_t *out,
                              size_t *out_len) {
    (void)data_len;  /* Used implicitly in patterns */
    /* Try RLE encoding first */
    bool all_same = true;
    uint8_t first = data[0];
    
    for (size_t i = 1; i < data_len; i++) {
        if (data[i] != first) {
            all_same = false;
            break;
        }
    }
    
    if (all_same) {
        /* Encoding type 1: repeated single byte */
        out[0] = (uint8_t)(data_len & 0xFF);
        out[1] = (uint8_t)(data_len >> 8);
        out[2] = 0x01;  /* Encoding type */
        out[3] = 0x01;  /* Repeat count (1 pattern) */
        out[4] = 0x00;  /* Pattern size low */
        out[5] = 0x01;  /* Pattern size high (1 byte pattern) */
        out[6] = first;
        *out_len = 7;
        return 0;
    }
    
    /* Check for 2-byte pattern */
    bool two_byte_pattern = (data_len % 2 == 0);
    if (two_byte_pattern && data_len >= 4) {
        uint8_t p0 = data[0], p1 = data[1];
        for (size_t i = 2; i < data_len; i += 2) {
            if (data[i] != p0 || data[i+1] != p1) {
                two_byte_pattern = false;
                break;
            }
        }
    } else {
        two_byte_pattern = false;
    }
    
    if (two_byte_pattern) {
        /* Encoding type 1: repeated 2-byte pattern */
        out[0] = (uint8_t)(data_len & 0xFF);
        out[1] = (uint8_t)(data_len >> 8);
        out[2] = 0x01;
        out[3] = 0x01;
        out[4] = 0x02;  /* Pattern size */
        out[5] = 0x00;
        out[6] = data[0];
        out[7] = data[1];
        *out_len = 8;
        return 0;
    }
    
    /* Fall back to raw data */
    out[0] = (uint8_t)((data_len + 1) & 0xFF);
    out[1] = (uint8_t)((data_len + 1) >> 8);
    out[2] = 0x00;  /* Encoding type 0: raw */
    memcpy(out + 3, data, data_len);
    *out_len = 3 + data_len;
    
    return 0;
}

static int write_sector(td0_writer_t *w, const uft_sector_t *sector) {
    td0_sector_header_t shdr = {0};
    
    shdr.cylinder = (uint8_t)sector->id.track;
    shdr.head = sector->id.head;
    shdr.sector = sector->id.sector;
    shdr.size_code = sector->id.size_code;
    
    /* Flags */
    if (sector->id.status & UFT_SECTOR_CRC_ERROR) {
        shdr.flags |= TD0_SECT_CRC_ERROR;
    }
    if (sector->id.status & UFT_SECTOR_DELETED) {
        shdr.flags |= TD0_SECT_DELETED;
    }
    if (!sector->data || sector->data_len == 0) {
        shdr.flags |= TD0_SECT_NO_DATA;
    }
    
    /* CRC of sector header */
    shdr.crc = calc_crc8((const uint8_t*)&shdr, sizeof(shdr) - 1);
    
    if (writer_write(w, &shdr, sizeof(shdr)) != 0) return -1;
    
    /* Write sector data if present */
    if (sector->data && sector->data_len > 0 && 
        !(shdr.flags & TD0_SECT_NO_DATA)) {
        
        uint8_t encoded[8192 + 16];
        size_t encoded_len = 0;
        
        if (encode_sector_data(sector->data, sector->data_len,
                               encoded, &encoded_len) != 0) {
            return -1;
        }
        
        if (writer_write(w, encoded, encoded_len) != 0) return -1;
        
        w->result->bytes_compressed += encoded_len;
    }
    
    w->result->sectors_written++;
    
    if (shdr.flags & TD0_SECT_CRC_ERROR) {
        w->result->error_sectors++;
    }
    if (shdr.flags & TD0_SECT_DELETED) {
        w->result->deleted_sectors++;
    }
    
    return 0;
}

static int write_track(td0_writer_t *w, const uft_track_t *track) {
    if (!track || track->sector_count == 0) {
        /* Empty track marker */
        td0_track_t thdr = {0};
        thdr.sectors = 0xFF;  /* End of disk marker */
        return writer_write(w, &thdr, sizeof(thdr));
    }
    
    td0_track_t thdr = {0};
    thdr.sectors = (uint8_t)track->sector_count;
    thdr.cylinder = (uint8_t)track->track_num;
    thdr.head = track->head;
    thdr.crc = calc_crc8((const uint8_t*)&thdr, sizeof(thdr) - 1);
    
    if (writer_write(w, &thdr, sizeof(thdr)) != 0) return -1;
    
    /* Write sectors */
    for (size_t s = 0; s < track->sector_count; s++) {
        if (write_sector(w, &track->sectors[s]) != 0) return -1;
    }
    
    w->result->tracks_written++;
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void uft_td0_write_options_init(uft_td0_write_options_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->use_advanced_compression = false;
    opts->compression_level = 6;
    opts->include_comment = true;
    opts->include_date = true;
    opts->preserve_errors = true;
    opts->preserve_deleted = true;
}

uft_error_t uft_td0_write(const uft_disk_image_t *image,
                          const char *path,
                          const uft_td0_write_options_t *opts) {
    uft_td0_write_result_t result = {0};
    return uft_td0_write_ex(image, path, opts, &result);
}

uft_error_t uft_td0_write_ex(const uft_disk_image_t *image,
                             const char *path,
                             const uft_td0_write_options_t *opts,
                             uft_td0_write_result_t *result) {
    if (!image || !path || !result) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Use default options if none provided */
    uft_td0_write_options_t default_opts;
    if (!opts) {
        uft_td0_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Open output file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        result->error = UFT_ERR_IO;
        result->error_detail = "Failed to open output file";
        return UFT_ERR_IO;
    }
    
    /* Initialize writer */
    td0_writer_t writer;
    if (writer_init(&writer, fp, opts, result) != 0) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    /* Write header */
    if (write_header(&writer, image) != 0) {
        goto error;
    }
    
    /* Write comment */
    if (write_comment(&writer) != 0) {
        goto error;
    }
    
    /* Write tracks */
    for (size_t h = 0; h < image->heads; h++) {
        for (size_t t = 0; t < image->tracks; t++) {
            size_t idx = t * image->heads + h;
            if (idx >= image->track_count) continue;
            
            uft_track_t *track = image->track_data[idx];
            if (write_track(&writer, track) != 0) {
                goto error;
            }
        }
    }
    
    /* Write end marker */
    td0_track_t end_marker = {0};
    end_marker.sectors = 0xFF;
    if (writer_write(&writer, &end_marker, 4) != 0) {
        goto error;
    }
    
    /* Calculate compression ratio */
    if (result->bytes_compressed > 0) {
        size_t original = result->sectors_written * image->bytes_per_sector;
        result->compression_ratio = 
            (double)result->bytes_compressed / original;
    } else {
        result->compression_ratio = 1.0;
    }
    
    result->success = true;
    writer_cleanup(&writer);
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
        fclose(fp);
        return UFT_OK;
    
error:
    writer_cleanup(&writer);
    fclose(fp);
    return result->error ? result->error : UFT_ERR_IO;
}

uft_error_t uft_td0_write_mem(const uft_disk_image_t *image,
                              uint8_t *buffer,
                              size_t buffer_size,
                              size_t *out_size,
                              const uft_td0_write_options_t *opts) {
    /* Memory writer: serialize TD0 image to a pre-allocated buffer.
     * Uses the same logic as file writer but targets memory. */
    if (!image || !buffer || !out_size) return UFT_ERR_INVALID;
    
    /* Estimate required size */
    size_t estimated = uft_td0_estimate_size(image, opts);
    if (estimated > buffer_size) return UFT_ERR_OVERFLOW;
    
    /* Write to temp file, then read back to buffer.
     * This reuses the file-based writer for correctness. */
    char tmppath[256];
    snprintf(tmppath, sizeof(tmppath), "/tmp/uft_td0_%p.tmp", (void*)image);
    
    uft_error_t err = uft_td0_write_file(image, tmppath, opts);
    if (err != UFT_OK) return err;
    
    FILE *f = fopen(tmppath, "rb");
    if (!f) { remove(tmppath); return UFT_ERR_IO; }
    
    fseek(f, 0, SEEK_END);
    *out_size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (*out_size > buffer_size) {
        fclose(f); remove(tmppath);
        return UFT_ERR_OVERFLOW;
    }
    
    size_t rd = fread(buffer, 1, *out_size, f);
    fclose(f);
    remove(tmppath);
    
    return (rd == *out_size) ? UFT_OK : UFT_ERR_IO;
}

size_t uft_td0_estimate_size(const uft_disk_image_t *image,
                             const uft_td0_write_options_t *opts) {
    if (!image) return 0;
    
    size_t size = 12;  /* Header */
    
    if (opts && opts->include_comment) {
        size += 64;  /* Comment estimate */
    }
    
    /* Estimate track/sector overhead */
    size_t sectors = (size_t)image->tracks * image->heads * 
                     image->sectors_per_track;
    size += sectors * (6 + 3);  /* Sector header + data header */
    
    /* Data size */
    size_t data_size = sectors * image->bytes_per_sector;
    
    /* Compression estimate */
    if (opts && opts->use_advanced_compression) {
        size += data_size / 2;  /* Estimate 50% compression */
    } else {
        size += data_size;
    }
    
    return size;
}

int uft_td0_validate(const uft_disk_image_t *image,
                     char **warnings,
                     size_t max_warnings) {
    if (!image) return -1;
    
    int issues = 0;
    size_t warn_idx = 0;
    
    /* Check geometry limits */
    if (image->tracks > 86) {
        if (warnings && warn_idx < max_warnings) {
            warnings[warn_idx++] = "Track count exceeds TD0 limit (86)";
        }
        issues++;
    }
    
    if (image->sectors_per_track > 36) {
        if (warnings && warn_idx < max_warnings) {
            warnings[warn_idx++] = "Sector count exceeds typical TD0 limit";
        }
        issues++;
    }
    
    if (image->bytes_per_sector > 8192) {
        if (warnings && warn_idx < max_warnings) {
            warnings[warn_idx++] = "Sector size exceeds TD0 limit";
        }
        issues++;
    }
    
    return issues;
}

void uft_td0_auto_settings(const uft_disk_image_t *image,
                           uft_td0_write_options_t *opts) {
    if (!image || !opts) return;
    
    uft_td0_write_options_init(opts);
    
    /* Auto-detect drive type and density */
    if (image->tracks <= 42) {
        opts->drive_type = TD0_DRIVE_525_360;
        opts->density = TD0_DENSITY_250K;
    } else if (image->bytes_per_sector == 512) {
        if (image->sectors_per_track >= 15) {
            opts->drive_type = TD0_DRIVE_35_144;
            opts->density = TD0_DENSITY_500K;
        } else {
            opts->drive_type = TD0_DRIVE_35_720;
            opts->density = TD0_DENSITY_250K;
        }
    }
    
    /* Enable compression for larger images */
    size_t data_size = (size_t)image->tracks * image->heads * 
                       image->sectors_per_track * image->bytes_per_sector;
    if (data_size > 512 * 1024) {
        opts->use_advanced_compression = true;
    }
}
