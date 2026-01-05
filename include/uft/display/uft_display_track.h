/**
 * @file uft_display_track.h
 * @brief Track Visualization API
 */

#ifndef UFT_DISPLAY_TRACK_H
#define UFT_DISPLAY_TRACK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Structures
 *===========================================================================*/

typedef struct {
    int sector_id;
    int cylinder;
    int head;
    size_t offset;
    size_t length;
    int data_size;
    bool valid;
    bool id_crc_ok;
    bool data_crc_ok;
    bool deleted;
    bool weak;
} uft_sector_info_t;

typedef struct {
    int cylinder;
    int head;
    size_t track_length;
    uft_sector_info_t sectors[64];
    int sector_count;
} uft_track_info_t;

typedef struct {
    const char *name;
    int tracks;
    int sides;
    int sectors_per_track;
    int *track_status;  /* Array[tracks * sides]: >0=OK, 0=missing, <0=error */
} uft_disk_info_t;

/*===========================================================================
 * Functions
 *===========================================================================*/

int uft_display_track_map(const uft_track_info_t *track, 
                          char *buffer, size_t size);

int uft_display_sector_table(const uft_track_info_t *track,
                             char *buffer, size_t size);

int uft_display_timing_diagram(const uint32_t *flux_times, size_t count,
                               double sample_clock, int width,
                               char *buffer, size_t size);

int uft_display_flux_histogram(const uint32_t *flux_times, size_t count,
                               double sample_clock,
                               char *buffer, size_t size);

int uft_display_disk_view(const uft_disk_info_t *disk,
                          char *buffer, size_t size);

int uft_display_svg_track(const uft_track_info_t *track,
                          char *buffer, size_t size);

int uft_display_html_report(const uft_disk_info_t *disk,
                            char *buffer, size_t size);

int uft_display_color_track(const uft_track_info_t *track,
                            char *buffer, size_t size, bool use_color);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISPLAY_TRACK_H */
