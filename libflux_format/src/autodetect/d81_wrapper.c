/**
 * @file d81_wrapper.c
 * @brief Autodetect wrapper for d81 format
 * @generated
 */

#include "../d81.h"
#include "../../include/uft/uft_autodetect.h"

/* Wrapper functions with namespaced names */

int d81_floppy_open(FloppyDevice* dev, const char* path) {
    /* Call format-specific open implementation */
    return uft_floppy_open(dev, path);
}

int d81_floppy_close(FloppyDevice* dev) {
    return uft_floppy_close(dev);
}

int d81_floppy_read_sector(FloppyDevice* dev, 
                                    uint32_t t, uint32_t h, 
                                    uint32_t s, uint8_t* buf) {
    return uft_floppy_read_sector(dev, t, h, s, buf);
}

int d81_floppy_write_sector(FloppyDevice* dev,
                                     uint32_t t, uint32_t h,
                                     uint32_t s, const uint8_t* buf) {
    return uft_floppy_write_sector(dev, t, h, s, buf);
}

int d81_floppy_analyze_protection(FloppyDevice* dev) {
    return uft_floppy_analyze_protection(dev);
}
