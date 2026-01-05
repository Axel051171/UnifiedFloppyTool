# UFT API Reference v3.3.2

## Overview

The UnifiedFloppyTool (UFT) API provides a comprehensive interface for floppy disk preservation, forensic analysis, and format conversion. This document covers all public APIs, organized by module.

---

## Table of Contents

1. [Core Types](#core-types)
2. [Error Handling](#error-handling)
3. [Memory Management](#memory-management)
4. [Format Detection](#format-detection)
5. [Image Operations](#image-operations)
6. [Flux Processing](#flux-processing)
7. [PLL System](#pll-system)
8. [Recovery System](#recovery-system)
9. [Protection Detection](#protection-detection)
10. [Encoding/Decoding](#encodingdecoding)
11. [Hardware Abstraction](#hardware-abstraction)
12. [Parameter System](#parameter-system)
13. [GUI Widgets (Qt6)](#gui-widgets-qt6)

---

## Core Types

### Basic Types

```c
#include <uft/uft_types.h>

/* Standard integer types (from stdint.h) */
typedef uint8_t   uft_byte_t;
typedef uint16_t  uft_word_t;
typedef uint32_t  uft_dword_t;
typedef uint64_t  uft_qword_t;

/* Size type for buffer lengths */
typedef size_t    uft_size_t;

/* Boolean type */
typedef int       uft_bool_t;
#define UFT_TRUE  1
#define UFT_FALSE 0
```

### Geometry Types

```c
typedef struct uft_geometry {
    uint16_t tracks;          /* Number of tracks (typically 35-84) */
    uint8_t  sides;           /* Number of sides (1 or 2) */
    uint8_t  sectors;         /* Sectors per track (variable) */
    uint16_t sector_size;     /* Bytes per sector (128-8192) */
    uint32_t total_size;      /* Total image size in bytes */
} uft_geometry_t;
```

### Track Types

```c
typedef struct uft_raw_track {
    uint8_t* data;            /* Raw track data */
    size_t   length;          /* Data length in bytes */
    uint32_t bitcell_ns;      /* Nominal bit cell time (ns) */
    uint8_t  track_num;       /* Physical track number */
    uint8_t  side;            /* Head/side (0 or 1) */
    uint16_t flags;           /* Track flags */
} uft_raw_track_t;

/* Track flags */
#define UFT_TRACK_FLAG_INDEX_HOLE   0x0001
#define UFT_TRACK_FLAG_WEAK_BITS    0x0002
#define UFT_TRACK_FLAG_PROTECTED    0x0004
#define UFT_TRACK_FLAG_DELETED      0x0008
```

### Sector Types

```c
typedef struct uft_sector {
    uint8_t  track;           /* Logical track */
    uint8_t  side;            /* Side (0 or 1) */
    uint8_t  sector;          /* Sector number */
    uint8_t  size_code;       /* Size code (0=128, 1=256, 2=512, 3=1024) */
    uint8_t* data;            /* Sector data (caller-owned) */
    size_t   data_length;     /* Actual data length */
    uint16_t crc_stored;      /* CRC from disk */
    uint16_t crc_computed;    /* Computed CRC */
    uint8_t  status;          /* Sector status flags */
} uft_sector_t;

/* Sector status */
typedef enum uft_sector_status {
    UFT_SECTOR_OK        = 0x00,
    UFT_SECTOR_CRC_ERROR = 0x01,
    UFT_SECTOR_DELETED   = 0x02,
    UFT_SECTOR_WEAK      = 0x04,
    UFT_SECTOR_MISSING   = 0x08,
    UFT_SECTOR_ID_ERROR  = 0x10
} uft_sector_status_t;
```

---

## Error Handling

### Error Codes

```c
#include <uft/uft_error.h>

typedef enum uft_error {
    UFT_ERR_OK              = 0,    /* Success */
    UFT_ERR_INVALID_PARAM   = -1,   /* Invalid parameter */
    UFT_ERR_INVALID_FORMAT  = -2,   /* Unknown/invalid format */
    UFT_ERR_IO              = -3,   /* I/O error */
    UFT_ERR_MEMORY          = -4,   /* Memory allocation failed */
    UFT_ERR_NOT_FOUND       = -5,   /* Resource not found */
    UFT_ERR_UNSUPPORTED     = -6,   /* Operation not supported */
    UFT_ERR_CRC             = -7,   /* CRC check failed */
    UFT_ERR_BOUNDS          = -8,   /* Buffer bounds exceeded */
    UFT_ERR_TIMEOUT         = -9,   /* Operation timed out */
    UFT_ERR_HARDWARE        = -10,  /* Hardware error */
    UFT_ERR_BUSY            = -11,  /* Resource busy */
    UFT_ERR_CANCELLED       = -12,  /* Operation cancelled */
    UFT_ERR_EOF             = -13,  /* End of file/data */
    UFT_ERR_PROTECTED       = -14,  /* Write protected */
    UFT_ERR_OVERFLOW        = -15   /* Integer overflow */
} uft_error_t;
```

### Error Functions

```c
/* Get human-readable error message */
const char* uft_error_string(uft_error_t err);

/* Get last error for current thread */
uft_error_t uft_get_last_error(void);

/* Set last error */
void uft_set_last_error(uft_error_t err);

/* Extended error info */
typedef struct uft_error_info {
    uft_error_t code;
    const char* message;
    const char* file;
    int         line;
    uint32_t    context;      /* Format-specific error context */
} uft_error_info_t;

uft_error_info_t uft_get_error_info(void);
```

---

## Memory Management

### Allocation Functions

```c
#include <uft/uft_memory.h>

/* Allocate zeroed memory */
void* uft_calloc(size_t count, size_t size);

/* Allocate uninitialized memory */
void* uft_malloc(size_t size);

/* Reallocate memory */
void* uft_realloc(void* ptr, size_t new_size);

/* Free memory */
void uft_free(void* ptr);

/* Aligned allocation (for SIMD) */
void* uft_aligned_alloc(size_t alignment, size_t size);
void  uft_aligned_free(void* ptr);
```

### Safe Arithmetic

```c
/* Safe multiplication (returns false on overflow) */
bool uft_safe_mul_size(size_t a, size_t b, size_t* result);

/* Safe addition */
bool uft_safe_add_size(size_t a, size_t b, size_t* result);

/* Checked allocation (returns NULL on overflow) */
void* uft_safe_calloc(size_t count, size_t size);
```

### Buffer Management

```c
/* Ownership: caller-owns-buffer pattern */
typedef struct uft_buffer {
    uint8_t* data;
    size_t   size;
    size_t   capacity;
} uft_buffer_t;

/* Initialize buffer (caller provides storage) */
void uft_buffer_init(uft_buffer_t* buf);

/* Ensure capacity (may reallocate) */
uft_error_t uft_buffer_reserve(uft_buffer_t* buf, size_t capacity);

/* Append data */
uft_error_t uft_buffer_append(uft_buffer_t* buf, const void* data, size_t len);

/* Release buffer resources */
void uft_buffer_free(uft_buffer_t* buf);
```

---

## Format Detection

### Probe Functions

```c
#include <uft/uft_format_detect.h>

/* Probe result structure */
typedef struct uft_probe_result {
    const char* format_id;    /* Format identifier (e.g., "adf", "scp") */
    const char* description;  /* Human-readable description */
    int         confidence;   /* 0-100 confidence score */
    uint32_t    capabilities; /* Format capabilities flags */
} uft_probe_result_t;

/* Probe a file buffer */
int uft_probe(const uint8_t* data, size_t size, 
              uft_probe_result_t* results, int max_results);

/* Probe by file extension */
int uft_probe_by_extension(const char* extension,
                           uft_probe_result_t* results, int max_results);

/* Probe with hints */
int uft_probe_with_hints(const uint8_t* data, size_t size,
                         const char* extension,
                         const uft_probe_hints_t* hints,
                         uft_probe_result_t* results, int max_results);
```

### Format Registry

```c
/* Format driver interface */
typedef struct uft_format_driver {
    const char* id;
    const char* name;
    const char* extensions;   /* Comma-separated: "adf,adz" */
    uint32_t    capabilities;
    
    /* Required functions */
    int         (*probe)(const uint8_t* data, size_t size);
    uft_error_t (*open)(const uint8_t* data, size_t size, void** ctx);
    void        (*close)(void* ctx);
    
    /* Optional functions */
    uft_error_t (*read_sector)(void* ctx, const uft_sector_id_t* id, 
                               uint8_t* buf, size_t* len);
    uft_error_t (*write_sector)(void* ctx, const uft_sector_id_t* id,
                                const uint8_t* buf, size_t len);
    uft_error_t (*read_track)(void* ctx, int track, int side,
                              uft_raw_track_t* raw);
    uft_error_t (*get_geometry)(void* ctx, uft_geometry_t* geom);
} uft_format_driver_t;

/* Register a format driver */
uft_error_t uft_register_format(const uft_format_driver_t* driver);

/* Get format by ID */
const uft_format_driver_t* uft_get_format(const char* id);

/* Iterate all formats */
typedef void (*uft_format_callback_t)(const uft_format_driver_t* driver, 
                                      void* user_data);
void uft_foreach_format(uft_format_callback_t callback, void* user_data);
```

### Capability Flags

```c
#define UFT_CAP_READ          0x00000001  /* Can read */
#define UFT_CAP_WRITE         0x00000002  /* Can write */
#define UFT_CAP_FLUX          0x00000004  /* Flux-level data */
#define UFT_CAP_SECTOR        0x00000008  /* Sector-level data */
#define UFT_CAP_TRACK_IMG     0x00000010  /* Raw track image */
#define UFT_CAP_PROTECTED     0x00000020  /* Copy protection support */
#define UFT_CAP_MULTI_REV     0x00000040  /* Multi-revolution */
#define UFT_CAP_TIMING        0x00000080  /* Timing data */
#define UFT_CAP_WEAK_BITS     0x00000100  /* Weak bit support */
#define UFT_CAP_VARIABLE      0x00000200  /* Variable sector size */
#define UFT_CAP_COMPRESSION   0x00000400  /* Compressed data */
#define UFT_CAP_METADATA      0x00000800  /* Metadata storage */
```

---

## Image Operations

### Opening Images

```c
#include <uft/uft_image.h>

/* Image handle (opaque) */
typedef struct uft_image uft_image_t;

/* Open image from file */
uft_error_t uft_image_open_file(const char* path, uft_image_t** image);

/* Open image from memory */
uft_error_t uft_image_open_memory(const uint8_t* data, size_t size,
                                  uft_image_t** image);

/* Open with specific format */
uft_error_t uft_image_open_as(const uint8_t* data, size_t size,
                              const char* format_id, uft_image_t** image);

/* Close image */
void uft_image_close(uft_image_t* image);
```

### Reading Data

```c
/* Get geometry */
uft_error_t uft_image_get_geometry(uft_image_t* image, uft_geometry_t* geom);

/* Read sector */
uft_error_t uft_image_read_sector(uft_image_t* image,
                                  uint8_t track, uint8_t side, uint8_t sector,
                                  uint8_t* buffer, size_t buffer_size,
                                  size_t* bytes_read);

/* Read raw track */
uft_error_t uft_image_read_track(uft_image_t* image,
                                 uint8_t track, uint8_t side,
                                 uft_raw_track_t* raw);

/* Read all sectors on track */
uft_error_t uft_image_read_track_sectors(uft_image_t* image,
                                         uint8_t track, uint8_t side,
                                         uft_sector_t* sectors, 
                                         int* sector_count);
```

### Writing Data

```c
/* Write sector */
uft_error_t uft_image_write_sector(uft_image_t* image,
                                   uint8_t track, uint8_t side, uint8_t sector,
                                   const uint8_t* data, size_t length);

/* Write raw track */
uft_error_t uft_image_write_track(uft_image_t* image,
                                  uint8_t track, uint8_t side,
                                  const uft_raw_track_t* raw);
```

### Conversion

```c
/* Convert image to different format */
uft_error_t uft_image_convert(uft_image_t* source, 
                              const char* dest_format,
                              uft_buffer_t* output);

/* Convert with options */
uft_error_t uft_image_convert_ex(uft_image_t* source,
                                 const char* dest_format,
                                 const uft_convert_options_t* options,
                                 uft_buffer_t* output);

typedef struct uft_convert_options {
    bool preserve_errors;     /* Keep CRC errors */
    bool preserve_timing;     /* Keep timing data */
    bool preserve_weak;       /* Keep weak bits */
    bool interleave;          /* Apply interleave */
    int  interleave_value;    /* Interleave factor */
} uft_convert_options_t;
```

---

## Flux Processing

### Flux Data Types

```c
#include <uft/uft_flux.h>

/* Single flux transition (nanoseconds) */
typedef uint32_t uft_flux_time_t;

/* Flux stream */
typedef struct uft_flux_stream {
    uft_flux_time_t* transitions;   /* Array of transition times */
    size_t           count;         /* Number of transitions */
    size_t           capacity;      /* Allocated capacity */
    uint32_t         index_offset;  /* Offset to index hole */
    uint8_t          track;         /* Track number */
    uint8_t          side;          /* Side */
    uint8_t          revolution;    /* Revolution number */
} uft_flux_stream_t;

/* Multi-revolution flux data */
typedef struct uft_flux_track {
    uft_flux_stream_t* revolutions;
    int                revolution_count;
    uint32_t           bitcell_ns;    /* Nominal bit cell */
    uint32_t           rpm;           /* Rotational speed */
} uft_flux_track_t;
```

### Flux Operations

```c
/* Allocate flux stream */
uft_error_t uft_flux_stream_alloc(uft_flux_stream_t* stream, size_t capacity);

/* Free flux stream */
void uft_flux_stream_free(uft_flux_stream_t* stream);

/* Read flux from image */
uft_error_t uft_flux_read_track(uft_image_t* image,
                                uint8_t track, uint8_t side,
                                uft_flux_track_t* flux);

/* Convert flux to raw track */
uft_error_t uft_flux_decode(const uft_flux_track_t* flux,
                            const uft_pll_params_t* pll,
                            uft_raw_track_t* raw);

/* Convert raw track to flux */
uft_error_t uft_flux_encode(const uft_raw_track_t* raw,
                            uint32_t bitcell_ns,
                            uft_flux_stream_t* flux);
```

### Flux Analysis

```c
/* Histogram bin */
typedef struct uft_flux_bin {
    uint32_t min_ns;
    uint32_t max_ns;
    uint32_t count;
} uft_flux_bin_t;

/* Create histogram */
uft_error_t uft_flux_histogram(const uft_flux_stream_t* flux,
                               uft_flux_bin_t* bins, int bin_count);

/* Detect encoding */
typedef enum uft_encoding_type {
    UFT_ENC_UNKNOWN = 0,
    UFT_ENC_FM,
    UFT_ENC_MFM,
    UFT_ENC_GCR_APPLE,
    UFT_ENC_GCR_C64,
    UFT_ENC_GCR_AMIGA
} uft_encoding_type_t;

uft_encoding_type_t uft_flux_detect_encoding(const uft_flux_stream_t* flux);

/* Detect RPM */
uint32_t uft_flux_detect_rpm(const uft_flux_stream_t* flux);
```

---

## PLL System

### PLL Parameters

```c
#include <uft/uft_pll.h>

typedef struct uft_pll_params {
    float    gain;              /* Proportional gain (Kp), 0.01-0.20 */
    float    integral_gain;     /* Integral gain (Ki), 0.001-0.05 */
    float    lock_threshold;    /* Lock detection threshold (cycles) */
    float    bit_cell_tolerance;/* Timing tolerance (±%) */
    float    max_freq_deviation;/* Max frequency deviation (±%) */
    uint32_t window_size;       /* Averaging window size */
    uint32_t sync_pattern;      /* Sync byte pattern */
    uint32_t min_sync_bits;     /* Minimum sync bits required */
    bool     adaptive;          /* Enable adaptive gain */
} uft_pll_params_t;

/* Initialize with defaults */
void uft_pll_params_init(uft_pll_params_t* params);
```

### PLL Context

```c
/* PLL state (opaque) */
typedef struct uft_pll_ctx uft_pll_ctx_t;

/* Create PLL context */
uft_error_t uft_pll_create(const uft_pll_params_t* params, 
                           uft_pll_ctx_t** ctx);

/* Destroy PLL context */
void uft_pll_destroy(uft_pll_ctx_t* ctx);

/* Reset PLL state */
void uft_pll_reset(uft_pll_ctx_t* ctx);
```

### PLL Processing

```c
/* Process flux stream */
uft_error_t uft_pll_process(uft_pll_ctx_t* ctx,
                            const uft_flux_stream_t* flux,
                            uft_raw_track_t* output);

/* Get PLL status */
typedef struct uft_pll_status {
    bool     locked;            /* PLL is locked */
    float    frequency;         /* Current frequency (Hz) */
    float    phase_error;       /* Current phase error */
    float    jitter;            /* RMS jitter */
    uint32_t lock_count;        /* Transitions since lock */
    uint32_t unlock_count;      /* Unlock events */
} uft_pll_status_t;

void uft_pll_get_status(const uft_pll_ctx_t* ctx, uft_pll_status_t* status);
```

### PLL Presets

```c
/* Built-in presets */
typedef enum uft_pll_preset {
    UFT_PLL_PRESET_DEFAULT,
    UFT_PLL_PRESET_AGGRESSIVE,
    UFT_PLL_PRESET_CONSERVATIVE,
    UFT_PLL_PRESET_FORENSIC,
    UFT_PLL_PRESET_IBM_DD,
    UFT_PLL_PRESET_IBM_HD,
    UFT_PLL_PRESET_AMIGA_DD,
    UFT_PLL_PRESET_AMIGA_HD,
    UFT_PLL_PRESET_ATARI_ST,
    UFT_PLL_PRESET_C64,
    UFT_PLL_PRESET_APPLE_II,
    UFT_PLL_PRESET_MAC_GCR,
    UFT_PLL_PRESET_GREASEWEAZLE,
    UFT_PLL_PRESET_KRYOFLUX,
    UFT_PLL_PRESET_FLUXENGINE,
    UFT_PLL_PRESET_SCP
} uft_pll_preset_t;

/* Load preset */
uft_error_t uft_pll_load_preset(uft_pll_preset_t preset, 
                                uft_pll_params_t* params);
```

---

## Recovery System

### Recovery Parameters

```c
#include <uft/uft_recovery.h>

typedef struct uft_recovery_params {
    int   max_retries;          /* Maximum retry count */
    int   max_crc_bits;         /* Max CRC correction bits (0-4) */
    float weak_bit_threshold;   /* Variance threshold for weak bits */
    float min_confidence;       /* Minimum confidence (0.0-1.0) */
    bool  multi_rev_enabled;    /* Enable multi-revolution fusion */
    int   revolution_count;     /* Revolutions to fuse (1-5) */
    bool  weak_interpolation;   /* Interpolate weak bits */
    bool  crc_brute_force;      /* Enable brute-force CRC correction */
} uft_recovery_params_t;

void uft_recovery_params_init(uft_recovery_params_t* params);
```

### Recovery Context

```c
typedef struct uft_recovery_ctx uft_recovery_ctx_t;

uft_error_t uft_recovery_create(const uft_recovery_params_t* params,
                                uft_recovery_ctx_t** ctx);
void uft_recovery_destroy(uft_recovery_ctx_t* ctx);
```

### Recovery Operations

```c
/* Recover single sector */
uft_error_t uft_recovery_sector(uft_recovery_ctx_t* ctx,
                                uft_image_t* image,
                                uint8_t track, uint8_t side, uint8_t sector,
                                uint8_t* buffer, size_t buffer_size,
                                uft_recovery_result_t* result);

/* Recovery result */
typedef struct uft_recovery_result {
    bool    success;
    float   confidence;         /* 0.0-1.0 */
    int     attempts;
    int     crc_bits_corrected;
    bool    weak_bits_detected;
    bool    multi_rev_used;
    int     revolutions_used;
} uft_recovery_result_t;

/* Recover entire track */
uft_error_t uft_recovery_track(uft_recovery_ctx_t* ctx,
                               uft_image_t* image,
                               uint8_t track, uint8_t side,
                               uft_sector_t* sectors, int* count,
                               uft_recovery_stats_t* stats);

/* Recovery statistics */
typedef struct uft_recovery_stats {
    int   total_sectors;
    int   recovered_sectors;
    int   failed_sectors;
    float average_confidence;
    int   total_crc_corrections;
    int   weak_bit_sectors;
} uft_recovery_stats_t;
```

### Multi-Revolution Fusion

```c
/* Fuse multiple revolutions */
uft_error_t uft_recovery_fuse_revolutions(
    const uft_flux_track_t* flux,
    const uft_pll_params_t* pll_params,
    uft_raw_track_t* output,
    uft_fusion_stats_t* stats);

typedef struct uft_fusion_stats {
    int   revolutions_used;
    int   weak_regions;
    int   resolved_regions;
    float overall_confidence;
} uft_fusion_stats_t;
```

---

## Protection Detection

### Protection Types

```c
#include <uft/uft_protection.h>

typedef enum uft_protection_type {
    UFT_PROT_NONE           = 0,
    UFT_PROT_WEAK_BITS      = 0x0001,
    UFT_PROT_TIMING         = 0x0002,
    UFT_PROT_FUZZY          = 0x0004,
    UFT_PROT_LONG_TRACK     = 0x0008,
    UFT_PROT_SHORT_TRACK    = 0x0010,
    UFT_PROT_NO_FLUX        = 0x0020,
    UFT_PROT_SECTOR_INTERLEAVE = 0x0040,
    UFT_PROT_NON_STANDARD_GAP  = 0x0080,
    UFT_PROT_DUPLICATE_SECTOR  = 0x0100,
    UFT_PROT_MISSING_SECTOR    = 0x0200,
    UFT_PROT_CRC_ERROR         = 0x0400,
    UFT_PROT_DENSITY_CHANGE    = 0x0800
} uft_protection_type_t;
```

### Detection Functions

```c
/* Scan for protection */
uft_error_t uft_protection_scan(uft_image_t* image,
                                uft_protection_report_t* report);

typedef struct uft_protection_report {
    uint32_t types;             /* Bitmask of uft_protection_type_t */
    char     scheme_name[64];   /* Known scheme name (if identified) */
    int      confidence;        /* 0-100 */
    int      protected_tracks;
    int      protected_sectors;
    
    /* Detailed track info */
    struct {
        uint8_t  track;
        uint8_t  side;
        uint32_t types;
        char     details[256];
    } track_info[168];          /* Max 84 tracks * 2 sides */
    int track_info_count;
} uft_protection_report_t;

/* Known protection schemes */
typedef enum uft_protection_scheme {
    UFT_SCHEME_UNKNOWN,
    UFT_SCHEME_COPYLOCK,
    UFT_SCHEME_SPEEDLOCK,
    UFT_SCHEME_HEXAGON,
    UFT_SCHEME_ROBCOPY,
    UFT_SCHEME_V_MAX,
    UFT_SCHEME_VORPAL,
    UFT_SCHEME_PROLOK,
    UFT_SCHEME_MACROVISION
} uft_protection_scheme_t;

uft_protection_scheme_t uft_protection_identify(
    const uft_protection_report_t* report);
```

---

## Encoding/Decoding

### MFM Encoding

```c
#include <uft/uft_encoding.h>

/* Encode data to MFM */
uft_error_t uft_mfm_encode(const uint8_t* data, size_t data_len,
                           uint8_t* mfm_out, size_t* mfm_len);

/* Decode MFM to data */
uft_error_t uft_mfm_decode(const uint8_t* mfm, size_t mfm_len,
                           uint8_t* data_out, size_t* data_len);

/* MFM with clock pattern */
uft_error_t uft_mfm_encode_with_clock(const uint8_t* data, size_t data_len,
                                      uint8_t clock_pattern,
                                      uint8_t* mfm_out, size_t* mfm_len);
```

### GCR Encoding

```c
/* Apple II 6&2 GCR */
uft_error_t uft_gcr_apple_encode(const uint8_t* data, size_t data_len,
                                 uint8_t* gcr_out, size_t* gcr_len);
uft_error_t uft_gcr_apple_decode(const uint8_t* gcr, size_t gcr_len,
                                 uint8_t* data_out, size_t* data_len);

/* Commodore 64 GCR */
uft_error_t uft_gcr_c64_encode(const uint8_t* data, size_t data_len,
                               uint8_t* gcr_out, size_t* gcr_len);
uft_error_t uft_gcr_c64_decode(const uint8_t* gcr, size_t gcr_len,
                               uint8_t* data_out, size_t* data_len);

/* Amiga MFM-GCR (Amiga ODD/EVEN) */
uft_error_t uft_gcr_amiga_encode(const uint8_t* data, size_t data_len,
                                 uint8_t* out, size_t* out_len);
uft_error_t uft_gcr_amiga_decode(const uint8_t* enc, size_t enc_len,
                                 uint8_t* data_out, size_t* data_len);
```

### FM Encoding

```c
/* FM (single density) */
uft_error_t uft_fm_encode(const uint8_t* data, size_t data_len,
                          uint8_t* fm_out, size_t* fm_len);
uft_error_t uft_fm_decode(const uint8_t* fm, size_t fm_len,
                          uint8_t* data_out, size_t* data_len);
```

---

## Hardware Abstraction

### Controller Interface

```c
#include <uft/uft_hal.h>

/* Supported controllers */
typedef enum uft_controller_type {
    UFT_CTRL_GREASEWEAZLE,
    UFT_CTRL_FLUXENGINE,
    UFT_CTRL_KRYOFLUX,
    UFT_CTRL_FC5025,
    UFT_CTRL_XUM1541,
    UFT_CTRL_SCP
} uft_controller_type_t;

/* Controller handle */
typedef struct uft_controller uft_controller_t;

/* Open controller */
uft_error_t uft_controller_open(uft_controller_type_t type,
                                const char* device_path,
                                uft_controller_t** ctrl);

/* Close controller */
void uft_controller_close(uft_controller_t* ctrl);

/* Get controller info */
typedef struct uft_controller_info {
    char     name[64];
    char     version[32];
    char     serial[64];
    uint32_t capabilities;
    uint32_t max_tracks;
    uint32_t max_sides;
} uft_controller_info_t;

uft_error_t uft_controller_get_info(uft_controller_t* ctrl,
                                    uft_controller_info_t* info);
```

### Read/Write Operations

```c
/* Read flux track */
uft_error_t uft_controller_read_flux(uft_controller_t* ctrl,
                                     uint8_t track, uint8_t side,
                                     int revolutions,
                                     uft_flux_track_t* flux);

/* Write flux track */
uft_error_t uft_controller_write_flux(uft_controller_t* ctrl,
                                      uint8_t track, uint8_t side,
                                      const uft_flux_stream_t* flux);

/* Seek to track */
uft_error_t uft_controller_seek(uft_controller_t* ctrl, uint8_t track);

/* Recalibrate (seek to track 0) */
uft_error_t uft_controller_recalibrate(uft_controller_t* ctrl);
```

### Drive Profiles

```c
typedef struct uft_drive_profile {
    char     name[64];
    uint32_t rpm;               /* Nominal RPM (300 or 360) */
    uint32_t tracks;            /* Max tracks */
    uint32_t step_time_us;      /* Step time in microseconds */
    uint32_t settle_time_ms;    /* Head settle time */
    bool     double_step;       /* Use double stepping (40→80 track) */
} uft_drive_profile_t;

/* Built-in profiles */
extern const uft_drive_profile_t UFT_DRIVE_PC_35_DD;    /* PC 3.5" DD */
extern const uft_drive_profile_t UFT_DRIVE_PC_35_HD;    /* PC 3.5" HD */
extern const uft_drive_profile_t UFT_DRIVE_PC_525_DD;   /* PC 5.25" DD */
extern const uft_drive_profile_t UFT_DRIVE_PC_525_HD;   /* PC 5.25" HD */
extern const uft_drive_profile_t UFT_DRIVE_AMIGA;       /* Amiga */
extern const uft_drive_profile_t UFT_DRIVE_C64_1541;    /* C64 1541 */
extern const uft_drive_profile_t UFT_DRIVE_APPLE_II;    /* Apple II */
extern const uft_drive_profile_t UFT_DRIVE_ATARI_ST;    /* Atari ST */

/* Set drive profile */
uft_error_t uft_controller_set_profile(uft_controller_t* ctrl,
                                       const uft_drive_profile_t* profile);
```

---

## Parameter System

### Parameter Registry

```c
#include <uft/uft_params.h>

/* Get parameter value */
uft_error_t uft_param_get_int(const char* key, int* value);
uft_error_t uft_param_get_float(const char* key, float* value);
uft_error_t uft_param_get_string(const char* key, char* value, size_t size);
uft_error_t uft_param_get_bool(const char* key, bool* value);

/* Set parameter value */
uft_error_t uft_param_set_int(const char* key, int value);
uft_error_t uft_param_set_float(const char* key, float value);
uft_error_t uft_param_set_string(const char* key, const char* value);
uft_error_t uft_param_set_bool(const char* key, bool value);

/* Load/save parameters */
uft_error_t uft_params_load_json(const char* path);
uft_error_t uft_params_save_json(const char* path);
```

### Presets

```c
/* Load preset by name */
uft_error_t uft_preset_load(const char* preset_name);

/* Get preset list */
int uft_preset_get_list(const char** names, int max_count);

/* Create preset from current settings */
uft_error_t uft_preset_create(const char* name, const char* description);
```

---

## GUI Widgets (Qt6)

### PLL Panel

```cpp
#include <uft/gui/uft_pll_panel.h>

class UftPllPanel : public QWidget
{
    Q_OBJECT
public:
    explicit UftPllPanel(QWidget* parent = nullptr);
    
    // Parameter accessors
    double gain() const;
    double integralGain() const;
    double lockThreshold() const;
    double bitCellTolerance() const;
    QString currentPreset() const;
    
public slots:
    void setGain(double value);
    void setIntegralGain(double value);
    void setLockThreshold(double value);
    void setBitCellTolerance(double value);
    void setPreset(const QString& preset);
    void apply();
    void resetToDefaults();
    
signals:
    void parametersChanged();
    void presetChanged(const QString& preset);
};
```

### Recovery Panel

```cpp
#include <uft/gui/uft_recovery_panel.h>

class UftRecoveryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit UftRecoveryPanel(QWidget* parent = nullptr);
    
    // Parameter accessors
    int maxRetries() const;
    int maxCRCBits() const;
    double weakBitThreshold() const;
    bool multiRevEnabled() const;
    int revolutionCount() const;
    
public slots:
    void setMaxRetries(int value);
    void setMaxCRCBits(int value);
    void setWeakBitThreshold(double value);
    void setMultiRevEnabled(bool enabled);
    void setRevolutionCount(int count);
    
    // Control
    void start();
    void stop();
    void pause();
    
signals:
    void started();
    void stopped();
    void progressChanged(int percent, const QString& status);
    void sectorRecovered(int track, int side, int sector, double confidence);
};
```

### Flux View Widget

```cpp
#include <uft/gui/uft_flux_view_widget.h>

class UftFluxViewWidget : public QWidget
{
    Q_OBJECT
public:
    enum ViewMode { Timeline, Histogram, Overlay, Difference };
    
    explicit UftFluxViewWidget(QWidget* parent = nullptr);
    
    // Data
    void setFluxData(const std::vector<uint32_t>& transitions);
    void setMultiRevFluxData(const std::vector<std::vector<uint32_t>>& revs);
    
    // View control
    ViewMode viewMode() const;
    double zoomLevel() const;
    
public slots:
    void setViewMode(ViewMode mode);
    void setZoomLevel(double zoom);
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    
    // Display options
    void setShowGrid(bool show);
    void setShowWeakBits(bool show);
    void setShowSectors(bool show);
    
signals:
    void viewModeChanged(ViewMode mode);
    void zoomChanged(double level);
    void selectionChanged(int start, int end);
};
```

### Track Grid Widget

```cpp
#include <uft/gui/trackgridwidget.h>

class TrackGridWidget : public QWidget
{
    Q_OBJECT
public:
    enum Status { 
        Empty, HeaderBad, DataBad, Ok, Deleted, 
        Weak, Protected, Writing, Verifying 
    };
    
    explicit TrackGridWidget(QWidget* parent = nullptr);
    
    void setSectorStatus(int track, int sector, Status status);
    Status sectorStatus(int track, int sector) const;
    void setGeometry(int tracks, int sectors);
    
    void setCurrentTrack(int track);
    void setElapsedTime(int seconds);
    
signals:
    void sectorClicked(int track, int sector);
    void sectorHovered(int track, int sector);
};
```

---

## Thread Safety

Most UFT functions are **thread-safe** when operating on different objects. The following rules apply:

1. **Context objects** (`uft_pll_ctx_t`, `uft_recovery_ctx_t`, etc.) are **not thread-safe** - use one per thread or synchronize access
2. **Image handles** (`uft_image_t`) are **not thread-safe** for concurrent read/write
3. **Format registry** is **thread-safe** after initialization
4. **Error functions** (`uft_get_last_error`) are **thread-local**

---

## Version Information

```c
#include <uft/uft_version.h>

#define UFT_VERSION_MAJOR  3
#define UFT_VERSION_MINOR  3
#define UFT_VERSION_PATCH  2
#define UFT_VERSION_STRING "3.3.2"

/* Runtime version check */
const char* uft_version_string(void);
int uft_version_major(void);
int uft_version_minor(void);
int uft_version_patch(void);
```

---

## Example Usage

### Reading an ADF File

```c
#include <uft/uft.h>

int main(void) {
    uft_image_t* image = NULL;
    uft_geometry_t geom;
    uint8_t sector_buf[512];
    size_t bytes_read;
    
    /* Open image */
    if (uft_image_open_file("game.adf", &image) != UFT_ERR_OK) {
        fprintf(stderr, "Failed to open: %s\n", uft_error_string(uft_get_last_error()));
        return 1;
    }
    
    /* Get geometry */
    uft_image_get_geometry(image, &geom);
    printf("Tracks: %d, Sides: %d, Sectors: %d\n", 
           geom.tracks, geom.sides, geom.sectors);
    
    /* Read boot sector */
    if (uft_image_read_sector(image, 0, 0, 0, 
                              sector_buf, sizeof(sector_buf), 
                              &bytes_read) == UFT_ERR_OK) {
        printf("Boot sector: %zu bytes\n", bytes_read);
    }
    
    uft_image_close(image);
    return 0;
}
```

### Processing Flux Data

```c
#include <uft/uft.h>

int main(void) {
    uft_image_t* image = NULL;
    uft_flux_track_t flux;
    uft_raw_track_t raw;
    uft_pll_params_t pll_params;
    uft_pll_ctx_t* pll = NULL;
    
    /* Open SCP flux image */
    uft_image_open_file("disk.scp", &image);
    
    /* Read flux for track 0, side 0 */
    uft_flux_read_track(image, 0, 0, &flux);
    
    /* Setup PLL for Amiga */
    uft_pll_load_preset(UFT_PLL_PRESET_AMIGA_DD, &pll_params);
    uft_pll_create(&pll_params, &pll);
    
    /* Decode flux to raw track */
    uft_pll_process(pll, &flux.revolutions[0], &raw);
    
    /* Process raw track... */
    
    uft_pll_destroy(pll);
    uft_image_close(image);
    return 0;
}
```

---

*Document Version: 3.3.2 | Last Updated: 2026-01-03*
