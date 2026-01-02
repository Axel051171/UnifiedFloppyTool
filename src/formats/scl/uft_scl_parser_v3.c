/**
 * @file uft_scl_parser_v3.c
 * @brief GOD MODE SCL Parser v3 - ZX Spectrum Sinclair Container
 * 
 * SCL ist ein einfaches Container-Format für TR-DOS Dateien:
 * - 8-Byte Header + Directory
 * - Concatenierte Dateien
 * - Kein Filesystem-Overhead
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SCL_SIGNATURE           "SINCLAIR"
#define SCL_SIGNATURE_LEN       8
#define SCL_HEADER_SIZE         9
#define SCL_ENTRY_SIZE          14
#define SCL_MAX_FILES           255

typedef enum {
    SCL_DIAG_OK = 0,
    SCL_DIAG_BAD_SIGNATURE,
    SCL_DIAG_TRUNCATED,
    SCL_DIAG_FILE_ERROR,
    SCL_DIAG_COUNT
} scl_diag_code_t;

typedef struct { float overall; bool valid; uint8_t files; } scl_score_t;
typedef struct { scl_diag_code_t code; char msg[128]; } scl_diagnosis_t;
typedef struct { scl_diagnosis_t* items; size_t count; size_t cap; float quality; } scl_diagnosis_list_t;

typedef struct {
    char name[9];
    char extension;
    uint16_t start;
    uint16_t length;
    uint8_t sectors;
    uint32_t data_offset;
} scl_file_t;

typedef struct {
    uint8_t file_count;
    
    scl_file_t files[SCL_MAX_FILES];
    uint8_t valid_files;
    uint32_t total_data;
    
    scl_score_t score;
    scl_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} scl_disk_t;

static scl_diagnosis_list_t* scl_diagnosis_create(void) {
    scl_diagnosis_list_t* l = calloc(1, sizeof(scl_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(scl_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void scl_diagnosis_free(scl_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t scl_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool scl_parse(const uint8_t* data, size_t size, scl_disk_t* disk) {
    if (!data || !disk || size < SCL_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(scl_disk_t));
    disk->diagnosis = scl_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, SCL_SIGNATURE, SCL_SIGNATURE_LEN) != 0) {
        return false;
    }
    
    disk->file_count = data[8];
    
    /* Calculate directory size */
    size_t dir_size = SCL_HEADER_SIZE + disk->file_count * SCL_ENTRY_SIZE;
    if (dir_size > size) return false;
    
    /* Parse file entries */
    disk->valid_files = 0;
    uint32_t data_offset = dir_size;
    
    for (int i = 0; i < disk->file_count && i < SCL_MAX_FILES; i++) {
        const uint8_t* entry = data + SCL_HEADER_SIZE + i * SCL_ENTRY_SIZE;
        scl_file_t* file = &disk->files[disk->valid_files];
        
        memcpy(file->name, entry, 8);
        file->name[8] = '\0';
        file->extension = entry[8];
        file->start = scl_read_le16(entry + 9);
        file->length = scl_read_le16(entry + 11);
        file->sectors = entry[13];
        file->data_offset = data_offset;
        
        data_offset += file->sectors * 256;
        disk->valid_files++;
    }
    
    disk->total_data = data_offset - dir_size;
    
    disk->score.files = disk->valid_files;
    disk->score.overall = disk->valid_files > 0 ? 1.0f : 0.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void scl_disk_free(scl_disk_t* disk) {
    if (disk && disk->diagnosis) scl_diagnosis_free(disk->diagnosis);
}

#ifdef SCL_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SCL Parser v3 Tests ===\n");
    
    printf("Testing SCL parsing... ");
    uint8_t scl[64];
    memset(scl, 0, sizeof(scl));
    memcpy(scl, "SINCLAIR", 8);
    scl[8] = 1;  /* 1 file */
    /* File entry */
    memcpy(scl + 9, "TEST    ", 8);
    scl[17] = 'C';  /* Extension */
    scl[18] = 0; scl[19] = 0x60;  /* Start $6000 */
    scl[20] = 0; scl[21] = 0x10;  /* Length $1000 */
    scl[22] = 16;  /* 16 sectors */
    
    scl_disk_t disk;
    bool ok = scl_parse(scl, sizeof(scl), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.file_count == 1);
    scl_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
