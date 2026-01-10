/**
 * @file config.h
 * @brief SAMdisk Configuration for UFT Integration
 * 
 * This config.h adapts SAMdisk for use within the UnifiedFloppyTool project.
 * Hardware-specific features are disabled by default.
 * 
 * Original SAMdisk: Copyright (c) Simon Owen
 * UFT Integration: 2025
 */

#ifndef SAMDISK_CONFIG_H
#define SAMDISK_CONFIG_H

/*============================================================================
 * Platform Detection
 *============================================================================*/

#if defined(_WIN32) || defined(_WIN64)
    #define SAMDISK_WINDOWS 1
    #define SAMDISK_LINUX   0
    #define SAMDISK_MACOS   0
#elif defined(__APPLE__)
    #define SAMDISK_WINDOWS 0
    #define SAMDISK_LINUX   0
    #define SAMDISK_MACOS   1
#elif defined(__linux__)
    #define SAMDISK_WINDOWS 0
    #define SAMDISK_LINUX   1
    #define SAMDISK_MACOS   0
#else
    #define SAMDISK_WINDOWS 0
    #define SAMDISK_LINUX   1  /* Default to Linux-like */
    #define SAMDISK_MACOS   0
#endif

/*============================================================================
 * Feature Flags - Hardware Support (disabled for UFT core)
 *============================================================================*/

/* Hardware drivers - disabled by default for library use */
#ifndef HAVE_FDRAWCMD
    #define HAVE_FDRAWCMD       0   /* Windows floppy driver */
#endif

#ifndef HAVE_KRYOFLUX
    #define HAVE_KRYOFLUX       0   /* KryoFlux hardware */
#endif

#ifndef HAVE_SUPERCARD
    #define HAVE_SUPERCARD      0   /* SuperCard Pro */
#endif

#ifndef HAVE_GREASEWEAZLE
    #define HAVE_GREASEWEAZLE   0   /* Greaseweazle */
#endif

#ifndef HAVE_FLUXENGINE
    #define HAVE_FLUXENGINE     0   /* FluxEngine */
#endif

#ifndef HAVE_CAPSLIB
    #define HAVE_CAPSLIB        0   /* SPS/CAPS library */
#endif

/*============================================================================
 * Feature Flags - Compression Libraries
 *============================================================================*/

#ifndef HAVE_ZLIB
    #define HAVE_ZLIB           0   /* zlib compression */
#endif

#ifndef HAVE_BZIP2
    #define HAVE_BZIP2          0   /* bzip2 compression */
#endif

#ifndef HAVE_LZMA
    #define HAVE_LZMA           0   /* LZMA/XZ compression */
#endif

/*============================================================================
 * Feature Flags - Core Features (enabled)
 *============================================================================*/

#define HAVE_BITSTREAM          1   /* Bitstream encoding/decoding */
#define HAVE_FLUX               1   /* Flux processing */
#define HAVE_FORMATS            1   /* Format handlers */

/*============================================================================
 * Version Information
 *============================================================================*/

#define SAMDISK_VERSION_MAJOR   4
#define SAMDISK_VERSION_MINOR   0
#define SAMDISK_VERSION_STRING  "4.0-uft"

/*============================================================================
 * Common Type Definitions
 *============================================================================*/

#include <cstdint>
#include <cstddef>

/* Byte type for raw data */
using Byte = uint8_t;

/* Data size type */
using DataSize = size_t;

/*============================================================================
 * Platform-Specific Adjustments
 *============================================================================*/

#if SAMDISK_WINDOWS
    /* Windows-specific defines handled in SAMdisk.h */
#else
    /* Unix-like systems */
    #ifndef MAX_PATH
        #define MAX_PATH 4096
    #endif
#endif

/*============================================================================
 * Debug/Release Configuration
 *============================================================================*/

#ifdef NDEBUG
    #define SAMDISK_DEBUG       0
#else
    #define SAMDISK_DEBUG       1
#endif

/*============================================================================
 * UFT Integration Macros
 *============================================================================*/

/* Mark functions for UFT export */
#ifdef UFT_BUILDING_SAMDISK
    #define SAMDISK_API         /* Building as part of UFT */
#else
    #define SAMDISK_API         /* Using SAMdisk from UFT */
#endif

/* Logging integration */
#ifndef SAMDISK_LOG
    #define SAMDISK_LOG(level, fmt, ...) \
        do { /* Integrate with UFT logging if needed */ } while(0)
#endif

#endif /* SAMDISK_CONFIG_H */
