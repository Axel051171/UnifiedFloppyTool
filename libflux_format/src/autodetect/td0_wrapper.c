/**
 * @file td0_wrapper.c
 * @brief Autodetect wrapper for td0 format
 * @generated
 */

#include "../td0.h"
#include "../../include/uft/uft_autodetect.h"

/* Wrapper functions with namespaced names */

int td0_floppy_open(FloppyDevice* dev, const char* path) {
    /* Call format-specific open implementation */
    return floppy_open(dev, path);
}

int td0_floppy_close(FloppyDevice* dev) {
    return floppy_close(dev);
}

int td0_floppy_read_sector(FloppyDevice* dev, 
                                    uint32_t t, uint32_t h, 
                                    uint32_t s, uint8_t* buf) {
    return floppy_read_sector(dev, t, h, s, buf);
}

int td0_floppy_write_sector(FloppyDevice* dev,
                                     uint32_t t, uint32_t h,
                                     uint32_t s, const uint8_t* buf) {
    return floppy_write_sector(dev, t, h, s, buf);
}

int td0_floppy_analyze_protection(FloppyDevice* dev) {
    return floppy_analyze_protection(dev);
}
