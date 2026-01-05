/**
 * @file uft_cqm_hardened.c
 * @brief CopyQM CQM Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define CQM_HEADER_SIZE 133
#define CQM_MAX_TRACKS  86
#define CQM_MAX_HEADS   2
#define CQM_MAX_SPT     36

typedef struct { 
    uint8_t*    data; 
    size_t      size;
    uint8_t     tracks;
    uint8_t     heads;
    uint8_t     spt;
    uint16_t    sec_size;
} cqm_data_t;

static size_t cqm_decompress(FILE* f, uint8_t* dst, size_t dst_sz) {
    size_t written = 0;
    
    while (written < dst_sz && !feof(f)) {
        uint8_t count_bytes[2];
        if (fread(count_bytes, 1, 2, f) != 2) break;
        
        int16_t count = (int16_t)(count_bytes[0] | ((uint16_t)count_bytes[1] << 8));
        
        if (count > 0) {
            // RLE: repeat byte
            int b = fgetc(f);
            if (b == EOF) break;
            
            for (int i = 0; i < count && written < dst_sz; i++) {
                dst[written++] = (uint8_t)b;
            }
        } else if (count < 0) {
            // Literal bytes
            int literal_count = -count;
            for (int i = 0; i < literal_count && written < dst_sz; i++) {
                int b = fgetc(f);
                if (b == EOF) break;
                dst[written++] = (uint8_t)b;
            }
        } else {
            // count == 0 -> end marker
            break;
        }
    }
    
    return written;
}

static bool cqm_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 3 && data[0] == 'C' && data[1] == 'Q' && data[2] == 0x14) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t cqm_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[CQM_HEADER_SIZE];
    if (fread(hdr, 1, 18, f) != 18) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    if (hdr[0] != 'C' || hdr[1] != 'Q') {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    cqm_data_t* p = calloc(1, sizeof(cqm_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    uint8_t sz_code = hdr[3];
    p->sec_size = (sz_code < 7) ? (128 << sz_code) : 512;
    p->spt = hdr[8];
    p->heads = hdr[9];
    p->tracks = hdr[15];
    
    // Validate
    if (p->spt > CQM_MAX_SPT || p->heads > CQM_MAX_HEADS || p->tracks > CQM_MAX_TRACKS) {
        free(p); fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Calculate total size with overflow check
    size_t total_size;
    if (!uft_safe_mul_size(p->tracks, p->heads, &total_size) ||
        !uft_safe_mul_size(total_size, p->spt, &total_size) ||
        !uft_safe_mul_size(total_size, p->sec_size, &total_size)) {
        free(p); fclose(f);
        return UFT_ERROR_OVERFLOW;
    }
    
    p->data = malloc(total_size);
    if (!p->data) { free(p); fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->size = total_size;
    memset(p->data, 0, total_size);
    
    // Skip comment
    uint16_t com_len = uft_read_le16(&hdr[16]);
    if (fseek(f, 18 + com_len, SEEK_SET) != 0) {
        free(p->data); free(p); fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Decompress
    cqm_decompress(f, p->data, p->size);
    fclose(f);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = p->sec_size;
    
    return UFT_OK;
}

static void cqm_close(uft_disk_t* disk) {
    if (!disk) return;
    cqm_data_t* p = disk->plugin_data;
    if (p) {
        free(p->data);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t cqm_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    cqm_data_t* p = disk->plugin_data;
    if (!p || !p->data) return UFT_ERROR_INVALID_STATE;
    
    if (cyl < 0 || cyl >= p->tracks) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head >= p->heads) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    size_t track_size = (size_t)p->spt * p->sec_size;
    size_t offset = ((size_t)cyl * p->heads + head) * track_size;
    
    if (offset + track_size > p->size) return UFT_ERROR_BOUNDS;
    
    for (int s = 0; s < p->spt; s++) {
        uft_format_add_sector(track, (uint8_t)s, &p->data[offset + s * p->sec_size],
                             p->sec_size, (uint8_t)cyl, (uint8_t)head);
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_cqm_hardened = {
    .name = "CQM", .description = "CopyQM (HARDENED)", .extensions = "cqm",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = cqm_probe, .open = cqm_open, .close = cqm_close, .read_track = cqm_read_track,
};
