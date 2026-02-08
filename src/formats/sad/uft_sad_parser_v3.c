/**
 * @file uft_sad_parser_v3.c
 * @brief GOD MODE SAD Parser v3 - Sam Coupe Disk Image
 * 
 * SAD ist das native Sam Coupe Format:
 * - 22-Byte Header
 * - 80 Tracks × 2 Seiten × 10 Sektoren
 * - 512 Bytes pro Sektor
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SAD_SIGNATURE           "Aley's disk backup"
#define SAD_SIGNATURE_LEN       18
#define SAD_HEADER_SIZE         22

#define SAD_SECTOR_SIZE         512
#define SAD_SECTORS_PER_TRACK   10
#define SAD_DEFAULT_TRACKS      80
#define SAD_DEFAULT_SIDES       2

typedef enum {
    SAD_DIAG_OK = 0,
    SAD_DIAG_BAD_SIGNATURE,
    SAD_DIAG_TRUNCATED,
    SAD_DIAG_COUNT
} sad_diag_code_t;

typedef struct { float overall; bool valid; } sad_score_t;
typedef struct { sad_diag_code_t code; char msg[128]; } sad_diagnosis_t;
typedef struct { sad_diagnosis_t* items; size_t count; size_t cap; float quality; } sad_diagnosis_list_t;

typedef struct {
    char signature[19];
    uint8_t sides;
    uint8_t tracks;
    uint8_t sectors;
    uint8_t sector_size_code;
    uint16_t sector_size;
    
    uint32_t data_size;
    
    sad_score_t score;
    sad_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} sad_disk_t;

static sad_diagnosis_list_t* sad_diagnosis_create(void) {
    sad_diagnosis_list_t* l = calloc(1, sizeof(sad_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(sad_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void sad_diagnosis_free(sad_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static bool sad_parse(const uint8_t* data, size_t size, sad_disk_t* disk) {
    if (!data || !disk || size < SAD_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(sad_disk_t));
    disk->diagnosis = sad_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, SAD_SIGNATURE, SAD_SIGNATURE_LEN) != 0) {
        return false;
    }
    memcpy(disk->signature, data, 18);
    disk->signature[18] = '\0';
    
    /* Parse header */
    disk->sides = data[18];
    disk->tracks = data[19];
    disk->sectors = data[20];
    disk->sector_size_code = data[21];
    
    /* Calculate sector size (0=256, 1=512, 2=1024) */
    disk->sector_size = 256 << disk->sector_size_code;
    
    /* Validate */
    if (disk->sides == 0) disk->sides = SAD_DEFAULT_SIDES;
    if (disk->tracks == 0) disk->tracks = SAD_DEFAULT_TRACKS;
    if (disk->sectors == 0) disk->sectors = SAD_SECTORS_PER_TRACK;
    
    disk->data_size = disk->sides * disk->tracks * disk->sectors * disk->sector_size;
    
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void sad_disk_free(sad_disk_t* disk) {
    if (disk && disk->diagnosis) sad_diagnosis_free(disk->diagnosis);
}

#ifdef SAD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SAD Parser v3 Tests ===\n");
    
    printf("Testing SAD parsing... ");
    uint8_t sad[64];
    memset(sad, 0, sizeof(sad));
    memcpy(sad, "Aley's disk backup", 18);
    sad[18] = 2;    /* Sides */
    sad[19] = 80;   /* Tracks */
    sad[20] = 10;   /* Sectors */
    sad[21] = 1;    /* 512 bytes */
    
    sad_disk_t disk;
    bool ok = sad_parse(sad, sizeof(sad), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.sides == 2);
    assert(disk.tracks == 80);
    assert(disk.sector_size == 512);
    sad_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
