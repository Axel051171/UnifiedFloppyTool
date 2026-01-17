/**
 * Standalone Greaseweazle connection test
 * Compile: gcc -o gw_test gw_test.c -I../include
 * Usage: gw_test COM4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif

/* Greaseweazle commands */
#define CMD_GET_INFO    0x00

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        printf("Example: %s COM4\n", argv[0]);
        return 1;
    }
    
    const char* port = argv[1];
    
#ifdef _WIN32
    /* Windows implementation */
    char full_port[280];
    snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port);
    
    printf("[1] Opening port: %s\n", full_port);
    
    HANDLE h = CreateFileA(full_port, GENERIC_READ | GENERIC_WRITE,
                          0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("ERROR: CreateFileA failed, error: %lu\n", GetLastError());
        return 1;
    }
    printf("    OK - handle opened\n");
    
    /* Configure port */
    printf("[2] Configuring serial port...\n");
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(h, &dcb);
    dcb.BaudRate = 115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    if (!SetCommState(h, &dcb)) {
        printf("ERROR: SetCommState failed\n");
        CloseHandle(h);
        return 1;
    }
    printf("    OK\n");
    
    /* Set timeouts */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadTotalTimeoutConstant = 3000;
    timeouts.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(h, &timeouts);
    
    /* Purge any pending data */
    printf("[3] Purging buffers...\n");
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    printf("    OK\n");
    
    /* DTR reset cycle */
    printf("[4] DTR reset cycle...\n");
    EscapeCommFunction(h, CLRDTR);
    Sleep(50);
    EscapeCommFunction(h, SETDTR);
    Sleep(300);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    printf("    OK\n");
    
    /* Send GET_INFO command */
    printf("[5] Sending GET_INFO command: 00 04 00 00\n");
    uint8_t cmd[4] = {0x00, 0x04, 0x00, 0x00};  /* cmd=0, len=4, subindex=0 */
    DWORD written;
    if (!WriteFile(h, cmd, 4, &written, NULL) || written != 4) {
        printf("ERROR: WriteFile failed, error: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    FlushFileBuffers(h);
    printf("    Sent %lu bytes\n", written);
    
    /* Read response */
    printf("[6] Reading response (3 second timeout)...\n");
    uint8_t resp[64] = {0};
    DWORD read_bytes;
    if (!ReadFile(h, resp, sizeof(resp), &read_bytes, NULL)) {
        printf("ERROR: ReadFile failed, error: %lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    
    printf("    Received %lu bytes: ", read_bytes);
    for (DWORD i = 0; i < read_bytes && i < 32; i++) {
        printf("%02X ", resp[i]);
    }
    printf("\n");
    
    if (read_bytes < 2) {
        printf("ERROR: Timeout or no response\n");
        CloseHandle(h);
        return 1;
    }
    
    /* Parse response */
    printf("[7] Parsing response...\n");
    if (resp[0] != 0x00) {
        printf("ERROR: Unexpected command echo: 0x%02X (expected 0x00)\n", resp[0]);
        CloseHandle(h);
        return 1;
    }
    if (resp[1] != 0x00) {
        printf("ERROR: Device returned error code: 0x%02X\n", resp[1]);
        CloseHandle(h);
        return 1;
    }
    
    if (read_bytes >= 10) {
        uint8_t fw_major = resp[2];
        uint8_t fw_minor = resp[3];
        uint8_t is_main = resp[4];
        uint8_t max_cmd = resp[5];
        uint32_t sample_freq = resp[6] | (resp[7] << 8) | (resp[8] << 16) | (resp[9] << 24);
        
        printf("\n=== Greaseweazle Connected! ===\n");
        printf("Firmware: v%d.%d\n", fw_major, fw_minor);
        printf("Main FW:  %s\n", is_main ? "Yes" : "No (bootloader)");
        printf("Max Cmd:  0x%02X\n", max_cmd);
        printf("Sample:   %u Hz\n", sample_freq);
        
        if (read_bytes >= 11) {
            printf("Model:    %d\n", resp[10]);
        }
    }
    
    CloseHandle(h);
    printf("\nConnection test PASSED!\n");
    
#else
    /* POSIX implementation */
    printf("[1] Opening port: %s\n", port);
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("ERROR: open failed");
        return 1;
    }
    printf("    OK\n");
    
    /* Configure port */
    struct termios tty;
    tcgetattr(fd, &tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag = CS8 | CREAD | CLOCAL;
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VTIME] = 30;  /* 3 second timeout */
    tty.c_cc[VMIN] = 0;
    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIOFLUSH);
    
    /* Send command and read response similar to Windows */
    printf("[2] Sending GET_INFO...\n");
    uint8_t cmd[4] = {0x00, 0x04, 0x00, 0x00};
    write(fd, cmd, 4);
    tcdrain(fd);
    
    printf("[3] Reading response...\n");
    uint8_t resp[64];
    ssize_t n = read(fd, resp, sizeof(resp));
    printf("    Received %zd bytes\n", n);
    
    close(fd);
#endif
    
    return 0;
}
