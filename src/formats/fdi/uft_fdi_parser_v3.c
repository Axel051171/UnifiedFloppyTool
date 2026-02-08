/**
 * @file uft_fdi_parser_v3.c
 * @brief GOD MODE FDI Parser v3 - Formatted Disk Image
 * 
 * FDI ist ein universelles Format für PC/Spectrum/etc:
 * - Variable Geometrie
 * - Mehrere Varianten (UKV, others)
 * - Sektor-basiert mit Metadaten
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FDI_SIGNATURE           "FDI"
#define FDI_HEADER_SIZE         14

typedef enum {
    FDI_DIAG_OK = 0,
    FDI_DIAG_BAD_SIGNATURE,
    FDI_DIAG_BAD_GEOMETRY,
    FDI_DIAG_TRUNCATED,
    FDI_DIAG_COUNT
} fdi_diag_code_t;

typedef struct { float overall; bool valid; } fdi_score_t;
typedef struct { fdi_diag_code_t code; char msg[128]; } fdi_diagnosis_t;
typedef struct { fdi_diagnosis_t* items; size_t count; size_t cap; float quality; } fdi_diagnosis_list_t;

typedef struct {
    char signature[4];
    uint8_t write_protect;
    uint16_t cylinders;
    uint16_t heads;
    uint16_t description_offset;
    uint16_t data_offset;
    uint16_t extra_offset;
    
    char description[256];
    
    fdi_score_t score;
    fdi_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} fdi_disk_t;

static fdi_diagnosis_list_t* fdi_diagnosis_create(void) {
    fdi_diagnosis_list_t* l = calloc(1, sizeof(fdi_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(fdi_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void fdi_diagnosis_free(fdi_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t fdi_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool fdi_parse(const uint8_t* data, size_t size, fdi_disk_t* disk) {
    if (!data || !disk || size < FDI_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(fdi_disk_t));
    disk->diagnosis = fdi_diagnosis_create();
    disk->source_size = size;
    
    if (memcmp(data, FDI_SIGNATURE, 3) != 0) {
        return false;
    }
    memcpy(disk->signature, data, 3);
    disk->signature[3] = '\0';
    
    disk->write_protect = data[3];
    disk->cylinders = fdi_read_le16(data + 4);
    disk->heads = fdi_read_le16(data + 6);
    disk->description_offset = fdi_read_le16(data + 8);
    disk->data_offset = fdi_read_le16(data + 10);
    disk->extra_offset = fdi_read_le16(data + 12);
    
    /* Read description if present */
    if (disk->description_offset > 0 && disk->description_offset < size) {
        size_t len = 0;
        while (disk->description_offset + len < size && 
               data[disk->description_offset + len] != 0 &&
               len < sizeof(disk->description) - 1) {
            disk->description[len] = data[disk->description_offset + len];
            len++;
        }
        disk->description[len] = '\0';
    }
    
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void fdi_disk_free(fdi_disk_t* disk) {
    if (disk && disk->diagnosis) fdi_diagnosis_free(disk->diagnosis);
}

#ifdef FDI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FDI Parser v3 Tests ===\n");
    
    printf("Testing FDI parsing... ");
    uint8_t fdi[32];
    memset(fdi, 0, sizeof(fdi));
    memcpy(fdi, "FDI", 3);
    fdi[4] = 80; fdi[5] = 0;  /* 80 cylinders */
    fdi[6] = 2; fdi[7] = 0;   /* 2 heads */
    
    fdi_disk_t disk;
    bool ok = fdi_parse(fdi, sizeof(fdi), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.cylinders == 80);
    assert(disk.heads == 2);
    fdi_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
