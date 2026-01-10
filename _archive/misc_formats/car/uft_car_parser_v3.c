/**
 * @file uft_car_parser_v3.c
 * @brief GOD MODE CAR Parser v3 - Atari 8-bit Cartridge
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CAR_MAGIC               "CART"

typedef struct {
    char signature[5];
    uint32_t type;
    uint32_t checksum;
    uint32_t unused;
    size_t source_size;
    bool valid;
} car_file_t;

static uint32_t car_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool car_parse(const uint8_t* data, size_t size, car_file_t* car) {
    if (!data || !car || size < 16) return false;
    memset(car, 0, sizeof(car_file_t));
    car->source_size = size;
    
    if (memcmp(data, CAR_MAGIC, 4) == 0) {
        memcpy(car->signature, data, 4);
        car->signature[4] = '\0';
        car->type = car_read_be32(data + 4);
        car->checksum = car_read_be32(data + 8);
        car->valid = true;
    }
    return true;
}

#ifdef CAR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari CAR Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t car[32] = {'C', 'A', 'R', 'T', 0, 0, 0, 1};
    car_file_t file;
    assert(car_parse(car, sizeof(car), &file) && file.type == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
