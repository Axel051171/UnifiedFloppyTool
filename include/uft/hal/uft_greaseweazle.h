#ifndef UFT_GREASEWEAZLE_H
#define UFT_GREASEWEAZLE_H
#include <stdint.h>
#include <stdbool.h>
typedef struct uft_gw_device uft_gw_device_t;
int uft_gw_open(uft_gw_device_t **dev, const char *port);
void uft_gw_close(uft_gw_device_t *dev);
#endif
