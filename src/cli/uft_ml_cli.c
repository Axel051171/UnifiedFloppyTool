/**
 * @file uft_ml_cli.c
 * @brief UFT ML Decoder Command Line Interface
 * 
 * CLI for training, evaluating, and using ML flux decoders.
 * 
 * Usage:
 *   uft-ml train <dataset> --output <model> [options]
 *   uft-ml eval <model> <dataset>
 *   uft-ml decode <model> <flux-file> [--output <bits>]
 *   uft-ml generate <flux-dir> --output <dataset> [--augment]
 *   uft-ml info <model>
 * 
 * @version 1.0.0
 * @date 2026-01-04
 */

#include "uft/ml/uft_ml_decoder.h"
#include "uft/core/uft_safe_parse.h"
#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <signal.h>
#include "uft/core/uft_safe_parse.h"

/*===========================================================================
 * Globals
 *===========================================================================*/

static volatile int g_interrupted = 0;

/*===========================================================================
 * Signal Handler
 *===========================================================================*/

static void signal_handler(int sig)
{
    (void)sig;
    g_interrupted = 1;
    printf("\nInterrupt received, stopping...\n");
}

/*===========================================================================
 * Progress Callback
 *===========================================================================*/

static void training_progress(int epoch, float loss, void *user)
{
    int total_epochs = *(int *)user;
    
    int bar_width = 40;
    float progress = (float)epoch / (float)total_epochs;
    int filled = (int)(progress * bar_width);
    
    printf("\rEpoch %3d/%d [", epoch, total_epochs);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) printf("=");
        else if (i == filled) printf(">");
        else printf(" ");
    }
    printf("] Loss: %.6f", loss);
    fflush(stdout);
    
    if (epoch == total_epochs) {
        printf("\n");
    }
}

/*===========================================================================
 * Usage
 *===========================================================================*/

static void print_usage(const char *prog)
{
    printf("UFT Machine Learning Decoder v1.0\n\n");
    printf("Usage: %s <command> [options]\n\n", prog);
    printf("Commands:\n");
    printf("  train <dataset>      Train a new model\n");
    printf("  eval <model> <data>  Evaluate model on test data\n");
    printf("  decode <model> <flux> Decode flux file using model\n");
    printf("  generate <dir>       Generate training data from flux files\n");
    printf("  info <model>         Show model information\n");
    printf("\nTraining Options:\n");
    printf("  -o, --output FILE    Output model file\n");
    printf("  -e, --epochs N       Training epochs (default: 100)\n");
    printf("  -b, --batch N        Batch size (default: 32)\n");
    printf("  -l, --lr RATE        Learning rate (default: 0.001)\n");
    printf("  -t, --target ENC     Target encoding: mfm, gcr, fm, apple, c64\n");
    printf("  --hidden N           Hidden layer size (default: 128)\n");
    printf("  --filters N          Conv filters (default: 32)\n");
    printf("  --kernel N           Conv kernel size (default: 5)\n");
    printf("  --split RATIO        Train/validation split (default: 0.8)\n");
    printf("\nGeneration Options:\n");
    printf("  --augment            Generate augmented samples\n");
    printf("  --quality Q          Target quality: pristine, good, fair, poor, critical\n");
    printf("\nGeneral Options:\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help\n");
    printf("\nExamples:\n");
    printf("  %s train data.bin -o model.bin -e 200 -t mfm\n", prog);
    printf("  %s eval model.bin test.bin\n", prog);
    printf("  %s decode model.bin track.scp -o decoded.bin\n", prog);
}

/*===========================================================================
 * Command: Train
 *===========================================================================*/

static int cmd_train(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "Error: train requires a dataset file\n");
        return 1;
    }
    
    const char *dataset_path = argv[0];
    const char *output_path = "model.bin";
    int epochs = 100;
    int batch_size = 32;
    float learning_rate = 0.001f;
    float split_ratio = 0.8f;
    uft_ml_target_t target = UFT_ML_TARGET_MFM;
    int hidden_size = 128;
    int num_filters = 32;
    int kernel_size = 5;
    bool verbose = false;
    
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            output_path = argv[++i];
        } else if ((strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--epochs") == 0) && i + 1 < argc) {
            { int32_t t; if(uft_parse_int32(argv[++i],&t,10)) epochs=t; }
        } else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--batch") == 0) && i + 1 < argc) {
            { int32_t t; if(uft_parse_int32(argv[++i],&t,10)) batch_size=t; }
        } else if ((strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--lr") == 0) && i + 1 < argc) {
            learning_rate = (float)atof(argv[++i]);
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target") == 0) && i + 1 < argc) {
            const char *t = argv[++i];
            if (strcmp(t, "mfm") == 0) target = UFT_ML_TARGET_MFM;
            else if (strcmp(t, "gcr") == 0) target = UFT_ML_TARGET_GCR;
            else if (strcmp(t, "fm") == 0) target = UFT_ML_TARGET_FM;
            else if (strcmp(t, "apple") == 0) target = UFT_ML_TARGET_APPLE_GCR;
            else if (strcmp(t, "c64") == 0) target = UFT_ML_TARGET_C64_GCR;
        } else if (strcmp(argv[i], "--hidden") == 0 && i + 1 < argc) {
            { int32_t t; if(uft_parse_int32(argv[++i],&t,10)) hidden_size=t; }
        } else if (strcmp(argv[i], "--filters") == 0 && i + 1 < argc) {
            { int32_t t; if(uft_parse_int32(argv[++i],&t,10)) num_filters=t; }
        } else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            { int32_t t; if(uft_parse_int32(argv[++i],&t,10)) kernel_size=t; }
        } else if (strcmp(argv[i], "--split") == 0 && i + 1 < argc) {
            split_ratio = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
    }
    
    /* Load dataset */
    printf("Loading dataset: %s\n", dataset_path);
    uft_ml_dataset_t *full_data = uft_ml_dataset_load(dataset_path);
    if (!full_data) {
        fprintf(stderr, "Error: Cannot load dataset\n");
        return 1;
    }
    
    if (verbose) {
        uft_ml_dataset_print_stats(full_data);
    }
    
    /* Split into train/validation */
    uft_ml_dataset_t *train_data = uft_ml_dataset_create(full_data->count);
    uft_ml_dataset_t *valid_data = uft_ml_dataset_create(full_data->count);
    
    if (!train_data || !valid_data) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        uft_ml_dataset_free(full_data);
        return 1;
    }
    
    uft_ml_dataset_split(full_data, train_data, valid_data, split_ratio);
    uft_ml_dataset_free(full_data);
    
    printf("Training samples: %zu, Validation samples: %zu\n",
           train_data->count, valid_data->count);
    
    /* Configure model */
    uft_ml_model_config_t config;
    uft_ml_config_recommended(&config, target);
    config.hidden_size = hidden_size;
    config.num_filters = num_filters;
    config.kernel_size = kernel_size;
    config.batch_size = batch_size;
    config.epochs = epochs;
    config.learning_rate = learning_rate;
    
    /* Create model */
    printf("Creating model: %s, %d filters, %d hidden\n",
           uft_ml_target_name(target), num_filters, hidden_size);
    
    uft_ml_model_t *model = uft_ml_model_create(&config);
    if (!model) {
        fprintf(stderr, "Error: Cannot create model\n");
        uft_ml_dataset_free(train_data);
        uft_ml_dataset_free(valid_data);
        return 1;
    }
    
    /* Train */
    printf("\nTraining for %d epochs...\n", epochs);
    int total_epochs = epochs;
    
    int ret = uft_ml_model_train(model, train_data, valid_data,
                                  training_progress, &total_epochs);
    
    if (ret != 0) {
        fprintf(stderr, "Error: Training failed\n");
        uft_ml_model_free(model);
        uft_ml_dataset_free(train_data);
        uft_ml_dataset_free(valid_data);
        return 1;
    }
    
    /* Evaluate */
    printf("\nEvaluating on validation set...\n");
    uft_ml_metrics_t metrics;
    uft_ml_model_evaluate(model, valid_data, &metrics);
    
    printf("\nResults:\n");
    printf("  Accuracy:         %.2f%%\n", metrics.accuracy * 100.0f);
    printf("  Bit Error Rate:   %.4f%%\n", metrics.bit_error_rate * 100.0f);
    printf("  Avg Inference:    %.2f ms\n", metrics.avg_inference_ms);
    
    if (verbose) {
        printf("\nPer-quality accuracy:\n");
        printf("  Pristine: %.2f%%\n", metrics.per_quality_accuracy[0] * 100.0f);
        printf("  Good:     %.2f%%\n", metrics.per_quality_accuracy[1] * 100.0f);
        printf("  Fair:     %.2f%%\n", metrics.per_quality_accuracy[2] * 100.0f);
        printf("  Poor:     %.2f%%\n", metrics.per_quality_accuracy[3] * 100.0f);
        printf("  Critical: %.2f%%\n", metrics.per_quality_accuracy[4] * 100.0f);
    }
    
    /* Save model */
    printf("\nSaving model to: %s\n", output_path);
    ret = uft_ml_model_save(model, output_path);
    
    if (ret != 0) {
        fprintf(stderr, "Error: Cannot save model\n");
    } else {
        printf("Model saved successfully.\n");
    }
    
    /* Cleanup */
    uft_ml_model_free(model);
    uft_ml_dataset_free(train_data);
    uft_ml_dataset_free(valid_data);
    
    return ret;
}

/*===========================================================================
 * Command: Evaluate
 *===========================================================================*/

static int cmd_eval(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: eval requires model and dataset files\n");
        return 1;
    }
    
    const char *model_path = argv[0];
    const char *dataset_path = argv[1];
    
    /* Load model */
    printf("Loading model: %s\n", model_path);
    uft_ml_model_t *model = uft_ml_model_load(model_path);
    if (!model) {
        fprintf(stderr, "Error: Cannot load model\n");
        return 1;
    }
    
    /* Load dataset */
    printf("Loading dataset: %s\n", dataset_path);
    uft_ml_dataset_t *dataset = uft_ml_dataset_load(dataset_path);
    if (!dataset) {
        fprintf(stderr, "Error: Cannot load dataset\n");
        uft_ml_model_free(model);
        return 1;
    }
    
    /* Evaluate */
    printf("Evaluating on %zu samples...\n", dataset->count);
    uft_ml_metrics_t metrics;
    uft_ml_model_evaluate(model, dataset, &metrics);
    
    printf("\nResults:\n");
    printf("  Accuracy:         %.2f%%\n", metrics.accuracy * 100.0f);
    printf("  Precision:        %.2f%%\n", metrics.precision * 100.0f);
    printf("  Recall:           %.2f%%\n", metrics.recall * 100.0f);
    printf("  F1 Score:         %.4f\n", metrics.f1_score);
    printf("  Bit Error Rate:   %.4f%%\n", metrics.bit_error_rate * 100.0f);
    printf("  Avg Inference:    %.2f ms\n", metrics.avg_inference_ms);
    
    printf("\nPer-quality accuracy:\n");
    printf("  Pristine: %.2f%%\n", metrics.per_quality_accuracy[0] * 100.0f);
    printf("  Good:     %.2f%%\n", metrics.per_quality_accuracy[1] * 100.0f);
    printf("  Fair:     %.2f%%\n", metrics.per_quality_accuracy[2] * 100.0f);
    printf("  Poor:     %.2f%%\n", metrics.per_quality_accuracy[3] * 100.0f);
    printf("  Critical: %.2f%%\n", metrics.per_quality_accuracy[4] * 100.0f);
    
    uft_ml_model_free(model);
    uft_ml_dataset_free(dataset);
    
    return 0;
}

/*===========================================================================
 * Command: Info
 *===========================================================================*/

static int cmd_info(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "Error: info requires a model file\n");
        return 1;
    }
    
    const char *model_path = argv[0];
    
    /* Load model */
    uft_ml_model_t *model = uft_ml_model_load(model_path);
    if (!model) {
        fprintf(stderr, "Error: Cannot load model: %s\n", model_path);
        return 1;
    }
    
    printf("=== UFT ML Model Info ===\n");
    printf("File:         %s\n", model_path);
    printf("Type:         %s\n", uft_ml_model_type_name(model->config.type));
    printf("Target:       %s\n", uft_ml_target_name(model->config.target));
    printf("Input size:   %d\n", model->config.input_size);
    printf("Hidden size:  %d\n", model->config.hidden_size);
    printf("Num filters:  %d\n", model->config.num_filters);
    printf("Kernel size:  %d\n", model->config.kernel_size);
    printf("Dropout:      %.2f\n", model->config.dropout);
    printf("Parameters:   %zu\n", model->total_params);
    
    uft_ml_model_free(model);
    
    return 0;
}

/*===========================================================================
 * Command: Decode (Stub)
 *===========================================================================*/

static int cmd_decode(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: decode requires model and flux files\n");
        return 1;
    }
    
    const char *model_path = argv[0];
    const char *flux_path = argv[1];
    
    printf("Loading model: %s\n", model_path);
    printf("Would decode flux file: %s\n", flux_path);
    printf("\nNote: Full flux file support requires format readers integration.\n");
    
    /* This would:
     * 1. Load model
     * 2. Create decoder
     * 3. Load flux file (SCP, A2R, etc.)
     * 4. Extract flux intervals
     * 5. Decode using ML
     * 6. Output decoded bits
     */
    
    return 0;
}

/*===========================================================================
 * Command: Generate (Stub)
 *===========================================================================*/

static int cmd_generate(int argc, char **argv)
{
    if (argc < 1) {
        fprintf(stderr, "Error: generate requires a directory\n");
        return 1;
    }
    
    const char *flux_dir = argv[0];
    
    printf("Would scan for flux files in: %s\n", flux_dir);
    printf("\nNote: Training data generation requires:\n");
    printf("  1. Flux file format support\n");
    printf("  2. Traditional PLL decoder for ground truth\n");
    printf("  3. Sample augmentation pipeline\n");
    
    return 0;
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    const char *cmd = argv[1];
    int remaining = argc - 2;
    char **args = argv + 2;
    
    int result;
    if (strcmp(cmd, "train") == 0) {
        result = cmd_train(remaining, args);
    } else if (strcmp(cmd, "eval") == 0) {
        result = cmd_eval(remaining, args);
    } else if (strcmp(cmd, "decode") == 0) {
        result = cmd_decode(remaining, args);
    } else if (strcmp(cmd, "generate") == 0) {
        result = cmd_generate(remaining, args);
    } else if (strcmp(cmd, "info") == 0) {
        result = cmd_info(remaining, args);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        result = 1;
    }
    
    return result;
}
