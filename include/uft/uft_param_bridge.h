#ifndef UFT_PARAM_BRIDGE_H
#define UFT_PARAM_BRIDGE_H

/**
 * @file uft_param_bridge.h
 * @brief CLI-GUI Parameter Bridge - Bidirektionale Konvertierung
 * 
 * @version 5.0.0
 * @date 2026-01-03
 * 
 * TICKET-004: CLI-GUI Parameter Bridge
 * Priority: MUST
 * 
 * ZWECK:
 * Bidirektionale Konvertierung zwischen CLI-Argumenten, JSON-Parametern
 * und GUI-Settings für vollständige Reproduzierbarkeit.
 * 
 * FEATURES:
 * - CLI-Args → JSON → CLI Round-Trip ohne Verlust
 * - GUI kann JSON importieren/exportieren
 * - CLI --export-session erzeugt reproduzierbare JSON
 * - Alle 52 Presets haben CLI-Äquivalent
 * - Dokumentation der Mapping-Regeln
 * 
 * DATENFLUSS:
 * ```
 *   CLI Arguments ←→ uft_params_t ←→ JSON String
 *         ↑               ↓               ↓
 *         └───────← GUI Settings ←────────┘
 * ```
 * 
 * USAGE:
 * ```c
 * // CLI → Params → JSON
 * uft_params_t *params = uft_params_from_cli(argc, argv);
 * char *json = uft_params_to_json(params);
 * 
 * // JSON → Params → CLI
 * uft_params_t *params = uft_params_from_json(json);
 * char *cli = uft_params_to_cli(params);
 * 
 * // GUI Integration
 * uft_params_apply_to_gui(params, gui_context);
 * uft_params_from_gui(params, gui_context);
 * 
 * // Export Session
 * uft_session_export_cli(session, "run_again.sh");
 * ```
 */


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parameter Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_PARAM_TYPE_BOOL,
    UFT_PARAM_TYPE_INT,
    UFT_PARAM_TYPE_FLOAT,
    UFT_PARAM_TYPE_STRING,
    UFT_PARAM_TYPE_ENUM,
    UFT_PARAM_TYPE_PATH,
    UFT_PARAM_TYPE_RANGE,       /* int mit min/max */
} uft_param_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parameter Categories
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_PARAM_CAT_GENERAL       = 0,    /* Allgemeine Optionen */
    UFT_PARAM_CAT_FORMAT        = 1,    /* Format-Optionen */
    UFT_PARAM_CAT_HARDWARE      = 2,    /* Hardware-Optionen */
    UFT_PARAM_CAT_RECOVERY      = 3,    /* Recovery-Optionen */
    UFT_PARAM_CAT_ENCODING      = 4,    /* Encoding-Optionen */
    UFT_PARAM_CAT_PLL           = 5,    /* PLL-Optionen */
    UFT_PARAM_CAT_OUTPUT        = 6,    /* Output-Optionen */
    UFT_PARAM_CAT_DEBUG         = 7,    /* Debug-Optionen */
    UFT_PARAM_CAT_ADVANCED      = 8,    /* Advanced/Expert */
} uft_param_category_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parameter Definition
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char          *name;          /* Interner Name */
    const char          *cli_short;     /* CLI Short Option (-f) */
    const char          *cli_long;      /* CLI Long Option (--format) */
    const char          *json_key;      /* JSON Key */
    const char          *gui_widget;    /* GUI Widget Name */
    
    uft_param_type_t    type;
    uft_param_category_t category;
    
    const char          *description;   /* Hilfetext */
    const char          *default_value; /* Default als String */
    
    /* Für ENUM */
    const char          **enum_values;  /* NULL-terminiert */
    int                 enum_count;
    
    /* Für RANGE */
    int                 range_min;
    int                 range_max;
    int                 range_step;
    
    /* Flags */
    bool                required;
    bool                expert;         /* Nur im Expert-Mode sichtbar */
    bool                deprecated;
    
} uft_param_def_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parameter Value
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const uft_param_def_t *definition;
    
    union {
        bool            bool_val;
        int             int_val;
        float           float_val;
        char            *string_val;
        int             enum_index;
    } value;
    
    bool                is_set;         /* Explizit gesetzt? */
    bool                is_default;     /* Default-Wert? */
    
} uft_param_value_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parameter Set
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_params uft_params_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Preset Definition
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char          *name;          /* Preset-Name */
    const char          *description;   /* Beschreibung */
    uft_param_category_t category;      /* Kategorie */
    
    const char          *json_params;   /* JSON-String mit Parametern */
    const char          *cli_args;      /* CLI-Argumente */
    
} uft_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Parameter Set Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Leeres Parameter-Set erstellen
 */
uft_params_t *uft_params_create(void);

/**
 * @brief Parameter-Set mit Defaults erstellen
 */
uft_params_t *uft_params_create_defaults(void);

/**
 * @brief Parameter-Set klonen
 */
uft_params_t *uft_params_clone(const uft_params_t *params);

/**
 * @brief Parameter-Set freigeben
 */
void uft_params_free(uft_params_t *params);

/**
 * @brief Alle Parameter auf Default zurücksetzen
 */
void uft_params_reset(uft_params_t *params);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - CLI Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parameter aus CLI-Argumenten parsen
 * @param argc Argument-Anzahl
 * @param argv Argument-Array
 * @return Parameter-Set oder NULL bei Fehler
 */
uft_params_t *uft_params_from_cli(int argc, char **argv);

/**
 * @brief Parameter aus CLI-String parsen
 * @param cli_string CLI-Argumente als String
 */
uft_params_t *uft_params_from_cli_string(const char *cli_string);

/**
 * @brief Parameter zu CLI-Argumenten konvertieren
 * @param params Parameter-Set
 * @return CLI-String (muss mit free() freigegeben werden)
 */
char *uft_params_to_cli(const uft_params_t *params);

/**
 * @brief Parameter zu CLI-Argumenten (nur geänderte)
 */
char *uft_params_to_cli_diff(const uft_params_t *params);

/**
 * @brief CLI-Hilfe ausgeben
 */
void uft_params_print_help(void);

/**
 * @brief CLI-Hilfe für Kategorie
 */
void uft_params_print_help_category(uft_param_category_t category);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - JSON Serialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parameter aus JSON parsen
 * @param json JSON-String
 * @return Parameter-Set oder NULL bei Fehler
 */
uft_params_t *uft_params_from_json(const char *json);

/**
 * @brief Parameter aus JSON-Datei laden
 */
uft_params_t *uft_params_load_json(const char *path);

/**
 * @brief Parameter zu JSON konvertieren
 * @param params Parameter-Set
 * @param pretty Pretty-Print mit Einrückung
 * @return JSON-String (muss mit free() freigegeben werden)
 */
char *uft_params_to_json(const uft_params_t *params, bool pretty);

/**
 * @brief Parameter in JSON-Datei speichern
 */
uft_error_t uft_params_save_json(const uft_params_t *params, const char *path);

/**
 * @brief Nur geänderte Parameter zu JSON
 */
char *uft_params_to_json_diff(const uft_params_t *params);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Parameter Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Bool-Parameter abrufen
 */
bool uft_params_get_bool(const uft_params_t *params, const char *name);

/**
 * @brief Int-Parameter abrufen
 */
int uft_params_get_int(const uft_params_t *params, const char *name);

/**
 * @brief Float-Parameter abrufen
 */
float uft_params_get_float(const uft_params_t *params, const char *name);

/**
 * @brief String-Parameter abrufen
 */
const char *uft_params_get_string(const uft_params_t *params, const char *name);

/**
 * @brief Enum-Parameter abrufen (als Index)
 */
int uft_params_get_enum(const uft_params_t *params, const char *name);

/**
 * @brief Enum-Parameter als String
 */
const char *uft_params_get_enum_string(const uft_params_t *params, const char *name);

/**
 * @brief Parameter setzen (Bool)
 */
uft_error_t uft_params_set_bool(uft_params_t *params, const char *name, bool value);

/**
 * @brief Parameter setzen (Int)
 */
uft_error_t uft_params_set_int(uft_params_t *params, const char *name, int value);

/**
 * @brief Parameter setzen (Float)
 */
uft_error_t uft_params_set_float(uft_params_t *params, const char *name, float value);

/**
 * @brief Parameter setzen (String)
 */
uft_error_t uft_params_set_string(uft_params_t *params, const char *name, const char *value);

/**
 * @brief Parameter setzen (Enum by index)
 */
uft_error_t uft_params_set_enum(uft_params_t *params, const char *name, int index);

/**
 * @brief Parameter setzen (Enum by string)
 */
uft_error_t uft_params_set_enum_string(uft_params_t *params, const char *name, const char *value);

/**
 * @brief Prüfen ob Parameter gesetzt ist
 */
bool uft_params_is_set(const uft_params_t *params, const char *name);

/**
 * @brief Parameter auf Default zurücksetzen
 */
void uft_params_unset(uft_params_t *params, const char *name);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Presets
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Preset laden
 * @param name Preset-Name
 * @return Parameter-Set oder NULL
 */
uft_params_t *uft_params_load_preset(const char *name);

/**
 * @brief Preset auf Parameter anwenden
 */
uft_error_t uft_params_apply_preset(uft_params_t *params, const char *name);

/**
 * @brief Alle verfügbaren Presets auflisten
 * @return NULL-terminiertes Array von Preset-Namen
 */
const char **uft_params_list_presets(void);

/**
 * @brief Presets in Kategorie auflisten
 */
const char **uft_params_list_presets_in_category(uft_param_category_t category);

/**
 * @brief Preset-Info abrufen
 */
const uft_preset_t *uft_params_get_preset_info(const char *name);

/**
 * @brief Eigenes Preset speichern
 */
uft_error_t uft_params_save_preset(
    const uft_params_t *params,
    const char *name,
    const char *description
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parameter validieren
 * @param params Parameter-Set
 * @param errors Output: Fehlermeldungen (NULL-terminiert)
 * @return Anzahl Fehler (0 = valid)
 */
int uft_params_validate(const uft_params_t *params, char ***errors);

/**
 * @brief Einzelnen Parameter validieren
 */
bool uft_params_validate_value(
    const char *name,
    const char *value,
    char **error_message
);

/**
 * @brief Parameter-Kombination validieren
 */
bool uft_params_validate_combination(
    const uft_params_t *params,
    char **error_message
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - GUI Bridge
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parameter-Definition abrufen
 */
const uft_param_def_t *uft_params_get_definition(const char *name);

/**
 * @brief Alle Parameter-Definitionen
 */
const uft_param_def_t **uft_params_get_all_definitions(int *count);

/**
 * @brief Parameter in Kategorie
 */
const uft_param_def_t **uft_params_get_definitions_in_category(
    uft_param_category_t category,
    int *count
);

/**
 * @brief GUI Widget-Name zu Parameter-Name
 */
const char *uft_params_widget_to_param(const char *widget_name);

/**
 * @brief Parameter-Name zu GUI Widget-Name
 */
const char *uft_params_param_to_widget(const char *param_name);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Diff & Merge
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Unterschiede zwischen zwei Parameter-Sets
 */
typedef struct {
    const char *name;
    const char *value1;
    const char *value2;
} uft_param_diff_t;

uft_param_diff_t *uft_params_diff(
    const uft_params_t *params1,
    const uft_params_t *params2,
    int *count
);

void uft_params_diff_free(uft_param_diff_t *diff, int count);

/**
 * @brief Parameter-Sets zusammenführen
 * @param base Basis-Parameter
 * @param overlay Overlay-Parameter (überschreiben base)
 * @return Neues Parameter-Set
 */
uft_params_t *uft_params_merge(
    const uft_params_t *base,
    const uft_params_t *overlay
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Session Export
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parameter als Shell-Script exportieren
 */
uft_error_t uft_params_export_shell(
    const uft_params_t *params,
    const char *path,
    const char *input_file,
    const char *output_file
);

/**
 * @brief Parameter als Batch-Datei exportieren
 */
uft_error_t uft_params_export_batch(
    const uft_params_t *params,
    const char *path,
    const char *input_file,
    const char *output_file
);

/**
 * @brief Parameter als Python-Script exportieren
 */
uft_error_t uft_params_export_python(
    const uft_params_t *params,
    const char *path
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Kategorie als String
 */
const char *uft_param_category_string(uft_param_category_t category);

/**
 * @brief Type als String
 */
const char *uft_param_type_string(uft_param_type_t type);

/**
 * @brief Parameter-Set ausgeben
 */
void uft_params_print(const uft_params_t *params);

/**
 * @brief Parameter-Set als Tabelle ausgeben
 */
void uft_params_print_table(const uft_params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARAM_BRIDGE_H */
