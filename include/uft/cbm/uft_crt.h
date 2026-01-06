#pragma once
/*
 * uft_crt.h - Commodore cartridge .CRT container parsing (C11).
 *
 * NOTE: .CRT is a cartridge container format (emulator/interchange), not a floppy image.
 * It is still useful for a "C64 media" toolchain because it carries structured ROM mapping
 * (banks, load addresses, hardware type).
 *
 * Design goals:
 *  - strict bounds checking (no UB)
 *  - no dynamic allocation in core API
 *  - iterate CHIP packets safely
 */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_CRT_MAGIC "C64 CARTRIDGE   "
#define UFT_CRT_MAGIC_LEN 16
#define UFT_CRT_CHIP_MAGIC "CHIP"
#define UFT_CRT_CHIP_MAGIC_LEN 4

typedef enum uft_crt_status {
    UFT_CRT_OK = 0,
    UFT_CRT_E_INVALID = 1,
    UFT_CRT_E_TRUNC = 2,
    UFT_CRT_E_MAGIC = 3,
    UFT_CRT_E_HEADER = 4,
    UFT_CRT_E_CHIP = 5
} uft_crt_status_t;

/* Big-endian CRT fields */
typedef struct uft_crt_header {
    uint8_t  magic[UFT_CRT_MAGIC_LEN]; /* "C64 CARTRIDGE   " */
    uint32_t header_len;               /* bytes, typically 0x40 */
    uint16_t version;                  /* e.g. 0x0100 */
    uint16_t hw_type;                  /* hardware type id */
    uint8_t  exrom;                    /* 0/1 */
    uint8_t  game;                     /* 0/1 */
    uint8_t  reserved[6];
    char     name[32];                 /* null/space padded */
    uint8_t  reserved2[32];
} uft_crt_header_t;

/* CHIP packet header (big-endian fields) */
typedef struct uft_crt_chip_header {
    uint8_t  magic[UFT_CRT_CHIP_MAGIC_LEN]; /* "CHIP" */
    uint32_t packet_len;                    /* total packet length incl. this header */
    uint16_t chip_type;                     /* 0=ROM, 1=RAM, others exist */
    uint16_t bank;                          /* bank number */
    uint16_t load_addr;                     /* C64 address */
    uint16_t rom_len;                       /* bytes of data following header */
} uft_crt_chip_header_t;

typedef struct uft_crt_view {
    const uint8_t *blob;
    size_t blob_len;

    uft_crt_header_t hdr;
    size_t chip_off; /* offset to first CHIP packet */
} uft_crt_view_t;

typedef struct uft_crt_chip_view {
    uft_crt_chip_header_t chip_hdr;
    const uint8_t *data; /* points into blob */
    size_t data_len;     /* equals chip_hdr.rom_len */
    size_t packet_off;   /* offset of this CHIP packet in blob */
    size_t packet_len;   /* equals chip_hdr.packet_len */
} uft_crt_chip_view_t;

/* Parse + validate header. */
uft_crt_status_t uft_crt_parse(const uint8_t *blob, size_t blob_len, uft_crt_view_t *out);

/* Iterate CHIP packets safely.
 * - Pass cursor=out->chip_off initially.
 * - On success: fills chip_out and advances *cursor to next packet.
 * - Returns UFT_CRT_OK while packets exist; returns UFT_CRT_E_TRUNC when cursor == blob_len (done).
 */
uft_crt_status_t uft_crt_next_chip(const uft_crt_view_t *crt, size_t *cursor, uft_crt_chip_view_t *chip_out);

#ifdef __cplusplus
}
#endif
