/**
 * @file uft_flux_loaders.c
 * @brief SCP and KryoFlux Flux Stream Loaders for RIDE module
 * 
 * Implements loading of:
 * - SuperCard Pro (.scp) flux images
 * - KryoFlux raw stream files (.raw)
 * 
 * @copyright Original algorithms from RIDE (c) Tomas Nestorovic
 * @copyright KryoFlux support based on Simon Owen's SamDisk (MIT)
 * @license MIT
 */

#include "uft/ride/uft_flux_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * SCP FILE FORMAT STRUCTURES
 *============================================================================*/

#define SCP_MAGIC           "SCP"
#define SCP_HEADER_SIZE     16
#define SCP_TRACK_HEADER    4

#include "uft/uft_packed.h"

/* SCP Header (offset 0x00) */
UFT_PACK_BEGIN
typedef struct {
    char     magic[3];          /* "SCP" */
    uint8_t  version;           /* Version (currently 0x19) */
    uint8_t  disk_type;         /* Disk type */
    uint8_t  revolutions;       /* Number of revolutions */
    uint8_t  start_track;       /* Starting track */
    uint8_t  end_track;         /* Ending track */
    uint8_t  flags;             /* Flags */
    uint8_t  bitcell_width;     /* Bit cell width (0=16bit) */
    uint8_t  heads;             /* 0=both, 1=side0, 2=side1 */
    uint8_t  resolution;        /* 25ns resolution multiplier */
    uint32_t checksum;          /* File checksum */
} scp_header_t;
UFT_PACK_END

/* Track Data Header */
UFT_PACK_BEGIN
typedef struct {
    char     trk_magic[3];      /* "TRK" */
    uint8_t  track_num;         /* Track number */
} scp_track_header_t;
UFT_PACK_END

/* Revolution Header */
UFT_PACK_BEGIN
typedef struct {
    uint32_t index_time;        /* Index to index time (25ns units) */
    uint32_t flux_count;        /* Number of flux transitions */
    uint32_t data_offset;       /* Offset to flux data (from track header) */
} scp_rev_header_t;
UFT_PACK_END

/* SCP Flags */
#define SCP_FLAG_INDEX      0x01    /* Index signal stored */
#define SCP_FLAG_96TPI      0x02    /* 96 TPI (otherwise 48) */
#define SCP_FLAG_360RPM     0x04    /* 360 RPM (otherwise 300) */
#define SCP_FLAG_NORMALIZE  0x08    /* Flux normalized */
#define SCP_FLAG_READONLY   0x10    /* Read-only */
#define SCP_FLAG_FOOTER     0x20    /* Footer present */
#define SCP_FLAG_EXTENDED   0x40    /* Extended mode */

/*============================================================================
 * KRYOFLUX STREAM FORMAT
 *============================================================================*/

/* KryoFlux OOB (Out-Of-Band) types */
#define KF_OOB_INVALID      0x00
#define KF_OOB_STREAM_INFO  0x01
#define KF_OOB_INDEX        0x02
#define KF_OOB_STREAM_END   0x03
#define KF_OOB_INFO         0x04
#define KF_OOB_EOF          0x0D

/* KryoFlux sample clock: 18.432 MHz / 2 * 73 = 24.027428 MHz */
#define KF_SAMPLE_CLOCK     24027428.0
#define KF_NS_PER_TICK      (1000000000.0 / KF_SAMPLE_CLOCK)  /* ~41.619 ns */

/* KryoFlux overflow value */
#define KF_OVERFLOW         0x0800
#define KF_OVERFLOW_ADD     0x10000

/*============================================================================
 * SCP LOADER IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Load SCP file into flux buffer
 */
uft_flux_buffer_t *uft_flux_load_scp(const char *path, uint8_t cylinder, uint8_t head) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    
    /* Read and validate header */
    scp_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return NULL;
    }
    
    if (memcmp(header.magic, SCP_MAGIC, 3) != 0) {
        fclose(f);
        return NULL;
    }
    
    /* Calculate track index */
    int track_idx;
    if (header.heads == 0) {
        /* Both sides: interleaved */
        track_idx = cylinder * 2 + head;
    } else {
        track_idx = cylinder;
    }
    
    /* Check track bounds */
    if (track_idx < header.start_track || track_idx > header.end_track) {
        fclose(f);
        return NULL;
    }
    
    /* Read track offset table */
    uint32_t track_offset;
    int offset_idx = track_idx - header.start_track;
    if (fseek(f, sizeof(scp_header_t) + offset_idx * sizeof(uint32_t), SEEK_SET) != 0) return NULL;
    if (fread(&track_offset, 1, sizeof(track_offset), f) != sizeof(track_offset)) {
        fclose(f);
        return NULL;
    }
    
    if (track_offset == 0) {
        /* Track not present */
        fclose(f);
        return NULL;
    }
    
    /* Seek to track data */
    if (fseek(f, track_offset, SEEK_SET) != 0) return NULL;
    
    /* Read track header */
    scp_track_header_t trk_header;
    if (fread(&trk_header, 1, sizeof(trk_header), f) != sizeof(trk_header)) {
        fclose(f);
        return NULL;
    }
    
    if (memcmp(trk_header.trk_magic, "TRK", 3) != 0) {
        fclose(f);
        return NULL;
    }
    
    /* Calculate total flux transitions across all revolutions */
    size_t total_flux = 0;
    scp_rev_header_t rev_headers[16];
    int num_revs = header.revolutions > 16 ? 16 : header.revolutions;
    
    for (int rev = 0; rev < num_revs; rev++) {
        if (fread(&rev_headers[rev], 1, sizeof(scp_rev_header_t), f) != sizeof(scp_rev_header_t)) {
            fclose(f);
            return NULL;
        }
        total_flux += rev_headers[rev].flux_count;
    }
    
    /* Create flux buffer */
    uft_flux_buffer_t *flux = uft_flux_create(total_flux + 1024);
    if (!flux) {
        fclose(f);
        return NULL;
    }
    
    flux->detected_enc = UFT_ENC_MFM;  /* Default, will be auto-detected */
    flux->detected_den = UFT_DENSITY_DD;
    
    /* Resolution multiplier (25ns base) */
    double ns_per_tick = 25.0 * (header.resolution + 1);
    
    /* Read flux data for each revolution */
    for (int rev = 0; rev < num_revs; rev++) {
        /* Seek to flux data */
        fseek(f, track_offset + rev_headers[rev].data_offset, SEEK_SET);
        
        /* Record index time */
        if (flux->index_count < UFT_REVOLUTION_MAX) {
            uft_logtime_t total_time = 0;
            for (size_t i = 0; i < flux->count; i++) {
                total_time += flux->times[i];
            }
            flux->index_times[flux->index_count++] = total_time;
        }
        
        /* Read flux transitions */
        uint32_t flux_count = rev_headers[rev].flux_count;
        uint16_t *flux_data = malloc(flux_count * sizeof(uint16_t));
        if (!flux_data) {
            uft_flux_free(flux);
            fclose(f);
            return NULL;
        }
        
        if (fread(flux_data, sizeof(uint16_t), flux_count, f) != flux_count) {
            free(flux_data);
            uft_flux_free(flux);
            fclose(f);
            return NULL;
        }
        
        /* Convert to nanoseconds and add to buffer */
        for (uint32_t i = 0; i < flux_count && flux->count < flux->capacity; i++) {
            /* Handle 16-bit overflow extension */
            uint32_t ticks = flux_data[i];
            
            /* Skip overflow markers, accumulate */
            while (ticks == 0 && i + 1 < flux_count) {
                i++;
                ticks = flux_data[i] + 0x10000;
            }
            
            /* Convert to nanoseconds */
            uft_logtime_t time_ns = (uft_logtime_t)(ticks * ns_per_tick);
            uft_flux_add_transition(flux, time_ns);
        }
        
        free(flux_data);
    }
    
    /* Final index marker */
    if (flux->index_count < UFT_REVOLUTION_MAX + 1) {
        uft_logtime_t total_time = 0;
        for (size_t i = 0; i < flux->count; i++) {
            total_time += flux->times[i];
        }
        flux->index_times[flux->index_count] = total_time;
    }
    
    fclose(f);
    
    /* Auto-detect encoding based on timing */
    uft_flux_detect_encoding(flux);
    
    return flux;
}

/**
 * @brief Get SCP file information
 */
int uft_scp_get_info(const char *path, uft_scp_info_t *info) {
    if (!path || !info) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    scp_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    
    if (memcmp(header.magic, SCP_MAGIC, 3) != 0) {
        return -1;
    }
    
    memset(info, 0, sizeof(uft_scp_info_t));
    info->version = header.version;
    info->disk_type = header.disk_type;
    info->revolutions = header.revolutions;
    info->start_track = header.start_track;
    info->end_track = header.end_track;
    info->flags = header.flags;
    info->heads = header.heads;
    info->resolution_ns = 25 * (header.resolution + 1);
    
    /* Decode disk type string */
    switch (header.disk_type & 0xF0) {
        case 0x00: strncpy(info->disk_type_str, "Commodore", 31); break;
        case 0x10: strncpy(info->disk_type_str, "Atari ST", 31); break;
        case 0x20: strncpy(info->disk_type_str, "Apple", 31); break;
        case 0x30: strncpy(info->disk_type_str, "PC-88", 31); break;
        case 0x40: strncpy(info->disk_type_str, "IBM PC", 31); break;
        case 0x50: strncpy(info->disk_type_str, "Tandy", 31); break;
        case 0x60: strncpy(info->disk_type_str, "TI-99", 31); break;
        case 0x70: strncpy(info->disk_type_str, "Roland", 31); break;
        case 0x80: strncpy(info->disk_type_str, "Amstrad CPC", 31); break;
        default:   strncpy(info->disk_type_str, "Unknown", 31); break;
    }
    
    return 0;
}

/*============================================================================
 * KRYOFLUX LOADER IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Parse KryoFlux OOB block
 * @return Number of bytes consumed, or -1 on error
 */
static int kf_parse_oob(const uint8_t *data, size_t len, 
                        uint32_t *stream_pos, uint32_t *index_time) {
    if (len < 2) return -1;
    
    uint8_t type = data[0];
    uint8_t size = data[1];
    
    if (len < (size_t)(2 + size)) return -1;
    
    switch (type) {
        case KF_OOB_STREAM_INFO:
            if (size >= 4 && stream_pos) {
                *stream_pos = data[2] | (data[3] << 8) | 
                              (data[4] << 16) | (data[5] << 24);
            }
            break;
            
        case KF_OOB_INDEX:
            if (size >= 12 && index_time) {
                /* Index time is at offset 8 (4 bytes) */
                *index_time = data[10] | (data[11] << 8) | 
                              (data[12] << 16) | (data[13] << 24);
            }
            break;
            
        case KF_OOB_STREAM_END:
            /* End of stream */
            break;
            
        case KF_OOB_INFO:
            /* Info string - ignore */
            break;
            
        case KF_OOB_EOF:
            return -2;  /* Signal EOF */
            
        default:
            break;
    }
    
    return 2 + size;
}

/**
 * @brief Load single KryoFlux raw stream file (internal)
 */
uft_flux_buffer_t *uft_flux_load_kryoflux_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 50 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    
    /* Read entire file */
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    /* Estimate flux count (typical: 50000-100000 per track) */
    uft_flux_buffer_t *flux = uft_flux_create(file_size);  /* Overestimate */
    if (!flux) {
        free(data);
        return NULL;
    }
    
    /* Parse stream */
    size_t pos = 0;
    uint32_t overflow = 0;
    uint32_t stream_pos = 0;
    
    while (pos < (size_t)file_size) {
        uint8_t byte = data[pos];
        
        if (byte == 0x0D) {
            /* OOB marker */
            pos++;
            if (pos >= (size_t)file_size) break;
            
            uint32_t index_time = 0;
            int oob_len = kf_parse_oob(data + pos, file_size - pos, 
                                       &stream_pos, &index_time);
            
            if (oob_len == -2) {
                /* EOF */
                break;
            } else if (oob_len > 0) {
                pos += oob_len;
                
                /* Record index if valid */
                if (index_time > 0 && flux->index_count < UFT_REVOLUTION_MAX) {
                    uft_logtime_t time_ns = (uft_logtime_t)(index_time * KF_NS_PER_TICK);
                    flux->index_times[flux->index_count++] = time_ns;
                }
            } else {
                pos++;
            }
        } else if (byte < 0x08) {
            /* Flux2 (2-byte flux value) */
            if (pos + 1 >= (size_t)file_size) break;
            
            uint16_t ticks = (byte << 8) | data[pos + 1];
            ticks += overflow;
            overflow = 0;
            
            uft_logtime_t time_ns = (uft_logtime_t)(ticks * KF_NS_PER_TICK);
            uft_flux_add_transition(flux, time_ns);
            
            pos += 2;
        } else if (byte == 0x08) {
            /* Nop1 */
            pos++;
        } else if (byte == 0x09) {
            /* Nop2 */
            pos += 2;
        } else if (byte == 0x0A) {
            /* Nop3 */
            pos += 3;
        } else if (byte == 0x0B) {
            /* Overflow16 */
            overflow += KF_OVERFLOW_ADD;
            pos++;
        } else if (byte == 0x0C) {
            /* Flux3 (3-byte flux value) */
            if (pos + 2 >= (size_t)file_size) break;
            
            uint32_t ticks = data[pos + 1] | (data[pos + 2] << 8);
            ticks += overflow;
            overflow = 0;
            
            uft_logtime_t time_ns = (uft_logtime_t)(ticks * KF_NS_PER_TICK);
            uft_flux_add_transition(flux, time_ns);
            
            pos += 3;
        } else {
            /* Flux1 (1-byte flux value, 0x0E-0xFF) */
            uint16_t ticks = byte + overflow;
            overflow = 0;
            
            uft_logtime_t time_ns = (uft_logtime_t)(ticks * KF_NS_PER_TICK);
            uft_flux_add_transition(flux, time_ns);
            
            pos++;
        }
    }
    
    free(data);
    
    /* Auto-detect encoding */
    uft_flux_detect_encoding(flux);
    
    return flux;
}

/**
 * @brief Load KryoFlux track from directory (matches existing API)
 */
uft_flux_buffer_t *uft_flux_load_kryoflux(const char *base_path,
                                           uint8_t cylinder, uint8_t head) {
    char filename[512];
    uft_kryoflux_build_filename(filename, sizeof(filename), base_path, cylinder, head);
    return uft_flux_load_kryoflux_file(filename);
}

/**
 * @brief Build KryoFlux track filename
 */
void uft_kryoflux_build_filename(char *buf, size_t buf_size,
                                  const char *base_path,
                                  int track, int side) {
    if (!buf || buf_size == 0) return;
    
    /* KryoFlux naming: track00.0.raw, track00.1.raw, etc. */
    snprintf(buf, buf_size, "%s/track%02d.%d.raw", base_path, track, side);
}

/*============================================================================
 * ENCODING AUTO-DETECTION
 *============================================================================*/

/**
 * @brief Auto-detect flux encoding from timing histogram
 */
void uft_flux_detect_encoding(uft_flux_buffer_t *flux) {
    if (!flux || flux->count < 100) return;
    
    /* Build timing histogram */
    uint32_t hist[20] = {0};  /* 0-20µs in 1µs bins */
    
    for (size_t i = 0; i < flux->count && i < 10000; i++) {
        int bin = flux->times[i] / 1000;  /* ns to µs */
        if (bin < 20) hist[bin]++;
    }
    
    /* Find peaks */
    int peak1 = 0, peak1_count = 0;
    int peak2 = 0, peak2_count = 0;
    int peak3 = 0, peak3_count = 0;
    
    for (int i = 1; i < 19; i++) {
        if (hist[i] > hist[i-1] && hist[i] > hist[i+1]) {
            if (hist[i] > peak1_count) {
                peak3 = peak2; peak3_count = peak2_count;
                peak2 = peak1; peak2_count = peak1_count;
                peak1 = i; peak1_count = hist[i];
            } else if (hist[i] > peak2_count) {
                peak3 = peak2; peak3_count = peak2_count;
                peak2 = i; peak2_count = hist[i];
            } else if (hist[i] > peak3_count) {
                peak3 = i; peak3_count = hist[i];
            }
        }
    }
    
    /* Sort peaks */
    if (peak1 > peak2) { int t = peak1; peak1 = peak2; peak2 = t; }
    if (peak2 > peak3) { int t = peak2; peak2 = peak3; peak3 = t; }
    if (peak1 > peak2) { int t = peak1; peak1 = peak2; peak2 = t; }
    
    /* Determine encoding and density */
    if (peak1 >= 3 && peak1 <= 5 && peak2 >= 5 && peak2 <= 7) {
        /* MFM DD: peaks at ~4µs, ~6µs, ~8µs */
        flux->detected_enc = UFT_ENC_MFM;
        flux->detected_den = UFT_DENSITY_DD;
    } else if (peak1 >= 1 && peak1 <= 3 && peak2 >= 2 && peak2 <= 4) {
        /* MFM HD: peaks at ~2µs, ~3µs, ~4µs */
        flux->detected_enc = UFT_ENC_MFM;
        flux->detected_den = UFT_DENSITY_HD;
    } else if (peak1 >= 7 && peak1 <= 9 && peak2 >= 14 && peak2 <= 18) {
        /* FM SD: peaks at ~8µs, ~16µs */
        flux->detected_enc = UFT_ENC_FM;
        flux->detected_den = UFT_DENSITY_SD;
    } else if (peak1 >= 2 && peak1 <= 4) {
        /* GCR: variable, typically ~3-4µs base */
        flux->detected_enc = UFT_ENC_GCR_APPLE;
        flux->detected_den = UFT_DENSITY_DD;
    }
}

/*============================================================================
 * FLUX STREAM UTILITIES
 *============================================================================*/

/**
 * @brief Convert flux buffer to bitstream
 */
int uft_flux_to_bitstream(const uft_flux_buffer_t *flux,
                           uint8_t *bits, size_t bits_capacity,
                           size_t *bits_written) {
    if (!flux || !bits || !bits_written) return -1;
    
    *bits_written = 0;
    memset(bits, 0, bits_capacity);
    
    /* Determine bit cell time based on encoding/density */
    uft_logtime_t cell_time;
    switch (flux->detected_den) {
        case UFT_DENSITY_SD: cell_time = UFT_FM_CELL_SD_NS; break;
        case UFT_DENSITY_HD: cell_time = UFT_MFM_CELL_HD_NS; break;
        case UFT_DENSITY_ED: cell_time = UFT_MFM_CELL_ED_NS; break;
        default:             cell_time = UFT_MFM_CELL_DD_NS; break;
    }
    
    /* Convert flux transitions to bits */
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < flux->count && bit_pos < bits_capacity * 8; i++) {
        /* Calculate number of bit cells */
        int cells = (flux->times[i] + cell_time / 2) / cell_time;
        if (cells < 1) cells = 1;
        if (cells > 8) cells = 8;  /* Limit to prevent runaway */
        
        /* Add zeros for clock bits, then a 1 for the flux transition */
        for (int c = 0; c < cells - 1 && bit_pos < bits_capacity * 8; c++) {
            bit_pos++;  /* Zero bit (implicit) */
        }
        
        /* Set the flux transition bit */
        if (bit_pos < bits_capacity * 8) {
            bits[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
            bit_pos++;
        }
    }
    
    *bits_written = (bit_pos + 7) / 8;
    return 0;
}

/**
 * @brief Get revolution data from flux buffer
 */
int uft_flux_get_revolution(const uft_flux_buffer_t *flux, int rev,
                             size_t *start_idx, size_t *end_idx,
                             uft_logtime_t *duration_ns) {
    if (!flux || rev < 0 || rev >= (int)flux->index_count) return -1;
    
    /* Find flux transitions for this revolution */
    uft_logtime_t target_start = (rev > 0) ? flux->index_times[rev - 1] : 0;
    uft_logtime_t target_end = flux->index_times[rev];
    
    uft_logtime_t running_time = 0;
    size_t start = 0, end = 0;
    bool found_start = false;
    
    for (size_t i = 0; i < flux->count; i++) {
        if (!found_start && running_time >= target_start) {
            start = i;
            found_start = true;
        }
        
        running_time += flux->times[i];
        
        if (running_time >= target_end) {
            end = i;
            break;
        }
    }
    
    if (start_idx) *start_idx = start;
    if (end_idx) *end_idx = end;
    if (duration_ns) *duration_ns = target_end - target_start;
    
    return 0;
}
