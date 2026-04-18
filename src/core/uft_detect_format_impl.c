/**
 * @file uft_detect_format_impl.c
 * @brief Correct-signature impl of uft_detect_format (detect/ variant).
 *
 * Replaces the ABI-broken stub in uft_core_stubs.c which had
 * `int uft_detect_format(const char *path)` — a 1-arg variant that
 * accepted a pointer expecting filename but was called with a data
 * buffer + length + result-struct (3 args per the canonical
 * declaration). Callers got their extra args dropped silently.
 *
 * Canonical declaration: include/uft/detect/uft_format_detect.h:173
 *   int uft_detect_format(const uint8_t *data, size_t len,
 *                          uft_detect_result_t *result);
 *
 * This file includes ONLY that header so we see the correct
 * uft_detect_result_t layout (one of 3 structs sharing this name —
 * a separate ABI-bomb tracked as ABI-003 in the detector report).
 */

#include "uft/detect/uft_format_detect.h"
#include "uft/uft_format_plugin.h"   /* for uft_probe_buffer_format */

#include <stddef.h>
#include <string.h>

/* Forward-decl the plugin-probe helper (defined in uft_format_plugin_*.c). */
extern const uft_format_plugin_t *uft_probe_buffer_format(const uint8_t *data,
                                                            size_t size,
                                                            size_t file_size);

int uft_detect_format(const uint8_t *data, size_t len,
                       uft_detect_result_t *result)
{
    if (!data || !result || len == 0) return -1;
    memset(result, 0, sizeof(*result));

    const uft_format_plugin_t *plugin = uft_probe_buffer_format(data, len, len);
    if (!plugin) {
        result->format = UFT_FORMAT_UNKNOWN;
        result->confidence = 0;
        return -1;
    }

    /* Map plugin info → detection result. The two `format` enums (plugin's
     * uft_format_t and this header's uft_format enum) share numeric values
     * via the UFT_FORMAT_ENUM_DEFINED guard, so the int cast is safe. */
    result->format       = (uft_format_t)plugin->format;
    result->format_name  = plugin->name;
    result->extensions   = plugin->extensions;
    result->confidence   = 90;  /* plugin probe matched — high confidence */
    result->probe_score  = 90;
    snprintf(result->reason, sizeof(result->reason),
             "Plugin '%s' accepted the buffer.", plugin->name ? plugin->name : "?");
    return 0;  /* UFT_OK */
}
