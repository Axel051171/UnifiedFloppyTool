/**
 * @file uft_udi.c
 * @brief UDI (Ultra Disk Image) Format Support
 * 
 * UDI is a disk image format developed by Alexander Makeev for ZX Spectrum.
 * It stores raw track bytes including gaps, sync bytes, and sector data.
 * 
 * Format specification: http://speccy.info/UDI
 * 
 * @license MIT (implementation), GPL v3 compatible
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "uft/uft_packed.h"

/*============================================================================
 * UDI FORMAT CONSTANTS
 *============================================================================*/

#define UDI_MAGIC           "UDI!"
#define UDI_HEADER_SIZE     16
#define UDI_VERSION         0x00

/* Track type flags */
#define UDI_TRACK_MFM       0x00
#define UDI_TRACK_FM        0x01

/*============================================================================
 * UDI STRUCTURES
 *============================================================================*/

/**
 * @brief UDI file header
 */
UFT_PACK_BEGIN
typedef struct {
    char     signature[4];      /* "UDI!" */
    uint32_t file_size;         /* Total file size */
    uint8_t  version;           /* Format version (0) */
    uint8_t  max_cylinder;      /* Maximum cylinder number */
    uint8_t  max_side;          /* Maximum side (0 or 1) */
    uint8_t  reserved;          /* Reserved */
    uint32_t ext_header_size;   /* Extended header size (0) */
} uft_udi_header_t;
UFT_PACK_END

/**
 * @brief UDI track header
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  track_type;        /* Track type (0=MFM, 1=FM) */
    uint16_t track_length;      /* Track data length in bytes */
} uft_udi_track_header_t;
UFT_PACK_END

/**
 * @brief UDI file info
 */
typedef struct {
    uint8_t  version;           /**< Format version */
    uint8_t  cylinders;         /**< Number of cylinders */
    uint8_t  sides;             /**< Number of sides (1 or 2) */
    uint32_t file_size;         /**< Total file size */
    bool     valid;             /**< File is valid */
    uint32_t crc_stored;        /**< Stored CRC-32 */
    uint32_t crc_calculated;    /**< Calculated CRC-32 */
    bool     crc_valid;         /**< CRC matches */
} uft_udi_info_t;

/**
 * @brief UDI track data
 */
typedef struct {
    uint8_t  cylinder;          /**< Cylinder number */
    uint8_t  side;              /**< Side (0 or 1) */
    uint8_t  track_type;        /**< Track type */
    uint16_t data_length;       /**< Track data length */
    uint8_t *data;              /**< Track data bytes */
    uint8_t *sync_map;          /**< Sync byte map */
    uint16_t sync_map_length;   /**< Sync map length */
} uft_udi_track_t;

/*============================================================================
 * CRC-32 IMPLEMENTATION
 *============================================================================*/

static uint32_t udi_crc32_table[256];
static bool udi_crc32_init = false;

/**
 * @brief Initialize CRC-32 table
 */
static void uft_udi_crc32_init(void) {
    if (udi_crc32_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        udi_crc32_table[i] = crc;
    }
    udi_crc32_init = true;
}

/**
 * @brief Calculate CRC-32 for single byte (UDI variant)
 */
static inline uint32_t uft_udi_crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= 0xFFFFFFFF ^ byte;
    for (int k = 0; k < 8; k++) {
        uint32_t temp = 0;
        if (crc & 1) temp = 0xEDB88320;
        crc >>= 1;
        crc ^= temp;
    }
    crc ^= 0xFFFFFFFF;
    return crc;
}

/**
 * @brief Calculate CRC-32 for data block
 */
static uint32_t uft_udi_crc32(const uint8_t *data, size_t length) {
    uft_udi_crc32_init();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = uft_udi_crc32_byte(crc, data[i]);
    }
    return crc;
}

/*============================================================================
 * UDI FILE OPERATIONS
 *============================================================================*/

/**
 * @brief Get UDI file information
 * @param path Path to UDI file
 * @param info Output info structure
 * @return 0 on success, -1 on error
 */
int uft_udi_get_info(const char *path, uft_udi_info_t *info) {
    if (!path || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < (long)(UDI_HEADER_SIZE + 4)) {
        fclose(f);
        return -1;
    }
    
    /* Read header */
    uft_udi_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return -1;
    }
    
    /* Check magic */
    if (memcmp(header.signature, UDI_MAGIC, 4) != 0) {
        fclose(f);
        return -1;
    }
    
    info->version = header.version;
    info->cylinders = header.max_cylinder + 1;
    info->sides = header.max_side + 1;
    info->file_size = header.file_size;
    
    /* Read CRC at end of file */
    fseek(f, file_size - 4, SEEK_SET);
    uint8_t crc_bytes[4];
    if (fread(crc_bytes, 1, 4, f) != 4) {
        fclose(f);
        return -1;
    }
    info->crc_stored = crc_bytes[0] | (crc_bytes[1] << 8) | 
                       (crc_bytes[2] << 16) | (crc_bytes[3] << 24);
    
    /* Read entire file for CRC calculation */
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(file_size - 4);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    if (fread(data, 1, file_size - 4, f) != (size_t)(file_size - 4)) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);
    
    /* Calculate CRC */
    info->crc_calculated = uft_udi_crc32(data, file_size - 4);
    info->crc_valid = (info->crc_stored == info->crc_calculated);
    info->valid = info->crc_valid;
    
    free(data);
    return 0;
}

/**
 * @brief Read UDI track
 * @param path Path to UDI file
 * @param cylinder Cylinder number
 * @param side Side (0 or 1)
 * @param track Output track structure
 * @return 0 on success, -1 on error
 */
int uft_udi_read_track(const char *path, uint8_t cylinder, uint8_t side,
                        uft_udi_track_t *track) {
    if (!path || !track) return -1;
    
    memset(track, 0, sizeof(*track));
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Read header */
    uft_udi_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return -1;
    }
    
    if (memcmp(header.signature, UDI_MAGIC, 4) != 0) {
        fclose(f);
        return -1;
    }
    
    /* Skip to requested track */
    int target_index = cylinder * (header.max_side + 1) + side;
    int current_index = 0;
    
    while (current_index < target_index) {
        uft_udi_track_header_t trk_hdr;
        if (fread(&trk_hdr, 1, sizeof(trk_hdr), f) != sizeof(trk_hdr)) {
            fclose(f);
            return -1;
        }
        
        /* Calculate sync map size */
        uint16_t sync_map_size = (trk_hdr.track_length + 7) / 8;
        
        /* Skip track data and sync map */
        fseek(f, trk_hdr.track_length + sync_map_size, SEEK_CUR);
        current_index++;
    }
    
    /* Read target track */
    uft_udi_track_header_t trk_hdr;
    if (fread(&trk_hdr, 1, sizeof(trk_hdr), f) != sizeof(trk_hdr)) {
        fclose(f);
        return -1;
    }
    
    track->cylinder = cylinder;
    track->side = side;
    track->track_type = trk_hdr.track_type;
    track->data_length = trk_hdr.track_length;
    
    /* Allocate and read track data */
    track->data = malloc(trk_hdr.track_length);
    if (!track->data) {
        fclose(f);
        return -1;
    }
    
    if (fread(track->data, 1, trk_hdr.track_length, f) != trk_hdr.track_length) {
        free(track->data);
        track->data = NULL;
        fclose(f);
        return -1;
    }
    
    /* Read sync map */
    track->sync_map_length = (trk_hdr.track_length + 7) / 8;
    track->sync_map = malloc(track->sync_map_length);
    if (!track->sync_map) {
        free(track->data);
        track->data = NULL;
        fclose(f);
        return -1;
    }
    
    if (fread(track->sync_map, 1, track->sync_map_length, f) != track->sync_map_length) {
        free(track->data);
        free(track->sync_map);
        track->data = NULL;
        track->sync_map = NULL;
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

/**
 * @brief Free UDI track resources
 * @param track Track to free
 */
void uft_udi_free_track(uft_udi_track_t *track) {
    if (!track) return;
    
    if (track->data) {
        free(track->data);
        track->data = NULL;
    }
    if (track->sync_map) {
        free(track->sync_map);
        track->sync_map = NULL;
    }
}

/**
 * @brief Check if byte is sync byte
 * @param track Track data
 * @param byte_index Byte index
 * @return true if sync byte
 */
bool uft_udi_is_sync_byte(const uft_udi_track_t *track, uint16_t byte_index) {
    if (!track || !track->sync_map || byte_index >= track->data_length) {
        return false;
    }
    
    return (track->sync_map[byte_index / 8] & (1 << (byte_index % 8))) != 0;
}

/*============================================================================
 * UDI WRITER
 *============================================================================*/

/**
 * @brief UDI writer context
 */
typedef struct {
    FILE *file;
    uint8_t max_cylinder;
    uint8_t max_side;
    uint32_t data_size;
    uint32_t crc;
} uft_udi_writer_t;

/**
 * @brief Create UDI writer
 * @param path Output file path
 * @return Writer context or NULL on error
 */
uft_udi_writer_t *uft_udi_writer_create(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    
    uft_udi_writer_t *writer = calloc(1, sizeof(uft_udi_writer_t));
    if (!writer) {
        fclose(f);
        return NULL;
    }
    
    writer->file = f;
    writer->crc = 0xFFFFFFFF;
    
    /* Write placeholder header */
    uft_udi_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.signature, UDI_MAGIC, 4);
    
    fwrite(&header, 1, sizeof(header), f);
    writer->data_size = sizeof(header);
    
    /* Update CRC */
    uint8_t *hdr_bytes = (uint8_t *)&header;
    for (size_t i = 0; i < sizeof(header); i++) {
        writer->crc = uft_udi_crc32_byte(writer->crc, hdr_bytes[i]);
    }
    
    return writer;
}

/**
 * @brief Add track to UDI file
 * @param writer Writer context
 * @param track Track data
 * @return 0 on success, -1 on error
 */
int uft_udi_writer_add_track(uft_udi_writer_t *writer, 
                              const uft_udi_track_t *track) {
    if (!writer || !writer->file || !track || !track->data) {
        return -1;
    }
    
    /* Update max cylinder/side */
    if (track->cylinder > writer->max_cylinder) {
        writer->max_cylinder = track->cylinder;
    }
    if (track->side > writer->max_side) {
        writer->max_side = track->side;
    }
    
    /* Write track header */
    uft_udi_track_header_t trk_hdr;
    trk_hdr.track_type = track->track_type;
    trk_hdr.track_length = track->data_length;
    
    fwrite(&trk_hdr, 1, sizeof(trk_hdr), writer->file);
    writer->data_size += sizeof(trk_hdr);
    
    /* Update CRC for header */
    uint8_t *hdr_bytes = (uint8_t *)&trk_hdr;
    for (size_t i = 0; i < sizeof(trk_hdr); i++) {
        writer->crc = uft_udi_crc32_byte(writer->crc, hdr_bytes[i]);
    }
    
    /* Write track data */
    fwrite(track->data, 1, track->data_length, writer->file);
    writer->data_size += track->data_length;
    
    /* Update CRC for data */
    for (uint16_t i = 0; i < track->data_length; i++) {
        writer->crc = uft_udi_crc32_byte(writer->crc, track->data[i]);
    }
    
    /* Write or generate sync map */
    uint16_t sync_map_length = (track->data_length + 7) / 8;
    if (track->sync_map) {
        fwrite(track->sync_map, 1, sync_map_length, writer->file);
        for (uint16_t i = 0; i < sync_map_length; i++) {
            writer->crc = uft_udi_crc32_byte(writer->crc, track->sync_map[i]);
        }
    } else {
        /* Write empty sync map */
        for (uint16_t i = 0; i < sync_map_length; i++) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, writer->file);
            writer->crc = uft_udi_crc32_byte(writer->crc, zero);
        }
    }
    writer->data_size += sync_map_length;
    
    return 0;
}

/**
 * @brief Finalize and close UDI writer
 * @param writer Writer context (freed on return)
 * @return 0 on success, -1 on error
 */
int uft_udi_writer_close(uft_udi_writer_t *writer) {
    if (!writer) return -1;
    
    if (writer->file) {
        /* Write CRC */
        uint8_t crc_bytes[4];
        crc_bytes[0] = writer->crc & 0xFF;
        crc_bytes[1] = (writer->crc >> 8) & 0xFF;
        crc_bytes[2] = (writer->crc >> 16) & 0xFF;
        crc_bytes[3] = (writer->crc >> 24) & 0xFF;
        fwrite(crc_bytes, 1, 4, writer->file);
        
        /* Update header */
        fseek(writer->file, 0, SEEK_SET);
        
        uft_udi_header_t header;
        memset(&header, 0, sizeof(header));
        memcpy(header.signature, UDI_MAGIC, 4);
        header.file_size = writer->data_size;
        header.version = UDI_VERSION;
        header.max_cylinder = writer->max_cylinder;
        header.max_side = writer->max_side;
        
        fwrite(&header, 1, sizeof(header), writer->file);
        
        fclose(writer->file);
    }
    
    free(writer);
    return 0;
}

/*============================================================================
 * UDI FORMAT PROBE
 *============================================================================*/

/**
 * @brief Probe if file is UDI format
 * @param path File path
 * @return Confidence (0-100), 0 if not UDI
 */
int uft_udi_probe(const char *path) {
    if (!path) return 0;
    
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    
    char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        fclose(f);
        return 0;
    }
    fclose(f);
    
    if (memcmp(magic, UDI_MAGIC, 4) == 0) {
        return 95;  /* High confidence */
    }
    
    return 0;
}

/**
 * @brief Verify UDI file integrity
 * @param path File path
 * @return 0 if valid, -1 if invalid
 */
int uft_udi_verify(const char *path) {
    uft_udi_info_t info;
    
    if (uft_udi_get_info(path, &info) != 0) {
        return -1;
    }
    
    return info.crc_valid ? 0 : -1;
}
