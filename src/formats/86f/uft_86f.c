/**
 * @file uft_86f.c
 * @brief 86Box 86F Format Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_86f.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

typedef struct {
    uft_86f_track_header_t header;
    uint8_t *data;              /**< Track bitstream */
    uint8_t *surface;           /**< Surface data (weak bits) */
    size_t data_size;
} f86_track_data_t;

struct uft_86f_context {
    FILE *file;
    char *path;
    uft_86f_header_t header;
    f86_track_data_t **tracks;  /**< [track][side] */
    int num_tracks;
    int num_sides;
    bool writable;
    bool modified;
};

/*===========================================================================
 * HELPERS
 *===========================================================================*/

static int track_index(uft_86f_t *ctx, int track, int side) {
    if (!ctx || track < 0 || track >= ctx->num_tracks ||
        side < 0 || side >= ctx->num_sides) return -1;
    return track * ctx->num_sides + side;
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

bool uft_86f_probe(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    char magic[4];
    bool valid = (fread(magic, 1, 4, f) == 4 && 
                  memcmp(magic, UFT_86F_MAGIC, 4) == 0);
    fclose(f);
    
    return valid;
}

uft_86f_t* uft_86f_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uft_86f_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    /* Verify magic */
    if (memcmp(header.magic, UFT_86F_MAGIC, 4) != 0) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    /* Allocate context */
    uft_86f_t *ctx = (uft_86f_t *)calloc(1, sizeof(*ctx));
    if (!ctx) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    ctx->path = strdup(path);
    ctx->header = header;
    ctx->num_tracks = header.num_tracks;
    ctx->num_sides = header.num_sides;
    ctx->writable = (header.flags & UFT_86F_FLAG_WRITEABLE) != 0;
    
    /* Allocate track array */
    int total_tracks = ctx->num_tracks * ctx->num_sides;
    ctx->tracks = (f86_track_data_t **)calloc(total_tracks, 
                                               sizeof(f86_track_data_t *));
    if (!ctx->tracks) {
        free(ctx->path);
        free(ctx);
        if (ferror(f)) {
            fclose(f);
            return UFT_ERR_IO;
        }
                fclose(f);
        return NULL;
    }
    
    /* Load tracks */
    for (int t = 0; t < ctx->num_tracks; t++) {
        for (int s = 0; s < ctx->num_sides; s++) {
            int idx = t * ctx->num_sides + s;
            uint32_t offset = header.track_offset[idx];
            
            if (offset == 0) continue;
            
            ctx->tracks[idx] = (f86_track_data_t *)calloc(1, 
                                                          sizeof(f86_track_data_t));
            if (!ctx->tracks[idx]) continue;
            
            /* Read track header */
            if (fseek(f, offset, SEEK_SET) != 0) continue;
            if (fread(&ctx->tracks[idx]->header, 
                      sizeof(uft_86f_track_header_t), 1, f) != 1) {
                free(ctx->tracks[idx]);
                ctx->tracks[idx] = NULL;
                continue;
            }
            
            /* Calculate data size */
            size_t data_bytes = (ctx->tracks[idx]->header.bit_count + 7) / 8;
            ctx->tracks[idx]->data_size = data_bytes;
            
            /* Read track data */
            if (data_bytes > 0) {
                ctx->tracks[idx]->data = (uint8_t *)malloc(data_bytes);
                if (ctx->tracks[idx]->data) {
                    if (fread(ctx->tracks[idx]->data, 1, data_bytes, f) != data_bytes) {
                        free(ctx->tracks[idx]->data);
                        ctx->tracks[idx]->data = NULL;
                        ctx->tracks[idx]->data_size = 0;
                        continue;
                    }
                }
                
                /* Read surface data if present */
                if (header.flags & UFT_86F_FLAG_HAS_SURFACE) {
                    ctx->tracks[idx]->surface = (uint8_t *)malloc(data_bytes);
                    if (ctx->tracks[idx]->surface) {
                        if (fread(ctx->tracks[idx]->surface, 1, data_bytes, f) != data_bytes) {
                            free(ctx->tracks[idx]->surface);
                            ctx->tracks[idx]->surface = NULL;
                        }
                    }
                }
            }
        }
    }
    
    return ctx;
}

uft_86f_t* uft_86f_create(const char *path, int tracks, int sides,
                           int encoding, int rpm) {
    if (!path || tracks <= 0 || sides <= 0) return NULL;
    
    uft_86f_t *ctx = (uft_86f_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    ctx->path = strdup(path);
    ctx->num_tracks = tracks;
    ctx->num_sides = sides;
    ctx->writable = true;
    ctx->modified = true;
    
    /* Initialize header */
    memcpy(ctx->header.magic, UFT_86F_MAGIC, 4);
    ctx->header.version = UFT_86F_VERSION_2;
    ctx->header.flags = UFT_86F_FLAG_WRITEABLE;
    ctx->header.encoding = encoding;
    ctx->header.rpm = (rpm == 360) ? 1 : 0;
    ctx->header.num_tracks = tracks;
    ctx->header.num_sides = sides;
    
    /* Allocate track array */
    int total = tracks * sides;
    ctx->tracks = (f86_track_data_t **)calloc(total, sizeof(f86_track_data_t *));
    
    return ctx;
}

void uft_86f_close(uft_86f_t *ctx) {
    if (!ctx) return;
    
    /* Auto-save on close: write back modified track data to 86F file */
    if (ctx->modified && ctx->path) {
        uft_86f_save(ctx, ctx->path);
    }
    
    /* Free tracks */
    if (ctx->tracks) {
        int total = ctx->num_tracks * ctx->num_sides;
        for (int i = 0; i < total; i++) {
            if (ctx->tracks[i]) {
                free(ctx->tracks[i]->data);
                free(ctx->tracks[i]->surface);
                free(ctx->tracks[i]);
            }
        }
        free(ctx->tracks);
    }
    
    if (ctx->file) fclose(ctx->file);
    free(ctx->path);
    free(ctx);
}

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

const uft_86f_header_t* uft_86f_get_header(uft_86f_t *ctx) {
    return ctx ? &ctx->header : NULL;
}

int uft_86f_get_tracks(uft_86f_t *ctx) {
    return ctx ? ctx->num_tracks : 0;
}

int uft_86f_get_sides(uft_86f_t *ctx) {
    return ctx ? ctx->num_sides : 0;
}

bool uft_86f_is_writable(uft_86f_t *ctx) {
    return ctx ? ctx->writable : false;
}

/*===========================================================================
 * TRACK OPERATIONS
 *===========================================================================*/

int uft_86f_get_track_header(uft_86f_t *ctx, int track, int side,
                              uft_86f_track_header_t *header) {
    if (!ctx || !header) return -1;
    
    int idx = track_index(ctx, track, side);
    if (idx < 0 || !ctx->tracks[idx]) return -1;
    
    *header = ctx->tracks[idx]->header;
    return 0;
}

int uft_86f_read_track_bits(uft_86f_t *ctx, int track, int side,
                             uint8_t *bits, size_t max_bytes,
                             uint32_t *bit_count) {
    if (!ctx || !bits) return -1;
    
    int idx = track_index(ctx, track, side);
    if (idx < 0 || !ctx->tracks[idx] || !ctx->tracks[idx]->data) return -1;
    
    f86_track_data_t *td = ctx->tracks[idx];
    size_t to_copy = td->data_size;
    if (to_copy > max_bytes) to_copy = max_bytes;
    
    memcpy(bits, td->data, to_copy);
    
    if (bit_count) *bit_count = td->header.bit_count;
    
    return (int)to_copy;
}

int uft_86f_write_track_bits(uft_86f_t *ctx, int track, int side,
                              const uint8_t *bits, uint32_t bit_count,
                              int encoding, int data_rate) {
    if (!ctx || !bits || !ctx->writable) return -1;
    
    int idx = track_index(ctx, track, side);
    if (idx < 0) return -1;
    
    /* Allocate track if needed */
    if (!ctx->tracks[idx]) {
        ctx->tracks[idx] = (f86_track_data_t *)calloc(1, sizeof(f86_track_data_t));
        if (!ctx->tracks[idx]) return -1;
    }
    
    f86_track_data_t *td = ctx->tracks[idx];
    
    /* Free old data */
    free(td->data);
    
    /* Allocate new */
    size_t bytes = (bit_count + 7) / 8;
    td->data = (uint8_t *)malloc(bytes);
    if (!td->data) return -1;
    
    memcpy(td->data, bits, bytes);
    td->data_size = bytes;
    
    /* Update header */
    td->header.cylinder = track;
    td->header.head = side;
    td->header.encoding = encoding;
    td->header.data_rate = data_rate;
    td->header.bit_count = bit_count;
    
    ctx->modified = true;
    return 0;
}

int uft_86f_read_surface(uft_86f_t *ctx, int track, int side,
                          uint8_t *surface, size_t max_bytes) {
    if (!ctx || !surface) return -1;
    
    int idx = track_index(ctx, track, side);
    if (idx < 0 || !ctx->tracks[idx] || !ctx->tracks[idx]->surface) return -1;
    
    f86_track_data_t *td = ctx->tracks[idx];
    size_t to_copy = td->data_size;
    if (to_copy > max_bytes) to_copy = max_bytes;
    
    memcpy(surface, td->surface, to_copy);
    return (int)to_copy;
}

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

int uft_86f_to_img(const char *f86_path, const char *img_path) {
    uft_86f_t *ctx = uft_86f_open(f86_path);
    if (!ctx) return -1;
    
    FILE *f = fopen(img_path, "wb");
    if (!f) {
        uft_86f_close(ctx);
        return -1;
    }
    
    /* Decode tracks to sectors */
    /* This is a simplified version - full implementation would decode MFM */
    uint8_t sector[512];
    
    for (int t = 0; t < ctx->num_tracks; t++) {
        for (int s = 0; s < ctx->num_sides; s++) {
            /* For each sector (assume 18 sectors/track for 1.44MB) */
            for (int sec = 0; sec < 18; sec++) {
                memset(sector, 0, sizeof(sector));
                /* Decode MFM: extract data bytes from bitstream encoding */
                if (ctx->tracks && ctx->tracks[t * ctx->num_sides + s]) {
                    /* Use raw sector data from parsed track */
                    memcpy(sector, ctx->tracks[t * ctx->num_sides + s]->raw + sec * 512, 512);
                }
                fwrite(sector, 1, sizeof(sector), f);
            }
        }
    }
    
    fclose(f);
    uft_86f_close(ctx);
    return 0;
}

int uft_img_to_86f(const char *img_path, const char *f86_path) {
    FILE *f = fopen(img_path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Determine geometry */
    int tracks = 80, sides = 2, spt = 18;
    if (size == 720 * 1024) { spt = 9; }
    else if (size == 360 * 1024) { tracks = 40; spt = 9; }
    
    uft_86f_t *ctx = uft_86f_create(f86_path, tracks, sides, 
                                     UFT_86F_ENC_MFM, 300);
    if (!ctx) {
        fclose(f);
        return -1;
    }
    
    /* Read sectors and encode to MFM */
    /* This is a simplified version */
    uint8_t sector[512];
    
    for (int t = 0; t < tracks; t++) {
        for (int s = 0; s < sides; s++) {
            /* Read all sectors for this track */
            size_t track_size = spt * 512;
            uint8_t *track_data = (uint8_t *)calloc(1, track_size);
            
            for (int sec = 0; sec < spt; sec++) {
                if (fread(sector, 1, 512, f) == 512) {
                    memcpy(track_data + sec * 512, sector, 512);
                }
            }
            
            /* Encode sector data to MFM bitstream (raw storage for now) */
            /* For now, just store raw sector data */
            uft_86f_write_track_bits(ctx, t, s, track_data, 
                                      track_size * 8,
                                      UFT_86F_ENC_MFM, UFT_86F_RATE_500K);
            
            free(track_data);
        }
    }
    
    fclose(f);
    
    /* Save 86F: file is built in-memory, written on close */
    uft_86f_close(ctx);
    return 0;
}
