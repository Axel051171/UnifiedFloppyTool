/**
 * @file uft_cloud.c
 * @brief UFT Cloud Backup Implementation
 * 
 * C-003: Cloud-Backup Integration
 * 
 * Note: Full HTTP implementation requires libcurl.
 * This provides the framework and local testing mode.
 */

#include "uft/cloud/uft_cloud.h"
#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include "uft/compat/uft_dirent.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
/* Windows compatibility for POSIX stat macros */
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#define mkdir(path, mode) _mkdir(path)
#endif

#ifdef UFT_HAVE_CURL
#include <curl/curl.h>
#endif

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

struct uft_cloud_ctx {
    uft_cloud_config_t config;
    
    /* Upload queue */
    uft_upload_file_t *files;
    uint32_t file_count;
    uint32_t file_capacity;
    
    /* Current upload state */
    uft_upload_progress_t progress;
    bool cancel_requested;
    
    /* IA metadata (for current upload) */
    uft_ia_metadata_t ia_metadata;
    
#ifdef UFT_HAVE_CURL
    CURL *curl;
#endif
};

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_cloud_config_init(uft_cloud_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->provider = UFT_CLOUD_INTERNET_ARCHIVE;
    config->encryption = UFT_ENCRYPT_NONE;
    config->verify_upload = true;
    config->use_delta_sync = true;
    config->compress = false;
    config->chunk_size = UFT_CLOUD_CHUNK_SIZE;
    config->max_retries = 3;
    
    strncpy(config->endpoint, UFT_IA_API_URL, UFT_CLOUD_MAX_URL - 1);
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_cloud_ctx_t *uft_cloud_create(const uft_cloud_config_t *config)
{
    if (!config) return NULL;
    
    uft_cloud_ctx_t *ctx = calloc(1, sizeof(uft_cloud_ctx_t));
    if (!ctx) return NULL;
    
    memcpy(&ctx->config, config, sizeof(uft_cloud_config_t));
    
    /* Allocate file queue */
    ctx->file_capacity = 100;
    ctx->files = calloc(ctx->file_capacity, sizeof(uft_upload_file_t));
    if (!ctx->files) {
        free(ctx);
        return NULL;
    }
    
#ifdef UFT_HAVE_CURL
    ctx->curl = curl_easy_init();
#endif
    
    return ctx;
}

void uft_cloud_destroy(uft_cloud_ctx_t *ctx)
{
    if (!ctx) return;
    
#ifdef UFT_HAVE_CURL
    if (ctx->curl) {
        curl_easy_cleanup(ctx->curl);
    }
#endif
    
    free(ctx->files);
    free(ctx);
}

int uft_cloud_test_connection(uft_cloud_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    /* Local backup always works */
    if (ctx->config.provider == UFT_CLOUD_LOCAL_BACKUP) {
        return 0;
    }
    
#ifdef UFT_HAVE_CURL
    /* TODO: Implement actual connection test */
    return 0;
#else
    /* Without curl, only local backup works */
    return (ctx->config.provider == UFT_CLOUD_LOCAL_BACKUP) ? 0 : -1;
#endif
}

/*===========================================================================
 * Internet Archive
 *===========================================================================*/

void uft_ia_metadata_init(uft_ia_metadata_t *metadata)
{
    if (!metadata) return;
    
    memset(metadata, 0, sizeof(*metadata));
    metadata->mediatype = UFT_MEDIA_SOFTWARE;
    strncpy(metadata->language, "eng", 7);
    strncpy(metadata->collection, "opensource_media", 63);
}

int uft_ia_metadata_add(uft_ia_metadata_t *metadata,
                        const char *key, const char *value)
{
    if (!metadata || !key || !value) return -1;
    if (metadata->custom_count >= UFT_CLOUD_MAX_METADATA) return -1;
    
    uft_cloud_metadata_t *field = &metadata->custom[metadata->custom_count++];
    strncpy(field->key, key, sizeof(field->key) - 1);
    strncpy(field->value, value, sizeof(field->value) - 1);
    
    return 0;
}

int uft_ia_generate_identifier(const char *filename, char *identifier, size_t max_len)
{
    if (!filename || !identifier || max_len == 0) return -1;
    
    /* Get base name without path */
    const char *base = strrchr(filename, '/');
    if (!base) base = strrchr(filename, '\\');
    base = base ? base + 1 : filename;
    
    /* Generate identifier: lowercase, alphanumeric + hyphen/underscore */
    size_t j = 0;
    
    /* Add prefix */
    const char *prefix = "uft-preservation-";
    size_t prefix_len = strlen(prefix);
    if (prefix_len < max_len - 1) {
        strncpy(identifier, prefix, 64); identifier[63] = '\0';
        j = prefix_len;
    }
    
    for (size_t i = 0; base[i] && j < max_len - 1; i++) {
        char c = base[i];
        
        if (isalnum(c)) {
            identifier[j++] = tolower(c);
        } else if (c == '-' || c == '_' || c == '.') {
            identifier[j++] = '-';
        }
    }
    
    /* Add timestamp for uniqueness */
    if (j < max_len - 16) {
        time_t now = time(NULL);
        j += snprintf(identifier + j, max_len - j, "-%ld", (long)now);
    }
    
    identifier[j] = '\0';
    
    /* Ensure valid: 3-80 chars, starts with alphanumeric */
    if (j < 3) {
        strncpy(identifier, "uft-item-", 64); identifier[63] = '\0';
        j = 9;
        j += snprintf(identifier + j, max_len - j, "%ld", (long)time(NULL));
    }
    
    return 0;
}

/**
 * @brief Generate IA S3 headers
 */
static void generate_ia_headers(const uft_ia_metadata_t *metadata,
                                char *headers, size_t max_len)
{
    size_t pos = 0;
    
    /* Standard IA headers */
    pos += snprintf(headers + pos, max_len - pos,
        "x-archive-meta-mediatype:%s\n",
        uft_ia_mediatype_string(metadata->mediatype));
    
    if (metadata->title[0]) {
        pos += snprintf(headers + pos, max_len - pos,
            "x-archive-meta-title:%s\n", metadata->title);
    }
    
    if (metadata->description[0]) {
        pos += snprintf(headers + pos, max_len - pos,
            "x-archive-meta-description:%s\n", metadata->description);
    }
    
    if (metadata->creator[0]) {
        pos += snprintf(headers + pos, max_len - pos,
            "x-archive-meta-creator:%s\n", metadata->creator);
    }
    
    if (metadata->date[0]) {
        pos += snprintf(headers + pos, max_len - pos,
            "x-archive-meta-date:%s\n", metadata->date);
    }
    
    if (metadata->subject[0]) {
        pos += snprintf(headers + pos, max_len - pos,
            "x-archive-meta-subject:%s\n", metadata->subject);
    }
    
    if (metadata->collection[0]) {
        pos += snprintf(headers + pos, max_len - pos,
            "x-archive-meta-collection:%s\n", metadata->collection);
    }
    
    /* Custom metadata */
    for (uint8_t i = 0; i < metadata->custom_count && pos < max_len; i++) {
        pos += snprintf(headers + pos, max_len - pos,
            "x-archive-meta-%s:%s\n",
            metadata->custom[i].key, metadata->custom[i].value);
    }
}

int uft_cloud_upload_ia(uft_cloud_ctx_t *ctx,
                        const uft_upload_file_t *files, uint32_t file_count,
                        const uft_ia_metadata_t *metadata)
{
    if (!ctx || !files || file_count == 0 || !metadata) return -1;
    
    /* Store metadata */
    memcpy(&ctx->ia_metadata, metadata, sizeof(uft_ia_metadata_t));
    
    /* Clear and add files to queue */
    uft_cloud_clear_queue(ctx);
    for (uint32_t i = 0; i < file_count; i++) {
        uft_cloud_add_file(ctx, files[i].local_path, files[i].remote_name);
    }
    
    /* Start upload */
    return uft_cloud_upload_start(ctx);
}

bool uft_cloud_ia_item_exists(uft_cloud_ctx_t *ctx, const char *identifier)
{
    if (!ctx || !identifier) return false;
    
#ifdef UFT_HAVE_CURL
    /* TODO: Check via IA API */
#endif
    
    return false;
}

int uft_cloud_ia_get_metadata(uft_cloud_ctx_t *ctx, const char *identifier,
                              uft_ia_metadata_t *metadata)
{
    if (!ctx || !identifier || !metadata) return -1;
    
#ifdef UFT_HAVE_CURL
    /* TODO: Fetch from IA API */
#endif
    
    return -1;
}

/*===========================================================================
 * Generic Upload
 *===========================================================================*/

int uft_cloud_add_file(uft_cloud_ctx_t *ctx, const char *local_path,
                       const char *remote_name)
{
    if (!ctx || !local_path) return -1;
    
    /* Expand queue if needed */
    if (ctx->file_count >= ctx->file_capacity) {
        uint32_t new_capacity = ctx->file_capacity * 2;
        uft_upload_file_t *new_files = realloc(ctx->files,
                                               new_capacity * sizeof(uft_upload_file_t));
        if (!new_files) return -1;
        ctx->files = new_files;
        ctx->file_capacity = new_capacity;
    }
    
    /* Add file */
    uft_upload_file_t *file = &ctx->files[ctx->file_count++];
    memset(file, 0, sizeof(*file));
    
    strncpy(file->local_path, local_path, UFT_CLOUD_MAX_PATH - 1);
    
    if (remote_name) {
        strncpy(file->remote_name, remote_name, sizeof(file->remote_name) - 1);
    } else {
        /* Use filename from path */
        const char *name = strrchr(local_path, '/');
        if (!name) name = strrchr(local_path, '\\');
        name = name ? name + 1 : local_path;
        strncpy(file->remote_name, name, sizeof(file->remote_name) - 1);
    }
    
    /* Get file size */
    struct stat st;
    if (stat(local_path, &st) == 0) {
        file->size = st.st_size;
    }
    
    return 0;
}

int uft_cloud_add_directory(uft_cloud_ctx_t *ctx, const char *local_dir,
                            const char *remote_prefix, bool recursive)
{
    if (!ctx || !local_dir) return -1;
    
    DIR *dir = opendir(local_dir);
    if (!dir) return -1;
    
    int added = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char local_path[UFT_CLOUD_MAX_PATH];
        snprintf(local_path, sizeof(local_path), "%s/%s", local_dir, entry->d_name);
        
        char remote_name[256];
        if (remote_prefix && remote_prefix[0]) {
            snprintf(remote_name, sizeof(remote_name), "%s/%s", 
                     remote_prefix, entry->d_name);
        } else {
            strncpy(remote_name, entry->d_name, sizeof(remote_name) - 1);
        }
        
        struct stat st;
        if (stat(local_path, &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            if (recursive) {
                added += uft_cloud_add_directory(ctx, local_path, remote_name, true);
            }
        } else if (S_ISREG(st.st_mode)) {
            if (uft_cloud_add_file(ctx, local_path, remote_name) == 0) {
                added++;
            }
        }
    }
    
    closedir(dir);
    return added;
}

void uft_cloud_clear_queue(uft_cloud_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->file_count = 0;
    memset(&ctx->progress, 0, sizeof(ctx->progress));
}

/**
 * @brief Upload single file (internal)
 */
static int upload_file_local(uft_cloud_ctx_t *ctx, uft_upload_file_t *file,
                             const char *dest_dir)
{
    char dest_path[UFT_CLOUD_MAX_PATH];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, file->remote_name);
    
    /* Create parent directories */
    char *slash = strrchr(dest_path, '/');
    if (slash) {
        *slash = '\0';
        /* Simple mkdir -p equivalent */
        char temp[UFT_CLOUD_MAX_PATH];
        strncpy(temp, dest_path, sizeof(temp));
        for (char *p = temp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(temp, 0755);
                *p = '/';
            }
        }
        mkdir(temp, 0755);
        *slash = '/';
    }
    
    /* Copy file */
    FILE *src = fopen(file->local_path, "rb");
    if (!src) return -1;
    
    FILE *dst = fopen(dest_path, "wb");
    if (!dst) {
        fclose(src);
        return -1;
    }
    
    uint8_t *buffer = (uint8_t*)malloc(65536); if (!buffer) return UFT_ERR_MEMORY;
    size_t bytes_read;
    uint64_t total = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) { /* I/O error */ }
        total += bytes_read;
        
        /* Update progress */
        ctx->progress.uploaded_bytes += bytes_read;
        
        /* Call progress callback */
        if (ctx->config.progress_cb) {
            strncpy(ctx->progress.current_file, file->remote_name,
                    sizeof(ctx->progress.current_file) - 1);
            ctx->config.progress_cb(&ctx->progress, ctx->config.user_data);
        }
        
        if (ctx->cancel_requested) {
            fclose(src);
            fclose(dst);
            return -2;
        }
    }
    
    fclose(src);
    fclose(dst);
    
    file->uploaded = true;
    return 0;
}

#ifdef UFT_HAVE_CURL
/**
 * @brief Upload to S3/IA (internal)
 */
static int upload_file_s3(uft_cloud_ctx_t *ctx, uft_upload_file_t *file)
{
    /* TODO: Implement S3 upload with curl */
    (void)ctx;
    (void)file;
    return -1;
}
#endif

int uft_cloud_upload_start(uft_cloud_ctx_t *ctx)
{
    if (!ctx || ctx->file_count == 0) return -1;
    
    ctx->cancel_requested = false;
    ctx->progress.status = UFT_UPLOAD_PREPARING;
    ctx->progress.total_files = ctx->file_count;
    ctx->progress.completed_files = 0;
    ctx->progress.uploaded_bytes = 0;
    
    /* Calculate total bytes */
    ctx->progress.total_bytes = 0;
    for (uint32_t i = 0; i < ctx->file_count; i++) {
        ctx->progress.total_bytes += ctx->files[i].size;
    }
    
    ctx->progress.status = UFT_UPLOAD_UPLOADING;
    
    /* Upload based on provider */
    int result = 0;
    
    for (uint32_t i = 0; i < ctx->file_count && !ctx->cancel_requested; i++) {
        uft_upload_file_t *file = &ctx->files[i];
        
        switch (ctx->config.provider) {
            case UFT_CLOUD_LOCAL_BACKUP:
                result = upload_file_local(ctx, file, ctx->config.endpoint);
                break;
                
            case UFT_CLOUD_INTERNET_ARCHIVE:
            case UFT_CLOUD_CUSTOM_S3:
#ifdef UFT_HAVE_CURL
                result = upload_file_s3(ctx, file);
#else
                result = -1;
                strncpy(ctx->progress.error_message, 
                        "curl not available", sizeof(ctx->progress.error_message));
#endif
                break;
        }
        
        if (result == 0) {
            ctx->progress.completed_files++;
        } else {
            ctx->progress.status = UFT_UPLOAD_FAILED;
            ctx->progress.error_code = result;
            return result;
        }
    }
    
    if (ctx->cancel_requested) {
        ctx->progress.status = UFT_UPLOAD_CANCELLED;
        return -2;
    }
    
    ctx->progress.status = UFT_UPLOAD_COMPLETED;
    return 0;
}

int uft_cloud_upload_cancel(uft_cloud_ctx_t *ctx)
{
    if (!ctx) return -1;
    ctx->cancel_requested = true;
    return 0;
}

int uft_cloud_upload_wait(uft_cloud_ctx_t *ctx, uint32_t timeout_ms)
{
    if (!ctx) return -1;
    
    /* For synchronous implementation, upload is already done */
    (void)timeout_ms;
    
    return (ctx->progress.status == UFT_UPLOAD_COMPLETED) ? 0 : -1;
}

void uft_cloud_get_progress(uft_cloud_ctx_t *ctx, uft_upload_progress_t *progress)
{
    if (!ctx || !progress) return;
    memcpy(progress, &ctx->progress, sizeof(uft_upload_progress_t));
}

/*===========================================================================
 * Delta Sync
 *===========================================================================*/

int uft_cloud_load_sync_state(const char *path, uft_sync_state_t *state)
{
    if (!path || !state) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    size_t read = fread(state, sizeof(uft_sync_state_t), 1, f);
    fclose(f);
    
    return (read == 1) ? 0 : -1;
}

int uft_cloud_save_sync_state(const char *path, const uft_sync_state_t *state)
{
    if (!path || !state) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    size_t written = fwrite(state, sizeof(uft_sync_state_t), 1, f);
    fclose(f);
    
    return (written == 1) ? 0 : -1;
}

int uft_cloud_calc_delta(uft_cloud_ctx_t *ctx, const uft_sync_state_t *prev_state,
                         uft_upload_file_t *changed_files, uint32_t *count)
{
    if (!ctx || !changed_files || !count) return -1;
    
    *count = 0;
    
    for (uint32_t i = 0; i < ctx->file_count; i++) {
        uft_upload_file_t *file = &ctx->files[i];
        bool is_new = true;
        
        /* Check if file existed in previous state */
        if (prev_state) {
            for (uint32_t j = 0; j < prev_state->file_count; j++) {
                if (strcmp(file->remote_name, prev_state->files[j].remote_name) == 0) {
                    /* Check if changed (by size for now) */
                    if (file->size != prev_state->files[j].size) {
                        file->changed = true;
                    } else {
                        is_new = false;
                        file->changed = false;
                    }
                    break;
                }
            }
        }
        
        if (is_new || file->changed) {
            memcpy(&changed_files[*count], file, sizeof(uft_upload_file_t));
            (*count)++;
        }
    }
    
    return 0;
}

int uft_cloud_sync_delta(uft_cloud_ctx_t *ctx, const char *local_dir,
                         const char *remote_prefix, uft_sync_state_t *state)
{
    if (!ctx || !local_dir || !state) return -1;
    
    /* Load previous state if exists */
    uft_sync_state_t prev_state;
    char state_path[UFT_CLOUD_MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/.uft_sync_state", local_dir);
    
    bool have_prev = (uft_cloud_load_sync_state(state_path, &prev_state) == 0);
    
    /* Scan directory */
    uft_cloud_clear_queue(ctx);
    uft_cloud_add_directory(ctx, local_dir, remote_prefix, true);
    
    /* Calculate delta */
    uft_upload_file_t *changed = calloc(ctx->file_count, sizeof(uft_upload_file_t));
    if (!changed) return -1;
    
    uint32_t changed_count = 0;
    uft_cloud_calc_delta(ctx, have_prev ? &prev_state : NULL, changed, &changed_count);
    
    /* Upload only changed files */
    if (changed_count > 0) {
        uft_cloud_clear_queue(ctx);
        for (uint32_t i = 0; i < changed_count; i++) {
            uft_cloud_add_file(ctx, changed[i].local_path, changed[i].remote_name);
        }
        
        int result = uft_cloud_upload_start(ctx);
        if (result != 0) {
            free(changed);
            return result;
        }
    }
    
    /* Update state */
    memset(state, 0, sizeof(*state));
    state->last_sync = time(NULL);
    state->file_count = ctx->file_count;
    for (uint32_t i = 0; i < ctx->file_count && i < UFT_CLOUD_MAX_FILES; i++) {
        memcpy(&state->files[i], &ctx->files[i], sizeof(uft_upload_file_t));
    }
    
    /* Save state */
    uft_cloud_save_sync_state(state_path, state);
    
    free(changed);
    return 0;
}

/*===========================================================================
 * Encryption (Placeholder)
 *===========================================================================*/

int uft_cloud_encrypt_file(const char *input_path, const char *output_path,
                           const char *key, uft_encrypt_type_t type)
{
    /* TODO: Implement AES-256-GCM or ChaCha20 encryption */
    /* Would require OpenSSL or similar library */
    (void)input_path;
    (void)output_path;
    (void)key;
    (void)type;
    return -1;
}

int uft_cloud_decrypt_file(const char *input_path, const char *output_path,
                           const char *key, uft_encrypt_type_t type)
{
    (void)input_path;
    (void)output_path;
    (void)key;
    (void)type;
    return -1;
}

int uft_cloud_derive_key(const char *password, uint8_t *key, size_t key_len)
{
    /* TODO: PBKDF2 or Argon2 key derivation */
    (void)password;
    (void)key;
    (void)key_len;
    return -1;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

int uft_cloud_hash_file(const char *path, char *md5, char *sha256)
{
    if (!path) return -1;
    
    /* TODO: Calculate actual hashes */
    /* Would require OpenSSL or similar */
    
    if (md5) { strncpy(md5, "00000000000000000000000000000000", 33); md5[32] = '\0'; }
    if (sha256) { strncpy(sha256, 
        "0000000000000000000000000000000000000000000000000000000000000000", 65); sha256[64] = '\0'; }
    
    return 0;
}

void uft_cloud_format_bytes(uint64_t bytes, char *buffer, size_t size)
{
    if (!buffer || size == 0) return;
    
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double value = (double)bytes;
    
    while (value >= 1024 && unit < 4) {
        value /= 1024;
        unit++;
    }
    
    snprintf(buffer, size, "%.2f %s", value, units[unit]);
}

const char *uft_cloud_provider_name(uft_cloud_provider_t provider)
{
    switch (provider) {
        case UFT_CLOUD_INTERNET_ARCHIVE: return "Internet Archive";
        case UFT_CLOUD_CUSTOM_S3:        return "Custom S3";
        case UFT_CLOUD_LOCAL_BACKUP:     return "Local Backup";
        default:                         return "Unknown";
    }
}

const char *uft_upload_status_name(uft_upload_status_t status)
{
    switch (status) {
        case UFT_UPLOAD_IDLE:       return "Idle";
        case UFT_UPLOAD_PREPARING:  return "Preparing";
        case UFT_UPLOAD_HASHING:    return "Hashing";
        case UFT_UPLOAD_ENCRYPTING: return "Encrypting";
        case UFT_UPLOAD_UPLOADING:  return "Uploading";
        case UFT_UPLOAD_VERIFYING:  return "Verifying";
        case UFT_UPLOAD_COMPLETED:  return "Completed";
        case UFT_UPLOAD_FAILED:     return "Failed";
        case UFT_UPLOAD_CANCELLED:  return "Cancelled";
        default:                    return "Unknown";
    }
}

const char *uft_ia_mediatype_string(uft_ia_mediatype_t type)
{
    switch (type) {
        case UFT_MEDIA_SOFTWARE: return "software";
        case UFT_MEDIA_TEXTS:    return "texts";
        case UFT_MEDIA_DATA:     return "data";
        case UFT_MEDIA_IMAGE:    return "image";
        case UFT_MEDIA_AUDIO:    return "audio";
        default:                 return "data";
    }
}
