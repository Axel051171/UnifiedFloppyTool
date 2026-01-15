/**
 * @file uft_usb.c
 * @brief USB device handling
 * @version 3.8.0
 */
#include "uft/compat/uft_platform.h"
// SPDX-License-Identifier: MIT
/*
 * uft_usb.c - Platform-Independent USB Access Implementation
 * 
 * BACKENDS:
 *   - Linux:   libusb-1.0 (statically linked if available)
 *   - macOS:   libusb-1.0 (via Homebrew) or IOKit native
 *   - Windows: WinUSB native (NO external deps!)
 * 
 * The goal is to minimize or eliminate external dependencies.
 * On Windows, we use WinUSB directly - no libusb installation needed!
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include "uft/uft_usb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*============================================================================*
 * PLATFORM DETECTION
 *============================================================================*/

#if defined(_WIN32) || defined(_WIN64)
    #define UFT_USB_PLATFORM_WINDOWS
    #define UFT_USB_BACKEND_WINUSB
#elif defined(__APPLE__)
    #define UFT_USB_PLATFORM_MACOS
    #define UFT_USB_BACKEND_LIBUSB  /* Or IOKit */
#elif defined(__linux__)
    #define UFT_USB_PLATFORM_LINUX
    #define UFT_USB_BACKEND_LIBUSB
#else
    #error "Unsupported platform"
#endif

/*============================================================================*
 * WINDOWS IMPLEMENTATION (WinUSB - Native, No Dependencies!)
 *============================================================================*/

#ifdef UFT_USB_BACKEND_WINUSB

#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <initguid.h>

/* WinUSB GUID for XUM1541 */
DEFINE_GUID(GUID_XUM1541, 
    0xa5dcbf10, 0x6530, 0x11d2, 0x90, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x51, 0xed);

struct uft_usb_device {
    HANDLE device_handle;
    WINUSB_INTERFACE_HANDLE winusb_handle;
    uint16_t vid;
    uint16_t pid;
};

static bool g_usb_initialized = false;

uft_usb_result_t uft_usb_init(void)
{
    g_usb_initialized = true;
    return UFT_USB_OK;
}

void uft_usb_exit(void)
{
    g_usb_initialized = false;
}

void uft_usb_get_backend_info(char *backend_name, size_t len)
{
    snprintf(backend_name, len, "WinUSB (Native Windows)");
}

uft_usb_result_t uft_usb_find_devices(
    uint16_t vendor_id,
    uint16_t product_id,
    uft_usb_device_info_t *devices_out,
    size_t max_devices,
    size_t *count_out)
{
    if (!devices_out || !count_out) {
        return UFT_USB_ERROR_OTHER;
    }
    
    *count_out = 0;
    
    HDEVINFO dev_info = SetupDiGetClassDevs(
        &GUID_XUM1541,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (dev_info == INVALID_HANDLE_VALUE) {
        return UFT_USB_ERROR_NOT_FOUND;
    }
    
    SP_DEVICE_INTERFACE_DATA iface_data;
    iface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    DWORD index = 0;
    while (*count_out < max_devices &&
           SetupDiEnumDeviceInterfaces(dev_info, NULL, &GUID_XUM1541, index++, &iface_data))
    {
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetail(dev_info, &iface_data, NULL, 0, &required_size, NULL);
        
        PSP_DEVICE_INTERFACE_DETAIL_DATA detail = 
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(required_size);
        if (!detail) continue;
        
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        
        if (SetupDiGetDeviceInterfaceDetail(dev_info, &iface_data, detail, required_size, NULL, NULL))
        {
            /* Parse VID/PID from device path */
            /* Path format: \\?\usb#vid_XXXX&pid_XXXX#... */
            uint16_t vid = 0, pid = 0;
            char *vid_str = strstr(detail->DevicePath, "vid_");
            char *pid_str = strstr(detail->DevicePath, "pid_");
            
            if (vid_str) vid = (uint16_t)strtol(vid_str + 4, NULL, 16);
            if (pid_str) pid = (uint16_t)strtol(pid_str + 4, NULL, 16);
            
            /* Filter by VID/PID if specified */
            if ((vendor_id == 0 || vid == vendor_id) &&
                (product_id == 0 || pid == product_id))
            {
                uft_usb_device_info_t *dev = &devices_out[*count_out];
                memset(dev, 0, sizeof(*dev));
                dev->vendor_id = vid;
                dev->product_id = pid;
                snprintf(dev->product, sizeof(dev->product), "USB Device %04X:%04X", vid, pid);
                (*count_out)++;
            }
        }
        
        free(detail);
    }
    
    SetupDiDestroyDeviceInfoList(dev_info);
    
    return (*count_out > 0) ? UFT_USB_OK : UFT_USB_ERROR_NOT_FOUND;
}

uft_usb_result_t uft_usb_open(
    uint16_t vendor_id,
    uint16_t product_id,
    uft_usb_device_t **device_out)
{
    if (!device_out) {
        return UFT_USB_ERROR_OTHER;
    }
    
    *device_out = NULL;
    
    /* Find device path */
    HDEVINFO dev_info = SetupDiGetClassDevs(
        &GUID_XUM1541,
        NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (dev_info == INVALID_HANDLE_VALUE) {
        return UFT_USB_ERROR_NOT_FOUND;
    }
    
    SP_DEVICE_INTERFACE_DATA iface_data;
    iface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    char device_path[256] = {0};
    bool found = false;
    DWORD index = 0;
    
    while (SetupDiEnumDeviceInterfaces(dev_info, NULL, &GUID_XUM1541, index++, &iface_data))
    {
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetail(dev_info, &iface_data, NULL, 0, &required_size, NULL);
        
        PSP_DEVICE_INTERFACE_DETAIL_DATA detail = 
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(required_size);
        if (!detail) continue;
        
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        
        if (SetupDiGetDeviceInterfaceDetail(dev_info, &iface_data, detail, required_size, NULL, NULL))
        {
            uint16_t vid = 0, pid = 0;
            char *vid_str = strstr(detail->DevicePath, "vid_");
            char *pid_str = strstr(detail->DevicePath, "pid_");
            
            if (vid_str) vid = (uint16_t)strtol(vid_str + 4, NULL, 16);
            if (pid_str) pid = (uint16_t)strtol(pid_str + 4, NULL, 16);
            
            if (vid == vendor_id && pid == product_id)
            {
                strncpy(device_path, detail->DevicePath, sizeof(device_path) - 1);
                found = true;
            }
        }
        
        free(detail);
        if (found) break;
    }
    
    SetupDiDestroyDeviceInfoList(dev_info);
    
    if (!found) {
        return UFT_USB_ERROR_NOT_FOUND;
    }
    
    /* Open device */
    HANDLE file_handle = CreateFile(
        device_path,
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if (file_handle == INVALID_HANDLE_VALUE) {
        return UFT_USB_ERROR_ACCESS;
    }
    
    /* Initialize WinUSB */
    WINUSB_INTERFACE_HANDLE winusb_handle;
    if (!WinUsb_Initialize(file_handle, &winusb_handle)) {
        CloseHandle(file_handle);
        return UFT_USB_ERROR_IO;
    }
    
    /* Create device structure */
    uft_usb_device_t *dev = (uft_usb_device_t*)calloc(1, sizeof(uft_usb_device_t));
    if (!dev) {
        WinUsb_Free(winusb_handle);
        CloseHandle(file_handle);
        return UFT_USB_ERROR_NO_MEM;
    }
    
    dev->device_handle = file_handle;
    dev->winusb_handle = winusb_handle;
    dev->vid = vendor_id;
    dev->pid = product_id;
    
    *device_out = dev;
    return UFT_USB_OK;
}

void uft_usb_close(uft_usb_device_t *device)
{
    if (device) {
        if (device->winusb_handle) {
            WinUsb_Free(device->winusb_handle);
        }
        if (device->device_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(device->device_handle);
        }
        free(device);
    }
}

uft_usb_result_t uft_usb_claim_interface(uft_usb_device_t *device, int interface)
{
    (void)device;
    (void)interface;
    /* WinUSB handles this automatically */
    return UFT_USB_OK;
}

uft_usb_result_t uft_usb_release_interface(uft_usb_device_t *device, int interface)
{
    (void)device;
    (void)interface;
    return UFT_USB_OK;
}

uft_usb_result_t uft_usb_bulk_transfer(
    uft_usb_device_t *device,
    uint8_t endpoint,
    uint8_t *data,
    int length,
    int *transferred,
    unsigned int timeout_ms)
{
    if (!device || !device->winusb_handle || !data) {
        return UFT_USB_ERROR_OTHER;
    }
    
    ULONG actual = 0;
    BOOL success;
    
    /* Set timeout */
    ULONG timeout = timeout_ms;
    WinUsb_SetPipePolicy(device->winusb_handle, endpoint, PIPE_TRANSFER_TIMEOUT, 
                         sizeof(ULONG), &timeout);
    
    if (endpoint & 0x80) {
        /* IN transfer (read) */
        success = WinUsb_ReadPipe(device->winusb_handle, endpoint, data, length, &actual, NULL);
    } else {
        /* OUT transfer (write) */
        success = WinUsb_WritePipe(device->winusb_handle, endpoint, data, length, &actual, NULL);
    }
    
    if (transferred) {
        *transferred = (int)actual;
    }
    
    if (!success) {
        DWORD err = GetLastError();
        if (err == ERROR_SEM_TIMEOUT) return UFT_USB_ERROR_TIMEOUT;
        return UFT_USB_ERROR_IO;
    }
    
    return UFT_USB_OK;
}

int uft_usb_control_transfer(
    uft_usb_device_t *device,
    uint8_t request_type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    uint8_t *data,
    uint16_t length,
    unsigned int timeout_ms)
{
    if (!device || !device->winusb_handle) {
        return UFT_USB_ERROR_OTHER;
    }
    
    WINUSB_SETUP_PACKET setup;
    setup.RequestType = request_type;
    setup.Request = request;
    setup.Value = value;
    setup.Index = index;
    setup.Length = length;
    
    ULONG actual = 0;
    
    if (!WinUsb_ControlTransfer(device->winusb_handle, setup, data, length, &actual, NULL)) {
        return -1;
    }
    
    return (int)actual;
}

bool uft_usb_has_admin_rights(void)
{
    BOOL is_admin = FALSE;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admin_group;
    
    if (AllocateAndInitializeSid(&nt_authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &admin_group))
    {
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    
    return is_admin;
}

void uft_usb_get_setup_instructions(
    const char *device_name,
    char *instructions_out,
    size_t instructions_len)
{
    snprintf(instructions_out, instructions_len,
        "Windows Setup for %s:\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        "\n"
        "The driver should be installed automatically.\n"
        "\n"
        "If the device is not recognized:\n"
        "1. Download Zadig: https://zadig.akeo.ie/\n"
        "2. Run Zadig as Administrator\n"
        "3. Select your device from the list\n"
        "4. Choose 'WinUSB' as the driver\n"
        "5. Click 'Replace Driver'\n"
        "6. Restart this application\n"
        "\n"
        "Note: UnifiedFloppyTool uses WinUSB directly.\n"
        "      No additional software is required!\n",
        device_name);
}

#endif /* UFT_USB_BACKEND_WINUSB */

/*============================================================================*
 * LINUX / MACOS IMPLEMENTATION (libusb-1.0)
 *============================================================================*/

#ifdef UFT_USB_BACKEND_LIBUSB

#include <libusb-1.0/libusb.h>

struct uft_usb_device {
    libusb_device_handle *handle;
    uint16_t vid;
    uint16_t pid;
};

static libusb_context *g_usb_ctx = NULL;

uft_usb_result_t uft_usb_init(void)
{
    if (g_usb_ctx) {
        return UFT_USB_OK;  /* Already initialized */
    }
    
    int rc = libusb_init(&g_usb_ctx);
    if (rc < 0) {
        return UFT_USB_ERROR_OTHER;
    }
    
    return UFT_USB_OK;
}

void uft_usb_exit(void)
{
    if (g_usb_ctx) {
        libusb_exit(g_usb_ctx);
        g_usb_ctx = NULL;
    }
}

void uft_usb_get_backend_info(char *backend_name, size_t len)
{
    const struct libusb_version *ver = libusb_get_version();
    snprintf(backend_name, len, "libusb-%d.%d.%d.%d",
             ver->major, ver->minor, ver->micro, ver->nano);
}

static uft_usb_result_t libusb_to_uft_error(int libusb_error)
{
    switch (libusb_error) {
        case LIBUSB_SUCCESS:           return UFT_USB_OK;
        case LIBUSB_ERROR_NO_DEVICE:   return UFT_USB_ERROR_NOT_FOUND;
        case LIBUSB_ERROR_ACCESS:      return UFT_USB_ERROR_ACCESS;
        case LIBUSB_ERROR_BUSY:        return UFT_USB_ERROR_BUSY;
        case LIBUSB_ERROR_TIMEOUT:     return UFT_USB_ERROR_TIMEOUT;
        case LIBUSB_ERROR_OVERFLOW:    return UFT_USB_ERROR_OVERFLOW;
        case LIBUSB_ERROR_PIPE:        return UFT_USB_ERROR_PIPE;
        case LIBUSB_ERROR_NO_MEM:      return UFT_USB_ERROR_NO_MEM;
        case LIBUSB_ERROR_NOT_SUPPORTED: return UFT_USB_ERROR_NOT_SUPPORTED;
        default:                       return UFT_USB_ERROR_OTHER;
    }
}

uft_usb_result_t uft_usb_find_devices(
    uint16_t vendor_id,
    uint16_t product_id,
    uft_usb_device_info_t *devices_out,
    size_t max_devices,
    size_t *count_out)
{
    if (!g_usb_ctx || !devices_out || !count_out) {
        return UFT_USB_ERROR_OTHER;
    }
    
    *count_out = 0;
    
    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(g_usb_ctx, &list);
    if (cnt < 0) {
        return libusb_to_uft_error((int)cnt);
    }
    
    for (ssize_t i = 0; i < cnt && *count_out < max_devices; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        
        /* Filter by VID/PID */
        if ((vendor_id == 0 || desc.idVendor == vendor_id) &&
            (product_id == 0 || desc.idProduct == product_id))
        {
            uft_usb_device_info_t *dev = &devices_out[*count_out];
            memset(dev, 0, sizeof(*dev));
            
            dev->vendor_id = desc.idVendor;
            dev->product_id = desc.idProduct;
            dev->bus = libusb_get_bus_number(list[i]);
            dev->address = libusb_get_device_address(list[i]);
            
            /* Try to get string descriptors */
            libusb_device_handle *handle;
            if (libusb_open(list[i], &handle) == 0) {
                if (desc.iManufacturer) {
                    libusb_get_string_descriptor_ascii(handle, desc.iManufacturer,
                        (unsigned char*)dev->manufacturer, sizeof(dev->manufacturer));
                }
                if (desc.iProduct) {
                    libusb_get_string_descriptor_ascii(handle, desc.iProduct,
                        (unsigned char*)dev->product, sizeof(dev->product));
                }
                if (desc.iSerialNumber) {
                    libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                        (unsigned char*)dev->serial, sizeof(dev->serial));
                }
                libusb_close(handle);
            }
            
            (*count_out)++;
        }
    }
    
    libusb_free_device_list(list, 1);
    
    return (*count_out > 0) ? UFT_USB_OK : UFT_USB_ERROR_NOT_FOUND;
}

uft_usb_result_t uft_usb_open(
    uint16_t vendor_id,
    uint16_t product_id,
    uft_usb_device_t **device_out)
{
    if (!g_usb_ctx || !device_out) {
        return UFT_USB_ERROR_OTHER;
    }
    
    *device_out = NULL;
    
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(g_usb_ctx, vendor_id, product_id);
    if (!handle) {
        return UFT_USB_ERROR_NOT_FOUND;
    }
    
    uft_usb_device_t *dev = (uft_usb_device_t*)calloc(1, sizeof(uft_usb_device_t));
    if (!dev) {
        libusb_close(handle);
        return UFT_USB_ERROR_NO_MEM;
    }
    
    dev->handle = handle;
    dev->vid = vendor_id;
    dev->pid = product_id;
    
    *device_out = dev;
    return UFT_USB_OK;
}

uft_usb_result_t uft_usb_open_by_address(
    uint8_t bus,
    uint8_t address,
    uft_usb_device_t **device_out)
{
    if (!g_usb_ctx || !device_out) {
        return UFT_USB_ERROR_OTHER;
    }
    
    *device_out = NULL;
    
    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(g_usb_ctx, &list);
    if (cnt < 0) {
        return UFT_USB_ERROR_OTHER;
    }
    
    libusb_device *found = NULL;
    for (ssize_t i = 0; i < cnt; i++) {
        if (libusb_get_bus_number(list[i]) == bus &&
            libusb_get_device_address(list[i]) == address)
        {
            found = list[i];
            break;
        }
    }
    
    if (!found) {
        libusb_free_device_list(list, 1);
        return UFT_USB_ERROR_NOT_FOUND;
    }
    
    libusb_device_handle *handle;
    int rc = libusb_open(found, &handle);
    libusb_free_device_list(list, 1);
    
    if (rc != 0) {
        return libusb_to_uft_error(rc);
    }
    
    uft_usb_device_t *dev = (uft_usb_device_t*)calloc(1, sizeof(uft_usb_device_t));
    if (!dev) {
        libusb_close(handle);
        return UFT_USB_ERROR_NO_MEM;
    }
    
    dev->handle = handle;
    
    *device_out = dev;
    return UFT_USB_OK;
}

void uft_usb_close(uft_usb_device_t *device)
{
    if (device) {
        if (device->handle) {
            libusb_close(device->handle);
        }
        free(device);
    }
}

uft_usb_result_t uft_usb_claim_interface(uft_usb_device_t *device, int interface)
{
    if (!device || !device->handle) {
        return UFT_USB_ERROR_OTHER;
    }
    
    /* Detach kernel driver if active */
    if (libusb_kernel_driver_active(device->handle, interface) == 1) {
        libusb_detach_kernel_driver(device->handle, interface);
    }
    
    int rc = libusb_claim_interface(device->handle, interface);
    return libusb_to_uft_error(rc);
}

uft_usb_result_t uft_usb_release_interface(uft_usb_device_t *device, int interface)
{
    if (!device || !device->handle) {
        return UFT_USB_ERROR_OTHER;
    }
    
    int rc = libusb_release_interface(device->handle, interface);
    return libusb_to_uft_error(rc);
}

uft_usb_result_t uft_usb_bulk_transfer(
    uft_usb_device_t *device,
    uint8_t endpoint,
    uint8_t *data,
    int length,
    int *transferred,
    unsigned int timeout_ms)
{
    if (!device || !device->handle || !data) {
        return UFT_USB_ERROR_OTHER;
    }
    
    int actual = 0;
    int rc = libusb_bulk_transfer(device->handle, endpoint, data, length, &actual, timeout_ms);
    
    if (transferred) {
        *transferred = actual;
    }
    
    return libusb_to_uft_error(rc);
}

int uft_usb_control_transfer(
    uft_usb_device_t *device,
    uint8_t request_type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    uint8_t *data,
    uint16_t length,
    unsigned int timeout_ms)
{
    if (!device || !device->handle) {
        return -1;
    }
    
    return libusb_control_transfer(device->handle, request_type, request,
                                   value, index, data, length, timeout_ms);
}

bool uft_usb_has_admin_rights(void)
{
#ifdef UFT_USB_PLATFORM_LINUX
    return (geteuid() == 0);
#else
    return false;  /* macOS doesn't need root for USB */
#endif
}

void uft_usb_get_setup_instructions(
    const char *device_name,
    char *instructions_out,
    size_t instructions_len)
{
#ifdef UFT_USB_PLATFORM_LINUX
    snprintf(instructions_out, instructions_len,
        "Linux Setup for %s:\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        "\n"
        "To use USB devices without root:\n"
        "\n"
        "1. Create udev rules file:\n"
        "   sudo nano /etc/udev/rules.d/50-floppy-tools.rules\n"
        "\n"
        "2. Add these lines:\n"
        "   # XUM1541 / ZoomFloppy\n"
        "   SUBSYSTEM==\"usb\", ATTR{idVendor}==\"16d0\", ATTR{idProduct}==\"0504\", MODE=\"0666\"\n"
        "   # Greaseweazle\n"
        "   SUBSYSTEM==\"usb\", ATTR{idVendor}==\"1209\", ATTR{idProduct}==\"4d69\", MODE=\"0666\"\n"
        "   SUBSYSTEM==\"usb\", ATTR{idVendor}==\"1209\", ATTR{idProduct}==\"0001\", MODE=\"0666\"\n"
        "\n"
        "3. Reload rules:\n"
        "   sudo udevadm control --reload-rules\n"
        "   sudo udevadm trigger\n"
        "\n"
        "4. Reconnect your device\n",
        device_name);
#else  /* macOS */
    snprintf(instructions_out, instructions_len,
        "macOS Setup for %s:\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        "\n"
        "No special setup required!\n"
        "\n"
        "Just connect your device and it should work.\n"
        "\n"
        "If you have issues:\n"
        "1. Check System Preferences → Security & Privacy\n"
        "2. Allow the USB device if prompted\n"
        "3. Try a different USB port\n",
        device_name);
#endif
}

#endif /* UFT_USB_BACKEND_LIBUSB */

/*============================================================================*
 * COMMON FUNCTIONS
 *============================================================================*/

uft_usb_result_t uft_usb_find_floppy_hardware(
    uft_usb_device_info_t *devices_out,
    size_t max_devices,
    size_t *count_out)
{
    if (!devices_out || !count_out) {
        return UFT_USB_ERROR_OTHER;
    }
    
    *count_out = 0;
    
    /* Known floppy hardware VID/PID pairs */
    static const struct {
        uint16_t vid;
        uint16_t pid;
        const char *name;
    } known_devices[] = {
        { UFT_USB_VID_XUM1541,       UFT_USB_PID_XUM1541,         "XUM1541/ZoomFloppy" },
        { UFT_USB_VID_GREASEWEAZLE,  UFT_USB_PID_UFT_GW_F1, "Greaseweazle F1" },
        { UFT_USB_VID_GREASEWEAZLE,  UFT_USB_PID_UFT_GW_F7, "Greaseweazle F7" },
        { UFT_USB_VID_KRYOFLUX,      UFT_USB_PID_KRYOFLUX,        "KryoFlux" },
        { UFT_USB_VID_SCP,           UFT_USB_PID_SCP,             "SuperCard Pro" },
        { UFT_USB_VID_FC5025,        UFT_USB_PID_FC5025,          "FC5025" },
    };
    
    const size_t num_known = sizeof(known_devices) / sizeof(known_devices[0]);
    
    for (size_t i = 0; i < num_known && *count_out < max_devices; i++) {
        uft_usb_device_info_t found[4];
        size_t found_count = 0;
        
        uft_usb_result_t rc = uft_usb_find_devices(
            known_devices[i].vid,
            known_devices[i].pid,
            found,
            4,
            &found_count
        );
        
        if (rc == UFT_USB_OK && found_count > 0) {
            for (size_t j = 0; j < found_count && *count_out < max_devices; j++) {
                devices_out[*count_out] = found[j];
                /* Add friendly name if product is empty */
                if (devices_out[*count_out].product[0] == '\0') {
                    strncpy(devices_out[*count_out].product, 
                            known_devices[i].name,
                            sizeof(devices_out[*count_out].product) - 1);
                }
                (*count_out)++;
            }
        }
    }
    
    return (*count_out > 0) ? UFT_USB_OK : UFT_USB_ERROR_NOT_FOUND;
}
