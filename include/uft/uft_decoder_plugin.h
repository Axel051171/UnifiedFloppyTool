/**
 * @file uft_decoder_plugin.h
 * @brief UnifiedFloppyTool - Decoder Plugin Interface
 * 
 * Definiert das Interface für Encoding/Decoding Plugins (MFM, GCR, etc.)
 */

#ifndef UFT_DECODER_PLUGIN_H
#define UFT_DECODER_PLUGIN_H

#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Decoder Capabilities
// ============================================================================

typedef enum uft_decoder_caps {
    UFT_DECODER_CAP_DECODE      = (1 << 0),  ///< Kann dekodieren
    UFT_DECODER_CAP_ENCODE      = (1 << 1),  ///< Kann enkodieren
    UFT_DECODER_CAP_AUTO_DETECT = (1 << 2),  ///< Kann Format erkennen
    UFT_DECODER_CAP_WEAK_BITS   = (1 << 3),  ///< Erkennt Weak Bits
    UFT_DECODER_CAP_COPY_PROT   = (1 << 4),  ///< Erkennt Copy Protection
} uft_decoder_caps_t;

// ============================================================================
// Decode Statistics
// ============================================================================

/**
 * @brief Dekodierungs-Statistiken
 */
typedef struct uft_decode_stats {
    // Flux-Analyse
    uint32_t    flux_transitions;    ///< Anzahl Flux-Transitions
    double      avg_bit_time_ns;     ///< Durchschnittliche Bit-Zeit
    double      bit_time_variance;   ///< Varianz
    double      data_rate_bps;       ///< Datenrate
    double      rpm;                 ///< Gemessene RPM
    
    // Dekodierung
    uint32_t    sync_found;          ///< Anzahl gefundener Syncs
    uint32_t    sectors_found;       ///< Gefundene Sektoren
    uint32_t    sectors_ok;          ///< Fehlerfreie Sektoren
    uint32_t    sectors_bad_crc;     ///< CRC-Fehler
    uint32_t    sectors_missing;     ///< Fehlende Sektoren
    
    // Spezial
    bool        weak_bits_detected;
    bool        copy_prot_detected;
    uint32_t    pll_locks;           ///< PLL Lock-Events
    uint32_t    pll_slips;           ///< PLL Slip-Events
} uft_decode_stats_t;

// ============================================================================
// Decode Options
// ============================================================================

/**
 * @brief Dekodierungs-Optionen
 */
typedef struct uft_decode_options {
    // Sync
    uint16_t    sync_pattern;        ///< Sync-Muster (0 = auto)
    uint8_t     sync_bits;           ///< Sync-Länge in Bits (0 = auto)
    
    // PLL
    double      pll_period_ns;       ///< Nominale Bit-Periode (0 = auto)
    double      pll_adjust_pct;      ///< PLL-Anpassungsrate (1-50%)
    
    // Toleranzen
    double      clock_tolerance_pct; ///< Toleranz für Clock-Detection
    
    // Flags
    bool        strict_crc;          ///< Bei CRC-Fehler abbrechen
    bool        detect_weak_bits;    ///< Weak Bits erkennen
    bool        detect_copy_prot;    ///< Copy Protection erkennen
    
    // Erwartete Geometrie (optional)
    uint8_t     expected_sectors;    ///< Erwartete Sektoren (0 = beliebig)
    uint16_t    expected_sector_size;///< Erwartete Sektorgröße
    
} uft_decode_options_t;

/**
 * @brief Standard-Dekodier-Optionen
 */
static inline uft_decode_options_t uft_default_decode_options(void) {
    return (uft_decode_options_t){
        .sync_pattern = 0,
        .sync_bits = 0,
        .pll_period_ns = 0,
        .pll_adjust_pct = 5.0,
        .clock_tolerance_pct = 10.0,
        .strict_crc = false,
        .detect_weak_bits = true,
        .detect_copy_prot = true,
        .expected_sectors = 0,
        .expected_sector_size = 0
    };
}

// ============================================================================
// Encoder Options
// ============================================================================

/**
 * @brief Enkodierungs-Optionen
 */
typedef struct uft_encode_options {
    // Timing
    double      bit_rate_bps;        ///< Bit-Rate (0 = Standard für Encoding)
    double      rpm;                 ///< Ziel-RPM (0 = 300)
    
    // Gaps
    uint16_t    gap1_size;           ///< Gap nach Index (0 = default)
    uint16_t    gap2_size;           ///< Gap nach ID (0 = default)
    uint16_t    gap3_size;           ///< Gap nach Data (0 = default)
    uint16_t    gap4_size;           ///< Gap am Ende (0 = auto-fill)
    
    // Fill
    uint8_t     gap_fill;            ///< Fill-Byte für Gaps (0x4E für MFM)
    uint8_t     format_fill;         ///< Fill-Byte für Sektoren (0xE5)
    
    // Write Precompensation
    int16_t     precomp_ns;          ///< Precomp in ns (-1 = auto)
    uint8_t     precomp_track;       ///< Ab welchem Track (typ. 40)
    
} uft_encode_options_t;

/**
 * @brief Standard-Enkodier-Optionen
 */
static inline uft_encode_options_t uft_default_encode_options(void) {
    return (uft_encode_options_t){
        .bit_rate_bps = 0,
        .rpm = 300.0,
        .gap1_size = 0,
        .gap2_size = 0,
        .gap3_size = 0,
        .gap4_size = 0,
        .gap_fill = 0x4E,
        .format_fill = 0xE5,
        .precomp_ns = -1,
        .precomp_track = 40
    };
}

// ============================================================================
// Decoder Plugin Interface
// ============================================================================

/**
 * @brief Decoder Plugin Struktur
 */
typedef struct uft_decoder_plugin {
    // === Identifikation ===
    const char*         name;          ///< Plugin-Name ("MFM", "GCR_C64", etc.)
    const char*         description;   ///< Beschreibung
    uint32_t            version;       ///< Plugin-Version
    uft_encoding_t      encoding;      ///< Encoding-Typ
    uint32_t            capabilities;  ///< uft_decoder_caps_t Flags
    
    // === Typische Parameter ===
    uint16_t            default_sync;   ///< Standard-Sync (z.B. 0x4489)
    double              default_clock;  ///< Standard-Clock (ns)
    
    // === Auto-Detection ===
    
    /**
     * @brief Prüft ob Flux-Daten dieser Kodierung entsprechen
     * 
     * @param flux Flux-Zeiten (ns)
     * @param count Anzahl Transitions
     * @param confidence [out] Confidence 0-100
     * @return true wenn erkannt
     */
    bool (*detect)(const uint32_t* flux, size_t count, int* confidence);
    
    // === Decode ===
    
    /**
     * @brief Flux zu Sektoren dekodieren
     * 
     * @param flux Flux-Zeiten (ns)
     * @param count Anzahl Transitions
     * @param options Dekodier-Optionen
     * @param sectors [out] Array für Sektoren
     * @param max_sectors Maximale Anzahl Sektoren
     * @param sector_count [out] Gefundene Sektoren
     * @param stats [out] Statistiken (optional)
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*decode)(const uint32_t* flux, size_t count,
                           const uft_decode_options_t* options,
                           uft_sector_t* sectors, size_t max_sectors,
                           size_t* sector_count, uft_decode_stats_t* stats);
    
    // === Encode ===
    
    /**
     * @brief Sektoren zu Flux enkodieren
     * 
     * @param sectors Sektor-Array
     * @param sector_count Anzahl Sektoren
     * @param cylinder Zylinder (für Precomp)
     * @param head Seite
     * @param options Enkodier-Optionen
     * @param flux [out] Flux-Zeiten (wird allokiert)
     * @param flux_count [out] Anzahl Transitions
     * @return UFT_OK bei Erfolg
     * 
     * @note Caller muss *flux mit free() freigeben!
     */
    uft_error_t (*encode)(const uft_sector_t* sectors, size_t sector_count,
                           int cylinder, int head,
                           const uft_encode_options_t* options,
                           uint32_t** flux, size_t* flux_count);
    
    // === Helpers ===
    
    /**
     * @brief Nominale Datenrate für diese Kodierung
     */
    double (*get_data_rate)(uft_geometry_preset_t preset);
    
    /**
     * @brief Standard-Gap-Größen für Geometrie
     */
    void (*get_default_gaps)(uft_geometry_preset_t preset,
                             uint16_t* gap1, uint16_t* gap2, 
                             uint16_t* gap3, uint16_t* gap4);
    
    // === Plugin Lifecycle ===
    uft_error_t (*init)(void);
    void (*shutdown)(void);
    
    // === Private ===
    void*               private_data;
    
} uft_decoder_plugin_t;

// ============================================================================
// Plugin Registry
// ============================================================================

/**
 * @brief Decoder-Plugin registrieren
 */
uft_error_t uft_register_decoder_plugin(const uft_decoder_plugin_t* plugin);

/**
 * @brief Decoder-Plugin deregistrieren
 */
uft_error_t uft_unregister_decoder_plugin(uft_encoding_t encoding);

/**
 * @brief Plugin für Encoding holen
 */
const uft_decoder_plugin_t* uft_get_decoder_plugin(uft_encoding_t encoding);

/**
 * @brief Bestes Plugin für Flux-Daten finden
 */
const uft_decoder_plugin_t* uft_find_decoder_plugin_for_flux(
    const uint32_t* flux, size_t count);

/**
 * @brief Alle Plugins auflisten
 */
size_t uft_list_decoder_plugins(const uft_decoder_plugin_t** plugins, size_t max);

// ============================================================================
// High-Level Decode/Encode
// ============================================================================

/**
 * @brief Flux dekodieren (Auto-Detect)
 * 
 * Findet automatisch das passende Plugin.
 */
uft_error_t uft_decode_flux(const uint32_t* flux, size_t count,
                             uft_sector_t* sectors, size_t max_sectors,
                             size_t* sector_count, uft_decode_stats_t* stats);

/**
 * @brief Sektoren enkodieren
 */
uft_error_t uft_encode_sectors(const uft_sector_t* sectors, size_t sector_count,
                                uft_encoding_t encoding, int cylinder, int head,
                                uint32_t** flux, size_t* flux_count);

// ============================================================================
// Built-in Decoder Plugin Deklarationen
// ============================================================================

/// IBM MFM Decoder
extern const uft_decoder_plugin_t uft_decoder_plugin_mfm;

/// IBM FM Decoder
extern const uft_decoder_plugin_t uft_decoder_plugin_fm;

/// Amiga MFM Decoder
extern const uft_decoder_plugin_t uft_decoder_plugin_amiga_mfm;

/// C64 GCR Decoder
extern const uft_decoder_plugin_t uft_decoder_plugin_gcr_cbm;

/// Apple GCR Decoder (5.25")
extern const uft_decoder_plugin_t uft_decoder_plugin_gcr_apple;

// ============================================================================
// Alle Built-in Decoder registrieren
// ============================================================================

/**
 * @brief Alle eingebauten Decoder-Plugins registrieren
 */
uft_error_t uft_register_builtin_decoder_plugins(void);

// ============================================================================
// PLL Utilities
// ============================================================================

/**
 * @brief PLL-Struktur für Decoder-Implementierungen
 */
typedef struct uft_pll {
    double      nominal_period;   ///< Nominale Bit-Periode (ns)
    double      current_period;   ///< Aktuelle Bit-Periode
    double      adjust_rate;      ///< Anpassungsrate (0.0-1.0)
    double      phase;            ///< Aktuelle Phase
    uint32_t    lock_count;       ///< Lock-Zähler
    uint32_t    slip_count;       ///< Slip-Zähler
} uft_pll_t;

/**
 * @brief PLL initialisieren
 */
void uft_pll_init(uft_pll_t* pll, double nominal_period_ns, double adjust_pct);

/**
 * @brief Flux-Transition verarbeiten
 * 
 * @param pll PLL-Struktur
 * @param delta Zeit seit letzter Transition (ns)
 * @param bits [out] Dekodierte Bits (0 oder 1)
 * @param bit_count [out] Anzahl Bits
 * @return true bei Lock, false bei Slip
 */
bool uft_pll_process(uft_pll_t* pll, uint32_t delta, uint8_t* bits, int* bit_count);

/**
 * @brief PLL resetten
 */
void uft_pll_reset(uft_pll_t* pll);

// ============================================================================
// CRC Utilities
// ============================================================================

/**
 * @brief CRC-16 CCITT berechnen (für MFM)
 * 
 * @param data Daten
 * @param len Länge
 * @param init Initial-Wert (0xFFFF für Standard)
 * @return CRC-16
 */
uint16_t uft_crc16_ccitt(const uint8_t* data, size_t len, uint16_t init);

/**
 * @brief CRC für MFM Address Mark
 * 
 * Inkludiert das $A1A1A1 Sync.
 */
uint16_t uft_crc16_mfm_idam(const uint8_t* id, size_t len);

/**
 * @brief GCR Checksum berechnen (C64)
 */
uint8_t uft_checksum_gcr_cbm(const uint8_t* data, size_t len);

/**
 * @brief Amiga Checksum berechnen
 */
uint32_t uft_checksum_amiga(const uint32_t* data, size_t count);

#ifdef __cplusplus
}
#endif

#endif // UFT_DECODER_PLUGIN_H
