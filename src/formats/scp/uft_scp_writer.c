/**
 * @file uft_scp_writer.c
 * @brief SCP (SuperCard Pro) Writer Implementation (W-P0-003)
 * 
 * Creates SCP flux images from flux data.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * SCP FORMAT DEFINITIONS
 *===========================================================================*/

#define SCP_MAGIC           "SCP"
#define SCP_VERSION         0x19    /* Version 2.5 */
#define SCP_MAX_TRACKS      168     /* 84 cylinders × 2 heads */
#define SCP_TICK_NS         25      /* 25ns per tick (40MHz) */

#pragma pack(push, 1)

typedef struct {
    char        magic[3];       /* "SCP" */
    uint8_t     version;        /* Version (0x19 = 2.5) */
    uint8_t     disk_type;      /* Disk type */
    uint8_t     revolutions;    /* Number of revolutions */
    uint8_t     start_track;    /* First track */
    uint8_t     end_track;      /* Last track */
    uint8_t     flags;          /* Flags */
    uint8_t     bitcell_width;  /* 0=16bit, 1=8bit */
    uint8_t     heads;          /* 0=both, 1=side0, 2=side1 */
    uint8_t     resolution;     /* 25ns * (resolution + 1) */
    uint32_t    checksum;       /* Data checksum */
} scp_header_t;

typedef struct {
    char        magic[3];       /* "TRK" */
    uint8_t     track_num;      /* Track number */
} scp_track_header_t;

typedef struct {
    uint32_t    duration;       /* Index time in ticks */
    uint32_t    length;         /* Data length in bytes */
    uint32_t    offset;         /* Offset from track header */
} scp_revolution_t;

#pragma pack(pop)

/* Disk Types */
#define SCP_TYPE_C64        0x00
#define SCP_TYPE_AMIGA      0x04
#define SCP_TYPE_ATARI_ST   0x08
#define SCP_TYPE_PC_DD      0x20
#define SCP_TYPE_PC_HD      0x30

/* Flags */
#define SCP_FLAG_INDEX      0x01
#define SCP_FLAG_96TPI      0x02
#define SCP_FLAG_360RPM     0x04
#define SCP_FLAG_NORMALIZED 0x08
#define SCP_FLAG_RW         0x10
#define SCP_FLAG_FOOTER     0x20

/*===========================================================================
 * WRITER CONTEXT
 *===========================================================================*/

typedef struct {
    uint32_t    *flux;          /* Flux transitions (in ns) */
    size_t      flux_count;     /* Number of transitions */
    uint32_t    duration_ns;    /* Total duration in ns */
} scp_revolution_data_t;

typedef struct {
    int         track_num;      /* Track number (0-167) */
    int         rev_count;      /* Number of revolutions */
    scp_revolution_data_t revs[5];  /* Max 5 revolutions */
} scp_track_data_t;

typedef struct {
    FILE        *file;
    scp_header_t header;
    uint32_t    track_offsets[SCP_MAX_TRACKS];
    scp_track_data_t tracks[SCP_MAX_TRACKS];
    int         track_count;
    uint32_t    checksum;
} scp_writer_t;

/*===========================================================================
 * CHECKSUM
 *===========================================================================*/

static void update_checksum(scp_writer_t *w, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++) {
        w->checksum += bytes[i];
    }
}

/*===========================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Create SCP writer
 */
scp_writer_t* scp_writer_create(uint8_t disk_type, uint8_t revolutions) {
    scp_writer_t *w = (scp_writer_t *)calloc(1, sizeof(scp_writer_t));
    if (!w) return NULL;
    
    /* Initialize header */
    memcpy(w->header.magic, SCP_MAGIC, 3);
    w->header.version = SCP_VERSION;
    w->header.disk_type = disk_type;
    w->header.revolutions = revolutions;
    w->header.start_track = 0xFF;
    w->header.end_track = 0;
    w->header.flags = SCP_FLAG_INDEX | SCP_FLAG_RW;
    w->header.bitcell_width = 0;  /* 16-bit */
    w->header.heads = 0;          /* Both sides */
    w->header.resolution = 0;     /* 25ns */
    
    return w;
}

/**
 * @brief Add track data to writer
 */
int scp_writer_add_track(
    scp_writer_t *w,
    int track_num,
    int side,
    const uint32_t *flux_ns,
    size_t flux_count,
    uint32_t duration_ns,
    int revolution)
{
    if (!w || track_num < 0 || track_num >= 84) return -1;
    if (side < 0 || side > 1) return -1;
    if (revolution < 0 || revolution >= 5) return -1;
    
    /* Calculate SCP track number (interleaved) */
    int scp_track = track_num * 2 + side;
    
    /* Find or create track entry */
    scp_track_data_t *track = NULL;
    for (int i = 0; i < w->track_count; i++) {
        if (w->tracks[i].track_num == scp_track) {
            track = &w->tracks[i];
            break;
        }
    }
    
    if (!track) {
        if (w->track_count >= SCP_MAX_TRACKS) return -1;
        track = &w->tracks[w->track_count++];
        track->track_num = scp_track;
        track->rev_count = 0;
    }
    
    /* Add revolution */
    if (revolution >= track->rev_count) {
        track->rev_count = revolution + 1;
    }
    
    scp_revolution_data_t *rev = &track->revs[revolution];
    
    /* Copy flux data */
    rev->flux = (uint32_t *)malloc(flux_count * sizeof(uint32_t));
    if (!rev->flux) return -1;
    
    memcpy(rev->flux, flux_ns, flux_count * sizeof(uint32_t));
    rev->flux_count = flux_count;
    rev->duration_ns = duration_ns;
    
    /* Update header bounds */
    if (scp_track < w->header.start_track) {
        w->header.start_track = scp_track;
    }
    if (scp_track > w->header.end_track) {
        w->header.end_track = scp_track;
    }
    
    return 0;
}

/**
 * @brief Write SCP file
 */
int scp_writer_save(scp_writer_t *w, const char *path) {
    if (!w || !path) return -1;
    
    w->file = fopen(path, "wb");
    if (!w->file) return -1;
    
    w->checksum = 0;
    
    /* Calculate file layout */
    size_t header_size = sizeof(scp_header_t);
    size_t offset_table_size = SCP_MAX_TRACKS * sizeof(uint32_t);
    size_t data_offset = header_size + offset_table_size;
    
    /* Initialize track offsets to 0 */
    memset(w->track_offsets, 0, sizeof(w->track_offsets));
    
    /* Write placeholder header */
    fwrite(&w->header, sizeof(scp_header_t), 1, w->file);
    
    /* Write placeholder offset table */
    fwrite(w->track_offsets, sizeof(w->track_offsets), 1, w->file);
    
    /* Write track data */
    for (int i = 0; i < w->track_count; i++) {
        scp_track_data_t *track = &w->tracks[i];
        
        /* Record offset */
        w->track_offsets[track->track_num] = (uint32_t)ftell(w->file);
        
        /* Write track header */
        scp_track_header_t trk_hdr = {
            .magic = {'T', 'R', 'K'},
            .track_num = (uint8_t)track->track_num
        };
        fwrite(&trk_hdr, sizeof(trk_hdr), 1, w->file);
        update_checksum(w, &trk_hdr, sizeof(trk_hdr));
        
        /* Calculate revolution data offsets */
        size_t rev_headers_size = track->rev_count * sizeof(scp_revolution_t);
        size_t flux_offset = sizeof(scp_track_header_t) + rev_headers_size;
        
        /* Write revolution headers */
        for (int r = 0; r < track->rev_count; r++) {
            scp_revolution_data_t *rev = &track->revs[r];
            
            /* Convert duration to ticks (40MHz = 25ns per tick) */
            uint32_t duration_ticks = rev->duration_ns / SCP_TICK_NS;
            uint32_t data_length = rev->flux_count * 2;  /* 16-bit values */
            
            scp_revolution_t rev_hdr = {
                .duration = duration_ticks,
                .length = data_length,
                .offset = (uint32_t)flux_offset
            };
            fwrite(&rev_hdr, sizeof(rev_hdr), 1, w->file);
            update_checksum(w, &rev_hdr, sizeof(rev_hdr));
            
            flux_offset += data_length;
        }
        
        /* Write flux data */
        for (int r = 0; r < track->rev_count; r++) {
            scp_revolution_data_t *rev = &track->revs[r];
            
            for (size_t f = 0; f < rev->flux_count; f++) {
                /* Convert ns to ticks */
                uint32_t ticks = rev->flux[f] / SCP_TICK_NS;
                
                /* Handle overflow (>65535 ticks) */
                while (ticks > 65535) {
                    uint16_t overflow = 0;
                    fwrite(&overflow, sizeof(uint16_t), 1, w->file);
                    update_checksum(w, &overflow, sizeof(uint16_t));
                    ticks -= 65536;
                }
                
                uint16_t val = (uint16_t)ticks;
                /* Big-endian in SCP */
                uint16_t be_val = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
                fwrite(&be_val, sizeof(uint16_t), 1, w->file);
                update_checksum(w, &be_val, sizeof(uint16_t));
            }
        }
    }
    
    /* Update header with checksum */
    w->header.checksum = w->checksum;
    
    /* Rewrite header and offset table */
    fseek(w->file, 0, SEEK_SET);
    fwrite(&w->header, sizeof(scp_header_t), 1, w->file);
    fwrite(w->track_offsets, sizeof(w->track_offsets), 1, w->file);
    
    fclose(w->file);
    w->file = NULL;
    
    return 0;
}

/**
 * @brief Free writer resources
 */
void scp_writer_free(scp_writer_t *w) {
    if (!w) return;
    
    for (int i = 0; i < w->track_count; i++) {
        for (int r = 0; r < w->tracks[i].rev_count; r++) {
            free(w->tracks[i].revs[r].flux);
        }
    }
    
    if (w->file) fclose(w->file);
    free(w);
}

/*===========================================================================
 * CONVENIENCE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get disk type from format hint
 */
uint8_t scp_disk_type_from_hint(const char *hint) {
    if (!hint) return SCP_TYPE_PC_DD;
    
    if (strstr(hint, "amiga") || strstr(hint, "adf")) return SCP_TYPE_AMIGA;
    if (strstr(hint, "c64") || strstr(hint, "d64")) return SCP_TYPE_C64;
    if (strstr(hint, "atari") || strstr(hint, "st")) return SCP_TYPE_ATARI_ST;
    if (strstr(hint, "hd") || strstr(hint, "1.44")) return SCP_TYPE_PC_HD;
    
    return SCP_TYPE_PC_DD;
}

/**
 * @brief Quick write - single function to create SCP from flux data
 * 
 * @param path Output file path
 * @param disk_type SCP disk type
 * @param tracks Array of track flux data
 * @param track_count Number of tracks
 * @param revolutions Revolutions per track
 * @return 0 on success
 */
int scp_write_quick(
    const char *path,
    uint8_t disk_type,
    int sides,
    int tracks_per_side,
    const uint32_t **flux_data,
    const size_t *flux_counts,
    const uint32_t *durations,
    int revolutions)
{
    if (!path || !flux_data || !flux_counts || !durations) return -1;
    
    scp_writer_t *w = scp_writer_create(disk_type, revolutions);
    if (!w) return -1;
    
    int result = 0;
    int idx = 0;
    
    for (int t = 0; t < tracks_per_side && result == 0; t++) {
        for (int s = 0; s < sides && result == 0; s++) {
            for (int r = 0; r < revolutions && result == 0; r++) {
                if (flux_data[idx] && flux_counts[idx] > 0) {
                    result = scp_writer_add_track(w, t, s, 
                                                   flux_data[idx], 
                                                   flux_counts[idx],
                                                   durations[idx], r);
                }
                idx++;
            }
        }
    }
    
    if (result == 0) {
        result = scp_writer_save(w, path);
    }
    
    scp_writer_free(w);
    return result;
}
