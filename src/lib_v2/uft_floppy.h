/**
 * @file uft_floppy.h
 * @brief UnifiedFloppyTool - Floppy Disk Library Main Header
 * 
 * Complete floppy disk access and FAT12 filesystem library.
 * 
 * Features:
 * - Cross-platform physical drive access (Linux, Windows, macOS)
 * - Disk image file support
 * - LBA/CHS address conversion
 * - Geometry detection and validation
 * - FAT12 filesystem read/write
 * 
 * Sources consolidated from:
 * - discdiag (Cross-platform disk I/O)
 * - lbacache (CHS/LBA conversion algorithms)
 * - BootLoader-Test (FAT12 implementation)
 * - Fosfat/libw32disk (Windows disk access)
 * 
 * @version 1.0.0
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UFT_FLOPPY_H
#define UFT_FLOPPY_H

/* Core types and definitions */
#include "uft_floppy_types.h"

/* Disk I/O abstraction layer */
#include "uft_floppy_io.h"

/* Geometry and address conversion */
#include "uft_floppy_geometry.h"

/* FAT12 filesystem */
#include "uft_fat12.h"

#endif /* UFT_FLOPPY_H */
