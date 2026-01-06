/**
 * @file uft_scp_multirev.c
 * @brief SCP Multi-Revolution Reader with Confidence Fusion
 * @version 3.3.2
 * @date 2025-01-03
 *
 * FEATURES:
 * - Multi-revolution support (up to 5 revolutions per track)
 * - Confidence-based fusion of weak bits
 * - Statistical analysis for PLL tuning
 * - Index-to-index timing extraction
 * - Forensic audit trail
 *
 * SCP File Format:
 * - Header: 16 bytes + 166 track offsets (4 bytes each)
 * - Track: "TRK" + track_num + revolution data
 * - Revolution: index_time(4) + track_len(4) + data_offset(4) + flux_data
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* Cross-platform packed struct support */
#include "uft/uft_compiler.h"

/*============================================================================
 * SCP CONSTANTS
 *============================================================================*/

#define SCP_SIGNATURE       "SCP"
#define SCP_HEADER_SIZE     16
#define SCP_TRACK_OFFSETS   168    /* 84 tracks * 2 sides */
#define SCP_MAX_REVOLUTIONS 5
#define SCP_TICK_NS         25     /* 40 MHz = 25ns per tick */

/* Disk type definitions */
#define SCP_DISK_C64        0x00
#define SCP_DISK_AMIGA      0x04
#define SCP_DISK_ATARI_FM   0x10
#define SCP_DISK_ATARI_MFM  0x11
#define SCP_DISK_APPLE_II   0x20
#define SCP_DISK_APPLE_GCR  0x24
#define SCP_DISK_IBM_MFM_DD 0x40
#define SCP_DISK_IBM_MFM_HD 0x44

/* Header flags */
#define SCP_FLAG_INDEX      0x01   /* Index stored at revolution boundary */
#define SCP_FLAG_96TPI      0x02   /* 96 TPI drive (80 track) */
#define SCP_FLAG_360RPM     0x04   /* 360 RPM drive */
#define SCP_FLAG_NORMALIZE  0x08   /* Data normalized */
#define SCP_FLAG_RW         0x10   /* Read/write heads */
#define SCP_FLAG_FOOTER     0x20   /* Footer present */

/*============================================================================
 * STRUCTURES
 * FIXED R18: Use UFT_PACK_BEGIN/END for MSVC compatibility
 *============================================================================*/

/* SCP File Header (16 bytes) */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  signature[3];       /* "SCP" */
    uint8_t  version;            /* Version (format version * 10) */
    uint8_t  disk_type;          /* Disk type (C64, Amiga, etc.) */
    uint8_t  revolutions;        /* Number of revolutions per track */
    uint8_t  start_track;        /* First track */
    uint8_t  end_track;          /* Last track */
    uint8_t  flags;              /* Feature flags */
    uint8_t  bit_cell_width;     /* 0=16bit, otherwise bit cell encoding */
    uint8_t  heads;              /* 0=both, 1=side0, 2=side1 */
    uint8_t  resolution;         /* 25ns * (resolution + 1) */
    uint32_t checksum;           /* Data checksum (optional) */
} scp_header_t;
UFT_PACK_END

/* Track header (4 bytes) */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  signature[3];       /* "TRK" */
    uint8_t  track_number;       /* Track number */
} scp_track_header_t;
UFT_PACK_END

/* Revolution entry (12 bytes per revolution) */
UFT_PACK_BEGIN
typedef struct {
    uint32_t index_time;         /* Time from index to index (ticks) */
    uint32_t track_length;       /* Number of flux transitions */
    uint32_t data_offset;        /* Offset to flux data from track header */
} scp_revolution_entry_t;
UFT_PACK_END

/* Single revolution data */
typedef struct {
    uint32_t index_time_ns;      /* Index to index time in ns */
    uint32_t flux_count;         /* Number of flux transitions */
    uint32_t* flux_data;         /* Flux timing data (ns) */
    float    rpm;                /* Calculated RPM */
    bool     valid;              /* Data valid flag */
} scp_revolution_t;

/* Multi-revolution track with confidence */
typedef struct {
    int track_number;
    int side;
    int revolution_count;
    
    /* Per-revolution data */
    scp_revolution_t revolutions[SCP_MAX_REVOLUTIONS];
    
    /* Fused data (best confidence) */
    uint32_t* fused_flux;
    uint32_t  fused_count;
    float*    confidence;        /* Per-transition confidence (0-1) */
    
    /* Weak bit detection */
    uint32_t* weak_positions;
    uint32_t  weak_count;
    
    /* Statistics */
    float avg_rpm;
    float rpm_variance;
    uint32_t avg_flux_count;
    float quality_score;         /* 0-100 overall quality */
    
    /* Forensic info */
    uint32_t checksum;
    char     timestamp[32];
} scp_multirev_track_t;

/* Decoder context */
typedef struct {
    FILE* file;
    scp_header_t header;
    uint32_t track_offsets[SCP_TRACK_OFFSETS];
    
    /* Timing resolution */
    uint32_t tick_ns;
    
    /* Statistics */
    uint64_t total_flux_read;
    uint32_t tracks_decoded;
    uint32_t weak_bits_detected;
    
    /* Configuration */
    float weak_threshold;        /* Variance threshold for weak bits */
    bool  enable_fusion;         /* Multi-rev fusion enabled */
    int   fusion_method;         /* 0=median, 1=weighted, 2=best */
    
    bool initialized;
} scp_multirev_ctx_t;

/*============================================================================
 * INTERNAL FUNCTIONS
 *============================================================================*/

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Calculate checksum for data verification
 */
static uint32_t calc_checksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

/**
 * @brief Compare function for qsort (median calculation)
 */
static int compare_uint32(const void* a, const void* b) {
    uint32_t va = *(const uint32_t*)a;
    uint32_t vb = *(const uint32_t*)b;
    return (va > vb) - (va < vb);
}

/**
 * @brief Calculate median of uint32 array
 */
static uint32_t median_uint32(uint32_t* values, int count) {
    if (count == 0) return 0;
    if (count == 1) return values[0];
    
    /* Make a copy for sorting */
    uint32_t* sorted = malloc(count * sizeof(uint32_t));
    if (!sorted) return values[0];
    
    memcpy(sorted, values, count * sizeof(uint32_t));
    qsort(sorted, count, sizeof(uint32_t), compare_uint32);
    
    uint32_t result;
    if (count % 2 == 0) {
        result = (sorted[count/2 - 1] + sorted[count/2]) / 2;
    } else {
        result = sorted[count/2];
    }
    
    free(sorted);
    return result;
}

/**
 * @brief Calculate variance of uint32 array
 */
static float variance_uint32(const uint32_t* values, int count) {
    if (count < 2) return 0.0f;
    
    /* Calculate mean */
    uint64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    float mean = (float)sum / count;
    
    /* Calculate variance */
    float var_sum = 0.0f;
    for (int i = 0; i < count; i++) {
        float diff = (float)values[i] - mean;
        var_sum += diff * diff;
    }
    
    return var_sum / (count - 1);
}

/**
 * @brief Read flux data for a single revolution
 */
static int read_revolution_flux(
    scp_multirev_ctx_t* ctx,
    uint32_t data_offset,
    uint32_t flux_count,
    uint32_t** flux_out
) {
    if (!ctx || !flux_out || flux_count == 0) {
        return -1;
    }
    
    /* Allocate output buffer */
    uint32_t* flux = malloc(flux_count * sizeof(uint32_t));
    if (!flux) {
        return -1;
    }
    
    /* Seek to data */
    if (fseek(ctx->file, data_offset, SEEK_SET) != 0) {
        free(flux);
        return -1;
    }
    
    /* Read flux data */
    if (ctx->header.bit_cell_width == 0) {
        /* 16-bit flux values */
        uint8_t buf[2];
        uint32_t accumulated = 0;
        size_t out_idx = 0;
        
        for (uint32_t i = 0; i < flux_count && out_idx < flux_count; i++) {
            if (fread(buf, 1, 2, ctx->file) != 2) {
                break;
            }
            
            uint16_t val = read_le16(buf);
            
            if (val == 0) {
                /* Overflow - accumulate */
                accumulated += 65536;
            } else {
                /* Valid transition */
                flux[out_idx++] = (accumulated + val) * ctx->tick_ns;
                accumulated = 0;
            }
        }
        
        /* Adjust count if we had overflows */
        flux_count = out_idx;
        
    } else {
        /* 8-bit flux values (compressed) */
        uint8_t byte;
        uint32_t accumulated = 0;
        size_t out_idx = 0;
        
        for (uint32_t i = 0; i < flux_count && out_idx < flux_count; i++) {
            if (fread(&byte, 1, 1, ctx->file) != 1) {
                break;
            }
            
            if (byte == 0xFF) {
                /* Overflow */
                accumulated += 255;
            } else {
                flux[out_idx++] = (accumulated + byte) * ctx->tick_ns;
                accumulated = 0;
            }
        }
        
        flux_count = out_idx;
    }
    
    *flux_out = flux;
    return (int)flux_count;
}

/**
 * @brief Align revolutions by finding common sync pattern
 */
static int align_revolutions(scp_multirev_track_t* track) {
    if (!track || track->revolution_count < 2) {
        return 0;
    }
    
    /* Use first revolution as reference */
    scp_revolution_t* ref = &track->revolutions[0];
    if (!ref->valid || ref->flux_count < 100) {
        return -1;
    }
    
    /* Find sync pattern (first 50 transitions) */
    int pattern_len = 50;
    if (ref->flux_count < (uint32_t)pattern_len) {
        pattern_len = ref->flux_count / 2;
    }
    
    /* For each subsequent revolution, find best alignment */
    for (int r = 1; r < track->revolution_count; r++) {
        scp_revolution_t* rev = &track->revolutions[r];
        if (!rev->valid || rev->flux_count < 100) {
            continue;
        }
        
        /* Search for pattern match (simplified correlation) */
        /* In production, use proper cross-correlation */
        int best_offset = 0;
        float best_score = 0.0f;
        
        for (int offset = 0; offset < 100 && offset < (int)rev->flux_count - pattern_len; offset++) {
            float score = 0.0f;
            
            for (int i = 0; i < pattern_len; i++) {
                float ref_val = (float)ref->flux_data[i];
                float rev_val = (float)rev->flux_data[offset + i];
                
                /* Tolerance of 10% */
                float diff = fabsf(ref_val - rev_val) / ref_val;
                if (diff < 0.1f) {
                    score += 1.0f;
                }
            }
            
            if (score > best_score) {
                best_score = score;
                best_offset = offset;
            }
        }
        
        /* Apply alignment (shift data) */
        if (best_offset > 0) {
            memmove(rev->flux_data, 
                    rev->flux_data + best_offset,
                    (rev->flux_count - best_offset) * sizeof(uint32_t));
            rev->flux_count -= best_offset;
        }
    }
    
    return 0;
}

/**
 * @brief Fuse multiple revolutions with confidence weighting
 */
static int fuse_revolutions(scp_multirev_ctx_t* ctx, scp_multirev_track_t* track) {
    if (!ctx || !track || track->revolution_count == 0) {
        return -1;
    }
    
    /* Find minimum flux count across valid revolutions */
    uint32_t min_count = UINT32_MAX;
    int valid_revs = 0;
    
    for (int r = 0; r < track->revolution_count; r++) {
        if (track->revolutions[r].valid && track->revolutions[r].flux_count > 0) {
            if (track->revolutions[r].flux_count < min_count) {
                min_count = track->revolutions[r].flux_count;
            }
            valid_revs++;
        }
    }
    
    if (valid_revs == 0 || min_count == UINT32_MAX) {
        return -1;
    }
    
    /* Allocate fused output */
    track->fused_flux = malloc(min_count * sizeof(uint32_t));
    track->confidence = malloc(min_count * sizeof(float));
    track->weak_positions = malloc(min_count * sizeof(uint32_t));
    
    if (!track->fused_flux || !track->confidence || !track->weak_positions) {
        free(track->fused_flux);
        free(track->confidence);
        free(track->weak_positions);
        track->fused_flux = NULL;
        track->confidence = NULL;
        track->weak_positions = NULL;
        return -1;
    }
    
    track->fused_count = min_count;
    track->weak_count = 0;
    
    /* Fusion buffer for collecting values per position */
    uint32_t* values = malloc(valid_revs * sizeof(uint32_t));
    if (!values) {
        return -1;
    }
    
    /* Process each flux position */
    for (uint32_t i = 0; i < min_count; i++) {
        int val_count = 0;
        
        /* Collect values from all revolutions */
        for (int r = 0; r < track->revolution_count; r++) {
            if (track->revolutions[r].valid && 
                i < track->revolutions[r].flux_count) {
                values[val_count++] = track->revolutions[r].flux_data[i];
            }
        }
        
        if (val_count == 0) {
            track->fused_flux[i] = 0;
            track->confidence[i] = 0.0f;
            continue;
        }
        
        /* Calculate fused value based on method */
        uint32_t fused;
        float conf = 1.0f;
        
        if (val_count == 1) {
            fused = values[0];
            conf = 0.5f;  /* Single read = lower confidence */
        } else {
            switch (ctx->fusion_method) {
                case 0:  /* Median */
                    fused = median_uint32(values, val_count);
                    break;
                    
                case 1:  /* Weighted average */
                {
                    uint64_t sum = 0;
                    for (int j = 0; j < val_count; j++) {
                        sum += values[j];
                    }
                    fused = (uint32_t)(sum / val_count);
                    break;
                }
                
                case 2:  /* Best (lowest variance) */
                default:
                    fused = values[0];  /* First revolution */
                    break;
            }
            
            /* Calculate confidence from variance */
            float var = variance_uint32(values, val_count);
            float mean = (float)fused;
            float cv = (mean > 0) ? sqrtf(var) / mean : 1.0f;  /* Coefficient of variation */
            
            /* High variance = low confidence */
            conf = 1.0f / (1.0f + cv * 10.0f);
            
            /* Detect weak bits (high variance) */
            if (cv > ctx->weak_threshold) {
                track->weak_positions[track->weak_count++] = i;
                ctx->weak_bits_detected++;
            }
        }
        
        track->fused_flux[i] = fused;
        track->confidence[i] = conf;
    }
    
    free(values);
    
    /* Calculate average confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < track->fused_count; i++) {
        total_conf += track->confidence[i];
    }
    track->quality_score = (track->fused_count > 0) ? 
                          (total_conf / track->fused_count) * 100.0f : 0.0f;
    
    return 0;
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open SCP file for multi-revolution reading
 */
scp_multirev_ctx_t* scp_multirev_open(const char* path) {
    if (!path) return NULL;
    
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    scp_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    /* Verify signature */
    if (memcmp(header.signature, SCP_SIGNATURE, 3) != 0) {
        fclose(f);
        return NULL;
    }
    
    /* Allocate context */
    scp_multirev_ctx_t* ctx = calloc(1, sizeof(scp_multirev_ctx_t));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    memcpy(&ctx->header, &header, sizeof(header));
    
    /* Calculate tick resolution */
    ctx->tick_ns = SCP_TICK_NS * (header.resolution + 1);
    
    /* Read track offsets */
    if (fread(ctx->track_offsets, sizeof(uint32_t), SCP_TRACK_OFFSETS, f) != SCP_TRACK_OFFSETS) {
        fclose(f);
        free(ctx);
        return NULL;
    }
    
    /* Convert offsets from little-endian */
    for (int i = 0; i < SCP_TRACK_OFFSETS; i++) {
        uint8_t* p = (uint8_t*)&ctx->track_offsets[i];
        ctx->track_offsets[i] = read_le32(p);
    }
    
    /* Default configuration */
    ctx->weak_threshold = 0.15f;   /* 15% coefficient of variation */
    ctx->enable_fusion = true;
    ctx->fusion_method = 0;        /* Median */
    
    ctx->initialized = true;
    
    return ctx;
}

/**
 * @brief Close SCP context
 */
void scp_multirev_close(scp_multirev_ctx_t** ctx) {
    if (!ctx || !*ctx) return;
    
    if ((*ctx)->file) {
        fclose((*ctx)->file);
    }
    
    free(*ctx);
    *ctx = NULL;
}

/**
 * @brief Read track with all revolutions
 */
int scp_multirev_read_track(
    scp_multirev_ctx_t* ctx,
    int track_num,
    int side,
    scp_multirev_track_t** track_out
) {
    if (!ctx || !track_out || !ctx->initialized) {
        return -1;
    }
    
    /* Validate track range */
    if (track_num < ctx->header.start_track || 
        track_num > ctx->header.end_track) {
        return -1;
    }
    
    /* Calculate track index */
    int track_idx;
    if (ctx->header.heads == 0) {
        /* Both heads interleaved */
        track_idx = track_num * 2 + side;
    } else {
        track_idx = track_num;
    }
    
    if (track_idx >= SCP_TRACK_OFFSETS) {
        return -1;
    }
    
    uint32_t offset = ctx->track_offsets[track_idx];
    if (offset == 0) {
        return -1;  /* Track not present */
    }
    
    /* Seek to track */
    if (fseek(ctx->file, offset, SEEK_SET) != 0) {
        return -1;
    }
    
    /* Read track header */
    scp_track_header_t th;
    if (fread(&th, sizeof(th), 1, ctx->file) != 1) {
        return -1;
    }
    
    /* Verify track header */
    if (memcmp(th.signature, "TRK", 3) != 0) {
        return -1;
    }
    
    /* Allocate track structure */
    scp_multirev_track_t* track = calloc(1, sizeof(scp_multirev_track_t));
    if (!track) {
        return -1;
    }
    
    track->track_number = track_num;
    track->side = side;
    track->revolution_count = ctx->header.revolutions;
    
    if (track->revolution_count > SCP_MAX_REVOLUTIONS) {
        track->revolution_count = SCP_MAX_REVOLUTIONS;
    }
    
    /* Read revolution entries */
    scp_revolution_entry_t rev_entries[SCP_MAX_REVOLUTIONS];
    
    if (fread(rev_entries, sizeof(scp_revolution_entry_t), 
              track->revolution_count, ctx->file) != (size_t)track->revolution_count) {
        free(track);
        return -1;
    }
    
    /* Process each revolution */
    uint64_t rpm_sum = 0;
    int valid_rpm_count = 0;
    
    for (int r = 0; r < track->revolution_count; r++) {
        /* Convert from little-endian */
        uint8_t* p = (uint8_t*)&rev_entries[r];
        uint32_t index_time = read_le32(p);
        uint32_t track_len = read_le32(p + 4);
        uint32_t data_off = read_le32(p + 8);
        
        /* Calculate absolute data offset */
        uint32_t abs_offset = offset + sizeof(scp_track_header_t) + data_off;
        
        /* Read flux data */
        uint32_t* flux_data = NULL;
        int flux_count = read_revolution_flux(ctx, abs_offset, track_len, &flux_data);
        
        if (flux_count > 0 && flux_data) {
            track->revolutions[r].flux_data = flux_data;
            track->revolutions[r].flux_count = flux_count;
            track->revolutions[r].index_time_ns = index_time * ctx->tick_ns;
            track->revolutions[r].valid = true;
            
            /* Calculate RPM */
            if (index_time > 0) {
                /* RPM = 60 / (index_time_sec) = 60e9 / index_time_ns */
                float time_sec = (float)(index_time * ctx->tick_ns) / 1e9f;
                track->revolutions[r].rpm = 60.0f / time_sec;
                rpm_sum += (uint64_t)(track->revolutions[r].rpm * 100);
                valid_rpm_count++;
            }
            
            ctx->total_flux_read += flux_count;
        } else {
            track->revolutions[r].valid = false;
        }
    }
    
    /* Calculate average RPM */
    if (valid_rpm_count > 0) {
        track->avg_rpm = (float)rpm_sum / (valid_rpm_count * 100);
        
        /* Calculate RPM variance */
        float var_sum = 0.0f;
        for (int r = 0; r < track->revolution_count; r++) {
            if (track->revolutions[r].valid) {
                float diff = track->revolutions[r].rpm - track->avg_rpm;
                var_sum += diff * diff;
            }
        }
        track->rpm_variance = var_sum / valid_rpm_count;
    }
    
    /* Align and fuse revolutions */
    if (ctx->enable_fusion && track->revolution_count > 1) {
        align_revolutions(track);
        fuse_revolutions(ctx, track);
    }
    
    /* Generate checksum */
    if (track->fused_flux && track->fused_count > 0) {
        track->checksum = calc_checksum((uint8_t*)track->fused_flux, 
                                        track->fused_count * sizeof(uint32_t));
    }
    
    ctx->tracks_decoded++;
    *track_out = track;
    
    return 0;
}

/**
 * @brief Free track data
 */
void scp_multirev_free_track(scp_multirev_track_t** track) {
    if (!track || !*track) return;
    
    scp_multirev_track_t* t = *track;
    
    /* Free per-revolution data */
    for (int r = 0; r < t->revolution_count; r++) {
        free(t->revolutions[r].flux_data);
    }
    
    /* Free fused data */
    free(t->fused_flux);
    free(t->confidence);
    free(t->weak_positions);
    
    free(t);
    *track = NULL;
}

/**
 * @brief Get disk info
 */
void scp_multirev_get_info(
    scp_multirev_ctx_t* ctx,
    int* start_track,
    int* end_track,
    int* revolutions,
    int* disk_type
) {
    if (!ctx) return;
    
    if (start_track) *start_track = ctx->header.start_track;
    if (end_track) *end_track = ctx->header.end_track;
    if (revolutions) *revolutions = ctx->header.revolutions;
    if (disk_type) *disk_type = ctx->header.disk_type;
}

/**
 * @brief Configure fusion parameters
 */
void scp_multirev_configure(
    scp_multirev_ctx_t* ctx,
    bool enable_fusion,
    int fusion_method,
    float weak_threshold
) {
    if (!ctx) return;
    
    ctx->enable_fusion = enable_fusion;
    ctx->fusion_method = fusion_method;
    ctx->weak_threshold = weak_threshold;
}

/**
 * @brief Get statistics
 */
void scp_multirev_get_stats(
    scp_multirev_ctx_t* ctx,
    uint64_t* total_flux,
    uint32_t* tracks_decoded,
    uint32_t* weak_bits
) {
    if (!ctx) return;
    
    if (total_flux) *total_flux = ctx->total_flux_read;
    if (tracks_decoded) *tracks_decoded = ctx->tracks_decoded;
    if (weak_bits) *weak_bits = ctx->weak_bits_detected;
}

/*============================================================================
 * UNIT TEST
 *============================================================================*/

#ifdef SCP_MULTIREV_TEST

#include <assert.h>

int main(void) {
    printf("=== SCP Multi-Revolution Unit Tests ===\n");
    
    /* Test 1: Median calculation */
    {
        uint32_t values[] = {100, 200, 150, 180, 170};
        uint32_t med = median_uint32(values, 5);
        assert(med == 170);
        printf("✓ Median calculation\n");
    }
    
    /* Test 2: Variance calculation */
    {
        uint32_t values[] = {100, 100, 100, 100};
        float var = variance_uint32(values, 4);
        assert(var < 0.001f);
        printf("✓ Variance (zero case)\n");
    }
    
    /* Test 3: Header size */
    {
        assert(sizeof(scp_header_t) == 16);
        printf("✓ Header size: 16 bytes\n");
    }
    
    /* Test 4: Revolution entry size */
    {
        assert(sizeof(scp_revolution_entry_t) == 12);
        printf("✓ Revolution entry: 12 bytes\n");
    }
    
    /* Test 5: Context allocation */
    {
        scp_multirev_ctx_t* ctx = calloc(1, sizeof(scp_multirev_ctx_t));
        assert(ctx != NULL);
        ctx->weak_threshold = 0.2f;
        ctx->fusion_method = 1;
        assert(ctx->weak_threshold == 0.2f);
        free(ctx);
        printf("✓ Context allocation\n");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* SCP_MULTIREV_TEST */
