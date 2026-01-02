/**
 * @file uft_msa.c
 * @brief Atari ST MSA (Magic Shadow Archiver) Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"

#define MSA_MAGIC_0         0x0E
#define MSA_MAGIC_1         0x0F
#define MSA_HEADER_SIZE     10
#define MSA_RLE_MARKER      0xE5
#define MSA_SECTOR_SIZE     512

typedef struct {
    FILE*       file;
    uint16_t    sectors_per_track;
    uint16_t    sides;
    uint16_t    start_track;
    uint16_t    end_track;
    uint8_t*    decompressed;
    size_t      decompressed_size;
} msa_data_t;

static size_t msa_decompress_track(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    size_t si = 0, di = 0;
    while (si < src_len && di < dst_len) {
        uint8_t b = src[si++];
        if (b == MSA_RLE_MARKER && si + 2 < src_len && src[si] == MSA_RLE_MARKER) {
            si++;
            uint8_t count = src[si++];
            uint8_t value = src[si++];
            for (int i = 0; i < count && di < dst_len; i++) dst[di++] = value;
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
        if (spt >= 9 && spt <= 11 && sides <= 1) {
            *confidence = 95;
            return true;
        }
    }
    return false;
}

static uft_error_t msa_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[MSA_HEADER_SIZE];
    if (fread(header, 1, MSA_HEADER_SIZE, f) != MSA_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    if (header[0] != MSA_MAGIC_0 || header[1] != MSA_MAGIC_1) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    msa_data_t* pdata = calloc(1, sizeof(msa_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->sectors_per_track = uft_read_be16(&header[2]);
    pdata->sides = uft_read_be16(&header[4]) + 1;
    pdata->start_track = uft_read_be16(&header[6]);
    pdata->end_track = uft_read_be16(&header[8]);
    
    // Alle Tracks dekomprimieren
    size_t track_size = pdata->sectors_per_track * MSA_SECTOR_SIZE;
    size_t num_tracks = (pdata->end_track - pdata->start_track + 1) * pdata->sides;
    pdata->decompressed_size = num_tracks * track_size;
    pdata->decompressed = malloc(pdata->decompressed_size);
    
    if (!pdata->decompressed) {
        fclose(f);
        free(pdata);
        return UFT_ERROR_NO_MEMORY;
    }
    
    uint8_t* comp_buf = malloc(track_size * 2);
    uint8_t* dest = pdata->decompressed;
    
    for (uint16_t t = pdata->start_track; t <= pdata->end_track; t++) {
        for (uint16_t s = 0; s < pdata->sides; s++) {
            uint8_t len_bytes[2];
            if (fread(len_bytes, 1, 2, f) != 2) break;
            uint16_t comp_len = uft_read_be16(len_bytes);
            
            if (comp_len > track_size * 2) break;
            fread(comp_buf, 1, comp_len, f);
            
            if (comp_len == track_size) {
                memcpy(dest, comp_buf, track_size);
            } else {
                msa_decompress_track(comp_buf, comp_len, dest, track_size);
            }
            dest += track_size;
        }
    }
    
    free(comp_buf);
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->end_track + 1;
    disk->geometry.heads = pdata->sides;
    disk->geometry.sectors = pdata->sectors_per_track;
    disk->geometry.sector_size = MSA_SECTOR_SIZE;
    disk->geometry.total_sectors = num_tracks * pdata->sectors_per_track;
    
    return UFT_OK;
}

static void msa_close(uft_disk_t* disk) {
    msa_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata->decompressed);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t msa_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    msa_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->decompressed) return UFT_ERROR_INVALID_STATE;
    
    if (cyl < pdata->start_track || cyl > pdata->end_track) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head >= pdata->sides) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    size_t track_size = pdata->sectors_per_track * MSA_SECTOR_SIZE;
    size_t track_idx = ((cyl - pdata->start_track) * pdata->sides + head);
    uint8_t* track_data = pdata->decompressed + track_idx * track_size;
    
    for (int sec = 0; sec < pdata->sectors_per_track; sec++) {
        uft_format_add_sector(track, sec, &track_data[sec * MSA_SECTOR_SIZE], 
                             MSA_SECTOR_SIZE, cyl, head);
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_msa = {
    .name = "MSA",
    .description = "Atari ST Magic Shadow Archiver",
    .extensions = "msa",
    .version = 0x00010000,
    .format = UFT_FORMAT_MSA,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = msa_probe,
    .open = msa_open,
    .close = msa_close,
    .read_track = msa_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(msa)
