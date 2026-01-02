    #include "uft/uft_error.h"
#include "flux_format/dsk.h"
    #include "fmt_util.h"
    #include "flux_logical.h"
    
    #include <string.h>
    #include <errno.h>
    #include <stdlib.h>

    /*
     * Minimal, lossless CPC DSK/EDSK sector-container implementation.
     * We do NOT attempt any encoding/decoding beyond what the container stores.
     *
     * Key principle: writers are deterministic and never "repair" or "optimize"
     * anything unless explicitly requested via parameters.
     */

    void fluxfmt_dsk_default_params(fluxfmt_dsk_params_t *p)
    {
        if (!p) return;
        memset(p, 0, sizeof(*p));
        p->gap3 = 0x4e;
        p->filler = 0xe5;
        p->extended = true;
    }

    /* --- On-disk layout constants --- */
    enum {
        DSK_DISK_HDR_SZ = 256,
        DSK_TRK_HDR_SZ  = 256,
    };

    static void wr_pad(uint8_t *dst, size_t cap, const char *s)
    {
        if (!dst || cap == 0) return;
        memset(dst, 0x00, cap);
        if (!s) return;
        const size_t n = strlen(s);
        memcpy(dst, s, n < cap ? n : cap);
    }

    static uint32_t secsize_from_n(uint8_t n)
    {
        /* Sector size in bytes = 128 << N */
        if (n > 7) return 0;
        return (uint32_t)128u << n;
    }

    static uint8_t n_from_secsize(uint32_t sz)
    {
        /* prefer exact powers of two in DSK encoding */
        uint8_t n = 0;
        uint32_t v = 128;
        while (n < 8) {
            if (v == sz) return n;
            v <<= 1;
            ++n;
        }
        return 2; /* sane default: 512 */
    }

    static int infer_geom_from_logical(const ufm_logical_image_t *li, uint16_t *out_cyls, uint16_t *out_heads)
    {
        if (!li || li->count == 0) return -EINVAL;
        uint16_t max_c = 0, max_h = 0;
        for (uint32_t i = 0; i < li->count; ++i) {
            if (li->sectors[i].cyl > max_c) max_c = li->sectors[i].cyl;
            if (li->sectors[i].head > max_h) max_h = li->sectors[i].head;
        }
        if (out_cyls) *out_cyls = (uint16_t)(max_c + 1u);
        if (out_heads) *out_heads = (uint16_t)(max_h + 1u);
        return 0;
    }

    static int cmp_sector_r(const void *a, const void *b)
    {
        const ufm_sector_t *sa = (const ufm_sector_t*)a;
        const ufm_sector_t *sb = (const ufm_sector_t*)b;
        if (sa->sec < sb->sec) return -1;
        if (sa->sec > sb->sec) return 1;
        return 0;
    }

    static uint16_t infer_spt_for_track(const ufm_logical_image_t *li, uint16_t cyl, uint16_t head)
    {
        uint16_t cnt = 0;
        for (uint32_t i = 0; i < li->count; ++i)
            if (li->sectors[i].cyl == cyl && li->sectors[i].head == head)
                ++cnt;
        return cnt;
    }

    static uint32_t infer_sector_size_for_track(const ufm_logical_image_t *li, uint16_t cyl, uint16_t head)
    {
        for (uint32_t i = 0; i < li->count; ++i)
            if (li->sectors[i].cyl == cyl && li->sectors[i].head == head)
                return li->sectors[i].size;
        return 512;
    }

    static bool probe_dsk(const uint8_t *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        /* Standard DSK and extended DSK have different header signatures. */
        static const char sig_dsk[]  = "MV - CPC";
        static const char sig_edsk[] = "EXTENDED CPC DSK File";
        if (len >= (sizeof(sig_dsk) - 1) && memcmp(buf, sig_dsk, sizeof(sig_dsk) - 1) == 0) return true;
        if (len >= (sizeof(sig_edsk) - 1) && memcmp(buf, sig_edsk, sizeof(sig_edsk) - 1) == 0) return true;
        return false;
    }

    /* DSK/EDSK: parse the container and populate ufm_logical_image_t.
     * This is a sector container: we keep the raw sector bytes as stored.
     */
    static int read_dsk(FILE *fp, ufm_disk_t *out)
    {
        if (!fp || !out) return -EINVAL;

        uint8_t hdr[256];
        if (fseek(fp, 0, SEEK_SET) != 0) return -EIO;
        if (!fmt_read_exact(fp, hdr, sizeof(hdr))) return -EIO;

        /* Signatures (CPC DSK vs Extended CPC DSK). */
        const char sig_dsk[]  = "MV - CPC";
        const char sig_edsk[] = "EXTENDED CPC DSK File";
        const bool is_edsk = (memcmp(hdr, sig_edsk, 8) == 0);
        if (memcmp(hdr, sig_dsk, 8) != 0 && !is_edsk)
            return -EINVAL;

        /* Layout per EDSK_HEADER in samdisk (tracks @ offset 0x30, sides @ 0x31). */
        uint8_t tracks = hdr[0x30];
        uint8_t sides  = (uint8_t)(hdr[0x31] & 0x7F);
        if (tracks == 0 || sides == 0) return -EINVAL;
        if (tracks > 0xFF || sides > 2) {
            /* keep permissive; some images store weird values */
            if (sides == 0) return -EINVAL;
        }

        ufm_disk_init(out);
        fmt_set_label(out, is_edsk ? "EDSK" : "DSK");

        int rc = fmt_ufm_alloc_geom(out, tracks, sides);
        if (rc < 0) return rc;

        rc = ufm_disk_attach_logical(out);
        if (rc < 0) return rc;

        /* Track size table:
         * - standard DSK: hdr[0x32..0x33] => track size in bytes (LE)
         * - EDSK: hdr[0x34 + i] => size in 256-byte blocks per track
         */
        const uint16_t std_trk_sz = (uint16_t)(hdr[0x32] | ((uint16_t)hdr[0x33] << 8));
        const size_t track_table_off = 0x34;

        for (uint16_t c = 0; c < tracks; ++c) {
            for (uint16_t h = 0; h < sides; ++h) {
                /* Resolve track size */
                uint32_t trk_sz = 0;
                if (is_edsk) {
                    const size_t idx = (size_t)c * sides + h;
                    if (track_table_off + idx < sizeof(hdr)) {
                        trk_sz = (uint32_t)hdr[track_table_off + idx] * 256u;
                    }
                } else {
                    trk_sz = std_trk_sz;
                }

                /* A size of 0 means "unformatted track" in EDSK */
                if (trk_sz == 0) continue;

                const long trk_start = ftell(fp);
                if (trk_start < 0) return -EIO;

                uint8_t th[DSK_TRK_HDR_SZ];
                if (!fmt_read_exact(fp, th, sizeof(th))) return -EIO;

                /* track header signature is typically "Track-Info\r\n" */
                static const char trk_sig[] = "Track-Info";
                if (memcmp(th, trk_sig, sizeof(trk_sig) - 1) != 0) {
                    /* tolerate some variants, but do not desync */
                    return -EINVAL;
                }

                /* Track header fields (common DSK/EDSK layout):
                 * 0x10..0x13: unused
                 * 0x14: sector size code N (bytes = 128<<N)
                 * 0x15: number of sectors
                 * 0x16: GAP3
                 * 0x17: filler byte
                 * 0x18.. : sector info list (8 bytes each)
                 */
                const uint8_t n_code = th[0x14];
                const uint8_t n_sectors = th[0x15];
                const uint32_t def_sz = secsize_from_n(n_code);
                const size_t info_off = 0x18;

                /* Data for sectors follows immediately after the info list.
                 * The number of bytes is trk_sz - 256, but we read per sector
                 * using either explicit size (EDSK) or 128<<N.
                 */
                uint32_t consumed = DSK_TRK_HDR_SZ;

                for (uint8_t si = 0; si < n_sectors; ++si) {
                    const size_t e = info_off + (size_t)si * 8u;
                    if (e + 8u > sizeof(th)) return -EINVAL;
                    const uint8_t C = th[e + 0];
                    const uint8_t H = th[e + 1];
                    const uint8_t R = th[e + 2];
                    const uint8_t N = th[e + 3];
                    (void)N;
                    const uint16_t sz_le = (uint16_t)(th[e + 6] | ((uint16_t)th[e + 7] << 8));
                    uint32_t sec_sz = sz_le ? (uint32_t)sz_le : def_sz;
                    if (sec_sz == 0) sec_sz = 512;

                    uint8_t *tmp = (uint8_t*)malloc(sec_sz);
                    if (!tmp) return -ENOMEM;
                    if (!fmt_read_exact(fp, tmp, sec_sz)) {
                        free(tmp);
                        return -EIO;
                    }

                    consumed += sec_sz;

                    rc = ufm_logical_add_sector(out->logical, C, H, R, tmp, sec_sz, UFM_SEC_OK);
                    free(tmp);
                    if (rc < 0) return rc;
                }

                /* Skip per-track padding to align to declared track size. */
                if (trk_sz >= consumed) {
                    const uint32_t pad = trk_sz - consumed;
                    if (pad) {
                        if (fseek(fp, (long)pad, SEEK_CUR) != 0) return -EIO;
                    }
                } else {
                    /* Declared track size smaller than what we consumed: file is inconsistent. */
                    return -EINVAL;
                }
            }
        }

        return 0;
    }

    static int write_dsk(FILE *fp, const ufm_disk_t *in)
    {
        if (!fp || !in) return -EINVAL;
        if (!in->logical) return -EINVAL;

        fluxfmt_dsk_params_t p;
        fluxfmt_dsk_default_params(&p);

        /* Derive geometry defaults */
        if (in->logical->cyls) p.cyls = in->logical->cyls;
        if (in->logical->heads) p.heads = in->logical->heads;
        if (!p.cyls || !p.heads) {
            int rc = infer_geom_from_logical(in->logical, &p.cyls, &p.heads);
            if (rc < 0) return rc;
        }

int fluxfmt_dsk_export_meta_json(FILE *out, const ufm_disk_t *disk)
{
    if (!out || !disk) return -EINVAL;

    const ufm_logical_image_t *li = disk->logical;
    if (!li) return -EINVAL;

    /* Minimal JSON (no external deps). */
    fprintf(out, "{\n");
    fprintf(out, "  \"container\": \"DSK\",\n");
    fprintf(out, "  \"label\": \"%s\",\n", disk->label[0] ? disk->label : "DSK");
    fprintf(out, "  \"geometry\": {\n");
    fprintf(out, "    \"cyls\": %u,\n", (unsigned)li->cyls);
    fprintf(out, "    \"heads\": %u,\n", (unsigned)li->heads);
    fprintf(out, "    \"spt\": %u,\n", (unsigned)li->spt);
    fprintf(out, "    \"sector_size\": %u\n", (unsigned)li->sector_size);
    fprintf(out, "  },\n");

    /* CPC-specific fields will be enriched later (data rate, encoding, gaps, ...). */
    fprintf(out, "  \"sectors\": [\n");
    for (uint32_t i = 0; i < li->count; ++i) {
        const ufm_sector_t *s = &li->sectors[i];
        fprintf(out,
            "    {\"c\":%u,\"h\":%u,\"r\":%u,\"size\":%u,\"flags\":%u}%s\n",
            (unsigned)s->cyl, (unsigned)s->head, (unsigned)s->sec,
            (unsigned)s->size, (unsigned)s->flags,
            (i + 1u < li->count) ? "," : "");
    }
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    return 0;
}
        if (p.heads > 2) return -EINVAL; /* CPC DSK is normally 1 or 2 heads */

        /* We always emit EDSK by default (supports variable track sizes). */
        const bool emit_edsk = p.extended;

        uint8_t hdr[DSK_DISK_HDR_SZ];
        memset(hdr, 0x00, sizeof(hdr));
        if (emit_edsk) {
            wr_pad(&hdr[0x00], 34, "EXTENDED CPC DSK File");
            wr_pad(&hdr[0x22], 14, "UFMT UFM");
        } else {
            wr_pad(&hdr[0x00], 34, "MV - CPCEMU Disk-File");
            wr_pad(&hdr[0x22], 14, "UFMT UFM");
        }
        hdr[0x30] = (uint8_t)p.cyls;
        hdr[0x31] = (uint8_t)p.heads;

        /* Track size table (EDSK): bytes at 0x34... size in 256-byte blocks. */
        const size_t ttab_off = 0x34;
        if (!emit_edsk) {
            /* Standard DSK requires constant track size; we do not guess here. */
            return -ENOTSUP;
        }

        /* We'll precompute per-track payload size to fill the table. */
        uint8_t *track_buf = NULL;
        size_t track_buf_cap = 0;

        /* Write header later after table filled: build it in memory first. */

        for (uint16_t c = 0; c < p.cyls; ++c) {
            for (uint16_t h = 0; h < p.heads; ++h) {
                uint16_t spt = p.spt ? p.spt : infer_spt_for_track(in->logical, c, h);
                if (spt == 0) {
                    /* No sectors on this track: mark unformatted. */
                    const size_t idx = (size_t)c * p.heads + h;
                    if (ttab_off + idx < sizeof(hdr)) hdr[ttab_off + idx] = 0;
                    continue;
                }

                /* Collect sectors for this track */
                ufm_sector_t *tmp = (ufm_sector_t*)calloc(spt, sizeof(ufm_sector_t));
                if (!tmp) return -ENOMEM;
                uint16_t got = 0;
                for (uint32_t i = 0; i < in->logical->count; ++i) {
                    const ufm_sector_t *s = &in->logical->sectors[i];
                    if (s->cyl == c && s->head == h) {
                        if (got < spt) tmp[got++] = *s;
                    }
                }
                /* Sort by sector id (R) for deterministic output */
                qsort(tmp, got, sizeof(tmp[0]), cmp_sector_r);

                /* Infer base sector size */
                uint32_t sec_sz = p.sector_size ? p.sector_size : infer_sector_size_for_track(in->logical, c, h);
                if (sec_sz == 0) sec_sz = 512;
                const uint8_t n_code = n_from_secsize(sec_sz);
                const uint32_t def_sz = secsize_from_n(n_code);

                /* Track header + sector info + data area */
                const size_t info_bytes = (size_t)spt * 8u;
                size_t data_bytes = 0;
                for (uint16_t i = 0; i < got; ++i) {
                    data_bytes += (tmp[i].size ? tmp[i].size : def_sz);
                }
                /* Fill missing sectors (if any) */
                if (got < spt) data_bytes += (size_t)(spt - got) * def_sz;

                size_t trk_total = DSK_TRK_HDR_SZ + info_bytes + data_bytes;
                /* EDSK stores track size rounded up to 256-byte blocks */
                size_t trk_blocks = (trk_total + 255u) / 256u;
                size_t trk_padded = trk_blocks * 256u;

                if (trk_padded > track_buf_cap) {
                    uint8_t *nb = (uint8_t*)realloc(track_buf, trk_padded);
                    if (!nb) { free(tmp); free(track_buf); return -ENOMEM; }
                    track_buf = nb;
                    track_buf_cap = trk_padded;
                }
                memset(track_buf, 0x00, trk_padded);

                /* Build track header */
                wr_pad(&track_buf[0x00], 12, "Track-Info\r\n");
                track_buf[0x10] = (uint8_t)c;
                track_buf[0x11] = (uint8_t)h;
                track_buf[0x14] = n_code;
                track_buf[0x15] = (uint8_t)spt;
                track_buf[0x16] = p.gap3;
                track_buf[0x17] = p.filler;

                /* Sector info list */
                size_t off = 0x18;
                uint16_t next_r = 1;
                for (uint16_t i = 0; i < spt; ++i) {
                    uint8_t C = (uint8_t)c;
                    uint8_t H = (uint8_t)h;
                    uint8_t R = (uint8_t)next_r;
                    uint32_t this_sz = def_sz;
                    const ufm_sector_t *src = NULL;
                    if (i < got) {
                        src = &tmp[i];
                        R = (uint8_t)src->sec;
                        if (src->size) this_sz = src->size;
                    }
                    next_r = (uint16_t)(R + 1u);

                    track_buf[off + 0] = C;
                    track_buf[off + 1] = H;
                    track_buf[off + 2] = R;
                    track_buf[off + 3] = n_code;
                    track_buf[off + 4] = 0; /* ST1 */
                    track_buf[off + 5] = 0; /* ST2 */
                    track_buf[off + 6] = (uint8_t)(this_sz & 0xffu);
                    track_buf[off + 7] = (uint8_t)((this_sz >> 8) & 0xffu);
                    off += 8;
                }

                /* Data area */
                size_t doff = DSK_TRK_HDR_SZ + info_bytes;
                for (uint16_t i = 0; i < spt; ++i) {
                    const ufm_sector_t *src = (i < got) ? &tmp[i] : NULL;
                    uint32_t this_sz = src && src->size ? src->size : def_sz;
                    if (src && src->data && src->size) {
                        memcpy(&track_buf[doff], src->data, src->size);
                    } else {
                        memset(&track_buf[doff], p.filler, this_sz);
                    }
                    doff += this_sz;
                }

                /* Fill EDSK track size table */
                const size_t idx = (size_t)c * p.heads + h;
                if (ttab_off + idx < sizeof(hdr)) {
                    hdr[ttab_off + idx] = (uint8_t)trk_blocks;
                }

                free(tmp);
            }
        }

        /* Now emit file: header + all tracks (in order) */
        if (fseek(fp, 0, SEEK_SET) != 0) { free(track_buf); return -EIO; }
        if (fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) { free(track_buf); return -EIO; }

        for (uint16_t c = 0; c < p.cyls; ++c) {
            for (uint16_t h = 0; h < p.heads; ++h) {
                const size_t idx = (size_t)c * p.heads + h;
                const uint32_t trk_bytes = (uint32_t)hdr[ttab_off + idx] * 256u;
                if (trk_bytes == 0) continue;

                /* Rebuild same as above (we could cache, but keep memory low) */
                uint16_t spt = p.spt ? p.spt : infer_spt_for_track(in->logical, c, h);
                if (spt == 0) continue;

                ufm_sector_t *tmp = (ufm_sector_t*)calloc(spt, sizeof(ufm_sector_t));
                if (!tmp) { free(track_buf); return -ENOMEM; }
                uint16_t got = 0;
                for (uint32_t i = 0; i < in->logical->count; ++i) {
                    const ufm_sector_t *s = &in->logical->sectors[i];
                    if (s->cyl == c && s->head == h) {
                        if (got < spt) tmp[got++] = *s;
                    }
                }
                qsort(tmp, got, sizeof(tmp[0]), cmp_sector_r);

                uint32_t sec_sz = p.sector_size ? p.sector_size : infer_sector_size_for_track(in->logical, c, h);
                if (sec_sz == 0) sec_sz = 512;
                const uint8_t n_code = n_from_secsize(sec_sz);
                const uint32_t def_sz = secsize_from_n(n_code);

                const size_t info_bytes = (size_t)spt * 8u;
                memset(track_buf, 0x00, trk_bytes);
                wr_pad(&track_buf[0x00], 12, "Track-Info\r\n");
                track_buf[0x10] = (uint8_t)c;
                track_buf[0x11] = (uint8_t)h;
                track_buf[0x14] = n_code;
                track_buf[0x15] = (uint8_t)spt;
                track_buf[0x16] = p.gap3;
                track_buf[0x17] = p.filler;

                size_t off = 0x18;
                uint16_t next_r = 1;
                for (uint16_t i = 0; i < spt; ++i) {
                    uint8_t C = (uint8_t)c;
                    uint8_t H = (uint8_t)h;
                    uint8_t R = (uint8_t)next_r;
                    uint32_t this_sz = def_sz;
                    const ufm_sector_t *src = NULL;
                    if (i < got) {
                        src = &tmp[i];
                        R = (uint8_t)src->sec;
                        if (src->size) this_sz = src->size;
                    }
                    next_r = (uint16_t)(R + 1u);
                    track_buf[off + 0] = C;
                    track_buf[off + 1] = H;
                    track_buf[off + 2] = R;
                    track_buf[off + 3] = n_code;
                    track_buf[off + 4] = 0;
                    track_buf[off + 5] = 0;
                    track_buf[off + 6] = (uint8_t)(this_sz & 0xffu);
                    track_buf[off + 7] = (uint8_t)((this_sz >> 8) & 0xffu);
                    off += 8;
                }

                size_t doff = DSK_TRK_HDR_SZ + info_bytes;
                for (uint16_t i = 0; i < spt; ++i) {
                    const ufm_sector_t *src = (i < got) ? &tmp[i] : NULL;
                    uint32_t this_sz = src && src->size ? src->size : def_sz;
                    if (src && src->data && src->size) {
                        memcpy(&track_buf[doff], src->data, src->size);
                    } else {
                        memset(&track_buf[doff], p.filler, this_sz);
                    }
                    doff += this_sz;
                }

                if (fwrite(track_buf, 1, trk_bytes, fp) != trk_bytes) {
                    free(tmp);
                    free(track_buf);
                    return -EIO;
                }
                free(tmp);
            }
        }

        free(track_buf);
        return 0;
    }

    const fluxfmt_plugin_t fluxfmt_dsk_plugin = {
        .name  = "DSK",
        .ext   = "dsk",
        .caps  = FLUXFMT_CAN_READ | FLUXFMT_CAN_WRITE,
        .probe = probe_dsk,
        .read  = read_dsk,
        .write = write_dsk,
    };

int fluxfmt_dsk_export_meta_json(FILE *out, const ufm_disk_t *disk)
{
    if (!out || !disk) return -EINVAL;
    const ufm_logical_image_t *li = disk->logical;
    if (!li) return -EINVAL;

    /* Determine bounds (for pretty summary). */
    uint16_t max_c = 0, max_h = 0;
    uint32_t bad_crc = 0, deleted = 0, weak = 0;
    for (uint32_t i = 0; i < li->count; ++i) {
        const ufm_sector_t *s = &li->sectors[i];
        if (s->cyl > max_c) max_c = s->cyl;
        if (s->head > max_h) max_h = s->head;
        if (s->flags & UFM_SEC_BAD_CRC) bad_crc++;
        if (s->flags & UFM_SEC_DELETED_DAM) deleted++;
        if (s->flags & UFM_SEC_WEAK) weak++;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"container\": \"DSK\",\n");
    fprintf(out, "  \"geometry_hint\": {\"cyls\": %u, \"heads\": %u},\n",
            (unsigned)(li->cyls ? li->cyls : (uint16_t)(max_c + 1u)),
            (unsigned)(li->heads ? li->heads : (uint16_t)(max_h + 1u)));
    fprintf(out, "  \"sector_summary\": {\"count\": %u, \"bad_crc\": %u, \"deleted\": %u, \"weak\": %u},\n",
            (unsigned)li->count, (unsigned)bad_crc, (unsigned)deleted, (unsigned)weak);

    /* Per-sector list (stable ordering as stored). */
    fprintf(out, "  \"sectors\": [\n");
    for (uint32_t i = 0; i < li->count; ++i) {
        const ufm_sector_t *s = &li->sectors[i];
        fprintf(out,
                "    {\"c\":%u,\"h\":%u,\"r\":%u,\"size\":%u,\"flags\":%u}%s\n",
                (unsigned)s->cyl, (unsigned)s->head, (unsigned)s->sec,
                (unsigned)s->size, (unsigned)s->flags,
                (i + 1u < li->count) ? "," : "");
    }
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    return 0;
}
