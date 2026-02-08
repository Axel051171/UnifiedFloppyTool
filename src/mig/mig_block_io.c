/**
 * @file mig_block_io.c
 * @brief MIG-Flash Block I/O Implementation
 * 
 * MIG-Flash is a USB Mass Storage device, NOT a serial device!
 * Communication happens via raw block I/O (sector reads/writes).
 * 
 * @version 2.0.0
 * @date 2026-01-20
 */

#include "uft/mig/mig_block_io.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#pragma comment(lib, "setupapi.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#ifdef __APPLE__
#include <sys/disk.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#else
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <libudev.h>
#endif
#endif

/*============================================================================
 * INTERNAL DEVICE STRUCTURE
 *============================================================================*/

struct mig_device {
#ifdef _WIN32
    HANDLE              handle;
    HANDLE              volume_handle;
#else
    int                 fd;
#endif
    char                path[256];
    bool                opened;
    bool                authenticated;
    
    /* Cached data */
    char                firmware_version[32];
    mig_xci_header_t    xci_header;
    bool                xci_header_valid;
    
    /* Cart info */
    bool                cart_inserted;
    uint64_t            cart_total_size;
    uint64_t            cart_trimmed_size;
};

/*============================================================================
 * PLATFORM-SPECIFIC BLOCK I/O
 *============================================================================*/

#ifdef _WIN32

/*----------------------------------------------------------------------------
 * WINDOWS IMPLEMENTATION
 *----------------------------------------------------------------------------*/

static mig_error_t mig_open_windows(mig_device_t *dev, const char *path) {
    /* Open physical drive */
    dev->handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (dev->handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return MIG_ERROR_ACCESS;
        }
        return MIG_ERROR_NOT_FOUND;
    }
    
    return MIG_OK;
}

static void mig_close_windows(mig_device_t *dev) {
    if (dev->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(dev->handle);
        dev->handle = INVALID_HANDLE_VALUE;
    }
}

static int64_t mig_read_windows(mig_device_t *dev, uint64_t offset, 
                                 void *buffer, size_t size) {
    LARGE_INTEGER li;
    li.QuadPart = offset;
    
    if (!SetFilePointerEx(dev->handle, li, NULL, FILE_BEGIN)) {
        return -1;
    }
    
    DWORD bytes_read = 0;
    if (!ReadFile(dev->handle, buffer, (DWORD)size, &bytes_read, NULL)) {
        return -1;
    }
    
    return (int64_t)bytes_read;
}

static int64_t mig_write_windows(mig_device_t *dev, uint64_t offset,
                                  const void *buffer, size_t size) {
    LARGE_INTEGER li;
    li.QuadPart = offset;
    
    if (!SetFilePointerEx(dev->handle, li, NULL, FILE_BEGIN)) {
        return -1;
    }
    
    DWORD bytes_written = 0;
    if (!WriteFile(dev->handle, buffer, (DWORD)size, &bytes_written, NULL)) {
        return -1;
    }
    
    return (int64_t)bytes_written;
}

/**
 * Find MIG devices on Windows by scanning physical drives
 */
static int mig_find_windows(mig_device_info_t *devices, int max_count) {
    int count = 0;
    
    /* Scan PhysicalDrive0 through PhysicalDrive31 */
    for (int i = 0; i < 32 && count < max_count; i++) {
        char path[64];
        snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", i);
        
        HANDLE h = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (h == INVALID_HANDLE_VALUE) {
            continue;
        }
        
        /* Check if removable */
        DISK_GEOMETRY geo;
        DWORD bytes;
        if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                           NULL, 0, &geo, sizeof(geo), &bytes, NULL)) {
            
            if (geo.MediaType == RemovableMedia) {
                /* Read GPT to verify it's a MIG device */
                uint8_t gpt_buf[512];
                LARGE_INTEGER li;
                li.QuadPart = MIG_GPT_PARTITION_OFFSET;
                
                SetFilePointerEx(h, li, NULL, FILE_BEGIN);
                DWORD read;
                if (ReadFile(h, gpt_buf, 512, &read, NULL) && read == 512) {
                    
                    /* Check GPT GUID */
                    if (memcmp(gpt_buf, MIG_GPT_MSDATA_GUID, 16) == 0) {
                        /* It's a MIG device! */
                        if (devices) {
                            strncpy(devices[count].path, path, 
                                   sizeof(devices[count].path) - 1);
                            devices[count].is_removable = true;
                            devices[count].is_valid = true;
                            
                            /* Try to get firmware version */
                            li.QuadPart = MIG_FIRMWARE_OFFSET;
                            SetFilePointerEx(h, li, NULL, FILE_BEGIN);
                            uint8_t fw_buf[32];
                            if (ReadFile(h, fw_buf, 32, &read, NULL)) {
                                memcpy(devices[count].firmware_version, 
                                      fw_buf, MIG_FIRMWARE_VERSION_LEN);
                                devices[count].firmware_version[MIG_FIRMWARE_VERSION_LEN] = '\0';
                            }
                        }
                        count++;
                    }
                }
            }
        }
        
        CloseHandle(h);
    }
    
    return count;
}

#else /* Unix/Linux/macOS */

/*----------------------------------------------------------------------------
 * UNIX IMPLEMENTATION
 *----------------------------------------------------------------------------*/

static mig_error_t mig_open_unix(mig_device_t *dev, const char *path) {
#ifdef __APPLE__
    /* On macOS, unmount the disk first */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "diskutil unmountDisk force %s 2>/dev/null", path);
    system(cmd);
#endif
    
    dev->fd = open(path, O_RDWR | O_SYNC);
    if (dev->fd < 0) {
        if (errno == EACCES || errno == EPERM) {
            return MIG_ERROR_ACCESS;
        }
        return MIG_ERROR_NOT_FOUND;
    }
    
    return MIG_OK;
}

static void mig_close_unix(mig_device_t *dev) {
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
}

static int64_t mig_read_unix(mig_device_t *dev, uint64_t offset,
                              void *buffer, size_t size) {
    if (lseek(dev->fd, offset, SEEK_SET) == (off_t)-1) {
        return -1;
    }
    
    ssize_t n = read(dev->fd, buffer, size);
    return (int64_t)n;
}

static int64_t mig_write_unix(mig_device_t *dev, uint64_t offset,
                               const void *buffer, size_t size) {
    if (lseek(dev->fd, offset, SEEK_SET) == (off_t)-1) {
        return -1;
    }
    
    ssize_t n = write(dev->fd, buffer, size);
    return (int64_t)n;
}

#ifdef __APPLE__

/**
 * Find MIG devices on macOS using diskutil
 */
static int mig_find_macos(mig_device_info_t *devices, int max_count) {
    int count = 0;
    
    /* Use diskutil to list external disks */
    FILE *fp = popen("diskutil list -plist external 2>/dev/null", "r");
    if (!fp) return 0;
    
    /* Read output - simplified parsing */
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[len] = '\0';
    pclose(fp);
    
    /* Look for disk identifiers */
    char *p = buf;
    while ((p = strstr(p, "disk")) != NULL && count < max_count) {
        /* Extract disk number */
        int disk_num;
        if (sscanf(p, "disk%d", &disk_num) == 1) {
            char path[64];
            snprintf(path, sizeof(path), "/dev/disk%d", disk_num);
            
            /* Try to open and verify */
            int fd = open(path, O_RDONLY);
            if (fd >= 0) {
                uint8_t gpt_buf[512];
                if (pread(fd, gpt_buf, 512, MIG_GPT_PARTITION_OFFSET) == 512) {
                    if (memcmp(gpt_buf, MIG_GPT_MSDATA_GUID, 16) == 0) {
                        if (devices) {
                            snprintf(devices[count].path, 
                                    sizeof(devices[count].path),
                                    "/dev/rdisk%d", disk_num);  /* Use raw device */
                            devices[count].is_removable = true;
                            devices[count].is_valid = true;
                            
                            /* Read firmware version */
                            uint8_t fw_buf[32];
                            if (pread(fd, fw_buf, 32, MIG_FIRMWARE_OFFSET) > 0) {
                                memcpy(devices[count].firmware_version,
                                      fw_buf, MIG_FIRMWARE_VERSION_LEN);
                            }
                        }
                        count++;
                    }
                }
                close(fd);
            }
        }
        p++;
    }
    
    return count;
}

#else /* Linux */

/**
 * Find MIG devices on Linux using /sys/block
 */
static int mig_find_linux(mig_device_info_t *devices, int max_count) {
    int count = 0;
    
    DIR *dir = opendir("/sys/block");
    if (!dir) return 0;
    
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && count < max_count) {
        /* Look for sd* devices (USB storage) */
        if (strncmp(ent->d_name, "sd", 2) != 0) {
            continue;
        }
        
        /* Check if USB device */
        char sysfs_path[256];
        snprintf(sysfs_path, sizeof(sysfs_path),
                "/sys/block/%s/device/../../removable", ent->d_name);
        
        FILE *fp = fopen(sysfs_path, "r");
        if (!fp) continue;
        
        int removable = 0;
        fscanf(fp, "%d", &removable);
        fclose(fp);
        
        if (!removable) continue;
        
        /* Try to verify MIG device */
        char dev_path[64];
        snprintf(dev_path, sizeof(dev_path), "/dev/%s", ent->d_name);
        
        int fd = open(dev_path, O_RDONLY);
        if (fd >= 0) {
            uint8_t gpt_buf[512];
            if (pread(fd, gpt_buf, 512, MIG_GPT_PARTITION_OFFSET) == 512) {
                if (memcmp(gpt_buf, MIG_GPT_MSDATA_GUID, 16) == 0) {
                    if (devices) {
                        strncpy(devices[count].path, dev_path,
                               sizeof(devices[count].path) - 1);
                        devices[count].is_removable = true;
                        devices[count].is_valid = true;
                        
                        /* Read firmware version */
                        uint8_t fw_buf[32];
                        if (pread(fd, fw_buf, 32, MIG_FIRMWARE_OFFSET) > 0) {
                            memcpy(devices[count].firmware_version,
                                  fw_buf, MIG_FIRMWARE_VERSION_LEN);
                        }
                    }
                    count++;
                }
            }
            close(fd);
        }
    }
    
    closedir(dir);
    return count;
}

#endif /* __APPLE__ */

#endif /* _WIN32 */

/*============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

int mig_find_devices(mig_device_info_t *devices, int max_count) {
    if (max_count <= 0) return 0;
    
    if (devices) {
        memset(devices, 0, sizeof(mig_device_info_t) * max_count);
    }
    
#ifdef _WIN32
    return mig_find_windows(devices, max_count);
#elif defined(__APPLE__)
    return mig_find_macos(devices, max_count);
#else
    return mig_find_linux(devices, max_count);
#endif
}

mig_error_t mig_open(const char *device_path, mig_device_t **device_out) {
    if (!device_path || !device_out) {
        return MIG_ERROR_INVALID;
    }
    
    /* Allocate device structure */
    mig_device_t *dev = (mig_device_t *)calloc(1, sizeof(mig_device_t));
    if (!dev) {
        return MIG_ERROR;
    }
    
#ifdef _WIN32
    dev->handle = INVALID_HANDLE_VALUE;
    dev->volume_handle = INVALID_HANDLE_VALUE;
#else
    dev->fd = -1;
#endif
    
    strncpy(dev->path, device_path, sizeof(dev->path) - 1);
    
    /* Open device */
    mig_error_t err;
#ifdef _WIN32
    err = mig_open_windows(dev, device_path);
#else
    err = mig_open_unix(dev, device_path);
#endif
    
    if (err != MIG_OK) {
        free(dev);
        return err;
    }
    
    /* Verify it's a MIG device by checking GPT */
    uint8_t gpt_buf[512];
    int64_t n = mig_read_raw(dev, MIG_GPT_PARTITION_OFFSET, gpt_buf, 512);
    if (n != 512 || memcmp(gpt_buf, MIG_GPT_MSDATA_GUID, 16) != 0) {
        mig_close(dev);
        return MIG_ERROR_INVALID;
    }
    
    dev->opened = true;
    
    /* Read firmware version */
    uint8_t fw_buf[32];
    n = mig_read_raw(dev, MIG_FIRMWARE_OFFSET, fw_buf, 32);
    if (n >= MIG_FIRMWARE_VERSION_LEN) {
        memcpy(dev->firmware_version, fw_buf, MIG_FIRMWARE_VERSION_LEN);
        /* Null-terminate */
        for (int i = 0; i < MIG_FIRMWARE_VERSION_LEN; i++) {
            if (dev->firmware_version[i] < 0x20 || dev->firmware_version[i] > 0x7E) {
                dev->firmware_version[i] = '\0';
                break;
            }
        }
    }
    
    *device_out = dev;
    return MIG_OK;
}

void mig_close(mig_device_t *device) {
    if (!device) return;
    
#ifdef _WIN32
    mig_close_windows(device);
#else
    mig_close_unix(device);
#endif
    
    device->opened = false;
    free(device);
}

const char* mig_get_firmware_version(mig_device_t *device) {
    if (!device || !device->opened) {
        return "Unknown";
    }
    return device->firmware_version;
}

/*============================================================================
 * CARTRIDGE DETECTION
 *============================================================================*/

bool mig_cart_inserted(mig_device_t *device) {
    if (!device || !device->opened) {
        return false;
    }
    
    /* Try to read XCI header magic */
    uint8_t buf[4];
    int64_t n = mig_read_raw(device, MIG_XCI_HEADER_OFFSET + 0x100, buf, 4);
    if (n != 4) {
        return false;
    }
    
    /* Check for "HEAD" magic */
    device->cart_inserted = (memcmp(buf, MIG_XCI_MAGIC, 4) == 0);
    return device->cart_inserted;
}

/*============================================================================
 * AUTHENTICATION
 *============================================================================*/

mig_error_t mig_authenticate(mig_device_t *device) {
    if (!device || !device->opened) {
        return MIG_ERROR_INVALID;
    }
    
    if (!mig_cart_inserted(device)) {
        return MIG_ERROR_NO_CART;
    }
    
    /* Read full XCI header */
    uint8_t header_buf[MIG_XCI_HEADER_SIZE];
    int64_t n = mig_read_raw(device, MIG_XCI_HEADER_OFFSET, 
                             header_buf, MIG_XCI_HEADER_SIZE);
    if (n != MIG_XCI_HEADER_SIZE) {
        return MIG_ERROR_READ;
    }
    
    /* Parse header */
    memcpy(&device->xci_header, header_buf, sizeof(mig_xci_header_t));
    
    /* Verify magic */
    if (memcmp(device->xci_header.magic, MIG_XCI_MAGIC, 4) != 0) {
        return MIG_ERROR_INVALID;
    }
    
    device->xci_header_valid = true;
    device->authenticated = true;
    
    /* Calculate sizes */
    device->cart_total_size = mig_cart_size_bytes(device->xci_header.game_card_size);
    device->cart_trimmed_size = device->xci_header.valid_data_end * MIG_SECTOR_SIZE;
    
    return MIG_OK;
}

bool mig_cart_authenticated(mig_device_t *device) {
    return device && device->authenticated;
}

/*============================================================================
 * CARTRIDGE INFO
 *============================================================================*/

mig_error_t mig_get_cart_info(mig_device_t *device, mig_cart_info_t *info) {
    if (!device || !info) {
        return MIG_ERROR_INVALID;
    }
    
    memset(info, 0, sizeof(mig_cart_info_t));
    
    info->inserted = mig_cart_inserted(device);
    if (!info->inserted) {
        return MIG_OK;
    }
    
    if (!device->authenticated) {
        mig_error_t err = mig_authenticate(device);
        if (err != MIG_OK) {
            return err;
        }
    }
    
    info->authenticated = device->authenticated;
    info->total_size = device->cart_total_size;
    info->used_size = device->cart_trimmed_size;
    
    /* HFS0 title parsing: scan NCA headers for title metadata */
    /* Requires decrypting NCA header with title key for full title info */
    
    return MIG_OK;
}

mig_error_t mig_get_xci_header(mig_device_t *device, mig_xci_header_t *header) {
    if (!device || !header) {
        return MIG_ERROR_INVALID;
    }
    
    if (!device->authenticated) {
        mig_error_t err = mig_authenticate(device);
        if (err != MIG_OK) {
            return err;
        }
    }
    
    memcpy(header, &device->xci_header, sizeof(mig_xci_header_t));
    return MIG_OK;
}

mig_error_t mig_get_xci_size(mig_device_t *device, 
                             uint64_t *total_size, 
                             uint64_t *trimmed_size) {
    if (!device) {
        return MIG_ERROR_INVALID;
    }
    
    if (!device->authenticated) {
        mig_error_t err = mig_authenticate(device);
        if (err != MIG_OK) {
            return err;
        }
    }
    
    if (total_size) {
        *total_size = device->cart_total_size;
    }
    if (trimmed_size) {
        *trimmed_size = device->cart_trimmed_size;
    }
    
    return MIG_OK;
}

/*============================================================================
 * XCI READING
 *============================================================================*/

int64_t mig_read_xci(mig_device_t *device, 
                     uint64_t offset, 
                     void *buffer, 
                     size_t length) {
    if (!device || !device->opened || !buffer) {
        return -1;
    }
    
    if (!device->authenticated) {
        return -1;
    }
    
    return mig_read_raw(device, MIG_XCI_DATA_OFFSET + offset, buffer, length);
}

mig_error_t mig_dump_xci(mig_device_t *device,
                         const char *filename,
                         bool trimmed,
                         mig_progress_cb progress,
                         void *user_data) {
    if (!device || !device->opened || !filename) {
        return MIG_ERROR_INVALID;
    }
    
    if (!device->authenticated) {
        mig_error_t err = mig_authenticate(device);
        if (err != MIG_OK) {
            return err;
        }
    }
    
    /* Determine dump size */
    uint64_t dump_size = trimmed ? device->cart_trimmed_size : device->cart_total_size;
    if (dump_size == 0) {
        return MIG_ERROR_INVALID;
    }
    
    /* Open output file */
    FILE *f = fopen(filename, "wb");
    if (!f) {
        return MIG_ERROR;
    }
    
    /* Dump in chunks */
    const size_t chunk_size = 1024 * 1024;  /* 1 MB */
    uint8_t *chunk = (uint8_t *)malloc(chunk_size);
    if (!chunk) {
        fclose(f);
        return MIG_ERROR;
    }
    
    uint64_t offset = 0;
    mig_error_t result = MIG_OK;
    
    while (offset < dump_size) {
        /* Calculate read size */
        size_t read_size = chunk_size;
        if (offset + read_size > dump_size) {
            read_size = (size_t)(dump_size - offset);
        }
        
        /* Read chunk */
        int64_t n = mig_read_xci(device, offset, chunk, read_size);
        if (n <= 0) {
            result = MIG_ERROR_READ;
            break;
        }
        
        /* Write to file */
        if (fwrite(chunk, 1, n, f) != (size_t)n) {
            result = MIG_ERROR;
            break;
        }
        
        offset += n;
        
        /* Progress callback */
        if (progress) {
            if (!progress(offset, dump_size, user_data)) {
                result = MIG_ERROR_ABORTED;
                break;
            }
        }
    }
    
    free(chunk);
    fclose(f);
    
    return result;
}

/*============================================================================
 * CERTIFICATE / UID
 *============================================================================*/

mig_error_t mig_read_uid(mig_device_t *device, uint8_t uid[16]) {
    if (!device || !device->opened || !uid) {
        return MIG_ERROR_INVALID;
    }
    
    /* UID is in the XCI header signature area */
    int64_t n = mig_read_raw(device, MIG_XCI_HEADER_OFFSET, uid, 16);
    if (n != 16) {
        return MIG_ERROR_READ;
    }
    
    return MIG_OK;
}

mig_error_t mig_read_certificate(mig_device_t *device, 
                                 void *cert, 
                                 size_t *size) {
    if (!device || !device->opened || !cert || !size) {
        return MIG_ERROR_INVALID;
    }
    
    if (!device->authenticated) {
        return MIG_ERROR_NOT_AUTH;
    }
    
    size_t read_size = *size;
    if (read_size > MIG_XCI_CERT_SIZE) {
        read_size = MIG_XCI_CERT_SIZE;
    }
    
    int64_t n = mig_read_raw(device, MIG_XCI_CERT_OFFSET, cert, read_size);
    if (n <= 0) {
        return MIG_ERROR_READ;
    }
    
    *size = (size_t)n;
    return MIG_OK;
}

/*============================================================================
 * LOW-LEVEL BLOCK I/O
 *============================================================================*/

int64_t mig_read_raw(mig_device_t *device, 
                     uint64_t offset, 
                     void *buffer, 
                     size_t size) {
    if (!device || !device->opened || !buffer) {
        return -1;
    }
    
#ifdef _WIN32
    return mig_read_windows(device, offset, buffer, size);
#else
    return mig_read_unix(device, offset, buffer, size);
#endif
}

int64_t mig_write_raw(mig_device_t *device, 
                      uint64_t offset, 
                      const void *buffer, 
                      size_t size) {
    if (!device || !device->opened || !buffer) {
        return -1;
    }
    
#ifdef _WIN32
    return mig_write_windows(device, offset, buffer, size);
#else
    return mig_write_unix(device, offset, buffer, size);
#endif
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * Get human-readable cart size string
 */
const char* mig_get_cart_size_string(uint8_t size_code) {
    switch (size_code) {
        case MIG_CART_SIZE_1GB:  return "1 GB";
        case MIG_CART_SIZE_2GB:  return "2 GB";
        case MIG_CART_SIZE_4GB:  return "4 GB";
        case MIG_CART_SIZE_8GB:  return "8 GB";
        case MIG_CART_SIZE_16GB: return "16 GB";
        case MIG_CART_SIZE_32GB: return "32 GB";
        default: return "Unknown";
    }
}

/**
 * Get error string
 */
const char* mig_error_string(mig_error_t err) {
    switch (err) {
        case MIG_OK:              return "Success";
        case MIG_ERROR:           return "Generic error";
        case MIG_ERROR_NOT_FOUND: return "Device not found";
        case MIG_ERROR_ACCESS:    return "Access denied (need admin/root)";
        case MIG_ERROR_INVALID:   return "Invalid device or parameter";
        case MIG_ERROR_NO_CART:   return "No cartridge inserted";
        case MIG_ERROR_NOT_AUTH:  return "Authentication required";
        case MIG_ERROR_READ:      return "Read error";
        case MIG_ERROR_WRITE:     return "Write error";
        case MIG_ERROR_TIMEOUT:   return "Operation timeout";
        case MIG_ERROR_ABORTED:   return "Operation aborted";
        default:                  return "Unknown error";
    }
}
