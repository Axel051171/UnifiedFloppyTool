/**
 * @file nib_wrapper.c
 * @brief Autodetect wrapper for nib format
 * @generated
 */

#include "../nib.h"
#include "../../include/uft/uft_autodetect.h"

/* Wrapper functions with namespaced names */

int nib_floppy_open(FloppyDevice* dev, const char* path) {
    /* Call format-specific open implementation */
    return uft_floppy_open(dev, path);
}

int nib_floppy_close(FloppyDevice* dev) {
    return uft_floppy_close(dev);
}

int nib_floppy_read_sector(FloppyDevice* dev, 
                                    uint32_t t, uint32_t h, 
                                    uint32_t s, uint8_t* buf) {
    return uft_floppy_read_sector(dev, t, h, s, buf);
}

int nib_floppy_write_sector(FloppyDevice* dev,
                                     uint32_t t, uint32_t h,
                                     uint32_t s, const uint8_t* buf) {
    return uft_floppy_write_sector(dev, t, h, s, buf);
}

int nib_floppy_analyze_protection(FloppyDevice* dev) {
    return uft_floppy_analyze_protection(dev);
}
