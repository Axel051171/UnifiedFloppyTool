/**
 * @file uft_formats_extended.h
 * @brief Erweiterte Format-Definitionen und Handler
 * 
 * Zusätzliche Formate:
 * - IPF (Interchangeable Preservation Format)
 * - STX (Pasti)
 * - TD0 (Teledisk)
 * - IMD (ImageDisk)
 * - FDI (Formatted Disk Image)
 * - WOZ (Apple II Flux)
 * - A2R (Applesauce)
 * - NIB (Apple II Nibble)
 */

#ifndef UFT_FORMATS_EXTENDED_H
#define UFT_FORMATS_EXTENDED_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Format Handler Interface
// ============================================================================

typedef struct uft_format_handler {
    uft_format_t    format;
    const char*     name;
    const char*     extension;
    const char*     description;
    const char*     mime_type;
    
    // Capabilities
    bool            supports_read;
    bool            supports_write;
    bool            supports_flux;
    bool            supports_weak_bits;
    bool            supports_multiple_revs;
    
    // Magic detection
    const uint8_t*  magic_bytes;
    size_t          magic_length;
    size_t          magic_offset;
    
    // Handler functions
    uft_error_t     (*probe)(const void* data, size_t size, int* confidence);
    uft_error_t     (*open)(const char* path, void** handle);
    void            (*close)(void* handle);
    uft_error_t     (*read_track)(void* handle, int cyl, int head, void* buffer, size_t* size);
    uft_error_t     (*write_track)(void* handle, int cyl, int head, const void* data, size_t size);
    uft_error_t     (*get_geometry)(void* handle, int* cyls, int* heads, int* sectors);
} uft_format_handler_t;

// ============================================================================
// IPF Format (CAPS/SPS)
// ============================================================================

#define IPF_MAGIC   "CAPS"

typedef struct ipf_header {
    char        magic[4];           // "CAPS"
    uint32_t    version;
    uint32_t    flags;
    uint32_t    num_tracks;
    uint32_t    track_list_offset;
}  ipf_header_t;

typedef struct ipf_track_info {
    uint32_t    cylinder;
    uint32_t    head;
    uint32_t    data_offset;
    uint32_t    data_size;
    uint32_t    num_sectors;
    uint32_t    flags;
    uint32_t    gap_density;
    uint32_t    data_bits;
}  ipf_track_info_t;

uft_error_t uft_ipf_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_ipf_open(const char* path, void** handle);
void uft_ipf_close(void* handle);
uft_error_t uft_ipf_read_track(void* handle, int cyl, int head, void* buf, size_t* size);
uft_error_t uft_ipf_get_info(void* handle, int* cyls, int* heads, int* sectors);

// ============================================================================
// STX Format (Pasti)
// ============================================================================

#define STX_MAGIC   "RSY\0"

typedef struct stx_header {
    char        magic[4];           // "RSY\0"
    uint16_t    version;
    uint16_t    tool_version;
    uint16_t    reserved1;
    uint8_t     tracks_per_side;
    uint8_t     sides;
    uint32_t    reserved2;
}  stx_header_t;

typedef struct stx_track_header {
    uint32_t    record_size;
    uint32_t    fuzzy_size;
    uint16_t    sector_count;
    uint16_t    flags;
    uint16_t    track_length;
    uint8_t     track_type;
    uint8_t     track_number;
}  stx_track_header_t;

uft_error_t uft_stx_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_stx_open(const char* path, void** handle);
void uft_stx_close(void* handle);
uft_error_t uft_stx_read_track(void* handle, int cyl, int head, void* buf, size_t* size);

// ============================================================================
// TD0 Format (Teledisk)
// ============================================================================

#define TD0_MAGIC_NORMAL    "TD"
#define TD0_MAGIC_ADVANCED  "td"

typedef struct td0_header {
    char        magic[2];           // "TD" or "td" (advanced compression)
    uint8_t     volume_sequence;
    uint8_t     check_sig;
    uint8_t     version;
    uint8_t     density;
    uint8_t     drive_type;
    uint8_t     stepping;
    uint8_t     dos_alloc;
    uint8_t     sides;
    uint16_t    crc;
}  td0_header_t;

typedef struct td0_track_header {
    uint8_t     sectors;
    uint8_t     cylinder;
    uint8_t     head;
    uint8_t     crc;
}  td0_track_header_t;

uft_error_t uft_td0_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_td0_open(const char* path, void** handle);
void uft_td0_close(void* handle);
uft_error_t uft_td0_read_track(void* handle, int cyl, int head, void* buf, size_t* size);
uft_error_t uft_td0_decompress(const uint8_t* src, size_t src_size, 
                                uint8_t* dst, size_t* dst_size);

// ============================================================================
// IMD Format (ImageDisk)
// ============================================================================

#define IMD_MAGIC   "IMD "

typedef struct imd_sector_header {
    uint8_t     mode;               // Mode/density
    uint8_t     cylinder;
    uint8_t     head;
    uint8_t     sector_count;
    uint8_t     sector_size;        // 0=128, 1=256, 2=512, 3=1024, etc.
}  imd_sector_header_t;

// IMD sector data types
#define IMD_SECTOR_UNAVAIL      0
#define IMD_SECTOR_NORMAL       1
#define IMD_SECTOR_COMPRESSED   2
#define IMD_SECTOR_DELETED      3
#define IMD_SECTOR_DEL_COMPR    4
#define IMD_SECTOR_ERROR        5
#define IMD_SECTOR_ERR_COMPR    6
#define IMD_SECTOR_DEL_ERR      7
#define IMD_SECTOR_DEL_ERR_COMPR 8

uft_error_t uft_imd_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_imd_open(const char* path, void** handle);
void uft_imd_close(void* handle);
uft_error_t uft_imd_read_track(void* handle, int cyl, int head, void* buf, size_t* size);

// ============================================================================
// WOZ Format (Apple II Flux)
// ============================================================================

#define WOZ_MAGIC   "WOZ1"
#define WOZ2_MAGIC  "WOZ2"

typedef struct woz_header {
    char        magic[4];           // "WOZ1" or "WOZ2"
    uint8_t     high_bit;           // 0xFF
    uint8_t     lfcrlf[3];          // LF CR LF
    uint32_t    crc32;
}  woz_header_t;

typedef struct woz_info_chunk {
    uint32_t    chunk_id;           // "INFO"
    uint32_t    chunk_size;
    uint8_t     version;
    uint8_t     disk_type;          // 1=5.25, 2=3.5
    uint8_t     write_protected;
    uint8_t     synchronized;
    uint8_t     cleaned;
    char        creator[32];
}  woz_info_chunk_t;

uft_error_t uft_woz_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_woz_open(const char* path, void** handle);
void uft_woz_close(void* handle);
uft_error_t uft_woz_read_track(void* handle, int track, void* buf, size_t* size);

// ============================================================================
// A2R Format (Applesauce)
// ============================================================================

#define A2R_MAGIC   "A2R2"

typedef struct a2r_header {
    char        magic[4];           // "A2R2"
    uint8_t     high_bit;           // 0xFF
    uint8_t     lfcrlf[3];
}  a2r_header_t;

uft_error_t uft_a2r_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_a2r_open(const char* path, void** handle);
void uft_a2r_close(void* handle);

// ============================================================================
// NIB Format (Apple II Nibble)
// ============================================================================

#define NIB_TRACK_SIZE  6656
#define NIB_DISK_SIZE   (NIB_TRACK_SIZE * 35)

uft_error_t uft_nib_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_nib_open(const char* path, void** handle);
void uft_nib_close(void* handle);
uft_error_t uft_nib_read_track(void* handle, int track, void* buf, size_t* size);

// ============================================================================
// FDI Format (Formatted Disk Image)
// ============================================================================

#define FDI_MAGIC   "Formatted Disk Image file\r\n"

typedef struct fdi_header {
    char        magic[27];
    uint8_t     write_protected;
    uint16_t    cylinders;
    uint16_t    heads;
    uint16_t    description_offset;
    uint16_t    data_offset;
    uint16_t    extra_header_size;
}  fdi_header_t;

uft_error_t uft_fdi_probe(const void* data, size_t size, int* confidence);
uft_error_t uft_fdi_open(const char* path, void** handle);
void uft_fdi_close(void* handle);

// ============================================================================
// Format Registry
// ============================================================================

uft_error_t uft_format_registry_init(void);
void uft_format_registry_shutdown(void);

const uft_format_handler_t* uft_format_get_handler(uft_format_t format);
const uft_format_handler_t* uft_format_detect(const void* data, size_t size);
const uft_format_handler_t* uft_format_detect_by_extension(const char* filename);

int uft_format_list_handlers(const uft_format_handler_t** handlers, int max);
int uft_format_list_by_capability(bool needs_flux, bool needs_write,
                                   const uft_format_handler_t** handlers, int max);

// ============================================================================
// Conversion Matrix
// ============================================================================

typedef struct uft_conversion_info {
    uft_format_t    source;
    uft_format_t    target;
    bool            possible;
    bool            lossy;
    const char*     warning;
} uft_conversion_info_t;

bool uft_format_can_convert(uft_format_t src, uft_format_t dst, 
                             const char** warning);
uft_error_t uft_format_convert(const char* src_path, const char* dst_path,
                                uft_format_t dst_format);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMATS_EXTENDED_H
