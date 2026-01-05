/**
 * @file uft_display_track.c
 * @brief Track Visualization Implementation
 * 
 * EXT4-010: Track display and visualization
 * 
 * Features:
 * - ASCII track visualization
 * - Sector map generation
 * - Timing diagram output
 * - Flux density visualization
 * - HTML/SVG export
 */

#include "uft/display/uft_display_track.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define DISPLAY_WIDTH       80
#define DISPLAY_HEIGHT      25
#define MAX_SECTORS         64
#define HISTOGRAM_HEIGHT    20

/* ASCII characters for visualization */
#define CHAR_FLUX           '|'
#define CHAR_GAP            '-'
#define CHAR_SYNC           'S'
#define CHAR_DATA           '#'
#define CHAR_ERROR          'X'
#define CHAR_EMPTY          ' '

/*===========================================================================
 * Track Map
 *===========================================================================*/

int uft_display_track_map(const uft_track_info_t *track, char *buffer, size_t size)
{
    if (!track || !buffer || size < DISPLAY_WIDTH + 1) return -1;
    
    memset(buffer, CHAR_EMPTY, DISPLAY_WIDTH);
    buffer[DISPLAY_WIDTH] = '\0';
    
    /* Calculate scale */
    double scale = (double)DISPLAY_WIDTH / track->track_length;
    
    /* Mark sectors */
    for (int s = 0; s < track->sector_count && s < MAX_SECTORS; s++) {
        const uft_sector_info_t *sec = &track->sectors[s];
        
        int start = (int)(sec->offset * scale);
        int end = (int)((sec->offset + sec->length) * scale);
        
        if (start >= 0 && start < DISPLAY_WIDTH) {
            buffer[start] = sec->valid ? '[' : 'x';
        }
        if (end > start && end < DISPLAY_WIDTH) {
            buffer[end - 1] = sec->valid ? ']' : 'x';
        }
        
        /* Fill sector area */
        for (int i = start + 1; i < end - 1 && i < DISPLAY_WIDTH; i++) {
            buffer[i] = sec->valid ? CHAR_DATA : CHAR_ERROR;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Sector Table
 *===========================================================================*/

int uft_display_sector_table(const uft_track_info_t *track, 
                             char *buffer, size_t size)
{
    if (!track || !buffer || size == 0) return -1;
    
    int written = 0;
    
    written += snprintf(buffer + written, size - written,
        "┌─────┬──────┬──────┬────────┬─────────┬────────┐\n"
        "│ Sec │ Cyl  │ Head │ Size   │ CRC     │ Status │\n"
        "├─────┼──────┼──────┼────────┼─────────┼────────┤\n");
    
    for (int s = 0; s < track->sector_count && s < MAX_SECTORS; s++) {
        const uft_sector_info_t *sec = &track->sectors[s];
        
        const char *status;
        if (sec->deleted) {
            status = "DEL";
        } else if (!sec->valid) {
            status = "ERR";
        } else if (sec->weak) {
            status = "WEAK";
        } else {
            status = "OK";
        }
        
        written += snprintf(buffer + written, size - written,
            "│ %3d │ %4d │ %4d │ %6d │ %s/%s │ %-6s │\n",
            sec->sector_id,
            sec->cylinder,
            sec->head,
            sec->data_size,
            sec->id_crc_ok ? "OK" : "XX",
            sec->data_crc_ok ? "OK" : "XX",
            status);
    }
    
    written += snprintf(buffer + written, size - written,
        "└─────┴──────┴──────┴────────┴─────────┴────────┘\n");
    
    return written;
}

/*===========================================================================
 * Timing Diagram
 *===========================================================================*/

int uft_display_timing_diagram(const uint32_t *flux_times, size_t count,
                               double sample_clock, int width,
                               char *buffer, size_t size)
{
    if (!flux_times || !buffer || count < 2 || width < 10) return -1;
    
    int written = 0;
    
    /* Calculate total time span */
    uint32_t total_time = flux_times[count - 1] - flux_times[0];
    double us_per_char = (double)total_time / sample_clock * 1000000.0 / width;
    
    /* Header */
    written += snprintf(buffer + written, size - written,
        "Timing Diagram (%.2f µs/char)\n", us_per_char);
    
    /* Build flux line */
    char *line = malloc(width + 1);
    if (!line) return -1;
    
    memset(line, '-', width);
    line[width] = '\0';
    
    /* Mark flux transitions */
    for (size_t i = 0; i < count; i++) {
        uint32_t rel_time = flux_times[i] - flux_times[0];
        int pos = (int)((double)rel_time / total_time * width);
        if (pos >= 0 && pos < width) {
            line[pos] = '|';
        }
    }
    
    written += snprintf(buffer + written, size - written, "%s\n", line);
    free(line);
    
    /* Time scale */
    written += snprintf(buffer + written, size - written,
        "0");
    for (int i = 1; i < 5; i++) {
        int pos = i * width / 4;
        double time_us = (double)total_time / sample_clock * 1000000.0 * i / 4;
        while (written < (int)size && pos > 0) {
            written += snprintf(buffer + written, size - written, " ");
            pos--;
        }
        written += snprintf(buffer + written, size - written, "%.0fµs", time_us);
    }
    written += snprintf(buffer + written, size - written, "\n");
    
    return written;
}

/*===========================================================================
 * Flux Density Histogram
 *===========================================================================*/

int uft_display_flux_histogram(const uint32_t *flux_times, size_t count,
                               double sample_clock,
                               char *buffer, size_t size)
{
    if (!flux_times || !buffer || count < 2) return -1;
    
    /* Build histogram */
    int bins[50] = {0};
    double bin_width = 0.2;  /* 200 ns */
    int max_count = 0;
    
    for (size_t i = 1; i < count; i++) {
        double us = (double)(flux_times[i] - flux_times[i - 1]) 
                   * 1000000.0 / sample_clock;
        int bin = (int)(us / bin_width);
        if (bin >= 0 && bin < 50) {
            bins[bin]++;
            if (bins[bin] > max_count) max_count = bins[bin];
        }
    }
    
    int written = 0;
    
    written += snprintf(buffer + written, size - written,
        "Flux Duration Histogram (%.0f ns bins)\n\n", bin_width * 1000);
    
    /* Draw histogram */
    for (int row = HISTOGRAM_HEIGHT; row > 0; row--) {
        int threshold = max_count * row / HISTOGRAM_HEIGHT;
        
        written += snprintf(buffer + written, size - written, "%6d |", threshold);
        
        for (int bin = 0; bin < 50; bin++) {
            char c = bins[bin] >= threshold ? '#' : ' ';
            written += snprintf(buffer + written, size - written, "%c", c);
        }
        written += snprintf(buffer + written, size - written, "\n");
    }
    
    /* X axis */
    written += snprintf(buffer + written, size - written,
        "       +--------------------------------------------------\n"
        "        0    2µs   4µs   6µs   8µs   10µs\n");
    
    return written;
}

/*===========================================================================
 * ASCII Disk View
 *===========================================================================*/

int uft_display_disk_view(const uft_disk_info_t *disk,
                          char *buffer, size_t size)
{
    if (!disk || !buffer || size == 0) return -1;
    
    int written = 0;
    
    /* Disk header */
    written += snprintf(buffer + written, size - written,
        "╔═══════════════════════════════════════════════════════╗\n"
        "║ Disk: %-48s ║\n"
        "║ Tracks: %3d  Sides: %d  Sectors/Track: %2d              ║\n"
        "╠═══════════════════════════════════════════════════════╣\n",
        disk->name ? disk->name : "Unknown",
        disk->tracks, disk->sides, disk->sectors_per_track);
    
    /* Track status grid */
    written += snprintf(buffer + written, size - written,
        "║ Track Status (. = OK, X = Error, ? = Missing)         ║\n"
        "║                                                       ║\n");
    
    /* Side 0 */
    written += snprintf(buffer + written, size - written, "║ S0: ");
    for (int t = 0; t < disk->tracks && t < 50; t++) {
        char c = '.';
        if (disk->track_status) {
            int status = disk->track_status[t * disk->sides];
            if (status == 0) c = '?';
            else if (status < 0) c = 'X';
        }
        written += snprintf(buffer + written, size - written, "%c", c);
    }
    if (disk->tracks > 50) {
        written += snprintf(buffer + written, size - written, "...");
    }
    written += snprintf(buffer + written, size - written, "\n");
    
    /* Side 1 */
    if (disk->sides > 1) {
        written += snprintf(buffer + written, size - written, "║ S1: ");
        for (int t = 0; t < disk->tracks && t < 50; t++) {
            char c = '.';
            if (disk->track_status) {
                int status = disk->track_status[t * disk->sides + 1];
                if (status == 0) c = '?';
                else if (status < 0) c = 'X';
            }
            written += snprintf(buffer + written, size - written, "%c", c);
        }
        written += snprintf(buffer + written, size - written, "\n");
    }
    
    written += snprintf(buffer + written, size - written,
        "║                                                       ║\n"
        "╚═══════════════════════════════════════════════════════╝\n");
    
    return written;
}

/*===========================================================================
 * SVG Export
 *===========================================================================*/

int uft_display_svg_track(const uft_track_info_t *track,
                          char *buffer, size_t size)
{
    if (!track || !buffer || size == 0) return -1;
    
    int written = 0;
    int width = 800;
    int height = 200;
    
    /* SVG header */
    written += snprintf(buffer + written, size - written,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n"
        "  <rect width=\"100%%\" height=\"100%%\" fill=\"#1a1a2e\"/>\n",
        width, height, width, height);
    
    /* Track outline */
    written += snprintf(buffer + written, size - written,
        "  <rect x=\"10\" y=\"50\" width=\"%d\" height=\"100\" "
        "fill=\"#16213e\" stroke=\"#0f3460\" stroke-width=\"2\"/>\n",
        width - 20);
    
    /* Draw sectors */
    double scale = (double)(width - 20) / track->track_length;
    
    for (int s = 0; s < track->sector_count && s < MAX_SECTORS; s++) {
        const uft_sector_info_t *sec = &track->sectors[s];
        
        int x = 10 + (int)(sec->offset * scale);
        int w = (int)(sec->length * scale);
        if (w < 2) w = 2;
        
        const char *color;
        if (!sec->valid) {
            color = "#e94560";  /* Red for error */
        } else if (sec->weak) {
            color = "#f39c12";  /* Orange for weak */
        } else if (sec->deleted) {
            color = "#9b59b6";  /* Purple for deleted */
        } else {
            color = "#00d9ff";  /* Cyan for OK */
        }
        
        written += snprintf(buffer + written, size - written,
            "  <rect x=\"%d\" y=\"60\" width=\"%d\" height=\"80\" "
            "fill=\"%s\" opacity=\"0.8\"/>\n",
            x, w, color);
        
        /* Sector label */
        if (w > 15) {
            written += snprintf(buffer + written, size - written,
                "  <text x=\"%d\" y=\"105\" fill=\"white\" "
                "font-size=\"10\" text-anchor=\"middle\">%d</text>\n",
                x + w / 2, sec->sector_id);
        }
    }
    
    /* Legend */
    written += snprintf(buffer + written, size - written,
        "  <text x=\"10\" y=\"180\" fill=\"#aaa\" font-size=\"12\">"
        "Track %d.%d - %d sectors</text>\n",
        track->cylinder, track->head, track->sector_count);
    
    /* SVG footer */
    written += snprintf(buffer + written, size - written, "</svg>\n");
    
    return written;
}

/*===========================================================================
 * HTML Report
 *===========================================================================*/

int uft_display_html_report(const uft_disk_info_t *disk,
                            char *buffer, size_t size)
{
    if (!disk || !buffer || size == 0) return -1;
    
    int written = 0;
    
    written += snprintf(buffer + written, size - written,
        "<!DOCTYPE html>\n"
        "<html><head>\n"
        "<title>UFT Disk Report</title>\n"
        "<style>\n"
        "body { background: #1a1a2e; color: #eee; font-family: monospace; }\n"
        "table { border-collapse: collapse; margin: 20px 0; }\n"
        "th, td { border: 1px solid #333; padding: 8px; text-align: center; }\n"
        "th { background: #16213e; }\n"
        ".ok { background: #27ae60; }\n"
        ".error { background: #e74c3c; }\n"
        ".weak { background: #f39c12; }\n"
        ".missing { background: #7f8c8d; }\n"
        "</style>\n"
        "</head><body>\n"
        "<h1>UFT Disk Analysis Report</h1>\n"
        "<h2>%s</h2>\n"
        "<p>Tracks: %d | Sides: %d | Sectors/Track: %d</p>\n",
        disk->name ? disk->name : "Unknown Disk",
        disk->tracks, disk->sides, disk->sectors_per_track);
    
    /* Track grid */
    written += snprintf(buffer + written, size - written,
        "<h3>Track Status</h3>\n"
        "<table><tr><th>Track</th>");
    
    for (int s = 0; s < disk->sides; s++) {
        written += snprintf(buffer + written, size - written,
            "<th>Side %d</th>", s);
    }
    written += snprintf(buffer + written, size - written, "</tr>\n");
    
    for (int t = 0; t < disk->tracks; t++) {
        written += snprintf(buffer + written, size - written,
            "<tr><td>%d</td>", t);
        
        for (int s = 0; s < disk->sides; s++) {
            const char *class_name = "ok";
            if (disk->track_status) {
                int status = disk->track_status[t * disk->sides + s];
                if (status == 0) class_name = "missing";
                else if (status < 0) class_name = "error";
            }
            written += snprintf(buffer + written, size - written,
                "<td class=\"%s\">%s</td>",
                class_name,
                strcmp(class_name, "ok") == 0 ? "✓" : 
                strcmp(class_name, "error") == 0 ? "✗" : "?");
        }
        written += snprintf(buffer + written, size - written, "</tr>\n");
    }
    
    written += snprintf(buffer + written, size - written,
        "</table>\n"
        "<p>Generated by UnifiedFloppyTool</p>\n"
        "</body></html>\n");
    
    return written;
}

/*===========================================================================
 * Console Color Output
 *===========================================================================*/

int uft_display_color_track(const uft_track_info_t *track,
                            char *buffer, size_t size, bool use_color)
{
    if (!track || !buffer || size == 0) return -1;
    
    int written = 0;
    
    /* ANSI color codes */
    const char *COLOR_RESET = use_color ? "\033[0m" : "";
    const char *COLOR_GREEN = use_color ? "\033[32m" : "";
    const char *COLOR_RED = use_color ? "\033[31m" : "";
    const char *COLOR_YELLOW = use_color ? "\033[33m" : "";
    const char *COLOR_CYAN = use_color ? "\033[36m" : "";
    
    written += snprintf(buffer + written, size - written,
        "%sTrack %d.%d%s - %d sectors\n",
        COLOR_CYAN, track->cylinder, track->head, COLOR_RESET,
        track->sector_count);
    
    /* Sector status line */
    written += snprintf(buffer + written, size - written, "Sectors: ");
    
    for (int s = 0; s < track->sector_count && s < MAX_SECTORS; s++) {
        const uft_sector_info_t *sec = &track->sectors[s];
        
        const char *color;
        char symbol;
        
        if (!sec->valid) {
            color = COLOR_RED;
            symbol = 'X';
        } else if (sec->weak) {
            color = COLOR_YELLOW;
            symbol = '?';
        } else {
            color = COLOR_GREEN;
            symbol = '.';
        }
        
        written += snprintf(buffer + written, size - written,
            "%s%c%s", color, symbol, COLOR_RESET);
    }
    
    written += snprintf(buffer + written, size - written, "\n");
    
    return written;
}
