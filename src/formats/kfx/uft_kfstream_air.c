/**
 * @file uft_kfstream_air.c
 * @brief Enhanced KryoFlux Stream Parser - ported from DrCoolzic AIR project
 * @version 4.0.0
 *
 * Full port of AIR KFReader.cs to C.
 * Original: Copyright (C) 2013-2015 SPS & Jean Louis-Guerin (GPL-3.0)
 *
 * Improvements over existing UFT KryoFlux parsers:
 * - Complete stream block decoding: Flux1/2/3, Nop1/2/3, Ovl16, OOB
 * - All OOB types: StreamInfo, Index, StreamEnd, HWInfo, EOF
 * - Full index analysis with sub-cell timing (preIndexTime/postIndexTime)
 * - Precise revolution time computation from flux accumulation
 * - Stream position validation (encoder vs decoder sync check)
 * - Hardware error detection (buffer overflow, missing index)
 * - HW info string extraction (sck, ick from firmware 2.0+)
 * - Statistics: RPM (avg/min/max), flux count, transfer rate
 * - Thread-safe / reentrant design
 *
 * "Kein Bit geht verloren" - UFT Preservation Philosophy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

/*============================================================================
 * KRYOFLUX CONSTANTS (from KFReader.cs)
 *============================================================================*/

/* Default clock values */
#define KF_SCK_DEFAULT      (((18432000.0 * 73.0) / 14.0) / 4.0)  /* ~24027428.57 Hz */
#define KF_ICK_DEFAULT      (KF_SCK_DEFAULT / 8.0)

/* Stream block headers (from KFReader.cs BHeader enum) */
#define KF_BH_FLUX2_MAX     0x07    /* Headers 0x00-0x07: Flux2 blocks */
#define KF_BH_NOP1          0x08
#define KF_BH_NOP2          0x09
#define KF_BH_NOP3          0x0A
#define KF_BH_OVL16         0x0B    /* Overflow: add 65536 to flux value */
#define KF_BH_FLUX3         0x0C    /* 3-byte flux value */
#define KF_BH_OOB           0x0D    /* Out-of-Band control block */
#define KF_BH_FLUX1_MIN     0x0E    /* Headers 0x0E-0xFF: Flux1 blocks */

/* OOB types (from KFReader.cs OOBType enum) */
#define KF_OOB_INVALID      0x00
#define KF_OOB_STREAM_INFO  0x01    /* Transfer info: position + time */
#define KF_OOB_INDEX        0x02    /* Index signal: position + counters */
#define KF_OOB_STREAM_END   0x03    /* End of stream: position + status */
#define KF_OOB_HW_INFO      0x04    /* KryoFlux HW info string */
#define KF_OOB_EOF          0x0D    /* End of file marker */

/* Hardware status codes */
#define KF_HW_OK            0x00
#define KF_HW_BUFFER        0x01    /* Buffer overflow */
#define KF_HW_INDEX         0x02    /* No index detected */

/* Limits */
#define KF_MAX_INDICES      128
#define KF_HW_INFO_MAX      4096

/*============================================================================
 * STATUS CODES (from KFReader.cs StreamStatus enum)
 *============================================================================*/

typedef enum {
    KF_OK = 0,
    KF_MISSING_DATA,        /* Incomplete block at end */
    KF_INVALID_CODE,        /* Unknown block header */
    KF_WRONG_POSITION,      /* Stream position mismatch */
    KF_DEV_BUFFER,          /* Hardware buffer error */
    KF_DEV_INDEX,           /* No index signal from hardware */
    KF_TRANSFER_ERROR,      /* Unknown hardware error */
    KF_INVALID_OOB,         /* Unknown OOB type */
    KF_MISSING_END,         /* No EOF block found */
    KF_INDEX_REFERENCE,     /* Index reference past stream end */
    KF_MISSING_INDEX,       /* Index not found in flux data */
    KF_READ_ERROR           /* File read error */
} kf_status_t;

/*============================================================================
 * STRUCTURES (from KFReader.cs)
 *============================================================================*/

/** Index info (from KFReader.cs IndexInfo struct) */
typedef struct {
    int flux_position;      /* Index in flux array containing index signal */
    int index_time;         /* Exact rotation time in sample clocks */
    int pre_index_time;     /* Sample clocks before index within flux cell */
    int post_index_time;    /* Sample clocks after index within flux cell */
} kf_index_info_t;

/** Internal index data (from KFReader.cs IndexInternal struct) */
typedef struct {
    int stream_pos;         /* Stream position when index detected */
    int sample_counter;     /* Sample counter value at index */
    int index_counter;      /* Index counter value at index */
} kf_index_internal_t;

/** Statistics (from KFReader.cs Statistic struct) */
typedef struct {
    double avg_rpm;         /* Average RPM */
    double max_rpm;         /* Maximum RPM */
    double min_rpm;         /* Minimum RPM */
    double avg_bps;         /* Average transfer rate (bytes/sec) */
    int    avg_flux;        /* Average flux reversals per revolution */
    int    flux_min;        /* Minimum flux value (sample clocks) */
    int    flux_max;        /* Maximum flux value (sample clocks) */
} kf_statistics_t;

/** Complete parsed stream (from KFReader.cs KFReader class) */
typedef struct {
    /* Flux data */
    int*     flux_values;       /* Flux transition values (sample clocks) */
    int*     flux_stream_pos;   /* Stream position for each flux */
    int      flux_count;        /* Number of flux transitions */
    int      flux_capacity;     /* Allocated capacity */

    /* Index data */
    kf_index_info_t     indices[KF_MAX_INDICES];
    kf_index_internal_t index_int[KF_MAX_INDICES];
    int                 index_count;

    /* HW info */
    char     hw_info[KF_HW_INFO_MAX];
    int      hw_info_len;

    /* Clock values */
    double   sck_value;         /* Sample clock frequency */
    double   ick_value;         /* Index clock frequency */

    /* Statistics */
    kf_statistics_t stats;
    int      flux_min;
    int      flux_max;

    /* Transfer stats (internal) */
    int      stat_data_count;
    int      stat_data_time;
    int      stat_data_trans;

    /* Status */
    kf_status_t status;
    bool     valid;
} kf_stream_t;

/*============================================================================
 * HELPERS
 *============================================================================*/

static inline int kf_extract_int(const uint8_t* buf, int pos) {
    return (int)buf[pos] | ((int)buf[pos+1] << 8) |
           ((int)buf[pos+2] << 16) | ((int)buf[pos+3] << 24);
}

/*============================================================================
 * PARSE STREAM - Ported from KFReader.cs parseStream()
 *
 * This is the core decoder. It processes stream blocks:
 * - Flux1 (0x0E-0xFF): 1-byte flux, value = header byte
 * - Flux2 (0x00-0x07): 2-byte flux, value = (header<<8) + next_byte
 * - Flux3 (0x0C):      3-byte flux, value = (byte1<<8) + byte2
 * - Ovl16 (0x0B):      Add 0x10000 to accumulating flux value
 * - Nop1/2/3:          Skip 1/2/3 bytes
 * - OOB (0x0D):        Out-of-band control blocks
 *============================================================================*/

static kf_status_t kf_parse_stream(const uint8_t* sbuf, size_t buf_len,
                                    kf_stream_t* stream)
{
    int last_index_pos = 0;
    int stream_pos = 0;
    int flux_value = 0;
    int last_stream_pos = 0;
    uint8_t hw_status = KF_HW_OK;
    bool eof_found = false;
    int pos = 0;

    if (buf_len == 0) return KF_OK;

    stream->flux_min = INT_MAX;
    stream->flux_max = 0;

    /* Allocate flux arrays (worst case: one flux per byte) */
    stream->flux_capacity = (int)buf_len + 1;
    stream->flux_values = (int*)calloc(stream->flux_capacity, sizeof(int));
    stream->flux_stream_pos = (int*)calloc(stream->flux_capacity, sizeof(int));
    if (!stream->flux_values || !stream->flux_stream_pos) return KF_READ_ERROR;
    stream->flux_count = 0;
    stream->index_count = 0;
    stream->hw_info_len = 0;
    stream->hw_info[0] = '\0';

    /* Process entire buffer */
    while (!eof_found && pos < (int)buf_len) {
        uint8_t bhead = sbuf[pos];
        int blen = 0;
        bool is_flux = false;

        /*
         * Step 1: Calculate block length
         * (from KFReader.cs parseStream() switch for blen)
         */
        if (bhead == KF_BH_NOP1 || bhead == KF_BH_OVL16) {
            blen = 1;
        } else if (bhead == KF_BH_NOP2) {
            blen = 2;
        } else if (bhead == KF_BH_NOP3 || bhead == KF_BH_FLUX3) {
            blen = 3;
        } else if (bhead == KF_BH_OOB) {
            blen = 4; /* header + type + size(2) */
            if (pos + 1 < (int)buf_len && sbuf[pos+1] != KF_OOB_EOF) {
                if (pos + 3 < (int)buf_len)
                    blen += (int)sbuf[pos+2] + ((int)sbuf[pos+3] << 8);
            }
        } else if (bhead >= KF_BH_FLUX1_MIN) {
            blen = 1; /* Flux1 */
        } else if (bhead <= KF_BH_FLUX2_MAX) {
            blen = 2; /* Flux2 */
        } else {
            return KF_INVALID_CODE;
        }

        /* Check for incomplete data */
        if ((int)buf_len - pos < blen)
            return KF_MISSING_DATA;

        /*
         * Step 2: Compute flux value
         * (from KFReader.cs parseStream() flux computation)
         */
        if (bhead == KF_BH_OVL16) {
            flux_value += 0x10000;
        } else if (bhead == KF_BH_FLUX3) {
            flux_value += ((int)sbuf[pos+1] << 8) + (int)sbuf[pos+2];
            is_flux = true;
        } else if (bhead >= KF_BH_FLUX1_MIN) {
            flux_value += (int)bhead;
            is_flux = true;
        } else if (bhead <= KF_BH_FLUX2_MAX) {
            flux_value += ((int)bhead << 8) + (int)sbuf[pos+1];
            is_flux = true;
        }

        /*
         * Step 3: Process block
         * (from KFReader.cs parseStream() processing section)
         */
        if (bhead != KF_BH_OOB) {
            /* Store flux value */
            if (is_flux && stream->flux_count < stream->flux_capacity) {
                stream->flux_values[stream->flux_count] = flux_value;
                if (flux_value < stream->flux_min) stream->flux_min = flux_value;
                if (flux_value > stream->flux_max) stream->flux_max = flux_value;
                stream->flux_stream_pos[stream->flux_count] = stream_pos;
                stream->flux_count++;
                flux_value = 0;
            }
            stream_pos += blen;
        } else {
            /* OOB: process control block */
            uint8_t oob_type = sbuf[pos + 1];

            switch (oob_type) {
            case KF_OOB_STREAM_INFO: {
                /* Validate stream position sync */
                int position = kf_extract_int(sbuf, pos + 4);
                if (stream_pos != position)
                    return KF_WRONG_POSITION;

                int sb = stream_pos - last_stream_pos;
                last_stream_pos = stream_pos;

                if (sb != 0) {
                    stream->stat_data_count += sb;
                    stream->stat_data_time += kf_extract_int(sbuf, pos + 8);
                    stream->stat_data_trans++;
                }
                break;
            }

            case KF_OOB_INDEX: {
                if (stream->index_count < KF_MAX_INDICES) {
                    int idx = stream->index_count;
                    stream->index_int[idx].stream_pos =
                        kf_extract_int(sbuf, pos + 4);
                    stream->index_int[idx].sample_counter =
                        kf_extract_int(sbuf, pos + 8);
                    stream->index_int[idx].index_counter =
                        kf_extract_int(sbuf, pos + 12);
                    stream->indices[idx].flux_position = 0;
                    stream->indices[idx].index_time = 0;
                    stream->indices[idx].pre_index_time = 0;
                    stream->indices[idx].post_index_time = 0;
                    stream->index_count++;
                    last_index_pos = stream->index_int[idx].stream_pos;
                }
                break;
            }

            case KF_OOB_STREAM_END: {
                hw_status = (uint8_t)(kf_extract_int(sbuf, pos + 8));
                if (hw_status == KF_HW_OK) {
                    if (stream_pos != kf_extract_int(sbuf, pos + 4))
                        return KF_WRONG_POSITION;
                }
                break;
            }

            case KF_OOB_HW_INFO: {
                int info_size = (int)sbuf[pos+2] + ((int)sbuf[pos+3] << 8);
                if (info_size > 0 && pos + 4 + info_size <= (int)buf_len) {
                    if (stream->hw_info_len > 0) {
                        /* Append separator */
                        if (stream->hw_info_len + 2 < KF_HW_INFO_MAX) {
                            stream->hw_info[stream->hw_info_len++] = ',';
                            stream->hw_info[stream->hw_info_len++] = ' ';
                        }
                    }
                    for (int i = 0; i < info_size - 1 &&
                         stream->hw_info_len < KF_HW_INFO_MAX - 1; i++) {
                        stream->hw_info[stream->hw_info_len++] =
                            (char)sbuf[pos + 4 + i];
                    }
                    stream->hw_info[stream->hw_info_len] = '\0';
                }
                break;
            }

            case KF_OOB_EOF:
                eof_found = true;
                break;

            default:
                return KF_INVALID_OOB;
            }
        }

        pos += blen;
    }

    /* Additional empty flux at end */
    if (stream->flux_count < stream->flux_capacity) {
        stream->flux_values[stream->flux_count] = flux_value;
        stream->flux_stream_pos[stream->flux_count] = stream_pos;
    }

    /* Check hardware errors */
    switch (hw_status) {
    case KF_HW_OK:     break;
    case KF_HW_BUFFER: return KF_DEV_BUFFER;
    case KF_HW_INDEX:  return KF_DEV_INDEX;
    default:           return KF_TRANSFER_ERROR;
    }

    if (!eof_found)
        return KF_MISSING_END;

    if (last_index_pos != 0 && stream_pos < last_index_pos)
        return KF_INDEX_REFERENCE;

    return KF_OK;
}

/*============================================================================
 * INDEX ANALYSIS - Ported from KFReader.cs indexAnalysis()
 *
 * This is the critical algorithm that computes precise revolution timing
 * by correlating index signals with flux transition positions.
 * The index signal occurs within a flux cell, so we need sub-cell
 * timing to get precise pre/post index times.
 *============================================================================*/

static kf_status_t kf_index_analysis(kf_stream_t* stream)
{
    if (stream->index_count == 0 || stream->flux_count == 0)
        return KF_OK;

    int iidx = 0;      /* Index index */
    int itime = 0;      /* Accumulated index time */

    int next_strpos = stream->index_int[iidx].stream_pos;

    /* Associate flux transitions with index signals */
    for (int fidx = 0; fidx < stream->flux_count; fidx++) {
        itime += stream->flux_values[fidx];

        int nfidx = fidx + 1;

        /* Continue summing until we reach the index stream position */
        if (stream->flux_stream_pos[nfidx] < next_strpos)
            continue;

        /* Edge case: very first flux has index signal */
        if (fidx == 0 && stream->flux_stream_pos[0] >= next_strpos)
            nfidx = 0;

        if (iidx < stream->index_count) {
            /* Set flux position for this index */
            stream->indices[iidx].flux_position = nfidx;

            /* Complete flux time of the cell containing index */
            int iftime = stream->flux_values[nfidx];

            /* If timer was sampled at signal edge, use flux length */
            if (stream->index_int[iidx].sample_counter == 0)
                stream->index_int[iidx].sample_counter = iftime & 0xFFFF;

            /* Handle unwritten last flux */
            if (nfidx >= stream->flux_count) {
                if (stream->flux_stream_pos[nfidx] == next_strpos) {
                    iftime += stream->index_int[iidx].sample_counter;
                    stream->flux_values[nfidx] = iftime;
                }
            }

            /*
             * Sub-cell timing computation
             * (from KFReader.cs indexAnalysis() lines 758-777)
             */

            /* Total overflow count in the flux containing index */
            int ic_overflow_cnt = iftime >> 16;

            /* Overflows to step back from flux cell to reach signal point */
            int pre_overflow_cnt = stream->flux_stream_pos[nfidx] - next_strpos;

            /* Sanity check */
            if (ic_overflow_cnt < pre_overflow_cnt)
                return KF_MISSING_INDEX;

            /* Pre-index time: overflows before signal + sample counter */
            int pre_index_time = (ic_overflow_cnt - pre_overflow_cnt) << 16;
            pre_index_time += stream->index_int[iidx].sample_counter;

            /* Set sub-cell timing */
            stream->indices[iidx].pre_index_time = pre_index_time;
            stream->indices[iidx].post_index_time = iftime - pre_index_time;

            /*
             * Revolution time computation
             * (from KFReader.cs indexAnalysis() lines 780-787)
             */
            if (iidx != 0)
                itime -= stream->indices[iidx - 1].pre_index_time;

            stream->indices[iidx].index_time =
                (nfidx != 0 ? itime : 0) + pre_index_time;

            /* Advance to next index */
            iidx++;
            next_strpos = (iidx < stream->index_count) ?
                          stream->index_int[iidx].stream_pos : 0;

            /* Restart timer (unless first cell had index) */
            if (nfidx != 0) itime = 0;
        }
    }

    /* All indices must have been found */
    if (iidx < stream->index_count)
        return KF_MISSING_INDEX;

    /* Use additional cell if last index had incomplete flux */
    if (stream->index_int[iidx - 1].stream_pos >= stream->flux_count)
        stream->flux_count++;

    /* Check for damaged index at last revolution */
    if (stream->index_int[iidx-1].sample_counter == 0 &&
        stream->indices[iidx-1].pre_index_time == 0 &&
        stream->indices[iidx-1].post_index_time == 0)
        return KF_MISSING_INDEX;

    return KF_OK;
}

/*============================================================================
 * FILL STATISTICS - Ported from KFReader.cs fillStreamStat()
 *============================================================================*/

static void kf_fill_statistics(kf_stream_t* stream)
{
    memset(&stream->stats, 0, sizeof(kf_statistics_t));

    /* Transfer rate */
    if (stream->stat_data_time != 0)
        stream->stats.avg_bps =
            ((double)stream->stat_data_count * 1000.0) / stream->stat_data_time;

    /* RPM from index times (skip first incomplete revolution) */
    int sum = 0, vmin = INT_MAX, vmax = 0;
    for (int i = 1; i < stream->index_count; i++) {
        int rot_time = stream->indices[i].index_time;
        if (rot_time < vmin) vmin = rot_time;
        if (rot_time > vmax) vmax = rot_time;
        sum += rot_time;
    }

    int rev_count = stream->index_count - 1;
    if (rev_count > 0) {
        stream->stats.avg_rpm = (stream->sck_value * (double)rev_count * 60.0) / (double)sum;
        stream->stats.max_rpm = (stream->sck_value * 60.0) / (double)vmin;
        stream->stats.min_rpm = (stream->sck_value * 60.0) / (double)vmax;
    }

    /* Average flux count per revolution */
    if (stream->index_count > 2) {
        sum = 0;
        for (int i = 1; i < stream->index_count; i++)
            sum += stream->indices[2].flux_position - stream->indices[1].flux_position;
        stream->stats.avg_flux = sum / rev_count;
    }

    stream->stats.flux_min = stream->flux_min;
    stream->stats.flux_max = stream->flux_max;
}

/*============================================================================
 * FIND HW INFO VALUE - Ported from KFReader.cs findName()
 *============================================================================*/

/**
 * @brief Search for name=value pair in HW info string
 *
 * Example: kf_find_hw_value(stream, "sck") returns "24027428.5714285"
 */
static bool kf_find_hw_value(const kf_stream_t* stream,
                              const char* name,
                              char* value_out, size_t value_size)
{
    if (!stream->hw_info_len || !name) return false;

    const char* found = strstr(stream->hw_info, name);
    if (!found) return false;

    /* Skip name and '=' */
    const char* start = found + strlen(name);
    if (*start != '=') return false;
    start++;

    /* Find end (comma or end of string) */
    const char* end = strchr(start, ',');
    if (!end) end = stream->hw_info + stream->hw_info_len;

    size_t len = (size_t)(end - start);
    if (len >= value_size) len = value_size - 1;
    memcpy(value_out, start, len);
    value_out[len] = '\0';
    return true;
}

/*============================================================================
 * PUBLIC API - readStream() equivalent
 *============================================================================*/

/**
 * @brief Parse a KryoFlux stream file
 *
 * Ported from KFReader.cs readStream().
 * Calls parseStream() → indexAnalysis() → fillStreamStat()
 * then updates clock values from HW info if available.
 *
 * @param data       Raw stream file data
 * @param size       Data size
 * @param stream     Output structure (caller allocates, caller frees with kf_stream_free)
 * @return Status code
 */
kf_status_t kf_stream_parse(const uint8_t* data, size_t size,
                              kf_stream_t* stream)
{
    if (!data || !stream) return KF_READ_ERROR;

    memset(stream, 0, sizeof(kf_stream_t));
    stream->sck_value = KF_SCK_DEFAULT;
    stream->ick_value = KF_ICK_DEFAULT;

    /* Step 1: Parse stream blocks */
    kf_status_t status = kf_parse_stream(data, size, stream);
    if (status != KF_OK) {
        stream->status = status;
        return status;
    }

    /* Step 2: Index analysis (sub-cell timing) */
    status = kf_index_analysis(stream);
    if (status != KF_OK) {
        stream->status = status;
        return status;
    }

    /* Step 3: Fill statistics */
    kf_fill_statistics(stream);

    /* Step 4: Update clocks from HW info (firmware 2.0+) */
    char val_buf[64];
    if (kf_find_hw_value(stream, "sck", val_buf, sizeof(val_buf))) {
        stream->sck_value = atof(val_buf);
        /* Recompute stats with corrected clock */
        kf_fill_statistics(stream);
    }
    if (kf_find_hw_value(stream, "ick", val_buf, sizeof(val_buf))) {
        stream->ick_value = atof(val_buf);
    }

    stream->valid = true;
    stream->status = KF_OK;
    return KF_OK;
}

/*============================================================================
 * FREE
 *============================================================================*/

void kf_stream_free(kf_stream_t* stream) {
    if (!stream) return;
    free(stream->flux_values);
    free(stream->flux_stream_pos);
    stream->flux_values = NULL;
    stream->flux_stream_pos = NULL;
    stream->flux_count = 0;
}

/*============================================================================
 * DIAGNOSTICS
 *============================================================================*/

static const char* kf_status_names[] = {
    "OK", "Missing Data", "Invalid Code", "Wrong Position",
    "Device Buffer Error", "Device Index Error", "Transfer Error",
    "Invalid OOB", "Missing End", "Index Reference Error",
    "Missing Index", "Read Error"
};

const char* kf_status_name(kf_status_t st) {
    return (st <= KF_READ_ERROR) ? kf_status_names[st] : "Unknown";
}

void kf_stream_print_info(const kf_stream_t* stream) {
    if (!stream) { printf("NULL stream\n"); return; }

    printf("=== KryoFlux Stream (AIR Enhanced) ===\n");
    printf("Status: %s\n", kf_status_name(stream->status));
    printf("Flux transitions: %d\n", stream->flux_count);
    printf("Index signals: %d (%d revolutions)\n",
           stream->index_count,
           stream->index_count > 0 ? stream->index_count - 1 : 0);
    printf("Sample clock: %.2f Hz\n", stream->sck_value);
    printf("Index clock: %.2f Hz\n", stream->ick_value);

    if (stream->hw_info_len > 0)
        printf("HW Info: %s\n", stream->hw_info);

    printf("Statistics:\n");
    printf("  RPM: avg=%.2f min=%.2f max=%.2f\n",
           stream->stats.avg_rpm, stream->stats.min_rpm,
           stream->stats.max_rpm);
    printf("  Flux: min=%d max=%d avg/rev=%d\n",
           stream->stats.flux_min, stream->stats.flux_max,
           stream->stats.avg_flux);
    printf("  Transfer: %.0f bytes/sec\n", stream->stats.avg_bps);

    /* Revolution details */
    for (int i = 0; i < stream->index_count; i++) {
        const kf_index_info_t* idx = &stream->indices[i];
        double rot_ms = (double)idx->index_time / stream->sck_value * 1000.0;
        printf("  Rev %d: time=%d sck (%.2f ms) flux_pos=%d "
               "pre=%d post=%d\n",
               i, idx->index_time, rot_ms, idx->flux_position,
               idx->pre_index_time, idx->post_index_time);
    }
}

/*============================================================================
 * SELF-TEST
 *============================================================================*/

#ifdef KF_AIR_TEST
#include <assert.h>

/* Build a minimal stream buffer for testing */
static size_t build_test_stream(uint8_t* buf, size_t cap) {
    size_t pos = 0;

    /* A few Flux1 values (0x0E-0xFF = value is the byte itself) */
    for (int i = 0; i < 50 && pos < cap; i++) {
        buf[pos++] = 0x40; /* Flux1: value = 0x40 = 64 sck */
    }

    /* An OOB StreamInfo block */
    buf[pos++] = KF_BH_OOB;
    buf[pos++] = KF_OOB_STREAM_INFO;
    buf[pos++] = 8; buf[pos++] = 0; /* size = 8 */
    /* stream_pos = 50 (little-endian) */
    buf[pos++] = 50; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
    /* transfer_time = 1 ms */
    buf[pos++] = 1; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;

    /* An OOB Index block */
    buf[pos++] = KF_BH_OOB;
    buf[pos++] = KF_OOB_INDEX;
    buf[pos++] = 12; buf[pos++] = 0; /* size = 12 */
    /* stream_pos = 25 */
    buf[pos++] = 25; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
    /* sample_counter = 32 */
    buf[pos++] = 32; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
    /* index_counter = 1000 */
    buf[pos++] = 0xE8; buf[pos++] = 0x03; buf[pos++] = 0; buf[pos++] = 0;

    /* More flux */
    for (int i = 0; i < 50 && pos < cap; i++) {
        buf[pos++] = 0x50; /* Flux1: value = 0x50 = 80 sck */
    }

    /* OOB StreamEnd */
    buf[pos++] = KF_BH_OOB;
    buf[pos++] = KF_OOB_STREAM_END;
    buf[pos++] = 8; buf[pos++] = 0;
    /* stream_pos = 100 */
    buf[pos++] = 100; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
    /* hw_status = OK */
    buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;

    /* OOB EOF */
    buf[pos++] = KF_BH_OOB;
    buf[pos++] = KF_OOB_EOF;
    buf[pos++] = 0; buf[pos++] = 0;

    return pos;
}

int main(void) {
    printf("=== KryoFlux Stream AIR Enhanced Parser Tests ===\n");

    /* Test 1: Empty stream */
    printf("Test 1: Empty stream... ");
    {
        kf_stream_t stream;
        uint8_t empty_stream[] = {
            KF_BH_OOB, KF_OOB_STREAM_END, 8, 0,
            0, 0, 0, 0,  /* stream_pos = 0 */
            0, 0, 0, 0,  /* hw_status = OK */
            KF_BH_OOB, KF_OOB_EOF, 0, 0
        };
        kf_status_t st = kf_stream_parse(empty_stream, sizeof(empty_stream), &stream);
        assert(st == KF_OK);
        assert(stream.flux_count == 0);
        assert(stream.index_count == 0);
        kf_stream_free(&stream);
        printf("OK\n");
    }

    /* Test 2: Stream with flux and index */
    printf("Test 2: Flux + Index... ");
    {
        uint8_t buf[1024];
        size_t len = build_test_stream(buf, sizeof(buf));

        kf_stream_t stream;
        kf_status_t st = kf_stream_parse(buf, len, &stream);
        assert(st == KF_OK);
        assert(stream.valid);
        assert(stream.flux_count == 100);
        assert(stream.index_count == 1);
        /* First 50 fluxes have value 0x40 */
        assert(stream.flux_values[0] == 0x40);
        /* Last 50 fluxes have value 0x50 */
        assert(stream.flux_values[50] == 0x50);
        kf_stream_print_info(&stream);
        kf_stream_free(&stream);
        printf("OK\n");
    }

    /* Test 3: Flux2 encoding */
    printf("Test 3: Flux2 encoding... ");
    {
        uint8_t buf[32];
        size_t pos = 0;
        /* Flux2: header=0x03, data=0x80 → value = 0x0380 = 896 */
        buf[pos++] = 0x03; buf[pos++] = 0x80;
        /* OOB StreamEnd + EOF */
        buf[pos++] = KF_BH_OOB; buf[pos++] = KF_OOB_STREAM_END;
        buf[pos++] = 8; buf[pos++] = 0;
        buf[pos++] = 2; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
        buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
        buf[pos++] = KF_BH_OOB; buf[pos++] = KF_OOB_EOF;
        buf[pos++] = 0; buf[pos++] = 0;

        kf_stream_t stream;
        kf_status_t st = kf_stream_parse(buf, pos, &stream);
        assert(st == KF_OK);
        assert(stream.flux_count == 1);
        assert(stream.flux_values[0] == 0x0380);
        kf_stream_free(&stream);
        printf("OK\n");
    }

    /* Test 4: Overflow */
    printf("Test 4: Overflow16... ");
    {
        uint8_t buf[32];
        size_t pos = 0;
        /* Ovl16 + Flux1(0x40) → value = 0x10000 + 0x40 = 65600 */
        buf[pos++] = KF_BH_OVL16;
        buf[pos++] = 0x40;
        /* Stream end + EOF */
        buf[pos++] = KF_BH_OOB; buf[pos++] = KF_OOB_STREAM_END;
        buf[pos++] = 8; buf[pos++] = 0;
        buf[pos++] = 2; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
        buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
        buf[pos++] = KF_BH_OOB; buf[pos++] = KF_OOB_EOF;
        buf[pos++] = 0; buf[pos++] = 0;

        kf_stream_t stream;
        kf_status_t st = kf_stream_parse(buf, pos, &stream);
        assert(st == KF_OK);
        assert(stream.flux_count == 1);
        assert(stream.flux_values[0] == 0x10040);
        kf_stream_free(&stream);
        printf("OK\n");
    }

    /* Test 5: Default clock values */
    printf("Test 5: Clock defaults... ");
    {
        kf_stream_t stream;
        memset(&stream, 0, sizeof(stream));
        stream.sck_value = KF_SCK_DEFAULT;
        stream.ick_value = KF_ICK_DEFAULT;
        /* SCK should be ~24027428.57 */
        assert(stream.sck_value > 24000000.0 && stream.sck_value < 24100000.0);
        assert(fabs(stream.ick_value - stream.sck_value / 8.0) < 1.0);
        printf("OK\n");
    }

    printf("\n=== All KryoFlux AIR tests passed ===\n");
    return 0;
}
#endif
