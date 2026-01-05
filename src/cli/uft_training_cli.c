/**
 * @file uft_training_cli.c
 * @brief CLI Tool for Training Data Generation
 * @version 1.0.0
 * @date 2025
 *
 * Usage:
 *   uft-training generate --flux <file> --ground-truth <file> --output <file>
 *   uft-training augment --input <file> --output <file> --variants 4
 *   uft-training stats --input <file>
 *   uft-training export --input <file> --format csv --output <file>
 *
 * SPDX-License-Identifier: MIT
 */

#include "uft/ml/uft_ml_training_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/*============================================================================
 * CLI OPTIONS
 *============================================================================*/

typedef enum {
    CMD_NONE = 0,
    CMD_GENERATE,
    CMD_AUGMENT,
    CMD_STATS,
    CMD_EXPORT,
    CMD_SYNTHETIC,
    CMD_SPLIT,
    CMD_HELP
} command_t;

typedef struct {
    command_t command;
    
    /* Input files */
    const char* flux_file;
    const char* ground_truth_file;
    const char* input_file;
    
    /* Output */
    const char* output_file;
    const char* export_format;  /* csv, numpy, binary */
    
    /* Generation options */
    uint32_t window_size;
    uint32_t window_stride;
    uint32_t bits_per_sample;
    uft_tg_encoding_t encoding;
    
    /* Augmentation options */
    uint32_t augment_variants;
    float augment_probability;
    
    /* Quality filters */
    float min_snr;
    float max_jitter;
    
    /* Split options */
    float train_ratio;
    float val_ratio;
    
    /* Misc */
    uint32_t max_samples;
    bool verbose;
    bool balance;
} cli_options_t;

/*============================================================================
 * HELP TEXT
 *============================================================================*/

static void print_usage(const char* prog) {
    printf("UFT Training Data Generator v%d.%d.%d\n\n",
           UFT_TG_VERSION_MAJOR, UFT_TG_VERSION_MINOR, UFT_TG_VERSION_PATCH);
    
    printf("Usage: %s <command> [options]\n\n", prog);
    
    printf("Commands:\n");
    printf("  generate   Generate training samples from flux + ground truth\n");
    printf("  augment    Augment existing dataset with variations\n");
    printf("  synthetic  Generate synthetic training data from patterns\n");
    printf("  stats      Show dataset statistics\n");
    printf("  export     Export dataset to CSV/NumPy format\n");
    printf("  split      Split dataset into train/val/test\n");
    printf("  help       Show this help\n\n");
    
    printf("Generate options:\n");
    printf("  --flux, -f <file>         Input flux file (SCP, raw)\n");
    printf("  --ground-truth, -g <file> Ground truth image (D64, ADF, IMG)\n");
    printf("  --output, -o <file>       Output dataset file\n");
    printf("  --window <size>           Flux window size (default: 128)\n");
    printf("  --stride <size>           Window stride (default: 32)\n");
    printf("  --bits <count>            Bits per sample (default: 64)\n");
    printf("  --encoding <type>         Force encoding (mfm, fm, gcr-c64, gcr-apple)\n");
    printf("  --min-snr <dB>            Minimum SNR threshold (default: 10)\n");
    printf("  --max-jitter <%%>          Maximum jitter threshold (default: 25)\n");
    printf("  --no-augment              Disable augmentation\n");
    printf("  --augment-prob <0-1>      Augmentation probability (default: 0.5)\n");
    printf("  --variants <n>            Augmented variants per sample (default: 4)\n\n");
    
    printf("Augment options:\n");
    printf("  --input, -i <file>        Input dataset\n");
    printf("  --output, -o <file>       Output augmented dataset\n");
    printf("  --variants <n>            Variants per sample\n\n");
    
    printf("Export options:\n");
    printf("  --input, -i <file>        Input dataset\n");
    printf("  --output, -o <file>       Output file\n");
    printf("  --format <type>           Format: csv, numpy (default: csv)\n");
    printf("  --max <n>                 Maximum samples to export\n\n");
    
    printf("Split options:\n");
    printf("  --input, -i <file>        Input dataset\n");
    printf("  --output, -o <prefix>     Output file prefix\n");
    printf("  --train <ratio>           Training ratio (default: 0.8)\n");
    printf("  --val <ratio>             Validation ratio (default: 0.1)\n\n");
    
    printf("Common options:\n");
    printf("  --verbose, -v             Verbose output\n");
    printf("  --help, -h                Show this help\n\n");
    
    printf("Examples:\n");
    printf("  %s generate -f disk.scp -g disk.d64 -o train.uft\n", prog);
    printf("  %s augment -i train.uft -o train_aug.uft --variants 8\n", prog);
    printf("  %s export -i train.uft -o samples.csv --format csv\n", prog);
    printf("  %s stats -i train.uft\n", prog);
}

/*============================================================================
 * OPTION PARSING
 *============================================================================*/

static uft_tg_encoding_t parse_encoding(const char* str) {
    if (strcasecmp(str, "mfm") == 0) return UFT_TG_ENC_MFM;
    if (strcasecmp(str, "fm") == 0) return UFT_TG_ENC_FM;
    if (strcasecmp(str, "gcr-c64") == 0 || strcasecmp(str, "gcr_c64") == 0) 
        return UFT_TG_ENC_GCR_C64;
    if (strcasecmp(str, "gcr-apple") == 0 || strcasecmp(str, "gcr_apple") == 0)
        return UFT_TG_ENC_GCR_APPLE;
    if (strcasecmp(str, "amiga") == 0) return UFT_TG_ENC_AMIGA;
    return UFT_TG_ENC_MIXED;
}

static int parse_options(int argc, char* argv[], cli_options_t* opts) {
    static struct option long_options[] = {
        {"flux", required_argument, 0, 'f'},
        {"ground-truth", required_argument, 0, 'g'},
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'F'},
        {"window", required_argument, 0, 'w'},
        {"stride", required_argument, 0, 's'},
        {"bits", required_argument, 0, 'b'},
        {"encoding", required_argument, 0, 'e'},
        {"min-snr", required_argument, 0, 'S'},
        {"max-jitter", required_argument, 0, 'J'},
        {"variants", required_argument, 0, 'V'},
        {"augment-prob", required_argument, 0, 'P'},
        {"max", required_argument, 0, 'M'},
        {"train", required_argument, 0, 'T'},
        {"val", required_argument, 0, 'L'},
        {"no-augment", no_argument, 0, 'N'},
        {"balance", no_argument, 0, 'B'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Set defaults */
    memset(opts, 0, sizeof(*opts));
    opts->window_size = 128;
    opts->window_stride = 32;
    opts->bits_per_sample = 64;
    opts->encoding = UFT_TG_ENC_MIXED;
    opts->augment_variants = 4;
    opts->augment_probability = 0.5f;
    opts->min_snr = 10.0f;
    opts->max_jitter = 25.0f;
    opts->train_ratio = 0.8f;
    opts->val_ratio = 0.1f;
    opts->export_format = "csv";
    
    /* Parse command */
    if (argc < 2) {
        opts->command = CMD_HELP;
        return 0;
    }
    
    const char* cmd = argv[1];
    if (strcmp(cmd, "generate") == 0) opts->command = CMD_GENERATE;
    else if (strcmp(cmd, "augment") == 0) opts->command = CMD_AUGMENT;
    else if (strcmp(cmd, "synthetic") == 0) opts->command = CMD_SYNTHETIC;
    else if (strcmp(cmd, "stats") == 0) opts->command = CMD_STATS;
    else if (strcmp(cmd, "export") == 0) opts->command = CMD_EXPORT;
    else if (strcmp(cmd, "split") == 0) opts->command = CMD_SPLIT;
    else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 || 
             strcmp(cmd, "--help") == 0) {
        opts->command = CMD_HELP;
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return -1;
    }
    
    /* Parse options */
    int opt;
    optind = 2;  /* Start after command */
    
    while ((opt = getopt_long(argc, argv, "f:g:i:o:w:s:b:e:v:h",
                               long_options, NULL)) != -1) {
        switch (opt) {
            case 'f': opts->flux_file = optarg; break;
            case 'g': opts->ground_truth_file = optarg; break;
            case 'i': opts->input_file = optarg; break;
            case 'o': opts->output_file = optarg; break;
            case 'F': opts->export_format = optarg; break;
            case 'w': opts->window_size = atoi(optarg); break;
            case 's': opts->window_stride = atoi(optarg); break;
            case 'b': opts->bits_per_sample = atoi(optarg); break;
            case 'e': opts->encoding = parse_encoding(optarg); break;
            case 'S': opts->min_snr = atof(optarg); break;
            case 'J': opts->max_jitter = atof(optarg); break;
            case 'V': opts->augment_variants = atoi(optarg); break;
            case 'P': opts->augment_probability = atof(optarg); break;
            case 'M': opts->max_samples = atoi(optarg); break;
            case 'T': opts->train_ratio = atof(optarg); break;
            case 'L': opts->val_ratio = atof(optarg); break;
            case 'N': opts->augment_probability = 0.0f; break;
            case 'B': opts->balance = true; break;
            case 'v': opts->verbose = true; break;
            case 'h': opts->command = CMD_HELP; return 0;
            default: return -1;
        }
    }
    
    return 0;
}

/*============================================================================
 * COMMAND IMPLEMENTATIONS
 *============================================================================*/

static int cmd_generate(const cli_options_t* opts) {
    if (!opts->flux_file) {
        fprintf(stderr, "Error: --flux file required\n");
        return 1;
    }
    if (!opts->ground_truth_file) {
        fprintf(stderr, "Error: --ground-truth file required\n");
        return 1;
    }
    if (!opts->output_file) {
        fprintf(stderr, "Error: --output file required\n");
        return 1;
    }
    
    printf("Generating training data...\n");
    printf("  Flux file: %s\n", opts->flux_file);
    printf("  Ground truth: %s\n", opts->ground_truth_file);
    printf("  Output: %s\n", opts->output_file);
    
    /* Configure generator */
    uft_tg_config_t config;
    uft_tg_config_default(&config);
    config.window_size = opts->window_size;
    config.window_stride = opts->window_stride;
    config.bits_per_sample = opts->bits_per_sample;
    config.min_snr_db = opts->min_snr;
    config.max_jitter_pct = opts->max_jitter;
    config.augment_probability = opts->augment_probability;
    config.augment_variants = opts->augment_variants;
    config.balance_encodings = opts->balance;
    
    uft_tg_generator_t* gen = uft_tg_create(&config);
    if (!gen) {
        fprintf(stderr, "Error: Failed to create generator\n");
        return 1;
    }
    
    /* Load ground truth */
    int err = uft_tg_load_ground_truth(gen, opts->ground_truth_file);
    if (err != UFT_TG_OK) {
        fprintf(stderr, "Error: Failed to load ground truth: %s\n",
                uft_tg_error_string(err));
        uft_tg_destroy(gen);
        return 1;
    }
    
    if (opts->verbose) {
        printf("  Ground truth loaded\n");
    }
    
    /* Load flux */
    err = uft_tg_load_flux(gen, opts->flux_file);
    if (err != UFT_TG_OK) {
        fprintf(stderr, "Error: Failed to load flux: %s\n",
                uft_tg_error_string(err));
        uft_tg_destroy(gen);
        return 1;
    }
    
    if (opts->verbose) {
        printf("  Flux loaded\n");
    }
    
    /* Generate samples */
    uft_tg_dataset_t* dataset = uft_tg_dataset_create(100000);
    if (!dataset) {
        fprintf(stderr, "Error: Failed to create dataset\n");
        uft_tg_destroy(gen);
        return 1;
    }
    
    printf("  Generating samples...\n");
    int count = uft_tg_generate_samples(gen, dataset);
    
    if (count < 0) {
        fprintf(stderr, "Error: Generation failed: %s\n",
                uft_tg_error_string(count));
        uft_tg_dataset_destroy(dataset);
        uft_tg_destroy(gen);
        return 1;
    }
    
    printf("  Generated %d samples\n", count);
    
    /* Shuffle */
    uft_tg_dataset_shuffle(dataset);
    
    /* Save */
    err = uft_tg_dataset_save(dataset, opts->output_file);
    if (err != UFT_TG_OK) {
        fprintf(stderr, "Error: Failed to save: %s\n",
                uft_tg_error_string(err));
        uft_tg_dataset_destroy(dataset);
        uft_tg_destroy(gen);
        return 1;
    }
    
    printf("  Saved to %s\n", opts->output_file);
    
    if (opts->verbose) {
        uft_tg_dataset_print_stats(dataset);
    }
    
    uft_tg_dataset_destroy(dataset);
    uft_tg_destroy(gen);
    
    return 0;
}

static int cmd_augment(const cli_options_t* opts) {
    if (!opts->input_file) {
        fprintf(stderr, "Error: --input file required\n");
        return 1;
    }
    if (!opts->output_file) {
        fprintf(stderr, "Error: --output file required\n");
        return 1;
    }
    
    printf("Augmenting dataset...\n");
    printf("  Input: %s\n", opts->input_file);
    printf("  Output: %s\n", opts->output_file);
    printf("  Variants: %u\n", opts->augment_variants);
    
    /* Load input */
    uft_tg_dataset_t* input = uft_tg_dataset_load(opts->input_file);
    if (!input) {
        fprintf(stderr, "Error: Failed to load input dataset\n");
        return 1;
    }
    
    printf("  Loaded %u samples\n", input->count);
    
    /* Create augmented dataset */
    uft_tg_dataset_t* output = uft_tg_dataset_create(
        input->count * (opts->augment_variants + 1));
    if (!output) {
        fprintf(stderr, "Error: Failed to create output dataset\n");
        uft_tg_dataset_destroy(input);
        return 1;
    }
    
    /* Copy originals and generate variants */
    for (uint32_t i = 0; i < input->count; i++) {
        /* Copy original */
        uft_tg_dataset_add(output, &input->samples[i]);
        
        /* Generate variants */
        for (uint32_t v = 0; v < opts->augment_variants; v++) {
            uft_tg_sample_t aug = input->samples[i];
            uft_tg_augment_t type = (uft_tg_augment_t)(1 + (v % 9));
            uft_tg_augment_sample(&aug, type, 0.5f);
            uft_tg_dataset_add(output, &aug);
        }
        
        if (opts->verbose && (i + 1) % 1000 == 0) {
            printf("  Processed %u/%u\n", i + 1, input->count);
        }
    }
    
    /* Shuffle */
    uft_tg_dataset_shuffle(output);
    
    /* Save */
    int err = uft_tg_dataset_save(output, opts->output_file);
    if (err != UFT_TG_OK) {
        fprintf(stderr, "Error: Failed to save: %s\n",
                uft_tg_error_string(err));
        uft_tg_dataset_destroy(output);
        uft_tg_dataset_destroy(input);
        return 1;
    }
    
    printf("  Generated %u augmented samples\n", output->count);
    printf("  Saved to %s\n", opts->output_file);
    
    uft_tg_dataset_destroy(output);
    uft_tg_dataset_destroy(input);
    
    return 0;
}

static int cmd_stats(const cli_options_t* opts) {
    if (!opts->input_file) {
        fprintf(stderr, "Error: --input file required\n");
        return 1;
    }
    
    uft_tg_dataset_t* ds = uft_tg_dataset_load(opts->input_file);
    if (!ds) {
        fprintf(stderr, "Error: Failed to load dataset\n");
        return 1;
    }
    
    printf("Dataset: %s\n\n", opts->input_file);
    uft_tg_dataset_print_stats(ds);
    
    /* Extended stats */
    uft_tg_stats_t stats;
    uft_tg_dataset_get_stats(ds, &stats);
    
    printf("\nQuality Metrics:\n");
    printf("  Average SNR:    %.2f dB\n", stats.avg_snr_db);
    printf("  Minimum SNR:    %.2f dB\n", stats.min_snr_db);
    printf("  Average Jitter: %.2f%%\n", stats.avg_jitter_pct);
    printf("  Maximum Jitter: %.2f%%\n", stats.max_jitter_pct);
    
    printf("\nData Volumes:\n");
    printf("  Total flux values: %u\n", stats.total_flux_values);
    printf("  Total bits:        %u\n", stats.total_bits);
    printf("  Avg flux/sample:   %.1f\n", 
           (float)stats.total_flux_values / stats.total_samples);
    printf("  Avg bits/sample:   %.1f\n",
           (float)stats.total_bits / stats.total_samples);
    
    uft_tg_dataset_destroy(ds);
    return 0;
}

static int cmd_export(const cli_options_t* opts) {
    if (!opts->input_file) {
        fprintf(stderr, "Error: --input file required\n");
        return 1;
    }
    if (!opts->output_file) {
        fprintf(stderr, "Error: --output file required\n");
        return 1;
    }
    
    printf("Exporting dataset...\n");
    printf("  Input: %s\n", opts->input_file);
    printf("  Output: %s\n", opts->output_file);
    printf("  Format: %s\n", opts->export_format);
    
    uft_tg_dataset_t* ds = uft_tg_dataset_load(opts->input_file);
    if (!ds) {
        fprintf(stderr, "Error: Failed to load dataset\n");
        return 1;
    }
    
    int err;
    if (strcasecmp(opts->export_format, "csv") == 0) {
        err = uft_tg_dataset_export_csv(ds, opts->output_file, opts->max_samples);
    } else if (strcasecmp(opts->export_format, "numpy") == 0 ||
               strcasecmp(opts->export_format, "npz") == 0) {
        err = uft_tg_dataset_export_numpy(ds, opts->output_file);
    } else {
        fprintf(stderr, "Error: Unknown format: %s\n", opts->export_format);
        uft_tg_dataset_destroy(ds);
        return 1;
    }
    
    if (err != UFT_TG_OK) {
        fprintf(stderr, "Error: Export failed: %s\n",
                uft_tg_error_string(err));
        uft_tg_dataset_destroy(ds);
        return 1;
    }
    
    printf("  Exported %u samples\n", 
           opts->max_samples > 0 && opts->max_samples < ds->count ?
           opts->max_samples : ds->count);
    
    uft_tg_dataset_destroy(ds);
    return 0;
}

static int cmd_split(const cli_options_t* opts) {
    if (!opts->input_file) {
        fprintf(stderr, "Error: --input file required\n");
        return 1;
    }
    if (!opts->output_file) {
        fprintf(stderr, "Error: --output prefix required\n");
        return 1;
    }
    
    printf("Splitting dataset...\n");
    printf("  Input: %s\n", opts->input_file);
    printf("  Train ratio: %.2f\n", opts->train_ratio);
    printf("  Val ratio: %.2f\n", opts->val_ratio);
    printf("  Test ratio: %.2f\n", 1.0f - opts->train_ratio - opts->val_ratio);
    
    uft_tg_dataset_t* ds = uft_tg_dataset_load(opts->input_file);
    if (!ds) {
        fprintf(stderr, "Error: Failed to load dataset\n");
        return 1;
    }
    
    /* Shuffle before split */
    uft_tg_dataset_shuffle(ds);
    
    /* Create splits */
    uft_tg_dataset_t* train = uft_tg_dataset_create(ds->count);
    uft_tg_dataset_t* val = uft_tg_dataset_create(ds->count);
    uft_tg_dataset_t* test = uft_tg_dataset_create(ds->count);
    
    if (!train || !val || !test) {
        fprintf(stderr, "Error: Failed to create split datasets\n");
        goto cleanup;
    }
    
    int err = uft_tg_dataset_split(ds, opts->train_ratio, opts->val_ratio,
                                   train, val, test);
    if (err != UFT_TG_OK) {
        fprintf(stderr, "Error: Split failed: %s\n",
                uft_tg_error_string(err));
        goto cleanup;
    }
    
    /* Save splits */
    char path[512];
    
    snprintf(path, sizeof(path), "%s_train.uft", opts->output_file);
    err = uft_tg_dataset_save(train, path);
    if (err == UFT_TG_OK) {
        printf("  Train: %u samples -> %s\n", train->count, path);
    }
    
    snprintf(path, sizeof(path), "%s_val.uft", opts->output_file);
    err = uft_tg_dataset_save(val, path);
    if (err == UFT_TG_OK) {
        printf("  Val: %u samples -> %s\n", val->count, path);
    }
    
    snprintf(path, sizeof(path), "%s_test.uft", opts->output_file);
    err = uft_tg_dataset_save(test, path);
    if (err == UFT_TG_OK) {
        printf("  Test: %u samples -> %s\n", test->count, path);
    }
    
cleanup:
    uft_tg_dataset_destroy(test);
    uft_tg_dataset_destroy(val);
    uft_tg_dataset_destroy(train);
    uft_tg_dataset_destroy(ds);
    
    return 0;
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char* argv[]) {
    cli_options_t opts;
    
    if (parse_options(argc, argv, &opts) != 0) {
        fprintf(stderr, "Use --help for usage information\n");
        return 1;
    }
    
    switch (opts.command) {
        case CMD_GENERATE:
            return cmd_generate(&opts);
        case CMD_AUGMENT:
            return cmd_augment(&opts);
        case CMD_STATS:
            return cmd_stats(&opts);
        case CMD_EXPORT:
            return cmd_export(&opts);
        case CMD_SPLIT:
            return cmd_split(&opts);
        case CMD_HELP:
        default:
            print_usage(argv[0]);
            return 0;
    }
}
