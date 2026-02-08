/**
 * @file uft_c64_t64.h
 * @brief C64 T64 Tape Archive Format Support
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * T64 is a container format for C64 tape files.
 * It stores multiple files in a single archive with directory.
 *
 * T64 Structure:
 * - 64-byte header (tape record info)
 * - 32-byte entries × max_entries (directory)
 * - File data (concatenated)
 *
 * Originally created by Miha Peternel for C64S emulator.
 * Common signatures: "C64 tape image file", "C64S tape image file"
 *
 * File types stored:
 * - PRG: Program files (most common)
 * - SEQ: Sequential files
 * - USR: User files
 * - REL: Relative files (rare)
 * - Frozen memory snapshots
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_C64_T64_H
#define UFT_C64_T64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * T64 Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief T64 header size */
#define UFT_T64_HEADER_SIZE         64

/** @brief T64 directory entry size */
#define UFT_T64_ENTRY_SIZE          32

/** @brief T64 signature variants */
#define UFT_T64_SIGNATURE_1         "C64 tape image file"
#define UFT_T64_SIGNATURE_2         "C64S tape image file"
#define UFT_T64_SIGNATURE_3         "C64S tape file"

/** @brief Maximum filename length */
#define UFT_T64_FILENAME_LEN        16

/** @brief Tape name length in header */
#define UFT_T64_TAPENAME_LEN        24

/* File types */
#define UFT_T64_TYPE_FREE           0x00    /**< Free entry */
#define UFT_T64_TYPE_NORMAL         0x01    /**< Normal tape file */
#define UFT_T64_TYPE_HEADER         0x02    /**< Tape header (with data) */
#define UFT_T64_TYPE_SNAPSHOT       0x03    /**< Memory snapshot */
#define UFT_T64_TYPE_BLOCK          0x04    /**< Tape block */
#define UFT_T64_TYPE_STREAM         0x05    /**< Sequential data stream */

/* C64 file types (in C1541 style) */
#define UFT_T64_FTYPE_DEL           0x00    /**< Deleted */
#define UFT_T64_FTYPE_SEQ           0x01    /**< Sequential */
#define UFT_T64_FTYPE_PRG           0x02    /**< Program */
#define UFT_T64_FTYPE_USR           0x03    /**< User */
#define UFT_T64_FTYPE_REL           0x04    /**< Relative */

/* ═══════════════════════════════════════════════════════════════════════════
 * T64 Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief T64 file header (64 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[32];             /**< "C64 tape image file" + padding */
    uint16_t version;               /**< Version (usually 0x0100 or 0x0101) */
    uint16_t max_entries;           /**< Maximum directory entries */
    uint16_t used_entries;          /**< Entries actually used */
    uint16_t reserved;              /**< Reserved (0) */
    char tape_name[24];             /**< Tape name (PETSCII, space-padded) */
} uft_t64_header_t;
#pragma pack(pop)

/**
 * @brief T64 directory entry (32 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t entry_type;             /**< Entry type (0=free, 1=normal, etc.) */
    uint8_t file_type;              /**< C64 file type (PRG, SEQ, etc.) */
    uint16_t start_addr;            /**< Start address (little endian) */
    uint16_t end_addr;              /**< End address (little endian) */
    uint16_t reserved1;             /**< Reserved */
    uint32_t data_offset;           /**< Offset in file to data */
    uint32_t reserved2;             /**< Reserved */
    char filename[16];              /**< Filename (PETSCII, space-padded) */
} uft_t64_entry_t;
#pragma pack(pop)

/**
 * @brief Parsed T64 file entry
 */
typedef struct {
    uint8_t entry_type;
    uint8_t file_type;
    uint16_t start_addr;
    uint16_t end_addr;
    uint32_t data_offset;
    uint32_t data_size;             /**< Calculated: end - start */
    char filename[17];              /**< Null-terminated filename */
    bool valid;
} uft_t64_file_info_t;

/**
 * @brief T64 archive information
 */
typedef struct {
    uint16_t version;
    uint16_t max_entries;
    uint16_t used_entries;
    char tape_name[25];             /**< Null-terminated */
    uint32_t total_size;
    uint32_t data_size;             /**< Total data bytes */
    bool valid;
} uft_t64_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_t64_header_t) == 64, "T64 header must be 64 bytes");
_Static_assert(sizeof(uft_t64_entry_t) == 32, "T64 entry must be 32 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get entry type name
 */
static inline const char* uft_t64_entry_type_name(uint8_t type) {
    switch (type) {
        case UFT_T64_TYPE_FREE:     return "Free";
        case UFT_T64_TYPE_NORMAL:   return "Normal";
        case UFT_T64_TYPE_HEADER:   return "Header";
        case UFT_T64_TYPE_SNAPSHOT: return "Snapshot";
        case UFT_T64_TYPE_BLOCK:    return "Block";
        case UFT_T64_TYPE_STREAM:   return "Stream";
        default: return "Unknown";
    }
}

/**
 * @brief Get C64 file type name
 */
static inline const char* uft_t64_file_type_name(uint8_t type) {
    switch (type & 0x07) {  /* Mask off flags */
        case UFT_T64_FTYPE_DEL: return "DEL";
        case UFT_T64_FTYPE_SEQ: return "SEQ";
        case UFT_T64_FTYPE_PRG: return "PRG";
        case UFT_T64_FTYPE_USR: return "USR";
        case UFT_T64_FTYPE_REL: return "REL";
        default: return "???";
    }
}

/**
 * @brief Convert PETSCII filename to ASCII
 */
static inline void uft_t64_petscii_to_ascii(const char* petscii, size_t len,
                                             char* ascii, size_t max_len) {
    if (!petscii || !ascii || max_len == 0) return;
    
    size_t out_len = (len < max_len - 1) ? len : max_len - 1;
    
    for (size_t i = 0; i < out_len; i++) {
        unsigned char c = (unsigned char)petscii[i];
        
        /* Stop at null */
        if (c == 0) {
            out_len = i;
            break;
        }
        
        /* Convert PETSCII to ASCII */
        if (c >= 0x41 && c <= 0x5A) {
            /* Uppercase letters stay uppercase */
            ascii[i] = c;
        } else if (c >= 0xC1 && c <= 0xDA) {
            /* Shifted uppercase -> lowercase */
            ascii[i] = c - 0x80;
        } else if (c >= 0x61 && c <= 0x7A) {
            /* Lowercase -> uppercase (PETSCII quirk) */
            ascii[i] = c - 0x20;
        } else if (c == 0xA0 || c == 0x20) {
            /* Space variants */
            ascii[i] = ' ';
        } else if (c >= 0x20 && c < 0x7F) {
            /* Printable ASCII */
            ascii[i] = c;
        } else {
            /* Non-printable -> underscore */
            ascii[i] = '_';
        }
    }
    ascii[out_len] = '\0';
    
    /* Trim trailing spaces */
    while (out_len > 0 && ascii[out_len - 1] == ' ') {
        ascii[--out_len] = '\0';
    }
}

/**
 * @brief Verify T64 signature
 */
static inline bool uft_t64_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_T64_HEADER_SIZE) return false;
    
    /* Check various known signatures */
    if (memcmp(data, UFT_T64_SIGNATURE_1, strlen(UFT_T64_SIGNATURE_1)) == 0) return true;
    if (memcmp(data, UFT_T64_SIGNATURE_2, strlen(UFT_T64_SIGNATURE_2)) == 0) return true;
    if (memcmp(data, UFT_T64_SIGNATURE_3, strlen(UFT_T64_SIGNATURE_3)) == 0) return true;
    
    /* Some files just have "C64" */
    if (memcmp(data, "C64", 3) == 0) return true;
    
    return false;
}

/**
 * @brief Probe for T64 format
 * @return Confidence score (0-100)
 */
static inline int uft_t64_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_T64_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_T64_SIGNATURE_1, strlen(UFT_T64_SIGNATURE_1)) == 0 ||
        memcmp(data, UFT_T64_SIGNATURE_2, strlen(UFT_T64_SIGNATURE_2)) == 0) {
        score += 50;
    } else if (memcmp(data, "C64", 3) == 0) {
        score += 30;
    } else {
        return 0;
    }
    
    const uft_t64_header_t* hdr = (const uft_t64_header_t*)data;
    
    /* Check version (usually 0x0100 or 0x0101) */
    if (hdr->version == 0x0100 || hdr->version == 0x0101) {
        score += 15;
    }
    
    /* Check entry counts are reasonable */
    if (hdr->max_entries > 0 && hdr->max_entries <= 1000 &&
        hdr->used_entries <= hdr->max_entries) {
        score += 15;
    }
    
    /* Check if we have enough space for directory */
    size_t dir_size = UFT_T64_HEADER_SIZE + (hdr->max_entries * UFT_T64_ENTRY_SIZE);
    if (dir_size <= size) {
        score += 10;
    }
    
    /* Check first entry if present */
    if (hdr->used_entries > 0 && size >= UFT_T64_HEADER_SIZE + UFT_T64_ENTRY_SIZE) {
        const uft_t64_entry_t* entry = (const uft_t64_entry_t*)(data + UFT_T64_HEADER_SIZE);
        
        if (entry->entry_type <= UFT_T64_TYPE_STREAM) {
            score += 5;
        }
        if (entry->start_addr < entry->end_addr) {
            score += 5;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse T64 header
 */
static inline bool uft_t64_parse_header(const uint8_t* data, size_t size,
                                         uft_t64_info_t* info) {
    if (!data || size < UFT_T64_HEADER_SIZE || !info) return false;
    
    if (!uft_t64_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_t64_header_t* hdr = (const uft_t64_header_t*)data;
    
    info->version = hdr->version;
    info->max_entries = hdr->max_entries;
    info->used_entries = hdr->used_entries;
    info->total_size = size;
    info->valid = true;
    
    /* Convert tape name */
    uft_t64_petscii_to_ascii(hdr->tape_name, UFT_T64_TAPENAME_LEN,
                             info->tape_name, sizeof(info->tape_name));
    
    return true;
}

/**
 * @brief Parse T64 directory entry
 */
static inline bool uft_t64_parse_entry(const uint8_t* data, size_t size,
                                        int index, uft_t64_file_info_t* file) {
    if (!data || !file) return false;
    
    size_t entry_offset = UFT_T64_HEADER_SIZE + (index * UFT_T64_ENTRY_SIZE);
    if (entry_offset + UFT_T64_ENTRY_SIZE > size) return false;
    
    memset(file, 0, sizeof(*file));
    
    const uft_t64_entry_t* entry = (const uft_t64_entry_t*)(data + entry_offset);
    
    file->entry_type = entry->entry_type;
    file->file_type = entry->file_type;
    file->start_addr = entry->start_addr;
    file->end_addr = entry->end_addr;
    file->data_offset = entry->data_offset;
    
    /* Calculate data size */
    if (entry->end_addr > entry->start_addr) {
        file->data_size = entry->end_addr - entry->start_addr;
    }
    
    /* Convert filename */
    uft_t64_petscii_to_ascii(entry->filename, UFT_T64_FILENAME_LEN,
                             file->filename, sizeof(file->filename));
    
    file->valid = (entry->entry_type != UFT_T64_TYPE_FREE);
    
    return true;
}

/**
 * @brief Get file data pointer
 */
static inline const uint8_t* uft_t64_get_file_data(const uint8_t* data, size_t size,
                                                    const uft_t64_file_info_t* file) {
    if (!data || !file || !file->valid) return NULL;
    if (file->data_offset + file->data_size > size) return NULL;
    
    return data + file->data_offset;
}

/**
 * @brief Print T64 info
 */
static inline void uft_t64_print_info(const uft_t64_info_t* info) {
    if (!info) return;
    
    printf("T64 Archive Information:\n");
    printf("  Version:     %04X\n", info->version);
    printf("  Tape Name:   %s\n", info->tape_name);
    printf("  Max Entries: %u\n", info->max_entries);
    printf("  Used:        %u\n", info->used_entries);
    printf("  Total Size:  %u bytes\n", info->total_size);
}

/**
 * @brief Print T64 file entry
 */
static inline void uft_t64_print_entry(const uft_t64_file_info_t* file, int index) {
    if (!file) return;
    
    printf("%3d  %-3s  $%04X-$%04X  %5u  %-16s  %s\n",
           index,
           uft_t64_file_type_name(file->file_type),
           file->start_addr,
           file->end_addr,
           file->data_size,
           file->filename,
           file->valid ? "" : "(empty)");
}

/**
 * @brief List all files in T64 archive
 */
static inline void uft_t64_list_files(const uint8_t* data, size_t size) {
    uft_t64_info_t info;
    if (!uft_t64_parse_header(data, size, &info)) {
        printf("Invalid T64 file\n");
        return;
    }
    
    printf("T64: %s\n", info.tape_name);
    printf("  #   Type  Address      Size   Filename\n");
    printf("─────────────────────────────────────────────────\n");
    
    for (int i = 0; i < info.max_entries; i++) {
        uft_t64_file_info_t file;
        if (uft_t64_parse_entry(data, size, i, &file)) {
            if (file.valid) {
                uft_t64_print_entry(&file, i);
            }
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_T64_H */
