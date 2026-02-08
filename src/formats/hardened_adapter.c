/**
 * @file hardened_adapter.c
 * @brief Adapter to integrate hardened parsers into plugin system
 * 
 * Wraps the secure APIs into the uft_format_plugin_t interface.
 */

#include "uft/uft_format_plugin.h"
#include "uft/core/uft_safe_math.h"
#include "uft/formats/scp_hardened.h"
#include "uft/formats/d64_hardened.h"
#include "uft/core/uft_error_compat.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// SCP ADAPTER
// ============================================================================

typedef struct {
    uft_scp_image_hardened_t *scp;
} scp_plugin_data_t;

static bool scp_probe_hardened(const uint8_t *data, size_t size, 
                                size_t file_size, int *confidence) {
    *confidence = 0;
    if (size < 3) return false;
    
    if (data[0] == 'S' && data[1] == 'C' && data[2] == 'P') {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t scp_open_hardened(uft_disk_t *disk, const char *path, 
                                      bool read_only) {
    (void)read_only;  // SCP is always read-only
    
    scp_plugin_data_t *pdata = calloc(1, sizeof(*pdata));
    if (!pdata) return UFT_ERROR_NO_MEMORY;
    
    uft_scp_error_t rc = uft_scp_open_safe(path, &pdata->scp);
    if (rc != UFT_SCP_OK) {
        free(pdata);
        switch (rc) {
            case UFT_SCP_EINVAL:   return UFT_ERROR_INVALID_ARG;
            case UFT_SCP_EIO:      return UFT_ERROR_FILE_READ;
            case UFT_SCP_EFORMAT:  return UFT_ERROR_FORMAT_INVALID;
            case UFT_SCP_EBOUNDS:  return UFT_ERROR_OUT_OF_RANGE;
            case UFT_SCP_ENOMEM:   return UFT_ERROR_NO_MEMORY;
            default:              return UFT_ERROR_UNKNOWN;
        }
    }
    
    // Get header for geometry
    uft_scp_header_t hdr;
    uft_scp_get_header(pdata->scp, &hdr);
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = hdr.end_track - hdr.start_track + 1;
    disk->geometry.heads = hdr.heads ? 2 : 1;
    disk->geometry.sectors = 0;  // Flux format - no sectors
    disk->geometry.sector_size = 0;
    disk->format = UFT_FORMAT_SCP;
    disk->read_only = true;
    
    return UFT_OK;
}

static void scp_close_hardened(uft_disk_t *disk) {
    if (!disk || !disk->plugin_data) return;
    
    scp_plugin_data_t *pdata = disk->plugin_data;
    uft_scp_close_safe(&pdata->scp);
    free(pdata);
    disk->plugin_data = NULL;
}

// ============================================================================
// D64 ADAPTER
// ============================================================================

typedef struct {
    uft_d64_image_hardened_t *d64;
} d64_plugin_data_t;

static bool d64_probe_hardened(const uint8_t *data, size_t size,
                                size_t file_size, int *confidence) {
    *confidence = 0;
    
    // Check known sizes
    switch (file_size) {
        case UFT_D64_SIZE_35:
        case UFT_D64_SIZE_35_ERR:
        case UFT_D64_SIZE_40:
        case UFT_D64_SIZE_40_ERR:
        case UFT_D64_SIZE_42:
        case UFT_D64_SIZE_42_ERR:
            *confidence = 50;
            break;
        default:
            return false;
    }
    
    // Check BAM location
    if (file_size >= 91648 && size >= 91648) {
        const uint8_t *bam = data + 91392;  // Track 18, Sector 0
        
        // Directory pointer should be 18, 1
        if (bam[0] == 18 && bam[1] == 1) {
            *confidence = 70;
        }
        
        // DOS version 'A'
        if (bam[2] == 0x41) {
            *confidence = 85;
        }
        
        // Check for 0xA0 separator
        if (bam[164] == 0xA0) {
            *confidence = 95;
        }
    }
    
    return *confidence > 0;
}

static uft_error_t d64_open_hardened(uft_disk_t *disk, const char *path,
                                      bool read_only) {
    d64_plugin_data_t *pdata = calloc(1, sizeof(*pdata));
    if (!pdata) return UFT_ERROR_NO_MEMORY;
    
    uft_d64_error_t rc = uft_d64_open_safe(path, read_only, &pdata->d64);
    if (rc != UFT_D64_OK) {
        free(pdata);
        switch (rc) {
            case UFT_D64_EINVAL:    return UFT_ERROR_INVALID_ARG;
            case UFT_D64_EIO:       return UFT_ERROR_FILE_READ;
            case UFT_D64_EFORMAT:   return UFT_ERROR_FORMAT_INVALID;
            case UFT_D64_EBOUNDS:   return UFT_ERROR_OUT_OF_RANGE;
            case UFT_D64_ENOMEM:    return UFT_ERROR_NO_MEMORY;
            default:               return UFT_ERROR_UNKNOWN;
        }
    }
    
    uint8_t num_tracks;
    uint16_t total_sectors;
    bool has_errors;
    uft_d64_get_geometry(pdata->d64, &num_tracks, &total_sectors, &has_errors);
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = num_tracks;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 17;  // Minimum for display
    disk->geometry.sector_size = UFT_D64_SECTOR_SIZE;
    disk->geometry.total_sectors = total_sectors;
    disk->format = UFT_FORMAT_D64;
    disk->read_only = read_only;
    
    return UFT_OK;
}

static void d64_close_hardened(uft_disk_t *disk) {
    if (!disk || !disk->plugin_data) return;
    
    d64_plugin_data_t *pdata = disk->plugin_data;
    uft_d64_close_safe(&pdata->d64);
    free(pdata);
    disk->plugin_data = NULL;
}

static uft_error_t d64_read_track_adapter(uft_disk_t *disk, int cylinder, 
                                           int head, uft_track_t *track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    if (head != 0) return UFT_ERROR_OUT_OF_RANGE;
    
    d64_plugin_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->d64) return UFT_ERROR_DISK_NOT_OPEN;
    
    uint8_t d64_track = (uint8_t)(cylinder + 1);
    uint8_t num_sectors = uft_d64_sectors_per_track(d64_track);
    
    if (num_sectors == 0) return UFT_ERROR_OUT_OF_RANGE;
    
    uft_track_init(track, cylinder, head);
    
    uft_d64_sector_t sectors[21];  // Max sectors per track
    size_t count;
    
    uft_d64_error_t rc = uft_d64_read_track_safe(pdata->d64, d64_track, 
                                                  sectors, 21, &count);
    if (rc != UFT_D64_OK) {
        return UFT_ERROR_FILE_READ;
    }
    
    for (size_t i = 0; i < count; i++) {
        uft_sector_t s = {0};
        s.id.cylinder = sectors[i].id.cylinder;
        s.id.head = sectors[i].id.head;
        s.id.sector = sectors[i].id.sector;
        s.id.size_code = 1;  // 256 bytes
        s.id.crc_ok = sectors[i].id.crc_ok;
        
        s.data = malloc(UFT_D64_SECTOR_SIZE);
        if (!s.data) {
            uft_track_clear(track);
            return UFT_ERROR_NO_MEMORY;
        }
        memcpy(s.data, sectors[i].data, UFT_D64_SECTOR_SIZE);
        s.data_size = UFT_D64_SECTOR_SIZE;
        
        // Convert error code to status
        switch (sectors[i].error_code) {
            case UFT_D64_DISK_OK:
                s.status = UFT_SECTOR_OK;
                break;
            case UFT_D64_DISK_CHECKSUM_ERR:
            case UFT_D64_DISK_HEADER_CRC:
                s.status = UFT_SECTOR_CRC_ERROR;
                break;
            case UFT_D64_DISK_NO_SYNC:
            case UFT_D64_DISK_HEADER_ERR:
            case UFT_D64_DISK_DATA_ERR:
                s.status = UFT_SECTOR_MISSING;
                break;
            default:
                s.status = UFT_SECTOR_CRC_ERROR;
        }
        
        uft_error_t err = uft_track_add_sector(track, &s);
        free(s.data);  // track makes a copy
        
        if (UFT_FAILED(err)) {
            uft_track_clear(track);
            return err;
        }
    }
    
    track->status = UFT_TRACK_OK;
    return UFT_OK;
}

// ============================================================================
// PLUGIN DEFINITIONS
// ============================================================================

static const uft_format_plugin_t g_scp_plugin_hardened = {
    .name = "SCP (Hardened)",
    .extensions = "scp",
    .probe = scp_probe_hardened,
    .open = scp_open_hardened,
    .close = scp_close_hardened,
    .read_track = NULL,  // Flux format - use dedicated API
    .write_track = NULL,
    .create = NULL,
    .flush = NULL,
    .read_metadata = NULL,
    .write_metadata = NULL,
};

static const uft_format_plugin_t g_d64_plugin_hardened = {
    .name = "D64 (Hardened)",
    .extensions = "d64",
    .probe = d64_probe_hardened,
    .open = d64_open_hardened,
    .close = d64_close_hardened,
    .read_track = d64_read_track_adapter,
    .write_track = NULL,  /* Write-back not supported for hardened D64 */
    .create = NULL,
    .flush = NULL,
    .read_metadata = NULL,
    .write_metadata = NULL,
};

// ============================================================================
// PUBLIC API
// ============================================================================

const uft_format_plugin_t *uft_get_scp_plugin_hardened(void) {
    return &g_scp_plugin_hardened;
}

const uft_format_plugin_t *uft_get_d64_plugin_hardened(void) {
    return &g_d64_plugin_hardened;
}

/**
 * @brief Register all hardened plugins
 * @return Number of plugins registered
 */
int uft_register_hardened_plugins(void) {
    /* Register hardened format plugins with the global registry.
     * Returns number of plugins successfully registered. */
    int registered = 0;
    
    /* Registration via extern function if available, otherwise just return count */
#ifdef UFT_HAVE_PLUGIN_REGISTRY
    if (uft_format_register_plugin(&g_scp_plugin_hardened) == 0) registered++;
    if (uft_format_register_plugin(&g_d64_plugin_hardened) == 0) registered++;
#else
    /* Registry not yet linked; plugins available via getter functions */
    registered = 2;
#endif
    
    return registered;
}
