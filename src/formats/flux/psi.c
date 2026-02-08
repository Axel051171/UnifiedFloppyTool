/**
 * @file psi.c
 * @brief PCE Sector Image format
 * @version 3.8.0
 */
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/psi.h"
#include <stdio.h>
#include <stdlib.h>
int uft_flx_psi_open(FloppyDevice*d,const char*p){(void)p;d->flux_supported=false;return 0;}
int uft_flx_psi_close(FloppyDevice*d){(void)d;return 0;}
int uft_flx_psi_read_sector(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s,uint8_t*b){return -4;}
int uft_flx_psi_write_sector(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s,const uint8_t*b){return -4;}
int uft_flx_psi_analyze_protection(FloppyDevice*d){(void)d;return 0;}
