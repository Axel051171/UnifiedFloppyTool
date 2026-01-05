/**
 * @file uft_samdisk.h
 * @brief SAMdisk Integration Header
 * 
 * EXT4-015: SAMdisk format support
 */

#ifndef UFT_SAMDISK_H
#define UFT_SAMDISK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Format Types
 *===========================================================================*/

typedef enum {
    UFT_SAMDISK_UNKNOWN,
    UFT_SAMDISK_SAD,        /* SAM Coupé SAD format */
    UFT_SAMDISK_DSK,        /* Standard DSK */
    UFT_SAMDISK_EDSK        /* Extended DSK */
} uft_samdisk_format_t;

typedef enum {
    UFT_SYSTEM_UNKNOWN,
    UFT_SYSTEM_SAM_COUPE,
    UFT_SYSTEM_SPECTRUM_P3,
    UFT_SYSTEM_CPC,
    UFT_SYSTEM_PCW,
    UFT_SYSTEM_MSX,
    UFT_SYSTEM_ENTERPRISE
} uft_samdisk_system_t;

/*===========================================================================
 * SAD Context
 *===========================================================================*/

typedef struct {
    const uint8_t *image;
    size_t image_size;
    int sides;
    int tracks;
    int sectors;
    int sector_size;
    bool valid;
} uft_sad_ctx_t;

/*===========================================================================
 * Extended DSK Structures
 *===========================================================================*/

#define UFT_EDSK_MAX_SECTORS    29
#define UFT_EDSK_MAX_TRACKS     84

typedef struct {
    int track;
    int side;
    int sector_id;
    int size_code;
    int data_length;
    uint8_t fdc_status1;
    uint8_t fdc_status2;
    const uint8_t *data;
} uft_edsk_sector_t;

typedef struct {
    int track;
    int side;
    int sector_size_code;
    int sector_count;
    int gap3_length;
    int filler_byte;
    uft_edsk_sector_t sectors[UFT_EDSK_MAX_SECTORS];
} uft_edsk_track_t;

typedef struct {
    const uint8_t *image;
    size_t image_size;
    int tracks;
    int sides;
    bool is_extended;
    uint8_t track_sizes[UFT_EDSK_MAX_TRACKS * 2];
    size_t track_offsets[UFT_EDSK_MAX_TRACKS * 2];
    bool valid;
} uft_edsk_ctx_t;

typedef struct {
    uint8_t *buffer;
    size_t buffer_size;
    size_t current_offset;
    int tracks;
    int sides;
} uft_edsk_writer_t;

typedef struct {
    int tracks;
    int sides;
    int total_sectors;
    int error_sectors;
    int weak_sectors;
    bool is_extended;
    bool has_size_variations;
    bool has_errors;
    bool has_protection;
} uft_edsk_analysis_t;

/*===========================================================================
 * System-Specific Structures
 *===========================================================================*/

/* SAM Coupé */
typedef struct {
    char name[16];
    int type;
    int size;
    int sectors;
    int start_track;
    int start_sector;
} uft_sam_file_t;

/* Spectrum +3 */
typedef struct {
    char name[16];
    int user;
    int extent;
    int records;
    int size;
    uint8_t blocks[16];
} uft_p3_file_t;

/* Amstrad CPC */
typedef struct {
    char name[16];
    int user;
    int extent;
    int records;
    int size;
} uft_cpc_file_t;

/* MSX */
typedef struct {
    int bytes_per_sector;
    int sectors_per_cluster;
    int reserved_sectors;
    int num_fats;
    int root_entries;
    int total_sectors;
    int media_type;
    int fat_sectors;
    const char *format_name;
} uft_msx_info_t;

/* Enterprise */
typedef struct {
    char name[16];
    int attributes;
    int extent;
    int records;
    int size;
} uft_ep_file_t;

/*===========================================================================
 * SAD Functions
 *===========================================================================*/

int uft_sad_open(uft_sad_ctx_t *ctx, const uint8_t *data, size_t size);
int uft_sad_read_sector(const uft_sad_ctx_t *ctx, int track, int side, int sector,
                        uint8_t *buffer, size_t size);

/*===========================================================================
 * Extended DSK Functions
 *===========================================================================*/

int uft_edsk_open(uft_edsk_ctx_t *ctx, const uint8_t *data, size_t size);
int uft_edsk_get_track_info(const uft_edsk_ctx_t *ctx, int track, int side,
                            uft_edsk_track_t *info);
int uft_edsk_read_sector(const uft_edsk_ctx_t *ctx, int track, int side,
                         int sector_id, uint8_t *buffer, size_t *size);

int uft_edsk_create(uft_edsk_writer_t *writer, int tracks, int sides,
                    const char *creator);
int uft_edsk_add_track(uft_edsk_writer_t *writer, const uft_edsk_track_t *track);
int uft_edsk_finish(uft_edsk_writer_t *writer, uint8_t **data, size_t *size);
void uft_edsk_writer_free(uft_edsk_writer_t *writer);

int uft_edsk_analyze(const uft_edsk_ctx_t *ctx, uft_edsk_analysis_t *analysis);
int uft_edsk_report_json(const uft_edsk_ctx_t *ctx, char *buffer, size_t size);

/*===========================================================================
 * Conversion Functions
 *===========================================================================*/

int uft_sad_to_edsk(const uft_sad_ctx_t *sad, uft_edsk_writer_t *writer);

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

uft_samdisk_format_t uft_samdisk_detect(const uint8_t *data, size_t size);
uft_samdisk_system_t uft_samdisk_detect_system(const uint8_t *image, size_t size);
const char *uft_samdisk_system_name(uft_samdisk_system_t system);

/*===========================================================================
 * System-Specific Functions
 *===========================================================================*/

/* SAM Coupé */
int uft_sam_read_directory(const uint8_t *image, size_t size,
                           uft_sam_file_t *files, size_t max_files, size_t *count);
int uft_sam_read_file(const uint8_t *image, size_t size,
                      const uft_sam_file_t *file, uint8_t *buffer, size_t *buf_size);

/* Spectrum +3 */
int uft_p3_validate_header(const uint8_t *data, size_t size);
int uft_p3_read_directory(const uint8_t *image, size_t size,
                          uft_p3_file_t *files, size_t max_files, size_t *count);

/* Amstrad CPC */
int uft_cpc_read_directory(const uint8_t *image, size_t size,
                           uft_cpc_file_t *files, size_t max_files, size_t *count);

/* MSX */
int uft_msx_parse_boot(const uint8_t *image, size_t size, uft_msx_info_t *info);

/* Enterprise */
int uft_ep_read_directory(const uint8_t *image, size_t size,
                          uft_ep_file_t *files, size_t max_files, size_t *count);

/* Report */
int uft_samdisk_system_report(const uint8_t *image, size_t size,
                              char *buffer, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAMDISK_H */
