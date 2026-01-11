/**
 * @file uft_c64_crt.h
 * @brief C64 CRT Cartridge Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * CRT is the standard cartridge image format for C64 emulators.
 * Stores ROM data with hardware type and banking information.
 *
 * File Structure:
 * - 64-byte header (Big Endian!)
 * - One or more CHIP packets
 *
 * Header (64 bytes):
 * - 16 bytes: Signature "C64 CARTRIDGE   "
 * - 4 bytes: Header length (usually 0x40 = 64)
 * - 2 bytes: Version (usually 0x0100)
 * - 2 bytes: Cartridge type
 * - 1 byte: EXROM line state
 * - 1 byte: GAME line state
 * - 6 bytes: Reserved
 * - 32 bytes: Cartridge name (null padded)
 *
 * CHIP Packet:
 * - 4 bytes: "CHIP" signature
 * - 4 bytes: Total packet length
 * - 2 bytes: Chip type (0=ROM, 1=RAM, 2=Flash)
 * - 2 bytes: Bank number
 * - 2 bytes: Starting load address
 * - 2 bytes: ROM image size
 * - N bytes: ROM data
 *
 * CBM80 Signature:
 * At offset 4 in ROM: 0xC3 0xC2 0xCD 0x38 0x30 ("CBM80")
 * Indicates autostart cartridge.
 *
 * References:
 * - https://vice-emu.sourceforge.io/vice_17.html
 * - CCS64 documentation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_C64_CRT_H
#define UFT_C64_CRT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CRT Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief CRT signature */
#define UFT_CRT_SIGNATURE           "C64 CARTRIDGE   "
#define UFT_CRT_SIGNATURE_LEN       16

/** @brief CHIP signature */
#define UFT_CRT_CHIP_SIGNATURE      "CHIP"
#define UFT_CRT_CHIP_SIGNATURE_LEN  4

/** @brief CRT header size */
#define UFT_CRT_HEADER_SIZE         64

/** @brief CHIP packet header size */
#define UFT_CRT_CHIP_HEADER_SIZE    16

/** @brief CBM80 autostart signature */
#define UFT_CRT_CBM80_SIGNATURE     "\xC3\xC2\xCD" "80"
#define UFT_CRT_CBM80_OFFSET        4   /**< Offset in ROM image */

/* ═══════════════════════════════════════════════════════════════════════════
 * CRT Chip Types
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_CRT_CHIP_ROM            0   /**< ROM image */
#define UFT_CRT_CHIP_RAM            1   /**< RAM image */
#define UFT_CRT_CHIP_FLASH          2   /**< Flash ROM */

/* ═══════════════════════════════════════════════════════════════════════════
 * CRT Cartridge Types
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_CRT_TYPE_NORMAL         0   /**< Normal cartridge */
#define UFT_CRT_TYPE_ACTION_REPLAY  1   /**< Action Replay */
#define UFT_CRT_TYPE_KCS_POWER      2   /**< KCS Power Cartridge */
#define UFT_CRT_TYPE_FINAL_III      3   /**< Final Cartridge III */
#define UFT_CRT_TYPE_SIMONS_BASIC   4   /**< Simons' BASIC */
#define UFT_CRT_TYPE_OCEAN_1        5   /**< Ocean type 1 */
#define UFT_CRT_TYPE_EXPERT         6   /**< Expert Cartridge */
#define UFT_CRT_TYPE_FUN_PLAY       7   /**< Fun Play, Power Play */
#define UFT_CRT_TYPE_SUPER_GAMES    8   /**< Super Games */
#define UFT_CRT_TYPE_ATOMIC_POWER   9   /**< Atomic Power */
#define UFT_CRT_TYPE_EPYX_FASTLOAD  10  /**< Epyx Fastload */
#define UFT_CRT_TYPE_WESTERMANN     11  /**< Westermann Learning */
#define UFT_CRT_TYPE_REX_UTILITY    12  /**< Rex Utility */
#define UFT_CRT_TYPE_FINAL_I        13  /**< Final Cartridge I */
#define UFT_CRT_TYPE_MAGIC_FORMEL   14  /**< Magic Formel */
#define UFT_CRT_TYPE_C64GS          15  /**< C64 Game System, System 3 */
#define UFT_CRT_TYPE_WARP_SPEED     16  /**< Warp Speed */
#define UFT_CRT_TYPE_DINAMIC        17  /**< Dinamic */
#define UFT_CRT_TYPE_ZAXXON         18  /**< Zaxxon, Super Zaxxon */
#define UFT_CRT_TYPE_MAGIC_DESK     19  /**< Magic Desk, Domark, HES */
#define UFT_CRT_TYPE_SUPER_SNAPSHOT 20  /**< Super Snapshot V5 */
#define UFT_CRT_TYPE_COMAL_80       21  /**< COMAL-80 */
#define UFT_CRT_TYPE_STRUCTURED_BASIC 22 /**< Structured BASIC */
#define UFT_CRT_TYPE_ROSS           23  /**< Ross */
#define UFT_CRT_TYPE_DELA_EP64      24  /**< Dela EP64 */
#define UFT_CRT_TYPE_DELA_EP7x8     25  /**< Dela EP7x8 */
#define UFT_CRT_TYPE_DELA_EP256     26  /**< Dela EP256 */
#define UFT_CRT_TYPE_REX_EP256      27  /**< Rex EP256 */
#define UFT_CRT_TYPE_MIKRO_ASS      28  /**< Mikro Assembler */
#define UFT_CRT_TYPE_FINAL_PLUS     29  /**< Final Cartridge Plus */
#define UFT_CRT_TYPE_ACTION_REPLAY4 30  /**< Action Replay 4 */
#define UFT_CRT_TYPE_STARDOS        31  /**< Stardos */
#define UFT_CRT_TYPE_EASYFLASH      32  /**< EasyFlash */
#define UFT_CRT_TYPE_EASYFLASH_XBANK 33 /**< EasyFlash Xbank */
#define UFT_CRT_TYPE_CAPTURE        34  /**< Capture */
#define UFT_CRT_TYPE_ACTION_REPLAY3 35  /**< Action Replay 3 */
#define UFT_CRT_TYPE_RETRO_REPLAY   36  /**< Retro Replay */
#define UFT_CRT_TYPE_MMC64          37  /**< MMC64 */
#define UFT_CRT_TYPE_MMC_REPLAY     38  /**< MMC Replay */
#define UFT_CRT_TYPE_IDE64          39  /**< IDE64 */
#define UFT_CRT_TYPE_SUPER_SNAPSHOT4 40 /**< Super Snapshot V4 */
#define UFT_CRT_TYPE_IEEE488        41  /**< IEEE-488 */
#define UFT_CRT_TYPE_GAME_KILLER    42  /**< Game Killer */
#define UFT_CRT_TYPE_P64            43  /**< Prophet64 */
#define UFT_CRT_TYPE_EXOS           44  /**< EXOS */
#define UFT_CRT_TYPE_FREEZE_FRAME   45  /**< Freeze Frame */
#define UFT_CRT_TYPE_FREEZE_MACHINE 46  /**< Freeze Machine */
#define UFT_CRT_TYPE_SNAPSHOT64     47  /**< Snapshot64 */
#define UFT_CRT_TYPE_SUPER_EXPLODE  48  /**< Super Explode V5.0 */
#define UFT_CRT_TYPE_MAGIC_VOICE    49  /**< Magic Voice */
#define UFT_CRT_TYPE_ACTION_REPLAY2 50  /**< Action Replay 2 */
#define UFT_CRT_TYPE_MACH5          51  /**< MACH 5 */
#define UFT_CRT_TYPE_DIASHOW_MAKER  52  /**< Diashow-Maker */
#define UFT_CRT_TYPE_PAGEFOX        53  /**< Pagefox */
#define UFT_CRT_TYPE_KINGSOFT       54  /**< Kingsoft */
#define UFT_CRT_TYPE_SILVERROCK     55  /**< Silverrock 128K */
#define UFT_CRT_TYPE_FORMEL64       56  /**< Formel 64 */
#define UFT_CRT_TYPE_RGCD           57  /**< RGCD */
#define UFT_CRT_TYPE_RRNETMK3       58  /**< RR-Net MK3 */
#define UFT_CRT_TYPE_EASYCALC       59  /**< EasyCalc */
#define UFT_CRT_TYPE_GMOD2          60  /**< GMod2 */
#define UFT_CRT_TYPE_MAX_BASIC      61  /**< MAX Basic */

/* ═══════════════════════════════════════════════════════════════════════════
 * CRT Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CRT file header (64 bytes, Big Endian)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[16];         /**< "C64 CARTRIDGE   " */
    uint32_t header_length;     /**< Header length (BE, usually 0x40) */
    uint16_t version;           /**< Version (BE, usually 0x0100) */
    uint16_t type;              /**< Cartridge type (BE) */
    uint8_t exrom;              /**< EXROM line (active low) */
    uint8_t game;               /**< GAME line (active low) */
    uint8_t reserved[6];        /**< Reserved */
    char name[32];              /**< Cartridge name (null padded) */
} uft_crt_header_t;
#pragma pack(pop)

/**
 * @brief CHIP packet header (16 bytes, Big Endian)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[4];          /**< "CHIP" */
    uint32_t packet_length;     /**< Total packet length (BE) */
    uint16_t chip_type;         /**< ROM=0, RAM=1, Flash=2 (BE) */
    uint16_t bank;              /**< Bank number (BE) */
    uint16_t load_address;      /**< Load address (BE) */
    uint16_t rom_size;          /**< ROM image size (BE) */
} uft_crt_chip_header_t;
#pragma pack(pop)

/**
 * @brief CHIP packet information
 */
typedef struct {
    uint16_t chip_type;
    uint16_t bank;
    uint16_t load_address;
    uint16_t rom_size;
    uint32_t data_offset;       /**< Offset in file to ROM data */
    bool has_cbm80;             /**< Has CBM80 autostart signature */
} uft_crt_chip_info_t;

/**
 * @brief CRT file information
 */
typedef struct {
    uint16_t version;
    uint16_t type;
    uint8_t exrom;
    uint8_t game;
    char name[33];              /**< +1 for null terminator */
    uint32_t header_length;
    uint32_t file_size;
    uint32_t chip_count;
    uint32_t total_rom_size;
    bool has_cbm80;             /**< Any chip has CBM80 */
    bool valid;
} uft_crt_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_crt_header_t) == 64, "CRT header must be 64 bytes");
_Static_assert(sizeof(uft_crt_chip_header_t) == 16, "CHIP header must be 16 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions - Big Endian
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit big-endian
 */
static inline uint16_t uft_crt_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/**
 * @brief Read 32-bit big-endian
 */
static inline uint32_t uft_crt_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/**
 * @brief Get cartridge type name
 */
static inline const char* uft_crt_type_name(uint16_t type) {
    static const char* names[] = {
        "Normal cartridge",
        "Action Replay",
        "KCS Power Cartridge",
        "Final Cartridge III",
        "Simons' BASIC",
        "Ocean type 1",
        "Expert Cartridge",
        "Fun Play, Power Play",
        "Super Games",
        "Atomic Power",
        "Epyx Fastload",
        "Westermann Learning",
        "Rex Utility",
        "Final Cartridge I",
        "Magic Formel",
        "C64 Game System",
        "Warp Speed",
        "Dinamic",
        "Zaxxon",
        "Magic Desk",
        "Super Snapshot V5",
        "COMAL-80",
        "Structured BASIC",
        "Ross",
        "Dela EP64",
        "Dela EP7x8",
        "Dela EP256",
        "Rex EP256",
        "Mikro Assembler",
        "Final Cartridge Plus",
        "Action Replay 4",
        "Stardos",
        "EasyFlash",
        "EasyFlash Xbank",
        "Capture",
        "Action Replay 3",
        "Retro Replay",
        "MMC64",
        "MMC Replay",
        "IDE64",
        "Super Snapshot V4",
    };
    
    if (type < sizeof(names) / sizeof(names[0])) {
        return names[type];
    }
    return "Unknown";
}

/**
 * @brief Get chip type name
 */
static inline const char* uft_crt_chip_type_name(uint16_t type) {
    switch (type) {
        case UFT_CRT_CHIP_ROM:   return "ROM";
        case UFT_CRT_CHIP_RAM:   return "RAM";
        case UFT_CRT_CHIP_FLASH: return "Flash";
        default:                return "Unknown";
    }
}

/**
 * @brief Check for CBM80 autostart signature
 */
static inline bool uft_crt_has_cbm80(const uint8_t* rom_data, size_t size) {
    if (!rom_data || size < 9) return false;
    
    /* CBM80 at offset 4: 0xC3 0xC2 0xCD '8' '0' */
    return (rom_data[4] == 0xC3 && rom_data[5] == 0xC2 &&
            rom_data[6] == 0xCD && rom_data[7] == '8' && rom_data[8] == '0');
}

/**
 * @brief Verify CRT signature
 */
static inline bool uft_crt_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_CRT_HEADER_SIZE) return false;
    return memcmp(data, UFT_CRT_SIGNATURE, UFT_CRT_SIGNATURE_LEN) == 0;
}

/**
 * @brief Probe for CRT format
 * @return Confidence score (0-100)
 */
static inline int uft_crt_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_CRT_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_CRT_SIGNATURE, UFT_CRT_SIGNATURE_LEN) == 0) {
        score += 50;
    } else {
        return 0;
    }
    
    /* Check header length */
    uint32_t hdr_len = uft_crt_be32(data + 16);
    if (hdr_len >= 0x20 && hdr_len <= 0x100) {
        score += 15;
    }
    
    /* Check version */
    uint16_t version = uft_crt_be16(data + 20);
    if ((version >> 8) <= 2) {  /* Major version 0, 1, or 2 */
        score += 10;
    }
    
    /* Check cartridge type */
    uint16_t type = uft_crt_be16(data + 22);
    if (type <= 61) {  /* Known type */
        score += 10;
    }
    
    /* Check for CHIP packet */
    if (size >= hdr_len + 4) {
        if (memcmp(data + hdr_len, UFT_CRT_CHIP_SIGNATURE, 4) == 0) {
            score += 15;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse CRT header and count CHIP packets
 */
static inline bool uft_crt_parse_header(const uint8_t* data, size_t size,
                                         uft_crt_file_info_t* info) {
    if (!data || !info) return false;
    if (!uft_crt_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_crt_header_t* hdr = (const uft_crt_header_t*)data;
    
    info->header_length = uft_crt_be32((const uint8_t*)&hdr->header_length);
    info->version = uft_crt_be16((const uint8_t*)&hdr->version);
    info->type = uft_crt_be16((const uint8_t*)&hdr->type);
    info->exrom = hdr->exrom;
    info->game = hdr->game;
    info->file_size = size;
    
    /* Copy name */
    memcpy(info->name, hdr->name, 32);
    info->name[32] = '\0';
    
    /* Trim trailing spaces/nulls */
    for (int i = 31; i >= 0 && (info->name[i] == ' ' || info->name[i] == '\0'); i--) {
        info->name[i] = '\0';
    }
    
    /* Count CHIP packets */
    size_t pos = info->header_length;
    while (pos + UFT_CRT_CHIP_HEADER_SIZE <= size) {
        if (memcmp(data + pos, UFT_CRT_CHIP_SIGNATURE, 4) != 0) {
            break;  /* Not a CHIP packet */
        }
        
        uint32_t packet_len = uft_crt_be32(data + pos + 4);
        uint16_t rom_size = uft_crt_be16(data + pos + 14);
        
        info->chip_count++;
        info->total_rom_size += rom_size;
        
        /* Check for CBM80 */
        if (pos + UFT_CRT_CHIP_HEADER_SIZE + 9 <= size) {
            if (uft_crt_has_cbm80(data + pos + UFT_CRT_CHIP_HEADER_SIZE, rom_size)) {
                info->has_cbm80 = true;
            }
        }
        
        pos += packet_len;
    }
    
    info->valid = true;
    return true;
}

/**
 * @brief Iterate CHIP packets in CRT file
 * @param data File data
 * @param size File size
 * @param offset Current offset (start with header_length)
 * @param chip Output chip info
 * @return Next offset, or 0 if no more chips
 */
static inline size_t uft_crt_next_chip(const uint8_t* data, size_t size,
                                        size_t offset, uft_crt_chip_info_t* chip) {
    if (!data || !chip) return 0;
    if (offset + UFT_CRT_CHIP_HEADER_SIZE > size) return 0;
    
    /* Verify CHIP signature */
    if (memcmp(data + offset, UFT_CRT_CHIP_SIGNATURE, 4) != 0) {
        return 0;
    }
    
    uint32_t packet_len = uft_crt_be32(data + offset + 4);
    
    chip->chip_type = uft_crt_be16(data + offset + 8);
    chip->bank = uft_crt_be16(data + offset + 10);
    chip->load_address = uft_crt_be16(data + offset + 12);
    chip->rom_size = uft_crt_be16(data + offset + 14);
    chip->data_offset = offset + UFT_CRT_CHIP_HEADER_SIZE;
    
    /* Check for CBM80 */
    if (chip->data_offset + 9 <= size) {
        chip->has_cbm80 = uft_crt_has_cbm80(data + chip->data_offset, chip->rom_size);
    } else {
        chip->has_cbm80 = false;
    }
    
    return offset + packet_len;
}

/**
 * @brief Print CRT file info
 */
static inline void uft_crt_print_info(const uft_crt_file_info_t* info) {
    if (!info) return;
    
    printf("C64 CRT Cartridge:\n");
    printf("  Name:          %s\n", info->name[0] ? info->name : "(unnamed)");
    printf("  Version:       %d.%d\n", info->version >> 8, info->version & 0xFF);
    printf("  Type:          %d - %s\n", info->type, uft_crt_type_name(info->type));
    printf("  EXROM:         %d\n", info->exrom);
    printf("  GAME:          %d\n", info->game);
    printf("  File Size:     %u bytes\n", info->file_size);
    printf("  CHIP Packets:  %u\n", info->chip_count);
    printf("  Total ROM:     %u bytes\n", info->total_rom_size);
    printf("  Autostart:     %s\n", info->has_cbm80 ? "Yes (CBM80)" : "No");
}

/**
 * @brief List all CHIP packets
 */
static inline void uft_crt_list_chips(const uint8_t* data, size_t size,
                                       uint32_t header_length) {
    if (!data) return;
    
    printf("CHIP Packets:\n");
    printf("  %-4s %-6s %-8s %-6s %s\n", 
           "Bank", "Type", "Address", "Size", "Autostart");
    printf("  %-4s %-6s %-8s %-6s %s\n",
           "----", "------", "--------", "------", "---------");
    
    size_t offset = header_length;
    uft_crt_chip_info_t chip;
    
    while ((offset = uft_crt_next_chip(data, size, offset, &chip)) != 0) {
        printf("  %-4d %-6s $%04X    %-6d %s\n",
               chip.bank,
               uft_crt_chip_type_name(chip.chip_type),
               chip.load_address,
               chip.rom_size,
               chip.has_cbm80 ? "Yes" : "No");
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_CRT_H */
