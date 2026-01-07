/**
 * @file uft_td0.c
 * @brief Teledisk (TD0) Format Implementation for UFT
 * 
 * Based on reverse-engineering work by various authors, Will Krantz,
 * 
 * @copyright UFT Project
 */

#include "uft_td0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Huffman Decode Tables (from Teledisk reverse-engineering)
 *============================================================================*/

const uint8_t uft_td0_d_code[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
    0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
    0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
    0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
    0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

const uint8_t uft_td0_d_len[16] = {
    2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7
};

/*============================================================================
 * LZSS-Huffman Decompression
 *============================================================================*/

void uft_td0_lzss_init(uft_td0_lzss_state_t* state,
                       const uint8_t* data, size_t size)
{
    if (!state) return;
    
    memset(state, 0, sizeof(*state));
    state->input = data;
    state->input_size = size;
    state->input_pos = 0;
    state->eof = false;
    
    /* Initialize Huffman tree */
    unsigned i, j;
    for (i = j = 0; i < UFT_TD0_LZSS_N_CHAR; i++) {
        state->freq[i] = 1;
        state->son[i] = i + UFT_TD0_LZSS_TSIZE;
        state->parent[i + UFT_TD0_LZSS_TSIZE] = i;
    }
    
    while (i <= UFT_TD0_LZSS_ROOT) {
        state->freq[i] = state->freq[j] + state->freq[j + 1];
        state->son[i] = j;
        state->parent[j] = state->parent[j + 1] = i++;
        j += 2;
    }
    
    /* Initialize ring buffer with spaces */
    memset(state->ring_buff, ' ', sizeof(state->ring_buff));
    
    state->freq[UFT_TD0_LZSS_TSIZE] = 0xFFFF;
    state->parent[UFT_TD0_LZSS_ROOT] = 0;
    state->bitbuff = 0;
    state->bits = 0;
    state->r = UFT_TD0_LZSS_SBSIZE - UFT_TD0_LZSS_LASIZE;
    state->state = 0;
}

/* Get character from input */
static unsigned lzss_getchar(uft_td0_lzss_state_t* state)
{
    if (state->input_pos >= state->input_size) {
        state->eof = true;
        return 0;
    }
    return state->input[state->input_pos++];
}

/* Get single bit from input stream */
static unsigned lzss_getbit(uft_td0_lzss_state_t* state)
{
    unsigned t;
    
    if (state->bits == 0) {
        state->bitbuff |= lzss_getchar(state) << 8;
        state->bits = 8;
    }
    state->bits--;
    
    t = state->bitbuff >> 15;
    state->bitbuff <<= 1;
    return t;
}

/* Get byte from input stream (not bit-aligned) */
static unsigned lzss_getbyte(uft_td0_lzss_state_t* state)
{
    unsigned t;
    
    if (state->bits < 8) {
        state->bitbuff |= lzss_getchar(state) << (8 - state->bits);
    } else {
        state->bits -= 8;
    }
    
    t = state->bitbuff >> 8;
    state->bitbuff <<= 8;
    return t;
}

/* Update Huffman tree */
static void lzss_update(uft_td0_lzss_state_t* state, int c)
{
    unsigned i, j, k, f, l;
    
    if (state->freq[UFT_TD0_LZSS_ROOT] == UFT_TD0_LZSS_MAX_FREQ) {
        /* Tree is full - rebuild */
        for (i = j = 0; i < UFT_TD0_LZSS_TSIZE; i++) {
            if (state->son[i] >= UFT_TD0_LZSS_TSIZE) {
                state->freq[j] = (state->freq[i] + 1) / 2;
                state->son[j] = state->son[i];
                j++;
            }
        }
        
        for (i = 0, j = UFT_TD0_LZSS_N_CHAR; j < UFT_TD0_LZSS_TSIZE; i += 2, j++) {
            k = i + 1;
            f = state->freq[j] = state->freq[i] + state->freq[k];
            for (k = j - 1; f < state->freq[k]; k--);
            k++;
            l = (j - k) * sizeof(state->freq[0]);
            
            memmove(&state->freq[k + 1], &state->freq[k], l);
            state->freq[k] = f;
            memmove(&state->son[k + 1], &state->son[k], l);
            state->son[k] = i;
        }
        
        for (i = 0; i < UFT_TD0_LZSS_TSIZE; i++) {
            if ((k = state->son[i]) >= UFT_TD0_LZSS_TSIZE) {
                state->parent[k] = i;
            } else {
                state->parent[k] = state->parent[k + 1] = i;
            }
        }
    }
    
    c = state->parent[c + UFT_TD0_LZSS_TSIZE];
    do {
        k = ++state->freq[c];
        if (k > state->freq[l = c + 1]) {
            while (k > state->freq[++l]);
            state->freq[c] = state->freq[--l];
            state->freq[l] = k;
            state->parent[i = state->son[c]] = l;
            if (i < UFT_TD0_LZSS_TSIZE)
                state->parent[i + 1] = l;
            state->parent[j = state->son[l]] = c;
            state->son[l] = i;
            if (j < UFT_TD0_LZSS_TSIZE)
                state->parent[j + 1] = c;
            state->son[c] = j;
            c = l;
        }
    } while ((c = state->parent[c]) != 0);
}

/* Decode character from Huffman tree */
static unsigned lzss_decode_char(uft_td0_lzss_state_t* state)
{
    unsigned c = UFT_TD0_LZSS_ROOT;
    
    while ((c = state->son[c]) < UFT_TD0_LZSS_TSIZE) {
        c += lzss_getbit(state);
    }
    
    lzss_update(state, c -= UFT_TD0_LZSS_TSIZE);
    return c;
}

/* Decode position from input */
static unsigned lzss_decode_position(uft_td0_lzss_state_t* state)
{
    unsigned i, j, c;
    
    i = lzss_getbyte(state);
    c = (unsigned)uft_td0_d_code[i] << 6;
    
    j = uft_td0_d_len[i >> 4];
    while (--j) {
        i = (i << 1) | lzss_getbit(state);
    }
    
    return (i & 0x3F) | c;
}

int uft_td0_lzss_getbyte(uft_td0_lzss_state_t* state)
{
    unsigned c;
    
    if (!state) return -1;
    
    for (;;) {
        if (state->eof) return -1;
        
        if (state->state == 0) {
            /* Not in the middle of a string */
            c = lzss_decode_char(state);
            if (c < 256) {
                /* Direct data extraction */
                state->ring_buff[state->r++] = c;
                state->r &= (UFT_TD0_LZSS_SBSIZE - 1);
                return c;
            }
            /* Begin extracting compressed string */
            state->state = 1;
            state->i = (state->r - lzss_decode_position(state) - 1) & 
                       (UFT_TD0_LZSS_SBSIZE - 1);
            state->j = c - 255 + UFT_TD0_LZSS_THRESHOLD;
            state->k = 0;
        }
        
        if (state->k < state->j) {
            /* Extract compressed string */
            c = state->ring_buff[(state->k++ + state->i) & (UFT_TD0_LZSS_SBSIZE - 1)];
            state->ring_buff[state->r++] = c;
            state->r &= (UFT_TD0_LZSS_SBSIZE - 1);
            return c;
        }
        
        state->state = 0;
    }
}

size_t uft_td0_lzss_read(uft_td0_lzss_state_t* state,
                         uint8_t* buffer, size_t size)
{
    size_t count = 0;
    int c;
    
    while (count < size) {
        c = uft_td0_lzss_getbyte(state);
        if (c < 0) break;
        buffer[count++] = (uint8_t)c;
    }
    
    return count;
}

/*============================================================================
 * TD0 Detection and Initialization
 *============================================================================*/

bool uft_td0_detect(const uint8_t* data, size_t size)
{
    if (!data || size < 2) return false;
    
    uint16_t sig = data[0] | (data[1] << 8);
    return (sig == UFT_TD0_SIG_NORMAL || sig == UFT_TD0_SIG_ADVANCED);
}

bool uft_td0_is_compressed(const uft_td0_header_t* header)
{
    if (!header) return false;
    return header->signature == UFT_TD0_SIG_ADVANCED;
}

int uft_td0_init(uft_td0_image_t* img)
{
    if (!img) return -1;
    memset(img, 0, sizeof(*img));
    return 0;
}

void uft_td0_free(uft_td0_image_t* img)
{
    if (!img) return;
    
    if (img->comment) {
        free(img->comment);
        img->comment = NULL;
    }
    
    if (img->tracks) {
        for (uint16_t t = 0; t < img->num_tracks; t++) {
            if (img->tracks[t].sectors) {
                for (uint8_t s = 0; s < img->tracks[t].nsectors; s++) {
                    if (img->tracks[t].sectors[s].data) {
                        free(img->tracks[t].sectors[s].data);
                    }
                }
                free(img->tracks[t].sectors);
            }
        }
        free(img->tracks);
        img->tracks = NULL;
    }
    
    img->num_tracks = 0;
}

/*============================================================================
 * Sector Data Decoding
 *============================================================================*/

int uft_td0_decode_sector(const uint8_t* src, size_t src_size,
                          uint8_t* dst, size_t dst_size,
                          uint8_t method)
{
    if (!src || !dst || dst_size == 0) return -1;
    
    size_t src_pos = 0;
    size_t dst_pos = 0;
    
    switch (method) {
        case UFT_TD0_ENC_RAW:
            /* Raw data - just copy */
            if (src_size < dst_size) return -1;
            memcpy(dst, src, dst_size);
            return dst_size;
            
        case UFT_TD0_ENC_REP2:
            /* 2-byte pattern repetition */
            while (dst_pos < dst_size && src_pos + 3 < src_size) {
                uint16_t count = src[src_pos] | (src[src_pos + 1] << 8);
                uint8_t b1 = src[src_pos + 2];
                uint8_t b2 = src[src_pos + 3];
                src_pos += 4;
                
                count *= 2;
                while (count-- && dst_pos < dst_size) {
                    dst[dst_pos++] = b1;
                    if (dst_pos < dst_size && count--) {
                        dst[dst_pos++] = b2;
                    }
                }
            }
            break;
            
        case UFT_TD0_ENC_RLE:
            /* Run-length encoding */
            while (dst_pos < dst_size && src_pos < src_size) {
                uint8_t type = src[src_pos++];
                
                if (type == 0) {
                    /* Literal run */
                    if (src_pos >= src_size) break;
                    uint8_t len = src[src_pos++];
                    while (len-- && dst_pos < dst_size && src_pos < src_size) {
                        dst[dst_pos++] = src[src_pos++];
                    }
                } else {
                    /* Repeated pattern */
                    if (src_pos + 1 >= src_size) break;
                    uint8_t len = src[src_pos++];
                    uint8_t val = src[src_pos++];
                    while (len-- && dst_pos < dst_size) {
                        dst[dst_pos++] = val;
                    }
                }
            }
            break;
            
        default:
            return -1;
    }
    
    /* Fill remaining with zeros */
    while (dst_pos < dst_size) {
        dst[dst_pos++] = 0;
    }
    
    return dst_pos;
}

/*============================================================================
 * Drive Type Names
 *============================================================================*/

const char* uft_td0_drive_name(uft_td0_drive_t type)
{
    switch (type) {
        case UFT_TD0_DRIVE_525_96:  return "5.25\" 96 TPI (1.2MB)";
        case UFT_TD0_DRIVE_525_48:  return "5.25\" 48 TPI (360K)";
        case UFT_TD0_DRIVE_35_HD:   return "3.5\" HD";
        case UFT_TD0_DRIVE_35_DD:   return "3.5\" DD";
        case UFT_TD0_DRIVE_8INCH:   return "8\"";
        case UFT_TD0_DRIVE_35_ED:   return "3.5\" ED";
        default:                    return "Unknown";
    }
}

/*============================================================================
 * TD0 Reading
 *============================================================================*/

int uft_td0_read_mem(const uint8_t* data, size_t size, uft_td0_image_t* img)
{
    if (!data || !img || size < sizeof(uft_td0_header_t)) return -1;
    
    uft_td0_init(img);
    
    /* Read header */
    memcpy(&img->header, data, sizeof(uft_td0_header_t));
    
    if (!uft_td0_detect(data, size)) return -1;
    
    img->advanced_compression = uft_td0_is_compressed(&img->header);
    
    /* Set up decompression if needed */
    uft_td0_lzss_state_t lzss;
    const uint8_t* src;
    size_t src_pos;
    
    if (img->advanced_compression) {
        uft_td0_lzss_init(&lzss, data + sizeof(uft_td0_header_t),
                          size - sizeof(uft_td0_header_t));
        src = NULL;
        src_pos = 0;
    } else {
        src = data;
        src_pos = sizeof(uft_td0_header_t);
    }
    
    /* Helper to read bytes */
    #define READ_BYTE() (img->advanced_compression ? \
        uft_td0_lzss_getbyte(&lzss) : \
        (src_pos < size ? src[src_pos++] : -1))
    
    #define READ_BLOCK(buf, len) do { \
        for (size_t _i = 0; _i < (len); _i++) { \
            int _c = READ_BYTE(); \
            if (_c < 0) break; \
            (buf)[_i] = _c; \
        } \
    } while(0)
    
    /* Check for comment block */
    if (img->header.stepping & 0x80) {
        /* Comment present */
        READ_BLOCK((uint8_t*)&img->comment_header, sizeof(img->comment_header));
        
        if (img->comment_header.length > 0 && img->comment_header.length < 65536) {
            img->comment = malloc(img->comment_header.length + 1);
            if (img->comment) {
                READ_BLOCK((uint8_t*)img->comment, img->comment_header.length);
                img->comment[img->comment_header.length] = '\0';
                img->has_comment = true;
            }
        }
    }
    
    /* Count and allocate tracks */
    /* First pass - count tracks */
    size_t track_count = 0;
    size_t max_tracks = 256;
    
    img->tracks = calloc(max_tracks, sizeof(uft_td0_track_t));
    if (!img->tracks) return -1;
    
    uint8_t max_cyl = 0, max_head = 0;
    
    /* Read tracks */
    while (track_count < max_tracks) {
        uft_td0_track_header_t thdr;
        READ_BLOCK((uint8_t*)&thdr, sizeof(thdr));
        
        if (thdr.nsectors == UFT_TD0_END_OF_IMAGE) break;
        
        uft_td0_track_t* track = &img->tracks[track_count];
        memcpy(&track->header, &thdr, sizeof(thdr));
        track->nsectors = thdr.nsectors;
        
        if (thdr.cylinder > max_cyl) max_cyl = thdr.cylinder;
        if (thdr.side > max_head) max_head = thdr.side;
        
        /* Allocate sectors */
        track->sectors = calloc(thdr.nsectors, sizeof(uft_td0_sector_t));
        if (!track->sectors) break;
        
        /* Read sectors */
        for (uint8_t s = 0; s < thdr.nsectors; s++) {
            uft_td0_sector_t* sector = &track->sectors[s];
            
            READ_BLOCK((uint8_t*)&sector->header, sizeof(uft_td0_sector_header_t));
            
            /* Check if sector has data */
            if (!(sector->header.flags & UFT_TD0_SEC_NODAT)) {
                uft_td0_data_header_t dhdr;
                READ_BLOCK((uint8_t*)&dhdr, sizeof(dhdr));
                
                uint16_t sector_size = 128 << sector->header.size;
                sector->data_size = sector_size;
                sector->data = malloc(sector_size);
                
                if (sector->data) {
                    if (dhdr.offset > 0) {
                        uint8_t* temp = malloc(dhdr.offset);
                        if (temp) {
                            READ_BLOCK(temp, dhdr.offset);
                            uft_td0_decode_sector(temp, dhdr.offset,
                                                  sector->data, sector_size,
                                                  dhdr.method);
                            free(temp);
                        }
                    }
                }
            }
        }
        
        track_count++;
    }
    
    img->num_tracks = track_count;
    img->cylinders = max_cyl + 1;
    img->heads = max_head + 1;
    
    #undef READ_BYTE
    #undef READ_BLOCK
    
    return 0;
}

int uft_td0_read(const char* filename, uft_td0_image_t* img)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 64*1024*1024) {
        fclose(fp);
        return -1;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(data, 1, size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    int result = uft_td0_read_mem(data, size, img);
    free(data);
    
    return result;
}

/*============================================================================
 * Information Display
 *============================================================================*/

void uft_td0_print_info(const uft_td0_image_t* img, bool verbose)
{
    if (!img) return;
    
    printf("Teledisk (TD0) Image Information:\n");
    printf("  Signature: %s\n", 
           img->advanced_compression ? "td (compressed)" : "TD (normal)");
    printf("  Version: %d.%d\n", 
           img->header.version >> 4, img->header.version & 0x0F);
    printf("  Drive type: %s\n", uft_td0_drive_name(img->header.drive_type));
    printf("  Data rate: %s\n", 
           img->header.data_rate == 0 ? "250K" :
           img->header.data_rate == 1 ? "300K" : "500K");
    printf("  Sides: %d\n", img->header.sides);
    
    if (img->has_comment && img->comment) {
        printf("  Comment date: %02d/%02d/%04d %02d:%02d:%02d\n",
               img->comment_header.month,
               img->comment_header.day,
               img->comment_header.year + 1900,
               img->comment_header.hour,
               img->comment_header.minute,
               img->comment_header.second);
        printf("  Comment: %s\n", img->comment);
    }
    
    printf("  Geometry: %d cylinders, %d heads, %d tracks\n",
           img->cylinders, img->heads, img->num_tracks);
    
    if (verbose && img->tracks) {
        printf("\n  Track Details:\n");
        for (uint16_t t = 0; t < img->num_tracks; t++) {
            const uft_td0_track_t* track = &img->tracks[t];
            printf("    C%02d/H%d: %d sectors\n",
                   track->header.cylinder,
                   track->header.side,
                   track->nsectors);
        }
    }
}
