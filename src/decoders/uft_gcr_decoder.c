/**
 * @file uft_gcr_decoder.c
 * @brief UnifiedFloppyTool - GCR (Group Coded Recording) Decoder Plugin
 * 
 * GCR ist das Encoding der Commodore 1541 Floppy-Laufwerke.
 * 
 * ENCODING SCHEMA:
 * - 4 Daten-Bits → 5 GCR-Bits
 * - Garantiert max. 2 aufeinanderfolgende Nullen
 * - Kein separates Clock-Bit wie bei MFM
 * 
 * DISK STRUKTUR (1541):
 * - 35-42 Tracks (Standard: 35)
 * - Variable Sektoren pro Track (Speed Zones)
 * - 256 Bytes pro Sektor
 * 
 * SYNC MARKER:
 * - 10× 0xFF Bytes (= 10× "11111" in GCR)
 * - Gefolgt von Header oder Data Block
 * 
 * HEADER BLOCK (10 GCR-Bytes = 8 Daten-Bytes):
 * - Byte 0: 0x08 (Header ID)
 * - Byte 1: Header Checksum (XOR von Bytes 2-5)
 * - Byte 2: Sektor-Nummer
 * - Byte 3: Track-Nummer
 * - Byte 4: Disk ID Low
 * - Byte 5: Disk ID High
 * - Byte 6-7: 0x0F 0x0F (Padding)
 * 
 * DATA BLOCK (325 GCR-Bytes = 260 Daten-Bytes):
 * - Byte 0: 0x07 (Data ID)
 * - Byte 1-256: Sektor-Daten
 * - Byte 257: Data Checksum (XOR von Bytes 1-256)
 * - Byte 258-259: 0x00 0x00 (Off-Bytes)
 * 
 * SPEED ZONES:
 * - Zone 0 (Tracks 1-17):  21 Sektoren, ~300 RPM
 * - Zone 1 (Tracks 18-24): 19 Sektoren
 * - Zone 2 (Tracks 25-30): 18 Sektoren
 * - Zone 3 (Tracks 31-42): 17 Sektoren
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_decoder_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// GCR Constants
// ============================================================================

#define GCR_SYNC_BYTE           0xFF
#define GCR_SYNC_MIN_COUNT      5       // Minimum 5× 0xFF für Sync
#define GCR_SYNC_TYPICAL        10      // Typisch 10× 0xFF

#define GCR_HEADER_ID           0x08
#define GCR_DATA_ID             0x07
#define GCR_DELETED_DATA_ID     0x09    // Selten verwendet

#define GCR_HEADER_SIZE         8       // 8 Daten-Bytes
#define GCR_HEADER_GCR_SIZE     10      // 10 GCR-Bytes
#define GCR_DATA_SIZE           260     // 260 Daten-Bytes (1 + 256 + 1 + 2)
#define GCR_DATA_GCR_SIZE       325     // 325 GCR-Bytes

#define GCR_SECTOR_DATA_SIZE    256     // Nutz-Daten pro Sektor

// Speed Zone Timing (Bit-Zeit in ns)
static const uint32_t gcr_zone_bitcell[4] = {
    4000,   // Zone 0: 4µs
    3750,   // Zone 1: 3.75µs
    3500,   // Zone 2: 3.5µs
    3250    // Zone 3: 3.25µs
};

// Sektoren pro Track in jeder Zone
static const uint8_t gcr_zone_sectors[4] = {
    21,     // Zone 0
    19,     // Zone 1
    18,     // Zone 2
    17      // Zone 3
};

// Track-Bereiche pro Zone (1-basiert)
// Zone 0: Tracks 1-17
// Zone 1: Tracks 18-24
// Zone 2: Tracks 25-30
// Zone 3: Tracks 31-42

// ============================================================================
// GCR Lookup Tables
// ============================================================================

/**
 * GCR 5-Bit → 4-Bit Dekodierung
 * 0xFF = ungültiger GCR-Code
 */
static const uint8_t gcr_decode_table[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 00-07: ungültig
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  // 08-0F
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  // 10-17
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   // 18-1F
};

/**
 * 4-Bit → GCR 5-Bit Enkodierung
 */
static const uint8_t gcr_encode_table[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,  // 0-7
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15   // 8-F
};

// ============================================================================
// GCR Decode/Encode Functions
// ============================================================================

/**
 * @brief Dekodiert 5 GCR-Bytes zu 4 Daten-Bytes
 * 
 * @param gcr 5 GCR-Bytes
 * @param data 4 Output-Bytes
 * @return true wenn erfolgreich, false bei ungültigem GCR
 */
static bool gcr_decode_group(const uint8_t* gcr, uint8_t* data) {
    // 40 GCR-Bits aus 5 Bytes extrahieren
    uint64_t bits = ((uint64_t)gcr[0] << 32) | ((uint64_t)gcr[1] << 24) |
                    ((uint64_t)gcr[2] << 16) | ((uint64_t)gcr[3] << 8) |
                    (uint64_t)gcr[4];
    
    // 8 Nibbles dekodieren (je 5 Bits → 4 Bits)
    uint8_t n[8];
    n[0] = gcr_decode_table[(bits >> 35) & 0x1F];
    n[1] = gcr_decode_table[(bits >> 30) & 0x1F];
    n[2] = gcr_decode_table[(bits >> 25) & 0x1F];
    n[3] = gcr_decode_table[(bits >> 20) & 0x1F];
    n[4] = gcr_decode_table[(bits >> 15) & 0x1F];
    n[5] = gcr_decode_table[(bits >> 10) & 0x1F];
    n[6] = gcr_decode_table[(bits >> 5) & 0x1F];
    n[7] = gcr_decode_table[bits & 0x1F];
    
    // Ungültige GCR-Codes prüfen
    for (int i = 0; i < 8; i++) {
        if (n[i] == 0xFF) return false;
    }
    
    // Nibbles zu Bytes kombinieren
    data[0] = (n[0] << 4) | n[1];
    data[1] = (n[2] << 4) | n[3];
    data[2] = (n[4] << 4) | n[5];
    data[3] = (n[6] << 4) | n[7];
    
    return true;
}

/**
 * @brief Enkodiert 4 Daten-Bytes zu 5 GCR-Bytes
 */
static void gcr_encode_group(const uint8_t* data, uint8_t* gcr) {
    // 8 Nibbles zu GCR konvertieren
    uint8_t n[8];
    n[0] = gcr_encode_table[(data[0] >> 4) & 0x0F];
    n[1] = gcr_encode_table[data[0] & 0x0F];
    n[2] = gcr_encode_table[(data[1] >> 4) & 0x0F];
    n[3] = gcr_encode_table[data[1] & 0x0F];
    n[4] = gcr_encode_table[(data[2] >> 4) & 0x0F];
    n[5] = gcr_encode_table[data[2] & 0x0F];
    n[6] = gcr_encode_table[(data[3] >> 4) & 0x0F];
    n[7] = gcr_encode_table[data[3] & 0x0F];
    
    // 8× 5-Bit zu 5 Bytes packen
    gcr[0] = (n[0] << 3) | (n[1] >> 2);
    gcr[1] = (n[1] << 6) | (n[2] << 1) | (n[3] >> 4);
    gcr[2] = (n[3] << 4) | (n[4] >> 1);
    gcr[3] = (n[4] << 7) | (n[5] << 2) | (n[6] >> 3);
    gcr[4] = (n[6] << 5) | n[7];
}

/**
 * @brief Dekodiert einen kompletten GCR-Block (beliebige Länge)
 * 
 * @param gcr GCR-Daten
 * @param gcr_len Länge in Bytes (muss durch 5 teilbar sein)
 * @param data Output-Buffer
 * @param data_len Output-Länge
 * @return Anzahl dekodierte Bytes, 0 bei Fehler
 */
static size_t gcr_decode_block(const uint8_t* gcr, size_t gcr_len,
                               uint8_t* data, size_t* data_len) {
    if (!gcr || !data || !data_len) return 0;
    if (gcr_len % 5 != 0) return 0;
    
    size_t groups = gcr_len / 5;
    *data_len = groups * 4;
    
    for (size_t i = 0; i < groups; i++) {
        if (!gcr_decode_group(&gcr[i * 5], &data[i * 4])) {
            return i * 4;  // Partial decode
        }
    }
    
    return *data_len;
}

// ============================================================================
// Sync Detection
// ============================================================================

/**
 * @brief Findet nächsten Sync-Marker
 * 
 * @param data GCR-Daten
 * @param len Länge
 * @param start Startposition
 * @return Position nach dem Sync, oder -1 wenn nicht gefunden
 */
static int find_sync(const uint8_t* data, size_t len, size_t start) {
    int consecutive = 0;
    
    for (size_t i = start; i < len; i++) {
        if (data[i] == GCR_SYNC_BYTE) {
            consecutive++;
            if (consecutive >= GCR_SYNC_MIN_COUNT) {
                // Sync gefunden - finde Ende
                while (i + 1 < len && data[i + 1] == GCR_SYNC_BYTE) {
                    i++;
                }
                return (int)(i + 1);
            }
        } else {
            consecutive = 0;
        }
    }
    
    return -1;
}

// ============================================================================
// Sector Parsing
// ============================================================================

/**
 * @brief GCR Sektor-Header Struktur
 */
typedef struct {
    uint8_t block_id;       // 0x08
    uint8_t checksum;       // XOR von sector, track, id1, id2
    uint8_t sector;         // Sektor-Nummer (0-20)
    uint8_t track;          // Track-Nummer (1-42)
    uint8_t id1;            // Disk ID Low
    uint8_t id2;            // Disk ID High
    bool    valid;          // Header gültig?
    bool    checksum_ok;    // Checksum korrekt?
} gcr_header_t;

/**
 * @brief Parst einen GCR-Header
 */
static bool parse_gcr_header(const uint8_t* gcr_data, gcr_header_t* header) {
    // 10 GCR-Bytes → 8 Daten-Bytes
    uint8_t decoded[8];
    
    if (!gcr_decode_group(&gcr_data[0], &decoded[0])) {
        header->valid = false;
        return false;
    }
    if (!gcr_decode_group(&gcr_data[5], &decoded[4])) {
        header->valid = false;
        return false;
    }
    
    header->block_id = decoded[0];
    header->checksum = decoded[1];
    header->sector = decoded[2];
    header->track = decoded[3];
    header->id1 = decoded[4];
    header->id2 = decoded[5];
    
    // Block ID prüfen
    if (header->block_id != GCR_HEADER_ID) {
        header->valid = false;
        return false;
    }
    
    // Checksum berechnen und prüfen
    uint8_t calc_checksum = header->sector ^ header->track ^ 
                            header->id1 ^ header->id2;
    header->checksum_ok = (calc_checksum == header->checksum);
    header->valid = true;
    
    return true;
}

/**
 * @brief Parst einen GCR-Datenblock
 * 
 * @param gcr_data 325 GCR-Bytes
 * @param sector_data 256 Output-Bytes
 * @param checksum_ok Output: Checksum gültig?
 * @return true wenn erfolgreich
 */
static bool parse_gcr_data(const uint8_t* gcr_data, uint8_t* sector_data,
                           bool* checksum_ok) {
    // 325 GCR-Bytes → 260 Daten-Bytes
    uint8_t decoded[260];
    
    for (int i = 0; i < 65; i++) {  // 65 Gruppen × 4 Bytes = 260
        if (!gcr_decode_group(&gcr_data[i * 5], &decoded[i * 4])) {
            *checksum_ok = false;
            return false;
        }
    }
    
    // Block ID prüfen
    if (decoded[0] != GCR_DATA_ID && decoded[0] != GCR_DELETED_DATA_ID) {
        *checksum_ok = false;
        return false;
    }
    
    // Sektor-Daten kopieren
    memcpy(sector_data, &decoded[1], GCR_SECTOR_DATA_SIZE);
    
    // Checksum berechnen
    uint8_t calc_checksum = 0;
    for (int i = 1; i <= GCR_SECTOR_DATA_SIZE; i++) {
        calc_checksum ^= decoded[i];
    }
    
    *checksum_ok = (calc_checksum == decoded[257]);
    
    return true;
}

// ============================================================================
// Plugin Interface
// ============================================================================

/**
 * @brief Auto-Detection für GCR
 */
static bool gcr_detect(const uint32_t* flux, size_t count, int* confidence) {
    if (!flux || count < 100 || !confidence) {
        *confidence = 0;
        return false;
    }
    
    *confidence = 0;
    
    // GCR-typische Intervalle analysieren
    // GCR hat 4µs Bitcell (Zone 0), also ~4000ns
    // Intervalle sollten bei 4000, 8000, 12000 ns liegen
    
    int short_count = 0;   // ~4000ns (1 cell)
    int medium_count = 0;  // ~8000ns (2 cells)
    int long_count = 0;    // ~12000ns (3 cells)
    int total = 0;
    
    for (size_t i = 0; i < count && i < 1000; i++) {
        uint32_t interval = flux[i];
        
        if (interval >= 3000 && interval <= 5000) {
            short_count++;
        } else if (interval >= 6000 && interval <= 10000) {
            medium_count++;
        } else if (interval >= 10000 && interval <= 16000) {
            long_count++;
        }
        total++;
    }
    
    if (total < 100) {
        *confidence = 0;
        return false;
    }
    
    // GCR sollte überwiegend kurze und mittlere Intervalle haben
    int valid = short_count + medium_count + long_count;
    int ratio = (valid * 100) / total;
    
    if (ratio >= 80) {
        *confidence = 70;
    } else if (ratio >= 60) {
        *confidence = 50;
    } else {
        *confidence = 20;
    }
    
    return (*confidence >= 50);
}

/**
 * @brief Dekodiert GCR Flux zu Sektoren
 */
static uft_error_t gcr_decode_flux(const uint32_t* flux, size_t count,
                                    const uft_decode_options_t* options,
                                    uft_sector_t* sectors, size_t max_sectors,
                                    size_t* sector_count, uft_decode_stats_t* stats) {
    if (!flux || !sectors || !sector_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *sector_count = 0;
    
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->flux_transitions = (uint32_t)count;
    }
    
    // TODO: Flux zu Bits konvertieren via PLL
    // Dann GCR-Dekodierung wie oben
    
    // Für jetzt: Stub - diese Funktion würde den Flux-Stream
    // durch eine PLL verarbeiten und dann die Bits an die
    // bestehende GCR-Dekodierungslogik übergeben
    
    return UFT_OK;
}

/**
 * @brief Enkodiert Sektoren zu GCR Flux
 */
static uft_error_t gcr_encode_flux(const uft_sector_t* sectors, size_t sector_count,
                                    int cylinder, int head,
                                    const uft_encode_options_t* options,
                                    uint32_t** flux, size_t* flux_count) {
    if (!sectors || !flux || !flux_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // TODO: Sektoren zu GCR-Bits, dann zu Flux-Transitions
    
    *flux = NULL;
    *flux_count = 0;
    
    return UFT_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief Gibt Datenrate für Geometrie zurück
 */
static double gcr_get_data_rate(uft_geometry_preset_t preset) {
    // GCR: 4µs Bitcell = 250000 bit/s (Zone 0)
    return 250000.0;
}

/**
 * @brief Gibt Standard-Gap-Größen zurück
 */
static void gcr_get_default_gaps(uft_geometry_preset_t preset,
                                 uint16_t* gap1, uint16_t* gap2, 
                                 uint16_t* gap3, uint16_t* gap4) {
    // C64 GCR Gaps
    if (gap1) *gap1 = 0;    // Kein Gap1 bei GCR
    if (gap2) *gap2 = 9;    // 9 Bytes nach Header
    if (gap3) *gap3 = 8;    // 8 Bytes nach Data
    if (gap4) *gap4 = 0;    // Auto-fill
}

// ============================================================================
// Plugin Registration
// ============================================================================

static const uft_decoder_plugin_t gcr_plugin = {
    .name = "GCR",
    .description = "Commodore 64 GCR (5-to-4) Decoder",
    .version = 0x00010000,
    .encoding = UFT_ENC_GCR_CBM,
    .capabilities = UFT_DECODER_CAP_DECODE | UFT_DECODER_CAP_AUTO_DETECT,
    
    .default_sync = 0xFFFF,     // GCR: 0xFF Bytes als Sync
    .default_clock = 4000.0,    // 4µs
    
    .detect = gcr_detect,
    .decode = gcr_decode_flux,
    .encode = gcr_encode_flux,
    .get_data_rate = gcr_get_data_rate,
    .get_default_gaps = gcr_get_default_gaps,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL
};

/**
 * @brief Registriert das GCR Decoder Plugin
 */
const uft_decoder_plugin_t* uft_decoder_gcr_get_plugin(void) {
    return &gcr_plugin;
}

// Für automatische Registrierung
const uft_decoder_plugin_t uft_decoder_plugin_gcr = {
    .name = "GCR",
    .description = "Commodore 64 GCR (5-to-4) Decoder",
    .version = 0x00010000,
    .encoding = UFT_ENC_GCR_CBM,
    .capabilities = UFT_DECODER_CAP_DECODE | UFT_DECODER_CAP_AUTO_DETECT,
    
    .default_sync = 0xFFFF,
    .default_clock = 4000.0,
    
    .detect = gcr_detect,
    .decode = gcr_decode_flux,
    .encode = gcr_encode_flux,
    .get_data_rate = gcr_get_data_rate,
    .get_default_gaps = gcr_get_default_gaps,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL
};
