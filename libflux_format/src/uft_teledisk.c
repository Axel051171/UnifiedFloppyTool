/**
 * @file uft_teledisk.c
 * @brief TeleDisk TD0 Format Implementation
 * 
 * - TD0Lzhuf: LZHUF decompression (TD0 2.x)
 * - TD0Lzw: LZW decompression (TD0 1.x)
 * - TD0Rle: RLE expansion
 */

#include "uft/uft_teledisk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * TD0 CRC IMPLEMENTATION
 *============================================================================*/

/* CRC-16 table for TD0 header */
static const uint16_t td0_crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

uint16_t uft_td0_crc16(const uint8_t* data, size_t len)
{
    uint16_t crc = 0;
    
    while (len--) {
        crc = (crc >> 8) ^ td0_crc16_table[(crc ^ *data++) & 0xFF];
    }
    
    return crc;
}

uint8_t uft_td0_crc8(const uint8_t* data, size_t len)
{
    uint8_t crc = 0;
    
    while (len--) {
        crc += *data++;
    }
    
    return crc;
}

/*============================================================================
 * LZHUF DECOMPRESSION (TD0 2.x)
 * From DiskImageTool's TD0Lzhuf
 *============================================================================*/

#define LZHUF_N         4096
#define LZHUF_F         60
#define LZHUF_THRESHOLD 2
#define LZHUF_N_CHAR    (256 - LZHUF_THRESHOLD + LZHUF_F)
#define LZHUF_T         (LZHUF_N_CHAR * 2 - 1)
#define LZHUF_ROOT      (LZHUF_T - 1)
#define LZHUF_MAX_FREQ  0x8000

typedef struct lzhuf_state {
    /* Huffman tree */
    uint16_t freq[LZHUF_T + 1];
    int16_t prnt[LZHUF_T + LZHUF_N_CHAR];
    int16_t son[LZHUF_T];
    
    /* Ring buffer */
    uint8_t text_buf[LZHUF_N + LZHUF_F - 1];
    int r;
    
    /* Bitstream state */
    const uint8_t* src;
    size_t src_pos;
    size_t src_len;
    uint16_t getbuf;
    int getlen;
    
    /* Output */
    uint8_t* dst;
    size_t dst_pos;
    size_t dst_max;
} lzhuf_state_t;

static void lzhuf_start_huff(lzhuf_state_t* s)
{
    /* Initialize Huffman tree */
    for (int i = 0; i < LZHUF_N_CHAR; i++) {
        s->freq[i] = 1;
        s->son[i] = i + LZHUF_T;
        s->prnt[i + LZHUF_T] = i;
    }
    
    int i = 0, j = LZHUF_N_CHAR;
    while (j <= LZHUF_ROOT) {
        s->freq[j] = s->freq[i] + s->freq[i + 1];
        s->son[j] = i;
        s->prnt[i] = s->prnt[i + 1] = j;
        i += 2;
        j++;
    }
    
    s->freq[LZHUF_T] = 0xFFFF;
    s->prnt[LZHUF_ROOT] = 0;
}

static int lzhuf_next_word(lzhuf_state_t* s)
{
    while (s->getlen <= 8) {
        if (s->src_pos >= s->src_len) return -1;
        
        int b = s->src[s->src_pos++];
        s->getbuf |= (uint16_t)b << (8 - s->getlen);
        s->getlen += 8;
    }
    return 0;
}

static int lzhuf_get_bit(lzhuf_state_t* s)
{
    if (lzhuf_next_word(s) < 0) return -1;
    
    int bit = (s->getbuf & 0x8000) ? 1 : 0;
    s->getbuf <<= 1;
    s->getlen--;
    
    return bit;
}

static int lzhuf_get_byte(lzhuf_state_t* s)
{
    if (lzhuf_next_word(s) < 0) return -1;
    
    int byte = (s->getbuf >> 8) & 0xFF;
    s->getbuf <<= 8;
    s->getlen -= 8;
    
    return byte;
}

static void lzhuf_update(lzhuf_state_t* s, int c)
{
    if (s->freq[LZHUF_ROOT] == LZHUF_MAX_FREQ) {
        /* Rebuild tree */
        int j = 0;
        for (int i = 0; i < LZHUF_T; i++) {
            if (s->son[i] >= LZHUF_T) {
                s->freq[j] = (s->freq[i] + 1) / 2;
                s->son[j] = s->son[i];
                j++;
            }
        }
        
        int i = 0;
        for (j = LZHUF_N_CHAR; j < LZHUF_T; j++) {
            int k = i + 1;
            uint16_t f = s->freq[j] = s->freq[i] + s->freq[k];
            for (k = j - 1; f < s->freq[k]; k--);
            k++;
            
            int l = (j - k) * sizeof(s->freq[0]);
            memmove(&s->freq[k + 1], &s->freq[k], l);
            s->freq[k] = f;
            memmove(&s->son[k + 1], &s->son[k], l);
            s->son[k] = i;
            i += 2;
        }
        
        for (int i = 0; i < LZHUF_T; i++) {
            int k = s->son[i];
            if (k >= LZHUF_T) {
                s->prnt[k] = i;
            } else {
                s->prnt[k] = s->prnt[k + 1] = i;
            }
        }
    }
    
    c = s->prnt[c + LZHUF_T];
    do {
        int k = ++s->freq[c];
        int l = c + 1;
        
        if (k > s->freq[l]) {
            while (k > s->freq[++l]);
            l--;
            s->freq[c] = s->freq[l];
            s->freq[l] = k;
            
            int i = s->son[c];
            s->prnt[i] = l;
            if (i < LZHUF_T) s->prnt[i + 1] = l;
            
            int j = s->son[l];
            s->son[l] = i;
            s->prnt[j] = c;
            if (j < LZHUF_T) s->prnt[j + 1] = c;
            s->son[c] = j;
            
            c = l;
        }
    } while ((c = s->prnt[c]) != 0);
}

static int lzhuf_decode_char(lzhuf_state_t* s)
{
    int c = s->son[LZHUF_ROOT];
    
    while (c < LZHUF_T) {
        int bit = lzhuf_get_bit(s);
        if (bit < 0) return -1;
        c = s->son[c + bit];
    }
    
    c -= LZHUF_T;
    lzhuf_update(s, c);
    
    return c;
}

static int lzhuf_decode_position(lzhuf_state_t* s)
{
    int i = lzhuf_get_byte(s);
    if (i < 0) return -1;
    
    int c = (int)((uint8_t)i >> 1) | 128;
    
    for (int j = 0; j < 6; j++) {
        int bit = lzhuf_get_bit(s);
        if (bit < 0) return -1;
        c = (c << 1) | bit;
    }
    
    return (c & (LZHUF_N - 1));
}

int uft_td0_lzhuf_decompress(const uint8_t* src, size_t src_len,
                              uint8_t* dst, size_t max_out)
{
    if (!src || !dst || max_out == 0) return -1;
    
    lzhuf_state_t* s = calloc(1, sizeof(lzhuf_state_t));
    if (!s) return -1;
    
    s->src = src;
    s->src_len = src_len;
    s->dst = dst;
    s->dst_max = max_out;
    
    /* Initialize */
    lzhuf_start_huff(s);
    for (int i = 0; i < LZHUF_N - LZHUF_F; i++) {
        s->text_buf[i] = 0x20;
    }
    s->r = LZHUF_N - LZHUF_F;
    
    /* Decode */
    while (s->dst_pos < max_out) {
        int c = lzhuf_decode_char(s);
        if (c < 0) break;
        
        if (c < 256) {
            /* Literal byte */
            if (s->dst_pos >= s->dst_max) break;
            s->dst[s->dst_pos++] = (uint8_t)c;
            s->text_buf[s->r++] = (uint8_t)c;
            s->r &= (LZHUF_N - 1);
        } else {
            /* Match */
            int i = (s->r - lzhuf_decode_position(s) - 1) & (LZHUF_N - 1);
            int j = c - 255 + LZHUF_THRESHOLD;
            
            for (int k = 0; k < j; k++) {
                c = s->text_buf[(i + k) & (LZHUF_N - 1)];
                if (s->dst_pos >= s->dst_max) break;
                s->dst[s->dst_pos++] = (uint8_t)c;
                s->text_buf[s->r++] = (uint8_t)c;
                s->r &= (LZHUF_N - 1);
            }
        }
    }
    
    int result = (int)s->dst_pos;
    free(s);
    
    return result;
}

/*============================================================================
 * LZW DECOMPRESSION (TD0 1.x)
 * From DiskImageTool's TD0Lzw
 *============================================================================*/

#define LZW_FIRST_CODE  256
#define LZW_MAX_CODES   4096
#define LZW_MAX_BLOCK   0x1800

typedef struct {
    uint8_t suffix;
    uint16_t prefix;
} lzw_entry_t;

static int lzw_get_code(const uint8_t* src, size_t block_start,
                        int buf_len_nibbles, int* buf_pos_nibbles)
{
    if (*buf_pos_nibbles >= buf_len_nibbles) return -1;
    
    int real_pos = *buf_pos_nibbles >> 1;
    int b0 = src[block_start + real_pos] & 0xFF;
    int b1 = src[block_start + real_pos + 1] & 0xFF;
    
    int code;
    if ((*buf_pos_nibbles) & 1) {
        /* Odd position */
        code = ((b0 & 0xF0) >> 4) | (b1 << 4);
    } else {
        /* Even position */
        code = b0 | ((b1 & 0x0F) << 8);
    }
    
    *buf_pos_nibbles += 3;
    return code & 0xFFF;
}

static uint8_t lzw_decode_string(int code, lzw_entry_t* dict,
                                 uint8_t* out, size_t* out_pos, size_t max_out)
{
    uint8_t stack[4096];
    int stack_pos = 0;
    
    int cur = code;
    while (cur >= LZW_FIRST_CODE) {
        stack[stack_pos++] = dict[cur - LZW_FIRST_CODE].suffix;
        cur = dict[cur - LZW_FIRST_CODE].prefix;
    }
    
    uint8_t first_char = (uint8_t)(cur & 0xFF);
    
    if (*out_pos < max_out) out[(*out_pos)++] = first_char;
    
    for (int i = stack_pos - 1; i >= 0; i--) {
        if (*out_pos < max_out) out[(*out_pos)++] = stack[i];
    }
    
    return first_char;
}

int uft_td0_lzw_decompress(const uint8_t* src, size_t src_len,
                            uint8_t* dst, size_t max_out)
{
    if (!src || !dst || max_out == 0) return -1;
    
    size_t pos = 0;
    size_t out_pos = 0;
    
    while (pos + 2 <= src_len && out_pos < max_out) {
        /* Read block header */
        int buf_len_nibbles = src[pos] | (src[pos + 1] << 8);
        pos += 2;
        
        int payload_bytes = buf_len_nibbles / 2;
        if (buf_len_nibbles & 1) payload_bytes++;
        
        if (payload_bytes > LZW_MAX_BLOCK || pos + payload_bytes > src_len) {
            break;
        }
        
        /* Decode block */
        lzw_entry_t dict[LZW_MAX_CODES - LZW_FIRST_CODE];
        int buf_pos_nibbles = 0;
        
        /* First code must be literal */
        int last_code = lzw_get_code(src, pos, buf_len_nibbles, &buf_pos_nibbles);
        if (last_code < 0 || last_code > 255) {
            pos += payload_bytes;
            continue;
        }
        
        uint8_t c = (uint8_t)last_code;
        if (out_pos < max_out) dst[out_pos++] = c;
        
        int next_code = LZW_FIRST_CODE;
        
        while (out_pos < max_out) {
            int code = lzw_get_code(src, pos, buf_len_nibbles, &buf_pos_nibbles);
            if (code < 0) break;
            
            if (code < next_code) {
                c = lzw_decode_string(code, dict, dst, &out_pos, max_out);
            } else {
                uint8_t tmp = c;
                c = lzw_decode_string(last_code, dict, dst, &out_pos, max_out);
                if (out_pos < max_out) dst[out_pos++] = tmp;
            }
            
            if (next_code < LZW_MAX_CODES) {
                dict[next_code - LZW_FIRST_CODE].prefix = (uint16_t)last_code;
                dict[next_code - LZW_FIRST_CODE].suffix = c;
                next_code++;
            }
            
            last_code = code;
        }
        
        pos += payload_bytes;
        
        /* Last block if smaller than max */
        if (payload_bytes < LZW_MAX_BLOCK) break;
    }
    
    return (int)out_pos;
}

/*============================================================================
 * RLE EXPANSION
 *============================================================================*/

int uft_td0_rle_expand(const uint8_t* src, size_t src_len,
                        uint8_t* dst, size_t expected_size)
{
    if (!src || !dst) return -1;
    
    size_t src_pos = 0;
    size_t dst_pos = 0;
    
    while (src_pos < src_len && dst_pos < expected_size) {
        uint8_t ctrl = src[src_pos++];
        
        if (ctrl == 0) {
            /* Literal run */
            if (src_pos >= src_len) break;
            int count = src[src_pos++];
            
            for (int i = 0; i < count && src_pos < src_len && dst_pos < expected_size; i++) {
                dst[dst_pos++] = src[src_pos++];
            }
        } else {
            /* Repeat run */
            int count = ctrl;
            if (src_pos + 1 >= src_len) break;
            
            uint8_t len = src[src_pos++];
            uint8_t pattern[256];
            
            for (int i = 0; i < len && src_pos < src_len; i++) {
                pattern[i] = src[src_pos++];
            }
            
            for (int i = 0; i < count; i++) {
                for (int j = 0; j < len && dst_pos < expected_size; j++) {
                    dst[dst_pos++] = pattern[j];
                }
            }
        }
    }
    
    return (int)dst_pos;
}

/*============================================================================
 * TD0 FILE OPERATIONS
 *============================================================================*/

bool uft_td0_validate_header(const uft_td0_header_t* header)
{
    if (!header) return false;
    
    /* Check signature */
    if (!((header->signature[0] == 'T' && header->signature[1] == 'D') ||
          (header->signature[0] == 't' && header->signature[1] == 'd'))) {
        return false;
    }
    
    /* Verify CRC */
    uint16_t calc_crc = uft_td0_crc16((const uint8_t*)header, 10);
    if (calc_crc != header->crc) {
        return false;
    }
    
    return true;
}

int uft_td0_open(const char* filename, uft_td0_image_t* image)
{
    if (!filename || !image) return -1;
    
    FILE* f = fopen(filename, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = malloc(file_size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    if (fread(data, 1, file_size, f) != file_size) {
        free(data);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    
    int result = uft_td0_parse(data, file_size, image);
    free(data);
    
    return result;
}

int uft_td0_parse(const uint8_t* data, size_t len, uft_td0_image_t* image)
{
    if (!data || !image || len < UFT_TD0_HEADER_SIZE) return -1;
    
    memset(image, 0, sizeof(*image));
    
    /* Parse header */
    memcpy(&image->header, data, sizeof(uft_td0_header_t));
    
    if (!uft_td0_validate_header(&image->header)) {
        return -1;
    }
    
    const uint8_t* payload = data + UFT_TD0_HEADER_SIZE;
    size_t payload_len = len - UFT_TD0_HEADER_SIZE;
    
    /* Decompress if needed */
    uint8_t* decompressed = NULL;
    if (uft_td0_is_compressed(&image->header)) {
        size_t max_decompressed = len * 10;  /* Estimate */
        decompressed = malloc(max_decompressed);
        if (!decompressed) return -1;
        
        int dec_len = uft_td0_lzhuf_decompress(payload, payload_len,
                                                decompressed, max_decompressed);
        if (dec_len <= 0) {
            free(decompressed);
            return -1;
        }
        
        payload = decompressed;
        payload_len = dec_len;
    }
    
    size_t pos = 0;
    
    /* Parse comment if present */
    if (uft_td0_has_comment(&image->header)) {
        if (pos + 10 > payload_len) {
            free(decompressed);
            return -1;
        }
        
        image->comment.crc = payload[pos] | (payload[pos + 1] << 8);
        image->comment.length = payload[pos + 2] | (payload[pos + 3] << 8);
        image->comment.year = payload[pos + 4];
        image->comment.month = payload[pos + 5];
        image->comment.day = payload[pos + 6];
        image->comment.hour = payload[pos + 7];
        image->comment.minute = payload[pos + 8];
        image->comment.second = payload[pos + 9];
        pos += 10;
        
        if (image->comment.length > 0 && pos + image->comment.length <= payload_len) {
            image->comment.text = malloc(image->comment.length + 1);
            if (image->comment.text) {
                memcpy(image->comment.text, payload + pos, image->comment.length);
                image->comment.text[image->comment.length] = '\0';
            }
            pos += image->comment.length;
        }
        
        image->has_comment = true;
    }
    
    /* Parse tracks */
    int track_capacity = 160;
    image->tracks = calloc(track_capacity, sizeof(uft_td0_track_t));
    if (!image->tracks) {
        free(decompressed);
        return -1;
    }
    
    while (pos + 4 <= payload_len) {
        uft_td0_track_header_t track_hdr;
        memcpy(&track_hdr, payload + pos, sizeof(track_hdr));
        pos += sizeof(track_hdr);
        
        /* End of tracks marker */
        if (track_hdr.sector_count == 255) break;
        
        if (image->track_count >= track_capacity) {
            track_capacity *= 2;
            image->tracks = realloc(image->tracks, 
                                    track_capacity * sizeof(uft_td0_track_t));
        }
        
        uft_td0_track_t* track = &image->tracks[image->track_count];
        track->cylinder = track_hdr.cylinder;
        track->head = track_hdr.head;
        track->sector_count = track_hdr.sector_count;
        track->sectors = calloc(track->sector_count, sizeof(uft_td0_sector_t));
        
        if (track->cylinder > image->max_cylinder) 
            image->max_cylinder = track->cylinder;
        if (track->head > image->max_head)
            image->max_head = track->head;
        
        /* Parse sectors */
        for (int s = 0; s < track->sector_count && pos + 6 <= payload_len; s++) {
            uft_td0_sector_t* sector = &track->sectors[s];
            memcpy(&sector->header, payload + pos, sizeof(sector->header));
            pos += sizeof(sector->header);
            
            if (sector->header.sector > image->max_sector)
                image->max_sector = sector->header.sector;
            
            /* Flags: bit 4 = no data */
            if (sector->header.flags & 0x30) {
                sector->has_data = false;
                continue;
            }
            
            /* Read data header */
            if (pos + 3 > payload_len) break;
            uft_td0_data_header_t data_hdr;
            memcpy(&data_hdr, payload + pos, sizeof(data_hdr));
            pos += sizeof(data_hdr);
            
            int sector_size = 128 << (sector->header.size_code & 7);
            sector->data = malloc(sector_size);
            sector->data_size = sector_size;
            
            int data_len = data_hdr.size - 1;
            
            switch (data_hdr.encoding) {
                case UFT_TD0_DATA_RAW:
                    if (pos + data_len <= payload_len) {
                        memcpy(sector->data, payload + pos, 
                               data_len < sector_size ? data_len : sector_size);
                    }
                    pos += data_len;
                    break;
                    
                case UFT_TD0_DATA_REPEAT:
                    if (pos + 4 <= payload_len) {
                        int count = payload[pos] | (payload[pos + 1] << 8);
                        uint8_t b1 = payload[pos + 2];
                        uint8_t b2 = payload[pos + 3];
                        
                        for (int i = 0; i < count * 2 && i < sector_size; i += 2) {
                            sector->data[i] = b1;
                            if (i + 1 < sector_size) sector->data[i + 1] = b2;
                        }
                        pos += 4;
                    }
                    break;
                    
                case UFT_TD0_DATA_RLE:
                    if (pos + data_len <= payload_len) {
                        uft_td0_rle_expand(payload + pos, data_len,
                                           sector->data, sector_size);
                    }
                    pos += data_len;
                    break;
            }
            
            sector->has_data = true;
            sector->crc_error = (sector->header.flags & 0x02) != 0;
            sector->deleted = (sector->header.flags & 0x04) != 0;
        }
        
        image->track_count++;
    }
    
    /* Set sector size from first sector found */
    if (image->track_count > 0 && image->tracks[0].sector_count > 0) {
        image->sector_size = 128 << (image->tracks[0].sectors[0].header.size_code & 7);
    }
    
    free(decompressed);
    return 0;
}

void uft_td0_close(uft_td0_image_t* image)
{
    if (!image) return;
    
    if (image->comment.text) {
        free(image->comment.text);
    }
    
    if (image->tracks) {
        for (int t = 0; t < image->track_count; t++) {
            if (image->tracks[t].sectors) {
                for (int s = 0; s < image->tracks[t].sector_count; s++) {
                    free(image->tracks[t].sectors[s].data);
                }
                free(image->tracks[t].sectors);
            }
        }
        free(image->tracks);
    }
    
    memset(image, 0, sizeof(*image));
}

int uft_td0_read_sector(const uft_td0_image_t* image,
                        int cylinder, int head, int sector,
                        uint8_t* buffer)
{
    if (!image || !buffer) return -1;
    
    for (int t = 0; t < image->track_count; t++) {
        if (image->tracks[t].cylinder == cylinder &&
            image->tracks[t].head == head) {
            
            for (int s = 0; s < image->tracks[t].sector_count; s++) {
                if (image->tracks[t].sectors[s].header.sector == sector &&
                    image->tracks[t].sectors[s].has_data) {
                    
                    memcpy(buffer, image->tracks[t].sectors[s].data,
                           image->tracks[t].sectors[s].data_size);
                    return 0;
                }
            }
        }
    }
    
    return -1;  /* Sector not found */
}

int uft_td0_to_raw(const uft_td0_image_t* image,
                   uint8_t* output, size_t max_size)
{
    if (!image || !output) return -1;
    
    int cylinders = image->max_cylinder + 1;
    int heads = image->max_head + 1;
    int sectors = image->max_sector;
    int sector_size = image->sector_size;
    
    size_t total = (size_t)cylinders * heads * sectors * sector_size;
    if (total > max_size) return -1;
    
    memset(output, 0, total);
    
    for (int c = 0; c < cylinders; c++) {
        for (int h = 0; h < heads; h++) {
            for (int s = 1; s <= sectors; s++) {
                size_t offset = ((c * heads + h) * sectors + (s - 1)) * sector_size;
                uft_td0_read_sector(image, c, h, s, output + offset);
            }
        }
    }
    
    return (int)total;
}

void uft_td0_version_string(const uft_td0_header_t* header,
                            char* buffer, size_t buf_len)
{
    if (!header || !buffer || buf_len < 4) return;
    
    int major = (header->version / 10) % 10;
    int minor = header->version % 10;
    
    snprintf(buffer, buf_len, "%d.%d", major, minor);
}

const char* uft_td0_drive_name(uft_td0_drive_t drive)
{
    switch (drive) {
        case UFT_TD0_DRIVE_525_96TPI: return "5.25\" 96 TPI";
        case UFT_TD0_DRIVE_525_48TPI: return "5.25\" 48 TPI";
        case UFT_TD0_DRIVE_35_135TPI: return "3.5\" 135 TPI";
        case UFT_TD0_DRIVE_8_INCH:    return "8\"";
        case UFT_TD0_DRIVE_35_HD:     return "3.5\" HD";
        default:                      return "Unknown";
    }
}

const char* uft_td0_rate_name(uft_td0_rate_t rate)
{
    switch (rate & 0x03) {
        case UFT_TD0_RATE_250K: return "250 Kbps (DD)";
        case UFT_TD0_RATE_300K: return "300 Kbps (HD 1.2MB)";
        case UFT_TD0_RATE_500K: return "500 Kbps (HD)";
        default:                return "Unknown";
    }
}
