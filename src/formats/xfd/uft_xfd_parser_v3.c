/**
 * @file uft_xfd_parser_v3.c
 * @brief GOD MODE XFD Parser v3 - Atari 8-bit Raw Disk Image
 * 
 * XFD ist ein headerless Atari 8-bit Format:
 * - Rohe Sektordaten
 * - Geometrie aus Dateigröße abgeleitet
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Standard sizes */
#define XFD_SIZE_SD_720         (720 * 128)     /* 90K Single Density */
#define XFD_SIZE_ED_1040        (1040 * 128)    /* 130K Enhanced */
#define XFD_SIZE_DD_720         (720 * 256)     /* 180K Double */
#define XFD_SIZE_DD_1440        (1440 * 256)    /* 360K Quad */

typedef enum {
    XFD_DIAG_OK = 0,
    XFD_DIAG_INVALID_SIZE,
    XFD_DIAG_COUNT
} xfd_diag_code_t;

typedef enum {
    XFD_DENSITY_SD = 0,
    XFD_DENSITY_ED = 1,
    XFD_DENSITY_DD = 2,
    XFD_DENSITY_QD = 3
} xfd_density_t;

typedef struct { float overall; bool valid; xfd_density_t density; } xfd_score_t;
typedef struct { xfd_diag_code_t code; char msg[128]; } xfd_diagnosis_t;
typedef struct { xfd_diagnosis_t* items; size_t count; size_t cap; float quality; } xfd_diagnosis_list_t;

typedef struct {
    xfd_density_t density;
    uint16_t sector_count;
    uint16_t sector_size;
    uint8_t tracks;
    uint8_t sectors_per_track;
    
    xfd_score_t score;
    xfd_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} xfd_disk_t;

static xfd_diagnosis_list_t* xfd_diagnosis_create(void) {
    xfd_diagnosis_list_t* l = calloc(1, sizeof(xfd_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(xfd_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void xfd_diagnosis_free(xfd_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static const char* xfd_density_name(xfd_density_t d) {
    switch (d) {
        case XFD_DENSITY_SD: return "Single Density";
        case XFD_DENSITY_ED: return "Enhanced Density";
        case XFD_DENSITY_DD: return "Double Density";
        case XFD_DENSITY_QD: return "Quad Density";
        default: return "Unknown";
    }
}

static bool xfd_detect_geometry(size_t size, xfd_disk_t* disk) {
    if (size == XFD_SIZE_SD_720) {
        disk->density = XFD_DENSITY_SD;
        disk->sector_size = 128;
        disk->sector_count = 720;
        disk->tracks = 40;
        disk->sectors_per_track = 18;
        return true;
    }
    if (size == XFD_SIZE_ED_1040) {
        disk->density = XFD_DENSITY_ED;
        disk->sector_size = 128;
        disk->sector_count = 1040;
        disk->tracks = 40;
        disk->sectors_per_track = 26;
        return true;
    }
    if (size == XFD_SIZE_DD_720) {
        disk->density = XFD_DENSITY_DD;
        disk->sector_size = 256;
        disk->sector_count = 720;
        disk->tracks = 40;
        disk->sectors_per_track = 18;
        return true;
    }
    if (size == XFD_SIZE_DD_1440) {
        disk->density = XFD_DENSITY_QD;
        disk->sector_size = 256;
        disk->sector_count = 1440;
        disk->tracks = 80;
        disk->sectors_per_track = 18;
        return true;
    }
    
    /* Try to guess */
    if (size % 128 == 0 && size <= 133120) {
        disk->density = XFD_DENSITY_SD;
        disk->sector_size = 128;
        disk->sector_count = size / 128;
        disk->tracks = 40;
        disk->sectors_per_track = 18;
        return true;
    }
    if (size % 256 == 0) {
        disk->density = XFD_DENSITY_DD;
        disk->sector_size = 256;
        disk->sector_count = size / 256;
        disk->tracks = 40;
        disk->sectors_per_track = 18;
        return true;
    }
    
    return false;
}

static bool xfd_parse(const uint8_t* data, size_t size, xfd_disk_t* disk) {
    if (!data || !disk || size < 128) return false;
    memset(disk, 0, sizeof(xfd_disk_t));
    disk->diagnosis = xfd_diagnosis_create();
    disk->source_size = size;
    
    if (!xfd_detect_geometry(size, disk)) {
        return false;
    }
    
    disk->score.density = disk->density;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void xfd_disk_free(xfd_disk_t* disk) {
    if (disk && disk->diagnosis) xfd_diagnosis_free(disk->diagnosis);
}

#ifdef XFD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== XFD Parser v3 Tests ===\n");
    
    printf("Testing density names... ");
    assert(strcmp(xfd_density_name(XFD_DENSITY_SD), "Single Density") == 0);
    assert(strcmp(xfd_density_name(XFD_DENSITY_DD), "Double Density") == 0);
    printf("✓\n");
    
    printf("Testing XFD geometry detection... ");
    uint8_t* xfd = calloc(1, XFD_SIZE_SD_720);
    
    xfd_disk_t disk;
    bool ok = xfd_parse(xfd, XFD_SIZE_SD_720, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.density == XFD_DENSITY_SD);
    assert(disk.sector_size == 128);
    assert(disk.sector_count == 720);
    xfd_disk_free(&disk);
    free(xfd);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
