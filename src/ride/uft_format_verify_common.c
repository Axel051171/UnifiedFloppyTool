/**
 * @file uft_format_verify_common.c
 * @brief Common Format Verification Functions
 * 
 * Implements verification for common disk image formats:
 * - IMG/IMA (Raw sector images)
 * - D71 (Commodore 1571)
 * - D81 (Commodore 1581)
 * - ST (Atari ST)
 * - MSA (Atari ST compressed)
 * 
 * @license MIT
 */

#include "uft/ride/uft_flux_decoder.h"
#include "uft/uft_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * IMG/IMA RAW SECTOR IMAGE VERIFICATION
 *============================================================================*/

/**
 * @brief Known floppy disk geometries for IMG/IMA validation
 */
typedef struct {
    size_t   size;              /**< Image size in bytes */
    int      cylinders;         /**< Number of cylinders */
    int      heads;             /**< Number of heads (sides) */
    int      sectors;           /**< Sectors per track */
    int      sector_size;       /**< Bytes per sector */
    const char *description;    /**< Format description */
} img_geometry_t;

static const img_geometry_t known_geometries[] = {
    /* PC Formats */
    { 163840,   40, 1,  8, 512, "PC 160KB SS/DD (5.25\")" },
    { 184320,   40, 1,  9, 512, "PC 180KB SS/DD (5.25\")" },
    { 327680,   40, 2,  8, 512, "PC 320KB DS/DD (5.25\")" },
    { 368640,   40, 2,  9, 512, "PC 360KB DS/DD (5.25\")" },
    { 737280,   80, 2,  9, 512, "PC 720KB DS/DD (3.5\")" },
    { 1228800,  80, 2, 15, 512, "PC 1.2MB DS/HD (5.25\")" },
    { 1474560,  80, 2, 18, 512, "PC 1.44MB DS/HD (3.5\")" },
    { 2949120,  80, 2, 36, 512, "PC 2.88MB DS/ED (3.5\")" },
    
    /* Atari ST Formats */
    { 357376,   80, 1,  9, 512, "Atari ST SS/DD (3.5\")" },
    { 368640,   80, 1,  9, 512, "Atari ST SS/DD alt" },
    { 737280,   80, 2,  9, 512, "Atari ST DS/DD (3.5\")" },
    { 819200,   82, 2, 10, 512, "Atari ST DS/DD 10 sect" },
    { 901120,   82, 2, 11, 512, "Atari ST DS/DD 11 sect" },
    
    /* Amiga Formats */
    { 901120,   80, 2, 11, 512, "Amiga DD (880KB)" },
    { 1802240,  80, 2, 22, 512, "Amiga HD (1.76MB)" },
    
    /* Other Formats */
    { 819200,   80, 2, 10, 512, "Generic 800KB" },
    { 409600,   40, 2, 10, 512, "Generic 400KB" },
    
    { 0, 0, 0, 0, 0, NULL }  /* Terminator */
};

/**
 * @brief Find matching geometry for image size
 */
static const img_geometry_t* find_img_geometry(size_t size) {
    for (int i = 0; known_geometries[i].size != 0; i++) {
        if (known_geometries[i].size == size) {
            return &known_geometries[i];
        }
    }
    return NULL;
}

/**
 * @brief Check if file size is a valid floppy geometry
 */
static bool is_valid_img_size(size_t size) {
    /* Check against known sizes */
    if (find_img_geometry(size) != NULL) {
        return true;
    }
    
    /* Check if it's a multiple of common sector sizes */
    if (size % 512 == 0 && size >= 163840 && size <= 2949120) {
        return true;  /* Potentially valid unknown format */
    }
    
    return false;
}

/**
 * @brief Verify IMG/IMA raw sector image
 * 
 * Validates by:
 * - Checking file size matches known geometry
 * - Verifying boot sector structure (optional)
 * - Checking for FAT signature if present
 * 
 * @param path Path to IMG/IMA file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_img(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format_name = "IMG/IMA";
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Cannot open file: %s", path);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Check if size matches known geometry */
    const img_geometry_t *geom = find_img_geometry(file_size);
    
    if (!geom && !is_valid_img_size(file_size)) {
        fclose(f);
        result->valid = false;
        result->error_code = 1;
        snprintf(result->details, sizeof(result->details),
                 "Unknown image size: %ld bytes", file_size);
        return 0;
    }
    
    /* Read boot sector for additional validation */
    uint8_t boot_sector[512];
    if (fread(boot_sector, 1, 512, f) != 512) {
        fclose(f);
        result->valid = false;
        result->error_code = 2;
        snprintf(result->details, sizeof(result->details),
                 "Cannot read boot sector");
        return 0;
    }
    
    /* Check for FAT boot signature (0x55AA at offset 510) */
    bool has_fat_sig = (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA);
    
    /* Check for FAT BPB (BIOS Parameter Block) */
    bool has_bpb = false;
    if (boot_sector[0] == 0xEB || boot_sector[0] == 0xE9) {
        /* JMP instruction - likely bootable */
        uint16_t bytes_per_sector = boot_sector[11] | (boot_sector[12] << 8);
        uint8_t sectors_per_cluster = boot_sector[13];
        
        if (bytes_per_sector == 512 && sectors_per_cluster > 0 && 
            sectors_per_cluster <= 64) {
            has_bpb = true;
        }
    }
    
    fclose(f);
    
    /* Determine result */
    result->valid = true;
    result->error_code = 0;
    
    if (geom) {
        snprintf(result->details, sizeof(result->details),
                 "%s, %d cyl × %d heads × %d sect, FAT:%s BPB:%s",
                 geom->description,
                 geom->cylinders, geom->heads, geom->sectors,
                 has_fat_sig ? "yes" : "no",
                 has_bpb ? "yes" : "no");
    } else {
        int total_sectors = file_size / 512;
        snprintf(result->details, sizeof(result->details),
                 "Unknown geometry, %ld bytes (%d sectors), FAT:%s",
                 file_size, total_sectors,
                 has_fat_sig ? "yes" : "no");
    }
    
    return 0;
}

/*============================================================================
 * COMMODORE D71 (1571) VERIFICATION
 *============================================================================*/

#define D71_STANDARD_SIZE   349696  /* 35 tracks × 2 sides */
#define D71_EXTENDED_SIZE   699392  /* 70 tracks × 2 sides */

#define D71_BAM_TRACK       18
#define D71_BAM_SECTOR      0
#define D71_DIR_TRACK       18
#define D71_DIR_SECTOR      1

/* D71 sector counts per track (same as D64 per side) */
static const int d71_sectors_per_track[] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24 */
    18, 18, 18, 18, 18, 18,                                              /* 25-30 */
    17, 17, 17, 17, 17                                                   /* 31-35 */
};

/**
 * @brief Calculate track offset in D71 image
 */
static long d71_track_offset(int track, int sector, int side) {
    if (track < 1 || track > 35 || sector < 0) return -1;
    
    long offset = 0;
    
    /* Add offset for side 1 */
    if (side == 1) {
        for (int t = 0; t < 35; t++) {
            offset += d71_sectors_per_track[t] * 256;
        }
    }
    
    /* Add offset for tracks */
    for (int t = 0; t < track - 1; t++) {
        offset += d71_sectors_per_track[t] * 256;
    }
    
    /* Add sector offset */
    if (sector >= d71_sectors_per_track[track - 1]) return -1;
    offset += sector * 256;
    
    return offset;
}

/**
 * @brief Verify Commodore D71 disk image
 * 
 * @param path Path to D71 file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_d71(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format_name = "D71";
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Cannot open file: %s", path);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Check size */
    if (file_size != D71_STANDARD_SIZE && file_size != D71_EXTENDED_SIZE) {
        fclose(f);
        result->valid = false;
        result->error_code = 1;
        snprintf(result->details, sizeof(result->details),
                 "Invalid D71 size: %ld (expected %d or %d)",
                 file_size, D71_STANDARD_SIZE, D71_EXTENDED_SIZE);
        return 0;
    }
    
    /* Read BAM sector */
    long bam_offset = d71_track_offset(D71_BAM_TRACK, D71_BAM_SECTOR, 0);
    if (bam_offset < 0) {
        fclose(f);
        result->valid = false;
        result->error_code = 2;
        snprintf(result->details, sizeof(result->details),
                 "Invalid BAM offset");
        return 0;
    }
    
    fseek(f, bam_offset, SEEK_SET);
    uint8_t bam[256];
    if (fread(bam, 1, 256, f) != 256) {
        fclose(f);
        result->valid = false;
        result->error_code = 3;
        snprintf(result->details, sizeof(result->details),
                 "Cannot read BAM sector");
        return 0;
    }
    
    /* Validate BAM */
    uint8_t dir_track = bam[0];
    uint8_t dir_sector = bam[1];
    uint8_t dos_version = bam[2];
    uint8_t double_sided = bam[3];
    
    /* D71 uses DOS version 0x41 ('A') for 1571 */
    bool valid_dos = (dos_version == 0x41 || dos_version == 0x44);
    bool valid_dir = (dir_track == D71_DIR_TRACK && dir_sector == D71_DIR_SECTOR);
    
    /* Read disk name (offset 0x90) */
    char disk_name[17];
    memcpy(disk_name, &bam[0x90], 16);
    disk_name[16] = '\0';
    
    /* Convert PETSCII to ASCII (basic) */
    for (int i = 0; i < 16; i++) {
        unsigned char c = (unsigned char)disk_name[i];
        if (c == 0xA0) disk_name[i] = ' ';  /* Shifted space */
        else if (c >= 0xC1 && c <= 0xDA) {
            disk_name[i] = (char)(c - 0xC1 + 'A');  /* Uppercase */
        }
        else if (c < 32 || c > 126) {
            disk_name[i] = '?';
        }
    }
    
    fclose(f);
    
    /* Determine validity */
    result->valid = valid_dos && valid_dir;
    result->error_code = result->valid ? 0 : 4;
    
    snprintf(result->details, sizeof(result->details),
             "D71 %s: \"%s\", DOS:%c, %s, %s",
             file_size == D71_EXTENDED_SIZE ? "70trk" : "35trk",
             disk_name,
             dos_version,
             double_sided ? "DS" : "SS",
             result->valid ? "OK" : "BAM invalid");
    
    return 0;
}

/*============================================================================
 * COMMODORE D81 (1581) VERIFICATION
 *============================================================================*/

#define D81_SIZE            819200  /* 80 tracks × 2 sides × 10 sect × 512 bytes */
#define D81_TRACK_SIZE      5120    /* 10 sectors × 512 bytes */
#define D81_HEADER_TRACK    40
#define D81_HEADER_SECTOR   0
#define D81_BAM_TRACK       40
#define D81_BAM_SECTOR      1

/**
 * @brief Verify Commodore D81 disk image
 * 
 * @param path Path to D81 file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_d81(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format_name = "D81";
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Cannot open file: %s", path);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Check size */
    if (file_size != D81_SIZE) {
        fclose(f);
        result->valid = false;
        result->error_code = 1;
        snprintf(result->details, sizeof(result->details),
                 "Invalid D81 size: %ld (expected %d)",
                 file_size, D81_SIZE);
        return 0;
    }
    
    /* Read header sector (track 40, sector 0) */
    long header_offset = (D81_HEADER_TRACK - 1) * D81_TRACK_SIZE * 2 + 
                         D81_HEADER_SECTOR * 512;
    fseek(f, header_offset, SEEK_SET);
    
    uint8_t header[512];
    if (fread(header, 1, 512, f) != 512) {
        fclose(f);
        result->valid = false;
        result->error_code = 2;
        snprintf(result->details, sizeof(result->details),
                 "Cannot read header sector");
        return 0;
    }
    
    /* Validate header */
    uint8_t dir_track = header[0];
    uint8_t dir_sector = header[1];
    uint8_t dos_version = header[2];
    
    /* D81 uses DOS version 0x44 ('D') */
    bool valid_dos = (dos_version == 0x44);
    bool valid_dir = (dir_track == 40 && dir_sector == 3);
    
    /* Read disk name (offset 0x04) */
    char disk_name[17];
    memcpy(disk_name, &header[0x04], 16);
    disk_name[16] = '\0';
    
    /* Convert PETSCII to ASCII */
    for (int i = 0; i < 16; i++) {
        unsigned char c = (unsigned char)disk_name[i];
        if (c == 0xA0) disk_name[i] = ' ';
        else if (c >= 0xC1 && c <= 0xDA) {
            disk_name[i] = (char)(c - 0xC1 + 'A');
        }
        else if (c < 32 || c > 126) {
            disk_name[i] = '?';
        }
    }
    
    /* Read BAM for additional validation */
    long bam_offset = (D81_BAM_TRACK - 1) * D81_TRACK_SIZE * 2 + 
                      D81_BAM_SECTOR * 512;
    fseek(f, bam_offset, SEEK_SET);
    
    uint8_t bam[512];
    if (fread(bam, 1, 512, f) != 512) {
        fclose(f);
        result->valid = false;
        result->error_code = 3;
        snprintf(result->details, sizeof(result->details),
                 "Cannot read BAM sector");
        return 0;
    }
    
    /* BAM signature check */
    bool valid_bam = (bam[0] == 40 && bam[1] == 2);  /* Points to BAM 2 */
    
    fclose(f);
    
    /* Determine validity */
    result->valid = valid_dos && valid_dir && valid_bam;
    result->error_code = result->valid ? 0 : 4;
    
    snprintf(result->details, sizeof(result->details),
             "D81 800KB: \"%s\", DOS:%c, %s",
             disk_name,
             dos_version,
             result->valid ? "OK" : "Structure invalid");
    
    return 0;
}

/*============================================================================
 * ATARI ST FORMAT VERIFICATION
 *============================================================================*/

/* Atari ST boot sector structure */
typedef struct __attribute__((packed)) {
    uint8_t  branch[2];         /* BRA.S to boot code or 0x0000 */
    uint8_t  oem[6];            /* OEM name */
    uint8_t  serial[3];         /* Serial number */
    uint16_t bytes_per_sector;  /* Usually 512 */
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;         /* Usually 2 */
    uint16_t root_entries;
    uint16_t total_sectors;     /* If 0, use 32-bit value */
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint16_t hidden_sectors;
    /* Extended fields for larger disks */
} st_boot_sector_t;

/**
 * @brief Known Atari ST disk sizes
 */
static const struct {
    size_t size;
    int tracks;
    int sides;
    int sectors;
    const char *desc;
} st_sizes[] = {
    { 357376,  80, 1,  9, "ST SS/DD 360KB" },
    { 368640,  80, 1,  9, "ST SS/DD 360KB alt" },
    { 399360,  80, 1, 10, "ST SS/DD 390KB" },
    { 737280,  80, 2,  9, "ST DS/DD 720KB" },
    { 798720,  80, 2, 10, "ST DS/DD 780KB" },
    { 819200,  82, 2, 10, "ST DS/DD 800KB" },
    { 901120,  82, 2, 11, "ST DS/DD 880KB" },
    { 0, 0, 0, 0, NULL }
};

/**
 * @brief Verify Atari ST disk image
 * 
 * @param path Path to ST file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_st(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format_name = "ST";
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Cannot open file: %s", path);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Find matching ST size */
    const char *size_desc = NULL;
    for (int i = 0; st_sizes[i].size != 0; i++) {
        if ((size_t)file_size == st_sizes[i].size) {
            size_desc = st_sizes[i].desc;
            break;
        }
    }
    
    if (!size_desc && file_size % 512 != 0) {
        fclose(f);
        result->valid = false;
        result->error_code = 1;
        snprintf(result->details, sizeof(result->details),
                 "Invalid ST size: %ld bytes", file_size);
        return 0;
    }
    
    /* Read boot sector */
    uint8_t boot[512];
    if (fread(boot, 1, 512, f) != 512) {
        fclose(f);
        result->valid = false;
        result->error_code = 2;
        snprintf(result->details, sizeof(result->details),
                 "Cannot read boot sector");
        return 0;
    }
    
    fclose(f);
    
    /* Parse boot sector */
    st_boot_sector_t *bpb = (st_boot_sector_t*)boot;
    
    uint16_t bytes_per_sector = bpb->bytes_per_sector;
    uint16_t sectors_per_track = bpb->sectors_per_track;
    uint16_t heads = bpb->heads;
    uint16_t total_sectors = bpb->total_sectors;
    
    /* Validate BPB */
    bool valid_bpb = false;
    if (bytes_per_sector == 512 && 
        sectors_per_track >= 9 && sectors_per_track <= 11 &&
        heads >= 1 && heads <= 2) {
        /* Calculate expected size */
        size_t expected_size = total_sectors * 512;
        if (expected_size == (size_t)file_size || total_sectors == 0) {
            valid_bpb = true;
        }
    }
    
    /* Check for executable boot sector */
    bool bootable = (boot[0] == 0x60 && boot[1] != 0x00);  /* BRA.S instruction */
    
    /* Calculate boot sector checksum */
    uint16_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        checksum += (boot[i*2] << 8) | boot[i*2+1];
    }
    bool valid_checksum = (checksum == 0x1234);
    
    /* Determine validity */
    result->valid = (size_desc != NULL || (file_size % 512 == 0 && valid_bpb));
    result->error_code = result->valid ? 0 : 3;
    
    if (size_desc) {
        snprintf(result->details, sizeof(result->details),
                 "%s, %d spt × %d heads, boot:%s, chksum:%s",
                 size_desc,
                 sectors_per_track, heads,
                 bootable ? "yes" : "no",
                 valid_checksum ? "valid" : "invalid");
    } else {
        snprintf(result->details, sizeof(result->details),
                 "ST %ld bytes, %d spt × %d heads, BPB:%s",
                 file_size,
                 sectors_per_track, heads,
                 valid_bpb ? "valid" : "invalid");
    }
    
    return 0;
}

/*============================================================================
 * MSA (MAGIC SHADOW ARCHIVER) VERIFICATION
 *============================================================================*/

#define MSA_MAGIC           0x0E0F

typedef struct __attribute__((packed)) {
    uint16_t magic;             /* 0x0E0F */
    uint16_t sectors_per_track;
    uint16_t sides;             /* 0 = SS, 1 = DS */
    uint16_t start_track;
    uint16_t end_track;
} msa_header_t;

/**
 * @brief Verify Atari ST MSA compressed image
 * 
 * @param path Path to MSA file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_msa(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format_name = "MSA";
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Cannot open file: %s", path);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Check minimum size */
    if (file_size < (long)sizeof(msa_header_t)) {
        fclose(f);
        result->valid = false;
        result->error_code = 1;
        snprintf(result->details, sizeof(result->details),
                 "File too small for MSA: %ld bytes", file_size);
        return 0;
    }
    
    /* Read header */
    msa_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        result->valid = false;
        result->error_code = 2;
        snprintf(result->details, sizeof(result->details),
                 "Cannot read MSA header");
        return 0;
    }
    
    fclose(f);
    
    /* MSA uses big-endian byte order */
    uint16_t magic = (header.magic >> 8) | (header.magic << 8);
    uint16_t spt = (header.sectors_per_track >> 8) | (header.sectors_per_track << 8);
    uint16_t sides = (header.sides >> 8) | (header.sides << 8);
    uint16_t start_track = (header.start_track >> 8) | (header.start_track << 8);
    uint16_t end_track = (header.end_track >> 8) | (header.end_track << 8);
    
    /* Validate magic */
    if (magic != MSA_MAGIC) {
        result->valid = false;
        result->error_code = 3;
        snprintf(result->details, sizeof(result->details),
                 "Invalid MSA magic: 0x%04X (expected 0x%04X)",
                 magic, MSA_MAGIC);
        return 0;
    }
    
    /* Validate parameters */
    bool valid_params = (spt >= 9 && spt <= 11 &&
                         sides <= 1 &&
                         start_track <= end_track &&
                         end_track <= 85);
    
    if (!valid_params) {
        result->valid = false;
        result->error_code = 4;
        snprintf(result->details, sizeof(result->details),
                 "Invalid MSA params: spt=%d sides=%d tracks=%d-%d",
                 spt, sides + 1, start_track, end_track);
        return 0;
    }
    
    /* Calculate uncompressed size */
    int tracks = end_track - start_track + 1;
    int total_sides = sides + 1;
    size_t uncompressed_size = tracks * total_sides * spt * 512;
    
    result->valid = true;
    result->error_code = 0;
    snprintf(result->details, sizeof(result->details),
             "MSA OK: %d spt, %s, tracks %d-%d, uncompressed %zu KB",
             spt,
             sides ? "DS" : "SS",
             start_track, end_track,
             uncompressed_size / 1024);
    
    return 0;
}
