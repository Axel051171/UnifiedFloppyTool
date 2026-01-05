/**
 * @file uft_apple_codecs.h
 * @brief Apple II Encoding/Decoding (4-and-4, 5-and-3, 6-and-2)
 * 
 * Basierend auf FC5025 Driver:
 * - endec_4and4.c (Address Field Encoding)
 * - endec_5and3.c (DOS 3.2 13-sector)
 * - endec_6and2.c (DOS 3.3 16-sector)
 * 
 * Referenzen:
 * - "Understanding the Apple II"
 * - "Beneath Apple DOS"
 * - Apple Assembly Lines (March/May 1981)
 * 
 * @version 1.0.0
 */

#ifndef UFT_APPLE_CODECS_H
#define UFT_APPLE_CODECS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 4-and-4 Encoding (Apple Address Fields)
 *============================================================================*/

/**
 * @brief Enkodiere ein Byte mit 4-and-4
 * @param out Output (2 Bytes)
 * @param in Input-Byte
 */
static inline void apple_4and4_encode_byte(uint8_t *out, uint8_t in)
{
    out[0] = 0;
    out[1] = 0;
    
    for (int i = 0; i < 4; i++) {
        out[0] <<= 2;
        out[1] <<= 2;
        out[0] |= 2;  /* Clock */
        out[1] |= 2;  /* Clock */
        if (in & 0x80) out[0] |= 1;  /* High bits */
        if (in & 0x40) out[1] |= 1;  /* Low bits */
        in <<= 2;
    }
}

/**
 * @brief Dekodiere 4-and-4 zu einem Byte
 * @param in Input (2 Bytes)
 * @return Dekodiertes Byte
 */
static inline uint8_t apple_4and4_decode_byte(const uint8_t *in)
{
    return ((in[0] << 1) | 1) & in[1];
}

/**
 * @brief Enkodiere Block mit 4-and-4
 * @param out Output (2x count)
 * @param in Input
 * @param count Anzahl Bytes
 */
static inline void apple_4and4_encode(uint8_t *out, const uint8_t *in, size_t count)
{
    while (count--) {
        apple_4and4_encode_byte(out, *in);
        out += 2;
        in++;
    }
}

/**
 * @brief Dekodiere Block von 4-and-4
 * @param out Output
 * @param in Input (2x count)
 * @param count Anzahl Output-Bytes
 */
static inline void apple_4and4_decode(uint8_t *out, const uint8_t *in, size_t count)
{
    while (count--) {
        *out = apple_4and4_decode_byte(in);
        out++;
        in += 2;
    }
}

/*============================================================================
 * 6-and-2 Encoding (DOS 3.3, ProDOS)
 *============================================================================*/

/**
 * @brief 6-and-2 Decode Tabelle (GCR Nibble → 6-bit Wert)
 * 0xFF = ungültiges Nibble
 */
static const uint8_t apple_6and2_decode_table[256] = {
    /* 0x00-0x8F: ungültig */
    [0x00 ... 0x8F] = 0xFF,
    /* 0x90-0x9F */
    [0x90] = 0xFF, [0x91] = 0xFF, [0x92] = 0xFF, [0x93] = 0xFF,
    [0x94] = 0xFF, [0x95] = 0xFF, [0x96] = 0x00, [0x97] = 0x04,
    [0x98] = 0xFF, [0x99] = 0xFF, [0x9A] = 0x08, [0x9B] = 0x0C,
    [0x9C] = 0xFF, [0x9D] = 0x10, [0x9E] = 0x14, [0x9F] = 0x18,
    /* 0xA0-0xAF */
    [0xA0] = 0xFF, [0xA1] = 0xFF, [0xA2] = 0xFF, [0xA3] = 0xFF,
    [0xA4] = 0xFF, [0xA5] = 0xFF, [0xA6] = 0x1C, [0xA7] = 0x20,
    [0xA8] = 0xFF, [0xA9] = 0xFF, [0xAA] = 0xFF, [0xAB] = 0x24,
    [0xAC] = 0x28, [0xAD] = 0x2C, [0xAE] = 0x30, [0xAF] = 0x34,
    /* 0xB0-0xBF */
    [0xB0] = 0xFF, [0xB1] = 0xFF, [0xB2] = 0x38, [0xB3] = 0x3C,
    [0xB4] = 0x40, [0xB5] = 0x44, [0xB6] = 0x48, [0xB7] = 0x4C,
    [0xB8] = 0xFF, [0xB9] = 0x50, [0xBA] = 0x54, [0xBB] = 0x58,
    [0xBC] = 0x5C, [0xBD] = 0x60, [0xBE] = 0x64, [0xBF] = 0x68,
    /* 0xC0-0xCF */
    [0xC0] = 0xFF, [0xC1] = 0xFF, [0xC2] = 0xFF, [0xC3] = 0xFF,
    [0xC4] = 0xFF, [0xC5] = 0xFF, [0xC6] = 0xFF, [0xC7] = 0xFF,
    [0xC8] = 0xFF, [0xC9] = 0xFF, [0xCA] = 0xFF, [0xCB] = 0x6C,
    [0xCC] = 0xFF, [0xCD] = 0x70, [0xCE] = 0x74, [0xCF] = 0x78,
    /* 0xD0-0xDF */
    [0xD0] = 0xFF, [0xD1] = 0xFF, [0xD2] = 0xFF, [0xD3] = 0x7C,
    [0xD4] = 0xFF, [0xD5] = 0xFF, [0xD6] = 0x80, [0xD7] = 0x84,
    [0xD8] = 0xFF, [0xD9] = 0x88, [0xDA] = 0x8C, [0xDB] = 0x90,
    [0xDC] = 0x94, [0xDD] = 0x98, [0xDE] = 0x9C, [0xDF] = 0xA0,
    /* 0xE0-0xEF */
    [0xE0] = 0xFF, [0xE1] = 0xFF, [0xE2] = 0xFF, [0xE3] = 0xFF,
    [0xE4] = 0xFF, [0xE5] = 0xA4, [0xE6] = 0xA8, [0xE7] = 0xAC,
    [0xE8] = 0xFF, [0xE9] = 0xB0, [0xEA] = 0xB4, [0xEB] = 0xB8,
    [0xEC] = 0xBC, [0xED] = 0xC0, [0xEE] = 0xC4, [0xEF] = 0xC8,
    /* 0xF0-0xFF */
    [0xF0] = 0xFF, [0xF1] = 0xFF, [0xF2] = 0xCC, [0xF3] = 0xD0,
    [0xF4] = 0xD4, [0xF5] = 0xD8, [0xF6] = 0xDC, [0xF7] = 0xE0,
    [0xF8] = 0xFF, [0xF9] = 0xE4, [0xFA] = 0xE8, [0xFB] = 0xEC,
    [0xFC] = 0xF0, [0xFD] = 0xF4, [0xFE] = 0xF8, [0xFF] = 0xFC,
};

/**
 * @brief Dekodiere Apple 6-and-2 Sektor (256 Bytes)
 * @param out Output (256 Bytes)
 * @param in Input (342 Nibbles + Checksum + Epilog)
 * @return 0 bei Erfolg, non-zero bei Fehler
 */
int apple_6and2_decode_sector(uint8_t *out, const uint8_t *in);

/*============================================================================
 * 5-and-3 Encoding (DOS 3.2)
 *============================================================================*/

/**
 * @brief 5-and-3 Decode Tabelle
 */
static const uint8_t apple_5and3_decode_table[256] = {
    [0x00 ... 0xAA] = 0xFF,
    [0xAB] = 0x00, [0xAC] = 0xFF, [0xAD] = 0x01, [0xAE] = 0x02, [0xAF] = 0x03,
    [0xB0] = 0xFF, [0xB1] = 0xFF, [0xB2] = 0xFF, [0xB3] = 0xFF, [0xB4] = 0xFF,
    [0xB5] = 0x04, [0xB6] = 0x05, [0xB7] = 0x06, [0xB8] = 0xFF, [0xB9] = 0xFF,
    [0xBA] = 0x07, [0xBB] = 0x08, [0xBC] = 0xFF, [0xBD] = 0x09, [0xBE] = 0x0A,
    [0xBF] = 0x0B,
    [0xC0 ... 0xCF] = 0xFF,
    [0xD0] = 0xFF, [0xD1] = 0xFF, [0xD2] = 0xFF, [0xD3] = 0xFF, [0xD4] = 0xFF,
    [0xD5] = 0xFF, [0xD6] = 0x0C, [0xD7] = 0x0D, [0xD8] = 0xFF, [0xD9] = 0xFF,
    [0xDA] = 0x0E, [0xDB] = 0x0F, [0xDC] = 0xFF, [0xDD] = 0x10, [0xDE] = 0x11,
    [0xDF] = 0x12,
    [0xE0] = 0xFF, [0xE1] = 0xFF, [0xE2] = 0xFF, [0xE3] = 0xFF, [0xE4] = 0xFF,
    [0xE5] = 0xFF, [0xE6] = 0xFF, [0xE7] = 0xFF, [0xE8] = 0xFF, [0xE9] = 0xFF,
    [0xEA] = 0x13, [0xEB] = 0x14, [0xEC] = 0xFF, [0xED] = 0x15, [0xEE] = 0x16,
    [0xEF] = 0x17,
    [0xF0] = 0xFF, [0xF1] = 0xFF, [0xF2] = 0xFF, [0xF3] = 0xFF, [0xF4] = 0xFF,
    [0xF5] = 0x18, [0xF6] = 0x19, [0xF7] = 0x1A, [0xF8] = 0xFF, [0xF9] = 0xFF,
    [0xFA] = 0x1B, [0xFB] = 0x1C, [0xFC] = 0xFF, [0xFD] = 0x1D, [0xFE] = 0x1E,
    [0xFF] = 0x1F,
};

/**
 * @brief Dekodiere Apple 5-and-3 Sektor (256 Bytes)
 * @param out Output (256 Bytes)
 * @param in Input (411 Nibbles + Checksum + Epilog)
 * @return 0 bei Erfolg, non-zero bei Fehler
 */
int apple_5and3_decode_sector(uint8_t *out, const uint8_t *in);

/*============================================================================
 * Interleave Tables
 *============================================================================*/

/** DOS 3.3 Logical → Physical Sector Mapping */
static const int apple_dos33_interleave[16] = {
    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
};

/** ProDOS Logical → Physical Sector Mapping */
static const int apple_prodos_interleave[16] = {
    0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
};

/** CP/M (Apple II) Logical → Physical Sector Mapping */
static const int apple_cpm_interleave[16] = {
    0, 6, 12, 3, 9, 15, 5, 11, 2, 8, 14, 4, 10, 1, 7, 13
};

/**
 * @brief Konvertiere logischen zu physischem Sektor (DOS 3.3)
 */
static inline int apple_dos33_to_physical(int logical)
{
    return apple_dos33_interleave[logical & 0x0F];
}

/**
 * @brief Konvertiere logischen zu physischem Sektor (ProDOS)
 */
static inline int apple_prodos_to_physical(int logical)
{
    return apple_prodos_interleave[logical & 0x0F];
}

/*============================================================================
 * Address Field Parsing
 *============================================================================*/

/** Apple Address Field Structure */
typedef struct {
    uint8_t volume;
    uint8_t track;
    uint8_t sector;
    uint8_t checksum;
    bool valid;
} apple_address_field_t;

/**
 * @brief Parse Apple Address Field (nach D5 AA 96)
 * @param addr Output-Struktur
 * @param raw Raw encoded data (8 bytes: 4x 4-and-4 encoded)
 * @return true wenn Checksum gültig
 */
static inline bool apple_parse_address_field(apple_address_field_t *addr, const uint8_t *raw)
{
    uint8_t decoded[4];
    apple_4and4_decode(decoded, raw, 4);
    
    addr->volume = decoded[0];
    addr->track = decoded[1];
    addr->sector = decoded[2];
    addr->checksum = decoded[3];
    addr->valid = (decoded[3] == (decoded[0] ^ decoded[1] ^ decoded[2]));
    
    return addr->valid;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_CODECS_H */
