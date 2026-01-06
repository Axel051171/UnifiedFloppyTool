/**
 * @file uft_tap.c
 * @brief C64/C128/VIC-20 TAP Tape Image Format Implementation
 * @version 1.0.0
 * 
 * Clean-room implementation based on TAP format specification.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <uft/cbm/uft_tap.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint32_t rd_le32(const uint8_t* p)
{
    return (uint32_t)p[0] | 
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline void wr_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static inline uint32_t rd_le24(const uint8_t* p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16);
}

static inline void wr_le24(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_tap_detect(const uint8_t* data, size_t size)
{
    if (!data || size < UFT_TAP_HEADER_SIZE) {
        return false;
    }
    
    return memcmp(data, UFT_TAP_MAGIC, UFT_TAP_MAGIC_LEN) == 0;
}

int uft_tap_detect_confidence(const uint8_t* data, size_t size)
{
    if (!data || size < UFT_TAP_HEADER_SIZE) {
        return 0;
    }
    
    int confidence = 0;
    
    /* Check magic */
    if (memcmp(data, UFT_TAP_MAGIC, UFT_TAP_MAGIC_LEN) == 0) {
        confidence += 50;
    } else {
        return 0;
    }
    
    /* Check version */
    uint8_t version = data[12];
    if (version <= 1) {
        confidence += 20;
    }
    
    /* Check reserved bytes (should be 0) */
    if (data[13] == 0 && data[14] == 0 && data[15] == 0) {
        confidence += 15;
    }
    
    /* Check data size consistency */
    uint32_t data_size = rd_le32(data + 16);
    if (size == UFT_TAP_HEADER_SIZE + data_size) {
        confidence += 15;
    } else if (size > UFT_TAP_HEADER_SIZE && size <= UFT_TAP_HEADER_SIZE + data_size) {
        confidence += 10; /* Truncated but reasonable */
    }
    
    return confidence > 100 ? 100 : confidence;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Reading Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_tap_status_t uft_tap_open(const uint8_t* data, size_t size,
                              uft_tap_view_t* view)
{
    if (!data || !view) {
        return UFT_TAP_E_INVALID;
    }
    
    memset(view, 0, sizeof(*view));
    
    if (size < UFT_TAP_HEADER_SIZE) {
        return UFT_TAP_E_TRUNC;
    }
    
    /* Check magic */
    if (memcmp(data, UFT_TAP_MAGIC, UFT_TAP_MAGIC_LEN) != 0) {
        return UFT_TAP_E_MAGIC;
    }
    
    /* Parse header */
    memcpy(view->header.magic, data, UFT_TAP_MAGIC_LEN);
    view->header.version = data[12];
    view->header.machine = data[13];
    view->header.video = data[14];
    view->header.reserved = data[15];
    view->header.data_size = rd_le32(data + 16);
    
    /* Validate version */
    if (view->header.version > 1) {
        return UFT_TAP_E_VERSION;
    }
    
    view->data = data;
    view->data_size = size;
    view->pulse_offset = UFT_TAP_HEADER_SIZE;
    
    /* Count pulses (approximate for now) */
    view->pulse_count = 0;
    size_t pos = view->pulse_offset;
    size_t end = UFT_TAP_HEADER_SIZE + view->header.data_size;
    if (end > size) end = size;
    
    while (pos < end) {
        if (data[pos] == 0) {
            if (view->header.version == 1) {
                pos += 4; /* 0x00 + 3 bytes */
            } else {
                pos += 1;
            }
        } else {
            pos += 1;
        }
        view->pulse_count++;
    }
    
    return UFT_TAP_OK;
}

const uft_tap_header_t* uft_tap_get_header(const uft_tap_view_t* view)
{
    return view ? &view->header : NULL;
}

size_t uft_tap_get_pulse_count(const uft_tap_view_t* view)
{
    return view ? view->pulse_count : 0;
}

uft_tap_status_t uft_tap_iter_begin(const uft_tap_view_t* view,
                                    uft_tap_iter_t* iter)
{
    if (!view || !iter) {
        return UFT_TAP_E_INVALID;
    }
    
    iter->tap = view;
    iter->position = view->pulse_offset;
    iter->pulse_num = 0;
    
    return UFT_TAP_OK;
}

bool uft_tap_iter_has_next(const uft_tap_iter_t* iter)
{
    if (!iter || !iter->tap) return false;
    
    size_t end = UFT_TAP_HEADER_SIZE + iter->tap->header.data_size;
    if (end > iter->tap->data_size) end = iter->tap->data_size;
    
    return iter->position < end;
}

uft_tap_status_t uft_tap_iter_next(uft_tap_iter_t* iter,
                                   uft_tap_pulse_t* pulse)
{
    if (!iter || !iter->tap || !pulse) {
        return UFT_TAP_E_INVALID;
    }
    
    memset(pulse, 0, sizeof(*pulse));
    
    size_t end = UFT_TAP_HEADER_SIZE + iter->tap->header.data_size;
    if (end > iter->tap->data_size) end = iter->tap->data_size;
    
    if (iter->position >= end) {
        return UFT_TAP_E_EOF;
    }
    
    const uint8_t* data = iter->tap->data;
    uint8_t byte = data[iter->position];
    
    if (byte == 0) {
        if (iter->tap->header.version == 0) {
            /* Version 0: long pulse (undefined length) */
            pulse->cycles = 0;
            pulse->is_long = true;
            iter->position++;
        } else {
            /* Version 1: extended encoding */
            if (iter->position + 4 > end) {
                return UFT_TAP_E_TRUNC;
            }
            pulse->cycles = rd_le24(data + iter->position + 1);
            pulse->is_long = false;
            iter->position += 4;
        }
    } else {
        pulse->cycles = (uint32_t)byte * 8;
        pulse->is_long = false;
        iter->position++;
    }
    
    /* Calculate microseconds */
    uft_tap_video_t video = (uft_tap_video_t)iter->tap->header.video;
    pulse->microseconds = uft_tap_cycles_to_us(pulse->cycles, video);
    
    iter->pulse_num++;
    return UFT_TAP_OK;
}

uft_tap_status_t uft_tap_get_pulse(const uft_tap_view_t* view,
                                   uint32_t index,
                                   uft_tap_pulse_t* pulse)
{
    if (!view || !pulse) {
        return UFT_TAP_E_INVALID;
    }
    
    uft_tap_iter_t iter;
    uft_tap_status_t st = uft_tap_iter_begin(view, &iter);
    if (st != UFT_TAP_OK) return st;
    
    for (uint32_t i = 0; i <= index; i++) {
        st = uft_tap_iter_next(&iter, pulse);
        if (st != UFT_TAP_OK) return st;
    }
    
    return UFT_TAP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Writing Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_tap_status_t uft_tap_writer_init(uft_tap_writer_t* writer,
                                     uint8_t version)
{
    if (!writer || version > 1) {
        return UFT_TAP_E_INVALID;
    }
    
    memset(writer, 0, sizeof(*writer));
    writer->version = version;
    writer->machine = UFT_TAP_C64;
    writer->video = UFT_TAP_PAL;
    
    /* Initial buffer */
    writer->capacity = 65536;
    writer->buffer = (uint8_t*)malloc(writer->capacity);
    if (!writer->buffer) {
        return UFT_TAP_E_ALLOC;
    }
    
    /* Write header placeholder */
    memcpy(writer->buffer, UFT_TAP_MAGIC, UFT_TAP_MAGIC_LEN);
    writer->buffer[12] = version;
    writer->buffer[13] = writer->machine;
    writer->buffer[14] = writer->video;
    writer->buffer[15] = 0;
    wr_le32(writer->buffer + 16, 0); /* Will be updated in finish */
    writer->position = UFT_TAP_HEADER_SIZE;
    
    return UFT_TAP_OK;
}

uft_tap_status_t uft_tap_writer_add_pulse(uft_tap_writer_t* writer,
                                          uint32_t cycles)
{
    if (!writer || !writer->buffer) {
        return UFT_TAP_E_INVALID;
    }
    
    /* Ensure capacity */
    size_t needed = writer->position + 4;
    if (needed > writer->capacity) {
        size_t new_cap = writer->capacity * 2;
        uint8_t* new_buf = (uint8_t*)realloc(writer->buffer, new_cap);
        if (!new_buf) {
            return UFT_TAP_E_ALLOC;
        }
        writer->buffer = new_buf;
        writer->capacity = new_cap;
    }
    
    if (cycles == 0 || cycles > UFT_TAP_SHORT_MAX) {
        if (writer->version == 0) {
            /* Version 0: just write 0 */
            writer->buffer[writer->position++] = 0;
        } else {
            /* Version 1: extended encoding */
            writer->buffer[writer->position++] = 0;
            wr_le24(writer->buffer + writer->position, cycles);
            writer->position += 3;
        }
    } else {
        /* Short pulse: cycles / 8 */
        uint8_t byte = (uint8_t)((cycles + 4) / 8);
        if (byte == 0) byte = 1;
        writer->buffer[writer->position++] = byte;
    }
    
    return UFT_TAP_OK;
}

uft_tap_status_t uft_tap_writer_finish(uft_tap_writer_t* writer,
                                       uint8_t** out_data,
                                       size_t* out_size)
{
    if (!writer || !writer->buffer || !out_data || !out_size) {
        return UFT_TAP_E_INVALID;
    }
    
    /* Update data size in header */
    uint32_t data_size = (uint32_t)(writer->position - UFT_TAP_HEADER_SIZE);
    wr_le32(writer->buffer + 16, data_size);
    
    *out_data = writer->buffer;
    *out_size = writer->position;
    
    /* Transfer ownership */
    writer->buffer = NULL;
    writer->capacity = 0;
    writer->position = 0;
    
    return UFT_TAP_OK;
}

void uft_tap_writer_free(uft_tap_writer_t* writer)
{
    if (writer) {
        free(writer->buffer);
        writer->buffer = NULL;
        writer->capacity = 0;
        writer->position = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

double uft_tap_cycles_to_us(uint32_t cycles, uft_tap_video_t video)
{
    uint32_t clock = (video == UFT_TAP_NTSC) ? UFT_TAP_NTSC_CLOCK : UFT_TAP_PAL_CLOCK;
    return (double)cycles * 1000000.0 / (double)clock;
}

uint32_t uft_tap_us_to_cycles(double us, uft_tap_video_t video)
{
    uint32_t clock = (video == UFT_TAP_NTSC) ? UFT_TAP_NTSC_CLOCK : UFT_TAP_PAL_CLOCK;
    return (uint32_t)(us * (double)clock / 1000000.0 + 0.5);
}

int uft_tap_classify_pulse(uint32_t cycles)
{
    if (cycles == 0) return -1;
    
    /* CBM ROM loader thresholds */
    if (cycles < 432) return 0;        /* Short (0 bit) */
    if (cycles < 592) return 1;        /* Medium (1 bit) */
    return 2;                           /* Long (sync/header) */
}

const char* uft_tap_machine_name(uft_tap_machine_t machine)
{
    switch (machine) {
        case UFT_TAP_C64:  return "C64";
        case UFT_TAP_VIC20: return "VIC-20";
        case UFT_TAP_C16:  return "C16/Plus4";
        case UFT_TAP_C128: return "C128";
        default:           return "Unknown";
    }
}

const char* uft_tap_video_name(uft_tap_video_t video)
{
    switch (video) {
        case UFT_TAP_PAL:  return "PAL";
        case UFT_TAP_NTSC: return "NTSC";
        default:           return "Unknown";
    }
}
