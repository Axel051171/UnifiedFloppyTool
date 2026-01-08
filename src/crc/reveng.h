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

/* Bitmap type for CRC calculations */
typedef uint32_t bmp_t;

/* Model structure */
typedef struct {
    bmp_t spoly;    /* polynomial */
    bmp_t init;     /* initial value */
    bmp_t xorout;   /* XOR output */
    int width;      /* width in bits */
    int flags;      /* flags */
} model_t;

/* Flag definitions */
#define P_REFIN   1   /* reflect input */
#define P_REFOUT  2   /* reflect output */

/* Function prototypes */
void mcpy(model_t *dest, const model_t *src);
void mfree(model_t *model);
int mbynum(model_t *dest, int num);
const char *mbyname(model_t *dest, const char *name);

#endif /* REVENG_H */
