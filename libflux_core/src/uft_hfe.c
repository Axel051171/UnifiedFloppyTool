/**
 * @file uft_hfe.c
 * @brief HFE Complete Implementation (Layers 2+3)
 */

#include "uft_hfe.h"
#include "uft_mfm.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * LAYER 2: GEOMETRY DETECTION
 * ======================================================================== */

uft_rc_t uft_hfe_detect_geometry(
    const uft_hfe_container_t* container,
    uft_geometry_t* geometry
) {
    UFT_CHECK_NULLS(container, geometry);
    
    /* HFE provides track/side count in header */
    geometry->cylinders = container->header.track_count;
    geometry->heads = container->header.side_count;
    
    /* Sectors per track and sector size must be analyzed from track data
     * For now, use common defaults based on encoding */
    switch (container->header.track_encoding) {
    case UFT_HFE_ENCODING_ISOIBM_MFM:
        /* Standard PC MFM */
        if (container->header.bitrate == 500) {
            /* HD 1.44MB */
            geometry->sectors_per_track = 18;
        } else {
            /* DD 720KB */
            geometry->sectors_per_track = 9;
        }
        geometry->sector_size = 512;
        break;
        
    case UFT_HFE_ENCODING_AMIGA_MFM:
        geometry->sectors_per_track = 11;
        geometry->sector_size = 512;
        break;
        
    case UFT_HFE_ENCODING_ISOIBM_FM:
        geometry->sectors_per_track = 8;
        geometry->sector_size = 512;
        break;
        
    default:
        /* Unknown - conservative guess */
        geometry->sectors_per_track = 9;
        geometry->sector_size = 512;
        break;
    }
    
    geometry->source = UFT_GEOM_SOURCE_HEADER;
    geometry->confidence = 75;  /* Medium confidence - should analyze tracks */
    
    /* Calculate capacity */
    geometry->total_sectors = geometry->cylinders * 
                             geometry->heads * 
                             geometry->sectors_per_track;
    geometry->total_bytes = geometry->total_sectors * geometry->sector_size;
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * LAYER 3: TRACK DECODING
 * ======================================================================== */

uft_rc_t uft_hfe_decode_track_bitstream(
    uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side,
    uint8_t** bitstream,
    uint32_t* bit_count
) {
    UFT_CHECK_NULLS(container, bitstream, bit_count);
    
    *bitstream = NULL;
    *bit_count = 0;
    
    /* Read raw track data */
    uint8_t* raw_data;
    size_t raw_size;
    
    uft_rc_t rc = uft_hfe_read_track_raw(container, track, side,
                                         &raw_data, &raw_size);
    if (uft_failed(rc)) {
        return rc;
    }
    
    /* HFE uses interleaved encoding:
     * - Data is in blocks (256 or 512 bytes)
     * - Blocks alternate between side 0 and side 1
     * - We need to extract only our side
     */
    
    uint32_t block_size = container->header.track_encoding_size;
    uint32_t block_count = raw_size / block_size;
    
    /* Allocate output bitstream */
    uint8_t* output = malloc(raw_size / 2);  /* Approximate */
    if (!output) {
        free(raw_data);
        return UFT_ERR_MEMORY;
    }
    
    uint32_t output_pos = 0;
    
    /* Extract blocks for this side */
    for (uint32_t block = 0; block < block_count; block++) {
        /* Side 0 = even blocks, Side 1 = odd blocks */
        if ((block % 2) == side) {
            uint32_t src_offset = block * block_size;
            memcpy(&output[output_pos], &raw_data[src_offset], block_size);
            output_pos += block_size;
        }
    }
    
    *bitstream = output;
    *bit_count = output_pos * 8;  /* Convert bytes to bits */
    
    free(raw_data);
    return UFT_SUCCESS;
}

/* ========================================================================
 * COMPLETE API
 * ======================================================================== */

uft_rc_t uft_hfe_open(const char* path, uft_hfe_ctx_t** ctx) {
    UFT_CHECK_NULL(path);
    UFT_CHECK_NULL(ctx);
    
    *ctx = NULL;
    
    /* Allocate context */
    uft_hfe_ctx_t* hfe = calloc(1, sizeof(uft_hfe_ctx_t));
    if (!hfe) {
        return UFT_ERR_MEMORY;
    }
    
    /* Layer 1: Parse container */
    uft_rc_t rc = uft_hfe_container_open(path, &hfe->container);
    if (uft_failed(rc)) {
        free(hfe);
        return rc;
    }
    
    /* Layer 2: Detect geometry */
    rc = uft_hfe_detect_geometry(hfe->container, &hfe->geometry);
    if (uft_failed(rc)) {
        uft_hfe_container_close(&hfe->container);
        free(hfe);
        return rc;
    }
    
    hfe->geometry_detected = true;
    
    /* Set capabilities */
    hfe->supports_track_api = true;
    hfe->supports_sector_api = true;  /* Via track decode */
    
    *ctx = hfe;
    return UFT_SUCCESS;
}

void uft_hfe_close(uft_hfe_ctx_t** ctx) {
    if (ctx && *ctx) {
        if ((*ctx)->container) {
            uft_hfe_container_close(&(*ctx)->container);
        }
        free(*ctx);
        *ctx = NULL;
    }
}

uft_rc_t uft_hfe_read_track(
    uft_hfe_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uint8_t** bitstream,
    uint32_t* bit_count
) {
    UFT_CHECK_NULLS(ctx, bitstream, bit_count);
    
    return uft_hfe_decode_track_bitstream(ctx->container, track, head,
                                          bitstream, bit_count);
}

uft_rc_t uft_hfe_read_sector(
    uft_hfe_ctx_t* ctx,
    uint32_t cylinder,
    uint32_t head,
    uint32_t sector,
    uint8_t* buffer,
    size_t buffer_size
) {
    UFT_CHECK_NULLS(ctx, buffer);
    
    /* Read track bitstream */
    uint8_t* bitstream;
    uint32_t bit_count;
    
    uft_rc_t rc = uft_hfe_read_track(ctx, cylinder, head, 
                                      &bitstream, &bit_count);
    if (uft_failed(rc)) {
        return rc;
    }
    
    /* Decode with MFM decoder */
    uft_mfm_ctx_t* mfm_ctx;
    rc = uft_mfm_create(&mfm_ctx);
    if (uft_failed(rc)) {
        free(bitstream);
        return rc;
    }
    
    /* Use geometry to set format */
    uft_mfm_geometry_t mfm_geom = {
        .cylinders = ctx->geometry.cylinders,
        .heads = ctx->geometry.heads,
        .sectors_per_track = ctx->geometry.sectors_per_track,
        .sector_size = ctx->geometry.sector_size,
        .bitrate = ctx->container->header.bitrate * 1000,
        .rpm = ctx->container->header.rpm / 100
    };
    
    uft_mfm_set_geometry(mfm_ctx, &mfm_geom);
    
    /* Decode track */
    uft_mfm_track_t mfm_track;
    rc = uft_mfm_decode_track(mfm_ctx, cylinder, head,
                              bitstream, bit_count, &mfm_track);
    
    if (uft_success(rc)) {
        /* Find requested sector */
        for (int i = 0; i < mfm_track.sectors_found; i++) {
            if (mfm_track.sectors[i].id.sector == sector &&
                mfm_track.sectors[i].data_valid) {
                
                /* Copy sector data */
                size_t copy_size = mfm_track.sectors[i].data.size;
                if (copy_size > buffer_size) copy_size = buffer_size;
                
                memcpy(buffer, mfm_track.sectors[i].data.data, copy_size);
                
                /* Free track data */
                for (int j = 0; j < mfm_track.sectors_found; j++) {
                    if (mfm_track.sectors[j].data.data) {
                        free(mfm_track.sectors[j].data.data);
                    }
                }
                
                free(bitstream);
                uft_mfm_destroy(&mfm_ctx);
                return UFT_SUCCESS;
            }
        }
        
        /* Sector not found */
        rc = UFT_ERR_NOT_FOUND;
    }
    
    free(bitstream);
    uft_mfm_destroy(&mfm_ctx);
    return rc;
}
