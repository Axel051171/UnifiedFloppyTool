/**
 * @file uft_fat_bootsector.h
 * @brief FAT Boot Sector Analysis Module
 * @version 4.1.6
 * 
 * Comprehensive FAT boot sector analysis for floppy disk images.
 * Based on OpenGate.at article and MS-DOS specifications.
 * 
 * Features:
 * - BPB (BIOS Parameter Block) parsing and validation
 * - Media Descriptor Byte identification
 * - Boot signature verification (0x55AA)
 * - Extended BPB (EBPB) support
 * - Disk geometry calculation
 * - Format identification (FAT12/FAT16/FAT32)
 * - OEM name extraction
 * - Volume label and serial number
 * 
 * References:
 * - https://www.opengate.at/blog/2024/01/bootsector/
 * - Microsoft FAT Specification (FATGEN103.DOC)
 * - ECMA-107 (Volume and File Structure of Disk Cartridges)
 */

#ifndef UFT_FAT_BOOTSECTOR_H
#define UFT_FAT_BOOTSECTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Boot Sector Constants
 * ============================================================================ */

#define FAT_SECTOR_SIZE             512
#define FAT_BOOT_SIGNATURE          0xAA55  /* Little-endian: 0x55, 0xAA */
#define FAT_BOOT_SIGNATURE_OFFSET   510     /* Offset 0x1FE */
#define FAT_EXTENDED_BPB_MARKER     0x29    /* Extended BPB signature */
#define FAT_EXTENDED_BPB_MARKER_OLD 0x28    /* Older extended BPB */

/* ============================================================================
 * Media Descriptor Bytes (from OpenGate article)
 * Byte at offset 0x15 in BPB and first byte of FAT
 * ============================================================================ */

typedef enum {
    /* Standard media descriptors */
    FAT_MEDIA_FIXED_DISK        = 0xF8,  /* Hard disk */
    FAT_MEDIA_35_1440K          = 0xF0,  /* 3.5" 1.44MB or 2.88MB */
    FAT_MEDIA_35_720K           = 0xF9,  /* 3.5" 720KB or 5.25" 1.2MB */
    FAT_MEDIA_525_180K          = 0xFC,  /* 5.25" 180KB single-sided */
    FAT_MEDIA_525_360K          = 0xFD,  /* 5.25" 360KB or 8" 500KB */
    FAT_MEDIA_525_160K          = 0xFE,  /* 5.25" 160KB or 8" 250KB/1.2MB */
    FAT_MEDIA_525_320K          = 0xFF,  /* 5.25" 320KB double-sided */
    
    /* Extended/alternate descriptors */
    FAT_MEDIA_35_2880K          = 0xF0,  /* 3.5" 2.88MB (ED) - same as 1.44M */
    FAT_MEDIA_525_1200K         = 0xF9,  /* 5.25" 1.2MB HD - same as 720K */
    FAT_MEDIA_8_1200K           = 0xFE,  /* 8" 1.2MB - same as 160K */
    FAT_MEDIA_8_500K            = 0xFD,  /* 8" 500KB - same as 360K */
    FAT_MEDIA_8_250K            = 0xFE,  /* 8" 250KB - same as 160K */
    
    /* RAM disk and other */
    FAT_MEDIA_RAMDISK           = 0xFA,  /* RAM disk */
    FAT_MEDIA_SUPERFLOPPY       = 0xF8,  /* SuperFloppy (no partition) */
} fat_media_descriptor_t;

/* ============================================================================
 * Disk Geometry Structures
 * ============================================================================ */

/**
 * @brief Standard floppy disk geometry
 */
typedef struct {
    const char *name;           /* Format name */
    uint8_t  media_byte;        /* Media descriptor byte */
    uint16_t total_sectors;     /* Total sectors on disk */
    uint16_t bytes_per_sector;  /* Bytes per sector (usually 512) */
    uint8_t  sectors_per_cluster;/* Sectors per cluster */
    uint16_t reserved_sectors;  /* Reserved sectors (usually 1) */
    uint8_t  fat_count;         /* Number of FATs (usually 2) */
    uint16_t root_entries;      /* Root directory entries */
    uint16_t sectors_per_fat;   /* Sectors per FAT */
    uint16_t sectors_per_track; /* Sectors per track */
    uint16_t heads;             /* Number of heads */
    uint8_t  tracks;            /* Number of tracks/cylinders */
} fat_disk_geometry_t;

/* Standard disk geometry definitions */
extern const fat_disk_geometry_t fat_geometry_160k;   /* 5.25" SS/DD 40T 8S */
extern const fat_disk_geometry_t fat_geometry_180k;   /* 5.25" SS/DD 40T 9S */
extern const fat_disk_geometry_t fat_geometry_320k;   /* 5.25" DS/DD 40T 8S */
extern const fat_disk_geometry_t fat_geometry_360k;   /* 5.25" DS/DD 40T 9S */
extern const fat_disk_geometry_t fat_geometry_720k;   /* 3.5" DS/DD 80T 9S */
extern const fat_disk_geometry_t fat_geometry_1200k;  /* 5.25" DS/HD 80T 15S */
extern const fat_disk_geometry_t fat_geometry_1440k;  /* 3.5" DS/HD 80T 18S */
extern const fat_disk_geometry_t fat_geometry_2880k;  /* 3.5" DS/ED 80T 36S */

/* ============================================================================
 * BPB (BIOS Parameter Block) Structure
 * ============================================================================ */

/**
 * @brief BPB structure (offsets 0x00-0x3D in boot sector)
 * 
 * Layout from OpenGate article:
 * 0x00-0x02: Jump instruction (JMP SHORT + NOP)
 * 0x03-0x0A: OEM name (8 bytes)
 * 0x0B-0x0C: Bytes per sector
 * 0x0D:      Sectors per cluster
 * 0x0E-0x0F: Reserved sectors
 * 0x10:      Number of FATs
 * 0x11-0x12: Root directory entries
 * 0x13-0x14: Total sectors (16-bit)
 * 0x15:      Media descriptor byte
 * 0x16-0x17: Sectors per FAT
 * 0x18-0x19: Sectors per track
 * 0x1A-0x1B: Number of heads
 * 0x1C-0x1F: Hidden sectors (32-bit)
 * 0x20-0x23: Total sectors (32-bit, if 16-bit is 0)
 */
typedef struct __attribute__((packed)) {
    /* Jump instruction (3 bytes) */
    uint8_t  jmp_boot[3];           /* 0x00: JMP SHORT xx, NOP or JMP NEAR */
    
    /* OEM name (8 bytes) */
    char     oem_name[8];           /* 0x03: OEM name, e.g., "MSDOS5.0" */
    
    /* DOS 2.0 BPB (13 bytes) */
    uint16_t bytes_per_sector;      /* 0x0B: Usually 512 */
    uint8_t  sectors_per_cluster;   /* 0x0D: 1, 2, 4, 8, 16, 32, 64, 128 */
    uint16_t reserved_sectors;      /* 0x0E: Usually 1 for FAT12/16 */
    uint8_t  fat_count;             /* 0x10: Usually 2 */
    uint16_t root_entry_count;      /* 0x11: 224 for 1.44MB, 0 for FAT32 */
    uint16_t total_sectors_16;      /* 0x13: Total sectors if < 65536 */
    uint8_t  media_type;            /* 0x15: Media descriptor byte */
    uint16_t sectors_per_fat_16;    /* 0x16: Sectors per FAT (FAT12/16) */
    
    /* DOS 3.31 BPB extensions (8 bytes) */
    uint16_t sectors_per_track;     /* 0x18: Sectors per track (CHS) */
    uint16_t head_count;            /* 0x1A: Number of heads (CHS) */
    uint32_t hidden_sectors;        /* 0x1C: Hidden sectors before this partition */
    uint32_t total_sectors_32;      /* 0x20: Total sectors if >= 65536 */
    
} fat_bpb_t;

/**
 * @brief Extended BPB (EBPB) for FAT12/FAT16
 * Follows BPB at offset 0x24
 */
typedef struct __attribute__((packed)) {
    uint8_t  drive_number;          /* 0x24: BIOS drive number (0x00 or 0x80) */
    uint8_t  reserved1;             /* 0x25: Reserved (used by Windows NT) */
    uint8_t  boot_signature;        /* 0x26: Extended boot signature (0x29) */
    uint32_t volume_serial;         /* 0x27: Volume serial number */
    char     volume_label[11];      /* 0x2B: Volume label (space-padded) */
    char     fs_type[8];            /* 0x36: File system type, e.g., "FAT12   " */
} fat_ebpb_t;

/**
 * @brief Extended BPB for FAT32
 * Different layout than FAT12/16
 */
typedef struct __attribute__((packed)) {
    uint32_t sectors_per_fat_32;    /* 0x24: Sectors per FAT */
    uint16_t ext_flags;             /* 0x28: Extended flags */
    uint16_t fs_version;            /* 0x2A: File system version */
    uint32_t root_cluster;          /* 0x2C: First cluster of root directory */
    uint16_t fs_info_sector;        /* 0x30: FSInfo sector number */
    uint16_t backup_boot_sector;    /* 0x32: Backup boot sector location */
    uint8_t  reserved[12];          /* 0x34: Reserved */
    uint8_t  drive_number;          /* 0x40: BIOS drive number */
    uint8_t  reserved1;             /* 0x41: Reserved */
    uint8_t  boot_signature;        /* 0x42: Extended boot signature (0x29) */
    uint32_t volume_serial;         /* 0x43: Volume serial number */
    char     volume_label[11];      /* 0x47: Volume label */
    char     fs_type[8];            /* 0x52: "FAT32   " */
} fat32_ebpb_t;

/* ============================================================================
 * Complete Boot Sector Structure
 * ============================================================================ */

/**
 * @brief Complete FAT boot sector (512 bytes)
 */
typedef struct __attribute__((packed)) {
    fat_bpb_t bpb;                  /* 0x00-0x23: BPB */
    union {
        fat_ebpb_t fat16;           /* FAT12/FAT16 EBPB */
        fat32_ebpb_t fat32;         /* FAT32 EBPB */
    } ebpb;
    uint8_t  boot_code[420];        /* Boot code (variable size) */
    uint16_t boot_signature;        /* 0x1FE: Must be 0xAA55 */
} fat_boot_sector_t;

/* ============================================================================
 * Analysis Result Structure
 * ============================================================================ */

/**
 * @brief FAT type enumeration
 */
typedef enum {
    FAT_TYPE_UNKNOWN = 0,
    FAT_TYPE_FAT12,
    FAT_TYPE_FAT16,
    FAT_TYPE_FAT32,
    FAT_TYPE_EXFAT,
} fat_type_t;

/**
 * @brief Boot sector analysis result
 */
typedef struct {
    /* Validation status */
    bool     valid;                 /* Overall validity */
    bool     has_boot_signature;    /* Has 0x55AA signature */
    bool     has_valid_jump;        /* Has valid JMP instruction */
    bool     has_jump_instruction;  /* Alias for has_valid_jump */
    bool     has_valid_bpb;         /* BPB values are consistent */
    bool     has_extended_bpb;      /* Has EBPB (0x29 signature) */
    
    /* Detected FAT type */
    fat_type_t fat_type;
    
    /* Extracted information */
    char     oem_name[9];           /* Null-terminated OEM name */
    char     volume_label[12];      /* Null-terminated volume label */
    char     fs_type_string[9];     /* Null-terminated FS type string */
    uint32_t volume_serial;         /* Volume serial number */
    
    /* Disk geometry */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint32_t total_sectors;
    uint8_t  media_type;
    uint32_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    
    /* Calculated values */
    uint32_t root_dir_sectors;      /* Sectors used by root directory */
    uint32_t data_sectors;          /* Data area sectors */
    uint32_t cluster_count;         /* Total clusters in data area */
    uint32_t first_data_sector;     /* First sector of data area */
    uint64_t total_bytes;           /* Total disk size in bytes */
    
    /* Media descriptor info */
    const char *media_description;  /* Human-readable media description */
    const fat_disk_geometry_t *geometry; /* Matching standard geometry or NULL */
    
} fat_analysis_result_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Analyze a FAT boot sector
 * @param data Pointer to boot sector data (at least 512 bytes)
 * @param size Size of data buffer
 * @param result Output analysis result
 * @return 0 on success, error code on failure
 */
int fat_analyze_boot_sector(const uint8_t *data, size_t size, 
                             fat_analysis_result_t *result);

/**
 * @brief Validate boot sector signature
 * @param data Pointer to boot sector data
 * @param size Size of data
 * @return true if valid signature (0x55AA) present
 */
bool fat_check_boot_signature(const uint8_t *data, size_t size);

/**
 * @brief Validate jump instruction
 * @param data Pointer to boot sector data
 * @return true if valid JMP instruction at offset 0
 */
bool fat_check_jump_instruction(const uint8_t *data);

/**
 * @brief Get media type description
 * @param media_byte Media descriptor byte
 * @return Human-readable description string
 */
const char *fat_media_description(uint8_t media_byte);

/**
 * @brief Determine FAT type from cluster count
 * @param cluster_count Number of clusters
 * @return FAT type (FAT12, FAT16, or FAT32)
 */
fat_type_t fat_determine_type(uint32_t cluster_count);

/**
 * @brief Get FAT type name string
 * @param type FAT type enum
 * @return Type name string
 */
const char *fat_type_string(fat_type_t type);

/**
 * @brief Find matching standard disk geometry
 * @param total_sectors Total sectors on disk
 * @param media_byte Media descriptor byte
 * @return Pointer to matching geometry or NULL
 */
const fat_disk_geometry_t *fat_find_geometry(uint32_t total_sectors, uint8_t media_byte);

/**
 * @brief Create a standard boot sector for a floppy disk
 * @param geometry Disk geometry to use
 * @param oem_name OEM name (8 chars max, NULL for default)
 * @param volume_label Volume label (11 chars max, NULL for default)
 * @param buffer Output buffer (at least 512 bytes)
 * @return 0 on success, error code on failure
 */
int fat_create_boot_sector(const fat_disk_geometry_t *geometry,
                            const char *oem_name,
                            const char *volume_label,
                            uint8_t *buffer);

/**
 * @brief Generate report string for boot sector analysis
 * @param result Analysis result
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
int fat_generate_report(const fat_analysis_result_t *result, 
                         char *buffer, size_t size);

/**
 * @brief Calculate total disk size from BPB
 * @param bpb Pointer to BPB structure
 * @return Total size in bytes
 */
uint64_t fat_calculate_disk_size(const fat_bpb_t *bpb);

/**
 * @brief Extract volume serial number as formatted string
 * @param serial Volume serial number
 * @param buffer Output buffer (at least 10 chars: "XXXX-XXXX\0")
 */
void fat_format_serial(uint32_t serial, char *buffer);

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FAT_OK                      0
#define FAT_ERR_NULL_POINTER        -1
#define FAT_ERR_BUFFER_TOO_SMALL    -2
#define FAT_ERR_INVALID_SIGNATURE   -3
#define FAT_ERR_INVALID_BPB         -4
#define FAT_ERR_UNSUPPORTED_FORMAT  -5

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_BOOTSECTOR_H */
