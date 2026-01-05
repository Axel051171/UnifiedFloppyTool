/**
 * @file uft_canonical_params.h
 * @brief Kanonisches Parameter-System für UFT
 * 
 * Definiert das universelle Parameter-Format, das zwischen
 * allen Tools und GUI-Elementen austauschbar ist.
 */

#ifndef UFT_CANONICAL_PARAMS_H
#define UFT_CANONICAL_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** Format-Typen */
typedef enum {
    UFT_FORMAT_AUTO = 0,
    UFT_FORMAT_RAW,
    UFT_FORMAT_IMG,
    UFT_FORMAT_ADF,
    UFT_FORMAT_D64,
    UFT_FORMAT_G64,
    UFT_FORMAT_D71,
    UFT_FORMAT_D81,
    UFT_FORMAT_ST,
    UFT_FORMAT_MSA,
    UFT_FORMAT_STX,
    UFT_FORMAT_SCP,
    UFT_FORMAT_HFE,
    UFT_FORMAT_IPF,
    UFT_FORMAT_KF_STREAM,
    UFT_FORMAT_WOZ,
    UFT_FORMAT_NIB,
    UFT_FORMAT_IMD,
    UFT_FORMAT_DMK,
    UFT_FORMAT_DSK,
    UFT_FORMAT_DO,
    UFT_FORMAT_PO,
    UFT_FORMAT_2MG,
    UFT_FORMAT_DC42,
    UFT_FORMAT_SSD,
    UFT_FORMAT_DSD,
    UFT_FORMAT_TRD,
    UFT_FORMAT_DSK_CPC,
    UFT_FORMAT_EDSK,
    UFT_FORMAT_D88,
    UFT_FORMAT_HDM,
    UFT_FORMAT_JV1,
    UFT_FORMAT_JV3,
    UFT_FORMAT_ADF_ACORN,
    UFT_FORMAT_SCL,
    UFT_FORMAT_MSX_DSK,
    UFT_FORMAT_MAX
} uft_format_e;

/** Encoding-Typen */
typedef enum {
    UFT_ENC_AUTO = 0,
    UFT_ENC_FM,
    UFT_ENC_MFM,
    UFT_ENC_M2FM,
    UFT_ENC_GCR_CBM,
    UFT_ENC_GCR_APPLE,
    UFT_ENC_GCR_VICTOR,
    UFT_ENC_AMIGA_MFM,
    UFT_ENC_RLL,
    UFT_ENC_MIXED,
    UFT_ENC_MAX
} uft_encoding_e;

/** Density-Typen */
typedef enum {
    UFT_DENSITY_AUTO = 0,
    UFT_DENSITY_SD,     /**< Single Density (FM) */
    UFT_DENSITY_DD,     /**< Double Density (MFM) */
    UFT_DENSITY_HD,     /**< High Density */
    UFT_DENSITY_ED,     /**< Extra Density */
    UFT_DENSITY_MAX
} uft_density_e;

/** Drive-Typen */
typedef enum {
    UFT_DRIVE_AUTO = 0,
    UFT_DRIVE_525_DD,
    UFT_DRIVE_525_HD,
    UFT_DRIVE_35_DD,
    UFT_DRIVE_35_HD,
    UFT_DRIVE_35_ED,
    UFT_DRIVE_8_SD,
    UFT_DRIVE_8_DD,
    UFT_DRIVE_MAX
} uft_drive_e;

/** Tool-Typen */
typedef enum {
    UFT_TOOL_INTERNAL = 0,
    UFT_TOOL_GREASEWEAZLE,
    UFT_TOOL_FLUXENGINE,
    UFT_TOOL_KRYOFLUX,
    UFT_TOOL_SCP,
    UFT_TOOL_ANADISK,
    UFT_TOOL_DISK2FDI,
    UFT_TOOL_HXCFE,
    UFT_TOOL_MAX
} uft_tool_e;

/** Parameter-Flags */
typedef enum {
    UFT_PFLAG_NONE      = 0x0000,
    UFT_PFLAG_DIRTY     = 0x0001,
    UFT_PFLAG_COMPUTED  = 0x0002,
    UFT_PFLAG_LOCKED    = 0x0004,
    UFT_PFLAG_INHERITED = 0x0008
} uft_param_flags_e;

/* ============================================================================
 * Structures
 * ============================================================================ */

/** Geometrie-Parameter */
typedef struct {
    int32_t cylinders;           /**< Anzahl Zylinder */
    int32_t heads;               /**< Anzahl Köpfe (1-2) */
    int32_t sectors_per_track;   /**< Sektoren pro Track */
    int32_t sector_size;         /**< Sektorgröße (Bytes) */
    int32_t cylinder_start;      /**< Start-Zylinder */
    int32_t cylinder_end;        /**< End-Zylinder (-1 = bis Ende) */
    uint32_t head_mask;          /**< Kopfmaske (Bit 0=Head0, Bit 1=Head1) */
    int32_t sector_base;         /**< Erste Sektornummer (0 oder 1) */
    int32_t interleave;          /**< Sektor-Interleave */
    int32_t skew;                /**< Track-Skew */
    uint32_t total_sectors;      /**< Computed: Gesamtsektoren */
    uint64_t total_bytes;        /**< Computed: Gesamtgröße */
    uint32_t flags;
} uft_geom_t;

/** Timing-Parameter */
typedef struct {
    uint64_t cell_time_ns;       /**< Bit-Zellzeit in ns */
    uint64_t rotation_ns;        /**< Rotationszeit in ns */
    uint32_t datarate_bps;       /**< Datenrate in bps */
    double rpm;                  /**< Drehzahl */
    double pll_phase_adjust;     /**< PLL Phasen-Einstellung */
    double pll_period_adjust;    /**< PLL Perioden-Einstellung */
    double pll_period_min;       /**< PLL Min-Periode */
    double pll_period_max;       /**< PLL Max-Periode */
    double weak_threshold;       /**< Schwellwert für Weak Bits */
    uint32_t flags;
} uft_timing_t;

/** CBM-spezifische Parameter */
typedef struct {
    bool half_tracks;
    bool error_map;
    int32_t track_range;
} uft_cbm_params_t;

/** Amiga-spezifische Parameter */
typedef struct {
    int32_t filesystem;
    bool bootable;
} uft_amiga_params_t;

/** IBM-spezifische Parameter */
typedef struct {
    int32_t gap0_bytes;
    int32_t gap1_bytes;
    int32_t gap2_bytes;
    int32_t gap3_bytes;
} uft_ibm_params_t;

/** Format-Parameter */
typedef struct {
    uft_format_e input_format;
    uft_format_e output_format;
    uft_encoding_e encoding;
    uft_density_e density;
    uft_cbm_params_t cbm;
    uft_amiga_params_t amiga;
    uft_ibm_params_t ibm;
    uint32_t flags;
} uft_format_params_t;

/** Hardware-spezifische Parameter */
typedef struct {
    int32_t bus_type;
    int32_t drive_select;
    int32_t densel_polarity;
} uft_hw_config_t;

/** Hardware-Parameter */
typedef struct {
    char device_path[256];
    int32_t device_index;
    uft_drive_e drive_type;
    bool double_step;
    uft_tool_e tool;
    uft_hw_config_t hw;
    uint32_t flags;
} uft_hardware_t;

/** Operations-Parameter */
typedef struct {
    bool dry_run;
    bool verify_after_write;
    int32_t retries;
    int32_t revolutions;
    bool attempt_recovery;
    bool preserve_errors;
    bool verbose;
    bool generate_audit;
    char audit_path[256];
    uint32_t flags;
} uft_operation_t;

/** Kanonische Parameter-Struktur */
typedef struct {
    uint32_t struct_size;        /**< Strukturgröße (für Versionierung) */
    uint32_t version;            /**< Struktur-Version */
    
    uft_geom_t geometry;         /**< Disk-Geometrie */
    uft_timing_t timing;         /**< Timing-Parameter */
    uft_format_params_t format;  /**< Format-Parameter */
    uft_hardware_t hardware;     /**< Hardware-Parameter */
    uft_operation_t operation;   /**< Operations-Parameter */
    
    bool validated;              /**< Validierungs-Flag */
    char error_msg[256];         /**< Fehlermeldung */
    char source[64];             /**< Quelle der Parameter */
    uint32_t flags;              /**< Globale Flags */
} uft_canonical_params_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/** Initialisiere Parameter mit Defaults */
uft_canonical_params_t uft_params_init(void);

/** Erstelle Parameter für ein spezifisches Format */
uft_canonical_params_t uft_params_for_format(uft_format_e format);

/** Lade Preset nach Name */
int uft_params_from_preset(const char *preset_name, uft_canonical_params_t *params);

/** Berechne Cell-Time aus Datarate */
uint64_t uft_params_compute_cell_time(uint32_t datarate_bps, uft_encoding_e enc);

/** Berechne Datarate aus Cell-Time */
uint32_t uft_params_compute_datarate(uint64_t cell_time_ns, uft_encoding_e enc);

/** Berechne abgeleitete Werte neu */
void uft_params_recompute(uft_canonical_params_t *params);

/** Validiere Parameter */
int uft_params_validate(uft_canonical_params_t *params);

/** Prüfe ob Parameter valide sind */
bool uft_params_is_valid(const uft_canonical_params_t *params);

/** Hole Fehlermeldungen */
int uft_params_get_errors(const uft_canonical_params_t *params,
                          const char **errors, int max_count);

/** Löse Alias für Tool auf */
const char *uft_params_resolve_alias(const char *alias, uft_tool_e tool);

/** Setze Integer-Parameter */
int uft_params_set_int(uft_canonical_params_t *params, 
                       const char *name, int64_t value);

/** Setze Double-Parameter */
int uft_params_set_double(uft_canonical_params_t *params,
                          const char *name, double value);

/** Setze Bool-Parameter */
int uft_params_set_bool(uft_canonical_params_t *params,
                        const char *name, bool value);

/** Hole Integer-Parameter */
int64_t uft_params_get_int(const uft_canonical_params_t *params, 
                           const char *name);

/** Hole Double-Parameter */
double uft_params_get_double(const uft_canonical_params_t *params,
                             const char *name);

/** Hole Bool-Parameter */
bool uft_params_get_bool(const uft_canonical_params_t *params,
                         const char *name);

/** Konvertiere zu CLI-Argumenten für ein Tool */
int uft_params_to_cli(const uft_canonical_params_t *params,
                      uft_tool_e tool,
                      char *buffer, size_t buffer_size);

/** Hole Tool-spezifisches Flag */
const char *uft_params_get_tool_flag(const char *canonical, uft_tool_e tool);

/** Formatiere für GUI-Anzeige */
const char *uft_params_format_for_gui(const uft_canonical_params_t *params,
                                       char *buffer, size_t buffer_size);

/** Exportiere als JSON */
int uft_params_to_json(const uft_canonical_params_t *params,
                       char *buffer, size_t buffer_size);

/** Importiere von JSON */
int uft_params_from_json(const char *json, uft_canonical_params_t *params);

/* ============================================================================
 * Preset API
 * ============================================================================ */

/** Anzahl verfügbarer Presets */
int uft_preset_count(void);

/** Liste alle Preset-Namen */
int uft_preset_list(const char **names, int max_count);

/** Liste Presets einer Kategorie */
int uft_preset_list_by_category(const char *category, const char **names, int max_count);

/** Liste alle Kategorien */
int uft_preset_get_categories(const char **categories, int max_count);

/** Hole Preset-Beschreibung */
const char *uft_preset_get_description(const char *name);

/** Wende Preset an */
int uft_preset_apply(const char *name, uft_canonical_params_t *params);

/** Hole erwartete Dateigröße für Preset */
uint32_t uft_preset_get_expected_size(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CANONICAL_PARAMS_H */
