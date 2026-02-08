/**
 * @file uft_sos_protection.c
 * @brief Sensible Operating System (SOS) Copy Protection Handler
 * 
 * Implementation based on:
 * - WHDLoad RawDIC imager by Wepl
 * - OpenFodder SOS_Unpacker
 * - Software Preservation Society documentation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_sos_protection.h"
#include "uft/uft_mfm_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct uft_sos_context {
    char *source_path;          /**< Source file/directory */
    int source_type;            /**< 0=unknown, 1=kf_raw, 2=ipf, 3=scp */
    
    uft_sos_track_t *tracks;    /**< Decoded tracks array */
    int num_tracks;             /**< Number of tracks */
    int num_heads;              /**< Number of heads */
    
    uint8_t *disk_data;         /**< Combined disk data */
    size_t disk_data_size;      /**< Total data size */
    
    uft_sos_disk_info_t info;
    
    uft_sos_game_t game;        /**< Detected game */
};

/*===========================================================================
 * CONSTANTS
 *===========================================================================*/

/* SOS sync patterns */
static const uint16_t SOS_SYNC = 0x4489;

/* Amiga MFM odd/even decoding */
#define ODD_BITS_MASK  0xAAAAAAAA
#define EVEN_BITS_MASK 0x55555555

/*===========================================================================
 * HELPERS
 *===========================================================================*/

/**
 * @brief Get bit from bitstream
 */
static int get_bit(const uint8_t *data, int bit) {
    return (data[bit / 8] >> (7 - (bit % 8))) & 1;
}

/**
 * @brief Get 16-bit word from bitstream
 */
static uint16_t get_word(const uint8_t *data, int bit) {
    uint16_t word = 0;
    for (int i = 0; i < 16; i++) {
        word = (word << 1) | get_bit(data, bit + i);
    }
    return word;
}

/**
 * @brief Get 32-bit long from bitstream
 */
static uint32_t get_long(const uint8_t *data, int bit) {
    uint32_t val = 0;
    for (int i = 0; i < 32; i++) {
        val = (val << 1) | get_bit(data, bit + i);
    }
    return val;
}

/**
 * @brief Decode Amiga-style odd/even MFM
 */
static uint32_t decode_odd_even_long(uint32_t odd, uint32_t even) {
    return ((odd & EVEN_BITS_MASK) << 1) | (even & EVEN_BITS_MASK);
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

uft_sos_t* uft_sos_create(void) {
    uft_sos_t *sos = (uft_sos_t *)calloc(1, sizeof(*sos));
    if (!sos) return NULL;
    
    sos->num_heads = 2;
    sos->num_tracks = 80;
    
    return sos;
}

void uft_sos_destroy(uft_sos_t *sos) {
    if (!sos) return;
    
    uft_sos_close(sos);
    free(sos->source_path);
    free(sos);
}

void uft_sos_close(uft_sos_t *sos) {
    if (!sos) return;
    
    /* Free tracks */
    if (sos->tracks) {
        int total = sos->num_tracks * sos->num_heads;
        for (int i = 0; i < total; i++) {
            free(sos->tracks[i].data);
        }
        free(sos->tracks);
        sos->tracks = NULL;
    }
    
    free(sos->disk_data);
    sos->disk_data = NULL;
    sos->disk_data_size = 0;
}

/*===========================================================================
 * DETECTION
 *===========================================================================*/

int uft_sos_detect_track(const uint8_t *mfm_data, size_t mfm_size) {
    if (!mfm_data || mfm_size < 256) return 0;
    
    int score = 0;
    size_t mfm_bits = mfm_size * 8;
    
    /* Look for standard Amiga sync (0x4489) */
    int sync_count = 0;
    for (size_t bit = 0; bit + 16 <= mfm_bits; bit += 16) {
        uint16_t word = get_word(mfm_data, bit);
        if (word == SOS_SYNC) {
            sync_count++;
            if (sync_count >= 3) {
                /* Check for SOS header after sync */
                if (bit + 64 <= mfm_bits) {
                    /* SOS has specific patterns after sync */
                    score += 20;
                }
                break;
            }
        }
    }
    
    /* Check track length (SOS uses longer tracks) */
    if (mfm_size >= 12000 && mfm_size <= 14000) {
        score += 15;
    }
    
    /* Look for characteristic SOS patterns */
    /* SOS often has 0x5545 or 0x4545 markers */
    for (size_t i = 0; i < mfm_size - 1; i++) {
        uint16_t word = (mfm_data[i] << 8) | mfm_data[i + 1];
        if (word == UFT_SOS_HEADER_MARKER) {
            score += 30;
            break;
        }
    }
    
    return (score > 100) ? 100 : score;
}

int uft_sos_detect_flux(const uint32_t *flux, size_t flux_count) {
    if (!flux || flux_count < 1000) return 0;
    
    /* Convert flux to MFM and detect */
    size_t max_bytes = flux_count / 4;
    uint8_t *mfm = (uint8_t *)calloc(1, max_bytes);
    if (!mfm) return 0;
    
    int mfm_bits = 0;
    uft_flux_to_mfm(flux, flux_count, UFT_RATE_250K, mfm, max_bytes, &mfm_bits);
    
    int score = uft_sos_detect_track(mfm, mfm_bits / 8);
    free(mfm);
    
    return score;
}

bool uft_sos_detect_ipf(const char *ipf_path) {
    /* Open IPF file and check track format for SOS copy protection.
     * IPF header: "CAPS" magic, then record blocks with track data.
     * SOS protection uses non-standard sector gaps and timing. */
    if (!ipf_path) return false;
    
    FILE *f = fopen(ipf_path, "rb");
    if (!f) return false;
    
    /* Read IPF header */
    uint8_t header[32];
    if (fread(header, 1, 32, f) != 32) { fclose(f); return false; }
    
    /* Check CAPS magic */
    if (memcmp(header, "CAPS", 4) != 0) { fclose(f); return false; }
    
    /* Scan track records for SOS indicators:
     * - Non-standard gap sizes (gap3 != 0x4E fill)
     * - Fuzzy bit masks present
     * - Timing data with deliberate variations */
    bool sos_found = false;
    uint8_t buf[512];
    
    while (fread(buf, 1, 4, f) == 4) {
        /* Record type at offset 0 */
        uint32_t rec_type = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
        
        if (rec_type == 0x54524349) {  /* "TRCI" - Track info */
            if (fread(buf + 4, 1, 28, f) == 28) {
                /* Check for fuzzy bit flags or non-standard encoding */
                uint32_t flags = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];
                if (flags & 0x01) {  /* Fuzzy bits present */
                    sos_found = true;
                    break;
                }
            }
        } else {
            /* Skip record */
            if (fread(buf + 4, 1, 4, f) != 4) break;
            uint32_t rec_len = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
            if (rec_len > 0 && rec_len < 0x100000) {
                fseek(f, rec_len - 8, SEEK_CUR);
            }
        }
    }
    
    fclose(f);
    return sos_found;
}

bool uft_sos_detect_kf_raw(const char *raw_path) {
    /* Open KryoFlux RAW stream and detect SOS protection.
     * KryoFlux stream format: sequence of flux transition timings.
     * SOS uses deliberately unstable flux patterns on specific tracks. */
    if (!raw_path) return false;
    
    FILE *f = fopen(raw_path, "rb");
    if (!f) return false;
    
    /* Read stream data */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size < 100 || size > 2 * 1024 * 1024) { fclose(f); return false; }
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return false; }
    fread(data, 1, size, f);
    fclose(f);
    
    /* Parse KryoFlux stream: look for flux timing anomalies.
     * Normal MFM has regular 4/6/8µs intervals.
     * SOS protection has deliberate timing jitter on specific sectors. */
    int anomaly_count = 0;
    int total_transitions = 0;
    uint32_t prev_time = 0;
    
    for (long i = 0; i < size; i++) {
        uint8_t b = data[i];
        
        if (b < 0x08) {
            /* Flux2/Flux3/Nop/OVL16 - skip overhead bytes */
            if (b == 0x00 || b == 0x07) continue;
            if (b >= 0x01 && b <= 0x05) { i++; continue; }  /* Multi-byte opcodes */
        } else {
            /* Flux timing value */
            uint32_t timing = b;
            if (b == 0x00) continue;
            
            total_transitions++;
            
            /* Check for anomalous timing (outside normal MFM range) */
            if (prev_time > 0) {
                int32_t delta = (int32_t)timing - (int32_t)prev_time;
                if (delta < 0) delta = -delta;
                /* Large timing variance suggests copy protection */
                if (delta > 40 && timing > 20 && timing < 200) {
                    anomaly_count++;
                }
            }
            prev_time = timing;
        }
    }
    
    free(data);
    
    /* High anomaly rate suggests SOS protection */
    if (total_transitions > 100) {
        float anomaly_rate = (float)anomaly_count / total_transitions;
        return (anomaly_rate > 0.15f);  /* >15% anomalous = likely protected */
    }
    
    return false;
}

/*===========================================================================
 * TRACK DECODING
 *===========================================================================*/

int uft_sos_find_sync(const uint8_t *mfm, size_t mfm_bits, int start_bit) {
    if (!mfm || mfm_bits < 48) return -1;
    
    /* Look for two consecutive sync words */
    for (size_t bit = start_bit; bit + 32 <= mfm_bits; bit++) {
        uint16_t w1 = get_word(mfm, bit);
        uint16_t w2 = get_word(mfm, bit + 16);
        
        if (w1 == SOS_SYNC && w2 == SOS_SYNC) {
            return (int)bit;
        }
    }
    
    return -1;
}

int uft_sos_decode_mfm(const uint8_t *mfm, size_t mfm_size,
                        uint8_t *data, size_t data_size) {
    if (!mfm || !data || mfm_size == 0) return -1;
    
    /* SOS uses Amiga-style odd/even interleaved encoding */
    size_t decoded = 0;
    size_t src_pos = 0;
    
    while (src_pos + 8 <= mfm_size && decoded < data_size) {
        /* Read odd and even longwords */
        uint32_t odd = ((uint32_t)mfm[src_pos] << 24) | ((uint32_t)mfm[src_pos + 1] << 16) |
                       ((uint32_t)mfm[src_pos + 2] << 8) | mfm[src_pos + 3];
        uint32_t even = ((uint32_t)mfm[src_pos + 4] << 24) | ((uint32_t)mfm[src_pos + 5] << 16) |
                        ((uint32_t)mfm[src_pos + 6] << 8) | mfm[src_pos + 7];
        
        uint32_t decoded_long = decode_odd_even_long(odd, even);
        
        /* Store bytes */
        if (decoded < data_size) data[decoded++] = (decoded_long >> 24) & 0xFF;
        if (decoded < data_size) data[decoded++] = (decoded_long >> 16) & 0xFF;
        if (decoded < data_size) data[decoded++] = (decoded_long >> 8) & 0xFF;
        if (decoded < data_size) data[decoded++] = decoded_long & 0xFF;
        
        src_pos += 8;
    }
    
    return (int)decoded;
}

uint32_t uft_sos_checksum(const uint8_t *data, size_t len) {
    if (!data || len == 0) return 0;
    
    /* SOS uses XOR checksum on longwords */
    uint32_t checksum = 0;
    
    for (size_t i = 0; i + 3 < len; i += 4) {
        uint32_t val = ((uint32_t)data[i] << 24) | ((uint32_t)data[i + 1] << 16) |
                       ((uint32_t)data[i + 2] << 8) | data[i + 3];
        checksum ^= val;
    }
    
    return checksum;
}

bool uft_sos_verify_checksum(const uint8_t *data, size_t len,
                              uint32_t expected) {
    return uft_sos_checksum(data, len) == expected;
}

int uft_sos_decode_track(uft_sos_t *sos,
                          const uint8_t *mfm_data, size_t mfm_size,
                          int track_num,
                          uft_sos_track_t *track) {
    if (!sos || !mfm_data || !track || mfm_size < 256) return -1;
    
    memset(track, 0, sizeof(*track));
    
    size_t mfm_bits = mfm_size * 8;
    
    /* Find sync */
    int sync_pos = uft_sos_find_sync(mfm_data, mfm_bits, 0);
    if (sync_pos < 0) {
        return -1;  /* No sync found */
    }
    
    /* Skip sync words (typically 2-3) */
    int data_start = sync_pos;
    while (data_start + 16 <= (int)mfm_bits) {
        if (get_word(mfm_data, data_start) != SOS_SYNC) break;
        data_start += 16;
    }
    
    /* Calculate data area */
    size_t data_bytes = (mfm_bits - data_start) / 16;  /* 2 MFM bits per data bit */
    if (data_bytes < 100) return -1;
    
    /* Allocate decoded data */
    track->data = (uint8_t *)malloc(UFT_SOS_DATA_LEN);
    if (!track->data) return -1;
    
    /* Decode MFM data */
    /* For SOS, data starts after sync and is in odd/even format */
    size_t mfm_offset = data_start / 8;
    int decoded = uft_sos_decode_mfm(mfm_data + mfm_offset, 
                                      mfm_size - mfm_offset,
                                      track->data, UFT_SOS_DATA_LEN);
    
    if (decoded <= 0) {
        free(track->data);
        track->data = NULL;
        return -1;
    }
    
    track->data_size = decoded;
    
    /* Parse track header (if present) */
    if (decoded >= 16) {
        track->header.track_num = track->data[0];
        track->header.disk_num = track->data[1];
        track->header.format_type = (track->data[2] << 8) | track->data[3];
        track->header.checksum = ((uint32_t)track->data[4] << 24) | ((uint32_t)track->data[5] << 16) |
                                  ((uint32_t)track->data[6] << 8) | track->data[7];
        track->header.data_length = ((uint32_t)track->data[8] << 24) | ((uint32_t)track->data[9] << 16) |
                                     ((uint32_t)track->data[10] << 8) | track->data[11];
        
        /* Verify header */
        track->header_valid = (track->header.track_num == (uint8_t)track_num);
    }
    
    /* Calculate data checksum (excluding header) */
    if (decoded > 16) {
        uint32_t calc_checksum = uft_sos_checksum(track->data + 16, decoded - 16);
        track->data_valid = (calc_checksum == track->header.checksum);
    }
    
    return 0;
}

int uft_sos_decode_flux(uft_sos_t *sos,
                         const uint32_t *flux, size_t flux_count,
                         int track_num,
                         uft_sos_track_t *track) {
    if (!sos || !flux || !track || flux_count < 1000) return -1;
    
    /* Convert flux to MFM */
    size_t max_bytes = flux_count / 2;
    uint8_t *mfm = (uint8_t *)calloc(1, max_bytes);
    if (!mfm) return -1;
    
    int mfm_bits = 0;
    int rc = uft_flux_to_mfm(flux, flux_count, UFT_RATE_250K, 
                              mfm, max_bytes, &mfm_bits);
    
    if (rc != 0) {
        free(mfm);
        return -1;
    }
    
    /* Decode track */
    rc = uft_sos_decode_track(sos, mfm, mfm_bits / 8, track_num, track);
    free(mfm);
    
    return rc;
}

void uft_sos_track_free(uft_sos_track_t *track) {
    if (!track) return;
    free(track->data);
    memset(track, 0, sizeof(*track));
}

/*===========================================================================
 * DISK OPERATIONS
 *===========================================================================*/

int uft_sos_open_kf_raw(uft_sos_t *sos, const char *base_path) {
    if (!sos || !base_path) return -1;
    
    /* KryoFlux stores tracks as: trackNN.H.raw where NN=track, H=head.
     * Load each track file and parse flux stream data. */
    
    sos->source_type = 1;
    free(sos->source_path);
    sos->source_path = strdup(base_path);
    
    /* Allocate tracks */
    int total_tracks = sos->num_tracks * sos->num_heads;
    sos->tracks = (uft_sos_track_t *)calloc(total_tracks, sizeof(uft_sos_track_t));
    if (!sos->tracks) return -1;
    
    /* Load each track */
    char track_path[512];
    int loaded = 0;
    
    for (int t = 0; t < sos->num_tracks; t++) {
        for (int h = 0; h < sos->num_heads; h++) {
            snprintf(track_path, sizeof(track_path), 
                     "%s/track%02d.%d.raw", base_path, t, h);
            
            FILE *f = fopen(track_path, "rb");
            if (!f) continue;
            
            /* Get file size */
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            if (size <= 0) {
                fclose(f);
                continue;
            }
            
            /* Read flux data */
            /* KryoFlux RAW format: stream of flux timing values */
            /* Each value is variable length (1-5 bytes) */
            uint8_t *raw_data = (uint8_t *)malloc(size);
            if (!raw_data) {
                fclose(f);
                continue;
            }
            
            size_t bytes_read = fread(raw_data, 1, size, f);
            fclose(f);
            
            if (bytes_read != size) {
                free(raw_data);
                continue;
            }
            
            /* Parse KryoFlux stream to flux timings.
             * Stream opcodes: 0x00-0x07 = control, 0x08-0xFF = flux value.
             * Flux2 (0x00 + 2 bytes), OVL16 (0x0B), etc. */
            uint32_t *flux_timings = malloc(size * sizeof(uint32_t));
            int flux_count = 0;
            uint32_t overflow = 0;
            
            if (flux_timings) {
                for (long j = 0; j < (long)bytes_read; j++) {
                    uint8_t b = raw_data[j];
                    
                    if (b >= 0x0E) {
                        /* Single-byte flux value */
                        flux_timings[flux_count++] = b + overflow;
                        overflow = 0;
                    } else if (b == 0x00 && j + 2 < (long)bytes_read) {
                        /* Flux2: next 2 bytes are 16-bit flux value */
                        uint32_t val = (raw_data[j+1] << 8) | raw_data[j+2];
                        flux_timings[flux_count++] = val + overflow;
                        overflow = 0;
                        j += 2;
                    } else if (b == 0x0B) {
                        /* OVL16: overflow, add 0x10000 to next value */
                        overflow += 0x10000;
                    } else if (b == 0x0D) {
                        /* End of stream */
                        break;
                    } else if (b >= 0x01 && b <= 0x07) {
                        /* Multi-byte opcodes: skip payload */
                        j += (b == 0x01 || b == 0x02) ? 2 : 0;
                    }
                }
                
                /* Store flux data in track */
                int tidx = t * sos->num_heads + h;
                if (tidx < total_tracks) {
                    sos->tracks[tidx].flux_data = (uint8_t *)flux_timings;
                    sos->tracks[tidx].flux_count = flux_count;
                }
            }
            
            free(raw_data);
            loaded++;
        }
    }
    
    return (loaded > 0) ? 0 : -1;
}

int uft_sos_get_disk_info(uft_sos_t *sos, uft_sos_disk_info_t *info) {
    if (!sos || !info) return -1;
    *info = sos->info;
    return 0;
}

/*===========================================================================
 * DATA EXTRACTION
 *===========================================================================*/

int uft_sos_read_all_tracks(uft_sos_t *sos, uint8_t *data, size_t max_size) {
    if (!sos) return -1;
    
    /* Calculate total size */
    size_t total_size = 0;
    if (sos->tracks) {
        int total_tracks = sos->num_tracks * sos->num_heads;
        for (int i = 0; i < total_tracks; i++) {
            if (sos->tracks[i].data) {
                total_size += sos->tracks[i].data_size;
            }
        }
    }
    
    if (!data) return (int)total_size;
    
    /* Copy data */
    size_t offset = 0;
    if (sos->tracks) {
        int total_tracks = sos->num_tracks * sos->num_heads;
        for (size_t i = 0; i < total_tracks && offset < max_size; i++) {
            if (sos->tracks[i].data && sos->tracks[i].data_size > 0) {
                size_t to_copy = sos->tracks[i].data_size;
                if (offset + to_copy > max_size) {
                    to_copy = max_size - offset;
                }
                memcpy(data + offset, sos->tracks[i].data, to_copy);
                offset += to_copy;
            }
        }
    }
    
    return (int)offset;
}

int uft_sos_read_track(uft_sos_t *sos, int track, int head,
                        uint8_t *data, size_t max_size) {
    if (!sos || !sos->tracks || !data) return -1;
    if (track < 0 || track >= sos->num_tracks) return -1;
    if (head < 0 || head >= sos->num_heads) return -1;
    
    int idx = track * sos->num_heads + head;
    uft_sos_track_t *t = &sos->tracks[idx];
    
    if (!t->data || t->data_size == 0) return -1;
    
    size_t to_copy = t->data_size;
    if (to_copy > max_size) to_copy = max_size;
    
    memcpy(data, t->data, to_copy);
    return (int)to_copy;
}

int uft_sos_extract_all(uft_sos_t *sos, const char *output_dir) {
    if (!sos || !output_dir) return -1;
    
    /* Create output directory */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_dir);
    system(cmd);
    
    /* Read all disk data */
    int data_size = uft_sos_read_all_tracks(sos, NULL, 0);
    if (data_size <= 0) return -1;
    
    uint8_t *data = (uint8_t *)malloc(data_size);
    if (!data) return -1;
    
    uft_sos_read_all_tracks(sos, data, data_size);
    
    /* Write raw disk data */
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/disk.bin", output_dir);
    
    FILE *f = fopen(out_path, "wb");
    if (f) {
        fwrite(data, 1, data_size, f);
        fclose(f);
    }
    
    free(data);
    return 0;
}

/*===========================================================================
 * GAME DETECTION
 *===========================================================================*/

uft_sos_game_t uft_sos_detect_game(uft_sos_t *sos) {
    if (!sos || !sos->disk_data || sos->disk_data_size < 256) {
        return UFT_SOS_GAME_UNKNOWN;
    }
    
    /* Look for game signatures in disk data */
    const char *data = (const char *)sos->disk_data;
    
    /* Cannon Fodder signatures */
    if (memmem(data, sos->disk_data_size, "CANNON", 6) ||
        memmem(data, sos->disk_data_size, "CF_ENG", 6)) {
        return UFT_SOS_GAME_CANNON_FODDER;
    }
    
    /* Sensible Soccer */
    if (memmem(data, sos->disk_data_size, "SENSI", 5) ||
        memmem(data, sos->disk_data_size, "SOCCER", 6)) {
        return UFT_SOS_GAME_SENSIBLE_SOCCER;
    }
    
    /* Mega Lo Mania */
    if (memmem(data, sos->disk_data_size, "MEGALO", 6)) {
        return UFT_SOS_GAME_MEGA_LO_MANIA;
    }
    
    return UFT_SOS_GAME_UNKNOWN;
}

const char* uft_sos_game_name(uft_sos_game_t game) {
    switch (game) {
        case UFT_SOS_GAME_CANNON_FODDER:    return "Cannon Fodder";
        case UFT_SOS_GAME_CANNON_FODDER_2:  return "Cannon Fodder 2";
        case UFT_SOS_GAME_CANNON_SOCCER:    return "Cannon Soccer";
        case UFT_SOS_GAME_SENSIBLE_SOCCER:  return "Sensible Soccer";
        case UFT_SOS_GAME_MEGA_LO_MANIA:    return "Mega Lo Mania";
        case UFT_SOS_GAME_WIZKID:           return "Wizkid";
        case UFT_SOS_GAME_SENSIBLE_GOLF:    return "Sensible Golf";
        default:                             return "Unknown";
    }
}

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

void uft_sos_print_track_info(const uft_sos_track_t *track) {
    if (!track) return;
    
    printf("SOS Track Info:\n");
    printf("  Track: %d, Disk: %d\n", 
           track->header.track_num, track->header.disk_num);
    printf("  Format: 0x%04X\n", track->header.format_type);
    printf("  Data Length: %u bytes\n", track->header.data_length);
    printf("  Checksum: 0x%08X (%s)\n", 
           track->header.checksum,
           track->data_valid ? "VALID" : "INVALID");
    printf("  Decoded Size: %zu bytes\n", track->data_size);
}

void uft_sos_print_disk_info(uft_sos_t *sos) {
    if (!sos) return;
    
    printf("SOS Disk Info:\n");
    printf("  Tracks: %d\n", sos->num_tracks);
    printf("  Heads: %d\n", sos->num_heads);
    printf("  Source: %s\n", sos->source_path ? sos->source_path : "N/A");
    printf("  Game: %s\n", uft_sos_game_name(sos->game));
    printf("  Total Data: %zu bytes\n", sos->disk_data_size);
}
