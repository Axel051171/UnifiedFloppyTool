#ifndef UFT_FDS_H
#define UFT_FDS_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t tracks, heads, sectors, sectorSize; bool flux_supported;
  void (*log_callback)(const char* msg); void *internal_ctx; } FloppyDevice;
int uft_floppy_open(FloppyDevice*, const char*);
int uft_floppy_close(FloppyDevice*);
int uft_floppy_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
int uft_floppy_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
int uft_floppy_analyze_protection(FloppyDevice*);

#endif
