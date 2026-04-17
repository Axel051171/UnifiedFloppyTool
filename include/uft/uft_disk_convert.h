/**
 * @file uft_disk_convert.h
 * @brief Cross-format disk conversion (Master-API Phase 7).
 */
#ifndef UFT_DISK_CONVERT_H
#define UFT_DISK_CONVERT_H

#include "uft/uft_disk_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_CONVERT_AUTO       = 0,
    UFT_CONVERT_SECTOR     = 1,
    UFT_CONVERT_BITSTREAM  = 2,
    UFT_CONVERT_FLUX       = 3,
} uft_convert_mode_t;

typedef struct {
    size_t              tracks_converted;
    size_t              tracks_failed;
    size_t              sectors_copied;
    size_t              sectors_lost;

    uft_convert_mode_t  mode_used;

    bool                weak_bits_lost;
    bool                timing_lost;
    bool                protection_lost;
} uft_convert_result_t;

typedef struct {
    uft_disk_op_options_t  base;

    uft_convert_mode_t     mode;
    bool                   force_geometry;
    uft_geometry_t         target_geometry;
    bool                   allow_data_loss;
} uft_disk_convert_options_t;

#define UFT_CONVERT_OPTIONS_DEFAULT \
    { UFT_DISK_OP_OPTIONS_DEFAULT, UFT_CONVERT_AUTO, false, {0}, false }

uft_error_t uft_disk_convert(uft_disk_t *source,
                              const char *target_path,
                              uft_format_t target_format,
                              const uft_disk_convert_options_t *options,
                              uft_convert_result_t *result);

uft_error_t uft_disk_convert_to_disk(uft_disk_t *source, uft_disk_t *target,
                                       const uft_disk_convert_options_t *options,
                                       uft_convert_result_t *result);

uft_error_t uft_disk_convert_check(uft_format_t source_format,
                                    uft_format_t target_format,
                                    uft_convert_mode_t *recommended_mode,
                                    bool *lossy_out);

uft_error_t uft_convert_result_to_text(const uft_convert_result_t *result,
                                         char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_CONVERT_H */
