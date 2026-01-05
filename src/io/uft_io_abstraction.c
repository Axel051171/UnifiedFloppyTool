/**
 * @file uft_io_abstraction.c
 * @brief Unified I/O Abstraction Implementation
 */

#include "uft/uft_io_abstraction.h"
#include "uft/uft_format_probe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Track Memory Management
// ============================================================================

void uft_track_init(uft_track_t* track) {
    if (track) {
        memset(track, 0, sizeof(*track));
    }
}

void uft_track_free(uft_track_t* track) {
    if (!track) return;
    
    // Free flux data
    free(track->flux.samples);
    
    // Free bitstream
    free(track->bitstream.bits);
    
    // Free sectors
    if (track->sectors.sectors) {
        for (int i = 0; i < track->sectors.sector_count; i++) {
            free(track->sectors.sectors[i].data);
            free(track->sectors.sectors[i].header);
        }
        free(track->sectors.sectors);
    }
    
    memset(track, 0, sizeof(*track));
}

uft_error_t uft_track_clone(const uft_track_t* src, uft_track_t* dst) {
    if (!src || !dst) return UFT_ERROR_NULL_POINTER;
    
    uft_track_init(dst);
    
    dst->cylinder = src->cylinder;
    dst->head = src->head;
    dst->available_layers = src->available_layers;
    dst->meta = src->meta;
    
    // Clone flux
    if (src->flux.samples && src->flux.sample_count > 0) {
        size_t size = src->flux.sample_count * sizeof(uint32_t);
        dst->flux.samples = malloc(size);
        if (!dst->flux.samples) {
            uft_track_free(dst);
            return UFT_ERROR_NO_MEMORY;
        }
        memcpy(dst->flux.samples, src->flux.samples, size);
        dst->flux.sample_count = src->flux.sample_count;
        dst->flux.revolution_count = src->flux.revolution_count;
        dst->flux.sample_rate_mhz = src->flux.sample_rate_mhz;
        dst->flux.index_time_us = src->flux.index_time_us;
    }
    
    // Clone bitstream
    if (src->bitstream.bits && src->bitstream.bit_count > 0) {
        size_t size = (src->bitstream.bit_count + 7) / 8;
        dst->bitstream.bits = malloc(size);
        if (!dst->bitstream.bits) {
            uft_track_free(dst);
            return UFT_ERROR_NO_MEMORY;
        }
        memcpy(dst->bitstream.bits, src->bitstream.bits, size);
        dst->bitstream.bit_count = src->bitstream.bit_count;
        dst->bitstream.bit_rate_kbps = src->bitstream.bit_rate_kbps;
        dst->bitstream.encoding = src->bitstream.encoding;
    }
    
    // Clone sectors
    if (src->sectors.sectors && src->sectors.sector_count > 0) {
        dst->sectors.sectors = calloc(src->sectors.sector_count, 
                                       sizeof(uft_sector_t));
        if (!dst->sectors.sectors) {
            uft_track_free(dst);
            return UFT_ERROR_NO_MEMORY;
        }
        
        for (int i = 0; i < src->sectors.sector_count; i++) {
            const uft_sector_t* ss = &src->sectors.sectors[i];
            uft_sector_t* ds = &dst->sectors.sectors[i];
            
            *ds = *ss;  // Copy fixed fields
            
            if (ss->data && ss->data_size > 0) {
                ds->data = malloc(ss->data_size);
                if (ds->data) {
                    memcpy(ds->data, ss->data, ss->data_size);
                }
            }
            if (ss->header && ss->header_size > 0) {
                ds->header = malloc(ss->header_size);
                if (ds->header) {
                    memcpy(ds->header, ss->header, ss->header_size);
                }
            }
        }
        
        dst->sectors.sector_count = src->sectors.sector_count;
        dst->sectors.sector_size = src->sectors.sector_size;
        dst->sectors.interleave = src->sectors.interleave;
    }
    
    return UFT_OK;
}

// ============================================================================
// Layer Conversion
// ============================================================================

static uft_error_t decode_flux_to_bitstream(uft_track_t* track);
static uft_error_t decode_bitstream_to_sectors(uft_track_t* track);
static uft_error_t synthesize_bitstream_from_sectors(uft_track_t* track);
static uft_error_t synthesize_flux_from_bitstream(uft_track_t* track);

bool uft_track_can_convert(const uft_track_t* track,
                            uft_data_layer_t target_layer,
                            const char** warning) {
    if (!track) return false;
    
    if (track->available_layers & (1 << target_layer)) {
        return true;
    }
    
    bool have_flux = track->available_layers & (1 << UFT_LAYER_FLUX);
    bool have_bits = track->available_layers & (1 << UFT_LAYER_BITSTREAM);
    bool have_sect = track->available_layers & (1 << UFT_LAYER_SECTOR);
    
    switch (target_layer) {
        case UFT_LAYER_FLUX:
            if (!have_bits && !have_sect) {
                if (warning) *warning = "No data to synthesize flux from";
                return false;
            }
            if (have_bits || have_sect) {
                if (warning) *warning = "Flux will be synthesized (not original)";
            }
            return true;
            
        case UFT_LAYER_BITSTREAM:
            if (have_flux) return true;
            if (have_sect) {
                if (warning) *warning = "Bitstream will be synthesized";
                return true;
            }
            return false;
            
        case UFT_LAYER_SECTOR:
            return have_flux || have_bits;
            
        case UFT_LAYER_FILESYSTEM:
            return have_sect || have_bits || have_flux;
    }
    
    return false;
}

uft_error_t uft_track_convert_layer(uft_track_t* track,
                                     uft_data_layer_t target_layer,
                                     const void* options) {
    (void)options;
    if (!track) return UFT_ERROR_NULL_POINTER;
    
    if (track->available_layers & (1 << target_layer)) {
        return UFT_OK;
    }
    
    bool have_flux = track->available_layers & (1 << UFT_LAYER_FLUX);
    bool have_bits = track->available_layers & (1 << UFT_LAYER_BITSTREAM);
    bool have_sect = track->available_layers & (1 << UFT_LAYER_SECTOR);
    
    uft_error_t err;
    
    switch (target_layer) {
        case UFT_LAYER_BITSTREAM:
            if (have_flux) {
                err = decode_flux_to_bitstream(track);
                if (err == UFT_OK) {
                    track->available_layers |= (1 << UFT_LAYER_BITSTREAM);
                }
                return err;
            }
            if (have_sect) {
                err = synthesize_bitstream_from_sectors(track);
                if (err == UFT_OK) {
                    track->available_layers |= (1 << UFT_LAYER_BITSTREAM);
                }
                return err;
            }
            return UFT_ERROR_INVALID_STATE;
            
        case UFT_LAYER_SECTOR:
            if (!have_bits && have_flux) {
                err = decode_flux_to_bitstream(track);
                if (err != UFT_OK) return err;
                track->available_layers |= (1 << UFT_LAYER_BITSTREAM);
                have_bits = true;
            }
            if (have_bits) {
                err = decode_bitstream_to_sectors(track);
                if (err == UFT_OK) {
                    track->available_layers |= (1 << UFT_LAYER_SECTOR);
                }
                return err;
            }
            return UFT_ERROR_INVALID_STATE;
            
        case UFT_LAYER_FLUX:
            if (!have_bits && have_sect) {
                err = synthesize_bitstream_from_sectors(track);
                if (err != UFT_OK) return err;
                track->available_layers |= (1 << UFT_LAYER_BITSTREAM);
                have_bits = true;
            }
            if (have_bits) {
                err = synthesize_flux_from_bitstream(track);
                if (err == UFT_OK) {
                    track->available_layers |= (1 << UFT_LAYER_FLUX);
                }
                return err;
            }
            return UFT_ERROR_INVALID_STATE;
            
        default:
            return UFT_ERROR_INVALID_ARG;
    }
}

// Stub implementations
static uft_error_t decode_flux_to_bitstream(uft_track_t* track) {
    if (!track->flux.samples) return UFT_ERROR_INVALID_STATE;
    return UFT_ERROR_NOT_IMPLEMENTED;
}

static uft_error_t decode_bitstream_to_sectors(uft_track_t* track) {
    if (!track->bitstream.bits) return UFT_ERROR_INVALID_STATE;
    return UFT_ERROR_NOT_IMPLEMENTED;
}

static uft_error_t synthesize_bitstream_from_sectors(uft_track_t* track) {
    if (!track->sectors.sectors) return UFT_ERROR_INVALID_STATE;
    return UFT_ERROR_NOT_IMPLEMENTED;
}

static uft_error_t synthesize_flux_from_bitstream(uft_track_t* track) {
    if (!track->bitstream.bits) return UFT_ERROR_INVALID_STATE;
    return UFT_ERROR_NOT_IMPLEMENTED;
}

// ============================================================================
// Source/Sink Management
// ============================================================================

void uft_io_source_close(uft_io_source_t* source) {
    if (source && source->ops && source->ops->close) {
        source->ops->close(source);
    }
    free(source);
}

void uft_io_sink_close(uft_io_sink_t* sink) {
    if (sink) {
        if (sink->ops && sink->ops->finalize) {
            sink->ops->finalize(sink);
        }
        if (sink->ops && sink->ops->close) {
            sink->ops->close(sink);
        }
    }
    free(sink);
}

uft_error_t uft_io_copy(uft_io_source_t* source,
                         uft_io_sink_t* sink,
                         void (*progress)(int percent, void* user),
                         void* user) {
    if (!source || !sink) return UFT_ERROR_NULL_POINTER;
    
    int cylinders = source->ops->get_cylinders(source);
    int heads = source->ops->get_heads(source);
    uft_data_layer_t required_layer = sink->ops->get_required_layer(sink);
    
    int total_tracks = cylinders * heads;
    int processed = 0;
    
    for (int c = 0; c < cylinders; c++) {
        for (int h = 0; h < heads; h++) {
            uft_track_t track;
            uft_track_init(&track);
            
            uft_error_t err = source->ops->read_track(source, c, h, 
                                                       source->native_layer, 
                                                       &track);
            if (err != UFT_OK) {
                uft_track_free(&track);
                continue;
            }
            
            if (!(track.available_layers & (1 << required_layer))) {
                err = uft_track_convert_layer(&track, required_layer, NULL);
                if (err != UFT_OK) {
                    uft_track_free(&track);
                    continue;
                }
            }
            
            err = sink->ops->write_track(sink, c, h, &track);
            uft_track_free(&track);
            
            if (err != UFT_OK) return err;
            
            processed++;
            if (progress) {
                progress((processed * 100) / total_tracks, user);
            }
        }
    }
    
    return UFT_OK;
}
