/**
 * @file uft_d64_hardened.c
 * @brief Commodore 64/1541 D64 Format Plugin - HARDENED VERSION
 * 
 * SECURITY: All malloc NULL-checked, all fread/fseek return-checked,
 *           bounds validation on all track/sector access
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define D64_SECTOR_SIZE     256
#define D64_TRACKS_STD      35
#define D64_TRACKS_EXT      40
#define D64_HEADS           1
#define D64_BAM_TRACK       18
#define D64_BAM_SECTOR      0

static const uint8_t d64_sectors_per_track[40] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17
};

static const uint16_t d64_track_offset[42] = {
    0,21,42,63,84,105,126,147,168,189,210,231,252,273,294,315,336,357,
    376,395,414,433,452,471,490,508,526,544,562,580,598,615,632,649,666,
    683,700,717,734,751,768
};

typedef struct {
    FILE*       file;
    uint8_t     num_tracks;
    bool        has_errors;
    uint8_t*    error_info;
    size_t      file_size;
    uint16_t    total_sectors;
} d64_data_t;

static uint16_t d64_total_sectors(uint8_t tracks) {
    if (tracks > 40) tracks = 40;
    return d64_track_offset[tracks];
}

static bool d64_detect_variant(size_t file_size, uint8_t* tracks, bool* has_errors) {
    *has_errors = false;
    
    // Standard 35 track
    if (file_size == 174848) { *tracks = 35; return true; }
    if (file_size == 175531) { *tracks = 35; *has_errors = true; return true; }
    
    // Extended 40 track
    if (file_size == 196608) { *tracks = 40; return true; }
    if (file_size == 197376) { *tracks = 40; *has_errors = true; return true; }
    
    return false;
}

static bool d64_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    uint8_t tracks;
    bool has_errors;
    
    if (!d64_detect_variant(file_size, &tracks, &has_errors)) {
        return false;
    }
    
    // Check BAM signature
    if (size >= D64_SECTOR_SIZE * (d64_track_offset[D64_BAM_TRACK - 1] + 1)) {
        size_t bam_offset = (size_t)d64_track_offset[D64_BAM_TRACK - 1] * D64_SECTOR_SIZE;
        if (bam_offset + 4 <= size) {
            const uint8_t* bam = data + bam_offset;
            if (bam[0] == 18 && bam[1] == 1) {
                *confidence = 95;
                return true;
            }
        }
    }
    
    *confidence = 75;
    return true;
}

static uft_error_t d64_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    // Get file size
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    long pos = ftell(f);
    if (pos < 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    size_t file_size = (size_t)pos;
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Detect variant
    uint8_t num_tracks;
    bool has_errors;
    if (!d64_detect_variant(file_size, &num_tracks, &has_errors)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Allocate plugin data
    d64_data_t* pdata = calloc(1, sizeof(d64_data_t));
    if (!pdata) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    pdata->file = f;
    pdata->num_tracks = num_tracks;
    pdata->has_errors = has_errors;
    pdata->file_size = file_size;
    pdata->total_sectors = d64_total_sectors(num_tracks);
    
    // Load error info if present
    if (has_errors) {
        pdata->error_info = malloc(pdata->total_sectors);
        if (!pdata->error_info) {
            free(pdata);
            fclose(f);
            return UFT_ERROR_NO_MEMORY;
        }
        
        size_t error_offset = (size_t)pdata->total_sectors * D64_SECTOR_SIZE;
        if (fseek(f, (long)error_offset, SEEK_SET) != 0 ||
            fread(pdata->error_info, 1, pdata->total_sectors, f) != pdata->total_sectors) {
            free(pdata->error_info);
            free(pdata);
            fclose(f);
            return UFT_ERROR_FILE_READ;
        }
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = num_tracks;
    disk->geometry.heads = D64_HEADS;
    disk->geometry.sectors = 17;
    disk->geometry.sector_size = D64_SECTOR_SIZE;
    disk->geometry.total_sectors = pdata->total_sectors;
    
    return UFT_OK;
}

static void d64_close(uft_disk_t* disk) {
    if (!disk) return;
    
    d64_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) {
            fclose(pdata->file);
            pdata->file = NULL;
        }
        free(pdata->error_info);
        pdata->error_info = NULL;
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d64_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    d64_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    
    // Bounds check
    if (head != 0) return UFT_ERROR_INVALID_ARG;
    if (cyl < 0 || cyl >= pdata->num_tracks) return UFT_ERROR_INVALID_ARG;
    
    int track_num = cyl + 1;  // 1-based
    int num_sectors = d64_sectors_per_track[cyl];
    
    uft_track_init(track, cyl, head);
    
    uint8_t sector_buf[D64_SECTOR_SIZE];
    for (int sec = 0; sec < num_sectors; sec++) {
        size_t offset = ((size_t)d64_track_offset[cyl] + sec) * D64_SECTOR_SIZE;
        
        if (fseek(pdata->file, (long)offset, SEEK_SET) != 0) {
            continue;  // Skip this sector
        }
        
        if (fread(sector_buf, 1, D64_SECTOR_SIZE, pdata->file) != D64_SECTOR_SIZE) {
            continue;  // Skip this sector
        }
        
        uft_sector_t sector;
        memset(&sector, 0, sizeof(sector));
        
        sector.id.cylinder = (uint8_t)track_num;
        sector.id.head = 0;
        sector.id.sector = (uint8_t)sec;
        sector.id.size_code = 1;  // 256 bytes
        sector.id.crc_ok = true;
        
        sector.data = malloc(D64_SECTOR_SIZE);
        if (!sector.data) continue;
        
        memcpy(sector.data, sector_buf, D64_SECTOR_SIZE);
        sector.data_size = D64_SECTOR_SIZE;
        sector.status = UFT_SECTOR_OK;
        
        // Check error info
        if (pdata->has_errors && pdata->error_info) {
            uint16_t sec_idx = d64_track_offset[cyl] + sec;
            if (sec_idx < pdata->total_sectors && pdata->error_info[sec_idx] != 0x01) {
                sector.status |= UFT_SECTOR_CRC_ERROR;
            }
        }
        
        uft_error_t err = uft_track_add_sector(track, &sector);
        if (err != UFT_OK) {
            free(sector.data);
        }
    }
    
    return UFT_OK;
}

static uft_error_t d64_write_track(uft_disk_t* disk, int cyl, int head, const uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    d64_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    
    if (head != 0) return UFT_ERROR_INVALID_ARG;
    if (cyl < 0 || cyl >= pdata->num_tracks) return UFT_ERROR_INVALID_ARG;
    
    int num_sectors = d64_sectors_per_track[cyl];
    
    for (int sec = 0; sec < num_sectors; sec++) {
        const uft_sector_t* sector = uft_track_find_sector(track, sec);
        if (!sector || !sector->data) continue;
        
        size_t offset = ((size_t)d64_track_offset[cyl] + sec) * D64_SECTOR_SIZE;
        
        if (fseek(pdata->file, (long)offset, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        size_t write_size = (sector->data_size < D64_SECTOR_SIZE) ? 
                            sector->data_size : D64_SECTOR_SIZE;
        
        if (fwrite(sector->data, 1, write_size, pdata->file) != write_size) {
            return UFT_ERROR_FILE_WRITE;
        }
        
        // Pad if needed
        if (write_size < D64_SECTOR_SIZE) {
            uint8_t pad[D64_SECTOR_SIZE] = {0};
            if (fwrite(pad, 1, D64_SECTOR_SIZE - write_size, pdata->file) != 
                D64_SECTOR_SIZE - write_size) {
                return UFT_ERROR_FILE_WRITE;
            }
        }
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d64_hardened = {
    .name = "D64",
    .description = "Commodore 64/1541 (HARDENED)",
    .extensions = "d64",
    .version = 0x00010001,
    .format = UFT_FORMAT_D64,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_CREATE,
    .probe = d64_probe,
    .open = d64_open,
    .close = d64_close,
    .read_track = d64_read_track,
    .write_track = d64_write_track,
};
