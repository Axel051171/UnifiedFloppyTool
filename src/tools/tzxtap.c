/**
 * @file tzxtap.c
 * @brief TZX <-> TAP Bi-directional Converter
 * 
 * Usage: tzxtap input.tzx output.tap
 *        tzxtap input.tap output.tzx
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L
#include "uft_zxtap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void print_usage(const char* progname) {
    printf("TZX <-> TAP Converter - UFT Project\n\n");
    printf("Usage: %s input output\n\n", progname);
    printf("Converts between ZX Spectrum TZX and TAP formats.\n");
    printf("Direction is auto-detected from file extensions.\n\n");
    printf("Examples:\n");
    printf("  %s game.tzx game.tap    (TZX -> TAP)\n", progname);
    printf("  %s game.tap game.tzx    (TAP -> TZX)\n", progname);
    printf("\nNote: TZX->TAP only extracts standard speed blocks (0x10).\n");
    printf("      Turbo loaders and other special blocks are discarded.\n");
}

static const char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    return dot ? dot : "";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return argc == 1 ? 0 : 1;
    }
    
    const char* input = argv[1];
    const char* output = argv[2];
    const char* ext_in = get_extension(input);
    const char* ext_out = get_extension(output);
    
    bool tzx_to_tap = false;
    bool tap_to_tzx = false;
    
    if (strcasecmp(ext_in, ".tzx") == 0 && strcasecmp(ext_out, ".tap") == 0) {
        tzx_to_tap = true;
    } else if (strcasecmp(ext_in, ".tap") == 0 && strcasecmp(ext_out, ".tzx") == 0) {
        tap_to_tzx = true;
    } else {
        fprintf(stderr, "Error: Cannot determine conversion direction.\n");
        fprintf(stderr, "       Use .tzx and .tap extensions.\n");
        return 1;
    }
    
    printf("Converting: %s\n", input);
    printf("Output:     %s\n", output);
    printf("Direction:  %s\n\n", tzx_to_tap ? "TZX -> TAP" : "TAP -> TZX");
    
    bool ok = false;
    
    if (tzx_to_tap) {
        ok = zxtap_tzx_to_tap_file(input, output);
        if (ok) {
            zxtap_file_t* tap = zxtap_file_read(output);
            if (tap) {
                printf("Extracted %zu blocks:\n", tap->block_count);
                zxtap_print_info(tap, stdout);
                zxtap_file_free(tap);
            }
        }
    } else {
        ok = zxtap_tap_to_tzx_file(input, output);
        if (ok) {
            zxtap_file_t* tap = zxtap_file_read(input);
            if (tap) {
                printf("Converted %zu blocks.\n", tap->block_count);
                zxtap_file_free(tap);
            }
        }
    }
    
    if (ok) {
        printf("\nDone!\n");
        return 0;
    } else {
        fprintf(stderr, "Conversion failed!\n");
        return 1;
    }
}
