/**
 * @file uft_batch_cli.c
 * @brief UFT Batch Processing Command Line Interface
 * 
 * C-004: CLI for batch disk image processing
 * 
 * Usage:
 *   uft-batch analyze <directory> [options]
 *   uft-batch convert <input-dir> <output-dir> --format <fmt> [options]
 *   uft-batch verify <directory> [options]
 *   uft-batch hash <directory> [options]
 *   uft-batch resume <state-file>
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 */

#include "uft/batch/uft_batch.h"
#include "uft/uft_format_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*===========================================================================
 * Globals
 *===========================================================================*/

static uft_batch_ctx_t *g_batch_ctx = NULL;
static volatile int g_interrupted = 0;

/*===========================================================================
 * Signal Handler
 *===========================================================================*/

static void signal_handler(int sig)
{
    (void)sig;
    g_interrupted = 1;
    if (g_batch_ctx) {
        printf("\nInterrupt received, stopping batch...\n");
        uft_batch_stop(g_batch_ctx);
    }
}

/*===========================================================================
 * Progress Callback
 *===========================================================================*/

static void progress_callback(const uft_batch_progress_t *progress, void *user_data)
{
    (void)user_data;
    
    /* Clear line and print progress */
    printf("\r[%5.1f%%] Job %u: %s - %s",
           progress->batch_progress * 100.0f,
           progress->job_id,
           progress->job_name ? progress->job_name : "Unknown",
           progress->current_op ? progress->current_op : "Processing...");
    fflush(stdout);
}

static void complete_callback(const uft_batch_job_t *job, void *user_data)
{
    (void)user_data;
    
    const char *status = uft_job_status_name(job->status);
    printf("\n  [%s] %s", status, job->input_path);
    
    if (job->status == UFT_JOB_COMPLETED && job->result_msg[0]) {
        printf(" - %s", job->result_msg);
    } else if (job->status == UFT_JOB_FAILED && job->result_msg[0]) {
        printf(" - ERROR: %s", job->result_msg);
    }
    printf("\n");
}

static void error_callback(const uft_batch_error_t *error, void *user_data)
{
    (void)user_data;
    
    const char *severity[] = {"INFO", "WARNING", "ERROR", "FATAL"};
    fprintf(stderr, "\n  [%s] Job %u: %s\n", 
            severity[error->severity], error->job_id, error->message);
}

/*===========================================================================
 * Usage
 *===========================================================================*/

static void print_usage(const char *prog)
{
    printf("UFT Batch Processing Tool v1.0\n\n");
    printf("Usage: %s <command> [options]\n\n", prog);
    printf("Commands:\n");
    printf("  analyze <dir>      Analyze all disk images in directory\n");
    printf("  convert <in> <out> Convert disk images to new format\n");
    printf("  verify <dir>       Verify integrity of disk images\n");
    printf("  hash <dir>         Calculate checksums for all images\n");
    printf("  resume <state>     Resume interrupted batch from state file\n");
    printf("\nOptions:\n");
    printf("  -r, --recursive    Process subdirectories\n");
    printf("  -p, --pattern PAT  File pattern (default: *)\n");
    printf("  -j, --jobs N       Number of parallel jobs (default: 4)\n");
    printf("  -f, --format FMT   Output format for convert\n");
    printf("  -o, --output DIR   Output directory\n");
    printf("  -s, --state FILE   State file for resume capability\n");
    printf("  --report FILE      Generate report file\n");
    printf("  --json             Generate JSON report\n");
    printf("  --csv              Generate CSV report\n");
    printf("  --skip-existing    Skip if output already exists\n");
    printf("  --verify           Verify output after writing\n");
    printf("  -v, --verbose      Verbose output\n");
    printf("  -q, --quiet        Quiet mode (errors only)\n");
    printf("  -h, --help         Show this help\n");
    printf("\nExamples:\n");
    printf("  %s analyze /mnt/floppies -r\n", prog);
    printf("  %s convert /in /out -f adf -r --verify\n", prog);
    printf("  %s hash /archive -r --csv --report hashes.csv\n", prog);
}

/*===========================================================================
 * Command Handlers
 *===========================================================================*/

static int cmd_analyze(int argc, char **argv, uft_batch_config_t *config)
{
    if (argc < 1) {
        fprintf(stderr, "Error: analyze requires a directory\n");
        return 1;
    }
    
    const char *input_dir = argv[0];
    bool recursive = false;
    const char *pattern = "*";
    
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
            recursive = true;
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pattern") == 0) && i + 1 < argc) {
            pattern = argv[++i];
        }
    }
    
    /* Create batch context */
    g_batch_ctx = uft_batch_create(config);
    if (!g_batch_ctx) {
        fprintf(stderr, "Error: Failed to create batch context\n");
        return 1;
    }
    
    /* Add jobs */
    printf("Scanning %s for %s...\n", input_dir, pattern);
    uint32_t count = uft_batch_add_directory(g_batch_ctx, input_dir, pattern, 
                                              recursive, UFT_JOB_READ);
    printf("Found %u files to analyze\n\n", count);
    
    if (count == 0) {
        uft_batch_destroy(g_batch_ctx);
        return 0;
    }
    
    /* Start processing */
    uft_batch_start(g_batch_ctx);
    uft_batch_wait(g_batch_ctx, 0);
    
    /* Print summary */
    printf("\n");
    uft_batch_print_summary(g_batch_ctx);
    
    /* Generate report if requested */
    if (config->report_file[0]) {
        uft_batch_report_json(g_batch_ctx, config->report_file);
        printf("Report saved to: %s\n", config->report_file);
    }
    
    uft_batch_destroy(g_batch_ctx);
    g_batch_ctx = NULL;
    
    return 0;
}

static int cmd_convert(int argc, char **argv, uft_batch_config_t *config)
{
    if (argc < 2) {
        fprintf(stderr, "Error: convert requires input and output directories\n");
        return 1;
    }
    
    const char *input_dir = argv[0];
    const char *output_dir = argv[1];
    bool recursive = false;
    const char *pattern = "*";
    const char *format = NULL;
    
    strncpy(config->output_dir, output_dir, UFT_BATCH_MAX_PATH - 1);
    
    /* Parse options */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
            recursive = true;
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pattern") == 0) && i + 1 < argc) {
            pattern = argv[++i];
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) && i + 1 < argc) {
            format = argv[++i];
        } else if (strcmp(argv[i], "--skip-existing") == 0) {
            config->skip_existing = true;
        } else if (strcmp(argv[i], "--verify") == 0) {
            config->verify_output = true;
        }
    }
    
    if (!format) {
        fprintf(stderr, "Error: output format required (-f/--format)\n");
        return 1;
    }
    
    /* Create batch context */
    g_batch_ctx = uft_batch_create(config);
    if (!g_batch_ctx) {
        fprintf(stderr, "Error: Failed to create batch context\n");
        return 1;
    }
    
    /* Add jobs */
    printf("Scanning %s for %s...\n", input_dir, pattern);
    uint32_t count = uft_batch_add_directory(g_batch_ctx, input_dir, pattern,
                                              recursive, UFT_JOB_CONVERT);
    printf("Found %u files to convert to %s\n\n", count, format);
    
    if (count == 0) {
        uft_batch_destroy(g_batch_ctx);
        return 0;
    }
    
    /* Start processing */
    uft_batch_start(g_batch_ctx);
    uft_batch_wait(g_batch_ctx, 0);
    
    /* Print summary */
    printf("\n");
    uft_batch_print_summary(g_batch_ctx);
    
    uft_batch_destroy(g_batch_ctx);
    g_batch_ctx = NULL;
    
    return 0;
}

static int cmd_hash(int argc, char **argv, uft_batch_config_t *config)
{
    if (argc < 1) {
        fprintf(stderr, "Error: hash requires a directory\n");
        return 1;
    }
    
    const char *input_dir = argv[0];
    bool recursive = false;
    const char *pattern = "*";
    
    config->calculate_hashes = true;
    
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
            recursive = true;
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pattern") == 0) && i + 1 < argc) {
            pattern = argv[++i];
        }
    }
    
    /* Create batch context */
    g_batch_ctx = uft_batch_create(config);
    if (!g_batch_ctx) {
        fprintf(stderr, "Error: Failed to create batch context\n");
        return 1;
    }
    
    /* Add jobs */
    printf("Scanning %s for %s...\n", input_dir, pattern);
    uint32_t count = uft_batch_add_directory(g_batch_ctx, input_dir, pattern,
                                              recursive, UFT_JOB_HASH);
    printf("Found %u files to hash\n\n", count);
    
    if (count == 0) {
        uft_batch_destroy(g_batch_ctx);
        return 0;
    }
    
    /* Start processing */
    uft_batch_start(g_batch_ctx);
    uft_batch_wait(g_batch_ctx, 0);
    
    /* Print summary */
    printf("\n");
    uft_batch_print_summary(g_batch_ctx);
    
    /* Generate report */
    if (config->report_file[0]) {
        uft_batch_report_csv(g_batch_ctx, config->report_file);
        printf("Hash report saved to: %s\n", config->report_file);
    }
    
    uft_batch_destroy(g_batch_ctx);
    g_batch_ctx = NULL;
    
    return 0;
}

static int cmd_resume(int argc, char **argv, uft_batch_config_t *config)
{
    if (argc < 1) {
        fprintf(stderr, "Error: resume requires a state file\n");
        return 1;
    }
    
    const char *state_file = argv[0];
    
    if (!uft_batch_state_exists(state_file)) {
        fprintf(stderr, "Error: State file not found or invalid: %s\n", state_file);
        return 1;
    }
    
    printf("Resuming batch from: %s\n", state_file);
    
    g_batch_ctx = uft_batch_load_state(config, state_file);
    if (!g_batch_ctx) {
        fprintf(stderr, "Error: Failed to load batch state\n");
        return 1;
    }
    
    /* Get stats */
    uft_batch_stats_t stats;
    uft_batch_get_stats(g_batch_ctx, &stats);
    printf("Resuming: %u completed, %u pending\n\n", 
           stats.completed_jobs, stats.pending_jobs);
    
    /* Start processing */
    uft_batch_start(g_batch_ctx);
    uft_batch_wait(g_batch_ctx, 0);
    
    /* Print summary */
    printf("\n");
    uft_batch_print_summary(g_batch_ctx);
    
    uft_batch_destroy(g_batch_ctx);
    g_batch_ctx = NULL;
    
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
    
    /* Check for help */
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize config */
    uft_batch_config_t config;
    uft_batch_config_init(&config);
    
    config.progress_cb = progress_callback;
    config.complete_cb = complete_callback;
    config.error_cb = error_callback;
    
    /* Parse global options */
    int cmd_start = 2;
    for (int i = 2; i < argc; i++) {
        if ((strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--jobs") == 0) && i + 1 < argc) {
            config.num_workers = atoi(argv[++i]);
            cmd_start = i + 1;
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--state") == 0) && i + 1 < argc) {
            strncpy(config.state_file, argv[++i], UFT_BATCH_MAX_PATH - 1);
            config.save_state = true;
            cmd_start = i + 1;
        } else if (strcmp(argv[i], "--report") == 0 && i + 1 < argc) {
            strncpy(config.report_file, argv[++i], UFT_BATCH_MAX_PATH - 1);
            config.generate_report = true;
            cmd_start = i + 1;
        } else {
            break;
        }
    }
    
    /* Dispatch command */
    const char *cmd = argv[1];
    int remaining = argc - cmd_start;
    char **args = argv + cmd_start;
    
    int result;
    if (strcmp(cmd, "analyze") == 0) {
        result = cmd_analyze(remaining, args, &config);
    } else if (strcmp(cmd, "convert") == 0) {
        result = cmd_convert(remaining, args, &config);
    } else if (strcmp(cmd, "verify") == 0) {
        /* Verify uses same logic as analyze */
        result = cmd_analyze(remaining, args, &config);
    } else if (strcmp(cmd, "hash") == 0) {
        result = cmd_hash(remaining, args, &config);
    } else if (strcmp(cmd, "resume") == 0) {
        result = cmd_resume(remaining, args, &config);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        result = 1;
    }
    
    return result;
}
