/**
 * @file uft_pauline.c
 * @brief Pauline Floppy Controller Implementation
 *
 * CORRECTED: Pauline uses an embedded Linux system on DE10-nano FPGA.
 * Control is via HTTP web server, SSH, or FTP - NOT a binary protocol.
 *
 * Architecture (from github.com/jfdelnero/Pauline):
 * - DE10-nano FPGA board with custom floppy controller IP
 * - Embedded Linux (built via custom LinuxFromScratch scripts)
 * - Embedded web server for browser-based control UI
 * - SSH access for scripted/CLI control
 * - FTP for file transfer (captured images)
 * - HxCFloppyEmulator library as submodule for format handling
 * - Captures flux as HxC Stream format (.hfe)
 * - 50 MHz or 25 MHz capture resolution
 * - Up to 16 channels, 4 drive emulation
 *
 * This implementation uses SSH commands for drive control and
 * HTTP/FTP for data transfer, matching Pauline's real architecture.
 *
 * @see https://github.com/jfdelnero/Pauline
 * @copyright MIT License
 */

#include "uft/hal/uft_pauline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#endif

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct uft_pauline_device_s {
    char hostname[256];
    int  http_port;          /* Web server port (default 80) */
    char ssh_user[64];       /* SSH user (default "root") */
    char ssh_keyfile[256];   /* SSH key file (optional) */

    bool connected;
    int  current_drive;
    int  current_track;
    int  current_head;
    bool motor_on;
    int  sample_rate;        /* 25 or 50 MHz */

    char last_error[256];
    char temp_dir[256];      /* Local temp dir for downloads */
};

/* ============================================================================
 * HTTP Client Helpers (minimal, for Pauline web server)
 * ============================================================================ */

/**
 * Perform HTTP GET request to Pauline web server.
 * Returns response body (caller frees), or NULL on error.
 */
static char* http_get(const char *hostname, int port, const char *path,
                      size_t *resp_len) {
#ifndef _WIN32
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;

    /* Set timeout */
    struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    /* Resolve and connect */
    struct hostent *he = gethostbyname(hostname);
    if (!he) { close(sock); return NULL; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }

    /* Send HTTP GET */
    char request[512];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n", path, hostname);
    if (write(sock, request, req_len) != req_len) {
        close(sock);
        return NULL;
    }

    /* Read response */
    size_t buf_size = 64 * 1024;
    char *buf = malloc(buf_size);
    if (!buf) { close(sock); return NULL; }

    size_t total = 0;
    while (total < buf_size - 1) {
        ssize_t n = read(sock, buf + total, buf_size - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';
    close(sock);

    /* Skip HTTP headers (find \r\n\r\n) */
    char *body = strstr(buf, "\r\n\r\n");
    if (!body) { free(buf); return NULL; }
    body += 4;

    size_t body_len = total - (body - buf);
    char *result = malloc(body_len + 1);
    if (!result) { free(buf); return NULL; }
    memcpy(result, body, body_len);
    result[body_len] = '\0';

    if (resp_len) *resp_len = body_len;
    free(buf);
    return result;
#else
    (void)hostname; (void)port; (void)path; (void)resp_len;
    return NULL;
#endif
}

/**
 * Download binary data from Pauline via HTTP GET.
 */
static uint8_t* http_get_binary(const char *hostname, int port,
                                const char *path, size_t *data_len) {
    return (uint8_t *)http_get(hostname, port, path, data_len);
}

/* ============================================================================
 * SSH Command Execution
 *
 * Pauline runs embedded Linux with SSH server.
 * Drive control is done via command-line tools on the device.
 * ============================================================================ */

/**
 * Execute command on Pauline via SSH, capture stdout.
 */
static int ssh_exec(uft_pauline_device_t *dev, const char *remote_cmd,
                    char *output, size_t output_len) {
#ifndef _WIN32
    char cmd[1024];

    if (dev->ssh_keyfile[0]) {
        snprintf(cmd, sizeof(cmd),
                 "ssh -i %s -o StrictHostKeyChecking=no -o ConnectTimeout=5 "
                 "%s@%s '%s' 2>/dev/null",
                 dev->ssh_keyfile, dev->ssh_user, dev->hostname, remote_cmd);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "
                 "%s@%s '%s' 2>/dev/null",
                 dev->ssh_user, dev->hostname, remote_cmd);
    }

    if (output && output_len > 0) {
        output[0] = '\0';
        FILE *fp = popen(cmd, "r");
        if (!fp) return -1;

        size_t total = 0;
        while (total < output_len - 1) {
            size_t n = fread(output + total, 1, output_len - 1 - total, fp);
            if (n == 0) break;
            total += n;
        }
        output[total] = '\0';
        int status = pclose(fp);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
    } else {
        return system(cmd) == 0 ? 0 : -1;
    }
#else
    (void)dev; (void)remote_cmd; (void)output; (void)output_len;
    return -1;
#endif
}

/* ============================================================================
 * Pauline API
 * ============================================================================ */

uft_pauline_device_t* uft_pauline_create(void) {
    uft_pauline_device_t *dev = calloc(1, sizeof(*dev));
    if (!dev) return NULL;
    dev->http_port = 80;
    strncpy(dev->ssh_user, "root", sizeof(dev->ssh_user));
    dev->sample_rate = UFT_PAULINE_SAMPLE_RATE_50MHZ;
    dev->current_drive = -1;
    return dev;
}

void uft_pauline_destroy(uft_pauline_device_t *dev) {
    if (!dev) return;
    if (dev->connected) uft_pauline_close(dev);
    free(dev);
}

int uft_pauline_open(uft_pauline_device_t *dev, const char *hostname, int port) {
    if (!dev || !hostname) return -1;

    strncpy(dev->hostname, hostname, sizeof(dev->hostname) - 1);
    if (port > 0) dev->http_port = port;

    /* Verify connectivity via HTTP - try to reach the web server */
    size_t resp_len = 0;
    char *resp = http_get(dev->hostname, dev->http_port, "/", &resp_len);
    if (resp) {
        free(resp);
        dev->connected = true;
        return 0;
    }

    /* Fallback: try SSH */
    char output[256];
    if (ssh_exec(dev, "echo ok", output, sizeof(output)) == 0 &&
        strstr(output, "ok")) {
        dev->connected = true;
        return 0;
    }

    snprintf(dev->last_error, sizeof(dev->last_error),
             "Pauline: cannot connect to %s (HTTP port %d / SSH)",
             hostname, dev->http_port);
    return -1;
}

void uft_pauline_close(uft_pauline_device_t *dev) {
    if (!dev) return;
    if (dev->motor_on) {
        uft_pauline_motor_off(dev);
    }
    dev->connected = false;
}

bool uft_pauline_is_connected(const uft_pauline_device_t *dev) {
    return dev && dev->connected;
}

int uft_pauline_get_info(uft_pauline_device_t *dev, uft_pauline_info_t *info) {
    if (!dev || !info || !dev->connected) return -1;
    memset(info, 0, sizeof(*info));

    /* Query device info via HTTP or SSH */
    char output[1024];
    if (ssh_exec(dev, "cat /etc/pauline/version 2>/dev/null || echo 'unknown'",
                 output, sizeof(output)) == 0) {
        strncpy(info->firmware_version, output,
                sizeof(info->firmware_version) - 1);
        /* Trim newline */
        char *nl = strchr(info->firmware_version, '\n');
        if (nl) *nl = '\0';
    }

    strncpy(info->device_name, "Pauline FDC", sizeof(info->device_name));
    info->sample_rate = dev->sample_rate;
    info->max_tracks = UFT_PAULINE_MAX_TRACKS;
    info->max_heads = UFT_PAULINE_MAX_HEADS;
    return 0;
}

int uft_pauline_get_status(uft_pauline_device_t *dev, uft_pauline_status_t *status) {
    if (!dev || !status) return -1;
    memset(status, 0, sizeof(*status));

    status->connected = dev->connected;
    status->motor_on = dev->motor_on;
    status->current_track = dev->current_track;
    status->current_head = dev->current_head;
    status->current_drive = dev->current_drive;
    return 0;
}

int uft_pauline_set_drive(uft_pauline_device_t *dev, int drive, int type) {
    if (!dev || !dev->connected) return -1;
    (void)type;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pauline_cli select_drive %d", drive);
    if (ssh_exec(dev, cmd, NULL, 0) != 0) return -1;

    dev->current_drive = drive;
    return 0;
}

int uft_pauline_motor_on(uft_pauline_device_t *dev) {
    if (!dev || !dev->connected) return -1;

    if (ssh_exec(dev, "pauline_cli motor_on", NULL, 0) != 0) return -1;
    dev->motor_on = true;
    return 0;
}

int uft_pauline_motor_off(uft_pauline_device_t *dev) {
    if (!dev || !dev->connected) return -1;

    if (ssh_exec(dev, "pauline_cli motor_off", NULL, 0) != 0) return -1;
    dev->motor_on = false;
    return 0;
}

int uft_pauline_seek(uft_pauline_device_t *dev, int track) {
    if (!dev || !dev->connected) return -1;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pauline_cli seek %d", track);
    if (ssh_exec(dev, cmd, NULL, 0) != 0) return -1;

    dev->current_track = track;
    return 0;
}

int uft_pauline_recalibrate(uft_pauline_device_t *dev) {
    if (!dev || !dev->connected) return -1;

    if (ssh_exec(dev, "pauline_cli recalibrate", NULL, 0) != 0) return -1;
    dev->current_track = 0;
    return 0;
}

int uft_pauline_select_head(uft_pauline_device_t *dev, int head) {
    if (!dev || !dev->connected) return -1;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pauline_cli select_head %d", head);
    if (ssh_exec(dev, cmd, NULL, 0) != 0) return -1;

    dev->current_head = head;
    return 0;
}

int uft_pauline_set_sample_rate(uft_pauline_device_t *dev, int rate_hz) {
    if (!dev || !dev->connected) return -1;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pauline_cli set_samplerate %d", rate_hz);
    if (ssh_exec(dev, cmd, NULL, 0) != 0) return -1;

    dev->sample_rate = rate_hz;
    return 0;
}

/**
 * Read track flux data from Pauline.
 *
 * Pauline captures flux data in HxC Stream format. We request a capture
 * via SSH command, then download the resulting stream file.
 *
 * The captured data uses Pauline's native 50MHz or 25MHz sampling
 * (20ns or 40ns per tick).
 */
int uft_pauline_read_track(uft_pauline_device_t *dev,
                           int track, int head, int revolutions,
                           uint8_t **stream_data, size_t *stream_len) {
    if (!dev || !dev->connected || !stream_data || !stream_len) return -1;

    *stream_data = NULL;
    *stream_len = 0;

    /* Seek and select head */
    if (uft_pauline_seek(dev, track) != 0) return -1;
    if (uft_pauline_select_head(dev, head) != 0) return -1;

    /* Command Pauline to capture track to a temp file on device */
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "pauline_cli capture_track /tmp/uft_capture.hfe %d 2>/dev/null && "
             "cat /tmp/uft_capture.hfe | base64",
             revolutions > 0 ? revolutions : 3);

    char *output = malloc(16 * 1024 * 1024);  /* Up to 16MB */
    if (!output) return -1;

    if (ssh_exec(dev, cmd, output, 16 * 1024 * 1024) != 0) {
        free(output);
        return -1;
    }

    /* Decode base64 response */
    size_t out_len = strlen(output);
    if (out_len == 0) {
        free(output);
        return -1;
    }

    /* Simple base64 decode - the stream file is transferred base64 */
    /* For production use, this should use a proper base64 library.
     * Alternative: use SCP/FTP to download the file directly. */

    /* For now, try HTTP download as alternative */
    free(output);

    /* Attempt HTTP download of captured file */
    char http_path[256];
    snprintf(http_path, sizeof(http_path),
             "/capture?track=%d&head=%d&revs=%d&format=hxcstream",
             track, head, revolutions > 0 ? revolutions : 3);

    size_t data_len = 0;
    uint8_t *data = http_get_binary(dev->hostname, dev->http_port,
                                    http_path, &data_len);
    if (data && data_len > 0) {
        *stream_data = data;
        *stream_len = data_len;
        return 0;
    }

    snprintf(dev->last_error, sizeof(dev->last_error),
             "Pauline: capture failed for track %d head %d", track, head);
    return -1;
}

/**
 * Read index pulse timing.
 */
int uft_pauline_read_index(uft_pauline_device_t *dev,
                           uint32_t *index_times, size_t max_indices,
                           size_t *index_count) {
    if (!dev || !dev->connected || !index_times || !index_count) return -1;
    *index_count = 0;

    char output[4096];
    if (ssh_exec(dev, "pauline_cli read_index", output, sizeof(output)) != 0)
        return -1;

    /* Parse index times from output (one per line, in sample counts) */
    char *line = strtok(output, "\n");
    while (line && *index_count < max_indices) {
        uint32_t val = 0;
        if (sscanf(line, "%u", &val) == 1) {
            index_times[*index_count] = val;
            (*index_count)++;
        }
        line = strtok(NULL, "\n");
    }

    return 0;
}

/**
 * Write track flux data to Pauline.
 */
int uft_pauline_write_track(uft_pauline_device_t *dev,
                            int track, int head,
                            const uint8_t *stream_data, size_t stream_len) {
    if (!dev || !dev->connected || !stream_data || stream_len == 0) return -1;

    /* Seek and select head */
    if (uft_pauline_seek(dev, track) != 0) return -1;
    if (uft_pauline_select_head(dev, head) != 0) return -1;

    /* Upload stream data to device and write.
     * In production, this should use SCP or FTP to upload the file,
     * then trigger the write via SSH command. */
    /* TODO: implement proper file upload + write trigger */

    snprintf(dev->last_error, sizeof(dev->last_error),
             "Pauline write: not yet fully implemented");
    return -1;
}

const char* uft_pauline_get_error(const uft_pauline_device_t *dev) {
    return dev ? dev->last_error : "null device";
}

/**
 * Set SSH credentials for Pauline access.
 */
int uft_pauline_set_ssh_credentials(uft_pauline_device_t *dev,
                                    const char *user, const char *keyfile) {
    if (!dev) return -1;
    if (user) strncpy(dev->ssh_user, user, sizeof(dev->ssh_user) - 1);
    if (keyfile) strncpy(dev->ssh_keyfile, keyfile, sizeof(dev->ssh_keyfile) - 1);
    return 0;
}

double uft_pauline_ticks_to_ns(const uft_pauline_device_t *dev, uint32_t ticks) {
    if (!dev) return 0;
    return (double)ticks * (1000000000.0 / dev->sample_rate);
}

uint32_t uft_pauline_ns_to_ticks(const uft_pauline_device_t *dev, double ns) {
    if (!dev) return 0;
    return (uint32_t)(ns * dev->sample_rate / 1000000000.0 + 0.5);
}
