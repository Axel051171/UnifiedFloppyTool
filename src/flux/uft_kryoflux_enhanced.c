/**
 * @file uft_kryoflux_enhanced.c
 * @brief KryoFlux Enhanced Parser Implementation
 * 
 * EXT2-014: Enhanced KryoFlux stream parsing
 * 
 * Features:
 * - CTool compatibility
 * - Enhanced OOB handling
 * - Multi-stream merging
 * - Weak bit detection
 * - Quality metrics
 */

#include "uft/flux/uft_kryoflux_enhanced.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* KryoFlux sample clock */
#define KF_SCK              24027428.57142857
#define KF_ICK              (KF_SCK / 8.0)

/* CTool command codes */
#define CT_RESET            0x00
#define CT_READ_TRACK       0x01
#define CT_READ_STREAM      0x02
#define CT_WRITE_TRACK      0x03
#define CT_GET_INFO         0x04
#define CT_SET_DENSITY      0x05

/* Enhanced OOB types */
#define OOB_STREAM_INFO     0x01
#define OOB_INDEX           0x02
#define OOB_STREAM_END      0x03
#define OOB_INFO            0x04
#define OOB_EOF             0x0D

/* Quality thresholds */
#define QUALITY_EXCELLENT   95.0
#define QUALITY_GOOD        80.0
#define QUALITY_FAIR        60.0
#define QUALITY_POOR        40.0

/*===========================================================================
 * Stream Parsing Context
 *===========================================================================*/

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
    
    uint32_t overflow;
    uint32_t stream_pos;
    
    /* Hardware info from OOB */
    char hw_name[64];
    char hw_version[32];
    double sample_clock;
    double index_clock;
    
    /* Stream statistics */
    uint32_t flux_count;
    uint32_t index_count;
    uint32_t oob_count;
    uint32_t error_count;
} kf_enhanced_ctx_t;

/*===========================================================================
 * Helpers
 *===========================================================================*/

static uint16_t read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/*===========================================================================
 * Enhanced Stream Parsing
 *===========================================================================*/

int uft_kf_enhanced_open(uft_kf_enhanced_t *stream, 
                         const uint8_t *data, size_t size)
{
    if (!stream || !data || size < 16) return -1;
    
    memset(stream, 0, sizeof(*stream));
    
    /* Allocate flux buffer */
    stream->flux_capacity = 100000;
    stream->flux_times = malloc(stream->flux_capacity * sizeof(uint32_t));
    if (!stream->flux_times) return -1;
    
    /* Allocate index buffer */
    stream->index_capacity = 20;
    stream->indices = malloc(stream->index_capacity * sizeof(uft_kf_index_info_t));
    if (!stream->indices) {
        free(stream->flux_times);
        return -1;
    }
    
    /* Default clocks */
    stream->sample_clock = KF_SCK;
    stream->index_clock = KF_ICK;
    
    /* Parse stream */
    size_t pos = 0;
    uint32_t overflow = 0;
    uint32_t stream_pos = 0;
    
    while (pos < size) {
        uint8_t byte = data[pos];
        
        /* Check for OOB */
        if (byte == 0x0D && pos + 1 < size && data[pos + 1] != 0x00) {
            /* Parse OOB block */
            uint8_t oob_type = data[pos + 1];
            uint16_t oob_len = read_le16(data + pos + 2);
            
            if (pos + 4 + oob_len > size) break;
            
            const uint8_t *oob_data = data + pos + 4;
            
            switch (oob_type) {
                case OOB_STREAM_INFO:
                    if (oob_len >= 8) {
                        stream->stream_pos = read_le32(oob_data);
                        stream->transfer_time = read_le32(oob_data + 4);
                    }
                    break;
                    
                case OOB_INDEX:
                    if (oob_len >= 12 && stream->index_count < stream->index_capacity) {
                        uft_kf_index_info_t *idx = &stream->indices[stream->index_count];
                        idx->stream_pos = read_le32(oob_data);
                        idx->sample_counter = read_le32(oob_data + 4);
                        idx->index_counter = read_le32(oob_data + 8);
                        idx->flux_offset = stream->flux_count;
                        stream->index_count++;
                    }
                    break;
                    
                case OOB_STREAM_END:
                    if (oob_len >= 8) {
                        stream->result_code = read_le32(oob_data + 4);
                    }
                    stream->stream_ended = true;
                    break;
                    
                case OOB_INFO:
                    /* Parse key=value pairs */
                    {
                        char info[256];
                        size_t copy_len = (oob_len < 255) ? oob_len : 255;
                        memcpy(info, oob_data, copy_len);
                        info[copy_len] = '\0';
                        
                        char *p = strstr(info, "sck=");
                        if (p) stream->sample_clock = atof(p + 4);
                        
                        p = strstr(info, "ick=");
                        if (p) stream->index_clock = atof(p + 4);
                        
                        p = strstr(info, "name=");
                        if (p) {
                            char *end = strchr(p + 5, ',');
                            size_t len = end ? (size_t)(end - p - 5) : strlen(p + 5);
                            if (len > 63) len = 63;
                            memcpy(stream->hw_name, p + 5, len);
                        }
                    }
                    break;
                    
                case OOB_EOF:
                    stream->eof_reached = true;
                    break;
            }
            
            pos += 4 + oob_len;
            continue;
        }
        
        /* Parse flux data */
        uint32_t flux_value = 0;
        
        if (byte <= 0x07) {
            /* 2-byte flux */
            if (pos + 1 >= size) break;
            flux_value = (byte << 8) | data[pos + 1];
            pos += 2;
        } else if (byte == 0x08) {
            /* 3-byte flux (NOP) */
            if (pos + 2 >= size) break;
            flux_value = (data[pos + 1] << 8) | data[pos + 2];
            pos += 3;
        } else if (byte >= 0x09 && byte <= 0x0B) {
            /* NOP bytes */
            pos += (byte - 0x08);
            continue;
        } else if (byte == 0x0C) {
            /* Overflow */
            overflow += 0x10000;
            pos++;
            continue;
        } else if (byte >= 0x0E) {
            /* 1-byte flux */
            flux_value = byte;
            pos++;
        } else {
            pos++;
            continue;
        }
        
        /* Add flux value */
        flux_value += overflow;
        overflow = 0;
        
        if (stream->flux_count >= stream->flux_capacity) {
            size_t new_cap = stream->flux_capacity * 2;
            uint32_t *new_times = realloc(stream->flux_times, 
                                          new_cap * sizeof(uint32_t));
            if (!new_times) break;
            
            stream->flux_times = new_times;
            stream->flux_capacity = new_cap;
        }
        
        stream->flux_times[stream->flux_count++] = flux_value;
        stream_pos++;
    }
    
    return 0;
}

void uft_kf_enhanced_close(uft_kf_enhanced_t *stream)
{
    if (!stream) return;
    
    free(stream->flux_times);
    free(stream->indices);
    memset(stream, 0, sizeof(*stream));
}

/*===========================================================================
 * Multi-Revolution Analysis
 *===========================================================================*/

int uft_kf_get_revolution_data(const uft_kf_enhanced_t *stream, int rev,
                               uint32_t **flux, size_t *count,
                               double *rpm)
{
    if (!stream || !flux || !count) return -1;
    if (rev < 0 || (size_t)rev >= stream->index_count) return -1;
    
    size_t start, end;
    
    if (rev == 0) {
        start = 0;
    } else {
        start = stream->indices[rev - 1].flux_offset;
    }
    
    if ((size_t)rev < stream->index_count) {
        end = stream->indices[rev].flux_offset;
    } else {
        end = stream->flux_count;
    }
    
    *flux = stream->flux_times + start;
    *count = end - start;
    
    /* Calculate RPM */
    if (rpm && stream->index_count >= 2 && rev < (int)stream->index_count - 1) {
        uint32_t idx_diff = stream->indices[rev + 1].index_counter - 
                           stream->indices[rev].index_counter;
        double seconds = (double)idx_diff / stream->index_clock;
        *rpm = 60.0 / seconds;
    }
    
    return 0;
}

/*===========================================================================
 * Weak Bit Detection
 *===========================================================================*/

int uft_kf_detect_weak_bits(const uft_kf_enhanced_t *stream,
                            uft_kf_weak_info_t *weak)
{
    if (!stream || !weak || stream->index_count < 2) return -1;
    
    memset(weak, 0, sizeof(*weak));
    
    /* Compare adjacent revolutions */
    for (size_t r = 0; r < stream->index_count - 1; r++) {
        size_t start1 = (r == 0) ? 0 : stream->indices[r - 1].flux_offset;
        size_t end1 = stream->indices[r].flux_offset;
        
        size_t start2 = stream->indices[r].flux_offset;
        size_t end2 = stream->indices[r + 1].flux_offset;
        
        size_t len1 = end1 - start1;
        size_t len2 = end2 - start2;
        size_t min_len = (len1 < len2) ? len1 : len2;
        
        for (size_t i = 1; i < min_len; i++) {
            uint32_t d1 = stream->flux_times[start1 + i] - 
                         stream->flux_times[start1 + i - 1];
            uint32_t d2 = stream->flux_times[start2 + i] - 
                         stream->flux_times[start2 + i - 1];
            
            double diff = (d1 > d2) ? d1 - d2 : d2 - d1;
            double avg = (d1 + d2) / 2.0;
            
            if (diff / avg > 0.15) {  /* 15% threshold */
                weak->weak_count++;
                
                if (weak->position_count < 256) {
                    weak->positions[weak->position_count++] = start1 + i;
                }
            }
        }
    }
    
    weak->has_weak_bits = (weak->weak_count > 10);
    
    if (stream->flux_count > 0) {
        weak->weak_ratio = (double)weak->weak_count / stream->flux_count;
    }
    
    return 0;
}

/*===========================================================================
 * Quality Metrics
 *===========================================================================*/

int uft_kf_quality_metrics(const uft_kf_enhanced_t *stream,
                           uft_kf_quality_t *quality)
{
    if (!stream || !quality) return -1;
    
    memset(quality, 0, sizeof(*quality));
    
    if (stream->flux_count < 100) {
        quality->overall = 0;
        return 0;
    }
    
    /* Calculate timing statistics */
    double sum = 0;
    uint32_t min_val = UINT32_MAX;
    uint32_t max_val = 0;
    
    for (size_t i = 1; i < stream->flux_count; i++) {
        uint32_t duration = stream->flux_times[i] - stream->flux_times[i - 1];
        sum += duration;
        if (duration < min_val) min_val = duration;
        if (duration > max_val) max_val = duration;
    }
    
    double mean = sum / (stream->flux_count - 1);
    
    /* Variance */
    double variance = 0;
    for (size_t i = 1; i < stream->flux_count; i++) {
        uint32_t duration = stream->flux_times[i] - stream->flux_times[i - 1];
        double diff = duration - mean;
        variance += diff * diff;
    }
    variance /= (stream->flux_count - 2);
    
    double std_dev = sqrt(variance);
    double cv = std_dev / mean;  /* Coefficient of variation */
    
    quality->timing_consistency = 100.0 * (1.0 - cv);
    if (quality->timing_consistency < 0) quality->timing_consistency = 0;
    
    /* Revolution consistency */
    if (stream->index_count >= 2) {
        double rpm_sum = 0;
        double rpm_min = 1e10;
        double rpm_max = 0;
        
        for (size_t r = 0; r < stream->index_count - 1; r++) {
            uint32_t idx_diff = stream->indices[r + 1].index_counter - 
                               stream->indices[r].index_counter;
            double rpm = 60.0 * stream->index_clock / idx_diff;
            
            rpm_sum += rpm;
            if (rpm < rpm_min) rpm_min = rpm;
            if (rpm > rpm_max) rpm_max = rpm;
        }
        
        double rpm_mean = rpm_sum / (stream->index_count - 1);
        double rpm_range = rpm_max - rpm_min;
        
        quality->revolution_consistency = 100.0 * (1.0 - rpm_range / rpm_mean);
        if (quality->revolution_consistency < 0) quality->revolution_consistency = 0;
    } else {
        quality->revolution_consistency = 50.0;  /* Unknown */
    }
    
    /* Weak bit impact */
    uft_kf_weak_info_t weak;
    uft_kf_detect_weak_bits(stream, &weak);
    
    quality->weak_bit_score = 100.0 * (1.0 - weak.weak_ratio * 10);
    if (quality->weak_bit_score < 0) quality->weak_bit_score = 0;
    
    /* Overall score */
    quality->overall = (quality->timing_consistency * 0.4 +
                       quality->revolution_consistency * 0.3 +
                       quality->weak_bit_score * 0.3);
    
    /* Grade */
    if (quality->overall >= QUALITY_EXCELLENT) {
        quality->grade = 'A';
    } else if (quality->overall >= QUALITY_GOOD) {
        quality->grade = 'B';
    } else if (quality->overall >= QUALITY_FAIR) {
        quality->grade = 'C';
    } else if (quality->overall >= QUALITY_POOR) {
        quality->grade = 'D';
    } else {
        quality->grade = 'F';
    }
    
    return 0;
}

/*===========================================================================
 * CTool Integration
 *===========================================================================*/

int uft_kf_ctool_read_track(const char *device, int track, int side,
                            uft_kf_enhanced_t *stream)
{
    if (!device || !stream) return -1;
    
    /* Build CTool command */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "dtc -f\"%s\" -i0 -t%d:%d -m1 -fdtemp.raw",
             device, track, side);
    
    /* Execute (placeholder - actual implementation would use system/popen) */
    (void)cmd;
    
    /* For now, return not implemented */
    return -1;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_kf_enhanced_report(const uft_kf_enhanced_t *stream,
                           char *buffer, size_t size)
{
    if (!stream || !buffer) return -1;
    
    uft_kf_quality_t quality;
    uft_kf_quality_metrics(stream, &quality);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"kryoflux_enhanced\": {\n"
        "    \"flux_count\": %u,\n"
        "    \"index_count\": %zu,\n"
        "    \"sample_clock\": %.0f,\n"
        "    \"hw_name\": \"%s\",\n"
        "    \"quality\": {\n"
        "      \"overall\": %.1f,\n"
        "      \"timing\": %.1f,\n"
        "      \"revolution\": %.1f,\n"
        "      \"weak_bits\": %.1f,\n"
        "      \"grade\": \"%c\"\n"
        "    }\n"
        "  }\n"
        "}",
        stream->flux_count,
        stream->index_count,
        stream->sample_clock,
        stream->hw_name,
        quality.overall,
        quality.timing_consistency,
        quality.revolution_consistency,
        quality.weak_bit_score,
        quality.grade
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
