/**
 * @file uft_msa.h
 * @brief UnifiedFloppyTool - Atari ST MSA Format Support
 * @version 3.1.4.008
 *
 * Portable MSA (Magic Shadow Archiver) disk image decoder.
 * MSA is an RLE-compressed format for Atari ST floppy images.
 *
 * MSA File Structure (Big-Endian):
 * - Header (10 bytes):
 *   - uint16 id            (0x0E0F magic)
 *   - uint16 sectors_per_track
 *   - uint16 sides_minus_1 (0=single, 1=double)
 *   - uint16 start_track
 *   - uint16 end_track
 *
 * - Per track (for track=start..end, for side=0..sides-1):
 *   - uint16 packed_len
 *   - if packed_len == track_size: raw track bytes
 *   - else: RLE stream:
 *       byte b
 *       if b != 0xE5: output b
 *       else:
 *          uint16 count (big-endian)
 *          byte value
 *          output value repeated count times
 *
 */

#ifndef UFT_MSA_H
#define UFT_MSA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define UFT_MSA_MAGIC      0x0E0F
#define UFT_MSA_RLE_MARKER 0xE5
#define UFT_MSA_SECTOR_SIZE 512

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    UFT_MSA_OK = 0,
    UFT_MSA_ERR_IO = -1,
    UFT_MSA_ERR_FORMAT = -2,
    UFT_MSA_ERR_RANGE = -3,
    UFT_MSA_ERR_OOM = -4
} uft_msa_status_t;

/*============================================================================
 * MSA Image Structure
 *============================================================================*/

typedef struct {
    uint16_t sectors_per_track;
    uint16_t sides;           /**< 1 or 2 */
    uint16_t start_track;
    uint16_t end_track;
    
    uint32_t track_size_bytes; /**< sectors_per_track * 512 */
    uint32_t track_count;      /**< (end-start+1) * sides */
    
    uint8_t *data;            /**< Decoded image data */
    size_t   data_len;
} uft_msa_image_t;

/*============================================================================
 * MSA Header Structure (on-disk)
 *============================================================================*/

typedef struct {
    uint16_t id;              /**< 0x0E0F */
    uint16_t sectors_per_track;
    uint16_t sides_minus_1;
    uint16_t start_track;
    uint16_t end_track;
} uft_msa_header_t;

/*============================================================================
 * Inline Helper Functions
 *============================================================================*/

/**
 * @brief Read big-endian uint16
 */
static inline bool uft_msa_read_be16(const uint8_t *buf, size_t len, 
                                      size_t *off, uint16_t *out) {
    if (*off + 2 > len) return false;
    *out = (uint16_t)((buf[*off] << 8) | buf[*off + 1]);
    *off += 2;
    return true;
}

/**
 * @brief Read uint8
 */
static inline bool uft_msa_read_u8(const uint8_t *buf, size_t len,
                                    size_t *off, uint8_t *out) {
    if (*off + 1 > len) return false;
    *out = buf[*off];
    *off += 1;
    return true;
}

/**
 * @brief Initialize MSA image structure
 */
static inline void uft_msa_image_init(uft_msa_image_t *img) {
    if (img) memset(img, 0, sizeof(*img));
}

/**
 * @brief Free MSA image data
 */
static inline void uft_msa_image_destroy(uft_msa_image_t *img) {
    if (img) {
        free(img->data);
        uft_msa_image_init(img);
    }
}

/**
 * @brief Get status string
 */
static inline const char* uft_msa_status_str(uft_msa_status_t st) {
    switch (st) {
        case UFT_MSA_OK:         return "UFT_MSA_OK";
        case UFT_MSA_ERR_IO:     return "UFT_MSA_ERR_IO";
        case UFT_MSA_ERR_FORMAT: return "UFT_MSA_ERR_FORMAT";
        case UFT_MSA_ERR_RANGE:  return "UFT_MSA_ERR_RANGE";
        case UFT_MSA_ERR_OOM:    return "UFT_MSA_ERR_OOM";
        default:                 return "UFT_MSA_ERR_UNKNOWN";
    }
}

/*============================================================================
 * Decoding Functions
 *============================================================================*/

/**
 * @brief Decode RLE-compressed track data
 */
static inline uft_msa_status_t uft_msa_decode_track(
    const uint8_t *buf, size_t len, size_t *off,
    uint8_t *dst, uint32_t track_size, uint16_t packed_len)
{
    if (packed_len == track_size) {
        /* Uncompressed */
        if (*off + packed_len > len) return UFT_MSA_ERR_FORMAT;
        memcpy(dst, buf + *off, track_size);
        *off += packed_len;
        return UFT_MSA_OK;
    }
    
    /* RLE decode */
    uint32_t out_pos = 0;
    size_t end = *off + packed_len;
    if (end > len) return UFT_MSA_ERR_FORMAT;
    
    while (*off < end && out_pos < track_size) {
        uint8_t b = 0;
        if (!uft_msa_read_u8(buf, len, off, &b)) return UFT_MSA_ERR_FORMAT;
        
        if (b != UFT_MSA_RLE_MARKER) {
            dst[out_pos++] = b;
            continue;
        }
        
        uint16_t count = 0;
        uint8_t value = 0;
        if (!uft_msa_read_be16(buf, len, off, &count)) return UFT_MSA_ERR_FORMAT;
        if (!uft_msa_read_u8(buf, len, off, &value)) return UFT_MSA_ERR_FORMAT;
        
        if (count == 0) return UFT_MSA_ERR_FORMAT;
        if (out_pos + count > track_size) return UFT_MSA_ERR_FORMAT;
        
        memset(dst + out_pos, value, count);
        out_pos += count;
    }
    
    /* Lenient mode: zero-pad if short */
    if (out_pos < track_size) {
        memset(dst + out_pos, 0, track_size - out_pos);
    }
    
    /* Skip trailing garbage */
    if (*off < end) {
        *off = end;
    }
    
    return UFT_MSA_OK;
}

/**
 * @brief Decode MSA from memory buffer
 */
static inline uft_msa_status_t uft_msa_decode_buffer(
    const uint8_t *buf, size_t len, uft_msa_image_t *out)
{
    if (!buf || !out) return UFT_MSA_ERR_RANGE;
    uft_msa_image_destroy(out);
    
    size_t off = 0;
    
    uint16_t id, spt, sides_minus_1, start_track, end_track;
    
    if (!uft_msa_read_be16(buf, len, &off, &id)) return UFT_MSA_ERR_FORMAT;
    if (id != UFT_MSA_MAGIC) return UFT_MSA_ERR_FORMAT;
    
    if (!uft_msa_read_be16(buf, len, &off, &spt)) return UFT_MSA_ERR_FORMAT;
    if (!uft_msa_read_be16(buf, len, &off, &sides_minus_1)) return UFT_MSA_ERR_FORMAT;
    if (!uft_msa_read_be16(buf, len, &off, &start_track)) return UFT_MSA_ERR_FORMAT;
    if (!uft_msa_read_be16(buf, len, &off, &end_track)) return UFT_MSA_ERR_FORMAT;
    
    uint16_t sides = sides_minus_1 + 1;
    if (sides != 1 && sides != 2) return UFT_MSA_ERR_RANGE;
    if (spt == 0 || spt > 255) return UFT_MSA_ERR_RANGE;
    if (end_track < start_track) return UFT_MSA_ERR_RANGE;
    
    uint32_t track_size = spt * UFT_MSA_SECTOR_SIZE;
    uint32_t track_count = (end_track - start_track + 1) * sides;
    size_t image_len = (size_t)track_size * track_count;
    
    if (image_len == 0) return UFT_MSA_ERR_RANGE;
    
    uint8_t *data = (uint8_t*)malloc(image_len);
    if (!data) return UFT_MSA_ERR_OOM;
    
    /* Decode all tracks */
    for (uint32_t t = 0; t < track_count; t++) {
        uint16_t packed_len = 0;
        if (!uft_msa_read_be16(buf, len, &off, &packed_len)) {
            free(data);
            return UFT_MSA_ERR_FORMAT;
        }
        
        uft_msa_status_t st = uft_msa_decode_track(
            buf, len, &off,
            data + (t * track_size), track_size, packed_len);
        
        if (st != UFT_MSA_OK) {
            free(data);
            return st;
        }
    }
    
    out->sectors_per_track = spt;
    out->sides = sides;
    out->start_track = start_track;
    out->end_track = end_track;
    out->track_size_bytes = track_size;
    out->track_count = track_count;
    out->data = data;
    out->data_len = image_len;
    
    return UFT_MSA_OK;
}

/**
 * @brief Decode MSA from file
 */
static inline uft_msa_status_t uft_msa_decode_file(const char *path,
                                                    uft_msa_image_t *out) {
    if (!path || !out) return UFT_MSA_ERR_RANGE;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_MSA_ERR_IO;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_MSA_ERR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_MSA_ERR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_MSA_ERR_IO; }
    
    size_t len = (size_t)sz;
    uint8_t *buf = (uint8_t*)malloc(len);
    if (!buf) { fclose(f); return UFT_MSA_ERR_OOM; }
    
    size_t got = fread(buf, 1, len, f);
    fclose(f);
    if (got != len) { free(buf); return UFT_MSA_ERR_IO; }
    
    uft_msa_status_t st = uft_msa_decode_buffer(buf, len, out);
    free(buf);
    return st;
}

/*============================================================================
 * Encoding Functions
 *============================================================================*/

/**
 * @brief RLE encode a track
 * @param src Source track data
 * @param src_len Track size in bytes
 * @param dst Destination buffer (should be >= src_len + overhead)
 * @param dst_cap Destination capacity
 * @return Packed length, or 0 if encoding would be larger
 */
static inline size_t uft_msa_encode_track(const uint8_t *src, size_t src_len,
                                           uint8_t *dst, size_t dst_cap) {
    if (!src || !dst || src_len == 0) return 0;
    
    size_t out = 0;
    size_t i = 0;
    
    while (i < src_len) {
        uint8_t b = src[i];
        size_t run = 1;
        
        /* Count run length */
        while (i + run < src_len && src[i + run] == b && run < 65535) {
            run++;
        }
        
        /* Decide: encode as RLE or literal */
        if (run >= 4 || (b == UFT_MSA_RLE_MARKER && run >= 1)) {
            /* RLE: marker + count(2) + value = 4 bytes */
            if (out + 4 > dst_cap) return 0;
            dst[out++] = UFT_MSA_RLE_MARKER;
            dst[out++] = (run >> 8) & 0xFF;
            dst[out++] = run & 0xFF;
            dst[out++] = b;
            i += run;
        } else if (b == UFT_MSA_RLE_MARKER) {
            /* Must encode marker as RLE even for single byte */
            if (out + 4 > dst_cap) return 0;
            dst[out++] = UFT_MSA_RLE_MARKER;
            dst[out++] = 0;
            dst[out++] = 1;
            dst[out++] = b;
            i++;
        } else {
            /* Literal */
            if (out + 1 > dst_cap) return 0;
            dst[out++] = b;
            i++;
        }
    }
    
    /* Only use RLE if it saves space */
    if (out >= src_len) {
        return 0; /* Use uncompressed */
    }
    
    return out;
}

/**
 * @brief Write MSA to buffer
 * @return Number of bytes written, or 0 on error
 */
static inline size_t uft_msa_encode_buffer(const uft_msa_image_t *img,
                                            uint8_t *buf, size_t cap) {
    if (!img || !buf || !img->data) return 0;
    
    size_t off = 0;
    
    /* Header */
    if (off + 10 > cap) return 0;
    buf[off++] = (UFT_MSA_MAGIC >> 8) & 0xFF;
    buf[off++] = UFT_MSA_MAGIC & 0xFF;
    buf[off++] = (img->sectors_per_track >> 8) & 0xFF;
    buf[off++] = img->sectors_per_track & 0xFF;
    buf[off++] = ((img->sides - 1) >> 8) & 0xFF;
    buf[off++] = (img->sides - 1) & 0xFF;
    buf[off++] = (img->start_track >> 8) & 0xFF;
    buf[off++] = img->start_track & 0xFF;
    buf[off++] = (img->end_track >> 8) & 0xFF;
    buf[off++] = img->end_track & 0xFF;
    
    /* Encode tracks */
    uint8_t *tmp = (uint8_t*)malloc(img->track_size_bytes + 256);
    if (!tmp) return 0;
    
    for (uint32_t t = 0; t < img->track_count; t++) {
        const uint8_t *src = img->data + (t * img->track_size_bytes);
        
        size_t packed = uft_msa_encode_track(src, img->track_size_bytes,
                                              tmp, img->track_size_bytes + 256);
        
        if (packed == 0) {
            /* Uncompressed */
            if (off + 2 + img->track_size_bytes > cap) { free(tmp); return 0; }
            buf[off++] = (img->track_size_bytes >> 8) & 0xFF;
            buf[off++] = img->track_size_bytes & 0xFF;
            memcpy(buf + off, src, img->track_size_bytes);
            off += img->track_size_bytes;
        } else {
            /* Compressed */
            if (off + 2 + packed > cap) { free(tmp); return 0; }
            buf[off++] = (packed >> 8) & 0xFF;
            buf[off++] = packed & 0xFF;
            memcpy(buf + off, tmp, packed);
            off += packed;
        }
    }
    
    free(tmp);
    return off;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get sector data pointer
 */
static inline uint8_t* uft_msa_get_sector(uft_msa_image_t *img,
                                           uint16_t track, uint16_t side,
                                           uint16_t sector) {
    if (!img || !img->data) return NULL;
    if (track < img->start_track || track > img->end_track) return NULL;
    if (side >= img->sides) return NULL;
    if (sector >= img->sectors_per_track) return NULL;
    
    uint32_t rel_track = track - img->start_track;
    uint32_t track_idx = rel_track * img->sides + side;
    size_t offset = track_idx * img->track_size_bytes + sector * UFT_MSA_SECTOR_SIZE;
    
    if (offset + UFT_MSA_SECTOR_SIZE > img->data_len) return NULL;
    return img->data + offset;
}

/**
 * @brief Calculate CRC-16 CCITT-FALSE
 */
static inline uint16_t uft_msa_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief Probe if buffer is MSA format
 */
static inline bool uft_msa_probe(const uint8_t *buf, size_t len) {
    if (len < 10) return false;
    uint16_t magic = (buf[0] << 8) | buf[1];
    return magic == UFT_MSA_MAGIC;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MSA_H */
