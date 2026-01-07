#include "uft_d64_view.h"
#include <string.h>

static uint8_t spt_for_track(uint8_t t) {
    if (t >= 1 && t <= 17) return 21;
    if (t >= 18 && t <= 24) return 19;
    if (t >= 25 && t <= 30) return 18;
    if (t >= 31 && t <= 35) return 17;
    return 0;
}

static uint32_t total_sectors_35(void) {
    uint32_t tot = 0;
    for (uint8_t t = 1; t <= 35; t++) tot += spt_for_track(t);
    return tot;
}

static void petscii_to_ascii_trim(const uint8_t *in16, char out17[17]) {
    char tmp[16];
    for (int i = 0; i < 16; i++) {
        uint8_t c = in16[i];
        char o = '.';
        if (c == 0xA0 || c == 0x00) o = ' ';
        else if (c >= 0x61 && c <= 0x7A) o = (char)(c - 0x20);
        else if (c >= 0x20 && c <= 0x7E) o = (char)c;
        tmp[i] = o;
    }
    int end = 15;
    while (end >= 0 && tmp[end] == ' ') end--;
    int n = (end >= 0) ? (end + 1) : 0;
    for (int i = 0; i < n; i++) out17[i] = tmp[i];
    out17[n] = '\0';
}

uft_d64_status_t uft_d64_open(const uint8_t *blob, size_t blob_len, uft_d64_view_t *out) {
    if (!out) return UFT_D64_E_INVALID;
    memset(out, 0, sizeof(*out));
    if (!blob) return UFT_D64_E_INVALID;

    const uint32_t total = total_sectors_35();
    const uint32_t image_bytes = total * 256u;
    uint8_t has_err = 0;

    if (blob_len == (size_t)image_bytes) has_err = 0;
    else if (blob_len == (size_t)(image_bytes + total)) has_err = 1;
    else return UFT_D64_E_GEOM;

    out->blob = blob;
    out->blob_len = blob_len;
    out->geom.tracks = 35;
    out->geom.total_sectors = total;
    out->geom.image_bytes = image_bytes;
    out->geom.has_error_bytes = has_err;
    return UFT_D64_OK;
}

static uft_d64_status_t ts_to_offset(uint8_t track, uint8_t sector, uint32_t *out_off) {
    if (!out_off) return UFT_D64_E_INVALID;
    *out_off = 0;

    if (track < 1 || track > 35) return UFT_D64_E_GEOM;
    const uint8_t spt = spt_for_track(track);
    if (spt == 0) return UFT_D64_E_GEOM;
    if (sector >= spt) return UFT_D64_E_GEOM;

    uint32_t idx = 0;
    for (uint8_t t = 1; t < track; t++) idx += spt_for_track(t);
    idx += sector;
    *out_off = idx * 256u;
    return UFT_D64_OK;
}

uft_d64_status_t uft_d64_sector_ptr(const uft_d64_view_t *d64, uint8_t track, uint8_t sector, const uint8_t **out_ptr) {
    if (!d64 || !out_ptr) return UFT_D64_E_INVALID;
    *out_ptr = NULL;

    uint32_t off = 0;
    uft_d64_status_t st = ts_to_offset(track, sector, &off);
    if (st != UFT_D64_OK) return st;
    if ((size_t)off + 256u > (size_t)d64->geom.image_bytes) return UFT_D64_E_TRUNC;

    *out_ptr = d64->blob + off;
    return UFT_D64_OK;
}

uft_d64_status_t uft_d64_dir_begin(uft_d64_dir_iter_t *it) {
    if (!it) return UFT_D64_E_INVALID;
    memset(it, 0, sizeof(*it));
    it->track = 18;
    it->sector = 1;
    it->entry_index = 0;
    it->done = 0;
    return UFT_D64_OK;
}

static uft_d64_filetype_t map_ft(uint8_t raw) { return (uft_d64_filetype_t)(raw & 0x07u); }

uft_d64_status_t uft_d64_dir_next(const uft_d64_view_t *d64, uft_d64_dir_iter_t *it, uft_d64_dirent_t *out_ent) {
    if (!d64 || !it || !out_ent) return UFT_D64_E_INVALID;
    memset(out_ent, 0, sizeof(*out_ent));
    if (it->done) return UFT_D64_E_DIR;

    const uint8_t *sec = NULL;
    uft_d64_status_t st = uft_d64_sector_ptr(d64, it->track, it->sector, &sec);
    if (st != UFT_D64_OK) return st;

    for (;;) {
        if (it->entry_index >= 8) {
            const uint8_t nt = sec[0];
            const uint8_t ns = sec[1];
            if (nt == 0) { it->done = 1; return UFT_D64_E_DIR; }
            it->track = nt;
            it->sector = ns;
            it->entry_index = 0;
            st = uft_d64_sector_ptr(d64, it->track, it->sector, &sec);
            if (st != UFT_D64_OK) return st;
            continue;
        }

        const size_t eoff = 2u + (size_t)it->entry_index * 32u;
        it->entry_index++;

        const uint8_t etype = sec[eoff + 2];
        if (etype == 0x00) continue;

        out_ent->raw_type = etype;
        out_ent->closed = (uint8_t)((etype & 0x80u) ? 1u : 0u);
        out_ent->type = map_ft(etype);
        out_ent->start_track = sec[eoff + 3];
        out_ent->start_sector = sec[eoff + 4];
        petscii_to_ascii_trim(&sec[eoff + 5], out_ent->name_ascii);
        out_ent->blocks = (uint16_t)sec[eoff + 30] | ((uint16_t)sec[eoff + 31] << 8);
        return UFT_D64_OK;
    }
}

uft_d64_status_t uft_d64_read_chain(const uft_d64_view_t *d64, uint8_t start_track, uint8_t start_sector,
                                   uint8_t *out_buf, size_t out_cap,
                                   size_t *out_len, size_t *out_blocks, uint8_t *out_chain_ok)
{
    if (out_len) *out_len = 0;
    if (out_blocks) *out_blocks = 0;
    if (out_chain_ok) *out_chain_ok = 0;
    if (!d64) return UFT_D64_E_INVALID;

    uint8_t t = start_track, s = start_sector;
    uint8_t visited[36][21];
    memset(visited, 0, sizeof(visited));

    size_t pos = 0, blocks = 0;

    while (t != 0) {
        const uint8_t spt = spt_for_track(t);
        if (spt == 0 || s >= spt) return UFT_D64_E_CHAIN;
        if (visited[t][s]) return UFT_D64_E_CHAIN;
        visited[t][s] = 1;

        const uint8_t *sec = NULL;
        const uft_d64_status_t st = uft_d64_sector_ptr(d64, t, s, &sec);
        if (st != UFT_D64_OK) return st;

        const uint8_t nt = sec[0];
        const uint8_t ns = sec[1];

        if (nt == 0) {
            uint8_t used = ns;
            if (used > 254) used = 254;
            if (out_buf && out_cap) {
                if (pos + (size_t)used > out_cap) return UFT_D64_E_TRUNC;
                memcpy(out_buf + pos, sec + 2, used);
            }
            pos += (size_t)used;
            blocks++;
            if (out_chain_ok) *out_chain_ok = 1;
            break;
        }

        if (out_buf && out_cap) {
            if (pos + 254u > out_cap) return UFT_D64_E_TRUNC;
            memcpy(out_buf + pos, sec + 2, 254u);
        }
        pos += 254u;
        blocks++;
        t = nt;
        s = ns;
    }

    if (out_len) *out_len = pos;
    if (out_blocks) *out_blocks = blocks;
    return UFT_D64_OK;
}
