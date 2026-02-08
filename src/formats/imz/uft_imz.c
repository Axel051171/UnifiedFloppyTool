/**
 * @file uft_imz.c
 * @brief IMZ Format Implementation (WinImage Compressed)
 * 
 * IMZ = ZIP-compressed IMG file
 * Uses miniz for ZIP handling.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_imz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Use zlib if available, otherwise simple deflate */
#ifdef HAVE_ZLIB
#include <zlib.h>
#define USE_ZLIB 1
#else
#define USE_ZLIB 0
#endif

/*===========================================================================
 * ZIP STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/* Local file header */
typedef struct {
    uint32_t signature;         /* 0x04034b50 */
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;       /* 0=store, 8=deflate */
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
} zip_local_header_t;

/* Central directory header */
typedef struct {
    uint32_t signature;         /* 0x02014b50 */
    uint16_t version_made;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
    uint16_t comment_len;
    uint16_t disk_start;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t local_offset;
} zip_central_header_t;

/* End of central directory */
typedef struct {
    uint32_t signature;         /* 0x06054b50 */
    uint16_t disk_number;
    uint16_t cd_disk;
    uint16_t disk_entries;
    uint16_t total_entries;
    uint32_t cd_size;
    uint32_t cd_offset;
    uint16_t comment_len;
} zip_end_header_t;

#pragma pack(pop)

#define ZIP_LOCAL_SIG   0x04034b50
#define ZIP_CENTRAL_SIG 0x02014b50
#define ZIP_END_SIG     0x06054b50

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

struct uft_imz_context {
    char *path;
    uint8_t *data;              /* Decompressed data */
    size_t size;                /* Decompressed size */
    size_t compressed_size;     /* Compressed file size */
    bool modified;
};

/*===========================================================================
 * CRC32 
 *===========================================================================*/

static uint32_t crc32_table[256];
static int crc32_inited = 0;

static void init_crc32(void) {
    if (crc32_inited) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_inited = 1;
}

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
    init_crc32();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/*===========================================================================
 * SIMPLE INFLATE/DEFLATE (stored only if no zlib)
 *===========================================================================*/

#if !USE_ZLIB

/* Without zlib, we only support stored (uncompressed) ZIP */
static int simple_inflate(const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t out_len,
                          int method) {
    if (method == 0) {  /* Stored */
        if (in_len > out_len) return -1;
        memcpy(out, in, in_len);
        return 0;
    }
    /* Deflate not supported without zlib */
    return -1;
}

static int simple_deflate(const uint8_t *in, size_t in_len,
                          uint8_t **out, size_t *out_len) {
    /* Store uncompressed */
    *out = (uint8_t *)malloc(in_len);
    if (!*out) return -1;
    memcpy(*out, in, in_len);
    *out_len = in_len;
    return 0;
}

#endif

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

bool uft_imz_probe(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint32_t sig;
    bool valid = (fread(&sig, 4, 1, f) == 1 && sig == ZIP_LOCAL_SIG);
    fclose(f);
    
    return valid;
}

uft_imz_t* uft_imz_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read local header */
    zip_local_header_t local;
    if (fread(&local, sizeof(local), 1, f) != 1) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    if (local.signature != ZIP_LOCAL_SIG) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    /* Skip filename and extra */
    fseek(f, local.filename_len + local.extra_len, SEEK_CUR);
    
    /* Read compressed data */
    uint8_t *compressed = (uint8_t *)malloc(local.compressed_size);
    if (!compressed) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    if (fread(compressed, 1, local.compressed_size, f) != local.compressed_size) {
        free(compressed);
        if (ferror(f)) {
            fclose(f);
            return UFT_ERR_IO;
        }
                fclose(f);
        return NULL;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);
    
    /* Allocate context */
    uft_imz_t *imz = (uft_imz_t *)calloc(1, sizeof(uft_imz_t));
    if (!imz) {
        free(compressed);
        return NULL;
    }
    
    imz->path = strdup(path);
    imz->compressed_size = file_size;
    imz->size = local.uncompressed_size;
    imz->data = (uint8_t *)malloc(local.uncompressed_size);
    
    if (!imz->data) {
        free(compressed);
        free(imz->path);
        free(imz);
        return NULL;
    }
    
    /* Decompress */
    int rc = -1;
    
#if USE_ZLIB
    if (local.compression == 8) {  /* Deflate */
        z_stream strm = {0};
        strm.next_in = compressed;
        strm.avail_in = local.compressed_size;
        strm.next_out = imz->data;
        strm.avail_out = local.uncompressed_size;
        
        if (inflateInit2(&strm, -MAX_WBITS) == Z_OK) {
            if (inflate(&strm, Z_FINISH) == Z_STREAM_END) {
                rc = 0;
            }
            inflateEnd(&strm);
        }
    } else if (local.compression == 0) {  /* Stored */
        memcpy(imz->data, compressed, local.compressed_size);
        rc = 0;
    }
#else
    rc = simple_inflate(compressed, local.compressed_size,
                        imz->data, local.uncompressed_size,
                        local.compression);
#endif
    
    free(compressed);
    
    if (rc != 0) {
        free(imz->data);
        free(imz->path);
        free(imz);
        return NULL;
    }
    
    return imz;
}

uft_imz_t* uft_imz_create(const char *path, size_t image_size) {
    if (!path || image_size == 0) return NULL;
    
    uft_imz_t *imz = (uft_imz_t *)calloc(1, sizeof(uft_imz_t));
    if (!imz) return NULL;
    
    imz->path = strdup(path);
    imz->size = image_size;
    imz->data = (uint8_t *)calloc(1, image_size);
    imz->modified = true;
    
    if (!imz->data) {
        free(imz->path);
        free(imz);
        return NULL;
    }
    
    return imz;
}

void uft_imz_close(uft_imz_t *imz) {
    if (!imz) return;
    
    /* Save if modified */
    if (imz->modified && imz->path && imz->data) {
        uft_imz_write_all(imz, imz->data, imz->size);
    }
    
    free(imz->data);
    free(imz->path);
    free(imz);
}

/*===========================================================================
 * I/O OPERATIONS
 *===========================================================================*/

int uft_imz_read_sector(uft_imz_t *imz, int track, int head, int sector,
                        uint8_t *buffer, size_t size) {
    if (!imz || !imz->data || !buffer) return -1;
    
    /* Assume standard 512-byte sectors, 18 sectors/track */
    size_t offset = ((track * 2 + head) * 18 + sector) * 512;
    
    if (offset + size > imz->size) return -1;
    
    memcpy(buffer, imz->data + offset, size);
    return 0;
}

int uft_imz_write_sector(uft_imz_t *imz, int track, int head, int sector,
                         const uint8_t *buffer, size_t size) {
    if (!imz || !imz->data || !buffer) return -1;
    
    size_t offset = ((track * 2 + head) * 18 + sector) * 512;
    
    if (offset + size > imz->size) return -1;
    
    memcpy(imz->data + offset, buffer, size);
    imz->modified = true;
    return 0;
}

int uft_imz_read_all(uft_imz_t *imz, uint8_t **data, size_t *size) {
    if (!imz || !data || !size) return -1;
    
    *data = (uint8_t *)malloc(imz->size);
    if (!*data) return -1;
    
    memcpy(*data, imz->data, imz->size);
    *size = imz->size;
    return 0;
}

int uft_imz_write_all(uft_imz_t *imz, const uint8_t *data, size_t size) {
    if (!imz || !data || !imz->path) return -1;
    
    FILE *f = fopen(imz->path, "wb");
    if (!f) return -1;
    
    /* Compress data */
    uint8_t *compressed = NULL;
    size_t compressed_size = 0;
    int method = 0;  /* Default: stored */
    
#if USE_ZLIB
    /* Try deflate compression */
    compressed_size = compressBound(size);
    compressed = (uint8_t *)malloc(compressed_size);
    if (compressed) {
        z_stream strm = {0};
        strm.next_in = (uint8_t *)data;
        strm.avail_in = size;
        strm.next_out = compressed;
        strm.avail_out = compressed_size;
        
        if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
            if (deflate(&strm, Z_FINISH) == Z_STREAM_END) {
                compressed_size = strm.total_out;
                method = 8;  /* Deflate */
            }
            deflateEnd(&strm);
        }
        
        /* If compression didn't help, store uncompressed */
        if (compressed_size >= size) {
            free(compressed);
            compressed = NULL;
        }
    }
#endif
    
    if (!compressed) {
        compressed = (uint8_t *)data;
        compressed_size = size;
        method = 0;
    }
    
    /* Calculate CRC */
    uint32_t crc = calc_crc32(data, size);
    
    /* Create filename from path */
    const char *basename = strrchr(imz->path, '/');
    if (!basename) basename = strrchr(imz->path, '\\');
    basename = basename ? basename + 1 : imz->path;
    
    /* Change extension to .img */
    char filename[256];
    strncpy(filename, basename, sizeof(filename) - 5);  /* Leave room for .img */
    filename[sizeof(filename) - 5] = '\0';
    char *ext = strrchr(filename, '.');
    if (ext) {
        snprintf(ext, 5, ".img");
    } else {
        size_t len = strlen(filename);
        if (len < sizeof(filename) - 4) {
            strncpy(filename + len, ".img", 4);
            filename[len + 4] = '\0';
        }
    }
    
    size_t filename_len = strlen(filename);
    
    /* Write local header */
    zip_local_header_t local = {
        .signature = ZIP_LOCAL_SIG,
        .version_needed = 20,
        .flags = 0,
        .compression = method,
        .mod_time = 0,
        .mod_date = 0,
        .crc32 = crc,
        .compressed_size = (uint32_t)compressed_size,
        .uncompressed_size = (uint32_t)size,
        .filename_len = (uint16_t)filename_len,
        .extra_len = 0
    };
    fwrite(&local, sizeof(local), 1, f);
    fwrite(filename, 1, filename_len, f);
    
    /* Write compressed data */
    fwrite(compressed, 1, compressed_size, f);
    
    uint32_t local_offset = 0;
    uint32_t cd_offset = (uint32_t)ftell(f);
    
    /* Write central directory */
    zip_central_header_t central = {
        .signature = ZIP_CENTRAL_SIG,
        .version_made = 20,
        .version_needed = 20,
        .flags = 0,
        .compression = method,
        .mod_time = 0,
        .mod_date = 0,
        .crc32 = crc,
        .compressed_size = (uint32_t)compressed_size,
        .uncompressed_size = (uint32_t)size,
        .filename_len = (uint16_t)filename_len,
        .extra_len = 0,
        .comment_len = 0,
        .disk_start = 0,
        .internal_attr = 0,
        .external_attr = 0,
        .local_offset = local_offset
    };
    fwrite(&central, sizeof(central), 1, f);
    fwrite(filename, 1, filename_len, f);
    
    uint32_t cd_size = (uint32_t)ftell(f) - cd_offset;
    
    /* Write end of central directory */
    zip_end_header_t end = {
        .signature = ZIP_END_SIG,
        .disk_number = 0,
        .cd_disk = 0,
        .disk_entries = 1,
        .total_entries = 1,
        .cd_size = cd_size,
        .cd_offset = cd_offset,
        .comment_len = 0
    };
    fwrite(&end, sizeof(end), 1, f);
    
    fclose(f);
    
#if USE_ZLIB
    if (compressed != data) {
        free(compressed);
    }
#endif
    
    imz->modified = false;
    return 0;
}

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

int uft_imz_compress(const char *img_path, const char *imz_path) {
    if (!img_path || !imz_path) return -1;
    
    /* Read IMG file */
    FILE *f = fopen(img_path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);
    
    /* Create IMZ */
    uft_imz_t *imz = uft_imz_create(imz_path, size);
    if (!imz) {
        free(data);
        return -1;
    }
    
    memcpy(imz->data, data, size);
    free(data);
    
    int rc = uft_imz_write_all(imz, imz->data, imz->size);
    
    imz->modified = false;
    uft_imz_close(imz);
    
    return rc;
}

int uft_imz_decompress(const char *imz_path, const char *img_path) {
    if (!imz_path || !img_path) return -1;
    
    uft_imz_t *imz = uft_imz_open(imz_path);
    if (!imz) return -1;
    
    FILE *f = fopen(img_path, "wb");
    if (!f) {
        uft_imz_close(imz);
        return -1;
    }
    
    fwrite(imz->data, 1, imz->size, f);
    fclose(f);
    
    imz->modified = false;
    uft_imz_close(imz);
    
    return 0;
}

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

size_t uft_imz_get_size(uft_imz_t *imz) {
    return imz ? imz->size : 0;
}

size_t uft_imz_get_compressed_size(uft_imz_t *imz) {
    return imz ? imz->compressed_size : 0;
}

float uft_imz_get_ratio(uft_imz_t *imz) {
    if (!imz || imz->size == 0) return 0.0f;
    return (float)imz->compressed_size / (float)imz->size;
}
