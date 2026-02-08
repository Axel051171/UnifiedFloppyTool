/**
 * @file uft_hw_opencbm.c
 * 
 * Backend für Commodore 1541/1571/1581 Laufwerke via:
 * - XUM1541 (USB)
 * - ZoomFloppy (XUM1541 mit Parallel-Support)
 * - XU1541 (USB, nur seriell)
 * - XA1541 (Active Adapter)
 * 
 * NIBTOOLS INTEGRATION:
 * - Liest GCR-Rohdaten direkt vom Laufwerk
 * - Umgeht DOS für Kopierschutz-Preservation
 * - Benötigt speziellen Code im Laufwerk (via M-E)
 * 
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_hardware.h"
#include "uft/core/uft_safe_parse.h"
#include "uft/uft_hardware_internal.h"
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <stdio.h>
#include "uft/core/uft_safe_parse.h"

// ============================================================================
// CBM library Interface (libopencbm)
// ============================================================================

#ifdef UFT_HAS_OPENCBM
#include <opencbm.h>
#include "uft/core/uft_safe_parse.h"
#else
// Stub-Deklarationen wenn CBM library nicht verfügbar
typedef void* CBM_FILE;
static int cbm_driver_open(CBM_FILE* f, int port) { (void)f; (void)port; return -1; }
static void cbm_driver_close(CBM_FILE f) { (void)f; }
static int cbm_talk(CBM_FILE f, unsigned char dev, unsigned char sec) { 
    (void)f; (void)dev; (void)sec; return -1; 
}
static int cbm_untalk(CBM_FILE f) { (void)f; return -1; }
static int cbm_listen(CBM_FILE f, unsigned char dev, unsigned char sec) {
    (void)f; (void)dev; (void)sec; return -1;
}
static int cbm_unlisten(CBM_FILE f) { (void)f; return -1; }
static int cbm_raw_write(CBM_FILE f, const void* buf, size_t len) {
    (void)f; (void)buf; (void)len; return -1;
}
static int cbm_raw_read(CBM_FILE f, void* buf, size_t len) {
    (void)f; (void)buf; (void)len; return -1;
}
static int cbm_exec_command(CBM_FILE f, unsigned char dev, const void* cmd, size_t len) {
    (void)f; (void)dev; (void)cmd; (void)len; return -1;
}
#endif

// ============================================================================
// 1541 Constants
// ============================================================================

#define NIBTOOLS_BUFFER     0x0300  // Arbeitspuffer im 1541
#define NIBTOOLS_CODE       0x0500  // Code-Bereich

// Nibtools Read-Code (6502 Assembler)
static const uint8_t GCR tools_read_code[] = {
    0x78,               // SEI
    0xA9, 0x00,         // LDA #$00
    0x8D, 0x00, 0x1C,   // STA $1C00
    0xA9, 0xFF,         // LDA #$FF
    0xCD, 0x00, 0x1C,   // CMP $1C00
    0xD0, 0xFB,         // BNE *-3
    0xA0, 0x00,         // LDY #$00
    0xAD, 0x01, 0x1C,   // LDA $1C01
    0x99, 0x00, 0x03,   // STA $0300,Y
    0xC8,               // INY
    0xD0, 0xF7,         // BNE *-7
    0x60,               // RTS
};

// ============================================================================
// Device State
// ============================================================================

typedef struct {
    CBM_FILE    handle;
    uint8_t     device_num;
    uint8_t     drive_type;
    uint8_t     current_track;
    bool        motor_on;
    bool        GCR tools_loaded;
} opencbm_state_t;

// ============================================================================
// Helper Functions
// ============================================================================

static uft_error_t send_memory_execute(opencbm_state_t* cbm, 
                                        uint16_t address,
                                        const uint8_t* code, size_t code_len) {
#ifdef UFT_HAS_OPENCBM
    uint8_t cmd[256];
    size_t pos = 0;
    
    while (pos < code_len) {
        size_t chunk = code_len - pos;
        if (chunk > 32) chunk = 32;
        
        cmd[0] = 'M'; cmd[1] = '-'; cmd[2] = 'W';
        cmd[3] = (uint8_t)((address + pos) & 0xFF);
        cmd[4] = (uint8_t)((address + pos) >> 8);
        cmd[5] = (uint8_t)chunk;
        memcpy(&cmd[6], &code[pos], chunk);
        
        if (cbm_exec_command(cbm->handle, cbm->device_num, cmd, 6 + chunk) < 0) {
            return UFT_ERROR_IO;
        }
        pos += chunk;
    }
    
    cmd[0] = 'M'; cmd[1] = '-'; cmd[2] = 'E';
    cmd[3] = (uint8_t)(address & 0xFF);
    cmd[4] = (uint8_t)(address >> 8);
    
    if (cbm_exec_command(cbm->handle, cbm->device_num, cmd, 5) < 0) {
        return UFT_ERROR_IO;
    }
    return UFT_OK;
#else
    (void)cbm; (void)address; (void)code; (void)code_len;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static uft_error_t read_drive_memory(opencbm_state_t* cbm,
                                     uint16_t address, 
                                     uint8_t* data, size_t len) {
#ifdef UFT_HAS_OPENCBM
    size_t pos = 0;
    while (pos < len) {
        size_t chunk = len - pos;
        if (chunk > 256) chunk = 256;
        
        uint8_t cmd[5] = {'M', '-', 'R',
                          (uint8_t)((address + pos) & 0xFF),
                          (uint8_t)((address + pos) >> 8)};
        
        if (cbm_exec_command(cbm->handle, cbm->device_num, cmd, 5) < 0) {
            return UFT_ERROR_IO;
        }
        
        if (cbm_talk(cbm->handle, cbm->device_num, 15) < 0) {
            return UFT_ERROR_IO;
        }
        
        int n = cbm_raw_read(cbm->handle, &data[pos], chunk);
        cbm_untalk(cbm->handle);
        
        if (n < 0) return UFT_ERROR_IO;
        pos += (size_t)n;
    }
    return UFT_OK;
#else
    (void)cbm; (void)address; (void)data; (void)len;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

// ============================================================================
// Backend Implementation
// ============================================================================

static uft_error_t opencbm_init(void) {
    return UFT_OK;
}

static void opencbm_shutdown(void) {
}

static uft_error_t opencbm_enumerate(uft_hw_info_t* devices, size_t max_devices,
                                     size_t* found) {
    *found = 0;
    
#ifdef UFT_HAS_OPENCBM
    CBM_FILE f;
    
    if (cbm_driver_open(&f, 0) < 0) {
        return UFT_OK;
    }
    
    for (int dev = 8; dev <= 11 && *found < max_devices; dev++) {
        if (cbm_talk(f, dev, 15) >= 0) {
            uint8_t status[64];
            int n = cbm_raw_read(f, status, sizeof(status) - 1);
            cbm_untalk(f);
            
            if (n > 0) {
                status[n] = 0;
                
                uft_hw_info_t* info = &devices[*found];
                memset(info, 0, sizeof(*info));
                
                info->type = UFT_HW_XUM1541;
                snprintf(info->name, sizeof(info->name), "CBM Drive #%d", dev);
                snprintf(info->serial, sizeof(info->serial), "%d", dev);
                
                if (strstr((char*)status, "1541")) {
                    snprintf(info->firmware, sizeof(info->firmware), "1541");
                } else if (strstr((char*)status, "1571")) {
                    snprintf(info->firmware, sizeof(info->firmware), "1571");
                } else if (strstr((char*)status, "1581")) {
                    snprintf(info->firmware, sizeof(info->firmware), "1581");
                }
                
                info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_WRITE | UFT_HW_CAP_MOTOR;
                (*found)++;
            }
        }
    }
    
    cbm_driver_close(f);
#else
    (void)devices; (void)max_devices;
#endif
    
    return UFT_OK;
}

static uft_error_t opencbm_open(const uft_hw_info_t* info, uft_hw_device_t** device) {
#ifdef UFT_HAS_OPENCBM
    opencbm_state_t* cbm = calloc(1, sizeof(opencbm_state_t));
    if (!cbm) return UFT_ERROR_NO_MEMORY;
    
    if (cbm_driver_open(&cbm->handle, 0) < 0) {
        free(cbm);
        return UFT_ERROR_FILE_OPEN;
    }
    
    { int32_t t; if(uft_parse_int32(info->serial,&t,10)) cbm->device_num=(uint8_t)t; }
    if (cbm->device_num < 8 || cbm->device_num > 11) {
        cbm->device_num = 8;
    }
    
    if (strstr(info->firmware, "1571")) {
        cbm->drive_type = 1;
    } else if (strstr(info->firmware, "1581")) {
        cbm->drive_type = 2;
    } else {
        cbm->drive_type = 0;
    }
    
    (*device)->handle = cbm;
    return UFT_OK;
#else
    (void)info; (void)device;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static void opencbm_close(uft_hw_device_t* device) {
    if (!device || !device->handle) return;
    
    opencbm_state_t* cbm = device->handle;
    
#ifdef UFT_HAS_OPENCBM
    cbm_driver_close(cbm->handle);
#endif
    
    free(cbm);
    device->handle = NULL;
}

static uft_error_t opencbm_motor(uft_hw_device_t* device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    opencbm_state_t* cbm = device->handle;
    
#ifdef UFT_HAS_OPENCBM
    if (on) {
        uint8_t cmd[] = "I0";
        cbm_exec_command(cbm->handle, cbm->device_num, cmd, 2);
    }
    cbm->motor_on = on;
    return UFT_OK;
#else
    (void)cbm; (void)on;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static uft_error_t opencbm_seek(uft_hw_device_t* device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    opencbm_state_t* cbm = device->handle;
    
#ifdef UFT_HAS_OPENCBM
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "U1:2 0 %d 0", track);
    
    if (cbm_listen(cbm->handle, cbm->device_num, 2) >= 0) {
        cbm_raw_write(cbm->handle, "#", 1);
        cbm_unlisten(cbm->handle);
    }
    
    cbm_exec_command(cbm->handle, cbm->device_num, cmd, strlen(cmd));
    cbm->current_track = track;
    return UFT_OK;
#else
    (void)cbm; (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static uft_error_t opencbm_read_track(uft_hw_device_t* device, 
                                       uft_track_t* track,
                                       uint8_t revolutions) {
    if (!device || !device->handle || !track) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    (void)revolutions;
    
    opencbm_state_t* cbm = device->handle;
    
#ifdef UFT_HAS_OPENCBM
    if (!cbm->GCR tools_loaded) {
        uft_error_t err = send_memory_execute(cbm, NIBTOOLS_CODE,
                                              GCR tools_read_code,
                                              sizeof(GCR tools_read_code));
        if (UFT_FAILED(err)) return err;
        cbm->GCR tools_loaded = true;
    }
    
    uint8_t exec_cmd[] = {'M', '-', 'E', 
                          (uint8_t)(NIBTOOLS_CODE & 0xFF),
                          (uint8_t)(NIBTOOLS_CODE >> 8)};
    
    if (cbm_exec_command(cbm->handle, cbm->device_num, 
                         exec_cmd, sizeof(exec_cmd)) < 0) {
        return UFT_ERROR_IO;
    }
    
    size_t track_size = 8000;
    uint8_t* gcr_data = malloc(track_size);
    if (!gcr_data) return UFT_ERROR_NO_MEMORY;
    
    uft_error_t err = read_drive_memory(cbm, NIBTOOLS_BUFFER, gcr_data, track_size);
    if (UFT_FAILED(err)) {
        free(gcr_data);
        return err;
    }
    
    track->raw_data = gcr_data;
    track->raw_size = track_size;
    track->encoding = UFT_ENC_GCR_CBM;
    track->status = UFT_TRACK_OK;
    
    return UFT_OK;
#else
    (void)cbm;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static uft_error_t opencbm_parallel_read(uft_hw_device_t* device,
                                          uint8_t* data, size_t len, 
                                          size_t* read_bytes) {
    (void)device; (void)data; (void)len; (void)read_bytes;
    return UFT_ERROR_NOT_SUPPORTED;
}

static uft_error_t opencbm_parallel_write(uft_hw_device_t* device,
                                           const uint8_t* data, size_t len) {
    (void)device; (void)data; (void)len;
    return UFT_ERROR_NOT_SUPPORTED;
}

static uft_error_t opencbm_iec_command(uft_hw_device_t* device,
                                        uint8_t device_num, uint8_t command,
                                        const uint8_t* data, size_t len) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    (void)command;
    
#ifdef UFT_HAS_OPENCBM
    opencbm_state_t* cbm = device->handle;
    return cbm_exec_command(cbm->handle, device_num, data, len) >= 0 
           ? UFT_OK : UFT_ERROR_IO;
#else
    (void)device_num; (void)data; (void)len;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

// ============================================================================
// Backend Definition
// ============================================================================

static const uft_hw_backend_t opencbm_backend = {
    .name = "CBM library/Nibtools",
    .type = UFT_HW_XUM1541,
    
    .init = opencbm_init,
    .shutdown = opencbm_shutdown,
    .enumerate = opencbm_enumerate,
    .open = opencbm_open,
    .close = opencbm_close,
    
    .get_status = NULL,
    .motor = opencbm_motor,
    .seek = opencbm_seek,
    .select_head = NULL,
    .select_density = NULL,
    
    .read_track = opencbm_read_track,
    .write_track = NULL,
    .read_flux = NULL,
    .write_flux = NULL,
    
    .parallel_write = opencbm_parallel_write,
    .parallel_read = opencbm_parallel_read,
    .iec_command = opencbm_iec_command,
    
    .private_data = NULL
};

uft_error_t uft_hw_register_opencbm(void) {
    return uft_hw_register_backend(&opencbm_backend);
}

const uft_hw_backend_t uft_hw_backend_opencbm = {
    .name = "CBM library/Nibtools",
    .type = UFT_HW_XUM1541,
    .init = opencbm_init,
    .shutdown = opencbm_shutdown,
    .enumerate = opencbm_enumerate,
    .open = opencbm_open,
    .close = opencbm_close,
    .motor = opencbm_motor,
    .seek = opencbm_seek,
    .read_track = opencbm_read_track,
    .parallel_write = opencbm_parallel_write,
    .parallel_read = opencbm_parallel_read,
    .iec_command = opencbm_iec_command,
    .private_data = NULL
};
