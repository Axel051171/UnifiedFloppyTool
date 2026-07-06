/**
 * @file uft_dmk.c
 * @brief TRS-80 DMK format core
 * @version 3.8.0
 */
#include "uft/uft_format_common.h"

#define DMK_HDR 16
#define DMK_IDAM_SIZE 128

/* WD177x FDC CRC = CRC-CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection,
 * no final XOR). Verified against the standard check value CRC("123456789")
 * == 0x29B1. Kept as a small self-contained helper so the plugin stays free of
 * link deps (canonical: uft_crc16_ccitt in uft_crc_polys.h). MF-353. */
static uint16_t dmk_crc16(const uint8_t *d, size_t n) {
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t)d[i] << 8;
        for (int j = 0; j < 8; j++)
            c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

typedef struct { FILE* file; uint8_t tracks; uint16_t track_len; bool ss; } dmk_data_t;

bool dmk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < DMK_HDR) return false;
    uint8_t tracks = data[1];
    uint16_t tlen = uft_read_le16(&data[2]);
    if (tracks > 0 && tracks <= 96 && tlen >= 1000 && tlen <= 20000) {
        *confidence = 85; return true;
    }
    return false;
}

static uft_error_t dmk_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    uint8_t hdr[DMK_HDR];
    if (fread(hdr, 1, DMK_HDR, f) != DMK_HDR) { fclose(f); return UFT_ERR_IO; }
    dmk_data_t* p = calloc(1, sizeof(dmk_data_t));
    if (!p) { fclose(f); return UFT_ERR_MEMORY; }
    p->file = f;
    p->tracks = hdr[1];
    p->track_len = uft_read_le16(&hdr[2]);
    p->ss = (hdr[4] & 0x10) != 0;
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->ss ? 1 : 2;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders * disk->geometry.heads * disk->geometry.sectors;
    return UFT_OK;
}

static void dmk_close(uft_disk_t* disk) {
    dmk_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t dmk_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    dmk_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    
    size_t off = DMK_HDR + ((size_t)cyl * (p->ss ? 1 : 2) + head) * p->track_len;
    if (fseek(p->file, off, SEEK_SET) != 0) { return UFT_ERR_IO; }
    if (p->track_len > UFT_MAX_ALLOC_SIZE) return UFT_ERR_IO;
    uint8_t* tbuf = malloc(p->track_len);
    if (!tbuf) return UFT_ERR_MEMORY;
    if (fread(tbuf, 1, p->track_len, p->file) != p->track_len) { free(tbuf); return UFT_ERR_IO; }
    // Parse IDAM table
    for (int i = 0; i < 64; i++) {
        uint16_t ptr = uft_read_le16(&tbuf[i * 2]);
        if (ptr == 0 || ptr == 0xFFFF) break;
        uint16_t idam_off = (ptr & 0x3FFF) - DMK_IDAM_SIZE;
        if (idam_off >= p->track_len - 20) continue;
        
        uint8_t* idam = &tbuf[DMK_IDAM_SIZE + idam_off];
        if (idam[0] != 0xFE) continue;
        
        uint8_t sec_id = idam[3], sz = idam[4] & 3;
        uint16_t sec_sz = 128 << sz;

        /* ID-field CRC (WD177x): CRC covers the address mark + C/H/R/N and, in
         * MFM, the three 0xA1 sync bytes that precede the 0xFE. Detect MFM from
         * the actual preceding bytes (FM has no A1). Stored CRC is the two bytes
         * after N, big-endian (FDC writes MSB first). MF-353. */
        bool id_mfm = (idam_off >= 3 && idam[-1] == 0xA1 &&
                       idam[-2] == 0xA1 && idam[-3] == 0xA1);
        uint16_t id_crc_calc = dmk_crc16(id_mfm ? idam - 3 : idam,
                                         id_mfm ? 8 : 5);
        uint16_t id_crc_stored = (uint16_t)(((uint16_t)idam[5] << 8) | idam[6]);
        bool id_crc_ok = (id_crc_calc == id_crc_stored);

        // Find DAM (0xFB=normal, 0xF8=deleted)
        for (int j = 7; j < 60 && idam_off + j < p->track_len - sec_sz; j++) {
            if (idam[j] == 0xFB || idam[j] == 0xF8) {
                uft_format_add_sector(track, sec_id - 1, &idam[j + 1], sec_sz, cyl, head);
                if (track->sector_count > 0) {
                    uft_sector_t *last = &track->sectors[track->sector_count - 1];
                    if (idam[j] == 0xF8) last->deleted = true;
                    /* ID-field CRC error is a header fault, kept separate from
                     * the data CRC (MF-338 id_crc_ok). */
                    if (!id_crc_ok) uft_sector_set_id_crc(last, false);
                    /* Data-field CRC: [A1 A1 A1] DAM + data, CRC stored as the
                     * two big-endian bytes after the data. */
                    const uint8_t *dam = &idam[j];
                    bool d_mfm = (idam_off + j >= 3 && dam[-1] == 0xA1 &&
                                  dam[-2] == 0xA1 && dam[-3] == 0xA1);
                    size_t crc_pos = (size_t)DMK_IDAM_SIZE + idam_off + j + 1 + sec_sz;
                    if (crc_pos + 2 <= (size_t)p->track_len) {
                        uint16_t d_crc_calc = dmk_crc16(d_mfm ? dam - 3 : dam,
                                                        (d_mfm ? 3u : 0u) + 1u + sec_sz);
                        uint16_t d_crc_stored = (uint16_t)(((uint16_t)dam[1 + sec_sz] << 8) |
                                                            dam[2 + sec_sz]);
                        if (d_crc_calc != d_crc_stored)
                            uft_sector_set_crc(last, false);
                    }
                }
                break;
            }
        }
    }
    
    free(tbuf);
    return UFT_OK;
}

static uft_error_t dmk_write_track(uft_disk_t* disk, int cyl, int head,
                                     const uft_track_t* track) {
    dmk_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERR_INVALID_STATE;
    if (disk->read_only) return UFT_ERR_NOT_SUPPORTED;

    size_t off = DMK_HDR + ((size_t)cyl * (p->ss ? 1 : 2) + head) * p->track_len;
    if (fseek(p->file, (long)off, SEEK_SET) != 0) return UFT_ERR_IO;
    if (p->track_len > UFT_MAX_ALLOC_SIZE) return UFT_ERR_IO;

    uint8_t* tbuf = malloc(p->track_len);
    if (!tbuf) return UFT_ERR_MEMORY;
    if (fread(tbuf, 1, p->track_len, p->file) != p->track_len) { free(tbuf); return UFT_ERR_IO; }

    /* Walk IDAM table, find each sector's DAM, replace data */
    for (int i = 0; i < 64; i++) {
        uint16_t ptr = uft_read_le16(&tbuf[i * 2]);
        if (ptr == 0 || ptr == 0xFFFF) break;
        uint16_t idam_off = (ptr & 0x3FFF) - DMK_IDAM_SIZE;
        if (idam_off >= p->track_len - 20) continue;

        uint8_t* idam = &tbuf[DMK_IDAM_SIZE + idam_off];
        if (idam[0] != 0xFE) continue;

        uint8_t sec_id = idam[3], sz = idam[4] & 3;
        uint16_t sec_sz = 128 << sz;

        /* Find DAM (0xFB or 0xF8) */
        for (int j = 7; j < 60 && idam_off + j < p->track_len - sec_sz; j++) {
            if (idam[j] == 0xFB || idam[j] == 0xF8) {
                /* Find matching sector in input track (sec_id is 1-based in DMK) */
                for (size_t ts = 0; ts < track->sector_count; ts++) {
                    if (track->sectors[ts].id.sector == (uint8_t)(sec_id - 1)) {
                        const uint8_t *src = track->sectors[ts].data;
                        if (src && track->sectors[ts].data_len >= sec_sz)
                            memcpy(&idam[j + 1], src, sec_sz);
                        break;
                    }
                }
                break;
            }
        }
    }

    /* Write modified track buffer back */
    if (fseek(p->file, (long)off, SEEK_SET) != 0) { free(tbuf); return UFT_ERR_IO; }
    if (fwrite(tbuf, 1, p->track_len, p->file) != p->track_len) { free(tbuf); return UFT_ERR_IO; }
    fflush(p->file);

    free(tbuf);
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_dmk_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dmk = {
    .name = "DMK", .description = "TRS-80 David Keil", .extensions = "dmk",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = dmk_probe, .open = dmk_open, .close = dmk_close, .read_track = dmk_read_track,
    .write_track = dmk_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_dmk_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dmk_features) / sizeof(uft_format_plugin_dmk_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(dmk)
