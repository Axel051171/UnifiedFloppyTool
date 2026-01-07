/**
 * @file uft_gui_params_extended.h
 * @brief UnifiedFloppyTool - Erweiterte GUI Parameter Mapping v3.1.4.010
 * 
 * Konsolidiert Parameter aus:
 * - UFT_ForensicImaging - Forensic Imager Settings
 * - UFT_HxC_Extract - Flux Profiles, Track/Sector/Flux API
 * - Bestehende UFT GUI Params (v3.1.4.004)
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_GUI_PARAMS_EXTENDED_H
#define UFT_GUI_PARAMS_EXTENDED_H

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TYPEN (aus uft_gui_params.h)
 *============================================================================*/

typedef float uft_percent_t;
typedef float uft_usec_t;
typedef int32_t uft_nsec_t;

/*============================================================================
 *============================================================================*/

/**
 */
typedef enum {
    UFT_PROC_NORMAL = 0,           /**< Standard-Decodierung */
    UFT_PROC_ADAPTIVE,             /**< Adaptive Thresholds */
    UFT_PROC_ADAPTIVE2,            /**< Adaptive v2 (Lowpass) */
    UFT_PROC_ADAPTIVE3,            /**< Adaptive v3 (Enhanced) */
    UFT_PROC_ADAPTIVE_ENTROPY,     /**< Entropy-basiert */
    UFT_PROC_ADAPTIVE_PREDICT,     /**< Predictive */
    UFT_PROC_AUFIT,                /**< AUFIT Algorithmus */
    UFT_PROC_WD1772_DPLL,          /**< WD1772 DPLL Emulation */
    UFT_PROC_MAME_PLL,             /**< MAME-style PLL */
    UFT_PROC_COUNT
} uft_processing_type_t;

/**
 */
typedef enum {
    UFT_PLATFORM_AUTO = 0,
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_AMIGA_HD,
    UFT_PLATFORM_AMIGA_DISKSPARE,
    UFT_PLATFORM_PC_DD,            /**< IBM PC 720K */
    UFT_PLATFORM_PC_HD,            /**< IBM PC 1.44M */
    UFT_PLATFORM_PC_2M,            /**< 2M Format */
    UFT_PLATFORM_PC_SS,            /**< Single Sided */
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_BBC_DFS,
    UFT_PLATFORM_C64_1541,
    UFT_PLATFORM_APPLE_DOS33,
    UFT_PLATFORM_APPLE_PRODOS,
    UFT_PLATFORM_MAC_GCR,
    UFT_PLATFORM_COUNT
} uft_platform_t;

/**
 * @brief Flux Encoding (aus UFT_HxC_Extract uft_fluxprofiles.h)
 */
typedef enum {
    UFT_ENC_AUTO = 0,
    UFT_ENC_FM = 1,
    UFT_ENC_MFM = 2,
    UFT_ENC_GCR = 3,
    UFT_ENC_APPLE_GCR = 4,
    UFT_ENC_MAC_GCR = 5,
    UFT_ENC_CUSTOM = 255
} uft_encoding_t;

/**
 */
typedef enum {
    UFT_DISKFMT_UNKNOWN = 0,
    UFT_DISKFMT_AMIGADOS,
    UFT_DISKFMT_DISKSPARE,
    UFT_DISKFMT_DISKSPARE_984KB,
    UFT_DISKFMT_PC_DD,
    UFT_DISKFMT_PC_HD,
    UFT_DISKFMT_PC_SS_DD,
    UFT_DISKFMT_PC_2M,
    UFT_DISKFMT_COUNT
} uft_disk_format_t;

/*============================================================================
 *============================================================================*/

/**
 * @brief Disk Geometry Parameter
 */
typedef struct uft_gui_geometry {
    /* Basic Geometry */
    int32_t tracks;               /**< Anzahl Tracks (35-84) */
    int32_t heads;                /**< Anzahl Köpfe (1-2) */
    int32_t sectors_per_track;    /**< Sektoren pro Track (8-22) */
    int32_t sector_size;          /**< Bytes pro Sektor (128-8192) */
    
    /* Encoding */
    uft_encoding_t encoding;      /**< FM/MFM/GCR */
    
    /* Computed (read-only für GUI) */
    uint64_t total_size;          /**< Berechnete Gesamtgröße */
    bool valid;                   /**< Geometrie plausibel? */
    
    /* Extended (für Track-Formate) */
    int32_t interleave;           /**< Sector Interleave (1-n) */
    int32_t skew;                 /**< Track-to-Track Skew */
    int32_t gap3_size;            /**< Gap 3 Größe (Bytes) */
} uft_gui_geometry_t;

/*============================================================================
 *============================================================================*/

/**
 * 
 * Diese Werte definieren die Grenzen für 4µs/6µs/8µs Pulse.
 * Hier: Als µs-Werte für GUI-Kompatibilität
 */
typedef struct uft_gui_mfm_timing {
    int32_t offset;               /**< Basis-Offset (Default: 0) */
    int32_t min;                  /**< Minimum Sample Count */
    int32_t four;                 /**< 4µs Threshold (2T) */
    int32_t six;                  /**< 6µs Threshold (3T) */
    int32_t max;                  /**< Maximum Sample Count */
    
    /* Als µs (für GUI-Display) */
    uft_usec_t thresh_4us;        /**< 4µs in µs */
    uft_usec_t thresh_6us;        /**< 6µs in µs */
    uft_usec_t thresh_8us;        /**< 8µs in µs */
    
    /* HD Modus */
    int32_t hd_shift;             /**< HD Bit-Shift (0=DD, 1=HD) */
} uft_gui_mfm_timing_t;

/**
 */
typedef struct uft_gui_adaptive_processing {
    float rate_of_change;         /**< Adaptionsrate (1.0-16.0, Default: 4.0) */
    float rate_of_change2;        /**< Lowpass Radius als Float */
    
    /* Als GUI-freundliche Prozent-Werte */
    uft_percent_t adapt_rate_pct; /**< 6-100% (100/rate_of_change) */
    
    /* Lowpass Filter */
    int32_t lowpass_radius;       /**< 0-1024 (Default: 100) */
    
    /* Adaptive Offset */
    int32_t adapt_offset;         /**< Integer Offset */
    float adapt_offset2;          /**< Float Offset */
    
    /* Entropy Tracking */
    bool use_entropy;             /**< Entropy-basierte Adaption */
    
    /* Noise Injection (für Testing) */
    bool add_noise;               /**< Rauschen hinzufügen */
    int32_t noise_limit_start;    /**< Noise Start Offset */
    int32_t noise_limit_end;      /**< Noise End Offset */
    int32_t noise_amount;         /**< Rausch-Amplitude */
} uft_gui_adaptive_processing_t;

/**
 */
typedef struct uft_gui_proc_settings {
    /* Basis */
    uft_processing_type_t proc_type;  /**< Algorithmus-Auswahl */
    uft_platform_t platform;          /**< Ziel-Platform */
    
    /* MFM Timing */
    uft_gui_mfm_timing_t timing;
    
    /* Adaptive */
    uft_gui_adaptive_processing_t adaptive;
    
    /* Processing Range */
    int32_t start;                /**< Start Offset in Buffer */
    int32_t end;                  /**< End Offset in Buffer */
    int32_t pattern;              /**< Pattern-Suche (0-4) */
    
    /* Options */
    bool skip_period_data;        /**< Period-Daten überspringen */
    bool find_dupes;              /**< Duplikate finden */
    bool use_error_correction;    /**< EC aktivieren */
    bool only_bad_sectors;        /**< Nur defekte Sektoren */
    bool ignore_header_error;     /**< Header-Fehler ignorieren */
    bool auto_refresh_sectormap;  /**< Sectormap auto-aktualisieren */
    
    /* Track/Sector Limitierung */
    bool limit_ts_on;             /**< Limitierung aktiv */
    int32_t limit_to_track;       /**< Auf Track limitieren */
    int32_t limit_to_sector;      /**< Auf Sektor limitieren */
    
    /* Duplikate */
    int32_t number_of_dups;       /**< Anzahl Duplikate */
    
    /* Output */
    char output_filename[256];    /**< Ausgabe-Dateiname */
} uft_gui_proc_settings_t;

/*============================================================================
 *============================================================================*/

/**
 * @brief Error Correction Parameter
 */
typedef struct uft_gui_ec_settings {
    /* Period Selection */
    int32_t period_start;         /**< Period Selection Start */
    int32_t period_end;           /**< Period Selection End */
    
    /* Index */
    int32_t index_s1;             /**< Index S1 */
    
    /* Threading */
    int32_t thread_id;            /**< Thread ID */
    
    /* Combinations */
    int32_t combinations;         /**< Anzahl Kombinationen */
    
    /* C6/C8 Start */
    int32_t c6_start;             /**< C6 Start */
    int32_t c8_start;             /**< C8 Start */
    
    /* MFM Byte Range */
    int32_t mfm_byte_start;       /**< MFM Byte Start */
    int32_t mfm_byte_length;      /**< MFM Byte Length */
} uft_gui_ec_settings_t;

/*============================================================================
 *============================================================================*/

/**
 */
typedef struct uft_gui_dpll_settings {
    /* Clock Base */
    int32_t pll_clk;              /**< PLL Clock (Default: 80 = 8MHz/50ns) */
    
    /* Phase Correction */
    int32_t phase_correction;     /**< Phase Correction (Default: 90) */
    int32_t low_correction;       /**< 128 - phase_correction */
    int32_t high_correction;      /**< 128 + phase_correction */
    
    /* Period Count Bounds */
    int32_t low_stop;             /**< Lower Bound (Default: 115) */
    int32_t high_stop;            /**< Upper Bound (Default: 141) */
    
    /* Density */
    bool high_density;            /**< HD Mode */
    
    /* GUI-freundliche Versionen */
    uft_percent_t phase_adjust_pct;  /**< Phase Adjust als % (Default: ~70%) */
    uft_percent_t period_min_pct;    /**< Min Period als % (Default: ~90%) */
    uft_percent_t period_max_pct;    /**< Max Period als % (Default: ~110%) */
} uft_gui_dpll_settings_t;

/*============================================================================
 * FLUX PROFILE (aus UFT_HxC_Extract uft_fluxprofiles.h)
 *============================================================================*/

/**
 * @brief Symbol Range für Flux-Klassifikation
 */
typedef struct uft_gui_symbol_range {
    uint32_t min_ticks;           /**< Minimum Ticks */
    uint32_t max_ticks;           /**< Maximum Ticks */
    uint8_t symbol_id;            /**< Symbol ID (2T/3T/4T für MFM) */
    char name[16];                /**< GUI-Display Name ("2T", "3T", etc.) */
} uft_gui_symbol_range_t;

/**
 * @brief Flux Profile Definition
 */
typedef struct uft_gui_flux_profile {
    uint32_t profile_id;          /**< Unique ID */
    char name[64];                /**< Human-readable Name */
    
    /* Encoding */
    uft_encoding_t encoding;      /**< FM/MFM/GCR */
    
    /* Timing */
    uint32_t tick_hz;             /**< Measurement Clock Hz */
    uint32_t nominal_bitrate;     /**< Nominal Bitrate */
    uint32_t rotation_us;         /**< Rotation Period µs (optional) */
    
    /* Jitter Tolerance */
    uint32_t jitter_abs_ticks;    /**< Absolute Jitter Ticks */
    uint32_t jitter_rel_ppm;      /**< Relative Jitter PPM */
    
    /* Symbol Ranges */
    uft_gui_symbol_range_t ranges[8]; /**< Symbol Classification */
    uint32_t ranges_count;
    
    /* GUI-freundliche Werte */
    uft_usec_t cell_time_us;      /**< Nominale Zellzeit µs */
    uft_percent_t jitter_pct;     /**< Jitter als % */
} uft_gui_flux_profile_t;

/*============================================================================
 * FORENSIC IMAGING SETTINGS (aus UFT_ForensicImaging)
 *============================================================================*/

/**
 * @brief Forensic Imaging Parameter (für Forensic Tab)
 */
typedef struct uft_gui_forensic_settings {
    /* Block Size */
    uint32_t block_size;          /**< Block Size (512-65536) */
    
    /* Retry Policy */
    uint32_t max_retries;         /**< Max Retries (0-10) */
    int32_t retry_delay_ms;       /**< Retry Delay ms (0-5000) */
    
    /* Recovery Mode */
    bool reverse_mode;            /**< Von hinten nach vorne */
    bool fill_bad_blocks;         /**< Bad Blocks füllen */
    uint8_t fill_pattern;         /**< Fill Pattern (Default: 0x00) */
    
    /* Hashing */
    bool hash_md5;                /**< MD5 aktivieren */
    bool hash_sha1;               /**< SHA1 aktivieren */
    bool hash_sha256;             /**< SHA256 aktivieren */
    bool hash_sha512;             /**< SHA512 aktivieren */
    
    /* Split Output */
    bool split_output;            /**< Split aktivieren */
    uint64_t split_size;          /**< Split Size Bytes */
    char split_format[16];        /**< "000"/"aaa"/"MAC"/"WIN" */
    
    /* Verification */
    bool verify_after_write;      /**< Nach Schreiben verifizieren */
    
    /* Logging */
    char log_path[256];           /**< Log-Datei Pfad */
    bool verbose_log;             /**< Verbose Logging */
} uft_gui_forensic_settings_t;

/*============================================================================
 *============================================================================*/

typedef enum {
    UFT_SECTOR_EMPTY = 0,         /**< Nicht gelesen */
    UFT_SECTOR_HEAD_BAD,          /**< Header CRC Fehler */
    UFT_SECTOR_HEAD_OK_DATA_BAD,  /**< Header OK, Data CRC Fehler */
    UFT_SECTOR_OK,                /**< Komplett OK */
    UFT_SECTOR_DELETED,           /**< Deleted Data Mark */
    UFT_SECTOR_WEAK,              /**< Weak/Unstable Data */
    UFT_SECTOR_PROTECTED          /**< Copy Protection detected */
} uft_sector_status_t;

/**
 * @brief Sector Flags (aus UFT_HxC uft_sector_api.h)
 */
typedef struct uft_gui_sector_flags {
    bool crc_ok;                  /**< CRC korrekt */
    bool deleted_dam;             /**< Deleted Data Address Mark */
    bool id_duplicate;            /**< Doppelte Sektor-ID */
    bool missing_dam;             /**< Fehlender Data Address Mark */
    bool weak_overlap;            /**< Weak/Overlap Region */
} uft_gui_sector_flags_t;

/*============================================================================
 * TRACK ANOMALIES (aus UFT_HxC uft_track_api.h)
 *============================================================================*/

typedef enum {
    UFT_ANOMALY_CRC_BAD = 0,
    UFT_ANOMALY_MISSING_AM,
    UFT_ANOMALY_DUP_ID,
    UFT_ANOMALY_WEAK_REGION,
    UFT_ANOMALY_TRACKLEN_ODD,
    UFT_ANOMALY_GAP_ODD,
    UFT_ANOMALY_DENSITY_CHANGE,
    UFT_ANOMALY_SYNC_MISSING
} uft_anomaly_type_t;

typedef struct uft_gui_anomaly {
    uft_anomaly_type_t type;
    uint32_t bit_offset;
    uint32_t severity;            /**< 0-100 */
    char description[64];
} uft_gui_anomaly_t;

/*============================================================================
 * MASTER GUI SETTINGS STRUKTUR
 *============================================================================*/

/**
 * @brief Komplette GUI Settings für UFT
 * 
 * Diese Struktur enthält ALLE Parameter für die GUI.
 * Sie kann serialisiert/deserialisiert werden für Presets/Profiles.
 */
typedef struct uft_gui_master_settings {
    /* Version */
    uint32_t version;             /**< Settings Version */
    
    /* Geometry */
    uft_gui_geometry_t geometry;
    
    /* Processing */
    uft_gui_proc_settings_t processing;
    
    /* Error Correction */
    uft_gui_ec_settings_t error_correction;
    
    /* DPLL */
    uft_gui_dpll_settings_t dpll;
    
    /* Flux Profile */
    uft_gui_flux_profile_t flux_profile;
    
    /* Forensic */
    uft_gui_forensic_settings_t forensic;
    
    /* Metadata */
    char preset_name[64];         /**< Preset Name */
    char description[256];        /**< Beschreibung */
    uint64_t last_modified;       /**< Timestamp */
} uft_gui_master_settings_t;

/*============================================================================
 * PRESET MANAGEMENT
 *============================================================================*/

/**
 * @brief Preset-IDs für Quick-Select
 */
typedef enum {
    /* Standard Formate */
    UFT_PRESET_AUTO = 0,
    UFT_PRESET_AMIGA_DD,
    UFT_PRESET_AMIGA_HD,
    UFT_PRESET_PC_DD,
    UFT_PRESET_PC_HD,
    UFT_PRESET_ATARI_ST,
    UFT_PRESET_BBC_DFS,
    UFT_PRESET_C64_1541,
    UFT_PRESET_APPLE_DOS33,
    
    /* Spezielle Modi */
    UFT_PRESET_DIRTY_DUMP,        /**< Beschädigte Disks */
    UFT_PRESET_COPY_PROTECTION,   /**< Kopierschutz-Analyse */
    UFT_PRESET_FORENSIC,          /**< Forensische Analyse */
    
    /* Custom */
    UFT_PRESET_CUSTOM,
    
    UFT_PRESET_COUNT
} uft_preset_id_t;

/*============================================================================
 * FUNKTIONEN
 *============================================================================*/

/**
 * @brief Default Settings initialisieren
 */
void uft_gui_settings_init_default(uft_gui_master_settings_t* settings);

/**
 * @brief Preset laden
 */
bool uft_gui_settings_load_preset(uft_preset_id_t preset,
                                   uft_gui_master_settings_t* settings);

/**
 * @brief Settings von Datei laden
 */
bool uft_gui_settings_load_file(const char* path,
                                 uft_gui_master_settings_t* settings);

/**
 * @brief Settings in Datei speichern
 */
bool uft_gui_settings_save_file(const char* path,
                                 const uft_gui_master_settings_t* settings);

/**
 * @brief Geometry aus Dateigröße ableiten
 */
bool uft_gui_geometry_from_size(uint64_t file_size,
                                 uft_gui_geometry_t* geometry);

/**
 * @brief Processing Settings validieren
 */
bool uft_gui_proc_settings_validate(const uft_gui_proc_settings_t* settings,
                                     char* error_msg, size_t error_len);

/**
 * @brief Flux Profile für Platform generieren
 */
void uft_gui_flux_profile_for_platform(uft_platform_t platform,
                                        uft_gui_flux_profile_t* profile);

/**
 * @brief DPLL Settings aus Phase/Freq Prozent
 */
void uft_gui_dpll_from_percent(uft_percent_t phase_pct,
                                uft_percent_t freq_pct,
                                uft_gui_dpll_settings_t* dpll);

/**
 * @brief Adaptive Settings aus Rate-of-Change
 */
void uft_gui_adaptive_from_roc(float rate_of_change,
                                int32_t lowpass_radius,
                                uft_gui_adaptive_processing_t* adaptive);

/**
 * @brief Preset Name abrufen
 */
const char* uft_gui_preset_name(uft_preset_id_t preset);

/**
 * @brief Platform Name abrufen
 */
const char* uft_gui_platform_name(uft_platform_t platform);

/**
 * @brief Processing Type Name abrufen
 */
const char* uft_gui_proc_type_name(uft_processing_type_t type);

/**
 * @brief Encoding Name abrufen
 */
const char* uft_gui_encoding_name(uft_encoding_t encoding);

/*============================================================================
 * DEFAULT WERTE
 *============================================================================*/

/* Processing Defaults */
#define UFT_GUI_DEFAULT_ROC             4.0f
#define UFT_GUI_DEFAULT_LOWPASS         100
#define UFT_GUI_DEFAULT_THRESH_4US      20      /* Sample counts @ 10MHz */
#define UFT_GUI_DEFAULT_THRESH_6US      30
#define UFT_GUI_DEFAULT_THRESH_8US      40

/* DPLL Defaults */
#define UFT_GUI_DEFAULT_PLL_CLK         80
#define UFT_GUI_DEFAULT_PHASE_CORR      90
#define UFT_GUI_DEFAULT_LOW_STOP        115
#define UFT_GUI_DEFAULT_HIGH_STOP       141

/* Forensic Defaults */
#define UFT_GUI_DEFAULT_BLOCK_SIZE      512
#define UFT_GUI_DEFAULT_MAX_RETRIES     3
#define UFT_GUI_DEFAULT_RETRY_DELAY     100
#define UFT_GUI_DEFAULT_SPLIT_SIZE      (4ULL * 1024 * 1024 * 1024) /* 4GB */

/* Geometry Presets */
#define UFT_GEOM_AMIGA_DD               { 80, 2, 11, 512, UFT_ENC_MFM, 901120, true, 1, 0, 0 }
#define UFT_GEOM_AMIGA_HD               { 80, 2, 22, 512, UFT_ENC_MFM, 1802240, true, 1, 0, 0 }
#define UFT_GEOM_PC_DD                  { 80, 2, 9, 512, UFT_ENC_MFM, 737280, true, 1, 0, 0 }
#define UFT_GEOM_PC_HD                  { 80, 2, 18, 512, UFT_ENC_MFM, 1474560, true, 1, 0, 0 }
#define UFT_GEOM_C64_1541               { 35, 1, 21, 256, UFT_ENC_GCR, 174848, true, 1, 0, 0 }

/*============================================================================
 * GUI WIDGET HELPERS
 *============================================================================*/

/**
 * @brief Slider-Konfiguration
 */
typedef struct uft_gui_slider_config {
    float min_value;
    float max_value;
    float default_value;
    float step;
    const char* label;
    const char* unit;
    const char* tooltip;
} uft_gui_slider_config_t;

/**
 * @brief Combobox-Item
 */
typedef struct uft_gui_combo_item {
    int32_t id;
    const char* text;
    const char* tooltip;
} uft_gui_combo_item_t;

/* Slider Configs */
uft_gui_slider_config_t uft_gui_get_slider_roc(void);
uft_gui_slider_config_t uft_gui_get_slider_lowpass(void);
uft_gui_slider_config_t uft_gui_get_slider_phase(void);
uft_gui_slider_config_t uft_gui_get_slider_freq(void);
uft_gui_slider_config_t uft_gui_get_slider_retries(void);

/* Combo Items */
const uft_gui_combo_item_t* uft_gui_get_platforms(int* count);
const uft_gui_combo_item_t* uft_gui_get_proc_types(int* count);
const uft_gui_combo_item_t* uft_gui_get_encodings(int* count);
const uft_gui_combo_item_t* uft_gui_get_presets(int* count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GUI_PARAMS_EXTENDED_H */
