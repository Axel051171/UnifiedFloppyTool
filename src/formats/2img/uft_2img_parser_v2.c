/**
 * @file uft_2img_parser_v2.c
 * @brief GOD MODE 2IMG Parser v2 - Apple II Universal Disk Image
 * 
 * 2IMG is the universal Apple II disk image format.
 * Supports multiple data formats:
 * - DOS 3.3 order (DO)
 * - ProDOS order (PO)
 * - Raw nibbles (NIB)
 * 
 * Features:
 * - 64-byte header with metadata
 * - Creator signature
 * - Optional comment
 * - Write-protect flag
 * - Volume number
 * - Multiple image formats
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include "uft/floppy/uft_floppy_device.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define IMG2_SIGNATURE          "2IMG"
#define IMG2_HEADER_SIZE        64

#define IMG2_SECTOR_SIZE        512     /* ProDOS uses 512-byte blocks */
#define IMG2_DOS_SECTOR_SIZE    256     /* DOS 3.3 uses 256-byte sectors */

#define IMG2_TRACKS             35
#define IMG2_SECTORS_16         16
#define IMG2_SECTORS_13         13

/* Standard disk sizes */
#define IMG2_SIZE_140K          143360  /* 35 tracks * 16 sectors * 256 bytes */
#define IMG2_SIZE_800K          819200  /* 3.5" 800K */
#define IMG2_SIZE_NIB           232960  /* 35 tracks * 6656 bytes */

/* Image format types */
#define IMG2_FORMAT_DOS         0       /* DOS 3.3 sector order */
#define IMG2_FORMAT_PRODOS      1       /* ProDOS block order */
#define IMG2_FORMAT_NIB         2       /* Raw nibbles */

/* Flags */
#define IMG2_FLAG_LOCKED        0x80000000
#define IMG2_FLAG_VOLUME_VALID  0x00000100

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 2IMG header (64 bytes)
 */
typedef struct {
    char signature[4];        /* "2IMG" */
    char creator[4];          /* Creator signature */
    uint16_t header_size;     /* Header size (64) */
    uint16_t version;         /* Version (1) */
    uint32_t format;          /* Image format */
    uint32_t flags;           /* Flags */
    uint32_t prodos_blocks;   /* ProDOS blocks (if applicable) */
    uint32_t data_offset;     /* Offset to disk data */
    uint32_t data_size;       /* Size of disk data */
    uint32_t comment_offset;  /* Offset to comment (0 if none) */
    uint32_t comment_size;    /* Size of comment */
    uint32_t creator_offset;  /* Offset to creator data */
    uint32_t creator_size;    /* Size of creator data */
    uint8_t reserved[16];     /* Reserved */
} img2_header_t;
UFT_PACK_END

/**
 * @brief Image format types
 */
typedef enum {
    IMG2_TYPE_DOS = 0,
    IMG2_TYPE_PRODOS = 1,
    IMG2_TYPE_NIB = 2,
    IMG2_TYPE_UNKNOWN = 255
} img2_format_t;

/**
 * @brief 2IMG disk structure
 */
typedef struct {
    img2_header_t header;
    
    /* Parsed info */
    img2_format_t format;
    char creator_sig[5];
    char* comment;
    
    /* Disk info */
    uint8_t volume;
    bool write_protected;
    uint32_t data_size;
    uint32_t num_blocks;
    uint32_t num_tracks;
    uint32_t sectors_per_track;
    
    /* Raw data reference */
    const uint8_t* disk_data;
    
    /* Status */
    bool valid;
    char error[256];
} img2_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR INTERLEAVE TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/* DOS 3.3 to ProDOS sector translation */
static const uint8_t dos_to_prodos[16] = {
    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
};

/* ProDOS to DOS 3.3 sector translation */
static const uint8_t prodos_to_dos[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

/* Physical to DOS 3.3 logical */
static const uint8_t physical_to_dos[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static uint32_t read_le32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool img2_is_valid(const uint8_t* data, size_t size) {
    if (size < IMG2_HEADER_SIZE) return false;
    return memcmp(data, IMG2_SIGNATURE, 4) == 0;
}

static const char* img2_format_name(img2_format_t format) {
    switch (format) {
        case IMG2_TYPE_DOS: return "DOS 3.3 Order";
        case IMG2_TYPE_PRODOS: return "ProDOS Order";
        case IMG2_TYPE_NIB: return "Raw Nibbles";
        default: return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool img2_parse_header(const uint8_t* data, img2_header_t* header) {
    memcpy(header->signature, data, 4);
    memcpy(header->creator, data + 4, 4);
    header->header_size = read_le16(data + 8);
    header->version = read_le16(data + 10);
    header->format = read_le32(data + 12);
    header->flags = read_le32(data + 16);
    header->prodos_blocks = read_le32(data + 20);
    header->data_offset = read_le32(data + 24);
    header->data_size = read_le32(data + 28);
    header->comment_offset = read_le32(data + 32);
    header->comment_size = read_le32(data + 36);
    header->creator_offset = read_le32(data + 40);
    header->creator_size = read_le32(data + 44);
    
    return true;
}

static bool img2_parse(const uint8_t* data, size_t size, img2_disk_t* disk) {
    memset(disk, 0, sizeof(img2_disk_t));
    
    if (!img2_is_valid(data, size)) {
        snprintf(disk->error, sizeof(disk->error), "Invalid 2IMG signature");
        return false;
    }
    
    img2_parse_header(data, &disk->header);
    
    /* Validate header */
    if (disk->header.header_size < 52) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Invalid header size: %u", disk->header.header_size);
        return false;
    }
    
    /* Extract creator signature */
    memcpy(disk->creator_sig, disk->header.creator, 4);
    disk->creator_sig[4] = '\0';
    
    /* Parse format */
    switch (disk->header.format) {
        case IMG2_FORMAT_DOS:
            disk->format = IMG2_TYPE_DOS;
            break;
        case IMG2_FORMAT_PRODOS:
            disk->format = IMG2_TYPE_PRODOS;
            break;
        case IMG2_FORMAT_NIB:
            disk->format = IMG2_TYPE_NIB;
            break;
        default:
            disk->format = IMG2_TYPE_UNKNOWN;
            break;
    }
    
    /* Parse flags */
    disk->write_protected = (disk->header.flags & IMG2_FLAG_LOCKED) != 0;
    
    if (disk->header.flags & IMG2_FLAG_VOLUME_VALID) {
        disk->volume = disk->header.flags & 0xFF;
    } else {
        disk->volume = 254;  /* Default */
    }
    
    /* Validate data section */
    disk->data_size = disk->header.data_size;
    if (disk->data_size == 0) {
        /* Calculate from file size */
        disk->data_size = size - disk->header.data_offset;
    }
    
    if (disk->header.data_offset + disk->data_size > size) {
        snprintf(disk->error, sizeof(disk->error), "Data section exceeds file size");
        return false;
    }
    
    disk->disk_data = data + disk->header.data_offset;
    
    /* Determine geometry */
    if (disk->format == IMG2_TYPE_NIB) {
        disk->num_tracks = disk->data_size / 6656;
        disk->sectors_per_track = 16;
    } else {
        disk->num_blocks = disk->header.prodos_blocks;
        if (disk->num_blocks == 0) {
            disk->num_blocks = disk->data_size / 512;
        }
        disk->num_tracks = 35;
        disk->sectors_per_track = 16;
    }
    
    /* Parse comment if present */
    if (disk->header.comment_offset > 0 && disk->header.comment_size > 0) {
        if (disk->header.comment_offset + disk->header.comment_size <= size) {
            disk->comment = malloc(disk->header.comment_size + 1);
            if (disk->comment) {
                memcpy(disk->comment, data + disk->header.comment_offset,
                       disk->header.comment_size);
                disk->comment[disk->header.comment_size] = '\0';
            }
        }
    }
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR ACCESS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read sector from image
 */
static const uint8_t* img2_read_sector(const img2_disk_t* disk, 
                                        uint8_t track, uint8_t sector) {
    if (!disk->valid || !disk->disk_data) return NULL;
    if (track >= disk->num_tracks || sector >= disk->sectors_per_track) return NULL;
    
    size_t offset;
    
    if (disk->format == IMG2_TYPE_DOS) {
        /* DOS 3.3 order */
        offset = (track * 16 + sector) * 256;
    } else if (disk->format == IMG2_TYPE_PRODOS) {
        /* ProDOS order - convert to DOS order */
        uint8_t dos_sector = prodos_to_dos[sector];
        offset = (track * 16 + dos_sector) * 256;
    } else {
        return NULL;  /* NIB requires different handling */
    }
    
    if (offset + 256 > disk->data_size) return NULL;
    
    return disk->disk_data + offset;
}

/**
 * @brief Read ProDOS block from image
 */
static const uint8_t* img2_read_block(const img2_disk_t* disk, uint16_t block) {
    if (!disk->valid || !disk->disk_data) return NULL;
    if (block >= disk->num_blocks) return NULL;
    
    size_t offset = block * 512;
    if (offset + 512 > disk->data_size) return NULL;
    
    return disk->disk_data + offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert DOS order to ProDOS order
 */
static uint8_t* img2_dos_to_prodos(const uint8_t* dos_data, size_t size) {
    if (size != IMG2_SIZE_140K) return NULL;
    
    uint8_t* prodos_data = malloc(size);
    if (!prodos_data) return NULL;
    
    for (int track = 0; track < 35; track++) {
        for (int sector = 0; sector < 16; sector++) {
            size_t dos_offset = (track * 16 + sector) * 256;
            uint8_t prodos_sector = dos_to_prodos[sector];
            size_t prodos_offset = (track * 16 + prodos_sector) * 256;
            
            memcpy(prodos_data + prodos_offset, dos_data + dos_offset, 256);
        }
    }
    
    return prodos_data;
}

/**
 * @brief Create 2IMG from raw disk data
 */
static uint8_t* img2_create(const uint8_t* disk_data, size_t disk_size,
                            img2_format_t format, const char* creator,
                            const char* comment, size_t* out_size) {
    size_t comment_len = comment ? strlen(comment) : 0;
    size_t total_size = IMG2_HEADER_SIZE + disk_size + comment_len;
    
    uint8_t* data = calloc(1, total_size);
    if (!data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Build header */
    memcpy(data, IMG2_SIGNATURE, 4);
    
    if (creator && strlen(creator) >= 4) {
        memcpy(data + 4, creator, 4);
    } else {
        memcpy(data + 4, "UFT!", 4);
    }
    
    /* Header size */
    data[8] = IMG2_HEADER_SIZE;
    data[9] = 0;
    
    /* Version */
    data[10] = 1;
    data[11] = 0;
    
    /* Format */
    data[12] = format;
    data[13] = 0;
    data[14] = 0;
    data[15] = 0;
    
    /* Flags (volume 254) */
    data[16] = 254;
    data[17] = 1;  /* Volume valid flag */
    data[18] = 0;
    data[19] = 0;
    
    /* ProDOS blocks */
    uint32_t blocks = disk_size / 512;
    data[20] = blocks & 0xFF;
    data[21] = (blocks >> 8) & 0xFF;
    data[22] = (blocks >> 16) & 0xFF;
    data[23] = (blocks >> 24) & 0xFF;
    
    /* Data offset */
    data[24] = IMG2_HEADER_SIZE;
    data[25] = 0;
    data[26] = 0;
    data[27] = 0;
    
    /* Data size */
    data[28] = disk_size & 0xFF;
    data[29] = (disk_size >> 8) & 0xFF;
    data[30] = (disk_size >> 16) & 0xFF;
    data[31] = (disk_size >> 24) & 0xFF;
    
    /* Comment */
    if (comment_len > 0) {
        uint32_t comment_offset = IMG2_HEADER_SIZE + disk_size;
        data[32] = comment_offset & 0xFF;
        data[33] = (comment_offset >> 8) & 0xFF;
        data[34] = (comment_offset >> 16) & 0xFF;
        data[35] = (comment_offset >> 24) & 0xFF;
        
        data[36] = comment_len & 0xFF;
        data[37] = (comment_len >> 8) & 0xFF;
        data[38] = (comment_len >> 16) & 0xFF;
        data[39] = (comment_len >> 24) & 0xFF;
        
        memcpy(data + comment_offset, comment, comment_len);
    }
    
    /* Copy disk data */
    memcpy(data + IMG2_HEADER_SIZE, disk_data, disk_size);
    
    *out_size = total_size;
    return data;
}

static void img2_free(img2_disk_t* disk) {
    if (disk && disk->comment) {
        free(disk->comment);
        disk->comment = NULL;
    }
}

static char* img2_info_to_text(const img2_disk_t* disk) {
    size_t buf_size = 2048;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    snprintf(buf, buf_size,
        "2IMG Disk Image\n"
        "═══════════════\n"
        "Creator: %s\n"
        "Version: %u\n"
        "Format: %s\n"
        "Volume: %u\n"
        "Write Protected: %s\n"
        "Data Size: %u bytes\n"
        "ProDOS Blocks: %u\n"
        "Tracks: %u\n"
        "%s%s%s",
        disk->creator_sig,
        disk->header.version,
        img2_format_name(disk->format),
        disk->volume,
        disk->write_protected ? "Yes" : "No",
        disk->data_size,
        disk->num_blocks,
        disk->num_tracks,
        disk->comment ? "Comment: " : "",
        disk->comment ? disk->comment : "",
        disk->comment ? "\n" : ""
    );
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef IMG2_PARSER_TEST

#include <assert.h>
#include "uft/uft_compiler.h"

int main(void) {
    printf("=== 2IMG Parser v2 Tests ===\n");
    
    printf("Testing signature check... ");
    uint8_t valid_sig[64] = { '2', 'I', 'M', 'G', 'U', 'F', 'T', '!' };
    uint8_t invalid_sig[64] = { 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X' };
    assert(img2_is_valid(valid_sig, sizeof(valid_sig)));
    assert(!img2_is_valid(invalid_sig, sizeof(invalid_sig)));
    printf("✓\n");
    
    printf("Testing format names... ");
    assert(strcmp(img2_format_name(IMG2_TYPE_DOS), "DOS 3.3 Order") == 0);
    assert(strcmp(img2_format_name(IMG2_TYPE_PRODOS), "ProDOS Order") == 0);
    assert(strcmp(img2_format_name(IMG2_TYPE_NIB), "Raw Nibbles") == 0);
    printf("✓\n");
    
    printf("Testing sector interleave... ");
    /* DOS sector 0 -> ProDOS sector 0 */
    assert(dos_to_prodos[0] == 0);
    /* ProDOS sector 0 -> DOS sector 0 */
    assert(prodos_to_dos[0] == 0);
    /* Round trip */
    for (int i = 0; i < 16; i++) {
        assert(prodos_to_dos[dos_to_prodos[i]] == i);
    }
    printf("✓\n");
    
    printf("Testing image creation... ");
    uint8_t disk_data[143360];
    memset(disk_data, 0xE5, sizeof(disk_data));
    
    size_t img_size = 0;
    uint8_t* img = img2_create(disk_data, sizeof(disk_data), 
                               IMG2_TYPE_DOS, "TEST", "Test disk", &img_size);
    assert(img != NULL);
    assert(img_size > IMG2_HEADER_SIZE);
    
    img2_disk_t disk;
    assert(img2_parse(img, img_size, &disk));
    assert(disk.valid);
    assert(disk.format == IMG2_TYPE_DOS);
    assert(strcmp(disk.creator_sig, "TEST") == 0);
    
    img2_free(&disk);
    free(img);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* IMG2_PARSER_TEST */
