/**
 * @file uft_scp_parser.c
 * @brief SuperCard Pro (SCP) Format Parser Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "uft/flux/uft_scp_parser.h"

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * @brief Read 16-bit big-endian value
 */
static uint16_t read_be16(const uint8_t* p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

/**
 * @brief Read 32-bit little-endian value
 */
static uint32_t read_le32(const uint8_t* p)
{
    return p[0] | ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

uft_scp_ctx_t* uft_scp_create(void)
{
    uft_scp_ctx_t* ctx = calloc(1, sizeof(uft_scp_ctx_t));
    if (!ctx) return NULL;
    
    ctx->period_ns = UFT_SCP_BASE_PERIOD_NS;
    return ctx;
}

void uft_scp_destroy(uft_scp_ctx_t* ctx)
{
    if (!ctx) return;
    
    uft_scp_close(ctx);
    
    if (ctx->creator_string) free(ctx->creator_string);
    if (ctx->app_name) free(ctx->app_name);
    
    free(ctx);
}

/*============================================================================
 * File Operations
 *============================================================================*/

int uft_scp_open(uft_scp_ctx_t* ctx, const char* filename)
{
    if (!ctx || !filename) return UFT_SCP_ERR_NULLPTR;
    
    /* Close any existing file */
    uft_scp_close(ctx);
    
    /* Open file */
    ctx->file = fopen(filename, "rb");
    if (!ctx->file) {
        ctx->last_error = UFT_SCP_ERR_OPEN;
        return UFT_SCP_ERR_OPEN;
    }
    
    /* Get file size */
    (void)fseek(ctx->file, 0, SEEK_END);
    ctx->file_size = ftell(ctx->file);
    (void)fseek(ctx->file, 0, SEEK_SET);
    
    /* Read header */
    if (fread(&ctx->header, sizeof(uft_scp_header_t), 1, ctx->file) != 1) {
        fclose(ctx->file);
        ctx->file = NULL;
        ctx->last_error = UFT_SCP_ERR_READ;
        return UFT_SCP_ERR_READ;
    }
    
    /* Verify signature */
    if (memcmp(ctx->header.signature, UFT_SCP_SIGNATURE, 3) != 0) {
        fclose(ctx->file);
        ctx->file = NULL;
        ctx->last_error = UFT_SCP_ERR_SIGNATURE;
        return UFT_SCP_ERR_SIGNATURE;
    }
    
    /* Parse version */
    ctx->version_major = ctx->header.version >> 4;
    ctx->version_minor = ctx->header.version & 0x0F;
    
    /* Parse disk type */
    ctx->manufacturer = ctx->header.disk_type & 0xF0;
    ctx->disk_subtype = ctx->header.disk_type & 0x0F;
    
    /* Parse resolution */
    ctx->period_ns = UFT_SCP_BASE_PERIOD_NS * (1 + ctx->header.resolution);
    
    /* Read track offset table */
    uint8_t offset_table[UFT_SCP_MAX_TRACKS * 4];
    if (fread(offset_table, 4, UFT_SCP_MAX_TRACKS, ctx->file) != UFT_SCP_MAX_TRACKS) {
        fclose(ctx->file);
        ctx->file = NULL;
        ctx->last_error = UFT_SCP_ERR_READ;
        return UFT_SCP_ERR_READ;
    }
    
    /* Parse offsets and count tracks */
    ctx->track_count = 0;
    for (int i = 0; i < UFT_SCP_MAX_TRACKS; i++) {
        ctx->track_offsets[i] = read_le32(&offset_table[i * 4]);
        if (ctx->track_offsets[i] != 0) {
            ctx->track_count++;
        }
    }
    
    /* Check for footer */
    if (ctx->header.flags & UFT_SCP_FLAG_FOOTER) {
        ctx->has_footer = true;
        /* Footer reading would go here */
    }
    
    return UFT_SCP_OK;
}

int uft_scp_open_memory(uft_scp_ctx_t* ctx, const uint8_t* data, size_t size)
{
    if (!ctx || !data || size < sizeof(uft_scp_header_t) + UFT_SCP_MAX_TRACKS * 4) {
        return UFT_SCP_ERR_NULLPTR;
    }
    
    /* Parse header */
    memcpy(&ctx->header, data, sizeof(uft_scp_header_t));
    
    /* Verify signature */
    if (memcmp(ctx->header.signature, UFT_SCP_SIGNATURE, 3) != 0) {
        ctx->last_error = UFT_SCP_ERR_SIGNATURE;
        return UFT_SCP_ERR_SIGNATURE;
    }
    
    /* Parse version */
    ctx->version_major = ctx->header.version >> 4;
    ctx->version_minor = ctx->header.version & 0x0F;
    
    /* Parse disk type */
    ctx->manufacturer = ctx->header.disk_type & 0xF0;
    ctx->disk_subtype = ctx->header.disk_type & 0x0F;
    
    /* Parse resolution */
    ctx->period_ns = UFT_SCP_BASE_PERIOD_NS * (1 + ctx->header.resolution);
    
    /* Parse track offsets */
    const uint8_t* offset_table = data + sizeof(uft_scp_header_t);
    ctx->track_count = 0;
    for (int i = 0; i < UFT_SCP_MAX_TRACKS; i++) {
        ctx->track_offsets[i] = read_le32(&offset_table[i * 4]);
        if (ctx->track_offsets[i] != 0) {
            ctx->track_count++;
        }
    }
    
    ctx->file_size = size;
    return UFT_SCP_OK;
}

void uft_scp_close(uft_scp_ctx_t* ctx)
{
    if (!ctx) return;
    
    if (ctx->file) {
        fclose(ctx->file);
        ctx->file = NULL;
    }
    
    memset(&ctx->header, 0, sizeof(ctx->header));
    memset(ctx->track_offsets, 0, sizeof(ctx->track_offsets));
    ctx->track_count = 0;
    ctx->file_size = 0;
}

/*============================================================================
 * Track Reading
 *============================================================================*/

int uft_scp_get_track_count(uft_scp_ctx_t* ctx)
{
    return ctx ? ctx->track_count : 0;
}

bool uft_scp_has_track(uft_scp_ctx_t* ctx, int track)
{
    if (!ctx || track < 0 || track >= UFT_SCP_MAX_TRACKS) {
        return false;
    }
    return ctx->track_offsets[track] != 0;
}

int uft_scp_read_track(uft_scp_ctx_t* ctx, int track, uft_scp_track_data_t* data)
{
    if (!ctx || !data) return UFT_SCP_ERR_NULLPTR;
    if (!ctx->file) return UFT_SCP_ERR_OPEN;
    if (track < 0 || track >= UFT_SCP_MAX_TRACKS) return UFT_SCP_ERR_TRACK;
    if (ctx->track_offsets[track] == 0) return UFT_SCP_ERR_TRACK;
    
    memset(data, 0, sizeof(*data));
    
    /* Seek to track */
    if (fseek(ctx->file, ctx->track_offsets[track], SEEK_SET) != 0) {
        return UFT_SCP_ERR_READ;
    }
    
    /* Read track header */
    uft_scp_track_header_t track_hdr;
    if (fread(&track_hdr, sizeof(track_hdr), 1, ctx->file) != 1) {
        return UFT_SCP_ERR_READ;
    }
    
    /* Verify track signature */
    if (memcmp(track_hdr.signature, UFT_SCP_TRACK_SIG, 3) != 0) {
        return UFT_SCP_ERR_SIGNATURE;
    }
    
    data->track_number = track_hdr.track_number;
    data->side = track_hdr.track_number & 1;
    data->revolution_count = ctx->header.revolutions;
    data->valid = true;
    
    /* Read revolution entries */
    for (int r = 0; r < ctx->header.revolutions && r < UFT_SCP_MAX_REVOLUTIONS; r++) {
        uft_scp_revolution_t rev_info;
        if (fread(&rev_info, sizeof(rev_info), 1, ctx->file) != 1) {
            return UFT_SCP_ERR_READ;
        }
        
        /* Convert index time to nanoseconds */
        data->revolutions[r].index_time_ns = rev_info.index_time * ctx->period_ns;
        data->revolutions[r].flux_count = rev_info.track_length;
        data->revolutions[r].rpm = uft_scp_calculate_rpm(data->revolutions[r].index_time_ns);
        
        /* Allocate flux data buffer */
        if (rev_info.track_length > 0) {
            data->revolutions[r].flux_data = malloc(rev_info.track_length * sizeof(uint32_t));
            if (!data->revolutions[r].flux_data) {
                uft_scp_free_track(data);
                return UFT_SCP_ERR_MEMORY;
            }
            
            /* Save position and read flux data */
            long saved_pos = ftell(ctx->file);
            long flux_offset = ctx->track_offsets[track] + rev_info.data_offset;
            
            if (fseek(ctx->file, flux_offset, SEEK_SET) != 0) {
                uft_scp_free_track(data);
                return UFT_SCP_ERR_READ;
            }
            
            /* Read 16-bit big-endian flux values */
            uint8_t* raw_flux = malloc(rev_info.track_length * 2);
            if (!raw_flux) {
                uft_scp_free_track(data);
                return UFT_SCP_ERR_MEMORY;
            }
            
            if (fread(raw_flux, 2, rev_info.track_length, ctx->file) != rev_info.track_length) {
                free(raw_flux);
                uft_scp_free_track(data);
                return UFT_SCP_ERR_READ;
            }
            
            /* Convert to nanoseconds, handling overflow (0x0000) */
            uint32_t overflow_acc = 0;
            for (uint32_t i = 0; i < rev_info.track_length; i++) {
                uint16_t raw = read_be16(&raw_flux[i * 2]);
                if (raw == 0) {
                    /* Overflow - accumulate 65536 */
                    overflow_acc += 65536;
                    data->revolutions[r].flux_data[i] = 0;  /* Placeholder */
                } else {
                    /* Normal value + any accumulated overflow */
                    data->revolutions[r].flux_data[i] = 
                        (overflow_acc + raw) * ctx->period_ns;
                    overflow_acc = 0;
                }
            }
            
            free(raw_flux);
            
            /* Restore position */
            (void)fseek(ctx->file, saved_pos, SEEK_SET);
        }
    }
    
    return UFT_SCP_OK;
}

void uft_scp_free_track(uft_scp_track_data_t* data)
{
    if (!data) return;
    
    for (int r = 0; r < UFT_SCP_MAX_REVOLUTIONS; r++) {
        if (data->revolutions[r].flux_data) {
            free(data->revolutions[r].flux_data);
            data->revolutions[r].flux_data = NULL;
        }
    }
    
    memset(data, 0, sizeof(*data));
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char* uft_scp_disk_type_name(uint8_t disk_type)
{
    uint8_t mfg = disk_type & 0xF0;
    uint8_t type = disk_type & 0x0F;
    
    switch (mfg) {
        case UFT_SCP_MAN_CBM:
            switch (type) {
                case 0x00: return "Commodore 64";
                case 0x04: return "Amiga";
                default: return "Commodore (Unknown)";
            }
        case UFT_SCP_MAN_ATARI:
            switch (type) {
                case 0x00: return "Atari FM SS";
                case 0x01: return "Atari FM DS";
                case 0x04: return "Atari ST SS";
                case 0x05: return "Atari ST DS";
                default: return "Atari (Unknown)";
            }
        case UFT_SCP_MAN_APPLE:
            switch (type) {
                case 0x00: return "Apple II";
                case 0x01: return "Apple II ProDOS";
                case 0x04: return "Macintosh 400K";
                case 0x05: return "Macintosh 800K";
                case 0x06: return "Macintosh 1.44MB";
                default: return "Apple (Unknown)";
            }
        case UFT_SCP_MAN_PC:
            switch (type) {
                case 0x00: return "PC 360KB";
                case 0x01: return "PC 720KB";
                case 0x02: return "PC 1.2MB";
                case 0x03: return "PC 1.44MB";
                default: return "PC (Unknown)";
            }
        case UFT_SCP_MAN_TANDY:
            switch (type) {
                case 0x00: return "TRS-80 SS/SD";
                case 0x01: return "TRS-80 SS/DD";
                case 0x02: return "TRS-80 DS/SD";
                case 0x03: return "TRS-80 DS/DD";
                default: return "TRS-80 (Unknown)";
            }
        case UFT_SCP_MAN_TI:
            return "TI-99/4A";
        case UFT_SCP_MAN_ROLAND:
            return "Roland D-20";
        case UFT_SCP_MAN_OTHER:
            return "Other";
        default:
            return "Unknown";
    }
}

const char* uft_scp_manufacturer_name(uint8_t disk_type)
{
    switch (disk_type & 0xF0) {
        case UFT_SCP_MAN_CBM:    return "Commodore";
        case UFT_SCP_MAN_ATARI:  return "Atari";
        case UFT_SCP_MAN_APPLE:  return "Apple";
        case UFT_SCP_MAN_PC:     return "IBM PC";
        case UFT_SCP_MAN_TANDY:  return "Tandy";
        case UFT_SCP_MAN_TI:     return "Texas Instruments";
        case UFT_SCP_MAN_ROLAND: return "Roland";
        case UFT_SCP_MAN_OTHER:  return "Other";
        default:                 return "Unknown";
    }
}

uint32_t uft_scp_calculate_rpm(uint32_t index_time_ns)
{
    if (index_time_ns == 0) return 0;
    /* RPM = 60 seconds / rotation time */
    return (uint32_t)(60000000000ULL / index_time_ns);
}

uint32_t uft_scp_flux_to_ns(uft_scp_ctx_t* ctx, uint16_t flux_value)
{
    if (!ctx) return flux_value * UFT_SCP_BASE_PERIOD_NS;
    return flux_value * ctx->period_ns;
}

bool uft_scp_verify_checksum(uft_scp_ctx_t* ctx)
{
    if (!ctx || !ctx->file) return false;
    if (ctx->header.flags & UFT_SCP_FLAG_RW) return true;  /* No checksum for R/W */
    
    /* Save position */
    long saved_pos = ftell(ctx->file);
    
    /* Calculate checksum from offset 0x10 to end */
    (void)fseek(ctx->file, 0x10, SEEK_SET);
    
    uint32_t checksum = 0;
    uint8_t buffer[4096];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            checksum += buffer[i];
        }
    }
    
    /* Restore position */
    (void)fseek(ctx->file, saved_pos, SEEK_SET);
    
    return checksum == ctx->header.checksum;
}
