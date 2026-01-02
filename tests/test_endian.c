/**
 * @file test_endian.c
 * @brief Unit tests for endianness-safe binary I/O
 */

#include "uft/uft_endian.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define TEST(name) \
    do { \
        printf("[TEST] %s...", #name); \
        fflush(stdout); \
        test_##name(); \
        printf(" OK\n"); \
    } while(0)

void test_read_le16()
{
    uint8_t buf[2] = {0x34, 0x12};
    uint16_t value = uft_read_le16(buf);
    assert(value == 0x1234);
}

void test_read_le32()
{
    uint8_t buf[4] = {0x78, 0x56, 0x34, 0x12};
    uint32_t value = uft_read_le32(buf);
    assert(value == 0x12345678);
}

void test_read_le64()
{
    uint8_t buf[8] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    uint64_t value = uft_read_le64(buf);
    assert(value == 0x1122334455667788ULL);
}

void test_read_be16()
{
    uint8_t buf[2] = {0x12, 0x34};
    uint16_t value = uft_read_be16(buf);
    assert(value == 0x1234);
}

void test_read_be32()
{
    uint8_t buf[4] = {0x12, 0x34, 0x56, 0x78};
    uint32_t value = uft_read_be32(buf);
    assert(value == 0x12345678);
}

void test_read_be64()
{
    uint8_t buf[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint64_t value = uft_read_be64(buf);
    assert(value == 0x1122334455667788ULL);
}

void test_write_le16()
{
    uint8_t buf[2];
    uft_write_le16(buf, 0x1234);
    assert(buf[0] == 0x34 && buf[1] == 0x12);
}

void test_write_le32()
{
    uint8_t buf[4];
    uft_write_le32(buf, 0x12345678);
    assert(buf[0] == 0x78 && buf[1] == 0x56 && buf[2] == 0x34 && buf[3] == 0x12);
}

void test_write_be16()
{
    uint8_t buf[2];
    uft_write_be16(buf, 0x1234);
    assert(buf[0] == 0x12 && buf[1] == 0x34);
}

void test_write_be32()
{
    uint8_t buf[4];
    uft_write_be32(buf, 0x12345678);
    assert(buf[0] == 0x12 && buf[1] == 0x34 && buf[2] == 0x56 && buf[3] == 0x78);
}

void test_roundtrip_le()
{
    uint8_t buf[8];
    uint32_t original = 0xDEADBEEF;
    
    uft_write_le32(buf, original);
    uint32_t readback = uft_read_le32(buf);
    
    assert(readback == original);
}

void test_roundtrip_be()
{
    uint8_t buf[8];
    uint32_t original = 0xCAFEBABE;
    
    uft_write_be32(buf, original);
    uint32_t readback = uft_read_be32(buf);
    
    assert(readback == original);
}

int main()
{
    printf("=== Endianness Helper Tests ===\n");
    
    TEST(read_le16);
    TEST(read_le32);
    TEST(read_le64);
    TEST(read_be16);
    TEST(read_be32);
    TEST(read_be64);
    TEST(write_le16);
    TEST(write_le32);
    TEST(write_be16);
    TEST(write_be32);
    TEST(roundtrip_le);
    TEST(roundtrip_be);
    
    printf("\nAll tests passed! ✅\n");
    return 0;
}
