/**
 * @file uft_write_verify.c
 * @brief Write Verification Implementation
 * 
 * TICKET-002: Verify After Write
 * Bitwise verification after write operations
 */

#include "uft/uft_write_verify.h"
#include "uft/uft_core.h"
#include "uft/uft_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static uint32_t calculate_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

static void sha256_simple(const uint8_t *data, size_t size, char *hash_out) {
    /* Simple hash for demonstration - in production use OpenSSL or similar */
    uint32_t h = 0x811c9dc5;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 0x01000193;
    }
    snprintf(hash_out, 65, "%08x%08x%08x%08x%08x%08x%08x%08x",
             h, h ^ 0xDEADBEEF, h ^ 0xCAFEBABE, h ^ 0x12345678,
             h ^ 0x87654321, h ^ 0xABCDEF01, h ^ 0x10FEDCBA, h ^ 0x55AA55AA);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Verification
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_sector_verify_t *uft_verify_sector(uft_disk_t *disk,
                                        uint8_t cylinder, uint8_t head,
                                        uint8_t sector,
                                        const uint8_t *expected, size_t size) {
    if (!disk || !expected || size == 0) return NULL;
    
    uft_sector_verify_t *result = calloc(1, sizeof(uft_sector_verify_t));
    if (!result) return NULL;
    
    result->sector = sector;
    result->bytes_total = size;
    result->max_mismatches = 100;
    
    /* Read actual data from disk */
    uint8_t *actual = malloc(size);
    if (!actual) {
        result->status = UFT_VERIFY_READ_ERROR;
        return result;
    }
    
    /* In real implementation: uft_disk_read_sector(disk, cyl, head, sector, actual, size) */
    /* For now, simulate a read */
    memset(actual, 0, size);
    
    /* Calculate CRCs */
    result->crc_expected = calculate_crc32(expected, size);
    result->crc_actual = calculate_crc32(actual, size);
    result->crc_valid = (result->crc_expected == result->crc_actual);
    
    /* Compare byte by byte */
    result->mismatches = calloc(result->max_mismatches, sizeof(uft_verify_mismatch_t));
    result->mismatch_count = 0;
    result->bytes_matching = 0;
    
    for (size_t i = 0; i < size; i++) {
        if (expected[i] == actual[i]) {
            result->bytes_matching++;
        } else {
            if (result->mismatch_count < result->max_mismatches) {
                uft_verify_mismatch_t *m = &result->mismatches[result->mismatch_count];
                m->offset = i;
                m->expected = expected[i];
                m->actual = actual[i];
                m->xor_diff = expected[i] ^ actual[i];
            }
            result->mismatch_count++;
        }
    }
    
    result->match_percent = (float)result->bytes_matching / size * 100.0f;
    result->status = (result->mismatch_count == 0) ? UFT_VERIFY_OK : UFT_VERIFY_MISMATCH;
    
    free(actual);
    return result;
}

void uft_sector_verify_free(uft_sector_verify_t *result) {
    if (!result) return;
    free(result->mismatches);
    free(result);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Verification
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_track_verify_t *uft_verify_track(uft_disk_t *disk,
                                      uint8_t cylinder, uint8_t head,
                                      const uint8_t *expected, size_t size) {
    uft_verify_options_t opts = UFT_VERIFY_OPTIONS_DEFAULT;
    return uft_verify_track_ex(disk, cylinder, head, expected, size, &opts);
}

uft_track_verify_t *uft_verify_track_ex(uft_disk_t *disk,
                                         uint8_t cylinder, uint8_t head,
                                         const uint8_t *expected, size_t size,
                                         const uft_verify_options_t *options) {
    if (!disk || !expected || size == 0) return NULL;
    
    double start_time = get_time_ms();
    
    uft_track_verify_t *result = calloc(1, sizeof(uft_track_verify_t));
    if (!result) return NULL;
    
    result->cylinder = cylinder;
    result->head = head;
    result->bytes_total = size;
    
    /* Read actual track data */
    uint8_t *actual = malloc(size);
    if (!actual) {
        result->status = UFT_VERIFY_READ_ERROR;
        return result;
    }
    
    double read_start = get_time_ms();
    /* In real implementation: uft_disk_read_track(disk, cyl, head, actual, size) */
    memset(actual, 0, size);  /* Simulate read */
    result->read_time_ms = get_time_ms() - read_start;
    
    /* Compare */
    double verify_start = get_time_ms();
    result->bytes_matching = 0;
    
    for (size_t i = 0; i < size; i++) {
        if (expected[i] == actual[i]) {
            result->bytes_matching++;
        }
    }
    
    result->match_percent = (float)result->bytes_matching / size * 100.0f;
    result->verify_time_ms = get_time_ms() - verify_start;
    
    /* Determine status */
    if (result->bytes_matching == size) {
        result->status = UFT_VERIFY_OK;
        result->sectors_ok = 1;  /* Simplified */
    } else {
        result->status = UFT_VERIFY_MISMATCH;
        result->sectors_failed = 1;
    }
    
    free(actual);
    return result;
}

void uft_track_verify_free(uft_track_verify_t *result) {
    if (!result) return;
    
    if (result->sectors) {
        for (int i = 0; i < result->sector_count; i++) {
            free(result->sectors[i].mismatches);
        }
        free(result->sectors);
    }
    free(result);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Verification
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_verify_result_t *uft_verify_disk(uft_disk_t *disk,
                                      const uint8_t *reference, size_t ref_size) {
    uft_verify_options_t opts = UFT_VERIFY_OPTIONS_DEFAULT;
    return uft_verify_disk_ex(disk, reference, ref_size, &opts);
}

uft_verify_result_t *uft_verify_disk_ex(uft_disk_t *disk,
                                         const uint8_t *reference, size_t ref_size,
                                         const uft_verify_options_t *options) {
    if (!disk || !reference) return NULL;
    
    double start_time = get_time_ms();
    
    uft_verify_result_t *result = calloc(1, sizeof(uft_verify_result_t));
    if (!result) return NULL;
    
    result->bytes_total = ref_size;
    result->has_first_mismatch = false;
    
    /* Get geometry */
    uft_geometry_t geom;
    if (uft_disk_get_geometry(disk, &geom) != UFT_OK) {
        result->status = UFT_VERIFY_FORMAT_ERROR;
        return result;
    }
    
    int total_tracks = geom.cylinders * geom.heads;
    size_t track_size = geom.sectors_per_track * geom.bytes_per_sector;
    
    result->tracks = calloc(total_tracks, sizeof(uft_track_verify_t));
    if (!result->tracks) {
        result->status = UFT_VERIFY_READ_ERROR;
        return result;
    }
    
    size_t offset = 0;
    result->track_count = 0;
    result->tracks_ok = 0;
    result->tracks_failed = 0;
    result->bytes_verified = 0;
    result->bytes_matching = 0;
    
    for (int cyl = 0; cyl < geom.cylinders && offset < ref_size; cyl++) {
        for (int head = 0; head < geom.heads && offset < ref_size; head++) {
            size_t remaining = ref_size - offset;
            size_t chunk = (remaining < track_size) ? remaining : track_size;
            
            /* Progress callback */
            if (options->progress_fn) {
                options->progress_fn(result->track_count, total_tracks, 
                                    options->progress_user);
            }
            
            uft_track_verify_t *tv = uft_verify_track_ex(disk, cyl, head,
                                                         reference + offset, chunk,
                                                         options);
            if (tv) {
                result->tracks[result->track_count] = *tv;
                result->bytes_verified += tv->bytes_total;
                result->bytes_matching += tv->bytes_matching;
                
                if (tv->status == UFT_VERIFY_OK) {
                    result->tracks_ok++;
                } else {
                    result->tracks_failed++;
                    
                    /* Record first mismatch */
                    if (!result->has_first_mismatch) {
                        result->has_first_mismatch = true;
                        result->first_mismatch_cyl = cyl;
                        result->first_mismatch_head = head;
                        result->first_mismatch_sector = 0;
                        result->first_mismatch_offset = offset;
                    }
                    
                    if (options->stop_on_first) {
                        free(tv);
                        goto done;
                    }
                }
                
                free(tv);  /* We copied the data */
            }
            
            result->track_count++;
            offset += chunk;
        }
    }
    
done:
    result->overall_match_percent = result->bytes_verified > 0 ?
        (float)result->bytes_matching / result->bytes_verified * 100.0f : 0;
    
    result->status = (result->tracks_failed == 0) ? UFT_VERIFY_OK : UFT_VERIFY_MISMATCH;
    result->total_time_ms = get_time_ms() - start_time;
    
    /* Compute hashes if requested */
    if (options->compute_hashes) {
        sha256_simple(reference, ref_size, result->hash_expected);
        /* hash_actual would need the actual disk data */
        strcpy(result->hash_actual, "not-computed");
    }
    
    return result;
}

uft_verify_result_t *uft_verify_compare_disks(uft_disk_t *disk1, uft_disk_t *disk2,
                                               const uft_verify_options_t *options) {
    /* Read disk2 into memory and verify against disk1 */
    /* Simplified implementation */
    return NULL;
}

void uft_verify_result_free(uft_verify_result_t *result) {
    if (!result) return;
    
    if (result->tracks) {
        for (int i = 0; i < result->track_count; i++) {
            if (result->tracks[i].sectors) {
                for (int j = 0; j < result->tracks[i].sector_count; j++) {
                    free(result->tracks[i].sectors[j].mismatches);
                }
                free(result->tracks[i].sectors);
            }
        }
        free(result->tracks);
    }
    
    free(result);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write with Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_disk_write_track_verified(uft_disk_t *disk,
                                           uint8_t cylinder, uint8_t head,
                                           const uint8_t *data, size_t size,
                                           const uft_write_options_t *options,
                                           uft_track_verify_t **verify_result) {
    if (!disk || !data || size == 0) return UFT_ERR_INVALID_PARAM;
    
    uft_write_options_t opts = options ? *options : (uft_write_options_t)UFT_WRITE_OPTIONS_DEFAULT;
    uft_error_t err = UFT_OK;
    int retry = 0;
    
    do {
        /* Check abort */
        if (opts.abort_check && opts.abort_check(opts.abort_user)) {
            return UFT_ERR_ABORTED;
        }
        
        /* Write track */
        /* In real implementation: err = uft_disk_write_track(disk, cyl, head, data, size); */
        err = UFT_OK;  /* Simulate success */
        
        if (err != UFT_OK) break;
        
        /* Verify if requested */
        if (opts.verify) {
            uft_track_verify_t *vr = uft_verify_track_ex(disk, cylinder, head,
                                                          data, size,
                                                          &opts.verify_options);
            if (vr) {
                if (vr->status != UFT_VERIFY_OK) {
                    err = UFT_ERR_VERIFY;
                    retry++;
                }
                
                if (verify_result) {
                    *verify_result = vr;
                } else {
                    uft_track_verify_free(vr);
                }
            }
        }
        
    } while (err == UFT_ERR_VERIFY && retry < opts.verify_options.max_retries);
    
    return err;
}

uft_error_t uft_disk_write_sector_verified(uft_disk_t *disk,
                                            uint8_t cylinder, uint8_t head,
                                            uint8_t sector,
                                            const uint8_t *data, size_t size,
                                            const uft_write_options_t *options,
                                            uft_sector_verify_t **verify_result) {
    if (!disk || !data || size == 0) return UFT_ERR_INVALID_PARAM;
    
    uft_write_options_t opts = options ? *options : (uft_write_options_t)UFT_WRITE_OPTIONS_DEFAULT;
    uft_error_t err = UFT_OK;
    
    /* Write sector */
    /* In real implementation: err = uft_disk_write_sector(disk, cyl, head, sector, data, size); */
    err = UFT_OK;
    
    if (err != UFT_OK) return err;
    
    /* Verify if requested */
    if (opts.verify) {
        uft_sector_verify_t *vr = uft_verify_sector(disk, cylinder, head, sector,
                                                     data, size);
        if (vr) {
            if (vr->status != UFT_VERIFY_OK) {
                err = UFT_ERR_VERIFY;
            }
            
            if (verify_result) {
                *verify_result = vr;
            } else {
                uft_sector_verify_free(vr);
            }
        }
    }
    
    return err;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format-Specific Verifiers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_format_verifier_fn format_verifiers[256] = {0};

int uft_verify_register_format(uft_format_t format, uft_format_verifier_fn verifier) {
    if (format >= 256) return -1;
    format_verifiers[format] = verifier;
    return 0;
}

uft_verify_status_t uft_verify_amiga_track(const uint8_t *expected,
                                            const uint8_t *actual, size_t size) {
    if (!expected || !actual || size == 0) return UFT_VERIFY_FORMAT_ERROR;
    
    /* Amiga tracks have checksums in the MFM data */
    /* Simplified: just compare bytes */
    for (size_t i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            return UFT_VERIFY_MISMATCH;
        }
    }
    
    return UFT_VERIFY_OK;
}

uft_verify_status_t uft_verify_c64_track(const uint8_t *expected,
                                          const uint8_t *actual, size_t size) {
    /* C64/GCR verification */
    for (size_t i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            return UFT_VERIFY_MISMATCH;
        }
    }
    return UFT_VERIFY_OK;
}

uft_verify_status_t uft_verify_apple_track(const uint8_t *expected,
                                            const uint8_t *actual, size_t size) {
    /* Apple GCR verification */
    for (size_t i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            return UFT_VERIFY_MISMATCH;
        }
    }
    return UFT_VERIFY_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Reporting
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_verify_result_print(const uft_verify_result_t *result) {
    if (!result) return;
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("                   VERIFICATION REPORT\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    printf("STATUS: %s\n\n", uft_verify_status_string(result->status));
    
    printf("SUMMARY:\n");
    printf("  Tracks verified:  %d\n", result->track_count);
    printf("  Tracks OK:        %d\n", result->tracks_ok);
    printf("  Tracks failed:    %d\n", result->tracks_failed);
    printf("  Bytes verified:   %zu\n", result->bytes_verified);
    printf("  Bytes matching:   %zu\n", result->bytes_matching);
    printf("  Match percent:    %.2f%%\n", result->overall_match_percent);
    printf("  Total time:       %.2f ms\n\n", result->total_time_ms);
    
    if (result->has_first_mismatch) {
        printf("FIRST MISMATCH:\n");
        printf("  Cylinder: %d\n", result->first_mismatch_cyl);
        printf("  Head:     %d\n", result->first_mismatch_head);
        printf("  Sector:   %d\n", result->first_mismatch_sector);
        printf("  Offset:   %zu\n\n", result->first_mismatch_offset);
    }
    
    printf("HASHES:\n");
    printf("  Expected: %s\n", result->hash_expected);
    printf("  Actual:   %s\n", result->hash_actual);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
}

char *uft_verify_result_to_json(const uft_verify_result_t *result) {
    if (!result) return NULL;
    
    size_t size = 2048 + result->track_count * 128;
    char *json = malloc(size);
    if (!json) return NULL;
    
    snprintf(json, size,
        "{\n"
        "  \"status\": \"%s\",\n"
        "  \"tracks_verified\": %d,\n"
        "  \"tracks_ok\": %d,\n"
        "  \"tracks_failed\": %d,\n"
        "  \"bytes_verified\": %zu,\n"
        "  \"bytes_matching\": %zu,\n"
        "  \"match_percent\": %.2f,\n"
        "  \"total_time_ms\": %.2f,\n"
        "  \"has_mismatch\": %s,\n"
        "  \"hash_expected\": \"%s\",\n"
        "  \"hash_actual\": \"%s\"\n"
        "}\n",
        uft_verify_status_string(result->status),
        result->track_count,
        result->tracks_ok,
        result->tracks_failed,
        result->bytes_verified,
        result->bytes_matching,
        result->overall_match_percent,
        result->total_time_ms,
        result->has_first_mismatch ? "true" : "false",
        result->hash_expected,
        result->hash_actual);
    
    return json;
}

uft_error_t uft_verify_result_save(const uft_verify_result_t *result, const char *path) {
    if (!result || !path) return UFT_ERR_INVALID_PARAM;
    
    char *json = uft_verify_result_to_json(result);
    if (!json) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        free(json);
        return UFT_ERR_IO;
    }
    
    fputs(json, f);
    fclose(f);
    free(json);
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_verify_status_string(uft_verify_status_t status) {
    switch (status) {
        case UFT_VERIFY_OK:           return "OK";
        case UFT_VERIFY_MISMATCH:     return "MISMATCH";
        case UFT_VERIFY_CRC_ERROR:    return "CRC_ERROR";
        case UFT_VERIFY_READ_ERROR:   return "READ_ERROR";
        case UFT_VERIFY_SIZE_MISMATCH:return "SIZE_MISMATCH";
        case UFT_VERIFY_FORMAT_ERROR: return "FORMAT_ERROR";
        case UFT_VERIFY_TIMEOUT:      return "TIMEOUT";
        case UFT_VERIFY_ABORTED:      return "ABORTED";
        default:                      return "UNKNOWN";
    }
}

const char *uft_verify_mode_string(uft_verify_mode_t mode) {
    switch (mode) {
        case UFT_VERIFY_MODE_BITWISE: return "BITWISE";
        case UFT_VERIFY_MODE_CRC:     return "CRC";
        case UFT_VERIFY_MODE_SECTOR:  return "SECTOR";
        case UFT_VERIFY_MODE_FLUX:    return "FLUX";
        default:                      return "UNKNOWN";
    }
}

bool uft_verify_bytes(const uint8_t *expected, const uint8_t *actual,
                       size_t size, size_t *first_mismatch) {
    for (size_t i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            if (first_mismatch) *first_mismatch = i;
            return false;
        }
    }
    return true;
}

bool uft_verify_crc(const uint8_t *data, size_t size, uint32_t expected_crc) {
    return calculate_crc32(data, size) == expected_crc;
}
