// test_d81.c
#include "uft/uft_error.h"
#include "d81.h"
#include <stdio.h>
#include <string.h>
static void log_cb(const char* m){ fprintf(stderr, "%s\n", m); }
int main(int argc,char**argv){
    if(argc<2){ fprintf(stderr,"usage: %s file.d81\n", argv[0]); return 1; }
    FloppyDevice d; memset(&d,0,sizeof(d)); d.log_callback=log_cb;
    if(floppy_open(&d, argv[1])!=0){ fprintf(stderr,"open failed\n"); return 1; }
    uint8_t buf[512];
    floppy_read_sector(&d,0,0,1,buf);
    floppy_analyze_protection(&d);
    floppy_close(&d);
    return 0;
}
