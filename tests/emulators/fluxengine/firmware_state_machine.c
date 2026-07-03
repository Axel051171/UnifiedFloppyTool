/**
 * @file tests/emulators/fluxengine/firmware_state_machine.c
 * @brief Implementation of the fluxengine-CLI + device model.
 *
 * A "read" produces a synthetic SCP file via the generator in
 * tests/flux_gen/fluxengine; the test then decodes it with the
 * PRODUCTION parser uft_scp_read_track_memory() and asserts on both the
 * flux and the parser status. "rpm" emits a stdout line the FluxEngine
 * provider's regex must parse.
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

/* ─── lifecycle ─────────────────────────────────────────────────────── */

void fe_reset(fe_dev_t *dev) {
    if (!dev) return;
    memset(dev, 0, sizeof(*dev));
    dev->state       = FE_STATE_DISCONNECTED;
    dev->rpm         = 0.0;
    dev->stream_seed = 0xFE0011ULL;
    dev->last_exit   = FE_EXIT_NO_DEVICE;
}

void fe_power_on_defaults(fe_dev_t *dev) {
    if (!dev) return;
    fe_reset(dev);
    dev->device_present = true;
    dev->disk_present   = true;
    dev->rpm            = 300.0;
    dev->rpm_variant    = 0;
    dev->state          = FE_STATE_READY;
}

void fe_set_device_present(fe_dev_t *dev, bool p) {
    if (dev) { dev->device_present = p; if (!p) dev->state = FE_STATE_DISCONNECTED; }
}
void fe_set_disk_present(fe_dev_t *dev, bool p) { if (dev) dev->disk_present = p; }
void fe_set_rpm(fe_dev_t *dev, double r) { if (dev) dev->rpm = r; }
void fe_set_rpm_variant(fe_dev_t *dev, int v) { if (dev) dev->rpm_variant = v; }
void fe_set_fail_next(fe_dev_t *dev, int n) { if (dev) dev->fail_next_n = n; }
void fe_set_stream_seed(fe_dev_t *dev, uint64_t s) { if (dev) dev->stream_seed = s; }
void fe_set_inject_defects(fe_dev_t *dev, uint32_t d) { if (dev) dev->inject_defects = d; }

/* ─── helpers ───────────────────────────────────────────────────────── */

static fe_exit_t fail(fe_dev_t *dev, fe_exit_t code) {
    dev->state = FE_STATE_ERROR;
    dev->last_exit = code;
    return code;
}

/* ─── run ───────────────────────────────────────────────────────────── */

fe_exit_t fe_run(fe_dev_t *dev, const fe_request_t *req, int max_retries,
                 uint8_t **out_scp, size_t *out_len) {
    if (out_scp) *out_scp = NULL;
    if (out_len) *out_len = 0;
    if (!dev || !req || !req->subcommand) return FE_EXIT_BAD_ARGS;

    dev->state = FE_STATE_BUSY;

    /* Common device gating. */
    if (!dev->device_present) return fail(dev, FE_EXIT_NO_DEVICE);

    /* ── rpm / detect ─────────────────────────────────────────────── */
    if (strcmp(req->subcommand, "rpm") == 0) {
        if (!dev->disk_present) return fail(dev, FE_EXIT_NO_DISK);
        uft_fe_format_rpm_line(dev->rpm, dev->rpm_variant,
                               dev->last_stdout, sizeof(dev->last_stdout));
        dev->state = FE_STATE_READY;
        dev->last_exit = FE_EXIT_OK;
        return FE_EXIT_OK;
    }

    /* ── write: always refused ────────────────────────────────────── */
    if (strcmp(req->subcommand, "write") == 0) {
        return fail(dev, FE_EXIT_WRITE_DENY);
    }

    /* ── read ─────────────────────────────────────────────────────── */
    if (strcmp(req->subcommand, "read") != 0) {
        return fail(dev, FE_EXIT_BAD_ARGS);
    }

    if (req->cylinder < 0 || req->cylinder > UFT_FE_MAX_TRACK ||
        req->head < 0 || req->head > 1 ||
        req->revolutions < UFT_FE_MIN_REVS || req->revolutions > UFT_FE_MAX_REVS)
        return fail(dev, FE_EXIT_BAD_ARGS);

    if (!dev->disk_present) return fail(dev, FE_EXIT_NO_DISK);

    /* Transient IO with retry (fluxengine retries internally). */
    int used = 0;
    while (dev->fail_next_n > 0) {
        if (used >= max_retries) return fail(dev, FE_EXIT_IO);
        used++;
        dev->fail_next_n--;
    }

    /* fluxengine tracks are addressed c<cyl>h<head>; the SCP file the
     * generator builds targets one track index = cylinder (side folds
     * into the seed). */
    uft_fe_gen_params_t gp = {
        .seed = dev->stream_seed ^ ((uint64_t)req->cylinder << 8)
                                 ^ (uint64_t)req->head,
        .track = req->cylinder,
        .revolutions = req->revolutions,
        .cell_ns = 0,
        .defects = dev->inject_defects,
        .weak_jitter_pct = (dev->inject_defects & UFT_FE_DEFECT_WEAK_BITS) ? 8 : 0,
    };
    uft_fe_gen_scp_t s;
    if (uft_fe_gen_scp(&gp, &s) != UFT_FE_GEN_OK)
        return fail(dev, FE_EXIT_IO);

    if (out_scp && out_len) {
        *out_scp = s.bytes;
        *out_len = s.bytes_len;
        s.bytes = NULL;
    }
    uft_fe_gen_free(&s);

    dev->state = FE_STATE_READY;
    dev->last_exit = FE_EXIT_OK;
    dev->reads_ok++;
    return FE_EXIT_OK;
}
