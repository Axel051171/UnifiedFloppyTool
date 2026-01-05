/**
 * @file uft_track_display.c
 * @brief Track Display and Visualization Implementation
 * 
 * EXT4-010: Track visualization and rendering
 * 
 * Features:
 * - ASCII track visualization
 * - Sector map generation
 * - Flux density visualization
 * - Heatmap generation
 * - SVG export
 */

#include "uft/display/uft_track_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * ASCII Track Visualization
 *===========================================================================*/

int uft_display_track_ascii(const uint8_t *track_data, size_t size,
                            int sectors, int sector_size,
                            char *buffer, size_t buf_size)
{
    if (!track_data || !buffer || buf_size == 0) return -1;
    
    size_t pos = 0;
    
    /* Header */
    pos += snprintf(buffer + pos, buf_size - pos,
                    "Track Layout (%zu bytes, %d sectors)\n", size, sectors);
    pos += snprintf(buffer + pos, buf_size - pos,
                    "═══════════════════════════════════════════════════════════\n");
    
    /* Each sector */
    for (int s = 0; s < sectors && pos < buf_size - 100; s++) {
        size_t sec_start = s * sector_size;
        if (sec_start >= size) break;
        
        /* Calculate fill pattern */
        int zeros = 0, ones = 0, other = 0;
        size_t sec_end = sec_start + sector_size;
        if (sec_end > size) sec_end = size;
        
        for (size_t i = sec_start; i < sec_end; i++) {
            if (track_data[i] == 0x00) zeros++;
            else if (track_data[i] == 0xFF) ones++;
            else other++;
        }
        
        /* Visual bar */
        char bar[52];
        memset(bar, ' ', 50);
        bar[50] = '\0';
        
        int used = (sec_end - sec_start) * 50 / sector_size;
        for (int i = 0; i < used && i < 50; i++) {
            if (ones > zeros && ones > other) bar[i] = '█';
            else if (zeros > ones && zeros > other) bar[i] = '░';
            else bar[i] = '▒';
        }
        
        pos += snprintf(buffer + pos, buf_size - pos,
                        "Sector %2d: [%s] %5zu bytes\n", s, bar, sec_end - sec_start);
    }
    
    pos += snprintf(buffer + pos, buf_size - pos,
                    "═══════════════════════════════════════════════════════════\n");
    
    return 0;
}

/*===========================================================================
 * Sector Map
 *===========================================================================*/

int uft_display_sector_map(const uft_sector_info_t *sectors, int count,
                           char *buffer, size_t buf_size)
{
    if (!sectors || !buffer || buf_size == 0) return -1;
    
    size_t pos = 0;
    
    pos += snprintf(buffer + pos, buf_size - pos,
                    "Sector Map (%d sectors)\n", count);
    pos += snprintf(buffer + pos, buf_size - pos,
                    "┌─────┬───────┬────────┬────────┬─────────┬────────┐\n");
    pos += snprintf(buffer + pos, buf_size - pos,
                    "│ Sec │ Track │  Head  │  Size  │  Status │  CRC   │\n");
    pos += snprintf(buffer + pos, buf_size - pos,
                    "├─────┼───────┼────────┼────────┼─────────┼────────┤\n");
    
    for (int i = 0; i < count && pos < buf_size - 100; i++) {
        const uft_sector_info_t *sec = &sectors[i];
        
        const char *status;
        if (sec->deleted) status = "DEL";
        else if (sec->crc_error) status = "ERR";
        else if (sec->weak) status = "WEAK";
        else status = "OK";
        
        const char *crc_mark = sec->crc_valid ? "✓" : "✗";
        
        pos += snprintf(buffer + pos, buf_size - pos,
                        "│ %3d │  %3d  │   %d    │  %4d  │  %4s   │   %s    │\n",
                        sec->sector, sec->track, sec->head,
                        sec->size, status, crc_mark);
    }
    
    pos += snprintf(buffer + pos, buf_size - pos,
                    "└─────┴───────┴────────┴────────┴─────────┴────────┘\n");
    
    return 0;
}

/*===========================================================================
 * Flux Density Map
 *===========================================================================*/

int uft_display_flux_density(const double *flux_times, size_t count,
                             int bins, char *buffer, size_t buf_size)
{
    if (!flux_times || !buffer || count < 2 || bins <= 0) return -1;
    
    /* Calculate track time */
    double track_time = flux_times[count - 1] - flux_times[0];
    double bin_time = track_time / bins;
    
    /* Count flux per bin */
    int *density = calloc(bins, sizeof(int));
    if (!density) return -1;
    
    for (size_t i = 0; i < count; i++) {
        int bin = (int)((flux_times[i] - flux_times[0]) / bin_time);
        if (bin >= 0 && bin < bins) {
            density[bin]++;
        }
    }
    
    /* Find max for scaling */
    int max_density = 1;
    for (int i = 0; i < bins; i++) {
        if (density[i] > max_density) max_density = density[i];
    }
    
    /* Generate visualization */
    size_t pos = 0;
    
    pos += snprintf(buffer + pos, buf_size - pos,
                    "Flux Density Map (%zu flux, %d bins)\n", count, bins);
    
    /* Vertical bar chart */
    const int chart_height = 10;
    const char *blocks[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    
    for (int row = chart_height - 1; row >= 0 && pos < buf_size - 200; row--) {
        pos += snprintf(buffer + pos, buf_size - pos, "%3d%% │", 
                        (row + 1) * 100 / chart_height);
        
        for (int col = 0; col < bins && col < 60; col++) {
            int level = density[col] * chart_height / max_density;
            
            if (level > row) {
                pos += snprintf(buffer + pos, buf_size - pos, "█");
            } else if (level == row && density[col] > 0) {
                int sub = (density[col] * chart_height * 8 / max_density) % 8;
                pos += snprintf(buffer + pos, buf_size - pos, "%s", blocks[sub]);
            } else {
                pos += snprintf(buffer + pos, buf_size - pos, " ");
            }
        }
        pos += snprintf(buffer + pos, buf_size - pos, "\n");
    }
    
    /* X-axis */
    pos += snprintf(buffer + pos, buf_size - pos, "     └");
    for (int i = 0; i < bins && i < 60; i++) {
        pos += snprintf(buffer + pos, buf_size - pos, "─");
    }
    pos += snprintf(buffer + pos, buf_size - pos, "\n");
    
    free(density);
    return 0;
}

/*===========================================================================
 * Timing Histogram
 *===========================================================================*/

int uft_display_timing_histogram(const double *flux_times, size_t count,
                                 char *buffer, size_t buf_size)
{
    if (!flux_times || !buffer || count < 2) return -1;
    
    /* Calculate intervals and build histogram */
    #define HIST_BINS 50
    #define HIST_MAX_US 10.0
    
    int histogram[HIST_BINS] = {0};
    double bin_size = HIST_MAX_US / HIST_BINS;
    
    for (size_t i = 1; i < count; i++) {
        double interval_us = (flux_times[i] - flux_times[i-1]) * 1000000.0;
        int bin = (int)(interval_us / bin_size);
        if (bin >= 0 && bin < HIST_BINS) {
            histogram[bin]++;
        }
    }
    
    /* Find max */
    int max_count = 1;
    for (int i = 0; i < HIST_BINS; i++) {
        if (histogram[i] > max_count) max_count = histogram[i];
    }
    
    /* Generate output */
    size_t pos = 0;
    
    pos += snprintf(buffer + pos, buf_size - pos,
                    "Timing Histogram (interval distribution)\n");
    pos += snprintf(buffer + pos, buf_size - pos,
                    "    µs │\n");
    
    for (int i = 0; i < HIST_BINS && pos < buf_size - 100; i++) {
        double time = i * bin_size;
        int bar_len = histogram[i] * 50 / max_count;
        
        char bar[52];
        memset(bar, '█', bar_len);
        bar[bar_len] = '\0';
        
        if (i % 5 == 0 || histogram[i] > max_count / 5) {
            pos += snprintf(buffer + pos, buf_size - pos,
                            "%5.1f │%s %d\n", time, bar, histogram[i]);
        }
    }
    
    return 0;
}

/*===========================================================================
 * SVG Export
 *===========================================================================*/

int uft_display_svg_track(const double *flux_times, size_t count,
                          int width, int height,
                          char *buffer, size_t buf_size)
{
    if (!flux_times || !buffer || count < 2 || buf_size < 1000) return -1;
    
    size_t pos = 0;
    
    /* SVG header */
    pos += snprintf(buffer + pos, buf_size - pos,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n",
        width, height, width, height);
    
    /* Background */
    pos += snprintf(buffer + pos, buf_size - pos,
        "  <rect width=\"100%%\" height=\"100%%\" fill=\"#1a1a2e\"/>\n");
    
    /* Track time range */
    double track_time = flux_times[count - 1] - flux_times[0];
    
    /* Draw flux lines */
    pos += snprintf(buffer + pos, buf_size - pos,
        "  <g stroke=\"#00ff88\" stroke-width=\"1\" opacity=\"0.6\">\n");
    
    int max_lines = 2000;  /* Limit for SVG size */
    int step = count > max_lines ? count / max_lines : 1;
    
    for (size_t i = 0; i < count && pos < buf_size - 200; i += step) {
        double x = (flux_times[i] - flux_times[0]) * (width - 40) / track_time + 20;
        
        pos += snprintf(buffer + pos, buf_size - pos,
            "    <line x1=\"%.1f\" y1=\"%d\" x2=\"%.1f\" y2=\"%d\"/>\n",
            x, 20, x, height - 20);
    }
    
    pos += snprintf(buffer + pos, buf_size - pos, "  </g>\n");
    
    /* Axis */
    pos += snprintf(buffer + pos, buf_size - pos,
        "  <line x1=\"20\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
        "stroke=\"white\" stroke-width=\"2\"/>\n",
        height - 20, width - 20, height - 20);
    
    /* Labels */
    pos += snprintf(buffer + pos, buf_size - pos,
        "  <text x=\"%d\" y=\"%d\" fill=\"white\" "
        "font-family=\"monospace\" font-size=\"12\" text-anchor=\"middle\">"
        "Track Time: %.2fms</text>\n",
        width / 2, height - 5, track_time * 1000);
    
    /* Close SVG */
    pos += snprintf(buffer + pos, buf_size - pos, "</svg>\n");
    
    return 0;
}

/*===========================================================================
 * Disk Heatmap
 *===========================================================================*/

int uft_display_disk_heatmap(const uft_track_quality_t *tracks, int track_count,
                             int sides, char *buffer, size_t buf_size)
{
    if (!tracks || !buffer || track_count <= 0) return -1;
    
    size_t pos = 0;
    
    pos += snprintf(buffer + pos, buf_size - pos,
                    "Disk Quality Heatmap (%d tracks, %d sides)\n\n",
                    track_count, sides);
    
    /* Color legend */
    pos += snprintf(buffer + pos, buf_size - pos,
                    "Legend: ██ Good  ▓▓ Fair  ░░ Poor  ·· Error\n\n");
    
    for (int side = 0; side < sides && pos < buf_size - 500; side++) {
        pos += snprintf(buffer + pos, buf_size - pos, "Side %d:\n", side);
        pos += snprintf(buffer + pos, buf_size - pos, "Track: ");
        
        /* Track numbers header */
        for (int t = 0; t < track_count && t < 80; t += 10) {
            pos += snprintf(buffer + pos, buf_size - pos, "%-10d", t);
        }
        pos += snprintf(buffer + pos, buf_size - pos, "\n       ");
        
        /* Quality bars */
        for (int t = 0; t < track_count && t < 80; t++) {
            int idx = side * track_count + t;
            int quality = tracks[idx].quality_score;
            
            const char *block;
            if (quality >= 80) block = "██";
            else if (quality >= 50) block = "▓▓";
            else if (quality >= 20) block = "░░";
            else block = "··";
            
            pos += snprintf(buffer + pos, buf_size - pos, "%s", block);
        }
        pos += snprintf(buffer + pos, buf_size - pos, "\n\n");
    }
    
    return 0;
}

/*===========================================================================
 * Report Summary
 *===========================================================================*/

int uft_display_summary(const uft_disk_summary_t *summary,
                        char *buffer, size_t buf_size)
{
    if (!summary || !buffer || buf_size == 0) return -1;
    
    size_t pos = 0;
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "╔════════════════════════════════════════════════════════╗\n"
        "║               DISK ANALYSIS SUMMARY                     ║\n"
        "╠════════════════════════════════════════════════════════╣\n");
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "║ Format:      %-40s ║\n", summary->format_name);
    pos += snprintf(buffer + pos, buf_size - pos,
        "║ Tracks:      %-3d  Sides: %-3d  Sectors/Track: %-3d     ║\n",
        summary->tracks, summary->sides, summary->sectors_per_track);
    pos += snprintf(buffer + pos, buf_size - pos,
        "║ Encoding:    %-40s ║\n", summary->encoding_name);
    pos += snprintf(buffer + pos, buf_size - pos,
        "║ Data Rate:   %d kbps                                   ║\n",
        summary->data_rate_kbps);
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "╠════════════════════════════════════════════════════════╣\n");
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "║ Total Sectors:     %-5d   Good Sectors:    %-5d      ║\n",
        summary->total_sectors, summary->good_sectors);
    pos += snprintf(buffer + pos, buf_size - pos,
        "║ Bad Sectors:       %-5d   Weak Sectors:    %-5d      ║\n",
        summary->bad_sectors, summary->weak_sectors);
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "╠════════════════════════════════════════════════════════╣\n");
    
    int quality = summary->good_sectors * 100 / 
                  (summary->total_sectors > 0 ? summary->total_sectors : 1);
    
    const char *grade;
    if (quality >= 95) grade = "EXCELLENT";
    else if (quality >= 85) grade = "GOOD";
    else if (quality >= 70) grade = "FAIR";
    else if (quality >= 50) grade = "POOR";
    else grade = "CRITICAL";
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "║ Overall Quality:   %3d%%  Grade: %-20s  ║\n",
        quality, grade);
    
    pos += snprintf(buffer + pos, buf_size - pos,
        "╚════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
