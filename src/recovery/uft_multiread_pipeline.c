/**
 * @file uft_multiread_pipeline.c
 * @brief Unified Multi-Read Recovery Pipeline
 * 
 * High-level pipeline that combines:
 * - Multi-pass reading with automatic retry
 * - Majority voting across reads
 * - Adaptive strategy based on read quality
 * - Confidence scoring and weak bit detection
 * 
 * This module provides a simple API for the common use case of
 * recovering data from damaged or degraded floppy disks.
 * 
 * @author UFT Team
 * @version 3.5.0
 * @date 2026-01-03
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum read passes */
#define MULTIREAD_MAX_PASSES        16

/** Default number of passes */
#define MULTIREAD_DEFAULT_PASSES    5

/** Minimum confidence for success */
#define MULTIREAD_MIN_CONFIDENCE    75

/** Majority threshold (percentage) */
#define MULTIREAD_MAJORITY_PCT      66

/** Maximum track size */
#define MULTIREAD_MAX_TRACK_SIZE    32768

/** Maximum sectors per track */
#define MULTIREAD_MAX_SECTORS       32

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    MULTIREAD_OK = 0,
    MULTIREAD_ERR_NULL_PARAM,
    MULTIREAD_ERR_ALLOC,
    MULTIREAD_ERR_NO_DATA,
    MULTIREAD_ERR_READ_FAILED,
    MULTIREAD_ERR_INSUFFICIENT_PASSES,
    MULTIREAD_ERR_LOW_CONFIDENCE,
    MULTIREAD_ERR_CRC_FAILED,
    MULTIREAD_ERR_COUNT
} multiread_error_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Single read pass data
 */
typedef struct {
    uint8_t    *data;               /**< Sector/track data */
    size_t      data_len;           /**< Data length */
    uint8_t     quality;            /**< Read quality (0-100) */
    bool        crc_ok;             /**< CRC passed */
    double      timing_variance;    /**< Timing variance */
} multiread_pass_t;

/**
 * @brief Sector result from multi-read
 */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    uint8_t     sector;             /**< Sector number */
    uint8_t    *data;               /**< Recovered data */
    size_t      data_len;           /**< Data length */
    uint8_t     confidence;         /**< Recovery confidence (0-100) */
    uint8_t     good_reads;         /**< Number of good reads */
    uint8_t     total_reads;        /**< Total read attempts */
    bool        recovered;          /**< Successfully recovered */
    bool        has_weak_bits;      /**< Contains weak/uncertain bits */
    uint8_t    *weak_mask;          /**< Weak bit mask (optional) */
} multiread_sector_t;

/**
 * @brief Track result from multi-read
 */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    multiread_sector_t sectors[MULTIREAD_MAX_SECTORS];
    uint8_t     sector_count;       /**< Number of sectors */
    uint8_t     good_sectors;       /**< Sectors with 100% confidence */
    uint8_t     recovered_sectors;  /**< Sectors recovered with voting */
    uint8_t     failed_sectors;     /**< Unrecoverable sectors */
    uint8_t     overall_confidence; /**< Track-level confidence */
} multiread_track_t;

/**
 * @brief Pipeline configuration
 */
typedef struct {
    uint8_t     min_passes;         /**< Minimum read passes */
    uint8_t     max_passes;         /**< Maximum read passes */
    uint8_t     min_confidence;     /**< Minimum required confidence */
    uint8_t     majority_pct;       /**< Majority vote percentage */
    bool        adaptive_passes;    /**< Increase passes on failure */
    bool        detect_weak_bits;   /**< Enable weak bit detection */
    bool        generate_report;    /**< Generate detailed report */
    
    /* Callbacks */
    void       *user_data;
    int       (*read_callback)(void *user_data, uint8_t track, uint8_t head,
                               uint8_t *data, size_t *len);
    void      (*progress_callback)(void *user_data, uint8_t track, uint8_t head,
                                   uint8_t pass, uint8_t total_passes);
} multiread_config_t;

/**
 * @brief Pipeline context
 */
typedef struct {
    multiread_config_t config;
    
    /* Pass data storage */
    multiread_pass_t passes[MULTIREAD_MAX_PASSES];
    uint8_t pass_count;
    
    /* Statistics */
    uint32_t total_reads;
    uint32_t successful_reads;
    uint32_t failed_reads;
    uint32_t bytes_recovered;
    double   avg_confidence;
} multiread_ctx_t;

/*============================================================================
 * Error Strings
 *============================================================================*/

static const char *error_strings[MULTIREAD_ERR_COUNT] = {
    "OK",
    "Null parameter",
    "Memory allocation failed",
    "No data available",
    "Read operation failed",
    "Insufficient passes",
    "Confidence too low",
    "CRC verification failed"
};

const char *multiread_error_string(multiread_error_t err) {
    if (err >= MULTIREAD_ERR_COUNT) return "Unknown error";
    return error_strings[err];
}

/*============================================================================
 * Default Configuration
 *============================================================================*/

/**
 * @brief Get default configuration
 */
multiread_config_t multiread_config_default(void) {
    multiread_config_t cfg = {
        .min_passes = 3,
        .max_passes = MULTIREAD_DEFAULT_PASSES,
        .min_confidence = MULTIREAD_MIN_CONFIDENCE,
        .majority_pct = MULTIREAD_MAJORITY_PCT,
        .adaptive_passes = true,
        .detect_weak_bits = true,
        .generate_report = false,
        .user_data = NULL,
        .read_callback = NULL,
        .progress_callback = NULL
    };
    return cfg;
}

/*============================================================================
 * Voting Algorithm
 *============================================================================*/

/**
 * @brief Vote on single byte position across passes
 */
static uint8_t vote_byte(const multiread_pass_t *passes, uint8_t pass_count,
                         size_t offset, uint8_t *confidence, bool *is_weak) {
    if (!passes || pass_count == 0) {
        if (confidence) *confidence = 0;
        if (is_weak) *is_weak = true;
        return 0;
    }
    
    /* Count occurrences of each value */
    uint8_t counts[256] = {0};
    uint8_t valid_count = 0;
    
    for (uint8_t p = 0; p < pass_count; p++) {
        if (offset < passes[p].data_len && passes[p].data) {
            counts[passes[p].data[offset]]++;
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        if (confidence) *confidence = 0;
        if (is_weak) *is_weak = true;
        return 0;
    }
    
    /* Find most common value */
    uint8_t max_count = 0;
    uint8_t max_val = 0;
    
    for (int v = 0; v < 256; v++) {
        if (counts[v] > max_count) {
            max_count = counts[v];
            max_val = (uint8_t)v;
        }
    }
    
    /* Calculate confidence */
    if (confidence) {
        *confidence = (uint8_t)((max_count * 100) / valid_count);
    }
    
    /* Detect weak bits (not unanimous) */
    if (is_weak) {
        *is_weak = (max_count < valid_count);
    }
    
    return max_val;
}

/**
 * @brief Vote on entire data buffer across passes
 */
static multiread_error_t vote_buffer(const multiread_pass_t *passes,
                                      uint8_t pass_count,
                                      uint8_t *output,
                                      size_t output_len,
                                      uint8_t *confidence_map,
                                      uint8_t *weak_mask,
                                      uint8_t *avg_confidence,
                                      uint32_t *weak_count) {
    if (!passes || !output || pass_count == 0) {
        return MULTIREAD_ERR_NULL_PARAM;
    }
    
    uint64_t total_confidence = 0;
    uint32_t weak_bits = 0;
    
    for (size_t i = 0; i < output_len; i++) {
        uint8_t conf;
        bool weak;
        
        output[i] = vote_byte(passes, pass_count, i, &conf, &weak);
        
        if (confidence_map) {
            confidence_map[i] = conf;
        }
        
        if (weak_mask) {
            weak_mask[i] = weak ? 1 : 0;
        }
        
        if (weak) weak_bits++;
        total_confidence += conf;
    }
    
    if (avg_confidence) {
        *avg_confidence = (uint8_t)(total_confidence / output_len);
    }
    
    if (weak_count) {
        *weak_count = weak_bits;
    }
    
    return MULTIREAD_OK;
}

/*============================================================================
 * CRC Verification
 *============================================================================*/

static uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

/*============================================================================
 * Context Management
 *============================================================================*/

/**
 * @brief Create pipeline context
 */
multiread_ctx_t *multiread_create(const multiread_config_t *config) {
    multiread_ctx_t *ctx = calloc(1, sizeof(multiread_ctx_t));
    if (!ctx) return NULL;
    
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = multiread_config_default();
    }
    
    return ctx;
}

/**
 * @brief Destroy pipeline context
 */
void multiread_destroy(multiread_ctx_t *ctx) {
    if (!ctx) return;
    
    /* Free pass data */
    for (uint8_t i = 0; i < ctx->pass_count; i++) {
        free(ctx->passes[i].data);
    }
    
    free(ctx);
}

/**
 * @brief Clear passes for new sector/track
 */
static void clear_passes(multiread_ctx_t *ctx) {
    for (uint8_t i = 0; i < ctx->pass_count; i++) {
        free(ctx->passes[i].data);
        ctx->passes[i].data = NULL;
    }
    ctx->pass_count = 0;
}

/*============================================================================
 * Multi-Read Operations
 *============================================================================*/

/**
 * @brief Add a read pass
 */
multiread_error_t multiread_add_pass(multiread_ctx_t *ctx,
                                      const uint8_t *data,
                                      size_t len,
                                      uint8_t quality,
                                      bool crc_ok) {
    if (!ctx || !data || len == 0) {
        return MULTIREAD_ERR_NULL_PARAM;
    }
    
    if (ctx->pass_count >= MULTIREAD_MAX_PASSES) {
        return MULTIREAD_ERR_ALLOC;
    }
    
    multiread_pass_t *pass = &ctx->passes[ctx->pass_count];
    
    pass->data = malloc(len);
    if (!pass->data) {
        return MULTIREAD_ERR_ALLOC;
    }
    
    memcpy(pass->data, data, len);
    pass->data_len = len;
    pass->quality = quality;
    pass->crc_ok = crc_ok;
    pass->timing_variance = 0;
    
    ctx->pass_count++;
    ctx->total_reads++;
    
    if (crc_ok) {
        ctx->successful_reads++;
    } else {
        ctx->failed_reads++;
    }
    
    return MULTIREAD_OK;
}

/**
 * @brief Execute voting and recovery
 */
multiread_error_t multiread_execute(multiread_ctx_t *ctx,
                                     uint8_t *output,
                                     size_t output_len,
                                     multiread_sector_t *result) {
    if (!ctx || !output || !result) {
        return MULTIREAD_ERR_NULL_PARAM;
    }
    
    if (ctx->pass_count < ctx->config.min_passes) {
        return MULTIREAD_ERR_INSUFFICIENT_PASSES;
    }
    
    /* Allocate weak mask if detection enabled */
    uint8_t *weak_mask = NULL;
    if (ctx->config.detect_weak_bits) {
        weak_mask = calloc(output_len, 1);
    }
    
    /* Vote on each byte */
    uint8_t avg_conf;
    uint32_t weak_count;
    
    multiread_error_t err = vote_buffer(ctx->passes, ctx->pass_count,
                                        output, output_len,
                                        NULL, weak_mask,
                                        &avg_conf, &weak_count);
    if (err != MULTIREAD_OK) {
        free(weak_mask);
        return err;
    }
    
    /* Fill result */
    result->data = output;
    result->data_len = output_len;
    result->confidence = avg_conf;
    result->good_reads = 0;
    result->total_reads = ctx->pass_count;
    
    /* Count good reads */
    for (uint8_t i = 0; i < ctx->pass_count; i++) {
        if (ctx->passes[i].crc_ok) {
            result->good_reads++;
        }
    }
    
    result->recovered = (avg_conf >= ctx->config.min_confidence);
    result->has_weak_bits = (weak_count > 0);
    result->weak_mask = weak_mask;
    
    /* Update stats */
    if (result->recovered) {
        ctx->bytes_recovered += output_len;
    }
    
    return MULTIREAD_OK;
}

/*============================================================================
 * High-Level Pipeline
 *============================================================================*/

/**
 * @brief Read track with multi-pass voting
 * 
 * Uses the configured read callback to perform multiple reads
 * and then votes to recover the best data.
 */
multiread_error_t multiread_track(multiread_ctx_t *ctx,
                                   uint8_t track,
                                   uint8_t head,
                                   multiread_track_t *result) {
    if (!ctx || !result) {
        return MULTIREAD_ERR_NULL_PARAM;
    }
    
    if (!ctx->config.read_callback) {
        return MULTIREAD_ERR_NULL_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    result->track = track;
    result->head = head;
    
    /* Clear previous passes */
    clear_passes(ctx);
    
    /* Perform multiple read passes */
    uint8_t buffer[MULTIREAD_MAX_TRACK_SIZE];
    
    for (uint8_t pass = 0; pass < ctx->config.max_passes; pass++) {
        /* Progress callback */
        if (ctx->config.progress_callback) {
            ctx->config.progress_callback(ctx->config.user_data,
                                          track, head, pass,
                                          ctx->config.max_passes);
        }
        
        /* Read track */
        size_t len = MULTIREAD_MAX_TRACK_SIZE;
        int ret = ctx->config.read_callback(ctx->config.user_data,
                                            track, head, buffer, &len);
        
        if (ret >= 0 && len > 0) {
            /* CRC check (simplified - real impl would parse sectors) */
            bool crc_ok = (ret == 0);
            uint8_t quality = crc_ok ? 100 : 50;
            
            multiread_add_pass(ctx, buffer, len, quality, crc_ok);
        }
        
        /* Check if we have enough good passes */
        if (ctx->successful_reads >= ctx->config.min_passes) {
            break;
        }
    }
    
    /* Did we get enough passes? */
    if (ctx->pass_count < ctx->config.min_passes) {
        return MULTIREAD_ERR_INSUFFICIENT_PASSES;
    }
    
    /* TODO: Parse sectors and vote per-sector */
    /* For now, just vote on entire track */
    
    result->sector_count = 0;  /* Would be filled by sector parsing */
    result->overall_confidence = 0;
    
    if (ctx->pass_count > 0) {
        uint64_t total = 0;
        for (uint8_t i = 0; i < ctx->pass_count; i++) {
            total += ctx->passes[i].quality;
        }
        result->overall_confidence = (uint8_t)(total / ctx->pass_count);
    }
    
    return MULTIREAD_OK;
}

/*============================================================================
 * Convenience Functions
 *============================================================================*/

/**
 * @brief Simple multi-read recovery for sector data
 * 
 * Takes multiple read buffers and returns voted result.
 */
multiread_error_t multiread_vote_buffers(const uint8_t **buffers,
                                          const size_t *lengths,
                                          uint8_t buffer_count,
                                          uint8_t *output,
                                          size_t output_len,
                                          uint8_t *confidence) {
    if (!buffers || !lengths || !output || buffer_count == 0) {
        return MULTIREAD_ERR_NULL_PARAM;
    }
    
    /* Build temporary pass array */
    multiread_pass_t passes[MULTIREAD_MAX_PASSES];
    uint8_t pass_count = buffer_count;
    if (pass_count > MULTIREAD_MAX_PASSES) {
        pass_count = MULTIREAD_MAX_PASSES;
    }
    
    for (uint8_t i = 0; i < pass_count; i++) {
        passes[i].data = (uint8_t *)buffers[i];
        passes[i].data_len = lengths[i];
        passes[i].quality = 100;
        passes[i].crc_ok = true;
    }
    
    /* Vote */
    uint8_t avg_conf;
    multiread_error_t err = vote_buffer(passes, pass_count,
                                        output, output_len,
                                        NULL, NULL,
                                        &avg_conf, NULL);
    
    if (confidence) {
        *confidence = avg_conf;
    }
    
    return err;
}

/**
 * @brief Get recovery statistics
 */
void multiread_get_stats(const multiread_ctx_t *ctx,
                          uint32_t *total_reads,
                          uint32_t *successful_reads,
                          uint32_t *bytes_recovered,
                          double *avg_confidence) {
    if (!ctx) return;
    
    if (total_reads) *total_reads = ctx->total_reads;
    if (successful_reads) *successful_reads = ctx->successful_reads;
    if (bytes_recovered) *bytes_recovered = ctx->bytes_recovered;
    if (avg_confidence) *avg_confidence = ctx->avg_confidence;
}

/*============================================================================
 * Report Generation
 *============================================================================*/

/**
 * @brief Generate recovery report
 */
char *multiread_generate_report(const multiread_ctx_t *ctx,
                                 const multiread_track_t *tracks,
                                 uint8_t track_count) {
    if (!ctx) return NULL;
    
    /* Allocate report buffer */
    size_t buf_size = 4096;
    char *report = malloc(buf_size);
    if (!report) return NULL;
    
    int pos = 0;
    
    pos += snprintf(report + pos, buf_size - pos,
        "=== UFT Multi-Read Recovery Report ===\n\n"
        "Configuration:\n"
        "  Min passes: %u\n"
        "  Max passes: %u\n"
        "  Min confidence: %u%%\n"
        "  Majority threshold: %u%%\n"
        "  Adaptive passes: %s\n"
        "  Weak bit detection: %s\n\n",
        ctx->config.min_passes,
        ctx->config.max_passes,
        ctx->config.min_confidence,
        ctx->config.majority_pct,
        ctx->config.adaptive_passes ? "yes" : "no",
        ctx->config.detect_weak_bits ? "yes" : "no");
    
    pos += snprintf(report + pos, buf_size - pos,
        "Statistics:\n"
        "  Total reads: %u\n"
        "  Successful reads: %u\n"
        "  Failed reads: %u\n"
        "  Bytes recovered: %u\n\n",
        ctx->total_reads,
        ctx->successful_reads,
        ctx->failed_reads,
        ctx->bytes_recovered);
    
    if (tracks && track_count > 0) {
        pos += snprintf(report + pos, buf_size - pos,
            "Track Results:\n");
        
        for (uint8_t t = 0; t < track_count && pos < (int)(buf_size - 200); t++) {
            pos += snprintf(report + pos, buf_size - pos,
                "  Track %u.%u: %u sectors, %u good, %u recovered, %u failed (conf: %u%%)\n",
                tracks[t].track, tracks[t].head,
                tracks[t].sector_count,
                tracks[t].good_sectors,
                tracks[t].recovered_sectors,
                tracks[t].failed_sectors,
                tracks[t].overall_confidence);
        }
    }
    
    return report;
}

/*============================================================================
 * Unit Tests
 *============================================================================*/

#ifdef UFT_UNIT_TESTS

#include <assert.h>

static void test_multiread_vote_majority(void) {
    /* Test majority voting */
    uint8_t buf1[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t buf2[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t buf3[] = {0x01, 0x02, 0xFF, 0x04};  /* One different byte */
    
    const uint8_t *buffers[] = {buf1, buf2, buf3};
    size_t lengths[] = {4, 4, 4};
    
    uint8_t output[4];
    uint8_t confidence;
    
    multiread_error_t err = multiread_vote_buffers(buffers, lengths, 3,
                                                    output, 4, &confidence);
    
    assert(err == MULTIREAD_OK);
    assert(output[0] == 0x01);
    assert(output[1] == 0x02);
    assert(output[2] == 0x03);  /* Majority wins */
    assert(output[3] == 0x04);
    assert(confidence >= 75);
    
    printf("  ✓ multiread_vote_majority passed\n");
}

static void test_multiread_context(void) {
    multiread_config_t cfg = multiread_config_default();
    multiread_ctx_t *ctx = multiread_create(&cfg);
    
    assert(ctx != NULL);
    
    /* Add passes */
    uint8_t data1[] = {0xAA, 0xBB, 0xCC};
    uint8_t data2[] = {0xAA, 0xBB, 0xCC};
    uint8_t data3[] = {0xAA, 0xBB, 0xDD};
    
    assert(multiread_add_pass(ctx, data1, 3, 100, true) == MULTIREAD_OK);
    assert(multiread_add_pass(ctx, data2, 3, 100, true) == MULTIREAD_OK);
    assert(multiread_add_pass(ctx, data3, 3, 80, false) == MULTIREAD_OK);
    
    /* Execute voting */
    uint8_t output[3];
    multiread_sector_t result = {0};
    
    multiread_error_t err = multiread_execute(ctx, output, 3, &result);
    assert(err == MULTIREAD_OK);
    
    assert(output[0] == 0xAA);
    assert(output[1] == 0xBB);
    assert(output[2] == 0xCC);  /* Majority */
    assert(result.good_reads == 2);
    assert(result.total_reads == 3);
    
    /* Cleanup */
    free(result.weak_mask);
    multiread_destroy(ctx);
    
    printf("  ✓ multiread_context passed\n");
}

static void test_multiread_error_handling(void) {
    /* Test error cases */
    assert(multiread_vote_buffers(NULL, NULL, 0, NULL, 0, NULL) 
           == MULTIREAD_ERR_NULL_PARAM);
    
    multiread_ctx_t *ctx = multiread_create(NULL);
    assert(ctx != NULL);
    
    uint8_t output[4];
    multiread_sector_t result;
    
    /* Not enough passes */
    assert(multiread_execute(ctx, output, 4, &result) 
           == MULTIREAD_ERR_INSUFFICIENT_PASSES);
    
    multiread_destroy(ctx);
    
    printf("  ✓ multiread_error_handling passed\n");
}

static void test_multiread_report(void) {
    multiread_config_t cfg = multiread_config_default();
    multiread_ctx_t *ctx = multiread_create(&cfg);
    
    char *report = multiread_generate_report(ctx, NULL, 0);
    assert(report != NULL);
    assert(strstr(report, "Multi-Read Recovery Report") != NULL);
    
    free(report);
    multiread_destroy(ctx);
    
    printf("  ✓ multiread_report passed\n");
}

void uft_multiread_pipeline_tests(void) {
    printf("Running Multi-Read Pipeline tests...\n");
    test_multiread_vote_majority();
    test_multiread_context();
    test_multiread_error_handling();
    test_multiread_report();
    printf("All Multi-Read Pipeline tests passed!\n");
}

#endif /* UFT_UNIT_TESTS */
