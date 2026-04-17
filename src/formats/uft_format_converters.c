/**
 * @file uft_format_converters.c
 * @brief Three Kat-A closures:
 *
 *   uft_imd_write    — Serialize an in-memory uft_imd_image_t to an IMD file.
 *   uft_imd_from_raw — Synthesize an IMD image from a flat raw byte buffer.
 *   uft_td0_to_imd   — Convert an already-decoded TD0 image to IMD.
 *
 * These three were declared in include/uft/formats/uft_imd.h and
 * include/uft/formats/uft_td0.h but had no implementation anywhere in
 * the tree. Callers (src/formats/uft_format_convert_*) relied on
 * broken ABI-mismatched stubs in uft_core_stubs.c that silently did
 * nothing.
 *
 * Scope: minimal but correct.
 *   - imd_write produces a file that can round-trip through imd_read_mem.
 *   - imd_from_raw uses the caller-supplied track_header as the template
 *     for every track (same mode, same nsectors, same sector_size).
 *   - td0_to_imd maps TD0 sector flags onto the 1..8 IMD stype codes
 *     per the Teledisk / IMD specs. Sectors without data become stype=0
 *     (unavailable).
 */

#include "uft/formats/uft_imd.h"
#include "uft/formats/uft_td0.h"
#include "uft/uft_error.h"     /* UFT_OK */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* IMD sector type codes. Not symbolically named in the header so define
 * locally — values per IMD spec v1.19. */
#define IMD_STYPE_UNAVAILABLE    0
#define IMD_STYPE_NORMAL         1
#define IMD_STYPE_NORMAL_COMP    2
#define IMD_STYPE_DELETED        3
#define IMD_STYPE_DELETED_COMP   4
#define IMD_STYPE_BAD            5
#define IMD_STYPE_BAD_COMP       6
#define IMD_STYPE_DEL_BAD        7
#define IMD_STYPE_DEL_BAD_COMP   8

/* TD0 sector flag bits (per Teledisk). */
#define TD0_FLAG_DUPLICATE       0x01
#define TD0_FLAG_CRC_ERROR       0x02
#define TD0_FLAG_DELETED         0x04
#define TD0_FLAG_NO_DATA         0x08  /* sector unavailable, no data block */

/* Map TD0 flags + data-presence to IMD stype. `compressed` means the
 * whole sector is a single fill byte. */
static uint8_t td0_flags_to_imd_stype(uint8_t td0_flags, bool compressed) {
    if (td0_flags & TD0_FLAG_NO_DATA) return IMD_STYPE_UNAVAILABLE;

    bool deleted = (td0_flags & TD0_FLAG_DELETED) != 0;
    bool bad     = (td0_flags & TD0_FLAG_CRC_ERROR) != 0;

    if (deleted && bad && compressed)  return IMD_STYPE_DEL_BAD_COMP;
    if (deleted && bad)                return IMD_STYPE_DEL_BAD;
    if (deleted && compressed)         return IMD_STYPE_DELETED_COMP;
    if (deleted)                       return IMD_STYPE_DELETED;
    if (bad && compressed)             return IMD_STYPE_BAD_COMP;
    if (bad)                           return IMD_STYPE_BAD;
    if (compressed)                    return IMD_STYPE_NORMAL_COMP;
    return IMD_STYPE_NORMAL;
}

/* ==========================================================================
 * uft_imd_write
 * ========================================================================== */

int uft_imd_write(const char *filename, const uft_imd_image_t *img)
{
    if (!filename || !img) return -1;
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    /* ASCII header: "IMD V.R: DD/MM/YYYY HH:MM:SS\r\n"
     * Version defaults to 1.19 if unset. */
    uint8_t vmaj = img->header.version_major ? img->header.version_major : 1;
    uint8_t vmin = img->header.version_minor ? img->header.version_minor : 19;
    fprintf(f, "IMD %u.%02u: %02u/%02u/%04u %02u:%02u:%02u\r\n",
            vmaj, vmin,
            img->header.day, img->header.month, img->header.year,
            img->header.hour, img->header.minute, img->header.second);

    /* Comment block, terminated by 0x1A. */
    if (img->comment && img->comment_len > 0)
        fwrite(img->comment, 1, img->comment_len, f);
    fputc(UFT_IMD_COMMENT_END, f);

    /* Track records. */
    for (unsigned t = 0; t < img->num_tracks; t++) {
        const uft_imd_track_t *tr = &img->tracks[t];
        const uft_imd_track_header_t *hdr = &tr->header;

        /* Track header: 5 bytes. Head byte carries optional cylmap/headmap flags. */
        uint8_t head_byte = hdr->head;
        if (tr->has_cylmap)  head_byte |= UFT_IMD_HEAD_CYLMAP;
        if (tr->has_headmap) head_byte |= UFT_IMD_HEAD_HEADMAP;

        uint8_t hdr_out[5] = {
            hdr->mode, hdr->cylinder, head_byte, hdr->nsectors, hdr->sector_size
        };
        if (fwrite(hdr_out, 1, 5, f) != 5) { fclose(f); return -1; }

        /* Sector numbering map (always present). */
        fwrite(tr->smap, 1, hdr->nsectors, f);
        if (tr->has_cylmap)  fwrite(tr->cmap, 1, hdr->nsectors, f);
        if (tr->has_headmap) fwrite(tr->hmap, 1, hdr->nsectors, f);

        /* Sector data records. */
        uint16_t ssize = uft_imd_ssize_to_bytes(hdr->sector_size);
        for (unsigned s = 0; s < hdr->nsectors; s++) {
            uint8_t stype = tr->stype[s];
            fputc(stype, f);
            if (stype == IMD_STYPE_UNAVAILABLE) continue;

            bool compressed = (stype == IMD_STYPE_NORMAL_COMP ||
                               stype == IMD_STYPE_DELETED_COMP ||
                               stype == IMD_STYPE_BAD_COMP ||
                               stype == IMD_STYPE_DEL_BAD_COMP);

            uint16_t this_size = tr->has_varsizes ? tr->ssize[s] : ssize;
            size_t offset = tr->sector_offsets[s];
            if (!tr->data || offset + (compressed ? 1 : this_size) > tr->data_size) {
                fclose(f); return -1;
            }
            if (compressed) fputc(tr->data[offset], f);
            else            fwrite(tr->data + offset, 1, this_size, f);
        }
    }

    if (ferror(f)) { fclose(f); return -1; }
    fclose(f);
    return UFT_OK;
}

/* ==========================================================================
 * uft_imd_from_raw
 * ========================================================================== */

int uft_imd_from_raw(const uint8_t *data, size_t size,
                      const uft_imd_track_header_t *params,
                      uft_imd_image_t *img)
{
    if (!data || !params || !img) return -1;
    if (params->nsectors == 0) return -1;

    uint16_t ssize = uft_imd_ssize_to_bytes(params->sector_size);
    if (ssize == 0) return -1;

    size_t track_bytes = (size_t)params->nsectors * ssize;
    if (track_bytes == 0 || size % track_bytes != 0) return -1;

    size_t ntracks = size / track_bytes;
    if (ntracks == 0) return -1;

    if (uft_imd_init(img) != UFT_OK) return -1;
    img->tracks = (uft_imd_track_t *)calloc(ntracks, sizeof(uft_imd_track_t));
    if (!img->tracks) return -1;
    img->num_tracks    = (uint16_t)ntracks;
    img->num_cylinders = (uint16_t)((ntracks + 1) / 2);   /* assume 2 heads */
    img->num_heads     = 2;
    if (img->num_cylinders * 2 != ntracks) {
        img->num_cylinders = (uint16_t)ntracks;           /* single-sided */
        img->num_heads     = 1;
    }

    for (size_t t = 0; t < ntracks; t++) {
        uft_imd_track_t *tr = &img->tracks[t];
        tr->header = *params;
        tr->header.cylinder = (uint8_t)(t / img->num_heads);
        tr->header.head     = (uint8_t)(t % img->num_heads);

        /* Default ascending sector map 1..N. */
        for (uint8_t s = 0; s < params->nsectors; s++) {
            tr->smap[s]  = (uint8_t)(s + 1);
            tr->stype[s] = IMD_STYPE_NORMAL;
            tr->sector_offsets[s] = (size_t)s * ssize;
        }

        tr->data_size = track_bytes;
        tr->data = (uint8_t *)malloc(track_bytes);
        if (!tr->data) { uft_imd_free(img); return -1; }
        memcpy(tr->data, data + t * track_bytes, track_bytes);

        img->total_sectors += params->nsectors;
    }
    return UFT_OK;
}

/* ==========================================================================
 * uft_td0_to_imd
 * ========================================================================== */

/* Map TD0 mode-byte-equivalent fields onto IMD mode. TD0 doesn't carry a
 * direct mode byte on each track in the same way IMD does; the choice
 * here follows the common case for PC floppies — MFM at the effective
 * rate matching the sector count. Callers who need exact mode mapping
 * should set it themselves after conversion. */
static uint8_t guess_imd_mode_mfm(uint8_t nsectors, uint8_t size_code) {
    uint16_t ssize = uft_imd_ssize_to_bytes(size_code);
    if (ssize == 0) ssize = 512;
    uint32_t track_bytes = (uint32_t)nsectors * ssize;
    if (track_bytes >= 12000) return UFT_IMD_MODE_500K_MFM;  /* HD */
    if (track_bytes >= 5000)  return UFT_IMD_MODE_250K_MFM;  /* DD */
    return UFT_IMD_MODE_300K_MFM;                             /* QD / slow */
}

int uft_td0_to_imd(const uft_td0_image_t *td0, struct uft_imd_image_t *imd)
{
    if (!td0 || !imd) return -1;
    if (uft_imd_init(imd) != UFT_OK) return -1;

    /* Copy timestamp from TD0 comment header when present. */
    if (td0->has_comment) {
        imd->header.year   = td0->comment_header.year + 1900;
        imd->header.month  = td0->comment_header.month + 1;
        imd->header.day    = td0->comment_header.day;
        imd->header.hour   = td0->comment_header.hour;
        imd->header.minute = td0->comment_header.minute;
        imd->header.second = td0->comment_header.second;
    }
    imd->header.version_major = 1;
    imd->header.version_minor = 19;

    if (td0->comment && td0->comment[0]) {
        size_t clen = strlen(td0->comment);
        imd->comment = (char *)malloc(clen + 1);
        if (imd->comment) {
            memcpy(imd->comment, td0->comment, clen + 1);
            imd->comment_len = clen;
        }
    }

    if (td0->num_tracks == 0) return UFT_OK;

    imd->tracks = (uft_imd_track_t *)calloc(td0->num_tracks, sizeof(uft_imd_track_t));
    if (!imd->tracks) return -1;
    imd->num_tracks    = td0->num_tracks;
    imd->num_cylinders = td0->cylinders;
    imd->num_heads     = td0->heads ? td0->heads : 1;

    for (size_t t = 0; t < td0->num_tracks; t++) {
        const uft_td0_track_t *tt = &td0->tracks[t];
        uft_imd_track_t       *it = &imd->tracks[t];

        it->header.cylinder    = tt->header.cylinder;
        it->header.head        = tt->header.side & 0x01;
        it->header.nsectors    = tt->nsectors;
        it->header.sector_size = (tt->nsectors > 0 && tt->sectors)
                                    ? tt->sectors[0].header.size
                                    : 2;   /* default 512 byte */
        it->header.mode = guess_imd_mode_mfm(it->header.nsectors,
                                              it->header.sector_size);

        /* Walk sectors: build smap + data blob, detect compression. */
        uint16_t ssize = uft_imd_ssize_to_bytes(it->header.sector_size);
        size_t total = 0;
        for (uint8_t s = 0; s < tt->nsectors; s++) total += ssize;
        if (total == 0) continue;

        it->data = (uint8_t *)malloc(total);
        if (!it->data) { uft_imd_free(imd); return -1; }
        it->data_size = total;

        size_t off = 0;
        for (uint8_t s = 0; s < tt->nsectors; s++) {
            const uft_td0_sector_t *ts = &tt->sectors[s];
            it->smap[s] = ts->header.sector;
            it->sector_offsets[s] = off;

            bool compressed = false;
            if (ts->data && ts->data_size == ssize) {
                memcpy(it->data + off, ts->data, ssize);
                /* Detect single-fill compressed block (all equal bytes). */
                compressed = true;
                for (uint16_t i = 1; i < ssize; i++)
                    if (ts->data[i] != ts->data[0]) { compressed = false; break; }
            } else if (ts->data) {
                size_t n = ts->data_size < ssize ? ts->data_size : ssize;
                memcpy(it->data + off, ts->data, n);
                if (n < ssize) memset(it->data + off + n, 0, ssize - n);
            } else {
                memset(it->data + off, 0, ssize);
            }
            it->stype[s] = td0_flags_to_imd_stype(ts->header.flags,
                                                   ts->data ? compressed : false);
            off += ssize;

            imd->total_sectors++;
            if (it->stype[s] == IMD_STYPE_UNAVAILABLE) imd->unavail_sectors++;
            if (compressed)                             imd->compressed_sectors++;
        }
    }
    return UFT_OK;
}
