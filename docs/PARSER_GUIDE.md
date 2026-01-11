# UFT Parser Development Guide

**Version:** 3.3.2  
**Date:** 2025-01-03

## Overview

This guide explains how to write format parsers for UnifiedFloppyTool. Parsers convert between raw disk formats and UFT's internal representation.

## Parser Architecture

### Driver Interface

Every format parser implements the driver interface:

```c
typedef struct uft_format_driver {
    const char*     name;           // "ADF", "D64", "SCP", etc.
    const char*     description;    // Human-readable description
    const char**    extensions;     // File extensions (NULL-terminated)
    uint32_t        flags;          // Capabilities flags
    
    // Core operations
    uft_error_t (*probe)(const uint8_t* data, size_t size, int* confidence);
    uft_error_t (*open)(const char* path, uft_disk_t** disk);
    uft_error_t (*close)(uft_disk_t** disk);
    
    // Read operations
    uft_error_t (*read_track)(uft_disk_t* disk, int track, int head, uft_track_t** out);
    uft_error_t (*read_sector)(uft_disk_t* disk, int track, int head, int sector, 
                               uint8_t* buffer, size_t size);
    
    // Write operations (optional)
    uft_error_t (*write_track)(uft_disk_t* disk, int track, int head, const uft_track_t* in);
    uft_error_t (*write_sector)(uft_disk_t* disk, int track, int head, int sector,
                                const uint8_t* buffer, size_t size);
    
    // Metadata
    uft_error_t (*get_info)(uft_disk_t* disk, uft_disk_info_t* info);
    uft_error_t (*analyze)(uft_disk_t* disk, char* report, size_t report_size);
} uft_format_driver_t;
```

### Capability Flags

```c
#define UFT_FMT_FLAG_READ       0x0001  // Can read
#define UFT_FMT_FLAG_WRITE      0x0002  // Can write
#define UFT_FMT_FLAG_FLUX       0x0004  // Contains raw flux data
#define UFT_FMT_FLAG_SECTOR     0x0008  // Sector-based format
#define UFT_FMT_FLAG_TRACK_IMG  0x0010  // Track image format
#define UFT_FMT_FLAG_PROTECTED  0x0020  // May contain copy protection
#define UFT_FMT_FLAG_MULTI_REV  0x0040  // Multi-revolution support
#define UFT_FMT_FLAG_TIMING     0x0080  // Contains timing data
#define UFT_FMT_FLAG_WEAK_BITS  0x0100  // Supports weak bits
#define UFT_FMT_FLAG_VARIABLE   0x0200  // Variable geometry
```

## Implementing a Parser

### Step 1: Define Context Structure

```c
typedef struct my_parser_ctx {
    FILE*       fp;
    uint8_t*    header;
    size_t      header_size;
    
    // Geometry
    int         tracks;
    int         heads;
    int         sectors_per_track;
    int         sector_size;
    
    // Format-specific
    uint32_t*   track_offsets;
    bool        is_extended;
    
    // I/O abstraction
    uft_io_t*   io;
} my_parser_ctx_t;
```

### Step 2: Implement Probe Function

The probe function detects if data matches the format:

```c
static uft_error_t my_probe(const uint8_t* data, size_t size, int* confidence)
{
    *confidence = 0;
    
    // Check minimum size
    if (size < MY_HEADER_SIZE) {
        return UFT_ERR_OK;  // Not an error, just doesn't match
    }
    
    // Check magic bytes
    if (memcmp(data, "MYFORMAT", 8) != 0) {
        return UFT_ERR_OK;
    }
    
    // Additional validation
    uint16_t version = read_le16(data + 8);
    if (version > MY_MAX_VERSION) {
        *confidence = 30;  // Might be newer version
        return UFT_ERR_OK;
    }
    
    // Full match
    *confidence = 100;
    return UFT_ERR_OK;
}
```

**Confidence Levels:**

| Score | Meaning |
|-------|---------|
| 0-25 | Unlikely match |
| 26-50 | Possible match |
| 51-75 | Probable match |
| 76-99 | Strong match |
| 100 | Definite match |

### Step 3: Implement Open Function

```c
static uft_error_t my_open(const char* path, uft_disk_t** disk)
{
    if (!path || !disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    // Allocate context
    my_parser_ctx_t* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return UFT_ERR_OUT_OF_MEMORY;
    }
    
    // Open file
    ctx->fp = fopen(path, "rb");
    if (!ctx->fp) {
        free(ctx);
        return UFT_ERR_IO_ERROR;
    }
    
    // Read header
    ctx->header = malloc(MY_HEADER_SIZE);
    if (!ctx->header) {
        fclose(ctx->fp);
        free(ctx);
        return UFT_ERR_OUT_OF_MEMORY;
    }
    
    if (fread(ctx->header, 1, MY_HEADER_SIZE, ctx->fp) != MY_HEADER_SIZE) {
        free(ctx->header);
        fclose(ctx->fp);
        free(ctx);
        return UFT_ERR_IO_ERROR;
    }
    
    // Parse header
    uft_error_t err = parse_header(ctx);
    if (err != UFT_ERR_OK) {
        free(ctx->header);
        fclose(ctx->fp);
        free(ctx);
        return err;
    }
    
    // Create disk object
    uft_disk_t* d = calloc(1, sizeof(*d));
    if (!d) {
        free(ctx->header);
        fclose(ctx->fp);
        free(ctx);
        return UFT_ERR_OUT_OF_MEMORY;
    }
    
    d->format = UFT_FORMAT_MY_FORMAT;
    d->geometry.tracks = ctx->tracks;
    d->geometry.heads = ctx->heads;
    d->geometry.sectors_per_track = ctx->sectors_per_track;
    d->geometry.sector_size = ctx->sector_size;
    d->private_data = ctx;
    
    *disk = d;
    return UFT_ERR_OK;
}
```

### Step 4: Implement Read Functions

```c
static uft_error_t my_read_sector(
    uft_disk_t* disk,
    int track,
    int head,
    int sector,
    uint8_t* buffer,
    size_t size)
{
    if (!disk || !buffer) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    my_parser_ctx_t* ctx = disk->private_data;
    
    // Validate parameters
    if (track < 0 || track >= ctx->tracks ||
        head < 0 || head >= ctx->heads ||
        sector < 0 || sector >= ctx->sectors_per_track) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    // Calculate offset
    size_t offset = calculate_sector_offset(ctx, track, head, sector);
    
    // Seek and read
    if (fseek(ctx->fp, offset, SEEK_SET) != 0) {
        return UFT_ERR_IO_ERROR;
    }
    
    size_t to_read = (size < ctx->sector_size) ? size : ctx->sector_size;
    if (fread(buffer, 1, to_read, ctx->fp) != to_read) {
        return UFT_ERR_IO_ERROR;
    }
    
    return UFT_ERR_OK;
}
```

### Step 5: Implement Close Function

```c
static uft_error_t my_close(uft_disk_t** disk)
{
    if (!disk || !*disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_disk_t* d = *disk;
    my_parser_ctx_t* ctx = d->private_data;
    
    if (ctx) {
        if (ctx->fp) {
            fclose(ctx->fp);
        }
        free(ctx->header);
        free(ctx->track_offsets);
        free(ctx);
    }
    
    free(d->comment);
    free(d);
    *disk = NULL;
    
    return UFT_ERR_OK;
}
```

### Step 6: Register Driver

```c
static const char* my_extensions[] = {".myf", ".myfmt", NULL};

static const uft_format_driver_t my_driver = {
    .name = "MyFormat",
    .description = "My Custom Floppy Format",
    .extensions = my_extensions,
    .flags = UFT_FMT_FLAG_READ | UFT_FMT_FLAG_WRITE | UFT_FMT_FLAG_SECTOR,
    
    .probe = my_probe,
    .open = my_open,
    .close = my_close,
    .read_track = my_read_track,
    .read_sector = my_read_sector,
    .write_track = NULL,  // Not implemented
    .write_sector = my_write_sector,
    .get_info = my_get_info,
    .analyze = my_analyze,
};

// Registration function
uft_error_t uft_register_my_format(void)
{
    return uft_format_register(&my_driver);
}
```

## Best Practices

### 1. Error Handling

Always check return values and clean up on error:

```c
uft_error_t func(void)
{
    void* a = NULL;
    void* b = NULL;
    uft_error_t err = UFT_ERR_OK;
    
    a = malloc(100);
    if (!a) {
        err = UFT_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    b = malloc(200);
    if (!b) {
        err = UFT_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    // ... work with a and b ...
    
cleanup:
    free(b);
    free(a);
    return err;
}
```

### 2. Endianness

Always use explicit endian conversion:

```c
#include "uft/core/uft_endian.h"

// Reading little-endian values
uint16_t val16 = uft_read_le16(buffer);
uint32_t val32 = uft_read_le32(buffer);

// Reading big-endian values
uint16_t val16 = uft_read_be16(buffer);
uint32_t val32 = uft_read_be32(buffer);

// Writing
uft_write_le16(buffer, val16);
uft_write_be32(buffer, val32);
```

### 3. Bounds Checking

Always validate array accesses:

```c
static uft_error_t read_track(ctx_t* ctx, int track)
{
    // Bounds check
    if (track < 0 || track >= ctx->track_count) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    // Safe array access
    uint32_t offset = ctx->track_offsets[track];
    
    // Validate offset
    if (offset >= ctx->file_size) {
        return UFT_ERR_FORMAT_ERROR;
    }
    
    // ...
}
```

### 4. Integer Overflow Protection

```c
#include "uft/core/uft_safe_math.h"

// Safe multiplication
size_t total;
if (!uft_safe_mul_size(track_count, track_size, &total)) {
    return UFT_ERR_OVERFLOW;
}

// Safe addition
if (!uft_safe_add_size(header_size, data_size, &total)) {
    return UFT_ERR_OVERFLOW;
}
```

### 5. Memory Ownership

Follow caller-owns-buffer pattern:

```c
// Caller provides buffer
uft_error_t read_sector(ctx_t* ctx, uint8_t* buffer, size_t size);

// Caller frees returned pointer
uft_error_t get_comment(ctx_t* ctx, char** comment_out);

// Function frees internal pointer
uft_error_t close(ctx_t** ctx);  // Sets *ctx to NULL
```

## Flux Format Parsers

For formats containing raw flux data (SCP, HFE, KryoFlux):

### Reading Flux Data

```c
typedef struct uft_flux_track {
    uint32_t*   flux_times;     // Transition times in nanoseconds
    size_t      count;          // Number of transitions
    uint32_t    index_time;     // Index-to-index time
    double      rpm;            // Calculated RPM
} uft_flux_track_t;

static uft_error_t read_flux_track(
    ctx_t* ctx,
    int track,
    int head,
    uft_flux_track_t** out)
{
    // Allocate flux track
    uft_flux_track_t* ft = calloc(1, sizeof(*ft));
    if (!ft) {
        return UFT_ERR_OUT_OF_MEMORY;
    }
    
    // Read raw flux data from file
    size_t raw_size;
    uint8_t* raw = read_track_raw(ctx, track, head, &raw_size);
    if (!raw) {
        free(ft);
        return UFT_ERR_IO_ERROR;
    }
    
    // Convert to nanoseconds
    ft->count = raw_size / sizeof(uint16_t);
    ft->flux_times = malloc(ft->count * sizeof(uint32_t));
    if (!ft->flux_times) {
        free(raw);
        free(ft);
        return UFT_ERR_OUT_OF_MEMORY;
    }
    
    // Convert from format-specific resolution to nanoseconds
    uint32_t resolution_ns = ctx->tick_ns;  // e.g., 25ns for SCP
    const uint16_t* raw16 = (const uint16_t*)raw;
    
    for (size_t i = 0; i < ft->count; i++) {
        ft->flux_times[i] = uft_read_le16(&raw16[i]) * resolution_ns;
    }
    
    // Calculate timing
    uint64_t total = 0;
    for (size_t i = 0; i < ft->count; i++) {
        total += ft->flux_times[i];
    }
    ft->index_time = (uint32_t)total;
    ft->rpm = 60000000000.0 / total;  // ns to RPM
    
    free(raw);
    *out = ft;
    return UFT_ERR_OK;
}
```

### Multi-Revolution Support

```c
static uft_error_t read_multirev_track(
    ctx_t* ctx,
    int track,
    int head,
    uft_flux_track_t** revolutions,
    int* rev_count)
{
    *rev_count = ctx->revolutions_per_track;
    
    for (int rev = 0; rev < *rev_count; rev++) {
        uft_error_t err = read_single_revolution(
            ctx, track, head, rev, &revolutions[rev]);
        if (err != UFT_ERR_OK) {
            // Clean up already read revolutions
            for (int j = 0; j < rev; j++) {
                free_flux_track(revolutions[j]);
            }
            return err;
        }
    }
    
    return UFT_ERR_OK;
}
```

## Testing Requirements

### Unit Tests

```c
void test_my_format_probe(void)
{
    uint8_t valid[] = "MYFORMAT\x01\x00...";
    int confidence = 0;
    
    assert(my_probe(valid, sizeof(valid), &confidence) == UFT_ERR_OK);
    assert(confidence == 100);
    
    uint8_t invalid[] = "NOTMYFORMAT";
    assert(my_probe(invalid, sizeof(invalid), &confidence) == UFT_ERR_OK);
    assert(confidence == 0);
}

void test_my_format_read(void)
{
    uft_disk_t* disk = NULL;
    assert(my_open("test_data/sample.myf", &disk) == UFT_ERR_OK);
    assert(disk != NULL);
    
    uint8_t buffer[512];
    assert(my_read_sector(disk, 0, 0, 0, buffer, 512) == UFT_ERR_OK);
    
    // Verify known content
    assert(memcmp(buffer, expected_sector_0, 512) == 0);
    
    my_close(&disk);
    assert(disk == NULL);
}
```

### Fuzz Testing

```c
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    int confidence;
    my_probe(data, size, &confidence);
    
    // Try to open if probe succeeds
    if (confidence > 50) {
        // Write to temp file and try to open
        // (fuzz harness handles this)
    }
    
    return 0;
}
```

## CMake Integration

```cmake
# Add your parser to the format library
add_library(uft_format_my STATIC
    my_format.c
    my_format.h
)

target_include_directories(uft_format_my
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_format_my
    PUBLIC uft_core
)

# Add tests
if(UFT_BUILD_TESTS)
    add_executable(test_my_format
        tests/test_my_format.c
    )
    target_link_libraries(test_my_format
        uft_format_my
        uft_test_utils
    )
    add_test(NAME test_my_format COMMAND test_my_format)
endif()
```

## See Also

- [ARCHITECTURE.md](ARCHITECTURE.md) - Overall system architecture
- [API_REFERENCE.md](API_REFERENCE.md) - Full API documentation
- [FORMATS.md](FORMATS.md) - Supported format details
