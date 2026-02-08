/**
 * @file uft_main.c
 * @brief UnifiedFloppyTool - Command Line Interface
 * 
 * @author UFT Team
 * @date 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "uft/uft.h"

static bool g_verbose = false;
static bool g_quiet = false;

static void print_version(void) {
    printf("UnifiedFloppyTool (UFT) %s\n", uft_version());
    printf("Copyright 2025 UFT Team\n");
}

static void print_usage(const char* prog) {
    printf("Usage: %s <command> [options] <file>\n\n", prog);
    printf("Commands:\n");
    printf("  info <file>              Show disk information\n");
    printf("  list <file>              List tracks and sectors\n");
    printf("  analyze <file>           Analyze disk health\n");
    printf("  convert <in> <out>       Convert between formats\n");
    printf("  read <file> -t C:H:S     Read sector to stdout\n");
    printf("  hist <file>              Flux timing histogram\n");
    printf("  check <file>             Validate disk image\n");
    printf("  formats                  List supported formats\n");
    printf("  help                     Show this help\n");
    printf("  version                  Show version\n\n");
    printf("Options:\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  -q, --quiet              Quiet mode\n");
    printf("  -f, --format <fmt>       Force format\n\n");
    printf("For command-specific help:\n");
    printf("  %s hist --help\n", prog);
    printf("  %s check --help\n", prog);
}

static void log_handler(uft_log_level_t level, const char* msg, void* user) {
    (void)user;
    if (g_quiet && level < UFT_LOG_ERROR) return;
    if (!g_verbose && level == UFT_LOG_DEBUG) return;
    
    const char* prefix = level == UFT_LOG_ERROR ? "[ERROR]" :
                         level == UFT_LOG_WARN ? "[WARN]" :
                         level == UFT_LOG_INFO ? "[INFO]" : "[DEBUG]";
    fprintf(stderr, "%s %s\n", prefix, msg);
}

static bool progress_cb(int cyl, int head, int pct, const char* msg, void* u) {
    (void)u;
    if (!g_quiet) {
        printf("\r[%3d%%] C%02d H%d: %s", pct, cyl, head, msg ? msg : "");
        fflush(stdout);
        if (pct >= 100) printf("\n");
    }
    return true;
}

static int cmd_info(const char* path) {
    uft_disk_t* disk = uft_disk_open(path, true);
    if (!disk) {
        fprintf(stderr, "Error: Cannot open '%s'\n", path);
        return 1;
    }
    
    uft_geometry_t geo;
    uft_disk_get_geometry(disk, &geo);
    const uft_format_info_t* fmt = uft_format_get_info(uft_disk_get_format(disk));
    
    printf("File:        %s\n", path);
    printf("Format:      %s (%s)\n", fmt->name, fmt->description);
    printf("Cylinders:   %d\n", geo.cylinders);
    printf("Heads:       %d\n", geo.heads);
    printf("Sectors:     %d per track\n", geo.sectors);
    printf("Sector size: %d bytes\n", geo.sector_size);
    printf("Total:       %d sectors (%zu KB)\n", 
           geo.total_sectors, (size_t)geo.total_sectors * geo.sector_size / 1024);
    
    uft_disk_close(disk);
    return 0;
}

static int cmd_list(const char* path) {
    uft_disk_t* disk = uft_disk_open(path, true);
    if (!disk) {
        fprintf(stderr, "Error: Cannot open '%s'\n", path);
        return 1;
    }
    
    uft_geometry_t geo;
    uft_disk_get_geometry(disk, &geo);
    
    printf("Track listing for: %s\n", path);
    printf("=====================================\n");
    
    for (int cyl = 0; cyl < geo.cylinders; cyl++) {
        for (int head = 0; head < geo.heads; head++) {
            uft_track_t* track = uft_track_read(disk, cyl, head, NULL);
            if (!track) {
                printf("C%02d H%d: READ ERROR\n", cyl, head);
                continue;
            }
            
            size_t sc = uft_track_get_sector_count(track);
            uint32_t st = uft_track_get_status(track);
            
            printf("C%02d H%d: %zu sectors", cyl, head, sc);
            if (st & UFT_TRACK_READ_ERROR) printf(" [ERROR]");
            if (st & UFT_TRACK_PROTECTED) printf(" [PROT]");
            printf("\n");
            
            uft_track_free(track);
        }
    }
    
    uft_disk_close(disk);
    return 0;
}

static int cmd_analyze(const char* path) {
    uft_disk_t* disk = uft_disk_open(path, true);
    if (!disk) {
        fprintf(stderr, "Error: Cannot open '%s'\n", path);
        return 1;
    }
    
    printf("Analyzing: %s\n", path);
    
    uft_analysis_t a;
    uft_error_t err = uft_analyze(disk, &a, g_quiet ? NULL : progress_cb, NULL);
    
    if (UFT_FAILED(err)) {
        fprintf(stderr, "Analysis failed: %s\n", uft_error_string(err));
        uft_disk_close(disk);
        return 1;
    }
    
    printf("\nResults:\n");
    printf("  Tracks:     %d/%d readable (%.1f%%)\n", 
           a.readable_tracks, a.total_tracks,
           100.0 * a.readable_tracks / a.total_tracks);
    printf("  Sectors:    %d/%d readable (%.1f%%)\n",
           a.readable_sectors, a.total_sectors,
           a.total_sectors ? 100.0 * a.readable_sectors / a.total_sectors : 0);
    printf("  CRC errors: %d\n", a.crc_errors);
    printf("  Protection: %s\n", a.has_copy_protection ? "DETECTED" : "No");
    printf("  Weak bits:  %s\n", a.has_weak_bits ? "DETECTED" : "No");
    
    uft_disk_close(disk);
    return 0;
}

static int cmd_convert(const char* in, const char* out, uft_format_t fmt) {
    uft_disk_t* src = uft_disk_open(in, true);
    if (!src) {
        fprintf(stderr, "Error: Cannot open '%s'\n", in);
        return 1;
    }
    
    if (fmt == UFT_FORMAT_UNKNOWN) {
        const char* ext = strrchr(out, '.');
        if (ext) fmt = uft_format_from_extension(ext);
    }
    
    if (fmt == UFT_FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Cannot determine output format\n");
        uft_disk_close(src);
        return 1;
    }
    
    printf("Converting to %s...\n", uft_format_get_info(fmt)->name);
    
    uft_convert_options_t opts = {
        .target_format = fmt,
        .progress = g_quiet ? NULL : progress_cb
    };
    
    uft_error_t err = uft_convert(src, out, &opts);
    uft_disk_close(src);
    
    if (UFT_FAILED(err)) {
        fprintf(stderr, "Conversion failed: %s\n", uft_error_string(err));
        return 1;
    }
    
    printf("Done.\n");
    return 0;
}

static int cmd_formats(void) {
    printf("Supported formats:\n\n");
    for (int i = 1; i < UFT_FORMAT_MAX; i++) {
        const uft_format_info_t* f = uft_format_get_info(i);
        if (f && f->name[0]) {
            printf("  %-6s  %s\n", f->name, f->description);
            printf("          ext: %s", f->extensions);
            if (f->has_flux) printf(" [flux]");
            if (f->can_write) printf(" [rw]");
            printf("\n");
        }
    }
    return 0;
}

static int cmd_read(const char* path, int c, int h, int s) {
    uft_disk_t* disk = uft_disk_open(path, true);
    if (!disk) {
        fprintf(stderr, "Error: Cannot open '%s'\n", path);
        return 1;
    }
    
    uint8_t *buf = (uint8_t*)malloc(8192); if (!buf) return 1;
    int n = uft_sector_read(disk, c, h, s, buf, sizeof(buf));
    
    if (n < 0) {
        fprintf(stderr, "Error reading C%d:H%d:S%d: %s\n", c, h, s, uft_error_string(n));
        uft_disk_close(disk);
        return 1;
    }
    
    fprintf(stderr, "Sector C%d:H%d:S%d (%d bytes):\n", c, h, s, n);
    
    for (int i = 0; i < n; i += 16) {
        printf("%04X: ", i);
        for (int j = 0; j < 16 && i+j < n; j++) printf("%02X ", buf[i+j]);
        for (int j = n-i; j < 16; j++) printf("   ");
        printf(" ");
        for (int j = 0; j < 16 && i+j < n; j++) {
            char ch = buf[i+j];
            printf("%c", (ch >= 32 && ch < 127) ? ch : '.');
        }
        printf("\n");
    }
    
    uft_disk_close(disk);
    return 0;
}

static uft_format_t parse_format(const char* s) {
    if (!s) return UFT_FORMAT_UNKNOWN;
    if (strcasecmp(s, "adf") == 0) return UFT_FORMAT_ADF;
    if (strcasecmp(s, "scp") == 0) return UFT_FORMAT_SCP;
    if (strcasecmp(s, "img") == 0) return UFT_FORMAT_IMG;
    if (strcasecmp(s, "hfe") == 0) return UFT_FORMAT_HFE;
    return UFT_FORMAT_UNKNOWN;
}

int main(int argc, char* argv[]) {
    uft_init();
    uft_set_log_handler(log_handler, NULL);
    
    if (argc < 2) { print_usage(argv[0]); return 1; }
    
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) g_verbose = true;
        if (strcmp(argv[i], "-q") == 0) g_quiet = true;
    }
    
    const char* cmd = argv[1];
    
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]); return 0;
    }
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-V") == 0) {
        print_version(); return 0;
    }
    if (strcmp(cmd, "formats") == 0) return cmd_formats();
    
    if (strcmp(cmd, "info") == 0 && argc >= 3) return cmd_info(argv[2]);
    if (strcmp(cmd, "list") == 0 && argc >= 3) return cmd_list(argv[2]);
    if (strcmp(cmd, "analyze") == 0 && argc >= 3) return cmd_analyze(argv[2]);
    
    if (strcmp(cmd, "convert") == 0 && argc >= 4) {
        uft_format_t fmt = UFT_FORMAT_UNKNOWN;
        for (int i = 4; i < argc-1; i++)
            if (strcmp(argv[i], "-f") == 0) fmt = parse_format(argv[i+1]);
        return cmd_convert(argv[2], argv[3], fmt);
    }
    
    if (strcmp(cmd, "read") == 0 && argc >= 3) {
        int c = 0, h = 0, s = 1;
        for (int i = 3; i < argc-1; i++)
            if (strcmp(argv[i], "-t") == 0)
                sscanf(argv[i+1], "%d:%d:%d", &c, &h, &s);
        return cmd_read(argv[2], c, h, s);
    }
    
    fprintf(stderr, "Unknown command: %s\n", cmd);
    return 1;
}
