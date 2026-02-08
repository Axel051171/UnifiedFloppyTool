/**
 * @file uft_mig_dumper.c
 * @brief MIG Dumper Hardware Interface Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-20
 */

#include "uft_mig_dumper.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define MIG_PLATFORM_WINDOWS 1
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
    #include <dirent.h>
    #include <errno.h>
    #define MIG_PLATFORM_POSIX 1
#endif

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

struct uft_mig_device {
#ifdef MIG_PLATFORM_WINDOWS
    HANDLE handle;
#else
    int fd;
#endif
    uft_mig_state_t state;
    uft_mig_device_info_t info;
    bool cart_present;
    bool cart_authenticated;
    bool abort_requested;
};

/* ============================================================================
 * Platform-Specific Serial I/O
 * ============================================================================ */

#ifdef MIG_PLATFORM_WINDOWS

static int serial_open(uft_mig_device_t *dev, const char *port) {
    char full_port[32];
    snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port);
    
    dev->handle = CreateFileA(full_port, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, 0, NULL);
    
    if (dev->handle == INVALID_HANDLE_VALUE) {
        return UFT_MIG_ERR_NO_DEVICE;
    }
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(dev->handle, &dcb);
    dcb.BaudRate = 921600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    SetCommState(dev->handle, &dcb);
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 2000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 2000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(dev->handle, &timeouts);
    
    return UFT_MIG_OK;
}

static void serial_close(uft_mig_device_t *dev) {
    if (dev->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(dev->handle);
        dev->handle = INVALID_HANDLE_VALUE;
    }
}

static int serial_write(uft_mig_device_t *dev, const uint8_t *data, size_t len) {
    DWORD written;
    if (!WriteFile(dev->handle, data, (DWORD)len, &written, NULL)) {
        return UFT_MIG_ERR_USB;
    }
    return (written == len) ? UFT_MIG_OK : UFT_MIG_ERR_USB;
}

static int serial_read(uft_mig_device_t *dev, uint8_t *data, size_t len, size_t *actual) {
    DWORD read_bytes;
    if (!ReadFile(dev->handle, data, (DWORD)len, &read_bytes, NULL)) {
        return UFT_MIG_ERR_USB;
    }
    if (actual) *actual = read_bytes;
    return UFT_MIG_OK;
}

#else /* POSIX */

static int serial_open(uft_mig_device_t *dev, const char *port) {
    dev->fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (dev->fd < 0) {
        return UFT_MIG_ERR_NO_DEVICE;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(dev->fd, &tty) != 0) {
        close(dev->fd);
        dev->fd = -1;
        return UFT_MIG_ERR_USB;
    }
    
    cfsetospeed(&tty, B921600);
    cfsetispeed(&tty, B921600);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 20; /* 2 second timeout */
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    
    if (tcsetattr(dev->fd, TCSANOW, &tty) != 0) {
        close(dev->fd);
        dev->fd = -1;
        return UFT_MIG_ERR_USB;
    }
    
    return UFT_MIG_OK;
}

static void serial_close(uft_mig_device_t *dev) {
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
}

static int serial_write(uft_mig_device_t *dev, const uint8_t *data, size_t len) {
    ssize_t written = write(dev->fd, data, len);
    return (written == (ssize_t)len) ? UFT_MIG_OK : UFT_MIG_ERR_USB;
}

static int serial_read(uft_mig_device_t *dev, uint8_t *data, size_t len, size_t *actual) {
    ssize_t r = read(dev->fd, data, len);
    if (r < 0) return UFT_MIG_ERR_USB;
    if (actual) *actual = r;
    return UFT_MIG_OK;
}

#endif

/* ============================================================================
 * Protocol Helpers
 * ============================================================================ */

static int mig_send_command(uft_mig_device_t *dev, uint8_t cmd, 
                            const uint8_t *payload, size_t payload_len) {
    uint8_t header[4];
    header[0] = 0x55;  /* Sync byte */
    header[1] = cmd;
    header[2] = (uint8_t)(payload_len >> 8);
    header[3] = (uint8_t)(payload_len & 0xFF);
    
    int rc = serial_write(dev, header, 4);
    if (rc != UFT_MIG_OK) return rc;
    
    if (payload && payload_len > 0) {
        rc = serial_write(dev, payload, payload_len);
    }
    
    return rc;
}

static int mig_recv_response(uft_mig_device_t *dev, uint8_t *status,
                             uint8_t *data, size_t max_len, size_t *actual_len) {
    uint8_t header[4];
    size_t received;
    
    int rc = serial_read(dev, header, 4, &received);
    if (rc != UFT_MIG_OK || received != 4) return UFT_MIG_ERR_USB;
    
    if (header[0] != 0xAA) return UFT_MIG_ERR_USB; /* Bad sync */
    
    *status = header[1];
    size_t data_len = ((size_t)header[2] << 8) | header[3];
    
    if (data && data_len > 0) {
        size_t to_read = (data_len < max_len) ? data_len : max_len;
        rc = serial_read(dev, data, to_read, &received);
        if (actual_len) *actual_len = received;
    }
    
    return UFT_MIG_OK;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int uft_mig_enumerate(char **ports, int max_ports) {
    int count = 0;
    
#ifdef MIG_PLATFORM_WINDOWS
    for (int i = 1; i <= 256 && count < max_ports; i++) {
        char port[16];
        snprintf(port, sizeof(port), "COM%d", i);
        
        HANDLE h = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            ports[count] = strdup(port);
            count++;
        }
    }
#else
    DIR *dir = opendir("/dev");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) && count < max_ports) {
            if (strncmp(entry->d_name, "ttyACM", 6) == 0 ||
                strncmp(entry->d_name, "ttyUSB", 6) == 0) {
                char path[64];
                snprintf(path, sizeof(path), "/dev/%s", entry->d_name);
                ports[count] = strdup(path);
                count++;
            }
        }
        closedir(dir);
    }
#endif
    
    return count;
}

int uft_mig_open(const char *port, uft_mig_device_t **device) {
    uft_mig_device_t *dev = calloc(1, sizeof(uft_mig_device_t));
    if (!dev) return UFT_MIG_ERR_NO_DEVICE;
    
#ifdef MIG_PLATFORM_WINDOWS
    dev->handle = INVALID_HANDLE_VALUE;
#else
    dev->fd = -1;
#endif
    dev->state = UFT_MIG_STATE_DISCONNECTED;
    
    /* Auto-detect if no port specified */
    if (!port) {
        char *ports[16];
        int found = uft_mig_enumerate(ports, 16);
        
        for (int i = 0; i < found; i++) {
            if (serial_open(dev, ports[i]) == UFT_MIG_OK) {
                /* Try ping to verify it's a MIG device */
                if (mig_send_command(dev, UFT_MIG_CMD_PING, NULL, 0) == UFT_MIG_OK) {
                    uint8_t status;
                    if (mig_recv_response(dev, &status, NULL, 0, NULL) == UFT_MIG_OK &&
                        status == UFT_MIG_OK) {
                        dev->state = UFT_MIG_STATE_CONNECTED;
                        *device = dev;
                        for (int j = 0; j < found; j++) free(ports[j]);
                        return UFT_MIG_OK;
                    }
                }
                serial_close(dev);
            }
            free(ports[i]);
        }
        
        free(dev);
        return UFT_MIG_ERR_NO_DEVICE;
    }
    
    int rc = serial_open(dev, port);
    if (rc != UFT_MIG_OK) {
        free(dev);
        return rc;
    }
    
    dev->state = UFT_MIG_STATE_CONNECTED;
    *device = dev;
    return UFT_MIG_OK;
}

void uft_mig_close(uft_mig_device_t *device) {
    if (device) {
        serial_close(device);
        free(device);
    }
}

bool uft_mig_is_connected(uft_mig_device_t *device) {
    return device && device->state != UFT_MIG_STATE_DISCONNECTED;
}

int uft_mig_get_info(uft_mig_device_t *device, uft_mig_device_info_t *info) {
    if (!device || !info) return UFT_MIG_ERR_NO_DEVICE;
    
    int rc = mig_send_command(device, UFT_MIG_CMD_GET_INFO, NULL, 0);
    if (rc != UFT_MIG_OK) return rc;
    
    uint8_t status;
    uint8_t data[128];
    size_t len;
    
    rc = mig_recv_response(device, &status, data, sizeof(data), &len);
    if (rc != UFT_MIG_OK) return rc;
    if (status != UFT_MIG_OK) return status;
    
    /* Parse response */
    memset(info, 0, sizeof(*info));
    if (len >= 64) {
        memcpy(info->firmware_version, data, 32);
        memcpy(info->serial_number, data + 32, 32);
    }
    if (len >= 68) {
        info->usb_vid = (data[64] << 8) | data[65];
        info->usb_pid = (data[66] << 8) | data[67];
    }
    if (len >= 70) {
        info->cart_inserted = data[68];
        info->cart_authenticated = data[69];
    }
    
    device->cart_present = info->cart_inserted;
    device->cart_authenticated = info->cart_authenticated;
    memcpy(&device->info, info, sizeof(*info));
    
    return UFT_MIG_OK;
}

bool uft_mig_cart_present(uft_mig_device_t *device) {
    if (!device) return false;
    
    int rc = mig_send_command(device, UFT_MIG_CMD_GET_CART, NULL, 0);
    if (rc != UFT_MIG_OK) return false;
    
    uint8_t status;
    uint8_t data[4];
    size_t len;
    
    rc = mig_recv_response(device, &status, data, sizeof(data), &len);
    if (rc != UFT_MIG_OK) return false;
    
    device->cart_present = (status == UFT_MIG_OK && len >= 1 && data[0]);
    return device->cart_present;
}

int uft_mig_auth_cart(uft_mig_device_t *device) {
    if (!device) return UFT_MIG_ERR_NO_DEVICE;
    if (!device->cart_present) return UFT_MIG_ERR_NO_CART;
    
    int rc = mig_send_command(device, UFT_MIG_CMD_AUTH_CART, NULL, 0);
    if (rc != UFT_MIG_OK) return rc;
    
    uint8_t status;
    rc = mig_recv_response(device, &status, NULL, 0, NULL);
    if (rc != UFT_MIG_OK) return rc;
    
    if (status == UFT_MIG_OK) {
        device->cart_authenticated = true;
    }
    
    return status;
}

int uft_mig_get_xci_info(uft_mig_device_t *device, uft_xci_info_t *info) {
    if (!device || !info) return UFT_MIG_ERR_NO_DEVICE;
    if (!device->cart_present) return UFT_MIG_ERR_NO_CART;
    
    /* Read XCI header (first 0x200 bytes) */
    uint8_t params[8] = {0, 0, 0, 0, 0, 0, 0x02, 0x00}; /* offset=0, len=0x200 */
    
    int rc = mig_send_command(device, UFT_MIG_CMD_READ_XCI, params, sizeof(params));
    if (rc != UFT_MIG_OK) return rc;
    
    uint8_t status;
    uint8_t header[0x200];
    size_t len;
    
    rc = mig_recv_response(device, &status, header, sizeof(header), &len);
    if (rc != UFT_MIG_OK) return rc;
    if (status != UFT_MIG_OK) return status;
    if (len < 0x200) return UFT_MIG_ERR_READ;
    
    /* Parse header */
    memset(info, 0, sizeof(*info));
    
    /* Check magic */
    if (memcmp(header + 0x100, "HEAD", 4) != 0) {
        return UFT_MIG_ERR_READ;
    }
    
    info->cart_type = header[0x10D];
    info->size_bytes = uft_mig_cart_size_bytes(info->cart_type);
    
    /* Header decryption requires console-specific keys (not distributed) */
    snprintf(info->title_name, sizeof(info->title_name), "Switch Game");
    
    return UFT_MIG_OK;
}

int uft_mig_dump_xci(uft_mig_device_t *device,
                     const char *output_path,
                     bool trim,
                     uft_mig_progress_cb progress_cb,
                     uft_mig_error_cb error_cb,
                     void *user_data) {
    if (!device) return UFT_MIG_ERR_NO_DEVICE;
    if (!device->cart_present) return UFT_MIG_ERR_NO_CART;
    if (!device->cart_authenticated) return UFT_MIG_ERR_AUTH_FAIL;
    
    device->state = UFT_MIG_STATE_DUMPING;
    device->abort_requested = false;
    
    FILE *fp = fopen(output_path, "wb");
    if (!fp) {
        device->state = UFT_MIG_STATE_ERROR;
        return UFT_MIG_ERR_READ;
    }
    
    /* Get cart size */
    uft_xci_info_t xci_info;
    int rc = uft_mig_get_xci_info(device, &xci_info);
    if (rc != UFT_MIG_OK) {
        fclose(fp);
        device->state = UFT_MIG_STATE_ERROR;
        return rc;
    }
    
    uint64_t total_bytes = xci_info.size_bytes;
    uint64_t bytes_read = 0;
    uint32_t sector_size = 0x8000; /* 32KB sectors */
    uint32_t total_sectors = (uint32_t)((total_bytes + sector_size - 1) / sector_size);
    uint8_t *buffer = malloc(sector_size);
    
    if (!buffer) {
        fclose(fp);
        device->state = UFT_MIG_STATE_ERROR;
        return UFT_MIG_ERR_READ;
    }
    
    uft_mig_dump_progress_t progress = {0};
    progress.bytes_total = total_bytes;
    progress.total_sectors = total_sectors;
    
    clock_t start_time = clock();
    
    for (uint32_t sector = 0; sector < total_sectors && !device->abort_requested; sector++) {
        uint64_t offset = (uint64_t)sector * sector_size;
        
        /* Build read command */
        uint8_t params[8];
        params[0] = (uint8_t)(offset >> 56);
        params[1] = (uint8_t)(offset >> 48);
        params[2] = (uint8_t)(offset >> 40);
        params[3] = (uint8_t)(offset >> 32);
        params[4] = (uint8_t)(offset >> 24);
        params[5] = (uint8_t)(offset >> 16);
        params[6] = (uint8_t)(offset >> 8);
        params[7] = (uint8_t)(offset);
        
        rc = mig_send_command(device, UFT_MIG_CMD_READ_XCI, params, sizeof(params));
        if (rc != UFT_MIG_OK) {
            progress.read_errors++;
            if (error_cb) {
                error_cb(rc, "Read command failed", user_data);
            }
            continue;
        }
        
        uint8_t status;
        size_t len;
        rc = mig_recv_response(device, &status, buffer, sector_size, &len);
        
        if (rc != UFT_MIG_OK || status != UFT_MIG_OK) {
            progress.read_errors++;
            if (error_cb) {
                error_cb(rc, "Read response failed", user_data);
            }
            memset(buffer, 0, sector_size);
            len = sector_size;
        }
        
        fwrite(buffer, 1, len, fp);
        bytes_read += len;
        
        /* Update progress */
        progress.bytes_dumped = bytes_read;
        progress.current_sector = sector + 1;
        progress.progress_percent = (uint8_t)((bytes_read * 100) / total_bytes);
        
        clock_t now = clock();
        double elapsed = (double)(now - start_time) / CLOCKS_PER_SEC;
        if (elapsed > 0) {
            progress.speed_mbps = (float)(bytes_read / (1024.0 * 1024.0)) / (float)elapsed;
        }
        
        if (progress_cb) {
            progress_cb(&progress, user_data);
        }
    }
    
    free(buffer);
    fclose(fp);
    
    if (device->abort_requested) {
        device->state = UFT_MIG_STATE_IDLE;
        return UFT_MIG_ERR_ABORTED;
    }
    
    device->state = UFT_MIG_STATE_IDLE;
    return UFT_MIG_OK;
}

int uft_mig_dump_cert(uft_mig_device_t *device, const char *output_path) {
    if (!device) return UFT_MIG_ERR_NO_DEVICE;
    if (!device->cart_present) return UFT_MIG_ERR_NO_CART;
    
    int rc = mig_send_command(device, UFT_MIG_CMD_READ_CERT, NULL, 0);
    if (rc != UFT_MIG_OK) return rc;
    
    uint8_t status;
    uint8_t cert[0x200];
    size_t len;
    
    rc = mig_recv_response(device, &status, cert, sizeof(cert), &len);
    if (rc != UFT_MIG_OK) return rc;
    if (status != UFT_MIG_OK) return status;
    
    FILE *fp = fopen(output_path, "wb");
    if (!fp) return UFT_MIG_ERR_READ;
    
    fwrite(cert, 1, len, fp);
    fclose(fp);
    
    return UFT_MIG_OK;
}

int uft_mig_dump_uid(uft_mig_device_t *device, const char *output_path) {
    if (!device) return UFT_MIG_ERR_NO_DEVICE;
    if (!device->cart_present) return UFT_MIG_ERR_NO_CART;
    
    int rc = mig_send_command(device, UFT_MIG_CMD_READ_UID, NULL, 0);
    if (rc != UFT_MIG_OK) return rc;
    
    uint8_t status;
    uint8_t uid[0x40];
    size_t len;
    
    rc = mig_recv_response(device, &status, uid, sizeof(uid), &len);
    if (rc != UFT_MIG_OK) return rc;
    if (status != UFT_MIG_OK) return status;
    
    FILE *fp = fopen(output_path, "wb");
    if (!fp) return UFT_MIG_ERR_READ;
    
    fwrite(uid, 1, len, fp);
    fclose(fp);
    
    return UFT_MIG_OK;
}

int uft_mig_abort(uft_mig_device_t *device) {
    if (!device) return UFT_MIG_ERR_NO_DEVICE;
    device->abort_requested = true;
    return mig_send_command(device, UFT_MIG_CMD_ABORT, NULL, 0);
}

const char *uft_mig_strerror(int status) {
    switch (status) {
        case UFT_MIG_OK:           return "OK";
        case UFT_MIG_ERR_NO_DEVICE: return "No device found";
        case UFT_MIG_ERR_NO_CART:   return "No cartridge inserted";
        case UFT_MIG_ERR_AUTH_FAIL: return "Authentication failed";
        case UFT_MIG_ERR_READ:      return "Read error";
        case UFT_MIG_ERR_USB:       return "USB communication error";
        case UFT_MIG_ERR_TIMEOUT:   return "Timeout";
        case UFT_MIG_ERR_ABORTED:   return "Operation aborted";
        default:                    return "Unknown error";
    }
}

uint64_t uft_mig_cart_size_bytes(uint8_t cart_type) {
    switch (cart_type) {
        case UFT_XCI_SIZE_1GB:  return 1ULL * 1024 * 1024 * 1024;
        case UFT_XCI_SIZE_2GB:  return 2ULL * 1024 * 1024 * 1024;
        case UFT_XCI_SIZE_4GB:  return 4ULL * 1024 * 1024 * 1024;
        case UFT_XCI_SIZE_8GB:  return 8ULL * 1024 * 1024 * 1024;
        case UFT_XCI_SIZE_16GB: return 16ULL * 1024 * 1024 * 1024;
        case UFT_XCI_SIZE_32GB: return 32ULL * 1024 * 1024 * 1024;
        default:                return 0;
    }
}

void uft_mig_format_title_id(const uint8_t *title_id, char *buffer) {
    snprintf(buffer, 17, "%02X%02X%02X%02X%02X%02X%02X%02X",
             title_id[0], title_id[1], title_id[2], title_id[3],
             title_id[4], title_id[5], title_id[6], title_id[7]);
}
