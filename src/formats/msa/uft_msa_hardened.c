/**
 * @file uft_msa_hardened.c
 * @brief Atari ST MSA Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define MSA_MAGIC_0         0x0E
#define MSA_MAGIC_1         0x0F
#define MSA_HEADER_SIZE     10
#define MSA_RLE_MARKER      0xE5
#define MSA_SECTOR_SIZE     512
#define MSA_MAX_TRACKS      86
#define MSA_MAX_SIDES       2
#define MSA_MAX_SPT         12

typedef struct {
    uint8_t*    decompressed;
    size_t      decompressed_size;
    uint16_t    spt;
    uint16_t    sides;
    uint16_t    start_track;
    uint16_t    end_track;
} msa_data_t;

static size_t msa_decompress_track(const uint8_t* src, size_t src_len, 
                                    uint8_t* dst, size_t dst_len) {
    size_t si = 0, di = 0;
    
    while (si < src_len && di < dst_len) {
        uint8_t b = src[si++];
        
        if (b == MSA_RLE_MARKER && si + 2 < src_len) {
            if (src[si] == MSA_RLE_MARKER) {
                si++;
                uint8_t count = src[si++];
                uint8_t value = src[si++];
                for (int i = 0; i < count && di < dst_len; i++) {
                    dst[di++] = value;
                }
            } else {
                dst[di++] = b;
            }
        } else {
            dst[di++] = b;
        }
    }
    return di;
}

static bool msa_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < MSA_HEADER_SIZE) return false;
    
    if (data[0] == MSA_MAGIC_0 && data[1] == MSA_MAGIC_1) {
        uint16_t spt = uft_read_be16(&data[2]);
        uint16_t sides = uft_read_be16(&data[4]);
        
        if (spt >= 9 && spt <= MSA_MAX_SPT && sides <= 1) {
            *confidence = 95;
            return true;
        }
    }
    return false;
}

static uft_error_t msa_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[MSA_HEADER_SIZE];
    if (fread(header, 1, MSA_HEADER_SIZE, f) != MSA_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    if (header[0] != MSA_MAGIC_0 || header[1] != MSA_MAGIC_1) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    msa_data_t* p = calloc(1, sizeof(msa_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->spt = uft_read_be16(&header[2]);
    p->sides = uft_read_be16(&header[4]) + 1;
    p->start_track = uft_read_be16(&header[6]);
    p->end_track = uft_read_be16(&header[8]);
    
    // Validate
    if (p->spt > MSA_MAX_SPT || p->sides > MSA_MAX_SIDES || 
        p->end_track >= MSA_MAX_TRACKS || p->end_track < p->start_track) {
        free(p); fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Calculate decompressed size
    size_t track_size = (size_t)p->spt * MSA_SECTOR_SIZE;
    size_t num_tracks = (p->end_track - p->start_track + 1) * p->sides;
    
    // Overflow check
    size_t total_size;
    if (!uft_safe_mul_size(num_tracks, track_size, &total_size)) {
        free(p); fclose(f);
        return UFT_ERROR_OVERFLOW;
    }
    
    p->decompressed = malloc(total_size);
    if (!p->decompressed) { free(p); fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->decompressed_size = total_size;
    memset(p->decompressed, 0, total_size);
    
    // Decompress tracks
    uint8_t* comp_buf = malloc(track_size * 2);
    if (!comp_buf) {
        free(p->decompressed); free(p); fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    uint8_t* dest = p->decompressed;
    for (uint16_t t = p->start_track; t <= p->end_track; t++) {
        for (uint16_t s = 0; s < p->sides; s++) {
            uint8_t len_bytes[2];
            if (fread(len_bytes, 1, 2, f) != 2) break;
            
            uint16_t comp_len = uft_read_be16(len_bytes);
            if (comp_len > track_size * 2) break;
            
            if (fread(comp_buf, 1, comp_len, f) != comp_len) break;
            
            if (comp_len == track_size) {
                memcpy(dest, comp_buf, track_size);
            } else {
                msa_decompress_track(comp_buf, comp_len, dest, track_size);
            }
            dest += track_size;
        }
    }
    
    free(comp_buf);
    fclose(f);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->end_track + 1;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = MSA_SECTOR_SIZE;
    
    return UFT_OK;
}

static void msa_close(uft_disk_t* disk) {
    if (!disk) return;
    msa_data_t* p = disk->plugin_data;
    if (p) {
        free(p->decompressed);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t msa_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    msa_data_t* p = disk->plugin_data;
    if (!p || !p->decompressed) return UFT_ERROR_INVALID_STATE;
    
    if (cyl < (int)p->start_track || cyl > (int)p->end_track) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head >= (int)p->sides) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    size_t track_size = (size_t)p->spt * MSA_SECTOR_SIZE;
    size_t track_idx = ((cyl - p->start_track) * p->sides + head);
    size_t offset = track_idx * track_size;
    
    if (offset + track_size > p->decompressed_size) return UFT_ERROR_BOUNDS;
    
    uint8_t* track_data = p->decompressed + offset;
    
    for (int sec = 0; sec < p->spt; sec++) {
        uft_format_add_sector(track, (uint8_t)sec, 
                             &track_data[sec * MSA_SECTOR_SIZE],
                             MSA_SECTOR_SIZE, (uint8_t)cyl, (uint8_t)head);
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_msa_hardened = {
    .name = "MSA", .description = "Atari ST MSA (HARDENED)", .extensions = "msa",
    .version = 0x00010001, .format = UFT_FORMAT_MSA,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = msa_probe, .open = msa_open, .close = msa_close, .read_track = msa_read_track,
};
