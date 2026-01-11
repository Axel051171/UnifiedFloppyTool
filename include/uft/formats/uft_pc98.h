/**
 * @file uft_pc98.h
 * @brief NEC PC-98 Format Support with Shift-JIS Encoding
 * @version 3.6.0
 */

#ifndef UFT_PC98_H
#define UFT_PC98_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
typedef enum uft_pc98_rc {
    UFT_PC98_SUCCESS        =  0,
    UFT_PC98_ERR_ARG        = -1,
    UFT_PC98_ERR_IO         = -2,
    UFT_PC98_ERR_NOMEM      = -3,
    UFT_PC98_ERR_FORMAT     = -4,
    UFT_PC98_ERR_GEOMETRY   = -5,
    UFT_PC98_ERR_ENCODING   = -6,
    UFT_PC98_ERR_NOTFOUND   = -7,
    UFT_PC98_ERR_RANGE      = -8,
    UFT_PC98_ERR_READONLY   = -9
} uft_pc98_rc_t;

/* PC-98 geometry types */
typedef enum uft_pc98_geometry_type {
    UFT_PC98_GEOM_UNKNOWN   = 0,
    UFT_PC98_GEOM_2DD_640   = 1,   /* 80T x 2H x 8S x 512B = 640KB */
    UFT_PC98_GEOM_2HD_1232  = 2,   /* 77T x 2H x 8S x 1024B = 1.2MB (native) */
    UFT_PC98_GEOM_2HC_1200  = 3,   /* 80T x 2H x 15S x 512B = 1.2MB (IBM compat) */
    UFT_PC98_GEOM_2HQ_1440  = 4,   /* 80T x 2H x 18S x 512B = 1.44MB */
    UFT_PC98_GEOM_2DD_320   = 5,   /* 40T x 2H x 8S x 512B = 320KB */
    UFT_PC98_GEOM_2D_360    = 6,   /* 40T x 2H x 9S x 512B = 360KB */
    UFT_PC98_GEOM_COUNT     = 7
} uft_pc98_geometry_type_t;

/* Geometry descriptor */
typedef struct uft_pc98_geometry {
    uft_pc98_geometry_type_t type;
    uint16_t tracks;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    uint32_t total_bytes;
    const char* name;
    uint8_t  media_byte;
} uft_pc98_geometry_t;

/* Shift-JIS conversion result */
typedef struct uft_sjis_result {
    char*    utf8_str;
    size_t   utf8_len;
    uint32_t errors;
    bool     has_fullwidth;
} uft_sjis_result_t;

/* FDI-98 header (Anex86 format) */
#pragma pack(push, 1)
typedef struct uft_fdi98_header {
    uint8_t  reserved[4];
    uint32_t fdd_type;
    uint32_t header_size;
    uint32_t sector_size;
    uint32_t sectors_per_track;
    uint32_t heads;
    uint32_t tracks;
} uft_fdi98_header_t;
#pragma pack(pop)

#define UFT_FDI98_TYPE_2DD_640   0x00
#define UFT_FDI98_TYPE_2HD_1232  0x10
#define UFT_FDI98_TYPE_2HC_1200  0x20
#define UFT_FDI98_TYPE_2HQ_1440  0x30
#define UFT_FDI98_HEADER_SIZE    4096

/* FDI-98 context */
typedef struct uft_fdi98_ctx {
    uft_fdi98_header_t header;
    char* path;
    bool writable;
    uint64_t file_size;
    uint64_t data_offset;
    uft_pc98_geometry_t geometry;
} uft_fdi98_ctx_t;

/* PC-98 format types */
typedef enum uft_pc98_format {
    UFT_PC98_FMT_UNKNOWN    = 0,
    UFT_PC98_FMT_D88        = 1,
    UFT_PC98_FMT_FDI98      = 2,
    UFT_PC98_FMT_NFD        = 3,
    UFT_PC98_FMT_HDM        = 4,
    UFT_PC98_FMT_RAW        = 5,
    UFT_PC98_FMT_DIM        = 6,
    UFT_PC98_FMT_FDD        = 7
} uft_pc98_format_t;

/* Detection result */
typedef struct uft_pc98_detect_result {
    uft_pc98_format_t format;
    uft_pc98_geometry_type_t geometry;
    uint8_t format_confidence;
    uint8_t geometry_confidence;
    bool has_sjis_label;
    char label_utf8[64];
} uft_pc98_detect_result_t;

/* Analysis report */
typedef struct uft_pc98_report {
    uft_pc98_format_t format;
    uft_pc98_geometry_t geometry;
    char label_sjis[20];
    char label_utf8[64];
    uint32_t total_sectors;
    uint32_t readable_sectors;
    uint32_t error_sectors;
    uint32_t deleted_sectors;
    bool has_boot_sector;
    bool has_fat;
    bool is_bootable;
    char filesystem[32];
} uft_pc98_report_t;

/* API Functions */
const uft_pc98_geometry_t* uft_pc98_get_geometry(uft_pc98_geometry_type_t type);
uft_pc98_geometry_type_t uft_pc98_detect_geometry_by_size(uint64_t file_size, uint8_t* confidence);
uft_pc98_rc_t uft_pc98_validate_geometry(uint16_t tracks, uint8_t heads, uint8_t sectors, uint16_t sector_size);

/* Shift-JIS functions */
uft_pc98_rc_t uft_pc98_sjis_to_utf8(const uint8_t* sjis_data, size_t sjis_len, uft_sjis_result_t* result);
uft_pc98_rc_t uft_pc98_utf8_to_sjis(const char* utf8_str, uint8_t* sjis_data, size_t sjis_capacity, size_t* sjis_len);
uft_pc98_rc_t uft_pc98_decode_disk_label(const uint8_t* raw_label, size_t raw_len, char* utf8_label, size_t utf8_capacity);
bool uft_pc98_is_valid_sjis(const uint8_t* data, size_t len, uint8_t* confidence);

/* FDI-98 functions */
bool uft_fdi98_detect(const uint8_t* buffer, size_t size, uint8_t* confidence);
uft_pc98_rc_t uft_fdi98_open(uft_fdi98_ctx_t* ctx, const char* path, bool writable);
uft_pc98_rc_t uft_fdi98_read_sector(uft_fdi98_ctx_t* ctx, uint16_t track, uint8_t head, uint8_t sector, uint8_t* buffer, size_t buffer_size);
uft_pc98_rc_t uft_fdi98_write_sector(uft_fdi98_ctx_t* ctx, uint16_t track, uint8_t head, uint8_t sector, const uint8_t* data, size_t data_size);
void uft_fdi98_close(uft_fdi98_ctx_t* ctx);
uft_pc98_rc_t uft_fdi98_to_raw(uft_fdi98_ctx_t* ctx, const char* output_path);
uft_pc98_rc_t uft_fdi98_create_from_raw(const char* raw_path, const char* fdi98_path, uft_pc98_geometry_type_t geometry);

/* Auto-detection and analysis */
uft_pc98_rc_t uft_pc98_detect(const char* path, uft_pc98_detect_result_t* result);
uft_pc98_rc_t uft_pc98_convert(const char* input_path, const char* output_path, uft_pc98_format_t output_format);
uft_pc98_rc_t uft_pc98_analyze(const char* path, uft_pc98_report_t* report);
int uft_pc98_report_to_json(const uft_pc98_report_t* report, char* json_out, size_t json_capacity);
int uft_pc98_report_to_markdown(const uft_pc98_report_t* report, char* md_out, size_t md_capacity);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PC98_H */
