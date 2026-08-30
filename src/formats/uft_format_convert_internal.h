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

#include "uft/uft_compiler.h"
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

/* MF-545: schreibt das Ergebnis, ODER lehnt ab, wenn `tracks_converted == 0`.
 *
 * Benutze DIESE Funktion statt `uftc_write_output_file()` + `success = true`.
 * Die zweite Bauart macht den Erfolg davon abhaengig, dass sich die Datei
 * ANLEGEN liess — nicht davon, dass etwas drinsteht. Zweimal gemessen
 * (MF-538: 160 von 160 Spuren gescheitert, 901120 Byte Nullen als UFT_OK;
 * MF-539: 80 Spuren "gewandelt" ohne eine einzige Synchronmarke).
 * Begruendung ausfuehrlich bei der Definition in
 * uft_format_convert_tables.c. */
uft_error_t uftc_finish_or_refuse(uft_convert_result_t* result,
                                   const char* dst_path,
                                   const uint8_t* data, size_t size,
                                   const char* what);
void uftc_report_progress(const uft_convert_options_ext_t* opts,
                           int percent, const char* stage);
bool uftc_is_cancelled(const uft_convert_options_ext_t* opts);

/* UFT-A04: bounded printf-style warning append. Use this instead of
 * `snprintf(result->warnings[result->warning_count++], ...)` to avoid
 * the OOB-write that happens when warning_count reaches the array
 * capacity. See implementation in uft_format_convert_tables.c. */
void uftc_add_warning(uft_convert_result_t* result, const char* fmt, ...)
    UFT_PRINTF_FMT(2, 3);   /* MF-448: gnu_printf on MinGW, see uft_compiler.h */

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
                                              const char* src_path,
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
                                          const char* src_path,
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
                                     const char* src_path,
                                     const char* dst_path,
                                     const uft_convert_options_ext_t* opts,
                                     uft_convert_result_t* result);

/* MF-695: der Speicher-Kern von D64->G64. `uft_convert_memory()` ruft
 * IHN statt einer eigenen, von Hand nachgebauten Kette — die stand seit
 * MF-655 als Doppelung im Baum, mit einem Kommentar, der ihre Ursache
 * (MF-567) benannte. Wer einen neuen Wandler eintraegt, traegt ihn hier
 * ein und nicht zweimal. */
uft_error_t uftc_d64_to_g64_mem(const uint8_t* src_data, size_t src_size,
                                 const char* src_path,
                                 const uft_convert_options_ext_t* opts,
                                 uft_convert_result_t* result,
                                 uint8_t** out_data, size_t* out_size);

uft_error_t uftc_convert_d64_to_g64(const uint8_t* src_data, size_t src_size,
                                     const char* src_path,
                                     const char* dst_path,
                                     const uft_convert_options_ext_t* opts,
                                     uft_convert_result_t* result);

/* MF-655: ATR <-> XFD. XFD ist das ATR ohne seinen 16-Byte-Kopf
 * (byteweise gemessen am Korpus-Paar). Verlustfrei nur bei Sektorgroesse
 * 128 — die Angabe steht im Kopf und kann in XFD nicht abgelegt werden. */
uft_error_t uftc_convert_atr_to_xfd(const uint8_t* src_data, size_t src_size,
                                     const char* dst_path,
                                     const uft_convert_options_ext_t* opts,
                                     uft_convert_result_t* result);

uft_error_t uftc_convert_xfd_to_atr(const uint8_t* src_data, size_t src_size,
                                     const char* dst_path,
                                     const uft_convert_options_ext_t* opts,
                                     uft_convert_result_t* result);

/* Speicherweg: dieselben Verlustregeln, derselbe Kern. Der Verteiler
 * hat zwei Ketten (Datei und Speicher) — das hier ist der Eintritt fuer
 * die zweite, damit die Regeln nicht zweimal geschrieben werden. */
uft_error_t uftc_atr_xfd_memory(uft_format_t src_format,
                                 uft_format_t dst_format,
                                 const uint8_t* src_data, size_t src_size,
                                 const uft_convert_options_ext_t* opts,
                                 uft_convert_result_t* result,
                                 uint8_t** out, size_t* out_size);

uft_error_t uftc_convert_imd_to_img(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

uft_error_t uftc_convert_img_to_imd(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result);

#endif /* UFT_FORMAT_CONVERT_INTERNAL_H */
