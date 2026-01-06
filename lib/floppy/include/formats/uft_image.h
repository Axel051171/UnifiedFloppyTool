/**
 * @file uft_image.h
 * @brief Disk image format support
 * 
 * Supports reading and writing various disk image formats:
 * 
 * Commodore:
 *   - D64 (1541 disk image)
 *   - G64 (1541 GCR image with timing)
 *   - D71 (1571 double-sided)
 *   - D81 (1581 3.5")
 *   - NIB (raw nibble data)
 * 
 * Amiga:
 *   - ADF (Amiga Disk File)
 *   - ADZ (gzipped ADF)
 *   - DMS (compressed)
 *   - FDI (raw track)
 * 
 * Atari ST:
 *   - ST (raw sector)
 *   - STX (PASTI format with protection)
 * 
 * Apple II:
 *   - DO (DOS order)
 *   - PO (ProDOS order)
 *   - NIB (raw nibble)
 *   - WOZ (flux-level)
 * 
 * IBM PC:
 *   - IMG/IMA (raw sector)
 *   - IMD (ImageDisk with metadata)
 *   - TD0 (TeleDisk)
 * 
 * Flux formats:
 *   - SCP (SuperCard Pro)
 *   - KF (KryoFlux stream)
 *   - HFE (HxC Floppy Emulator)
 */

#ifndef UFT_IMAGE_H
#define UFT_IMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Platform compatibility for ssize_t */
#ifdef _MSC_VER
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
#else
    #include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Image Format Types
 * ============================================================================ */

typedef enum {
    UFT_IMG_UNKNOWN = 0,
    
    /* Commodore */
    UFT_IMG_D64,        /**< C64 1541 disk */
    UFT_IMG_G64,        /**< C64 GCR with timing */
    UFT_IMG_D71,        /**< C128 1571 double-sided */
    UFT_IMG_D81,        /**< C128 1581 3.5" */
    UFT_IMG_D80,        /**< CBM 8050 */
    UFT_IMG_D82,        /**< CBM 8250 */
    UFT_IMG_NIB_C64,    /**< C64 raw nibble */
    
    /* Amiga */
    UFT_IMG_ADF,        /**< Amiga DD/HD */
    UFT_IMG_ADZ,        /**< Compressed ADF */
    UFT_IMG_DMS,        /**< Amiga DiskMasher */
    UFT_IMG_FDI,        /**< Amiga FDI */
    
    /* Atari ST */
    UFT_IMG_ST,         /**< Atari ST raw */
    UFT_IMG_MSA,        /**< Atari MSA compressed */
    UFT_IMG_STX,        /**< PASTI with protection */
    
    /* Apple */
    UFT_IMG_DO,         /**< Apple DOS order */
    UFT_IMG_PO,         /**< Apple ProDOS order */
    UFT_IMG_NIB_APPLE,  /**< Apple raw nibble */
    UFT_IMG_WOZ,        /**< Apple WOZ flux */
    
    /* IBM PC */
    UFT_IMG_IMG,        /**< Raw sector image */
    UFT_IMG_IMA,        /**< Raw sector (alternate) */
    UFT_IMG_IMD,        /**< ImageDisk */
    UFT_IMG_TD0,        /**< TeleDisk */
    UFT_IMG_DSK,        /**< Generic DSK */
    UFT_IMG_FLP,        /**< Raw floppy */
    
    /* Flux formats */
    UFT_IMG_SCP,        /**< SuperCard Pro */
    UFT_IMG_KF,         /**< KryoFlux raw */
    UFT_IMG_HFE,        /**< HxC Floppy Emulator */
    UFT_IMG_MFM,        /**< Raw MFM stream */
    UFT_IMG_FLUX,       /**< Generic flux */
    
    UFT_IMG_COUNT
} uft_image_type_t;

/**
 * @brief Image capability flags
 */
typedef enum {
    UFT_IMG_CAP_READ        = 0x0001,  /**< Can read data */
    UFT_IMG_CAP_WRITE       = 0x0002,  /**< Can write data */
    UFT_IMG_CAP_FORMAT      = 0x0004,  /**< Can format tracks */
    UFT_IMG_CAP_TIMING      = 0x0008,  /**< Has timing information */
    UFT_IMG_CAP_FLUX        = 0x0010,  /**< Has flux-level data */
    UFT_IMG_CAP_WEAK_BITS   = 0x0020,  /**< Can store weak bits */
    UFT_IMG_CAP_PROTECTION  = 0x0040,  /**< Can store protection info */
    UFT_IMG_CAP_METADATA    = 0x0080,  /**< Has metadata support */
} uft_image_caps_t;

/* ============================================================================
 * Image Structure
 * ============================================================================ */

/**
 * @brief Disk geometry
 */
typedef struct {
    uint8_t cylinders;      /**< Number of cylinders/tracks */
    uint8_t heads;          /**< Number of heads/sides */
    uint8_t sectors;        /**< Sectors per track (0 = variable) */
    uint16_t sector_size;   /**< Bytes per sector */
    uint32_t total_size;    /**< Total image size */
    bool double_step;       /**< Double-step required */
} uft_image_geometry_t;

/**
 * @brief Track data
 */
typedef struct {
    uint8_t *data;          /**< Track data */
    size_t data_len;        /**< Data length */
    uint32_t *flux;         /**< Flux timing data (optional) */
    size_t flux_len;        /**< Number of flux samples */
    uint8_t *weak_mask;     /**< Weak bit mask (optional) */
    uint8_t encoding;       /**< Encoding (0=MFM, 1=FM, 2=GCR) */
    uint32_t bit_rate;      /**< Bit rate in bps */
    bool formatted;         /**< Track is formatted */
} uft_track_t;

/**
 * @brief Disk image handle
 */
typedef struct uft_image uft_image_t;

/* ============================================================================
 * Image Operations
 * ============================================================================ */

/**
 * @brief Open a disk image file
 * 
 * @param filename  Path to image file
 * @param mode      "r" for read, "w" for write, "rw" for both
 * @return Image handle, or NULL on error
 */
uft_image_t* uft_image_open(const char *filename, const char *mode);

/**
 * @brief Create a new disk image
 * 
 * @param filename  Path for new image
 * @param type      Image format type
 * @param geometry  Disk geometry
 * @return Image handle, or NULL on error
 */
uft_image_t* uft_image_create(const char *filename, uft_image_type_t type,
                               const uft_image_geometry_t *geometry);

/**
 * @brief Close a disk image
 * 
 * @param image Image to close
 */
void uft_image_close(uft_image_t *image);

/**
 * @brief Get image type
 * 
 * @param image Image handle
 * @return Image type
 */
uft_image_type_t uft_image_get_type(const uft_image_t *image);

/**
 * @brief Get image geometry
 * 
 * @param image Image handle
 * @param geometry Output geometry structure
 * @return true on success
 */
bool uft_image_get_geometry(const uft_image_t *image, uft_image_geometry_t *geometry);

/**
 * @brief Get image capabilities
 * 
 * @param image Image handle
 * @return Capability flags
 */
uint32_t uft_image_get_caps(const uft_image_t *image);

/* ============================================================================
 * Track Operations
 * ============================================================================ */

/**
 * @brief Read a track from the image
 * 
 * @param image     Image handle
 * @param track     Track/cylinder number
 * @param head      Head/side number
 * @param data      Output track structure
 * @return true on success
 */
bool uft_image_read_track(uft_image_t *image, uint8_t track, uint8_t head,
                           uft_track_t *data);

/**
 * @brief Write a track to the image
 * 
 * @param image     Image handle
 * @param track     Track/cylinder number
 * @param head      Head/side number
 * @param data      Track data to write
 * @return true on success
 */
bool uft_image_write_track(uft_image_t *image, uint8_t track, uint8_t head,
                            const uft_track_t *data);

/**
 * @brief Free track data
 * 
 * @param track Track to free
 */
void uft_track_free(uft_track_t *track);

/* ============================================================================
 * Sector Operations
 * ============================================================================ */

/**
 * @brief Read a sector from the image
 * 
 * @param image     Image handle
 * @param track     Track/cylinder number
 * @param head      Head/side number
 * @param sector    Sector number
 * @param data      Output buffer
 * @param data_len  Buffer size
 * @return Bytes read, or -1 on error
 */
ssize_t uft_image_read_sector(uft_image_t *image, uint8_t track, uint8_t head,
                               uint8_t sector, uint8_t *data, size_t data_len);

/**
 * @brief Write a sector to the image
 * 
 * @param image     Image handle
 * @param track     Track/cylinder number
 * @param head      Head/side number
 * @param sector    Sector number
 * @param data      Data to write
 * @param data_len  Data length
 * @return Bytes written, or -1 on error
 */
ssize_t uft_image_write_sector(uft_image_t *image, uint8_t track, uint8_t head,
                                uint8_t sector, const uint8_t *data, size_t data_len);

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * @brief Detect image format from file
 * 
 * @param filename  Path to image file
 * @return Detected format type
 */
uft_image_type_t uft_image_detect_format(const char *filename);

/**
 * @brief Detect format from magic bytes
 * 
 * @param data      File header data
 * @param len       Length of data
 * @return Detected format type
 */
uft_image_type_t uft_image_detect_magic(const uint8_t *data, size_t len);

/**
 * @brief Get format name
 * 
 * @param type Image format type
 * @return Format name string
 */
const char* uft_image_type_name(uft_image_type_t type);

/**
 * @brief Get file extension for format
 * 
 * @param type Image format type
 * @return File extension (without dot)
 */
const char* uft_image_type_extension(uft_image_type_t type);

/* ============================================================================
 * Format Conversion
 * ============================================================================ */

/**
 * @brief Convert between image formats
 * 
 * @param src           Source image
 * @param dest_filename Destination filename
 * @param dest_type     Destination format
 * @return true on success
 */
bool uft_image_convert(uft_image_t *src, const char *dest_filename,
                        uft_image_type_t dest_type);

/* ============================================================================
 * D64 Specific Functions
 * ============================================================================ */

/**
 * @brief D64 directory entry
 */
typedef struct {
    uint8_t file_type;      /**< File type (PRG, SEQ, etc.) */
    uint8_t start_track;    /**< First track of file */
    uint8_t start_sector;   /**< First sector of file */
    char name[17];          /**< Filename (null-terminated) */
    uint16_t blocks;        /**< Size in blocks */
} uft_d64_dir_entry_t;

/**
 * @brief Read D64 directory
 * 
 * @param image     D64 image handle
 * @param entries   Output array for entries
 * @param max_entries Maximum entries to return
 * @return Number of entries found
 */
size_t uft_d64_read_directory(uft_image_t *image, uft_d64_dir_entry_t *entries,
                               size_t max_entries);

/**
 * @brief Read file from D64
 * 
 * @param image     D64 image handle
 * @param name      Filename
 * @param data      Output buffer
 * @param max_len   Maximum length
 * @return Bytes read, or -1 on error
 */
ssize_t uft_d64_read_file(uft_image_t *image, const char *name,
                           uint8_t *data, size_t max_len);

/* ============================================================================
 * ADF Specific Functions
 * ============================================================================ */

/**
 * @brief ADF boot block info
 */
typedef struct {
    char disk_name[32];     /**< Volume name */
    uint32_t root_block;    /**< Root directory block */
    uint32_t bitmap_block;  /**< Bitmap block */
    bool is_ffs;            /**< Fast File System */
    bool is_intl;           /**< International mode */
    bool is_dircache;       /**< Directory cache */
} uft_adf_info_t;

/**
 * @brief Read ADF volume info
 * 
 * @param image     ADF image handle
 * @param info      Output info structure
 * @return true on success
 */
bool uft_adf_read_info(uft_image_t *image, uft_adf_info_t *info);

/* ============================================================================
 * G64 Specific Functions
 * ============================================================================ */

/**
 * @brief G64 track entry
 */
typedef struct {
    uint16_t size;          /**< Track data size */
    uint32_t speed_zone;    /**< Speed zone (0-3) */
    uint8_t *raw_data;      /**< Raw GCR data */
} uft_g64_track_t;

/**
 * @brief Read G64 track with timing
 * 
 * @param image     G64 image handle
 * @param track     Track number (0-83)
 * @param data      Output track structure
 * @return true on success
 */
bool uft_g64_read_track(uft_image_t *image, uint8_t track, uft_g64_track_t *data);

/* ============================================================================
 * SCP (SuperCard Pro) Functions
 * ============================================================================ */

/**
 * @brief SCP file header
 */
typedef struct {
    uint8_t version;        /**< SCP version */
    uint8_t disk_type;      /**< Disk type code */
    uint8_t revolutions;    /**< Revolutions captured */
    uint8_t start_track;    /**< First track */
    uint8_t end_track;      /**< Last track */
    uint8_t flags;          /**< Flags */
    uint8_t bit_cell_width; /**< Bit cell encoding */
    uint8_t heads;          /**< Number of heads */
    uint32_t checksum;      /**< File checksum */
} uft_scp_header_t;

/**
 * @brief Read SCP header
 * 
 * @param image     SCP image handle
 * @param header    Output header structure
 * @return true on success
 */
bool uft_scp_read_header(uft_image_t *image, uft_scp_header_t *header);

/**
 * @brief Read SCP track flux data
 * 
 * @param image     SCP image handle
 * @param track     Track number
 * @param head      Head number
 * @param rev       Revolution number
 * @param flux      Output flux array
 * @param max_flux  Maximum flux samples
 * @return Number of flux samples, or -1 on error
 */
ssize_t uft_scp_read_flux(uft_image_t *image, uint8_t track, uint8_t head,
                           uint8_t rev, uint32_t *flux, size_t max_flux);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMAGE_H */
