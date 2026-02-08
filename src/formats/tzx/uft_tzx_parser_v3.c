/**
 * @file uft_tzx_parser_v3.c
 * @brief GOD MODE TZX Parser v3 - ZX Spectrum Tape Format
 * 
 * TZX ist das universelle Tape-Format für ZX Spectrum:
 * - Block-basiert mit vielen Block-Typen
 * - Unterstützt Standard/Turbo/Direct Recording
 * - Pause, Loop, Jump, etc.
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TZX_SIGNATURE           "ZXTape!\x1A"
#define TZX_SIGNATURE_LEN       8
#define TZX_HEADER_SIZE         10

/* Block IDs */
#define TZX_ID_STANDARD         0x10
#define TZX_ID_TURBO            0x11
#define TZX_ID_PURE_TONE        0x12
#define TZX_ID_PULSES           0x13
#define TZX_ID_PURE_DATA        0x14
#define TZX_ID_DIRECT           0x15
#define TZX_ID_CSW              0x18
#define TZX_ID_GENERALIZED      0x19
#define TZX_ID_PAUSE            0x20
#define TZX_ID_GROUP_START      0x21
#define TZX_ID_GROUP_END        0x22
#define TZX_ID_JUMP             0x23
#define TZX_ID_LOOP_START       0x24
#define TZX_ID_LOOP_END         0x25
#define TZX_ID_SELECT           0x28
#define TZX_ID_STOP_48K         0x2A
#define TZX_ID_SET_LEVEL        0x2B
#define TZX_ID_TEXT             0x30
#define TZX_ID_MESSAGE          0x31
#define TZX_ID_ARCHIVE          0x32
#define TZX_ID_HARDWARE         0x33
#define TZX_ID_CUSTOM           0x35
#define TZX_ID_GLUE             0x5A

typedef enum {
    TZX_DIAG_OK = 0,
    TZX_DIAG_BAD_SIGNATURE,
    TZX_DIAG_BAD_VERSION,
    TZX_DIAG_TRUNCATED,
    TZX_DIAG_UNKNOWN_BLOCK,
    TZX_DIAG_COUNT
} tzx_diag_code_t;

typedef struct { float overall; bool valid; uint16_t blocks; float duration_sec; } tzx_score_t;
typedef struct { tzx_diag_code_t code; uint16_t block; char msg[128]; } tzx_diagnosis_t;
typedef struct { tzx_diagnosis_t* items; size_t count; size_t cap; float quality; } tzx_diagnosis_list_t;

typedef struct {
    uint8_t id;
    uint32_t offset;
    uint32_t length;
    uint16_t pause_ms;
    char description[64];
} tzx_block_t;

typedef struct {
    uint8_t version_major;
    uint8_t version_minor;
    
    tzx_block_t* blocks;
    uint16_t block_count;
    uint16_t block_capacity;
    
    /* Statistics */
    uint16_t standard_blocks;
    uint16_t turbo_blocks;
    uint16_t data_blocks;
    uint32_t total_pause_ms;
    float duration_seconds;
    
    tzx_score_t score;
    tzx_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} tzx_disk_t;

static tzx_diagnosis_list_t* tzx_diagnosis_create(void) {
    tzx_diagnosis_list_t* l = calloc(1, sizeof(tzx_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(tzx_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void tzx_diagnosis_free(tzx_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t tzx_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t tzx_read_le24(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16); }
static uint32_t tzx_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static const char* tzx_block_name(uint8_t id) {
    switch (id) {
        case TZX_ID_STANDARD: return "Standard Speed Data";
        case TZX_ID_TURBO: return "Turbo Speed Data";
        case TZX_ID_PURE_TONE: return "Pure Tone";
        case TZX_ID_PULSES: return "Pulse Sequence";
        case TZX_ID_PURE_DATA: return "Pure Data";
        case TZX_ID_DIRECT: return "Direct Recording";
        case TZX_ID_PAUSE: return "Pause";
        case TZX_ID_GROUP_START: return "Group Start";
        case TZX_ID_GROUP_END: return "Group End";
        case TZX_ID_TEXT: return "Text Description";
        case TZX_ID_ARCHIVE: return "Archive Info";
        default: return "Unknown";
    }
}

static bool tzx_parse(const uint8_t* data, size_t size, tzx_disk_t* disk) {
    if (!data || !disk || size < TZX_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(tzx_disk_t));
    disk->diagnosis = tzx_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, TZX_SIGNATURE, TZX_SIGNATURE_LEN) != 0) {
        return false;
    }
    
    disk->version_major = data[8];
    disk->version_minor = data[9];
    
    /* Allocate blocks array */
    disk->block_capacity = 256;
    disk->blocks = calloc(disk->block_capacity, sizeof(tzx_block_t));
    if (!disk->blocks) return false;
    
    /* Parse blocks */
    size_t pos = TZX_HEADER_SIZE;
    while (pos < size && disk->block_count < disk->block_capacity) {
        uint8_t id = data[pos];
        tzx_block_t* block = &disk->blocks[disk->block_count];
        
        block->id = id;
        block->offset = pos;
        strncpy(block->description, tzx_block_name(id), sizeof(block->description) - 1);
        
        pos++;
        
        switch (id) {
            case TZX_ID_STANDARD:
                if (pos + 4 > size) goto done;
                block->pause_ms = tzx_read_le16(data + pos);
                block->length = tzx_read_le16(data + pos + 2);
                pos += 4 + block->length;
                disk->standard_blocks++;
                disk->total_pause_ms += block->pause_ms;
                break;
                
            case TZX_ID_TURBO:
                if (pos + 18 > size) goto done;
                block->pause_ms = tzx_read_le16(data + pos + 13);
                block->length = tzx_read_le24(data + pos + 15);
                pos += 18 + block->length;
                disk->turbo_blocks++;
                disk->total_pause_ms += block->pause_ms;
                break;
                
            case TZX_ID_PURE_TONE:
                pos += 4;
                break;
                
            case TZX_ID_PULSES:
                if (pos + 1 > size) goto done;
                pos += 1 + data[pos] * 2;
                break;
                
            case TZX_ID_PURE_DATA:
                if (pos + 10 > size) goto done;
                block->pause_ms = tzx_read_le16(data + pos + 5);
                block->length = tzx_read_le24(data + pos + 7);
                pos += 10 + block->length;
                disk->data_blocks++;
                disk->total_pause_ms += block->pause_ms;
                break;
                
            case TZX_ID_DIRECT:
                if (pos + 8 > size) goto done;
                block->pause_ms = tzx_read_le16(data + pos + 3);
                block->length = tzx_read_le24(data + pos + 5);
                pos += 8 + block->length;
                disk->total_pause_ms += block->pause_ms;
                break;
                
            case TZX_ID_PAUSE:
                if (pos + 2 > size) goto done;
                block->pause_ms = tzx_read_le16(data + pos);
                pos += 2;
                disk->total_pause_ms += block->pause_ms;
                break;
                
            case TZX_ID_GROUP_START:
                if (pos + 1 > size) goto done;
                pos += 1 + data[pos];
                break;
                
            case TZX_ID_GROUP_END:
                break;
                
            case TZX_ID_JUMP:
            case TZX_ID_LOOP_START:
                pos += 2;
                break;
                
            case TZX_ID_LOOP_END:
            case TZX_ID_STOP_48K:
                break;
                
            case TZX_ID_TEXT:
            case TZX_ID_MESSAGE:
                if (pos + 1 > size) goto done;
                pos += 1 + data[pos];
                break;
                
            case TZX_ID_ARCHIVE:
                if (pos + 2 > size) goto done;
                pos += 2 + tzx_read_le16(data + pos);
                break;
                
            case TZX_ID_HARDWARE:
                if (pos + 1 > size) goto done;
                pos += 1 + data[pos] * 3;
                break;
                
            case TZX_ID_CUSTOM:
                if (pos + 4 > size) goto done;
                pos += 4 + tzx_read_le32(data + pos + 16);
                break;
                
            case TZX_ID_GLUE:
                pos += 9;
                break;
                
            default:
                /* Unknown block - try to skip */
                if (pos + 4 <= size) {
                    uint32_t len = tzx_read_le32(data + pos);
                    if (len < size) pos += 4 + len;
                    else goto done;
                } else {
                    goto done;
                }
                break;
        }
        
        disk->block_count++;
    }
    
done:
    disk->duration_seconds = disk->total_pause_ms / 1000.0f;
    
    disk->score.blocks = disk->block_count;
    disk->score.duration_sec = disk->duration_seconds;
    disk->score.overall = disk->block_count > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->block_count > 0;
    disk->valid = true;
    
    return true;
}

static void tzx_disk_free(tzx_disk_t* disk) {
    if (disk) {
        if (disk->blocks) free(disk->blocks);
        if (disk->diagnosis) tzx_diagnosis_free(disk->diagnosis);
    }
}

#ifdef TZX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TZX Parser v3 Tests ===\n");
    
    printf("Testing block names... ");
    assert(strcmp(tzx_block_name(TZX_ID_STANDARD), "Standard Speed Data") == 0);
    assert(strcmp(tzx_block_name(TZX_ID_TURBO), "Turbo Speed Data") == 0);
    printf("✓\n");
    
    printf("Testing TZX parsing... ");
    uint8_t tzx[64];
    memset(tzx, 0, sizeof(tzx));
    memcpy(tzx, "ZXTape!\x1A", 8);
    tzx[8] = 1;  /* Version 1.20 */
    tzx[9] = 20;
    /* Add a pause block */
    tzx[10] = TZX_ID_PAUSE;
    tzx[11] = 0xE8; tzx[12] = 0x03;  /* 1000ms */
    
    tzx_disk_t disk;
    bool ok = tzx_parse(tzx, sizeof(tzx), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.version_major == 1);
    assert(disk.block_count >= 1);
    tzx_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
