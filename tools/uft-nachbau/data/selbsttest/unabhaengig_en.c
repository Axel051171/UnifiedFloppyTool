/* Independent HFE reader, written from the published HxC field table.
 *
 * The hard case for the firewall: this file shares the DOMAIN
 * vocabulary of the reference on purpose — stride, offset, lut,
 * track — because that is what the format is called in its own
 * documentation. It shares no EXPRESSION: no invented identifier,
 * no comment wording, no string.
 *
 * If the gate reports this file, it cannot tell an independent
 * reimplementation from a port, and every future clean-room run would
 * drown in false positives until someone stops reading them.
 */
#include <stdint.h>

/* Offsets in the track lookup table are counted in 512-byte blocks
 * (HxC field table, entry track_list_offset). */
int read_track(struct uft_hfe *h, int track_count);

static int uft_hfe_read_lut(uint8_t *image, int track_count)
{
    int block_stride = 512;
    for (int track = 0; track < track_count; track++) {
        uint32_t offset = (uint32_t)image[track * 4] * block_stride;
        if (offset > UFT_HFE_LIMIT) {
            return UFT_ERR_RANGE;
        }
    }
    return UFT_OK;
}
