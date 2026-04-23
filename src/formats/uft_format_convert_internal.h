/**
 * @file uft_format_convert_internal.h
 * @brief Internal header for the format conversion module.
 *
 * Declares all shared types, tables, helpers, and converter functions
 * that are split across the uft_format_convert_*.c translation units.
 * This header is NOT part of the public API.
 */

#ifndef UFT_FORMAT_CONVERT_INTERNAL_H
#define UFT_FORMAT_CONVERT_INTERNAL_H

#include "uft/uft_format_convert.h"
#include "uft/uft_format_probe.h"
#include "uft/uft_error.h"

/* Format parsers */
#include "uft/uft_format_parsers.h"
#include "uft/formats/uft_scp.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/uft_scp_format.h"
#include "uft/uft_hfe_format.h"
#include "uft/uft_c64_gcr.h"
#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/uft_d64_writer.h"
#include "uft/formats/uft_td0.h"
#include "uft/uft_flux_pll.h"
#include "uft/formats/uft_mfm.h"
#include "uft/uft_imd.h"

#include "uft/core/uft_forensic_constants.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================== */
/* Format info table type (shared between tables.c and dispatch.c)            */
/* ========================================================================== */

typedef struct format_info {
    uft_format_t        format;
    const char*         name;
    const char*         extension;
    uft_format_class_t  fclass;
} format_info_t;

/* ========================================================================== */
/* Tables (defined in uft_format_convert_tables.c)                            */
/* ========================================================================== */

extern const format_info_t g_format_info[];
extern const uft_conversion_path_t g_conversion_paths[];
extern const size_t g_num_conversion_paths;

/* ========================================================================== */
/* Shared helpers (defined in uft_format_convert_tables.c)                    */
/* ========================================================================== */

uft_error_t uftc_write_output_file(const char* path, const uint8_t* data,
                                    size_t size);
void uftc_report_progress(const uft_convert_options_ext_t* opts,
                           int percent, const char* stage);
bool uftc_is_cancelled(const uft_convert_options_ext_t* opts);

/* ========================================================================== */
/* Archive converters (defined in uft_format_convert_archive.c)               */
/* ========================================================================== */

uft_error_t uftc_convert_td0_to_img(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_td0_to_imd(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_nbz_to_d64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_nbz_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

/* ========================================================================== */
/* Flux converters (defined in uft_format_convert_flux.c)                     */
/* ========================================================================== */

uft_error_t uftc_convert_scp_to_d64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_scp_to_mfm_sectors(const uint8_t* src_data,
                                              size_t src_size,
                                              const char* dst_path,
                                              uft_format_t dst_format,
                                              const uft_convert_options_ext_t* opts,
                                              uft_convert_result_t* result);

uft_error_t uftc_convert_kryoflux_to_d64(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result);

uft_error_t uftc_convert_kryoflux_to_adf(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result);

uft_error_t uftc_convert_hfe_to_sectors(const uint8_t* src_data,
                                          size_t src_size,
                                          const char* dst_path,
                                          uft_format_t dst_format,
                                          const uft_convert_options_ext_t* opts,
                                          uft_convert_result_t* result);

uft_error_t uftc_convert_scp_to_hfe(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_scp_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_kryoflux_to_scp(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result);

uft_error_t uftc_convert_kryoflux_to_hfe(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result);

/* ========================================================================== */
/* Bitstream converters (defined in uft_format_convert_bitstream.c)           */
/* ========================================================================== */

uft_error_t uftc_convert_hfe_to_scp(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_g64_to_scp(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_g64_to_hfe(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_hfe_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_sectors_to_hfe(const uint8_t* src_data,
                                          size_t src_size,
                                          const char* dst_path,
                                          uft_format_t src_format,
                                          const uft_convert_options_ext_t* opts,
                                          uft_convert_result_t* result);

/* ========================================================================== */
/* Sector converters (defined in uft_format_convert_sector.c)                 */
/* ========================================================================== */

uft_error_t uftc_convert_g64_to_d64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_d64_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_imd_to_img(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_img_to_imd(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

#endif /* UFT_FORMAT_CONVERT_INTERNAL_H */
