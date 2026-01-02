#ifndef UFT_FDS_H
#define UFT_FDS_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t tracks, heads, sectors, sectorSize; bool flux_supported;
  void (*log_callback)(const char* msg); void *internal_ctx; } FloppyDevice;
int floppy_open(FloppyDevice*, const char*);
int floppy_close(FloppyDevice*);
int floppy_read_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,uint8_t*);
int floppy_write_sector(FloppyDevice*, uint32_t,uint32_t,uint32_t,const uint8_t*);
int floppy_analyze_protection(FloppyDevice*);

#endif
