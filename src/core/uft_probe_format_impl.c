/**
 * @file uft_probe_format_impl.c
 * @brief Correct-signature impl of uft_probe_format (uft_format_probe.h).
 *
 * Replaces the ABI-broken stub in uft_core_stubs.c:
 *   int uft_probe_format(const char *path, void *result);    (wrong)
 * Canonical declaration in include/uft/uft_format_probe.h:148:
 *   uft_format_t uft_probe_format(const uint8_t *data, size_t size,
 *                                  const char *filename,
 *                                  uft_probe_result_t *result);
 *
 * Caller in src/formats/uft_format_convert_dispatch.c:420 passes the
 * 4-arg data+size+path+result form. The old stub read path+void* and
 * returned int — on x86-64 the extra args sat in unused registers, the
 * stub called uft_probe_file_format(data_ptr_cast_to_char_star) →
 * garbage open path → always NULL → convert dispatch silently failed
 * for every conversion.
 */

#include "uft/uft_format_probe.h"
#include "uft/uft_format_plugin.h"

#include <stddef.h>
#include <string.h>

extern const uft_format_plugin_t *uft_probe_buffer_format(const uint8_t *data,
                                                            size_t size,
                                                            size_t file_size);

uft_format_t uft_probe_format(const uint8_t *data, size_t size,
                               const char *filename,
                               uft_probe_result_t *result)
{
    if (!data || size == 0) return UFT_FORMAT_UNKNOWN;
    (void)filename;

    const uft_format_plugin_t *plugin = uft_probe_buffer_format(data, size, size);
    if (!plugin) {
        if (result) memset(result, 0, sizeof(*result));
        return UFT_FORMAT_UNKNOWN;
    }

    if (result) {
        memset(result, 0, sizeof(*result));
        /* Populate whichever fields the canonical uft_probe_result_t has.
         * We only touch the ones we can be certain about across versions:
         * the top-level `format` field is guaranteed to exist per header. */
        result->format = (uft_format_t)plugin->format;
    }
    return (uft_format_t)plugin->format;
}
