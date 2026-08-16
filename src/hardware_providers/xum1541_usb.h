/**
 * @file xum1541_usb.h
 * @brief XUM1541/ZoomFloppy USB protocol definitions for UnifiedFloppyTool
 *
 * Contains USB identifiers, XUM1541 firmware command codes, IEC bus protocol
 * constants, and OpenCBM dynamic-loading helpers.  The constants here mirror
 * the values found in the OpenCBM project (opencbm.h, xum1541.h, archlib.h)
 * and are used by Xum1541HardwareProvider for both the OpenCBM library path
 * and the direct USB fallback path.
 *
 * References:
 *   https://github.com/OpenCBM/OpenCBM
 *   https://github.com/OpenCBM/OpenCBM/blob/master/xum1541/xum1541.h
 *   https://github.com/OpenCBM/OpenCBM/blob/master/include/opencbm.h
 */

#ifndef XUM1541_USB_H
#define XUM1541_USB_H

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

/* =========================================================================
 * USB Vendor / Product IDs
 * ========================================================================= */

namespace xum1541 {

/** ZoomFloppy (RETRO Innovations) USB VID/PID */
constexpr uint16_t USB_VID_ZOOMFLOPPY       = 0x16D0;
constexpr uint16_t USB_PID_ZOOMFLOPPY       = 0x04B2;

/** XUM1541 (community / homebrew) USB VID/PID */
constexpr uint16_t USB_VID_XUM1541          = 0x16D0;
constexpr uint16_t USB_PID_XUM1541          = 0x0504;

/** Alternate community PID sometimes used by DIY XUM1541 boards */
constexpr uint16_t USB_PID_XUM1541_ALT      = 0x0503;

/* =========================================================================
 * USB Endpoint & Transfer Constants
 * ========================================================================= */

/** Default USB configuration for XUM1541 */
constexpr uint8_t  USB_CONFIG               = 1;
constexpr uint8_t  USB_INTERFACE            = 0;

/** Bulk endpoints (from xum1541 firmware descriptor) */
constexpr uint8_t  EP_BULK_IN              = 0x81;  /* EP 1 IN  */
constexpr uint8_t  EP_BULK_OUT             = 0x02;  /* EP 2 OUT */

/** Control-transfer request type (vendor, device) */
constexpr uint8_t  USB_REQ_TYPE_VENDOR_DEV = 0x40;
constexpr uint8_t  USB_REQ_TYPE_VENDOR_IN  = 0xC0;

/** Timeout for USB transfers (ms) */
constexpr int      USB_TIMEOUT_MS          = 30000;
constexpr int      USB_TIMEOUT_SHORT_MS    = 5000;
constexpr int      USB_TIMEOUT_RESET_MS    = 2000;

/* =========================================================================
 * XUM1541 Firmware Commands  (control-transfer bRequest values)
 *
 * These match the enum in OpenCBM's xum1541.h.  The firmware accepts them
 * as USB vendor control-transfer bRequest codes.
 * ========================================================================= */

enum Xum1541Cmd : uint8_t {
    XUM1541_ECHO            = 0,   /**< Echo test                         */
    XUM1541_READ            = 1,   /**< Bulk-IN: read IEC data            */
    XUM1541_WRITE           = 2,   /**< Bulk-OUT: write IEC data          */
    XUM1541_TALK            = 3,   /**< IEC TALK  + secondary address     */
    XUM1541_LISTEN          = 4,   /**< IEC LISTEN + secondary address    */
    XUM1541_UNTALK          = 5,   /**< IEC UNTALK                        */
    XUM1541_UNLISTEN        = 6,   /**< IEC UNLISTEN                      */
    XUM1541_OPEN            = 7,   /**< IEC OPEN (LISTEN + OPEN SA)       */
    XUM1541_CLOSE           = 8,   /**< IEC CLOSE (LISTEN + CLOSE SA)     */
    XUM1541_RESET           = 9,   /**< Reset IEC bus                     */
    XUM1541_GET_EOI         = 10,  /**< Query EOI flag after last read    */
    XUM1541_CLEAR_EOI       = 11,  /**< Clear pending EOI flag            */
    XUM1541_PP_READ         = 12,  /**< Parallel-port read (directly)     */
    XUM1541_PP_WRITE        = 13,  /**< Parallel-port write (directly)    */
    XUM1541_IEC_POLL        = 14,  /**< Poll IEC bus lines                */
    XUM1541_IEC_WAIT        = 15,  /**< Wait for IEC bus state change     */
    XUM1541_IEC_SETRELEASE  = 16,  /**< Set/release IEC bus lines         */
    XUM1541_PARBURST_READ   = 17,  /**< Parallel burst read (1571/1581)   */
    XUM1541_PARBURST_WRITE  = 18,  /**< Parallel burst write (1571/1581)  */
    XUM1541_SRQBURST_READ   = 19,  /**< SRQ burst read (1581)             */
    XUM1541_SRQBURST_WRITE  = 20,  /**< SRQ burst write (1581)            */
    XUM1541_TAP_MOTOR_ON    = 21,  /**< Tape motor on                     */
    XUM1541_TAP_MOTOR_OFF   = 22,  /**< Tape motor off                    */
    XUM1541_TAP_PREPARE_CAPTURE = 23,
    XUM1541_TAP_PREPARE_WRITE   = 24,
    XUM1541_TAP_GET_SENSE       = 25,
    XUM1541_TAP_WAIT_FOR_STOP_SENSE = 26,
    XUM1541_TAP_WAIT_FOR_PLAY_SENSE = 27,
    XUM1541_NIBTOOLS_READ       = 28,  /**< Nibtools-style raw GCR read   */
    XUM1541_NIBTOOLS_WRITE      = 29,  /**< Nibtools-style raw GCR write  */
    XUM1541_INIT                = 30,  /**< Initialize adapter             */
    XUM1541_SHUTDOWN            = 31,  /**< Shut down adapter cleanly      */

    /** Internal sentinel */
    XUM1541_CMD_MAX             = 32
};

/* =========================================================================
 * XUM1541 Capabilities / Info  (GET_RESULT sub-commands)
 * ========================================================================= */

enum Xum1541Info : uint8_t {
    XUM1541_INFO_CAPS       = 0,   /**< Firmware capabilities bitmask     */
    XUM1541_INFO_FW_VERSION = 1,   /**< Firmware version (major.minor)    */
    XUM1541_INFO_PROTO_VERSION = 2 /**< Protocol version                  */
};

/** Capability bits returned by XUM1541_INFO_CAPS */
enum Xum1541Caps : uint16_t {
    XUM1541_CAP_CBM         = (1 << 0),  /**< Standard CBM IEC protocol   */
    XUM1541_CAP_PARBURST    = (1 << 1),  /**< 1571/1581 parallel burst    */
    XUM1541_CAP_SRQBURST    = (1 << 2),  /**< 1581 SRQ burst              */
    XUM1541_CAP_NIB         = (1 << 3),  /**< Nibtools-style raw GCR      */
    XUM1541_CAP_TAPE        = (1 << 4),  /**< Datasette tape support      */
    XUM1541_CAP_PP          = (1 << 5),  /**< Parallel port access        */
    XUM1541_CAP_S1          = (1 << 6),  /**< Speed1 protocol (fast IEC)  */
    XUM1541_CAP_S2          = (1 << 7),  /**< Speed2 protocol             */
};

/* =========================================================================
 * IEC Bus Protocol Constants
 *
 * These are the raw IEEE-488/IEC bus command bytes.  The device number
 * (0-30) is OR'd into the LISTEN/TALK commands.
 * ========================================================================= */

/** IEC bus ATN commands (primary address) */
constexpr uint8_t IEC_LISTEN              = 0x20;  /**< LISTEN  + device (0-30) */
constexpr uint8_t IEC_UNLISTEN            = 0x3F;  /**< UNLISTEN (global)       */
constexpr uint8_t IEC_TALK                = 0x40;  /**< TALK    + device (0-30) */
constexpr uint8_t IEC_UNTALK              = 0x5F;  /**< UNTALK (global)         */

/** IEC bus secondary address commands */
constexpr uint8_t IEC_OPEN                = 0xF0;  /**< OPEN   + channel (0-15) */
constexpr uint8_t IEC_CLOSE               = 0xE0;  /**< CLOSE  + channel (0-15) */
constexpr uint8_t IEC_DATA                = 0x60;  /**< DATA   + channel (0-15) */

/** IEC bus line bits (for IEC_POLL / IEC_SETRELEASE) */
constexpr uint8_t IEC_LINE_DATA           = 0x01;
constexpr uint8_t IEC_LINE_CLOCK          = 0x02;
constexpr uint8_t IEC_LINE_ATN            = 0x04;
constexpr uint8_t IEC_LINE_RESET          = 0x08;
constexpr uint8_t IEC_LINE_SRQ            = 0x10;

/** Default CBM device numbers */
constexpr uint8_t CBM_DEVICE_DEFAULT      = 8;     /**< Standard floppy #8      */
constexpr uint8_t CBM_DEVICE_MIN          = 4;
constexpr uint8_t CBM_DEVICE_MAX          = 30;

/** CBM channel numbers */
constexpr uint8_t CBM_CHANNEL_LOAD        = 0;     /**< LOAD channel            */
constexpr uint8_t CBM_CHANNEL_SAVE        = 1;     /**< SAVE channel            */
constexpr uint8_t CBM_CHANNEL_DATA        = 2;     /**< Direct-access buffer    */
constexpr uint8_t CBM_CHANNEL_CMD         = 15;    /**< Command / error channel */

/* =========================================================================
 * CBM DOS Status Codes  (error channel responses)
 * ========================================================================= */

enum CbmDosStatus : uint8_t {
    CBM_STATUS_OK               = 0,   /**< 00, OK, 00, 00            */
    CBM_STATUS_FILES_SCRATCHED  = 1,   /**< 01, FILES SCRATCHED       */
    CBM_STATUS_READ_ERROR_20    = 20,  /**< Header block not found    */
    CBM_STATUS_READ_ERROR_21    = 21,  /**< Sync character not found  */
    CBM_STATUS_READ_ERROR_22    = 22,  /**< Data block not present    */
    CBM_STATUS_READ_ERROR_23    = 23,  /**< Checksum error in data    */
    CBM_STATUS_READ_ERROR_24    = 24,  /**< Byte decoding error       */
    CBM_STATUS_READ_ERROR_25    = 25,  /**< Write-verify error        */
    CBM_STATUS_READ_ERROR_27    = 27,  /**< Read error (generic)      */
    CBM_STATUS_WRITE_PROTECT    = 26,  /**< Write protect on          */
    CBM_STATUS_DISK_ID_MISMATCH = 29,  /**< Disk ID mismatch          */
    CBM_STATUS_SYNTAX_ERROR     = 30,  /**< Syntax error              */
    CBM_STATUS_UNKNOWN_CMD      = 31,  /**< Unknown command           */
    CBM_STATUS_RECORD_NOT_PRESENT = 50,
    CBM_STATUS_OVERFLOW         = 51,
    CBM_STATUS_DISK_FULL        = 72,  /**< Disk full                 */
    CBM_STATUS_FILE_NOT_FOUND   = 62,  /**< File not found            */
    CBM_STATUS_FILE_EXISTS      = 63,  /**< File exists               */
    CBM_STATUS_DRIVE_NOT_READY  = 74,  /**< Drive not ready           */
};

/* =========================================================================
 * CBM File Type Codes
 * ========================================================================= */

enum CbmFileType : uint8_t {
    CBM_FTYPE_DEL   = 0x00,
    CBM_FTYPE_SEQ   = 0x01,
    CBM_FTYPE_PRG   = 0x02,
    CBM_FTYPE_USR   = 0x03,
    CBM_FTYPE_REL   = 0x04,
    CBM_FTYPE_CBM   = 0x05,  /**< CBM partition (D81) */
};

/* =========================================================================
 * Drive Geometry Constants
 * ========================================================================= */

/** 1541: 35 tracks, variable sectors per zone */
constexpr int CBM_1541_TRACKS              = 35;
constexpr int CBM_1541_TRACKS_EXT          = 40;  /**< Extended (tracks 36-40) */
constexpr int CBM_1541_SECTOR_SIZE         = 256;
constexpr int CBM_1541_DISK_SIZE           = 174848;   /**< 35-track D64 */
constexpr int CBM_1541_DISK_SIZE_EXT       = 196608;   /**< 40-track D64 */

/** 1571: double-sided 1541 */
constexpr int CBM_1571_TRACKS              = 35;  /**< Per side */
constexpr int CBM_1571_DISK_SIZE           = 349696;   /**< D71 */

/** 1581: 80 tracks, 40 sectors/track, 256 bytes/sector, MFM */
constexpr int CBM_1581_TRACKS              = 80;
constexpr int CBM_1581_SECTORS_PER_TRACK   = 40;
constexpr int CBM_1581_SECTOR_SIZE         = 256;
constexpr int CBM_1581_DISK_SIZE           = 819200;   /**< D81 */

/* =========================================================================
 * GCR Speed Zones (1541/1571)
 *
 * Zone 0: tracks  1-17  -> 21 sectors, 3.25 us bit cell
 * Zone 1: tracks 18-24  -> 19 sectors, 3.50 us bit cell
 * Zone 2: tracks 25-30  -> 18 sectors, 3.75 us bit cell
 * Zone 3: tracks 31-35+ -> 17 sectors, 4.00 us bit cell
 * ========================================================================= */

constexpr int GCR_ZONE_SECTORS[4] = { 21, 19, 18, 17 };

/** Maximum raw GCR track size in bytes (zone 0 = longest) */
constexpr int GCR_MAX_TRACK_SIZE   = 7928;

/** Nibtools-style raw track read size */
constexpr int NIB_TRACK_SIZE       = 8192;

/* =========================================================================
 * OpenCBM Dynamic Library Wrapper
 *
 * Provides function pointers for the OpenCBM shared library so we can
 * load it at runtime rather than requiring a compile-time dependency.
 * ========================================================================= */

/** OpenCBM handle type (intptr_t on all platforms, matching OpenCBM's CBM_FILE) */
#ifdef _WIN32
typedef void* CbmFileHandle;
#else
typedef int CbmFileHandle;
#endif

/** OpenCBM function pointer types */
typedef int          (*pfn_cbm_driver_open)(CbmFileHandle *handle, int port);
typedef void         (*pfn_cbm_driver_close)(CbmFileHandle handle);
typedef const char*  (*pfn_cbm_get_driver_name)(int port);

typedef int  (*pfn_cbm_listen)(CbmFileHandle handle, uint8_t device, uint8_t secondary);
typedef int  (*pfn_cbm_talk)(CbmFileHandle handle, uint8_t device, uint8_t secondary);
typedef int  (*pfn_cbm_unlisten)(CbmFileHandle handle);
typedef int  (*pfn_cbm_untalk)(CbmFileHandle handle);
typedef int  (*pfn_cbm_open)(CbmFileHandle handle, uint8_t device, uint8_t secondary,
                              const void *filename, size_t filename_len);
typedef int  (*pfn_cbm_close)(CbmFileHandle handle, uint8_t device, uint8_t secondary);

typedef int  (*pfn_cbm_raw_read)(CbmFileHandle handle, void *buf, size_t count);
typedef int  (*pfn_cbm_raw_write)(CbmFileHandle handle, const void *buf, size_t count);

typedef int  (*pfn_cbm_reset)(CbmFileHandle handle);
typedef int  (*pfn_cbm_device_status)(CbmFileHandle handle, uint8_t device,
                                       void *buf, size_t bufsize);
typedef int  (*pfn_cbm_exec_command)(CbmFileHandle handle, uint8_t device,
                                      const void *cmd, size_t cmdlen);
typedef int  (*pfn_cbm_identify)(CbmFileHandle handle, uint8_t device,
                                  void *buf, size_t bufsize);

typedef uint8_t (*pfn_cbm_iec_poll)(CbmFileHandle handle);
typedef int     (*pfn_cbm_iec_set)(CbmFileHandle handle, int line);
typedef int     (*pfn_cbm_iec_release)(CbmFileHandle handle, int line);
typedef int     (*pfn_cbm_iec_setrelease)(CbmFileHandle handle, int set, int release);
typedef int     (*pfn_cbm_iec_wait)(CbmFileHandle handle, int line, int state);

/** Parallel burst (1571 / 1581 fast protocols) */
typedef int  (*pfn_cbm_parallel_burst_read)(CbmFileHandle handle, uint8_t *data);
typedef int  (*pfn_cbm_parallel_burst_write)(CbmFileHandle handle, uint8_t data);

/**
 * @brief Holds all dynamically loaded OpenCBM function pointers.
 *
 * Populated by OpenCbmLibrary::load().  If a function pointer is nullptr
 * the corresponding feature is unavailable (non-fatal for optional funcs).
 */
struct OpenCbmFunctions {
    /* Library handle */
    void *lib_handle             = nullptr;
    bool  loaded                 = false;
    bool  available              = false;

    /* Core driver */
    pfn_cbm_driver_open      driver_open      = nullptr;
    pfn_cbm_driver_close     driver_close     = nullptr;
    pfn_cbm_get_driver_name  get_driver_name  = nullptr;

    /* IEC bus primitives */
    pfn_cbm_listen           listen           = nullptr;
    pfn_cbm_talk             talk             = nullptr;
    pfn_cbm_unlisten         unlisten         = nullptr;
    pfn_cbm_untalk           untalk           = nullptr;
    pfn_cbm_open             open             = nullptr;
    pfn_cbm_close            close            = nullptr;

    /* Data transfer */
    pfn_cbm_raw_read         raw_read         = nullptr;
    pfn_cbm_raw_write        raw_write        = nullptr;

    /* Device management */
    pfn_cbm_reset            reset            = nullptr;
    pfn_cbm_device_status    device_status    = nullptr;
    pfn_cbm_exec_command     exec_command     = nullptr;
    pfn_cbm_identify         identify         = nullptr;

    /* Low-level IEC (optional) */
    pfn_cbm_iec_poll         iec_poll         = nullptr;
    pfn_cbm_iec_set          iec_set          = nullptr;
    pfn_cbm_iec_release      iec_release      = nullptr;
    pfn_cbm_iec_setrelease   iec_setrelease   = nullptr;
    pfn_cbm_iec_wait         iec_wait         = nullptr;

    /* Parallel burst (optional - 1571/1581 fast) */
    pfn_cbm_parallel_burst_read   parallel_burst_read   = nullptr;
    pfn_cbm_parallel_burst_write  parallel_burst_write  = nullptr;
};

/**
 * @brief RAII-style loader for the OpenCBM shared library.
 */
class OpenCbmLibrary {
public:
    /**
     * @brief Attempt to load the OpenCBM library.
     * @return true if the minimum required set of symbols were resolved.
     */
    static bool load(OpenCbmFunctions &fn) {
        if (fn.loaded) return fn.available;
        fn.loaded = true;

#ifdef _WIN32
        fn.lib_handle = LoadLibraryA("opencbm.dll");
        if (!fn.lib_handle)
            fn.lib_handle = LoadLibraryA("C:\\Program Files\\opencbm\\opencbm.dll");
        if (!fn.lib_handle)
            fn.lib_handle = LoadLibraryA("C:\\Program Files (x86)\\opencbm\\opencbm.dll");
#define XUM_LOADSYM(dest, name) \
    fn.dest = reinterpret_cast<decltype(fn.dest)>( \
        reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(fn.lib_handle), name)))
#else
        fn.lib_handle = dlopen("libopencbm.so", RTLD_NOW);
        if (!fn.lib_handle) fn.lib_handle = dlopen("libopencbm.so.0", RTLD_NOW);
        if (!fn.lib_handle) fn.lib_handle = dlopen("/usr/local/lib/libopencbm.so", RTLD_NOW);
        if (!fn.lib_handle) fn.lib_handle = dlopen("/usr/lib/libopencbm.so", RTLD_NOW);
#define XUM_LOADSYM(dest, name) \
    fn.dest = reinterpret_cast<decltype(fn.dest)>(dlsym(fn.lib_handle, name))
#endif

        if (!fn.lib_handle) return false;

        /* Core (required) */
        XUM_LOADSYM(driver_open,      "cbm_driver_open");
        XUM_LOADSYM(driver_close,     "cbm_driver_close");
        XUM_LOADSYM(get_driver_name,  "cbm_get_driver_name");

        /* IEC bus (required) */
        XUM_LOADSYM(listen,           "cbm_listen");
        XUM_LOADSYM(talk,             "cbm_talk");
        XUM_LOADSYM(unlisten,         "cbm_unlisten");
        XUM_LOADSYM(untalk,           "cbm_untalk");
        XUM_LOADSYM(open,             "cbm_open");
        XUM_LOADSYM(close,            "cbm_close");

        /* Data (required) */
        XUM_LOADSYM(raw_read,         "cbm_raw_read");
        XUM_LOADSYM(raw_write,        "cbm_raw_write");

        /* Device (optional) */
        XUM_LOADSYM(reset,            "cbm_reset");
        XUM_LOADSYM(device_status,    "cbm_device_status");
        XUM_LOADSYM(exec_command,     "cbm_exec_command");
        XUM_LOADSYM(identify,         "cbm_identify");

        /* Low-level IEC (optional) */
        XUM_LOADSYM(iec_poll,         "cbm_iec_poll");
        XUM_LOADSYM(iec_set,          "cbm_iec_set");
        XUM_LOADSYM(iec_release,      "cbm_iec_release");
        XUM_LOADSYM(iec_setrelease,   "cbm_iec_setrelease");
        XUM_LOADSYM(iec_wait,         "cbm_iec_wait");

        /* Parallel burst (optional) */
        XUM_LOADSYM(parallel_burst_read,  "cbm_parallel_burst_read");
        XUM_LOADSYM(parallel_burst_write, "cbm_parallel_burst_write");

#undef XUM_LOADSYM

        /* Minimum viable set: driver lifecycle + basic IEC */
        fn.available = (fn.driver_open && fn.driver_close &&
                        fn.listen && fn.talk &&
                        fn.unlisten && fn.untalk &&
                        fn.raw_read && fn.raw_write);
        return fn.available;
    }

    /**
     * @brief Unload the OpenCBM library (call once at shutdown).
     */
    static void unload(OpenCbmFunctions &fn) {
        if (!fn.lib_handle) return;
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(fn.lib_handle));
#else
        dlclose(fn.lib_handle);
#endif
        fn.lib_handle = nullptr;
        fn.available  = false;
        fn.loaded     = false;
    }
};

/* =========================================================================
 * DeviceHandle  (USB-level, for direct fallback without OpenCBM)
 * ========================================================================= */

/**
 * @brief Low-level device handle for direct USB communication with XUM1541.
 *
 * Used only when OpenCBM is not available.  Requires libusb or platform USB
 * APIs.  Most users should prefer the OpenCBM path.
 */
struct DeviceHandle {
    void    *usb_handle   = nullptr;   /**< libusb_device_handle* or HANDLE */
    bool     connected    = false;
    uint8_t  fw_major     = 0;
    uint8_t  fw_minor     = 0;
    uint16_t capabilities = 0;         /**< Xum1541Caps bitmask */
    uint16_t vid          = 0;
    uint16_t pid          = 0;
};

/* =========================================================================
 * Inline Helpers
 * ========================================================================= */

/**
 * @brief Return the number of sectors per track for a given 1541/1571 track.
 *        Returns 0 for out-of-range tracks.
 */
inline int sectorsForTrack1541(int track) {
    if (track >= 1  && track <= 17) return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 40) return 17;
    return 0;
}

/**
 * @brief Return the GCR speed zone (0-3) for a 1541/1571 track number.
 */
inline int gcrSpeedZone(int track) {
    if (track >= 1  && track <= 17) return 3;  /* Fastest: 21 sectors */
    if (track >= 18 && track <= 24) return 2;
    if (track >= 25 && track <= 30) return 1;
    return 0;  /* tracks 31+ = slowest */
}

/**
 * @brief Check if a VID/PID matches a known XUM1541 / ZoomFloppy device.
 */
inline bool isKnownXum1541(uint16_t vid, uint16_t pid) {
    if (vid == USB_VID_ZOOMFLOPPY && pid == USB_PID_ZOOMFLOPPY) return true;
    if (vid == USB_VID_XUM1541   && pid == USB_PID_XUM1541)     return true;
    if (vid == USB_VID_XUM1541   && pid == USB_PID_XUM1541_ALT) return true;
    return false;
}

} // namespace xum1541

#endif // XUM1541_USB_H
