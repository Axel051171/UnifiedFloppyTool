/**
 * @file uft_params.h
 * @brief GUI/CLI friendly parameter schema for formats and exporters.
 *
 * Goal: The GUI can query supported parameters, render widgets, validate input,
 * and pass a normalized key/value blob to the backend.
 */
#ifndef UFT_PARAMS_H
#define UFT_PARAMS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/uft_output.h"
#include "uft/uft_formats.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_PARAM_BOOL = 0,
    UFT_PARAM_INT,
    UFT_PARAM_FLOAT,
    UFT_PARAM_STRING,
    UFT_PARAM_ENUM
} uft_param_type_t;

typedef struct {
    const char *key;         /**< stable key, e.g. "interleave" */
    const char *label;       /**< GUI label */
    uft_param_type_t type;
    const char *help;        /**< tooltip/help text */

    /* Defaults are stored as strings to keep the C API simple. */
    const char *default_value;

    /* Numeric constraints (optional; ignore if min>max). */
    double min_value;
    double max_value;
    double step;

    /* ENUM values (optional). */
    const char *const *enum_values;
    size_t enum_count;
} uft_param_def_t;

/**
 * @brief Get output-exporter parameter definitions for a given container.
 *
 * The returned pointer is to a static array owned by the library.
 */
const uft_param_def_t *uft_output_param_defs(uft_output_format_t fmt, size_t *count);

/**
 * @brief Get decode/recovery parameter definitions (multi-pass, PLL, voting).
 *
 * These parameters are format-agnostic and can be shown in the GUI as a JSON blob.
 */
const uft_param_def_t *uft_recovery_param_defs(size_t *count);

/**
 * @brief Get per-disk-format parameter definitions (geometry overrides, quirks).
 *
 * Most classic formats have fixed geometry; this API exposes only the few knobs that
 * make sense for recovery (e.g., expected RPM, sector size quirks, tolerance).
 */
/**
 * @brief Get per-disk-format parameter definitions (geometry overrides, quirks).
 *
 * @param fmt Disk format id.
 */
const uft_param_def_t *uft_format_param_defs(uft_disk_format_id_t fmt, size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARAMS_H */
