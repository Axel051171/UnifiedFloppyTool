/**
 * @file uft_format_registry.c
 * @brief Format Handler Registry
 */

#include "uft/uft_formats_extended.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uft/uft_compat.h"

// ============================================================================
// Magic Bytes
// ============================================================================

static const uint8_t magic_scp[]  = { 'S', 'C', 'P' };
static const uint8_t magic_hfe[]  = { 'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E' };
static const uint8_t magic_ipf[]  = { 'C', 'A', 'P', 'S' };
static const uint8_t magic_stx[]  = { 'R', 'S', 'Y', 0 };
// static const uint8_t magic_td0[]  = { 'T', 'D' };
static const uint8_t magic_imd[]  = { 'I', 'M', 'D', ' ' };
static const uint8_t magic_woz1[] = { 'W', 'O', 'Z', '1' };
static const uint8_t magic_woz2[] = { 'W', 'O', 'Z', '2' };
static const uint8_t magic_a2r[]  = { 'A', '2', 'R', '2' };
static const uint8_t magic_fdi[]  = { 'F', 'o', 'r', 'm', 'a', 't', 't', 'e', 'd' };
static const uint8_t magic_g64[]  = { 'G', 'C', 'R', '-', '1', '5', '4', '1' };

// ============================================================================
// Probe Functions
// ============================================================================

static uft_error_t probe_by_magic(const void* data, size_t size,
                                   const uint8_t* magic, size_t magic_len,
                                   size_t offset, int* confidence) {
    if (!data || size < offset + magic_len) {
        *confidence = 0;
        return UFT_ERR_FORMAT;
    }
    
    if (memcmp((const uint8_t*)data + offset, magic, magic_len) == 0) {
        *confidence = 95;
        return UFT_OK;
    }
    
    *confidence = 0;
    return UFT_ERR_FORMAT;
}

uft_error_t uft_scp_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_scp, 3, 0, confidence);
}

uft_error_t uft_hfe_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_hfe, 8, 0, confidence);
}

uft_error_t uft_ipf_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_ipf, 4, 0, confidence);
}

uft_error_t uft_stx_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_stx, 4, 0, confidence);
}

uft_error_t uft_td0_probe(const void* data, size_t size, int* confidence) {
    if (!data || size < 2) {
        *confidence = 0;
        return UFT_ERR_FORMAT;
    }
    
    const uint8_t* d = data;
    // Normal: "TD", Advanced: "td"
    if ((d[0] == 'T' && d[1] == 'D') || (d[0] == 't' && d[1] == 'd')) {
        *confidence = 90;
        return UFT_OK;
    }
    
    *confidence = 0;
    return UFT_ERR_FORMAT;
}

uft_error_t uft_imd_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_imd, 4, 0, confidence);
}

uft_error_t uft_woz_probe(const void* data, size_t size, int* confidence) {
    if (probe_by_magic(data, size, magic_woz1, 4, 0, confidence) == UFT_OK) {
        return UFT_OK;
    }
    return probe_by_magic(data, size, magic_woz2, 4, 0, confidence);
}

uft_error_t uft_a2r_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_a2r, 4, 0, confidence);
}

uft_error_t uft_fdi_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_fdi, 9, 0, confidence);
}

uft_error_t uft_g64_probe(const void* data, size_t size, int* confidence) {
    return probe_by_magic(data, size, magic_g64, 8, 0, confidence);
}

uft_error_t uft_nib_probe(const void* data, size_t size, int* confidence) {
    // NIB has no magic, check size
    if (size == NIB_DISK_SIZE || size == NIB_DISK_SIZE * 2) {
        *confidence = 60;  // Lower confidence due to no magic
        return UFT_OK;
    }
    *confidence = 0;
    return UFT_ERR_FORMAT;
}

uft_error_t uft_d64_probe(const void* data, size_t size, int* confidence) {
    // D64 has no magic, check size
    // 35 tracks: 174848, 40 tracks: 196608, with errors: +683/+768
    if (size == 174848 || size == 175531 ||
        size == 196608 || size == 197376) {
        *confidence = 70;
        return UFT_OK;
    }
    *confidence = 0;
    return UFT_ERR_FORMAT;
}

uft_error_t uft_adf_probe(const void* data, size_t size, int* confidence) {
    // ADF has no magic, check size
    // DD: 901120, HD: 1802240
    if (size == 901120 || size == 1802240) {
        *confidence = 70;
        return UFT_OK;
    }
    *confidence = 0;
    return UFT_ERR_FORMAT;
}

uft_error_t uft_img_probe(const void* data, size_t size, int* confidence) {
    // IMG/IMA has no magic, check for common sizes
    // 360KB, 720KB, 1.2MB, 1.44MB, 2.88MB
    size_t valid_sizes[] = { 368640, 737280, 1228800, 1474560, 2949120 };
    for (int i = 0; i < 5; i++) {
        if (size == valid_sizes[i]) {
            *confidence = 65;
            return UFT_OK;
        }
    }
    *confidence = 0;
    return UFT_ERR_FORMAT;
}

// ============================================================================
// Built-in Format Handlers
// ============================================================================

static const uft_format_handler_t g_format_handlers[] = {
    // === FLUX FORMATS ===
    {
        .format = UFT_FORMAT_SCP,
        .name = "SCP",
        .extension = ".scp",
        .description = "SuperCard Pro Flux Image",
        .mime_type = "application/x-supercard-pro",
        .supports_read = true,
        .supports_write = true,
        .supports_flux = true,
        .supports_weak_bits = true,
        .supports_multiple_revs = true,
        .magic_bytes = magic_scp,
        .magic_length = 3,
        .magic_offset = 0,
        .probe = uft_scp_probe,
    },
    {
        .format = UFT_FORMAT_HFE,
        .name = "HFE",
        .extension = ".hfe",
        .description = "UFT HFE Format Image",
        .mime_type = "application/x-hxc-floppy",
        .supports_read = true,
        .supports_write = true,
        .supports_flux = true,
        .supports_weak_bits = false,
        .magic_bytes = magic_hfe,
        .magic_length = 8,
        .magic_offset = 0,
        .probe = uft_hfe_probe,
    },
    {
        .format = UFT_FORMAT_IPF,
        .name = "IPF",
        .extension = ".ipf",
        .description = "Interchangeable Preservation Format (CAPS/SPS)",
        .mime_type = "application/x-ipf",
        .supports_read = true,
        .supports_write = false,  // Usually read-only
        .supports_flux = true,
        .supports_weak_bits = true,
        .magic_bytes = magic_ipf,
        .magic_length = 4,
        .magic_offset = 0,
        .probe = uft_ipf_probe,
    },
    {
        .format = UFT_FORMAT_STX,
        .name = "STX",
        .extension = ".stx",
        .description = "Pasti Atari ST Image",
        .mime_type = "application/x-pasti",
        .supports_read = true,
        .supports_write = true,
        .supports_flux = true,
        .supports_weak_bits = true,
        .magic_bytes = magic_stx,
        .magic_length = 4,
        .magic_offset = 0,
        .probe = uft_stx_probe,
    },
    {
        .format = UFT_FORMAT_KRYOFLUX,
        .name = "Kryoflux",
        .extension = ".raw",
        .description = "Kryoflux Stream Files",
        .mime_type = "application/x-kryoflux",
        .supports_read = true,
        .supports_write = false,
        .supports_flux = true,
        .supports_weak_bits = true,
        .supports_multiple_revs = true,
    },
    {
        .format = UFT_FORMAT_A2R,
        .name = "A2R",
        .extension = ".a2r",
        .description = "Applesauce Flux Image",
        .mime_type = "application/x-applesauce",
        .supports_read = true,
        .supports_write = true,
        .supports_flux = true,
        .magic_bytes = magic_a2r,
        .magic_length = 4,
        .magic_offset = 0,
        .probe = uft_a2r_probe,
    },
    {
        .format = UFT_FORMAT_WOZ,
        .name = "WOZ",
        .extension = ".woz",
        .description = "Apple II Flux Image",
        .mime_type = "application/x-woz",
        .supports_read = true,
        .supports_write = true,
        .supports_flux = true,
        .probe = uft_woz_probe,
    },
    
    // === SECTOR FORMATS ===
    {
        .format = UFT_FORMAT_D64,
        .name = "D64",
        .extension = ".d64",
        .description = "Commodore 64 Disk Image",
        .mime_type = "application/x-d64",
        .supports_read = true,
        .supports_write = true,
        .supports_flux = false,
        .probe = uft_d64_probe,
    },
    {
        .format = UFT_FORMAT_G64,
        .name = "G64",
        .extension = ".g64",
        .description = "Commodore GCR Disk Image",
        .mime_type = "application/x-g64",
        .supports_read = true,
        .supports_write = true,
        .supports_flux = true,
        .magic_bytes = magic_g64,
        .magic_length = 8,
        .probe = uft_g64_probe,
    },
    {
        .format = UFT_FORMAT_ADF,
        .name = "ADF",
        .extension = ".adf",
        .description = "Amiga Disk File",
        .mime_type = "application/x-amiga-disk-format",
        .supports_read = true,
        .supports_write = true,
        .probe = uft_adf_probe,
    },
    {
        .format = UFT_FORMAT_IMG,
        .name = "IMG",
        .extension = ".img",
        .description = "Raw Disk Image",
        .mime_type = "application/x-raw-disk-image",
        .supports_read = true,
        .supports_write = true,
        .probe = uft_img_probe,
    },
    {
        .format = UFT_FORMAT_DSK,
        .name = "DSK",
        .extension = ".dsk",
        .description = "Apple/Atari Disk Image",
        .mime_type = "application/x-dsk",
        .supports_read = true,
        .supports_write = true,
    },
    {
        .format = UFT_FORMAT_IMD,
        .name = "IMD",
        .extension = ".imd",
        .description = "ImageDisk Image",
        .mime_type = "application/x-imagedisk",
        .supports_read = true,
        .supports_write = true,
        .magic_bytes = magic_imd,
        .magic_length = 4,
        .probe = uft_imd_probe,
    },
    {
        .format = UFT_FORMAT_TD0,
        .name = "TD0",
        .extension = ".td0",
        .description = "Teledisk Image",
        .mime_type = "application/x-teledisk",
        .supports_read = true,
        .supports_write = false,
        .probe = uft_td0_probe,
    },
    {
        .format = UFT_FORMAT_FDI,
        .name = "FDI",
        .extension = ".fdi",
        .description = "Formatted Disk Image",
        .mime_type = "application/x-fdi",
        .supports_read = true,
        .supports_write = true,
        .magic_bytes = magic_fdi,
        .magic_length = 9,
        .probe = uft_fdi_probe,
    },
    {
        .format = UFT_FORMAT_NIB,
        .name = "NIB",
        .extension = ".nib",
        .description = "Apple II Nibble Image",
        .mime_type = "application/x-nibble",
        .supports_read = true,
        .supports_write = true,
        .probe = uft_nib_probe,
    },
    
    // Terminator
    { .format = UFT_FORMAT_UNKNOWN }
};

#define NUM_FORMAT_HANDLERS \
    (sizeof(g_format_handlers) / sizeof(g_format_handlers[0]) - 1)

// ============================================================================
// Registry API
// ============================================================================

static bool g_format_registry_initialized = false;

uft_error_t uft_format_registry_init(void) {
    g_format_registry_initialized = true;
    return UFT_OK;
}

void uft_format_registry_shutdown(void) {
    g_format_registry_initialized = false;
}

const uft_format_handler_t* uft_format_get_handler(uft_format_t format) {
    for (size_t i = 0; i < NUM_FORMAT_HANDLERS; i++) {
        if (g_format_handlers[i].format == format) {
            return &g_format_handlers[i];
        }
    }
    return NULL;
}

const uft_format_handler_t* uft_format_detect(const void* data, size_t size) {
    if (!data || size == 0) return NULL;
    
    int best_confidence = 0;
    const uft_format_handler_t* best = NULL;
    
    for (size_t i = 0; i < NUM_FORMAT_HANDLERS; i++) {
        if (!g_format_handlers[i].probe) continue;
        
        int confidence = 0;
        if (g_format_handlers[i].probe(data, size, &confidence) == UFT_OK) {
            if (confidence > best_confidence) {
                best_confidence = confidence;
                best = &g_format_handlers[i];
            }
        }
    }
    
    return best;
}

const uft_format_handler_t* uft_format_detect_by_extension(const char* filename) {
    if (!filename) return NULL;
    
    const char* ext = strrchr(filename, '.');
    if (!ext) return NULL;
    
    for (size_t i = 0; i < NUM_FORMAT_HANDLERS; i++) {
        if (g_format_handlers[i].extension &&
            strcasecmp(ext, g_format_handlers[i].extension) == 0) {
            return &g_format_handlers[i];
        }
    }
    
    return NULL;
}

int uft_format_list_handlers(const uft_format_handler_t** handlers, int max) {
    int count = 0;
    for (size_t i = 0; i < NUM_FORMAT_HANDLERS && count < max; i++) {
        if (handlers) handlers[count] = &g_format_handlers[i];
        count++;
    }
    return count;
}

int uft_format_list_by_capability(bool needs_flux, bool needs_write,
                                   const uft_format_handler_t** handlers, int max) {
    int count = 0;
    for (size_t i = 0; i < NUM_FORMAT_HANDLERS && count < max; i++) {
        const uft_format_handler_t* h = &g_format_handlers[i];
        
        if (needs_flux && !h->supports_flux) continue;
        if (needs_write && !h->supports_write) continue;
        
        if (handlers) handlers[count] = h;
        count++;
    }
    return count;
}

// ============================================================================
// Conversion Matrix
// ============================================================================

bool uft_format_can_convert(uft_format_t src, uft_format_t dst, 
                             const char** warning) {
    const uft_format_handler_t* src_h = uft_format_get_handler(src);
    const uft_format_handler_t* dst_h = uft_format_get_handler(dst);
    
    if (!src_h || !dst_h) {
        if (warning) *warning = "Unknown format";
        return false;
    }
    
    // Flux → Sector: possible but lossy
    if (src_h->supports_flux && !dst_h->supports_flux) {
        if (warning) *warning = "Converting flux to sector format will lose timing information";
        return true;
    }
    
    // Sector → Flux: possible via synthesis
    if (!src_h->supports_flux && dst_h->supports_flux) {
        if (warning) *warning = "Synthesized flux data (original timing unavailable)";
        return true;
    }
    
    // Weak bits loss
    if (src_h->supports_weak_bits && !dst_h->supports_weak_bits) {
        if (warning) *warning = "Target format does not preserve weak bits";
        return true;
    }
    
    if (warning) *warning = NULL;
    return true;
}
