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
#include <stdio.h>
#include <string.h>

/* MF-450: the alternatives are filled now, and the confidence is real.
 *
 * uft_probe_result_t has carried these fields since the header was written:
 *
 *     int          confidence;
 *     int          alternative_count;
 *     uft_format_t alternatives[4];
 *     int          alt_confidence[4];
 *     char         warnings[256];
 *
 * and the only implementation memset the struct and assigned exactly one of
 * them. `alternative_count` was therefore permanently 0 — which does not read
 * as "not filled in", it reads as "checked, nothing else matched". A field
 * meant to report ambiguity that never does is worse than no field.
 *
 * The other half was the return value: it is plugin->format, and before MF-450
 * that was UFT_FORMAT_DSK for 131 of 137 plugins. uft_convert_file() picks its
 * conversion path from it, and the path table is keyed on the real formats
 * (SCP->HFE, D64->G64, ...), so a source that came back as DSK matched almost
 * nothing. Correcting the 30 plugin declarations made the return value mean
 * something; this function can now report what it found. */
uft_format_t uft_probe_format(const uint8_t *data, size_t size,
                               const char *filename,
                               uft_probe_result_t *result)
{
    if (result) memset(result, 0, sizeof(*result));
    if (!data || size == 0) return UFT_FORMAT_UNKNOWN;
    (void)filename;   /* the name is not evidence about the bytes (MF-444) */

    uft_probe_ranking_t r;
    uft_probe_buffer_ranked(data, size, size, &r);
    if (!r.winner) return UFT_FORMAT_UNKNOWN;

    if (result) {
        result->format     = (uft_format_t)r.winner->format;
        result->confidence = r.confidence;

        /* Everything that scored as high as the winner, minus the winner. Only
         * what the ranking actually captured is reported — r.tied may be larger
         * than tied_with[] holds, and in that case the warning says so rather
         * than the array pretending to be complete. */
        int n = 0;
        for (size_t i = 0; i < r.tied_listed && n < 4; i++) {
            if (r.tied_with[i] == r.winner) continue;
            result->alternatives[n]   = (uft_format_t)r.tied_with[i]->format;
            result->alt_confidence[n] = r.confidence;
            n++;
        }
        result->alternative_count = n;

        if (r.tied > 1) {
            snprintf(result->warnings, sizeof(result->warnings),
                     "%zu plugins claim this data at confidence %d; '%s' wins "
                     "by registration order, not by evidence",
                     r.tied, r.confidence, r.winner->name);
        }
    }
    return (uft_format_t)r.winner->format;
}
