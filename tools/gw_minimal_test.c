/**
 * @file gw_minimal_test.c
 * @brief Minimal Greaseweazle test - matches pyserial behavior exactly
 * 
 * Compile: gcc -o gw_minimal_test.exe gw_minimal_test.c
 * Usage:   gw_minimal_test.exe COM4
 * 
 * This test program:
 * 1. Opens the port exactly like pyserial does
 * 2. Sends GET_INFO command
 * 3. Reads and displays full response
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

/* Greaseweazle commands */
#define GW_CMD_GET_INFO     0x00
#define GW_CMD_UPDATE       0x01
#define GW_CMD_SEEK         0x02
#define GW_CMD_HEAD         0x03
#define GW_CMD_SET_PARAMS   0x04
#define GW_CMD_GET_PARAMS   0x05
#define GW_CMD_MOTOR        0x06
#define GW_CMD_READ_FLUX    0x07
#define GW_CMD_WRITE_FLUX   0x08
#define GW_CMD_GET_FLUX_STATUS 0x09
#define GW_CMD_SELECT       0x12
#define GW_CMD_DESELECT     0x13
#define GW_CMD_SET_BUS_TYPE 0x14
#define GW_CMD_SET_PIN      0x15
#define GW_CMD_RESET        0x16
#define GW_CMD_ERASE_FLUX   0x17
#define GW_CMD_SOURCE_BYTES 0x18
#define GW_CMD_SINK_BYTES   0x19
#define GW_CMD_GET_PIN      0x1a
#define GW_CMD_SWITCH_FW_MODE 0x1b

/* ACK codes */
#define GW_ACK_OK           0x00
#define GW_ACK_BAD_COMMAND  0x01
#define GW_ACK_NO_INDEX     0x02
#define GW_ACK_NO_TRK0      0x03
#define GW_ACK_FLUX_OVERFLOW 0x04
#define GW_ACK_FLUX_UNDERFLOW 0x05
#define GW_ACK_WRPROT       0x06
#define GW_ACK_NO_UNIT      0x07
#define GW_ACK_NO_BUS       0x08
#define GW_ACK_BAD_UNIT     0x09
#define GW_ACK_BAD_PIN      0x0a
#define GW_ACK_BAD_CYLINDER 0x0b

static void hexdump(const char* prefix, const uint8_t* data, size_t len) {
    printf("%s (%zu bytes): ", prefix, len);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

static const char* ack_name(uint8_t ack) {
    switch (ack) {
        case GW_ACK_OK: return "OK";
        case GW_ACK_BAD_COMMAND: return "BAD_COMMAND";
        case GW_ACK_NO_INDEX: return "NO_INDEX";
        case GW_ACK_NO_TRK0: return "NO_TRK0";
        case GW_ACK_FLUX_OVERFLOW: return "FLUX_OVERFLOW";
        case GW_ACK_FLUX_UNDERFLOW: return "FLUX_UNDERFLOW";
        case GW_ACK_WRPROT: return "WRPROT";
        case GW_ACK_NO_UNIT: return "NO_UNIT";
        case GW_ACK_NO_BUS: return "NO_BUS";
        case GW_ACK_BAD_UNIT: return "BAD_UNIT";
        case GW_ACK_BAD_PIN: return "BAD_PIN";
        case GW_ACK_BAD_CYLINDER: return "BAD_CYLINDER";
        default: return "UNKNOWN";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s COMx\n", argv[0]);
        printf("Example: %s COM4\n", argv[0]);
        return 1;
    }
    
    char port_path[256];
    snprintf(port_path, sizeof(port_path), "\\\\.\\%s", argv[1]);
    
    printf("==============================================\n");
    printf("Greaseweazle Minimal Test\n");
    printf("==============================================\n\n");
    
    /* Step 1: Open port - EXACTLY like pyserial */
    printf("[1] Opening port: %s\n", port_path);
    
    HANDLE h = CreateFileA(
        port_path,
        GENERIC_READ | GENERIC_WRITE,
        0,                  /* no sharing */
        NULL,               /* no security */
        OPEN_EXISTING,
        0,                  /* no flags */
        NULL                /* no template */
    );
    
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("    FAILED! Error: %lu\n", err);
        if (err == 2) printf("    Port does not exist\n");
        if (err == 5) printf("    Access denied (port in use?)\n");
        return 1;
    }
    printf("    OK - Handle opened\n");
    
    /* Step 2: Configure DCB - match pyserial defaults */
    printf("\n[2] Configuring serial port (pyserial defaults)...\n");
    
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    
    if (!GetCommState(h, &dcb)) {
        printf("    GetCommState FAILED!\n");
        CloseHandle(h);
        return 1;
    }
    
    /* pyserial defaults for Serial(port, baudrate=9600) */
    dcb.BaudRate = CBR_9600;      /* CDC ACM ignores this */
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    
    /* CRITICAL: pyserial defaults - no flow control */
    dcb.fDtrControl = DTR_CONTROL_DISABLE;  /* dsrdtr=False (pyserial default) */
    dcb.fRtsControl = RTS_CONTROL_DISABLE;  /* rtscts=False (pyserial default) */
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;                       /* xonxoff=False */
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    
    if (!SetCommState(h, &dcb)) {
        printf("    SetCommState FAILED!\n");
        CloseHandle(h);
        return 1;
    }
    printf("    DCB: 9600 8N1, no flow control\n");
    
    /* Step 3: Set timeouts - pyserial style with timeout=2.0 */
    printf("\n[3] Setting timeouts...\n");
    
    COMMTIMEOUTS timeouts;
    /* pyserial with timeout=2.0:
     * ReadIntervalTimeout = 0
     * ReadTotalTimeoutMultiplier = 0
     * ReadTotalTimeoutConstant = timeout * 1000
     */
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 2000;   /* 2 second read timeout */
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 2000;  /* 2 second write timeout */
    
    if (!SetCommTimeouts(h, &timeouts)) {
        printf("    SetCommTimeouts FAILED!\n");
        CloseHandle(h);
        return 1;
    }
    printf("    Timeouts: read=2000ms, write=2000ms\n");
    
    /* Step 4: Clear buffers */
    printf("\n[4] Clearing buffers...\n");
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    printf("    Done\n");
    
    /* Step 5: Try to drain any pending data */
    printf("\n[5] Draining pending data...\n");
    {
        /* Quick non-blocking read */
        COMMTIMEOUTS quick_to;
        quick_to.ReadIntervalTimeout = MAXDWORD;
        quick_to.ReadTotalTimeoutMultiplier = MAXDWORD;
        quick_to.ReadTotalTimeoutConstant = 100;  /* 100ms max */
        quick_to.WriteTotalTimeoutMultiplier = 0;
        quick_to.WriteTotalTimeoutConstant = 0;
        SetCommTimeouts(h, &quick_to);
        
        uint8_t drain[256];
        DWORD drained;
        while (ReadFile(h, drain, sizeof(drain), &drained, NULL) && drained > 0) {
            hexdump("    Drained", drain, drained);
        }
        
        /* Restore normal timeouts */
        SetCommTimeouts(h, &timeouts);
    }
    printf("    Done\n");
    
    /* Step 6: Send GET_INFO command */
    printf("\n[6] Sending GET_INFO command...\n");
    
    /* Protocol: cmd(1) + len(1) + subindex(2) = 4 bytes */
    uint8_t get_info_cmd[4] = {GW_CMD_GET_INFO, 0x04, 0x00, 0x00};
    hexdump("    TX", get_info_cmd, 4);
    
    DWORD written;
    if (!WriteFile(h, get_info_cmd, 4, &written, NULL) || written != 4) {
        printf("    WriteFile FAILED!\n");
        CloseHandle(h);
        return 1;
    }
    printf("    Sent %lu bytes\n", written);
    
    /* Step 7: Read response */
    printf("\n[7] Reading response...\n");
    
    /* Wait a bit for device to process */
    Sleep(100);
    
    uint8_t resp[256];
    DWORD bytes_read;
    if (!ReadFile(h, resp, sizeof(resp), &bytes_read, NULL)) {
        printf("    ReadFile FAILED! Error: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    
    if (bytes_read == 0) {
        printf("    No response (timeout)\n");
        CloseHandle(h);
        return 1;
    }
    
    hexdump("    RX", resp, bytes_read);
    
    /* Step 8: Parse response */
    printf("\n[8] Parsing response...\n");
    
    if (bytes_read < 2) {
        printf("    Response too short!\n");
        CloseHandle(h);
        return 1;
    }
    
    printf("    Echo: 0x%02X (expected 0x%02X)\n", resp[0], GW_CMD_GET_INFO);
    printf("    ACK:  0x%02X (%s)\n", resp[1], ack_name(resp[1]));
    
    if (resp[0] != GW_CMD_GET_INFO) {
        printf("    ERROR: Wrong echo!\n");
        CloseHandle(h);
        return 1;
    }
    
    if (resp[1] != GW_ACK_OK) {
        printf("    ERROR: Command rejected!\n");
        
        /* If BAD_COMMAND, try without subindex (old protocol) */
        if (resp[1] == GW_ACK_BAD_COMMAND) {
            printf("\n[8b] Trying old protocol (no subindex)...\n");
            
            PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
            
            uint8_t old_cmd[2] = {GW_CMD_GET_INFO, 0x02};
            hexdump("    TX", old_cmd, 2);
            
            if (!WriteFile(h, old_cmd, 2, &written, NULL) || written != 2) {
                printf("    WriteFile FAILED!\n");
                CloseHandle(h);
                return 1;
            }
            
            Sleep(100);
            
            if (!ReadFile(h, resp, sizeof(resp), &bytes_read, NULL) || bytes_read == 0) {
                printf("    No response!\n");
                CloseHandle(h);
                return 1;
            }
            
            hexdump("    RX", resp, bytes_read);
            printf("    Echo: 0x%02X\n", resp[0]);
            printf("    ACK:  0x%02X (%s)\n", resp[1], ack_name(resp[1]));
            
            if (resp[1] != GW_ACK_OK) {
                printf("    Still failing - device may need USB unplug/replug\n");
                CloseHandle(h);
                return 1;
            }
        } else {
            CloseHandle(h);
            return 1;
        }
    }
    
    /* Parse firmware info */
    if (bytes_read >= 10) {
        printf("\n[9] Device Information:\n");
        printf("    Firmware:    v%d.%d\n", resp[2], resp[3]);
        printf("    Is Main FW:  %s\n", resp[4] ? "Yes" : "No (bootloader)");
        printf("    Max Command: 0x%02X\n", resp[5]);
        
        uint32_t sample_freq = (uint32_t)resp[6] | 
                               ((uint32_t)resp[7] << 8) |
                               ((uint32_t)resp[8] << 16) |
                               ((uint32_t)resp[9] << 24);
        printf("    Sample Freq: %u Hz\n", sample_freq);
        
        if (bytes_read >= 11) printf("    HW Model:    %d\n", resp[10]);
        if (bytes_read >= 12) printf("    HW Submodel: %d\n", resp[11]);
        if (bytes_read >= 13) printf("    USB Speed:   %d\n", resp[12]);
        
        printf("\n==============================================\n");
        printf("SUCCESS!\n");
        printf("==============================================\n");
    }
    
    CloseHandle(h);
    return 0;
}
