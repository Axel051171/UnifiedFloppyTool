#pragma once
/*
 * uft_c64_prg_analyzer.h
 *
 * Portable C11 analyzer for Commodore 64 PRG binaries.
 * Targeted at floppy copiers/nibblers/speeders:
 * - PRG header parse (load address)
 * - printable string extraction (ASCII/PETSCII-ish; CR -> '\n')
 * - keyword scoring for floppy / nibble / GCR indicators
 *
 * Forensic mode: no emulation, no execution, deterministic output.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_prg_status {
    UFT_PRG_OK = 0,
    UFT_PRG_E_INVALID = 1,
    UFT_PRG_E_TOO_SMALL = 2,
    UFT_PRG_E_BUF = 3
} uft_prg_status_t;

typedef struct uft_prg_view {
    uint16_t load_address;
    const uint8_t *data;
    size_t data_len;
} uft_prg_view_t;

typedef struct uft_prg_string {
    uint32_t offset; /* into data[] (not counting 2-byte load address) */
    uint32_t length;
    const char *text; /* points into caller buffer */
} uft_prg_string_t;

typedef struct uft_prg_score {
    uint32_t nib;
    uint32_t burst;
    uint32_t gcr;
    uint32_t sync;
    uint32_t track;
    uint32_t sector;
    uint32_t disk;
    uint32_t drive;
    uint32_t dev1541;
    uint32_t dev1571;
    uint32_t dev1581;
    uint32_t copy;
    uint32_t rapid;
    uint32_t fast;
    uint32_t turbo;
    uint32_t verify;
    uint32_t retry;
    uint32_t error;
    uint32_t check;
    uint32_t crc;
    uint32_t checksum;
    uint32_t protect;
    uint32_t weak;
    uint32_t bits;
    uint32_t density;
    uint32_t align;
    uint32_t speed;
    uint32_t head;
    uint32_t step;
    uint32_t read;
    uint32_t write;
    uint32_t gap;
    uint32_t backup;
    uint32_t format;
    uint32_t bam;
    uint32_t directory;
} uft_prg_score_t;

uft_prg_status_t uft_prg_parse(const uint8_t *blob, size_t blob_len, uft_prg_view_t *out);

uft_prg_status_t uft_prg_extract_strings(const uft_prg_view_t *prg,
                                         uft_prg_string_t *out_strings, size_t max_strings,
                                         char *text_buf, size_t text_cap,
                                         uint32_t min_visible_len,
                                         size_t *out_count, size_t *out_text_used);

void uft_prg_score_floppy_keywords(const uft_prg_string_t *strings, size_t count, uft_prg_score_t *out_score);

#ifdef __cplusplus
}
#endif
