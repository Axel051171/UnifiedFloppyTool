/**
 * @file uft_layouts.h
 * @brief Common floppy on-disk structures (packed).
 */

#ifndef UFT_LAYOUTS_H
#define UFT_LAYOUTS_H

#include <stdint.h>

#if defined(_MSC_VER)
#  pragma pack(push, 1)
#  define UFT_PACKED
#else
#  define UFT_PACKED __attribute__((packed))
#endif

/* =============================================================================
 * IBM PC / Generic MFM
 * ============================================================================= */

typedef struct UFT_PACKED {
    uint8_t cyl;
    uint8_t head;
    uint8_t sec;
    uint8_t size_code; /* 0=>128,1=>256,2=>512,3=>1024 */
} uft_ibm_id_field_t;

typedef struct UFT_PACKED {
    uint8_t mark;      /* 0xFE */
    uft_ibm_id_field_t id;
    uint16_t crc;      /* CRC over 0xFE..size_code */
} uft_ibm_idam_t;

typedef struct UFT_PACKED {
    uint8_t mark;      /* 0xFB (data) or 0xF8 (deleted) */
    /* data follows */
} uft_ibm_dam_t;

/* =============================================================================
 * AmigaDOS MFM
 * ============================================================================= */

typedef struct UFT_PACKED {
    uint32_t info;
    uint32_t checksum;
    uint32_t sector_label[4]; /* 16 bytes label */
} uft_amiga_header_t;

/* =============================================================================
 * Apple II (GCR) - high level record descriptors (WOZ/A2R carry flux)
 * ============================================================================= */

typedef struct UFT_PACKED {
    uint8_t volume;
    uint8_t track;
    uint8_t sector;
    uint8_t checksum;
} uft_apple2_addr_field_t;

/* =============================================================================
 * Commodore 1541 (GCR) - high level
 * ============================================================================= */

typedef struct UFT_PACKED {
    uint8_t header_id;
    uint8_t sector;
    uint8_t track;
    uint8_t disk_id1;
    uint8_t disk_id2;
    uint8_t checksum;
} uft_c64_header_t;

#if defined(_MSC_VER)
#  pragma pack(pop)
#endif

#undef UFT_PACKED

#endif /* UFT_LAYOUTS_H */
