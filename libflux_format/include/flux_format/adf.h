// SPDX-License-Identifier: MIT
/*
 * adf.h - Amiga Disk File (ADF) Format Plugin
 * 
 * Standard Amiga floppy disk image format (880KB)
 * Based on flux_preservation_architect implementation
 */
#pragma once

#include "../../libflux_core/include/flux_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADF format probe/read/write */
int adf_probe(const char *filename);
int adf_read(const char *filename, ufm_disk_t *disk);
int adf_write(const char *filename, const ufm_disk_t *disk);

#ifdef __cplusplus
}
#endif
