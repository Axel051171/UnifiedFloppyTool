/**
 * @file uft_track_writer.c
 * @brief Track Writer Module Implementation
 * 
 * Based on nibtools write.c by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/hardware/uft_track_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Default density per track */
static const uint8_t speed_map[43] = {
    0,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /*  1 - 10 */
    3, 3, 3, 3, 3, 3, 3, 2, 2, 2,  /* 11 - 20 */
    2, 2, 2, 2, 1, 1, 1, 1, 1, 1,  /* 21 - 30 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 31 - 40 */
    0, 0                           /* 41 - 42 */
};

/** Sectors per track */
static const int sector_map[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17
};

/** Default capacity at each density */
static const size_t default_capacity[4] = {
    WRITER_CAPACITY_D0, WRITER_CAPACITY_D1,
    WRITER_CAPACITY_D2, WRITER_CAPACITY_D3
};

/** Density constants for RPM calculation */
static const float density_const[4] = {
    WRITER_DENSITY0_CONST, WRITER_DENSITY1_CONST,
    WRITER_DENSITY2_CONST, WRITER_DENSITY3_CONST
};

/* ============================================================================
 * Session Management
 * ============================================================================ */

/**
 * @brief Create writer session
 */
writer_session_t *writer_create_session(void)
{
    writer_session_t *session = calloc(1, sizeof(writer_session_t));
    if (!session) return NULL;
    
    /* Set default options */
    writer_get_defaults(&session->options);
    
    /* Set default capacities */
    session->calibration.capacity[0] = WRITER_CAPACITY_D0;
    session->calibration.capacity[1] = WRITER_CAPACITY_D1;
    session->calibration.capacity[2] = WRITER_CAPACITY_D2;
    session->calibration.capacity[3] = WRITER_CAPACITY_D3;
    session->calibration.rpm = 300.0f;
    session->calibration.valid = true;
    
    session->current_track = -1;
    
    return session;
}

/**
 * @brief Close writer session
 */
void writer_close_session(writer_session_t *session)
{
    if (!session) return;
    
    /* Turn off motor if possible */
    if (session->motor_off && session->hw_context) {
        session->motor_off(session->hw_context);
    }
    
    free(session);
}

/**
 * @brief Get default options
 */
void writer_get_defaults(write_options_t *options)
{
    if (!options) return;
    
    memset(options, 0, sizeof(write_options_t));
    options->verify = true;
    options->fillbyte = 0x55;
    options->verify_tol = WRITER_VERIFY_TOLERANCE;
}

/* ============================================================================
 * Calibration
 * ============================================================================ */

/**
 * @brief Calibrate motor speed
 */
int writer_calibrate(writer_session_t *session, motor_calibration_t *result)
{
    if (!session) return -1;
    
    motor_calibration_t cal;
    memset(&cal, 0, sizeof(cal));
    
    /* Track positions for density measurement */
    int track_pos[4] = { 32*2, 27*2, 21*2, 10*2 };
    int cap_high[4] = {0};
    int cap_low[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    int capacity_margin = 0;
    
    /* Turn on motor */
    if (session->motor_on && session->hw_context) {
        session->motor_on(session->hw_context);
    }
    
    /* Measure capacity at each density */
    for (int density = 0; density <= 3; density++) {
        if (session->step_to && session->hw_context) {
            session->step_to(session->hw_context, track_pos[density]);
        }
        
        if (session->set_density && session->hw_context) {
            session->set_density(session->hw_context, density);
        }
        
        int total = 0;
        for (int sample = 0; sample < WRITER_DENSITY_SAMPLES; sample++) {
            int cap;
            
            if (session->track_capacity && session->hw_context) {
                cap = session->track_capacity(session->hw_context);
            } else {
                cap = default_capacity[density];  /* Use default if no hardware */
            }
            
            total += cap;
            if (cap > cap_high[density]) cap_high[density] = cap;
            if (cap < cap_low[density]) cap_low[density] = cap;
        }
        
        cal.capacity[density] = total / WRITER_DENSITY_SAMPLES;
        
        int margin = cap_high[density] - cap_low[density];
        if (margin > capacity_margin) {
            capacity_margin = margin;
        }
    }
    
    /* Adjust for margin */
    for (int d = 0; d < 4; d++) {
        cal.capacity[d] -= capacity_margin + session->options.extra_margin;
    }
    
    cal.margin = capacity_margin + session->options.extra_margin;
    
    /* Calculate average RPM */
    float rpm_sum = 0;
    for (int d = 0; d < 4; d++) {
        int adj_cap = cal.capacity[d] + capacity_margin + session->options.extra_margin;
        rpm_sum += density_const[d] / adj_cap;
    }
    cal.rpm = rpm_sum / 4.0f;
    
    /* Check if valid */
    cal.valid = writer_speed_valid(cal.rpm);
    
    if (cal.valid) {
        snprintf(cal.message, sizeof(cal.message),
                 "Motor speed: %.2f RPM, margin: %d bytes",
                 cal.rpm, cal.margin);
    } else {
        snprintf(cal.message, sizeof(cal.message),
                 "ERROR: Motor speed %.2f RPM out of range (280-320)",
                 cal.rpm);
    }
    
    /* Store calibration */
    session->calibration = cal;
    session->calibrated = true;
    
    if (result) {
        *result = cal;
    }
    
    return cal.valid ? 0 : -1;
}

/**
 * @brief Get capacity at density
 */
int writer_get_capacity(const writer_session_t *session, int density)
{
    if (!session || density < 0 || density > 3) return -1;
    
    if (session->calibrated) {
        return session->calibration.capacity[density];
    }
    
    return default_capacity[density];
}

/**
 * @brief Check if speed is valid
 */
bool writer_speed_valid(float rpm)
{
    return (rpm >= 280.0f && rpm <= 320.0f);
}

/* ============================================================================
 * Track Processing
 * ============================================================================ */

/**
 * @brief Check sync flags
 */
uint8_t writer_check_sync_flags(const uint8_t *track_data, uint8_t density, size_t length)
{
    if (!track_data || length == 0) return density;
    
    int sync_count = 0;
    int gap_count = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (track_data[i] == 0xFF) sync_count++;
        else if (track_data[i] == 0x55) gap_count++;
    }
    
    /* All sync = killer track */
    if (sync_count >= (int)(length * 90 / 100)) {
        return (density & 0x03) | WRITER_BM_FF_TRACK;
    }
    
    /* No sync found */
    if (sync_count < 10) {
        return (density & 0x03) | WRITER_BM_NO_SYNC;
    }
    
    return density & 0x03;
}

/**
 * @brief Check if track is formatted
 */
bool writer_check_formatted(const uint8_t *track_data, size_t length)
{
    if (!track_data || length == 0) return false;
    
    /* Look for sync marks */
    int syncs = 0;
    for (size_t i = 0; i + 1 < length; i++) {
        if (track_data[i] == 0xFF && track_data[i + 1] == 0xFF) {
            syncs++;
        }
    }
    
    return (syncs >= 10);  /* At least 10 sync marks for formatted track */
}

/**
 * @brief Compress track
 */
size_t writer_compress_track(int halftrack, uint8_t *track_data,
                              uint8_t density, size_t length)
{
    if (!track_data || length == 0) return 0;
    
    int dens = density & 0x03;
    size_t capacity = default_capacity[dens];
    
    /* If track is longer than capacity, truncate */
    if (length > capacity) {
        return capacity;
    }
    
    return length;
}

/**
 * @brief Lengthen sync marks
 */
int writer_lengthen_sync(uint8_t *track_data, size_t length, size_t capacity)
{
    if (!track_data || length == 0 || length >= capacity) return 0;
    
    int added = 0;
    size_t available = capacity - length;
    size_t pos = 0;
    
    /* Find short sync marks and extend them */
    while (pos < length - 5 && added < (int)available) {
        /* Find sync mark */
        if (track_data[pos] == 0xFF) {
            size_t sync_start = pos;
            while (pos < length && track_data[pos] == 0xFF) pos++;
            size_t sync_len = pos - sync_start;
            
            /* If sync is short (< 5 bytes), extend it */
            if (sync_len < 5 && (sync_len + (available - added)) >= 5) {
                int extend = 5 - sync_len;
                if (extend > (int)(available - added)) extend = available - added;
                
                /* Shift data after sync */
                memmove(track_data + pos + extend, track_data + pos, length - pos);
                memset(track_data + pos, 0xFF, extend);
                length += extend;
                added += extend;
                pos += extend;
            }
        } else {
            pos++;
        }
    }
    
    return added;
}

/**
 * @brief Replace bytes
 */
int writer_replace_bytes(uint8_t *data, size_t length, uint8_t find, uint8_t replace)
{
    if (!data || length == 0) return 0;
    
    int count = 0;
    for (size_t i = 0; i < length; i++) {
        if (data[i] == find) {
            data[i] = replace;
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Prepare track for writing
 */
int writer_prepare_track(uint8_t *track_data, size_t length,
                         uint8_t density, const write_options_t *options,
                         size_t *output_len)
{
    if (!track_data || length == 0 || !output_len) return -1;
    
    write_options_t opts;
    if (options) {
        opts = *options;
    } else {
        writer_get_defaults(&opts);
    }
    
    uint8_t *raw = malloc(WRITER_TRACK_SIZE * 2);
    if (!raw) return -2;
    
    int dens = density & 0x03;
    size_t capacity = default_capacity[dens];
    
    /* Initialize with fill byte or gap */
    if (density & WRITER_BM_NO_SYNC) {
        memset(raw, 0x55, WRITER_TRACK_SIZE * 2);
    } else {
        memset(raw, opts.fillbyte, WRITER_TRACK_SIZE * 2);
    }
    
    /* Leader (gap before data) */
    int leader = 10;
    
    /* Bounds check: ensure data fits in buffer */
    size_t buf_size = WRITER_TRACK_SIZE * 2;
    if ((size_t)leader + length > buf_size) {
        free(raw);
        return -3;
    }
    
    /* Copy track data */
    memcpy(raw + leader, track_data, length);
    
    /* Add pre-sync if needed */
    if (opts.presync > 0 && !(density & WRITER_BM_NO_SYNC) && length >= 2) {
        if (track_data[0] == 0xFF && track_data[1] != 0xFF) {
            int ps = opts.presync;
            if (ps >= leader) ps = leader - 2;
            memset(raw + leader - ps, 0xFF, ps + 1);
        }
    }
    
    /* Handle short tracks (pad) */
    if (length < capacity) {
        length = capacity;
    }
    
    /* Track 18 fix for mastering */
    int track = 18;  /* Assume track 18 if not specified */
    if (track == 18) {
        memcpy(raw + leader + length - 5, "UJMSU", 5);
    }
    
    /* Replace 0x00 bytes (end-of-track marker) */
    writer_replace_bytes(raw, length + leader + 1, 0x00, 0x01);
    
    /* Copy result back */
    memcpy(track_data, raw, length + leader + 1);
    *output_len = length + leader + 1;
    
    free(raw);
    return 0;
}

/* ============================================================================
 * Track Writing
 * ============================================================================ */

/**
 * @brief Write single track
 */
int writer_write_track(writer_session_t *session, int halftrack,
                       const uint8_t *data, size_t length,
                       uint8_t density, track_write_result_t *result)
{
    if (!session || !data || halftrack < 2 || halftrack > 84) return -1;
    
    track_write_result_t res;
    memset(&res, 0, sizeof(res));
    
    /* Prepare track buffer */
    uint8_t *rawtrack = malloc(WRITER_TRACK_SIZE * 2);
    if (!rawtrack) return -2;
    
    int dens = density & 0x03;
    int leader = 10;
    
    /* Initialize buffer */
    if (density & WRITER_BM_NO_SYNC) {
        memset(rawtrack, 0x55, WRITER_TRACK_SIZE * 2);
    } else {
        memset(rawtrack, session->options.fillbyte, WRITER_TRACK_SIZE * 2);
    }
    
    /* Copy track data */
    size_t buf_size = WRITER_TRACK_SIZE * 2;
    if ((size_t)leader + length > buf_size) {
        free(rawtrack);
        return -3;
    }
    memcpy(rawtrack + leader, data, length);
    
    /* Handle short tracks */
    size_t capacity = writer_get_capacity(session, dens);
    if (length < capacity) {
        length = capacity;
    }
    
    /* Replace end-of-track marker */
    writer_replace_bytes(rawtrack, length + leader + 1, 0x00, 0x01);
    
    /* Step to track */
    if (session->step_to && session->hw_context) {
        /* Handle fat tracks */
        if (session->options.fattrack > 0 && halftrack == session->options.fattrack + 2) {
            session->step_to(session->hw_context, halftrack + 1);
        } else {
            session->step_to(session->hw_context, halftrack);
        }
        session->current_track = halftrack;
    }
    
    /* Set density */
    if (session->set_density && session->hw_context) {
        session->set_density(session->hw_context, dens);
        session->current_density = dens;
    }
    
    /* Write track (with retries) */
    int retries = 0;
    bool success = false;
    
    for (int i = 0; i < 3 && !success; i++) {
        if (session->send_cmd && session->hw_context) {
            session->send_cmd(session->hw_context, WRITER_CMD_WRITE, NULL, 0);
        }
        
        if (session->burst_write && session->hw_context) {
            session->burst_write(session->hw_context, 
                                  session->options.use_ihs ? 0x00 : 0x03);
            session->burst_write(session->hw_context, 0x00);
        }
        
        if (session->burst_write_track && session->hw_context) {
            int ret = session->burst_write_track(session->hw_context, 
                                                  rawtrack, length + leader + 1);
            if (ret == 0) {
                success = true;
            } else {
                retries++;
                if (session->burst_read && session->hw_context) {
                    session->burst_read(session->hw_context);
                }
            }
        } else {
            success = true;  /* No hardware, assume success */
        }
    }
    
    free(rawtrack);
    
    res.success = success;
    res.retries = retries;
    res.verify_result = WRITER_VERIFY_OK;
    
    if (success) {
        snprintf(res.message, sizeof(res.message),
                 "Track %.1f written (%zu bytes, density %d)",
                 halftrack / 2.0f, length, dens);
        session->tracks_written++;
    } else {
        snprintf(res.message, sizeof(res.message),
                 "Track %.1f write FAILED after %d retries",
                 halftrack / 2.0f, retries);
        session->errors++;
    }
    
    session->retries += retries;
    
    if (result) {
        *result = res;
    }
    
    return success ? 0 : -1;
}

/**
 * @brief Fill track with byte
 */
int writer_fill_track(writer_session_t *session, int halftrack, uint8_t fill_byte)
{
    if (!session || halftrack < 2 || halftrack > 84) return -1;
    
    /* Step to track */
    if (session->step_to && session->hw_context) {
        session->step_to(session->hw_context, halftrack);
    }
    
    /* Send fill command */
    if (session->send_cmd && session->hw_context) {
        session->send_cmd(session->hw_context, WRITER_CMD_FILLTRACK, NULL, 0);
    }
    
    if (session->burst_write && session->hw_context) {
        session->burst_write(session->hw_context, fill_byte);
    }
    
    if (session->burst_read && session->hw_context) {
        session->burst_read(session->hw_context);
    }
    
    return 0;
}

/**
 * @brief Write killer track
 */
int writer_kill_track(writer_session_t *session, int halftrack)
{
    return writer_fill_track(session, halftrack, 0xFF);
}

/**
 * @brief Erase track
 */
int writer_erase_track(writer_session_t *session, int halftrack)
{
    return writer_fill_track(session, halftrack, 0x00);
}

/* ============================================================================
 * Disk Mastering
 * ============================================================================ */

/**
 * @brief Master disk
 */
int writer_master_disk(writer_session_t *session, const master_image_t *image,
                       void (*progress_cb)(int track, int total, const char *msg, void *ud),
                       void *user_data)
{
    if (!session || !image || !image->track_data) return -1;
    
    int total_tracks = (image->end_track - image->start_track) / 
                       (image->has_halftracks ? 1 : 2) + 1;
    int current = 0;
    int track_inc = image->has_halftracks ? 1 : 2;
    
    /* Turn on motor */
    if (session->motor_on && session->hw_context) {
        session->motor_on(session->hw_context);
    }
    
    /* Iterate tracks */
    int start = session->options.backwards ? image->end_track : image->start_track;
    int end = session->options.backwards ? image->start_track : image->end_track;
    int step = session->options.backwards ? -track_inc : track_inc;
    
    for (int track = start; 
         session->options.backwards ? (track >= end) : (track <= end);
         track += step) {
        
        uint8_t *track_data = image->track_data + (track * WRITER_TRACK_SIZE);
        uint8_t density = image->track_density[track];
        size_t length = image->track_length[track];
        
        /* Check sync flags */
        density = writer_check_sync_flags(track_data, density, length);
        
        /* Handle killer tracks */
        if (density & WRITER_BM_FF_TRACK) {
            writer_kill_track(session, track);
            
            if (progress_cb) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Track %.1f: KILLER", track / 2.0f);
                progress_cb(current++, total_tracks, msg, user_data);
            }
            continue;
        }
        
        /* Handle unformatted tracks */
        if (!writer_check_formatted(track_data, length)) {
            if (!image->has_halftracks) {
                writer_erase_track(session, track);
            }
            
            if (progress_cb) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Track %.1f: UNFORMATTED", track / 2.0f);
                progress_cb(current++, total_tracks, msg, user_data);
            }
            continue;
        }
        
        /* Process track for writing */
        size_t compressed_len = writer_compress_track(track, track_data, density, length);
        
        /* Write track */
        track_write_result_t result;
        writer_write_track(session, track, track_data, compressed_len, density, &result);
        
        /* Verify if enabled */
        if (session->options.verify && result.success) {
            writer_verify_track(session, track, track_data, compressed_len, density, &result);
        }
        
        if (progress_cb) {
            progress_cb(current++, total_tracks, result.message, user_data);
        }
    }
    
    return 0;
}

/**
 * @brief Unformat disk
 */
int writer_unformat_disk(writer_session_t *session, int start_track,
                         int end_track, int passes)
{
    if (!session || start_track < 2 || end_track > 84) return -1;
    
    if (session->motor_on && session->hw_context) {
        session->motor_on(session->hw_context);
    }
    
    if (session->set_density && session->hw_context) {
        session->set_density(session->hw_context, 2);  /* Medium density */
    }
    
    for (int track = start_track; track <= end_track; track++) {
        for (int pass = 0; pass < passes; pass++) {
            writer_erase_track(session, track);
        }
    }
    
    return 0;
}

/**
 * @brief Initialize aligned disk
 */
int writer_init_aligned(writer_session_t *session, int start_track, int end_track)
{
    if (!session || start_track < 2 || end_track > 84) return -1;
    
    /* First wipe with gap bytes */
    for (int track = start_track; track <= end_track; track++) {
        writer_fill_track(session, track, 0x55);
    }
    
    /* Sync sweep */
    if (session->send_cmd && session->hw_context) {
        session->send_cmd(session->hw_context, WRITER_CMD_ALIGNDISK, NULL, 0);
    }
    
    if (session->burst_write && session->hw_context) {
        session->burst_write(session->hw_context, 0x00);
    }
    
    if (session->burst_read && session->hw_context) {
        session->burst_read(session->hw_context);
    }
    
    return 0;
}

/* ============================================================================
 * Verification
 * ============================================================================ */

/**
 * @brief Verify track
 */
int writer_verify_track(writer_session_t *session, int halftrack,
                        const uint8_t *original, size_t original_len,
                        uint8_t density, track_write_result_t *result)
{
    if (!session || !original || !result) return -1;
    
    uint8_t *readback = malloc(WRITER_TRACK_SIZE);
    if (!readback) return -2;
    
    memset(readback, 0, WRITER_TRACK_SIZE);
    
    /* Read track back */
    if (session->send_cmd && session->hw_context) {
        uint8_t read_cmd = WRITER_CMD_READNORMAL;
        
        if (density & WRITER_BM_NO_SYNC) {
            read_cmd = WRITER_CMD_READWOSYNC;
        } else if (session->options.use_ihs) {
            read_cmd = WRITER_CMD_READIHS;
        }
        
        session->send_cmd(session->hw_context, read_cmd, NULL, 0);
    }
    
    if (session->burst_read && session->hw_context) {
        session->burst_read(session->hw_context);
    }
    
    if (session->burst_read_track && session->hw_context) {
        session->burst_read_track(session->hw_context, readback, WRITER_TRACK_SIZE);
    }
    
    /* Compare tracks */
    size_t diff_count = 0;
    size_t compare_len = (original_len < WRITER_TRACK_SIZE) ? original_len : WRITER_TRACK_SIZE;
    
    for (size_t i = 0; i < compare_len; i++) {
        if (readback[i] != original[i]) {
            diff_count++;
        }
    }
    
    result->gcr_diff = diff_count;
    
    /* Determine result */
    int tolerance = sector_map[halftrack / 2] + session->options.verify_tol;
    
    if (diff_count <= (size_t)tolerance) {
        result->verify_result = WRITER_VERIFY_OK;
        result->success = true;
    } else {
        /* Check for weak bits */
        size_t bad_gcr = 0;
        for (size_t i = 0; i < compare_len; i++) {
            uint8_t b = readback[i];
            /* Simple bad GCR check */
            if ((b & 0xF0) == 0x00 || (b & 0xF0) == 0xF0 ||
                (b & 0x0F) == 0x00 || (b & 0x0F) == 0x0F) {
                bad_gcr++;
            }
        }
        result->bad_gcr = bad_gcr;
        
        if (diff_count <= bad_gcr) {
            result->verify_result = WRITER_VERIFY_WEAK_OK;
            result->success = true;
        } else {
            result->verify_result = WRITER_VERIFY_RETRY;
            result->success = false;
        }
    }
    
    free(readback);
    return result->success ? 0 : -1;
}

/* ============================================================================
 * Image Management
 * ============================================================================ */

/**
 * @brief Create master image
 */
master_image_t *writer_create_image(uint8_t *track_data, uint8_t *track_density,
                                    size_t *track_length, int start_track, int end_track)
{
    if (!track_data || !track_density || !track_length) return NULL;
    
    master_image_t *image = calloc(1, sizeof(master_image_t));
    if (!image) return NULL;
    
    image->track_data = track_data;
    image->track_density = track_density;
    image->track_length = track_length;
    image->start_track = start_track;
    image->end_track = end_track;
    image->num_tracks = (end_track - start_track) / 2 + 1;
    image->has_halftracks = false;
    
    return image;
}

/**
 * @brief Free master image
 */
void writer_free_image(master_image_t *image)
{
    free(image);  /* Does not free track buffers */
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get default density
 */
uint8_t writer_default_density(int track)
{
    if (track < 1 || track > 42) return 0;
    return speed_map[track];
}

/**
 * @brief Get default capacity
 */
size_t writer_default_capacity(int density)
{
    if (density < 0 || density > 3) return 0;
    return default_capacity[density];
}

/**
 * @brief Calculate RPM
 */
float writer_calc_rpm(int capacity, int density)
{
    if (capacity <= 0 || density < 0 || density > 3) return 0;
    return density_const[density] / capacity;
}

/**
 * @brief Get sectors per track
 */
int writer_sectors_per_track(int track)
{
    if (track < 1 || track > 42) return 0;
    return sector_map[track];
}

/* ============================================================================
 * Null Session (for testing)
 * ============================================================================ */

static int null_send_cmd(void *ctx, uint8_t cmd, const uint8_t *data, size_t len)
{
    (void)ctx; (void)cmd; (void)data; (void)len;
    return 0;
}

static int null_burst_rw(void *ctx)
{
    (void)ctx;
    return 0;
}

static int null_burst_write_byte(void *ctx, uint8_t byte)
{
    (void)ctx; (void)byte;
    return 0;
}

static int null_burst_write_track(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx; (void)data; (void)len;
    return 0;
}

static int null_burst_read_track(void *ctx, uint8_t *data, size_t max_len)
{
    (void)ctx;
    if (data && max_len > 0) {
        memset(data, 0x55, max_len);
    }
    return 0;
}

static int null_step_to(void *ctx, int halftrack)
{
    (void)ctx; (void)halftrack;
    return 0;
}

static int null_set_density(void *ctx, uint8_t density)
{
    (void)ctx; (void)density;
    return 0;
}

static int null_motor(void *ctx)
{
    (void)ctx;
    return 0;
}

static int null_track_capacity(void *ctx)
{
    (void)ctx;
    return 7000;  /* Default capacity */
}

/**
 * @brief Create null session
 */
writer_session_t *writer_create_null_session(void)
{
    writer_session_t *session = writer_create_session();
    if (!session) return NULL;
    
    session->hw_context = NULL;
    session->send_cmd = null_send_cmd;
    session->burst_read = null_burst_rw;
    session->burst_write = null_burst_write_byte;
    session->burst_write_track = null_burst_write_track;
    session->burst_read_track = null_burst_read_track;
    session->step_to = null_step_to;
    session->set_density = null_set_density;
    session->motor_on = null_motor;
    session->motor_off = null_motor;
    session->track_capacity = null_track_capacity;
    
    return session;
}
