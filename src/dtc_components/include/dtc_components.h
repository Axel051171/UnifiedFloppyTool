#ifndef DTC_COMPONENTS_H
#define DTC_COMPONENTS_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t *data; size_t bit_count; int writable; } dtc_bits;
void dtc_bits_init(dtc_bits*, void*, size_t, int);
int dtc_bit_get(const dtc_bits*, size_t, unsigned*);
int dtc_bit_set(dtc_bits*, size_t, unsigned);
int dtc_bits_copy(dtc_bits*, size_t, const dtc_bits*, size_t, size_t);
size_t dtc_bits_compare(const dtc_bits*, size_t, const dtc_bits*, size_t, size_t);
size_t dtc_find_run_violation(const dtc_bits*, size_t, size_t, unsigned, unsigned, int);

uint32_t dtc_crc32(const void*, size_t, uint32_t);
uint16_t dtc_crc16_ccitt(const void*, size_t, uint16_t);
uint16_t dtc_crc16_ibm(const void*, size_t, uint16_t);
uint32_t dtc_crc_generic(const void*, size_t, unsigned, uint32_t, uint32_t, int);

int dtc_fm_encode(const uint8_t*, size_t, uint16_t*);
int dtc_fm_decode(const uint16_t*, size_t, uint8_t*, size_t*);
int dtc_mfm_encode(const uint8_t*, size_t, uint16_t*, unsigned*);
int dtc_mfm_decode(const uint16_t*, size_t, uint8_t*, size_t*, size_t*);
int dtc_gcr_encode(const uint8_t*, size_t, const uint8_t*, unsigned, uint8_t*, size_t*);
int dtc_gcr_decode(const uint8_t*, size_t, const uint8_t*, unsigned, uint8_t*, size_t*);
extern const uint8_t dtc_gcr_cbm_4to5[16];
extern const uint8_t dtc_gcr_apple_6and2[64];
extern const uint8_t dtc_gcr_vorpal_4to5[16];
extern const uint8_t dtc_gcr_vmax_6to8[64];

typedef struct { double mean_ns, stddev_ns, min_ns, max_ns; size_t count; } dtc_flux_stats;
int dtc_flux_measure(const uint32_t*, size_t, double, dtc_flux_stats*);
int dtc_flux_to_bits(const uint32_t*, size_t, double, uint8_t*, size_t, size_t*);

typedef enum { DTC_FMT_UNKNOWN, DTC_FMT_ADF, DTC_FMT_IMG, DTC_FMT_D64,
  DTC_FMT_G64, DTC_FMT_SCP, DTC_FMT_KRYO_RAW, DTC_FMT_CTRAW, DTC_FMT_IPF } dtc_format;
const char *dtc_format_name(dtc_format);
dtc_format dtc_detect_buffer(const uint8_t*, size_t, const char*);

typedef struct { size_t bit_offset, bit_length; unsigned cylinder, head, sector;
  uint32_t data_crc; int header_ok, data_ok; } dtc_sector;
size_t dtc_scan_mfm_ibm(const uint8_t*, size_t, dtc_sector*, size_t);
size_t dtc_scan_amiga_sync(const uint8_t*, size_t, size_t*, size_t);

typedef struct { double similarity; size_t compared_bits, differing_bits, best_shift; } dtc_match;
int dtc_track_match(const dtc_bits*, const dtc_bits*, size_t, dtc_match*);
size_t dtc_weak_regions(const uint8_t*const*, size_t, size_t, double, size_t*, size_t);

enum { DTC_PROT_NONE=0, DTC_PROT_LONG_TRACK=1, DTC_PROT_WEAK_BITS=2,
  DTC_PROT_ILLEGAL_MFM=4, DTC_PROT_VORPAL=8, DTC_PROT_VMAX=16 };
unsigned dtc_detect_protection(const dtc_bits*, size_t, size_t, size_t);

typedef struct { uint32_t version, flags, clock_ns; uint16_t cylinders, heads;
  uint32_t track_count; } dtc_ctraw_info;
int dtc_ctraw_write(FILE*, const dtc_ctraw_info*, const uint32_t*const*, const uint32_t*, const uint32_t*);
int dtc_ctraw_read(FILE*, dtc_ctraw_info*, uint32_t***, uint32_t**, uint32_t**);
void dtc_ctraw_free(uint32_t**, uint32_t*, uint32_t*, uint32_t);

typedef struct { uint32_t type, size, crc, id; long offset; } dtc_ipf_chunk;
int dtc_ipf_probe(FILE*);
size_t dtc_ipf_list_chunks(FILE*, dtc_ipf_chunk*, size_t);

#ifdef __cplusplus
}
#endif
#endif

