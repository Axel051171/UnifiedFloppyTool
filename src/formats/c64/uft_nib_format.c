/**
 * @file uft_nib_format.c
 * @brief NIB/NB2/NBZ Disk Image Format Implementation
 * 
 * Based on nibtools by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/formats/c64/uft_nib_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Sectors per track for 1541 */
static const int sectors_per_track[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /*  1 - 10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 11 - 20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 21 - 30 */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,  /* 31 - 40 */
    17, 17                                    /* 41 - 42 */
};

/** Track capacity at each density */
static const size_t capacity[4] = { 6250, 6666, 7142, 7692 };
static const size_t capacity_min[4] = { 6100, 6500, 7000, 7500 };
static const size_t capacity_max[4] = { 6400, 6850, 7300, 7900 };

/** Format names */
static const char *format_names[] = {
    "Unknown", "NIB", "NB2", "NBZ", "G64"
};

/* ============================================================================
 * LZ77 Compression (from nibtools by Marcus Geelnard)
 * ============================================================================ */

/** Maximum offset for LZ77 */
#define LZ_MAX_OFFSET 100000

/**
 * @brief Write variable-length integer
 */
static int lz_write_var_size(uint8_t *out, size_t val)
{
    int bytes = 0;
    uint8_t byte;
    
    do {
        byte = (uint8_t)(val & 0x7F);
        val >>= 7;
        if (val) byte |= 0x80;
        out[bytes++] = byte;
    } while (val);
    
    return bytes;
}

/**
 * @brief Read variable-length integer
 */
static int lz_read_var_size(const uint8_t *in, size_t *val)
{
    int bytes = 0;
    size_t result = 0;
    int shift = 0;
    uint8_t byte;
    
    do {
        byte = in[bytes++];
        result |= ((size_t)(byte & 0x7F)) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    *val = result;
    return bytes;
}

/**
 * @brief Find most common byte in data
 */
static uint8_t lz_find_marker(const uint8_t *in, size_t insize)
{
    size_t histogram[256] = {0};
    uint8_t marker = 0;
    size_t min_count = insize + 1;
    
    for (size_t i = 0; i < insize; i++) {
        histogram[in[i]]++;
    }
    
    for (int i = 0; i < 256; i++) {
        if (histogram[i] < min_count) {
            min_count = histogram[i];
            marker = (uint8_t)i;
        }
    }
    
    return marker;
}

/**
 * @brief LZ77 compress
 */
size_t lz77_compress(const uint8_t *in, uint8_t *out, size_t insize)
{
    if (!in || !out || insize == 0) return 0;
    
    uint8_t marker = lz_find_marker(in, insize);
    out[0] = marker;
    size_t outpos = 1;
    size_t inpos = 0;
    
    while (inpos < insize) {
        /* Find best match in history */
        size_t best_len = 0;
        size_t best_offset = 0;
        
        size_t max_offset = (inpos < LZ_MAX_OFFSET) ? inpos : LZ_MAX_OFFSET;
        
        for (size_t offset = 1; offset <= max_offset; offset++) {
            size_t len = 0;
            while (inpos + len < insize && 
                   in[inpos + len] == in[inpos - offset + (len % offset)] &&
                   len < 65535) {
                len++;
            }
            
            if (len > best_len && len >= 4) {
                best_len = len;
                best_offset = offset;
            }
        }
        
        if (best_len >= 4) {
            /* Output reference */
            out[outpos++] = marker;
            outpos += lz_write_var_size(out + outpos, best_len);
            outpos += lz_write_var_size(out + outpos, best_offset);
            inpos += best_len;
        } else {
            /* Output literal */
            if (in[inpos] == marker) {
                out[outpos++] = marker;
                out[outpos++] = 0;
            } else {
                out[outpos++] = in[inpos];
            }
            inpos++;
        }
    }
    
    return outpos;
}

/**
 * @brief LZ77 fast compress using jump table
 */
size_t lz77_compress_fast(const uint8_t *in, uint8_t *out, size_t insize)
{
    if (!in || !out || insize == 0) return 0;
    
    /* Allocate jump table */
    int *jump = malloc(insize * sizeof(int));
    int *last_pos = malloc(256 * sizeof(int));
    
    if (!jump || !last_pos) {
        free(jump);
        free(last_pos);
        return lz77_compress(in, out, insize);  /* Fallback */
    }
    
    /* Initialize */
    for (int i = 0; i < 256; i++) last_pos[i] = -1;
    for (size_t i = 0; i < insize; i++) {
        jump[i] = last_pos[in[i]];
        last_pos[in[i]] = (int)i;
    }
    
    uint8_t marker = lz_find_marker(in, insize);
    out[0] = marker;
    size_t outpos = 1;
    size_t inpos = 0;
    
    while (inpos < insize) {
        /* Find best match using jump table */
        size_t best_len = 0;
        size_t best_offset = 0;
        
        int pos = jump[inpos];
        int max_checks = 100;  /* Limit search depth */
        
        while (pos >= 0 && (inpos - pos) <= LZ_MAX_OFFSET && max_checks-- > 0) {
            size_t len = 0;
            while (inpos + len < insize && in[inpos + len] == in[pos + len] && len < 65535) {
                len++;
            }
            
            if (len > best_len && len >= 4) {
                best_len = len;
                best_offset = inpos - pos;
            }
            
            pos = jump[pos];
        }
        
        if (best_len >= 4) {
            out[outpos++] = marker;
            outpos += lz_write_var_size(out + outpos, best_len);
            outpos += lz_write_var_size(out + outpos, best_offset);
            inpos += best_len;
        } else {
            if (in[inpos] == marker) {
                out[outpos++] = marker;
                out[outpos++] = 0;
            } else {
                out[outpos++] = in[inpos];
            }
            inpos++;
        }
    }
    
    free(jump);
    free(last_pos);
    
    return outpos;
}

/**
 * @brief LZ77 decompress
 */
size_t lz77_decompress(const uint8_t *in, uint8_t *out, size_t insize)
{
    if (!in || !out || insize == 0) return 0;
    
    uint8_t marker = in[0];
    size_t inpos = 1;
    size_t outpos = 0;
    
    while (inpos < insize) {
        if (in[inpos] == marker) {
            inpos++;
            if (inpos >= insize) break;
            
            if (in[inpos] == 0) {
                /* Escaped marker byte */
                out[outpos++] = marker;
                inpos++;
            } else {
                /* Reference */
                size_t len, offset;
                inpos += lz_read_var_size(in + inpos, &len);
                inpos += lz_read_var_size(in + inpos, &offset);
                
                for (size_t i = 0; i < len; i++) {
                    out[outpos] = out[outpos - offset];
                    outpos++;
                }
            }
        } else {
            out[outpos++] = in[inpos++];
        }
    }
    
    return outpos;
}

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * @brief Detect format from buffer
 */
nib_format_t nib_detect_format_buffer(const uint8_t *data, size_t size)
{
    if (!data || size < 8) return NIB_FORMAT_UNKNOWN;
    
    /* Check for G64 signature first (can be smaller buffer) */
    if (memcmp(data, "GCR-1541", 8) == 0) {
        return NIB_FORMAT_G64;
    }
    
    if (size < NIB_HEADER_SIZE) return NIB_FORMAT_UNKNOWN;
    
    /* Check for NIB/NB2 signature */
    if (memcmp(data, NIB_SIGNATURE, NIB_SIGNATURE_LEN) == 0) {
        /* Determine if NIB or NB2 based on file size */
        /* NB2 has 16 passes per track, so much larger */
        int num_track_entries = 0;
        for (int i = 0; i < 120; i += 2) {
            if (data[NIB_TRACK_ENTRY_OFFSET + i] == 0) break;
            num_track_entries++;
        }
        
        size_t expected_nib_size = NIB_HEADER_SIZE + (num_track_entries * NIB_TRACK_LENGTH);
        size_t expected_nb2_size = NIB_HEADER_SIZE + (num_track_entries * NIB_TRACK_LENGTH * NB2_PASSES_PER_TRACK);
        
        /* Allow some tolerance */
        if (size >= expected_nb2_size - NIB_TRACK_LENGTH * 4) {
            return NIB_FORMAT_NB2;
        }
        return NIB_FORMAT_NIB;
    }
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) == 0) {
        return NIB_FORMAT_G64;
    }
    
    /* Check for NBZ - try decompressing small amount */
    /* NBZ starts with marker byte, not "MNIB" */
    if (size > 32) {
        uint8_t test_out[256];
        size_t decompressed = lz77_decompress(data, test_out, 32);
        if (decompressed > 0 && memcmp(test_out, NIB_SIGNATURE, NIB_SIGNATURE_LEN) == 0) {
            return NIB_FORMAT_NBZ;
        }
    }
    
    return NIB_FORMAT_UNKNOWN;
}

/**
 * @brief Detect format from file
 */
nib_format_t nib_detect_format(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NIB_FORMAT_UNKNOWN;
    
    uint8_t header[256];
    size_t read = fread(header, 1, sizeof(header), fp);
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fclose(fp);
    
    if (read < NIB_HEADER_SIZE) return NIB_FORMAT_UNKNOWN;
    
    /* Check extension first as a hint */
    const char *ext = strrchr(filename, '.');
    if (ext) {
        if (strcasecmp(ext, ".nbz") == 0) {
            /* Verify it's compressed NIB */
            return NIB_FORMAT_NBZ;
        }
        if (strcasecmp(ext, ".nb2") == 0) {
            if (memcmp(header, NIB_SIGNATURE, NIB_SIGNATURE_LEN) == 0) {
                return NIB_FORMAT_NB2;
            }
        }
    }
    
    return nib_detect_format_buffer(header, file_size);
}

/* ============================================================================
 * NIB Loading/Saving
 * ============================================================================ */

/**
 * @brief Load NIB from buffer
 */
int nib_load_buffer(const uint8_t *data, size_t size, nib_image_t **image)
{
    if (!data || !image || size < NIB_HEADER_SIZE) return -1;
    
    /* Verify signature */
    if (memcmp(data, NIB_SIGNATURE, NIB_SIGNATURE_LEN) != 0) {
        return -2;  /* Invalid signature */
    }
    
    /* Allocate image */
    nib_image_t *img = calloc(1, sizeof(nib_image_t));
    if (!img) return -3;
    
    /* Copy header */
    memcpy(&img->header, data, sizeof(nib_header_t));
    img->has_halftracks = (img->header.halftracks != 0);
    
    /* Parse track entries */
    int t_index = 0;
    for (int h_index = 0; h_index < 240; h_index += 2) {
        uint8_t track = img->header.track_entries[h_index];
        if (track == 0) break;
        
        uint8_t density = img->header.track_entries[h_index + 1];
        density &= ~NIB_FLAG_MATCH;  /* Clear unused match flag */
        
        if (track < NIB_MAX_TRACKS) {
            img->track_density[track] = density;
            img->track_length[track] = NIB_TRACK_LENGTH;
            
            /* Allocate and copy track data */
            size_t track_offset = NIB_HEADER_SIZE + (t_index * NIB_TRACK_LENGTH);
            if (track_offset + NIB_TRACK_LENGTH <= size) {
                img->track_data[track] = malloc(NIB_TRACK_LENGTH);
                if (img->track_data[track]) {
                    memcpy(img->track_data[track], data + track_offset, NIB_TRACK_LENGTH);
                }
            }
        }
        
        t_index++;
    }
    
    img->num_tracks = t_index;
    
    /* Try to extract disk ID from track 18 (halftrack 36) */
    if (img->track_data[36]) {
        nib_extract_disk_id(img->track_data[36], NIB_TRACK_LENGTH, img->disk_id);
    }
    
    *image = img;
    return 0;
}

/**
 * @brief Load NIB file
 */
int nib_load(const char *filename, nib_image_t **image)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -2;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -3;
    }
    fclose(fp);
    
    int result = nib_load_buffer(data, size, image);
    free(data);
    
    return result;
}

/**
 * @brief Save NIB to buffer
 */
int nib_save_buffer(const nib_image_t *image, uint8_t **data, size_t *size)
{
    if (!image || !data || !size) return -1;
    
    /* Count tracks */
    int num_tracks = 0;
    for (int i = 0; i < NIB_MAX_TRACKS; i++) {
        if (image->track_data[i]) num_tracks++;
    }
    
    size_t total_size = NIB_HEADER_SIZE + (num_tracks * NIB_TRACK_LENGTH);
    uint8_t *buf = calloc(1, total_size);
    if (!buf) return -2;
    
    /* Build header */
    memcpy(buf, NIB_SIGNATURE, NIB_SIGNATURE_LEN);
    buf[13] = NIB_VERSION;
    buf[14] = 0;
    buf[15] = image->has_halftracks ? 1 : 0;
    
    int track_inc = image->has_halftracks ? 1 : 2;
    int header_entry = 0;
    int t_index = 0;
    
    for (int track = 2; track < NIB_MAX_TRACKS; track += track_inc) {
        if (image->track_data[track]) {
            buf[NIB_TRACK_ENTRY_OFFSET + (header_entry * 2)] = track;
            buf[NIB_TRACK_ENTRY_OFFSET + (header_entry * 2) + 1] = image->track_density[track];
            
            memcpy(buf + NIB_HEADER_SIZE + (t_index * NIB_TRACK_LENGTH),
                   image->track_data[track], NIB_TRACK_LENGTH);
            
            header_entry++;
            t_index++;
        }
    }
    
    *data = buf;
    *size = total_size;
    
    return 0;
}

/**
 * @brief Save NIB file
 */
int nib_save(const char *filename, const nib_image_t *image)
{
    uint8_t *data;
    size_t size;
    
    int result = nib_save_buffer(image, &data, &size);
    if (result != 0) return result;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(data);
        return -1;
    }
    
    if (fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        free(data);
        return -2;
    }
    
    fclose(fp);
    free(data);
    
    return 0;
}

/**
 * @brief Free NIB image
 */
void nib_free(nib_image_t *image)
{
    if (!image) return;
    
    for (int i = 0; i < NIB_MAX_TRACKS; i++) {
        free(image->track_data[i]);
    }
    
    free(image);
}

/**
 * @brief Create new NIB image
 */
nib_image_t *nib_create(bool has_halftracks)
{
    nib_image_t *img = calloc(1, sizeof(nib_image_t));
    if (!img) return NULL;
    
    memcpy(img->header.signature, NIB_SIGNATURE, NIB_SIGNATURE_LEN);
    img->header.version = NIB_VERSION;
    img->header.halftracks = has_halftracks ? 1 : 0;
    img->has_halftracks = has_halftracks;
    
    return img;
}

/**
 * @brief Get track data
 */
int nib_get_track(const nib_image_t *image, int halftrack,
                  const uint8_t **data, size_t *length, uint8_t *density)
{
    if (!image || halftrack < 0 || halftrack >= NIB_MAX_TRACKS) return -1;
    if (!image->track_data[halftrack]) return -2;
    
    if (data) *data = image->track_data[halftrack];
    if (length) *length = image->track_length[halftrack];
    if (density) *density = image->track_density[halftrack];
    
    return 0;
}

/**
 * @brief Set track data
 */
int nib_set_track(nib_image_t *image, int halftrack,
                  const uint8_t *data, size_t length, uint8_t density)
{
    if (!image || !data || halftrack < 0 || halftrack >= NIB_MAX_TRACKS) return -1;
    
    /* Free existing */
    free(image->track_data[halftrack]);
    
    /* Allocate and copy */
    image->track_data[halftrack] = malloc(NIB_TRACK_LENGTH);
    if (!image->track_data[halftrack]) return -2;
    
    memcpy(image->track_data[halftrack], data, 
           (length < NIB_TRACK_LENGTH) ? length : NIB_TRACK_LENGTH);
    
    if (length < NIB_TRACK_LENGTH) {
        memset(image->track_data[halftrack] + length, 0, NIB_TRACK_LENGTH - length);
    }
    
    image->track_length[halftrack] = length;
    image->track_density[halftrack] = density;
    
    return 0;
}

/* ============================================================================
 * NB2 Loading
 * ============================================================================ */

/**
 * @brief Load NB2 from buffer
 */
int nb2_load_buffer(const uint8_t *data, size_t size, nb2_image_t **image)
{
    if (!data || !image || size < NIB_HEADER_SIZE) return -1;
    
    /* Verify signature */
    if (memcmp(data, NIB_SIGNATURE, NIB_SIGNATURE_LEN) != 0) {
        return -2;
    }
    
    /* Allocate image */
    nb2_image_t *img = calloc(1, sizeof(nb2_image_t));
    if (!img) return -3;
    
    /* Copy header to base */
    memcpy(&img->base.header, data, sizeof(nib_header_t));
    img->base.has_halftracks = true;  /* NB2 always has halftracks */
    
    /* Estimate number of tracks from file size */
    int num_tracks = (size - NIB_HEADER_SIZE) / (NIB_TRACK_LENGTH * NB2_PASSES_PER_TRACK);
    
    /* Parse tracks */
    int header_entry = 0;
    
    for (int track = 2; track < NIB_MAX_TRACKS && header_entry < num_tracks; track++) {
        uint8_t stored_density = img->base.header.track_entries[(header_entry * 2) + 1];
        img->base.track_density[track] = stored_density;
        
        size_t best_errors = (size_t)-1;
        int best_pass = 0;
        
        /* Read all 16 passes */
        for (int pass_density = 0; pass_density < 4; pass_density++) {
            for (int pass = 0; pass < NB2_PASSES_PER_DENSITY; pass++) {
                int pass_index = pass_density * NB2_PASSES_PER_DENSITY + pass;
                
                size_t offset = NIB_HEADER_SIZE + 
                               (header_entry * NIB_TRACK_LENGTH * NB2_PASSES_PER_TRACK) +
                               (pass_index * NIB_TRACK_LENGTH);
                
                if (offset + NIB_TRACK_LENGTH <= size) {
                    /* Allocate and copy pass data */
                    img->all_passes[track][pass_index] = malloc(NIB_TRACK_LENGTH);
                    if (img->all_passes[track][pass_index]) {
                        memcpy(img->all_passes[track][pass_index], data + offset, NIB_TRACK_LENGTH);
                        
                        /* Track pass info */
                        img->track_info[track].passes[pass_index].density = pass_density;
                        
                        /* Select best pass (matching density with lowest errors) */
                        if (pass_density == (stored_density & NIB_DENSITY_MASK)) {
                            size_t errors = nib_check_track_errors(
                                img->all_passes[track][pass_index], 
                                NIB_TRACK_LENGTH, track / 2, img->base.disk_id);
                            
                            img->track_info[track].passes[pass_index].errors = errors;
                            
                            if (errors < best_errors || pass == 0) {
                                best_errors = errors;
                                best_pass = pass_index;
                            }
                        }
                    }
                }
            }
        }
        
        /* Use best pass as main track data */
        img->track_info[track].best_pass = best_pass;
        img->track_info[track].best_errors = best_errors;
        img->track_info[track].passes[best_pass].selected = true;
        
        if (img->all_passes[track][best_pass]) {
            img->base.track_data[track] = malloc(NIB_TRACK_LENGTH);
            if (img->base.track_data[track]) {
                memcpy(img->base.track_data[track], 
                       img->all_passes[track][best_pass], NIB_TRACK_LENGTH);
            }
            img->base.track_length[track] = NIB_TRACK_LENGTH;
        }
        
        header_entry++;
    }
    
    img->base.num_tracks = header_entry;
    
    *image = img;
    return 0;
}

/**
 * @brief Load NB2 file
 */
int nb2_load(const char *filename, nb2_image_t **image)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -2;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -3;
    }
    fclose(fp);
    
    int result = nb2_load_buffer(data, size, image);
    free(data);
    
    return result;
}

/**
 * @brief Get best pass info
 */
int nb2_get_best_pass(const nb2_image_t *image, int halftrack, nb2_pass_info_t *info)
{
    if (!image || halftrack < 0 || halftrack >= NIB_MAX_TRACKS) return -1;
    
    int best = image->track_info[halftrack].best_pass;
    
    if (info) {
        *info = image->track_info[halftrack].passes[best];
    }
    
    return best;
}

/**
 * @brief Get specific pass
 */
int nb2_get_pass(const nb2_image_t *image, int halftrack, int pass, const uint8_t **data)
{
    if (!image || halftrack < 0 || halftrack >= NIB_MAX_TRACKS) return -1;
    if (pass < 0 || pass >= NB2_PASSES_PER_TRACK) return -2;
    if (!image->all_passes[halftrack][pass]) return -3;
    
    if (data) *data = image->all_passes[halftrack][pass];
    
    return 0;
}

/**
 * @brief Free NB2 image
 */
void nb2_free(nb2_image_t *image)
{
    if (!image) return;
    
    for (int i = 0; i < NIB_MAX_TRACKS; i++) {
        free(image->base.track_data[i]);
        for (int j = 0; j < NB2_PASSES_PER_TRACK; j++) {
            free(image->all_passes[i][j]);
        }
    }
    
    free(image);
}

/* ============================================================================
 * NBZ Loading/Saving
 * ============================================================================ */

/**
 * @brief Load NBZ from buffer
 */
int nbz_load_buffer(const uint8_t *data, size_t size, nib_image_t **image)
{
    if (!data || !image || size == 0) return -1;
    
    /* Allocate decompression buffer (worst case: slightly larger than original) */
    size_t max_size = size * 2;  /* Generous estimate */
    if (max_size < NIB_HEADER_SIZE + NIB_MAX_TRACKS * NIB_TRACK_LENGTH) {
        max_size = NIB_HEADER_SIZE + NIB_MAX_TRACKS * NIB_TRACK_LENGTH;
    }
    
    uint8_t *decompressed = malloc(max_size);
    if (!decompressed) return -2;
    
    size_t decompressed_size = lz77_decompress(data, decompressed, size);
    if (decompressed_size == 0) {
        free(decompressed);
        return -3;
    }
    
    int result = nib_load_buffer(decompressed, decompressed_size, image);
    free(decompressed);
    
    return result;
}

/**
 * @brief Load NBZ file
 */
int nbz_load(const char *filename, nib_image_t **image)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -2;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -3;
    }
    fclose(fp);
    
    int result = nbz_load_buffer(data, size, image);
    free(data);
    
    return result;
}

/**
 * @brief Save NBZ to buffer
 */
int nbz_save_buffer(const nib_image_t *image, uint8_t **data, size_t *size)
{
    uint8_t *nib_data;
    size_t nib_size;
    
    int result = nib_save_buffer(image, &nib_data, &nib_size);
    if (result != 0) return result;
    
    /* Allocate compression buffer */
    size_t max_compressed = nib_size + (nib_size / 256) + 1;
    uint8_t *compressed = malloc(max_compressed);
    if (!compressed) {
        free(nib_data);
        return -2;
    }
    
    size_t compressed_size = lz77_compress_fast(nib_data, compressed, nib_size);
    free(nib_data);
    
    if (compressed_size == 0) {
        free(compressed);
        return -3;
    }
    
    *data = compressed;
    *size = compressed_size;
    
    return 0;
}

/**
 * @brief Save NBZ file
 */
int nbz_save(const char *filename, const nib_image_t *image)
{
    uint8_t *data;
    size_t size;
    
    int result = nbz_save_buffer(image, &data, &size);
    if (result != 0) return result;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(data);
        return -1;
    }
    
    if (fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        free(data);
        return -2;
    }
    
    fclose(fp);
    free(data);
    
    return 0;
}

/* ============================================================================
 * Analysis and Utilities
 * ============================================================================ */

/**
 * @brief Analyze NIB/NB2/NBZ file
 */
int nib_analyze(const char *filename, nib_analysis_t *result)
{
    if (!filename || !result) return -1;
    
    memset(result, 0, sizeof(nib_analysis_t));
    
    /* Get file size */
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    result->file_size = ftell(fp);
    fclose(fp);
    
    /* Detect format */
    result->format = nib_detect_format(filename);
    strncpy(result->format_name, nib_format_name(result->format), 
            sizeof(result->format_name) - 1);
    
    /* Load image based on format */
    nib_image_t *img = NULL;
    nb2_image_t *nb2_img = NULL;
    
    switch (result->format) {
        case NIB_FORMAT_NIB:
            if (nib_load(filename, &img) == 0) {
                result->version = img->header.version;
                result->num_tracks = img->num_tracks;
                result->has_halftracks = img->has_halftracks;
                memcpy(result->disk_id, img->disk_id, 2);
                result->uncompressed_size = result->file_size;
                result->compression_ratio = 1.0f;
                nib_free(img);
            }
            break;
            
        case NIB_FORMAT_NB2:
            if (nb2_load(filename, &nb2_img) == 0) {
                result->version = nb2_img->base.header.version;
                result->num_tracks = nb2_img->base.num_tracks;
                result->has_halftracks = true;
                memcpy(result->disk_id, nb2_img->base.disk_id, 2);
                result->uncompressed_size = result->file_size;
                result->compression_ratio = 1.0f;
                nb2_free(nb2_img);
            }
            break;
            
        case NIB_FORMAT_NBZ:
            if (nbz_load(filename, &img) == 0) {
                result->version = img->header.version;
                result->num_tracks = img->num_tracks;
                result->has_halftracks = img->has_halftracks;
                memcpy(result->disk_id, img->disk_id, 2);
                
                uint8_t *nib_data;
                size_t nib_size;
                if (nib_save_buffer(img, &nib_data, &nib_size) == 0) {
                    result->uncompressed_size = nib_size;
                    result->compression_ratio = (float)nib_size / result->file_size;
                    free(nib_data);
                }
                
                nib_free(img);
            }
            break;
            
        default:
            break;
    }
    
    /* Generate description */
    snprintf(result->description, sizeof(result->description),
             "%s v%d: %d tracks, %s, ID=%c%c",
             result->format_name, result->version, result->num_tracks,
             result->has_halftracks ? "halftracks" : "whole tracks",
             result->disk_id[0] ? result->disk_id[0] : '?',
             result->disk_id[1] ? result->disk_id[1] : '?');
    
    if (result->format == NIB_FORMAT_NBZ) {
        char comp_info[64];
        snprintf(comp_info, sizeof(comp_info), ", %.1fx compression", result->compression_ratio);
        strncat(result->description, comp_info, 
                sizeof(result->description) - strlen(result->description) - 1);
    }
    
    return 0;
}

/**
 * @brief Get format name
 */
const char *nib_format_name(nib_format_t format)
{
    if (format <= NIB_FORMAT_G64) {
        return format_names[format];
    }
    return "Unknown";
}

/**
 * @brief Extract disk ID from track 18
 */
bool nib_extract_disk_id(const uint8_t *track_data, size_t track_length, uint8_t *disk_id)
{
    if (!track_data || !disk_id || track_length < 256) return false;
    
    /* Look for sync + sector header pattern */
    for (size_t i = 0; i < track_length - 10; i++) {
        /* Find sync followed by header marker */
        if (track_data[i] == 0xFF && track_data[i+1] == 0xFF) {
            /* Skip to after sync */
            size_t j = i + 2;
            while (j < track_length && track_data[j] == 0xFF) j++;
            
            /* Check for header marker (GCR encoded 0x08) */
            if (j < track_length - 8 && (track_data[j] & 0xC0) == 0x40) {
                /* This is simplified - full implementation would decode GCR */
                /* For now, just look for typical ID patterns */
                continue;
            }
        }
    }
    
    /* Fallback: search for common ID patterns */
    disk_id[0] = '?';
    disk_id[1] = '?';
    
    return false;  /* Full implementation would decode GCR */
}

/**
 * @brief Check track for errors (simplified)
 */
size_t nib_check_track_errors(const uint8_t *track_data, size_t track_length, 
                              int track, const uint8_t *disk_id)
{
    if (!track_data || track_length == 0) return (size_t)-1;
    
    size_t errors = 0;
    
    /* Count bad GCR bytes (simplified check) */
    for (size_t i = 0; i + 1 < track_length; i++) {
        /* Check for obviously bad patterns */
        uint8_t byte = track_data[i];
        
        /* 4+ consecutive zeros in a row is bad GCR */
        if ((byte & 0xF0) == 0x00 || (byte & 0x0F) == 0x00) {
            errors++;
        }
    }
    
    return errors;
}

/**
 * @brief Generate analysis report
 */
size_t nib_generate_report(const nib_analysis_t *analysis, char *buffer, size_t buffer_size)
{
    if (!analysis || !buffer || buffer_size == 0) return 0;
    
    size_t written = snprintf(buffer, buffer_size,
        "=== NIB Format Analysis ===\n"
        "Format: %s\n"
        "Version: %d\n"
        "Tracks: %d\n"
        "Halftracks: %s\n"
        "Disk ID: %c%c\n"
        "File Size: %zu bytes\n",
        analysis->format_name,
        analysis->version,
        analysis->num_tracks,
        analysis->has_halftracks ? "Yes" : "No",
        analysis->disk_id[0] ? analysis->disk_id[0] : '?',
        analysis->disk_id[1] ? analysis->disk_id[1] : '?',
        analysis->file_size);
    
    if (analysis->format == NIB_FORMAT_NBZ) {
        written += snprintf(buffer + written, buffer_size - written,
            "Uncompressed Size: %zu bytes\n"
            "Compression Ratio: %.2fx\n",
            analysis->uncompressed_size,
            analysis->compression_ratio);
    }
    
    return written;
}
