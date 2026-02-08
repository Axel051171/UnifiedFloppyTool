/**
 * @file uft_cart7.c
 * @brief 7-in-1 Cartridge Reader HAL Provider Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-20
 */

#include "uft/cart7/uft_cart7.h"
#include "uft/cart7/cart7_protocol.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <sys/ioctl.h>
#endif

/*============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

struct uft_cart7_device {
#ifdef _WIN32
    HANDLE      handle;
#else
    int         fd;
#endif
    char        port[64];
    bool        connected;
    bool        abort_flag;
    
    /* Cached state */
    cart7_slot_t current_slot;
    uft_cart7_device_info_t info;
    uft_cart7_cart_info_t   cart_info;
    
    /* Frame buffer */
    uint8_t     tx_buf[CART7_MAX_PAYLOAD + 8];
    uint8_t     rx_buf[CART7_MAX_PAYLOAD + 8];
};

/*============================================================================
 * CRC-8 CALCULATION
 *============================================================================*/

static uint8_t crc8_table[256];
static bool crc8_table_init = false;

static void init_crc8_table(void) {
    if (crc8_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint8_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
        }
        crc8_table[i] = crc;
    }
    crc8_table_init = true;
}

static uint8_t calc_crc8(const uint8_t *data, size_t len) {
    init_crc8_table();
    uint8_t crc = 0x00;
    while (len--) {
        crc = crc8_table[crc ^ *data++];
    }
    return crc;
}

/*============================================================================
 * PLATFORM-SPECIFIC SERIAL I/O
 *============================================================================*/

#ifdef _WIN32

static int serial_open(uft_cart7_device_t *dev, const char *port) {
    char path[128];
    snprintf(path, sizeof(path), "\\\\.\\%s", port);
    
    dev->handle = CreateFileA(path,
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);
    
    if (dev->handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    /* Configure port */
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(dev->handle, &dcb);
    
    dcb.BaudRate = CART7_USB_BAUDRATE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    
    SetCommState(dev->handle, &dcb);
    
    /* Timeouts */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 5000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 5000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(dev->handle, &timeouts);
    
    return 0;
}

static void serial_close(uft_cart7_device_t *dev) {
    if (dev->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(dev->handle);
        dev->handle = INVALID_HANDLE_VALUE;
    }
}

static int serial_write(uft_cart7_device_t *dev, const uint8_t *data, size_t len) {
    DWORD written;
    if (!WriteFile(dev->handle, data, (DWORD)len, &written, NULL)) {
        return -1;
    }
    return (int)written;
}

static int serial_read(uft_cart7_device_t *dev, uint8_t *data, size_t len) {
    DWORD read;
    if (!ReadFile(dev->handle, data, (DWORD)len, &read, NULL)) {
        return -1;
    }
    return (int)read;
}

#else /* Unix/Linux/macOS */

static int serial_open(uft_cart7_device_t *dev, const char *port) {
    dev->fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (dev->fd < 0) {
        return -1;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(dev->fd, &tty) != 0) {
        close(dev->fd);
        return -1;
    }
    
    cfsetospeed(&tty, B921600);
    cfsetispeed(&tty, B921600);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= (CLOCAL | CREAD);
    
    tty.c_iflag &= ~(IGNBRK | IXON | IXOFF | IXANY);
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 50;  /* 5 second timeout */
    
    tcsetattr(dev->fd, TCSANOW, &tty);
    
    /* Set DTR */
    int status;
    ioctl(dev->fd, TIOCMGET, &status);
    status |= TIOCM_DTR | TIOCM_RTS;
    ioctl(dev->fd, TIOCMSET, &status);
    
    return 0;
}

static void serial_close(uft_cart7_device_t *dev) {
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
}

static int serial_write(uft_cart7_device_t *dev, const uint8_t *data, size_t len) {
    return write(dev->fd, data, len);
}

static int serial_read(uft_cart7_device_t *dev, uint8_t *data, size_t len) {
    return read(dev->fd, data, len);
}

#endif

/*============================================================================
 * FRAME SEND/RECEIVE
 *============================================================================*/

static uft_cart7_error_t send_command(uft_cart7_device_t *dev, 
                                      uint8_t cmd,
                                      const uint8_t *payload,
                                      uint16_t payload_len) {
    /* Build frame */
    dev->tx_buf[0] = CART7_SYNC_COMMAND;
    dev->tx_buf[1] = cmd;
    dev->tx_buf[2] = payload_len & 0xFF;
    dev->tx_buf[3] = (payload_len >> 8) & 0xFF;
    
    if (payload && payload_len > 0) {
        memcpy(&dev->tx_buf[4], payload, payload_len);
    }
    
    /* Calculate CRC */
    size_t frame_len = 4 + payload_len;
    dev->tx_buf[frame_len] = calc_crc8(dev->tx_buf, frame_len);
    frame_len++;
    
    /* Send */
    int written = serial_write(dev, dev->tx_buf, frame_len);
    if (written != (int)frame_len) {
        return CART7_ERROR_WRITE;
    }
    
    return CART7_OK;
}

static uft_cart7_error_t receive_response(uft_cart7_device_t *dev,
                                          uint8_t expected_cmd,
                                          uint8_t *status_out,
                                          uint8_t *data_out,
                                          uint16_t *data_len) {
    /* Read header (6 bytes minimum) */
    int n = serial_read(dev, dev->rx_buf, 6);
    if (n < 6) {
        return CART7_ERROR_TIMEOUT;
    }
    
    /* Validate sync */
    if (dev->rx_buf[0] != CART7_SYNC_RESPONSE) {
        return CART7_ERROR;
    }
    
    uint8_t status = dev->rx_buf[1];
    uint8_t cmd_echo = dev->rx_buf[2];
    uint16_t len = dev->rx_buf[3] | (dev->rx_buf[4] << 8);
    
    /* Verify command echo */
    if (cmd_echo != expected_cmd) {
        return CART7_ERROR;
    }
    
    /* Read data + CRC */
    if (len > 0) {
        n = serial_read(dev, &dev->rx_buf[5], len + 1);
        if (n < (int)(len + 1)) {
            return CART7_ERROR_TIMEOUT;
        }
    } else {
        n = serial_read(dev, &dev->rx_buf[5], 1);
        if (n < 1) {
            return CART7_ERROR_TIMEOUT;
        }
    }
    
    /* Verify CRC */
    uint8_t crc = calc_crc8(dev->rx_buf, 5 + len);
    if (crc != dev->rx_buf[5 + len]) {
        return CART7_ERROR_CRC;
    }
    
    /* Output */
    if (status_out) *status_out = status;
    if (data_out && len > 0) memcpy(data_out, &dev->rx_buf[5], len);
    if (data_len) *data_len = len;
    
    /* Convert protocol status to error */
    if (status == CART7_STATUS_NO_CART) return CART7_ERROR_NO_CART;
    if (status == CART7_STATUS_WRONG_SLOT) return CART7_ERROR_WRONG_SLOT;
    if (status == CART7_STATUS_READ_ERROR) return CART7_ERROR_READ;
    if (status >= 0x10 && status != CART7_STATUS_OK) return CART7_ERROR;
    
    return CART7_OK;
}

/*============================================================================
 * DEVICE ENUMERATION
 *============================================================================*/

int uft_cart7_enumerate(char **ports, int max_ports) {
    int count = 0;
    
#ifdef _WIN32
    /* Scan COM ports */
    for (int i = 1; i <= 256 && count < max_ports; i++) {
        char name[16];
        snprintf(name, sizeof(name), "COM%d", i);
        
        char path[32];
        snprintf(path, sizeof(path), "\\\\.\\%s", name);
        
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            
            if (ports) {
                ports[count] = strdup(name);
            }
            count++;
        }
    }
#else
    /* Scan /dev for ttyUSB* and ttyACM* */
    DIR *dir = opendir("/dev");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL && count < max_ports) {
            if (strncmp(ent->d_name, "ttyUSB", 6) == 0 ||
                strncmp(ent->d_name, "ttyACM", 6) == 0) {
                
                char path[64];
                snprintf(path, sizeof(path), "/dev/%s", ent->d_name);
                
                if (ports) {
                    ports[count] = strdup(path);
                }
                count++;
            }
        }
        closedir(dir);
    }
    
#ifdef __APPLE__
    /* Also scan /dev/cu.* on macOS */
    dir = opendir("/dev");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL && count < max_ports) {
            if (strncmp(ent->d_name, "cu.usb", 6) == 0) {
                char path[64];
                snprintf(path, sizeof(path), "/dev/%s", ent->d_name);
                
                if (ports) {
                    ports[count] = strdup(path);
                }
                count++;
            }
        }
        closedir(dir);
    }
#endif
#endif
    
    return count;
}

/*============================================================================
 * CONNECTION
 *============================================================================*/

uft_cart7_error_t uft_cart7_open(const char *port, uft_cart7_device_t **device) {
    uft_cart7_device_t *dev = calloc(1, sizeof(uft_cart7_device_t));
    if (!dev) return CART7_ERROR;
    
#ifdef _WIN32
    dev->handle = INVALID_HANDLE_VALUE;
#else
    dev->fd = -1;
#endif
    
    strncpy(dev->port, port, sizeof(dev->port) - 1);
    
    /* Open port */
    if (serial_open(dev, port) != 0) {
        free(dev);
        return CART7_ERROR_NOT_FOUND;
    }
    
    /* Small delay for device ready */
#ifdef _WIN32
    Sleep(100);
#else
    usleep(100000);
#endif
    
    /* Send PING */
    uft_cart7_error_t err = send_command(dev, CART7_CMD_PING, NULL, 0);
    if (err != CART7_OK) {
        serial_close(dev);
        free(dev);
        return err;
    }
    
    /* Receive response */
    uint8_t data[8];
    uint16_t len;
    err = receive_response(dev, CART7_CMD_PING, NULL, data, &len);
    if (err != CART7_OK) {
        serial_close(dev);
        free(dev);
        return err;
    }
    
    /* Verify magic */
    if (len < 4 || memcmp(&data[1], "C7", 2) != 0) {
        serial_close(dev);
        free(dev);
        return CART7_ERROR;
    }
    
    dev->connected = true;
    *device = dev;
    
    /* Get device info */
    uft_cart7_get_info(dev, &dev->info);
    
    return CART7_OK;
}

void uft_cart7_close(uft_cart7_device_t *device) {
    if (!device) return;
    
    serial_close(device);
    device->connected = false;
    free(device);
}

bool uft_cart7_is_connected(uft_cart7_device_t *device) {
    return device && device->connected;
}

uft_cart7_error_t uft_cart7_get_info(uft_cart7_device_t *device, 
                                     uft_cart7_device_info_t *info) {
    if (!device || !device->connected) return CART7_ERROR;
    
    uft_cart7_error_t err = send_command(device, CART7_CMD_GET_INFO, NULL, 0);
    if (err != CART7_OK) return err;
    
    uint8_t data[64];
    uint16_t len;
    err = receive_response(device, CART7_CMD_GET_INFO, NULL, data, &len);
    if (err != CART7_OK) return err;
    
    if (len >= 48) {
        strncpy(info->port, device->port, sizeof(info->port) - 1);
        info->hw_revision = data[1];
        memcpy(info->firmware_version, &data[2], 16);
        info->firmware_version[16] = '\0';
        memcpy(info->serial, &data[18], 16);
        info->serial[16] = '\0';
        memcpy(&info->features, &data[42], 4);
    }
    
    return CART7_OK;
}

/*============================================================================
 * SLOT SELECTION
 *============================================================================*/

uft_cart7_error_t uft_cart7_select_slot(uft_cart7_device_t *device, 
                                        cart7_slot_t slot) {
    if (!device || !device->connected) return CART7_ERROR;
    
    uint8_t payload[4] = {slot, 0, 0, 0};
    
    /* Set voltage based on system */
    switch (slot) {
        case SLOT_NES:
        case SLOT_SNES:
        case SLOT_N64:
        case SLOT_MD:
        case SLOT_FC:
        case SLOT_SFC:
            payload[1] = 5;  /* 5V */
            break;
        case SLOT_GBA:
        case SLOT_GB:
            payload[1] = 3;  /* 3.3V */
            break;
        default:
            break;
    }
    
    uft_cart7_error_t err = send_command(device, CART7_CMD_SELECT_SLOT, payload, 4);
    if (err != CART7_OK) return err;
    
    uint8_t status;
    err = receive_response(device, CART7_CMD_SELECT_SLOT, &status, NULL, NULL);
    if (err != CART7_OK) return err;
    
    device->current_slot = slot;
    return CART7_OK;
}

cart7_slot_t uft_cart7_get_current_slot(uft_cart7_device_t *device) {
    if (!device) return SLOT_NONE;
    return device->current_slot;
}

bool uft_cart7_cart_inserted(uft_cart7_device_t *device) {
    if (!device || !device->connected) return false;
    
    uft_cart7_error_t err = send_command(device, CART7_CMD_GET_CART_STATUS, NULL, 0);
    if (err != CART7_OK) return false;
    
    uint8_t data[8];
    uint16_t len;
    err = receive_response(device, CART7_CMD_GET_CART_STATUS, NULL, data, &len);
    if (err != CART7_OK) return false;
    
    return (len >= 1 && data[0] != 0);
}

/*============================================================================
 * ROM READING
 *============================================================================*/

int64_t uft_cart7_read_rom(uft_cart7_device_t *device,
                           uint64_t offset,
                           void *buffer,
                           size_t length) {
    if (!device || !device->connected || !buffer) return -1;
    
    /* Determine read command based on slot */
    uint8_t cmd;
    switch (device->current_slot) {
        case SLOT_NES:
        case SLOT_FC:
            cmd = CART7_CMD_NES_READ_PRG;
            break;
        case SLOT_SNES:
        case SLOT_SFC:
            cmd = CART7_CMD_SNES_READ_ROM;
            break;
        case SLOT_N64:
            cmd = CART7_CMD_N64_READ_ROM;
            break;
        case SLOT_MD:
            cmd = CART7_CMD_MD_READ_ROM;
            break;
        case SLOT_GBA:
            cmd = CART7_CMD_GBA_READ_ROM;
            break;
        case SLOT_GB:
            cmd = CART7_CMD_GB_READ_ROM;
            break;
        default:
            return -1;
    }
    
    /* Build read request */
    uint8_t payload[12];
    memcpy(&payload[0], &offset, 4);
    uint32_t len32 = (uint32_t)length;
    memcpy(&payload[4], &len32, 4);
    uint16_t chunk = CART7_STREAM_CHUNK_SIZE;
    memcpy(&payload[8], &chunk, 2);
    payload[10] = 0;  /* flags */
    payload[11] = 0;
    
    uft_cart7_error_t err = send_command(device, cmd, payload, 12);
    if (err != CART7_OK) return -1;
    
    /* Receive streaming data */
    size_t total_read = 0;
    uint8_t *out = (uint8_t *)buffer;
    
    while (total_read < length && !device->abort_flag) {
        uint8_t status;
        uint16_t data_len;
        err = receive_response(device, cmd, &status, out + total_read, &data_len);
        if (err != CART7_OK) break;
        
        total_read += data_len;
        
        if (status == CART7_STATUS_OK_DONE || status == CART7_STATUS_OK) {
            break;
        }
    }
    
    return (int64_t)total_read;
}

uft_cart7_error_t uft_cart7_dump_rom(uft_cart7_device_t *device,
                                     const char *filename,
                                     uft_cart7_progress_cb progress,
                                     void *user_data) {
    if (!device || !device->connected || !filename) return CART7_ERROR;
    
    /* Get cart info to determine size */
    uft_cart7_cart_info_t info;
    uft_cart7_error_t err = uft_cart7_get_cart_info(device, &info);
    if (err != CART7_OK) return err;
    
    /* Determine ROM size based on system */
    uint64_t rom_size = 0;
    switch (device->current_slot) {
        case SLOT_NES:
        case SLOT_FC:
            rom_size = info.nes.prg_size;
            break;
        case SLOT_SNES:
        case SLOT_SFC:
            rom_size = info.snes.rom_size;
            break;
        case SLOT_N64:
            rom_size = info.n64.rom_size;
            break;
        case SLOT_MD:
            rom_size = info.md.rom_size;
            break;
        case SLOT_GBA:
            rom_size = info.gba.rom_size;
            break;
        case SLOT_GB:
            rom_size = info.gb.rom_size;
            break;
        default:
            return CART7_ERROR_UNSUPPORTED;
    }
    
    if (rom_size == 0) return CART7_ERROR;
    
    /* Open output file */
    FILE *f = fopen(filename, "wb");
    if (!f) return CART7_ERROR;
    
    /* Dump in chunks */
    uint8_t chunk[CART7_STREAM_CHUNK_SIZE];
    uint64_t offset = 0;
    device->abort_flag = false;
    
    while (offset < rom_size && !device->abort_flag) {
        size_t read_size = CART7_STREAM_CHUNK_SIZE;
        if (offset + read_size > rom_size) {
            read_size = rom_size - offset;
        }
        
        int64_t n = uft_cart7_read_rom(device, offset, chunk, read_size);
        if (n <= 0) {
            fclose(f);
            return CART7_ERROR_READ;
        }
        
        fwrite(chunk, 1, n, f);
        offset += n;
        
        if (progress) {
            if (!progress(offset, rom_size, user_data)) {
                device->abort_flag = true;
                fclose(f);
                return CART7_ERROR_ABORTED;
            }
        }
    }
    
    fclose(f);
    return CART7_OK;
}

/*============================================================================
 * SAVE DATA
 *============================================================================*/

int64_t uft_cart7_read_save(uft_cart7_device_t *device,
                            uint64_t offset,
                            void *buffer,
                            size_t length) {
    if (!device || !device->connected || !buffer) return -1;
    
    /* Determine read command based on slot */
    uint8_t cmd;
    switch (device->current_slot) {
        case SLOT_NES:
        case SLOT_FC:
            cmd = CART7_CMD_NES_READ_SRAM;
            break;
        case SLOT_SNES:
        case SLOT_SFC:
            cmd = CART7_CMD_SNES_READ_SRAM;
            break;
        case SLOT_N64:
            cmd = CART7_CMD_N64_READ_SAVE;
            break;
        case SLOT_MD:
            cmd = CART7_CMD_MD_READ_SRAM;
            break;
        case SLOT_GBA:
            cmd = CART7_CMD_GBA_READ_SAVE;
            break;
        case SLOT_GB:
            cmd = CART7_CMD_GB_READ_SRAM;
            break;
        default:
            return -1;
    }
    
    /* Build read request */
    uint8_t payload[12];
    memcpy(&payload[0], &offset, 4);
    uint32_t len32 = (uint32_t)length;
    memcpy(&payload[4], &len32, 4);
    uint16_t chunk = 512;  /* Smaller chunks for SRAM */
    memcpy(&payload[8], &chunk, 2);
    payload[10] = 0;
    payload[11] = 0;
    
    uft_cart7_error_t err = send_command(device, cmd, payload, 12);
    if (err != CART7_OK) return -1;
    
    /* Receive data */
    uint8_t status;
    uint16_t data_len;
    err = receive_response(device, cmd, &status, buffer, &data_len);
    if (err != CART7_OK) return -1;
    
    return data_len;
}

int64_t uft_cart7_write_save(uft_cart7_device_t *device,
                             uint64_t offset,
                             const void *buffer,
                             size_t length) {
    if (!device || !device->connected || !buffer) return -1;
    
    /* Determine write command based on slot */
    uint8_t cmd;
    switch (device->current_slot) {
        case SLOT_NES:
        case SLOT_FC:
            cmd = CART7_CMD_NES_WRITE_SRAM;
            break;
        case SLOT_SNES:
        case SLOT_SFC:
            cmd = CART7_CMD_SNES_WRITE_SRAM;
            break;
        case SLOT_N64:
            cmd = CART7_CMD_N64_WRITE_SAVE;
            break;
        case SLOT_MD:
            cmd = CART7_CMD_MD_WRITE_SRAM;
            break;
        case SLOT_GBA:
            cmd = CART7_CMD_GBA_WRITE_SAVE;
            break;
        case SLOT_GB:
            cmd = CART7_CMD_GB_WRITE_SRAM;
            break;
        default:
            return -1;
    }
    
    /* Build write request (offset + data) */
    size_t payload_len = 8 + length;
    uint8_t *payload = malloc(payload_len);
    if (!payload) return -1;
    
    memcpy(&payload[0], &offset, 4);
    uint32_t len32 = (uint32_t)length;
    memcpy(&payload[4], &len32, 4);
    memcpy(&payload[8], buffer, length);
    
    uft_cart7_error_t err = send_command(device, cmd, payload, payload_len);
    free(payload);
    
    if (err != CART7_OK) return -1;
    
    /* Receive response */
    uint8_t status;
    err = receive_response(device, cmd, &status, NULL, NULL);
    if (err != CART7_OK) return -1;
    
    return length;
}

/*============================================================================
 * CARTRIDGE INFO
 *============================================================================*/

uft_cart7_error_t uft_cart7_get_cart_info(uft_cart7_device_t *device,
                                          uft_cart7_cart_info_t *info) {
    if (!device || !device->connected || !info) return CART7_ERROR;
    
    memset(info, 0, sizeof(*info));
    info->slot = device->current_slot;
    
    /* Determine header command */
    uint8_t cmd;
    switch (device->current_slot) {
        case SLOT_NES:
        case SLOT_FC:
            cmd = CART7_CMD_NES_GET_HEADER;
            break;
        case SLOT_SNES:
        case SLOT_SFC:
            cmd = CART7_CMD_SNES_GET_HEADER;
            break;
        case SLOT_N64:
            cmd = CART7_CMD_N64_GET_HEADER;
            break;
        case SLOT_MD:
            cmd = CART7_CMD_MD_GET_HEADER;
            break;
        case SLOT_GBA:
            cmd = CART7_CMD_GBA_GET_HEADER;
            break;
        case SLOT_GB:
            cmd = CART7_CMD_GB_GET_HEADER;
            break;
        default:
            return CART7_ERROR_UNSUPPORTED;
    }
    
    uft_cart7_error_t err = send_command(device, cmd, NULL, 0);
    if (err != CART7_OK) return err;
    
    uint8_t data[256];
    uint16_t len;
    err = receive_response(device, cmd, NULL, data, &len);
    if (err == CART7_ERROR_NO_CART) {
        info->inserted = false;
        return CART7_OK;
    }
    if (err != CART7_OK) return err;
    
    info->inserted = true;
    
    /* Parse based on system */
    switch (device->current_slot) {
        case SLOT_NES:
        case SLOT_FC:
            if (len >= 20) {
                memcpy(&info->nes.prg_size, &data[0], 4);
                memcpy(&info->nes.chr_size, &data[4], 4);
                memcpy(&info->nes.mapper, &data[8], 2);
                info->nes.submapper = data[10];
                info->nes.mirroring = data[11];
                info->nes.has_battery = data[12];
                info->nes.has_trainer = data[13];
                info->nes.prg_ram_size = data[14];
                info->nes.chr_ram_size = data[15];
                info->nes.tv_system = data[16];
            }
            break;
            
        case SLOT_SNES:
        case SLOT_SFC:
            if (len >= 44) {
                memcpy(info->snes.title, data, 21);
                info->snes.title[21] = '\0';
                info->snes.rom_type = data[22];
                info->snes.special_chip = data[23];
                memcpy(&info->snes.rom_size, &data[24], 4);
                memcpy(&info->snes.sram_size, &data[28], 4);
                info->snes.country = data[32];
                info->snes.version = data[34];
                info->snes.has_battery = data[35];
                memcpy(&info->snes.checksum, &data[36], 2);
                memcpy(&info->snes.checksum_comp, &data[38], 2);
            }
            break;
            
        case SLOT_N64:
            if (len >= 71) {
                memcpy(info->n64.title, &data[32], 20);
                info->n64.title[20] = '\0';
                memcpy(info->n64.game_code, &data[59], 4);
                info->n64.game_code[4] = '\0';
                memcpy(&info->n64.crc1, &data[16], 4);
                memcpy(&info->n64.crc2, &data[20], 4);
                info->n64.version = data[63];
                info->n64.cic_type = data[64];
                info->n64.save_type = data[65];
                info->n64.region = data[66];
                memcpy(&info->n64.rom_size, &data[67], 4);
            }
            break;
            
        case SLOT_MD:
            if (len >= 192) {
                memcpy(info->md.console, data, 16);
                info->md.console[16] = '\0';
                memcpy(info->md.title_domestic, &data[32], 48);
                info->md.title_domestic[48] = '\0';
                memcpy(info->md.title_overseas, &data[80], 48);
                info->md.title_overseas[48] = '\0';
                memcpy(info->md.serial, &data[128], 14);
                info->md.serial[14] = '\0';
                memcpy(&info->md.checksum, &data[142], 2);
                memcpy(info->md.region, &data[176], 3);
                info->md.region[3] = '\0';
                memcpy(&info->md.rom_size, &data[180], 4);
                memcpy(&info->md.sram_size, &data[184], 4);
                info->md.has_sram = data[188];
                info->md.sram_type = data[189];
            }
            break;
            
        case SLOT_GBA:
            if (len >= 200) {
                memcpy(info->gba.title, &data[0xA0], 12);
                info->gba.title[12] = '\0';
                memcpy(info->gba.game_code, &data[0xAC], 4);
                info->gba.game_code[4] = '\0';
                memcpy(info->gba.maker_code, &data[0xB0], 2);
                info->gba.maker_code[2] = '\0';
                info->gba.version = data[0xBC];
                memcpy(&info->gba.rom_size, &data[0xC0], 4);
                info->gba.save_type = data[0xC4];
                info->gba.gpio_type = data[0xC5];
                info->gba.logo_valid = data[0xC6];
                info->gba.checksum_valid = data[0xC7];
            }
            break;
            
        case SLOT_GB:
            if (len >= 96) {
                memcpy(info->gb.title, &data[0x34], 16);
                info->gb.title[16] = '\0';
                info->gb.cgb_flag = data[0x43];
                info->gb.sgb_flag = data[0x46];
                info->gb.cart_type = data[0x47];
                info->gb.mbc_type = data[0x58];
                memcpy(&info->gb.rom_size, &data[0x50], 4);
                memcpy(&info->gb.ram_size, &data[0x54], 4);
                info->gb.has_battery = data[0x59];
                info->gb.has_rtc = data[0x5A];
                info->gb.has_rumble = data[0x5B];
                info->gb.is_gbc = data[0x5C];
                info->gb.logo_valid = data[0x5D];
                info->gb.header_checksum = data[0x4D];
                memcpy(&info->gb.global_checksum, &data[0x4E], 2);
            }
            break;
            
        default:
            break;
    }
    
    return CART7_OK;
}

/*============================================================================
 * ABORT
 *============================================================================*/

void uft_cart7_abort(uft_cart7_device_t *device) {
    if (!device) return;
    device->abort_flag = true;
    
    /* Also send abort command */
    send_command(device, CART7_CMD_ABORT, NULL, 0);
}
