/**
 * @file uft_zxtap.c
 * @brief ZX Spectrum TAP Format Implementation
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L
#include "uft_zxtap.h"
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Constants (for conversion)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define TZX_SIGNATURE       "ZXTape!\x1A"
#define TZX_HEADER_SIZE     10
#define TZX_ID_STANDARD     0x10

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_le24(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16));
}

static void write_le16(uint8_t* p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP File Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

zxtap_file_t* zxtap_file_create(void) {
    zxtap_file_t* tap = calloc(1, sizeof(zxtap_file_t));
    if (!tap) return NULL;
    
    tap->capacity = 16;
    tap->blocks = calloc(tap->capacity, sizeof(zxtap_block_t));
    if (!tap->blocks) {
        free(tap);
        return NULL;
    }
    
    return tap;
}

void zxtap_file_free(zxtap_file_t* tap) {
    if (!tap) return;
    
    for (size_t i = 0; i < tap->block_count; i++) {
        free(tap->blocks[i].data);
    }
    free(tap->blocks);
    free(tap);
}

zxtap_file_t* zxtap_file_parse(const uint8_t* data, size_t size) {
    if (!data || size < 3) return NULL;
    
    zxtap_file_t* tap = zxtap_file_create();
    if (!tap) return NULL;
    
    size_t pos = 0;
    
    while (pos + 2 < size) {
        uint16_t block_len = read_le16(data + pos);
        pos += 2;
        
        if (block_len == 0 || pos + block_len > size) break;
        
        /* Grow array if needed */
        if (tap->block_count >= tap->capacity) {
            tap->capacity *= 2;
            zxtap_block_t* new_blocks = realloc(tap->blocks, 
                                                 tap->capacity * sizeof(zxtap_block_t));
            if (!new_blocks) {
                zxtap_file_free(tap);
                return NULL;
            }
            tap->blocks = new_blocks;
        }
        
        zxtap_block_t* block = &tap->blocks[tap->block_count];
        block->length = block_len;
        block->flag = data[pos];
        
        /* Copy data (excluding flag, including checksum) */
        size_t data_len = block_len - 1;  /* -1 for flag */
        block->data = malloc(data_len);
        if (!block->data) {
            zxtap_file_free(tap);
            return NULL;
        }
        memcpy(block->data, data + pos + 1, data_len);
        
        /* Verify checksum */
        block->checksum = zxtap_checksum(block->flag, block->data, data_len - 1);
        block->checksum_ok = (block->checksum == block->data[data_len - 1]);
        
        pos += block_len;
        tap->block_count++;
    }
    
    return tap;
}

zxtap_file_t* zxtap_file_read(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    (void)fseek(f, 0, SEEK_END);
    long size = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    zxtap_file_t* tap = zxtap_file_parse(data, size);
    free(data);
    
    return tap;
}

bool zxtap_file_write(const zxtap_file_t* tap, const char* filename) {
    if (!tap || !filename) return false;
    
    FILE* f = fopen(filename, "wb");
    if (!f) return false;
    
    uint8_t len_buf[2];
    
    for (size_t i = 0; i < tap->block_count; i++) {
        const zxtap_block_t* block = &tap->blocks[i];
        
        /* Write length */
        write_le16(len_buf, block->length);
        if (fwrite(len_buf, 2, 1, f) != 1) goto error;
        
        /* Write flag */
        if (fputc(block->flag, f) == EOF) goto error;
        
        /* Write data (includes checksum as last byte) */
        size_t data_len = block->length - 1;
        if (fwrite(block->data, 1, data_len, f) != data_len) goto error;
    }
    
    fclose(f);
    return true;
    
error:
    fclose(f);
    return false;
}

bool zxtap_file_add_block(zxtap_file_t* tap, uint8_t flag,
                          const uint8_t* data, size_t length) {
    if (!tap || !data || length == 0) return false;
    
    /* Grow array if needed */
    if (tap->block_count >= tap->capacity) {
        tap->capacity *= 2;
        zxtap_block_t* new_blocks = realloc(tap->blocks, 
                                             tap->capacity * sizeof(zxtap_block_t));
        if (!new_blocks) return false;
        tap->blocks = new_blocks;
    }
    
    zxtap_block_t* block = &tap->blocks[tap->block_count];
    
    /* Length includes flag and checksum */
    block->length = length + 2;
    block->flag = flag;
    
    /* Allocate data + checksum */
    block->data = malloc(length + 1);
    if (!block->data) return false;
    
    memcpy(block->data, data, length);
    
    /* Calculate and append checksum */
    block->checksum = zxtap_checksum(flag, data, length);
    block->data[length] = block->checksum;
    block->checksum_ok = true;
    
    tap->block_count++;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

bool zxtap_parse_header(const zxtap_block_t* block, zxtap_header_t* header) {
    if (!block || !header) return false;
    if (block->flag != ZXTAP_FLAG_HEADER) return false;
    if (block->length != 19) return false;  /* Header is always 19 bytes */
    
    memset(header, 0, sizeof(zxtap_header_t));
    
    const uint8_t* d = block->data;
    
    header->type = d[0];
    memcpy(header->name, d + 1, 10);
    header->name[10] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 9; i >= 0 && header->name[i] == ' '; i--) {
        header->name[i] = '\0';
    }
    
    header->length = read_le16(d + 11);
    header->param1 = read_le16(d + 13);
    header->param2 = read_le16(d + 15);
    
    return true;
}

const char* zxtap_type_name(uint8_t type) {
    switch (type) {
        case ZXTAP_TYPE_PROGRAM:   return "Program";
        case ZXTAP_TYPE_NUMARRAY:  return "Number array";
        case ZXTAP_TYPE_CHARARRAY: return "Character array";
        case ZXTAP_TYPE_CODE:      return "Bytes";
        default:                   return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Checksum Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

uint8_t zxtap_checksum(uint8_t flag, const uint8_t* data, size_t length) {
    uint8_t csum = flag;
    for (size_t i = 0; i < length; i++) {
        csum ^= data[i];
    }
    return csum;
}

bool zxtap_verify_checksum(const zxtap_block_t* block) {
    if (!block || !block->data) return false;
    return block->checksum_ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Conversion
 * ═══════════════════════════════════════════════════════════════════════════ */

zxtap_file_t* zxtap_from_tzx(const uint8_t* tzx_data, size_t tzx_size) {
    if (!tzx_data || tzx_size < TZX_HEADER_SIZE) return NULL;
    
    /* Verify TZX signature */
    if (memcmp(tzx_data, TZX_SIGNATURE, 8) != 0) return NULL;
    
    zxtap_file_t* tap = zxtap_file_create();
    if (!tap) return NULL;
    
    size_t pos = TZX_HEADER_SIZE;
    
    while (pos < tzx_size) {
        uint8_t block_id = tzx_data[pos];
        pos++;
        
        if (pos >= tzx_size) break;
        
        switch (block_id) {
            case TZX_ID_STANDARD: {  /* 0x10 - Standard Speed Data */
                if (pos + 4 > tzx_size) goto done;
                
                /* uint16_t pause_ms = read_le16(tzx_data + pos); */
                uint16_t data_len = read_le16(tzx_data + pos + 2);
                pos += 4;
                
                if (pos + data_len > tzx_size) goto done;
                
                /* TAP block: flag is first byte, checksum is last */
                uint8_t flag = tzx_data[pos];
                const uint8_t* block_data = tzx_data + pos + 1;
                size_t block_data_len = data_len - 2;  /* -flag -checksum */
                
                zxtap_file_add_block(tap, flag, block_data, block_data_len);
                
                pos += data_len;
                break;
            }
            
            case 0x11: {  /* Turbo Speed Data */
                if (pos + 18 > tzx_size) goto done;
                uint32_t data_len = read_le24(tzx_data + pos + 15);
                pos += 18 + data_len;
                break;
            }
            
            case 0x12:   /* Pure Tone */
                pos += 4;
                break;
                
            case 0x13: { /* Pulse Sequence */
                if (pos >= tzx_size) goto done;
                uint8_t count = tzx_data[pos];
                pos += 1 + count * 2;
                break;
            }
            
            case 0x14: { /* Pure Data */
                if (pos + 10 > tzx_size) goto done;
                uint32_t data_len = read_le24(tzx_data + pos + 7);
                pos += 10 + data_len;
                break;
            }
            
            case 0x15: { /* Direct Recording */
                if (pos + 8 > tzx_size) goto done;
                uint32_t data_len = read_le24(tzx_data + pos + 5);
                pos += 8 + data_len;
                break;
            }
            
            case 0x20:   /* Pause */
                pos += 2;
                break;
                
            case 0x21: { /* Group Start */
                if (pos >= tzx_size) goto done;
                pos += 1 + tzx_data[pos];
                break;
            }
            
            case 0x22:   /* Group End */
                break;
                
            case 0x23:   /* Jump */
            case 0x24:   /* Loop Start */
                pos += 2;
                break;
                
            case 0x25:   /* Loop End */
            case 0x27:   /* Return */
                break;
                
            case 0x26: { /* Call Sequence */
                if (pos + 2 > tzx_size) goto done;
                uint16_t count = read_le16(tzx_data + pos);
                pos += 2 + count * 2;
                break;
            }
            
            case 0x28: { /* Select Block */
                if (pos + 2 > tzx_size) goto done;
                uint16_t len = read_le16(tzx_data + pos);
                pos += 2 + len;
                break;
            }
            
            case 0x2A:   /* Stop if 48K */
                pos += 4;
                break;
                
            case 0x2B:   /* Set Signal Level */
                pos += 5;
                break;
                
            case 0x30:   /* Text Description */
            case 0x31: { /* Message Block */
                if (pos >= tzx_size) goto done;
                pos += 1 + tzx_data[pos];
                break;
            }
            
            case 0x32: { /* Archive Info */
                if (pos + 2 > tzx_size) goto done;
                uint16_t len = read_le16(tzx_data + pos);
                pos += 2 + len;
                break;
            }
            
            case 0x33: { /* Hardware Type */
                if (pos >= tzx_size) goto done;
                pos += 1 + tzx_data[pos] * 3;
                break;
            }
            
            case 0x35: { /* Custom Info */
                if (pos + 20 > tzx_size) goto done;
                uint32_t len = read_le16(tzx_data + pos + 16) | 
                              ((uint32_t)read_le16(tzx_data + pos + 18) << 16);
                pos += 20 + len;
                break;
            }
            
            case 0x5A:   /* Glue Block */
                pos += 9;
                break;
                
            default:
                /* Unknown block - skip entire rest */
                goto done;
        }
    }
    
done:
    if (tap->block_count == 0) {
        zxtap_file_free(tap);
        return NULL;
    }
    
    return tap;
}

uint8_t* zxtap_to_tzx(const zxtap_file_t* tap, size_t* out_size) {
    if (!tap || !out_size || tap->block_count == 0) return NULL;
    
    /* Calculate output size */
    size_t size = TZX_HEADER_SIZE;
    for (size_t i = 0; i < tap->block_count; i++) {
        /* Block 0x10: 1 (ID) + 2 (pause) + 2 (len) + data */
        size += 5 + tap->blocks[i].length;
    }
    
    uint8_t* tzx = malloc(size);
    if (!tzx) return NULL;
    
    /* Write header */
    memcpy(tzx, TZX_SIGNATURE, 8);
    tzx[8] = 1;   /* Major version */
    tzx[9] = 20;  /* Minor version */
    
    size_t pos = TZX_HEADER_SIZE;
    
    for (size_t i = 0; i < tap->block_count; i++) {
        const zxtap_block_t* block = &tap->blocks[i];
        
        /* Block ID */
        tzx[pos++] = TZX_ID_STANDARD;
        
        /* Pause: 1000ms for headers, 0ms for data (or last block) */
        uint16_t pause = (block->flag == ZXTAP_FLAG_HEADER) ? 1000 : 
                         (i == tap->block_count - 1) ? 0 : 1000;
        write_le16(tzx + pos, pause);
        pos += 2;
        
        /* Data length */
        write_le16(tzx + pos, block->length);
        pos += 2;
        
        /* Flag byte */
        tzx[pos++] = block->flag;
        
        /* Data (includes checksum) */
        memcpy(tzx + pos, block->data, block->length - 1);
        pos += block->length - 1;
    }
    
    *out_size = pos;
    return tzx;
}

bool zxtap_tzx_to_tap_file(const char* tzx_filename, const char* tap_filename) {
    /* Read TZX */
    FILE* f = fopen(tzx_filename, "rb");
    if (!f) return false;
    
    (void)fseek(f, 0, SEEK_END);
    long size = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return false;
    }
    
    uint8_t* tzx_data = malloc(size);
    if (!tzx_data) {
        fclose(f);
        return false;
    }
    
    if (fread(tzx_data, 1, size, f) != (size_t)size) {
        free(tzx_data);
        fclose(f);
        return false;
    }
    fclose(f);
    
    /* Convert */
    zxtap_file_t* tap = zxtap_from_tzx(tzx_data, size);
    free(tzx_data);
    
    if (!tap) return false;
    
    /* Write TAP */
    bool ok = zxtap_file_write(tap, tap_filename);
    zxtap_file_free(tap);
    
    return ok;
}

bool zxtap_tap_to_tzx_file(const char* tap_filename, const char* tzx_filename) {
    /* Read TAP */
    zxtap_file_t* tap = zxtap_file_read(tap_filename);
    if (!tap) return false;
    
    /* Convert */
    size_t tzx_size;
    uint8_t* tzx_data = zxtap_to_tzx(tap, &tzx_size);
    zxtap_file_free(tap);
    
    if (!tzx_data) return false;
    
    /* Write TZX */
    FILE* f = fopen(tzx_filename, "wb");
    if (!f) {
        free(tzx_data);
        return false;
    }
    
    bool ok = (fwrite(tzx_data, 1, tzx_size, f) == tzx_size);
    fclose(f);
    free(tzx_data);
    
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

void zxtap_print_info(const zxtap_file_t* tap, FILE* out) {
    if (!tap || !out) return;
    
    fprintf(out, "TAP File: %zu blocks\n", tap->block_count);
    fprintf(out, "─────────────────────────────────────────\n");
    
    for (size_t i = 0; i < tap->block_count; i++) {
        const zxtap_block_t* block = &tap->blocks[i];
        
        fprintf(out, "Block %zu: ", i + 1);
        
        if (block->flag == ZXTAP_FLAG_HEADER) {
            zxtap_header_t header;
            if (zxtap_parse_header(block, &header)) {
                fprintf(out, "Header: \"%s\" (%s) %u bytes",
                        header.name, zxtap_type_name(header.type), header.length);
                
                if (header.type == ZXTAP_TYPE_PROGRAM) {
                    if (header.param1 < 32768) {
                        fprintf(out, ", LINE %u", header.param1);
                    }
                } else if (header.type == ZXTAP_TYPE_CODE) {
                    fprintf(out, ", ORG %u", header.param1);
                }
            } else {
                fprintf(out, "Header (invalid)");
            }
        } else {
            fprintf(out, "Data: %u bytes", (unsigned)(block->length - 2));
        }
        
        fprintf(out, " [%s]\n", block->checksum_ok ? "OK" : "BAD CHECKSUM");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Program
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef ZXTAP_TEST
#include <assert.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    printf("=== ZX Spectrum TAP Tests ===\n\n");
    
    /* Test checksum */
    printf("Testing checksum... ");
    uint8_t test_data[] = {0x03, 'T', 'E', 'S', 'T', ' ', ' ', ' ', ' ', ' ', ' ',
                           0x00, 0x10, 0x00, 0x80, 0x00, 0x00};
    uint8_t csum = zxtap_checksum(ZXTAP_FLAG_HEADER, test_data, sizeof(test_data));
    printf("0x%02X ", csum);
    printf("OK\n");
    
    /* Test TAP creation */
    printf("Testing TAP creation... ");
    zxtap_file_t* tap = zxtap_file_create();
    assert(tap != NULL);
    
    /* Add header block */
    assert(zxtap_file_add_block(tap, ZXTAP_FLAG_HEADER, test_data, sizeof(test_data)));
    assert(tap->block_count == 1);
    
    /* Add data block */
    uint8_t data_block[16] = {0};
    assert(zxtap_file_add_block(tap, ZXTAP_FLAG_DATA, data_block, sizeof(data_block)));
    assert(tap->block_count == 2);
    printf("OK\n");
    
    /* Test header parsing */
    printf("Testing header parsing... ");
    zxtap_header_t header;
    assert(zxtap_parse_header(&tap->blocks[0], &header));
    assert(header.type == ZXTAP_TYPE_CODE);
    assert(strcmp(header.name, "TEST") == 0);
    printf("OK\n");
    
    /* Test TAP -> TZX conversion */
    printf("Testing TAP -> TZX... ");
    size_t tzx_size;
    uint8_t* tzx_data = zxtap_to_tzx(tap, &tzx_size);
    assert(tzx_data != NULL);
    assert(tzx_size > TZX_HEADER_SIZE);
    assert(memcmp(tzx_data, TZX_SIGNATURE, 8) == 0);
    printf("OK (%zu bytes)\n", tzx_size);
    
    /* Test TZX -> TAP conversion */
    printf("Testing TZX -> TAP... ");
    zxtap_file_t* tap2 = zxtap_from_tzx(tzx_data, tzx_size);
    assert(tap2 != NULL);
    assert(tap2->block_count == 2);
    printf("OK (%zu blocks)\n", tap2->block_count);
    
    /* Print info */
    printf("\nTAP Info:\n");
    zxtap_print_info(tap2, stdout);
    
    /* Cleanup */
    free(tzx_data);
    zxtap_file_free(tap);
    zxtap_file_free(tap2);
    
    /* Test with files if provided */
    if (argc >= 3) {
        printf("\nConverting %s -> %s... ", argv[1], argv[2]);
        
        const char* ext1 = strrchr(argv[1], '.');
        const char* ext2 = strrchr(argv[2], '.');
        
        bool ok = false;
        if (ext1 && ext2) {
            if (strcasecmp(ext1, ".tzx") == 0 && strcasecmp(ext2, ".tap") == 0) {
                ok = zxtap_tzx_to_tap_file(argv[1], argv[2]);
            } else if (strcasecmp(ext1, ".tap") == 0 && strcasecmp(ext2, ".tzx") == 0) {
                ok = zxtap_tap_to_tzx_file(argv[1], argv[2]);
            }
        }
        
        printf("%s\n", ok ? "OK" : "FAILED");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
#endif
