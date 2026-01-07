/**
 * @file msa_wrapper.c
 * @brief Autodetect wrapper for msa format
 * @generated
 */

#include "../msa.h"
#include "../../include/uft/uft_autodetect.h"

/* Wrapper functions with namespaced names */

int msa_floppy_open(FloppyDevice* dev, const char* path) {
    /* Call format-specific open implementation */
    return uft_floppy_open(dev, path);
}

int msa_floppy_close(FloppyDevice* dev) {
    return uft_floppy_close(dev);
}

int msa_floppy_read_sector(FloppyDevice* dev, 
                                    uint32_t t, uint32_t h, 
                                    uint32_t s, uint8_t* buf) {
    return uft_floppy_read_sector(dev, t, h, s, buf);
}

int msa_floppy_write_sector(FloppyDevice* dev,
                                     uint32_t t, uint32_t h,
                                     uint32_t s, const uint8_t* buf) {
    return uft_floppy_write_sector(dev, t, h, s, buf);
}

int msa_floppy_analyze_protection(FloppyDevice* dev) {
    return uft_floppy_analyze_protection(dev);
}
