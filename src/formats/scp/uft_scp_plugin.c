/**
 * @file uft_scp_plugin.c
 * @brief UnifiedFloppyTool - SCP (SuperCard Pro) Format Plugin
 * 
 * SCP ist ein Flux-basiertes Format:
 * - Header mit Disk-Info
 * - Track-Offsets Table
 * - Pro Track: TDH Header + Flux-Daten (multiple Revolutions)
 * 
 * Dieses Plugin bietet:
 * - Lesen von SCP-Dateien
 * - Flux-Extraktion
 * - Dekodierung zu Sektoren (via Decoder-Plugin)
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_decoder_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// SCP Format Structures
// ============================================================================

#define SCP_MAGIC           "SCP"
#define SCP_VERSION         0x19    // Version 2.5
#define SCP_MAX_TRACKS      168     // 84 cylinders × 2 heads
#define SCP_TICK_NS         25      // 25ns per tick (40MHz)

#pragma pack(push, 1)

/**
 * @brief SCP File Header (16 bytes)
 */
typedef struct {
    char        magic[3];       // "SCP"
    uint8_t     version;        // Version (0x19 = 2.5)
    uint8_t     disk_type;      // Disk type
    uint8_t     revolutions;    // Number of revolutions
    uint8_t     start_track;    // First track
    uint8_t     end_track;      // Last track
    uint8_t     flags;          // Flags
    uint8_t     bitcell_width;  // Bitcell encoding (0=16bit, 1=8bit)
    uint8_t     heads;          // 0=both, 1=side0, 2=side1
    uint8_t     resolution;     // 25ns * (resolution + 1)
    uint32_t    checksum;       // Data checksum
} scp_header_t;

/**
 * @brief SCP Track Data Header (4 bytes)
 */
typedef struct {
    char        magic[3];       // "TRK"
    uint8_t     track_num;      // Track number
} scp_track_header_t;

/**
 * @brief SCP Revolution Header (12 bytes)
 */
typedef struct {
    uint32_t    duration;       // Index time in ticks
    uint32_t    length;         // Data length in bytes
    uint32_t    offset;         // Offset from track header
} scp_revolution_t;

#pragma pack(pop)

// SCP Disk Types
#define SCP_TYPE_C64        0x00
#define SCP_TYPE_AMIGA      0x04
#define SCP_TYPE_ATARI_ST   0x08
#define SCP_TYPE_ATARI_800  0x0C
#define SCP_TYPE_APPLE_II   0x10
#define SCP_TYPE_APPLE_35   0x14
#define SCP_TYPE_PC_DD      0x20
#define SCP_TYPE_PC_HD      0x30

// SCP Flags
#define SCP_FLAG_INDEX      0x01    // Index mark stored
#define SCP_FLAG_96TPI      0x02    // 96 TPI drive
#define SCP_FLAG_360RPM     0x04    // 360 RPM
#define SCP_FLAG_NORMALIZED 0x08    // Flux normalized
#define SCP_FLAG_RW         0x10    // Read/Write capable
#define SCP_FLAG_FOOTER     0x20    // Has footer

// ============================================================================
// Plugin Data
// ============================================================================

typedef struct {
    FILE*           file;
    scp_header_t    header;
    uint32_t*       track_offsets;      // Offset für jeden Track
    size_t          file_size;
} scp_data_t;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Liest Flux-Daten für eine Revolution
 */
static uft_error_t scp_read_revolution_flux(scp_data_t* scp, uint32_t offset,
                                             uint32_t length, uint32_t** flux,
                                             size_t* flux_count) {
    if (fseek(scp->file, offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Flux-Daten sind 16-bit Big-Endian
    size_t num_words = length / 2;
    uint16_t* raw = malloc(length);
    if (!raw) return UFT_ERROR_NO_MEMORY;
    
    if (fread(raw, 1, length, scp->file) != length) {
        free(raw);
        return UFT_ERROR_FILE_READ;
    }
    
    // Konvertiere zu 32-bit Ticks (in Nanosekunden)
    uint32_t* result = malloc(num_words * sizeof(uint32_t));
    if (!result) {
        free(raw);
        return UFT_ERROR_NO_MEMORY;
    }
    
    size_t out_count = 0;
    uint32_t overflow = 0;
    
    for (size_t i = 0; i < num_words; i++) {
        // Big-Endian zu Little-Endian
        uint16_t val = (raw[i] >> 8) | (raw[i] << 8);
        
        if (val == 0) {
            // Overflow: 65536 Ticks addieren
            overflow += 65536;
        } else {
            // Normale Transition
            uint32_t ticks = overflow + val;
            overflow = 0;
            
            // Ticks zu Nanosekunden (25ns pro Tick)
            uint32_t ns = ticks * SCP_TICK_NS * (scp->header.resolution + 1);
            result[out_count++] = ns;
        }
    }
    
    free(raw);
    
    *flux = result;
    *flux_count = out_count;
    
    return UFT_OK;
}

/**
 * @brief Ermittelt Geometrie aus Disk-Type
 */
static void scp_get_geometry(uint8_t disk_type, uint8_t start, uint8_t end,
                              uft_geometry_t* geo) {
    geo->sector_size = 512;
    geo->double_step = false;
    
    int num_tracks = end - start + 1;
    geo->cylinders = (num_tracks + 1) / 2;
    geo->heads = 2;
    
    switch (disk_type & 0xF0) {
        case SCP_TYPE_AMIGA:
            geo->sectors = 11;
            break;
        case SCP_TYPE_PC_DD:
            geo->sectors = 9;
            break;
        case SCP_TYPE_PC_HD:
            geo->sectors = 18;
            break;
        case SCP_TYPE_ATARI_ST:
            geo->sectors = 9;
            break;
        default:
            geo->sectors = 9;
            break;
    }
    
    geo->total_sectors = geo->cylinders * geo->heads * geo->sectors;
}

// ============================================================================
// Probe
// ============================================================================

bool scp_probe(const uint8_t* data, size_t size, size_t file_size,
                      int* confidence) {
    *confidence = 0;
    
    if (size < sizeof(scp_header_t)) {
        return false;
    }
    
    // Magic prüfen
    if (data[0] != 'S' || data[1] != 'C' || data[2] != 'P') {
        return false;
    }
    
    *confidence = 95;
    
    // Version prüfen (0x00-0x2F sind gültig)
    if (data[3] > 0x2F) {
        *confidence = 50;  // Unbekannte Version
    }
    
    return true;
}

// ============================================================================
// Open
// ============================================================================

static uft_error_t scp_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Dateigröße
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    // Header lesen
    scp_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    // Magic prüfen
    if (memcmp(header.magic, SCP_MAGIC, 3) != 0) {
        fclose(f);
        return UFT_ERROR_BAD_MAGIC;
    }
    
    // Track-Offsets lesen
    int num_tracks = header.end_track - header.start_track + 1;
    if (num_tracks <= 0 || num_tracks > SCP_MAX_TRACKS) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    uint32_t* track_offsets = malloc(SCP_MAX_TRACKS * sizeof(uint32_t));
    if (!track_offsets) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    memset(track_offsets, 0, SCP_MAX_TRACKS * sizeof(uint32_t));
    
    // Track-Offsets stehen nach dem Header
    if (fread(&track_offsets[header.start_track], sizeof(uint32_t), 
              num_tracks, f) != (size_t)num_tracks) {
        free(track_offsets);
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    // Plugin-Daten
    scp_data_t* pdata = calloc(1, sizeof(scp_data_t));
    if (!pdata) {
        free(track_offsets);
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    pdata->file = f;
    pdata->header = header;
    pdata->track_offsets = track_offsets;
    pdata->file_size = file_size;
    
    disk->plugin_data = pdata;
    
    // Geometrie ermitteln
    scp_get_geometry(header.disk_type, header.start_track, header.end_track,
                     &disk->geometry);
    
    return UFT_OK;
}

// ============================================================================
// Close
// ============================================================================

static void scp_close(uft_disk_t* disk) {
    if (!disk || !disk->plugin_data) return;
    
    scp_data_t* pdata = disk->plugin_data;
    
    if (pdata->file) {
        fclose(pdata->file);
    }
    free(pdata->track_offsets);
    free(pdata);
    
    disk->plugin_data = NULL;
}

// ============================================================================
// Read Track
// ============================================================================

static uft_error_t scp_read_track(uft_disk_t* disk, int cylinder, int head,
                                   uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    
    scp_data_t* pdata = disk->plugin_data;
    if (!pdata) return UFT_ERROR_FILE_READ;
    
    uft_track_init(track, cylinder, head);
    
    // SCP Track-Nummer (interleaved: 0=C0H0, 1=C0H1, 2=C1H0, ...)
    int track_num = cylinder * 2 + head;
    
    if (track_num < pdata->header.start_track || 
        track_num > pdata->header.end_track) {
        return UFT_ERROR_TRACK_NOT_FOUND;
    }
    
    uint32_t track_offset = pdata->track_offsets[track_num];
    if (track_offset == 0) {
        track->status = UFT_TRACK_UNFORMATTED;
        return UFT_OK;
    }
    
    // Track Header lesen
    if (fseek(pdata->file, track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    scp_track_header_t th;
    if (fread(&th, sizeof(th), 1, pdata->file) != 1) {
        return UFT_ERROR_FILE_READ;
    }
    
    if (memcmp(th.magic, "TRK", 3) != 0) {
        return UFT_ERROR_BAD_MAGIC;
    }
    
    // Revolution Headers lesen
    scp_revolution_t revs[16];
    int num_revs = pdata->header.revolutions;
    if (num_revs > 16) num_revs = 16;
    
    if (fread(revs, sizeof(scp_revolution_t), num_revs, pdata->file) != (size_t)num_revs) {
        return UFT_ERROR_FILE_READ;
    }
    
    // Flux der ersten Revolution lesen
    // (Für bessere Ergebnisse könnte man alle Revolutions kombinieren)
    uint32_t flux_offset = track_offset + revs[0].offset;
    uint32_t flux_length = revs[0].length;
    
    uint32_t* flux;
    size_t flux_count;
    
    uft_error_t err = scp_read_revolution_flux(pdata, flux_offset, flux_length,
                                                &flux, &flux_count);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Flux-Daten zum Track hinzufügen
    err = uft_track_set_flux(track, flux, flux_count, 1);  // 1ns Einheit
    free(flux);
    
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Metriken setzen
    track->metrics.flux_count = flux_count;
    track->metrics.index_time_ns = revs[0].duration * SCP_TICK_NS * 
                                   (pdata->header.resolution + 1);
    track->metrics.rpm = 60.0e9 / track->metrics.index_time_ns;
    
    // Flux dekodieren (wenn Decoder verfügbar)
    const uft_decoder_plugin_t* decoder = uft_find_decoder_plugin_for_flux(
        track->flux, track->flux_count);
    
    if (decoder && decoder->decode) {
        uft_sector_t sectors[64];
        size_t sector_count;
        uft_decode_stats_t stats;
        uft_decode_options_t opts = uft_default_decode_options();
        
        err = decoder->decode(track->flux, track->flux_count, &opts,
                              sectors, 64, &sector_count, &stats);
        
        if (UFT_SUCCEEDED(err)) {
            for (size_t i = 0; i < sector_count; i++) {
                sectors[i].id.cylinder = cylinder;
                sectors[i].id.head = head;
                uft_track_add_sector(track, &sectors[i]);
                free(sectors[i].data);
            }
            
            track->metrics.data_rate = stats.data_rate_bps;
        }
    }
    
    track->status = UFT_TRACK_OK;
    
    return UFT_OK;
}

// ============================================================================
// Plugin Definition
// ============================================================================

const uft_format_plugin_t uft_format_plugin_scp = {
    .name = "SCP",
    .description = "SuperCard Pro flux image",
    .extensions = "scp",
    .version = 0x00010000,
    .format = UFT_FORMAT_SCP,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | 
                    UFT_FORMAT_CAP_TIMING | UFT_FORMAT_CAP_MULTI_REV,
    
    .probe = scp_probe,
    .open = scp_open,
    .close = scp_close,
    .create = NULL,  // TODO: Implement
    .flush = NULL,
    .read_track = scp_read_track,
    .write_track = NULL,  // TODO: Implement
    .detect_geometry = NULL,
    .read_metadata = NULL,
    .write_metadata = NULL,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL
};
