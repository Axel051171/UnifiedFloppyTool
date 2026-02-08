#ifndef UFT_CONFIDENCE_H
#define UFT_CONFIDENCE_H
#include <stdint.h>
#include <stddef.h>
typedef struct uft_confidence uft_confidence_t;
float uft_confidence_calculate(const uint8_t *data, size_t len);
#endif
