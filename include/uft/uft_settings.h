/**
 * @file uft_settings.h
 * @brief Runtime Settings System (W-P3-003)
 * 
 * Manages runtime configuration:
 * - Load/save from JSON files
 * - Environment variable override
 * - Change notifications
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_SETTINGS_H
#define UFT_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * SETTINGS GROUPS
 *===========================================================================*/

typedef enum {
    UFT_SET_GENERAL,      /**< General settings */
    UFT_SET_FORMAT,       /**< Format defaults */
    UFT_SET_HARDWARE,     /**< Hardware settings */
    UFT_SET_RECOVERY,     /**< Recovery options */
    UFT_SET_PLL,          /**< PLL parameters */
    UFT_SET_GUI,          /**< GUI preferences */
    UFT_SET_LOGGING,      /**< Logging settings */
    UFT_SET_PATHS,        /**< Path settings */
    UFT_SET_COUNT
} uft_settings_group_t;

/*===========================================================================
 * COMMON KEYS
 *===========================================================================*/

/* General */
#define UFT_SET_VERBOSE             "general.verbose"
#define UFT_SET_QUIET               "general.quiet"
#define UFT_SET_EXPERT_MODE         "general.expert_mode"

/* Format */
#define UFT_SET_DEFAULT_FORMAT      "format.default"
#define UFT_SET_DEFAULT_SIDES       "format.sides"
#define UFT_SET_DEFAULT_TRACKS      "format.tracks"

/* Hardware */
#define UFT_SET_HW_INTERFACE        "hardware.interface"
#define UFT_SET_HW_DEVICE           "hardware.device"

/* Recovery */
#define UFT_SET_RETRIES             "recovery.retries"
#define UFT_SET_REVOLUTIONS         "recovery.revolutions"
#define UFT_SET_MERGE_REVS          "recovery.merge_revolutions"

/* PLL */
#define UFT_SET_PLL_PRESET          "pll.preset"
#define UFT_SET_PLL_ADJUST          "pll.adjust"

/* Logging */
#define UFT_SET_LOG_LEVEL           "logging.level"
#define UFT_SET_LOG_FILE            "logging.file"

/* Paths */
#define UFT_SET_PATH_OUTPUT         "paths.output"
#define UFT_SET_PATH_TEMP           "paths.temp"

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Initialize settings system with defaults
 */
int uft_settings_init(void);

/**
 * @brief Shutdown settings system
 */
void uft_settings_shutdown(void);

/**
 * @brief Load settings from JSON file
 */
int uft_settings_load(const char *path);

/**
 * @brief Save settings to JSON file
 */
int uft_settings_save(const char *path);

/**
 * @brief Reset to defaults
 */
void uft_settings_reset(void);

/*===========================================================================
 * GETTERS
 *===========================================================================*/

const char* uft_settings_get_string(const char *key, const char *def);
int uft_settings_get_int(const char *key, int def);
float uft_settings_get_float(const char *key, float def);
bool uft_settings_get_bool(const char *key, bool def);

/*===========================================================================
 * SETTERS
 *===========================================================================*/

int uft_settings_set_string(const char *key, const char *value);
int uft_settings_set_int(const char *key, int value);
int uft_settings_set_float(const char *key, float value);
int uft_settings_set_bool(const char *key, bool value);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Check if key exists
 */
bool uft_settings_has(const char *key);

/**
 * @brief Get group name
 */
const char* uft_settings_group_name(uft_settings_group_t group);

/**
 * @brief Export to JSON string
 */
char* uft_settings_to_json(bool pretty);

/**
 * @brief Get default settings path
 */
int uft_settings_default_path(char *path, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SETTINGS_H */
