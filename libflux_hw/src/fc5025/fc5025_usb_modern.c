// SPDX-License-Identifier: MIT
/*
 * uft_fc5025_usb_modern.c - FC5025 USB Wrapper for libusb-1.0 and WinUSB
 * 
 * Wraps the official FC5025 code to work with modern USB libraries.
 * Original code used libusb-0.1, this adapter provides libusb-1.0 and
 * native Windows WinUSB support.
 * 
 * Based on official Device Side Data FC5025 driver v1309
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*============================================================================*
 * PLATFORM DETECTION
 *============================================================================*/

#if defined(_WIN32) || defined(_WIN64)
    #define UFT_FC5025_PLATFORM_WINDOWS
    #include <windows.h>
    #include <setupapi.h>
    #include <winusb.h>
    #pragma comment(lib, "setupapi.lib")
    #pragma comment(lib, "winusb.lib")
    #define usleep(x) Sleep((x)/1000)
#else
    #define UFT_FC5025_PLATFORM_UNIX
    #include <libusb-1.0/libusb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

/*============================================================================*
 * FC5025 CONSTANTS (from official fc5025.h)
 *============================================================================*/

#define UFT_FC5025_VID  0x16c0
#define UFT_FC5025_PID  0x06d6

/* Opcodes */
#define OPCODE_SEEK          0xc0
#define OPCODE_SELF_TEST     0xc1
#define OPCODE_FLAGS         0xc2
#define OPCODE_DRIVE_STATUS  0xc3
#define OPCODE_INDEXES       0xc4
#define OPCODE_READ_FLEXIBLE 0xc6
#define OPCODE_READ_ID       0xc7

/* Formats */
#define FORMAT_APPLE_GCR     1
#define FORMAT_COMMODORE_GCR 2
#define FORMAT_FM            3
#define FORMAT_MFM           4

/* Read flags */
#define READ_FLAG_SIDE       1
#define READ_FLAG_ID_FIELD   2
#define READ_FLAG_ORUN_RECOV 4
#define READ_FLAG_NO_AUTOSYNC 8
#define READ_FLAG_ANGULAR    16
#define READ_FLAG_NO_ADAPTIVE 32

/*============================================================================*
 * BYTE SWAP MACROS
 *============================================================================*/

#define swap32(x) (((((uint32_t)x) & 0xff000000) >> 24) | \
                  ((((uint32_t)x) & 0x00ff0000) >>  8) | \
                  ((((uint32_t)x) & 0x0000ff00) <<  8) | \
                  ((((uint32_t)x) & 0x000000ff) << 24))

#ifdef UFT_FC5025_PLATFORM_WINDOWS
static inline uint32_t htonl_compat(uint32_t x) {
    return ((x & 0xff) << 24) | ((x & 0xff00) << 8) | 
           ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24);
}
#define htov32(x) swap32(htonl_compat(x))
#else
#define htov32(x) swap32(htonl(x))
#endif

/*============================================================================*
 * USB HANDLE
 *============================================================================*/

typedef struct uft_fc5025_device {
#ifdef UFT_FC5025_PLATFORM_WINDOWS
    HANDLE device_handle;
    WINUSB_INTERFACE_HANDLE winusb_handle;
#else
    libusb_context *ctx;
    libusb_device_handle *handle;
#endif
    uint32_t tag;
    int is_open;
} uft_fc5025_device_t;

static uft_fc5025_device_t g_fc5025 = {0};

/*============================================================================*
 * COMMAND BLOCK WRAPPER (CBW)
 *============================================================================*/

#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];  /* "CFBC" */
    uint32_t tag;
    uint32_t xferlen;
    uint8_t flags;
    uint8_t padding1;
    uint8_t padding2;
    uint8_t cdb[48];
} uft_fc5025_cbw_t;

typedef struct {
    uint32_t signature;  /* 0x46435342 = "FCSB" */
    uint32_t tag;
    uint8_t status;
    uint8_t sense;
    uint8_t asc;
    uint8_t ascq;
    uint8_t padding[20];
} uft_fc5025_csw_t;
#pragma pack(pop)

/*============================================================================*
 * USB I/O FUNCTIONS
 *============================================================================*/

#ifdef UFT_FC5025_PLATFORM_UNIX

static int usb_bulk_write_wrapper(uint8_t endpoint, void *data, int length, int timeout) {
    int transferred;
    int rc = libusb_bulk_transfer(g_fc5025.handle, endpoint, data, length, &transferred, timeout);
    if (rc < 0) return -1;
    return transferred;
}

static int usb_bulk_read_wrapper(uint8_t endpoint, void *data, int length, int timeout) {
    int transferred;
    int rc = libusb_bulk_transfer(g_fc5025.handle, endpoint, data, length, &transferred, timeout);
    if (rc < 0) return -1;
    return transferred;
}

#else /* WINDOWS */

static int usb_bulk_write_wrapper(uint8_t endpoint, void *data, int length, int timeout) {
    ULONG transferred;
    ULONG to = timeout;
    WinUsb_SetPipePolicy(g_fc5025.winusb_handle, endpoint, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &to);
    if (!WinUsb_WritePipe(g_fc5025.winusb_handle, endpoint, data, length, &transferred, NULL)) {
        return -1;
    }
    return (int)transferred;
}

static int usb_bulk_read_wrapper(uint8_t endpoint, void *data, int length, int timeout) {
    ULONG transferred;
    ULONG to = timeout;
    WinUsb_SetPipePolicy(g_fc5025.winusb_handle, endpoint, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &to);
    if (!WinUsb_ReadPipe(g_fc5025.winusb_handle, endpoint, data, length, &transferred, NULL)) {
        return -1;
    }
    return (int)transferred;
}

#endif

/*============================================================================*
 * FC5025 CORE FUNCTIONS (adapted from official code)
 *============================================================================*/

int fc_bulk_cdb(void *cdb, int length, int timeout, void *csw_out, void *xferbuf, int xferlen, int *xferlen_out) {
    uft_fc5025_cbw_t cbw = {{'C','F','B','C'}, 0, 0, 0x80, 0, 0, {0}};
    uft_fc5025_csw_t csw;
    int ret;
    
    cbw.tag = ++g_fc5025.tag;
    cbw.xferlen = htov32(xferlen);
    memset(cbw.cdb, 0, 48);
    memcpy(cbw.cdb, cdb, length);
    
    if (xferlen_out != NULL)
        *xferlen_out = 0;
    
    /* Send command */
    ret = usb_bulk_write_wrapper(0x01, &cbw, 63, 1500);
    if (ret != 63)
        return 1;
    
    /* Read data if expected */
    if (xferlen != 0) {
        ret = usb_bulk_read_wrapper(0x81, xferbuf, xferlen, timeout);
        if (ret < 0)
            return 1;
        if (xferlen_out != NULL)
            *xferlen_out = ret;
        timeout = 500;
    }
    
    /* Read status */
    ret = usb_bulk_read_wrapper(0x81, &csw, 32, timeout);
    if (ret < 12 || ret > 31)
        return 1;
    
    if (csw.signature != htov32(0x46435342))
        return 1;
    if (csw.tag != cbw.tag)
        return 1;
    
    if (csw_out != NULL)
        memcpy(csw_out, &csw, 12);
    
    return csw.status;
}

int fc_recalibrate(void) {
    struct {
        uint8_t opcode, mode, steprate, track;
    } __attribute__((packed)) cdb = {OPCODE_SEEK, 3, 15, 100};
    
    int ret = fc_bulk_cdb(&cdb, sizeof(cdb), 600, NULL, NULL, 0, NULL);
    usleep(15000);
    return ret;
}

int fc_SEEK_abs(int track) {
    struct {
        uint8_t opcode, mode, steprate, track;
    } __attribute__((packed)) cdb = {OPCODE_SEEK, 0, 15, track};
    
    int ret = fc_bulk_cdb(&cdb, sizeof(cdb), 600, NULL, NULL, 0, NULL);
    usleep(15000);
    return ret;
}

int fc_READ_ID(unsigned char *out, int length, char side, char format, int bitcell, 
               unsigned char idam0, unsigned char idam1, unsigned char idam2) {
    struct {
        uint8_t opcode, side, format;
        uint16_t bitcell;
        uint8_t idam0, idam1, idam2;
    } __attribute__((packed)) cdb = {OPCODE_READ_ID, side, format, htons(bitcell), idam0, idam1, idam2};
    
    int xferlen_out;
    int status = fc_bulk_cdb(&cdb, sizeof(cdb), 3000, NULL, out, length, &xferlen_out);
    if (xferlen_out != length)
        status |= 1;
    return status;
}

int fc_FLAGS(int in, int mask, int *out) {
    struct {
        uint8_t opcode, mask, flags;
    } __attribute__((packed)) cdb = {OPCODE_FLAGS, mask, in};
    
    unsigned char buf;
    int xferlen_out;
    
    int ret = fc_bulk_cdb(&cdb, sizeof(cdb), 1500, NULL, &buf, 1, &xferlen_out);
    if (xferlen_out == 1) {
        if (out != NULL)
            *out = buf;
        return ret;
    }
    return 1;
}

int fc_set_density(int density) {
    return fc_FLAGS(density << 2, 4, NULL);
}

int fc_drive_status(int *status_out) {
    struct {
        uint8_t opcode;
    } __attribute__((packed)) cdb = {OPCODE_DRIVE_STATUS};
    
    unsigned char buf;
    int xferlen_out;
    
    int ret = fc_bulk_cdb(&cdb, sizeof(cdb), 1500, NULL, &buf, 1, &xferlen_out);
    if (xferlen_out == 1) {
        if (status_out != NULL)
            *status_out = buf;
        return ret;
    }
    return 1;
}

/*============================================================================*
 * DEVICE OPEN/CLOSE
 *============================================================================*/

#ifdef UFT_FC5025_PLATFORM_UNIX

int uft_fc5025_open(void) {
    if (g_fc5025.is_open)
        return 0;
    
    int rc = libusb_init(&g_fc5025.ctx);
    if (rc < 0)
        return 1;
    
    g_fc5025.handle = libusb_open_device_with_vid_pid(g_fc5025.ctx, UFT_FC5025_VID, UFT_FC5025_PID);
    if (!g_fc5025.handle) {
        libusb_exit(g_fc5025.ctx);
        return 1;
    }
    
    /* Detach kernel driver if active */
    if (libusb_kernel_driver_active(g_fc5025.handle, 0) == 1) {
        libusb_detach_kernel_driver(g_fc5025.handle, 0);
    }
    
    rc = libusb_claim_interface(g_fc5025.handle, 0);
    if (rc < 0) {
        libusb_close(g_fc5025.handle);
        libusb_exit(g_fc5025.ctx);
        return 1;
    }
    
    g_fc5025.tag = (uint32_t)time(NULL);
    g_fc5025.is_open = 1;
    return 0;
}

int uft_fc5025_close(void) {
    if (!g_fc5025.is_open)
        return 0;
    
    libusb_release_interface(g_fc5025.handle, 0);
    libusb_close(g_fc5025.handle);
    libusb_exit(g_fc5025.ctx);
    
    g_fc5025.is_open = 0;
    return 0;
}

int uft_fc5025_find(int *count_out) {
    libusb_context *ctx;
    int count = 0;
    
    if (libusb_init(&ctx) < 0)
        return 0;
    
    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(ctx, &list);
    
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) == 0) {
            if (desc.idVendor == UFT_FC5025_VID && desc.idProduct == UFT_FC5025_PID) {
                count++;
            }
        }
    }
    
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    
    if (count_out) *count_out = count;
    return count;
}

#else /* WINDOWS */

/* Windows-specific GUID for FC5025 */
DEFINE_GUID(GUID_FC5025, 
    0x16c006d6, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

int uft_fc5025_open(void) {
    if (g_fc5025.is_open)
        return 0;
    
    HDEVINFO dev_info = SetupDiGetClassDevs(
        &GUID_FC5025, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (dev_info == INVALID_HANDLE_VALUE)
        return 1;
    
    SP_DEVICE_INTERFACE_DATA iface_data;
    iface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    if (!SetupDiEnumDeviceInterfaces(dev_info, NULL, &GUID_FC5025, 0, &iface_data)) {
        SetupDiDestroyDeviceInfoList(dev_info);
        return 1;
    }
    
    DWORD required_size;
    SetupDiGetDeviceInterfaceDetail(dev_info, &iface_data, NULL, 0, &required_size, NULL);
    
    PSP_DEVICE_INTERFACE_DETAIL_DATA detail = malloc(required_size);
    if (!detail) {
        SetupDiDestroyDeviceInfoList(dev_info);
        return 1;
    }
    
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    
    if (!SetupDiGetDeviceInterfaceDetail(dev_info, &iface_data, detail, required_size, NULL, NULL)) {
        free(detail);
        SetupDiDestroyDeviceInfoList(dev_info);
        return 1;
    }
    
    g_fc5025.device_handle = CreateFile(
        detail->DevicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    free(detail);
    SetupDiDestroyDeviceInfoList(dev_info);
    
    if (g_fc5025.device_handle == INVALID_HANDLE_VALUE)
        return 1;
    
    if (!WinUsb_Initialize(g_fc5025.device_handle, &g_fc5025.winusb_handle)) {
        CloseHandle(g_fc5025.device_handle);
        return 1;
    }
    
    g_fc5025.tag = GetTickCount();
    g_fc5025.is_open = 1;
    return 0;
}

int uft_fc5025_close(void) {
    if (!g_fc5025.is_open)
        return 0;
    
    WinUsb_Free(g_fc5025.winusb_handle);
    CloseHandle(g_fc5025.device_handle);
    
    g_fc5025.is_open = 0;
    return 0;
}

int uft_fc5025_find(int *count_out) {
    /* Simplified for Windows */
    int count = 0;
    
    if (uft_fc5025_open() == 0) {
        count = 1;
        uft_fc5025_close();
    }
    
    if (count_out) *count_out = count;
    return count;
}

#endif

/*============================================================================*
 * HIGH-LEVEL API
 *============================================================================*/

int uft_fc5025_is_open(void) {
    return g_fc5025.is_open;
}

/* Export functions for use by official format handlers */
int uft_fc5025_read_flexible(unsigned char *out, int length, int timeout,
                         unsigned char flags, unsigned char format,
                         unsigned short bitcell, unsigned char sectorhole,
                         unsigned short rdelay, unsigned char idam,
                         unsigned char *id_pat, unsigned char *id_mask,
                         unsigned char *dam, int *xferlen_out) {
    struct {
        uint8_t opcode, flags, format;
        uint16_t bitcell;
        uint8_t sectorhole;
        uint8_t rdelayh;
        uint16_t rdelayl;
        uint8_t idam;
        uint8_t id_pat[12];
        uint8_t id_mask[12];
        uint8_t dam[3];
    } __attribute__((packed)) cdb;
    
    memset(&cdb, 0, sizeof(cdb));
    cdb.opcode = OPCODE_READ_FLEXIBLE;
    cdb.flags = flags;
    cdb.format = format;
    cdb.bitcell = htons(bitcell);
    cdb.sectorhole = sectorhole;
    cdb.rdelayh = (rdelay >> 16) & 0xff;
    cdb.rdelayl = htons(rdelay & 0xffff);
    cdb.idam = idam;
    
    if (id_pat) memcpy(cdb.id_pat, id_pat, 12);
    if (id_mask) memcpy(cdb.id_mask, id_mask, 12);
    if (dam) memcpy(cdb.dam, dam, 3);
    
    return fc_bulk_cdb(&cdb, sizeof(cdb), timeout, NULL, out, length, xferlen_out);
}

const char *uft_fc5025_get_version(void) {
    return "FC5025 Driver v1309 (libusb-1.0/WinUSB wrapper)";
}
