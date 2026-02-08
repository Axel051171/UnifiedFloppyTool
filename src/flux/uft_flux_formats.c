/**
 * @file uft_flux_formats.c
 * @brief Flux format file converters (SCP, KryoFlux, DFI → Disk Image)
 * @version 3.9.0
 */

#include "uft/flux/uft_flux_decoder.h"
#include "uft/formats/uft_scp.h"
#include "uft/formats/uft_dfi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * SCP to Disk Image Converter
 * ============================================================================ */

flux_status_t flux_decode_scp_file(const char *path,
                                   uft_disk_image_t **out_disk,
                                   const flux_decoder_options_t *opts) {
    if (!path || !out_disk) return FLUX_ERR_INVALID;
    
    /* Read SCP file */
    scp_image_t scp;
    uft_scp_image_init(&scp);
    
    scp_read_result_t result;
    uft_error_t err = uft_scp_read(path, &scp, NULL, &result);
    if (err != UFT_OK) {
        return FLUX_ERR_INVALID;
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(scp.cylinders, scp.heads);
    if (!disk) {
        uft_scp_image_free(&scp);
        return FLUX_ERR_OVERFLOW;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "SCP-Decoded");
    
    /* Decode each track */
    flux_decoder_options_t local_opts;
    if (!opts) {
        flux_decoder_options_init(&local_opts);
        opts = &local_opts;
    }
    
    for (size_t t = 0; t < scp.num_tracks; t++) {
        scp_track_data_t *scp_track = &scp.tracks[t];
        if (scp_track->revolutions == 0) continue;
        
        /* Determine cylinder and head from track number */
        uint8_t cyl, head;
        if (scp.heads == 2) {
            cyl = scp_track->track_num / 2;
            head = scp_track->track_num & 1;
        } else {
            cyl = scp_track->track_num;
            head = 0;
        }
        
        /* Use first revolution (or specified) */
        uint8_t rev = opts->revolution;
        if (rev >= scp_track->revolutions) rev = 0;
        
        /* Convert SCP flux data to raw flux format */
        flux_raw_data_t raw_flux;
        memset(&raw_flux, 0, sizeof(raw_flux));
        
        /* SCP uses 16-bit flux times at 25ns (40MHz) */
        raw_flux.sample_rate = SCP_SAMPLE_RATE;
        raw_flux.transition_count = scp_track->rev[rev].flux_count;
        raw_flux.transitions = malloc(raw_flux.transition_count * sizeof(uint32_t));
        
        if (raw_flux.transitions) {
            /* Convert 16-bit to 32-bit and accumulate */
            uint32_t accum = 0;
            for (size_t i = 0; i < raw_flux.transition_count; i++) {
                accum += scp_track->rev[rev].flux_data[i];
                raw_flux.transitions[i] = accum;
            }
            
            /* Decode track */
            flux_decoded_track_t decoded;
            flux_decoded_track_init(&decoded);
            
            flux_status_t status = flux_decode_track(&raw_flux, &decoded, opts);
            
            if (status == FLUX_OK && decoded.sector_count > 0) {
                /* Create UFT track from decoded sectors */
                size_t idx = cyl * disk->heads + head;
                
                uft_track_t *track = uft_track_alloc(decoded.sector_count, 0);
                if (track) {
                    track->track_num = cyl;
                    track->head = head;
                    track->encoding = (decoded.detected_encoding == FLUX_ENC_FM) ?
                                     UFT_ENC_FM : UFT_ENC_MFM;
                    
                    for (size_t s = 0; s < decoded.sector_count; s++) {
                        flux_decoded_sector_t *src = &decoded.sectors[s];
                        uft_sector_t *dst = &track->sectors[s];
                        
                        dst->id.cylinder = src->cylinder;
                        dst->id.head = src->head;
                        dst->id.sector = src->sector;
                        dst->id.size_code = src->size_code;
                        
                        dst->data = src->data;
                        dst->data_size = src->data_size;
                        src->data = NULL;  /* Transfer ownership */
                        
                        if (src->id_crc_ok && src->data_crc_ok) {
                            dst->status = UFT_SECTOR_OK;
                        } else if (!src->data_crc_ok) {
                            dst->status = UFT_SECTOR_DATA_ERROR;
                        } else {
                            dst->status = UFT_SECTOR_ID_ERROR;
                        }
                        
                        track->sector_count++;
                    }
                    
                    disk->track_data[idx] = track;
                    
                    /* Update disk geometry */
                    if (decoded.sector_count > 0) {
                        disk->sectors_per_track = decoded.sector_count;
                        disk->bytes_per_sector = decoded.sectors[0].data_size;
                    }
                }
            }
            
            flux_decoded_track_free(&decoded);
            free(raw_flux.transitions);
        }
    }
    
    uft_scp_image_free(&scp);
    *out_disk = disk;
    
    return FLUX_OK;
}

/* ============================================================================
 * KryoFlux to Disk Image Converter
 * ============================================================================ */

/* KryoFlux stream constants */
#define KF_SAMPLE_RATE      24027428    /* PAL subcarrier * 6 */
#define KF_FLUX_MAX_8BIT    0xE7
#define KF_OOB_MARKER       0x0D

static flux_status_t parse_kryoflux_stream(const uint8_t *data, size_t size,
                                           flux_raw_data_t *flux) {
    /* Count transitions first */
    size_t count = 0;
    uint32_t accum = 0;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t b = data[i];
        
        if (b <= KF_FLUX_MAX_8BIT) {
            accum += b;
            count++;
        } else if (b == 0xFF) {
            /* Overflow: add 0x100 to accumulator */
            accum += 0x100;
        } else if (b >= 0xE8 && b <= 0xED) {
            /* OOB block - skip */
            if (i + 1 < size) {
                size_t oob_len = data[i + 1];
                i += 1 + oob_len;
            }
        }
    }
    
    if (count == 0) return FLUX_ERR_NO_SYNC;
    
    /* Allocate and fill transitions */
    flux->transitions = malloc(count * sizeof(uint32_t));
    if (!flux->transitions) return FLUX_ERR_OVERFLOW;
    
    flux->transition_count = 0;
    flux->sample_rate = KF_SAMPLE_RATE;
    accum = 0;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t b = data[i];
        
        if (b <= KF_FLUX_MAX_8BIT) {
            accum += b;
            flux->transitions[flux->transition_count++] = accum;
        } else if (b == 0xFF) {
            accum += 0x100;
        } else if (b >= 0xE8 && b <= 0xED) {
            if (i + 1 < size) {
                size_t oob_len = data[i + 1];
                i += 1 + oob_len;
            }
        }
    }
    
    return FLUX_OK;
}

flux_status_t flux_decode_kryoflux_files(const char *base_path,
                                         uft_disk_image_t **out_disk,
                                         const flux_decoder_options_t *opts) {
    if (!base_path || !out_disk) return FLUX_ERR_INVALID;
    
    /* KryoFlux files are named: trackXX.Y.raw where XX=cylinder, Y=head */
    /* base_path should be the directory containing the files */
    
    /* First pass: determine geometry */
    uint8_t max_cyl = 0, max_head = 0;
    char filename[512];
    
    for (int cyl = 0; cyl < 100; cyl++) {
        for (int head = 0; head < 2; head++) {
            snprintf(filename, sizeof(filename), "%s/track%02d.%d.raw", 
                    base_path, cyl, head);
            FILE *fp = fopen(filename, "rb");
            if (fp) {
                fclose(fp);
                if (cyl > max_cyl) max_cyl = cyl;
                if (head > max_head) max_head = head;
            }
        }
    }
    
    if (max_cyl == 0) return FLUX_ERR_INVALID;
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(max_cyl + 1, max_head + 1);
    if (!disk) return FLUX_ERR_OVERFLOW;
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "KryoFlux-Decoded");
    
    /* Decode each track */
    flux_decoder_options_t local_opts;
    if (!opts) {
        flux_decoder_options_init(&local_opts);
        opts = &local_opts;
    }
    
    for (int cyl = 0; cyl <= max_cyl; cyl++) {
        for (int head = 0; head <= max_head; head++) {
            snprintf(filename, sizeof(filename), "%s/track%02d.%d.raw",
                    base_path, cyl, head);
            
            FILE *fp = fopen(filename, "rb");
            if (!fp) continue;
            
            fseek(fp, 0, SEEK_END);
            size_t size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            
            uint8_t *data = malloc(size);
            if (!data) {
                fclose(fp);
                continue;
            }
            
            size_t bytes_read = fread(data, 1, size, fp);
            fclose(fp);
            
            if (bytes_read != (size_t)size) {
                free(data);
                continue;
            }
            
            /* Parse KryoFlux stream */
            flux_raw_data_t raw_flux;
            memset(&raw_flux, 0, sizeof(raw_flux));
            
            if (parse_kryoflux_stream(data, size, &raw_flux) == FLUX_OK) {
                /* Decode track */
                flux_decoded_track_t decoded;
                flux_decoded_track_init(&decoded);
                
                if (flux_decode_track(&raw_flux, &decoded, opts) == FLUX_OK) {
                    /* Create UFT track */
                    size_t idx = cyl * disk->heads + head;
                    
                    uft_track_t *track = uft_track_alloc(decoded.sector_count, 0);
                    if (track) {
                        track->track_num = cyl;
                        track->head = head;
                        
                        for (size_t s = 0; s < decoded.sector_count; s++) {
                            flux_decoded_sector_t *src = &decoded.sectors[s];
                            uft_sector_t *dst = &track->sectors[s];
                            
                            dst->id.cylinder = src->cylinder;
                            dst->id.head = src->head;
                            dst->id.sector = src->sector;
                            dst->id.size_code = src->size_code;
                            dst->data = src->data;
                            dst->data_size = src->data_size;
                            src->data = NULL;
                            dst->status = (src->id_crc_ok && src->data_crc_ok) ?
                                         UFT_SECTOR_OK : UFT_SECTOR_DATA_ERROR;
                            track->sector_count++;
                        }
                        
                        disk->track_data[idx] = track;
                    }
                }
                
                flux_decoded_track_free(&decoded);
                free(raw_flux.transitions);
            }
            
            free(data);
        }
    }
    
    *out_disk = disk;
    return FLUX_OK;
}

/* ============================================================================
 * DFI to Disk Image Converter
 * ============================================================================ */

flux_status_t flux_decode_dfi_file(const char *path,
                                   uft_disk_image_t **out_disk,
                                   const flux_decoder_options_t *opts) {
    if (!path || !out_disk) return FLUX_ERR_INVALID;
    
    /* Read DFI file */
    dfi_image_t dfi;
    dfi_read_result_t result;
    
    uft_error_t err = uft_dfi_read(path, &dfi, NULL, &result);
    if (err != UFT_OK) {
        return FLUX_ERR_INVALID;
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(dfi.cylinders, dfi.heads);
    if (!disk) {
        uft_dfi_image_free(&dfi);
        return FLUX_ERR_OVERFLOW;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "DFI-Decoded");
    
    flux_decoder_options_t local_opts;
    if (!opts) {
        flux_decoder_options_init(&local_opts);
        opts = &local_opts;
    }
    
    /* Decode each track */
    for (size_t t = 0; t < dfi.track_count; t++) {
        dfi_track_t *dfi_track = &dfi.tracks[t];
        
        /* Convert DFI flux data */
        flux_raw_data_t raw_flux;
        memset(&raw_flux, 0, sizeof(raw_flux));
        
        raw_flux.sample_rate = dfi.sample_rate;
        raw_flux.transition_count = dfi_track->flux_count;
        raw_flux.transitions = malloc(dfi_track->flux_count * sizeof(uint32_t));
        
        if (raw_flux.transitions) {
            /* DFI stores delta times, convert to absolute */
            uint32_t accum = 0;
            for (size_t i = 0; i < dfi_track->flux_count; i++) {
                accum += dfi_track->flux_times[i];
                raw_flux.transitions[i] = accum;
            }
            
            /* Decode track */
            flux_decoded_track_t decoded;
            flux_decoded_track_init(&decoded);
            
            if (flux_decode_track(&raw_flux, &decoded, opts) == FLUX_OK) {
                size_t idx = dfi_track->cylinder * disk->heads + dfi_track->head;
                
                uft_track_t *track = uft_track_alloc(decoded.sector_count, 0);
                if (track) {
                    track->track_num = dfi_track->cylinder;
                    track->head = dfi_track->head;
                    
                    for (size_t s = 0; s < decoded.sector_count; s++) {
                        flux_decoded_sector_t *src = &decoded.sectors[s];
                        uft_sector_t *dst = &track->sectors[s];
                        
                        dst->id.cylinder = src->cylinder;
                        dst->id.head = src->head;
                        dst->id.sector = src->sector;
                        dst->id.size_code = src->size_code;
                        dst->data = src->data;
                        dst->data_size = src->data_size;
                        src->data = NULL;
                        dst->status = (src->id_crc_ok && src->data_crc_ok) ?
                                     UFT_SECTOR_OK : UFT_SECTOR_DATA_ERROR;
                        track->sector_count++;
                    }
                    
                    disk->track_data[idx] = track;
                }
            }
            
            flux_decoded_track_free(&decoded);
            free(raw_flux.transitions);
        }
    }
    
    uft_dfi_image_free(&dfi);
    *out_disk = disk;
    
    return FLUX_OK;
}
