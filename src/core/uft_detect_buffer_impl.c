/**
 * @file uft_detect_buffer_impl.c
 * @brief Correct-signature impl of uft_detect_buffer (uft_format_detect.h).
 *
 * Replaces the ABI-broken stub in uft_core_stubs.c which had
 * `int uft_detect_buffer(const uint8_t *data, size_t size)` — 2 args,
 * int return — versus the canonical 4-arg bool-return declaration in
 * include/uft/uft_format_detect.h:131. Caller in
 * src/analysis/uft_sector_compare.c:357 passed (data, size, path, result)
 * but the stub only read the first 2. Always returned 0 → false, so
 * detection silently failed for every compared sector.
 *
 * Canonical declaration:
 *   bool uft_detect_buffer(const uint8_t *data, size_t size,
 *                           const char *filename,
 *                           uft_detect_result_t *result);
 *
 * This header's uft_detect_result_t has a DIFFERENT layout than the one
 * used by uft_detect_format (ABI-003 in the bomb report). Keeping this
 * file in its own TU so only one struct definition is visible.
 */

#include "uft/uft_format_detect.h"
#include "uft/uft_format_plugin.h"

#include <stddef.h>
#include <string.h>

extern const uft_format_plugin_t *uft_probe_buffer_format(const uint8_t *data,
                                                            size_t size,
                                                            size_t file_size);

bool uft_detect_buffer(const uint8_t *data, size_t size,
                        const char *filename,
                        uft_detect_result_t *result)
{
    if (!data || !result || size == 0) return false;
    memset(result, 0, sizeof(*result));
    (void)filename;  /* filename hint: future work (extension-based boost) */

    const uft_format_plugin_t *plugin = uft_probe_buffer_format(data, size, size);
    if (!plugin) return false;

    /* Plugin → detect_result mapping. This header's uft_detect_result_t
     * has {format (uft_detect_format_t), variant_name, description,
     * confidence, geom fields, feature flags}. We only populate format +
     * variant_name + confidence + description — geometry/features are
     * plugin-specific and left zero. Caller inspects `format` only. */
    result->format = (uft_detect_format_t)plugin->format;
    result->variant_name = plugin->name;
    result->description = plugin->description;
    result->confidence = 90;
    return true;
}
