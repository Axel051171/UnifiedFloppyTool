/**
 * @file stx_wrapper.c
 * @brief Autodetect wrapper for stx format
 * @generated
 */

#include "../stx.h"
#include "../../include/uft/uft_autodetect.h"

/* Wrapper functions with namespaced names */

int stx_floppy_open(FloppyDevice* dev, const char* path) {
    /* Call format-specific open implementation */
    return uft_floppy_open(dev, path);
}

int stx_floppy_close(FloppyDevice* dev) {
    return uft_floppy_close(dev);
}

int stx_floppy_read_sector(FloppyDevice* dev, 
                                    uint32_t t, uint32_t h, 
                                    uint32_t s, uint8_t* buf) {
    return uft_floppy_read_sector(dev, t, h, s, buf);
}

int stx_floppy_write_sector(FloppyDevice* dev,
                                     uint32_t t, uint32_t h,
                                     uint32_t s, const uint8_t* buf) {
    return uft_floppy_write_sector(dev, t, h, s, buf);
}

int stx_floppy_analyze_protection(FloppyDevice* dev) {
    return uft_floppy_analyze_protection(dev);
}
