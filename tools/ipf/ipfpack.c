/**
 * @file ipfpack.c
 * @brief IPF Container Packer Tool
 * 
 * Creates minimal IPF containers from raw data.
 * 
 * Usage: ipfpack -o <output.ipf> <input_files...>
 */

#include "uft/formats/ipf/uft_ipf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s -o <output.ipf> [options] <input_files...>\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o, --output <file>  Output file (required)\n");
    fprintf(stderr, "  -b, --big-endian     Use big-endian encoding\n");
    fprintf(stderr, "  -c, --crc            Include CRC32 in chunks\n");
    fprintf(stderr, "  -h, --help           Show this help\n");
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (len <= 0) {
        fclose(f);
        return NULL;
    }
    
    uint8_t *buf = (uint8_t*)malloc((size_t)len);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    *size = (size_t)len;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }
    
    const char *output = NULL;
    bool big_endian = false;
    bool add_crc = false;
    const char *inputs[256];
    int input_count = 0;
    
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--big-endian") == 0) {
            big_endian = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--crc") == 0) {
            add_crc = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            if (input_count < 256) {
                inputs[input_count++] = argv[i];
            }
        }
    }
    
    if (!output) {
        fprintf(stderr, "Error: No output file specified\n");
        return 2;
    }
    
    uft_ipf_writer_t w;
    uft_ipf_err_t e = uft_ipf_writer_open(&w, output, UFT_IPF_MAGIC_CAPS,
                                          big_endian, add_crc ? 12 : 8);
    if (e != UFT_IPF_OK) {
        fprintf(stderr, "Error creating '%s': %s\n", output, uft_ipf_strerror(e));
        return 1;
    }
    
    /* Add each input file as a DATA chunk */
    for (int i = 0; i < input_count; i++) {
        size_t size;
        uint8_t *data = read_file(inputs[i], &size);
        if (!data) {
            fprintf(stderr, "Error reading '%s'\n", inputs[i]);
            uft_ipf_writer_close(&w);
            return 1;
        }
        
        if (size > UINT32_MAX) {
            fprintf(stderr, "File too large: '%s'\n", inputs[i]);
            free(data);
            uft_ipf_writer_close(&w);
            return 1;
        }
        
        e = uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_DATA, data, (uint32_t)size, add_crc);
        free(data);
        
        if (e != UFT_IPF_OK) {
            fprintf(stderr, "Error adding chunk: %s\n", uft_ipf_strerror(e));
            uft_ipf_writer_close(&w);
            return 1;
        }
        
        printf("Added: %s (%zu bytes)\n", inputs[i], size);
    }
    
    uft_ipf_writer_close(&w);
    printf("Created: %s (%u chunks, %llu bytes)\n", output, w.chunk_count,
           (unsigned long long)w.bytes_written);
    
    return 0;
}
