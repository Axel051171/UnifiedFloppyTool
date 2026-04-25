/**
 * @file uft_decoder_plugin_stub.c
 * @brief Null stub for the decoder-plugin lookup.
 *
 * `uft_find_decoder_plugin_for_flux` is declared in
 * include/uft/uft_decoder_plugin.h but never implemented in v4.1's
 * tree. Restored v3.7 plugins (e.g. src/formats/scp/uft_scp_plugin.c)
 * call it but null-check the result, so a NULL-returning stub keeps
 * the link happy and preserves the "no decoder available, skip flux
 * decode" code path without changing observable behaviour.
 *
 * Lives in its own TU to avoid pulling uft_decoder_plugin.h into
 * src/core/uft_core_stubs.c — that include order surfaces a
 * pre-existing UFT_COMPILER_VERSION/UFT_PREFETCH redefinition between
 * uft_platform.h and uft_compiler.h.
 *
 * Replace with real plugin-registry lookup when the decoder plugin
 * system is reactivated in M3.
 */

#include <stddef.h>
#include <stdint.h>

/* Forward-declare the opaque plugin struct — full definition lives in
 * include/uft/uft_decoder_plugin.h, which we deliberately avoid here. */
struct uft_decoder_plugin;

const struct uft_decoder_plugin *uft_find_decoder_plugin_for_flux(
    const uint32_t *flux, size_t flux_count)
{
    (void)flux;
    (void)flux_count;
    return NULL;
}
