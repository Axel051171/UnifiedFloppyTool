/**
 * @file uft_ihs.c
 * @brief Index Hole Sensor (IHS) Support Implementation
 * 
 * Based on nibtools ihs.c by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/hardware/uft_ihs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Result names */
static const char *result_names[] = {
    "IHS Detected",
    "Index hole not detected",
    "IHS disabled",
    "IHS hardware not present",
    "Operation timed out",
    "Unknown error"
};

/** Default bitrate per track */
static const uint8_t default_bitrate[43] = {
    0,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /*  1 - 10 */
    3, 3, 3, 3, 3, 3, 3, 2, 2, 2,  /* 11 - 20 */
    2, 2, 2, 2, 1, 1, 1, 1, 1, 1,  /* 21 - 30 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 31 - 40 */
    0, 0                           /* 41 - 42 */
};

/* ============================================================================
 * Session Management
 * ============================================================================ */

/**
 * @brief Create IHS session
 */
ihs_session_t *ihs_session_create(ihs_interface_t *iface)
{
    if (!iface) return NULL;
    
    ihs_session_t *session = calloc(1, sizeof(ihs_session_t));
    if (!session) return NULL;
    
    session->iface = iface;
    session->timeout_ms = IHS_DEFAULT_TIMEOUT;
    session->current_track = -1;
    
    return session;
}

/**
 * @brief Close IHS session
 */
void ihs_session_close(ihs_session_t *session)
{
    if (!session) return;
    
    /* Disable IHS if enabled */
    if (session->ihs_enabled && session->iface) {
        ihs_disable(session);
    }
    
    /* Close interface */
    if (session->iface && session->iface->close) {
        session->iface->close(session->iface);
    }
    
    free(session);
}

/**
 * @brief Set timeout
 */
void ihs_set_timeout(ihs_session_t *session, int timeout_ms)
{
    if (session) {
        session->timeout_ms = timeout_ms;
    }
}

/* ============================================================================
 * IHS Control
 * ============================================================================ */

/**
 * @brief Check IHS presence
 */
ihs_result_t ihs_check_presence(ihs_session_t *session, bool keep_on)
{
    if (!session || !session->iface) return IHS_RESULT_ERROR;
    
    ihs_result_t result = IHS_RESULT_ERROR;
    
    /* Enable motor */
    if (session->iface->motor_on) {
        session->iface->motor_on(session->iface);
    }
    
    /* Turn IHS on */
    if (session->iface->send_cmd) {
        session->iface->send_cmd(session->iface, IHS_CMD_ON, NULL, 0);
    }
    
    /* Check presence */
    if (session->iface->send_cmd && session->iface->read_byte) {
        session->iface->send_cmd(session->iface, IHS_CMD_PRESENT, NULL, 0);
        
        uint8_t status;
        if (session->iface->read_byte(session->iface, &status) == 0) {
            switch (status) {
                case IHS_STATUS_OK:
                    result = IHS_RESULT_DETECTED;
                    break;
                case IHS_STATUS_NO_HOLE:
                    result = IHS_RESULT_NO_HOLE;
                    break;
                case IHS_STATUS_DISABLED:
                    result = IHS_RESULT_DISABLED;
                    break;
                default:
                    result = IHS_RESULT_ERROR;
            }
        } else {
            result = IHS_RESULT_TIMEOUT;
        }
    }
    
    /* Turn off if requested */
    if (!keep_on && session->iface->send_cmd) {
        session->iface->send_cmd(session->iface, IHS_CMD_OFF, NULL, 0);
    } else if (result == IHS_RESULT_DETECTED) {
        session->ihs_enabled = true;
    }
    
    session->last_result = result;
    return result;
}

/**
 * @brief Enable IHS
 */
int ihs_enable(ihs_session_t *session)
{
    if (!session || !session->iface) return -1;
    
    if (session->iface->send_cmd) {
        int ret = session->iface->send_cmd(session->iface, IHS_CMD_ON, NULL, 0);
        if (ret == 0) {
            session->ihs_enabled = true;
        }
        return ret;
    }
    
    return -1;
}

/**
 * @brief Disable IHS
 */
int ihs_disable(ihs_session_t *session)
{
    if (!session || !session->iface) return -1;
    
    if (session->iface->send_cmd) {
        int ret = session->iface->send_cmd(session->iface, IHS_CMD_OFF, NULL, 0);
        session->ihs_enabled = false;
        return ret;
    }
    
    return -1;
}

/**
 * @brief Check if enabled
 */
bool ihs_is_enabled(const ihs_session_t *session)
{
    return session ? session->ihs_enabled : false;
}

/* ============================================================================
 * Track Reading
 * ============================================================================ */

/**
 * @brief Read track with IHS
 */
int ihs_read_track(ihs_session_t *session, int halftrack,
                   uint8_t *buffer, size_t max_len,
                   size_t *actual_len, uint8_t *density)
{
    if (!session || !session->iface || !buffer) return -1;
    if (halftrack < 2 || halftrack > 84) return -2;
    
    /* Step to track */
    if (session->iface->step_to) {
        session->iface->step_to(session->iface, halftrack);
        session->current_track = halftrack;
    }
    
    /* Scan density if needed */
    uint8_t track_density = ihs_default_bitrate(halftrack / 2);
    if (density) {
        ihs_scan_density(session, halftrack, &track_density);
        *density = track_density;
    }
    
    /* Set density */
    if (session->iface->set_density) {
        session->iface->set_density(session->iface, track_density & 0x03);
        session->current_density = track_density;
    }
    
    /* Read track */
    if (session->iface->send_cmd && session->iface->read_track) {
        session->iface->send_cmd(session->iface, IHS_CMD_READ_SCP, NULL, 0);
        
        size_t len;
        int ret = session->iface->read_track(session->iface, buffer, max_len, &len);
        
        if (actual_len) *actual_len = len;
        
        return ret;
    }
    
    return -1;
}

/**
 * @brief Scan track density
 */
int ihs_scan_density(ihs_session_t *session, int halftrack, uint8_t *density)
{
    if (!session || !density) return -1;
    
    /* For now, just return default density */
    /* Real implementation would do multiple reads at different densities */
    *density = ihs_default_bitrate(halftrack / 2);
    
    return 0;
}

/* ============================================================================
 * Track Analysis
 * ============================================================================ */

/**
 * @brief Analyze buffer for sync patterns
 */
int ihs_analyze_buffer(const uint8_t *track_data, size_t track_len,
                       ihs_track_analysis_t *result)
{
    if (!track_data || !result || track_len == 0) return -1;
    
    memset(result, 0, sizeof(ihs_track_analysis_t));
    
    /* Check for killer track (mostly 0xFF) */
    int sync_count = 0;
    for (size_t i = 0; i < track_len; i++) {
        if (track_data[i] == 0xFF) sync_count++;
    }
    
    if (sync_count > (int)(track_len * 90 / 100)) {
        result->is_killer = true;
        return 0;
    }
    
    /* Find first sync */
    size_t sync_start = 0;
    while (sync_start < track_len && track_data[sync_start] != 0xFF) {
        sync_start++;
    }
    
    if (sync_start >= track_len) {
        result->no_sync = true;
        return 0;
    }
    
    /* Count pre-sync bytes */
    result->pre_sync_lo = (int)sync_start;
    
    /* Find end of sync */
    size_t sync_end = sync_start;
    while (sync_end < track_len && track_data[sync_end] == 0xFF) {
        sync_end++;
    }
    
    /* Count syncs in track */
    int syncs = 0;
    size_t pos = 0;
    while (pos < track_len) {
        if (track_data[pos] == 0xFF) {
            syncs++;
            while (pos < track_len && track_data[pos] == 0xFF) pos++;
        } else {
            pos++;
        }
    }
    result->sync_count_lo = syncs;
    
    /* Get first 5 data bytes after sync */
    if (sync_end < track_len - 5) {
        memcpy(result->data_bytes, track_data + sync_end, 5);
    }
    
    return 0;
}

/**
 * @brief Analyze single track
 */
int ihs_analyze_track(ihs_session_t *session, int halftrack,
                      ihs_track_analysis_t *result)
{
    if (!session || !result) return -1;
    
    uint8_t buffer[IHS_MAX_TRACK_SIZE];
    size_t actual_len;
    uint8_t density;
    
    int ret = ihs_read_track(session, halftrack, buffer, sizeof(buffer),
                             &actual_len, &density);
    if (ret != 0) return ret;
    
    ret = ihs_analyze_buffer(buffer, actual_len, result);
    if (ret != 0) return ret;
    
    result->track = halftrack / 2;
    result->halftrack = halftrack;
    result->bitrate = density;
    
    return 0;
}

/**
 * @brief Generate alignment report
 */
int ihs_alignment_report(ihs_session_t *session,
                         int start_track, int end_track,
                         bool include_halftracks,
                         ihs_alignment_report_t *report)
{
    if (!session || !report) return -1;
    
    memset(report, 0, sizeof(ihs_alignment_report_t));
    report->has_halftracks = include_halftracks;
    
    /* Check IHS presence first */
    ihs_result_t ihs_result = ihs_check_presence(session, true);
    if (ihs_result != IHS_RESULT_DETECTED) {
        snprintf(report->description, sizeof(report->description),
                 "IHS Error: %s", ihs_result_name(ihs_result));
        return -2;
    }
    
    int track_inc = include_halftracks ? 1 : 2;
    int track_idx = 0;
    
    for (int ht = start_track; ht <= end_track && track_idx < 84; ht += track_inc) {
        ihs_track_analysis_t *analysis = &report->tracks[track_idx];
        
        int ret = ihs_analyze_track(session, ht, analysis);
        if (ret == 0) {
            track_idx++;
        }
    }
    
    report->num_tracks = track_idx;
    
    snprintf(report->description, sizeof(report->description),
             "Analyzed %d %s from track %d to %d",
             report->num_tracks,
             include_halftracks ? "half-tracks" : "tracks",
             start_track / 2, end_track / 2);
    
    return 0;
}

/**
 * @brief Deep bitrate analysis
 */
int ihs_deep_bitrate_analysis(ihs_session_t *session, int halftrack,
                              ihs_bitrate_analysis_t *result)
{
    if (!session || !result) return -1;
    
    memset(result, 0, sizeof(ihs_bitrate_analysis_t));
    result->track = halftrack / 2;
    
    /* Read track at each density multiple times */
    uint8_t buffer[IHS_MAX_TRACK_SIZE];
    
    for (int pass = 0; pass < IHS_DBR_PASSES; pass++) {
        uint8_t test_density = pass % 4;
        
        if (session->iface->set_density) {
            session->iface->set_density(session->iface, test_density);
        }
        
        size_t len;
        if (ihs_read_track(session, halftrack, buffer, sizeof(buffer), &len, NULL) == 0) {
            /* Simple heuristic: check for valid sync patterns */
            int valid = 0;
            for (size_t i = 0; i < len && i < 1000; i++) {
                if (buffer[i] == 0xFF) valid++;
            }
            
            /* If we got good data, record this density */
            if (valid > 10) {
                result->densities[pass] = test_density;
                result->density_counts[test_density]++;
            }
        }
    }
    
    /* Find best density */
    int best_count = 0;
    for (int d = 0; d < 4; d++) {
        if (result->density_counts[d] > best_count) {
            best_count = result->density_counts[d];
            result->best_density = d;
        }
    }
    
    result->confidence = (float)best_count * 100.0f / IHS_DBR_PASSES;
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get result name
 */
const char *ihs_result_name(ihs_result_t result)
{
    if (result <= IHS_RESULT_ERROR) {
        return result_names[result];
    }
    return result_names[IHS_RESULT_ERROR];
}

/**
 * @brief Print report
 */
void ihs_print_report(const ihs_alignment_report_t *report, FILE *fp)
{
    if (!report || !fp) return;
    
    fprintf(fp, "\n=== IHS Track Alignment Report ===\n");
    fprintf(fp, "%s\n\n", report->description);
    
    if (report->has_halftracks) {
        fprintf(fp, "    |           Full Track           |       Half Track (+0.5)       \n");
        fprintf(fp, " #T +--------------------------------+-------------------------------\n");
        fprintf(fp, " RA |      pre  #sync   data BYTEs   |      pre  #sync   data BYTEs  \n");
        fprintf(fp, " CK | BR  lo hi lo hi A1 A2 A3 A4 A5 | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
        fprintf(fp, "----+--------------------------------+-------------------------------\n");
    } else {
        fprintf(fp, "    |             Full Track        \n");
        fprintf(fp, "    |      pre  #sync   data BYTEs  \n");
        fprintf(fp, "    | BR  lo hi lo hi A1 A2 A3 A4 A5\n");
        fprintf(fp, "----+-------------------------------\n");
    }
    
    for (int i = 0; i < report->num_tracks; i++) {
        const ihs_track_analysis_t *t = &report->tracks[i];
        
        if (t->halftrack % 2 == 0) {
            fprintf(fp, " %2d ", t->track);
        }
        
        if (t->is_killer) {
            fprintf(fp, "| <*> KILLER                     ");
        } else if (t->no_sync) {
            fprintf(fp, "| <*> NOSYNC                     ");
        } else {
            fprintf(fp, "| <%d> %2d %2d %2d %2d %02X %02X %02X %02X %02X ",
                    t->bitrate & 3,
                    t->pre_sync_lo, t->pre_sync_hi,
                    t->sync_count_lo, t->sync_count_hi,
                    t->data_bytes[0], t->data_bytes[1], t->data_bytes[2],
                    t->data_bytes[3], t->data_bytes[4]);
        }
        
        if (t->halftrack % 2 == 1 || !report->has_halftracks) {
            fprintf(fp, "\n");
        }
    }
    
    fprintf(fp, "\n");
}

/**
 * @brief Get default bitrate
 */
uint8_t ihs_default_bitrate(int track)
{
    if (track < 1 || track > 42) return 0;
    return default_bitrate[track];
}

/**
 * @brief Find index position
 */
int ihs_find_index_position(const uint8_t *track_data, size_t track_len, int track)
{
    if (!track_data || track_len == 0) return -1;
    
    /* Find first sync mark - this is often near the index hole */
    for (size_t i = 0; i < track_len - 1; i++) {
        if (track_data[i] == 0xFF && track_data[i + 1] == 0xFF) {
            return (int)i;
        }
    }
    
    return -1;
}

/**
 * @brief Rotate track to index
 */
int ihs_rotate_to_index(uint8_t *track_data, size_t track_len, size_t index_pos)
{
    if (!track_data || track_len == 0 || index_pos >= track_len) return -1;
    
    /* Simple rotation using temp buffer */
    uint8_t *temp = malloc(track_len);
    if (!temp) return -2;
    
    /* Copy from index_pos to end */
    memcpy(temp, track_data + index_pos, track_len - index_pos);
    
    /* Copy from start to index_pos */
    memcpy(temp + (track_len - index_pos), track_data, index_pos);
    
    /* Copy back */
    memcpy(track_data, temp, track_len);
    
    free(temp);
    return 0;
}

/* ============================================================================
 * Null/Simulation Interfaces
 * ============================================================================ */

/**
 * @brief Null interface implementation
 */
typedef struct {
    ihs_interface_t base;
    uint8_t dummy_density;
} null_interface_t;

static int null_send_cmd(ihs_interface_t *iface, uint8_t cmd, 
                         const uint8_t *data, size_t len)
{
    (void)iface; (void)cmd; (void)data; (void)len;
    return 0;
}

static int null_read_byte(ihs_interface_t *iface, uint8_t *byte)
{
    (void)iface;
    if (byte) *byte = IHS_STATUS_OK;
    return 0;
}

static int null_read_track(ihs_interface_t *iface, uint8_t *buffer,
                           size_t max_len, size_t *actual_len)
{
    (void)iface;
    if (buffer && max_len > 0) {
        memset(buffer, 0x55, max_len);
        if (max_len > 10) memset(buffer, 0xFF, 10);  /* Add sync */
    }
    if (actual_len) *actual_len = max_len;
    return 0;
}

static int null_motor_on(ihs_interface_t *iface)
{
    (void)iface;
    return 0;
}

static int null_motor_off(ihs_interface_t *iface)
{
    (void)iface;
    return 0;
}

static int null_step_to(ihs_interface_t *iface, int halftrack)
{
    (void)iface; (void)halftrack;
    return 0;
}

static int null_set_density(ihs_interface_t *iface, uint8_t density)
{
    null_interface_t *null_if = (null_interface_t *)iface;
    null_if->dummy_density = density;
    return 0;
}

static void null_close(ihs_interface_t *iface)
{
    free(iface);
}

/**
 * @brief Create null interface
 */
ihs_interface_t *ihs_create_null_interface(void)
{
    null_interface_t *iface = calloc(1, sizeof(null_interface_t));
    if (!iface) return NULL;
    
    iface->base.context = NULL;
    iface->base.send_cmd = null_send_cmd;
    iface->base.read_byte = null_read_byte;
    iface->base.read_track = null_read_track;
    iface->base.motor_on = null_motor_on;
    iface->base.motor_off = null_motor_off;
    iface->base.step_to = null_step_to;
    iface->base.set_density = null_set_density;
    iface->base.close = null_close;
    
    return &iface->base;
}

/**
 * @brief Simulation interface implementation
 */
typedef struct {
    ihs_interface_t base;
    uint8_t **track_data;
    int num_tracks;
    int current_track;
} sim_interface_t;

static int sim_read_track(ihs_interface_t *iface, uint8_t *buffer,
                          size_t max_len, size_t *actual_len)
{
    sim_interface_t *sim = (sim_interface_t *)iface;
    
    if (sim->current_track >= 0 && sim->current_track < sim->num_tracks &&
        sim->track_data[sim->current_track]) {
        
        size_t len = (max_len < IHS_MAX_TRACK_SIZE) ? max_len : IHS_MAX_TRACK_SIZE;
        memcpy(buffer, sim->track_data[sim->current_track], len);
        if (actual_len) *actual_len = len;
        return 0;
    }
    
    /* Return empty track */
    if (buffer && max_len > 0) {
        memset(buffer, 0x55, max_len);
    }
    if (actual_len) *actual_len = max_len;
    return 0;
}

static int sim_step_to(ihs_interface_t *iface, int halftrack)
{
    sim_interface_t *sim = (sim_interface_t *)iface;
    sim->current_track = halftrack;
    return 0;
}

static void sim_close(ihs_interface_t *iface)
{
    free(iface);
}

/**
 * @brief Create simulation interface
 */
ihs_interface_t *ihs_create_sim_interface(uint8_t **track_data, int num_tracks)
{
    sim_interface_t *iface = calloc(1, sizeof(sim_interface_t));
    if (!iface) return NULL;
    
    iface->base.context = NULL;
    iface->base.send_cmd = null_send_cmd;
    iface->base.read_byte = null_read_byte;
    iface->base.read_track = sim_read_track;
    iface->base.motor_on = null_motor_on;
    iface->base.motor_off = null_motor_off;
    iface->base.step_to = sim_step_to;
    iface->base.set_density = null_set_density;
    iface->base.close = sim_close;
    
    iface->track_data = track_data;
    iface->num_tracks = num_tracks;
    iface->current_track = -1;
    
    return &iface->base;
}
