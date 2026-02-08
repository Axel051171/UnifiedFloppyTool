#pragma once
/*
 * guess.h — format/geometry guesser for raw disk images (C11)
 *
 * Inputs:
 * - raw image bytes (unknown)
 *
 * Outputs:
 * - guessed container/FS and geometry hints
 *
 * This is meant to complement your existing format detection.
 */
#include <stddef.h>
#include <stdint.h>
#include "uft/uft_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_guess_kind {
    UFT_GUESS_UNKNOWN = 0,
    UFT_GUESS_FAT = 1,
    UFT_GUESS_RAW_GEOMETRY = 2
} uft_guess_kind_t;

typedef struct uft_geometry_hint {
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors_per_track;
    uint16_t bytes_per_sector;
} uft_geometry_hint_t;

typedef struct uft_guess_result {
    uft_guess_kind_t kind;
    int confidence; /* 0..100 */
    uft_geometry_hint_t geom;
    char label[64]; /* short label for GUI */
} uft_guess_result_t;

/* Guess based on size + FAT probe + common geometry table. */
uft_rc_t uft_guess_image(const uint8_t *img, size_t len, uft_guess_result_t *out, uft_diag_t *diag);

#ifdef __cplusplus
}
#endif
