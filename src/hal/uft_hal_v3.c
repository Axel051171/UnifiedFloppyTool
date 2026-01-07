/**
 * @file uft_hal.c
 * @brief Hardware Abstraction Layer Implementation
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/hal/uft_hal.h"
#include "uft/hal/uft_greaseweazle.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_hal_device {
    uft_hal_controller_t type;
    uft_hal_info_t info;
    uft_hal_drive_profile_t profile;
    uint8_t current_unit;
    
    /* Controller-specific handle */
    union {
        uft_gw_device_t* greaseweazle;
        void* generic;
    } handle;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void* safe_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    if (count > SIZE_MAX / size) return NULL;
    return calloc(count, size);
}

static int uft_gw_to_hal_error(int uft_gw_err) {
    switch (uft_gw_err) {
        case UFT_GW_OK:              return UFT_HAL_OK;
        case UFT_GW_ERR_NOT_FOUND:   return UFT_HAL_ERR_NOT_FOUND;
        case UFT_GW_ERR_OPEN_FAILED: return UFT_HAL_ERR_OPEN_FAILED;
        case UFT_GW_ERR_IO:          return UFT_HAL_ERR_IO;
        case UFT_GW_ERR_TIMEOUT:     return UFT_HAL_ERR_TIMEOUT;
        case UFT_GW_ERR_NO_INDEX:    return UFT_HAL_ERR_NO_INDEX;
        case UFT_GW_ERR_NO_TRK0:     return UFT_HAL_ERR_NO_TRK0;
        case UFT_GW_ERR_OVERFLOW:    return UFT_HAL_ERR_OVERFLOW;
        case UFT_GW_ERR_WRPROT:      return UFT_HAL_ERR_WRPROT;
        case UFT_GW_ERR_INVALID:     return UFT_HAL_ERR_INVALID;
        case UFT_GW_ERR_NOMEM:       return UFT_HAL_ERR_NOMEM;
        case UFT_GW_ERR_NOT_CONNECTED: return UFT_HAL_ERR_NOT_CONNECTED;
        default:                 return UFT_HAL_ERR_IO;
    }
}

static int uft_gw_flux_to_ir_track(const uft_gw_flux_data_t* flux, uint8_t cylinder,
                                uint8_t head, uft_ir_track_t** track_out) {
    if (!flux || !track_out) return UFT_HAL_ERR_INVALID;
    
    uft_ir_track_t* track = uft_ir_track_create(cylinder, head);
    if (!track) return UFT_HAL_ERR_NOMEM;
    
    /* Set basic track info */
    track->encoding = UFT_IR_ENC_UNKNOWN;  /* Will be detected later */
    
    /* Split flux data into revolutions using index times */
    if (flux->index_count > 1) {
        uint32_t start_sample = 0;
        uint32_t current_tick = 0;
        uint32_t sample_idx = 0;
        
        for (int rev = 0; rev < flux->index_count - 1 && rev < UFT_IR_MAX_REVOLUTIONS; rev++) {
            uint32_t index_start = flux->index_times[rev];
            uint32_t index_end = flux->index_times[rev + 1];
            
            /* Count samples in this revolution */
            uint32_t rev_samples = 0;
            uint32_t scan_tick = 0;
            uint32_t scan_idx = 0;
            
            /* Find start sample for this revolution */
            while (scan_idx < flux->sample_count && scan_tick < index_start) {
                scan_tick += flux->samples[scan_idx++];
            }
            uint32_t rev_start_idx = scan_idx;
            
            /* Count samples until end of revolution */
            while (scan_idx < flux->sample_count && scan_tick < index_end) {
                scan_tick += flux->samples[scan_idx++];
                rev_samples++;
            }
            
            if (rev_samples == 0) continue;
            
            /* Create revolution */
            uft_ir_revolution_t* ir_rev = uft_ir_revolution_create(rev_samples);
            if (!ir_rev) continue;
            
            /* Copy flux data, converting ticks to nanoseconds */
            uint64_t total_ns = 0;
            for (uint32_t i = 0; i < rev_samples && (rev_start_idx + i) < flux->sample_count; i++) {
                uint32_t ticks = flux->samples[rev_start_idx + i];
                uint32_t ns = uft_gw_ticks_to_ns(ticks, flux->sample_freq);
                ir_rev->flux_deltas[i] = ns;
                total_ns += ns;
            }
            ir_rev->flux_count = rev_samples;
            ir_rev->data_size = rev_samples * sizeof(uint32_t);
            ir_rev->duration_ns = (uint32_t)(total_ns & 0xFFFFFFFF);
            ir_rev->flags = UFT_IR_RF_INDEX_START | UFT_IR_RF_INDEX_END | UFT_IR_RF_COMPLETE;
            
            /* Calculate statistics */
            uft_ir_revolution_calc_stats(ir_rev);
            
            /* Add to track */
            uft_ir_track_add_revolution(track, ir_rev);
        }
    } else {
        /* No index times - treat all data as one revolution */
        uft_ir_revolution_t* ir_rev = uft_ir_revolution_create(flux->sample_count);
        if (!ir_rev) {
            uft_ir_track_free(track);
            return UFT_HAL_ERR_NOMEM;
        }
        
        uint64_t total_ns = 0;
        for (uint32_t i = 0; i < flux->sample_count; i++) {
            uint32_t ticks = flux->samples[i];
            uint32_t ns = uft_gw_ticks_to_ns(ticks, flux->sample_freq);
            ir_rev->flux_deltas[i] = ns;
            total_ns += ns;
        }
        ir_rev->flux_count = flux->sample_count;
        ir_rev->data_size = flux->sample_count * sizeof(uint32_t);
        ir_rev->duration_ns = (uint32_t)(total_ns & 0xFFFFFFFF);
        
        uft_ir_revolution_calc_stats(ir_rev);
        uft_ir_track_add_revolution(track, ir_rev);
    }
    
    /* Detect encoding from first revolution */
    if (track->revolution_count > 0 && track->revolutions[0]) {
        uint8_t confidence;
        track->encoding = uft_ir_detect_encoding(track->revolutions[0], &confidence);
    }
    
    /* Calculate track quality */
    uft_ir_calc_quality(track);
    
    /* Find best revolution */
    track->best_revolution = (uint8_t)uft_ir_find_best_revolution(track);
    
    *track_out = track;
    return UFT_HAL_OK;
}

static int ir_track_to_gw_flux(const uft_ir_track_t* track, uint32_t sample_freq,
                                uint32_t** samples, uint32_t* sample_count) {
    if (!track || !samples || !sample_count) return UFT_HAL_ERR_INVALID;
    
    /* Use best revolution or first available */
    int rev_idx = track->best_revolution;
    if (rev_idx < 0 || rev_idx >= track->revolution_count) {
        rev_idx = 0;
    }
    
    const uft_ir_revolution_t* rev = track->revolutions[rev_idx];
    if (!rev || rev->flux_count == 0) {
        return UFT_HAL_ERR_INVALID;
    }
    
    /* Allocate output */
    uint32_t* out = (uint32_t*)safe_calloc(rev->flux_count, sizeof(uint32_t));
    if (!out) return UFT_HAL_ERR_NOMEM;
    
    /* Convert nanoseconds back to ticks */
    for (uint32_t i = 0; i < rev->flux_count; i++) {
        out[i] = uft_gw_ns_to_ticks(rev->flux_deltas[i], sample_freq);
    }
    
    *samples = out;
    *sample_count = rev->flux_count;
    return UFT_HAL_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEVICE DISCOVERY
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Discovery context for collecting devices */
typedef struct {
    uft_hal_discover_cb user_callback;
    void* user_data;
    uft_hal_info_t* array;
    int max_count;
    int count;
} discover_ctx_t;

static void uft_gw_discover_callback(void* user_data, const char* port, const uft_gw_info_t* uft_gw_info) {
    discover_ctx_t* ctx = (discover_ctx_t*)user_data;
    
    uft_hal_info_t info;
    memset(&info, 0, sizeof(info));
    
    info.type = UFT_HAL_GREASEWEAZLE;
    snprintf(info.name, sizeof(info.name), "Greaseweazle F%d", uft_gw_info->hw_model);
    snprintf(info.version, sizeof(info.version), "%d.%d", uft_gw_info->fw_major, uft_gw_info->fw_minor);
    strncpy(info.serial, uft_gw_info->serial, sizeof(info.serial) - 1);
    strncpy(info.port, port, sizeof(info.port) - 1);
    info.sample_freq = uft_gw_info->sample_freq;
    info.max_drives = 2;
    info.can_write = true;
    info.supports_hd = true;
    info.supports_ed = (uft_gw_info->hw_model >= 7);
    
    if (ctx->user_callback) {
        ctx->user_callback(ctx->user_data, &info);
    }
    
    if (ctx->array && ctx->count < ctx->max_count) {
        ctx->array[ctx->count++] = info;
    } else {
        ctx->count++;
    }
}

int uft_hal_discover(uft_hal_discover_cb callback, void* user_data) {
    discover_ctx_t ctx = {
        .user_callback = callback,
        .user_data = user_data,
        .array = NULL,
        .max_count = 0,
        .count = 0
    };
    
    uft_gw_discover(uft_gw_discover_callback, &ctx);
    
    /* TODO: Add other controller discovery */
    
    return ctx.count;
}

int uft_hal_list(uft_hal_info_t* infos, int max_count) {
    discover_ctx_t ctx = {
        .user_callback = NULL,
        .user_data = NULL,
        .array = infos,
        .max_count = max_count,
        .count = 0
    };
    
    uft_gw_discover(uft_gw_discover_callback, &ctx);
    
    return ctx.count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEVICE CONNECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_hal_open(uft_hal_controller_t type, const char* port,
                 uft_hal_device_t** device) {
    if (!device) return UFT_HAL_ERR_INVALID;
    
    *device = NULL;
    
    uft_hal_device_t* dev = (uft_hal_device_t*)safe_calloc(1, sizeof(uft_hal_device_t));
    if (!dev) return UFT_HAL_ERR_NOMEM;
    
    dev->type = type;
    
    int ret;
    
    switch (type) {
        case UFT_HAL_GREASEWEAZLE: {
            uft_gw_device_t* gw;
            if (port) {
                ret = uft_gw_open(port, &gw);
            } else {
                ret = uft_gw_open_first(&gw);
            }
            
            if (ret != UFT_GW_OK) {
                free(dev);
                return uft_gw_to_hal_error(ret);
            }
            
            dev->handle.greaseweazle = gw;
            
            /* Get device info */
            uft_gw_info_t uft_gw_info;
            uft_gw_get_info(gw, &uft_gw_info);
            
            dev->info.type = UFT_HAL_GREASEWEAZLE;
            snprintf(dev->info.name, sizeof(dev->info.name), "Greaseweazle F%d", uft_gw_info.hw_model);
            snprintf(dev->info.version, sizeof(dev->info.version), "%d.%d", 
                     uft_gw_info.fw_major, uft_gw_info.fw_minor);
            strncpy(dev->info.serial, uft_gw_info.serial, sizeof(dev->info.serial) - 1);
            dev->info.sample_freq = uft_gw_info.sample_freq;
            dev->info.max_drives = 2;
            dev->info.can_write = true;
            dev->info.supports_hd = true;
            dev->info.supports_ed = (uft_gw_info.hw_model >= 7);
            break;
        }
        
        case UFT_HAL_FLUXENGINE:
        case UFT_HAL_KRYOFLUX:
        case UFT_HAL_FC5025:
        case UFT_HAL_XUM1541:
            free(dev);
            return UFT_HAL_ERR_UNSUPPORTED;
        
        default:
            free(dev);
            return UFT_HAL_ERR_INVALID;
    }
    
    *device = dev;
    return UFT_HAL_OK;
}

int uft_hal_open_first(uft_hal_device_t** device) {
    int ret = uft_hal_open(UFT_HAL_GREASEWEAZLE, NULL, device);
    if (ret == UFT_HAL_OK) return ret;
    
    /* TODO: Try other controllers */
    
    return UFT_HAL_ERR_NOT_FOUND;
}

void uft_hal_close(uft_hal_device_t* device) {
    if (!device) return;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE:
            uft_gw_close(device->handle.greaseweazle);
            break;
        default:
            break;
    }
    
    free(device);
}

int uft_hal_get_info(uft_hal_device_t* device, uft_hal_info_t* info) {
    if (!device || !info) return UFT_HAL_ERR_INVALID;
    *info = device->info;
    return UFT_HAL_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DRIVE CONTROL
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_hal_select_drive(uft_hal_device_t* device, uint8_t unit) {
    if (!device) return UFT_HAL_ERR_NOT_CONNECTED;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE:
            return uft_gw_to_hal_error(uft_gw_select_drive(device->handle.greaseweazle, unit));
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

int uft_hal_set_profile(uft_hal_device_t* device, uft_hal_drive_profile_t profile) {
    if (!device) return UFT_HAL_ERR_NOT_CONNECTED;
    device->profile = profile;
    
    /* Apply profile-specific settings */
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE: {
            uft_gw_delays_t delays;
            uft_gw_get_delays(device->handle.greaseweazle, &delays);
            
            /* Adjust delays based on profile */
            switch (profile) {
                case UFT_HAL_DRIVE_35_DD:
                case UFT_HAL_DRIVE_35_HD:
                    delays.settle_delay_ms = 15;
                    delays.step_delay_us = 3000;
                    break;
                case UFT_HAL_DRIVE_525_DD:
                case UFT_HAL_DRIVE_525_HD:
                    delays.settle_delay_ms = 20;
                    delays.step_delay_us = 6000;
                    break;
                case UFT_HAL_DRIVE_C64_1541:
                case UFT_HAL_DRIVE_AMIGA_DD:
                    delays.settle_delay_ms = 18;
                    delays.step_delay_us = 3000;
                    break;
                default:
                    break;
            }
            
            uft_gw_set_delays(device->handle.greaseweazle, &delays);
            break;
        }
        default:
            break;
    }
    
    return UFT_HAL_OK;
}

int uft_hal_recalibrate(uft_hal_device_t* device) {
    if (!device) return UFT_HAL_ERR_NOT_CONNECTED;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE:
            return uft_gw_to_hal_error(uft_gw_recalibrate(device->handle.greaseweazle));
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

bool uft_hal_is_write_protected(uft_hal_device_t* device) {
    if (!device) return true;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE:
            return uft_gw_is_write_protected(device->handle.greaseweazle);
        default:
            return true;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * READING - UFT-IR OUTPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_hal_read_track(uft_hal_device_t* device, uint8_t cylinder, uint8_t head,
                       uint8_t revolutions, uft_ir_track_t** track) {
    if (!device || !track) return UFT_HAL_ERR_INVALID;
    
    *track = NULL;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE: {
            uft_gw_flux_data_t* flux;
            int ret = uft_gw_read_track(device->handle.greaseweazle, cylinder, head,
                                    revolutions, &flux);
            if (ret != UFT_GW_OK) {
                return uft_gw_to_hal_error(ret);
            }
            
            ret = uft_gw_flux_to_ir_track(flux, cylinder, head, track);
            uft_gw_flux_free(flux);
            return ret;
        }
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

int uft_hal_read_disk(uft_hal_device_t* device, const uft_hal_read_params_t* params,
                      uft_hal_progress_cb progress, void* user_data,
                      uft_ir_disk_t** disk) {
    if (!device || !params || !disk) return UFT_HAL_ERR_INVALID;
    
    *disk = NULL;
    
    /* Validate parameters */
    uint8_t cyl_start = params->cylinder_start;
    uint8_t cyl_end = params->cylinder_end;
    uint8_t head_mask = params->head_mask;
    uint8_t revolutions = params->revolutions ? params->revolutions : 3;
    
    if (cyl_end < cyl_start) cyl_end = cyl_start;
    if (head_mask == 0) head_mask = 0x03;  /* Both heads */
    
    uint8_t num_cyls = cyl_end - cyl_start + 1;
    uint8_t num_heads = ((head_mask & 1) ? 1 : 0) + ((head_mask & 2) ? 1 : 0);
    
    /* Create disk */
    uft_ir_disk_t* ir_disk = uft_ir_disk_create(num_cyls, num_heads);
    if (!ir_disk) return UFT_HAL_ERR_NOMEM;
    
    /* Set metadata */
    ir_disk->metadata.source_type = (uft_ir_source_t)device->type;
    strncpy(ir_disk->metadata.source_name, device->info.name, 
            sizeof(ir_disk->metadata.source_name) - 1);
    ir_disk->geometry.cylinders = num_cyls;
    ir_disk->geometry.heads = num_heads;
    
    /* Calculate total tracks */
    int total_tracks = num_cyls * num_heads;
    int track_num = 0;
    int ret = UFT_HAL_OK;
    
    /* Ensure drive is ready */
    uft_hal_select_drive(device, 0);
    uft_hal_set_motor(device, true);
    uft_hal_recalibrate(device);
    
    /* Read tracks */
    for (uint8_t cyl = cyl_start; cyl <= cyl_end && ret == UFT_HAL_OK; cyl++) {
        for (uint8_t head = 0; head < 2; head++) {
            if (!(head_mask & (1 << head))) continue;
            
            /* Progress callback */
            if (progress) {
                uft_hal_progress_t prog = {
                    .cylinder = cyl,
                    .head = head,
                    .revolution = 0,
                    .retry = 0,
                    .percent = (track_num * 100) / total_tracks,
                    .message = "Reading track",
                    .error = false,
                    .error_code = 0
                };
                
                if (!progress(user_data, &prog)) {
                    ret = UFT_HAL_ERR_CANCELLED;
                    break;
                }
            }
            
            /* Read track with retries */
            uft_ir_track_t* track = NULL;
            int last_err = UFT_HAL_OK;
            
            for (int retry = 0; retry <= params->retries; retry++) {
                last_err = uft_hal_read_track(device, cyl, head, revolutions, &track);
                
                if (last_err == UFT_HAL_OK) break;
                
                if (progress) {
                    uft_hal_progress_t prog = {
                        .cylinder = cyl,
                        .head = head,
                        .revolution = 0,
                        .retry = (uint8_t)(retry + 1),
                        .percent = (track_num * 100) / total_tracks,
                        .message = "Retrying track",
                        .error = true,
                        .error_code = last_err
                    };
                    progress(user_data, &prog);
                }
            }
            
            if (track) {
                uft_ir_disk_add_track(ir_disk, track);
            }
            
            track_num++;
        }
    }
    
    /* Turn off motor */
    uft_hal_set_motor(device, false);
    
    if (ret != UFT_HAL_OK && ret != UFT_HAL_ERR_CANCELLED) {
        uft_ir_disk_free(ir_disk);
        return ret;
    }
    
    *disk = ir_disk;
    return UFT_HAL_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WRITING - UFT-IR INPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_hal_write_track(uft_hal_device_t* device, const uft_ir_track_t* track) {
    if (!device || !track) return UFT_HAL_ERR_INVALID;
    
    if (uft_hal_is_write_protected(device)) {
        return UFT_HAL_ERR_WRPROT;
    }
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE: {
            uint32_t* samples;
            uint32_t sample_count;
            
            int ret = ir_track_to_gw_flux(track, device->info.sample_freq,
                                          &samples, &sample_count);
            if (ret != UFT_HAL_OK) return ret;
            
            ret = uft_gw_write_track(device->handle.greaseweazle, track->cylinder,
                                 track->head, samples, sample_count);
            
            free(samples);
            return uft_gw_to_hal_error(ret);
        }
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

int uft_hal_write_disk(uft_hal_device_t* device, const uft_ir_disk_t* disk,
                       const uft_hal_write_params_t* params,
                       uft_hal_progress_cb progress, void* user_data) {
    if (!device || !disk || !params) return UFT_HAL_ERR_INVALID;
    
    if (uft_hal_is_write_protected(device)) {
        return UFT_HAL_ERR_WRPROT;
    }
    
    int ret = UFT_HAL_OK;
    int track_num = 0;
    int total_tracks = disk->track_count;
    
    /* Ensure drive is ready */
    uft_hal_select_drive(device, 0);
    uft_hal_set_motor(device, true);
    uft_hal_recalibrate(device);
    
    /* Write tracks */
    for (uint16_t i = 0; i < disk->track_count && ret == UFT_HAL_OK; i++) {
        const uft_ir_track_t* track = disk->tracks[i];
        if (!track) continue;
        
        /* Progress callback */
        if (progress) {
            uft_hal_progress_t prog = {
                .cylinder = track->cylinder,
                .head = track->head,
                .revolution = 0,
                .retry = 0,
                .percent = (track_num * 100) / total_tracks,
                .message = "Writing track",
                .error = false,
                .error_code = 0
            };
            
            if (!progress(user_data, &prog)) {
                ret = UFT_HAL_ERR_CANCELLED;
                break;
            }
        }
        
        /* Seek to track position */
        uft_hal_seek(device, track->cylinder);
        uft_hal_select_head(device, track->head);
        
        /* Write track */
        ret = uft_hal_write_track(device, track);
        
        if (ret != UFT_HAL_OK && progress) {
            uft_hal_progress_t prog = {
                .cylinder = track->cylinder,
                .head = track->head,
                .revolution = 0,
                .retry = 0,
                .percent = (track_num * 100) / total_tracks,
                .message = "Write failed",
                .error = true,
                .error_code = ret
            };
            progress(user_data, &prog);
        }
        
        track_num++;
    }
    
    /* Turn off motor */
    uft_hal_set_motor(device, false);
    
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LOW-LEVEL ACCESS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_hal_seek(uft_hal_device_t* device, uint8_t cylinder) {
    if (!device) return UFT_HAL_ERR_NOT_CONNECTED;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE:
            return uft_gw_to_hal_error(uft_gw_seek(device->handle.greaseweazle, cylinder));
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

int uft_hal_select_head(uft_hal_device_t* device, uint8_t head) {
    if (!device) return UFT_HAL_ERR_NOT_CONNECTED;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE:
            return uft_gw_to_hal_error(uft_gw_select_head(device->handle.greaseweazle, head));
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

int uft_hal_set_motor(uft_hal_device_t* device, bool on) {
    if (!device) return UFT_HAL_ERR_NOT_CONNECTED;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE:
            return uft_gw_to_hal_error(uft_gw_set_motor(device->handle.greaseweazle, on));
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

int uft_hal_erase_track(uft_hal_device_t* device, uint8_t cylinder, uint8_t head) {
    if (!device) return UFT_HAL_ERR_NOT_CONNECTED;
    
    switch (device->type) {
        case UFT_HAL_GREASEWEAZLE: {
            int ret = uft_gw_seek(device->handle.greaseweazle, cylinder);
            if (ret != UFT_GW_OK) return uft_gw_to_hal_error(ret);
            
            ret = uft_gw_select_head(device->handle.greaseweazle, head);
            if (ret != UFT_GW_OK) return uft_gw_to_hal_error(ret);
            
            return uft_gw_to_hal_error(uft_gw_erase_track(device->handle.greaseweazle, 2));
        }
        default:
            return UFT_HAL_ERR_UNSUPPORTED;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_hal_get_default_read_params(uft_hal_drive_profile_t profile,
                                      uft_hal_read_params_t* params) {
    if (!params) return;
    
    memset(params, 0, sizeof(*params));
    params->cylinder_start = 0;
    params->head_mask = 0x03;  /* Both heads */
    params->revolutions = 3;
    params->retries = 2;
    params->index_sync = true;
    params->skip_empty = false;
    params->profile = profile;
    
    switch (profile) {
        case UFT_HAL_DRIVE_35_DD:
            params->cylinder_end = 79;
            break;
        case UFT_HAL_DRIVE_35_HD:
            params->cylinder_end = 79;
            break;
        case UFT_HAL_DRIVE_525_DD:
            params->cylinder_end = 39;
            break;
        case UFT_HAL_DRIVE_525_HD:
            params->cylinder_end = 79;
            break;
        case UFT_HAL_DRIVE_C64_1541:
            params->cylinder_end = 34;
            params->head_mask = 0x01;  /* Single head */
            params->revolutions = 5;   /* More revolutions for weak bits */
            break;
        case UFT_HAL_DRIVE_AMIGA_DD:
            params->cylinder_end = 79;
            params->revolutions = 3;
            break;
        default:
            params->cylinder_end = 79;
            break;
    }
}

void uft_hal_get_default_write_params(uft_hal_drive_profile_t profile,
                                       uft_hal_write_params_t* params) {
    if (!params) return;
    
    memset(params, 0, sizeof(*params));
    params->cylinder_start = 0;
    params->head_mask = 0x03;
    params->verify = true;
    params->erase_empty = false;
    params->profile = profile;
    
    switch (profile) {
        case UFT_HAL_DRIVE_35_DD:
        case UFT_HAL_DRIVE_35_HD:
            params->cylinder_end = 79;
            break;
        case UFT_HAL_DRIVE_525_DD:
            params->cylinder_end = 39;
            break;
        case UFT_HAL_DRIVE_525_HD:
            params->cylinder_end = 79;
            break;
        case UFT_HAL_DRIVE_C64_1541:
            params->cylinder_end = 34;
            params->head_mask = 0x01;
            break;
        default:
            params->cylinder_end = 79;
            break;
    }
}

const char* uft_hal_controller_name(uft_hal_controller_t type) {
    switch (type) {
        case UFT_HAL_NONE:           return "None";
        case UFT_HAL_GREASEWEAZLE:   return "Greaseweazle";
        case UFT_HAL_FLUXENGINE:     return "FluxEngine";
        case UFT_HAL_KRYOFLUX:       return "KryoFlux";
        case UFT_HAL_FC5025:         return "FC5025";
        case UFT_HAL_XUM1541:        return "XUM1541";
        case UFT_HAL_SUPERCARD_PRO:  return "SuperCard Pro";
        case UFT_HAL_PAULINE:        return "Pauline";
        case UFT_HAL_APPLESAUCE:     return "Applesauce";
        default:                     return "Unknown";
    }
}

const char* uft_hal_profile_name(uft_hal_drive_profile_t profile) {
    switch (profile) {
        case UFT_HAL_DRIVE_AUTO:     return "Auto-detect";
        case UFT_HAL_DRIVE_35_DD:    return "3.5\" DD (720K)";
        case UFT_HAL_DRIVE_35_HD:    return "3.5\" HD (1.44M)";
        case UFT_HAL_DRIVE_35_ED:    return "3.5\" ED (2.88M)";
        case UFT_HAL_DRIVE_525_DD:   return "5.25\" DD (360K)";
        case UFT_HAL_DRIVE_525_HD:   return "5.25\" HD (1.2M)";
        case UFT_HAL_DRIVE_8_SD:     return "8\" SD";
        case UFT_HAL_DRIVE_8_DD:     return "8\" DD";
        case UFT_HAL_DRIVE_C64_1541: return "Commodore 1541";
        case UFT_HAL_DRIVE_AMIGA_DD: return "Amiga DD";
        case UFT_HAL_DRIVE_AMIGA_HD: return "Amiga HD";
        case UFT_HAL_DRIVE_APPLE_525:return "Apple II 5.25\"";
        case UFT_HAL_DRIVE_APPLE_35: return "Apple 3.5\"";
        default:                     return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR MESSAGES
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_hal_strerror(int err) {
    switch (err) {
        case UFT_HAL_OK:              return "Success";
        case UFT_HAL_ERR_NOT_FOUND:   return "Device not found";
        case UFT_HAL_ERR_OPEN_FAILED: return "Failed to open device";
        case UFT_HAL_ERR_IO:          return "I/O error";
        case UFT_HAL_ERR_TIMEOUT:     return "Operation timed out";
        case UFT_HAL_ERR_NO_INDEX:    return "No index pulse detected";
        case UFT_HAL_ERR_NO_TRK0:     return "Track 0 not found";
        case UFT_HAL_ERR_OVERFLOW:    return "Buffer overflow";
        case UFT_HAL_ERR_WRPROT:      return "Disk is write protected";
        case UFT_HAL_ERR_INVALID:     return "Invalid parameter";
        case UFT_HAL_ERR_NOMEM:       return "Out of memory";
        case UFT_HAL_ERR_NOT_CONNECTED: return "Device not connected";
        case UFT_HAL_ERR_UNSUPPORTED: return "Operation not supported";
        case UFT_HAL_ERR_CANCELLED:   return "Operation cancelled";
        default:                      return "Unknown error";
    }
}
