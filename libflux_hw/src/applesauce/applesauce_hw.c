// SPDX-License-Identifier: MIT
/*
 * applesauce_hw.c - Applesauce FDC Hardware Support
 * 
 * Professional implementation of Applesauce serial protocol.
 * Enables flux-level reading/writing on Apple II and other platforms.
 * 
 * Supported Devices:
 *   - Applesauce FDC (USB-Serial adapter)
 * 
 * Protocol:
 *   - Serial communication over USB
 *   - Text-based command protocol
 *   - Binary flux data transfer
 *   - A2R format native support
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

/*============================================================================*
 * APPLESAUCE CONSTANTS
 *============================================================================*/

#define APPLESAUCE_ID_STRING    "Applesauce"
#define APPLESAUCE_PROTOCOL_V2  "client:v2"
#define APPLESAUCE_SUCCESS      "."

#define APPLESAUCE_BUFFER_SIZE  8192
#define APPLESAUCE_TIMEOUT      5000  /* 5 seconds */
#define APPLESAUCE_BAUD_RATE    B115200

/* Flux timing */
#define APPLESAUCE_TICK_NS      125   /* 8 MHz = 125ns per tick */

/*============================================================================*
 * DEVICE HANDLE
 *============================================================================*/

typedef struct {
    int serial_fd;              /* Serial port file descriptor */
    char port_path[256];        /* Serial port path */
    bool connected;             /* Drive connection state */
    bool motor_on;              /* Motor state */
    int current_track;          /* Current head position */
    
    /* Protocol */
    char cmd_buffer[1024];      /* Command buffer */
    uint8_t data_buffer[APPLESAUCE_BUFFER_SIZE];  /* Data buffer */
    
    /* Statistics */
    unsigned long bytes_read;
    unsigned long bytes_written;
} applesauce_handle_t;

/*============================================================================*
 * SERIAL PORT COMMUNICATION
 *============================================================================*/

/**
 * @brief Open serial port
 */
static int serial_open(const char *port_path, int *fd_out)
{
    if (!port_path || !fd_out) {
        return -1;
    }
    
    int fd = open(port_path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", port_path, strerror(errno));
        return -1;
    }
    
    /* Configure serial port */
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    /* Set baud rate */
    cfsetospeed(&tty, APPLESAUCE_BAUD_RATE);
    cfsetispeed(&tty, APPLESAUCE_BAUD_RATE);
    
    /* 8N1 */
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= (CLOCAL | CREAD);
    
    /* Raw mode */
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    /* Timeout */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;  /* 1 second timeout */
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    *fd_out = fd;
    return 0;
}

/**
 * @brief Write line to serial port
 */
static int serial_write_line(int fd, const char *line)
{
    size_t len = strlen(line);
    char *buf = malloc(len + 2);
    if (!buf) {
        return -1;
    }
    
    memcpy(buf, line, len);
    buf[len] = '\r';
    buf[len + 1] = '\n';
    
    ssize_t written = write(fd, buf, len + 2);
    free(buf);
    
    return (written == (ssize_t)(len + 2)) ? 0 : -1;
}

/**
 * @brief Read line from serial port
 */
static int serial_read_line(int fd, char *line, size_t max_len)
{
    size_t pos = 0;
    
    while (pos < max_len - 1) {
        uint8_t c;
        ssize_t n = read(fd, &c, 1);
        
        if (n < 0) {
            return -1;
        }
        
        if (n == 0) {
            /* Timeout */
            usleep(10000);  /* 10ms */
            continue;
        }
        
        if (c == '\r' || c == '\n') {
            if (pos > 0) {
                break;
            }
            continue;
        }
        
        line[pos++] = c;
    }
    
    line[pos] = '\0';
    return (int)pos;
}

/**
 * @brief Write byte to serial port
 */
static int serial_write_byte(int fd, uint8_t byte)
{
    ssize_t written = write(fd, &byte, 1);
    return (written == 1) ? 0 : -1;
}

/**
 * @brief Read bytes from serial port
 */
static int serial_read_bytes(int fd, uint8_t *buffer, size_t len)
{
    size_t total_read = 0;
    
    while (total_read < len) {
        ssize_t n = read(fd, buffer + total_read, len - total_read);
        
        if (n < 0) {
            return -1;
        }
        
        if (n == 0) {
            usleep(10000);  /* 10ms */
            continue;
        }
        
        total_read += n;
    }
    
    return (int)total_read;
}

/*============================================================================*
 * APPLESAUCE PROTOCOL
 *============================================================================*/

/**
 * @brief Send command and receive response
 */
static int applesauce_sendrecv(
    applesauce_handle_t *handle,
    const char *command,
    char *response,
    size_t max_len
) {
    if (!handle || !command || !response) {
        return -1;
    }
    
    /* Send command */
    if (serial_write_line(handle->serial_fd, command) < 0) {
        return -1;
    }
    
    /* Receive response */
    if (serial_read_line(handle->serial_fd, response, max_len) < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Execute command (expect "." response)
 */
static int applesauce_do_command(applesauce_handle_t *handle, const char *command)
{
    char response[256];
    
    if (applesauce_sendrecv(handle, command, response, sizeof(response)) < 0) {
        return -1;
    }
    
    if (strcmp(response, APPLESAUCE_SUCCESS) != 0) {
        fprintf(stderr, "Applesauce error: '%s' (command: '%s')\n", 
                response, command);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Execute command and read additional response
 */
static int applesauce_do_command_x(
    applesauce_handle_t *handle,
    const char *command,
    char *data,
    size_t max_len
) {
    /* First execute command */
    if (applesauce_do_command(handle, command) < 0) {
        return -1;
    }
    
    /* Then read additional data */
    if (serial_read_line(handle->serial_fd, data, max_len) < 0) {
        return -1;
    }
    
    return 0;
}

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize Applesauce device
 */
int applesauce_init(const char *port_path, applesauce_handle_t **handle_out)
{
    if (!port_path || !handle_out) {
        return -1;
    }
    
    applesauce_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return -1;
    }
    
    strncpy(handle->port_path, port_path, sizeof(handle->port_path) - 1);
    
    /* Open serial port */
    if (serial_open(port_path, &handle->serial_fd) < 0) {
        free(handle);
        return -1;
    }
    
    /* Check device ID */
    char response[256];
    if (applesauce_sendrecv(handle, "?", response, sizeof(response)) < 0) {
        close(handle->serial_fd);
        free(handle);
        return -1;
    }
    
    if (strcmp(response, APPLESAUCE_ID_STRING) != 0) {
        fprintf(stderr, "Not an Applesauce device (got '%s')\n", response);
        close(handle->serial_fd);
        free(handle);
        return -1;
    }
    
    /* Set protocol version */
    if (applesauce_do_command(handle, APPLESAUCE_PROTOCOL_V2) < 0) {
        close(handle->serial_fd);
        free(handle);
        return -1;
    }
    
    handle->connected = false;
    handle->motor_on = false;
    handle->current_track = -1;
    
    *handle_out = handle;
    return 0;
}

/**
 * @brief Close Applesauce device
 */
void applesauce_close(applesauce_handle_t *handle)
{
    if (!handle) {
        return;
    }
    
    /* Disconnect gracefully */
    if (handle->connected) {
        applesauce_do_command(handle, "motor:off");
        applesauce_do_command(handle, "disconnect");
    }
    
    close(handle->serial_fd);
    free(handle);
}

/**
 * @brief Connect to drive
 */
int applesauce_connect(applesauce_handle_t *handle)
{
    if (!handle) {
        return -1;
    }
    
    if (handle->connected) {
        return 0;  /* Already connected */
    }
    
    /* Connect sequence */
    if (applesauce_do_command(handle, "connect") < 0) {
        return -1;
    }
    
    if (applesauce_do_command(handle, "drive:enable") < 0) {
        return -1;
    }
    
    if (applesauce_do_command(handle, "motor:on") < 0) {
        return -1;
    }
    
    if (applesauce_do_command(handle, "head:zero") < 0) {
        return -1;
    }
    
    handle->connected = true;
    handle->motor_on = true;
    handle->current_track = 0;
    
    return 0;
}

/**
 * @brief Seek to track
 */
int applesauce_seek(applesauce_handle_t *handle, int track)
{
    if (!handle) {
        return -1;
    }
    
    if (!handle->connected) {
        if (applesauce_connect(handle) < 0) {
            return -1;
        }
    }
    
    char cmd[256];
    
    if (track == 0) {
        if (applesauce_do_command(handle, "head:zero") < 0) {
            return -1;
        }
    } else {
        snprintf(cmd, sizeof(cmd), "head:track%d", track);
        if (applesauce_do_command(handle, cmd) < 0) {
            return -1;
        }
    }
    
    handle->current_track = track;
    return 0;
}

/**
 * @brief Get rotational period (RPM measurement)
 */
int applesauce_get_rpm(
    applesauce_handle_t *handle,
    double *period_us_out
) {
    if (!handle || !period_us_out) {
        return -1;
    }
    
    if (!handle->connected) {
        if (applesauce_connect(handle) < 0) {
            return -1;
        }
    }
    
    char response[256];
    
    /* Query speed */
    if (applesauce_do_command_x(handle, "sync:?speed", response, sizeof(response)) < 0) {
        return -1;
    }
    
    /* Send 'X' to finish */
    serial_write_byte(handle->serial_fd, 'X');
    serial_read_line(handle->serial_fd, response, sizeof(response));
    
    /* Parse period */
    double period = atof(response);
    *period_us_out = period;
    
    return 0;
}

/**
 * @brief Read flux data from track
 */
int applesauce_read_flux(
    applesauce_handle_t *handle,
    int side,
    uint8_t **flux_data_out,
    size_t *flux_len_out
) {
    if (!handle || !flux_data_out || !flux_len_out) {
        return -1;
    }
    
    if (!handle->connected) {
        if (applesauce_connect(handle) < 0) {
            return -1;
        }
    }
    
    /* Set side */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "head:side%d", side);
    if (applesauce_do_command(handle, cmd) < 0) {
        return -1;
    }
    
    /* Read flux data */
    /* Query max data size */
    char response[256];
    if (applesauce_sendrecv(handle, "data:?max", response, sizeof(response)) < 0) {
        return -1;
    }
    
    int max_bytes = atoi(response);
    if (max_bytes <= 0 || max_bytes > 1024 * 1024) {
        return -1;
    }
    
    /* Request flux data read */
    snprintf(cmd, sizeof(cmd), "data:<%d", max_bytes);
    if (applesauce_do_command(handle, cmd) < 0) {
        return -1;
    }
    
    /* Allocate buffer */
    uint8_t *flux_data = malloc(max_bytes);
    if (!flux_data) {
        return -1;
    }
    
    /* Read binary data */
    int bytes_read = serial_read_bytes(handle->serial_fd, flux_data, max_bytes);
    if (bytes_read < 0) {
        free(flux_data);
        return -1;
    }
    
    /* Read confirmation */
    serial_read_line(handle->serial_fd, response, sizeof(response));
    
    *flux_data_out = flux_data;
    *flux_len_out = bytes_read;
    
    handle->bytes_read += bytes_read;
    
    return 0;
}

/**
 * @brief Write flux data to track
 */
int applesauce_write_flux(
    applesauce_handle_t *handle,
    int side,
    const uint8_t *flux_data,
    size_t flux_len
) {
    if (!handle || !flux_data) {
        return -1;
    }
    
    if (!handle->connected) {
        if (applesauce_connect(handle) < 0) {
            return -1;
        }
    }
    
    /* Set side */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "head:side%d", side);
    if (applesauce_do_command(handle, cmd) < 0) {
        return -1;
    }
    
    /* Write flux data */
    snprintf(cmd, sizeof(cmd), "data:>%zu", flux_len);
    if (applesauce_do_command(handle, cmd) < 0) {
        return -1;
    }
    
    /* Send binary data */
    ssize_t written = write(handle->serial_fd, flux_data, flux_len);
    if (written != (ssize_t)flux_len) {
        return -1;
    }
    
    /* Read confirmation */
    char response[256];
    serial_read_line(handle->serial_fd, response, sizeof(response));
    
    handle->bytes_written += flux_len;
    
    return 0;
}

/**
 * @brief Detect Applesauce devices
 */
int applesauce_detect_devices(char ***device_list_out, int *count_out)
{
    if (!device_list_out || !count_out) {
        return -1;
    }
    
    /* Common serial port paths */
    const char *serial_paths[] = {
        "/dev/ttyUSB0",
        "/dev/ttyUSB1",
        "/dev/ttyACM0",
        "/dev/ttyACM1",
        "/dev/cu.usbserial",
        "/dev/cu.usbmodem",
        NULL
    };
    
    int count = 0;
    char **list = calloc(10, sizeof(char*));
    if (!list) {
        return -1;
    }
    
    /* Try each path */
    for (int i = 0; serial_paths[i] != NULL; i++) {
        applesauce_handle_t *handle;
        if (applesauce_init(serial_paths[i], &handle) == 0) {
            list[count] = strdup(serial_paths[i]);
            count++;
            applesauce_close(handle);
        }
    }
    
    *device_list_out = list;
    *count_out = count;
    
    return 0;
}

/**
 * @brief Get statistics
 */
void applesauce_get_stats(
    applesauce_handle_t *handle,
    unsigned long *bytes_read,
    unsigned long *bytes_written
) {
    if (!handle) {
        return;
    }
    
    if (bytes_read) {
        *bytes_read = handle->bytes_read;
    }
    
    if (bytes_written) {
        *bytes_written = handle->bytes_written;
    }
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
