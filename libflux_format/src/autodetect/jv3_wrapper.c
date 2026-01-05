/**
 * @file jv3_wrapper.c
 * @brief Autodetect wrapper for jv3 format
 * @generated
 */

#include "../jv3.h"
#include "../../include/uft/uft_autodetect.h"

/* Wrapper functions with namespaced names */

int jv3_floppy_open(FloppyDevice* dev, const char* path) {
    /* Call format-specific open implementation */
    return floppy_open(dev, path);
}

int jv3_floppy_close(FloppyDevice* dev) {
    return floppy_close(dev);
}

int jv3_floppy_read_sector(FloppyDevice* dev, 
                                    uint32_t t, uint32_t h, 
                                    uint32_t s, uint8_t* buf) {
    return floppy_read_sector(dev, t, h, s, buf);
}

int jv3_floppy_write_sector(FloppyDevice* dev,
                                     uint32_t t, uint32_t h,
                                     uint32_t s, const uint8_t* buf) {
    return floppy_write_sector(dev, t, h, s, buf);
}

int jv3_floppy_analyze_protection(FloppyDevice* dev) {
    return floppy_analyze_protection(dev);
}
