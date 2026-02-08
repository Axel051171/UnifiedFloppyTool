/**
 * @file uft_woz_parser.c
 * @brief WOZ Format Parser Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "uft/flux/uft_woz_parser.h"

/*============================================================================
 * CRC32 Implementation
 *============================================================================*/

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void)
{
    if (crc32_table_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_init = true;
}

static uint32_t calc_crc32(const uint8_t* data, size_t size)
{
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

static uint32_t read_le32(const uint8_t* p)
{
    return p[0] | ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t* p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Find chunk in file
 */
static int find_chunk(const uint8_t* data, size_t size, uint32_t chunk_id)
{
    size_t offset = sizeof(uft_woz_header_t);
    
    while (offset + 8 <= size) {
        uint32_t id = read_le32(&data[offset]);
        uint32_t chunk_size = read_le32(&data[offset + 4]);
        
        if (id == chunk_id) {
            return (int)offset;
        }
        
        offset += 8 + chunk_size;
    }
    
    return -1;
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

uft_woz_ctx_t* uft_woz_create(void)
{
    uft_woz_ctx_t* ctx = calloc(1, sizeof(uft_woz_ctx_t));
    if (!ctx) return NULL;
    
    memset(ctx->tmap, 0xFF, sizeof(ctx->tmap));
    return ctx;
}

void uft_woz_destroy(uft_woz_ctx_t* ctx)
{
    if (!ctx) return;
    
    uft_woz_close(ctx);
    
    if (ctx->metadata) {
        free(ctx->metadata);
    }
    
    free(ctx);
}

/*============================================================================
 * File Operations
 *============================================================================*/

static int parse_woz(uft_woz_ctx_t* ctx)
{
    if (!ctx || !ctx->file_data) return UFT_WOZ_ERR_NULLPTR;
    
    /* Verify header */
    if (ctx->file_size < sizeof(uft_woz_header_t)) {
        return UFT_WOZ_ERR_FORMAT;
    }
    
    memcpy(&ctx->header, ctx->file_data, sizeof(uft_woz_header_t));
    
    /* Check signature */
    if (memcmp(ctx->header.signature, UFT_WOZ_SIGNATURE, 3) != 0) {
        return UFT_WOZ_ERR_SIGNATURE;
    }
    
    /* Check version */
    if (ctx->header.version != '1' && ctx->header.version != '2' && 
        ctx->header.version != '3') {
        return UFT_WOZ_ERR_VERSION;
    }
    
    ctx->woz_version = ctx->header.version - '0';
    
    /* Check high bit and line endings */
    if (ctx->header.high_bit != 0xFF ||
        ctx->header.lfcrlf[0] != 0x0A ||
        ctx->header.lfcrlf[1] != 0x0D ||
        ctx->header.lfcrlf[2] != 0x0A) {
        return UFT_WOZ_ERR_FORMAT;
    }
    
    /* Verify CRC */
    uint32_t calc_crc = calc_crc32(&ctx->file_data[12], ctx->file_size - 12);
    ctx->crc_valid = (calc_crc == ctx->header.crc32);
    
    /* Find INFO chunk */
    ctx->info_offset = find_chunk(ctx->file_data, ctx->file_size, UFT_WOZ_CHUNK_INFO);
    if (ctx->info_offset < 0) {
        return UFT_WOZ_ERR_CHUNK;
    }
    
    /* Parse INFO */
    const uint8_t* info_data = &ctx->file_data[ctx->info_offset + 8];
    ctx->info.version = info_data[0];
    ctx->info.disk_type = info_data[1];
    ctx->info.write_protected = info_data[2];
    ctx->info.synchronized = info_data[3];
    ctx->info.cleaned = info_data[4];
    memcpy(ctx->info.creator, &info_data[5], 32);
    
    if (ctx->woz_version >= 2) {
        ctx->info.sides = info_data[37];
        ctx->info.boot_sector_fmt = info_data[38];
        ctx->info.optimal_bit_timing = info_data[39];
        ctx->info.compatible_hw = read_le16(&info_data[40]);
        ctx->info.required_ram = read_le16(&info_data[42]);
        ctx->info.largest_track = read_le16(&info_data[44]);
    }
    
    if (ctx->woz_version >= 3) {
        ctx->info.flux_block = read_le16(&info_data[46]);
        ctx->info.largest_flux_track = read_le16(&info_data[48]);
    }
    
    ctx->has_info = true;
    
    /* Find TMAP chunk */
    ctx->tmap_offset = find_chunk(ctx->file_data, ctx->file_size, UFT_WOZ_CHUNK_TMAP);
    if (ctx->tmap_offset < 0) {
        return UFT_WOZ_ERR_CHUNK;
    }
    
    uint32_t tmap_size = read_le32(&ctx->file_data[ctx->tmap_offset + 4]);
    ctx->tmap_size = (tmap_size > UFT_WOZ_MAX_TRACKS) ? UFT_WOZ_MAX_TRACKS : tmap_size;
    memcpy(ctx->tmap, &ctx->file_data[ctx->tmap_offset + 8], ctx->tmap_size);
    ctx->has_tmap = true;
    
    /* Count tracks */
    ctx->track_count = 0;
    ctx->max_track = 0;
    for (int i = 0; i < ctx->tmap_size; i++) {
        if (ctx->tmap[i] != 0xFF) {
            ctx->track_count++;
            ctx->max_track = i;
        }
    }
    
    /* Find TRKS chunk */
    ctx->trks_offset = find_chunk(ctx->file_data, ctx->file_size, UFT_WOZ_CHUNK_TRKS);
    if (ctx->trks_offset < 0) {
        return UFT_WOZ_ERR_CHUNK;
    }
    
    /* Find META chunk (optional) */
    ctx->meta_offset = find_chunk(ctx->file_data, ctx->file_size, UFT_WOZ_CHUNK_META);
    
    /* Find FLUX chunk (v3 only, optional) */
    if (ctx->woz_version >= 3) {
        ctx->flux_offset = find_chunk(ctx->file_data, ctx->file_size, UFT_WOZ_CHUNK_FLUX);
    }
    
    return UFT_WOZ_OK;
}

int uft_woz_open(uft_woz_ctx_t* ctx, const char* filename)
{
    if (!ctx || !filename) return UFT_WOZ_ERR_NULLPTR;
    
    uft_woz_close(ctx);
    
    FILE* f = fopen(filename, "rb");
    if (!f) {
        ctx->last_error = UFT_WOZ_ERR_OPEN;
        return UFT_WOZ_ERR_OPEN;
    }
    
    /* Get file size */
    (void)fseek(f, 0, SEEK_END);
    ctx->file_size = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    
    /* Allocate and read */
    ctx->file_data = malloc(ctx->file_size);
    if (!ctx->file_data) {
        fclose(f);
        ctx->last_error = UFT_WOZ_ERR_MEMORY;
        return UFT_WOZ_ERR_MEMORY;
    }
    
    if (fread(ctx->file_data, 1, ctx->file_size, f) != ctx->file_size) {
        free(ctx->file_data);
        ctx->file_data = NULL;
        fclose(f);
        ctx->last_error = UFT_WOZ_ERR_READ;
        return UFT_WOZ_ERR_READ;
    }
    
    fclose(f);
    
    return parse_woz(ctx);
}

int uft_woz_open_memory(uft_woz_ctx_t* ctx, const uint8_t* data, size_t size)
{
    if (!ctx || !data || size == 0) return UFT_WOZ_ERR_NULLPTR;
    
    uft_woz_close(ctx);
    
    ctx->file_data = malloc(size);
    if (!ctx->file_data) {
        ctx->last_error = UFT_WOZ_ERR_MEMORY;
        return UFT_WOZ_ERR_MEMORY;
    }
    
    memcpy(ctx->file_data, data, size);
    ctx->file_size = size;
    
    return parse_woz(ctx);
}

void uft_woz_close(uft_woz_ctx_t* ctx)
{
    if (!ctx) return;
    
    if (ctx->file_data) {
        free(ctx->file_data);
        ctx->file_data = NULL;
    }
    
    if (ctx->file) {
        fclose(ctx->file);
        ctx->file = NULL;
    }
    
    ctx->file_size = 0;
    ctx->has_info = false;
    ctx->has_tmap = false;
    ctx->track_count = 0;
    memset(ctx->tmap, 0xFF, sizeof(ctx->tmap));
}

/*============================================================================
 * Track Reading
 *============================================================================*/

int uft_woz_get_track_count(uft_woz_ctx_t* ctx)
{
    return ctx ? ctx->track_count : 0;
}

bool uft_woz_has_track(uft_woz_ctx_t* ctx, int quarter_track)
{
    if (!ctx || quarter_track < 0 || quarter_track >= ctx->tmap_size) {
        return false;
    }
    return ctx->tmap[quarter_track] != 0xFF;
}

int uft_woz_read_track(uft_woz_ctx_t* ctx, int quarter_track, uft_woz_track_t* track)
{
    if (!ctx || !track) return UFT_WOZ_ERR_NULLPTR;
    if (!ctx->file_data) return UFT_WOZ_ERR_FORMAT;
    if (quarter_track < 0 || quarter_track >= ctx->tmap_size) return UFT_WOZ_ERR_TRACK;
    
    memset(track, 0, sizeof(*track));
    track->splice_point = 0xFFFF;
    
    uint8_t trk_idx = ctx->tmap[quarter_track];
    if (trk_idx == 0xFF) {
        return UFT_WOZ_ERR_TRACK;
    }
    
    track->track_index = quarter_track;
    track->quarter_track = quarter_track;
    track->physical_track = quarter_track / 4;
    track->side = 0;  /* For 3.5" this would be different */
    
    const uint8_t* trks_data = &ctx->file_data[ctx->trks_offset + 8];
    
    if (ctx->woz_version == 1) {
        /* v1: Fixed-size track entries */
        const uft_woz_trk_v1_t* trk_v1 = (const uft_woz_trk_v1_t*)&trks_data[trk_idx * sizeof(uft_woz_trk_v1_t)];
        
        track->bit_count = trk_v1->bit_count;
        track->byte_count = (track->bit_count + 7) / 8;
        track->splice_point = trk_v1->splice_point;
        track->splice_nibble = trk_v1->splice_nibble;
        
        /* Allocate and copy bitstream */
        track->bitstream = malloc(track->byte_count);
        if (!track->bitstream) {
            return UFT_WOZ_ERR_MEMORY;
        }
        memcpy(track->bitstream, trk_v1->bitstream, track->byte_count);
        
    } else {
        /* v2/v3: Variable-size track entries */
        const uft_woz_trk_v2_t* trk_v2 = (const uft_woz_trk_v2_t*)&trks_data[trk_idx * sizeof(uft_woz_trk_v2_t)];
        
        track->bit_count = trk_v2->bit_count;
        track->byte_count = (track->bit_count + 7) / 8;
        
        /* Calculate offset */
        uint32_t data_offset = trk_v2->starting_block * UFT_WOZ_BLOCK_SIZE;
        uint32_t data_size = trk_v2->block_count * UFT_WOZ_BLOCK_SIZE;
        
        if (data_offset + data_size > ctx->file_size) {
            return UFT_WOZ_ERR_FORMAT;
        }
        
        /* Allocate and copy bitstream */
        track->bitstream = malloc(track->byte_count);
        if (!track->bitstream) {
            return UFT_WOZ_ERR_MEMORY;
        }
        memcpy(track->bitstream, &ctx->file_data[data_offset], track->byte_count);
    }
    
    track->valid = true;
    return UFT_WOZ_OK;
}

int uft_woz_read_flux(uft_woz_ctx_t* ctx, int quarter_track, uft_woz_track_t* track)
{
    if (!ctx || !track) return UFT_WOZ_ERR_NULLPTR;
    if (ctx->woz_version < 3) return UFT_WOZ_ERR_VERSION;
    if (ctx->flux_offset < 0) return UFT_WOZ_ERR_CHUNK;
    
    /* First read the regular track */
    int err = uft_woz_read_track(ctx, quarter_track, track);
    if (err != UFT_WOZ_OK) return err;
    
    /* Check if flux data exists for this track */
    if (ctx->info.flux_block == 0) {
        return UFT_WOZ_OK;  /* No flux data */
    }
    
    /* Parse FLUX chunk data for this track.
     * WOZ v3 flux data is stored at flux_block, one FLUX entry per track.
     * Each entry starts at (flux_block + track_entry.starting_block) * 512.
     * Data consists of 16-bit LE timing values (in 125ns units).
     * A value of 0x0000 marks end of flux data for the track. */
    {
        /* Look up flux track entry (same TRK map, separate block range) */
        int flux_block_start = ctx->info.flux_block;
        size_t flux_base = (size_t)flux_block_start * 512;
        
        /* Read flux timing values */
        if (flux_base < ctx->file_size) {
            /* Track flux entries follow after main FLUX header */
            size_t track_flux_off = flux_base + (size_t)quarter_track * 8;
            if (track_flux_off + 8 <= ctx->file_size) {
                uint16_t start_block = read_le16(&ctx->file_data[track_flux_off]);
                uint16_t block_count = read_le16(&ctx->file_data[track_flux_off + 2]);
                
                if (start_block > 0 && block_count > 0) {
                    size_t data_off = (size_t)start_block * 512;
                    size_t data_len = (size_t)block_count * 512;
                    
                    if (data_off + data_len <= ctx->file_size) {
                        /* Count flux transitions */
                        uint32_t count = 0;
                        for (size_t i = 0; i + 1 < data_len; i += 2) {
                            uint16_t val = read_le16(&ctx->file_data[data_off + i]);
                            if (val == 0) break;
                            count++;
                        }
                        
                        if (count > 0) {
                            track->flux_data = malloc(count * sizeof(uint32_t));
                            if (track->flux_data) {
                                track->flux_count = count;
                                for (uint32_t i = 0; i < count; i++) {
                                    /* Convert 16-bit LE values to 32-bit timing */
                                    track->flux_data[i] = read_le16(&ctx->file_data[data_off + i * 2]);
                                }
                                track->has_flux = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (!track->has_flux) track->has_flux = false;
    return UFT_WOZ_OK;
}

void uft_woz_free_track(uft_woz_track_t* track)
{
    if (!track) return;
    
    if (track->bitstream) {
        free(track->bitstream);
        track->bitstream = NULL;
    }
    
    if (track->flux_data) {
        free(track->flux_data);
        track->flux_data = NULL;
    }
    
    memset(track, 0, sizeof(*track));
}

/*============================================================================
 * Metadata
 *============================================================================*/

const char* uft_woz_get_metadata(uft_woz_ctx_t* ctx, const char* key)
{
    if (!ctx || !key || ctx->meta_offset < 0) return NULL;
    
    /* Parse metadata on demand */
    if (!ctx->metadata && ctx->meta_offset > 0) {
        uint32_t meta_size = read_le32(&ctx->file_data[ctx->meta_offset + 4]);
        if (meta_size > 0 && meta_size < 1024 * 1024) {
            /* WOZ META chunk is UTF-8 text with tab-separated key\tvalue pairs,
             * separated by line feed (0x0A). */
            const char *meta_text = (const char *)&ctx->file_data[ctx->meta_offset + 8];
            
            /* Count entries first */
            int count = 0;
            for (size_t i = 0; i < meta_size; i++) {
                if (meta_text[i] == '\n') count++;
            }
            if (meta_size > 0 && meta_text[meta_size - 1] != '\n') count++;
            
            ctx->metadata = calloc(count, sizeof(uft_woz_meta_t));
            if (ctx->metadata) {
                ctx->meta_count = 0;
                size_t pos = 0;
                
                while (pos < meta_size && ctx->meta_count < count) {
                    /* Find tab separator */
                    size_t tab_pos = pos;
                    while (tab_pos < meta_size && meta_text[tab_pos] != '\t') tab_pos++;
                    
                    /* Find end of line */
                    size_t eol_pos = tab_pos;
                    while (eol_pos < meta_size && meta_text[eol_pos] != '\n') eol_pos++;
                    
                    if (tab_pos > pos && tab_pos < meta_size) {
                        uft_woz_meta_t *m = &ctx->metadata[ctx->meta_count];
                        size_t key_len = tab_pos - pos;
                        size_t val_len = eol_pos - (tab_pos + 1);
                        
                        if (key_len >= sizeof(m->key)) key_len = sizeof(m->key) - 1;
                        if (val_len >= sizeof(m->value)) val_len = sizeof(m->value) - 1;
                        
                        memcpy(m->key, meta_text + pos, key_len);
                        m->key[key_len] = '\0';
                        if (tab_pos + 1 < eol_pos) {
                            memcpy(m->value, meta_text + tab_pos + 1, val_len);
                        }
                        m->value[val_len] = '\0';
                        ctx->meta_count++;
                    }
                    
                    pos = eol_pos + 1;
                }
            }
        }
    }
    
    /* Search for key */
    for (int i = 0; i < ctx->meta_count; i++) {
        if (strcmp(ctx->metadata[i].key, key) == 0) {
            return ctx->metadata[i].value;
        }
    }
    
    return NULL;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char* uft_woz_disk_type_name(uint8_t disk_type)
{
    switch (disk_type) {
        case UFT_WOZ_DISK_525: return "5.25\" Apple II";
        case UFT_WOZ_DISK_35:  return "3.5\" Macintosh";
        default:              return "Unknown";
    }
}

void uft_woz_hw_names(uint16_t flags, char* buffer, size_t size)
{
    if (!buffer || size == 0) return;
    
    buffer[0] = '\0';
    size_t pos = 0;
    
    const struct { uint16_t flag; const char* name; } hw_list[] = {
        { UFT_WOZ_HW_APPLE_II,       "Apple ]["      },
        { UFT_WOZ_HW_APPLE_II_PLUS,  "Apple ][+"     },
        { UFT_WOZ_HW_APPLE_IIE,      "Apple //e"     },
        { UFT_WOZ_HW_APPLE_IIC,      "Apple //c"     },
        { UFT_WOZ_HW_APPLE_IIE_ENH,  "Apple //e enh" },
        { UFT_WOZ_HW_APPLE_IIGS,     "Apple IIgs"    },
        { UFT_WOZ_HW_APPLE_IIC_PLUS, "Apple //c+"    },
        { UFT_WOZ_HW_APPLE_III,      "Apple ///"     },
        { UFT_WOZ_HW_APPLE_III_PLUS, "Apple ///+"    },
        { 0, NULL }
    };
    
    for (int i = 0; hw_list[i].name; i++) {
        if (flags & hw_list[i].flag) {
            if (pos > 0 && pos + 2 < size) {
                buffer[pos++] = ',';
                buffer[pos++] = ' ';
            }
            size_t len = strlen(hw_list[i].name);
            if (pos + len < size) {
                memcpy(&buffer[pos], hw_list[i].name, len);
                pos += len;
            }
        }
    }
    buffer[pos] = '\0';
}

uint32_t uft_woz_bit_timing_ns(uint8_t bit_timing)
{
    /* Default is 32 (4µs), value is in 125ns increments */
    if (bit_timing == 0) bit_timing = 32;
    return bit_timing * 125;
}

bool uft_woz_verify_crc(uft_woz_ctx_t* ctx)
{
    if (!ctx || !ctx->file_data) return false;
    return ctx->crc_valid;
}

int uft_woz_decode_nibbles(const uint8_t* bitstream, uint32_t bit_count,
                           uint8_t* nibbles, int max_nibbles)
{
    if (!bitstream || !nibbles || max_nibbles == 0) return 0;
    
    int nibble_count = 0;
    uint32_t bit_pos = 0;
    uint8_t shift_reg = 0;
    int bits_in_reg = 0;
    
    /* Apple II nibbles are 8-bit values with high bit set */
    while (bit_pos < bit_count && nibble_count < max_nibbles) {
        /* Get next bit */
        int byte_idx = bit_pos / 8;
        int bit_idx = 7 - (bit_pos % 8);
        uint8_t bit = (bitstream[byte_idx] >> bit_idx) & 1;
        bit_pos++;
        
        /* Shift into register */
        shift_reg = (shift_reg << 1) | bit;
        bits_in_reg++;
        
        /* Check for valid nibble (high bit set) */
        if (bits_in_reg >= 8 && (shift_reg & 0x80)) {
            nibbles[nibble_count++] = shift_reg;
            shift_reg = 0;
            bits_in_reg = 0;
        }
        
        /* Limit shift register */
        if (bits_in_reg > 10) {
            shift_reg = 0;
            bits_in_reg = 0;
        }
    }
    
    return nibble_count;
}
