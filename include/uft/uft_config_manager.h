/**
 * @file uft_config_manager.h
 * @brief Centralized Configuration Management System
 * 
 * P3-003: Zentrale Config
 * 
 * Features:
 * - INI/JSON file support
 * - Type-safe getters/setters
 * - Default values
 * - Environment variable override
 * - Section-based organization
 * - Change notifications
 * - Validation
 */

#ifndef UFT_CONFIG_MANAGER_H
#define UFT_CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_CONFIG_MAX_KEY_LEN      128
#define UFT_CONFIG_MAX_VALUE_LEN    1024
#define UFT_CONFIG_MAX_SECTION_LEN  64

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration value types
 */
typedef enum {
    UFT_CFG_TYPE_STRING,
    UFT_CFG_TYPE_INT,
    UFT_CFG_TYPE_FLOAT,
    UFT_CFG_TYPE_BOOL,
    UFT_CFG_TYPE_PATH,
    UFT_CFG_TYPE_ENUM
} uft_config_type_t;

/**
 * @brief Configuration value
 */
typedef struct {
    uft_config_type_t type;
    union {
        char str_val[UFT_CONFIG_MAX_VALUE_LEN];
        int64_t int_val;
        double float_val;
        bool bool_val;
    };
} uft_config_value_t;

/**
 * @brief Configuration entry definition
 */
typedef struct {
    const char *section;
    const char *key;
    uft_config_type_t type;
    uft_config_value_t default_value;
    const char *description;
    const char *env_override;      /* Environment variable name */
    int64_t min_val;               /* For int/float validation */
    int64_t max_val;
    const char **enum_values;      /* For enum type */
    int enum_count;
} uft_config_def_t;

/**
 * @brief Change callback
 */
typedef void (*uft_config_changed_fn)(const char *section, const char *key, 
                                       const uft_config_value_t *value, void *ctx);

/**
 * @brief Configuration manager handle
 */
typedef struct uft_config_manager uft_config_manager_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create configuration manager
 */
uft_config_manager_t* uft_config_create(void);

/**
 * @brief Destroy configuration manager
 */
void uft_config_destroy(uft_config_manager_t *cfg);

/**
 * @brief Register configuration definitions
 */
int uft_config_register(uft_config_manager_t *cfg, 
                        const uft_config_def_t *defs, int count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * File I/O
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Load configuration from INI file
 */
int uft_config_load_ini(uft_config_manager_t *cfg, const char *path);

/**
 * @brief Save configuration to INI file
 */
int uft_config_save_ini(const uft_config_manager_t *cfg, const char *path);

/**
 * @brief Load configuration from JSON file
 */
int uft_config_load_json(uft_config_manager_t *cfg, const char *path);

/**
 * @brief Save configuration to JSON file
 */
int uft_config_save_json(const uft_config_manager_t *cfg, const char *path);

/**
 * @brief Load from environment variables
 */
int uft_config_load_env(uft_config_manager_t *cfg);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Getters
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get string value
 */
const char* uft_config_get_string(const uft_config_manager_t *cfg,
                                   const char *section, const char *key);

/**
 * @brief Get integer value
 */
int64_t uft_config_get_int(const uft_config_manager_t *cfg,
                           const char *section, const char *key);

/**
 * @brief Get float value
 */
double uft_config_get_float(const uft_config_manager_t *cfg,
                            const char *section, const char *key);

/**
 * @brief Get boolean value
 */
bool uft_config_get_bool(const uft_config_manager_t *cfg,
                         const char *section, const char *key);

/**
 * @brief Get raw value
 */
bool uft_config_get(const uft_config_manager_t *cfg,
                    const char *section, const char *key,
                    uft_config_value_t *out);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Setters
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set string value
 */
int uft_config_set_string(uft_config_manager_t *cfg,
                          const char *section, const char *key,
                          const char *value);

/**
 * @brief Set integer value
 */
int uft_config_set_int(uft_config_manager_t *cfg,
                       const char *section, const char *key,
                       int64_t value);

/**
 * @brief Set float value
 */
int uft_config_set_float(uft_config_manager_t *cfg,
                         const char *section, const char *key,
                         double value);

/**
 * @brief Set boolean value
 */
int uft_config_set_bool(uft_config_manager_t *cfg,
                        const char *section, const char *key,
                        bool value);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Reset to defaults
 */
void uft_config_reset(uft_config_manager_t *cfg);

/**
 * @brief Reset single key to default
 */
void uft_config_reset_key(uft_config_manager_t *cfg,
                          const char *section, const char *key);

/**
 * @brief Check if key exists
 */
bool uft_config_exists(const uft_config_manager_t *cfg,
                       const char *section, const char *key);

/**
 * @brief Get key count in section
 */
int uft_config_section_count(const uft_config_manager_t *cfg, const char *section);

/**
 * @brief Enumerate keys in section
 */
int uft_config_enumerate(const uft_config_manager_t *cfg, const char *section,
                         void (*callback)(const char *key, 
                                         const uft_config_value_t *value, 
                                         void *ctx),
                         void *ctx);

/**
 * @brief Register change callback
 */
int uft_config_on_change(uft_config_manager_t *cfg, 
                         uft_config_changed_fn callback, void *ctx);

/**
 * @brief Validate all values
 */
int uft_config_validate(const uft_config_manager_t *cfg, 
                        char *errors, size_t errors_size);

/**
 * @brief Print configuration (for debugging)
 */
void uft_config_print(const uft_config_manager_t *cfg);

/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT Default Configuration Definitions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get UFT default configuration definitions
 */
const uft_config_def_t* uft_config_get_defaults(int *count);

/**
 * @brief Convenience macro for config access
 */
#define UFT_CFG_STR(cfg, sec, key) uft_config_get_string(cfg, sec, key)
#define UFT_CFG_INT(cfg, sec, key) uft_config_get_int(cfg, sec, key)
#define UFT_CFG_BOOL(cfg, sec, key) uft_config_get_bool(cfg, sec, key)
#define UFT_CFG_FLOAT(cfg, sec, key) uft_config_get_float(cfg, sec, key)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Section Names
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_SEC_GENERAL     "general"
#define UFT_SEC_HARDWARE    "hardware"
#define UFT_SEC_RECOVERY    "recovery"
#define UFT_SEC_FORMAT      "format"
#define UFT_SEC_GUI         "gui"
#define UFT_SEC_LOGGING     "logging"
#define UFT_SEC_PATHS       "paths"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Common Keys
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* General */
#define UFT_KEY_VERSION         "version"
#define UFT_KEY_LAST_DIR        "last_directory"

/* Hardware */
#define UFT_KEY_DEVICE          "device"
#define UFT_KEY_DRIVE_NUM       "drive_number"
#define UFT_KEY_AUTO_DETECT     "auto_detect"

/* Recovery */
#define UFT_KEY_MAX_RETRIES     "max_retries"
#define UFT_KEY_REVOLUTIONS     "revolutions"
#define UFT_KEY_WEAK_BITS       "detect_weak_bits"
#define UFT_KEY_RECALIBRATE     "recalibrate"

/* Format */
#define UFT_KEY_DEFAULT_FMT     "default_format"
#define UFT_KEY_CYLINDERS       "cylinders"
#define UFT_KEY_HEADS           "heads"
#define UFT_KEY_SECTORS         "sectors"
#define UFT_KEY_ENCODING        "encoding"

/* GUI */
#define UFT_KEY_DARK_MODE       "dark_mode"
#define UFT_KEY_WINDOW_X        "window_x"
#define UFT_KEY_WINDOW_Y        "window_y"
#define UFT_KEY_WINDOW_W        "window_width"
#define UFT_KEY_WINDOW_H        "window_height"

/* Logging */
#define UFT_KEY_LOG_LEVEL       "log_level"
#define UFT_KEY_LOG_FILE        "log_file"
#define UFT_KEY_LOG_CONSOLE     "log_to_console"

/* Paths */
#define UFT_KEY_INPUT_DIR       "input_directory"
#define UFT_KEY_OUTPUT_DIR      "output_directory"
#define UFT_KEY_TEMP_DIR        "temp_directory"

#ifdef __cplusplus
}
#endif

#endif /* UFT_CONFIG_MANAGER_H */
