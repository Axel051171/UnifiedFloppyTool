/**
 * @file uft_ocr.h
 * @brief UFT Disk Label OCR System
 * 
 * C-005: Automatic text recognition on disk labels
 * 
 * Features:
 * - OCR engine abstraction (Tesseract, etc.)
 * - Disk label image preprocessing
 * - Metadata extraction (title, publisher, year)
 * - Manual correction workflow
 * - Multiple language support
 */

#ifndef UFT_OCR_H
#define UFT_OCR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum values */
#define UFT_OCR_MAX_TEXT        4096    /**< Maximum OCR text length */
#define UFT_OCR_MAX_TITLE       256     /**< Maximum title length */
#define UFT_OCR_MAX_FIELDS      32      /**< Maximum metadata fields */
#define UFT_OCR_MAX_LANGUAGES   8       /**< Maximum language count */

/** Image limits */
#define UFT_OCR_MIN_DPI         150     /**< Minimum recommended DPI */
#define UFT_OCR_OPTIMAL_DPI     300     /**< Optimal DPI for OCR */
#define UFT_OCR_MAX_IMAGE_SIZE  (50*1024*1024)  /**< 50MB max */

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief OCR engine types
 */
typedef enum {
    UFT_OCR_ENGINE_AUTO = 0,    /**< Auto-select best available */
    UFT_OCR_ENGINE_TESSERACT,   /**< Tesseract OCR */
    UFT_OCR_ENGINE_CUNEIFORM,   /**< Cuneiform OCR */
    UFT_OCR_ENGINE_GOCR,        /**< GOCR */
    UFT_OCR_ENGINE_CUSTOM       /**< Custom engine via callback */
} uft_ocr_engine_t;

/**
 * @brief Image format
 */
typedef enum {
    UFT_IMG_UNKNOWN = 0,
    UFT_IMG_PNG,
    UFT_IMG_JPEG,
    UFT_IMG_TIFF,
    UFT_IMG_BMP,
    UFT_IMG_RAW                 /**< Raw pixel data */
} uft_img_format_t;

/**
 * @brief Preprocessing steps
 */
typedef enum {
    UFT_PREPROC_NONE        = 0,
    UFT_PREPROC_DESKEW      = (1 << 0),  /**< Correct skew angle */
    UFT_PREPROC_DENOISE     = (1 << 1),  /**< Remove noise */
    UFT_PREPROC_BINARIZE    = (1 << 2),  /**< Convert to black/white */
    UFT_PREPROC_CONTRAST    = (1 << 3),  /**< Enhance contrast */
    UFT_PREPROC_SHARPEN     = (1 << 4),  /**< Sharpen edges */
    UFT_PREPROC_INVERT      = (1 << 5),  /**< Invert colors */
    UFT_PREPROC_ROTATE      = (1 << 6),  /**< Auto-rotate */
    UFT_PREPROC_CROP        = (1 << 7),  /**< Auto-crop to content */
    UFT_PREPROC_ALL         = 0xFF       /**< All preprocessing */
} uft_preproc_flags_t;

/**
 * @brief Confidence level
 */
typedef enum {
    UFT_OCR_CONF_UNKNOWN = 0,
    UFT_OCR_CONF_LOW,           /**< < 60% */
    UFT_OCR_CONF_MEDIUM,        /**< 60-80% */
    UFT_OCR_CONF_HIGH,          /**< 80-95% */
    UFT_OCR_CONF_VERIFIED       /**< > 95% or manually verified */
} uft_ocr_conf_level_t;

/**
 * @brief Label type
 */
typedef enum {
    UFT_LABEL_UNKNOWN = 0,
    UFT_LABEL_FLOPPY_525,       /**< 5.25" floppy label */
    UFT_LABEL_FLOPPY_35,        /**< 3.5" floppy label */
    UFT_LABEL_SLEEVE,           /**< Paper sleeve/envelope */
    UFT_LABEL_MANUAL,           /**< Manual page */
    UFT_LABEL_BOX,              /**< Box/case cover */
    UFT_LABEL_DISK_SCAN         /**< Direct disk surface scan */
} uft_label_type_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Image data structure
 */
typedef struct {
    uint8_t        *data;           /**< Pixel data (or encoded data) */
    size_t          size;           /**< Data size in bytes */
    uft_img_format_t format;        /**< Image format */
    
    uint32_t        width;          /**< Image width */
    uint32_t        height;         /**< Image height */
    uint8_t         channels;       /**< Color channels (1=gray, 3=RGB, 4=RGBA) */
    uint16_t        dpi;            /**< Resolution (DPI) */
} uft_image_t;

/**
 * @brief OCR word with position and confidence
 */
typedef struct {
    char        text[64];           /**< Word text */
    float       confidence;         /**< Recognition confidence (0-1) */
    
    /* Bounding box */
    uint32_t    x, y;               /**< Top-left corner */
    uint32_t    width, height;      /**< Dimensions */
    
    /* Attributes */
    bool        is_bold;            /**< Bold text */
    bool        is_italic;          /**< Italic text */
    uint8_t     font_size;          /**< Estimated font size */
} uft_ocr_word_t;

/**
 * @brief OCR line
 */
typedef struct {
    char           *text;           /**< Full line text */
    float           confidence;     /**< Line confidence */
    
    uft_ocr_word_t *words;          /**< Words in line */
    uint16_t        word_count;     /**< Number of words */
    
    uint32_t        y;              /**< Line Y position */
    uint32_t        height;         /**< Line height */
} uft_ocr_line_t;

/**
 * @brief Metadata field
 */
typedef struct {
    char    name[32];               /**< Field name (e.g., "title") */
    char    value[256];             /**< Field value */
    float   confidence;             /**< Extraction confidence */
    bool    verified;               /**< Manually verified */
} uft_ocr_field_t;

/**
 * @brief Disk label metadata
 */
typedef struct {
    /* Basic info */
    char            title[UFT_OCR_MAX_TITLE];       /**< Software title */
    char            publisher[128];                  /**< Publisher/company */
    char            year[16];                        /**< Release year */
    char            version[32];                     /**< Version number */
    char            serial[64];                      /**< Serial/catalog number */
    
    /* Additional fields */
    char            platform[32];                    /**< Platform (e.g., "C64") */
    char            media_type[32];                  /**< Media type */
    char            disk_number[16];                 /**< Disk X of Y */
    char            side[8];                         /**< Side A/B */
    
    /* Copy protection */
    char            protection[64];                  /**< Protection scheme */
    
    /* Generic fields */
    uft_ocr_field_t fields[UFT_OCR_MAX_FIELDS];     /**< Additional fields */
    uint8_t         field_count;                     /**< Number of fields */
    
    /* Confidence */
    float           overall_confidence;              /**< Overall extraction confidence */
    uft_ocr_conf_level_t conf_level;                /**< Confidence category */
} uft_label_metadata_t;

/**
 * @brief Full OCR result
 */
typedef struct {
    /* Raw text */
    char           *raw_text;                       /**< Full OCR text */
    size_t          raw_text_len;                   /**< Text length */
    
    /* Structured data */
    uft_ocr_line_t *lines;                          /**< Recognized lines */
    uint16_t        line_count;                     /**< Number of lines */
    
    /* Extracted metadata */
    uft_label_metadata_t metadata;                  /**< Extracted metadata */
    
    /* Statistics */
    float           mean_confidence;                /**< Mean word confidence */
    uint32_t        total_words;                    /**< Total words recognized */
    uint32_t        low_conf_words;                 /**< Low confidence words */
    
    /* Image info */
    uft_label_type_t label_type;                    /**< Detected label type */
    float           skew_angle;                     /**< Detected skew (degrees) */
    
    /* Processing time */
    double          preproc_time_ms;                /**< Preprocessing time */
    double          ocr_time_ms;                    /**< OCR engine time */
    double          extract_time_ms;                /**< Metadata extraction time */
} uft_ocr_result_t;

/**
 * @brief OCR configuration
 */
typedef struct {
    uft_ocr_engine_t engine;                        /**< OCR engine to use */
    
    /* Languages */
    char            languages[UFT_OCR_MAX_LANGUAGES][8];  /**< Language codes */
    uint8_t         language_count;                 /**< Number of languages */
    
    /* Preprocessing */
    uint32_t        preproc_flags;                  /**< Preprocessing flags */
    uint16_t        target_dpi;                     /**< Target DPI for scaling */
    
    /* Recognition */
    uint8_t         page_seg_mode;                  /**< Page segmentation mode */
    bool            detect_orientation;             /**< Auto-detect orientation */
    bool            preserve_interword_spaces;      /**< Preserve spaces */
    
    /* Metadata extraction */
    bool            extract_metadata;               /**< Extract structured data */
    uft_label_type_t expected_type;                 /**< Expected label type */
    
    /* Output */
    bool            include_positions;              /**< Include word positions */
    bool            include_confidence;             /**< Include confidence values */
    
    /* Custom engine callback */
    int (*custom_engine)(const uft_image_t *img, char *text, size_t max_len);
} uft_ocr_config_t;

/**
 * @brief OCR context (opaque)
 */
typedef struct uft_ocr_ctx uft_ocr_ctx_t;

/*===========================================================================
 * Function Prototypes - Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize OCR configuration with defaults
 */
void uft_ocr_config_init(uft_ocr_config_t *config);

/**
 * @brief Create OCR context
 * 
 * @param config Configuration
 * @return OCR context or NULL on error
 */
uft_ocr_ctx_t *uft_ocr_create(const uft_ocr_config_t *config);

/**
 * @brief Destroy OCR context
 */
void uft_ocr_destroy(uft_ocr_ctx_t *ctx);

/**
 * @brief Check if OCR engine is available
 */
bool uft_ocr_engine_available(uft_ocr_engine_t engine);

/**
 * @brief Get available engines (bitmask)
 */
uint32_t uft_ocr_get_available_engines(void);

/*===========================================================================
 * Function Prototypes - Image Handling
 *===========================================================================*/

/**
 * @brief Load image from file
 * 
 * @param path Image file path
 * @param image Output image structure
 * @return 0 on success
 */
int uft_ocr_load_image(const char *path, uft_image_t *image);

/**
 * @brief Load image from memory
 */
int uft_ocr_load_image_mem(const uint8_t *data, size_t size,
                           uft_img_format_t format, uft_image_t *image);

/**
 * @brief Free image data
 */
void uft_ocr_free_image(uft_image_t *image);

/**
 * @brief Preprocess image for OCR
 * 
 * @param image Input/output image
 * @param flags Preprocessing flags
 * @return 0 on success
 */
int uft_ocr_preprocess(uft_image_t *image, uint32_t flags);

/**
 * @brief Detect skew angle
 */
float uft_ocr_detect_skew(const uft_image_t *image);

/**
 * @brief Rotate image
 */
int uft_ocr_rotate(uft_image_t *image, float angle);

/**
 * @brief Detect label type from image
 */
uft_label_type_t uft_ocr_detect_label_type(const uft_image_t *image);

/*===========================================================================
 * Function Prototypes - OCR
 *===========================================================================*/

/**
 * @brief Perform OCR on image
 * 
 * @param ctx OCR context
 * @param image Input image
 * @param result Output result (caller must free with uft_ocr_free_result)
 * @return 0 on success
 */
int uft_ocr_recognize(uft_ocr_ctx_t *ctx, const uft_image_t *image,
                      uft_ocr_result_t **result);

/**
 * @brief Perform OCR on file
 */
int uft_ocr_recognize_file(uft_ocr_ctx_t *ctx, const char *path,
                           uft_ocr_result_t **result);

/**
 * @brief Free OCR result
 */
void uft_ocr_free_result(uft_ocr_result_t *result);

/**
 * @brief Get raw text only (simplified)
 */
int uft_ocr_get_text(uft_ocr_ctx_t *ctx, const uft_image_t *image,
                     char *text, size_t max_len);

/*===========================================================================
 * Function Prototypes - Metadata Extraction
 *===========================================================================*/

/**
 * @brief Extract metadata from OCR text
 * 
 * @param text OCR text
 * @param label_type Label type hint
 * @param metadata Output metadata
 * @return 0 on success
 */
int uft_ocr_extract_metadata(const char *text, uft_label_type_t label_type,
                             uft_label_metadata_t *metadata);

/**
 * @brief Extract title from text
 */
int uft_ocr_extract_title(const char *text, char *title, size_t max_len);

/**
 * @brief Extract year from text
 */
int uft_ocr_extract_year(const char *text, char *year, size_t max_len);

/**
 * @brief Extract publisher from text
 */
int uft_ocr_extract_publisher(const char *text, char *publisher, size_t max_len);

/*===========================================================================
 * Function Prototypes - Correction
 *===========================================================================*/

/**
 * @brief Set manual correction for field
 * 
 * @param result OCR result
 * @param field_name Field name to correct
 * @param value Corrected value
 * @return 0 on success
 */
int uft_ocr_correct_field(uft_ocr_result_t *result, const char *field_name,
                          const char *value);

/**
 * @brief Mark field as verified
 */
int uft_ocr_verify_field(uft_ocr_result_t *result, const char *field_name);

/**
 * @brief Apply spell correction to text
 */
int uft_ocr_spell_correct(char *text, size_t max_len, const char *language);

/*===========================================================================
 * Function Prototypes - Export
 *===========================================================================*/

/**
 * @brief Export metadata to JSON
 */
int uft_ocr_export_json(const uft_ocr_result_t *result, const char *path);

/**
 * @brief Export metadata to NFO format
 */
int uft_ocr_export_nfo(const uft_label_metadata_t *metadata, const char *path);

/**
 * @brief Export to hOCR format (searchable PDF)
 */
int uft_ocr_export_hocr(const uft_ocr_result_t *result, const char *path);

/**
 * @brief Generate searchable PDF
 */
int uft_ocr_export_pdf(const uft_image_t *image, const uft_ocr_result_t *result,
                       const char *path);

/*===========================================================================
 * Function Prototypes - Utilities
 *===========================================================================*/

/**
 * @brief Get engine name
 */
const char *uft_ocr_engine_name(uft_ocr_engine_t engine);

/**
 * @brief Get format name
 */
const char *uft_img_format_name(uft_img_format_t format);

/**
 * @brief Get label type name
 */
const char *uft_label_type_name(uft_label_type_t type);

/**
 * @brief Get confidence level name
 */
const char *uft_ocr_conf_name(uft_ocr_conf_level_t level);

/**
 * @brief Calculate confidence level from value
 */
uft_ocr_conf_level_t uft_ocr_conf_from_value(float confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OCR_H */
