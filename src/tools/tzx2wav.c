/**
 * @file tzx2wav.c
 * @brief TZX/CDT to WAV Command Line Converter
 * 
 * Usage: tzx2wav [options] input.tzx [output.wav]
 * 
 * Options:
 *   -r, --rate RATE    Sample rate (default: 44100)
 *   -s, --speed PCT    Speed adjustment in percent (-50 to +50)
 *   -a, --amplitude A  Amplitude 0.0-1.0 (default: 0.8)
 *   -h, --help         Show help
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L
#include "uft_tzx_wav.h"
#include "uft/core/uft_safe_parse.h"
#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <strings.h>
#include "uft/core/uft_safe_parse.h"

static void print_usage(const char* progname) {
    printf("TZX/CDT to WAV Converter - UFT Project\n");
    printf("Usage: %s [options] input.tzx [output.wav]\n\n", progname);
    printf("Options:\n");
    printf("  -r, --rate RATE     Sample rate in Hz (default: 44100)\n");
    printf("  -s, --speed PCT     Speed adjustment -50 to +50%% (default: 0)\n");
    printf("  -a, --amplitude A   Output amplitude 0.0-1.0 (default: 0.8)\n");
    printf("  -h, --help          Show this help\n\n");
    printf("Supported formats:\n");
    printf("  .tzx  ZX Spectrum tape files\n");
    printf("  .cdt  Amstrad CPC tape files\n\n");
    printf("Examples:\n");
    printf("  %s game.tzx\n", progname);
    printf("  %s game.tzx game.wav\n", progname);
    printf("  %s -r 48000 -s +5 game.cdt output.wav\n", progname);
}

static char* generate_output_name(const char* input) {
    size_t len = strlen(input);
    char* output = malloc(len + 5);
    if (!output) return NULL;
    
    memcpy(output, input, len + 1);  /* Safe: output is malloc(len+5) */
    
    /* Find extension */
    char* dot = strrchr(output, '.');
    if (dot && (strcasecmp(dot, ".tzx") == 0 || strcasecmp(dot, ".cdt") == 0)) {
        memcpy(dot, ".wav", 5);  /* Safe: includes NUL */
    } else {
        { size_t _l = strlen(output); if (_l + 5 < len + 5) memcpy(output + _l, ".wav", 5); }
    }
    
    return output;
}

int main(int argc, char* argv[]) {
    tzx_wav_config_t config;
    tzx_wav_config_init(&config);
    
    const char* input_file = NULL;
    const char* output_file = NULL;
    char* generated_output = NULL;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) && i + 1 < argc) {
            { int32_t t; if(uft_parse_int32(argv[++i],&t,10) && t>0) config.sample_rate=(uint32_t)t; }
            if (config.sample_rate < 8000 || config.sample_rate > 192000) {
                fprintf(stderr, "Error: Sample rate must be 8000-192000 Hz\n");
                return 1;
            }
        }
        else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--speed") == 0) && i + 1 < argc) {
            { int32_t t; if(uft_parse_int32(argv[++i],&t,10)) config.speed_adjust_percent=t; }
            if (config.speed_adjust_percent < -50 || config.speed_adjust_percent > 50) {
                fprintf(stderr, "Error: Speed adjustment must be -50 to +50%%\n");
                return 1;
            }
        }
        else if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--amplitude") == 0) && i + 1 < argc) {
            config.amplitude = (float)atof(argv[++i]);
            if (config.amplitude < 0.0f || config.amplitude > 1.0f) {
                fprintf(stderr, "Error: Amplitude must be 0.0-1.0\n");
                return 1;
            }
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
        else if (!input_file) {
            input_file = argv[i];
        }
        else if (!output_file) {
            output_file = argv[i];
        }
        else {
            fprintf(stderr, "Too many arguments\n");
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (!input_file) {
        fprintf(stderr, "Error: No input file specified\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Generate output filename if not provided */
    if (!output_file) {
        generated_output = generate_output_name(input_file);
        if (!generated_output) {
            fprintf(stderr, "Error: Out of memory\n");
            return 1;
        }
        output_file = generated_output;
    }
    
    /* Detect platform from extension */
    tzx_wav_config_from_extension(&config, input_file);
    
    printf("Converting: %s\n", input_file);
    printf("Output:     %s\n", output_file);
    printf("Platform:   %s\n", config.platform == TZX_PLATFORM_CPC ? "Amstrad CPC" : "ZX Spectrum");
    printf("Sample rate: %u Hz\n", config.sample_rate);
    if (config.speed_adjust_percent != 0) {
        printf("Speed:      %+d%%\n", config.speed_adjust_percent);
    }
    printf("\n");
    
    /* Convert */
    if (tzx_to_wav_file(input_file, output_file, &config)) {
        printf("Done!\n");
        free(generated_output);
        return 0;
    } else {
        fprintf(stderr, "Conversion failed!\n");
        free(generated_output);
        return 1;
    }
}
