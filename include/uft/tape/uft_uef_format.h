/**
 * @file uft_uef_format.h
 * @brief BBC Micro UEF (Unified Emulator Format) Tape Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * UEF is a chunk-based tape format for BBC Micro and Acorn Electron.
 * Uses gzip compression for the outer container.
 *
 * File Structure:
 * - 12-byte header: "UEF File!" + 0x00 + minor + major version
 * - Sequence of chunks
 *
 * Chunk Structure:
 * - 2 bytes: Chunk ID (little endian)
 * - 4 bytes: Chunk length (little endian)
 * - N bytes: Chunk data
 *
 * Chunk Categories:
 * - 0x0000-0x00FF: Tape data chunks
 * - 0x0100-0x01FF: Tape emulator state chunks
 * - 0x0200-0x02FF: Disc chunks
 * - 0x0300-0x03FF: ROM chunks
 * - 0x0400-0x04FF: Emulator state (BeebEm)
 * - 0xFF00-0xFFFF: Reserved
 *
 * References:
 * - http://electrem.emuunlim.com/UEFSpecs.htm
 * - BeebEm source code
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_UEF_FORMAT_H
#define UFT_UEF_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * UEF Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief UEF signature */
#define UFT_UEF_SIGNATURE           "UEF File!"
#define UFT_UEF_SIGNATURE_LEN       10  /* Including null terminator */

/** @brief UEF header size */
#define UFT_UEF_HEADER_SIZE         12

/** @brief UEF chunk header size */
#define UFT_UEF_CHUNK_HEADER_SIZE   6

/* ═══════════════════════════════════════════════════════════════════════════
 * UEF Chunk IDs - Tape Data (0x0000-0x00FF)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_UEF_CHUNK_ORIGIN            0x0000  /**< Origin information */
#define UFT_UEF_CHUNK_INSTRUCTIONS      0x0001  /**< Game instructions/manual */
#define UFT_UEF_CHUNK_INLAY_SCAN        0x0003  /**< Inlay scan (compressed) */
#define UFT_UEF_CHUNK_TARGET_MACHINE    0x0005  /**< Target machine info */

#define UFT_UEF_CHUNK_START_STOP_TONE   0x0100  /**< Implicit start/stop bit */
#define UFT_UEF_CHUNK_DATA_BLOCK        0x0100  /**< Data block (same as START_STOP) */
#define UFT_UEF_CHUNK_CARRIER_TONE      0x0110  /**< High tone (carrier) */
#define UFT_UEF_CHUNK_CARRIER_TONE_INT  0x0111  /**< High tone with dummy byte */
#define UFT_UEF_CHUNK_GAP_INTEGER       0x0112  /**< Integer gap */
#define UFT_UEF_CHUNK_GAP_FLOAT         0x0116  /**< Floating point gap */
#define UFT_UEF_CHUNK_BAUD_RATE         0x0113  /**< Change of base frequency */
#define UFT_UEF_CHUNK_SECURITY_CYCLES   0x0114  /**< Security cycles (anti-copy) */
#define UFT_UEF_CHUNK_PHASE_CHANGE      0x0115  /**< Phase change */
#define UFT_UEF_CHUNK_DATA_BLOCK        0x0100  /**< Data block */
#define UFT_UEF_CHUNK_DEFINED_FORMAT    0x0104  /**< Defined tape format data */
#define UFT_UEF_CHUNK_MULTIPLEXED       0x0120  /**< Multiplexed data block */

/* ═══════════════════════════════════════════════════════════════════════════
 * UEF Chunk IDs - Emulator State (0x0400-0x04FF) - BeebEm
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_UEF_CHUNK_CPU_STATE         0x0460  /**< 6502 CPU state */
#define UFT_UEF_CHUNK_RAM               0x0462  /**< RAM dump */
#define UFT_UEF_CHUNK_ROM               0x0464  /**< ROM dump */
#define UFT_UEF_CHUNK_VIDEO_ULA         0x0468  /**< Video ULA state */
#define UFT_UEF_CHUNK_CRTC              0x046A  /**< CRTC state */
#define UFT_UEF_CHUNK_SYSTEM_VIA        0x046C  /**< System VIA state */
#define UFT_UEF_CHUNK_USER_VIA          0x046E  /**< User VIA state */

/* ═══════════════════════════════════════════════════════════════════════════
 * UEF Target Machines
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_UEF_MACHINE_BBC_A           0x00    /**< BBC Model A */
#define UFT_UEF_MACHINE_BBC_B           0x01    /**< BBC Model B */
#define UFT_UEF_MACHINE_BBC_B_PLUS      0x02    /**< BBC Model B+ */
#define UFT_UEF_MACHINE_BBC_MASTER      0x03    /**< BBC Master */
#define UFT_UEF_MACHINE_ELECTRON        0x04    /**< Acorn Electron */

/* ═══════════════════════════════════════════════════════════════════════════
 * UEF Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief UEF file header (12 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[10];         /**< "UEF File!" + 0x00 */
    uint8_t version_minor;      /**< Minor version */
    uint8_t version_major;      /**< Major version */
} uft_uef_header_t;
#pragma pack(pop)

/**
 * @brief UEF chunk header (6 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t chunk_id;          /**< Chunk type ID (LE) */
    uint32_t length;            /**< Chunk data length (LE) */
} uft_uef_chunk_header_t;
#pragma pack(pop)

/**
 * @brief UEF chunk information
 */
typedef struct {
    uint16_t id;                /**< Chunk ID */
    uint32_t length;            /**< Data length */
    uint32_t offset;            /**< Offset in file (after header) */
    const char* name;           /**< Chunk type name */
} uft_uef_chunk_info_t;

/**
 * @brief UEF 6502 CPU state (chunk 0x0460)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t a;                  /**< Accumulator */
    uint8_t x;                  /**< X register */
    uint8_t y;                  /**< Y register */
    uint8_t flags;              /**< Status flags */
    uint8_t sp;                 /**< Stack pointer */
    uint16_t pc;                /**< Program counter */
} uft_uef_cpu_state_t;
#pragma pack(pop)

/**
 * @brief UEF file information
 */
typedef struct {
    uint8_t version_major;
    uint8_t version_minor;
    uint32_t file_size;
    uint32_t chunk_count;
    uint32_t data_chunks;       /**< Tape data chunks */
    uint32_t state_chunks;      /**< Emulator state chunks */
    bool has_cpu_state;
    bool has_ram;
    bool has_rom;
    uint8_t target_machine;
    bool valid;
} uft_uef_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_uef_header_t) == 12, "UEF header must be 12 bytes");
_Static_assert(sizeof(uft_uef_chunk_header_t) == 6, "UEF chunk header must be 6 bytes");
_Static_assert(sizeof(uft_uef_cpu_state_t) == 7, "UEF CPU state must be 7 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_uef_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian
 */
static inline uint32_t uft_uef_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Get chunk category name
 */
static inline const char* uft_uef_chunk_category(uint16_t id) {
    uint8_t cat = (id >> 8) & 0xFF;
    switch (cat) {
        case 0x00: return "Tape Data";
        case 0x01: return "Tape Format";
        case 0x02: return "Disc";
        case 0x03: return "ROM";
        case 0x04: return "Emulator State";
        case 0xFF: return "Reserved";
        default:   return "Unknown";
    }
}

/**
 * @brief Get chunk type name
 */
static inline const char* uft_uef_chunk_name(uint16_t id) {
    switch (id) {
        case UFT_UEF_CHUNK_ORIGIN:          return "Origin Info";
        case UFT_UEF_CHUNK_INSTRUCTIONS:    return "Instructions";
        case UFT_UEF_CHUNK_INLAY_SCAN:      return "Inlay Scan";
        case UFT_UEF_CHUNK_TARGET_MACHINE:  return "Target Machine";
        case UFT_UEF_CHUNK_START_STOP_TONE: return "Data Block";  /* 0x0100 */
        case UFT_UEF_CHUNK_DEFINED_FORMAT:  return "Defined Format";
        case UFT_UEF_CHUNK_CARRIER_TONE:    return "Carrier Tone";
        case UFT_UEF_CHUNK_CARRIER_TONE_INT:return "Carrier Tone (Int)";
        case UFT_UEF_CHUNK_GAP_INTEGER:     return "Gap (Integer)";
        case UFT_UEF_CHUNK_GAP_FLOAT:       return "Gap (Float)";
        case UFT_UEF_CHUNK_BAUD_RATE:       return "Baud Rate";
        case UFT_UEF_CHUNK_SECURITY_CYCLES: return "Security Cycles";
        case UFT_UEF_CHUNK_PHASE_CHANGE:    return "Phase Change";
        case UFT_UEF_CHUNK_MULTIPLEXED:     return "Multiplexed Data";
        case UFT_UEF_CHUNK_CPU_STATE:       return "CPU State";
        case UFT_UEF_CHUNK_RAM:             return "RAM";
        case UFT_UEF_CHUNK_ROM:             return "ROM";
        case UFT_UEF_CHUNK_VIDEO_ULA:       return "Video ULA";
        case UFT_UEF_CHUNK_CRTC:            return "CRTC";
        case UFT_UEF_CHUNK_SYSTEM_VIA:      return "System VIA";
        case UFT_UEF_CHUNK_USER_VIA:        return "User VIA";
        default:                            return "Unknown";
    }
}

/**
 * @brief Get target machine name
 */
static inline const char* uft_uef_machine_name(uint8_t machine) {
    switch (machine) {
        case UFT_UEF_MACHINE_BBC_A:      return "BBC Model A";
        case UFT_UEF_MACHINE_BBC_B:      return "BBC Model B";
        case UFT_UEF_MACHINE_BBC_B_PLUS: return "BBC Model B+";
        case UFT_UEF_MACHINE_BBC_MASTER: return "BBC Master";
        case UFT_UEF_MACHINE_ELECTRON:   return "Acorn Electron";
        default:                         return "Unknown";
    }
}

/**
 * @brief Verify UEF signature
 */
static inline bool uft_uef_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_UEF_HEADER_SIZE) return false;
    
    /* Check "UEF File!" signature */
    if (memcmp(data, UFT_UEF_SIGNATURE, 9) != 0) return false;
    
    /* Check null terminator */
    if (data[9] != 0x00) return false;
    
    return true;
}

/**
 * @brief Probe for UEF format
 * @return Confidence score (0-100)
 */
static inline int uft_uef_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_UEF_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_UEF_SIGNATURE, 9) == 0 && data[9] == 0x00) {
        score += 60;
    } else {
        return 0;
    }
    
    /* Check version */
    uint8_t major = data[11];
    uint8_t minor = data[10];
    if (major == 0 && minor <= 10) {
        score += 20;  /* Version 0.x */
    } else if (major == 1 && minor <= 10) {
        score += 20;  /* Version 1.x */
    }
    
    /* Check first chunk is valid */
    if (size >= UFT_UEF_HEADER_SIZE + UFT_UEF_CHUNK_HEADER_SIZE) {
        uint16_t chunk_id = uft_uef_le16(data + 12);
        uint32_t chunk_len = uft_uef_le32(data + 14);
        
        /* Valid chunk ID range */
        uint8_t cat = (chunk_id >> 8) & 0xFF;
        if (cat <= 0x04 || cat == 0xFF) {
            score += 10;
        }
        
        /* Reasonable chunk length */
        if (chunk_len < size && chunk_len < 0x100000) {
            score += 10;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse UEF header and count chunks
 */
static inline bool uft_uef_parse_header(const uint8_t* data, size_t size,
                                         uft_uef_file_info_t* info) {
    if (!data || !info) return false;
    if (!uft_uef_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_uef_header_t* hdr = (const uft_uef_header_t*)data;
    info->version_major = hdr->version_major;
    info->version_minor = hdr->version_minor;
    info->file_size = size;
    info->target_machine = 0xFF;  /* Unknown */
    
    /* Count chunks */
    size_t pos = UFT_UEF_HEADER_SIZE;
    while (pos + UFT_UEF_CHUNK_HEADER_SIZE <= size) {
        uint16_t chunk_id = uft_uef_le16(data + pos);
        uint32_t chunk_len = uft_uef_le32(data + pos + 2);
        
        if (pos + UFT_UEF_CHUNK_HEADER_SIZE + chunk_len > size) {
            break;  /* Truncated chunk */
        }
        
        info->chunk_count++;
        
        /* Categorize chunk */
        uint8_t cat = (chunk_id >> 8) & 0xFF;
        if (cat == 0x00 || cat == 0x01) {
            info->data_chunks++;
        } else if (cat == 0x04) {
            info->state_chunks++;
            
            if (chunk_id == UFT_UEF_CHUNK_CPU_STATE) {
                info->has_cpu_state = true;
            } else if (chunk_id == UFT_UEF_CHUNK_RAM) {
                info->has_ram = true;
            } else if (chunk_id == UFT_UEF_CHUNK_ROM) {
                info->has_rom = true;
            }
        }
        
        /* Target machine info */
        if (chunk_id == UFT_UEF_CHUNK_TARGET_MACHINE && chunk_len >= 1) {
            info->target_machine = data[pos + UFT_UEF_CHUNK_HEADER_SIZE];
        }
        
        pos += UFT_UEF_CHUNK_HEADER_SIZE + chunk_len;
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Iterate chunks in UEF file
 * @param data File data
 * @param size File size
 * @param offset Current offset (start with UFT_UEF_HEADER_SIZE)
 * @param chunk Output chunk info
 * @return Next offset, or 0 if no more chunks
 */
static inline size_t uft_uef_next_chunk(const uint8_t* data, size_t size,
                                         size_t offset, uft_uef_chunk_info_t* chunk) {
    if (!data || !chunk) return 0;
    if (offset + UFT_UEF_CHUNK_HEADER_SIZE > size) return 0;
    
    chunk->id = uft_uef_le16(data + offset);
    chunk->length = uft_uef_le32(data + offset + 2);
    chunk->offset = offset + UFT_UEF_CHUNK_HEADER_SIZE;
    chunk->name = uft_uef_chunk_name(chunk->id);
    
    if (chunk->offset + chunk->length > size) {
        return 0;  /* Truncated */
    }
    
    return chunk->offset + chunk->length;
}

/**
 * @brief Extract CPU state from chunk data
 */
static inline bool uft_uef_extract_cpu_state(const uint8_t* data, size_t len,
                                              uft_uef_cpu_state_t* state) {
    if (!data || !state || len < sizeof(uft_uef_cpu_state_t)) {
        return false;
    }
    
    state->a = data[0];
    state->x = data[1];
    state->y = data[2];
    state->flags = data[3];
    state->sp = data[4];
    state->pc = uft_uef_le16(data + 5);
    
    return true;
}

/**
 * @brief Print UEF file info
 */
static inline void uft_uef_print_info(const uft_uef_file_info_t* info) {
    if (!info) return;
    
    printf("UEF File Information:\n");
    printf("  Version:       %d.%02d\n", info->version_major, info->version_minor);
    printf("  File Size:     %u bytes\n", info->file_size);
    printf("  Total Chunks:  %u\n", info->chunk_count);
    printf("  Data Chunks:   %u\n", info->data_chunks);
    printf("  State Chunks:  %u\n", info->state_chunks);
    
    if (info->target_machine != 0xFF) {
        printf("  Target:        %s\n", uft_uef_machine_name(info->target_machine));
    }
    
    if (info->has_cpu_state) printf("  Has CPU State: Yes\n");
    if (info->has_ram) printf("  Has RAM:       Yes\n");
    if (info->has_rom) printf("  Has ROM:       Yes\n");
}

/**
 * @brief List all chunks in UEF file
 */
static inline void uft_uef_list_chunks(const uint8_t* data, size_t size) {
    if (!data || size < UFT_UEF_HEADER_SIZE) return;
    
    printf("UEF Chunks:\n");
    printf("  %-6s %-20s %s\n", "ID", "Name", "Length");
    printf("  %-6s %-20s %s\n", "------", "--------------------", "------");
    
    size_t offset = UFT_UEF_HEADER_SIZE;
    uft_uef_chunk_info_t chunk;
    
    while ((offset = uft_uef_next_chunk(data, size, offset, &chunk)) != 0) {
        printf("  0x%04X %-20s %u\n", chunk.id, chunk.name, chunk.length);
        offset = chunk.offset + chunk.length;
    }
}

/**
 * @brief Create UEF header
 */
static inline void uft_uef_create_header(uft_uef_header_t* hdr,
                                          uint8_t version_major,
                                          uint8_t version_minor) {
    memset(hdr, 0, sizeof(*hdr));
    memcpy(hdr->signature, UFT_UEF_SIGNATURE, UFT_UEF_SIGNATURE_LEN);
    hdr->version_major = version_major;
    hdr->version_minor = version_minor;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_UEF_FORMAT_H */
