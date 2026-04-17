/**
 * @file uft_zoomtape.h
 * @brief ZoomTape - C2N/VIC-1530 Tape Device Interface
 * @version 4.1.1
 */

#ifndef UFT_ZOOMTAPE_H
#define UFT_ZOOMTAPE_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Clock frequencies */
#define TAPE_CLOCK_PAL   985248
#define TAPE_CLOCK_NTSC  1022727

/* Error codes */
#define UFT_ERR_TAPE_NOT_READY  -100

/* Progress callback */
typedef bool (*zoomtape_progress_cb)(uint32_t current, uint32_t total, void *user_data);

/* Status structure */
typedef struct {
    bool motor_on;
    bool sense_pressed;
    uint32_t pulses_read;
    uint32_t pulses_written;
    uint32_t errors;
    int clock_frequency;
    bool half_wave;
} zoomtape_status_t;

/* Device info */
typedef struct {
    bool found;
    char device_name[64];
    char adapter_type[32];
    bool supports_read;
    bool supports_write;
} zoomtape_device_info_t;

/* Opaque context */
typedef struct uft_zoomtape_ctx uft_zoomtape_ctx_t;

/* Context management */
uft_zoomtape_ctx_t* uft_zoomtape_create(void);
void uft_zoomtape_destroy(uft_zoomtape_ctx_t *ctx);

/* Connection */
int uft_zoomtape_connect(uft_zoomtape_ctx_t *ctx, const char *device);
int uft_zoomtape_disconnect(uft_zoomtape_ctx_t *ctx);

/* Motor control */
int uft_zoomtape_motor_on(uft_zoomtape_ctx_t *ctx);
int uft_zoomtape_motor_off(uft_zoomtape_ctx_t *ctx);
bool uft_zoomtape_get_sense(uft_zoomtape_ctx_t *ctx);

/* Low-level pulse I/O */
int uft_zoomtape_read_pulse(uft_zoomtape_ctx_t *ctx, uint32_t *duration);
int uft_zoomtape_write_pulse(uft_zoomtape_ctx_t *ctx, uint32_t duration);

/* TAP file operations */
int uft_zoomtape_read_tap(uft_zoomtape_ctx_t *ctx, const char *output_path,
                          zoomtape_progress_cb progress, void *user_data);
int uft_zoomtape_write_tap(uft_zoomtape_ctx_t *ctx, const char *input_path,
                           zoomtape_progress_cb progress, void *user_data);

/* PRG operations */
int uft_zoomtape_read_prg(uft_zoomtape_ctx_t *ctx, const char *output_path,
                          int file_index);
int uft_zoomtape_write_prg(uft_zoomtape_ctx_t *ctx, const char *input_path,
                           const char *filename);

/* Configuration */
int uft_zoomtape_set_clock(uft_zoomtape_ctx_t *ctx, int frequency);
int uft_zoomtape_set_half_wave(uft_zoomtape_ctx_t *ctx, bool enable);

/* Status */
int uft_zoomtape_get_status(uft_zoomtape_ctx_t *ctx, zoomtape_status_t *status);
void uft_zoomtape_reset_stats(uft_zoomtape_ctx_t *ctx);

/* Detection */
int uft_zoomtape_detect(zoomtape_device_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ZOOMTAPE_H */
