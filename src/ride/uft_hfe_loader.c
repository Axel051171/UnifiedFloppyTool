/**
 * @file uft_hfe_loader.c
 * @brief HFE (HxC Floppy Emulator) Format Loader
 * 
 * Implements loading of HFE v1 and v3 disk images used by
 * HxC Floppy Emulator hardware and software.
 * 
 * @license MIT
 */

#include "uft/ride/uft_flux_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * HFE FILE FORMAT STRUCTURES
 *============================================================================*/

#define HFE_MAGIC           "HXCPICFE"
#define HFE_MAGIC_V3        "HXCHFEV3"
#define HFE_HEADER_SIZE     512
#define HFE_BLOCK_SIZE      512

/* HFE Header (offset 0x00) */
typedef struct __attribute__((packed)) {
    char     magic[8];              /* "HXCPICFE" or "HXCHFEV3" */
    uint8_t  format_revision;       /* File format revision */
    uint8_t  number_of_tracks;      /* Total tracks */
    uint8_t  number_of_sides;       /* 1 or 2 */
    uint8_t  track_encoding;        /* Encoding type */
    uint16_t bitrate;               /* Bitrate in kbit/s */
    uint16_t rpm;                   /* RPM (300 or 360) */
    uint8_t  interface_mode;        /* Interface mode */
    uint8_t  reserved;
    uint16_t track_list_offset;     /* Offset to track list (blocks) */
    uint8_t  write_allowed;         /* Write allowed flag */
    uint8_t  single_step;           /* Single step flag */
    uint8_t  track0_alt_encoding;   /* Track 0 side 0 encoding */
    uint8_t  track0_alt_encoding_s1;/* Track 0 side 1 encoding */
    uint8_t  track1_alt_encoding;   /* Track 1 encoding */
    uint8_t  reserved2[5];
} hfe_header_t;

/* Track entry in track list */
typedef struct __attribute__((packed)) {
    uint16_t offset;                /* Track data offset (blocks) */
    uint16_t track_len;             /* Track length (bytes) */
} hfe_track_entry_t;

/* HFE encoding types */
#define HFE_ENC_UNKNOWN     0x00
#define HFE_ENC_MFM         0x00
#define HFE_ENC_FM          0x01
#define HFE_ENC_GCR         0x02

/* HFE interface modes */
#define HFE_IF_IBMPC_DD     0x00
#define HFE_IF_IBMPC_HD     0x01
#define HFE_IF_ATARI_ST_DD  0x02
#define HFE_IF_ATARI_ST_HD  0x03
#define HFE_IF_AMIGA_DD     0x04
#define HFE_IF_AMIGA_HD     0x05
#define HFE_IF_CPC_DD       0x06
#define HFE_IF_MSX2_DD      0x08
#define HFE_IF_C64_DD       0x09
#define HFE_IF_EMU_SHUGART  0x0A

/*============================================================================
 * HFE LOADER IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Decode HFE encoding type to string
 */
static const char *hfe_encoding_name(uint8_t enc) {
    switch (enc) {
        case HFE_ENC_MFM: return "MFM";
        case HFE_ENC_FM:  return "FM";
        case HFE_ENC_GCR: return "GCR";
        default:          return "Unknown";
    }
}

/**
 * @brief Decode HFE interface mode to string
 */
static const char *hfe_interface_name(uint8_t mode) {
    switch (mode) {
        case HFE_IF_IBMPC_DD:   return "IBM PC DD";
        case HFE_IF_IBMPC_HD:   return "IBM PC HD";
        case HFE_IF_ATARI_ST_DD: return "Atari ST DD";
        case HFE_IF_ATARI_ST_HD: return "Atari ST HD";
        case HFE_IF_AMIGA_DD:   return "Amiga DD";
        case HFE_IF_AMIGA_HD:   return "Amiga HD";
        case HFE_IF_CPC_DD:     return "Amstrad CPC DD";
        case HFE_IF_MSX2_DD:    return "MSX2 DD";
        case HFE_IF_C64_DD:     return "Commodore 64 DD";
        case HFE_IF_EMU_SHUGART: return "Shugart";
        default:               return "Unknown";
    }
}

/**
 * @brief Get HFE file information
 */
int uft_hfe_get_info(const char *path, uft_hfe_info_t *info) {
    if (!path || !info) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    hfe_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    
    /* Check magic */
    if (memcmp(header.magic, HFE_MAGIC, 8) == 0) {
        info->version = 1;
    } else if (memcmp(header.magic, HFE_MAGIC_V3, 8) == 0) {
        info->version = 3;
    } else {
        return -1;
    }
    
    info->tracks = header.number_of_tracks;
    info->sides = header.number_of_sides;
    info->encoding = header.track_encoding;
    info->bitrate = header.bitrate;
    info->rpm = header.rpm;
    info->interface_mode = header.interface_mode;
    info->write_allowed = header.write_allowed != 0;
    
    strncpy(info->encoding_str, hfe_encoding_name(header.track_encoding), 
            sizeof(info->encoding_str) - 1);
    strncpy(info->interface_str, hfe_interface_name(header.interface_mode),
            sizeof(info->interface_str) - 1);
    
    return 0;
}

/**
 * @brief Load HFE track into flux buffer
 */
uft_flux_buffer_t *uft_flux_load_hfe(const char *path, int track, int side) {
    if (!path || track < 0 || side < 0 || side > 1) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    hfe_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return NULL;
    }
    
    /* Validate magic */
    bool v3 = false;
    if (memcmp(header.magic, HFE_MAGIC, 8) == 0) {
        v3 = false;
    } else if (memcmp(header.magic, HFE_MAGIC_V3, 8) == 0) {
        v3 = true;
    } else {
        fclose(f);
        return NULL;
    }
    
    /* TODO: Handle HFEv3 specific track format differences */
    (void)v3;
    
    /* Check track bounds */
    if (track >= header.number_of_tracks || side >= header.number_of_sides) {
        fclose(f);
        return NULL;
    }
    
    /* Read track list entry */
    size_t track_list_pos = header.track_list_offset * HFE_BLOCK_SIZE;
    fseek(f, track_list_pos + track * sizeof(hfe_track_entry_t), SEEK_SET);
    
    hfe_track_entry_t track_entry;
    if (fread(&track_entry, 1, sizeof(track_entry), f) != sizeof(track_entry)) {
        fclose(f);
        return NULL;
    }
    
    if (track_entry.offset == 0 || track_entry.track_len == 0) {
        fclose(f);
        return NULL;
    }
    
    /* Seek to track data */
    size_t track_data_pos = track_entry.offset * HFE_BLOCK_SIZE;
    fseek(f, track_data_pos, SEEK_SET);
    
    /* HFE stores both sides interleaved in 256-byte blocks */
    size_t total_len = track_entry.track_len;
    uint8_t *track_data = malloc(total_len);
    if (!track_data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(track_data, 1, total_len, f) != total_len) {
        free(track_data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    /* Extract side data (interleaved 256-byte blocks) */
    size_t side_len = total_len / 2;
    uint8_t *side_data = malloc(side_len);
    if (!side_data) {
        free(track_data);
        return NULL;
    }
    
    size_t out_pos = 0;
    for (size_t block = 0; block < total_len / 512; block++) {
        size_t base = block * 512 + side * 256;
        memcpy(side_data + out_pos, track_data + base, 256);
        out_pos += 256;
    }
    
    free(track_data);
    
    /* Create flux buffer from MFM/FM bitstream */
    /* HFE stores raw bitcell data, we need to convert to flux transitions */
    
    /* Calculate timing based on bitrate */
    double cell_ns = 1000000.0 / header.bitrate;  /* ns per bit cell */
    
    /* Estimate flux count (approximately 1 transition per 2 bits for MFM) */
    size_t est_flux = side_len * 8 / 2;
    
    uft_flux_buffer_t *flux = uft_flux_create(est_flux + 1024);
    if (!flux) {
        free(side_data);
        return NULL;
    }
    
    /* Set encoding based on header */
    switch (header.track_encoding) {
        case HFE_ENC_FM:
            flux->detected_enc = UFT_ENC_FM;
            break;
        case HFE_ENC_GCR:
            flux->detected_enc = UFT_ENC_GCR_APPLE;
            break;
        default:
            flux->detected_enc = UFT_ENC_MFM;
            break;
    }
    
    /* Convert bitstream to flux transitions */
    uft_logtime_t accum = 0;
    
    for (size_t i = 0; i < side_len; i++) {
        uint8_t byte = side_data[i];
        
        /* HFE stores bits MSB first */
        for (int bit = 7; bit >= 0; bit--) {
            accum += (uft_logtime_t)cell_ns;
            
            if (byte & (1 << bit)) {
                /* Flux transition */
                uft_flux_add_transition(flux, accum);
                accum = 0;
            }
        }
    }
    
    /* Add final transition if needed */
    if (accum > 0) {
        uft_flux_add_transition(flux, accum);
    }
    
    free(side_data);
    
    /* Set density based on bitrate */
    if (header.bitrate >= 400) {
        flux->detected_den = UFT_DENSITY_HD;
    } else if (header.bitrate >= 200) {
        flux->detected_den = UFT_DENSITY_DD;
    } else {
        flux->detected_den = UFT_DENSITY_SD;
    }
    
    return flux;
}

/**
 * @brief Write flux buffer to HFE format
 */
int uft_flux_write_hfe(const char *path, 
                        uft_flux_buffer_t **tracks,
                        int num_tracks, int num_sides) {
    if (!path || !tracks || num_tracks < 1 || num_sides < 1) {
        return -1;
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    /* Prepare header */
    hfe_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, HFE_MAGIC, 8);
    header.format_revision = 0;
    header.number_of_tracks = num_tracks;
    header.number_of_sides = num_sides;
    header.track_encoding = HFE_ENC_MFM;  /* Default to MFM */
    header.bitrate = 250;  /* DD default */
    header.rpm = 300;
    header.interface_mode = HFE_IF_IBMPC_DD;
    header.track_list_offset = 1;  /* After header */
    header.write_allowed = 1;
    header.single_step = 0;
    
    /* Detect encoding from first track */
    if (tracks[0]) {
        switch (tracks[0]->detected_enc) {
            case UFT_ENC_FM:
                header.track_encoding = HFE_ENC_FM;
                header.bitrate = 125;
                break;
            case UFT_ENC_GCR_APPLE:
                header.track_encoding = HFE_ENC_GCR;
                break;
            default:
                header.track_encoding = HFE_ENC_MFM;
                break;
        }
        
        if (tracks[0]->detected_den == UFT_DENSITY_HD) {
            header.bitrate = 500;
            header.interface_mode = HFE_IF_IBMPC_HD;
        }
    }
    
    /* Write header */
    fwrite(&header, 1, sizeof(header), f);
    
    /* Pad to block boundary */
    uint8_t padding[HFE_BLOCK_SIZE - sizeof(header)];
    memset(padding, 0, sizeof(padding));
    fwrite(padding, 1, sizeof(padding), f);
    
    /* Prepare track list */
    hfe_track_entry_t *track_list = calloc(num_tracks, sizeof(hfe_track_entry_t));
    if (!track_list) {
        fclose(f);
        return -1;
    }
    
    /* Calculate track offsets */
    uint16_t current_offset = 2;  /* After header + track list */
    
    /* Account for track list size */
    size_t track_list_blocks = (num_tracks * sizeof(hfe_track_entry_t) + 
                                HFE_BLOCK_SIZE - 1) / HFE_BLOCK_SIZE;
    current_offset += track_list_blocks;
    
    for (int t = 0; t < num_tracks; t++) {
        track_list[t].offset = current_offset;
        track_list[t].track_len = 12500;  /* ~100ms at 250kbit/s */
        current_offset += (track_list[t].track_len + HFE_BLOCK_SIZE - 1) / HFE_BLOCK_SIZE;
    }
    
    /* Write track list */
    fwrite(track_list, sizeof(hfe_track_entry_t), num_tracks, f);
    
    /* Pad track list to block boundary */
    size_t list_bytes = num_tracks * sizeof(hfe_track_entry_t);
    size_t list_padding = (HFE_BLOCK_SIZE - (list_bytes % HFE_BLOCK_SIZE)) % HFE_BLOCK_SIZE;
    if (list_padding > 0) {
        uint8_t pad[HFE_BLOCK_SIZE];
        memset(pad, 0, sizeof(pad));
        fwrite(pad, 1, list_padding, f);
    }
    
    /* Write track data */
    for (int t = 0; t < num_tracks; t++) {
        /* TODO: Convert flux to HFE bitstream format */
        /* For now, write empty track */
        uint8_t empty_track[12500];
        memset(empty_track, 0, sizeof(empty_track));
        fwrite(empty_track, 1, track_list[t].track_len, f);
        
        /* Pad to block boundary */
        size_t track_padding = (HFE_BLOCK_SIZE - (track_list[t].track_len % HFE_BLOCK_SIZE)) 
                               % HFE_BLOCK_SIZE;
        if (track_padding > 0) {
            uint8_t pad[HFE_BLOCK_SIZE];
            memset(pad, 0, sizeof(pad));
            fwrite(pad, 1, track_padding, f);
        }
    }
    
    free(track_list);
    fclose(f);
    
    return 0;
}

/*============================================================================
 * HFE VERIFICATION
 *============================================================================*/

/**
 * @brief Verify HFE file integrity
 * 
 * Checks:
 * - Valid magic signature (HXC or HFEv3)
 * - Consistent header values
 * - Track list integrity
 * - File size consistency
 * 
 * @param path Path to HFE file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_hfe(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format_name = "HFE";
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Cannot open file: %s", path);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Check minimum size */
    if (file_size < (long)sizeof(hfe_header_t)) {
        fclose(f);
        result->valid = false;
        result->error_code = 1;
        snprintf(result->details, sizeof(result->details),
                 "File too small: %ld bytes (min %zu)",
                 file_size, sizeof(hfe_header_t));
        return 0;
    }
    
    /* Read header */
    hfe_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        result->valid = false;
        result->error_code = 2;
        snprintf(result->details, sizeof(result->details),
                 "Failed to read header");
        return 0;
    }
    
    /* Check magic */
    bool v1 = (memcmp(header.magic, HFE_MAGIC, 8) == 0);
    bool v3 = (memcmp(header.magic, HFE_MAGIC_V3, 8) == 0);
    
    if (!v1 && !v3) {
        fclose(f);
        result->valid = false;
        result->error_code = 3;
        snprintf(result->details, sizeof(result->details),
                 "Invalid signature: %.8s", header.magic);
        return 0;
    }
    
    /* Validate header values */
    if (header.number_of_tracks == 0 || header.number_of_tracks > 200) {
        fclose(f);
        result->valid = false;
        result->error_code = 4;
        snprintf(result->details, sizeof(result->details),
                 "Invalid track count: %d", header.number_of_tracks);
        return 0;
    }
    
    if (header.number_of_sides != 1 && header.number_of_sides != 2) {
        fclose(f);
        result->valid = false;
        result->error_code = 5;
        snprintf(result->details, sizeof(result->details),
                 "Invalid side count: %d", header.number_of_sides);
        return 0;
    }
    
    /* Check track list */
    long track_list_pos = header.track_list_offset * HFE_BLOCK_SIZE;
    if (track_list_pos >= file_size) {
        fclose(f);
        result->valid = false;
        result->error_code = 6;
        snprintf(result->details, sizeof(result->details),
                 "Track list offset past EOF: %ld", track_list_pos);
        return 0;
    }
    
    /* Read and validate track entries */
    fseek(f, track_list_pos, SEEK_SET);
    int total_tracks = header.number_of_tracks;
    size_t track_list_size = total_tracks * sizeof(hfe_track_entry_t);
    hfe_track_entry_t *entries = malloc(track_list_size);
    
    if (!entries) {
        fclose(f);
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Memory allocation failed");
        return -1;
    }
    
    if (fread(entries, 1, track_list_size, f) != track_list_size) {
        free(entries);
        fclose(f);
        result->valid = false;
        result->error_code = 7;
        snprintf(result->details, sizeof(result->details),
                 "Failed to read track list");
        return 0;
    }
    
    /* Validate each track entry */
    int bad_tracks = 0;
    for (int t = 0; t < total_tracks; t++) {
        long track_pos = entries[t].offset * HFE_BLOCK_SIZE;
        if (track_pos >= file_size || track_pos < 0) {
            bad_tracks++;
        }
        if (entries[t].track_len == 0) {
            bad_tracks++;
        }
    }
    
    free(entries);
    fclose(f);
    
    if (bad_tracks > 0) {
        result->valid = false;
        result->error_code = 8;
        snprintf(result->details, sizeof(result->details),
                 "HFE %s, %d tracks, %d sides, %d bad entries",
                 v3 ? "v3" : "v1",
                 header.number_of_tracks,
                 header.number_of_sides,
                 bad_tracks);
        return 0;
    }
    
    /* All checks passed */
    result->valid = true;
    result->error_code = 0;
    snprintf(result->details, sizeof(result->details),
             "HFE %s OK: %d tracks, %d sides, %s, %d kbit/s",
             v3 ? "v3" : "v1",
             header.number_of_tracks,
             header.number_of_sides,
             hfe_encoding_name(header.track_encoding),
             header.bitrate);
    
    return 0;
}
