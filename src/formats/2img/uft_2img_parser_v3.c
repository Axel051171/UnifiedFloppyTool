/**
 * @file uft_2img_parser_v3.c
 * @brief GOD MODE 2IMG Parser v3 - Apple II Universal Disk Image
 * 
 * 2IMG ist das universelle Apple II Image-Format:
 * - DOS 3.3, ProDOS, NIB Support
 * - Creator und Comment Felder
 * - Write Protection Flag
 * - Volume Number
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define IMG2_SIGNATURE          "2IMG"
#define IMG2_HEADER_SIZE        64
#define IMG2_HEADER_SIZE_MIN    52

/* Image formats */
#define IMG2_FORMAT_DOS_ORDER   0       /* DOS 3.3 sector order */
#define IMG2_FORMAT_PRODOS      1       /* ProDOS sector order */
#define IMG2_FORMAT_NIB         2       /* Nibble data */

/* Standard sizes */
#define IMG2_SIZE_140K          143360  /* 5.25" */
#define IMG2_SIZE_800K          819200  /* 3.5" */

typedef enum {
    IMG2_DIAG_OK = 0,
    IMG2_DIAG_BAD_SIGNATURE,
    IMG2_DIAG_BAD_HEADER_SIZE,
    IMG2_DIAG_TRUNCATED,
    IMG2_DIAG_BAD_FORMAT,
    IMG2_DIAG_COUNT
} img2_diag_code_t;

typedef struct { float overall; bool valid; uint8_t format; } img2_score_t;
typedef struct { img2_diag_code_t code; char msg[128]; } img2_diagnosis_t;
typedef struct { img2_diagnosis_t* items; size_t count; size_t cap; float quality; } img2_diagnosis_list_t;

typedef struct {
    char signature[5];
    char creator[5];
    uint16_t header_size;
    uint16_t version;
    uint32_t image_format;
    uint32_t flags;
    uint32_t prodos_blocks;
    uint32_t data_offset;
    uint32_t data_length;
    uint32_t comment_offset;
    uint32_t comment_length;
    uint32_t creator_offset;
    uint32_t creator_length;
    
    /* Derived */
    bool locked;
    bool has_volume;
    uint8_t volume_number;
    char comment[256];
    char creator_data[64];
    
    img2_score_t score;
    img2_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} img2_disk_t;

static img2_diagnosis_list_t* img2_diagnosis_create(void) {
    img2_diagnosis_list_t* l = calloc(1, sizeof(img2_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(img2_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void img2_diagnosis_free(img2_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t img2_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t img2_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static const char* img2_format_name(uint32_t f) {
    switch (f) {
        case IMG2_FORMAT_DOS_ORDER: return "DOS 3.3 Order";
        case IMG2_FORMAT_PRODOS: return "ProDOS Order";
        case IMG2_FORMAT_NIB: return "Nibble";
        default: return "Unknown";
    }
}

static bool img2_parse(const uint8_t* data, size_t size, img2_disk_t* disk) {
    if (!data || !disk || size < IMG2_HEADER_SIZE_MIN) return false;
    memset(disk, 0, sizeof(img2_disk_t));
    disk->diagnosis = img2_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, IMG2_SIGNATURE, 4) != 0) {
        return false;
    }
    memcpy(disk->signature, data, 4);
    disk->signature[4] = '\0';
    
    /* Creator */
    memcpy(disk->creator, data + 4, 4);
    disk->creator[4] = '\0';
    
    /* Parse header */
    disk->header_size = img2_read_le16(data + 8);
    disk->version = img2_read_le16(data + 10);
    disk->image_format = img2_read_le32(data + 12);
    disk->flags = img2_read_le32(data + 16);
    disk->prodos_blocks = img2_read_le32(data + 20);
    disk->data_offset = img2_read_le32(data + 24);
    disk->data_length = img2_read_le32(data + 28);
    disk->comment_offset = img2_read_le32(data + 32);
    disk->comment_length = img2_read_le32(data + 36);
    disk->creator_offset = img2_read_le32(data + 40);
    disk->creator_length = img2_read_le32(data + 44);
    
    /* Flags */
    disk->locked = (disk->flags & 0x80000000) != 0;
    disk->has_volume = (disk->flags & 0x00000100) != 0;
    if (disk->has_volume) {
        disk->volume_number = disk->flags & 0xFF;
    }
    
    /* Read comment if present */
    if (disk->comment_offset > 0 && disk->comment_length > 0 &&
        disk->comment_offset + disk->comment_length <= size) {
        size_t len = disk->comment_length;
        if (len > sizeof(disk->comment) - 1) len = sizeof(disk->comment) - 1;
        memcpy(disk->comment, data + disk->comment_offset, len);
        disk->comment[len] = '\0';
    }
    
    /* Validate data */
    if (disk->data_offset + disk->data_length > size) {
        disk->diagnosis->quality *= 0.8f;
    }
    
    disk->score.format = disk->image_format;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void img2_disk_free(img2_disk_t* disk) {
    if (disk && disk->diagnosis) img2_diagnosis_free(disk->diagnosis);
}

#ifdef IMG2_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== 2IMG Parser v3 Tests ===\n");
    
    printf("Testing format names... ");
    assert(strcmp(img2_format_name(IMG2_FORMAT_DOS_ORDER), "DOS 3.3 Order") == 0);
    assert(strcmp(img2_format_name(IMG2_FORMAT_PRODOS), "ProDOS Order") == 0);
    printf("✓\n");
    
    printf("Testing 2IMG parsing... ");
    uint8_t img2[128];
    memset(img2, 0, sizeof(img2));
    memcpy(img2, "2IMG", 4);
    memcpy(img2 + 4, "XGS!", 4);
    img2[8] = 64; img2[9] = 0;  /* Header size */
    img2[10] = 1; img2[11] = 0; /* Version */
    img2[12] = 1; /* ProDOS format */
    img2[24] = 64; /* Data offset */
    img2[28] = 64; /* Data length */
    
    img2_disk_t disk;
    bool ok = img2_parse(img2, sizeof(img2), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.image_format == IMG2_FORMAT_PRODOS);
    assert(disk.header_size == 64);
    img2_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
