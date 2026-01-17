/**
 * @file uft_zoomtape.c
 * @brief ZoomTape - C2N/VIC-1530 Tape Device Support
 * @version 4.1.1
 * 
 * ZoomTape provides support for reading and writing Commodore
 * tape formats using:
 * - ZoomFloppy with tape adapter
 * - Direct C2N/VIC-1530 datasette connection
 * - 1531 tape drive
 * 
 * Supports:
 * - TAP format (raw tape signals)
 * - T64 format (tape archive)
 * - M2I format (tape image)
 * - PRG extraction
 * 
 * Hardware connection via cassette port pins:
 * - MOTOR (active low)
 * - READ (data from tape)
 * - WRITE (data to tape)
 * - SENSE (tape button pressed)
 * 
 * Reference: OpenCBM tape support, VICE tape emulation
 */

#include "uft/hal/uft_zoomtape.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Tape timing constants (PAL) */
#define TAPE_CLOCK_PAL          985248      /* PAL C64 clock frequency */
#define TAPE_CLOCK_NTSC         1022727     /* NTSC C64 clock frequency */

/* Pulse types */
#define PULSE_SHORT             0x30        /* ~352 cycles */
#define PULSE_MEDIUM            0x42        /* ~512 cycles */
#define PULSE_LONG              0x56        /* ~672 cycles */

/* TAP file constants */
#define TAP_SIGNATURE           "C64-TAPE-RAW"
#define TAP_SIGNATURE_LEN       12
#define TAP_VERSION_0           0           /* Original TAP */
#define TAP_VERSION_1           1           /* Extended TAP with overflow bytes */

/* Data encoding */
#define TAPE_LEADER_BYTE        0x89        /* Leader/countdown byte */
#define TAPE_SYNC_BYTE          0x09        /* Sync after countdown */

/* Hardware timeouts */
#define MOTOR_DELAY_MS          500         /* Motor spin-up time */
#define READ_TIMEOUT_MS         30000       /* 30 second read timeout */
#define WRITE_TIMEOUT_MS        60000       /* 60 second write timeout */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

struct uft_zoomtape_ctx {
    void *device_handle;        /* USB device handle */
    bool motor_on;              /* Motor state */
    bool sense_pressed;         /* Play button state */
    
    /* Configuration */
    int clock_frequency;        /* PAL or NTSC */
    bool half_wave;             /* Half-wave recording */
    int threshold;              /* Pulse detection threshold */
    
    /* Statistics */
    uint32_t pulses_read;
    uint32_t pulses_written;
    uint32_t errors;
    
    /* Buffer */
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_pos;
};

/* ============================================================================
 * Context Management
 * ============================================================================ */

uft_zoomtape_ctx_t* uft_zoomtape_create(void) {
    uft_zoomtape_ctx_t *ctx = calloc(1, sizeof(uft_zoomtape_ctx_t));
    if (ctx) {
        ctx->clock_frequency = TAPE_CLOCK_PAL;
        ctx->half_wave = false;
        ctx->threshold = PULSE_MEDIUM;
        ctx->buffer_size = 1024 * 1024;  /* 1MB buffer */
        ctx->buffer = malloc(ctx->buffer_size);
    }
    return ctx;
}

void uft_zoomtape_destroy(uft_zoomtape_ctx_t *ctx) {
    if (ctx) {
        if (ctx->motor_on) {
            uft_zoomtape_motor_off(ctx);
        }
        if (ctx->buffer) {
            free(ctx->buffer);
        }
        free(ctx);
    }
}

/* ============================================================================
 * Hardware Connection
 * ============================================================================ */

int uft_zoomtape_connect(uft_zoomtape_ctx_t *ctx, const char *device) {
    if (!ctx || !device) return UFT_ERR_INVALID_ARG;
    
    /*
     * Connection would:
     * 1. Open ZoomFloppy device
     * 2. Check for tape adapter presence
     * 3. Initialize tape interface
     */
    
    /* Placeholder - actual USB communication would go here */
    ctx->motor_on = false;
    ctx->sense_pressed = false;
    
    return UFT_OK;
}

int uft_zoomtape_disconnect(uft_zoomtape_ctx_t *ctx) {
    if (!ctx) return UFT_ERR_INVALID_ARG;
    
    if (ctx->motor_on) {
        uft_zoomtape_motor_off(ctx);
    }
    
    ctx->device_handle = NULL;
    return UFT_OK;
}

/* ============================================================================
 * Motor Control
 * ============================================================================ */

int uft_zoomtape_motor_on(uft_zoomtape_ctx_t *ctx) {
    if (!ctx) return UFT_ERR_INVALID_ARG;
    
    /* Send motor control command */
    ctx->motor_on = true;
    
    /* Wait for motor to spin up */
    /* In real implementation: sleep(MOTOR_DELAY_MS) */
    
    return UFT_OK;
}

int uft_zoomtape_motor_off(uft_zoomtape_ctx_t *ctx) {
    if (!ctx) return UFT_ERR_INVALID_ARG;
    
    ctx->motor_on = false;
    return UFT_OK;
}

bool uft_zoomtape_get_sense(uft_zoomtape_ctx_t *ctx) {
    if (!ctx) return false;
    
    /* Read SENSE line (play button pressed) */
    /* In real implementation: read from hardware */
    
    return ctx->sense_pressed;
}

/* ============================================================================
 * Low-Level Pulse I/O
 * ============================================================================ */

int uft_zoomtape_read_pulse(uft_zoomtape_ctx_t *ctx, uint32_t *duration) {
    if (!ctx || !duration) return UFT_ERR_INVALID_ARG;
    
    /*
     * Read a single pulse from tape.
     * Duration is in clock cycles.
     */
    
    /* Placeholder */
    *duration = 0;
    ctx->pulses_read++;
    
    return UFT_OK;
}

int uft_zoomtape_write_pulse(uft_zoomtape_ctx_t *ctx, uint32_t duration) {
    if (!ctx) return UFT_ERR_INVALID_ARG;
    
    /*
     * Write a single pulse to tape.
     * Duration is in clock cycles.
     */
    
    ctx->pulses_written++;
    return UFT_OK;
}

/* ============================================================================
 * TAP File Reading
 * ============================================================================ */

int uft_zoomtape_read_tap(uft_zoomtape_ctx_t *ctx, const char *output_path,
                          zoomtape_progress_cb progress, void *user_data) {
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;
    
    /* Check motor and sense */
    if (!ctx->sense_pressed) {
        return UFT_ERR_TAPE_NOT_READY;
    }
    
    FILE *f = fopen(output_path, "wb");
    if (!f) return UFT_ERR_FILE_CREATE;
    
    /* Write TAP header */
    uint8_t header[20];
    memcpy(header, TAP_SIGNATURE, TAP_SIGNATURE_LEN);
    header[12] = TAP_VERSION_1;      /* Version */
    header[13] = 0;                  /* Machine: C64 */
    header[14] = 0;                  /* Video: PAL */
    header[15] = 0;                  /* Reserved */
    /* Bytes 16-19: data length (filled later) */
    memset(&header[16], 0, 4);
    fwrite(header, 1, 20, f);
    
    /* Turn on motor */
    uft_zoomtape_motor_on(ctx);
    
    /* Read pulses and convert to TAP format */
    uint32_t data_length = 0;
    uint32_t duration;
    
    while (uft_zoomtape_read_pulse(ctx, &duration) == UFT_OK) {
        if (duration == 0) break;  /* End of tape */
        
        /* Convert cycle count to TAP byte */
        uint8_t tap_byte = duration / 8;
        
        if (tap_byte == 0) {
            /* Overflow: write 0x00 followed by 3-byte duration */
            uint8_t overflow[4] = {0x00, 
                duration & 0xFF,
                (duration >> 8) & 0xFF,
                (duration >> 16) & 0xFF};
            fwrite(overflow, 1, 4, f);
            data_length += 4;
        } else {
            fwrite(&tap_byte, 1, 1, f);
            data_length++;
        }
        
        /* Progress callback */
        if (progress && (ctx->pulses_read % 1000) == 0) {
            if (!progress(ctx->pulses_read, 0, user_data)) {
                break;  /* Cancelled */
            }
        }
    }
    
    /* Update data length in header */
    fseek(f, 16, SEEK_SET);
    uint8_t len_bytes[4] = {
        data_length & 0xFF,
        (data_length >> 8) & 0xFF,
        (data_length >> 16) & 0xFF,
        (data_length >> 24) & 0xFF
    };
    fwrite(len_bytes, 1, 4, f);
    
    uft_zoomtape_motor_off(ctx);
    fclose(f);
    
    return UFT_OK;
}

/* ============================================================================
 * TAP File Writing
 * ============================================================================ */

int uft_zoomtape_write_tap(uft_zoomtape_ctx_t *ctx, const char *input_path,
                           zoomtape_progress_cb progress, void *user_data) {
    if (!ctx || !input_path) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(input_path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    /* Read and verify TAP header */
    uint8_t header[20];
    if (fread(header, 1, 20, f) != 20) {
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    
    if (memcmp(header, TAP_SIGNATURE, TAP_SIGNATURE_LEN) != 0) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    uint8_t version = header[12];
    uint32_t data_length = header[16] | (header[17] << 8) |
                           (header[18] << 16) | (header[19] << 24);
    
    /* Check motor and sense */
    if (!ctx->sense_pressed) {
        fclose(f);
        return UFT_ERR_TAPE_NOT_READY;
    }
    
    /* Turn on motor */
    uft_zoomtape_motor_on(ctx);
    
    /* Write pulses from TAP data */
    uint32_t bytes_written = 0;
    
    while (bytes_written < data_length) {
        uint8_t tap_byte;
        if (fread(&tap_byte, 1, 1, f) != 1) break;
        bytes_written++;
        
        uint32_t duration;
        if (tap_byte == 0 && version >= 1) {
            /* Overflow: read 3-byte duration */
            uint8_t overflow[3];
            if (fread(overflow, 1, 3, f) != 3) break;
            bytes_written += 3;
            duration = overflow[0] | (overflow[1] << 8) | (overflow[2] << 16);
        } else {
            duration = tap_byte * 8;
        }
        
        uft_zoomtape_write_pulse(ctx, duration);
        
        /* Progress callback */
        if (progress && (ctx->pulses_written % 1000) == 0) {
            if (!progress(bytes_written, data_length, user_data)) {
                break;  /* Cancelled */
            }
        }
    }
    
    uft_zoomtape_motor_off(ctx);
    fclose(f);
    
    return UFT_OK;
}

/* ============================================================================
 * High-Level Operations
 * ============================================================================ */

int uft_zoomtape_read_prg(uft_zoomtape_ctx_t *ctx, const char *output_path,
                          int file_index) {
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;
    
    /*
     * Read a PRG file from tape.
     * This involves:
     * 1. Reading leader
     * 2. Decoding header block
     * 3. Decoding data block
     */
    
    /* Placeholder - full implementation would decode CBM tape format */
    
    return UFT_OK;
}

int uft_zoomtape_write_prg(uft_zoomtape_ctx_t *ctx, const char *input_path,
                           const char *filename) {
    if (!ctx || !input_path) return UFT_ERR_INVALID_ARG;
    
    /*
     * Write a PRG file to tape.
     * This involves:
     * 1. Writing leader
     * 2. Encoding header block
     * 3. Encoding data block
     * 4. Writing trailer
     */
    
    /* Placeholder - full implementation would encode CBM tape format */
    
    return UFT_OK;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int uft_zoomtape_set_clock(uft_zoomtape_ctx_t *ctx, int frequency) {
    if (!ctx) return UFT_ERR_INVALID_ARG;
    
    if (frequency != TAPE_CLOCK_PAL && frequency != TAPE_CLOCK_NTSC) {
        return UFT_ERR_INVALID_ARG;
    }
    
    ctx->clock_frequency = frequency;
    return UFT_OK;
}

int uft_zoomtape_set_half_wave(uft_zoomtape_ctx_t *ctx, bool enable) {
    if (!ctx) return UFT_ERR_INVALID_ARG;
    
    ctx->half_wave = enable;
    return UFT_OK;
}

/* ============================================================================
 * Status and Statistics
 * ============================================================================ */

int uft_zoomtape_get_status(uft_zoomtape_ctx_t *ctx, 
                            zoomtape_status_t *status) {
    if (!ctx || !status) return UFT_ERR_INVALID_ARG;
    
    status->motor_on = ctx->motor_on;
    status->sense_pressed = ctx->sense_pressed;
    status->pulses_read = ctx->pulses_read;
    status->pulses_written = ctx->pulses_written;
    status->errors = ctx->errors;
    status->clock_frequency = ctx->clock_frequency;
    status->half_wave = ctx->half_wave;
    
    return UFT_OK;
}

void uft_zoomtape_reset_stats(uft_zoomtape_ctx_t *ctx) {
    if (!ctx) return;
    
    ctx->pulses_read = 0;
    ctx->pulses_written = 0;
    ctx->errors = 0;
}

/* ============================================================================
 * Device Detection
 * ============================================================================ */

int uft_zoomtape_detect(zoomtape_device_info_t *info) {
    if (!info) return UFT_ERR_INVALID_ARG;
    
    memset(info, 0, sizeof(*info));
    
    /*
     * Detection would:
     * 1. Look for ZoomFloppy with tape adapter
     * 2. Query adapter type
     * 3. Return capabilities
     */
    
    /* Placeholder */
    info->found = false;
    
    return UFT_OK;
}
