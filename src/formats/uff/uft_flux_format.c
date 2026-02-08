// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_flux_format.c
 * @brief UFT Flux Format (UFF) Implementation
 * @version 1.0.0
 * @date 2025-01-02
 *
 * "Kein Bit geht verloren" - UFT Preservation Philosophy
 */

#include "uft/uft_safe_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* CRC32 Tabelle */
static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void) {
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

static uint32_t calc_crc32(const void* data, size_t len) {
    init_crc32_table();
    
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    while (len--) {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    
    return ~crc;
}

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define UFF_MAGIC           "UFF\x00"
#define UFF_VERSION_MAJOR   1
#define UFF_VERSION_MINOR   0
#define UFF_VERSION_PATCH   0

#define UFF_CHUNK_INFO      0x4F464E49
#define UFF_CHUNK_TRCK      0x4B435254
#define UFF_CHUNK_FLUX      0x58554C46
#define UFF_CHUNK_BITS      0x53544942
#define UFF_CHUNK_SECT      0x54434553
#define UFF_CHUNK_WEAK      0x4B414557
#define UFF_CHUNK_PROT      0x544F5250
#define UFF_CHUNK_META      0x4154454D
#define UFF_CHUNK_HASH      0x48534148
#define UFF_CHUNK_AUDT      0x54445541
#define UFF_CHUNK_CONF      0x464E4F43
#define UFF_CHUNK_CAPT      0x54504143
#define UFF_CHUNK_HARD      0x44524148
#define UFF_CHUNK_INDX      0x58444E49

#define UFF_MAX_TRACKS      168
#define UFF_MAX_SIDES       2
#define UFF_MAX_REVOLUTIONS 5
#define UFF_MAX_AUDIT       1000
#define UFF_MAX_WEAK        1000

/*============================================================================
 * STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

typedef struct {
    uint8_t  magic[4];
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t version_patch;
    uint16_t flags;
    uint32_t header_size;
    uint32_t total_chunks;
    uint64_t total_size;
    uint64_t flux_data_size;
    uint32_t crc32;
    uint8_t  reserved[24];
} uff_header_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t crc32;
    uint32_t flags;
} uff_chunk_header_t;

typedef struct {
    uint8_t  disk_type;
    uint8_t  encoding;
    uint8_t  tracks;
    uint8_t  sides;
    uint16_t rpm;
    uint16_t bitcell_ns;
    uint32_t data_rate;
    uint8_t  write_precomp;
    uint8_t  track_density;
    uint8_t  sectors_per_track;
    uint8_t  bytes_per_sector;
    char     title[64];
    char     platform[32];
} uff_info_t;

typedef struct {
    uint8_t  track_num;
    uint8_t  side;
    uint8_t  encoding;
    uint8_t  revolutions;
    uint32_t bit_count;
    uint32_t index_offset;
    uint32_t flux_offset;
    uint32_t flux_size;
    uint32_t bits_offset;
    uint32_t bits_size;
    uint16_t rpm_measured;
    uint16_t flags;
    float    confidence;
} uff_track_header_t;

typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  revolution;
    uint8_t  resolution;
    uint32_t sample_count;
    uint32_t index_position;
    uint32_t total_time_ns;
} uff_flux_header_t;

typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint16_t count;
} uff_weak_header_t;

typedef struct {
    uint32_t bit_offset;
    uint16_t bit_count;
    uint8_t  variance;
    uint8_t  flags;
} uff_weak_region_t;

typedef struct {
    uint8_t  protection_type;
    uint8_t  confidence;
    uint16_t affected_tracks;
    char     name[32];
    char     details[128];
} uff_protection_info_t;

typedef struct {
    uint8_t  hardware;
    uint8_t  capture_quality;
    uint16_t flags;
    uint32_t timestamp;
    char     hardware_name[32];
    char     software_name[32];
    char     firmware_ver[16];
    char     serial[32];
    char     operator_name[64];
} uff_capture_t;

typedef struct {
    uint32_t timestamp;
    uint8_t  action;
    uint8_t  track;
    uint8_t  side;
    uint8_t  severity;
    char     message[120];
} uff_audit_entry_t;

typedef struct {
    uint8_t  algorithm;
    uint8_t  scope;
    uint8_t  track;
    uint8_t  side;
    uint32_t offset;
    uint32_t length;
    uint8_t  hash[64];
} uff_hash_entry_t;

typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  sector;
    uint8_t  method;
    float    score;
    float    pll_quality;
    float    sync_quality;
    float    crc_rate;
} uff_confidence_entry_t;

#pragma pack(pop)

/*============================================================================
 * TRACK DATA STRUCTURE
 *============================================================================*/

typedef struct {
    bool valid;
    
    /* Flux data per revolution */
    uint32_t* flux_samples[UFF_MAX_REVOLUTIONS];
    uint32_t  flux_count[UFF_MAX_REVOLUTIONS];
    uint32_t  index_pos[UFF_MAX_REVOLUTIONS];
    uint8_t   revolutions;
    
    /* Decoded bits */
    uint8_t*  bits;
    uint32_t  bit_count;
    
    /* Weak regions */
    uff_weak_region_t weak_regions[UFF_MAX_WEAK];
    uint16_t          weak_count;
    
    /* Track header info */
    uff_track_header_t header;
    
} uff_track_t;

/*============================================================================
 * FILE HANDLE
 *============================================================================*/

struct uff_file_s {
    FILE* fp;
    char* path;
    bool  write_mode;
    
    /* Header */
    uff_header_t header;
    
    /* Info */
    uff_info_t info;
    bool       info_valid;
    
    /* Tracks */
    uff_track_t tracks[UFF_MAX_TRACKS][UFF_MAX_SIDES];
    
    /* Capture info */
    uff_capture_t capture;
    bool          capture_valid;
    
    /* Protection */
    uff_protection_info_t protection[16];
    uint8_t               protection_count;
    
    /* Audit trail */
    uff_audit_entry_t audit[UFF_MAX_AUDIT];
    uint16_t          audit_count;
    
    /* Hashes */
    uff_hash_entry_t hashes[256];
    uint16_t         hash_count;
    
    /* Confidence */
    uff_confidence_entry_t confidence[UFF_MAX_TRACKS * UFF_MAX_SIDES];
    uint16_t               confidence_count;
    
    /* Statistics */
    uint32_t total_chunks;
    uint64_t flux_data_size;
};

typedef struct uff_file_s uff_file_t;

/*============================================================================
 * CREATE / OPEN / CLOSE
 *============================================================================*/

/**
 * @brief Create new UFF file
 */
uff_file_t* uff_create(const char* path) {
    uff_file_t* uff = calloc(1, sizeof(uff_file_t));
    if (!uff) return NULL;
    
    uff->fp = fopen(path, "wb");
    if (!uff->fp) {
        free(uff);
        return NULL;
    }
    
    uff->path = strdup(path);
    uff->write_mode = true;
    
    /* Initialize header */
    memcpy(uff->header.magic, UFF_MAGIC, 4);
    uff->header.version_major = UFF_VERSION_MAJOR;
    uff->header.version_minor = UFF_VERSION_MINOR;
    uff->header.version_patch = UFF_VERSION_PATCH;
    uff->header.header_size = sizeof(uff_header_t);
    
    /* Write placeholder header */
    if (fwrite(&uff->header, sizeof(uff_header_t), 1, uff->fp) != 1) { /* write error */ }
    
    /* Add creation audit entry */
    uff_audit_entry_t* entry = &uff->audit[uff->audit_count++];
    entry->timestamp = (uint32_t)time(NULL);
    entry->action = 1; /* CAPTURE */
    entry->severity = 1; /* INFO */
    snprintf(entry->message, sizeof(entry->message),
             "UFF file created by UFT v5.3.4-GOD");
    
    return uff;
}

/**
 * @brief Open existing UFF file
 */
uff_file_t* uff_open(const char* path) {
    uff_file_t* uff = calloc(1, sizeof(uff_file_t));
    if (!uff) return NULL;
    
    uff->fp = fopen(path, "rb");
    if (!uff->fp) {
        free(uff);
        return NULL;
    }
    
    uff->path = strdup(path);
    uff->write_mode = false;
    
    /* Read header */
    if (fread(&uff->header, sizeof(uff_header_t), 1, uff->fp) != 1) {
        fclose(uff->fp);
        free(uff->path);
        free(uff);
        return NULL;
    }
    
    /* Validate magic */
    if (memcmp(uff->header.magic, UFF_MAGIC, 4) != 0) {
        fclose(uff->fp);
        free(uff->path);
        free(uff);
        return NULL;
    }
    
    /* Read chunks */
    while (!feof(uff->fp)) {
        uff_chunk_header_t chunk;
        if (fread(&chunk, sizeof(chunk), 1, uff->fp) != 1) break;
        
        switch (chunk.type) {
            case UFF_CHUNK_INFO:
                if (fread(&uff->info, sizeof(uff_info_t), 1, uff->fp) != 1) break;
                uff->info_valid = true;
                break;
                
            case UFF_CHUNK_CAPT:
                if (fread(&uff->capture, sizeof(uff_capture_t), 1, uff->fp) != 1) break;
                uff->capture_valid = true;
                break;
                
            case UFF_CHUNK_TRCK: {
                uff_track_header_t th;
                if (fread(&th, sizeof(th), 1, uff->fp) != 1) break;
                if (th.track_num < UFF_MAX_TRACKS && th.side < UFF_MAX_SIDES) {
                    uff->tracks[th.track_num][th.side].header = th;
                    uff->tracks[th.track_num][th.side].valid = true;
                }
                break;
            }
            
            case UFF_CHUNK_FLUX: {
                uff_flux_header_t fh;
                if (fread(&fh, sizeof(fh), 1, uff->fp) != 1) break;
                
                if (fh.track < UFF_MAX_TRACKS && fh.side < UFF_MAX_SIDES &&
                    fh.revolution < UFF_MAX_REVOLUTIONS) {
                    
                    uff_track_t* t = &uff->tracks[fh.track][fh.side];
                    t->flux_count[fh.revolution] = fh.sample_count;
                    t->index_pos[fh.revolution] = fh.index_position;
                    t->flux_samples[fh.revolution] = malloc(fh.sample_count * sizeof(uint32_t));
                    
                    if (t->flux_samples[fh.revolution]) {
                        if (fread(t->flux_samples[fh.revolution], 
                              sizeof(uint32_t), fh.sample_count, uff->fp) != fh.sample_count) { /* I/O error */ }
                    }
                    
                    if (fh.revolution + 1 > t->revolutions) {
                        t->revolutions = fh.revolution + 1;
                    }
                }
                break;
            }
            
            case UFF_CHUNK_WEAK: {
                uff_weak_header_t wh;
                if (fread(&wh, sizeof(wh), 1, uff->fp) != 1) { /* I/O error */ }
                if (wh.track < UFF_MAX_TRACKS && wh.side < UFF_MAX_SIDES) {
                    uff_track_t* t = &uff->tracks[wh.track][wh.side];
                    t->weak_count = wh.count < UFF_MAX_WEAK ? wh.count : UFF_MAX_WEAK;
                    if (fread(t->weak_regions, sizeof(uff_weak_region_t),
                          t->weak_count, uff->fp) != t->weak_count) { /* I/O error */ }
                }
                break;
            }
            
            case UFF_CHUNK_PROT: {
                if (uff->protection_count < 16) {
                    if (fread(&uff->protection[uff->protection_count++],
                          sizeof(uff_protection_info_t), 1, uff->fp) != 1) { /* I/O error */ }
                }
                break;
            }
            
            case UFF_CHUNK_AUDT: {
                if (uff->audit_count < UFF_MAX_AUDIT) {
                    if (fread(&uff->audit[uff->audit_count++],
                          sizeof(uff_audit_entry_t), 1, uff->fp) != 1) { /* I/O error */ }
                }
                break;
            }
            
            default:
                /* Skip unknown chunk */
                if (fseek(uff->fp, chunk.size, SEEK_CUR) != 0) { /* I/O error */ }
                break;
        }
        
        uff->total_chunks++;
    }
    
    return uff;
}

/**
 * @brief Close UFF file
 */
void uff_close(uff_file_t* uff) {
    if (!uff) return;
    
    if (uff->write_mode && uff->fp) {
        /* Write all pending chunks */
        
        /* INFO chunk */
        if (uff->info_valid) {
            uff_chunk_header_t chunk = {
                .type = UFF_CHUNK_INFO,
                .size = sizeof(uff_info_t),
                .crc32 = calc_crc32(&uff->info, sizeof(uff_info_t)),
                .flags = 0
            };
            if (fwrite(&chunk, sizeof(chunk), 1, uff->fp) != 1) { /* write error */ }
            if (fwrite(&uff->info, sizeof(uff_info_t), 1, uff->fp) != 1) { /* write error */ }
            uff->total_chunks++;
        }
        
        /* CAPT chunk */
        if (uff->capture_valid) {
            uff_chunk_header_t chunk = {
                .type = UFF_CHUNK_CAPT,
                .size = sizeof(uff_capture_t),
                .crc32 = calc_crc32(&uff->capture, sizeof(uff_capture_t)),
                .flags = 0
            };
            if (fwrite(&chunk, sizeof(chunk), 1, uff->fp) != 1) { /* write error */ }
            if (fwrite(&uff->capture, sizeof(uff_capture_t), 1, uff->fp) != 1) { /* write error */ }
            uff->total_chunks++;
        }
        
        /* Track data */
        for (int t = 0; t < UFF_MAX_TRACKS; t++) {
            for (int s = 0; s < UFF_MAX_SIDES; s++) {
                uff_track_t* track = &uff->tracks[t][s];
                if (!track->valid) continue;
                
                /* TRCK chunk */
                uff_chunk_header_t chunk = {
                    .type = UFF_CHUNK_TRCK,
                    .size = sizeof(uff_track_header_t),
                    .crc32 = calc_crc32(&track->header, sizeof(uff_track_header_t)),
                    .flags = 0
                };
                if (fwrite(&chunk, sizeof(chunk), 1, uff->fp) != 1) { /* write error */ }
                if (fwrite(&track->header, sizeof(uff_track_header_t), 1, uff->fp) != 1) { /* write error */ }
                uff->total_chunks++;
                
                /* FLUX chunks */
                for (int r = 0; r < track->revolutions; r++) {
                    if (!track->flux_samples[r]) continue;
                    
                    uff_flux_header_t fh = {
                        .track = t,
                        .side = s,
                        .revolution = r,
                        .resolution = 25,  /* 25ns resolution */
                        .sample_count = track->flux_count[r],
                        .index_position = track->index_pos[r],
                        .total_time_ns = 0  /* Calculate if needed */
                    };
                    
                    size_t flux_size = sizeof(uff_flux_header_t) + 
                                       track->flux_count[r] * sizeof(uint32_t);
                    
                    uff_chunk_header_t fchunk = {
                        .type = UFF_CHUNK_FLUX,
                        .size = (uint32_t)flux_size,
                        .flags = 0
                    };
                    
                    if (fwrite(&fchunk, sizeof(fchunk), 1, uff->fp) != 1) { /* write error */ }
                    if (fwrite(&fh, sizeof(fh), 1, uff->fp) != 1) { /* write error */ }
                    if (fwrite(track->flux_samples[r], sizeof(uint32_t),
                           track->flux_count[r], uff->fp) != track->flux_count[r]) { /* I/O error */ }
                    uff->total_chunks++;
                    uff->flux_data_size += track->flux_count[r] * sizeof(uint32_t);
                }
                
                /* WEAK chunk */
                if (track->weak_count > 0) {
                    uff_weak_header_t wh = {
                        .track = t,
                        .side = s,
                        .count = track->weak_count
                    };
                    
                    size_t weak_size = sizeof(wh) + 
                                       track->weak_count * sizeof(uff_weak_region_t);
                    
                    uff_chunk_header_t wchunk = {
                        .type = UFF_CHUNK_WEAK,
                        .size = (uint32_t)weak_size,
                        .flags = 0
                    };
                    
                    if (fwrite(&wchunk, sizeof(wchunk), 1, uff->fp) != 1) { /* write error */ }
                    if (fwrite(&wh, sizeof(wh), 1, uff->fp) != 1) { /* write error */ }
                    if (fwrite(track->weak_regions, sizeof(uff_weak_region_t),
                           track->weak_count, uff->fp) != track->weak_count) { /* I/O error */ }
                    uff->total_chunks++;
                }
            }
        }
        
        /* Protection chunks */
        for (int i = 0; i < uff->protection_count; i++) {
            uff_chunk_header_t chunk = {
                .type = UFF_CHUNK_PROT,
                .size = sizeof(uff_protection_info_t),
                .crc32 = calc_crc32(&uff->protection[i], sizeof(uff_protection_info_t)),
                .flags = 0
            };
            if (fwrite(&chunk, sizeof(chunk), 1, uff->fp) != 1) { /* write error */ }
            if (fwrite(&uff->protection[i], sizeof(uff_protection_info_t), 1, uff->fp) != 1) { /* write error */ }
            uff->total_chunks++;
        }
        
        /* Audit chunks */
        for (int i = 0; i < uff->audit_count; i++) {
            uff_chunk_header_t chunk = {
                .type = UFF_CHUNK_AUDT,
                .size = sizeof(uff_audit_entry_t),
                .crc32 = calc_crc32(&uff->audit[i], sizeof(uff_audit_entry_t)),
                .flags = 0
            };
            if (fwrite(&chunk, sizeof(chunk), 1, uff->fp) != 1) { /* write error */ }
            if (fwrite(&uff->audit[i], sizeof(uff_audit_entry_t), 1, uff->fp) != 1) { /* write error */ }
            uff->total_chunks++;
        }
        
        /* Update and rewrite header */
        uff->header.total_chunks = uff->total_chunks;
        uff->header.flux_data_size = uff->flux_data_size;
        
        long end_pos = ftell(uff->fp);
        uff->header.total_size = (uint64_t)end_pos;
        uff->header.crc32 = calc_crc32(&uff->header, 
                                        sizeof(uff_header_t) - sizeof(uint32_t) - 24);
        
        if (fseek(uff->fp, 0, SEEK_SET) != 0) { /* I/O error */ }
        if (fwrite(&uff->header, sizeof(uff_header_t), 1, uff->fp) != 1) { /* write error */ }
    }
    
    /* Free track data */
    for (int t = 0; t < UFF_MAX_TRACKS; t++) {
        for (int s = 0; s < UFF_MAX_SIDES; s++) {
            for (int r = 0; r < UFF_MAX_REVOLUTIONS; r++) {
                free(uff->tracks[t][s].flux_samples[r]);
            }
            free(uff->tracks[t][s].bits);
        }
    }
    
    if (uff->fp) fclose(uff->fp);
    free(uff->path);
    free(uff);
}

/*============================================================================
 * INFO FUNCTIONS
 *============================================================================*/

int uff_set_info(uff_file_t* uff, const uff_info_t* info) {
    if (!uff || !info || !uff->write_mode) return -1;
    
    memcpy(&uff->info, info, sizeof(uff_info_t));
    uff->info_valid = true;
    
    return 0;
}

int uff_get_info(uff_file_t* uff, uff_info_t* info) {
    if (!uff || !info || !uff->info_valid) return -1;
    
    memcpy(info, &uff->info, sizeof(uff_info_t));
    return 0;
}

/*============================================================================
 * TRACK FUNCTIONS
 *============================================================================*/

int uff_write_flux(uff_file_t* uff, uint8_t track, uint8_t side,
                    uint8_t revolution, const uint32_t* samples,
                    uint32_t count, uint32_t index_pos) {
    if (!uff || !uff->write_mode) return -1;
    if (track >= UFF_MAX_TRACKS || side >= UFF_MAX_SIDES) return -1;
    if (revolution >= UFF_MAX_REVOLUTIONS) return -1;
    if (!samples || count == 0) return -1;
    
    uff_track_t* t = &uff->tracks[track][side];
    
    /* Allocate and copy flux data */
    t->flux_samples[revolution] = malloc(count * sizeof(uint32_t));
    if (!t->flux_samples[revolution]) return -1;
    
    memcpy(t->flux_samples[revolution], samples, count * sizeof(uint32_t));
    t->flux_count[revolution] = count;
    t->index_pos[revolution] = index_pos;
    
    if (revolution + 1 > t->revolutions) {
        t->revolutions = revolution + 1;
    }
    
    t->valid = true;
    t->header.track_num = track;
    t->header.side = side;
    t->header.revolutions = t->revolutions;
    t->header.flags |= 0x0020; /* UFF_TF_MULTI_REV */
    
    return 0;
}

int uff_add_weak_region(uff_file_t* uff, uint8_t track, uint8_t side,
                         uint32_t bit_offset, uint16_t bit_count,
                         uint8_t variance) {
    if (!uff || !uff->write_mode) return -1;
    if (track >= UFF_MAX_TRACKS || side >= UFF_MAX_SIDES) return -1;
    
    uff_track_t* t = &uff->tracks[track][side];
    if (t->weak_count >= UFF_MAX_WEAK) return -1;
    
    uff_weak_region_t* w = &t->weak_regions[t->weak_count++];
    w->bit_offset = bit_offset;
    w->bit_count = bit_count;
    w->variance = variance;
    w->flags = 0;
    
    t->header.flags |= 0x0001; /* UFF_TF_HAS_WEAK_BITS */
    
    return 0;
}

/*============================================================================
 * PROTECTION FUNCTIONS
 *============================================================================*/

int uff_add_protection(uff_file_t* uff, uint8_t type,
                        const char* name, const char* details,
                        uint8_t confidence) {
    if (!uff || !uff->write_mode) return -1;
    if (uff->protection_count >= 16) return -1;
    
    uff_protection_info_t* p = &uff->protection[uff->protection_count++];
    p->protection_type = type;
    p->confidence = confidence;
    
    if (name) {
        strncpy(p->name, name, sizeof(p->name) - 1);
    }
    if (details) {
        strncpy(p->details, details, sizeof(p->details) - 1);
    }
    
    return 0;
}

/*============================================================================
 * CAPTURE INFO
 *============================================================================*/

int uff_set_capture_info(uff_file_t* uff, const uff_capture_t* capture) {
    if (!uff || !capture || !uff->write_mode) return -1;
    
    memcpy(&uff->capture, capture, sizeof(uff_capture_t));
    uff->capture_valid = true;
    
    return 0;
}

/*============================================================================
 * AUDIT TRAIL
 *============================================================================*/

int uff_add_audit(uff_file_t* uff, uint8_t action, uint8_t track,
                   uint8_t side, uint8_t severity, const char* message) {
    if (!uff || uff->audit_count >= UFF_MAX_AUDIT) return -1;
    
    uff_audit_entry_t* e = &uff->audit[uff->audit_count++];
    e->timestamp = (uint32_t)time(NULL);
    e->action = action;
    e->track = track;
    e->side = side;
    e->severity = severity;
    
    if (message) {
        strncpy(e->message, message, sizeof(e->message) - 1);
    }
    
    return 0;
}

/*============================================================================
 * VALIDATION
 *============================================================================*/

int uff_validate(uff_file_t* uff) {
    if (!uff) return -1;
    
    /* Check magic */
    if (memcmp(uff->header.magic, UFF_MAGIC, 4) != 0) {
        return -2;
    }
    
    /* Check version */
    if (uff->header.version_major > UFF_VERSION_MAJOR) {
        return -3;
    }
    
    /* Validate has some tracks */
    int track_count = 0;
    for (int t = 0; t < UFF_MAX_TRACKS; t++) {
        for (int s = 0; s < UFF_MAX_SIDES; s++) {
            if (uff->tracks[t][s].valid) track_count++;
        }
    }
    
    if (track_count == 0) {
        return -4;
    }
    
    return 0;
}

/*============================================================================
 * UNIT TESTS
 *============================================================================*/

#ifdef UFF_TEST

#include <assert.h>

int main(void) {
    printf("=== UFT Flux Format (UFF) Tests ===\n");
    
    /* Test 1: Create and close */
    {
        uff_file_t* uff = uff_create("/tmp/test.uff");
        assert(uff != NULL);
        
        uff_info_t info = {
            .disk_type = 7,  /* AMIGA_DD */
            .encoding = 2,   /* MFM */
            .tracks = 80,
            .sides = 2,
            .rpm = 300,
            .bitcell_ns = 2000,
            .data_rate = 250000
        };
        strncpy(info.title, "Test Disk", sizeof(info.title));
        strncpy(info.platform, "Amiga", sizeof(info.platform));
        
        assert(uff_set_info(uff, &info) == 0);
        
        uff_close(uff);
        printf("✓ Create and close\n");
    }
    
    /* Test 2: Write flux data */
    {
        uff_file_t* uff = uff_create("/tmp/test2.uff");
        assert(uff != NULL);
        
        /* Generate test flux */
        uint32_t flux[1000];
        for (int i = 0; i < 1000; i++) {
            flux[i] = 4000 + (i % 100);  /* ~4us */
        }
        
        assert(uff_write_flux(uff, 0, 0, 0, flux, 1000, 500) == 0);
        assert(uff_write_flux(uff, 0, 0, 1, flux, 1000, 502) == 0);
        
        uff_close(uff);
        printf("✓ Write flux data\n");
    }
    
    /* Test 3: Add weak regions */
    {
        uff_file_t* uff = uff_create("/tmp/test3.uff");
        assert(uff != NULL);
        
        uint32_t flux[100] = {0};
        uff_write_flux(uff, 5, 0, 0, flux, 100, 0);
        
        assert(uff_add_weak_region(uff, 5, 0, 1000, 16, 200) == 0);
        assert(uff_add_weak_region(uff, 5, 0, 5000, 32, 180) == 0);
        
        uff_close(uff);
        printf("✓ Add weak regions\n");
    }
    
    /* Test 4: Add protection info */
    {
        uff_file_t* uff = uff_create("/tmp/test4.uff");
        assert(uff != NULL);
        
        assert(uff_add_protection(uff, 1, "Weak Bits", 
                                   "Track 5-8 contain weak bit protection", 95) == 0);
        assert(uff_add_protection(uff, 2, "Long Track",
                                   "Track 35 exceeds normal length", 80) == 0);
        
        uff_close(uff);
        printf("✓ Add protection info\n");
    }
    
    /* Test 5: Audit trail */
    {
        uff_file_t* uff = uff_create("/tmp/test5.uff");
        assert(uff != NULL);
        
        assert(uff_add_audit(uff, 1, 0, 0, 1, "Capture started") == 0);
        assert(uff_add_audit(uff, 2, 5, 0, 2, "CRC error, retrying") == 0);
        assert(uff_add_audit(uff, 2, 5, 0, 1, "Track recovered with multi-rev fusion") == 0);
        
        assert(uff->audit_count == 4);  /* +1 for creation */
        
        uff_close(uff);
        printf("✓ Audit trail\n");
    }
    
    /* Test 6: Open and read */
    {
        /* First create a file */
        uff_file_t* uff = uff_create("/tmp/test6.uff");
        
        uff_info_t info = {
            .disk_type = 1,
            .tracks = 35,
            .sides = 1
        };
        strncpy(info.title, "C64 Game", sizeof(info.title));
        uff_set_info(uff, &info);
        
        uint32_t flux[500];
        for (int i = 0; i < 500; i++) flux[i] = 3200;
        uff_write_flux(uff, 0, 0, 0, flux, 500, 0);
        
        uff_close(uff);
        
        /* Now open and read */
        uff = uff_open("/tmp/test6.uff");
        assert(uff != NULL);
        assert(uff->info_valid);
        assert(uff->info.tracks == 35);
        assert(strcmp(uff->info.title, "C64 Game") == 0);
        
        uff_close(uff);
        printf("✓ Open and read\n");
    }
    
    /* Test 7: Validate */
    {
        uff_file_t* uff = uff_create("/tmp/test7.uff");
        
        /* Empty file should fail validation */
        assert(uff_validate(uff) == -4);
        
        /* Add a track */
        uint32_t flux[100] = {0};
        uff_write_flux(uff, 0, 0, 0, flux, 100, 0);
        
        assert(uff_validate(uff) == 0);
        
        uff_close(uff);
        printf("✓ Validation\n");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* UFF_TEST */
