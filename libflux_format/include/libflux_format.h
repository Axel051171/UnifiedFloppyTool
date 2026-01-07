// SPDX-License-Identifier: MIT
/*
 * libflux_format.h - HXC Format Support
 * 
 * Professional implementation of HXC Floppy Emulator formats.
 * Supports 100+ floppy disk formats through HFE container.
 * 
 * Supported Formats:
 *   - HFE (UFT HFE Format native format)
 *   - MFM encoding/decoding (universal)
 *   - IBM MFM sectors
 *   - Amiga MFM
 *   - FM encoding
 * 
 * @version 2.7.5
 * @date 2024-12-25
 */

#ifndef HXC_FORMAT_H
#define HXC_FORMAT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * ERROR CODES
 *============================================================================*/

typedef enum {
    HXC_OK           =  0,   /**< Success */
    HXC_ERR_INVALID  = -1,   /**< Invalid parameter */
    HXC_ERR_NOMEM    = -2,   /**< Out of memory */
    HXC_ERR_FORMAT   = -3,   /**< Invalid format */
    HXC_ERR_UNSUPPORTED = -4 /**< Unsupported format */
} libflux_error_t;

/*============================================================================*
 * TRACK ENCODING TYPES
 *============================================================================*/

typedef enum {
    HXC_ENC_ISOIBM_MFM   = 0x00,  /**< ISO/IBM MFM encoding */
    HXC_ENC_AMIGA_MFM    = 0x01,  /**< Amiga MFM encoding */
    HXC_ENC_ISOIBM_FM    = 0x02,  /**< ISO/IBM FM encoding */
    HXC_ENC_EMU_FM       = 0x03,  /**< Emulator FM encoding */
    HXC_ENC_UNKNOWN      = 0xFF   /**< Unknown encoding */
} libflux_track_encoding_t;

/*============================================================================*
 * HFE FORMAT STRUCTURES
 *============================================================================*/

/**
 * @brief Track data
 */
typedef struct {
    uint8_t *data;          /**< Track data */
    size_t size;            /**< Data size in bytes */
    uint16_t bitrate;       /**< Bitrate in Kbps */
    uint8_t encoding;       /**< Track encoding */
} libflux_track_t;

/**
 * @brief HFE disk image
 */
typedef struct {
    uint8_t format_revision;    /**< Format revision */
    uint8_t number_of_tracks;   /**< Number of tracks */
    uint8_t number_of_sides;    /**< Number of sides (1 or 2) */
    uint8_t track_encoding;     /**< Default track encoding */
    uint16_t bitrate_kbps;      /**< Bitrate in Kbps */
    uint16_t rpm;               /**< RPM (300 or 360) */
    uint8_t write_protected;    /**< Write protection flag */
    
    libflux_track_t *tracks;        /**< Track array */
    uint32_t track_count;       /**< Number of tracks */
} libflux_hfe_image_t;

/*============================================================================*
 * MFM SECTOR STRUCTURES
 *============================================================================*/

/**
 * @brief Decoded sector
 */
typedef struct {
    uint8_t cylinder;       /**< Cylinder/track number */
    uint8_t head;           /**< Head/side number */
    uint8_t sector;         /**< Sector number */
    uint8_t size_code;      /**< Size code (0=128, 1=256, 2=512, 3=1024) */
    uint16_t data_size;     /**< Actual data size */
    uint8_t *data;          /**< Sector data */
    bool crc_valid;         /**< CRC check result */
} libflux_sector_t;

/**
 * @brief Disk with decoded sectors
 */
typedef struct {
    libflux_sector_t *sectors;  /**< Sector array */
    uint32_t sector_count;  /**< Number of sectors */
    uint8_t cylinders;      /**< Number of cylinders */
    uint8_t heads;          /**< Number of heads */
    uint8_t sectors_per_track; /**< Sectors per track */
} libflux_disk_t;

/*============================================================================*
 * HFE FORMAT API
 *============================================================================*/

/**
 * @brief Parse HFE file from memory
 * 
 * @param file File buffer
 * @param file_len File size
 * @param hfe_out Parsed HFE image (output)
 * @return 0 on success, negative on error
 */
int libflux_parse_hfe(
    const uint8_t *file,
    size_t file_len,
    libflux_hfe_image_t *hfe_out
);

/**
 * @brief Free HFE image
 * 
 * @param hfe HFE image
 */
void libflux_free_hfe(libflux_hfe_image_t *hfe);

/**
 * @brief Load HFE from file
 * 
 * @param path File path
 * @param hfe_out Parsed HFE (output)
 * @return 0 on success, negative on error
 */
int libflux_load_hfe_file(
    const char *path,
    libflux_hfe_image_t *hfe_out
);

/**
 * @brief Print HFE information
 * 
 * @param hfe HFE image
 */
void libflux_hfe_print_info(const libflux_hfe_image_t *hfe);

/*============================================================================*
 * MFM ENCODING/DECODING API
 *============================================================================*/

/**
 * @brief Decode MFM bitstream to bytes
 * 
 * @param mfm_bits MFM-encoded bitstream
 * @param mfm_bit_count Number of MFM bits
 * @param bytes_out Decoded bytes (allocated by this function)
 * @param byte_count_out Number of decoded bytes
 * @return 0 on success, negative on error
 */
int libflux_decode_mfm(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    uint8_t **bytes_out,
    size_t *byte_count_out
);

/**
 * @brief Encode bytes to MFM bitstream
 * 
 * @param bytes Input bytes
 * @param byte_count Number of bytes
 * @param mfm_bits_out MFM bitstream (allocated by this function)
 * @param mfm_bit_count_out Number of MFM bits
 * @return 0 on success, negative on error
 */
int libflux_encode_mfm(
    const uint8_t *bytes,
    size_t byte_count,
    uint8_t **mfm_bits_out,
    size_t *mfm_bit_count_out
);

/*============================================================================*
 * IBM MFM SECTOR API
 *============================================================================*/

/**
 * @brief Find IBM MFM sector marker
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Bit count
 * @param start_bit Start position
 * @return Bit position of marker, or -1 if not found
 */
int libflux_find_ibm_sector_marker(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t start_bit
);

/**
 * @brief Decode IBM MFM sector
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Bit count
 * @param marker_pos Position of sector marker
 * @param sector_out Decoded sector (output)
 * @return 0 on success, negative on error
 */
int libflux_decode_ibm_sector(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t marker_pos,
    libflux_sector_t *sector_out
);

/**
 * @brief Free sector data
 * 
 * @param sector Sector to free
 */
void libflux_free_sector(libflux_sector_t *sector);

/**
 * @brief Free disk data
 * 
 * @param disk Disk to free
 */
void libflux_free_disk(libflux_disk_t *disk);

/*============================================================================*
 * COMPLETE MFM SECTOR DECODER
 *============================================================================*/

/**
 * @brief Decode complete IBM MFM sector with CRC verification
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param start_bit Start search position
 * @param sector_out Decoded sector (output)
 * @return Bit position after sector, or -1 on error
 */
int libflux_decode_ibm_sector_complete(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t start_bit,
    libflux_sector_t *sector_out
);

/**
 * @brief Scan entire track for IBM MFM sectors
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param disk_out Decoded disk (output)
 * @return 0 on success
 */
int libflux_scan_track_sectors(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    libflux_disk_t *disk_out
);

/*============================================================================*
 * HXC USB HARDWARE SUPPORT
 *============================================================================*/

/* Forward declaration */
typedef struct libflux_device_handle_t libflux_device_handle_t;
typedef struct libflux_device_info_t libflux_device_info_t;

/**
 * @brief Initialize HxC USB device
 * 
 * @param handle_out Device handle (output)
 * @return 0 on success
 */
int libflux_usb_init(libflux_device_handle_t **handle_out);

/**
 * @brief Close HxC USB device
 * 
 * @param handle Device handle
 */
void libflux_usb_close(libflux_device_handle_t *handle);

/**
 * @brief Get device information
 * 
 * @param handle Device handle
 * @param info_out Device info (output)
 * @return 0 on success
 */
int libflux_usb_get_info(
    libflux_device_handle_t *handle,
    libflux_device_info_t *info_out
);

/**
 * @brief Read track from floppy
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param track_data_out Track data (allocated by function)
 * @param track_len_out Track length
 * @return 0 on success
 */
int libflux_usb_read_track(
    libflux_device_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    uint8_t **track_data_out,
    size_t *track_len_out
);

/**
 * @brief Write track to floppy
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param track_data Track data
 * @param track_len Track length
 * @return 0 on success
 */
int libflux_usb_write_track(
    libflux_device_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *track_data,
    size_t track_len
);

/**
 * @brief Control drive motor
 * 
 * @param handle Device handle
 * @param on true = motor on, false = motor off
 * @return 0 on success
 */
int libflux_usb_motor(libflux_device_handle_t *handle, bool on);

/**
 * @brief Detect floppy emulators
 * 
 * @param device_list_out Device list (allocated by function)
 * @param count_out Number of devices
 * @return 0 on success
 */
int libflux_usb_detect_devices(char ***device_list_out, int *count_out);

/*============================================================================*
 * AMIGA MFM SUPPORT
 *============================================================================*/

/* Amiga sector */
typedef struct {
    uint8_t track;
    uint8_t sector;
    uint8_t data[512];
    bool valid;
} amiga_sector_t;

/**
 * @brief Decode Amiga sector
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param start_bit Start position
 * @param sector_out Decoded sector (output)
 * @return Bit position after sector, or -1 on error
 */
int libflux_decode_amiga_sector(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    size_t start_bit,
    amiga_sector_t *sector_out
);

/**
 * @brief Scan Amiga track for sectors
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Total MFM bits
 * @param sectors_out Sector array (allocated)
 * @param count_out Number of sectors
 * @return 0 on success
 */
int libflux_scan_amiga_track(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    amiga_sector_t **sectors_out,
    uint32_t *count_out
);

/**
 * @brief Detect Amiga disk format
 * 
 * @param sector_count Sectors per track
 * @return Format name
 */
const char* libflux_amiga_detect_format(uint32_t sector_count);

/*============================================================================*
 * UTILITIES
 *============================================================================*/

/**
 * @brief Detect HXC format from file signature
 * 
 * @param file File buffer (at least 8 bytes)
 * @return Format name ("HFE", "MFM_HFE", or "Unknown")
 */
const char* libflux_detect_format(const uint8_t *file);

/**
 * @brief Get encoding name
 * 
 * @param encoding Encoding code
 * @return Encoding name string
 */
const char* libflux_get_encoding_name(uint8_t encoding);

#ifdef __cplusplus
}
#endif

#endif /* HXC_FORMAT_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
