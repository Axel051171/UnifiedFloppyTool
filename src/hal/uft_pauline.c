/**
 * @file uft_pauline.c
 * @brief Pauline Floppy Controller Implementation
 * 
 * @copyright MIT License
 */

#include "uft/hal/uft_pauline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int pauline_send(uft_pauline_device_t *dev, const uint8_t *data, size_t len) {
    if (!dev || !dev->connected) return -1;
    
#ifdef _WIN32
    int fd = dev->conn_type == UFT_PAULINE_CONN_TCP ? dev->socket_fd : dev->serial_fd;
    return send(fd, (const char *)data, (int)len, 0) == (int)len ? 0 : -1;
#else
    int fd = dev->conn_type == UFT_PAULINE_CONN_TCP ? dev->socket_fd : dev->serial_fd;
    return write(fd, data, len) == (ssize_t)len ? 0 : -1;
#endif
}

static int pauline_recv(uft_pauline_device_t *dev, uint8_t *data, size_t len, int timeout_ms) {
    if (!dev || !dev->connected) return -1;
    
    int fd = dev->conn_type == UFT_PAULINE_CONN_TCP ? dev->socket_fd : dev->serial_fd;
    
#ifdef _WIN32
    /* Set receive timeout */
    DWORD tv = timeout_ms;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    
    int received = recv(fd, (char *)data, (int)len, 0);
    return (received == (int)len) ? 0 : -1;
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    ssize_t received = read(fd, data, len);
    return (received == (ssize_t)len) ? 0 : -1;
#endif
}

static int pauline_command(uft_pauline_device_t *dev, uint8_t cmd, 
                           const uint8_t *params, size_t param_len,
                           uint8_t *response, size_t *resp_len) {
    uint8_t packet[1024];
    size_t pkt_len = 0;
    
    /* Build packet: [LEN_HI][LEN_LO][CMD][PARAMS...] */
    uint16_t total_len = 1 + param_len;
    packet[pkt_len++] = (total_len >> 8) & 0xFF;
    packet[pkt_len++] = total_len & 0xFF;
    packet[pkt_len++] = cmd;
    
    if (params && param_len > 0) {
        memcpy(packet + pkt_len, params, param_len);
        pkt_len += param_len;
    }
    
    /* Send command */
    if (pauline_send(dev, packet, pkt_len) != 0) {
        return -1;
    }
    
    /* Receive response header */
    uint8_t resp_hdr[3];
    if (pauline_recv(dev, resp_hdr, 3, 5000) != 0) {
        return -2;
    }
    
    uint16_t resp_total = (resp_hdr[0] << 8) | resp_hdr[1];
    uint8_t status = resp_hdr[2];
    
    /* Receive response data */
    if (resp_total > 1 && response && resp_len) {
        size_t data_len = resp_total - 1;
        if (data_len > *resp_len) data_len = *resp_len;
        
        if (pauline_recv(dev, response, data_len, 5000) != 0) {
            return -3;
        }
        *resp_len = data_len;
    } else if (resp_len) {
        *resp_len = 0;
    }
    
    return (status == UFT_PAULINE_STATUS_OK) ? 0 : status;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pauline_connect_tcp(uft_pauline_device_t *dev, 
                            const char *host, uint16_t port) {
    if (!dev || !host) return -1;
    
    memset(dev, 0, sizeof(*dev));
    dev->conn_type = UFT_PAULINE_CONN_TCP;
    strncpy(dev->host, host, sizeof(dev->host) - 1);
    dev->port = port ? port : UFT_PAULINE_DEFAULT_PORT;
    
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return -2;
    }
#endif
    
    /* Resolve hostname */
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", dev->port);
    
    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        return -3;
    }
    
    /* Create socket */
    dev->socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (dev->socket_fd < 0) {
        freeaddrinfo(result);
        return -4;
    }
    
    /* Connect */
    if (connect(dev->socket_fd, result->ai_addr, (int)result->ai_addrlen) < 0) {
        freeaddrinfo(result);
#ifdef _WIN32
        closesocket(dev->socket_fd);
#else
        close(dev->socket_fd);
#endif
        return -5;
    }
    
    freeaddrinfo(result);
    
    /* Allocate receive buffer */
    dev->rx_buffer_size = UFT_PAULINE_MAX_BUFFER;
    dev->rx_buffer = (uint8_t *)malloc(dev->rx_buffer_size);
    if (!dev->rx_buffer) {
#ifdef _WIN32
        closesocket(dev->socket_fd);
#else
        close(dev->socket_fd);
#endif
        return -6;
    }
    
    dev->connected = true;
    
    /* Get device info */
    uft_pauline_get_info(dev, &dev->info);
    
    return 0;
}

int uft_pauline_connect_usb(uft_pauline_device_t *dev, const char *port) {
    if (!dev || !port) return -1;
    
    memset(dev, 0, sizeof(*dev));
    dev->conn_type = UFT_PAULINE_CONN_USB;
    strncpy(dev->serial_port, port, sizeof(dev->serial_port) - 1);
    
#ifdef _WIN32
    /* Windows serial port */
    HANDLE hSerial = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        return -2;
    }
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(hSerial, &dcb);
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(hSerial, &dcb);
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 5000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 5000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hSerial, &timeouts);
    
    dev->serial_fd = (int)(intptr_t)hSerial;
#else
    /* POSIX serial port */
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return -2;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -3;
    }
    
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= (CLOCAL | CREAD);
    
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 50;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -4;
    }
    
    dev->serial_fd = fd;
#endif
    
    /* Allocate receive buffer */
    dev->rx_buffer_size = UFT_PAULINE_MAX_BUFFER;
    dev->rx_buffer = (uint8_t *)malloc(dev->rx_buffer_size);
    if (!dev->rx_buffer) {
#ifdef _WIN32
        CloseHandle((HANDLE)(intptr_t)dev->serial_fd);
#else
        close(dev->serial_fd);
#endif
        return -5;
    }
    
    dev->connected = true;
    
    /* Get device info */
    uft_pauline_get_info(dev, &dev->info);
    
    return 0;
}

void uft_pauline_disconnect(uft_pauline_device_t *dev) {
    if (!dev) return;
    
    if (dev->connected) {
        /* Turn off motor before disconnecting */
        uft_pauline_motor_off(dev);
        
        if (dev->conn_type == UFT_PAULINE_CONN_TCP) {
#ifdef _WIN32
            closesocket(dev->socket_fd);
            WSACleanup();
#else
            close(dev->socket_fd);
#endif
        } else {
#ifdef _WIN32
            CloseHandle((HANDLE)(intptr_t)dev->serial_fd);
#else
            close(dev->serial_fd);
#endif
        }
    }
    
    if (dev->rx_buffer) {
        free(dev->rx_buffer);
    }
    
    memset(dev, 0, sizeof(*dev));
}

bool uft_pauline_is_connected(const uft_pauline_device_t *dev) {
    return dev && dev->connected;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Information Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pauline_get_info(uft_pauline_device_t *dev, uft_pauline_info_t *info) {
    if (!dev || !info) return -1;
    
    uint8_t response[256];
    size_t resp_len = sizeof(response);
    
    int ret = pauline_command(dev, UFT_PAULINE_CMD_GET_INFO, NULL, 0, response, &resp_len);
    if (ret != 0) return ret;
    
    if (resp_len >= 64) {
        memcpy(info->firmware_version, response, 32);
        info->firmware_version[31] = '\0';
        memcpy(info->hardware_version, response + 32, 32);
        info->hardware_version[31] = '\0';
    }
    
    if (resp_len >= 76) {
        info->capabilities = (response[64] << 24) | (response[65] << 16) |
                            (response[66] << 8) | response[67];
        info->max_sample_rate = (response[68] << 24) | (response[69] << 16) |
                               (response[70] << 8) | response[71];
        info->buffer_size = (response[72] << 24) | (response[73] << 16) |
                           (response[74] << 8) | response[75];
    }
    
    return 0;
}

int uft_pauline_get_status(uft_pauline_device_t *dev, uft_pauline_status_t *status) {
    if (!dev || !status) return -1;
    
    uint8_t response[32];
    size_t resp_len = sizeof(response);
    
    int ret = pauline_command(dev, UFT_PAULINE_CMD_GET_STATUS, NULL, 0, response, &resp_len);
    if (ret != 0) return ret;
    
    if (resp_len >= 8) {
        status->connected = dev->connected;
        status->motor_on = (response[0] & 0x01) != 0;
        status->disk_present = (response[0] & 0x02) != 0;
        status->write_protected = (response[0] & 0x04) != 0;
        status->index_detected = (response[0] & 0x08) != 0;
        status->current_track = response[1];
        status->current_head = response[2];
        status->rpm = (response[3] << 8) | response[4];
    }
    
    dev->status = *status;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pauline_select_drive(uft_pauline_device_t *dev, uint8_t drive, uint8_t type) {
    if (!dev) return -1;
    
    uint8_t params[2] = { drive, type };
    return pauline_command(dev, UFT_PAULINE_CMD_SET_DRIVE, params, 2, NULL, NULL);
}

int uft_pauline_motor_on(uft_pauline_device_t *dev) {
    if (!dev) return -1;
    return pauline_command(dev, UFT_PAULINE_CMD_MOTOR_ON, NULL, 0, NULL, NULL);
}

int uft_pauline_motor_off(uft_pauline_device_t *dev) {
    if (!dev) return -1;
    return pauline_command(dev, UFT_PAULINE_CMD_MOTOR_OFF, NULL, 0, NULL, NULL);
}

int uft_pauline_seek(uft_pauline_device_t *dev, uint8_t track) {
    if (!dev) return -1;
    
    uint8_t params[1] = { track };
    return pauline_command(dev, UFT_PAULINE_CMD_SEEK, params, 1, NULL, NULL);
}

int uft_pauline_recalibrate(uft_pauline_device_t *dev) {
    if (!dev) return -1;
    return pauline_command(dev, UFT_PAULINE_CMD_RECALIBRATE, NULL, 0, NULL, NULL);
}

int uft_pauline_select_head(uft_pauline_device_t *dev, uint8_t head) {
    if (!dev) return -1;
    
    uint8_t params[1] = { head };
    return pauline_command(dev, UFT_PAULINE_CMD_SELECT_HEAD, params, 1, NULL, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_pauline_read_params_init(uft_pauline_read_params_t *params) {
    if (!params) return;
    
    params->sample_rate = UFT_PAULINE_SAMPLE_RATE_50MHZ;
    params->revolutions = 3;
    params->index_sync = true;
    params->capture_index = true;
    params->timeout_ms = 10000;
}

int uft_pauline_read_flux(uft_pauline_device_t *dev,
                          const uft_pauline_read_params_t *params,
                          uft_pauline_flux_t *flux) {
    if (!dev || !flux) return -1;
    
    uft_pauline_read_params_t default_params;
    if (!params) {
        uft_pauline_read_params_init(&default_params);
        params = &default_params;
    }
    
    memset(flux, 0, sizeof(*flux));
    
    /* Build read command parameters */
    uint8_t cmd_params[16];
    cmd_params[0] = (params->sample_rate >> 24) & 0xFF;
    cmd_params[1] = (params->sample_rate >> 16) & 0xFF;
    cmd_params[2] = (params->sample_rate >> 8) & 0xFF;
    cmd_params[3] = params->sample_rate & 0xFF;
    cmd_params[4] = params->revolutions;
    cmd_params[5] = (params->index_sync ? 0x01 : 0x00) | 
                    (params->capture_index ? 0x02 : 0x00);
    cmd_params[6] = (params->timeout_ms >> 8) & 0xFF;
    cmd_params[7] = params->timeout_ms & 0xFF;
    
    /* Send read command */
    uint8_t header[16];
    size_t header_len = sizeof(header);
    
    int ret = pauline_command(dev, UFT_PAULINE_CMD_READ_TRACK, 
                              cmd_params, 8, header, &header_len);
    if (ret != 0) return ret;
    
    if (header_len < 12) return -2;
    
    /* Parse response header */
    size_t data_size = (header[0] << 24) | (header[1] << 16) |
                       (header[2] << 8) | header[3];
    size_t index_count = header[4];
    flux->sample_rate = (header[5] << 24) | (header[6] << 16) |
                        (header[7] << 8) | header[8];
    flux->track = header[9];
    flux->head = header[10];
    
    /* Allocate data buffer */
    flux->data = (uint8_t *)malloc(data_size);
    if (!flux->data) return -3;
    
    /* Receive flux data */
    if (pauline_recv(dev, flux->data, data_size, params->timeout_ms) != 0) {
        free(flux->data);
        flux->data = NULL;
        return -4;
    }
    flux->data_size = data_size;
    flux->bit_count = data_size * 8;  /* Adjust based on actual format */
    
    /* Receive index times */
    if (index_count > 0 && params->capture_index) {
        flux->index_times = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        if (flux->index_times) {
            uint8_t *idx_buf = (uint8_t *)malloc(index_count * 4);
            if (idx_buf) {
                if (pauline_recv(dev, idx_buf, index_count * 4, 1000) == 0) {
                    for (size_t i = 0; i < index_count; i++) {
                        flux->index_times[i] = (idx_buf[i*4] << 24) |
                                               (idx_buf[i*4+1] << 16) |
                                               (idx_buf[i*4+2] << 8) |
                                               idx_buf[i*4+3];
                    }
                    flux->index_count = index_count;
                }
                free(idx_buf);
            }
        }
    }
    
    return 0;
}

int uft_pauline_read_track(uft_pauline_device_t *dev,
                           uint8_t track, uint8_t head,
                           const uft_pauline_read_params_t *params,
                           uft_pauline_flux_t *flux) {
    int ret;
    
    ret = uft_pauline_seek(dev, track);
    if (ret != 0) return ret;
    
    ret = uft_pauline_select_head(dev, head);
    if (ret != 0) return ret;
    
    ret = uft_pauline_read_flux(dev, params, flux);
    if (ret != 0) return ret;
    
    flux->track = track;
    flux->head = head;
    
    return 0;
}

void uft_pauline_flux_free(uft_pauline_flux_t *flux) {
    if (!flux) return;
    
    if (flux->data) free(flux->data);
    if (flux->index_times) free(flux->index_times);
    
    memset(flux, 0, sizeof(*flux));
}

int uft_pauline_measure_rpm(uft_pauline_device_t *dev, uint16_t *rpm) {
    if (!dev || !rpm) return -1;
    
    uint8_t response[4];
    size_t resp_len = sizeof(response);
    
    int ret = pauline_command(dev, UFT_PAULINE_CMD_READ_INDEX, NULL, 0, response, &resp_len);
    if (ret != 0) return ret;
    
    if (resp_len >= 2) {
        *rpm = (response[0] << 8) | response[1];
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_pauline_write_params_init(uft_pauline_write_params_t *params) {
    if (!params) return;
    
    params->data_rate = 250000;  /* MFM DD */
    params->precomp_enable = true;
    params->precomp_ns = 140;
    params->verify = true;
}

int uft_pauline_write_flux(uft_pauline_device_t *dev,
                           const uft_pauline_write_params_t *params,
                           const uint8_t *data, size_t data_size) {
    if (!dev || !data || data_size == 0) return -1;
    
    uft_pauline_write_params_t default_params;
    if (!params) {
        uft_pauline_write_params_init(&default_params);
        params = &default_params;
    }
    
    /* Build write command */
    size_t cmd_size = 16 + data_size;
    uint8_t *cmd_buf = (uint8_t *)malloc(cmd_size);
    if (!cmd_buf) return -2;
    
    /* Parameters */
    cmd_buf[0] = (params->data_rate >> 24) & 0xFF;
    cmd_buf[1] = (params->data_rate >> 16) & 0xFF;
    cmd_buf[2] = (params->data_rate >> 8) & 0xFF;
    cmd_buf[3] = params->data_rate & 0xFF;
    cmd_buf[4] = params->precomp_enable ? 0x01 : 0x00;
    cmd_buf[5] = params->precomp_ns;
    cmd_buf[6] = params->verify ? 0x01 : 0x00;
    cmd_buf[7] = 0;
    cmd_buf[8] = (data_size >> 24) & 0xFF;
    cmd_buf[9] = (data_size >> 16) & 0xFF;
    cmd_buf[10] = (data_size >> 8) & 0xFF;
    cmd_buf[11] = data_size & 0xFF;
    
    /* Data */
    memcpy(cmd_buf + 16, data, data_size);
    
    int ret = pauline_command(dev, UFT_PAULINE_CMD_WRITE_TRACK, 
                              cmd_buf, cmd_size, NULL, NULL);
    
    free(cmd_buf);
    return ret;
}

int uft_pauline_write_track(uft_pauline_device_t *dev,
                            uint8_t track, uint8_t head,
                            const uft_pauline_write_params_t *params,
                            const uint8_t *data, size_t data_size) {
    int ret;
    
    ret = uft_pauline_seek(dev, track);
    if (ret != 0) return ret;
    
    ret = uft_pauline_select_head(dev, head);
    if (ret != 0) return ret;
    
    return uft_pauline_write_flux(dev, params, data, data_size);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Conversion Functions (Stubs - need full implementation)
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_pauline_to_hfe(const uft_pauline_flux_t *flux, 
                       uint8_t **hfe_data, size_t *hfe_size) {
    /* TODO: Implement Pauline raw to HFE conversion */
    (void)flux;
    (void)hfe_data;
    (void)hfe_size;
    return -1;
}

int uft_pauline_to_scp(const uft_pauline_flux_t *flux,
                       uint8_t **scp_data, size_t *scp_size) {
    /* TODO: Implement Pauline raw to SCP conversion */
    (void)flux;
    (void)scp_data;
    (void)scp_size;
    return -1;
}

int uft_pauline_to_mfm(const uft_pauline_flux_t *flux,
                       uint8_t **mfm_data, size_t *mfm_bits) {
    /* TODO: Implement Pauline raw to MFM bitstream conversion */
    (void)flux;
    (void)mfm_data;
    (void)mfm_bits;
    return -1;
}
