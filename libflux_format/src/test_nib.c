// test_nib.c
#include "uft/uft_error.h"
#include "nib.h"
#include <stdio.h>
#include <string.h>
static void log_cb(const char* m){ fprintf(stderr, "%s\n", m); }
int main(int argc,char**argv){
    if(argc<2){ fprintf(stderr,"usage: %s file.nib\n", argv[0]); return 1; }
    FloppyDevice d; memset(&d,0,sizeof(d)); d.log_callback=log_cb;
    if(uft_floppy_open(&d, argv[1])!=0){ fprintf(stderr,"open failed\n"); return 1; }
    uft_floppy_analyze_protection(&d);
    uft_floppy_close(&d);
    return 0;
}
