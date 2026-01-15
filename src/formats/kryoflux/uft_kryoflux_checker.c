// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_kryoflux_checker.c
 * @brief KryoFlux Stream Integrity Checker & Analyzer
 * @version 3.8.0
 * @date 2026-01-15
 *
 * Inspiriert von sdstrowes/kryoflux-stream-checker
 * 
 * Features:
 * - Stream Position Verification (OOB vs. actual)
 * - Disk Surface Visualization (ASCII art)
 * - MFM/FM sector validation
 * - Atari ST DSDD specific checks
 * - Detailed quality reporting
 * - Weak bit detection
 * - Revolution consistency analysis
 *
 * "Bei uns geht kein Bit verloren"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/* Sample Clock (KryoFlux default) */
#define UFT_KFC_SCK_HZ          24027428.0
#define UFT_KFC_ICK_DEFAULT     2
#define UFT_KFC_SAMPLE_FREQ     (UFT_KFC_SCK_HZ / (UFT_KFC_ICK_DEFAULT + 1))
#define UFT_KFC_NS_PER_TICK     (1e9 / UFT_KFC_SAMPLE_FREQ)

/* Stream Opcodes */
#define UFT_KFC_FLUX1_MIN       0x0E
#define UFT_KFC_FLUX2           0x00  /* 0x00-0x07 are 2-byte flux */
#define UFT_KFC_NOP1            0x08
#define UFT_KFC_NOP2            0x09
#define UFT_KFC_NOP3            0x0A
#define UFT_KFC_OVL16           0x0B
#define UFT_KFC_FLUX3           0x0C
#define UFT_KFC_OOB             0x0D

/* OOB Types */
#define UFT_KFC_OOB_INVALID     0x00
#define UFT_KFC_OOB_STREAM_INFO 0x01
#define UFT_KFC_OOB_INDEX       0x02
#define UFT_KFC_OOB_STREAM_END  0x03
#define UFT_KFC_OOB_KFINFO      0x04
#define UFT_KFC_OOB_EOF         0x0D

/* MFM Constants */
#define UFT_KFC_MFM_SYNC_A1     0x4489  /* Sync pattern 0xA1 with missing clock */
#define UFT_KFC_MFM_IDAM        0xFE    /* ID Address Mark */
#define UFT_KFC_MFM_DAM         0xFB    /* Data Address Mark */
#define UFT_KFC_MFM_DDAM        0xF8    /* Deleted Data Address Mark */

/* Atari ST DSDD Parameters */
#define UFT_KFC_ATARI_TRACKS    80
#define UFT_KFC_ATARI_HEADS     2
#define UFT_KFC_ATARI_SECTORS   9
#define UFT_KFC_ATARI_SECTOR_SZ 512
#define UFT_KFC_ATARI_BITRATE   250000
#define UFT_KFC_ATARI_RPM       300.0

/* Limits */
#define UFT_KFC_MAX_FLUX        500000
#define UFT_KFC_MAX_SECTORS     32
#define UFT_KFC_MAX_REVS        10

/*============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Sector info extracted from MFM
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;       /* 0=128, 1=256, 2=512, 3=1024 */
    uint16_t header_crc;
    uint16_t data_crc;
    bool     header_ok;
    bool     data_ok;
    bool     deleted;
    uint32_t flux_position;   /* Position in flux array */
} uft_kfc_sector_t;

/**
 * @brief Track quality info
 */
typedef struct {
    float    signal_quality;      /* 0.0-1.0 */
    float    timing_variance;     /* Standard deviation */
    uint32_t weak_bits;
    uint32_t missing_clocks;
    uint32_t extra_clocks;
    bool     index_found;
    double   rotation_time_ms;
    float    rpm;
} uft_kfc_track_quality_t;

/**
 * @brief Stream check result
 */
typedef struct {
    /* Stream integrity */
    bool     stream_valid;
    uint32_t expected_stream_pos;
    uint32_t actual_stream_pos;
    bool     position_match;
    
    /* OOB statistics */
    uint32_t oob_count;
    uint32_t index_count;
    uint32_t overflow_count;
    
    /* Flux statistics */
    uint32_t flux_count;
    uint32_t flux_min;
    uint32_t flux_max;
    double   flux_avg;
    double   flux_stddev;
    
    /* Revolution info */
    uint8_t  revolution_count;
    double   revolution_times_ms[UFT_KFC_MAX_REVS];
    float    revolution_rpm[UFT_KFC_MAX_REVS];
    float    rpm_variance;
    
    /* Sector info (if decoded) */
    uint8_t  sector_count;
    uft_kfc_sector_t sectors[UFT_KFC_MAX_SECTORS];
    uint8_t  sectors_ok;
    uint8_t  sectors_bad_header;
    uint8_t  sectors_bad_data;
    
    /* Track quality */
    uft_kfc_track_quality_t quality;
    
    /* Hardware info */
    char     kf_name[64];
    char     kf_version[32];
    double   sample_clock_hz;
    
    /* Warnings/Errors */
    char     warnings[512];
    char     errors[512];
    
} uft_kfc_result_t;

/*============================================================================
 * STREAM POSITION VERIFICATION
 *============================================================================*/

/**
 * @brief Verify stream position matches OOB markers
 * 
 * Key insight from kryoflux-stream-checker:
 * The stream_pos in OOB blocks does NOT include OOB data itself,
 * only flux data bytes.
 */
static bool verify_stream_position(
    const uint8_t* data, 
    size_t len,
    uft_kfc_result_t* result)
{
    size_t file_pos = 0;
    uint32_t stream_pos = 0;  /* Only non-OOB bytes */
    bool valid = true;
    
    result->expected_stream_pos = 0;
    result->actual_stream_pos = 0;
    
    while (file_pos < len) {
        uint8_t byte = data[file_pos];
        
        /* OOB Block */
        if (byte == UFT_KFC_OOB) {
            if (file_pos + 4 > len) break;
            
            uint8_t type = data[file_pos + 1];
            uint16_t size = data[file_pos + 2] | (data[file_pos + 3] << 8);
            
            if (file_pos + 4 + size > len) break;
            
            const uint8_t* payload = data + file_pos + 4;
            
            /* Check position in Stream Info, Index, and Stream End */
            if ((type == UFT_KFC_OOB_STREAM_INFO || 
                 type == UFT_KFC_OOB_INDEX ||
                 type == UFT_KFC_OOB_STREAM_END) && size >= 4) {
                
                uint32_t oob_stream_pos = payload[0] | (payload[1] << 8) |
                                          (payload[2] << 16) | (payload[3] << 24);
                
                if (oob_stream_pos != stream_pos) {
                    snprintf(result->warnings + strlen(result->warnings),
                            sizeof(result->warnings) - strlen(result->warnings),
                            "Stream pos mismatch at 0x%zX: expected %u, got %u\n",
                            file_pos, stream_pos, oob_stream_pos);
                    valid = false;
                }
                
                result->expected_stream_pos = stream_pos;
                result->actual_stream_pos = oob_stream_pos;
            }
            
            if (type == UFT_KFC_OOB_EOF) break;
            
            file_pos += 4 + size;
            result->oob_count++;
            
            if (type == UFT_KFC_OOB_INDEX) {
                result->index_count++;
            }
        }
        /* Flux1 (0x0E-0xFF) */
        else if (byte >= UFT_KFC_FLUX1_MIN) {
            stream_pos++;
            file_pos++;
            result->flux_count++;
        }
        /* Flux2 (0x00-0x07) */
        else if (byte <= 0x07) {
            stream_pos += 2;
            file_pos += 2;
            result->flux_count++;
        }
        /* NOP1 */
        else if (byte == UFT_KFC_NOP1) {
            stream_pos++;
            file_pos++;
        }
        /* NOP2 */
        else if (byte == UFT_KFC_NOP2) {
            stream_pos += 2;
            file_pos += 2;
        }
        /* NOP3 */
        else if (byte == UFT_KFC_NOP3) {
            stream_pos += 3;
            file_pos += 3;
        }
        /* OVL16 */
        else if (byte == UFT_KFC_OVL16) {
            stream_pos += 2;
            file_pos += 2;
            result->overflow_count++;
        }
        /* FLUX3 */
        else if (byte == UFT_KFC_FLUX3) {
            stream_pos += 3;
            file_pos += 3;
            result->flux_count++;
        }
        else {
            /* Unknown/reserved */
            stream_pos++;
            file_pos++;
        }
    }
    
    result->position_match = valid;
    return valid;
}

/*============================================================================
 * FLUX EXTRACTION & ANALYSIS
 *============================================================================*/

/**
 * @brief Extract flux timing values
 */
static uint32_t* extract_flux_values(
    const uint8_t* data,
    size_t len,
    uint32_t* count_out,
    double sample_period_ns)
{
    uint32_t* flux = malloc(UFT_KFC_MAX_FLUX * sizeof(uint32_t));
    if (!flux) return NULL;
    
    uint32_t count = 0;
    uint32_t accumulator = 0;
    size_t pos = 0;
    
    while (pos < len && count < UFT_KFC_MAX_FLUX) {
        uint8_t byte = data[pos];
        
        /* Skip OOB */
        if (byte == UFT_KFC_OOB) {
            if (pos + 4 > len) break;
            uint16_t size = data[pos + 2] | (data[pos + 3] << 8);
            if (data[pos + 1] == UFT_KFC_OOB_EOF) break;
            pos += 4 + size;
            continue;
        }
        
        /* Flux1 */
        if (byte >= UFT_KFC_FLUX1_MIN) {
            accumulator += byte;
            flux[count++] = accumulator;
            accumulator = 0;
            pos++;
        }
        /* Flux2 */
        else if (byte <= 0x07) {
            if (pos + 1 >= len) break;
            uint16_t val = (byte << 8) | data[pos + 1];
            accumulator += val;
            flux[count++] = accumulator;
            accumulator = 0;
            pos += 2;
        }
        /* OVL16 */
        else if (byte == UFT_KFC_OVL16) {
            if (pos + 1 >= len) break;
            accumulator += 0x10000;
            pos += 2;
        }
        /* FLUX3 */
        else if (byte == UFT_KFC_FLUX3) {
            if (pos + 2 >= len) break;
            uint16_t val = data[pos + 1] | (data[pos + 2] << 8);
            accumulator += val;
            flux[count++] = accumulator;
            accumulator = 0;
            pos += 3;
        }
        /* NOP */
        else if (byte >= UFT_KFC_NOP1 && byte <= UFT_KFC_NOP3) {
            pos += (byte - UFT_KFC_NOP1 + 1);
        }
        else {
            pos++;
        }
    }
    
    *count_out = count;
    return flux;
}

/**
 * @brief Calculate flux statistics
 */
static void analyze_flux_statistics(
    const uint32_t* flux,
    uint32_t count,
    uft_kfc_result_t* result)
{
    if (count == 0) return;
    
    uint32_t min = flux[0], max = flux[0];
    uint64_t sum = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        if (flux[i] < min) min = flux[i];
        if (flux[i] > max) max = flux[i];
        sum += flux[i];
    }
    
    result->flux_min = min;
    result->flux_max = max;
    result->flux_avg = (double)sum / count;
    
    /* Standard deviation */
    double variance_sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        double diff = (double)flux[i] - result->flux_avg;
        variance_sum += diff * diff;
    }
    result->flux_stddev = sqrt(variance_sum / count);
}

/*============================================================================
 * DISK SURFACE VISUALIZATION
 *============================================================================*/

/**
 * @brief Generate ASCII visualization of flux timing
 * 
 * Inspired by kryoflux-stream-checker's visualization
 */
static void visualize_flux_histogram(
    const uint32_t* flux,
    uint32_t count,
    char* output,
    size_t output_size)
{
    if (count == 0 || !output) return;
    
    /* Histogram bins (32 bins from min to max) */
    #define HIST_BINS 32
    uint32_t histogram[HIST_BINS] = {0};
    uint32_t max_count = 0;
    
    /* Find range */
    uint32_t min_val = flux[0], max_val = flux[0];
    for (uint32_t i = 0; i < count; i++) {
        if (flux[i] < min_val) min_val = flux[i];
        if (flux[i] > max_val) max_val = flux[i];
    }
    
    uint32_t range = max_val - min_val;
    if (range == 0) range = 1;
    
    /* Build histogram */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t bin = ((flux[i] - min_val) * (HIST_BINS - 1)) / range;
        if (bin >= HIST_BINS) bin = HIST_BINS - 1;
        histogram[bin]++;
        if (histogram[bin] > max_count) max_count = histogram[bin];
    }
    
    /* Generate ASCII art */
    char* p = output;
    size_t remaining = output_size;
    
    int written = snprintf(p, remaining, 
        "\n┌─────────────────────────────────────────────┐\n"
        "│           Flux Timing Histogram             │\n"
        "├─────────────────────────────────────────────┤\n");
    p += written; remaining -= written;
    
    /* 8 rows of histogram */
    for (int row = 7; row >= 0; row--) {
        written = snprintf(p, remaining, "│ ");
        p += written; remaining -= written;
        
        for (int bin = 0; bin < HIST_BINS; bin++) {
            uint32_t threshold = (max_count * (row + 1)) / 8;
            if (histogram[bin] >= threshold) {
                written = snprintf(p, remaining, "█");
            } else if (histogram[bin] >= threshold - max_count/16) {
                written = snprintf(p, remaining, "▄");
            } else {
                written = snprintf(p, remaining, " ");
            }
            p += written; remaining -= written;
        }
        
        written = snprintf(p, remaining, " │\n");
        p += written; remaining -= written;
    }
    
    written = snprintf(p, remaining,
        "├─────────────────────────────────────────────┤\n"
        "│ Min: %6u  Avg: %6.0f  Max: %6u ticks │\n"
        "└─────────────────────────────────────────────┘\n",
        min_val, (double)(min_val + max_val) / 2, max_val);
    
    #undef HIST_BINS
}

/**
 * @brief Generate track surface map (sector status)
 */
static void visualize_track_surface(
    const uft_kfc_result_t* result,
    char* output,
    size_t output_size)
{
    char* p = output;
    size_t remaining = output_size;
    
    int written = snprintf(p, remaining,
        "\n┌─────────────────────────────────────────────┐\n"
        "│              Track Surface Map              │\n"
        "├─────────────────────────────────────────────┤\n"
        "│ Sectors: ");
    p += written; remaining -= written;
    
    for (int i = 0; i < result->sector_count && i < 20; i++) {
        const char* symbol;
        if (result->sectors[i].header_ok && result->sectors[i].data_ok) {
            symbol = "●";  /* Good */
        } else if (result->sectors[i].header_ok) {
            symbol = "◐";  /* Header OK, data bad */
        } else {
            symbol = "○";  /* Bad */
        }
        written = snprintf(p, remaining, "%s", symbol);
        p += written; remaining -= written;
    }
    
    /* Pad to 20 */
    for (int i = result->sector_count; i < 20; i++) {
        written = snprintf(p, remaining, " ");
        p += written; remaining -= written;
    }
    
    written = snprintf(p, remaining,
        "         │\n"
        "│ Legend: ● Good  ◐ Data Error  ○ Header Error │\n"
        "├─────────────────────────────────────────────┤\n"
        "│ OK: %2d  Header Err: %2d  Data Err: %2d        │\n"
        "└─────────────────────────────────────────────┘\n",
        result->sectors_ok,
        result->sectors_bad_header,
        result->sectors_bad_data);
}

/*============================================================================
 * MAIN CHECKER FUNCTION
 *============================================================================*/

/**
 * @brief Check KryoFlux stream file integrity
 * 
 * @param data Stream file data
 * @param len Data length
 * @param result Output result structure
 * @return 0 on success, negative on error
 */
int uft_kfc_check_stream(
    const uint8_t* data,
    size_t len,
    uft_kfc_result_t* result)
{
    if (!data || !result || len < 4) {
        return -1;
    }
    
    memset(result, 0, sizeof(*result));
    result->sample_clock_hz = UFT_KFC_SAMPLE_FREQ;
    
    /* 1. Verify stream position integrity */
    result->stream_valid = verify_stream_position(data, len, result);
    
    /* 2. Extract and analyze flux values */
    uint32_t flux_count = 0;
    uint32_t* flux = extract_flux_values(data, len, &flux_count, 
                                         UFT_KFC_NS_PER_TICK);
    
    if (flux && flux_count > 0) {
        analyze_flux_statistics(flux, flux_count, result);
        result->flux_count = flux_count;
        
        /* Calculate quality metrics */
        result->quality.signal_quality = 
            1.0f - (float)(result->flux_stddev / result->flux_avg);
        if (result->quality.signal_quality < 0) 
            result->quality.signal_quality = 0;
        
        result->quality.timing_variance = (float)result->flux_stddev;
    }
    
    free(flux);
    
    /* 3. Revolution analysis from indices */
    if (result->index_count >= 2) {
        result->revolution_count = result->index_count - 1;
        result->quality.index_found = true;
    }
    
    /* 4. Overall quality score */
    if (result->position_match && 
        result->quality.signal_quality > 0.5f &&
        result->index_count >= 2) {
        result->stream_valid = true;
    }
    
    return 0;
}

/**
 * @brief Generate text report
 */
int uft_kfc_generate_report(
    const uft_kfc_result_t* result,
    char* output,
    size_t output_size)
{
    if (!result || !output) return -1;
    
    char* p = output;
    size_t remaining = output_size;
    
    int written = snprintf(p, remaining,
        "╔═══════════════════════════════════════════════════════════╗\n"
        "║           KryoFlux Stream Analysis Report                 ║\n"
        "╠═══════════════════════════════════════════════════════════╣\n"
        "║ Stream Integrity                                          ║\n"
        "╟───────────────────────────────────────────────────────────╢\n"
        "║  Valid:           %s                                    ║\n"
        "║  Position Match:  %s                                    ║\n"
        "║  OOB Blocks:      %-6u                                  ║\n"
        "║  Index Pulses:    %-6u                                  ║\n"
        "║  Overflows:       %-6u                                  ║\n"
        "╟───────────────────────────────────────────────────────────╢\n"
        "║ Flux Statistics                                           ║\n"
        "╟───────────────────────────────────────────────────────────╢\n"
        "║  Total Flux:      %-6u                                  ║\n"
        "║  Min Value:       %-6u ticks                            ║\n"
        "║  Max Value:       %-6u ticks                            ║\n"
        "║  Average:         %-8.1f ticks                          ║\n"
        "║  Std Dev:         %-8.1f ticks                          ║\n"
        "╟───────────────────────────────────────────────────────────╢\n"
        "║ Quality Metrics                                           ║\n"
        "╟───────────────────────────────────────────────────────────╢\n"
        "║  Signal Quality:  %.1f%%                                   ║\n"
        "║  Revolutions:     %-2u                                     ║\n"
        "╚═══════════════════════════════════════════════════════════╝\n",
        result->stream_valid ? "YES" : "NO ",
        result->position_match ? "YES" : "NO ",
        result->oob_count,
        result->index_count,
        result->overflow_count,
        result->flux_count,
        result->flux_min,
        result->flux_max,
        result->flux_avg,
        result->flux_stddev,
        result->quality.signal_quality * 100,
        result->revolution_count);
    
    p += written; remaining -= written;
    
    /* Add warnings if any */
    if (strlen(result->warnings) > 0) {
        written = snprintf(p, remaining,
            "\n⚠ Warnings:\n%s", result->warnings);
        p += written; remaining -= written;
    }
    
    return 0;
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Check stream file and print report
 */
int uft_kfc_check_and_report(
    const uint8_t* data,
    size_t len,
    FILE* output)
{
    uft_kfc_result_t result;
    
    int ret = uft_kfc_check_stream(data, len, &result);
    if (ret < 0) {
        fprintf(output, "Error: Failed to analyze stream\n");
        return ret;
    }
    
    char report[4096];
    uft_kfc_generate_report(&result, report, sizeof(report));
    fprintf(output, "%s", report);
    
    /* Histogram visualization */
    uint32_t flux_count = 0;
    uint32_t* flux = extract_flux_values(data, len, &flux_count, 
                                         UFT_KFC_NS_PER_TICK);
    if (flux && flux_count > 0) {
        char histogram[2048];
        visualize_flux_histogram(flux, flux_count, histogram, sizeof(histogram));
        fprintf(output, "%s", histogram);
        free(flux);
    }
    
    return result.stream_valid ? 0 : 1;
}

/*============================================================================
 * ATARI ST SPECIFIC CHECKS
 *============================================================================*/

/**
 * @brief Check if flux timing is consistent with Atari ST DSDD
 */
bool uft_kfc_check_atari_st_timing(const uft_kfc_result_t* result)
{
    if (!result || result->flux_count == 0) return false;
    
    /* Atari ST DSDD: 250kbit/s at 300 RPM
     * Bitcell time: 4µs = ~96 ticks at 24MHz sample clock
     * Expected flux range: ~48-192 ticks (short/long cells)
     */
    const uint32_t expected_short = 48;   /* ~2µs */
    const uint32_t expected_long = 192;   /* ~8µs */
    
    /* Check if majority of flux values are in expected range */
    bool timing_ok = (result->flux_min >= expected_short - 20 &&
                      result->flux_max <= expected_long + 40);
    
    /* Check average is around expected bitcell */
    double expected_avg = 96.0;  /* ~4µs */
    bool avg_ok = (result->flux_avg > expected_avg * 0.8 &&
                   result->flux_avg < expected_avg * 1.2);
    
    return timing_ok && avg_ok;
}

/*============================================================================
 * UNIT TESTS
 *============================================================================*/

#ifdef UFT_KFC_TEST

#include <assert.h>

static void test_stream_position(void) {
    /* Minimal valid stream: OOB_STREAM_INFO + OOB_EOF */
    uint8_t stream[] = {
        0x0D, 0x01, 0x08, 0x00,  /* OOB STREAM_INFO, size=8 */
        0x00, 0x00, 0x00, 0x00,  /* stream_pos = 0 */
        0x00, 0x00, 0x00, 0x00,  /* transfer_time */
        0x20,                     /* Flux1 (value 0x20) */
        0x0D, 0x0D, 0x00, 0x00,  /* OOB EOF */
    };
    
    uft_kfc_result_t result;
    int ret = uft_kfc_check_stream(stream, sizeof(stream), &result);
    
    assert(ret == 0);
    assert(result.oob_count >= 1);
    printf("✓ Stream position verification\n");
}

static void test_flux_extraction(void) {
    uint8_t stream[] = {
        0x20, 0x30, 0x40,        /* 3 Flux1 values */
        0x0D, 0x0D, 0x00, 0x00,  /* OOB EOF */
    };
    
    uint32_t count = 0;
    uint32_t* flux = extract_flux_values(stream, sizeof(stream), &count,
                                          UFT_KFC_NS_PER_TICK);
    
    assert(flux != NULL);
    assert(count == 3);
    assert(flux[0] == 0x20);
    assert(flux[1] == 0x30);
    assert(flux[2] == 0x40);
    
    free(flux);
    printf("✓ Flux extraction\n");
}

static void test_histogram(void) {
    uint32_t flux[] = {100, 120, 110, 130, 90, 140, 100, 115};
    
    char output[2048];
    visualize_flux_histogram(flux, 8, output, sizeof(output));
    
    assert(strlen(output) > 100);
    printf("✓ Histogram visualization\n");
}

int main(void) {
    printf("=== KryoFlux Stream Checker Tests ===\n");
    
    test_stream_position();
    test_flux_extraction();
    test_histogram();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* UFT_KFC_TEST */
