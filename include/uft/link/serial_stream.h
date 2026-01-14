#pragma once
/*
 * serial_stream.h — minimal, portable serial read/write with timeouts (POSIX/Windows)
 *
 * Designed to support "raw ADF stream" workflows inspired by TransWarp:
 * - Sender writes raw 512-byte sectors sequentially (no framing).
 * - Receiver reads exactly N bytes (derived from geometry) and writes to file.
 */
#include <stddef.h>
#include <stdint.h>
#include "uft/uft_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_serial {
#ifdef _WIN32
    void *h; /* HANDLE */
#else
    int fd;
#endif
    int is_open;
} uft_serial_t;

typedef struct uft_serial_opts {
    uint32_t baud;
    uint8_t databits; /* 8 */
    uint8_t stopbits; /* 1 */
    char parity;      /* 'N','E','O' */
    int rtscts;       /* 0/1 */
    uint32_t read_timeout_ms; /* per read call */
} uft_serial_opts_t;

uft_rc_t uft_stream_open(uft_serial_t *s, const char *device, const uft_serial_opts_t *opt, uft_diag_t *diag);
void     uft_stream_close(uft_serial_t *s);

/* Read up to `n` bytes; returns bytes read in out_n. */
uft_rc_t uft_stream_read(uft_serial_t *s, void *buf, size_t n, size_t *out_n, uft_diag_t *diag);
/* Write exactly `n` bytes (best effort; retries partial writes). */
uft_rc_t uft_stream_write_all(uft_serial_t *s, const void *buf, size_t n, uft_diag_t *diag);

#ifdef __cplusplus
}
#endif
