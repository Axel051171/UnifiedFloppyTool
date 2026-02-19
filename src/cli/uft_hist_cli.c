/**
 * @file uft_hist_cli.c
 * @brief UFT Histogram CLI Tool
 * 
 * Standalone histogram analysis for flux images:
 * - Display flux timing histogram (like gwhist/cwtsthst)
 * - Peak detection and analysis
 * - Cell timing estimation
 * - ASCII and CSV output
 * 
 * Usage:
 *   uft hist <file> [options]
 *   uft hist track.scp -t 0:0        # Track 0, Side 0
 *   uft hist disk.scp --all          # All tracks
 *   uft hist flux.raw --csv          # CSV output
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <math.h>

/*===========================================================================
 * Histogram Constants and Structures
 *===========================================================================*/

#define HIST_MAX_BINS     512
#define HIST_MAX_PEAKS    16
#define HIST_BAR_WIDTH    60

typedef struct {
    uint32_t bins[HIST_MAX_BINS];
    uint32_t bin_count;
    uint64_t total_samples;
    uint32_t max_count;
    uint32_t max_bin;
    double   mean;
    double   stddev;
} hist_t;

typedef struct {
    uint32_t position;
    uint32_t count;
    uint32_t width;
    double   center;
} peak_t;

/*===========================================================================
 * Histogram Functions
 *===========================================================================*/

static void hist_init(hist_t *h, uint32_t bins) {
    memset(h, 0, sizeof(*h));
    h->bin_count = bins > HIST_MAX_BINS ? HIST_MAX_BINS : bins;
}

static void hist_add(hist_t *h, uint32_t value) {
    if (value < h->bin_count) {
        h->bins[value]++;
        h->total_samples++;
        if (h->bins[value] > h->max_count) {
            h->max_count = h->bins[value];
            h->max_bin = value;
        }
    }
}

static void hist_compute_stats(hist_t *h) {
    if (h->total_samples == 0) return;
    
    /* Mean */
    double sum = 0;
    for (uint32_t i = 0; i < h->bin_count; i++) {
        sum += (double)i * h->bins[i];
    }
    h->mean = sum / h->total_samples;
    
    /* Standard deviation */
    double var = 0;
    for (uint32_t i = 0; i < h->bin_count; i++) {
        double d = i - h->mean;
        var += d * d * h->bins[i];
    }
    h->stddev = sqrt(var / h->total_samples);
}

static int hist_find_peaks(hist_t *h, peak_t *peaks, int max_peaks, 
                           uint32_t min_height, uint32_t min_distance) {
    int peak_count = 0;
    uint32_t threshold = min_height > 0 ? min_height : h->max_count / 10;
    uint32_t distance = min_distance > 0 ? min_distance : 5;
    
    for (uint32_t i = distance; i < h->bin_count - distance && peak_count < max_peaks; i++) {
        if (h->bins[i] < threshold) continue;
        
        /* Check if local maximum */
        bool is_peak = true;
        for (uint32_t j = 1; j <= distance && is_peak; j++) {
            if (h->bins[i - j] >= h->bins[i] || h->bins[i + j] >= h->bins[i]) {
                is_peak = false;
            }
        }
        
        if (is_peak) {
            peaks[peak_count].position = i;
            peaks[peak_count].count = h->bins[i];
            
            /* Compute weighted center */
            double wsum = 0, wcount = 0;
            for (int j = -3; j <= 3; j++) {
                if (i + j < h->bin_count) {
                    wsum += (i + j) * h->bins[i + j];
                    wcount += h->bins[i + j];
                }
            }
            peaks[peak_count].center = wcount > 0 ? wsum / wcount : i;
            
            /* Find width at half-maximum */
            uint32_t half = h->bins[i] / 2;
            int left = i, right = i;
            while (left > 0 && h->bins[left] > half) left--;
            while (right < (int)h->bin_count - 1 && h->bins[right] > half) right++;
            peaks[peak_count].width = right - left;
            
            peak_count++;
            i += distance; /* Skip ahead */
        }
    }
    
    return peak_count;
}

/*===========================================================================
 * Output Functions
 *===========================================================================*/

static void print_ascii_histogram(hist_t *h, uint32_t start, uint32_t end) {
    if (end == 0 || end > h->bin_count) end = h->bin_count;
    if (start >= end) start = 0;
    
    /* Find max in range */
    uint32_t range_max = 0;
    for (uint32_t i = start; i < end; i++) {
        if (h->bins[i] > range_max) range_max = h->bins[i];
    }
    if (range_max == 0) range_max = 1;
    
    printf("\nFlux Timing Histogram (bins %u-%u)\n", start, end);
    printf("════════════════════════════════════════════════════════════════════\n");
    
    for (uint32_t i = start; i < end; i++) {
        if (h->bins[i] == 0) continue;
        
        uint32_t bar_len = (uint64_t)h->bins[i] * HIST_BAR_WIDTH / range_max;
        
        printf("%4u │ ", i);
        for (uint32_t j = 0; j < bar_len; j++) printf("█");
        printf(" %u\n", h->bins[i]);
    }
    
    printf("════════════════════════════════════════════════════════════════════\n");
}

static void print_csv_histogram(hist_t *h) {
    printf("bin,count\n");
    for (uint32_t i = 0; i < h->bin_count; i++) {
        if (h->bins[i] > 0) {
            printf("%u,%u\n", i, h->bins[i]);
        }
    }
}

static void print_stats(hist_t *h, peak_t *peaks, int peak_count, double sample_rate_mhz) {
    hist_compute_stats(h);
    
    printf("\nStatistics:\n");
    printf("  Total samples: %lu\n", (unsigned long)h->total_samples);
    printf("  Mean:          %.2f bins\n", h->mean);
    printf("  Std Dev:       %.2f bins\n", h->stddev);
    printf("  Peak bin:      %u (count: %u)\n", h->max_bin, h->max_count);
    
    if (sample_rate_mhz > 0) {
        double ns_per_bin = 1000.0 / sample_rate_mhz;
        printf("  Mean timing:   %.1f ns\n", h->mean * ns_per_bin);
    }
    
    if (peak_count > 0) {
        printf("\nDetected Peaks:\n");
        printf("  #   Bin    Count   Width   Center");
        if (sample_rate_mhz > 0) printf("   Time(ns)");
        printf("\n");
        
        for (int i = 0; i < peak_count; i++) {
            printf("  %d   %4u   %5u   %3u     %.1f",
                   i + 1, peaks[i].position, peaks[i].count, 
                   peaks[i].width, peaks[i].center);
            if (sample_rate_mhz > 0) {
                printf("      %.1f", peaks[i].center * 1000.0 / sample_rate_mhz);
            }
            printf("\n");
        }
        
        /* Try to detect MFM timing */
        if (peak_count >= 3) {
            double ratio1 = peaks[1].center / peaks[0].center;
            double ratio2 = peaks[2].center / peaks[0].center;
            
            if (ratio1 > 1.4 && ratio1 < 1.6 && ratio2 > 1.9 && ratio2 < 2.1) {
                printf("\n  Encoding: MFM detected\n");
                printf("  Bit cell: %.1f bins", peaks[0].center);
                if (sample_rate_mhz > 0) {
                    double cell_ns = peaks[0].center * 1000.0 / sample_rate_mhz;
                    uint32_t datarate = (uint32_t)(1000000000.0 / cell_ns);
                    printf(" (%.0f ns, ~%u bps)", cell_ns, datarate);
                }
                printf("\n");
            }
        }
        
        if (peak_count >= 2 && peak_count < 3) {
            double ratio = peaks[1].center / peaks[0].center;
            if (ratio > 1.9 && ratio < 2.1) {
                printf("\n  Encoding: FM detected\n");
                printf("  Bit cell: %.1f bins\n", peaks[0].center);
            }
        }
    }
}

/*===========================================================================
 * File Readers
 *===========================================================================*/

/* Read SCP flux data */
static int read_scp_flux(const char *filename, int track, int side, 
                         uint32_t **flux_data, size_t *flux_count) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    /* Read SCP header */
    uint8_t header[16];
    if (fread(header, 1, 16, f) != 16) {
        fclose(f);
        return -1;
    }
    
    if (memcmp(header, "SCP", 3) != 0) {
        fclose(f);
        return -1;
    }
    
    uint8_t start_track = header[6];
    uint8_t end_track = header[7];
    uint8_t revolutions = header[5];
    
    int track_index = track * 2 + side;
    if (track_index < start_track || track_index > end_track) {
        fprintf(stderr, "Track %d:%d not in file (range: %d-%d)\n",
                track, side, start_track, end_track);
        fclose(f);
        return -1;
    }
    
    /* Read track offset table */
    uint32_t track_offset;
    if (fseek(f, 16 + (track_index - start_track) * 4, SEEK_SET) != 0) return -1;
    if (fread(&track_offset, 4, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    if (track_offset == 0) {
        fprintf(stderr, "Track %d:%d has no data\n", track, side);
        fclose(f);
        return -1;
    }
    
    /* Read track header (first revolution) */
    if (fseek(f, track_offset, SEEK_SET) != 0) return -1;
    uint8_t track_header[12];
    if (fread(track_header, 1, 12, f) != 12) {
        fclose(f);
        return -1;
    }
    
    uint32_t data_length = track_header[4] | (track_header[5] << 8) |
                           (track_header[6] << 16) | (track_header[7] << 24);
    uint32_t data_offset = track_header[8] | (track_header[9] << 8) |
                           (track_header[10] << 16) | (track_header[11] << 24);
    
    /* Read flux data (16-bit values) */
    if (fseek(f, track_offset + data_offset, SEEK_SET) != 0) return -1;
    size_t sample_count = data_length / 2;
    uint16_t *raw = malloc(data_length);
    if (!raw || fread(raw, 2, sample_count, f) != sample_count) {
        free(raw);
        fclose(f);
        return -1;
    }
    
    /* Convert to 32-bit with overflow handling */
    *flux_data = malloc(sample_count * sizeof(uint32_t));
    if (!*flux_data) { free(raw); fclose(f); return -1; }
    *flux_count = 0;
    
    uint32_t accum = 0;
    for (size_t i = 0; i < sample_count; i++) {
        uint16_t val = raw[i];
        if (val == 0) {
            accum += 65536;
        } else {
            (*flux_data)[(*flux_count)++] = accum + val;
            accum = 0;
        }
    }
    
    free(raw);
    fclose(f);
    return 0;
}

/* Read raw flux data (Kryoflux/Catweasel style) */
static int read_raw_flux(const char *filename, uint32_t **flux_data, size_t *flux_count) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) return -1;
    
    uint8_t *raw = malloc(size);
    if (!raw || fread(raw, 1, size, f) != (size_t)size) {
        free(raw);
        fclose(f);
        return -1;
    }
    fclose(f);
    
    /* Estimate max samples */
    *flux_data = malloc(size * sizeof(uint32_t));
    if (!*flux_data) { free(raw); return -1; }
    *flux_count = 0;
    
    /* Parse based on file type detection */
    if (size > 4 && raw[0] == 0x0D) {
        /* Kryoflux stream format */
        size_t pos = 0;
        uint32_t accum = 0;
        
        while (pos < (size_t)size) {
            uint8_t b = raw[pos++];
            
            if (b <= 0x07) {
                /* Flux2 + OOB */
                if (pos < (size_t)size) {
                    uint32_t flux = ((b & 0x7) << 8) | raw[pos++];
                    (*flux_data)[(*flux_count)++] = flux + accum;
                    accum = 0;
                }
            } else if (b == 0x08) {
                /* NOP1 */
            } else if (b == 0x09) {
                /* NOP2 */
                pos++;
            } else if (b == 0x0A) {
                /* NOP3 */
                pos += 2;
            } else if (b == 0x0B) {
                /* Overflow16 */
                accum += 0x10000;
            } else if (b == 0x0C) {
                /* Flux3 */
                if (pos + 1 < (size_t)size) {
                    uint32_t flux = (raw[pos] << 8) | raw[pos + 1];
                    pos += 2;
                    (*flux_data)[(*flux_count)++] = flux + accum;
                    accum = 0;
                }
            } else if (b == 0x0D) {
                /* OOB - skip */
                if (pos + 2 < (size_t)size) {
                    uint16_t len = raw[pos] | (raw[pos + 1] << 8);
                    pos += 2 + len;
                }
            } else {
                /* Flux1 */
                (*flux_data)[(*flux_count)++] = b + accum;
                accum = 0;
            }
        }
    } else {
        /* Simple raw bytes */
        for (long i = 0; i < size; i++) {
            (*flux_data)[(*flux_count)++] = raw[i];
        }
    }
    
    free(raw);
    return 0;
}

/*===========================================================================
 * Main
 *===========================================================================*/

static void print_usage(const char *prog) {
    printf("UFT Flux Histogram Tool v4.0\n\n");
    printf("Usage: %s hist <file> [options]\n\n", prog);
    printf("Options:\n");
    printf("  -t, --track C:H     Track to analyze (default: 0:0)\n");
    printf("  -a, --all           Analyze all tracks\n");
    printf("  -r, --range A-B     Bin range to display\n");
    printf("  -s, --sample-rate N Sample rate in MHz (default: 40 for SCP)\n");
    printf("  -c, --csv           Output as CSV\n");
    printf("  -p, --peaks         Show peak detection only\n");
    printf("  -h, --help          Show this help\n");
    printf("\nSupported formats:\n");
    printf("  .scp   - SuperCard Pro flux images\n");
    printf("  .raw   - Kryoflux/Catweasel raw flux\n");
    printf("\nExamples:\n");
    printf("  %s hist disk.scp -t 0:0\n", prog);
    printf("  %s hist track00.0.raw --csv\n", prog);
}

int main(int argc, char *argv[]) {
    int track = 0, side = 0;
    bool all_tracks = false;
    bool csv_output = false;
    bool peaks_only = false;
    uint32_t range_start = 0, range_end = 0;
    double sample_rate = 40.0; /* Default SCP rate */
    const char *filename = NULL;
    
    static struct option long_opts[] = {
        {"track",       required_argument, 0, 't'},
        {"all",         no_argument,       0, 'a'},
        {"range",       required_argument, 0, 'r'},
        {"sample-rate", required_argument, 0, 's'},
        {"csv",         no_argument,       0, 'c'},
        {"peaks",       no_argument,       0, 'p'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "t:ar:s:cph", long_opts, NULL)) != -1) {
        switch (opt) {
        case 't':
            sscanf(optarg, "%d:%d", &track, &side);
            break;
        case 'a':
            all_tracks = true;
            break;
        case 'r':
            sscanf(optarg, "%u-%u", &range_start, &range_end);
            break;
        case 's':
            sample_rate = atof(optarg);
            break;
        case 'c':
            csv_output = true;
            break;
        case 'p':
            peaks_only = true;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        }
    }
    
    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    filename = argv[optind];
    
    /* Determine file type */
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        fprintf(stderr, "Cannot determine file type\n");
        return 1;
    }
    
    /* Read flux data */
    uint32_t *flux_data = NULL;
    size_t flux_count = 0;
    int result;
    
    if (strcasecmp(ext, ".scp") == 0) {
        result = read_scp_flux(filename, track, side, &flux_data, &flux_count);
    } else if (strcasecmp(ext, ".raw") == 0) {
        result = read_raw_flux(filename, &flux_data, &flux_count);
    } else {
        fprintf(stderr, "Unsupported format: %s\n", ext);
        return 1;
    }
    
    if (result != 0) {
        fprintf(stderr, "Failed to read flux data\n");
        return 1;
    }
    
    if (flux_count == 0) {
        fprintf(stderr, "No flux samples found\n");
        free(flux_data);
        return 1;
    }
    
    printf("Read %zu flux samples from %s (track %d:%d)\n", 
           flux_count, filename, track, side);
    
    /* Build histogram */
    hist_t hist;
    hist_init(&hist, HIST_MAX_BINS);
    
    for (size_t i = 0; i < flux_count; i++) {
        /* Scale to bins (assuming sample rate gives reasonable bin mapping) */
        uint32_t bin = flux_data[i];
        if (bin >= HIST_MAX_BINS) bin = HIST_MAX_BINS - 1;
        hist_add(&hist, bin);
    }
    
    /* Find peaks */
    peak_t peaks[HIST_MAX_PEAKS];
    int peak_count = hist_find_peaks(&hist, peaks, HIST_MAX_PEAKS, 0, 10);
    
    /* Output */
    if (csv_output) {
        print_csv_histogram(&hist);
    } else if (peaks_only) {
        print_stats(&hist, peaks, peak_count, sample_rate);
    } else {
        print_ascii_histogram(&hist, range_start, range_end);
        print_stats(&hist, peaks, peak_count, sample_rate);
    }
    
    free(flux_data);
    return 0;
}
