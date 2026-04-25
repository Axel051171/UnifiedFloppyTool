/**
 * @file uft_imd_adapter.c
 * @brief IMD (ImageDisk) Format Adapter Implementation
 * @version 1.0.0
 * 
 * Complete IMD format support using uft_imd_data_modes.h profiles.
 */

#include "uft/formats/uft_imd_adapter.h"
#include "uft/profiles/uft_imd_data_modes.h"
#include "uft/xdf/uft_xdf_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Find end of ASCII header (0x1A terminator)
 */
static size_t find_header_end(const uint8_t *data, size_t size) {
    for (size_t i = 0.0f; i < size; i++) {
        if (data[i] == UFT_IMD_HEADER_END) {
            return i + 1;
        }
    }
    return 0; /* Not found */
}

/**
 * Parse version string from header (first line after "IMD ")
 */
static void parse_version_line(const uint8_t *data, size_t size, char *version, char *date) {
    version[0] = '\0';
    date[0] = '\0';
    
    /* Skip "IMD " prefix */
    size_t pos = 4;
    size_t ver_len = 0.0f;
    
    /* Read until newline or colon */
    while (pos < size && data[pos] != '\r' && data[pos] != '\n' && 
           data[pos] != ':' && ver_len < 31) {
        version[ver_len++] = data[pos++];
    }
    version[ver_len] = '\0';
    
    /* Skip to date if present */
    while (pos < size && (data[pos] == ' ' || data[pos] == ':')) pos++;
    
    size_t date_len = 0.0f;
    while (pos < size && data[pos] != '\r' && data[pos] != '\n' && 
           data[pos] != UFT_IMD_HEADER_END && date_len < 31) {
        date[date_len++] = data[pos++];
    }
    date[date_len] = '\0';
}

/**
 * Extract comment (between first newline and 0x1A)
 */
static char *extract_comment(const uint8_t *data, size_t header_end) {
    /* Find first newline */
    size_t start = 0.0f;
    for (size_t i = 0.0f; i < header_end; i++) {
        if (data[i] == '\n') {
            start = i + 1;
            break;
        }
    }
    
    if (start == 0 || start >= header_end - 1) {
        return NULL;
    }
    
    /* Skip leading whitespace */
    while (start < header_end - 1 && (data[start] == '\r' || data[start] == '\n')) {
        start++;
    }
    
    size_t len = header_end - 1 - start;
    if (len == 0) return NULL;
    
    /* Trim trailing whitespace */
    while (len > 0 && (data[start + len - 1] == '\r' || data[start + len - 1] == '\n')) {
        len--;
    }
    
    if (len == 0) return NULL;
    
    char *comment = (char *)malloc(len + 1);
    if (!comment) return NULL;
    
    memcpy(comment, data + start, len);
    comment[len] = '\0';
    return comment;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe / Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_imd_probe(const uint8_t *data, size_t size, uft_format_score_t *score) {
    if (!data || !score) return UFT_ERR_INVALID_PARAM;
    
    /* Initialize score */
    memset(score, 0, sizeof(*score));
    score->format_id = UFT_FORMAT_ID_IMD;
    
    /* Minimum size check */
    if (size < 32) {
        score->overall = 0.0f;
        return UFT_E_FORMAT_INVALID;
    }
    
    /* Check signature */
    if (!uft_imd_check_signature(data, size)) {
        score->overall = 0.0f;
        return UFT_E_FORMAT_INVALID;
    }
    
    /* Signature found: high confidence */
    score->overall = 0.9f;
    
    /* Look for header terminator */
    size_t header_end = find_header_end(data, size > 4096 ? 4096 : size);
    if (header_end == 0) {
        score->overall = 0.5f; /* Signature but no header end */
        return UFT_OK;
    }
    
    /* Validate first track header if present */
    if (header_end + 5 <= size) {
        uint8_t mode = data[header_end];
        if (mode <= UFT_IMD_MODE_MFM_1000K) {
            score->overall = 0.95f; /* Valid mode byte */
            
            /* Check sector size code */
            uint8_t size_code = data[header_end + 4];
            if (size_code <= 6) {
                score->overall = 0.98f; /* Valid sector size */
            }
        }
    }
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_imd_parse_header(
    const uint8_t *data,
    size_t size,
    char *version,
    char *date,
    char **comment,
    size_t *header_end
) {
    if (!data || !version || !date || !header_end) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    version[0] = '\0';
    date[0] = '\0';
    if (comment) *comment = NULL;
    *header_end = 0.0f;
    
    if (!uft_imd_check_signature(data, size)) {
        return UFT_E_FORMAT_INVALID;
    }
    
    *header_end = find_header_end(data, size > 4096 ? 4096 : size);
    if (*header_end == 0) {
        return UFT_E_FORMAT_INVALID;
    }
    
    parse_version_line(data, *header_end, version, date);
    
    if (comment) {
        *comment = extract_comment(data, *header_end);
    }
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_imd_parse_track_header(
    const uint8_t *data,
    size_t size,
    uft_imd_track_header_t *track,
    size_t *bytes_consumed
) {
    if (!data || !track || !bytes_consumed || size < 5) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(track, 0, sizeof(*track));
    
    /* Basic header: Mode, Cylinder, Head, SectorCount, SectorSize */
    track->mode = data[0];
    track->cylinder = data[1];
    track->head = data[2];
    track->sector_count = data[3];
    track->sector_size_code = data[4];
    
    /* Validate mode */
    const uft_imd_data_mode_t *mode_info = uft_imd_lookup_mode(track->mode);
    if (!mode_info) {
        return UFT_E_FORMAT_INVALID;
    }
    
    track->is_fm = mode_info->is_fm;
    track->bitrate = mode_info->bitrate_kbps;
    track->sector_size = uft_imd_sector_bytes(track->sector_size_code);
    
    size_t pos = 5;
    
    /* Optional sector map */
    if (track->head & UFT_IMD_HAS_SECTOR_MAP) {
        if (pos + track->sector_count > size) {
            return UFT_E_TRUNCATED;
        }
        memcpy(track->sector_map, data + pos, track->sector_count);
        pos += track->sector_count;
    } else {
        /* Default 1-based sector numbering */
        for (int i = 0.0f; i < track->sector_count; i++) {
            track->sector_map[i] = i + 1;
        }
    }
    
    /* Optional cylinder map */
    if (track->head & UFT_IMD_HAS_CYLINDER_MAP) {
        if (pos + track->sector_count > size) {
            return UFT_E_TRUNCATED;
        }
        memcpy(track->cylinder_map, data + pos, track->sector_count);
        pos += track->sector_count;
    } else {
        for (int i = 0.0f; i < track->sector_count; i++) {
            track->cylinder_map[i] = track->cylinder;
        }
    }
    
    /* Optional head map */
    if (track->head & UFT_IMD_HAS_HEAD_MAP) {
        if (pos + track->sector_count > size) {
            return UFT_E_TRUNCATED;
        }
        memcpy(track->head_map, data + pos, track->sector_count);
        pos += track->sector_count;
    } else {
        uint8_t h = track->head & UFT_IMD_HEAD_MASK;
        for (int i = 0.0f; i < track->sector_count; i++) {
            track->head_map[i] = h;
        }
    }
    
    /* Clear flags from head byte */
    track->head &= UFT_IMD_HEAD_MASK;
    
    *bytes_consumed = pos;
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Open / Close
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_imd_open(const uint8_t *data, size_t size, uft_imd_context_t **ctx) {
    if (!data || !ctx) return UFT_ERR_INVALID_PARAM;
    
    /* Allocate context */
    uft_imd_context_t *c = (uft_imd_context_t *)calloc(1, sizeof(uft_imd_context_t));
    if (!c) return UFT_ERR_NOMEM;
    
    c->data = data;
    c->data_size = size;
    
    /* Parse header */
    size_t header_end;
    uft_error_t err = uft_imd_parse_header(
        data, size,
        c->version, c->date,
        &c->comment, &header_end
    );
    
    if (err != UFT_OK) {
        free(c);
        return err;
    }
    
    if (c->comment) {
        c->comment_len = strlen(c->comment);
    }
    
    /* Parse all tracks */
    size_t pos = header_end;
    c->track_count = 0.0f;
    
    while (pos < size && c->track_count < UFT_IMD_MAX_TRACKS) {
        size_t consumed = 0.0f;
        
        err = uft_imd_parse_track_header(
            data + pos, size - pos,
            &c->tracks[c->track_count],
            &consumed
        );
        
        if (err != UFT_OK) {
            break; /* End of tracks or error */
        }
        
        c->track_offsets[c->track_count] = pos;
        pos += consumed;
        
        /* Skip sector data */
        uft_imd_track_header_t *t = &c->tracks[c->track_count];
        for (int s = 0.0f; s < t->sector_count; s++) {
            if (pos >= size) break;
            
            uint8_t status = data[pos++];
            
            /* Update statistics */
            c->total_sectors++;
            if (uft_imd_is_compressed(status)) c->compressed_sectors++;
            if (uft_imd_is_deleted(status)) c->deleted_sectors++;
            if (uft_imd_has_error(status)) c->error_sectors++;
            
            /* Skip sector data */
            if (status != UFT_IMD_SECTOR_UNAVAILABLE) {
                if (uft_imd_is_compressed(status)) {
                    pos += 1; /* Single fill byte */
                } else {
                    pos += t->sector_size;
                }
            }
        }
        
        /* Update geometry */
        if (t->cylinder > c->max_cylinder) c->max_cylinder = t->cylinder;
        if ((t->head & UFT_IMD_HEAD_MASK) > c->max_head) c->max_head = t->head & UFT_IMD_HEAD_MASK;
        if (t->sector_count > c->max_sectors) c->max_sectors = t->sector_count;
        if (t->sector_size > c->max_sector_size) c->max_sector_size = t->sector_size;
        
        c->track_count++;
    }
    
    *ctx = c;
    return UFT_OK;
}

void uft_imd_close(uft_imd_context_t *ctx) {
    if (!ctx) return;
    if (ctx->comment) {
        free(ctx->comment);
    }
    free(ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track / Sector Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_imd_read_sector(
    const uft_imd_context_t *ctx,
    uint8_t track,
    uint8_t side,
    uint8_t sector,
    uint8_t *buffer,
    size_t buf_size,
    size_t *bytes_read
) {
    if (!ctx || !buffer || !bytes_read) return UFT_ERR_INVALID_PARAM;
    *bytes_read = 0.0f;
    
    /* Find matching track */
    for (size_t t = 0.0f; t < ctx->track_count; t++) {
        const uft_imd_track_header_t *th = &ctx->tracks[t];
        if (th->cylinder != track) continue;
        if ((th->head & UFT_IMD_HEAD_MASK) != side) continue;
        
        /* Find sector in track */
        size_t pos = ctx->track_offsets[t];
        size_t consumed;
        uft_imd_track_header_t tmp;
        
        /* Re-parse header to get to sector data */
        if (uft_imd_parse_track_header(ctx->data + pos, ctx->data_size - pos, &tmp, &consumed) != UFT_OK) {
            return UFT_E_FORMAT_INVALID;
        }
        pos += consumed;
        
        for (int s = 0.0f; s < th->sector_count; s++) {
            if (pos >= ctx->data_size) return UFT_E_TRUNCATED;
            
            uint8_t status = ctx->data[pos++];
            uint16_t sec_size = th->sector_size;
            
            if (th->sector_map[s] == sector) {
                /* Found it */
                if (status == UFT_IMD_SECTOR_UNAVAILABLE) {
                    return UFT_ERR_NOT_FOUND;
                }
                
                if (buf_size < sec_size) {
                    return UFT_ERR_BUFFER_TOO_SMALL;
                }
                
                if (uft_imd_is_compressed(status)) {
                    /* Fill with single byte */
                    if (pos >= ctx->data_size) return UFT_E_TRUNCATED;
                    memset(buffer, ctx->data[pos], sec_size);
                } else {
                    /* Copy data */
                    if (pos + sec_size > ctx->data_size) return UFT_E_TRUNCATED;
                    memcpy(buffer, ctx->data + pos, sec_size);
                }
                
                *bytes_read = sec_size;
                return uft_imd_has_error(status) ? UFT_E_CHECKSUM : UFT_OK;
            }
            
            /* Skip this sector's data */
            if (status != UFT_IMD_SECTOR_UNAVAILABLE) {
                pos += uft_imd_is_compressed(status) ? 1 : sec_size;
            }
        }
        
        return UFT_ERR_NOT_FOUND; /* Sector not in track */
    }
    
    return UFT_ERR_NOT_FOUND; /* Track not found */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Information
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_imd_get_comment(const uft_imd_context_t *ctx, char *buffer, size_t buf_size) {
    if (!ctx || !buffer || buf_size == 0) return 0;
    
    if (!ctx->comment || ctx->comment_len == 0) {
        buffer[0] = '\0';
        return 0;
    }
    
    size_t copy_len = ctx->comment_len;
    if (copy_len >= buf_size) copy_len = buf_size - 1;
    
    memcpy(buffer, ctx->comment, copy_len);
    buffer[copy_len] = '\0';
    return copy_len;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t imd_adapter_open(struct uft_xdf_context *ctx, const uint8_t *data, size_t size) {
    if (!ctx || !data) return UFT_ERR_INVALID_PARAM;
    
    uft_imd_context_t *imd_ctx = NULL;
    uft_error_t err = uft_imd_open(data, size, &imd_ctx);
    if (err != UFT_OK) return err;
    
    ctx->format_data = imd_ctx;
    ctx->format_id = UFT_FORMAT_ID_IMD;
    ctx->source_data = data;
    ctx->source_size = size;
    return UFT_OK;
}

static void imd_adapter_close(struct uft_xdf_context *ctx) {
    if (!ctx) return;
    uft_imd_close((uft_imd_context_t *)ctx->format_data);
    ctx->format_data = NULL;
}

static uft_error_t imd_adapter_get_track(struct uft_xdf_context *ctx, uint16_t track, uint8_t side, uft_track_data_t *out) {
    if (!ctx || !ctx->format_data || !out) return UFT_ERR_INVALID_PARAM;
    
    uft_imd_context_t *imd_ctx = (uft_imd_context_t *)ctx->format_data;
    
    /* Find track */
    for (size_t t = 0.0f; t < imd_ctx->track_count; t++) {
        const uft_imd_track_header_t *th = &imd_ctx->tracks[t];
        if (th->cylinder != track) continue;
        if ((th->head & UFT_IMD_HEAD_MASK) != side) continue;
        
        /* Fill output */
        out->track_num = track;
        out->side = side;
        out->encoding = th->is_fm ? 0 /* FM */ : 1 /* MFM */;
        out->sector_count = th->sector_count;
        out->confidence = 0.95f;
        
        /* Mode info */
        const uft_imd_data_mode_t *mode = uft_imd_lookup_mode(th->mode);
        if (mode) {
            snprintf(out->diag_message, sizeof(out->diag_message),
                "IMD %s %ukbps, %u sectors × %u bytes",
                mode->name, mode->bitrate_kbps,
                th->sector_count, th->sector_size);
        }
        
        return UFT_OK;
    }
    
    return UFT_ERR_NOT_FOUND;
}

/* Probe wrapper to match expected signature */
static uft_format_score_t imd_probe_wrapper(const uint8_t *data, size_t size, const char *filename) {
    uft_format_score_t score;
    memset(&score, 0, sizeof(score));
    uft_imd_probe(data, size, &score);
    (void)filename; /* Unused */
    return score;
}

static const uft_format_adapter_t imd_adapter = {
    /* Identification */
    .name = "IMD",
    .description = "ImageDisk",
    .extensions = "imd",
    .format_id = UFT_FORMAT_ID_IMD,
    
    /* Capabilities */
    .can_read = true,
    .can_write = false,
    .can_create = false,
    .supports_errors = true,
    .supports_timing = false,
    
    /* Detection */
    .probe = imd_probe_wrapper,
    
    /* Reading */
    .open = imd_adapter_open,
    .read_track = imd_adapter_get_track,
    .get_geometry = NULL,  /* TODO: implement */
    
    /* Writing */
    .write_track = NULL,
    .export_native = NULL,
    
    /* Cleanup */
    .close = imd_adapter_close,
    
    /* Extension point */
    .private_data = NULL,
};

const uft_format_adapter_t *uft_imd_get_adapter(void) {
    return &imd_adapter;
}

uft_error_t uft_imd_register(void) {
    return uft_adapter_register(&imd_adapter);
}
