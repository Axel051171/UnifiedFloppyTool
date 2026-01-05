/**
 * @file uft_fm_decoder.c
 * @brief UnifiedFloppyTool - FM (Single Density) Decoder
 * 
 * FM (Frequency Modulation) ist die älteste Floppy-Kodierung.
 * Jedes Datenbit wird von einem Clock-Bit begleitet → 50% Effizienz.
 * 
 * UNTERSTÜTZTE FORMATE:
 * 
 * IBM-Kompatibel (Standard):
 * - IBM 3740 8" SSSD (26×128, 250 kbps)
 * - CP/M 8" SSSD (26×128, 250 kbps)
 * - Shugart SA800 (26×128, 250 kbps)
 * 
 * DEC:
 * - RX01 (26×128, Standard FM)
 * - RX02 (26×256, FM Header + MFM Data!) ← Hybrid!
 * 
 * TRS-80:
 * - Model I (10×256, 125 kbps, eigene Sync)
 * - Model III/4 (10×256, 125 kbps)
 * 
 * Andere:
 * - Atari 810 (18×128, 125 kbps)
 * - Osborne 1 (10×256, 125 kbps)
 * - Heathkit H-17 (10×256, Hard-Sectored)
 * 
 * ENCODING:
 * - Clock-Puls vor jedem Bit
 * - Data-Puls nur bei '1'
 * - Bitcell: 4µs @ 250kbps, 8µs @ 125kbps
 * 
 * SYNC PATTERNS:
 * - IBM: 0x00 (6×), dann Address Mark
 * - TRS-80: 0x00 (viele), dann 0xFE
 * 
 * ADDRESS MARKS (IBM):
 * - 0xFC: Index Address Mark
 * - 0xFE: ID Address Mark
 * - 0xFB: Data Address Mark
 * - 0xF8: Deleted Data Address Mark
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_decoder_plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// FM Format Variants
// ============================================================================

/**
 * @brief Bekannte FM-Format-Varianten
 */
typedef enum {
    FM_VARIANT_UNKNOWN      = 0,
    
    // IBM-Standard (8")
    FM_VARIANT_IBM_3740     = 1,    ///< IBM 3740 8" SSSD
    FM_VARIANT_IBM_SYSTEM34 = 2,    ///< IBM System/34
    
    // DEC
    FM_VARIANT_DEC_RX01     = 10,   ///< DEC RX01 (Standard FM)
    FM_VARIANT_DEC_RX02     = 11,   ///< DEC RX02 (FM Header + MFM Data)
    
    // TRS-80
    FM_VARIANT_TRS80_MOD1   = 20,   ///< TRS-80 Model I
    FM_VARIANT_TRS80_MOD3   = 21,   ///< TRS-80 Model III/4
    
    // Atari
    FM_VARIANT_ATARI_810    = 30,   ///< Atari 810/1050 SD
    
    // CP/M
    FM_VARIANT_CPM_8_SSSD   = 40,   ///< Standard CP/M 8" SSSD
    
    // Osborne
    FM_VARIANT_OSBORNE      = 50,   ///< Osborne 1
    
    // Heathkit (Hard-Sectored)
    FM_VARIANT_HEATHKIT     = 60,   ///< Heathkit H-17
    
} fm_variant_t;

/**
 * @brief Format-Beschreibung
 */
typedef struct {
    fm_variant_t    variant;
    const char*     name;
    uint8_t         sectors_per_track;
    uint16_t        sector_size;
    uint32_t        data_rate;          ///< Bits pro Sekunde
    uint16_t        gap3_size;          ///< Gap nach Daten
    uint8_t         sync_bytes;         ///< Anzahl Sync-Bytes vor Mark
    uint8_t         id_mark;            ///< ID Address Mark (0xFE standard)
    uint8_t         data_mark;          ///< Data Address Mark (0xFB standard)
    bool            soft_sectored;      ///< true = Soft-Sectored
    bool            hybrid_mfm;         ///< true = FM Header + MFM Data (RX02)
} fm_format_info_t;

// Format-Tabelle
static const fm_format_info_t fm_formats[] = {
    // IBM 8" Standard
    {
        .variant = FM_VARIANT_IBM_3740,
        .name = "IBM 3740 8\" SSSD",
        .sectors_per_track = 26,
        .sector_size = 128,
        .data_rate = 250000,
        .gap3_size = 27,
        .sync_bytes = 6,
        .id_mark = 0xFE,
        .data_mark = 0xFB,
        .soft_sectored = true,
        .hybrid_mfm = false
    },
    // CP/M 8"
    {
        .variant = FM_VARIANT_CPM_8_SSSD,
        .name = "CP/M 8\" SSSD",
        .sectors_per_track = 26,
        .sector_size = 128,
        .data_rate = 250000,
        .gap3_size = 27,
        .sync_bytes = 6,
        .id_mark = 0xFE,
        .data_mark = 0xFB,
        .soft_sectored = true,
        .hybrid_mfm = false
    },
    // DEC RX01
    {
        .variant = FM_VARIANT_DEC_RX01,
        .name = "DEC RX01",
        .sectors_per_track = 26,
        .sector_size = 128,
        .data_rate = 250000,
        .gap3_size = 27,
        .sync_bytes = 6,
        .id_mark = 0xFE,
        .data_mark = 0xFB,
        .soft_sectored = true,
        .hybrid_mfm = false
    },
    // DEC RX02 (Hybrid!)
    {
        .variant = FM_VARIANT_DEC_RX02,
        .name = "DEC RX02 (Hybrid FM/MFM)",
        .sectors_per_track = 26,
        .sector_size = 256,
        .data_rate = 250000,
        .gap3_size = 27,
        .sync_bytes = 6,
        .id_mark = 0xFE,
        .data_mark = 0xFD,  // RX02 verwendet 0xFD für MFM-Daten!
        .soft_sectored = true,
        .hybrid_mfm = true
    },
    // TRS-80 Model I
    {
        .variant = FM_VARIANT_TRS80_MOD1,
        .name = "TRS-80 Model I",
        .sectors_per_track = 10,
        .sector_size = 256,
        .data_rate = 125000,
        .gap3_size = 11,
        .sync_bytes = 6,
        .id_mark = 0xFE,
        .data_mark = 0xFB,
        .soft_sectored = true,
        .hybrid_mfm = false
    },
    // TRS-80 Model III
    {
        .variant = FM_VARIANT_TRS80_MOD3,
        .name = "TRS-80 Model III/4",
        .sectors_per_track = 10,
        .sector_size = 256,
        .data_rate = 125000,
        .gap3_size = 11,
        .sync_bytes = 6,
        .id_mark = 0xFE,
        .data_mark = 0xFB,
        .soft_sectored = true,
        .hybrid_mfm = false
    },
    // Atari 810
    {
        .variant = FM_VARIANT_ATARI_810,
        .name = "Atari 810 SD",
        .sectors_per_track = 18,
        .sector_size = 128,
        .data_rate = 125000,
        .gap3_size = 10,
        .sync_bytes = 5,
        .id_mark = 0xFE,
        .data_mark = 0xFB,
        .soft_sectored = true,
        .hybrid_mfm = false
    },
    // Osborne 1
    {
        .variant = FM_VARIANT_OSBORNE,
        .name = "Osborne 1",
        .sectors_per_track = 10,
        .sector_size = 256,
        .data_rate = 125000,
        .gap3_size = 12,
        .sync_bytes = 6,
        .id_mark = 0xFE,
        .data_mark = 0xFB,
        .soft_sectored = true,
        .hybrid_mfm = false
    },
    // Ende-Marker
    {
        .variant = FM_VARIANT_UNKNOWN,
        .name = NULL
    }
};

// ============================================================================
// FM Timing Constants
// ============================================================================

// Bitcell-Zeiten (Nanosekunden)
#define FM_BITCELL_250K     4000    // 4µs @ 250 kbps
#define FM_BITCELL_125K     8000    // 8µs @ 125 kbps

// Clock/Data Pulse-Positionen innerhalb Bitcell
// FM: Clock bei 0%, Data bei 50% der Bitcell
#define FM_CLOCK_POS        0.0
#define FM_DATA_POS         0.5

// Toleranz für PLL
#define FM_PLL_TOLERANCE    0.25    // ±25%

// Address Marks (mit Clock-Violations)
#define FM_MARK_INDEX       0xFC    // Index Address Mark
#define FM_MARK_ID          0xFE    // ID Address Mark
#define FM_MARK_DATA        0xFB    // Data Address Mark
#define FM_MARK_DELETED     0xF8    // Deleted Data Address Mark
#define FM_MARK_RX02_MFM    0xFD    // RX02 MFM Data Mark

// ============================================================================
// CRC-16 (CCITT)
// ============================================================================

static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static uint16_t fm_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

// ============================================================================
// FM Decoder State
// ============================================================================

typedef struct {
    // PLL State
    double      pll_period;         ///< Aktuelle Bitcell-Periode
    double      pll_phase;          ///< Aktuelle Phase
    double      pll_freq;           ///< Aktuelle Frequenz
    
    // Decode State
    uint16_t    shift_reg;          ///< Schieberegister für Clock+Data
    uint8_t     bit_count;          ///< Gezählte Bits
    bool        in_sync;            ///< In Sync mit Clock?
    
    // Sector State
    bool        found_sync;
    bool        reading_id;
    bool        reading_data;
    size_t      bytes_read;
    
    // Current Sector Info
    uint8_t     sector_track;
    uint8_t     sector_side;
    uint8_t     sector_num;
    uint8_t     sector_size_code;
    
    // Format Detection
    fm_variant_t detected_variant;
    uint32_t    detected_rate;
    
} fm_decoder_state_t;

// ============================================================================
// FM Bit Extraction
// ============================================================================

/**
 * @brief Extrahiert Datenbyte aus FM-kodiertem Bitstream
 * 
 * FM: C D C D C D C D C D C D C D C D (16 bits → 8 data bits)
 *     0 0 0 1 0 2 0 3 0 4 0 5 0 6 0 7
 * 
 * C = Clock (immer 1), D = Data
 */
static uint8_t fm_extract_byte(uint16_t raw) {
    uint8_t result = 0;
    
    // Jedes zweite Bit ist Data (ungerade Positionen)
    for (int i = 0; i < 8; i++) {
        if (raw & (1 << (14 - i * 2))) {
            result |= (1 << (7 - i));
        }
    }
    
    return result;
}

/**
 * @brief Enkodiert Byte zu FM
 */
static uint16_t fm_encode_byte(uint8_t data) {
    uint16_t result = 0;
    
    for (int i = 0; i < 8; i++) {
        // Clock-Bit (immer 1)
        result |= (1 << (15 - i * 2));
        
        // Data-Bit
        if (data & (1 << (7 - i))) {
            result |= (1 << (14 - i * 2));
        }
    }
    
    return result;
}

// ============================================================================
// PLL (Phase-Locked Loop)
// ============================================================================

/**
 * @brief PLL für FM-Dekodierung
 * 
 * FM hat regelmäßige Clock-Pulse, was die PLL einfacher macht als bei MFM.
 */
static void fm_pll_process(fm_decoder_state_t* state, 
                           uint32_t flux_ns,
                           uint8_t* bit_out, bool* valid) {
    *valid = false;
    
    // Erwartete Intervalle bei FM:
    // - 2µs: Clock-to-Clock (nur Clock, kein Data) → Bit 0
    // - 1µs + 1µs: Clock-Data-Clock → Bit 1
    
    double interval = (double)flux_ns;
    double expected = state->pll_period / 2.0;  // Halbe Bitcell für Clock
    
    // Anzahl Pulses in diesem Intervall
    double ratio = interval / expected;
    int pulses = (int)(ratio + 0.5);
    
    if (pulses < 1) pulses = 1;
    if (pulses > 4) pulses = 4;
    
    // PLL Anpassung
    double error = interval - (pulses * expected);
    state->pll_period += error * 0.05;  // Sanfte Korrektur
    
    // Bit Output basierend auf Pulse-Muster
    if (pulses == 1) {
        // Kurzes Intervall: Clock + Data (Bit 1) oder nur Data-Teil
        state->shift_reg = (state->shift_reg << 1) | 1;
        state->bit_count++;
        *valid = true;
        *bit_out = 1;
    } else if (pulses == 2) {
        // Standard Clock-Intervall: Bit 0
        state->shift_reg = (state->shift_reg << 1);
        state->bit_count++;
        *valid = true;
        *bit_out = 0;
    }
    
    // Byte-Grenze prüfen
    if (state->bit_count >= 16) {
        state->bit_count = 0;
    }
}

// ============================================================================
// Address Mark Detection
// ============================================================================

/**
 * @brief Sucht nach FM Sync-Sequenz (6× 0x00)
 */
static bool fm_find_sync(const uint8_t* data, size_t len, size_t* pos) {
    int zero_count = 0;
    
    for (size_t i = *pos; i < len; i++) {
        if (data[i] == 0x00) {
            zero_count++;
            if (zero_count >= 6) {
                *pos = i + 1;
                return true;
            }
        } else {
            zero_count = 0;
        }
    }
    
    return false;
}

/**
 * @brief Erkennt Address Mark nach Sync
 */
static uint8_t fm_detect_mark(const uint8_t* data, size_t len, size_t pos) {
    if (pos >= len) return 0;
    
    uint8_t mark = data[pos];
    
    // Bekannte Address Marks
    switch (mark) {
        case FM_MARK_INDEX:
        case FM_MARK_ID:
        case FM_MARK_DATA:
        case FM_MARK_DELETED:
        case FM_MARK_RX02_MFM:
            return mark;
        default:
            return 0;
    }
}

// ============================================================================
// Sector Parsing
// ============================================================================

/**
 * @brief Parst FM Sector Header (ID Field)
 * 
 * Format: [IDAM] [Track] [Side] [Sector] [Size] [CRC-Hi] [CRC-Lo]
 */
static bool fm_parse_sector_id(const uint8_t* data, size_t len,
                                uint8_t* track, uint8_t* side,
                                uint8_t* sector, uint8_t* size_code,
                                bool* crc_ok) {
    // Mindestens 7 Bytes: Mark + 4 ID + 2 CRC
    if (len < 7) return false;
    
    // Prüfe ID Address Mark
    if (data[0] != FM_MARK_ID) return false;
    
    *track = data[1];
    *side = data[2];
    *sector = data[3];
    *size_code = data[4];
    
    // CRC prüfen (über Mark + ID)
    uint16_t calc_crc = fm_crc16(data, 5);
    uint16_t read_crc = (data[5] << 8) | data[6];
    
    *crc_ok = (calc_crc == read_crc);
    
    return true;
}

/**
 * @brief Parst FM Sector Data
 * 
 * Format: [DAM] [Data...] [CRC-Hi] [CRC-Lo]
 */
static bool fm_parse_sector_data(const uint8_t* data, size_t len,
                                  uint16_t sector_size,
                                  uint8_t* data_out,
                                  bool* crc_ok, bool* deleted) {
    // Mindestens Mark + Data + CRC
    if (len < sector_size + 3) return false;
    
    // Data Mark prüfen
    if (data[0] == FM_MARK_DATA) {
        *deleted = false;
    } else if (data[0] == FM_MARK_DELETED) {
        *deleted = true;
    } else if (data[0] == FM_MARK_RX02_MFM) {
        *deleted = false;  // RX02 MFM-Daten
        // TODO: MFM-Dekodierung für Daten!
    } else {
        return false;
    }
    
    // Daten kopieren
    memcpy(data_out, &data[1], sector_size);
    
    // CRC prüfen
    uint16_t calc_crc = fm_crc16(data, 1 + sector_size);
    uint16_t read_crc = (data[1 + sector_size] << 8) | data[2 + sector_size];
    
    *crc_ok = (calc_crc == read_crc);
    
    return true;
}

// ============================================================================
// Format Auto-Detection
// ============================================================================

/**
 * @brief Erkennt FM-Format anhand von Track-Daten
 * 
 * Analysiert:
 * - Anzahl Sektoren
 * - Sektorgröße
 * - Bitrate (aus Timing)
 * - Spezielle Marker (RX02, TRS-80, etc.)
 */
static fm_variant_t fm_detect_format(const uint8_t* track_data, size_t len,
                                      uint32_t data_rate,
                                      int* confidence) {
    *confidence = 0;
    
    // Statistiken sammeln
    int sector_count = 0;
    int sector_sizes[4] = {0, 0, 0, 0};  // 128, 256, 512, 1024
    bool found_rx02_mark = false;
    
    size_t pos = 0;
    while (fm_find_sync(track_data, len, &pos)) {
        uint8_t mark = fm_detect_mark(track_data, len, pos);
        
        if (mark == FM_MARK_ID && pos + 7 <= len) {
            uint8_t track, side, sector, size_code;
            bool crc_ok;
            
            if (fm_parse_sector_id(&track_data[pos], len - pos,
                                   &track, &side, &sector, &size_code, &crc_ok)) {
                sector_count++;
                if (size_code < 4) {
                    sector_sizes[size_code]++;
                }
            }
        } else if (mark == FM_MARK_RX02_MFM) {
            found_rx02_mark = true;
        }
        
        pos++;
    }
    
    // Format erkennen
    
    // RX02 Hybrid?
    if (found_rx02_mark && sector_count >= 20 && sector_sizes[1] > 0) {
        *confidence = 90;
        return FM_VARIANT_DEC_RX02;
    }
    
    // IBM 8" Standard (26 Sektoren × 128 Bytes)?
    if (sector_count >= 24 && sector_count <= 26 && sector_sizes[0] > 20) {
        if (data_rate >= 200000) {
            *confidence = 85;
            return FM_VARIANT_IBM_3740;
        }
    }
    
    // TRS-80 (10 Sektoren × 256 Bytes @ 125 kbps)?
    if (sector_count >= 9 && sector_count <= 10 && sector_sizes[1] >= 8) {
        if (data_rate <= 150000) {
            *confidence = 80;
            return FM_VARIANT_TRS80_MOD1;
        }
    }
    
    // Atari 810 (18 Sektoren × 128 Bytes)?
    if (sector_count >= 17 && sector_count <= 18 && sector_sizes[0] >= 15) {
        *confidence = 75;
        return FM_VARIANT_ATARI_810;
    }
    
    // Osborne (10 Sektoren × 256 Bytes)?
    if (sector_count >= 9 && sector_count <= 10 && sector_sizes[1] >= 8) {
        *confidence = 70;
        return FM_VARIANT_OSBORNE;
    }
    
    // Default: IBM-kompatibel
    *confidence = 50;
    return FM_VARIANT_IBM_3740;
}

/**
 * @brief Erkennt FM anhand von Flux-Timing
 */
static bool fm_detect_from_flux(const uint32_t* flux, size_t count,
                                 uint32_t* detected_rate,
                                 int* confidence) {
    if (!flux || count < 100) return false;
    
    // Histogram der Intervalle erstellen
    int hist_4us = 0;   // ~4µs (250 kbps)
    int hist_8us = 0;   // ~8µs (125 kbps)
    int hist_2us = 0;   // ~2µs (zwischen Clock und Data)
    
    for (size_t i = 0; i < count && i < 10000; i++) {
        uint32_t ns = flux[i];
        
        if (ns >= 1500 && ns <= 2500) {
            hist_2us++;
        } else if (ns >= 3500 && ns <= 4500) {
            hist_4us++;
        } else if (ns >= 7000 && ns <= 9000) {
            hist_8us++;
        }
    }
    
    int total = hist_2us + hist_4us + hist_8us;
    if (total < 50) {
        return false;  // Nicht genug erkennbare Intervalle
    }
    
    // FM hat charakteristisches Muster: viele 2µs und 4µs Intervalle
    float fm_ratio = (float)(hist_2us + hist_4us) / total;
    
    if (fm_ratio > 0.7) {
        // Wahrscheinlich FM
        if (hist_8us > hist_4us) {
            *detected_rate = 125000;  // 125 kbps (TRS-80, Atari)
        } else {
            *detected_rate = 250000;  // 250 kbps (IBM 8")
        }
        
        *confidence = (int)(fm_ratio * 100);
        return true;
    }
    
    return false;
}

// ============================================================================
// Plugin Interface
// ============================================================================

static bool fm_detect(const uint32_t* flux, size_t count, int* confidence) {
    uint32_t rate;
    return fm_detect_from_flux(flux, count, &rate, confidence);
}

static uft_error_t fm_decode_flux(const uint32_t* flux, size_t flux_count,
                                   const uft_decode_options_t* options,
                                   uft_sector_t* sectors, size_t max_sectors,
                                   size_t* sector_count,
                                   uft_decode_stats_t* stats) {
    if (!flux || !sectors || !sector_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *sector_count = 0;
    
    // Defaults
    uft_decode_options_t opts = options ? *options : uft_default_decode_options();
    
    // Decoder State initialisieren
    fm_decoder_state_t state = {0};
    
    // Bitrate bestimmen
    if (opts.pll_period_ns > 0) {
        state.pll_period = opts.pll_period_ns;
        state.detected_rate = (uint32_t)(1000000000.0 / opts.pll_period_ns);
    } else {
        // Auto-Detect
        uint32_t detected_rate;
        int confidence;
        if (fm_detect_from_flux(flux, flux_count, &detected_rate, &confidence)) {
            state.pll_period = 1000000000.0 / detected_rate;
            state.detected_rate = detected_rate;
        } else {
            state.pll_period = FM_BITCELL_250K;  // Default 250 kbps
            state.detected_rate = 250000;
        }
    }
    
    // Rohdaten dekodieren (Flux → Bits → Bytes)
    size_t raw_size = flux_count * 2;  // Geschätzte Größe
    uint8_t* raw_data = calloc(raw_size, 1);
    if (!raw_data) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    size_t raw_pos = 0;
    uint8_t current_byte = 0;
    int bit_pos = 0;
    
    for (size_t i = 0; i < flux_count && raw_pos < raw_size; i++) {
        uint8_t bit;
        bool valid;
        
        fm_pll_process(&state, flux[i], &bit, &valid);
        
        if (valid) {
            current_byte = (current_byte << 1) | bit;
            bit_pos++;
            
            if (bit_pos >= 8) {
                raw_data[raw_pos++] = current_byte;
                current_byte = 0;
                bit_pos = 0;
            }
        }
    }
    
    // Format erkennen
    int format_confidence;
    fm_variant_t variant = fm_detect_format(raw_data, raw_pos, 
                                             state.detected_rate,
                                             &format_confidence);
    state.detected_variant = variant;
    
    // Format-Info holen
    const fm_format_info_t* format = NULL;
    for (const fm_format_info_t* f = fm_formats; f->name; f++) {
        if (f->variant == variant) {
            format = f;
            break;
        }
    }
    
    if (!format) {
        format = &fm_formats[0];  // Default: IBM 3740
    }
    
    // Sektoren parsen
    size_t pos = 0;
    while (*sector_count < max_sectors && fm_find_sync(raw_data, raw_pos, &pos)) {
        uint8_t mark = fm_detect_mark(raw_data, raw_pos, pos);
        
        if (mark == FM_MARK_ID && pos + 7 <= raw_pos) {
            uint8_t track, side, sector_num, size_code;
            bool id_crc_ok;
            
            if (fm_parse_sector_id(&raw_data[pos], raw_pos - pos,
                                   &track, &side, &sector_num, &size_code,
                                   &id_crc_ok)) {
                
                // Sektorgröße berechnen
                uint16_t sector_size = 128 << size_code;
                
                // Nach Data Mark suchen
                size_t data_pos = pos + 7;
                bool found_data = false;
                
                for (size_t gap = 0; gap < 50 && data_pos + gap < raw_pos; gap++) {
                    if (raw_data[data_pos + gap] == FM_MARK_DATA ||
                        raw_data[data_pos + gap] == FM_MARK_DELETED ||
                        raw_data[data_pos + gap] == FM_MARK_RX02_MFM) {
                        data_pos += gap;
                        found_data = true;
                        break;
                    }
                }
                
                if (found_data && data_pos + sector_size + 3 <= raw_pos) {
                    uft_sector_t* sec = &sectors[*sector_count];
                    memset(sec, 0, sizeof(*sec));
                    
                    sec->id.cylinder = track;
                    sec->id.head = side;
                    sec->id.sector = sector_num;
                    sec->id.size_code = size_code;
                    sec->data_size = sector_size;
                    sec->data = malloc(sector_size);
                    
                    if (sec->data) {
                        bool data_crc_ok, deleted;
                        
                        if (fm_parse_sector_data(&raw_data[data_pos], 
                                                  raw_pos - data_pos,
                                                  sector_size,
                                                  sec->data,
                                                  &data_crc_ok, &deleted)) {
                            
                            sec->status = 0;
                            if (!id_crc_ok) sec->status |= UFT_SECTOR_ID_CRC_ERROR;
                            if (!data_crc_ok) sec->status |= UFT_SECTOR_CRC_ERROR;
                            if (deleted) sec->status |= UFT_SECTOR_DELETED;
                            
                            (*sector_count)++;
                        } else {
                            free(sec->data);
                            sec->data = NULL;
                        }
                    }
                }
                
                pos = data_pos + sector_size + 3;
            }
        }
        
        pos++;
    }
    
    // Stats
    if (stats) {
        stats->flux_transitions = (uint32_t)flux_count;
        stats->sectors_found = (uint32_t)*sector_count;
        stats->sectors_ok = 0;
        stats->sectors_bad_crc = 0;
        
        for (size_t i = 0; i < *sector_count; i++) {
            if (sectors[i].status == 0) {
                stats->sectors_ok++;
            } else {
                stats->sectors_bad_crc++;
            }
        }
        
        stats->data_rate_bps = (double)state.detected_rate;
    }
    
    free(raw_data);
    return UFT_OK;
}

static double fm_get_data_rate(uft_geometry_preset_t preset) {
    (void)preset;  // Unused for FM - always 250kbps
    return 250000.0;  // Default 250 kbps for FM
}

static void fm_get_default_gaps(uft_geometry_preset_t preset,
                                 uint16_t* gap1, uint16_t* gap2, 
                                 uint16_t* gap3, uint16_t* gap4) {
    (void)preset;  // Use same gaps for all FM presets
    if (gap1) *gap1 = 40;   // Gap vor Track (IBM)
    if (gap2) *gap2 = 26;   // Gap zwischen ID und Data
    if (gap3) *gap3 = 27;   // Gap nach Data
    if (gap4) *gap4 = 0;    // No gap4 for FM
}

// ============================================================================
// Plugin Registration
// ============================================================================

static const uft_decoder_plugin_t fm_decoder_plugin = {
    .name = "FM (Single Density)",
    .version = 0x0100,
    .encoding = UFT_ENC_FM,
    .capabilities = UFT_DECODER_CAP_DECODE | UFT_DECODER_CAP_AUTO_DETECT,
    
    .detect = fm_detect,
    .decode = fm_decode_flux,
    .encode = NULL,  // TODO
    
    .get_data_rate = fm_get_data_rate,
    .get_default_gaps = fm_get_default_gaps,
    
    .private_data = NULL
};

uft_error_t uft_register_fm_decoder(void) {
    return uft_register_decoder_plugin(&fm_decoder_plugin);
}

// Externe Referenz für Plugin-Registrierung
const uft_decoder_plugin_t uft_decoder_plugin_fm_instance = {
    .name = "FM (Single Density)",
    .version = 0x0100,
    .encoding = UFT_ENC_FM,
    .capabilities = UFT_DECODER_CAP_DECODE | UFT_DECODER_CAP_AUTO_DETECT,
    
    .detect = fm_detect,
    .decode = fm_decode_flux,
    .encode = NULL,
    
    .get_data_rate = fm_get_data_rate,
    .get_default_gaps = fm_get_default_gaps,
    
    .private_data = NULL
};

// ============================================================================
// Format Query Functions
// ============================================================================

/**
 * @brief Gibt Info zu erkanntem Format zurück
 */
const char* fm_get_variant_name(fm_variant_t variant) {
    for (const fm_format_info_t* f = fm_formats; f->name; f++) {
        if (f->variant == variant) {
            return f->name;
        }
    }
    return "Unknown FM Format";
}

/**
 * @brief Gibt alle unterstützten Formate aus
 */
void fm_print_supported_formats(void) {
    printf("FM (Single Density) Supported Formats:\n");
    printf("======================================\n\n");
    
    for (const fm_format_info_t* f = fm_formats; f->name; f++) {
        printf("  %-25s %2d×%4d  %6d bps  %s\n",
               f->name,
               f->sectors_per_track,
               f->sector_size,
               f->data_rate,
               f->hybrid_mfm ? "(Hybrid FM/MFM)" : "");
    }
    
    printf("\n");
}
