// test_hdm.c - minimal test for HDM module
// Build: cc -std=c11 -O2 -Wall -Wextra hdm.c test_hdm.c -o test_hdm
//
// Usage:
//   ./test_hdm in.hdm
//   ./test_hdm --create out.hdm

#include "uft/uft_error.h"
#include "hdm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void log_cb(const char* msg){ fprintf(stderr, "%s\n", msg); }
static void die(const char* msg){ fprintf(stderr, "ERR: %s\n", msg); exit(1); }

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Usage:\n  %s in.hdm\n  %s --create out.hdm\n", argv[0], argv[0]);
        return 2;
    }

    if(strcmp(argv[1], "--create") == 0){
        if(argc != 3) die("bad args");
        if(hdm_create_new(argv[2]) != 0) die("create failed");
        fprintf(stderr, "OK: created %s\n", argv[2]);
        return 0;
    }

    FloppyDevice dev; memset(&dev, 0, sizeof(dev)); dev.log_callback = log_cb;
    if(floppy_open(&dev, argv[1]) != 0) die("open failed");

    uint8_t *buf = (uint8_t*)malloc(dev.sectorSize);
    if(!buf) die("oom");
    if(floppy_read_sector(&dev, 0, 0, 1, buf) != 0) die("read failed");
    fprintf(stdout, "First 16 bytes of sector 0/0/1:\n");
    for(int i=0;i<16;i++) fprintf(stdout, "%02X ", buf[i]);
    fprintf(stdout, "\n");
    free(buf);

    floppy_analyze_protection(&dev);
    floppy_close(&dev);
    return 0;
}
