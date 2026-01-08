#ifndef UFT_SAFE_MATH_H
#define UFT_SAFE_MATH_H
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/* Safe add with overflow check */
static inline bool uft_safe_add_u32(uint32_t a, uint32_t b, uint32_t *result) {
    if (a > UINT32_MAX - b) return false;
    *result = a + b;
    return true;
}

/* Safe multiply with overflow check */
static inline bool uft_safe_mul_u32(uint32_t a, uint32_t b, uint32_t *result) {
    if (b != 0 && a > UINT32_MAX / b) return false;
    *result = a * b;
    return true;
}
#endif
