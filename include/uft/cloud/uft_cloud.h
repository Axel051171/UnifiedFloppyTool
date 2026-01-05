/**
 * @file uft_cloud.h
 * @brief UFT Cloud Backup Integration
 * 
 * C-003: Cloud-Backup Integration
 * 
 * Features:
 * - Internet Archive upload
 * - Archive.org metadata format
 * - Encryption option
 * - Delta-sync for updates
 * - Progress tracking
 */

#ifndef UFT_CLOUD_H
#define UFT_CLOUD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum values */
#define UFT_CLOUD_MAX_PATH          1024
#define UFT_CLOUD_MAX_URL           512
#define UFT_CLOUD_MAX_METADATA      32
#define UFT_CLOUD_MAX_FILES         1000
#define UFT_CLOUD_MAX_KEY           256

/** Chunk sizes */
#define UFT_CLOUD_CHUNK_SIZE        (5 * 1024 * 1024)   /**< 5MB chunks */
#define UFT_CLOUD_MIN_DELTA         1024                 /**< Min size for delta */

/** Internet Archive constants */
#define UFT_IA_API_URL              "https://s3.us.archive.org"
#define UFT_IA_METADATA_URL         "https://archive.org/metadata"

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Cloud provider types
 */
typedef enum {
    UFT_CLOUD_INTERNET_ARCHIVE = 0, /**< archive.org */
    UFT_CLOUD_CUSTOM_S3,            /**< S3-compatible */
    UFT_CLOUD_LOCAL_BACKUP          /**< Local folder (testing) */
} uft_cloud_provider_t;

/**
 * @brief Upload status
 */
typedef enum {
    UFT_UPLOAD_IDLE = 0,
    UFT_UPLOAD_PREPARING,       /**< Preparing files */
    UFT_UPLOAD_HASHING,         /**< Calculating hashes */
    UFT_UPLOAD_ENCRYPTING,      /**< Encrypting data */
    UFT_UPLOAD_UPLOADING,       /**< Uploading */
    UFT_UPLOAD_VERIFYING,       /**< Verifying upload */
    UFT_UPLOAD_COMPLETED,       /**< Successfully completed */
    UFT_UPLOAD_FAILED,          /**< Upload failed */
    UFT_UPLOAD_CANCELLED        /**< Cancelled by user */
} uft_upload_status_t;

/**
 * @brief Encryption type
 */
typedef enum {
    UFT_ENCRYPT_NONE = 0,
    UFT_ENCRYPT_AES256_GCM,     /**< AES-256-GCM */
    UFT_ENCRYPT_CHACHA20        /**< ChaCha20-Poly1305 */
} uft_encrypt_type_t;

/**
 * @brief Media type for Internet Archive
 */
typedef enum {
    UFT_MEDIA_SOFTWARE = 0,
    UFT_MEDIA_TEXTS,
    UFT_MEDIA_DATA,
    UFT_MEDIA_IMAGE,
    UFT_MEDIA_AUDIO
} uft_ia_mediatype_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Metadata field
 */
typedef struct {
    char key[64];
    char value[256];
} uft_cloud_metadata_t;

/**
 * @brief Internet Archive item metadata
 */
typedef struct {
    char identifier[128];           /**< Unique item identifier */
    char title[256];                /**< Item title */
    char description[1024];         /**< Item description */
    char creator[128];              /**< Creator/uploader */
    char date[32];                  /**< Date (YYYY-MM-DD) */
    char subject[256];              /**< Subject tags (comma-separated) */
    char collection[64];            /**< Collection name */
    uft_ia_mediatype_t mediatype;   /**< Media type */
    char language[8];               /**< Language code */
    char licenseurl[256];           /**< License URL */
    
    /* Custom metadata */
    uft_cloud_metadata_t custom[UFT_CLOUD_MAX_METADATA];
    uint8_t custom_count;
} uft_ia_metadata_t;

/**
 * @brief Upload file entry
 */
typedef struct {
    char local_path[UFT_CLOUD_MAX_PATH];    /**< Local file path */
    char remote_name[256];                   /**< Remote filename */
    uint64_t size;                           /**< File size */
    char md5[33];                            /**< MD5 hash */
    char sha256[65];                         /**< SHA256 hash */
    bool uploaded;                           /**< Upload complete */
    bool changed;                            /**< Changed since last sync */
} uft_upload_file_t;

/**
 * @brief Upload progress
 */
typedef struct {
    uft_upload_status_t status;
    
    /* File progress */
    uint32_t total_files;
    uint32_t completed_files;
    char current_file[256];
    
    /* Byte progress */
    uint64_t total_bytes;
    uint64_t uploaded_bytes;
    
    /* Rate */
    double bytes_per_second;
    double estimated_remaining;
    
    /* Error info */
    int error_code;
    char error_message[256];
} uft_upload_progress_t;

/**
 * @brief Progress callback
 */
typedef void (*uft_cloud_progress_cb)(const uft_upload_progress_t *progress,
                                      void *user_data);

/**
 * @brief Cloud configuration
 */
typedef struct {
    uft_cloud_provider_t provider;
    
    /* Credentials */
    char access_key[UFT_CLOUD_MAX_KEY];     /**< API access key */
    char secret_key[UFT_CLOUD_MAX_KEY];     /**< API secret key */
    char endpoint[UFT_CLOUD_MAX_URL];       /**< Custom endpoint URL */
    
    /* Encryption */
    uft_encrypt_type_t encryption;
    char encrypt_key[UFT_CLOUD_MAX_KEY];    /**< Encryption key/password */
    
    /* Options */
    bool verify_upload;                     /**< Verify after upload */
    bool use_delta_sync;                    /**< Enable delta sync */
    bool compress;                          /**< Compress before upload */
    uint32_t chunk_size;                    /**< Upload chunk size */
    uint8_t max_retries;                    /**< Max retry count */
    
    /* Callbacks */
    uft_cloud_progress_cb progress_cb;
    void *user_data;
} uft_cloud_config_t;

/**
 * @brief Sync state for delta uploads
 */
typedef struct {
    char item_id[128];                      /**< Item identifier */
    time_t last_sync;                       /**< Last sync time */
    
    uft_upload_file_t files[UFT_CLOUD_MAX_FILES];
    uint32_t file_count;
    
    char state_hash[65];                    /**< State checksum */
} uft_sync_state_t;

/**
 * @brief Cloud context (opaque)
 */
typedef struct uft_cloud_ctx uft_cloud_ctx_t;

/*===========================================================================
 * Function Prototypes - Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize cloud configuration with defaults
 */
void uft_cloud_config_init(uft_cloud_config_t *config);

/**
 * @brief Create cloud context
 */
uft_cloud_ctx_t *uft_cloud_create(const uft_cloud_config_t *config);

/**
 * @brief Destroy cloud context
 */
void uft_cloud_destroy(uft_cloud_ctx_t *ctx);

/**
 * @brief Test connection/credentials
 */
int uft_cloud_test_connection(uft_cloud_ctx_t *ctx);

/*===========================================================================
 * Function Prototypes - Internet Archive
 *===========================================================================*/

/**
 * @brief Initialize IA metadata with defaults
 */
void uft_ia_metadata_init(uft_ia_metadata_t *metadata);

/**
 * @brief Add custom metadata field
 */
int uft_ia_metadata_add(uft_ia_metadata_t *metadata, 
                        const char *key, const char *value);

/**
 * @brief Generate IA identifier from filename
 * 
 * Creates a valid archive.org identifier following naming rules
 */
int uft_ia_generate_identifier(const char *filename, char *identifier, size_t max_len);

/**
 * @brief Upload to Internet Archive
 * 
 * @param ctx Cloud context
 * @param files Array of files to upload
 * @param file_count Number of files
 * @param metadata Item metadata
 * @return 0 on success
 */
int uft_cloud_upload_ia(uft_cloud_ctx_t *ctx,
                        const uft_upload_file_t *files, uint32_t file_count,
                        const uft_ia_metadata_t *metadata);

/**
 * @brief Check if IA item exists
 */
bool uft_cloud_ia_item_exists(uft_cloud_ctx_t *ctx, const char *identifier);

/**
 * @brief Get IA item metadata
 */
int uft_cloud_ia_get_metadata(uft_cloud_ctx_t *ctx, const char *identifier,
                              uft_ia_metadata_t *metadata);

/*===========================================================================
 * Function Prototypes - Generic Upload
 *===========================================================================*/

/**
 * @brief Add file to upload queue
 */
int uft_cloud_add_file(uft_cloud_ctx_t *ctx, const char *local_path,
                       const char *remote_name);

/**
 * @brief Add directory to upload queue
 */
int uft_cloud_add_directory(uft_cloud_ctx_t *ctx, const char *local_dir,
                            const char *remote_prefix, bool recursive);

/**
 * @brief Clear upload queue
 */
void uft_cloud_clear_queue(uft_cloud_ctx_t *ctx);

/**
 * @brief Start upload
 */
int uft_cloud_upload_start(uft_cloud_ctx_t *ctx);

/**
 * @brief Cancel upload
 */
int uft_cloud_upload_cancel(uft_cloud_ctx_t *ctx);

/**
 * @brief Wait for upload completion
 */
int uft_cloud_upload_wait(uft_cloud_ctx_t *ctx, uint32_t timeout_ms);

/**
 * @brief Get upload progress
 */
void uft_cloud_get_progress(uft_cloud_ctx_t *ctx, uft_upload_progress_t *progress);

/*===========================================================================
 * Function Prototypes - Delta Sync
 *===========================================================================*/

/**
 * @brief Load sync state from file
 */
int uft_cloud_load_sync_state(const char *path, uft_sync_state_t *state);

/**
 * @brief Save sync state to file
 */
int uft_cloud_save_sync_state(const char *path, const uft_sync_state_t *state);

/**
 * @brief Calculate delta (changed files)
 */
int uft_cloud_calc_delta(uft_cloud_ctx_t *ctx, const uft_sync_state_t *prev_state,
                         uft_upload_file_t *changed_files, uint32_t *count);

/**
 * @brief Sync with delta (upload only changes)
 */
int uft_cloud_sync_delta(uft_cloud_ctx_t *ctx, const char *local_dir,
                         const char *remote_prefix, uft_sync_state_t *state);

/*===========================================================================
 * Function Prototypes - Encryption
 *===========================================================================*/

/**
 * @brief Encrypt file for upload
 * 
 * @param input_path Input file path
 * @param output_path Output encrypted file path
 * @param key Encryption key
 * @param type Encryption algorithm
 * @return 0 on success
 */
int uft_cloud_encrypt_file(const char *input_path, const char *output_path,
                           const char *key, uft_encrypt_type_t type);

/**
 * @brief Decrypt downloaded file
 */
int uft_cloud_decrypt_file(const char *input_path, const char *output_path,
                           const char *key, uft_encrypt_type_t type);

/**
 * @brief Derive encryption key from password
 */
int uft_cloud_derive_key(const char *password, uint8_t *key, size_t key_len);

/*===========================================================================
 * Function Prototypes - Utilities
 *===========================================================================*/

/**
 * @brief Calculate file hash
 */
int uft_cloud_hash_file(const char *path, char *md5, char *sha256);

/**
 * @brief Format bytes as human-readable string
 */
void uft_cloud_format_bytes(uint64_t bytes, char *buffer, size_t size);

/**
 * @brief Get provider name
 */
const char *uft_cloud_provider_name(uft_cloud_provider_t provider);

/**
 * @brief Get status name
 */
const char *uft_upload_status_name(uft_upload_status_t status);

/**
 * @brief Get media type string for IA
 */
const char *uft_ia_mediatype_string(uft_ia_mediatype_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CLOUD_H */
