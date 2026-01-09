#ifndef TRACKS_LUTS_H
#define TRACKS_LUTS_H
#include <stdint.h>

/* MFM encoding lookup tables */
extern const uint16_t mfm_encode_table[256];
extern const uint8_t mfm_decode_table[65536];

/* FM encoding lookup tables */
extern const uint16_t fm_encode_table[256];
extern const uint8_t fm_decode_table[65536];

/* GCR lookup tables */
extern const uint8_t gcr_encode_table[16];
extern const uint8_t gcr_decode_table[32];

#endif
