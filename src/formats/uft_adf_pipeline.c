/**
 * @file uft_adf_pipeline.c
 * @brief ADF Parser Pipeline implementation
 * 
 * P1-006: Full pipeline for ADF format
 */

#include "uft/formats/uft_adf_pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Amiga filesystem types */
#define ADF_FS_OFS      0   /* Original File System */
#define ADF_FS_FFS      1   /* Fast File System */
#define ADF_FS_OFS_INTL 2   /* OFS International */
#define ADF_FS_FFS_INTL 3   /* FFS International */
#define ADF_FS_OFS_DC   4   /* OFS Dir Cache */
#define ADF_FS_FFS_DC   5   /* FFS Dir Cache */

/* ============================================================================
 * Initialization
 * ============================================================================ */

void adf_pipeline_options_init(adf_pipeline_options_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->analyze_checksums = true;
    opts->analyze_timing = false;
    opts->detect_weak_bits = true;
    opts->use_multi_rev = false;
    opts->min_confidence = 80;
    opts->preserve_original = false;
    opts->preserve_errors = true;
    opts->preserve_timing = false;
    opts->generate_extended = false;
    opts->include_analysis = false;
}

void adf_pipeline_init(adf_pipeline_ctx_t *ctx) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->stage = ADF_STAGE_INIT;
    adf_pipeline_options_init(&ctx->opts);
}

void adf_pipeline_free(adf_pipeline_ctx_t *ctx) {
    if (!ctx) return;
    
    if (ctx->disk) {
        uft_disk_free(ctx->disk);
        ctx->disk = NULL;
    }
}

int adf_pipeline_add_revision(adf_pipeline_ctx_t *ctx,
                              const uint8_t *data, size_t size,
                              uint8_t quality) {
    if (!ctx || !data || ctx->revision_count >= 16) {
        return -1;
    }
    
    adf_revision_t *rev = &ctx->revisions[ctx->revision_count];
    rev->data = data;
    rev->size = size;
    rev->quality = quality;
    rev->crc_checked = false;
    
    ctx->revision_count++;
    ctx->opts.use_multi_rev = (ctx->revision_count > 1);
    
    return (int)ctx->revision_count - 1;
}

/* ============================================================================
 * Checksum Functions
 * ============================================================================ */

uint32_t adf_checksum(const uint8_t *data, size_t size) {
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < size; i += 4) {
        uint32_t word = ((uint32_t)data[i] << 24) |
                        ((uint32_t)data[i+1] << 16) |
                        ((uint32_t)data[i+2] << 8) |
                        (uint32_t)data[i+3];
        
        uint32_t old = checksum;
        checksum += word;
        if (checksum < old) checksum++;  /* Carry */
    }
    
    return ~checksum;
}

bool adf_verify_header(const uint8_t *header) {
    if (!header) return false;
    
    /* Amiga sector header is 4 bytes format + 4 bytes header checksum */
    uint32_t stored = ((uint32_t)header[4] << 24) |
                      ((uint32_t)header[5] << 16) |
                      ((uint32_t)header[6] << 8) |
                      (uint32_t)header[7];
    
    uint32_t calculated = adf_checksum(header, 4);
    
    return stored == calculated;
}

bool adf_verify_data(const uint8_t *data, size_t size,
                     uint32_t expected_checksum) {
    uint32_t calculated = adf_checksum(data, size);
    return calculated == expected_checksum;
}

/* ============================================================================
 * Stage 1: READ
 * ============================================================================ */

bool adf_validate(const uint8_t *data, size_t size) {
    if (!data) return false;
    
    /* Check file size */
    if (size != ADF_FILE_SIZE_DD && size != ADF_FILE_SIZE_HD) {
        return false;
    }
    
    /* Check boot block signature */
    if (memcmp(data, "DOS", 3) != 0) {
        /* Not bootable, but might still be valid */
        /* Check for valid root block */
    }
    
    return true;
}

uft_error_t adf_read(const char *path, uft_disk_image_t **out_disk) {
    if (!path || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    long file_size_l = ftell(fp);
    if (file_size_l < 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    size_t file_size = (size_t)file_size_l;
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, file_size, fp) != file_size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    uft_error_t err = adf_read_mem(data, file_size, out_disk);
    free(data);
    
    return err;
}

uft_error_t adf_read_mem(const uint8_t *data, size_t size,
                         uft_disk_image_t **out_disk) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (!adf_validate(data, size)) {
        return UFT_ERR_CORRUPT;
    }
    
    bool is_hd = (size == ADF_FILE_SIZE_HD);
    uint8_t num_tracks = is_hd ? ADF_TRACKS_HD : ADF_TRACKS_DD;
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(num_tracks, ADF_HEADS);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_ADF;
    snprintf(disk->format_name, sizeof(disk->format_name), "ADF");
    disk->sectors_per_track = ADF_SECTORS_PER_TRACK;
    disk->bytes_per_sector = ADF_SECTOR_SIZE;
    
    /* Read sectors */
    size_t data_pos = 0;
    
    for (uint8_t t = 0; t < num_tracks; t++) {
        for (uint8_t h = 0; h < ADF_HEADS; h++) {
            size_t idx = t * ADF_HEADS + h;
            
            uft_track_t *track = uft_track_alloc(ADF_SECTORS_PER_TRACK, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = t;
            track->head = h;
            track->encoding = UFT_ENC_AMIGA_MFM;
            
            for (uint8_t s = 0; s < ADF_SECTORS_PER_TRACK; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.track = t;
                sect->id.head = h;
                sect->id.sector = s;
                sect->id.size_code = 2;  /* 512 bytes */
                sect->id.status = UFT_SECTOR_OK;
                sect->id.encoding = UFT_ENC_AMIGA_MFM;
                
                sect->data = malloc(ADF_SECTOR_SIZE);
                sect->data_len = ADF_SECTOR_SIZE;
                
                if (data_pos + ADF_SECTOR_SIZE <= size) {
                    memcpy(sect->data, data + data_pos, ADF_SECTOR_SIZE);
                } else {
                    memset(sect->data, 0, ADF_SECTOR_SIZE);
                }
                data_pos += ADF_SECTOR_SIZE;
                
                track->sector_count++;
            }
            
            track->complete = true;
            disk->track_data[idx] = track;
        }
    }
    
    *out_disk = disk;
    return UFT_OK;
}

/* ============================================================================
 * Stage 2: ANALYZE
 * ============================================================================ */

uft_error_t adf_analyze_sector(const uint8_t *sector_data,
                               size_t size,
                               adf_sector_analysis_t *out_analysis) {
    if (!sector_data || !out_analysis || size < ADF_SECTOR_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(out_analysis, 0, sizeof(*out_analysis));
    
    /* Calculate checksum */
    out_analysis->calculated_checksum = adf_checksum(sector_data, size);
    out_analysis->data_valid = true;  /* Would need stored checksum to verify */
    out_analysis->confidence = 100;
    
    return UFT_OK;
}

uft_error_t adf_analyze_track(const uft_track_t *track,
                              adf_track_analysis_t *out_analysis) {
    if (!track || !out_analysis) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(out_analysis, 0, sizeof(*out_analysis));
    out_analysis->track = track->track_num;
    out_analysis->head = track->head;
    
    uint8_t valid_count = 0;
    
    for (size_t s = 0; s < track->sector_count && s < ADF_SECTORS_PER_TRACK; s++) {
        const uft_sector_t *sect = &track->sectors[s];
        adf_sector_analysis_t *sa = &out_analysis->sectors[s];
        
        sa->sector = sect->id.sector;
        
        if (sect->data && sect->data_len >= ADF_SECTOR_SIZE) {
            adf_analyze_sector(sect->data, sect->data_len, sa);
            out_analysis->sectors_found++;
            
            if (sa->data_valid) {
                valid_count++;
            }
        }
    }
    
    out_analysis->sectors_valid = valid_count;
    out_analysis->complete = (valid_count == ADF_SECTORS_PER_TRACK);
    out_analysis->quality = (valid_count * 100) / ADF_SECTORS_PER_TRACK;
    
    return UFT_OK;
}

uft_error_t adf_analyze(const uft_disk_image_t *disk,
                        adf_disk_analysis_t *out_analysis) {
    if (!disk || !out_analysis) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(out_analysis, 0, sizeof(*out_analysis));
    
    out_analysis->tracks = disk->tracks;
    out_analysis->heads = disk->heads;
    out_analysis->is_hd = (disk->tracks > ADF_TRACKS_DD);
    
    uint16_t total_valid = 0;
    uint16_t total_sectors = 0;
    
    for (size_t t = 0; t < disk->track_count; t++) {
        uft_track_t *track = disk->track_data[t];
        if (!track) continue;
        
        adf_track_analysis_t *ta = &out_analysis->track_analysis[t];
        adf_analyze_track(track, ta);
        
        total_sectors += ta->sectors_found;
        total_valid += ta->sectors_valid;
    }
    
    out_analysis->total_sectors = total_sectors;
    out_analysis->valid_sectors = total_valid;
    out_analysis->error_sectors = total_sectors - total_valid;
    
    if (total_sectors > 0) {
        out_analysis->overall_quality = 
            (float)total_valid * 100.0f / total_sectors;
    }
    
    /* Detect filesystem from boot block */
    if (disk->track_data[0] && 
        disk->track_data[0]->sector_count > 0 &&
        disk->track_data[0]->sectors[0].data) {
        
        const uint8_t *boot = disk->track_data[0]->sectors[0].data;
        out_analysis->filesystem = adf_detect_filesystem(boot);
        out_analysis->is_bootable = (memcmp(boot, "DOS", 3) == 0);
        
        /* Copy disk name if present */
        if (boot[0] == 'D' && boot[1] == 'O' && boot[2] == 'S') {
            out_analysis->root_block = 880;  /* Standard Amiga root */
        }
    }
    
    out_analysis->success = true;
    return UFT_OK;
}

uint8_t adf_detect_filesystem(const uint8_t *boot_block) {
    if (!boot_block) return ADF_FS_OFS;
    
    if (memcmp(boot_block, "DOS", 3) != 0) {
        return ADF_FS_OFS;  /* Unknown, assume OFS */
    }
    
    uint8_t flags = boot_block[3];
    
    switch (flags & 0x07) {
        case 0: return ADF_FS_OFS;
        case 1: return ADF_FS_FFS;
        case 2: return ADF_FS_OFS_INTL;
        case 3: return ADF_FS_FFS_INTL;
        case 4: return ADF_FS_OFS_DC;
        case 5: return ADF_FS_FFS_DC;
        default: return ADF_FS_OFS;
    }
}

const char* adf_filesystem_name(uint8_t type) {
    switch (type) {
        case ADF_FS_OFS:      return "OFS";
        case ADF_FS_FFS:      return "FFS";
        case ADF_FS_OFS_INTL: return "OFS-INTL";
        case ADF_FS_FFS_INTL: return "FFS-INTL";
        case ADF_FS_OFS_DC:   return "OFS-DC";
        case ADF_FS_FFS_DC:   return "FFS-DC";
        default:              return "Unknown";
    }
}

/* ============================================================================
 * Stage 3: DECIDE
 * ============================================================================ */

int adf_decide_sector(const adf_sector_analysis_t *analyses, size_t count) {
    if (!analyses || count == 0) return -1;
    
    int best = 0;
    uint8_t best_confidence = 0;
    bool best_valid = false;
    
    for (size_t i = 0; i < count; i++) {
        const adf_sector_analysis_t *a = &analyses[i];
        
        /* Prefer valid over invalid */
        if (a->data_valid && !best_valid) {
            best = (int)i;
            best_confidence = a->confidence;
            best_valid = true;
        }
        /* If both valid (or both invalid), use confidence */
        else if (a->data_valid == best_valid && a->confidence > best_confidence) {
            best = (int)i;
            best_confidence = a->confidence;
        }
    }
    
    return best;
}

uft_error_t adf_decide(adf_pipeline_ctx_t *ctx) {
    if (!ctx || ctx->revision_count == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (ctx->revision_count == 1) {
        /* Only one revision, nothing to decide */
        return UFT_OK;
    }
    
    /* Analyze each revision */
    adf_disk_analysis_t *analyses = calloc(ctx->revision_count, 
                                           sizeof(adf_disk_analysis_t));
    if (!analyses) {
        return UFT_ERR_MEMORY;
    }
    
    for (size_t r = 0; r < ctx->revision_count; r++) {
        uft_disk_image_t *disk = NULL;
        if (adf_read_mem(ctx->revisions[r].data, ctx->revisions[r].size, 
                         &disk) == UFT_OK) {
            adf_analyze(disk, &analyses[r]);
            uft_disk_free(disk);
        }
    }
    
    /* Create merged disk */
    uft_disk_image_t *merged = NULL;
    if (adf_read_mem(ctx->revisions[0].data, ctx->revisions[0].size, 
                     &merged) != UFT_OK) {
        free(analyses);
        return UFT_ERR_MEMORY;
    }
    
    /* For each sector, pick best from revisions */
    for (size_t t = 0; t < merged->track_count; t++) {
        uft_track_t *track = merged->track_data[t];
        if (!track) continue;
        
        for (size_t s = 0; s < track->sector_count; s++) {
            adf_sector_analysis_t sector_analyses[16];
            
            for (size_t r = 0; r < ctx->revision_count && r < 16; r++) {
                sector_analyses[r] = analyses[r].track_analysis[t].sectors[s];
            }
            
            int best = adf_decide_sector(sector_analyses, ctx->revision_count);
            
            if (best > 0 && (size_t)best < ctx->revision_count) {
                /* Copy sector from best revision */
                const adf_revision_t *rev = &ctx->revisions[best];
                size_t offset = (t * ADF_SECTORS_PER_TRACK + s) * ADF_SECTOR_SIZE;
                
                if (offset + ADF_SECTOR_SIZE <= rev->size) {
                    memcpy(track->sectors[s].data, rev->data + offset, 
                           ADF_SECTOR_SIZE);
                }
            }
        }
    }
    
    free(analyses);
    
    if (ctx->disk) {
        uft_disk_free(ctx->disk);
    }
    ctx->disk = merged;
    
    return UFT_OK;
}

uft_error_t adf_merge_revisions(const adf_revision_t *revisions,
                                size_t count,
                                uint8_t *out_data,
                                size_t *out_size) {
    if (!revisions || count == 0 || !out_data || !out_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Use first revision as base */
    size_t size = revisions[0].size;
    memcpy(out_data, revisions[0].data, size);
    
    /* TODO: Implement byte-level merge with confidence */
    
    *out_size = size;
    return UFT_OK;
}

/* ============================================================================
 * Stage 4: PRESERVE
 * ============================================================================ */

uft_error_t adf_preserve(const uft_disk_image_t *disk,
                         const adf_disk_analysis_t *analysis,
                         uft_disk_image_t *out_preserved) {
    if (!disk || !out_preserved) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Copy disk structure */
    out_preserved->format = disk->format;
    snprintf(out_preserved->format_name, sizeof(out_preserved->format_name), "%s", disk->format_name);
    out_preserved->tracks = disk->tracks;
    out_preserved->heads = disk->heads;
    out_preserved->sectors_per_track = disk->sectors_per_track;
    out_preserved->bytes_per_sector = disk->bytes_per_sector;
    
    /* Copy track data */
    for (size_t t = 0; t < disk->track_count; t++) {
        if (!disk->track_data[t]) continue;
        
        out_preserved->track_data[t] = uft_track_alloc(
            disk->track_data[t]->sector_capacity,
            disk->track_data[t]->raw_capacity * 8);
        
        if (!out_preserved->track_data[t]) {
            return UFT_ERR_MEMORY;
        }
        
        uft_track_copy(out_preserved->track_data[t], disk->track_data[t]);
        
        /* Add analysis metadata if available */
        if (analysis && t < ADF_TRACKS_HD) {
            out_preserved->track_data[t]->quality = 
                analysis->track_analysis[t].quality;
            out_preserved->track_data[t]->complete = 
                analysis->track_analysis[t].complete;
        }
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Stage 5: WRITE
 * ============================================================================ */

uft_error_t adf_write(const uft_disk_image_t *disk, const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t file_size = ADF_FILE_SIZE_DD;
    if (disk->tracks > ADF_TRACKS_DD / 2) {
        file_size = ADF_FILE_SIZE_HD;
    }
    
    uint8_t *buffer = malloc(file_size);
    if (!buffer) {
        return UFT_ERR_MEMORY;
    }
    
    memset(buffer, 0, file_size);
    
    /* Copy sectors to buffer */
    size_t pos = 0;
    
    for (size_t t = 0; t < disk->track_count && pos < file_size; t++) {
        uft_track_t *track = disk->track_data[t];
        if (!track) {
            pos += ADF_SECTORS_PER_TRACK * ADF_SECTOR_SIZE;
            continue;
        }
        
        for (size_t s = 0; s < track->sector_count && 
                          s < ADF_SECTORS_PER_TRACK &&
                          pos < file_size; s++) {
            if (track->sectors[s].data) {
                memcpy(buffer + pos, track->sectors[s].data, ADF_SECTOR_SIZE);
            }
            pos += ADF_SECTOR_SIZE;
        }
        
        /* Pad remaining sectors */
        while (pos % (ADF_SECTORS_PER_TRACK * ADF_SECTOR_SIZE) != 0 &&
               pos < file_size) {
            pos += ADF_SECTOR_SIZE;
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(buffer);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(buffer, 1, file_size, fp);
    fclose(fp);
    free(buffer);
    
    if (written != file_size) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Full Pipeline
 * ============================================================================ */

uft_error_t adf_pipeline_run_stage(adf_pipeline_ctx_t *ctx, int stage) {
    if (!ctx) return UFT_ERR_INVALID_PARAM;
    
    uft_error_t err = UFT_OK;
    
    switch (stage) {
        case ADF_STAGE_ANALYZE:
            if (ctx->disk) {
                err = adf_analyze(ctx->disk, &ctx->analysis);
            }
            break;
            
        case ADF_STAGE_DECIDE:
            err = adf_decide(ctx);
            break;
            
        default:
            break;
    }
    
    if (err == UFT_OK) {
        ctx->stage = stage + 1;
    }
    
    return err;
}

uft_error_t adf_pipeline_run(adf_pipeline_ctx_t *ctx,
                             const char *input_path,
                             const char *output_path) {
    if (!ctx) return UFT_ERR_INVALID_PARAM;
    
    uft_error_t err;
    
    /* Stage 1: Read */
    if (input_path) {
        err = adf_read(input_path, &ctx->disk);
        if (err != UFT_OK) return err;
    } else if (ctx->revision_count > 0) {
        err = adf_read_mem(ctx->revisions[0].data, 
                           ctx->revisions[0].size, 
                           &ctx->disk);
        if (err != UFT_OK) return err;
    } else {
        return UFT_ERR_INVALID_PARAM;
    }
    ctx->stage = ADF_STAGE_READ;
    
    /* Stage 2: Analyze */
    err = adf_analyze(ctx->disk, &ctx->analysis);
    if (err != UFT_OK) return err;
    ctx->stage = ADF_STAGE_ANALYZE;
    
    /* Stage 3: Decide (multi-rev) */
    if (ctx->opts.use_multi_rev && ctx->revision_count > 1) {
        err = adf_decide(ctx);
        if (err != UFT_OK) return err;
    }
    ctx->stage = ADF_STAGE_DECIDE;
    
    /* Stage 4: Preserve */
    /* (preserved data is already in ctx->disk) */
    ctx->stage = ADF_STAGE_PRESERVE;
    
    /* Stage 5: Write */
    if (output_path) {
        err = adf_write(ctx->disk, output_path);
        if (err != UFT_OK) return err;
    }
    ctx->stage = ADF_STAGE_WRITE;
    
    ctx->stage = ADF_STAGE_DONE;
    return UFT_OK;
}
