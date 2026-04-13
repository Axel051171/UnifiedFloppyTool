/**
 * @file test_crc16.c
 * @brief WHDLoad CRC-16 test
 * @version 3.8.0
 */
#include "uft/whdload/whd_crc16.h"
#include <stdio.h>
#include <string.h>

int main(void){
    const char *s = "123456789";
    uint16_t crc = uft_crc16_ansi(s, strlen(s));
    /* CRC-16/IBM (ANSI) of "123456789" is 0xBB3D with init 0x0000 */
    if (crc != 0xBB3Du) {
        fprintf(stderr, "crc16 mismatch: got 0x%04X expected 0xBB3D\n", crc);
        return 1;
    }
    printf("ok crc16=0x%04X\n", crc);
    return 0;
}
