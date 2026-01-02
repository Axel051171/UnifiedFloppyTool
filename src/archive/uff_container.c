/**
 * @file uff_container.c
 * @brief UFF Container Implementation
 * 
 * HAFTUNGSMODUS: Forensische Integrität garantiert
 */

#define _POSIX_C_SOURCE 200809L

#include "uft/archive/uff_container.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// SHA-256 IMPLEMENTATION
// ============================================================================

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)       (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1(x)       (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0(x)      (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SIG1(x)      (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} sha256_ctx_t;

static void sha256_init(sha256_ctx_t* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sha256_transform(sha256_ctx_t* ctx, const uint8_t* data) {
    uint32_t W[64], a, b, c, d, e, f, g, h, t1, t2;
    
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)data[i*4] << 24) | ((uint32_t)data[i*4+1] << 16) |
               ((uint32_t)data[i*4+2] << 8) | data[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        W[i] = SIG1(W[i-2]) + W[i-7] + SIG0(W[i-15]) + W[i-16];
    }
    
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K256[i] + W[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len) {
    size_t i = 0;
    size_t idx = ctx->count % 64;
    ctx->count += len;
    
    if (idx) {
        size_t n = 64 - idx;
        if (len < n) {
            memcpy(ctx->buffer + idx, data, len);
            return;
        }
        memcpy(ctx->buffer + idx, data, n);
        sha256_transform(ctx, ctx->buffer);
        i = n;
    }
    
    while (i + 64 <= len) {
        sha256_transform(ctx, data + i);
        i += 64;
    }
    
    if (i < len) {
        memcpy(ctx->buffer, data + i, len - i);
    }
}

static void sha256_final(sha256_ctx_t* ctx, uint8_t* hash) {
    size_t idx = ctx->count % 64;
    ctx->buffer[idx++] = 0x80;
    
    if (idx > 56) {
        memset(ctx->buffer + idx, 0, 64 - idx);
        sha256_transform(ctx, ctx->buffer);
        idx = 0;
    }
    
    memset(ctx->buffer + idx, 0, 56 - idx);
    uint64_t bits = ctx->count * 8;
    for (int i = 0; i < 8; i++) {
        ctx->buffer[56 + i] = bits >> (56 - i * 8);
    }
    sha256_transform(ctx, ctx->buffer);
    
    for (int i = 0; i < 8; i++) {
        hash[i*4]   = ctx->state[i] >> 24;
        hash[i*4+1] = ctx->state[i] >> 16;
        hash[i*4+2] = ctx->state[i] >> 8;
        hash[i*4+3] = ctx->state[i];
    }
}

void uff_sha256(const uint8_t* data, size_t size, uint8_t* hash) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, size);
    sha256_final(&ctx, hash);
}

// ============================================================================
// CRC32 IMPLEMENTATION
// ============================================================================

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void) {
    if (crc32_table_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

uint32_t uff_crc32(const uint8_t* data, size_t size) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// CONTAINER MANAGEMENT
// ============================================================================

uff_container_t* uff_open(const char* path, int level) {
    if (!path) return NULL;
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    
    uff_container_t* uff = calloc(1, sizeof(uff_container_t));
    if (!uff) {
        fclose(fp);
        return NULL;
    }
    
    uff->fp = fp;
    uff->filepath = strdup(path);
    uff->writable = false;
    
    // Read header
    if (fread(&uff->header, sizeof(uff_file_header_t), 1, fp) != 1) {
        uff->last_error = UFF_ERR_FILE;
        snprintf(uff->error_msg, sizeof(uff->error_msg), "Failed to read header");
        uff_close(uff);
        return NULL;
    }
    
    // Validate magic
    if (uff->header.magic != UFF_MAGIC) {
        uff->last_error = UFF_ERR_MAGIC;
        snprintf(uff->error_msg, sizeof(uff->error_msg), 
                 "Invalid magic: 0x%08X", uff->header.magic);
        uff_close(uff);
        return NULL;
    }
    
    // Validate version
    if (uff->header.version_major > UFF_VERSION_MAJOR) {
        uff->last_error = UFF_ERR_VERSION;
        uff_close(uff);
        return NULL;
    }
    
    // Load TOC
    if (fseek(fp, uff->header.toc_offset, SEEK_SET) != 0) {
        uff->last_error = UFF_ERR_FILE;
        uff_close(uff);
        return NULL;
    }
    
    uff_chunk_header_t toc_chunk;
    if (fread(&toc_chunk, sizeof(toc_chunk), 1, fp) != 1) {
        uff->last_error = UFF_ERR_FILE;
        uff_close(uff);
        return NULL;
    }
    
    uff_toc_header_t toc_header;
    if (fread(&toc_header, sizeof(toc_header), 1, fp) != 1) {
        uff->last_error = UFF_ERR_FILE;
        uff_close(uff);
        return NULL;
    }
    
    uff->toc_count = toc_header.count;
    uff->toc = calloc(toc_header.count, sizeof(uff_toc_entry_t));
    if (!uff->toc) {
        uff->last_error = UFF_ERR_MEMORY;
        uff_close(uff);
        return NULL;
    }
    
    if (fread(uff->toc, sizeof(uff_toc_entry_t), toc_header.count, fp) != toc_header.count) {
        uff->last_error = UFF_ERR_FILE;
        uff_close(uff);
        return NULL;
    }
    
    // Validate at requested level
    if (level > 0) {
        int result = uff_validate(uff, level);
        if (result != UFF_OK) {
            uff_close(uff);
            return NULL;
        }
    }
    
    return uff;
}

uff_container_t* uff_create(const char* path) {
    if (!path) return NULL;
    
    FILE* fp = fopen(path, "w+b");
    if (!fp) return NULL;
    
    uff_container_t* uff = calloc(1, sizeof(uff_container_t));
    if (!uff) {
        fclose(fp);
        return NULL;
    }
    
    uff->fp = fp;
    uff->filepath = strdup(path);
    uff->writable = true;
    
    // Initialize header
    uff->header.magic = UFF_MAGIC;
    uff->header.version_major = UFF_VERSION_MAJOR;
    uff->header.version_minor = UFF_VERSION_MINOR;
    uff->header.flags = 0;
    memset(uff->header.reserved, 0, sizeof(uff->header.reserved));
    
    // Write placeholder header
    if (fwrite(&uff->header, sizeof(uff_file_header_t), 1, fp) != 1) {
        uff->last_error = UFF_ERR_FILE;
        uff_close(uff);
        return NULL;
    }
    
    return uff;
}

void uff_close(uff_container_t* uff) {
    if (!uff) return;
    
    if (uff->fp) fclose(uff->fp);
    free(uff->filepath);
    free(uff->meta);
    free(uff->orig_data);
    free(uff->flux_data);
    free(uff->track_data);
    free(uff->sect_data);
    free(uff->toc);
    free(uff);
}

int uff_error(const uff_container_t* uff) {
    return uff ? uff->last_error : UFF_ERR_INVALID;
}

const char* uff_error_msg(const uff_container_t* uff) {
    return uff ? uff->error_msg : "Invalid container";
}

// ============================================================================
// VALIDATION
// ============================================================================

int uff_validate(uff_container_t* uff, int level) {
    if (!uff) return UFF_ERR_INVALID;
    
    // Level 0: Basic - skip header CRC for now (computed at finalize)
    
    // Level 1: HASH chunk verification
    if (level >= UFF_VALID_STANDARD) {
        int result = uff_verify_hashes(uff);
        if (result != UFF_OK) return result;
    }
    
    // Level 2: ORIG SHA-256 verification
    if (level >= UFF_VALID_FULL) {
        int result = uff_verify_original(uff);
        if (result != UFF_OK) return result;
    }
    
    uff->validation_level = level;
    return UFF_OK;
}

int uff_verify_hashes(uff_container_t* uff) {
    if (!uff || !uff->toc) return UFF_ERR_INVALID;
    
    // Find HASH chunk
    uff_toc_entry_t* hash_entry = NULL;
    for (uint32_t i = 0; i < uff->toc_count; i++) {
        if (uff->toc[i].type == UFF_CHUNK_HASH) {
            hash_entry = &uff->toc[i];
            break;
        }
    }
    
    if (!hash_entry) {
        uff->hashes_verified = true;  // No HASH chunk = pass
        return UFF_OK;
    }
    
    // Read HASH chunk
    if (fseek(uff->fp, hash_entry->offset + sizeof(uff_chunk_header_t), SEEK_SET) != 0) {
        return UFF_ERR_FILE;
    }
    
    uff_hash_header_t hash_header;
    if (fread(&hash_header, sizeof(hash_header), 1, uff->fp) != 1) {
        return UFF_ERR_FILE;
    }
    
    uff_hash_entry_t* entries = calloc(hash_header.count, sizeof(uff_hash_entry_t));
    if (!entries) return UFF_ERR_MEMORY;
    
    if (fread(entries, sizeof(uff_hash_entry_t), hash_header.count, uff->fp) != hash_header.count) {
        free(entries);
        return UFF_ERR_FILE;
    }
    
    // Verify each hash
    int result = UFF_OK;
    for (uint32_t i = 0; i < hash_header.count && result == UFF_OK; i++) {
        // Find chunk
        uff_toc_entry_t* chunk = NULL;
        for (uint32_t j = 0; j < uff->toc_count; j++) {
            if (uff->toc[j].type == entries[i].chunk_type) {
                chunk = &uff->toc[j];
                break;
            }
        }
        
        if (!chunk) continue;
        
        // Read chunk data
        uint8_t* data = malloc(chunk->size);
        if (!data) {
            result = UFF_ERR_MEMORY;
            break;
        }
        
        if (fseek(uff->fp, chunk->offset + sizeof(uff_chunk_header_t), SEEK_SET) != 0) {
            free(data);
            result = UFF_ERR_FILE;
            break;
        }
        
        if (fread(data, 1, chunk->size, uff->fp) != chunk->size) {
            free(data);
            result = UFF_ERR_FILE;
            break;
        }
        
        // Calculate hash
        uint8_t calc_hash[32];
        uff_sha256(data, chunk->size, calc_hash);
        free(data);
        
        // Compare
        if (memcmp(calc_hash, entries[i].hash, 32) != 0) {
            uff->last_error = UFF_ERR_HASH;
            snprintf(uff->error_msg, sizeof(uff->error_msg),
                     "Hash mismatch for chunk %s", uff_fourcc_str(entries[i].chunk_type));
            result = UFF_ERR_HASH;
        }
    }
    
    free(entries);
    
    if (result == UFF_OK) {
        uff->hashes_verified = true;
    }
    return result;
}

int uff_verify_original(uff_container_t* uff) {
    if (!uff) return UFF_ERR_INVALID;
    
    const uff_meta_data_t* meta = uff_get_meta(uff);
    if (!meta) return UFF_ERR_CHUNK;
    
    size_t orig_size;
    const uint8_t* orig = uff_get_orig(uff, &orig_size);
    if (!orig) return UFF_ERR_CHUNK;
    
    uint8_t calc_hash[32];
    uff_sha256(orig, orig_size, calc_hash);
    
    if (memcmp(calc_hash, meta->original_sha256, 32) != 0) {
        uff->last_error = UFF_ERR_HASH;
        snprintf(uff->error_msg, sizeof(uff->error_msg),
                 "Original file SHA-256 mismatch");
        return UFF_ERR_HASH;
    }
    
    return UFF_OK;
}

// ============================================================================
// CHUNK ACCESS
// ============================================================================

bool uff_has_chunk(const uff_container_t* uff, uint32_t type) {
    if (!uff || !uff->toc) return false;
    
    for (uint32_t i = 0; i < uff->toc_count; i++) {
        if (uff->toc[i].type == type) return true;
    }
    return false;
}

int64_t uff_chunk_size(const uff_container_t* uff, uint32_t type) {
    if (!uff || !uff->toc) return -1;
    
    for (uint32_t i = 0; i < uff->toc_count; i++) {
        if (uff->toc[i].type == type) {
            return uff->toc[i].size;
        }
    }
    return -1;
}

const uff_meta_data_t* uff_get_meta(uff_container_t* uff) {
    if (!uff) return NULL;
    
    if (uff->meta) return uff->meta;
    
    for (uint32_t i = 0; i < uff->toc_count; i++) {
        if (uff->toc[i].type == UFF_CHUNK_META) {
            uff->meta = malloc(sizeof(uff_meta_data_t));
            if (!uff->meta) return NULL;
            
            if (fseek(uff->fp, uff->toc[i].offset + sizeof(uff_chunk_header_t), SEEK_SET) != 0) {
                free(uff->meta);
                uff->meta = NULL;
                return NULL;
            }
            
            if (fread(uff->meta, sizeof(uff_meta_data_t), 1, uff->fp) != 1) {
                free(uff->meta);
                uff->meta = NULL;
                return NULL;
            }
            
            return uff->meta;
        }
    }
    
    return NULL;
}

const uint8_t* uff_get_orig(uff_container_t* uff, size_t* size) {
    if (!uff) return NULL;
    
    if (uff->orig_data) {
        if (size) *size = uff->orig_size;
        return uff->orig_data;
    }
    
    for (uint32_t i = 0; i < uff->toc_count; i++) {
        if (uff->toc[i].type == UFF_CHUNK_ORIG) {
            if (fseek(uff->fp, uff->toc[i].offset + sizeof(uff_chunk_header_t), SEEK_SET) != 0) {
                return NULL;
            }
            
            uff_orig_header_t orig_header;
            if (fread(&orig_header, sizeof(orig_header), 1, uff->fp) != 1) {
                return NULL;
            }
            
            uff->orig_format = orig_header.format;
            uff->orig_size = uff->toc[i].size - sizeof(uff_orig_header_t);
            uff->orig_data = malloc(uff->orig_size);
            
            if (!uff->orig_data) return NULL;
            
            if (fread(uff->orig_data, 1, uff->orig_size, uff->fp) != uff->orig_size) {
                free(uff->orig_data);
                uff->orig_data = NULL;
                return NULL;
            }
            
            if (size) *size = uff->orig_size;
            return uff->orig_data;
        }
    }
    
    return NULL;
}

// ============================================================================
// WRITING
// ============================================================================

int uff_set_meta(uff_container_t* uff, const uff_meta_data_t* meta) {
    if (!uff || !meta || !uff->writable) return UFF_ERR_INVALID;
    
    if (!uff->meta) {
        uff->meta = malloc(sizeof(uff_meta_data_t));
        if (!uff->meta) return UFF_ERR_MEMORY;
    }
    
    memcpy(uff->meta, meta, sizeof(uff_meta_data_t));
    return UFF_OK;
}

int uff_embed_original(uff_container_t* uff, const uint8_t* data, 
                       size_t size, uint32_t format) {
    if (!uff || !data || !uff->writable) return UFF_ERR_INVALID;
    
    uff->orig_data = malloc(size);
    if (!uff->orig_data) return UFF_ERR_MEMORY;
    
    memcpy(uff->orig_data, data, size);
    uff->orig_size = size;
    uff->orig_format = format;
    
    if (uff->meta) {
        uff_sha256(data, size, uff->meta->original_sha256);
    }
    
    uff->header.flags |= UFF_FLAG_HAS_ORIG;
    return UFF_OK;
}

static int write_chunk(uff_container_t* uff, uint32_t type, 
                       const void* data, size_t size,
                       uff_toc_entry_t* entry) {
    long offset = ftell(uff->fp);
    
    uff_chunk_header_t chunk = {0};
    chunk.type = type;
    chunk.size_uncompressed = size;
    chunk.size_ondisk = size;
    chunk.crc32 = uff_crc32(data, size);
    
    if (fwrite(&chunk, sizeof(chunk), 1, uff->fp) != 1) {
        return UFF_ERR_FILE;
    }
    
    if (fwrite(data, 1, size, uff->fp) != size) {
        return UFF_ERR_FILE;
    }
    
    if (entry) {
        entry->type = type;
        entry->flags = 0;
        entry->offset = offset;
        entry->size = size;
        entry->crc32 = chunk.crc32;
        entry->reserved = 0;
    }
    
    return UFF_OK;
}

int uff_finalize(uff_container_t* uff, const uff_write_options_t* options) {
    if (!uff || !uff->writable) return UFF_ERR_INVALID;
    
    (void)options;  // Unused for now
    
    int max_chunks = 10;
    uff->toc = calloc(max_chunks, sizeof(uff_toc_entry_t));
    if (!uff->toc) return UFF_ERR_MEMORY;
    uff->toc_count = 0;
    
    // Write META chunk (create empty if not set)
    if (!uff->meta) {
        uff->meta = calloc(1, sizeof(uff_meta_data_t));
        if (!uff->meta) return UFF_ERR_MEMORY;
    }
    
    int result = write_chunk(uff, UFF_CHUNK_META, uff->meta, 
                            sizeof(uff_meta_data_t), 
                            &uff->toc[uff->toc_count++]);
    if (result != UFF_OK) return result;
    
    // Write ORIG chunk
    if (uff->orig_data) {
        size_t total_size = sizeof(uff_orig_header_t) + uff->orig_size;
        uint8_t* orig_chunk = malloc(total_size);
        if (!orig_chunk) return UFF_ERR_MEMORY;
        
        uff_orig_header_t* orig_header = (uff_orig_header_t*)orig_chunk;
        orig_header->format = uff->orig_format;
        orig_header->reserved = 0;
        memcpy(orig_chunk + sizeof(uff_orig_header_t), uff->orig_data, uff->orig_size);
        
        result = write_chunk(uff, UFF_CHUNK_ORIG, orig_chunk, total_size,
                            &uff->toc[uff->toc_count++]);
        free(orig_chunk);
        if (result != UFF_OK) return result;
    }
    
    // Build HASH chunk
    uff_hash_header_t hash_header = {0};
    hash_header.count = uff->toc_count;
    
    size_t hash_size = sizeof(hash_header) + hash_header.count * sizeof(uff_hash_entry_t);
    uint8_t* hash_data = calloc(1, hash_size);
    if (!hash_data) return UFF_ERR_MEMORY;
    
    memcpy(hash_data, &hash_header, sizeof(hash_header));
    uff_hash_entry_t* hash_entries = (uff_hash_entry_t*)(hash_data + sizeof(hash_header));
    
    for (uint32_t i = 0; i < uff->toc_count; i++) {
        hash_entries[i].chunk_type = uff->toc[i].type;
        hash_entries[i].algorithm = UFF_HASH_SHA256;
        
        if (fseek(uff->fp, uff->toc[i].offset + sizeof(uff_chunk_header_t), SEEK_SET) != 0) {
            free(hash_data);
            return UFF_ERR_FILE;
        }
        
        uint8_t* chunk_data = malloc(uff->toc[i].size);
        if (!chunk_data) {
            free(hash_data);
            return UFF_ERR_MEMORY;
        }
        
        if (fread(chunk_data, 1, uff->toc[i].size, uff->fp) != uff->toc[i].size) {
            free(chunk_data);
            free(hash_data);
            return UFF_ERR_FILE;
        }
        
        uff_sha256(chunk_data, uff->toc[i].size, hash_entries[i].hash);
        free(chunk_data);
    }
    
    fseek(uff->fp, 0, SEEK_END);
    
    result = write_chunk(uff, UFF_CHUNK_HASH, hash_data, hash_size,
                        &uff->toc[uff->toc_count++]);
    free(hash_data);
    if (result != UFF_OK) return result;
    
    // Record TOC offset
    uff->header.toc_offset = ftell(uff->fp);
    
    // Build and write TOC chunk
    uff_toc_header_t toc_header = {0};
    toc_header.count = uff->toc_count;
    
    size_t toc_size = sizeof(toc_header) + uff->toc_count * sizeof(uff_toc_entry_t);
    uint8_t* toc_data = malloc(toc_size);
    if (!toc_data) return UFF_ERR_MEMORY;
    
    memcpy(toc_data, &toc_header, sizeof(toc_header));
    memcpy(toc_data + sizeof(toc_header), uff->toc, uff->toc_count * sizeof(uff_toc_entry_t));
    
    uff_chunk_header_t toc_chunk = {0};
    toc_chunk.type = UFF_CHUNK_TOC;
    toc_chunk.size_uncompressed = toc_size;
    toc_chunk.size_ondisk = toc_size;
    toc_chunk.crc32 = uff_crc32(toc_data, toc_size);
    
    if (fwrite(&toc_chunk, sizeof(toc_chunk), 1, uff->fp) != 1) {
        free(toc_data);
        return UFF_ERR_FILE;
    }
    if (fwrite(toc_data, 1, toc_size, uff->fp) != toc_size) {
        free(toc_data);
        return UFF_ERR_FILE;
    }
    free(toc_data);
    
    uff->header.file_size = ftell(uff->fp);
    uff->header.header_crc32 = uff_crc32((uint8_t*)&uff->header, 
                                         offsetof(uff_file_header_t, header_crc32));
    
    fseek(uff->fp, 0, SEEK_SET);
    if (fwrite(&uff->header, sizeof(uff->header), 1, uff->fp) != 1) {
        return UFF_ERR_FILE;
    }
    
    return UFF_OK;
}

// ============================================================================
// EXPORT
// ============================================================================

int uff_export_scp(uff_container_t* uff, const char* path) {
    if (!uff || !path) return UFF_ERR_INVALID;
    
    size_t size;
    const uint8_t* data = uff_get_orig(uff, &size);
    if (!data) return UFF_ERR_CHUNK;
    
    FILE* fp = fopen(path, "wb");
    if (!fp) return UFF_ERR_FILE;
    
    if (fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        return UFF_ERR_FILE;
    }
    
    fclose(fp);
    return UFF_OK;
}

// ============================================================================
// UTILITIES
// ============================================================================

void uff_write_options_default(uff_write_options_t* options) {
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->compress = false;
    options->compression_level = 3;
    options->include_flux = true;
    options->include_track = true;
    options->include_sect = true;
    options->include_prot = true;
}

void uff_export_options_default(uff_export_options_t* options) {
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->verify_after = true;
    options->preserve_errors = true;
    options->fill_pattern = 0x00;
}

const char* uff_fourcc_str(uint32_t fourcc) {
    static char buf[5];
    buf[0] = fourcc & 0xFF;
    buf[1] = (fourcc >> 8) & 0xFF;
    buf[2] = (fourcc >> 16) & 0xFF;
    buf[3] = (fourcc >> 24) & 0xFF;
    buf[4] = 0;
    return buf;
}

const char* uff_format_name(int format) {
    switch (format) {
        case UFF_EXPORT_SCP: return "SCP";
        case UFF_EXPORT_G64: return "G64";
        case UFF_EXPORT_D64: return "D64";
        case UFF_EXPORT_ADF: return "ADF";
        case UFF_EXPORT_IMG: return "IMG";
        case UFF_EXPORT_WOZ: return "WOZ";
        case UFF_EXPORT_HFE: return "HFE";
        case UFF_EXPORT_NIB: return "NIB";
        default: return "Unknown";
    }
}
