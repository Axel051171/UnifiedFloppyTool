/**
 * @file ipfinfo.c
 * @brief IPF Container Information Tool
 * 
 * Displays chunk structure and validates IPF files.
 * 
 * Usage: ipfinfo <file.ipf>
 */

#include "uft/formats/ipf/uft_ipf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] <file.ipf>\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -v, --validate   Strict validation (bounds + CRC)\n");
    fprintf(stderr, "  -h, --help       Show this help\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }
    
    bool validate = false;
    const char *path = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--validate") == 0) {
            validate = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            path = argv[i];
        }
    }
    
    if (!path) {
        fprintf(stderr, "Error: No input file specified\n");
        return 2;
    }
    
    uft_ipf_t ipf;
    uft_ipf_err_t e;
    
    e = uft_ipf_open(&ipf, path);
    if (e != UFT_IPF_OK) {
        fprintf(stderr, "Error opening '%s': %s\n", path, uft_ipf_strerror(e));
        return 1;
    }
    
    e = uft_ipf_parse(&ipf);
    if (e != UFT_IPF_OK) {
        fprintf(stderr, "Error parsing '%s': %s\n", path, uft_ipf_strerror(e));
        uft_ipf_close(&ipf);
        return 1;
    }
    
    /* Dump info */
    uft_ipf_dump_info(&ipf, stdout);
    
    /* Validate if requested */
    if (validate) {
        e = uft_ipf_validate(&ipf, true);
        printf("\nValidation: %s\n", e == UFT_IPF_OK ? "PASS" : uft_ipf_strerror(e));
    }
    
    uft_ipf_close(&ipf);
    return (validate && e != UFT_IPF_OK) ? 1 : 0;
}
