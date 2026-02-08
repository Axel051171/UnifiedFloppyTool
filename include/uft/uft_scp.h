/*
 * uft_scp.h — SuperCard Pro (.scp) reader (C99)
 *
 * Focus: Floppy imaging / recovery.
 * - Strict bounds checks
 * - No hidden heap allocations (except the FILE* opened by scp_open)
 * - Deterministic, GUI-friendly metadata output
 *
 * This module parses:
 *  - SCP file header (0x2B0 bytes, "SCP")
 *  - Extended header mode (flag 0x40) using track offsets table at absolute 0x80
 *  - Track blocks ("TRK") containing per-revolution descriptors and flux delta lists
 *
 * Flux deltas are 16-bit BE values:
 *  - non-zero: add to time
 *  - zero: overflow, add 0x10000
 *
 * NOTE: This reads *flux transition times* (cumulative), not decoded bits/bytes.
 * You can feed the transitions into your flux->bit decoder.
 *
 * License: MIT
 */
#ifndef UFT_SCP_H
#define UFT_SCP_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- constants ----- */
#define UFT_SCP_MAX_TRACK_ENTRIES 168u

/* ----- status / error codes ----- */
typedef enum uft_scp_rc {
    UFT_SCP_OK = 0,
    UFT_SCP_EINVAL = -1,
    UFT_SCP_EIO = -2,
    UFT_SCP_EFORMAT = -3,
    UFT_SCP_EBOUNDS = -4,
    UFT_SCP_ENOMEM = -5
} uft_scp_rc_t;

/* ----- on-disk header (packed) ----- */
#pragma pack(push, 1)
typedef struct uft_scp_file_header {
    uint8_t  signature[3];      /* "SCP" */
    uint8_t  version;
    uint8_t  disk_type;
    uint8_t  num_revs;
    uint8_t  start_track;
    uint8_t  end_track;
    uint8_t  flags;
    uint8_t  bitcell_encoding;
    uint8_t  sides;
    uint8_t  reserved;
    uint32_t checksum;
    uint32_t track_offsets[UFT_SCP_MAX_TRACK_ENTRIES]; /* little-endian offsets */
} uft_scp_file_header_t;

typedef struct uft_scp_track_rev {
    uint32_t time_duration; /* little-endian */
    uint32_t data_length;   /* number of 16-bit values */
    uint32_t data_offset;   /* byte offset from track block start */
} uft_scp_track_rev_t;

typedef struct uft_scp_track_header {
    uint8_t signature[3];   /* "TRK" */
    uint8_t track_number;
} uft_scp_track_header_t;
#pragma pack(pop)

/* ----- parsed image handle ----- */
typedef struct uft_scp_image {
    FILE *f;
    uft_scp_file_header_t hdr;      /* raw header as read */
    uint32_t track_offsets[UFT_SCP_MAX_TRACK_ENTRIES]; /* normalized offsets table (host endian) */
    uint8_t  extended_mode;         /* hdr.flags & 0x40 */
} uft_scp_image_t;

/* ----- GUI-facing metadata ----- */
typedef struct uft_scp_track_info {
    uint8_t  track_index;       /* 0..167 entry index in offsets table */
    uint32_t file_offset;       /* absolute file offset for TRK block */
    uint8_t  present;           /* offset != 0 */
    uint8_t  track_number;      /* from TRK header if present */
    uint8_t  num_revs;          /* from file header */
} uft_scp_track_info_t;

/* ----- API ----- */

/* Open + parse header (and extended offsets if needed). */
int uft_scp_open(uft_scp_image_t *img, const char *path);

/* Close file handle. */
void uft_scp_close(uft_scp_image_t *img);

/* Read track info for offsets table entry 'track_index' (0..167). */
int uft_scp_get_track_info(uft_scp_image_t *img, uint8_t track_index, uft_scp_track_info_t *out);

/* Read per-revolution descriptors for a given track entry. Caller provides array revs[num_revs]. */
int uft_scp_read_track_revs(uft_scp_image_t *img, uint8_t track_index,
                           uft_scp_track_rev_t *revs, size_t revs_cap,
                           uft_scp_track_header_t *out_trk_hdr);

/*
 * Read flux transition times for a given track + revolution.
 *
 * Output:
 *  - transitions_out: cumulative times (ticks) of each transition in the revolution.
 *    (No implicit index, first transition is relative to 0.)
 *  - out_count: number of transitions written
 *  - out_total_time: cumulative time at end of revolution (ticks)
 *
 * The function will:
 *  - parse delta list (16-bit big endian values)
 *  - convert to cumulative times
 *  - stop early if transitions_cap is reached (returns UFT_SCP_EBOUNDS but still writes what fits)
 */
int uft_scp_read_rev_transitions(uft_scp_image_t *img, uint8_t track_index, uint8_t rev_index,
                                uint32_t *transitions_out, size_t transitions_cap,
                                size_t *out_count, uint32_t *out_total_time);

#ifdef __cplusplus
}
#endif
#endif /* UFT_SCP_H */
