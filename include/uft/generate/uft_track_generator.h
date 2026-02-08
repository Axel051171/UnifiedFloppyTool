/**
 * @file uft_track_generator.h
 * @brief Track Generation API
 */

#ifndef UFT_TRACK_GENERATOR_H
#define UFT_TRACK_GENERATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Enums
 *===========================================================================*/

typedef enum {
    UFT_DENSITY_SD = 0,     /* Single density (FM) */
    UFT_DENSITY_DD = 1,     /* Double density (MFM) */
    UFT_DENSITY_HD = 2,     /* High density */
    UFT_DENSITY_ED = 3      /* Extra density */
} uft_density_t;

typedef enum {
    UFT_GEN_FORMAT_IBM = 0,
    UFT_GEN_FORMAT_AMIGA,
    UFT_GEN_FORMAT_FM
} uft_gen_format_t;

/*===========================================================================
 * Structures
 *===========================================================================*/

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t length;
} uft_track_buffer_t;

typedef struct {
    int cylinder;
    int head;
    int sectors;
    int size_code;          /* 0=128, 1=256, 2=512, 3=1024, ... */
    int first_sector;
    uft_density_t density;
    uint8_t fill_byte;
    size_t track_length;
    int gap3_size;
    bool deleted;
    const uint8_t *data;    /* Sector data (sectors * sector_size bytes) */
    const int *interleave;  /* Sector interleave table (optional) */
} uft_gen_params_t;

/*===========================================================================
 * Buffer Management
 *===========================================================================*/

int uft_track_buffer_init(uft_track_buffer_t *buf, size_t capacity);
void uft_track_buffer_free(uft_track_buffer_t *buf);

/*===========================================================================
 * Track Generation
 *===========================================================================*/

int uft_gen_ibm_track(const uft_gen_params_t *params, uft_track_buffer_t *buf);
int uft_gen_amiga_track(const uft_gen_params_t *params, uft_track_buffer_t *buf);
int uft_gen_format_track(uft_gen_format_t format, const uft_gen_params_t *params,
                         uft_track_buffer_t *buf);

/*===========================================================================
 * Presets
 *===========================================================================*/

void uft_gen_preset_ibm_720k(uft_gen_params_t *params);
void uft_gen_preset_ibm_1440k(uft_gen_params_t *params);
void uft_gen_preset_amiga_dd(uft_gen_params_t *params);
void uft_gen_preset_amiga_hd(uft_gen_params_t *params);

/*===========================================================================
 * Interleave
 *===========================================================================*/

int uft_gen_interleave(int sectors, int interleave, int first,
                       int *table, size_t table_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_GENERATOR_H */
