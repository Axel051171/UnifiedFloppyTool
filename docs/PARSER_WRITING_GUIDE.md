# UFT Parser Writing Guide

**Version:** 4.1.0  
**Date:** 2026-01-03  
**Audience:** Developers adding new format support to UFT

---

## Table of Contents

1. [Overview](#overview)
2. [Parser Architecture](#parser-architecture)
3. [Required Functions](#required-functions)
4. [Step-by-Step Guide](#step-by-step-guide)
5. [Error Handling](#error-handling)
6. [Memory Management](#memory-management)
7. [Testing](#testing)
8. [Examples](#examples)
9. [Checklist](#checklist)

---

## Overview

A UFT parser is a module that reads and writes a specific disk image format. Each parser
must follow UFT conventions for:

- **Naming**: `uft_<format>.h` and `uft_<format>.c`
- **API**: Standard init/free/read/write functions
- **Error Handling**: Return UFT_ERR_* codes
- **Memory**: Caller-owns-buffer pattern

### Supported Parser Types

| Type | Description | Examples |
|------|-------------|----------|
| **Sector Image** | Raw sector data | ADF, D64, IMG |
| **Track Image** | Full track with gaps | DMK, MFM |
| **Flux Image** | Raw flux transitions | SCP, A2R, KryoFlux |
| **Filesystem** | Mounted filesystem | FAT12, DFS, OFS |

---

## Parser Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        UFT Core                                  │
├─────────────────────────────────────────────────────────────────┤
│                    Format Registry                               │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐    │
│  │  ADF      │  │  D64      │  │  SCP      │  │  YOUR     │    │
│  │  Parser   │  │  Parser   │  │  Parser   │  │  FORMAT   │    │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘    │
├─────────────────────────────────────────────────────────────────┤
│                    Common Utilities                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │  CRC        │  │  Encoding   │  │  Geometry   │             │
│  │  Functions  │  │  MFM/GCR    │  │  Database   │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

### File Organization

```
include/uft/
  uft_myformat.h        # Public API header

src/formats/
  myformat/
    uft_myformat.c      # Implementation
    myformat_internal.h # Private header (optional)

tests/formats/
  test_myformat.c       # Unit tests
```

---

## Required Functions

Every parser MUST implement these core functions:

### 1. Detection

```c
/**
 * @brief Detect if data is in this format
 * @param data Raw file data
 * @param size Data size
 * @return Confidence score (0-100), 0 = not this format
 */
int uft_myformat_detect(const uint8_t *data, size_t size);
```

### 2. Initialization

```c
/**
 * @brief Initialize image structure
 * @param img Image structure (caller-allocated)
 * @return UFT_ERR_OK on success
 */
int uft_myformat_init(uft_myformat_image_t *img);
```

### 3. Cleanup

```c
/**
 * @brief Free image resources
 * @param img Image to free (does not free img itself)
 */
void uft_myformat_free(uft_myformat_image_t *img);
```

### 4. Read from File

```c
/**
 * @brief Read image from file
 * @param filename Path to image file
 * @param img Output image structure
 * @return UFT_ERR_OK on success
 */
int uft_myformat_read(const char *filename, uft_myformat_image_t *img);
```

### 5. Read from Memory

```c
/**
 * @brief Read image from memory buffer
 * @param data Buffer containing image data
 * @param size Buffer size
 * @param img Output image structure
 * @return UFT_ERR_OK on success
 */
int uft_myformat_read_mem(const uint8_t *data, size_t size, 
                          uft_myformat_image_t *img);
```

### 6. Write to File

```c
/**
 * @brief Write image to file
 * @param filename Output path
 * @param img Image to write
 * @return UFT_ERR_OK on success
 */
int uft_myformat_write(const char *filename, 
                       const uft_myformat_image_t *img);
```

### Optional Functions

```c
/* Sector access */
int uft_myformat_read_sector(const uft_myformat_image_t *img,
                              int cyl, int head, int sector,
                              uint8_t *buffer, size_t size);

int uft_myformat_write_sector(uft_myformat_image_t *img,
                               int cyl, int head, int sector,
                               const uint8_t *data, size_t size);

/* Track access */
int uft_myformat_read_track(const uft_myformat_image_t *img,
                             int cyl, int head,
                             uft_track_t *track);

/* Conversion */
int uft_myformat_to_raw(const uft_myformat_image_t *img,
                         uint8_t **data, size_t *size);

int uft_myformat_from_raw(const uint8_t *data, size_t size,
                          uft_myformat_image_t *img);

/* Info */
void uft_myformat_print_info(const uft_myformat_image_t *img);
```

---

## Step-by-Step Guide

### Step 1: Create the Header

```c
/* uft_myformat.h */
#ifndef UFT_MYFORMAT_H
#define UFT_MYFORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Format Constants
 *============================================================================*/

#define UFT_MYFORMAT_MAGIC          0x1234  /* File signature */
#define UFT_MYFORMAT_VERSION        1
#define UFT_MYFORMAT_MAX_TRACKS     164     /* 82 cyl * 2 heads */
#define UFT_MYFORMAT_SECTOR_SIZE    512

/*============================================================================
 * File Header (packed binary structure)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint16_t magic;         /* File signature */
    uint8_t  version;       /* Format version */
    uint8_t  flags;         /* Option flags */
    uint16_t cylinders;     /* Number of cylinders */
    uint8_t  heads;         /* Number of heads */
    uint8_t  sectors;       /* Sectors per track */
    uint32_t data_offset;   /* Offset to track data */
    uint32_t data_size;     /* Size of track data */
} uft_myformat_header_t;

/*============================================================================
 * Track Structure
 *============================================================================*/

typedef struct {
    uint8_t  cylinder;      /* Cylinder number */
    uint8_t  head;          /* Head number */
    uint8_t  sectors;       /* Sectors in this track */
    uint8_t  *data;         /* Sector data */
    size_t   data_size;     /* Data size */
    uint16_t *crcs;         /* CRC for each sector */
} uft_myformat_track_t;

/*============================================================================
 * Image Structure
 *============================================================================*/

typedef struct {
    uft_myformat_header_t header;
    
    uint16_t num_tracks;
    uft_myformat_track_t *tracks;
    
    /* Computed values */
    uint32_t total_sectors;
    uint32_t formatted_size;
    
    /* Metadata */
    char *comment;
    bool modified;
} uft_myformat_image_t;

/*============================================================================
 * API Functions
 *============================================================================*/

int  uft_myformat_detect(const uint8_t *data, size_t size);
int  uft_myformat_init(uft_myformat_image_t *img);
void uft_myformat_free(uft_myformat_image_t *img);

int  uft_myformat_read(const char *filename, uft_myformat_image_t *img);
int  uft_myformat_read_mem(const uint8_t *data, size_t size,
                           uft_myformat_image_t *img);

int  uft_myformat_write(const char *filename, 
                        const uft_myformat_image_t *img);

void uft_myformat_print_info(const uft_myformat_image_t *img);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MYFORMAT_H */
```

### Step 2: Implement Detection

```c
int uft_myformat_detect(const uint8_t *data, size_t size)
{
    /* Must have at least header size */
    if (!data || size < sizeof(uft_myformat_header_t)) {
        return 0;
    }
    
    const uft_myformat_header_t *hdr = (const uft_myformat_header_t *)data;
    
    /* Check magic number */
    if (hdr->magic != UFT_MYFORMAT_MAGIC) {
        return 0;  /* Not this format */
    }
    
    /* Check version */
    if (hdr->version > UFT_MYFORMAT_VERSION) {
        return 50;  /* Maybe this format, unknown version */
    }
    
    /* Validate geometry */
    if (hdr->cylinders == 0 || hdr->cylinders > 100) {
        return 25;  /* Suspicious */
    }
    if (hdr->heads == 0 || hdr->heads > 2) {
        return 25;
    }
    
    /* High confidence */
    return 100;
}
```

### Step 3: Implement Init/Free

```c
int uft_myformat_init(uft_myformat_image_t *img)
{
    if (!img) {
        return UFT_ERR_INVALID;
    }
    
    memset(img, 0, sizeof(*img));
    img->header.magic = UFT_MYFORMAT_MAGIC;
    img->header.version = UFT_MYFORMAT_VERSION;
    
    return UFT_ERR_OK;
}

void uft_myformat_free(uft_myformat_image_t *img)
{
    if (!img) return;
    
    /* Free track data */
    if (img->tracks) {
        for (uint16_t i = 0; i < img->num_tracks; i++) {
            free(img->tracks[i].data);
            free(img->tracks[i].crcs);
        }
        free(img->tracks);
    }
    
    free(img->comment);
    
    memset(img, 0, sizeof(*img));
}
```

### Step 4: Implement Read

```c
int uft_myformat_read(const char *filename, uft_myformat_image_t *img)
{
    if (!filename || !img) {
        return UFT_ERR_INVALID;
    }
    
    /* Open file */
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return UFT_ERR_IO;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Read into buffer */
    uint8_t *data = (uint8_t*)malloc(size);
    if (!data) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return UFT_ERR_IO;
    }
    fclose(f);
    
    /* Parse */
    int result = uft_myformat_read_mem(data, size, img);
    free(data);
    
    return result;
}

int uft_myformat_read_mem(const uint8_t *data, size_t size,
                          uft_myformat_image_t *img)
{
    if (!data || !img) {
        return UFT_ERR_INVALID;
    }
    
    /* Initialize */
    uft_myformat_init(img);
    
    /* Check detection */
    if (uft_myformat_detect(data, size) < 50) {
        return UFT_ERR_FORMAT;
    }
    
    /* Parse header */
    memcpy(&img->header, data, sizeof(img->header));
    
    /* Validate */
    if (img->header.data_offset + img->header.data_size > size) {
        return UFT_ERR_CORRUPT;
    }
    
    /* Allocate tracks */
    img->num_tracks = img->header.cylinders * img->header.heads;
    img->tracks = (uft_myformat_track_t*)calloc(
        img->num_tracks, sizeof(uft_myformat_track_t));
    
    if (!img->tracks) {
        return UFT_ERR_MEMORY;
    }
    
    /* Parse tracks */
    const uint8_t *track_data = data + img->header.data_offset;
    size_t track_size = img->header.sectors * UFT_MYFORMAT_SECTOR_SIZE;
    
    for (uint16_t t = 0; t < img->num_tracks; t++) {
        img->tracks[t].cylinder = t / img->header.heads;
        img->tracks[t].head = t % img->header.heads;
        img->tracks[t].sectors = img->header.sectors;
        img->tracks[t].data_size = track_size;
        
        img->tracks[t].data = (uint8_t*)malloc(track_size);
        if (!img->tracks[t].data) {
            uft_myformat_free(img);
            return UFT_ERR_MEMORY;
        }
        
        memcpy(img->tracks[t].data, track_data, track_size);
        track_data += track_size;
    }
    
    return UFT_ERR_OK;
}
```

---

## Error Handling

### Standard Error Codes

```c
typedef enum {
    UFT_ERR_OK         = 0,    /* Success */
    UFT_ERR_INVALID    = -1,   /* Invalid parameter */
    UFT_ERR_MEMORY     = -2,   /* Memory allocation failed */
    UFT_ERR_IO         = -3,   /* I/O error */
    UFT_ERR_FORMAT     = -4,   /* Invalid format */
    UFT_ERR_CORRUPT    = -5,   /* Corrupted data */
    UFT_ERR_CRC        = -6,   /* CRC error */
    UFT_ERR_NOTFOUND   = -7,   /* Item not found */
    UFT_ERR_OVERFLOW   = -8,   /* Buffer overflow */
    UFT_ERR_UNSUPPORTED = -9   /* Unsupported feature */
} uft_error_t;
```

### Best Practices

```c
/* Always validate inputs first */
if (!img || !data || size == 0) {
    return UFT_ERR_INVALID;
}

/* Check bounds before access */
if (offset + length > buffer_size) {
    return UFT_ERR_OVERFLOW;
}

/* Clean up on error */
if (error) {
    uft_myformat_free(img);
    return error;
}
```

---

## Memory Management

### Rules

1. **Caller owns input buffers** - Don't free input parameters
2. **Parser owns internal buffers** - Free in `_free()` function
3. **Always check allocations** - Return `UFT_ERR_MEMORY` on failure
4. **Zero-initialize** - Use `calloc` or `memset`
5. **Document ownership** - Comment who frees what

### Pattern

```c
/* Good: Caller-owns-buffer pattern */
int uft_myformat_to_raw(const uft_myformat_image_t *img,
                         uint8_t **data, size_t *size)
{
    /* Allocate output - caller must free */
    *data = (uint8_t*)malloc(calculated_size);
    if (!*data) {
        return UFT_ERR_MEMORY;
    }
    *size = calculated_size;
    
    /* Fill buffer... */
    
    return UFT_ERR_OK;
}

/* Caller usage */
uint8_t *raw_data;
size_t raw_size;
if (uft_myformat_to_raw(img, &raw_data, &raw_size) == UFT_ERR_OK) {
    /* Use raw_data... */
    free(raw_data);  /* Caller frees */
}
```

---

## Testing

### Unit Test Template

```c
/* test_myformat.c */
#include "uft/uft_myformat.h"
#include <assert.h>
#include <stdio.h>

/* Test data */
static const uint8_t test_image[] = {
    0x34, 0x12,  /* Magic */
    0x01,        /* Version */
    0x00,        /* Flags */
    0x50, 0x00,  /* 80 cylinders */
    0x02,        /* 2 heads */
    0x09,        /* 9 sectors */
    /* ... */
};

static void test_detect(void)
{
    printf("Testing detection...\n");
    
    /* Valid image */
    assert(uft_myformat_detect(test_image, sizeof(test_image)) >= 50);
    
    /* Invalid magic */
    uint8_t bad_magic[] = {0x00, 0x00};
    assert(uft_myformat_detect(bad_magic, sizeof(bad_magic)) == 0);
    
    /* Too small */
    assert(uft_myformat_detect(test_image, 2) == 0);
    
    printf("  PASSED\n");
}

static void test_read_write(void)
{
    printf("Testing read/write...\n");
    
    uft_myformat_image_t img;
    
    /* Read from memory */
    int err = uft_myformat_read_mem(test_image, sizeof(test_image), &img);
    assert(err == UFT_ERR_OK);
    assert(img.header.cylinders == 80);
    assert(img.header.heads == 2);
    
    /* Write to file */
    err = uft_myformat_write("/tmp/test.myfmt", &img);
    assert(err == UFT_ERR_OK);
    
    /* Read back and compare */
    uft_myformat_image_t img2;
    err = uft_myformat_read("/tmp/test.myfmt", &img2);
    assert(err == UFT_ERR_OK);
    assert(img2.header.cylinders == img.header.cylinders);
    
    uft_myformat_free(&img);
    uft_myformat_free(&img2);
    
    printf("  PASSED\n");
}

int main(void)
{
    printf("=== MyFormat Parser Tests ===\n\n");
    
    test_detect();
    test_read_write();
    
    printf("\nAll tests passed!\n");
    return 0;
}
```

---

## Examples

### Example: Simple Sector Image Parser

See `src/formats/img/uft_img.c` - Raw sector dump format

### Example: Track Image Parser  

See `src/formats/dmk/uft_dmk.c` - TRS-80 DMK with IDAM tables

### Example: Flux Image Parser

See `src/loaders/scp/uft_scp.c` - SuperCard Pro multi-revolution flux

### Example: Filesystem Parser

See `src/filesystems/fat12/uft_fat12.c` - FAT12 with directory parsing

---

## Checklist

Before submitting a new parser, verify:

### Code Quality
- [ ] Follows UFT naming conventions (`uft_<format>_*`)
- [ ] Uses standard error codes (`UFT_ERR_*`)
- [ ] Includes comprehensive documentation
- [ ] No compiler warnings with `-Wall -Wextra`
- [ ] No memory leaks (tested with Valgrind/ASan)
- [ ] Handles NULL inputs gracefully
- [ ] Checks all buffer bounds

### Functionality
- [ ] `_detect()` returns 0-100 confidence
- [ ] `_init()` zeros all fields
- [ ] `_free()` releases all memory
- [ ] `_read()` and `_read_mem()` work correctly
- [ ] `_write()` produces valid output
- [ ] Round-trip (read→write→read) preserves data

### Testing
- [ ] Unit tests for all public functions
- [ ] Test with real-world sample files
- [ ] Test with corrupted/truncated files
- [ ] Test with edge cases (0 tracks, max tracks)
- [ ] Added to CTest configuration

### Integration
- [ ] Header added to `include/uft/`
- [ ] Source added to `src/formats/`
- [ ] Added to CMakeLists.txt
- [ ] Registered in format detection system
- [ ] Added to CHANGELOG.md
- [ ] Added to documentation

---

## Support

For questions or issues with parser development:

- Review existing parsers in `src/formats/`
- Check the Doxygen API documentation
- Open an issue on GitHub

---

*Document generated: 2026-01-03 | UFT v4.1.0*
