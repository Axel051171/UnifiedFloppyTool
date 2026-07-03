/**
 * @file tests/emulators/fc5025/firmware_state_machine.c
 * @brief Implementation of the FC5025 CBW/CSW device model.
 *
 * See firmware_state_machine.h. The distinguishing behaviour is Rule F-3:
 * a CRC-error sector's divergent re-reads are preserved (>= 2 distinct
 * copies), never collapsed.
 */

#include "firmware_state_machine.h"

#include <stdlib.h>
#include <string.h>

/* ─── lifecycle ─────────────────────────────────────────────────────── */

void fc_reset(fc_dev_t *dev) {
    if (!dev) return;
    memset(dev, 0, sizeof(*dev));
    dev->state       = FC_STATE_DISCONNECTED;
    dev->max_retries = 3;
    dev->last_csw    = FC_CSW_NO_DISK;
}

void fc_power_on_defaults(fc_dev_t *dev) {
    if (!dev) return;
    fc_reset(dev);
    dev->device_present = true;
    dev->disk_present   = true;
    dev->max_retries    = 3;
    dev->state          = FC_STATE_READY;
}

void fc_set_device_present(fc_dev_t *dev, bool p) {
    if (dev) { dev->device_present = p; if (!p) dev->state = FC_STATE_DISCONNECTED; }
}
void fc_set_disk_present(fc_dev_t *dev, bool p) { if (dev) dev->disk_present = p; }
void fc_mount_track(fc_dev_t *dev, const uft_fc_gen_track_t *t) { if (dev) dev->track = t; }
void fc_set_max_retries(fc_dev_t *dev, int r) { if (dev) dev->max_retries = r; }

int fc_detect(fc_dev_t *dev) {
    if (!dev) return -1;
    if (!dev->device_present) { dev->state = FC_STATE_DISCONNECTED; return 0; }
    dev->state = FC_STATE_READY;
    return 1;
}

/* ─── CBW/CSW framing ───────────────────────────────────────────────── */

static void put_u32le(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}
static uint32_t get_u32le(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

size_t fc_build_read_cbw(uint8_t *cbw, size_t cap, uint32_t tag,
                         int track, int side, int sector,
                         uint16_t sector_size) {
    if (!cbw || cap < FC_CBW_LEN) return 0;
    memset(cbw, 0, FC_CBW_LEN);
    put_u32le(&cbw[0], FC_CBW_SIGNATURE);
    put_u32le(&cbw[4], tag);
    put_u32le(&cbw[8], sector_size);      /* data transfer length */
    cbw[12] = 0x80;                        /* flags: data IN */
    cbw[13] = 0x00;                        /* LUN */
    cbw[14] = 0x0A;                        /* command length (10) */
    /* CDB (command block): opcode + CHS + size (FC5025 v1309 layout). */
    cbw[15] = FC_CMD_READ_FLEXIBLE;
    cbw[16] = (uint8_t)track;
    cbw[17] = (uint8_t)side;
    cbw[18] = (uint8_t)sector;
    cbw[19] = (uint8_t)(sector_size >> 8);
    cbw[20] = (uint8_t)(sector_size & 0xFF);
    return FC_CBW_LEN;
}

bool fc_parse_csw(const uint8_t *csw, size_t len,
                  uint32_t *tag, uint32_t *residue, fc_csw_status_t *status) {
    if (!csw || len < FC_CSW_LEN) return false;
    if (get_u32le(&csw[0]) != FC_CSW_SIGNATURE) return false;
    if (tag)     *tag     = get_u32le(&csw[4]);
    if (residue) *residue = get_u32le(&csw[8]);
    if (status)  *status  = (fc_csw_status_t)csw[12];
    return true;
}

/* ─── sector read with divergent-read preservation ──────────────────── */

static bool copies_equal(const uint8_t *a, const uint8_t *b, uint16_t n) {
    return memcmp(a, b, n) == 0;
}

fc_csw_status_t fc_read_sector(fc_dev_t *dev, int track, int side,
                               int sector, fc_read_result_t *out) {
    if (out) memset(out, 0, sizeof(*out));
    if (!dev || !out) return FC_CSW_FAILED;

    if (!dev->device_present) { dev->last_csw = FC_CSW_FAILED; return FC_CSW_FAILED; }
    if (!dev->disk_present)   { dev->last_csw = FC_CSW_NO_DISK; return FC_CSW_NO_DISK; }
    if (!dev->track)          { dev->last_csw = FC_CSW_NO_SECTOR; return FC_CSW_NO_SECTOR; }

    const uft_fc_gen_track_t *t = dev->track;
    if (track != t->track || side != t->side ||
        sector < 0 || sector >= t->sector_count) {
        dev->last_csw = FC_CSW_NO_SECTOR;
        return FC_CSW_NO_SECTOR;
    }

    uint16_t ssz = t->sector_size;
    out->copy_len = ssz;

    /* First read to learn the sector status. */
    uint8_t *first = (uint8_t *)malloc(ssz);
    if (!first) return FC_CSW_FAILED;
    uft_fc_sec_status_t sec_st = uft_fc_gen_read_sector(t, sector, 0, first, ssz);

    if (sec_st == UFT_FC_SEC_MISSING) {
        free(first);
        dev->last_csw = FC_CSW_NO_SECTOR;
        out->status = FC_CSW_NO_SECTOR;
        out->attempts = 1;
        return FC_CSW_NO_SECTOR;
    }

    /* Preserve the first copy. */
    out->copies[0] = first;
    out->divergent_count = 1;
    out->attempts = 1;

    fc_csw_status_t csw = (sec_st == UFT_FC_SEC_CRC_ERROR)
                              ? FC_CSW_CRC_ERROR : FC_CSW_OK;

    /* Rule F-3: on CRC error (or a divergent sector), re-read up to the
     * retry limit and KEEP each DISTINCT copy — never collapse. */
    bool weak = (sec_st == UFT_FC_SEC_CRC_ERROR) ||
                (t->defect_sector == sector &&
                 (t->defects & UFT_FC_DEFECT_WEAK_SECTOR));
    if (weak) {
        for (int a = 1; a <= dev->max_retries &&
                        out->divergent_count < 8; a++) {
            uint8_t *cp = (uint8_t *)malloc(ssz);
            if (!cp) break;
            uft_fc_gen_read_sector(t, sector, a, cp, ssz);
            out->attempts++;
            /* Keep only genuinely distinct copies. */
            bool dup = false;
            for (int j = 0; j < out->divergent_count; j++)
                if (copies_equal(out->copies[j], cp, ssz)) { dup = true; break; }
            if (dup) { free(cp); continue; }
            out->copies[out->divergent_count++] = cp;
        }
    }

    out->status = csw;
    dev->last_csw = csw;
    if (csw == FC_CSW_OK) dev->sectors_ok++;
    else if (csw == FC_CSW_CRC_ERROR) dev->sectors_crc++;
    return csw;
}

fc_csw_status_t fc_write_sector(fc_dev_t *dev, int track, int side, int sector) {
    (void)track; (void)side; (void)sector;
    if (dev) dev->last_csw = FC_CSW_WRITE_DENY;
    /* No write opcode exists in the FC5025 command table. */
    return FC_CSW_WRITE_DENY;
}

void fc_free_read_result(fc_read_result_t *r) {
    if (!r) return;
    for (int i = 0; i < r->divergent_count && i < 8; i++) {
        free(r->copies[i]);
        r->copies[i] = NULL;
    }
    r->divergent_count = 0;
}
