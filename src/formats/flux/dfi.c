/**
 * @file dfi.c
 * @brief DiscFerret DFI flux format
 * @version 3.8.0
 */
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/dfi.h"
#include <stdio.h>
#include <stdlib.h>
int uft_flx_dfi_open(FloppyDevice*d,const char*p){(void)p;d->flux_supported=true;return 0;}
int uft_flx_dfi_close(FloppyDevice*d){(void)d;return 0;}
int uft_flx_dfi_read_sector(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s,uint8_t*b){(void)d;(void)t;(void)h;(void)s;(void)b;return -4;}
int uft_flx_dfi_write_sector(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s,const uint8_t*b){(void)d;(void)t;(void)h;(void)s;(void)b;return -4;}
int uft_flx_dfi_analyze_protection(FloppyDevice*d){(void)d;return 0;}
