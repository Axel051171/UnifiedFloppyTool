#include "uft/compat/uft_platform.h"
/**
 * @file uft_hw_kryoflux.c
 * @brief UnifiedFloppyTool - KryoFlux Hardware Backend
 *
 * IMPORTANT: The KryoFlux USB protocol is PROPRIETARY and UNDOCUMENTED.
 * Direct USB communication is not possible without reverse-engineering.
 *
 * This backend uses the official DTC (Disk Tool Console) command-line tool
 * as a subprocess for all hardware operations. This is the only supported
 * and legal way to communicate with KryoFlux hardware.
 *
 * DTC commands used:
 * - "dtc -c2"              → Device reset / initialization
 * - "dtc -i0 -g0"          → Get device info
 * - "dtc -m1"              → Motor on
 * - "dtc -m0"              → Motor off
 * - "dtc -t<N>"            → Seek to track N
 * - "dtc -d<N>"            → Select drive N (0 or 1)
 * - "dtc -s<N>"            → Select side N (0 or 1)
 * - "dtc -p -tN -eN -iT"  → Read track(s) to file(s), T=output type
 *
 * Output types: 0=KF stream, 1=CT Raw, 2=DSK, ...
 *
 * Stream format (OOB blocks are documented and correct):
 * - Flux values: 0x00-0xE7 = 8-bit value, 0xE8-0xFF = opcodes
 * - OOB blocks: 0x08=NOP1, 0x09=NOP2, 0x0A=NOP3, 0x0B=Overflow
 *               0x0C=value16, 0x0D=OOB marker
 * - OOB types:  0x01=StreamInfo, 0x02=Index, 0x03=StreamEnd,
 *               0x04=KFInfo, 0x0D=EOF
 *
 * Sample clock: 18.432 MHz × (73/56) ÷ 2 ≈ 24.027428 MHz (sck/2)
 *
 * @see https://kryoflux.com
 * @see uft_kryoflux_dtc.c (HAL layer for DTC wrapper)
 *
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_hardware.h"
#include "uft/uft_hardware_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef UFT_OS_LINUX
    #include <libusb-1.0/libusb.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

/* ============================================================================
 * KryoFlux Constants
 * ============================================================================ */

#define KF_VID                  0x03EB  /* Atmel USB VID */
#define KF_PID                  0x6124  /* KryoFlux PID */

/* Sample clock: PAL subcarrier × 6 / 2 ≈ 24.027428 MHz */
#define KF_SAMPLE_FREQ          24027428
#define KF_TICK_NS              (1000000000.0 / KF_SAMPLE_FREQ)

/* Stream format opcodes (0x08-0x0D) - these ARE documented */
#define KF_OP_NOP1              0x08  /* 1-byte NOP */
#define KF_OP_NOP2              0x09  /* 2-byte NOP (skip 1) */
#define KF_OP_NOP3              0x0A  /* 3-byte NOP (skip 2) */
#define KF_OP_OVERFLOW16        0x0B  /* Overflow: add 0x10000 to next flux */
#define KF_OP_VALUE16           0x0C  /* 16-bit flux value follows (BE) */
#define KF_OP_OOB               0x0D  /* Out-of-Band block follows */

/* OOB block types */
#define KF_OOB_INVALID          0x00
#define KF_OOB_STREAM_INFO      0x01  /* Stream position info */
#define KF_OOB_INDEX            0x02  /* Index pulse timing */
#define KF_OOB_STREAM_END       0x03  /* End of stream */
#define KF_OOB_KF_INFO          0x04  /* KryoFlux device info string */
#define KF_OOB_EOF              0x0D  /* End of file */

/* Flux encoding threshold */
#define KF_FLUX_MAX_8BIT        0x07  /* 0x00-0x07 = flux values (+ cell offset) */

/* DTC tool name */
#define DTC_TOOL_NAME           "dtc"

/* ============================================================================
 * Device State
 * ============================================================================ */

typedef struct {
    bool        dtc_available;   /* DTC tool found in PATH */
    char        dtc_path[256];   /* Full path to DTC binary */
    char        temp_dir[256];   /* Temp dir for stream files */

    uint8_t     current_track;
    uint8_t     current_head;
    uint8_t     current_drive;   /* 0 or 1 */
    bool        motor_on;
    bool        initialized;
} uft_kf_state_t;

/* ============================================================================
 * DTC Subprocess Helpers
 * ============================================================================ */

/**
 * Find DTC binary in PATH
 */
static bool find_dtc(char *path_out, size_t path_len) {
    /* Try common locations */
    const char *candidates[] = {
        "/usr/local/bin/dtc",
        "/usr/bin/dtc",
        "/opt/kryoflux/dtc",
        "./dtc",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) {
            strncpy(path_out, candidates[i], path_len - 1);
            return true;
        }
    }

    /* Try PATH via 'which' */
#ifdef UFT_OS_LINUX
    FILE *fp = popen("which dtc 2>/dev/null", "r");
    if (fp) {
        if (fgets(path_out, (int)path_len, fp)) {
            /* Remove trailing newline */
            size_t len = strlen(path_out);
            if (len > 0 && path_out[len - 1] == '\n')
                path_out[len - 1] = '\0';
            pclose(fp);
            return path_out[0] != '\0';
        }
        pclose(fp);
    }
#endif

    return false;
}

/**
 * Run DTC with given arguments, capture stdout.
 *
 * @return 0 on success, -1 on failure
 */
static int run_dtc(uft_kf_state_t *kf, const char *args,
                   char *output, size_t output_len) {
    if (!kf->dtc_available) return -1;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", kf->dtc_path, args);

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
}

/* ============================================================================
 * Backend Implementation
 * ============================================================================ */

static uft_error_t uft_kf_init(void) {
#ifdef UFT_OS_LINUX
    return (libusb_init(NULL) == 0) ? UFT_OK : UFT_ERROR_IO;
#else
    return UFT_OK;
#endif
}

static void uft_kf_shutdown(void) {
#ifdef UFT_OS_LINUX
    libusb_exit(NULL);
#endif
}

static uft_error_t uft_kf_enumerate(uft_hw_info_t *devices, size_t max_devices,
                                    size_t *found) {
    *found = 0;

#ifdef UFT_OS_LINUX
    libusb_device **dev_list;
    ssize_t cnt = libusb_get_device_list(NULL, &dev_list);
    if (cnt < 0) return UFT_OK;

    for (ssize_t i = 0; i < cnt && *found < max_devices; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(dev_list[i], &desc) < 0) continue;

        if (desc.idVendor == KF_VID && desc.idProduct == KF_PID) {
            uft_hw_info_t *info = &devices[*found];
            memset(info, 0, sizeof(*info));
            info->type = UFT_HW_KRYOFLUX;
            snprintf(info->name, sizeof(info->name), "KryoFlux");
            info->usb_vid = desc.idVendor;
            info->usb_pid = desc.idProduct;

            uint8_t bus = libusb_get_bus_number(dev_list[i]);
            uint8_t addr = libusb_get_device_address(dev_list[i]);
            snprintf(info->usb_path, sizeof(info->usb_path), "%d-%d", bus, addr);

            info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_FLUX |
                                 UFT_HW_CAP_INDEX | UFT_HW_CAP_MULTI_REV |
                                 UFT_HW_CAP_MOTOR | UFT_HW_CAP_TIMING;
            info->sample_rate_hz = KF_SAMPLE_FREQ;
            info->resolution_ns = (uint32_t)(KF_TICK_NS + 0.5);
            (*found)++;
        }
    }
    libusb_free_device_list(dev_list, 1);
#else
    (void)devices; (void)max_devices;
#endif
    return UFT_OK;
}

static uft_error_t uft_kf_open(const uft_hw_info_t *info, uft_hw_device_t **device) {
    (void)info;

    uft_kf_state_t *kf = calloc(1, sizeof(uft_kf_state_t));
    if (!kf) return UFT_ERROR_NO_MEMORY;

    /* Find DTC binary - required for all operations */
    kf->dtc_available = find_dtc(kf->dtc_path, sizeof(kf->dtc_path));
    if (!kf->dtc_available) {
        free(kf);
        return UFT_ERROR_NOT_SUPPORTED;  /* DTC tool not found */
    }

    /* Create temp directory for stream files */
    snprintf(kf->temp_dir, sizeof(kf->temp_dir), "/tmp/uft_kryoflux_%d", getpid());
    char mkdir_cmd[300];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", kf->temp_dir);
    system(mkdir_cmd);

    /* Initialize device via DTC */
    char output[256];
    if (run_dtc(kf, "-c2", output, sizeof(output)) != 0) {
        free(kf);
        return UFT_ERROR_IO;
    }

    kf->initialized = true;
    (*device)->handle = kf;
    return UFT_OK;
}

static void uft_kf_close(uft_hw_device_t *device) {
    if (!device || !device->handle) return;
    uft_kf_state_t *kf = device->handle;

    /* Motor off and cleanup */
    if (kf->motor_on) {
        run_dtc(kf, "-m0", NULL, 0);
    }

    /* Remove temp directory */
    char rm_cmd[300];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", kf->temp_dir);
    system(rm_cmd);

    free(kf);
    device->handle = NULL;
}

static uft_error_t uft_kf_get_status(uft_hw_device_t *device,
                                      uft_drive_status_t *status) {
    if (!device || !device->handle || !status) return UFT_ERROR_NULL_POINTER;
    uft_kf_state_t *kf = device->handle;
    memset(status, 0, sizeof(*status));

    status->connected = kf->initialized;
    status->ready = kf->initialized;
    status->motor_on = kf->motor_on;
    status->current_track = kf->current_track;
    status->current_head = kf->current_head;
    return UFT_OK;
}

static uft_error_t uft_kf_motor(uft_hw_device_t *device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_kf_state_t *kf = device->handle;

    char args[32];
    snprintf(args, sizeof(args), "-m%d", on ? 1 : 0);
    if (run_dtc(kf, args, NULL, 0) != 0) return UFT_ERROR_IO;

    kf->motor_on = on;
    return UFT_OK;
}

static uft_error_t uft_kf_seek(uft_hw_device_t *device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_kf_state_t *kf = device->handle;

    char args[32];
    snprintf(args, sizeof(args), "-t%d", track);
    if (run_dtc(kf, args, NULL, 0) != 0) return UFT_ERROR_SEEK_ERROR;

    kf->current_track = track;
    return UFT_OK;
}

static uft_error_t uft_kf_select_head(uft_hw_device_t *device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_kf_state_t *kf = device->handle;

    char args[32];
    snprintf(args, sizeof(args), "-s%d", head);
    if (run_dtc(kf, args, NULL, 0) != 0) return UFT_ERROR_IO;

    kf->current_head = head;
    return UFT_OK;
}

static uft_error_t uft_kf_select_density(uft_hw_device_t *device, bool high_density) {
    (void)device; (void)high_density;
    /* KryoFlux auto-detects density from flux timing */
    return UFT_OK;
}

/**
 * Parse KryoFlux stream data into flux values.
 *
 * Stream format (documented):
 * - Bytes 0x00-0x07: flux cell value (cell + overflow)
 * - Byte 0x08: NOP1 (skip)
 * - Byte 0x09: NOP2 (skip 1 more byte)
 * - Byte 0x0A: NOP3 (skip 2 more bytes)
 * - Byte 0x0B: Overflow16 (add 0x10000 to next cell)
 * - Byte 0x0C: Value16 (next 2 bytes = 16-bit flux value, big-endian)
 * - Byte 0x0D: OOB marker
 *   - OOB: [0x0D][type.b][size.w_LE][payload...]
 *   - Type 0x01: StreamInfo [streampos.l, transferpos.l]
 *   - Type 0x02: Index [samplecounter.l, indexcounter.l]
 *   - Type 0x03: StreamEnd [streampos.l, result.l]
 *   - Type 0x04: KFInfo (ASCII string, null-terminated)
 *   - Type 0x0D: EOF
 */
static size_t parse_kf_stream(const uint8_t *data, size_t data_len,
                              uint32_t *flux_out, size_t max_flux) {
    size_t pos = 0;
    size_t flux_count = 0;
    uint32_t overflow = 0;

    while (pos < data_len && flux_count < max_flux) {
        uint8_t b = data[pos];

        if (b <= 0x07) {
            /* 8-bit flux cell: value is in following byte */
            if (pos + 1 >= data_len) break;
            uint16_t cell = ((uint16_t)b << 8) | data[pos + 1];
            flux_out[flux_count++] = cell + overflow;
            overflow = 0;
            pos += 2;
        }
        else if (b == KF_OP_NOP1) {
            pos += 1;
        }
        else if (b == KF_OP_NOP2) {
            pos += 2;
        }
        else if (b == KF_OP_NOP3) {
            pos += 3;
        }
        else if (b == KF_OP_OVERFLOW16) {
            overflow += 0x10000;
            pos += 1;
        }
        else if (b == KF_OP_VALUE16) {
            /* 16-bit value follows (big-endian) */
            if (pos + 2 >= data_len) break;
            uint16_t cell = ((uint16_t)data[pos + 1] << 8) | data[pos + 2];
            flux_out[flux_count++] = cell + overflow;
            overflow = 0;
            pos += 3;
        }
        else if (b == KF_OP_OOB) {
            /* OOB block: [0x0D][type][size_lo][size_hi][payload...] */
            if (pos + 3 >= data_len) break;
            uint8_t oob_type = data[pos + 1];
            uint16_t oob_size = data[pos + 2] | ((uint16_t)data[pos + 3] << 8);

            if (oob_type == KF_OOB_EOF) break;  /* End of stream */

            pos += 4 + oob_size;
        }
        else {
            /* Unknown opcode, skip */
            pos += 1;
        }
    }

    return flux_count;
}

/**
 * Read flux via DTC: capture stream to temp file, then parse.
 *
 * DTC command: dtc -p -t<track> -e<track> -s<side> -i0 -ftemp_dir/track
 * Output type 0 = KryoFlux stream format
 */
static uft_error_t uft_kf_read_flux(uft_hw_device_t *device, uint32_t *flux,
                                    size_t max_flux, size_t *flux_count,
                                    uint8_t revolutions) {
    if (!device || !device->handle || !flux || !flux_count)
        return UFT_ERROR_NULL_POINTER;

    uft_kf_state_t *kf = device->handle;
    *flux_count = 0;
    (void)revolutions;  /* DTC captures full revolutions automatically */

    /* Capture track to stream file via DTC */
    char args[512];
    char stream_path[300];
    snprintf(stream_path, sizeof(stream_path), "%s/track", kf->temp_dir);
    snprintf(args, sizeof(args),
             "-p -t%d -e%d -s%d -i0 -f%s",
             kf->current_track, kf->current_track,
             kf->current_head, stream_path);

    char output[1024];
    if (run_dtc(kf, args, output, sizeof(output)) != 0) {
        return UFT_ERROR_IO;
    }

    /* Read the stream file
     * DTC outputs: track_dir/trackNN.S.raw (S=side) */
    char filename[350];
    snprintf(filename, sizeof(filename), "%s/track%02d.%d.raw",
             stream_path, kf->current_track, kf->current_head);

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        /* Try alternate naming */
        snprintf(filename, sizeof(filename), "%s%02d.%d.raw",
                 stream_path, kf->current_track, kf->current_head);
        fp = fopen(filename, "rb");
    }
    if (!fp) return UFT_ERROR_IO;

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 4 * 1024 * 1024) {
        fclose(fp);
        return UFT_ERROR_IO;
    }

    uint8_t *stream_data = malloc((size_t)file_size);
    if (!stream_data) { fclose(fp); return UFT_ERROR_NO_MEMORY; }

    if (fread(stream_data, 1, (size_t)file_size, fp) != (size_t)file_size) {
        free(stream_data);
        fclose(fp);
        return UFT_ERROR_IO;
    }
    fclose(fp);

    /* Parse stream into flux values */
    *flux_count = parse_kf_stream(stream_data, (size_t)file_size,
                                  flux, max_flux);
    free(stream_data);

    /* Convert from sample counts to nanoseconds */
    for (size_t i = 0; i < *flux_count; i++) {
        flux[i] = (uint32_t)(flux[i] * KF_TICK_NS);
    }

    return (*flux_count > 0) ? UFT_OK : UFT_ERROR_IO;
}

/**
 * KryoFlux write support is limited and requires DTC -w flag.
 * Not all firmware versions support writing.
 */
static uft_error_t uft_kf_write_flux(uft_hw_device_t *device,
                                     const uint32_t *flux, size_t flux_count) {
    (void)device; (void)flux; (void)flux_count;
    return UFT_ERROR_NOT_SUPPORTED;
    /* TODO: implement via DTC -w if firmware supports it */
}

/* ============================================================================
 * Backend Definition
 * ============================================================================ */

static const uft_hw_backend_t uft_kf_backend = {
    .name = "KryoFlux (via DTC)",
    .type = UFT_HW_KRYOFLUX,
    .init = uft_kf_init,
    .shutdown = uft_kf_shutdown,
    .enumerate = uft_kf_enumerate,
    .open = uft_kf_open,
    .close = uft_kf_close,
    .get_status = uft_kf_get_status,
    .motor = uft_kf_motor,
    .seek = uft_kf_seek,
    .select_head = uft_kf_select_head,
    .select_density = uft_kf_select_density,
    .read_track = NULL,
    .write_track = NULL,
    .read_flux = uft_kf_read_flux,
    .write_flux = uft_kf_write_flux,
    .parallel_write = NULL,
    .parallel_read = NULL,
    .iec_command = NULL,
    .private_data = NULL
};

uft_error_t uft_hw_register_kryoflux(void) {
    return uft_hw_register_backend(&uft_kf_backend);
}

const uft_hw_backend_t uft_hw_backend_kryoflux = {
    .name = "KryoFlux (via DTC)",
    .type = UFT_HW_KRYOFLUX,
    .init = uft_kf_init,
    .shutdown = uft_kf_shutdown,
    .enumerate = uft_kf_enumerate,
    .open = uft_kf_open,
    .close = uft_kf_close,
    .get_status = uft_kf_get_status,
    .motor = uft_kf_motor,
    .seek = uft_kf_seek,
    .select_head = uft_kf_select_head,
    .select_density = uft_kf_select_density,
    .read_flux = uft_kf_read_flux,
    .write_flux = uft_kf_write_flux,
    .private_data = NULL
};
