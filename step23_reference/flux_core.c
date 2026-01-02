/**
 * @file flux_core.c
 * @brief Implementation of Flux Core Data Structures
 */

#include "uft/flux_core.h"
#include "uft/uft_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/* =============================================================================
 * FLUX BITSTREAM IMPLEMENTATION
 * ============================================================================= */

flux_bitstream_t* flux_bitstream_create(size_t initial_capacity)
{
    flux_bitstream_t *bitstream = calloc(1, sizeof(flux_bitstream_t));
    if (!bitstream) {
        return NULL;
    }
    
    size_t byte_capacity = (initial_capacity + 7) / 8;
    if (byte_capacity == 0) {
        byte_capacity = 1024;  /* Default: 1KB */
    }
    
    bitstream->bits = malloc(byte_capacity);
    if (!bitstream->bits) {
        free(bitstream);
        return NULL;
    }
    
    memset(bitstream->bits, 0, byte_capacity);
    bitstream->byte_capacity = byte_capacity;
    bitstream->bit_count = 0;
    bitstream->encoding = BITSTREAM_RAW;
    bitstream->sync_patterns_found = 0;
    bitstream->decode_errors = 0;
    
    return bitstream;
}

void flux_bitstream_destroy(flux_bitstream_t *bitstream)
{
    if (bitstream) {
        if (bitstream->bits) {
            free(bitstream->bits);
        }
        free(bitstream);
    }
}

bool flux_bitstream_append_bit(flux_bitstream_t *bitstream, uint8_t bit)
{
    if (!bitstream) {
        return false;
    }
    
    /* Check if we need to expand */
    size_t needed_bytes = (bitstream->bit_count + 1 + 7) / 8;
    if (needed_bytes > bitstream->byte_capacity) {
        size_t new_capacity = bitstream->byte_capacity ? bitstream->byte_capacity : 1;
        while (new_capacity < needed_bytes) {
            if (new_capacity > SIZE_MAX / 2) {
                return false;
            }
            new_capacity *= 2;
        }
        uint8_t *new_bits = realloc(bitstream->bits, new_capacity);
        if (!new_bits) {
            return false;
        }
        
        /* Zero out new space */
        memset(new_bits + bitstream->byte_capacity, 0, 
               new_capacity - bitstream->byte_capacity);
        
        bitstream->bits = new_bits;
        bitstream->byte_capacity = new_capacity;
    }
    
    /* Set bit */
    size_t byte_idx = bitstream->bit_count / 8;
    size_t bit_idx = 7 - (bitstream->bit_count % 8);
    
    if (bit) {
        bitstream->bits[byte_idx] |= (1 << bit_idx);
    } else {
        bitstream->bits[byte_idx] &= ~(1 << bit_idx);
    }
    
    bitstream->bit_count++;
    return true;
}

bool flux_bitstream_append_byte(flux_bitstream_t *bitstream, uint8_t byte)
{
    for (int i = 7; i >= 0; i--) {
        if (!flux_bitstream_append_bit(bitstream, (byte >> i) & 1)) {
            return false;
        }
    }
    return true;
}

/* =============================================================================
 * FLUX TRACK IMPLEMENTATION
 * ============================================================================= */

flux_track_t* flux_track_create(int cylinder, int head)
{
    flux_track_t *track = calloc(1, sizeof(flux_track_t));
    if (!track) {
        return NULL;
    }
    
    track->cylinder = cylinder;
    track->head = head;
    track->sample_capacity = 10000;  /* Initial capacity */
    
    track->samples = malloc(track->sample_capacity * sizeof(flux_sample_t));
    if (!track->samples) {
        free(track);
        return NULL;
    }
    
    track->sample_count = 0;
    track->track_duration_ns = 0;
    track->index_count = 0;
    track->signal_quality = 0.0f;
    track->weak_bits = 0;
    track->read_retries = 0;
    track->bitstream = NULL;
    track->refcount = 1;
    
    return track;
}

void flux_track_destroy(flux_track_t **track_ptr)
{
    if (!track_ptr || !*track_ptr) {
        return;
    }
    
    flux_track_t *track = *track_ptr;
    
    track->refcount--;
    if (track->refcount <= 0) {
        if (track->samples) {
            free(track->samples);
        }
        
        if (track->bitstream) {
            flux_bitstream_destroy(track->bitstream);
        }
        
        free(track);
        *track_ptr = NULL;
    }
}

bool flux_track_add_sample(flux_track_t *track, uint64_t timestamp_ns, uint8_t index)
{
    if (!track) {
        return false;
    }
    
    /* Expand if needed */
    if (track->sample_count >= track->sample_capacity) {
        size_t new_capacity = track->sample_capacity ? track->sample_capacity : 1;
        if (new_capacity > SIZE_MAX / 2) {
            return false;
        }
        new_capacity *= 2;
        if (new_capacity > SIZE_MAX / sizeof(flux_sample_t)) {
            return false;
        }
        flux_sample_t *new_samples = realloc(track->samples,
                                             new_capacity * sizeof(flux_sample_t));
        if (!new_samples) {
            return false;
        }
        
        track->samples = new_samples;
        track->sample_capacity = new_capacity;
    }
    
    /* Add sample */
    track->samples[track->sample_count].timestamp_ns = timestamp_ns;
    track->samples[track->sample_count].index_pulse = index;
    track->sample_count++;
    
    /* Update duration */
    if (timestamp_ns > track->track_duration_ns) {
        track->track_duration_ns = timestamp_ns;
    }
    
    /* Count index pulses */
    if (index) {
        track->index_count++;
    }
    
    return true;
}

float flux_track_get_rpm(const flux_track_t *track)
{
    if (!track || track->index_count < 2) {
        return 0.0f;
    }
    
    /* Find time between first two index pulses */
    uint64_t first_index = 0;
    uint64_t second_index = 0;
    int index_found = 0;
    
    for (size_t i = 0; i < track->sample_count; i++) {
        if (track->samples[i].index_pulse) {
            if (index_found == 0) {
                first_index = track->samples[i].timestamp_ns;
                index_found = 1;
            } else if (index_found == 1) {
                second_index = track->samples[i].timestamp_ns;
                break;
            }
        }
    }
    
    if (index_found < 2) {
        return 0.0f;
    }
    
    /* RPM = 60 seconds / revolution_time */
    uint64_t rev_time_ns = second_index - first_index;
    double rev_time_sec = rev_time_ns / 1000000000.0;
    
    return (float)(60.0 / rev_time_sec);
}

/* =============================================================================
 * FLUX DISK IMPLEMENTATION
 * ============================================================================= */

flux_disk_t* flux_disk_create(int cylinders, int heads)
{
    if (cylinders <= 0 || heads <= 0) {
        return NULL;
    }

    flux_disk_t *disk = calloc(1, sizeof(flux_disk_t));
    if (!disk) {
        return NULL;
    }
    
    disk->max_cylinders = cylinders;
    disk->max_heads = heads;
    if ((size_t)cylinders > SIZE_MAX / (size_t)heads) {
        free(disk);
        return NULL;
    }
    disk->track_count = (size_t)cylinders * (size_t)heads;
    
    disk->tracks = calloc(disk->track_count, sizeof(flux_track_t*));
    if (!disk->tracks) {
        free(disk);
        return NULL;
    }
    
    disk->name = NULL;
    disk->source_file = NULL;
    disk->format = DISK_FORMAT_UNKNOWN;
    disk->total_size_bytes = 0;
    disk->overall_quality = 0.0f;
    disk->refcount = 1;
    
    return disk;
}

void flux_disk_destroy(flux_disk_t **disk_ptr)
{
    if (!disk_ptr || !*disk_ptr) {
        return;
    }
    
    flux_disk_t *disk = *disk_ptr;
    
    disk->refcount--;
    if (disk->refcount <= 0) {
        /* Free all tracks */
        if (disk->tracks) {
            for (size_t i = 0; i < disk->track_count; i++) {
                if (disk->tracks[i]) {
                    flux_track_destroy(&disk->tracks[i]);
                }
            }
            free(disk->tracks);
        }
        
        if (disk->name) {
            free(disk->name);
        }
        
        if (disk->source_file) {
            free(disk->source_file);
        }
        
        free(disk);
        *disk_ptr = NULL;
    }
}

flux_track_t* flux_disk_get_track(flux_disk_t *disk, int cylinder, int head)
{
    if (!disk || cylinder < 0 || head < 0 ||
        cylinder >= disk->max_cylinders || head >= disk->max_heads) {
        return NULL;
    }
    
    size_t index = cylinder * disk->max_heads + head;
    return disk->tracks[index];
}

bool flux_disk_set_track(flux_disk_t *disk, int cylinder, int head, flux_track_t *track)
{
    if (!disk || !track || cylinder < 0 || head < 0 ||
        cylinder >= disk->max_cylinders || head >= disk->max_heads) {
        return false;
    }
    
    size_t index = cylinder * disk->max_heads + head;
    
    /* Release old track if exists */
    if (disk->tracks[index]) {
        flux_track_destroy(&disk->tracks[index]);
    }
    
    /* Set new track (increment refcount) */
    disk->tracks[index] = track;
    track->refcount++;
    
    return true;
}

size_t flux_disk_calculate_size(const flux_disk_t *disk)
{
    if (!disk) {
        return 0;
    }
    
    size_t total = 0;
    
    for (size_t i = 0; i < disk->track_count; i++) {
        if (disk->tracks[i]) {
            total += disk->tracks[i]->sample_count * sizeof(flux_sample_t);
            
            if (disk->tracks[i]->bitstream) {
                total += disk->tracks[i]->bitstream->byte_capacity;
            }
        }
    }
    
    return total;
}

/* =============================================================================
 * HELPER FUNCTIONS
 * ============================================================================= */

size_t flux_convert_timestamps(
    const uint64_t *timestamps,
    size_t count,
    flux_sample_t *samples)
{
    if (!timestamps || !samples) {
        return 0;
    }
    
    for (size_t i = 0; i < count; i++) {
        samples[i].timestamp_ns = timestamps[i];
        samples[i].index_pulse = 0;  /* No index info in simple timestamp array */
    }
    
    return count;
}

int flux_detect_index_pulses(
    const flux_sample_t *samples,
    size_t count,
    size_t *index_positions,
    size_t max_indices)
{
    if (!samples || !index_positions) {
        return -1;
    }
    
    size_t found = 0;
    
    for (size_t i = 0; i < count && found < max_indices; i++) {
        if (samples[i].index_pulse) {
            index_positions[found++] = i;
        }
    }
    
    return (int)found;
}

uint32_t flux_calculate_bitrate(
    const flux_sample_t *samples,
    size_t count)
{
    if (!samples || count < 10) {
        return 0;
    }
    
    /* Calculate average delta between transitions */
    uint64_t total_delta = 0;
    size_t delta_count = 0;
    
    for (size_t i = 0; i < count - 1 && delta_count < 1000; i++) {
        uint64_t delta = samples[i + 1].timestamp_ns - samples[i].timestamp_ns;
        
        /* Filter out very long gaps (index pulses, etc) */
        if (delta < 100000) {  /* < 100µs */
            total_delta += delta;
            delta_count++;
        }
    }
    
    if (delta_count == 0) {
        return 0;
    }
    
    uint64_t avg_delta_ns = total_delta / delta_count;
    
    /* Bitrate = 1 / avg_delta */
    /* In bits per second: 1000000000 / avg_delta_ns */
    
    if (avg_delta_ns == 0) {
        return 0;
    }
    
    return (uint32_t)(1000000000ULL / avg_delta_ns);
}
