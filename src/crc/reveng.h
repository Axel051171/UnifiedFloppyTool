/* reveng.h
 * Greg Cook, 7/Aug/2024
 *
 * CRC RevEng: arbitrary-precision CRC calculator and target file analyzer
 * Copyright (C) 2010-2024 Gregory Cook
 *
 * This file is part of CRC RevEng.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef REVENG_H
#define REVENG_H 1

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bitmap type for CRC calculations */
typedef uint32_t bmp_t;

/* Model structure */
typedef struct {
    bmp_t spoly;    /* search polynomial */
    bmp_t init;     /* initial register value */
    bmp_t xorout;   /* XOR mask for output */
    bmp_t check;    /* check value */
    int width;      /* width in bits */
    int flags;      /* flags */
    const char *name; /* algorithm name */
} model_t;

/* Flag definitions - bit positions */
#define P_REFIN   (1 << 0)   /* reflect input bytes */
#define P_REFOUT  (1 << 1)   /* reflect output CRC */
#define P_MULXN   (1 << 2)   /* multiply by x^n */
#define P_BE      (1 << 3)   /* big-endian */
#define P_LTLBYT  (1 << 4)   /* little-endian bytes */
#define P_DIRECT  (1 << 5)   /* direct algorithm */
#define P_UNDFCL  (1 << 6)   /* undefined class */
#define P_CLMASK  (P_MULXN | P_BE | P_LTLBYT | P_DIRECT | P_UNDFCL)

/* Preset table entry */
typedef struct {
    bmp_t spoly;
    int width;
    bmp_t init;
    int flags;
    bmp_t xorout;
    bmp_t check;
    bmp_t residue;
    const char *name;
} preset_t;

/* External preset table */
extern const preset_t crc_presets[];

/* Function prototypes */
void mcpy(model_t *dest, const model_t *src);
void mfree(model_t *model);
void mbynum(model_t *dest, int num);
const char *mbyname(model_t *dest, const char *name);
bmp_t crc_calc(const model_t *model, const unsigned char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* REVENG_H */
