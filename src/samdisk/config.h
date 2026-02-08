/**
 * @file config.h
 * @brief SAMdisk configuration header for UnifiedFloppyTool
 * 
 * Platform detection and feature flags
 */

#ifndef SAMDISK_CONFIG_H
#define SAMDISK_CONFIG_H

/* Version info */
#define PACKAGE_NAME "UnifiedFloppyTool"
#define PACKAGE_VERSION "3.7.3"

/* Platform detection */
#ifdef _WIN32
    #define HAVE_WINSOCK2_H 1
    #define HAVE_IO_H 1
    #define HAVE__STRCMPI 1
    #define HAVE__SNPRINTF 1
    #define HAVE__LSEEKI64 1
    #define HAVE_WINUSB 0  /* Optional, needs WinUSB SDK */
    #define HAVE_FDRAWCMD_H 0
#else
    /* Unix/Linux/macOS */
    #define HAVE_UNISTD_H 1
    #define HAVE_SYS_SOCKET_H 1
    #define HAVE_ARPA_INET_H 1
    #define HAVE_NETINET_IN_H 1
    #define HAVE_SYS_TIMEB_H 1
    #define HAVE_STRCASECMP 1
    #define HAVE_SNPRINTF 1
    #define HAVE_LSEEK64 1
    #define HAVE_LIBUSB1 0  /* Optional, needs libusb-1.0 */
#endif

/* Common */
#define HAVE_O_BINARY 1

/* Disable hardware drivers by default for GUI-only build */
#ifndef ENABLE_HARDWARE_DRIVERS
#define ENABLE_HARDWARE_DRIVERS 0
#endif

#endif /* SAMDISK_CONFIG_H */
