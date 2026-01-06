/**
 * @file uft_img_parser_v3.c
 * @brief GOD MODE IMG Parser v3 - PC Raw Sector Images (IMG/IMA/DSK)
 * 
 * IMG ist das einfache PC-Sektor-Format:
 * - 160K bis 2.88M Support
 * - Automatische Geometrie-Erkennung
 * - FAT12/FAT16 Boot-Sektor Parsing
 * - BPB (BIOS Parameter Block) Analyse
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Standard PC disk sizes */
#define IMG_SIZE_160K           163840
#define IMG_SIZE_180K           184320
#define IMG_SIZE_320K           327680
#define IMG_SIZE_360K           368640
#define IMG_SIZE_720K           737280
#define IMG_SIZE_1200K          1228800
#define IMG_SIZE_1440K          1474560
#define IMG_SIZE_2880K          2949120

#define IMG_SECTOR_SIZE         512
#define IMG_MAX_TRACKS          84
#define IMG_MAX_SECTORS         36

typedef enum {
    IMG_DIAG_OK = 0,
    IMG_DIAG_INVALID_SIZE,
    IMG_DIAG_BAD_BOOT_SECTOR,
    IMG_DIAG_BAD_BPB,
    IMG_DIAG_GEOMETRY_MISMATCH,
    IMG_DIAG_FAT_ERROR,
    IMG_DIAG_ROOT_DIR_ERROR,
    IMG_DIAG_NON_STANDARD,
    IMG_DIAG_COUNT
} img_diag_code_t;

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} img_bpb_t;

typedef struct { float overall; bool valid; bool bpb_valid; } img_score_t;
typedef struct { img_diag_code_t code; char msg[128]; } img_diagnosis_t;
typedef struct { img_diagnosis_t* items; size_t count; size_t cap; float quality; } img_diagnosis_list_t;

typedef struct {
    /* Geometry */
    uint8_t tracks;
    uint8_t heads;
    uint8_t sectors;
    uint32_t total_sectors;
    size_t disk_size;
    char format_name[32];
    
    /* Boot sector */
    uint8_t boot_sector[512];
    bool has_boot_sector;
    uint8_t boot_signature;
    char oem_name[9];
    
    /* BPB */
    img_bpb_t bpb;
    bool bpb_valid;
    
    /* FAT info */
    uint8_t fat_type;       /* 12, 16, or 32 */
    uint32_t fat_start;
    uint32_t root_start;
    uint32_t data_start;
    uint32_t free_clusters;
    
    img_score_t score;
    img_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} img_disk_t;

static img_diagnosis_list_t* img_diagnosis_create(void) {
    img_diagnosis_list_t* l = calloc(1, sizeof(img_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(img_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void img_diagnosis_free(img_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t img_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t img_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

static bool img_detect_geometry(size_t size, uint8_t* tracks, uint8_t* heads, uint8_t* sectors, char* name) {
    switch (size) {
        case IMG_SIZE_160K:  *tracks = 40; *heads = 1; *sectors = 8;  strcpy(name, "160K SS/DD");  /* REVIEW: Consider bounds check */ return true;
        case IMG_SIZE_180K:  *tracks = 40; *heads = 1; *sectors = 9;  strcpy(name, "180K SS/DD");  /* REVIEW: Consider bounds check */ return true;
        case IMG_SIZE_320K:  *tracks = 40; *heads = 2; *sectors = 8;  strcpy(name, "320K DS/DD");  /* REVIEW: Consider bounds check */ return true;
        case IMG_SIZE_360K:  *tracks = 40; *heads = 2; *sectors = 9;  strcpy(name, "360K DS/DD");  /* REVIEW: Consider bounds check */ return true;
        case IMG_SIZE_720K:  *tracks = 80; *heads = 2; *sectors = 9;  strcpy(name, "720K DS/DD");  /* REVIEW: Consider bounds check */ return true;
        case IMG_SIZE_1200K: *tracks = 80; *heads = 2; *sectors = 15; strcpy(name, "1.2M DS/HD");  /* REVIEW: Consider bounds check */ return true;
        case IMG_SIZE_1440K: *tracks = 80; *heads = 2; *sectors = 18; strcpy(name, "1.44M DS/HD");  /* REVIEW: Consider bounds check */ return true;
        case IMG_SIZE_2880K: *tracks = 80; *heads = 2; *sectors = 36; strcpy(name, "2.88M DS/ED");  /* REVIEW: Consider bounds check */ return true;
        default: strcpy(name, "Unknown");  /* REVIEW: Consider bounds check */ return false;
    }
}

static bool img_parse_bpb(const uint8_t* boot, img_bpb_t* bpb) {
    if (!boot || !bpb) return false;
    
    /* Check for valid jump instruction */
    if (boot[0] != 0xEB && boot[0] != 0xE9 && boot[0] != 0x00) {
        return false;
    }
    
    bpb->bytes_per_sector = img_read_le16(boot + 11);
    bpb->sectors_per_cluster = boot[13];
    bpb->reserved_sectors = img_read_le16(boot + 14);
    bpb->fat_count = boot[16];
    bpb->root_entries = img_read_le16(boot + 17);
    bpb->total_sectors_16 = img_read_le16(boot + 19);
    bpb->media_descriptor = boot[21];
    bpb->sectors_per_fat = img_read_le16(boot + 22);
    bpb->sectors_per_track = img_read_le16(boot + 24);
    bpb->heads = img_read_le16(boot + 26);
    bpb->hidden_sectors = img_read_le32(boot + 28);
    bpb->total_sectors_32 = img_read_le32(boot + 32);
    
    /* Validate BPB */
    if (bpb->bytes_per_sector != 512 && bpb->bytes_per_sector != 1024 &&
        bpb->bytes_per_sector != 2048 && bpb->bytes_per_sector != 4096) {
        return false;
    }
    if (bpb->sectors_per_cluster == 0 || bpb->fat_count == 0) {
        return false;
    }
    
    return true;
}

static bool img_parse(const uint8_t* data, size_t size, img_disk_t* disk) {
    if (!data || !disk || size < IMG_SECTOR_SIZE) return false;
    memset(disk, 0, sizeof(img_disk_t));
    disk->diagnosis = img_diagnosis_create();
    disk->source_size = size;
    disk->disk_size = size;
    
    /* Detect geometry from size */
    bool known = img_detect_geometry(size, &disk->tracks, &disk->heads, 
                                      &disk->sectors, disk->format_name);
    
    disk->total_sectors = size / IMG_SECTOR_SIZE;
    
    /* Copy and parse boot sector */
    memcpy(disk->boot_sector, data, 512);
    disk->has_boot_sector = true;
    disk->boot_signature = data[510] == 0x55 && data[511] == 0xAA;
    
    /* OEM name */
    memcpy(disk->oem_name, data + 3, 8);
    disk->oem_name[8] = '\0';
    
    /* Parse BPB */
    disk->bpb_valid = img_parse_bpb(data, &disk->bpb);
    
    if (disk->bpb_valid) {
        /* Calculate FAT locations */
        disk->fat_start = disk->bpb.reserved_sectors;
        disk->root_start = disk->fat_start + (disk->bpb.fat_count * disk->bpb.sectors_per_fat);
        uint32_t root_sectors = (disk->bpb.root_entries * 32 + 511) / 512;
        disk->data_start = disk->root_start + root_sectors;
        
        /* Determine FAT type */
        uint32_t total = disk->bpb.total_sectors_16 ? disk->bpb.total_sectors_16 : disk->bpb.total_sectors_32;
        uint32_t data_sectors = total - disk->data_start;
        uint32_t clusters = data_sectors / disk->bpb.sectors_per_cluster;
        
        if (clusters < 4085) disk->fat_type = 12;
        else if (clusters < 65525) disk->fat_type = 16;
        else disk->fat_type = 32;
    }
    
    disk->score.overall = known ? 1.0f : 0.8f;
    disk->score.valid = true;
    disk->score.bpb_valid = disk->bpb_valid;
    disk->valid = true;
    return true;
}

static void img_disk_free(img_disk_t* disk) {
    if (disk && disk->diagnosis) img_diagnosis_free(disk->diagnosis);
}

#ifdef IMG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== IMG Parser v3 Tests ===\n");
    
    printf("Testing geometry detection... ");
    uint8_t t, h, s; char name[32];
    assert(img_detect_geometry(IMG_SIZE_1440K, &t, &h, &s, name));
    assert(t == 80 && h == 2 && s == 18);
    assert(strcmp(name, "1.44M DS/HD") == 0);
    printf("✓\n");
    
    printf("Testing BPB parsing... ");
    uint8_t boot[512];
    memset(boot, 0, sizeof(boot));
    boot[0] = 0xEB;  /* Jump */
    boot[11] = 0x00; boot[12] = 0x02;  /* 512 bytes/sector */
    boot[13] = 1;    /* 1 sector/cluster */
    boot[14] = 1; boot[15] = 0;  /* 1 reserved */
    boot[16] = 2;    /* 2 FATs */
    boot[17] = 0xE0; boot[18] = 0x00;  /* 224 root entries */
    boot[19] = 0x40; boot[20] = 0x0B;  /* 2880 sectors */
    boot[21] = 0xF0; /* Media descriptor */
    boot[22] = 9; boot[23] = 0;  /* 9 sectors/FAT */
    boot[24] = 18; boot[25] = 0; /* 18 sectors/track */
    boot[26] = 2; boot[27] = 0;  /* 2 heads */
    
    img_bpb_t bpb;
    assert(img_parse_bpb(boot, &bpb));
    assert(bpb.bytes_per_sector == 512);
    assert(bpb.sectors_per_track == 18);
    printf("✓\n");
    
    printf("Testing IMG parsing... ");
    uint8_t* img = calloc(1, IMG_SIZE_1440K);
    memcpy(img, boot, 512);
    img[510] = 0x55; img[511] = 0xAA;
    
    img_disk_t disk;
    bool ok = img_parse(img, IMG_SIZE_1440K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 80);
    assert(disk.bpb_valid);
    assert(disk.fat_type == 12);
    img_disk_free(&disk);
    free(img);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 3, Failed: 0\n");
    return 0;
}
#endif
