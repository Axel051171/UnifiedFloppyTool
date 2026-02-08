/**
 * @file uft_mfm.c
 * @brief HxC MFM Format Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_mfm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

struct uft_mfm_context {
    FILE *file;
    char *path;
    uft_mfm_header_t header;
    uft_mfm_track_t *tracks;
    int num_track_entries;
    bool writable;
};

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

bool uft_mfm_probe(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    char sig[8];
    bool valid = (fread(sig, 1, 8, f) == 8 && 
                  memcmp(sig, "HXCMFM", 6) == 0);
    fclose(f);
    
    return valid;
}

uft_mfm_context_t* uft_mfm_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uft_mfm_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
    if (ferror(f)) {
        fclose(f);
        return NULL;
    }
                fclose(f);
        return NULL;
    }
    
    /* Verify signature */
    if (memcmp(header.signature, "HXCMFM", 6) != 0) {
    if (ferror(f)) {
        fclose(f);
        return NULL;
    }
                fclose(f);
        return NULL;
    }
    
    /* Allocate context */
    uft_mfm_context_t *ctx = (uft_mfm_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) {
    if (ferror(f)) {
        fclose(f);
        return NULL;
    }
                fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    ctx->path = strdup(path);
    ctx->header = header;
    ctx->num_track_entries = header.num_tracks * header.num_sides;
    
    /* Read track list */
    if (header.track_list_offset > 0 && ctx->num_track_entries > 0) {
        ctx->tracks = (uft_mfm_track_t *)calloc(ctx->num_track_entries, 
                                                 sizeof(uft_mfm_track_t));
        if (ctx->tracks) {
            if (fseek(f, header.track_list_offset, SEEK_SET) != 0) return NULL;
            fread(ctx->tracks, sizeof(uft_mfm_track_t), 
                  ctx->num_track_entries, f);
        }
    }
    
    return ctx;
}

uft_mfm_context_t* uft_mfm_create(const char *path,
                                   int num_tracks, int num_sides,
                                   int rpm, uint32_t bitrate) {
    if (!path || num_tracks <= 0 || num_sides <= 0) return NULL;
    
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    
    uft_mfm_context_t *ctx = (uft_mfm_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) {
    if (ferror(f)) {
        fclose(f);
        return NULL;
    }
                fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    ctx->path = strdup(path);
    ctx->writable = true;
    ctx->num_track_entries = num_tracks * num_sides;
    
    /* Initialize header */
    memcpy(ctx->header.signature, "HXCMFM\0\0", 8);
    ctx->header.format_revision = 1;
    ctx->header.num_tracks = num_tracks;
    ctx->header.num_sides = num_sides;
    ctx->header.rpm = rpm;
    ctx->header.bitrate = bitrate;
    ctx->header.track_encoding = UFT_MFM_ENC_MFM;
    ctx->header.interface_mode = UFT_MFM_IF_IBM_PC;
    ctx->header.track_list_offset = sizeof(uft_mfm_header_t);
    
    /* Allocate track list */
    ctx->tracks = (uft_mfm_track_t *)calloc(ctx->num_track_entries,
                                             sizeof(uft_mfm_track_t));
    
    /* Write header (will be updated on close) */
    fwrite(&ctx->header, sizeof(ctx->header), 1, f);
    
    /* Write placeholder track list */
    if (ctx->tracks) {
        for (int t = 0; t < num_tracks; t++) {
            for (int s = 0; s < num_sides; s++) {
                int idx = t * num_sides + s;
                ctx->tracks[idx].track_number = t;
                ctx->tracks[idx].side_number = s;
            }
        }
        fwrite(ctx->tracks, sizeof(uft_mfm_track_t), 
               ctx->num_track_entries, f);
    }
    
    return ctx;
}

void uft_mfm_close(uft_mfm_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->writable && ctx->file) {
        /* Update header and track list */
        if (fseek(ctx->file, 0, SEEK_SET) != 0) return -1;
        fwrite(&ctx->header, sizeof(ctx->header), 1, ctx->file);
        
        if (ctx->tracks) {
            if (fseek(ctx->file, ctx->header.track_list_offset, SEEK_SET) != 0) return -1;
            fwrite(ctx->tracks, sizeof(uft_mfm_track_t),
                   ctx->num_track_entries, ctx->file);
        }
    }
    
    if (ctx->file) fclose(ctx->file);
    free(ctx->tracks);
    free(ctx->path);
    free(ctx);
}

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

const uft_mfm_header_t* uft_mfm_get_header(uft_mfm_context_t *ctx) {
    return ctx ? &ctx->header : NULL;
}

int uft_mfm_get_num_tracks(uft_mfm_context_t *ctx) {
    return ctx ? ctx->header.num_tracks : 0;
}

int uft_mfm_get_num_sides(uft_mfm_context_t *ctx) {
    return ctx ? ctx->header.num_sides : 0;
}

const uft_mfm_track_t* uft_mfm_get_track_info(uft_mfm_context_t *ctx,
                                               int track, int side) {
    if (!ctx || !ctx->tracks) return NULL;
    if (track < 0 || track >= ctx->header.num_tracks) return NULL;
    if (side < 0 || side >= ctx->header.num_sides) return NULL;
    
    int idx = track * ctx->header.num_sides + side;
    return &ctx->tracks[idx];
}

/*===========================================================================
 * TRACK I/O
 *===========================================================================*/

int uft_mfm_read_track(uft_mfm_context_t *ctx, int track, int side,
                        uint8_t *data, size_t max_size) {
    if (!ctx || !ctx->file || !data) return -1;
    
    const uft_mfm_track_t *ti = uft_mfm_get_track_info(ctx, track, side);
    if (!ti || ti->data_offset == 0 || ti->data_length == 0) return -1;
    
    size_t to_read = ti->data_length;
    if (to_read > max_size) to_read = max_size;
    
    if (fseek(ctx->file, ti->data_offset, SEEK_SET) != 0) return -1;
    return (int)fread(data, 1, to_read, ctx->file);
}

int uft_mfm_write_track(uft_mfm_context_t *ctx, int track, int side,
                         const uint8_t *data, size_t size) {
    if (!ctx || !ctx->file || !ctx->writable || !data) return -1;
    if (!ctx->tracks) return -1;
    
    int idx = track * ctx->header.num_sides + side;
    if (idx < 0 || idx >= ctx->num_track_entries) return -1;
    
    /* Seek to end for new data */
    if (fseek(ctx->file, 0, SEEK_END) != 0) return -1;
    uint32_t offset = (uint32_t)ftell(ctx->file);
    
    /* Write track data */
    size_t written = fwrite(data, 1, size, ctx->file);
    if (written != size) return -1;
    
    /* Update track entry */
    ctx->tracks[idx].track_number = track;
    ctx->tracks[idx].side_number = side;
    ctx->tracks[idx].data_offset = offset;
    ctx->tracks[idx].data_length = (uint32_t)size;
    
    return 0;
}

size_t uft_mfm_get_track_length(uft_mfm_context_t *ctx, int track, int side) {
    const uft_mfm_track_t *ti = uft_mfm_get_track_info(ctx, track, side);
    return ti ? ti->data_length : 0;
}

/*===========================================================================
 * BITSTREAM OPERATIONS
 *===========================================================================*/

int uft_mfm_to_flux(const uint8_t *mfm_data, size_t mfm_bits,
                    uint32_t bitrate,
                    uint32_t *flux_ns, size_t max_flux) {
    if (!mfm_data || !flux_ns || mfm_bits == 0 || bitrate == 0) return 0;
    
    /* Calculate bit time in nanoseconds */
    uint32_t bit_time_ns = (uint32_t)(1000000000ULL / bitrate);
    
    size_t flux_count = 0;
    uint32_t accumulated_time = 0;
    
    for (size_t bit = 0; bit < mfm_bits && flux_count < max_flux; bit++) {
        size_t byte_idx = bit / 8;
        int bit_idx = 7 - (bit % 8);
        
        int bit_val = (mfm_data[byte_idx] >> bit_idx) & 1;
        
        accumulated_time += bit_time_ns;
        
        if (bit_val) {
            /* Flux transition */
            flux_ns[flux_count++] = accumulated_time;
            accumulated_time = 0;
        }
    }
    
    return (int)flux_count;
}

int uft_flux_to_mfm(const uint32_t *flux_ns, size_t flux_count,
                    uint32_t bitrate,
                    uint8_t *mfm_data, size_t max_bytes) {
    if (!flux_ns || !mfm_data || flux_count == 0 || bitrate == 0) return 0;
    
    uint32_t bit_time_ns = (uint32_t)(1000000000ULL / bitrate);
    uint32_t half_bit = bit_time_ns / 2;
    
    memset(mfm_data, 0, max_bytes);
    
    size_t bit_pos = 0;
    size_t max_bits = max_bytes * 8;
    
    for (size_t i = 0; i < flux_count && bit_pos < max_bits; i++) {
        /* Calculate number of bit cells for this flux interval */
        uint32_t interval = flux_ns[i];
        int bits = (interval + half_bit) / bit_time_ns;
        
        /* Advance by (bits-1) zeros, then write a 1 */
        bit_pos += (bits > 0) ? (bits - 1) : 0;
        
        if (bit_pos < max_bits) {
            size_t byte_idx = bit_pos / 8;
            int bit_idx = 7 - (bit_pos % 8);
            mfm_data[byte_idx] |= (1 << bit_idx);
            bit_pos++;
        }
    }
    
    return (int)((bit_pos + 7) / 8);
}

/*===========================================================================
 * FORMAT CONVERSIONS
 *===========================================================================*/

int uft_mfm_to_hfe(const char *mfm_path, const char *hfe_path) {
    if (!mfm_path || !hfe_path) return -1;
    
    /* Read MFM bitstream file */
    FILE *fin = fopen(mfm_path, "rb");
    if (!fin) return -1;
    
    fseek(fin, 0, SEEK_END);
    long mfm_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    
    if (mfm_size <= 0 || mfm_size > 4 * 1024 * 1024) {
        fclose(fin);
        return -1;
    }
    
    uint8_t *mfm_data = malloc((size_t)mfm_size);
    if (!mfm_data) { fclose(fin); return -1; }
    if (fread(mfm_data, 1, (size_t)mfm_size, fin) != (size_t)mfm_size) {
        free(mfm_data); fclose(fin); return -1;
    }
    fclose(fin);
    
    /* Create HFE file with header + interleaved track data */
    FILE *fout = fopen(hfe_path, "wb");
    if (!fout) { free(mfm_data); return -1; }
    
    /* HFE header: "HXCPICFE" + metadata */
    uint8_t header[512];
    memset(header, 0xFF, 512);
    memcpy(header, "HXCPICFE", 8);  /* Magic */
    header[8] = 0;   /* revision 0 */
    
    /* Estimate: 80 tracks, 2 sides */
    size_t bytes_per_side = (size_t)mfm_size / 160;
    if (bytes_per_side == 0) bytes_per_side = 6250;  /* DD default */
    
    header[9] = 80;  /* number of tracks */
    header[10] = 2;  /* number of sides */
    header[11] = 0;  /* MFM encoding */
    /* bitrate: 250000 = 0x0003D090 (little-endian 16-bit field at offset 12) */
    header[12] = 0x90; header[13] = 0xD0;  /* 250000 & 0xFFFF */
    header[14] = 0x03; header[15] = 0x00;  /* RPM: 300 */
    /* Track list offset: block 1 (offset 0x200) */
    header[18] = 1; header[19] = 0;
    
    if (fwrite(header, 1, 512, fout) != 512) {
        free(mfm_data); fclose(fout); return -1;
    }
    
    /* Track lookup table at block 1 */
    uint8_t track_lut[512];
    memset(track_lut, 0xFF, 512);
    size_t data_offset = 2;  /* Tracks start at block 2 */
    size_t track_block_size = ((bytes_per_side * 2 + 511) / 512);
    
    for (int t = 0; t < 80 && t * 4 + 3 < 512; t++) {
        uint16_t offset16 = (uint16_t)(data_offset + t * track_block_size);
        uint16_t len16 = (uint16_t)(bytes_per_side * 2);
        track_lut[t * 4 + 0] = offset16 & 0xFF;
        track_lut[t * 4 + 1] = (offset16 >> 8) & 0xFF;
        track_lut[t * 4 + 2] = len16 & 0xFF;
        track_lut[t * 4 + 3] = (len16 >> 8) & 0xFF;
    }
    if (fwrite(track_lut, 1, 512, fout) != 512) {
        free(mfm_data); fclose(fout); return -1;
    }
    
    /* Write interleaved track data (HFE format: 256 bytes side 0, 256 bytes side 1, repeat) */
    for (int t = 0; t < 80; t++) {
        size_t s0_offset = (t * 2) * bytes_per_side;
        size_t s1_offset = (t * 2 + 1) * bytes_per_side;
        
        for (size_t chunk = 0; chunk < bytes_per_side; chunk += 256) {
            uint8_t block[512];
            memset(block, 0, 512);
            
            size_t copy_len = (bytes_per_side - chunk < 256) ? (bytes_per_side - chunk) : 256;
            if (s0_offset + chunk + copy_len <= (size_t)mfm_size)
                memcpy(block, mfm_data + s0_offset + chunk, copy_len);
            if (s1_offset + chunk + copy_len <= (size_t)mfm_size)
                memcpy(block + 256, mfm_data + s1_offset + chunk, copy_len);
            
            if (fwrite(block, 1, 512, fout) != 512) break;
        }
    }
    
    fclose(fout);
    free(mfm_data);
    return 0;
}

int uft_hfe_to_mfm(const char *hfe_path, const char *mfm_path) {
    if (!hfe_path || !mfm_path) return -1;
    
    FILE *fin = fopen(hfe_path, "rb");
    if (!fin) return -1;
    
    /* Read and validate HFE header */
    uint8_t header[512];
    if (fread(header, 1, 512, fin) != 512) { fclose(fin); return -1; }
    
    if (memcmp(header, "HXCPICFE", 8) != 0 && memcmp(header, "HXCHFEV3", 8) != 0) {
        fclose(fin); return -1;
    }
    
    int num_tracks = header[9];
    int num_sides = header[10];
    if (num_tracks == 0 || num_sides == 0) { fclose(fin); return -1; }
    
    /* Read track lookup table */
    uint16_t lut_offset = header[18] | (header[19] << 8);
    fseek(fin, lut_offset * 512, SEEK_SET);
    
    uint8_t track_lut[512];
    if (fread(track_lut, 1, 512, fin) != 512) { fclose(fin); return -1; }
    
    FILE *fout = fopen(mfm_path, "wb");
    if (!fout) { fclose(fin); return -1; }
    
    /* De-interleave tracks back to raw MFM */
    for (int t = 0; t < num_tracks && t < 80; t++) {
        uint16_t trk_offset = track_lut[t*4] | (track_lut[t*4+1] << 8);
        uint16_t trk_len = track_lut[t*4+2] | (track_lut[t*4+3] << 8);
        
        size_t bytes_per_side = trk_len / 2;
        uint8_t *side0 = calloc(bytes_per_side, 1);
        uint8_t *side1 = calloc(bytes_per_side, 1);
        if (!side0 || !side1) { free(side0); free(side1); break; }
        
        /* Read interleaved blocks */
        fseek(fin, trk_offset * 512L, SEEK_SET);
        size_t s0_pos = 0, s1_pos = 0;
        for (size_t chunk = 0; chunk < bytes_per_side; chunk += 256) {
            uint8_t block[512];
            if (fread(block, 1, 512, fin) != 512) break;
            
            size_t copy = (bytes_per_side - chunk < 256) ? (bytes_per_side - chunk) : 256;
            memcpy(side0 + s0_pos, block, copy); s0_pos += copy;
            memcpy(side1 + s1_pos, block + 256, copy); s1_pos += copy;
        }
        
        fwrite(side0, 1, bytes_per_side, fout);
        if (num_sides > 1) fwrite(side1, 1, bytes_per_side, fout);
        
        free(side0);
        free(side1);
    }
    
    fclose(fin);
    fclose(fout);
    return 0;
}

int uft_mfm_to_scp(const char *mfm_path, const char *scp_path) {
    if (!mfm_path || !scp_path) return -1;
    
    /* Read MFM bitstream */
    FILE *fin = fopen(mfm_path, "rb");
    if (!fin) return -1;
    
    fseek(fin, 0, SEEK_END);
    long mfm_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    
    if (mfm_size <= 0 || mfm_size > 4 * 1024 * 1024) { fclose(fin); return -1; }
    
    uint8_t *mfm_data = malloc((size_t)mfm_size);
    if (!mfm_data) { fclose(fin); return -1; }
    if (fread(mfm_data, 1, (size_t)mfm_size, fin) != (size_t)mfm_size) {
        free(mfm_data); fclose(fin); return -1;
    }
    fclose(fin);
    
    /* Convert MFM bitstream to flux transitions for SCP */
    /* SCP uses 25ns resolution (40 MHz clock) */
    /* MFM bit time: 2000ns (HD) or 4000ns (DD) */
    uint32_t bit_time_ns = 4000;  /* Default DD */
    size_t bytes_per_track = (size_t)mfm_size / 160;
    
    FILE *fout = fopen(scp_path, "wb");
    if (!fout) { free(mfm_data); return -1; }
    
    /* SCP header: "SCP" + version + disk type + revolutions + tracks */
    uint8_t scp_header[16];
    memset(scp_header, 0, 16);
    memcpy(scp_header, "SCP", 3);
    scp_header[3] = 0x18;  /* Version 1.8 */
    scp_header[4] = 0x80;  /* Disk type: generic MFM */
    scp_header[5] = 1;     /* 1 revolution */
    scp_header[6] = 0;     /* Start track 0 */
    scp_header[7] = 159;   /* End track 159 (80 tracks × 2 sides) */
    scp_header[8] = 0x01;  /* Flags: index */
    scp_header[9] = 0;     /* Single bit cell width */
    scp_header[10] = 0;    /* Number of heads: 0=both */
    scp_header[11] = 0;    /* Resolution: 25ns */
    /* Checksum placeholder at offset 12-15 */
    fwrite(scp_header, 1, 16, fout);
    
    /* Track offset table: 160 × 4 bytes */
    long tbl_offset = 16;
    uint32_t *track_offsets = calloc(160, sizeof(uint32_t));
    fwrite(track_offsets, sizeof(uint32_t), 160, fout);
    
    long data_pos = 16 + 160 * 4;
    
    /* Convert each track */
    for (int t = 0; t < 160; t++) {
        size_t src_offset = t * bytes_per_track;
        if (src_offset + bytes_per_track > (size_t)mfm_size) break;
        
        track_offsets[t] = (uint32_t)data_pos;
        
        /* Convert bitstream to flux: scan for 1-bits, measure intervals */
        size_t flux_count = 0;
        uint16_t *flux_data = malloc(bytes_per_track * 8 * sizeof(uint16_t));
        if (!flux_data) break;
        
        uint32_t ticks_since_last = 0;
        uint32_t ticks_per_bit = bit_time_ns / 25;  /* Convert ns to 25ns ticks */
        
        for (size_t b = 0; b < bytes_per_track * 8; b++) {
            size_t byte_idx = src_offset + b / 8;
            int bit_idx = 7 - (b % 8);
            
            ticks_since_last += ticks_per_bit;
            
            if ((mfm_data[byte_idx] >> bit_idx) & 1) {
                if (ticks_since_last > 0 && ticks_since_last <= 0xFFFF) {
                    flux_data[flux_count++] = (uint16_t)ticks_since_last;
                }
                ticks_since_last = 0;
            }
        }
        
        /* Write SCP track header: 3 bytes "TRK" + track# + 1 revolution entry */
        uint8_t trk_hdr[16];
        memcpy(trk_hdr, "TRK", 3);
        trk_hdr[3] = (uint8_t)t;
        /* Revolution entry: index time (4), flux count (4), data offset (4) */
        uint32_t index_time = (uint32_t)(200000000 / 25);  /* 200ms in 25ns ticks */
        uint32_t rev_flux_count = (uint32_t)flux_count;
        uint32_t rev_data_offset = 16;  /* Relative to track header start */
        memcpy(trk_hdr + 4, &index_time, 4);
        memcpy(trk_hdr + 8, &rev_flux_count, 4);
        memcpy(trk_hdr + 12, &rev_data_offset, 4);
        fwrite(trk_hdr, 1, 16, fout);
        
        /* Write flux data as 16-bit big-endian values */
        for (size_t f = 0; f < flux_count; f++) {
            uint8_t be[2] = { (uint8_t)(flux_data[f] >> 8), (uint8_t)(flux_data[f] & 0xFF) };
            fwrite(be, 1, 2, fout);
        }
        
        data_pos = ftell(fout);
        free(flux_data);
    }
    
    /* Write track offset table */
    fseek(fout, tbl_offset, SEEK_SET);
    fwrite(track_offsets, sizeof(uint32_t), 160, fout);
    
    fclose(fout);
    free(track_offsets);
    free(mfm_data);
    return 0;
}
