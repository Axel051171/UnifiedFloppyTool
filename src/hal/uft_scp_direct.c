/**
 * @file uft_scp_direct.c
 * @brief SuperCard Pro direct-USB HAL backend (M3.1 scaffold).
 *
 * Scaffold commit: API surface is final; all I/O callbacks return
 * UFT_ERR_NOT_IMPLEMENTED honestly until the libusb layer is wired
 * in a follow-up commit. Capabilities are declared to match real SCP
 * hardware (read-flux + write-flux at 40ns, no sector mode).
 *
 * Prinzip 1: no silent no-op. Every function either does the real
 * work or returns NOT_IMPLEMENTED with an actionable error code.
 */

#include "uft/hal/uft_scp_direct.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Opaque context definition. Kept here so only this TU sees it. */
struct uft_scp_direct_ctx {
    /* Placeholder. Real impl will hold libusb_device_handle*,
     * endpoint descriptors, current track state, timeout settings. */
    bool      is_open;
    int       current_track;
    uint32_t  revolutions_setting;
};

uft_error_t uft_scp_direct_open(uft_scp_direct_ctx_t **out_ctx) {
    if (out_ctx == NULL) return UFT_ERR_INVALID_ARG;
    *out_ctx = NULL;
    /*
     * M3.1 TODO: enumerate libusb devices matching VID/PID, claim
     * interface, probe firmware version, init defaults.
     * Returning NOT_IMPLEMENTED rather than a silent success keeps
     * callers honest.
     */
    return UFT_ERR_NOT_IMPLEMENTED;
}

void uft_scp_direct_close(uft_scp_direct_ctx_t *ctx) {
    if (ctx == NULL) return;
    /* M3.1 TODO: libusb_release_interface + libusb_close */
    ctx->is_open = false;
    free(ctx);
}

uft_error_t uft_scp_direct_seek(uft_scp_direct_ctx_t *ctx,
                                  int track_index)
{
    if (ctx == NULL) return UFT_ERR_INVALID_ARG;
    if (track_index < 0 || track_index > UFT_SCP_MAX_TRACK_INDEX) {
        return UFT_ERR_INVALID_ARG;
    }
    /* M3.1 TODO: send CMD_SELECT_DRIVE + step to track_index */
    ctx->current_track = track_index;
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_scp_direct_read_flux(uft_scp_direct_ctx_t *ctx,
                                      int side,
                                      int revolutions,
                                      uint32_t *out_flux,
                                      size_t out_capacity,
                                      size_t *out_count)
{
    if (ctx == NULL || out_flux == NULL || out_count == NULL) {
        return UFT_ERR_INVALID_ARG;
    }
    if (side < 0 || side > 1) return UFT_ERR_INVALID_ARG;
    if (revolutions < 1 || revolutions > UFT_SCP_MAX_REVOLUTIONS) {
        return UFT_ERR_INVALID_ARG;
    }
    if (out_capacity == 0) return UFT_ERR_BUFFER_TOO_SMALL;

    *out_count = 0;
    /* M3.1 TODO: CMD_READ_FLUX → bulk-read transitions into out_flux */
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_scp_direct_write_flux(uft_scp_direct_ctx_t *ctx,
                                       int side,
                                       const uint32_t *flux,
                                       size_t count)
{
    if (ctx == NULL || flux == NULL) return UFT_ERR_INVALID_ARG;
    if (side < 0 || side > 1) return UFT_ERR_INVALID_ARG;
    if (count == 0) return UFT_ERR_INVALID_ARG;
    /* M3.1 TODO: CMD_WRITE_FLUX → bulk-write transitions from flux */
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_scp_direct_get_capabilities(
    uft_scp_direct_capabilities_t *out)
{
    if (out == NULL) return UFT_ERR_INVALID_ARG;
    /*
     * Capabilities describe the HARDWARE, not whether the software
     * has implemented each feature. This is important: a HAL caller
     * should be able to see "SCP supports flux write" even if the
     * scaffold's write_flux returns NOT_IMPLEMENTED. That's the
     * contract — caps tell you what the hardware can do, the
     * callbacks tell you whether THIS build can talk to it yet.
     */
    out->can_read_flux      = true;
    out->can_write_flux     = true;
    out->can_read_sector    = false;
    out->max_revolutions    = UFT_SCP_MAX_REVOLUTIONS;
    out->flux_ns_per_sample = UFT_SCP_FLUX_NS_PER_SAMPLE;
    out->max_track_index    = UFT_SCP_MAX_TRACK_INDEX;
    return UFT_OK;
}
