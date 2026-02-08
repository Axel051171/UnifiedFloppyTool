/**
 * @file uft_scp.c
 * @brief SuperCard Pro SCP format core
 * @version 3.8.0
 */
#include "uft/formats/uft_scp.h"

#include <string.h>

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static int fseek_abs(FILE *f, uint32_t off) {
#if defined(_WIN32)
    return _fseeki64(f, (long long)off, SEEK_SET);
#else
    return (void)fseek(f, (long)off, SEEK_SET);
#endif
}

static int fread_exact(FILE *f, void *buf, size_t n) {
    return fread(buf, 1, n, f) == n ? 1 : 0;
}

int uft_scp_open(uft_scp_image_t *img, const char *path) {
    if (!img || !path) return UFT_SCP_EINVAL;
    memset(img, 0, sizeof(*img));

    img->f = fopen(path, "rb");
    if (!img->f) return UFT_SCP_EIO;

    if (!fread_exact(img->f, &img->hdr, sizeof(img->hdr))) {
        uft_scp_close(img);
        return UFT_SCP_EIO;
    }

    if (memcmp(img->hdr.signature, "SCP", 3) != 0) {
        uft_scp_close(img);
        return UFT_SCP_EFORMAT;
    }

    img->extended_mode = (img->hdr.flags & 0x40u) ? 1u : 0u;

    /* Offsets are stored LE. Normalize to host endian. */
    for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
        img->track_offsets[i] = img->hdr.track_offsets[i];
    }

    /* Extended mode: offsets table at absolute 0x80 (per Atari spec/spec). */
    if (img->extended_mode) {
        if (fseek_abs(img->f, 0x80u) != 0) {
            uft_scp_close(img);
            return UFT_SCP_EIO;
        }
        uint32_t tmp[UFT_SCP_MAX_TRACK_ENTRIES];
        if (!fread_exact(img->f, tmp, sizeof(tmp))) {
            uft_scp_close(img);
            return UFT_SCP_EIO;
        }
        for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) img->track_offsets[i] = tmp[i];
    }

    return UFT_SCP_OK;
}

void uft_scp_close(uft_scp_image_t *img) {
    if (!img) return;
    if (img->f) fclose(img->f);
    memset(img, 0, sizeof(*img));
}

int uft_scp_get_track_info(uft_scp_image_t *img, uint8_t track_index, uft_scp_track_info_t *out) {
    if (!img || !img->f || !out) return UFT_SCP_EINVAL;
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) return UFT_SCP_EBOUNDS;

    memset(out, 0, sizeof(*out));
    out->track_index = track_index;
    out->file_offset = img->track_offsets[track_index];
    out->present = out->file_offset ? 1u : 0u;
    out->num_revs = img->hdr.num_revs;

    if (!out->present) return UFT_SCP_OK;

    /* Read TRK header for track_number */
    if (fseek_abs(img->f, out->file_offset) != 0) return UFT_SCP_EIO;
    uft_scp_track_header_t trk = {0};
    if (!fread_exact(img->f, &trk, sizeof(trk))) return UFT_SCP_EIO;
    if (memcmp(trk.signature, "TRK", 3) != 0) return UFT_SCP_EFORMAT;

    out->track_number = trk.track_number;
    return UFT_SCP_OK;
}

int uft_scp_read_track_revs(uft_scp_image_t *img, uint8_t track_index,
                           uft_scp_track_rev_t *revs, size_t revs_cap,
                           uft_scp_track_header_t *out_trk_hdr)
{
    if (!img || !img->f || !revs) return UFT_SCP_EINVAL;
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) return UFT_SCP_EBOUNDS;
    if (revs_cap < (size_t)img->hdr.num_revs) return UFT_SCP_EBOUNDS;

    uint32_t off = img->track_offsets[track_index];
    if (!off) return UFT_SCP_EFORMAT;

    if (fseek_abs(img->f, off) != 0) return UFT_SCP_EIO;

    uft_scp_track_header_t trk = {0};
    if (!fread_exact(img->f, &trk, sizeof(trk))) return UFT_SCP_EIO;
    if (memcmp(trk.signature, "TRK", 3) != 0) return UFT_SCP_EFORMAT;

    if (out_trk_hdr) *out_trk_hdr = trk;

    /* Revolutions are stored little-endian in-file. We'll read raw and normalize. */
    for (size_t i = 0; i < (size_t)img->hdr.num_revs; i++) {
        uint8_t raw[12];
        if (!fread_exact(img->f, raw, sizeof(raw))) return UFT_SCP_EIO;
        revs[i].time_duration = rd_u32_le(&raw[0]);
        revs[i].data_length   = rd_u32_le(&raw[4]);
        revs[i].data_offset   = rd_u32_le(&raw[8]);
    }

    return UFT_SCP_OK;
}

int uft_scp_read_rev_transitions(uft_scp_image_t *img, uint8_t track_index, uint8_t rev_index,
                                uint32_t *transitions_out, size_t transitions_cap,
                                size_t *out_count, uint32_t *out_total_time)
{
    if (!img || !img->f || !transitions_out || transitions_cap == 0) return UFT_SCP_EINVAL;
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) return UFT_SCP_EBOUNDS;
    if (rev_index >= img->hdr.num_revs) return UFT_SCP_EBOUNDS;

    uint32_t track_off = img->track_offsets[track_index];
    if (!track_off) return UFT_SCP_EFORMAT;

    uft_scp_track_rev_t revs[32];
    if (img->hdr.num_revs > 32) return UFT_SCP_EBOUNDS; /* sanity */
    uft_scp_track_header_t trk = {0};
    int rc = uft_scp_read_track_revs(img, track_index, revs, 32, &trk);
    if (rc != UFT_SCP_OK) return rc;

    uft_scp_track_rev_t rev = revs[rev_index];

    /* Seek to flux data list inside TRK block */
    uint32_t data_off_abs = track_off + rev.data_offset;
    if (fseek_abs(img->f, data_off_abs) != 0) return UFT_SCP_EIO;

    /* Read 16-bit big-endian deltas: rev.data_length entries => 2*len bytes */
    const size_t nvals = (size_t)rev.data_length;
    uint32_t time = 0;
    size_t outn = 0;

    /* Stream read in chunks to avoid large allocations */
    uint8_t buf[4096];
    size_t remaining_bytes = nvals * 2;

    while (remaining_bytes > 0) {
        size_t toread = remaining_bytes > sizeof(buf) ? sizeof(buf) : remaining_bytes;
        if (!fread_exact(img->f, buf, toread)) return UFT_SCP_EIO;
        remaining_bytes -= toread;

        for (size_t i = 0; i + 1 < toread; i += 2) {
            uint16_t be = (uint16_t)((uint16_t)buf[i] << 8) | (uint16_t)buf[i+1];
            if (be) {
                time += (uint32_t)be;
                if (outn < transitions_cap) {
                    transitions_out[outn++] = time;
                } else {
                    if (out_count) *out_count = outn;
                    if (out_total_time) *out_total_time = time;
                    return UFT_SCP_EBOUNDS;
                }
            } else {
                time += 0x10000u;
            }
        }
    }

    if (out_count) *out_count = outn;
    if (out_total_time) *out_total_time = time;
    return UFT_SCP_OK;
}
