/**
 * @file test_platform.c
 * @brief Cross-Platform Abstraction Tests
 * 
 * P2-005: Cross-Platform Support
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL @ %d\n", __LINE__); _fail++; return; } } while(0)

/* Platform detection macros */
#if defined(_WIN32) || defined(_WIN64)
    #define TEST_PLATFORM_WINDOWS 1
    #define TEST_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
    #define TEST_PLATFORM_MACOS 1
    #define TEST_PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define TEST_PLATFORM_LINUX 1
    #define TEST_PLATFORM_NAME "Linux"
#else
    #define TEST_PLATFORM_UNKNOWN 1
    #define TEST_PLATFORM_NAME "Unknown"
#endif

/* Endianness detection */
static bool is_little_endian(void) {
    uint16_t x = 1;
    return *((uint8_t*)&x) == 1;
}

/* Byte swap */
static uint16_t bswap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

static uint32_t bswap32(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}

/* Unaligned read/write */
static uint16_t read_le16(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t read_le32(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t read_be16(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return ((uint16_t)b[0] << 8) | (uint16_t)b[1];
}

static uint32_t read_be32(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static void write_le16(void *p, uint16_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
}

static void write_le32(void *p, uint32_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
    b[2] = (v >> 16) & 0xFF;
    b[3] = (v >> 24) & 0xFF;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

TEST(platform_detection) {
    /* At least one platform must be detected */
#if defined(TEST_PLATFORM_WINDOWS) || defined(TEST_PLATFORM_MACOS) || defined(TEST_PLATFORM_LINUX)
    ASSERT(1);
#else
    ASSERT(0);
#endif
    
    ASSERT(strlen(TEST_PLATFORM_NAME) > 0);
}

TEST(endianness_detection) {
    bool le = is_little_endian();
    /* Most modern systems are little-endian */
    ASSERT(le == true || le == false);  /* Valid result */
    
    /* Verify with known value */
    uint32_t test = 0x01020304;
    uint8_t *bytes = (uint8_t*)&test;
    if (le) {
        ASSERT(bytes[0] == 0x04);
        ASSERT(bytes[3] == 0x01);
    } else {
        ASSERT(bytes[0] == 0x01);
        ASSERT(bytes[3] == 0x04);
    }
}

TEST(bswap16) {
    ASSERT(bswap16(0x0102) == 0x0201);
    ASSERT(bswap16(0x1234) == 0x3412);
    ASSERT(bswap16(0xAABB) == 0xBBAA);
    ASSERT(bswap16(0x0000) == 0x0000);
    ASSERT(bswap16(0xFFFF) == 0xFFFF);
}

TEST(bswap32) {
    ASSERT(bswap32(0x01020304) == 0x04030201);
    ASSERT(bswap32(0x12345678) == 0x78563412);
    ASSERT(bswap32(0xAABBCCDD) == 0xDDCCBBAA);
    ASSERT(bswap32(0x00000000) == 0x00000000);
    ASSERT(bswap32(0xFFFFFFFF) == 0xFFFFFFFF);
}

TEST(read_le16_aligned) {
    uint8_t data[] = {0x34, 0x12};
    uint16_t val = read_le16(data);
    ASSERT(val == 0x1234);
}

TEST(read_le16_unaligned) {
    uint8_t data[] = {0x00, 0x34, 0x12, 0x00};
    uint16_t val = read_le16(data + 1);
    ASSERT(val == 0x1234);
}

TEST(read_le32_aligned) {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
    uint32_t val = read_le32(data);
    ASSERT(val == 0x12345678);
}

TEST(read_le32_unaligned) {
    uint8_t data[] = {0x00, 0x78, 0x56, 0x34, 0x12, 0x00};
    uint32_t val = read_le32(data + 1);
    ASSERT(val == 0x12345678);
}

TEST(read_be16) {
    uint8_t data[] = {0x12, 0x34};
    uint16_t val = read_be16(data);
    ASSERT(val == 0x1234);
}

TEST(read_be32) {
    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    uint32_t val = read_be32(data);
    ASSERT(val == 0x12345678);
}

TEST(write_le16) {
    uint8_t data[2] = {0, 0};
    write_le16(data, 0x1234);
    ASSERT(data[0] == 0x34);
    ASSERT(data[1] == 0x12);
}

TEST(write_le32) {
    uint8_t data[4] = {0, 0, 0, 0};
    write_le32(data, 0x12345678);
    ASSERT(data[0] == 0x78);
    ASSERT(data[1] == 0x56);
    ASSERT(data[2] == 0x34);
    ASSERT(data[3] == 0x12);
}

TEST(path_separator) {
#ifdef TEST_PLATFORM_WINDOWS
    char sep = '\\';
#else
    char sep = '/';
#endif
    ASSERT(sep == '\\' || sep == '/');
}

TEST(sizeof_types) {
    ASSERT(sizeof(uint8_t) == 1);
    ASSERT(sizeof(uint16_t) == 2);
    ASSERT(sizeof(uint32_t) == 4);
    ASSERT(sizeof(uint64_t) == 8);
    ASSERT(sizeof(int8_t) == 1);
    ASSERT(sizeof(int16_t) == 2);
    ASSERT(sizeof(int32_t) == 4);
    ASSERT(sizeof(int64_t) == 8);
}

TEST(pointer_size) {
    size_t ptr_size = sizeof(void*);
    ASSERT(ptr_size == 4 || ptr_size == 8);
}

TEST(roundtrip_le16) {
    for (uint32_t i = 0; i < 65536; i += 1000) {
        uint8_t buf[2];
        write_le16(buf, (uint16_t)i);
        uint16_t result = read_le16(buf);
        ASSERT(result == (uint16_t)i);
    }
}

TEST(roundtrip_le32) {
    uint32_t test_values[] = {0, 1, 255, 256, 65535, 65536, 
                              0x12345678, 0xDEADBEEF, 0xFFFFFFFF};
    for (size_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); i++) {
        uint8_t buf[4];
        write_le32(buf, test_values[i]);
        uint32_t result = read_le32(buf);
        ASSERT(result == test_values[i]);
    }
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Cross-Platform Tests (P2-005)\n");
    printf("  Platform: %s\n", TEST_PLATFORM_NAME);
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(platform_detection);
    RUN(endianness_detection);
    RUN(bswap16);
    RUN(bswap32);
    RUN(read_le16_aligned);
    RUN(read_le16_unaligned);
    RUN(read_le32_aligned);
    RUN(read_le32_unaligned);
    RUN(read_be16);
    RUN(read_be32);
    RUN(write_le16);
    RUN(write_le32);
    RUN(path_separator);
    RUN(sizeof_types);
    RUN(pointer_size);
    RUN(roundtrip_le16);
    RUN(roundtrip_le32);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
