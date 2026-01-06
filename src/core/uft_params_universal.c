/**
 * @file uft_params_universal.c
 * @brief GOD MODE Universal Parameter System Implementation
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Include the header - in real build this would be from include path */
/* For now, inline the necessary types */

#include "uft_params_universal.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * NAME TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char* platform_names[] = {
    [UFT_PLATFORM_UNKNOWN] = "Unknown",
    [UFT_PLATFORM_COMMODORE_64] = "Commodore 64",
    [UFT_PLATFORM_COMMODORE_128] = "Commodore 128",
    [UFT_PLATFORM_COMMODORE_VIC20] = "VIC-20",
    [UFT_PLATFORM_COMMODORE_PET] = "Commodore PET",
    [UFT_PLATFORM_COMMODORE_PLUS4] = "Plus/4",
    [UFT_PLATFORM_ATARI_8BIT] = "Atari 8-bit",
    [UFT_PLATFORM_APPLE_II] = "Apple II",
    [UFT_PLATFORM_APPLE_III] = "Apple III",
    [UFT_PLATFORM_BBC_MICRO] = "BBC Micro",
    [UFT_PLATFORM_ZX_SPECTRUM] = "ZX Spectrum",
    [UFT_PLATFORM_AMSTRAD_CPC] = "Amstrad CPC",
    [UFT_PLATFORM_MSX] = "MSX",
    [UFT_PLATFORM_TRS80] = "TRS-80",
    [UFT_PLATFORM_ORIC] = "Oric",
    [UFT_PLATFORM_THOMSON] = "Thomson",
    [UFT_PLATFORM_TI99] = "TI-99/4A",
    [UFT_PLATFORM_DRAGON] = "Dragon 32/64",
    [UFT_PLATFORM_SAM_COUPE] = "SAM Coupé",
    [UFT_PLATFORM_AMIGA] = "Amiga",
    [UFT_PLATFORM_ATARI_ST] = "Atari ST",
    [UFT_PLATFORM_MACINTOSH] = "Macintosh",
    [UFT_PLATFORM_PC] = "IBM PC",
    [UFT_PLATFORM_PC98] = "NEC PC-98",
    [UFT_PLATFORM_X68000] = "Sharp X68000",
    [UFT_PLATFORM_FM_TOWNS] = "FM Towns",
    [UFT_PLATFORM_FAMICOM_DISK] = "Famicom Disk",
    [UFT_PLATFORM_GENERIC] = "Generic"
};

static const char* encoding_names[] = {
    [UFT_ENCODING_UNKNOWN] = "Unknown",
    [UFT_ENCODING_FM] = "FM (SD)",
    [UFT_ENCODING_MFM] = "MFM (DD)",
    [UFT_ENCODING_GCR_COMMODORE] = "GCR (Commodore)",
    [UFT_ENCODING_GCR_APPLE] = "GCR (Apple)",
    [UFT_ENCODING_GCR_VICTOR] = "GCR (Victor)",
    [UFT_ENCODING_M2FM] = "M2FM",
    [UFT_ENCODING_RLL] = "RLL",
    [UFT_ENCODING_RAW_FLUX] = "Raw Flux"
};

static const char* recovery_level_names[] = {
    [UFT_RECOVERY_NONE] = "None (Strict)",
    [UFT_RECOVERY_MINIMAL] = "Minimal",
    [UFT_RECOVERY_STANDARD] = "Standard",
    [UFT_RECOVERY_AGGRESSIVE] = "Aggressive",
    [UFT_RECOVERY_FORENSIC] = "Forensic"
};

static const char* rev_select_names[] = {
    [UFT_REV_SELECT_FIRST] = "First",
    [UFT_REV_SELECT_BEST] = "Best",
    [UFT_REV_SELECT_VOTING] = "Voting",
    [UFT_REV_SELECT_MERGE] = "Merge",
    [UFT_REV_SELECT_ALL] = "All"
};

/* ═══════════════════════════════════════════════════════════════════════════
 * NAME ACCESSORS
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_platform_name(uft_platform_t platform) {
    if (platform < UFT_PLATFORM_COUNT) {
        return platform_names[platform];
    }
    return "Invalid";
}

const char* uft_encoding_name(uft_encoding_t encoding) {
    if (encoding < UFT_ENCODING_COUNT) {
        return encoding_names[encoding];
    }
    return "Invalid";
}

const char* uft_recovery_level_name(uft_recovery_level_t level) {
    if (level < UFT_RECOVERY_COUNT) {
        return recovery_level_names[level];
    }
    return "Invalid";
}

const char* uft_rev_select_name(uft_rev_select_t mode) {
    if (mode < UFT_REV_SELECT_COUNT) {
        return rev_select_names[mode];
    }
    return "Invalid";
}

/* ═══════════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize parameters with sensible defaults
 */
void uft_params_init(uft_params_t* params) {
    if (!params) return;
    
    memset(params, 0, sizeof(uft_params_t));
    
    /* File defaults */
    params->file.read_only = true;
    params->file.extended_mode = false;
    
    /* IO defaults */
    params->io.auto_detect = true;
    
    /* Analysis defaults */
    params->analysis.show_summary = true;
    params->analysis.verbose = false;
    params->analysis.quiet = false;
    
    /* Flux dump defaults */
    params->flux_dump.track = -1;      /* All tracks */
    params->flux_dump.side = -1;       /* All sides */
    params->flux_dump.revolution = -1; /* Best revolution */
    params->flux_dump.max_transitions = UFT_MAX_FLUX_TRANSITIONS;
    params->flux_dump.binary_output = false;
    params->flux_dump.include_timing = true;
    
    /* Recovery defaults - Standard mode */
    params->recovery.rev_selection = UFT_REV_SELECT_BEST;
    params->recovery.level = UFT_RECOVERY_STANDARD;
    params->recovery.merge_revolutions = false;
    params->recovery.max_revs_to_use = 5;
    params->recovery.ignore_short_revs = true;
    params->recovery.normalize_timebase = true;
    params->recovery.pll_bandwidth = 0.1f;
    params->recovery.bitcell_tolerance = 15;
    params->recovery.allow_crc_errors = false;
    params->recovery.attempt_crc_recovery = true;
    params->recovery.max_correction_bits = 2;
    params->recovery.detect_weak_bits = true;
    params->recovery.weak_bit_threshold = 3;
    params->recovery.score_crc_weight = 1.0f;
    params->recovery.score_timing_weight = 0.5f;
    params->recovery.score_complete_weight = 0.8f;
    
    /* Conversion defaults */
    params->conversion.preserve_errors = false;
    params->conversion.preserve_timing = true;
    params->conversion.preserve_protection = false;
    params->conversion.fill_missing = true;
    params->conversion.fill_byte = 0x00;
    
    /* Verify defaults */
    params->verify.verify_checksums = true;
    params->verify.verify_structure = true;
    params->verify.verify_filesystem = false;
    params->verify.hash_output = false;
    strncpy(params->verify.hash_algorithm, "SHA256", sizeof(params->verify.hash_algorithm)-1);
    
    /* Operation */
    params->operation = UFT_OP_READ;
    params->initialized = true;
}

/**
 * @brief Set platform-specific defaults
 */
void uft_params_set_platform_defaults(uft_params_t* params, uft_platform_t platform) {
    if (!params) return;
    
    params->metadata.platform = platform;
    
    switch (platform) {
        case UFT_PLATFORM_COMMODORE_64:
        case UFT_PLATFORM_COMMODORE_128:
        case UFT_PLATFORM_COMMODORE_VIC20:
        case UFT_PLATFORM_COMMODORE_PET:
        case UFT_PLATFORM_COMMODORE_PLUS4:
            params->metadata.encoding = UFT_ENCODING_GCR_COMMODORE;
            params->metadata.num_tracks = 35;  /* or 40 for extended */
            params->metadata.num_sides = 1;    /* or 2 for D71/D81 */
            params->metadata.bit_rate = 250000;
            break;
            
        case UFT_PLATFORM_ATARI_8BIT:
            params->metadata.encoding = UFT_ENCODING_FM;  /* or MFM for DD */
            params->metadata.num_tracks = 40;
            params->metadata.num_sides = 1;
            params->metadata.sectors_per_track = 18;
            params->metadata.sector_size = 128;
            params->metadata.bit_rate = 125000;
            break;
            
        case UFT_PLATFORM_APPLE_II:
            params->metadata.encoding = UFT_ENCODING_GCR_APPLE;
            params->metadata.num_tracks = 35;
            params->metadata.num_sides = 1;
            params->metadata.sectors_per_track = 16;  /* DOS 3.3 */
            params->metadata.sector_size = 256;
            params->metadata.bit_rate = 250000;
            break;
            
        case UFT_PLATFORM_BBC_MICRO:
            params->metadata.encoding = UFT_ENCODING_FM;  /* DFS */
            params->metadata.num_tracks = 80;
            params->metadata.num_sides = 1;
            params->metadata.sectors_per_track = 10;
            params->metadata.sector_size = 256;
            params->metadata.bit_rate = 125000;
            break;
            
        case UFT_PLATFORM_ZX_SPECTRUM:
            params->metadata.encoding = UFT_ENCODING_MFM;
            params->metadata.num_tracks = 80;
            params->metadata.num_sides = 2;
            params->metadata.sectors_per_track = 16;
            params->metadata.sector_size = 256;
            params->metadata.bit_rate = 250000;
            break;
            
        case UFT_PLATFORM_AMSTRAD_CPC:
            params->metadata.encoding = UFT_ENCODING_MFM;
            params->metadata.num_tracks = 40;  /* or 80 for 3" */
            params->metadata.num_sides = 1;
            params->metadata.sectors_per_track = 9;
            params->metadata.sector_size = 512;
            params->metadata.bit_rate = 250000;
            break;
            
        case UFT_PLATFORM_AMIGA:
            params->metadata.encoding = UFT_ENCODING_MFM;
            params->metadata.num_tracks = 80;
            params->metadata.num_sides = 2;
            params->metadata.sectors_per_track = 11;
            params->metadata.sector_size = 512;
            params->metadata.bit_rate = 250000;
            break;
            
        case UFT_PLATFORM_ATARI_ST:
            params->metadata.encoding = UFT_ENCODING_MFM;
            params->metadata.num_tracks = 80;
            params->metadata.num_sides = 2;
            params->metadata.sectors_per_track = 9;
            params->metadata.sector_size = 512;
            params->metadata.bit_rate = 250000;
            break;
            
        case UFT_PLATFORM_PC:
            params->metadata.encoding = UFT_ENCODING_MFM;
            params->metadata.num_tracks = 80;
            params->metadata.num_sides = 2;
            params->metadata.sectors_per_track = 18;  /* 1.44MB */
            params->metadata.sector_size = 512;
            params->metadata.bit_rate = 500000;  /* HD */
            break;
            
        case UFT_PLATFORM_PC98:
            params->metadata.encoding = UFT_ENCODING_MFM;
            params->metadata.num_tracks = 77;
            params->metadata.num_sides = 2;
            params->metadata.sectors_per_track = 26;
            params->metadata.sector_size = 256;
            params->metadata.bit_rate = 500000;
            break;
            
        default:
            /* Keep generic defaults */
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CLI PARSING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse command line arguments
 */
bool uft_params_parse_cli(uft_params_t* params, int argc, char** argv) {
    if (!params || argc < 2) return false;
    
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        
        /* Input file */
        if (strcmp(arg, "--in") == 0 || strcmp(arg, "-i") == 0) {
            if (++i < argc) {
                strncpy(params->io.input_file, argv[i], UFT_MAX_FILENAME - 1);
                strncpy(params->file.path, argv[i], UFT_MAX_FILENAME - 1);
            }
        }
        /* Output file */
        else if (strcmp(arg, "--out") == 0 || strcmp(arg, "-o") == 0) {
            if (++i < argc) {
                strncpy(params->io.output_file, argv[i], UFT_MAX_FILENAME - 1);
            }
        }
        /* Format override */
        else if (strcmp(arg, "--format") == 0 || strcmp(arg, "-f") == 0) {
            if (++i < argc) {
                strncpy(params->io.format_override, argv[i], 31);
                params->io.auto_detect = false;
            }
        }
        /* Summary */
        else if (strcmp(arg, "--summary") == 0 || strcmp(arg, "-s") == 0) {
            params->analysis.show_summary = true;
        }
        /* Catalog output */
        else if (strcmp(arg, "--catalog") == 0) {
            if (++i < argc) {
                strncpy(params->analysis.catalog_output, argv[i], UFT_MAX_FILENAME - 1);
                params->analysis.show_catalog = true;
            }
        }
        /* Track selection */
        else if (strcmp(arg, "--track") == 0 || strcmp(arg, "-t") == 0) {
            if (++i < argc) {
                params->flux_dump.track = atoi(argv[i]);
            }
        }
        /* Side selection */
        else if (strcmp(arg, "--side") == 0) {
            if (++i < argc) {
                params->flux_dump.side = atoi(argv[i]);
            }
        }
        /* Revolution selection */
        else if (strcmp(arg, "--rev") == 0 || strcmp(arg, "-r") == 0) {
            if (++i < argc) {
                params->flux_dump.revolution = atoi(argv[i]);
            }
        }
        /* Flux dump */
        else if (strcmp(arg, "--dump") == 0) {
            if (++i < argc) {
                strncpy(params->flux_dump.output_file, argv[i], UFT_MAX_FILENAME - 1);
            }
        }
        /* Max transitions */
        else if (strcmp(arg, "--max-transitions") == 0) {
            if (++i < argc) {
                params->flux_dump.max_transitions = atoi(argv[i]);
            }
        }
        /* Recovery level */
        else if (strcmp(arg, "--recovery") == 0) {
            if (++i < argc) {
                const char* level = argv[i];
                if (strcmp(level, "none") == 0 || strcmp(level, "strict") == 0) {
                    params->recovery.level = UFT_RECOVERY_NONE;
                } else if (strcmp(level, "minimal") == 0) {
                    params->recovery.level = UFT_RECOVERY_MINIMAL;
                } else if (strcmp(level, "standard") == 0) {
                    params->recovery.level = UFT_RECOVERY_STANDARD;
                } else if (strcmp(level, "aggressive") == 0) {
                    params->recovery.level = UFT_RECOVERY_AGGRESSIVE;
                } else if (strcmp(level, "forensic") == 0) {
                    params->recovery.level = UFT_RECOVERY_FORENSIC;
                }
            }
        }
        /* Revolution selection mode */
        else if (strcmp(arg, "--rev-select") == 0) {
            if (++i < argc) {
                const char* mode = argv[i];
                if (strcmp(mode, "first") == 0) {
                    params->recovery.rev_selection = UFT_REV_SELECT_FIRST;
                } else if (strcmp(mode, "best") == 0) {
                    params->recovery.rev_selection = UFT_REV_SELECT_BEST;
                } else if (strcmp(mode, "voting") == 0) {
                    params->recovery.rev_selection = UFT_REV_SELECT_VOTING;
                } else if (strcmp(mode, "merge") == 0) {
                    params->recovery.rev_selection = UFT_REV_SELECT_MERGE;
                } else if (strcmp(mode, "all") == 0) {
                    params->recovery.rev_selection = UFT_REV_SELECT_ALL;
                }
            }
        }
        /* Merge revolutions */
        else if (strcmp(arg, "--merge-revs") == 0) {
            params->recovery.merge_revolutions = true;
        }
        /* Max revolutions */
        else if (strcmp(arg, "--max-revs") == 0) {
            if (++i < argc) {
                params->recovery.max_revs_to_use = atoi(argv[i]);
            }
        }
        /* Allow CRC errors */
        else if (strcmp(arg, "--allow-crc-errors") == 0) {
            params->recovery.allow_crc_errors = true;
        }
        /* Verbose */
        else if (strcmp(arg, "--verbose") == 0 || strcmp(arg, "-v") == 0) {
            params->analysis.verbose = true;
        }
        /* Quiet */
        else if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "-q") == 0) {
            params->analysis.quiet = true;
        }
        /* Convert operation */
        else if (strcmp(arg, "--convert") == 0) {
            params->operation = UFT_OP_CONVERT;
            if (++i < argc) {
                strncpy(params->conversion.target_format, argv[i], 31);
            }
        }
        /* Verify operation */
        else if (strcmp(arg, "--verify") == 0) {
            params->operation = UFT_OP_VERIFY;
        }
        /* Hash output */
        else if (strcmp(arg, "--hash") == 0) {
            params->verify.hash_output = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                strncpy(params->verify.hash_algorithm, argv[++i], 15);
            }
        }
        /* Help */
        else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            return false;  /* Trigger help display */
        }
        /* Assume positional argument is input file */
        else if (arg[0] != '-' && params->io.input_file[0] == '\0') {
            strncpy(params->io.input_file, arg, UFT_MAX_FILENAME - 1);
            strncpy(params->file.path, arg, UFT_MAX_FILENAME - 1);
        }
    }
    
    return params->io.input_file[0] != '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate parameter consistency
 */
bool uft_params_validate(const uft_params_t* params, char* error, size_t error_size) {
    if (!params) {
        if (error) snprintf(error, error_size, "NULL params");
        return false;
    }
    
    if (!params->initialized) {
        if (error) snprintf(error, error_size, "Parameters not initialized");
        return false;
    }
    
    /* Check input file for read operations */
    if (params->operation == UFT_OP_READ || 
        params->operation == UFT_OP_CONVERT ||
        params->operation == UFT_OP_ANALYZE ||
        params->operation == UFT_OP_VERIFY) {
        if (params->io.input_file[0] == '\0') {
            if (error) snprintf(error, error_size, "No input file specified");
            return false;
        }
    }
    
    /* Check output file for write/convert operations */
    if (params->operation == UFT_OP_WRITE || params->operation == UFT_OP_CONVERT) {
        if (params->io.output_file[0] == '\0') {
            if (error) snprintf(error, error_size, "No output file specified");
            return false;
        }
    }
    
    /* Check track range */
    if (params->flux_dump.track >= UFT_MAX_TRACKS) {
        if (error) snprintf(error, error_size, "Track number out of range");
        return false;
    }
    
    /* Check revolution count */
    if (params->recovery.max_revs_to_use > UFT_MAX_REVOLUTIONS) {
        if (error) snprintf(error, error_size, "Max revolutions out of range");
        return false;
    }
    
    /* Check PLL bandwidth */
    if (params->recovery.pll_bandwidth < 0.0f || params->recovery.pll_bandwidth > 1.0f) {
        if (error) snprintf(error, error_size, "PLL bandwidth must be 0.0-1.0");
        return false;
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * JSON I/O (Simplified)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Save parameters to JSON (simplified format)
 */
bool uft_params_save_json(const uft_params_t* params, const char* json_path) {
    if (!params || !json_path) return false;
    
    FILE* f = fopen(json_path, "w");
    if (!f) return false;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": \"2.0\",\n");
    fprintf(f, "  \"io\": {\n");
    fprintf(f, "    \"input_file\": \"%s\",\n", params->io.input_file);
    fprintf(f, "    \"output_file\": \"%s\",\n", params->io.output_file);
    fprintf(f, "    \"format_override\": \"%s\",\n", params->io.format_override);
    fprintf(f, "    \"auto_detect\": %s\n", params->io.auto_detect ? "true" : "false");
    fprintf(f, "  },\n");
    
    fprintf(f, "  \"recovery\": {\n");
    fprintf(f, "    \"level\": \"%s\",\n", uft_recovery_level_name(params->recovery.level));
    fprintf(f, "    \"rev_selection\": \"%s\",\n", uft_rev_select_name(params->recovery.rev_selection));
    fprintf(f, "    \"merge_revolutions\": %s,\n", params->recovery.merge_revolutions ? "true" : "false");
    fprintf(f, "    \"max_revs_to_use\": %d,\n", params->recovery.max_revs_to_use);
    fprintf(f, "    \"pll_bandwidth\": %.2f,\n", params->recovery.pll_bandwidth);
    fprintf(f, "    \"bitcell_tolerance\": %d,\n", params->recovery.bitcell_tolerance);
    fprintf(f, "    \"allow_crc_errors\": %s,\n", params->recovery.allow_crc_errors ? "true" : "false");
    fprintf(f, "    \"detect_weak_bits\": %s\n", params->recovery.detect_weak_bits ? "true" : "false");
    fprintf(f, "  },\n");
    
    fprintf(f, "  \"flux_dump\": {\n");
    fprintf(f, "    \"track\": %d,\n", params->flux_dump.track);
    fprintf(f, "    \"side\": %d,\n", params->flux_dump.side);
    fprintf(f, "    \"revolution\": %d,\n", params->flux_dump.revolution);
    fprintf(f, "    \"max_transitions\": %zu\n", params->flux_dump.max_transitions);
    fprintf(f, "  }\n");
    
    fprintf(f, "}\n");
    
    fclose(f);
    return true;
}

/**
 * @brief Print usage help
 */
void uft_params_print_help(const char* program_name) {
    printf("UFT Universal Floppy Tool - Parameter Reference\n");
    printf("================================================\n\n");
    printf("Usage: %s [options] <input_file>\n\n", program_name ? program_name : "uft");
    
    printf("Input/Output:\n");
    printf("  --in, -i <file>       Input file\n");
    printf("  --out, -o <file>      Output file\n");
    printf("  --format, -f <fmt>    Force format (skip auto-detect)\n");
    printf("\n");
    
    printf("Analysis:\n");
    printf("  --summary, -s         Show disk summary\n");
    printf("  --catalog <file>      Export catalog as JSON\n");
    printf("  --verbose, -v         Verbose output\n");
    printf("  --quiet, -q           Suppress output\n");
    printf("\n");
    
    printf("Flux Dump:\n");
    printf("  --track, -t <n>       Track number (0-%d, -1=all)\n", UFT_MAX_TRACKS - 1);
    printf("  --side <n>            Side (0-1, -1=all)\n");
    printf("  --rev, -r <n>         Revolution (-1=best)\n");
    printf("  --dump <file>         Dump flux to CSV\n");
    printf("  --max-transitions <n> Limit transitions\n");
    printf("\n");
    
    printf("Recovery:\n");
    printf("  --recovery <level>    none|minimal|standard|aggressive|forensic\n");
    printf("  --rev-select <mode>   first|best|voting|merge|all\n");
    printf("  --merge-revs          Merge multiple revolutions\n");
    printf("  --max-revs <n>        Max revolutions to use\n");
    printf("  --allow-crc-errors    Accept sectors with CRC errors\n");
    printf("\n");
    
    printf("Operations:\n");
    printf("  --convert <format>    Convert to format\n");
    printf("  --verify              Verify image integrity\n");
    printf("  --hash [algo]         Generate hash (MD5|SHA1|SHA256)\n");
    printf("\n");
    
    printf("Supported Platforms:\n");
    for (int i = 1; i < UFT_PLATFORM_COUNT; i++) {
        printf("  - %s\n", platform_names[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef PARAMS_TEST

#include <assert.h>

int main(void) {
    printf("=== Universal Params Tests ===\n");
    
    /* Test initialization */
    printf("Testing init... ");
    uft_params_t params;
    uft_params_init(&params);
    assert(params.initialized);
    assert(params.recovery.level == UFT_RECOVERY_STANDARD);
    assert(params.recovery.rev_selection == UFT_REV_SELECT_BEST);
    printf("✓\n");
    
    /* Test platform names */
    printf("Testing platform names... ");
    assert(strcmp(uft_platform_name(UFT_PLATFORM_COMMODORE_64), "Commodore 64") == 0);
    assert(strcmp(uft_platform_name(UFT_PLATFORM_ATARI_8BIT), "Atari 8-bit") == 0);
    assert(strcmp(uft_platform_name(UFT_PLATFORM_APPLE_II), "Apple II") == 0);
    printf("✓\n");
    
    /* Test encoding names */
    printf("Testing encoding names... ");
    assert(strcmp(uft_encoding_name(UFT_ENCODING_GCR_COMMODORE), "GCR (Commodore)") == 0);
    assert(strcmp(uft_encoding_name(UFT_ENCODING_MFM), "MFM (DD)") == 0);
    printf("✓\n");
    
    /* Test platform defaults */
    printf("Testing platform defaults... ");
    uft_params_set_platform_defaults(&params, UFT_PLATFORM_COMMODORE_64);
    assert(params.metadata.encoding == UFT_ENCODING_GCR_COMMODORE);
    assert(params.metadata.num_tracks == 35);
    printf("✓\n");
    
    /* Test validation */
    printf("Testing validation... ");
    char error[256];
    assert(!uft_params_validate(&params, error, sizeof(error)));  /* No input file */
    strncpy(params.io.input_file, "test.d64", UFT_MAX_FILENAME);
    assert(uft_params_validate(&params, error, sizeof(error)));
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* PARAMS_TEST */
