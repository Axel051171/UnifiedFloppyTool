#ifndef UFT_PRESET_H
#define UFT_PRESET_H
#include <stdint.h>
typedef struct uft_preset uft_preset_t;
const uft_preset_t *uft_preset_find(const char *name);
#endif
