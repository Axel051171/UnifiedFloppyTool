/**
 * @file dms_writer.c
 * @brief DMS (Disk Masher System) Writer for Amiga
 * 
 * DMS is a compressed disk format popular on Amiga.
 * Supports multiple compression modes: None, Simple, Quick, Medium, Deep, Heavy.
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* DMS Constants */
#define DMS_SIGNATURE   "DMS!"
#define DMS_TRACK_SIZE  11264   /* Amiga track: 11 sectors × 512 bytes + MFM */

/* Compression modes */
#define DMS_COMP_NONE    0
#define DMS_COMP_SIMPLE  1
#define DMS_COMP_QUICK   2
#define DMS_COMP_MEDIUM  3
#define DMS_COMP_DEEP    4
#define DMS_COMP_HEAVY1  5
#define DMS_COMP_HEAVY2  6

/* Track flags */
#define DMS_TRACK_RLE    0x01
#define DMS_TRACK_QUICK  0x02
#define DMS_TRACK_MEDIUM 0x04
#define DMS_TRACK_DEEP   0x08
#define DMS_TRACK_HEAVY  0x10

#pragma pack(push, 1)
typedef struct {
    char     signature[4];  /* "DMS!" */
    uint32_t info_size;     /* Size of info header (56) */
    uint32_t date;          /* Creation date */
    uint16_t lowtrack;      /* First track */
    uint16_t hightrack;     /* Last track */
    uint32_t pack_size;     /* Packed size */
    uint32_t unpack_size;   /* Unpacked size */
    uint8_t  os_version;    /* OS version (1=1.x, 2=2.x, 3=3.x) */
    uint8_t  os_revision;   /* OS revision */
    uint16_t cpu_type;      /* CPU type */
    uint16_t copro_type;    /* Coprocessor type */
    uint16_t machine_type;  /* Machine type */
    uint16_t cpu_speed;     /* CPU speed */
    uint32_t time_created;  /* Time to create */
    uint16_t creator_ver;   /* Creator version */
    uint16_t needed_ver;    /* Needed version */
    uint16_t disk_type;     /* Disk type */
    uint16_t comp_mode;     /* Compression mode */
    uint16_t crc;           /* Header CRC */
} dms_header_t;

typedef struct {
    uint16_t type;          /* Track type */
    uint16_t track_num;     /* Track number */
    uint16_t pack_crc;      /* Packed CRC */
    uint16_t unpack_crc;    /* Unpacked CRC */
    uint16_t flags;         /* Compression flags */
    uint16_t pack_size;     /* Packed size */
    uint16_t unpack_size;   /* Unpacked size */
    uint8_t  data_crc_lo;   /* Data CRC low */
    uint8_t  data_crc_hi;   /* Data CRC high */
    uint16_t header_crc;    /* Header CRC */
} dms_track_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t *data;
    size_t   size;
    bool     valid;
} dms_track_t;

typedef struct {
    dms_header_t header;
    dms_track_t  tracks[160];
    int          first_track;
    int          last_track;
    int          comp_mode;
    char        *banner;
} dms_image_t;

/* ============================================================================
 * CRC Calculation
 * ============================================================================ */

static uint16_t dms_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/* ============================================================================
 * RLE Compression (Simple Mode)
 * ============================================================================ */

/**
 * @brief Compress track using RLE
 */
static int rle_compress(const uint8_t *in, size_t in_len,
                        uint8_t *out, size_t out_cap)
{
    size_t in_pos = 0, out_pos = 0;
    
    while (in_pos < in_len && out_pos < out_cap - 3) {
        /* Count run */
        uint8_t byte = in[in_pos];
        size_t run = 1;
        
        while (in_pos + run < in_len && 
               run < 255 && 
               in[in_pos + run] == byte) {
            run++;
        }
        
        if (run >= 4 || byte == 0x90) {
            /* Encode run */
            out[out_pos++] = 0x90;  /* RLE marker */
            out[out_pos++] = byte;
            out[out_pos++] = run;
            in_pos += run;
        } else {
            /* Literal */
            out[out_pos++] = byte;
            in_pos++;
        }
    }
    
    return (in_pos == in_len) ? out_pos : -1;
}

/* ============================================================================
 * Quick/Medium Compression (LZ77-style)
 * ============================================================================ */

#define LZ_WINDOW  4096
#define LZ_MAX_LEN 18

/**
 * @brief Compress using LZ77 variant
 */
static int lz_compress(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_cap, int mode)
{
    size_t in_pos = 0, out_pos = 0;
    int window_size = (mode == DMS_COMP_QUICK) ? 256 : LZ_WINDOW;
    
    while (in_pos < in_len && out_pos < out_cap - 3) {
        /* Find best match in window */
        int best_len = 0, best_off = 0;
        
        int start = (in_pos > (size_t)window_size) ? in_pos - window_size : 0;
        
        for (size_t i = start; i < in_pos; i++) {
            int len = 0;
            while (len < LZ_MAX_LEN && 
                   in_pos + len < in_len &&
                   in[i + len] == in[in_pos + len]) {
                len++;
            }
            
            if (len > best_len) {
                best_len = len;
                best_off = in_pos - i;
            }
        }
        
        if (best_len >= 3) {
            /* Output match */
            out[out_pos++] = 0xFF;  /* Match marker */
            out[out_pos++] = ((best_off >> 4) & 0xF0) | (best_len - 3);
            out[out_pos++] = best_off & 0xFF;
            in_pos += best_len;
        } else {
            /* Output literal */
            if (in[in_pos] == 0xFF) {
                out[out_pos++] = 0xFF;
                out[out_pos++] = 0x00;
            } else {
                out[out_pos++] = in[in_pos];
            }
            in_pos++;
        }
    }
    
    return (in_pos == in_len) ? out_pos : -1;
}

/* ============================================================================
 * DMS Writer
 * ============================================================================ */

/**
 * @brief Create DMS image
 */
int dms_create(dms_image_t *img, int comp_mode)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    
    memcpy(img->header.signature, DMS_SIGNATURE, 4);
    img->header.info_size = 56;
    img->header.date = (uint32_t)time(NULL);
    img->header.lowtrack = 0;
    img->header.hightrack = 79;
    img->header.os_version = 3;
    img->header.os_revision = 1;
    img->header.cpu_type = 68000;
    img->header.machine_type = 1;  /* A500 */
    img->header.creator_ver = 529; /* 5.29 */
    img->header.needed_ver = 111;
    img->header.disk_type = 1;     /* OFS */
    img->header.comp_mode = comp_mode;
    
    img->first_track = 0;
    img->last_track = 79;
    img->comp_mode = comp_mode;
    
    return 0;
}

/**
 * @brief Add track to DMS image
 */
int dms_add_track(dms_image_t *img, int track_num, const uint8_t *data,
                  size_t size)
{
    if (!img || !data || track_num < 0 || track_num >= 160) return -1;
    
    img->tracks[track_num].data = malloc(size);
    if (!img->tracks[track_num].data) return -1;
    
    memcpy(img->tracks[track_num].data, data, size);
    img->tracks[track_num].size = size;
    img->tracks[track_num].valid = true;
    
    if (track_num < img->first_track) img->first_track = track_num;
    if (track_num > img->last_track) img->last_track = track_num;
    
    return 0;
}

/**
 * @brief Set banner text
 */
int dms_set_banner(dms_image_t *img, const char *banner)
{
    if (!img) return -1;
    
    free(img->banner);
    img->banner = banner ? strdup(banner) : NULL;
    
    return 0;
}

/**
 * @brief Compress track based on mode
 */
static int compress_track(const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t out_cap,
                          int mode, uint16_t *flags)
{
    int result = -1;
    *flags = 0;
    
    switch (mode) {
        case DMS_COMP_NONE:
            if (in_len <= out_cap) {
                memcpy(out, in, in_len);
                result = in_len;
            }
            break;
            
        case DMS_COMP_SIMPLE:
            result = rle_compress(in, in_len, out, out_cap);
            if (result > 0) *flags = DMS_TRACK_RLE;
            break;
            
        case DMS_COMP_QUICK:
            result = lz_compress(in, in_len, out, out_cap, DMS_COMP_QUICK);
            if (result > 0) *flags = DMS_TRACK_QUICK;
            break;
            
        case DMS_COMP_MEDIUM:
        case DMS_COMP_DEEP:
            result = lz_compress(in, in_len, out, out_cap, DMS_COMP_MEDIUM);
            if (result > 0) *flags = DMS_TRACK_MEDIUM;
            break;
            
        default:
            /* Fall back to RLE */
            result = rle_compress(in, in_len, out, out_cap);
            if (result > 0) *flags = DMS_TRACK_RLE;
            break;
    }
    
    /* If compression didn't help, store uncompressed */
    if (result < 0 || result >= (int)in_len) {
        memcpy(out, in, in_len);
        result = in_len;
        *flags = 0;
    }
    
    return result;
}

/**
 * @brief Save DMS image
 */
int dms_save(const dms_image_t *img, const char *filename)
{
    if (!img || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* Calculate sizes */
    uint32_t pack_size = 0;
    uint32_t unpack_size = 0;
    
    for (int t = img->first_track; t <= img->last_track; t++) {
        if (img->tracks[t].valid) {
            unpack_size += img->tracks[t].size;
        }
    }
    
    /* Write header (will update pack_size later) */
    dms_header_t header = img->header;
    header.lowtrack = img->first_track;
    header.hightrack = img->last_track;
    header.unpack_size = unpack_size;
    
    long header_pos = ftell(fp);
    fwrite(&header, sizeof(header), 1, fp);
    
    /* Write banner if present */
    if (img->banner && strlen(img->banner) > 0) {
        dms_track_header_t banner_hdr = {0};
        banner_hdr.type = 0xFFFF;  /* Banner */
        banner_hdr.unpack_size = strlen(img->banner);
        banner_hdr.pack_size = banner_hdr.unpack_size;
        banner_hdr.header_crc = dms_crc16((uint8_t*)&banner_hdr, 18);
        
        fwrite(&banner_hdr, sizeof(banner_hdr), 1, fp);
        fwrite(img->banner, 1, strlen(img->banner), fp);
        
        pack_size += sizeof(banner_hdr) + strlen(img->banner);
    }
    
    /* Compress buffer */
    uint8_t *comp_buf = malloc(DMS_TRACK_SIZE * 2);
    if (!comp_buf) {
        fclose(fp);
        return -1;
    }
    
    /* Write tracks */
    for (int t = img->first_track; t <= img->last_track; t++) {
        if (!img->tracks[t].valid) continue;
        
        uint16_t flags;
        int comp_size = compress_track(img->tracks[t].data, img->tracks[t].size,
                                       comp_buf, DMS_TRACK_SIZE * 2,
                                       img->comp_mode, &flags);
        
        if (comp_size < 0) {
            free(comp_buf);
            fclose(fp);
            return -1;
        }
        
        /* Track header */
        dms_track_header_t track_hdr;
        track_hdr.type = 1;  /* Normal track */
        track_hdr.track_num = t;
        track_hdr.unpack_crc = dms_crc16(img->tracks[t].data, img->tracks[t].size);
        track_hdr.pack_crc = dms_crc16(comp_buf, comp_size);
        track_hdr.flags = flags;
        track_hdr.pack_size = comp_size;
        track_hdr.unpack_size = img->tracks[t].size;
        track_hdr.data_crc_lo = track_hdr.pack_crc & 0xFF;
        track_hdr.data_crc_hi = (track_hdr.pack_crc >> 8) & 0xFF;
        track_hdr.header_crc = dms_crc16((uint8_t*)&track_hdr, 18);
        
        fwrite(&track_hdr, sizeof(track_hdr), 1, fp);
        fwrite(comp_buf, 1, comp_size, fp);
        
        pack_size += sizeof(track_hdr) + comp_size;
    }
    
    free(comp_buf);
    
    /* Update header with pack_size */
    header.pack_size = pack_size;
    header.crc = dms_crc16((uint8_t*)&header, 50);
    
    fseek(fp, header_pos, SEEK_SET);
    fwrite(&header, sizeof(header), 1, fp);
    
    fclose(fp);
    return 0;
}

/**
 * @brief Convert ADF to DMS
 */
int dms_from_adf(const char *adf_file, const char *dms_file, int comp_mode)
{
    FILE *fp = fopen(adf_file, "rb");
    if (!fp) return -1;
    
    dms_image_t dms;
    dms_create(&dms, comp_mode);
    dms_set_banner(&dms, "Created by UnifiedFloppyTool");
    
    uint8_t track_data[DMS_TRACK_SIZE];
    
    for (int t = 0; t < 160; t++) {
        size_t read = fread(track_data, 1, DMS_TRACK_SIZE, fp);
        if (read == 0) break;
        
        /* Pad if necessary */
        if (read < DMS_TRACK_SIZE) {
            memset(track_data + read, 0, DMS_TRACK_SIZE - read);
        }
        
        dms_add_track(&dms, t, track_data, DMS_TRACK_SIZE);
    }
    
    fclose(fp);
    
    int result = dms_save(&dms, dms_file);
    dms_free(&dms);
    
    return result;
}

/**
 * @brief Free DMS image
 */
void dms_free(dms_image_t *img)
{
    if (img) {
        free(img->banner);
        
        for (int t = 0; t < 160; t++) {
            free(img->tracks[t].data);
        }
        
        memset(img, 0, sizeof(*img));
    }
}
