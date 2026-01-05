// test_prodos.c
#include "uft/uft_error.h"
#include "prodos_po_do.h"
#include <stdio.h>
#include <string.h>
static void log_cb(const char* m){ fprintf(stderr, "%s\n", m); }
int main(int argc,char**argv){
    if(argc<2){ fprintf(stderr,"usage: %s file.po|file.do\n", argv[0]); return 1; }
    FloppyDevice d; memset(&d,0,sizeof(d)); d.log_callback=log_cb;
    if(floppy_open(&d, argv[1])!=0){ fprintf(stderr,"open failed\n"); return 1; }
    uint8_t buf[256];
    floppy_read_sector(&d,0,0,1,buf);
    floppy_analyze_protection(&d);
    floppy_close(&d);
    return 0;
}
