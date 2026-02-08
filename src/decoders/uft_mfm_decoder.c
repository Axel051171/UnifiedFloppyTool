/**
 * @file uft_mfm_decoder.c
 * @brief UnifiedFloppyTool - IBM MFM Decoder Plugin
 * 
 * Dekodiert IBM-kompatibles MFM (Modified Frequency Modulation):
 * - PC 3.5"/5.25" DD/HD
 * - Standard Sync: 0x4489 (drei $A1 mit Clock violation)
 * - Sector format: IDAM + Data + CRC
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_decoder_plugin.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// MFM Constants
// ============================================================================

#define MFM_SYNC_WORD       0x4489      // A1 mit Clock-Fehler
#define MFM_SYNC_COUNT      3           // Dreimal $A1
#define MFM_IDAM_MARK       0xFE        // ID Address Mark
#define MFM_DAM_MARK        0xFB        // Data Address Mark
#define MFM_DDAM_MARK       0xF8        // Deleted Data Address Mark

#define MFM_DD_BITRATE      250000      // 250 kbit/s DD
#define MFM_HD_BITRATE      500000      // 500 kbit/s HD

#define MFM_DD_BIT_TIME_NS  4000        // 4µs
#define MFM_HD_BIT_TIME_NS  2000        // 2µs

// ============================================================================
// PLL State
// ============================================================================

typedef struct {
    double      period;         // Aktuelle Bit-Periode
    double      nominal;        // Nominale Periode
    double      adjust;         // Anpassungsrate
    double      phase;          // Phase-Akkumulator
    uint32_t    bitbuf;         // Bit-Puffer für Sync-Erkennung
    int         bitcount;       // Bits im Puffer
} mfm_pll_t;

static void pll_init(mfm_pll_t* pll, double nominal_ns) {
    pll->period = nominal_ns;
    pll->nominal = nominal_ns;
    pll->adjust = 0.05;  // 5% Anpassung
    pll->phase = 0;
    pll->bitbuf = 0;
    pll->bitcount = 0;
}

static int pll_process(mfm_pll_t* pll, uint32_t delta_ns) {
    // Wie viele Bit-Perioden?
    double periods = delta_ns / pll->period;
    int bits = (int)(periods + 0.5);
    
    if (bits < 1) bits = 1;
    if (bits > 4) bits = 4;  // Max 4 Nullen
    
    // PLL anpassen
    double error = delta_ns - bits * pll->period;
    pll->period += error * pll->adjust / bits;
    
    // Grenzen
    double min_p = pll->nominal * 0.75;
    double max_p = pll->nominal * 1.25;
    if (pll->period < min_p) pll->period = min_p;
    if (pll->period > max_p) pll->period = max_p;
    
    return bits;
}

// ============================================================================
// MFM Decoding
// ============================================================================

/**
 * @brief Dekodiert MFM-Bits zu Datenbytes
 */
static void mfm_decode_bits(const uint8_t* bits, size_t bit_count,
                            uint8_t* data, size_t* data_len) {
    *data_len = 0;
    
    // MFM: Jedes Datenbit hat ein Clock-Bit davor
    // D0 C1 D1 C2 D2 C3 D3 C4 D4 C5 D5 C6 D6 C7 D7 C0
    // Wir nehmen nur die ungeraden Bits (Daten)
    
    for (size_t i = 0; i + 15 < bit_count; i += 16) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            byte <<= 1;
            if (bits[i + 1 + b * 2]) {  // Ungerade Bits sind Daten
                byte |= 1;
            }
        }
        data[(*data_len)++] = byte;
    }
}

/**
 * @brief Sucht nach Sync-Pattern (3× A1 mit Clock-Fehler)
 */
static bool find_sync(mfm_pll_t* pll, const uint32_t* flux, size_t count,
                      size_t* pos) {
    pll_init(pll, pll->nominal);
    
    for (size_t i = *pos; i < count; i++) {
        int bits = pll_process(pll, flux[i]);
        
        // Bits in Puffer schieben
        for (int b = 0; b < bits - 1; b++) {
            pll->bitbuf = (pll->bitbuf << 1) & 0xFFFFFFFF;
        }
        pll->bitbuf = ((pll->bitbuf << 1) | 1) & 0xFFFFFFFF;
        
        // Sync suchen (0x4489 4489 4489 = A1 A1 A1)
        // 48 Bits = 3 × 16 Bits
        if ((pll->bitbuf & 0xFFFFFFFF) == 0x44894489) {
            // Prüfe ob davor auch 4489 war
            *pos = i + 1;
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Liest Bytes ab aktueller Position
 */
static size_t read_bytes(mfm_pll_t* pll, const uint32_t* flux, size_t count,
                         size_t pos, uint8_t* data, size_t max_bytes) {
    size_t bytes_read = 0;
    uint16_t shiftreg = 0;
    int bits = 0;
    
    for (size_t i = pos; i < count && bytes_read < max_bytes; i++) {
        int num_bits = pll_process(pll, flux[i]);
        
        for (int b = 0; b < num_bits; b++) {
            shiftreg <<= 1;
            if (b == num_bits - 1) {
                shiftreg |= 1;  // Flux = 1
            }
            bits++;
            
            if (bits == 16) {
                // MFM dekodieren: Daten sind in ungeraden Bits
                uint8_t byte = 0;
                for (int d = 0; d < 8; d++) {
                    if (shiftreg & (1 << (14 - d * 2))) {
                        byte |= (1 << (7 - d));
                    }
                }
                data[bytes_read++] = byte;
                bits = 0;
                shiftreg = 0;
            }
        }
    }
    
    return bytes_read;
}

// ============================================================================
// CRC-16 CCITT
// ============================================================================

static uint16_t crc16_ccitt_byte(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static uint16_t crc16_mfm(const uint8_t* data, size_t len) {
    // MFM CRC startet nach drei $A1 Sync-Bytes
    uint16_t crc = 0xFFFF;
    crc = crc16_ccitt_byte(crc, 0xA1);
    crc = crc16_ccitt_byte(crc, 0xA1);
    crc = crc16_ccitt_byte(crc, 0xA1);
    
    for (size_t i = 0; i < len; i++) {
        crc = crc16_ccitt_byte(crc, data[i]);
    }
    
    return crc;
}

// ============================================================================
// Detect
// ============================================================================

static bool mfm_detect(const uint32_t* flux, size_t count, int* confidence) {
    *confidence = 0;
    
    if (count < 1000) return false;
    
    // Durchschnittliche Transition-Zeit berechnen
    uint64_t total = 0;
    for (size_t i = 0; i < count && i < 10000; i++) {
        total += flux[i];
    }
    double avg = (double)total / (count < 10000 ? count : 10000);
    
    // MFM DD: ~4000ns, HD: ~2000ns
    if (avg > 3000 && avg < 5000) {
        *confidence = 70;  // Wahrscheinlich DD MFM
    } else if (avg > 1500 && avg < 2500) {
        *confidence = 70;  // Wahrscheinlich HD MFM
    } else {
        return false;
    }
    
    // Nach Sync suchen
    mfm_pll_t pll;
    pll_init(&pll, avg);
    size_t pos = 0;
    
    int syncs_found = 0;
    for (int i = 0; i < 20; i++) {
        if (find_sync(&pll, flux, count, &pos)) {
            syncs_found++;
        }
    }
    
    if (syncs_found >= 5) {
        *confidence = 90;
    } else if (syncs_found >= 2) {
        *confidence = 75;
    }
    
    return *confidence > 50;
}

// ============================================================================
// Decode
// ============================================================================

static uft_error_t mfm_decode(const uint32_t* flux, size_t count,
                               const uft_decode_options_t* options,
                               uft_sector_t* sectors, size_t max_sectors,
                               size_t* sector_count, uft_decode_stats_t* stats) {
    if (!flux || !sectors || !sector_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *sector_count = 0;
    
    // Nominale Bit-Zeit ermitteln
    double nominal = options->pll_period_ns;
    if (nominal == 0) {
        // Auto-detect aus Flux
        uint64_t total = 0;
        size_t samples = count < 5000 ? count : 5000;
        for (size_t i = 0; i < samples; i++) {
            total += flux[i];
        }
        nominal = (double)total / samples / 2.0;  // Durchschnitt ÷ 2
    }
    
    mfm_pll_t pll;
    pll_init(&pll, nominal);
    
    // Statistiken
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->flux_transitions = count;
        stats->avg_bit_time_ns = nominal;
    }
    
    size_t pos = 0;
    
    while (pos < count && *sector_count < max_sectors) {
        // Sync suchen
        if (!find_sync(&pll, flux, count, &pos)) {
            break;
        }
        
        if (stats) stats->sync_found++;
        
        // Mark-Byte lesen
        uint8_t header[8];
        size_t read = read_bytes(&pll, flux, count, pos, header, 7);
        
        if (read < 7) continue;
        
        // IDAM?
        if (header[0] != MFM_IDAM_MARK) continue;
        
        // ID-Feld: Mark, C, H, R, N, CRC1, CRC2
        uint8_t cyl = header[1];
        uint8_t head = header[2];
        uint8_t sec = header[3];
        uint8_t size_code = header[4];
        uint16_t id_crc = (header[5] << 8) | header[6];
        
        // ID-CRC prüfen
        uint16_t calc_crc = crc16_mfm(header, 5);
        bool id_ok = (calc_crc == id_crc);
        
        // Sektorgröße
        size_t sector_size = 128 << size_code;
        if (sector_size > 8192) continue;  // Ungültig
        
        // Data-Sync suchen (sollte direkt folgen)
        size_t data_pos = pos + 100;  // ~Gap2
        if (!find_sync(&pll, flux, count, &data_pos)) {
            continue;
        }
        
        // DAM lesen
        uint8_t dam;
        if (read_bytes(&pll, flux, count, data_pos, &dam, 1) < 1) {
            continue;
        }
        
        bool deleted = (dam == MFM_DDAM_MARK);
        if (dam != MFM_DAM_MARK && dam != MFM_DDAM_MARK) {
            continue;
        }
        
        // Daten + CRC lesen
        uint8_t* data_buf = malloc(sector_size + 2);
        if (!data_buf) return UFT_ERROR_NO_MEMORY;
        
        read = read_bytes(&pll, flux, count, data_pos + 50, data_buf, sector_size + 2);
        
        if (read < sector_size + 2) {
            free(data_buf);
            continue;
        }
        
        // Data-CRC prüfen
        uint16_t data_crc = (data_buf[sector_size] << 8) | data_buf[sector_size + 1];
        
        // CRC berechnen (über DAM + Daten)
        uint16_t calc_data_crc = 0xFFFF;
        calc_data_crc = crc16_ccitt_byte(calc_data_crc, 0xA1);
        calc_data_crc = crc16_ccitt_byte(calc_data_crc, 0xA1);
        calc_data_crc = crc16_ccitt_byte(calc_data_crc, 0xA1);
        calc_data_crc = crc16_ccitt_byte(calc_data_crc, dam);
        for (size_t i = 0; i < sector_size; i++) {
            calc_data_crc = crc16_ccitt_byte(calc_data_crc, data_buf[i]);
        }
        
        bool data_ok = (calc_data_crc == data_crc);
        
        // Sektor speichern
        uft_sector_t* s = &sectors[*sector_count];
        memset(s, 0, sizeof(*s));
        
        s->id.cylinder = cyl;
        s->id.head = head;
        s->id.sector = sec;
        s->id.size_code = size_code;
        s->id.crc = id_crc;
        s->id.crc_ok = id_ok;
        
        s->data = malloc(sector_size);
        if (!s->data) {
            free(data_buf);
            return UFT_ERROR_NO_MEMORY;
        }
        memcpy(s->data, data_buf, sector_size);
        s->data_size = sector_size;
        s->data_crc = data_crc;
        
        s->status = 0;
        if (!id_ok) s->status |= UFT_SECTOR_ID_CRC_ERROR;
        if (!data_ok) s->status |= UFT_SECTOR_CRC_ERROR;
        if (deleted) s->status |= UFT_SECTOR_DELETED;
        
        free(data_buf);
        
        (*sector_count)++;
        
        if (stats) {
            stats->sectors_found++;
            if (data_ok && id_ok) {
                stats->sectors_ok++;
            } else {
                stats->sectors_bad_crc++;
            }
        }
        
        // Weiter nach diesem Sektor suchen
        pos = data_pos + sector_size * 20;  // Grob überspringen
    }
    
    // RPM schätzen
    if (stats && count > 0) {
        uint64_t total_time = 0;
        for (size_t i = 0; i < count; i++) {
            total_time += flux[i];
        }
        stats->rpm = 60.0e9 / total_time;
        stats->data_rate_bps = 1e9 / pll.period / 2;  // Datenrate
    }
    
    return UFT_OK;
}

// ============================================================================
// MFM Encoding: Sectors → Flux Transitions
// ============================================================================

static uft_error_t mfm_encode(const uft_sector_t* sectors, size_t sector_count,
                               int cylinder, int head,
                               const uft_encode_options_t* options,
                               uint32_t** flux, size_t* flux_count) {
    if (!sectors || !flux || !flux_count) return UFT_ERROR_NULL_POINTER;
    
    double bit_time_ns = (options && options->bit_rate_bps > 0) ?
                         1e9 / options->bit_rate_bps : MFM_DD_BIT_TIME_NS;
    
    uint16_t gap1 = (options && options->gap1_size) ? options->gap1_size : 80;
    uint16_t gap2 = (options && options->gap2_size) ? options->gap2_size : 22;
    uint16_t gap3 = (options && options->gap3_size) ? options->gap3_size : 80;
    
    /* Estimate max flux transitions */
    size_t max_flux = 200000;
    uint32_t* out = malloc(max_flux * sizeof(uint32_t));
    if (!out) return UFT_ERROR_NO_MEMORY;
    
    size_t fi = 0;
    bool prev_bit = false;
    
    /* Helper: emit one MFM bit pair (clock + data) */
    #define EMIT_MFM_BIT(data_bit) do { \
        bool clk = !(data_bit) && !prev_bit; \
        if (clk && fi < max_flux) out[fi++] = (uint32_t)(bit_time_ns * 0.5); \
        if ((data_bit) && fi < max_flux) out[fi++] = (uint32_t)(bit_time_ns); \
        prev_bit = (data_bit); \
    } while(0)
    
    /* Helper: emit one MFM byte */
    #define EMIT_MFM_BYTE(byte) do { \
        for (int _b = 7; _b >= 0; _b--) { \
            bool _bit = ((byte) >> _b) & 1; \
            EMIT_MFM_BIT(_bit); \
        } \
    } while(0)
    
    /* GAP1: 0x4E bytes */
    for (uint16_t i = 0; i < gap1; i++) EMIT_MFM_BYTE(0x4E);
    
    for (size_t si = 0; si < sector_count && si < 64; si++) {
        /* Sync: 12 × 0x00 */
        for (int i = 0; i < 12; i++) EMIT_MFM_BYTE(0x00);
        
        /* 3 × A1 sync (with missing clock = 0x4489) */
        for (int i = 0; i < 3; i++) {
            /* Special: 0x4489 raw */
            uint16_t sync = 0x4489;
            for (int b = 15; b >= 0; b--) {
                bool bit = (sync >> b) & 1;
                if (bit && fi < max_flux) out[fi++] = (uint32_t)bit_time_ns;
                prev_bit = bit;
            }
        }
        
        /* IDAM: 0xFE + C/H/S/N */
        uint8_t id_field[4] = {
            (uint8_t)cylinder, (uint8_t)head,
            sectors[si].sector_id, sectors[si].size_code
        };
        
        EMIT_MFM_BYTE(0xFE);
        uint16_t crc = 0xCDB4;  /* CRC after 3×A1 + FE */
        for (int i = 0; i < 4; i++) {
            EMIT_MFM_BYTE(id_field[i]);
            crc = crc16_ccitt_byte(crc, id_field[i]);
        }
        EMIT_MFM_BYTE(crc >> 8);
        EMIT_MFM_BYTE(crc & 0xFF);
        
        /* GAP2 */
        for (uint16_t i = 0; i < gap2; i++) EMIT_MFM_BYTE(0x4E);
        
        /* Data sync: 12 × 0x00 + 3 × A1 */
        for (int i = 0; i < 12; i++) EMIT_MFM_BYTE(0x00);
        for (int i = 0; i < 3; i++) {
            uint16_t sync = 0x4489;
            for (int b = 15; b >= 0; b--) {
                bool bit = (sync >> b) & 1;
                if (bit && fi < max_flux) out[fi++] = (uint32_t)bit_time_ns;
                prev_bit = bit;
            }
        }
        
        /* DAM: 0xFB + data */
        EMIT_MFM_BYTE(0xFB);
        crc = 0xCDB4;  /* After 3×A1 */
        crc = crc16_ccitt_byte(crc, 0xFB);
        
        size_t data_size = 128u << (sectors[si].size_code & 7);
        for (size_t d = 0; d < data_size; d++) {
            uint8_t byte = (sectors[si].data && d < sectors[si].data_size) ?
                           sectors[si].data[d] : 0;
            EMIT_MFM_BYTE(byte);
            crc = crc16_ccitt_byte(crc, byte);
        }
        EMIT_MFM_BYTE(crc >> 8);
        EMIT_MFM_BYTE(crc & 0xFF);
        
        /* GAP3 */
        for (uint16_t i = 0; i < gap3; i++) EMIT_MFM_BYTE(0x4E);
    }
    
    #undef EMIT_MFM_BIT
    #undef EMIT_MFM_BYTE
    
    *flux = out;
    *flux_count = fi;
    return UFT_OK;
}

// ============================================================================
// Helpers
// ============================================================================

static double mfm_get_data_rate(uft_geometry_preset_t preset) {
    switch (preset) {
        case UFT_GEO_PC_1200K:
        case UFT_GEO_PC_1440K:
        case UFT_GEO_AMIGA_HD:
            return MFM_HD_BITRATE;
        default:
            return MFM_DD_BITRATE;
    }
}

static void mfm_get_default_gaps(uft_geometry_preset_t preset,
                                  uint16_t* gap1, uint16_t* gap2,
                                  uint16_t* gap3, uint16_t* gap4) {
    // Standard-Werte für IBM-Format
    *gap1 = 80;   // Post-Index
    *gap2 = 22;   // Post-ID
    *gap3 = 80;   // Post-Data
    *gap4 = 0;    // Pre-Index (Auto-Fill)
}

// ============================================================================
// Plugin Definition
// ============================================================================

const uft_decoder_plugin_t uft_decoder_plugin_mfm = {
    .name = "IBM MFM",
    .description = "IBM-compatible MFM (PC, Atari ST)",
    .version = 0x00010000,
    .encoding = UFT_ENC_MFM,
    .capabilities = UFT_DECODER_CAP_DECODE | UFT_DECODER_CAP_AUTO_DETECT,
    
    .default_sync = MFM_SYNC_WORD,
    .default_clock = MFM_DD_BIT_TIME_NS,
    
    .detect = mfm_detect,
    .decode = mfm_decode,
    .encode = mfm_encode,
    .get_data_rate = mfm_get_data_rate,
    .get_default_gaps = mfm_get_default_gaps,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL
};
