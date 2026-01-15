/**
 * @file uft_cqm.c
 * @brief CQM format v2 implementation
 * @version 3.8.0
 */
#include <stdio.h>  // FIXED R18
#include "uft/uft_format_common.h"

typedef struct { 
    uint8_t* data; 
    size_t size;
    uint8_t tracks, heads, spt;
    uint16_t sec_size;
} cqm_data_t;

static size_t cqm_decomp(FILE* f, uint8_t* dst, size_t dst_sz) {
    size_t w = 0;
    while (w < dst_sz && !feof(f)) {
        uint8_t cb[2];
        if (fread(cb, 1, 2, f) != 2) break;
        int16_t cnt = (int16_t)(cb[0] | (cb[1] << 8));
        if (cnt > 0) {
            int b = fgetc(f);
            if (b == EOF) break;
            for (int i = 0; i < cnt && w < dst_sz; i++) dst[w++] = b;
        } else if (cnt < 0) {
            for (int i = 0; i < -cnt && w < dst_sz; i++) {
                int b = fgetc(f);
                if (b == EOF) break;
                dst[w++] = b;
            }
        } else break;
    }
    return w;
}

static bool cqm_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 3 && data[0] == 'C' && data[1] == 'Q' && data[2] == 0x14) {
        *confidence = 95; return true;
    }
    return false;
}

static uft_error_t cqm_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[18];
    if (fread(hdr, 1, 18, f) != 18) { /* I/O error */ }
    if (hdr[0] != 'C' || hdr[1] != 'Q') { fclose(f); return UFT_ERROR_FORMAT_INVALID; }
    
    cqm_data_t* p = calloc(1, sizeof(cqm_data_t));
    uint8_t sz_code = hdr[3];
    p->sec_size = (sz_code < 7) ? (128 << sz_code) : 512;
    p->spt = hdr[8];
    p->heads = hdr[9];
    p->tracks = hdr[15];
    
    uint16_t com_len = uft_read_le16(&hdr[16]);
    if (fseek(f, 18 + com_len, SEEK_SET) != 0) { /* seek error */ }
    p->size = (size_t)p->tracks * p->heads * p->spt * p->sec_size;
    p->data = malloc(p->size);
    if(!p->data) { fclose(f); return NULL; } /* P0-SEC-001 */
    memset(p->data, 0, p->size);
    cqm_decomp(f, p->data, p->size);
    fclose(f);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = p->sec_size;
    return UFT_OK;
}

static void cqm_close(uft_disk_t* disk) {
    cqm_data_t* p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

static uft_error_t cqm_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    cqm_data_t* p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    size_t off = ((size_t)cyl * p->heads + head) * p->spt * p->sec_size;
    for (int s = 0; s < p->spt && off + p->sec_size <= p->size; s++) {
        uft_format_add_sector(track, s, &p->data[off], p->sec_size, cyl, head);
        off += p->sec_size;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_cqm = {
    .name = "CQM", .description = "CopyQM Compressed", .extensions = "cqm",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = cqm_probe, .open = cqm_open, .close = cqm_close, .read_track = cqm_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(cqm)
