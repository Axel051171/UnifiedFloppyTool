// test_d71.c
#include "uft/uft_error.h"
#include "d71.h"
#include <stdio.h>
#include <string.h>

static void log_cb(const char* m){ fprintf(stderr, "%s\n", m); }

int main(int argc, char** argv){
    if(argc < 2){ fprintf(stderr, "usage: %s file.d71\n", argv[0]); return 1; }
    FloppyDevice d; memset(&d,0,sizeof(d)); d.log_callback = log_cb;

    if(uft_floppy_open(&d, argv[1]) != 0){ fprintf(stderr, "open failed\n"); return 1; }

    /* Read BAM sector (track 18, sector 0) on side 0 => t=17,h=0,s=1 */
    uint8_t bam[256];
    if(uft_floppy_read_sector(&d, 17, 0, 1, bam) == 0){
        fprintf(stdout, "BAM first 8 bytes: ");
        for(int i=0;i<8;i++) fprintf(stdout, "%02X ", bam[i]);
        fprintf(stdout, "\n");
    }

    uft_floppy_analyze_protection(&d);
    uft_floppy_close(&d);
    return 0;
}
