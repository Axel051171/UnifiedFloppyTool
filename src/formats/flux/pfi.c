#include "uft/formats/pfi.h"
#include <stdio.h>
#include <stdlib.h>
int floppy_open(FloppyDevice*d,const char*p){(void)p;d->flux_supported=true;return 0;}
int floppy_close(FloppyDevice*d){(void)d;return 0;}
int floppy_read_sector(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s,uint8_t*b){return -4;}
int floppy_write_sector(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s,const uint8_t*b){return -4;}
int floppy_analyze_protection(FloppyDevice*d){(void)d;return 0;}
