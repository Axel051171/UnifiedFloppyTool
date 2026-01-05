/**
 * @file uft_types.h
 * @brief UnifiedFloppyTool - Basis-Typdefinitionen
 * 
 * Dieses Header definiert alle grundlegenden Typen, die von
 * allen anderen UFT-Modulen verwendet werden.
 */

#ifndef UFT_TYPES_H
#define UFT_TYPES_H

#include "uft_platform.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Version
// ============================================================================

#define UFT_VERSION_MAJOR  0
#define UFT_VERSION_MINOR  1
#define UFT_VERSION_PATCH  0
#define UFT_VERSION_STRING "0.1.0-dev"

// ============================================================================
// Basis-Typen
// ============================================================================

/// Opaque Handle für Disk
typedef struct uft_disk uft_disk_t;

/// Opaque Handle für Track  
typedef struct uft_track uft_track_t;

/// Opaque Handle für Device (Hardware)
typedef struct uft_device uft_device_t;

// ============================================================================
// Geometrie
// ============================================================================

/**
 * @brief Disk-Geometrie
 */
typedef struct uft_geometry {
    uint16_t cylinders;      ///< Anzahl Zylinder (typ. 80-84)
    uint16_t heads;          ///< Anzahl Köpfe (1 oder 2)
    uint16_t sectors;        ///< Sektoren pro Track
    uint16_t sector_size;    ///< Bytes pro Sektor (128-8192)
    uint32_t total_sectors;  ///< Gesamtzahl Sektoren
    bool     double_step;    ///< 40-Track Disk in 80-Track Drive
} uft_geometry_t;

/**
 * @brief Bekannte Geometrie-Presets
 */
typedef enum uft_geometry_preset {
    UFT_GEO_UNKNOWN = 0,
    
    // PC Formate
    UFT_GEO_PC_360K,         ///< 5.25" DD (40/2/9)
    UFT_GEO_PC_720K,         ///< 3.5" DD (80/2/9)
    UFT_GEO_PC_1200K,        ///< 5.25" HD (80/2/15)
    UFT_GEO_PC_1440K,        ///< 3.5" HD (80/2/18)
    UFT_GEO_PC_2880K,        ///< 3.5" ED (80/2/36)
    
    // Amiga Formate
    UFT_GEO_AMIGA_DD,        ///< 880KB (80/2/11)
    UFT_GEO_AMIGA_HD,        ///< 1.76MB (80/2/22)
    
    // C64 Formate
    UFT_GEO_C64_1541,        ///< 170KB (35/1/variable)
    UFT_GEO_C64_1571,        ///< 340KB (70/2/variable)
    
    // Apple Formate
    UFT_GEO_APPLE_DOS,       ///< 140KB (35/1/16)
    UFT_GEO_APPLE_PRODOS,    ///< 140KB (35/1/16)
    UFT_GEO_APPLE_400K,      ///< 400KB (80/1/variable)
    UFT_GEO_APPLE_800K,      ///< 800KB (80/2/variable)
    
    // Atari Formate
    UFT_GEO_ATARI_SS_SD,     ///< 90KB (40/1/18, 128b)
    UFT_GEO_ATARI_SS_DD,     ///< 180KB (40/1/18, 256b)
    UFT_GEO_ATARI_ST_SS,     ///< 360KB (80/1/9)
    UFT_GEO_ATARI_ST_DS,     ///< 720KB (80/2/9)
    
    UFT_GEO_MAX
} uft_geometry_preset_t;

// ============================================================================
// Disk-Format (Container)
// ============================================================================

/**
 * @brief Unterstützte Disk-Image-Formate
 */
typedef enum uft_format {
    UFT_FORMAT_UNKNOWN = 0,
    
    // Sektor-Images (dekodiert)
    UFT_FORMAT_RAW,          ///< Raw sector dump
    UFT_FORMAT_IMG,          ///< Generic IMG/IMA
    UFT_FORMAT_ADF,          ///< Amiga Disk File
    UFT_FORMAT_D64,          ///< C64 Disk Image
    UFT_FORMAT_DSK,          ///< Generic DSK
    UFT_FORMAT_ST,           ///< Atari ST
    UFT_FORMAT_MSA,          ///< Atari MSA (compressed)
    UFT_FORMAT_STX,          ///< Atari STX (Pasti)
    UFT_FORMAT_IMD,          ///< ImageDisk
    
    // Flux-Images (raw)
    UFT_FORMAT_SCP,          ///< SuperCard Pro
    UFT_FORMAT_KF_STREAM,    ///< KryoFlux Stream
    UFT_FORMAT_KF_RAW,       ///< KryoFlux Raw
    UFT_FORMAT_KRYOFLUX,     ///< KryoFlux (alias)
    UFT_FORMAT_HFE,          ///< HxC Floppy Emulator
    UFT_FORMAT_IPF,          ///< Interchangeable Preservation Format
    UFT_FORMAT_CT_RAW,       ///< CatWeasel Raw
    UFT_FORMAT_A2R,          ///< Applesauce A2R
    UFT_FORMAT_FLUX,         ///< Generic Flux
    
    // Spezial
    UFT_FORMAT_G64,          ///< C64 GCR Image
    UFT_FORMAT_G71,          ///< C128 GCR Image
    UFT_FORMAT_NIB,          ///< Apple Nibble
    UFT_FORMAT_NBZ,          ///< Nibble Compressed
    UFT_FORMAT_WOZ,          ///< WOZ (Apple II)
    UFT_FORMAT_FDI,          ///< Formatted Disk Image
    UFT_FORMAT_TD0,          ///< Teledisk
    UFT_FORMAT_DMK,          ///< TRS-80 DMK
    UFT_FORMAT_D71,          ///< C128 D71
    UFT_FORMAT_D81,          ///< C128 D81
    UFT_FORMAT_D80,          ///< CBM D80
    UFT_FORMAT_D82,          ///< CBM D82
    UFT_FORMAT_ATR,          ///< Atari 8-bit
    UFT_FORMAT_XFD,          ///< Atari XFD
    UFT_FORMAT_SSD,          ///< BBC Micro SSD
    UFT_FORMAT_DSD,          ///< BBC Micro DSD
    UFT_FORMAT_TRD,          ///< TR-DOS
    UFT_FORMAT_SAD,          ///< SAM Coupe SAD
    UFT_FORMAT_DSK_CPC,      ///< Amstrad CPC DSK
    UFT_FORMAT_D88,          ///< PC-98/X68000 D88
    UFT_FORMAT_CQM,          ///< CopyQM
    UFT_FORMAT_DC42,         ///< DiskCopy 4.2 (Macintosh)
    UFT_FORMAT_2MG,          ///< Apple 2IMG
    UFT_FORMAT_DO,           ///< Apple DOS Order
    UFT_FORMAT_PO,           ///< Apple ProDOS Order
    UFT_FORMAT_EDSK,         ///< Extended DSK (Amstrad)
    UFT_FORMAT_JV1,          ///< JV1 TRS-80
    UFT_FORMAT_JV3,          ///< JV3 TRS-80
    UFT_FORMAT_ADF_ACORN,    ///< Acorn ADFS
    UFT_FORMAT_HDM,          ///< PC-98 HDM
    UFT_FORMAT_NFD,          ///< PC-98 NFD
    UFT_FORMAT_FDD,          ///< PC-98 FDD
    UFT_FORMAT_SCL,          ///< Sinclair SCL
    UFT_FORMAT_MSX_DSK,      ///< MSX DSK
    UFT_FORMAT_DSK_SAM,      ///< SAM Coupe DSK
    
    UFT_FORMAT_MAX
} uft_format_t;

/**
 * @brief Format-Eigenschaften
 */
typedef struct uft_format_info {
    uft_format_t  format;
    const char*   name;           ///< Kurzer Name
    const char*   description;    ///< Beschreibung
    const char*   extensions;     ///< File-Extensions (";"-getrennt)
    bool          has_flux;       ///< Enthält Flux-Daten
    bool          can_write;      ///< Schreibbar
    bool          preserves_timing; ///< Erhält Timing-Info
} uft_format_info_t;

// ============================================================================
// Encoding (Disk-Kodierung)
// ============================================================================

/**
 * @brief Disk-Kodierungen
 */
typedef enum uft_encoding {
    UFT_ENC_UNKNOWN = 0,
    
    // IBM-kompatibel
    UFT_ENC_FM,              ///< Single Density FM (generic)
    UFT_ENC_FM_SD,           ///< Single Density FM (SD drives)
    UFT_ENC_MFM,             ///< Double Density MFM (generic)
    UFT_ENC_MFM_SD,          ///< MFM Single Density
    UFT_ENC_MFM_DD,          ///< MFM Double Density
    UFT_ENC_MFM_HD,          ///< MFM High Density
    UFT_ENC_MFM_ED,          ///< MFM Extra Density
    
    // Amiga
    UFT_ENC_AMIGA_MFM,       ///< Amiga-spezifisches MFM
    
    // GCR (Commodore)
    UFT_ENC_GCR_CBM,         ///< C64/1541 GCR
    UFT_ENC_GCR_CBM_V,       ///< GCR mit Variationen (Copy Protection)
    
    // GCR (Apple)
    UFT_ENC_GCR_APPLE_525,   ///< Apple II 5.25"
    UFT_ENC_GCR_APPLE_35,    ///< Apple 3.5" (Sony GCR)
    
    // Andere
    UFT_ENC_MIXED,           ///< Gemischte Kodierung
    UFT_ENC_RLL,             ///< RLL Encoding
    UFT_ENC_M2FM,            ///< DEC M2FM (Modified MFM)
    UFT_ENC_GCR_VICTOR,      ///< Victor 9000 GCR
    UFT_ENC_AUTO,            ///< Auto-detect encoding
    
    UFT_ENC_MAX,
    
    // Aliases für Kompatibilität
    UFT_ENC_GCR_C64 = UFT_ENC_GCR_CBM,
    UFT_ENC_GCR_APPLE = UFT_ENC_GCR_APPLE_525,
} uft_encoding_t;

// ============================================================================
// Sektor-Strukturen
// ============================================================================

/**
 * @brief Sektor-ID (Address Mark)
 */
typedef struct uft_sector_id {
    uint8_t  cylinder;       ///< C - Zylinder (oft logisch, nicht physisch!)
    uint8_t  head;           ///< H - Seite
    uint8_t  sector;         ///< R - Sektornummer
    uint8_t  size_code;      ///< N - Größencode (2^N * 128)
    uint16_t crc;            ///< ID CRC
    bool     crc_ok;         ///< CRC valid?
} uft_sector_id_t;

/**
 * @brief Sektor-Status-Flags
 */
typedef enum uft_sector_status {
    UFT_SECTOR_OK           = 0,
    UFT_SECTOR_CRC_ERROR    = (1 << 0),  ///< Daten-CRC falsch
    UFT_SECTOR_ID_CRC_ERROR = (1 << 1),  ///< ID-CRC falsch
    UFT_SECTOR_MISSING      = (1 << 2),  ///< Sektor fehlt
    UFT_SECTOR_DELETED      = (1 << 3),  ///< Deleted Data Mark
    UFT_SECTOR_WEAK         = (1 << 4),  ///< Schwache/variable Bits
    UFT_SECTOR_DUPLICATE    = (1 << 5),  ///< Mehrfach vorhanden
    UFT_SECTOR_EXTRA        = (1 << 6),  ///< Über Normal hinaus
} uft_sector_status_t;

/* Sector status aliases */
#define UFT_SECTOR_HEADER_CRC_ERROR UFT_SECTOR_ID_CRC_ERROR
#define UFT_SECTOR_DATA_CRC_ERROR   UFT_SECTOR_CRC_ERROR

/**
 * @brief Einzelner Sektor
 */
typedef struct uft_sector {
    uft_sector_id_t  id;           ///< Sektor-ID
    uint8_t*         data;         ///< Sektor-Daten
    uint16_t         data_size;    ///< Tatsächliche Datengröße
    uint16_t         data_crc;     ///< Daten-CRC
    uint32_t         status;       ///< uft_sector_status_t Flags
    
    // Timing-Info (wenn verfügbar)
    uint32_t         bit_position; ///< Position im Track (Bits)
    uint32_t         gap_before;   ///< Gap-Größe vor diesem Sektor
} uft_sector_t;

// ============================================================================
// Track-Strukturen
// ============================================================================

/**
 * @brief Track-Status-Flags
 */
typedef enum uft_track_status {
    UFT_TRACK_OK            = 0,
    UFT_TRACK_READ_ERROR    = (1 << 0),
    UFT_TRACK_WRITE_ERROR   = (1 << 1),
    UFT_TRACK_UNFORMATTED   = (1 << 2),
    UFT_TRACK_PROTECTED     = (1 << 3),  ///< Copy protection detected
    UFT_TRACK_WEAK_BITS     = (1 << 4),
    UFT_TRACK_FUZZY         = (1 << 5),
} uft_track_status_t;

/**
 * @brief Track-Metriken
 */
typedef struct uft_track_metrics {
    double   rpm;                 ///< Gemessene RPM
    double   data_rate;           ///< Datenrate (bits/sec)
    uint32_t index_time_ns;       ///< Zeit zwischen Index-Pulsen
    uint32_t flux_count;          ///< Anzahl Flux-Transitions
    double   avg_bit_time_ns;     ///< Durchschnittliche Bit-Zeit
    double   bit_time_variance;   ///< Varianz der Bit-Zeiten
} uft_track_metrics_t;

// ============================================================================
// Callback-Typen
// ============================================================================

/**
 * @brief Progress-Callback
 * 
 * @param cylinder Aktueller Zylinder
 * @param head     Aktuelle Seite
 * @param percent  Fortschritt (0-100)
 * @param message  Status-Nachricht
 * @param user     User-Daten
 * @return false um Operation abzubrechen
 */
typedef bool (*uft_progress_fn)(int cylinder, int head, int percent,
                                 const char* message, void* user);

/**
 * @brief Log-Callback
 */
typedef enum uft_log_level {
    UFT_LOG_DEBUG = 0,
    UFT_LOG_INFO,
    UFT_LOG_WARN,
    UFT_LOG_ERROR
} uft_log_level_t;

typedef void (*uft_log_fn)(uft_log_level_t level, const char* message, void* user);

// ============================================================================
// Optionen
// ============================================================================

/**
 * @brief Lese-Optionen
 */
typedef struct uft_read_options {
    uint8_t  retries;            ///< Wiederholungen bei Fehler
    bool     ignore_crc_errors;  ///< CRC-Fehler ignorieren
    bool     read_deleted;       ///< Deleted sectors lesen
    bool     raw_mode;           ///< Raw-Modus (kein Dekodieren)
    uint16_t sync_word;          ///< Sync-Pattern (0 = auto)
} uft_read_options_t;

/**
 * @brief Schreib-Optionen
 */
typedef struct uft_write_options {
    bool     verify;             ///< Nach Schreiben verifizieren
    bool     format_track;       ///< Track komplett formatieren
    uint8_t  gap3_size;          ///< Gap3-Größe (0 = default)
    uint8_t  fill_byte;          ///< Füllbyte für Format
    int16_t  precomp_ns;         ///< Write Precompensation (-1 = auto)
} uft_write_options_t;

/**
 * @brief Konvertierungs-Optionen
 */
typedef struct uft_convert_options {
    uft_format_t    target_format;
    uft_encoding_t  target_encoding;
    uft_geometry_t  target_geometry;  ///< (0 = auto)
    bool            preserve_errors;  ///< Fehler erhalten
    bool            preserve_timing;  ///< Timing erhalten
    bool            normalize;        ///< Normalisieren
    uft_progress_fn progress;
    void*           progress_user;
} uft_convert_options_t;

// ============================================================================
// Default-Werte
// ============================================================================

/**
 * @brief Standard Lese-Optionen
 */
static inline uft_read_options_t uft_default_read_options(void) {
    return (uft_read_options_t){
        .retries = 3,
        .ignore_crc_errors = false,
        .read_deleted = true,
        .raw_mode = false,
        .sync_word = 0
    };
}

/**
 * @brief Standard Schreib-Optionen
 */
static inline uft_write_options_t uft_default_write_options(void) {
    return (uft_write_options_t){
        .verify = true,
        .format_track = false,
        .gap3_size = 0,
        .fill_byte = 0x4E,
        .precomp_ns = -1
    };
}

/**
 * @brief Geometrie für Preset holen
 */
uft_geometry_t uft_geometry_for_preset(uft_geometry_preset_t preset);

/**
 * @brief Format-Info holen
 */
const uft_format_info_t* uft_format_get_info(uft_format_t format);

/**
 * @brief Format aus Extension erraten
 */
uft_format_t uft_format_from_extension(const char* extension);

#ifdef __cplusplus
}
#endif

#endif // UFT_TYPES_H
