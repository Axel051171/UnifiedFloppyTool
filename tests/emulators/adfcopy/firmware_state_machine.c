/**
 * @file tests/emulators/adfcopy/firmware_state_machine.c
 * @brief Implementation of the ADFCopy Teensy-firmware model.
 *
 * See firmware_state_machine.h. CMD_READ_FLUX produces the synthetic
 * Amiga-MFM flux payload via the generator in tests/flux_gen/adfcopy.
 */

#include "firmware_state_machine.h"

#include <stdlib.h>
#include <string.h>

/* ─── lifecycle ─────────────────────────────────────────────────────── */

void adfc_reset(adfc_dev_t *dev) {
    if (!dev) return;
    memset(dev, 0, sizeof(*dev));
    dev->state         = ADFC_STATE_IDLE;
    dev->current_track = -1;
    dev->flux_capable  = true;
    dev->stream_seed   = 0xADFC0001ULL;
    dev->last_response = ADFC_RSP_ERROR;
}

void adfc_power_on_defaults(adfc_dev_t *dev) {
    if (!dev) return;
    adfc_reset(dev);
    dev->disk_present    = true;
    dev->write_protected = false;
    dev->flux_capable    = true;
}

void adfc_set_disk_present(adfc_dev_t *dev, bool p) { if (dev) dev->disk_present = p; }
void adfc_set_write_protected(adfc_dev_t *dev, bool wp) { if (dev) dev->write_protected = wp; }
void adfc_set_stream_seed(adfc_dev_t *dev, uint64_t s) { if (dev) dev->stream_seed = s; }
void adfc_set_inject_defects(adfc_dev_t *dev, uint32_t d) { if (dev) dev->inject_defects = d; }

/* ─── commands ──────────────────────────────────────────────────────── */

uint8_t adfc_cmd_init(adfc_dev_t *dev) {
    if (!dev) return ADFC_RSP_ERROR;
    if (!dev->disk_present) {
        dev->last_response = ADFC_RSP_NODISK;
        return ADFC_RSP_NODISK;
    }
    /* Spin motor + home head. */
    dev->motor_on      = true;
    dev->current_track = 0;
    dev->state         = ADFC_STATE_READY;
    dev->last_response = ADFC_RSP_OK;
    return ADFC_RSP_OK;
}

uint8_t adfc_cmd_seek(adfc_dev_t *dev, int track) {
    if (!dev) return ADFC_RSP_ERROR;
    if (dev->state != ADFC_STATE_READY) {
        dev->last_response = ADFC_RSP_ERROR;   /* must CMD_INIT first */
        return ADFC_RSP_ERROR;
    }
    if (track < 0 || track > UFT_ADFC_MAX_TRACK) {
        dev->last_response = ADFC_RSP_ERROR;
        return ADFC_RSP_ERROR;
    }
    dev->current_track = track;
    dev->last_response = ADFC_RSP_OK;
    return ADFC_RSP_OK;
}

uint8_t adfc_cmd_get_status(const adfc_dev_t *dev) {
    if (!dev) return 0;
    uint8_t s = 0;
    if (dev->disk_present)    s |= ADFC_STATUS_DISK;
    if (dev->write_protected) s |= ADFC_STATUS_WPROT;
    if (dev->motor_on)        s |= ADFC_STATUS_MOTOR;
    if (dev->flux_capable)    s |= ADFC_STATUS_FLUX;
    return s;
}

uint8_t adfc_cmd_read_flux(adfc_dev_t *dev, int track, int revolutions,
                           uint8_t **out_reply, size_t *out_len) {
    if (out_reply) *out_reply = NULL;
    if (out_len) *out_len = 0;
    if (!dev) return ADFC_RSP_ERROR;

    if (dev->state != ADFC_STATE_READY) {
        dev->last_response = ADFC_RSP_ERROR;
        return ADFC_RSP_ERROR;
    }
    if (!dev->disk_present) {
        /* 3-byte no-disk header, no flux. */
        if (out_reply && out_len) {
            uint8_t *hdr = (uint8_t *)malloc(3);
            if (hdr) { hdr[0] = UFT_ADFC_FLUX_NODISK; hdr[1] = 0; hdr[2] = 0;
                       *out_reply = hdr; *out_len = 3; }
        }
        dev->last_response = ADFC_RSP_NODISK;
        return ADFC_RSP_NODISK;
    }
    if (track < 0 || track > UFT_ADFC_MAX_TRACK ||
        revolutions < UFT_ADFC_MIN_REVS || revolutions > UFT_ADFC_MAX_REVS) {
        dev->last_response = ADFC_RSP_ERROR;
        return ADFC_RSP_ERROR;
    }

    /* Seek implicitly to the requested track (firmware does this). */
    dev->current_track = track;

    uft_adfc_gen_params_t gp = {
        .seed = dev->stream_seed ^ ((uint64_t)track << 8),
        .track = track,
        .revolutions = revolutions,
        .defects = dev->inject_defects,
        .weak_jitter_pct = (dev->inject_defects & UFT_ADFC_DEFECT_WEAK_BITS) ? 8 : 0,
    };
    uft_adfc_gen_flux_t f;
    if (uft_adfc_gen_flux(&gp, &f) != UFT_ADFC_GEN_OK) {
        dev->last_response = ADFC_RSP_ERROR;
        return ADFC_RSP_ERROR;
    }

    if (out_reply && out_len) {
        *out_reply = f.bytes;
        *out_len = f.bytes_len;
        f.bytes = NULL;
    }
    uft_adfc_gen_free(&f);

    dev->last_response = ADFC_RSP_OK;
    dev->reads_ok++;
    return ADFC_RSP_OK;
}
