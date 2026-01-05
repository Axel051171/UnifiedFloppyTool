#pragma once

#include "uft/uft_error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "flux_core.h"

/* Small helpers shared by format plugins.
 * Goal: keep per-format code tiny, and always valgrind-safe.
 */

/* fread() that either reads exactly n bytes or returns false. */
bool fmt_read_exact(FILE *fp, void *dst, size_t n);

/* Read a small prefix (rewinds to start). Returns number of bytes read. */
size_t fmt_read_prefix(FILE *fp, uint8_t *buf, size_t cap);

/* Little-endian helpers (safe on unaligned buffers). */
static inline uint16_t fmt_u16le(const void *p)
{
    const uint8_t *b = (const uint8_t*)p;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static inline uint32_t fmt_u32le(const void *p)
{
    const uint8_t *b = (const uint8_t*)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static inline uint16_t fmt_u16be(const void *p)
{
    const uint8_t *b = (const uint8_t*)p;
    return (uint16_t)b[1] | ((uint16_t)b[0] << 8);
}

static inline uint32_t fmt_u32be(const void *p)
{
    const uint8_t *b = (const uint8_t*)p;
    return (uint32_t)b[3] | ((uint32_t)b[2] << 8) | ((uint32_t)b[1] << 16) | ((uint32_t)b[0] << 24);
}

/* Allocate UFM tracks and set geometry. Returns 0 on success, <0 on error. */
int fmt_ufm_alloc_geom(ufm_disk_t *d, uint16_t cyls, uint16_t heads);

/* Write a short label (truncated). */
void fmt_set_label(ufm_disk_t *d, const char *label);
