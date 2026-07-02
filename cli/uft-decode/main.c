/**
 * @file cli/uft-decode/main.c
 * @brief V415-PLAN EMUCI.real (MF-263) — headless decode CLI scaffold.
 *
 * Purpose: a Qt-free binary that wraps `uft_convert_file()` so the
 * Emulator-CI workflow (`.github/workflows/emulator.yml`) can do
 *   `uft-decode --in capture.scp --out disk.d64`
 * round-trip checks against VICE/WinUAE in a container. The existing
 * UFT app pulls in the Qt6 GUI stack which makes it unsuitable for
 * containerized CI.
 *
 * This scaffold establishes the shape (argv, exit codes, format
 * detection via uft_probe_format()). The full feature set (every
 * src→dst pair the GUI supports) is one focused follow-up session
 * because the core conversion path is already complete in
 * src/formats/uft_format_convert_dispatch.c — this binary just
 * exposes it.
 *
 * Exit codes:
 *   0 = success (file produced, no warnings beyond preflight notes)
 *   1 = invalid argv
 *   2 = source open / read error
 *   3 = format unknown or no conversion path
 *   4 = preflight ABORT (LOSSY without --accept-data-loss, etc.)
 *   5 = converter internal failure
 *
 * BUILD STATUS: scaffold only. The CMake/qmake wiring is NOT added
 * in this commit — see `cli/uft-decode/README.md` for the integration
 * checklist. Wiring is deferred to v4.1.6 because emulator.yml is
 * still a scaffold itself; adding the binary in advance would be
 * unused code (Master-Plan Regel 2).
 */
#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_error.h"
#include "uft/uft_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog)
{
    fprintf(stderr,
        "uft-decode " UFT_VERSION_STRING "  (V415-PLAN EMUCI scaffold)\n"
        "\n"
        "Usage: %s --in <source-file> --out <target-file>\n"
        "                --format <target-format-name>\n"
        "                [--accept-data-loss]\n"
        "                [--verify-after]\n"
        "\n"
        "Examples:\n"
        "  %s --in capture.scp --out disk.d64 --format D64\n"
        "  %s --in disk.adf --out disk.scp --format SCP --accept-data-loss\n"
        "\n"
        "Returns exit-code 0 on success, 1-5 on classified failure\n"
        "(see source-file header comment for taxonomy).\n",
        prog, prog, prog);
}

static uft_format_t parse_format_name(const char *name)
{
    for (uft_format_t f = (uft_format_t)0; f < UFT_FORMAT_MAX;
         f = (uft_format_t)(f + 1))
    {
        const char *fn = uft_format_get_name(f);
        if (fn && strcasecmp(fn, name) == 0) {
            return f;
        }
    }
    return UFT_FORMAT_UNKNOWN;
}

int main(int argc, char **argv)
{
    const char *src_path = NULL;
    const char *dst_path = NULL;
    const char *fmt_name = NULL;
    int  accept_data_loss = 0;
    int  verify_after     = 1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--in") && i + 1 < argc) {
            src_path = argv[++i];
        } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
            dst_path = argv[++i];
        } else if (!strcmp(argv[i], "--format") && i + 1 < argc) {
            fmt_name = argv[++i];
        } else if (!strcmp(argv[i], "--accept-data-loss")) {
            accept_data_loss = 1;
        } else if (!strcmp(argv[i], "--no-verify")) {
            verify_after = 0;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "uft-decode: unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (!src_path || !dst_path || !fmt_name) {
        usage(argv[0]);
        return 1;
    }

    uft_format_t target = parse_format_name(fmt_name);
    if (target == UFT_FORMAT_UNKNOWN) {
        fprintf(stderr, "uft-decode: unknown target format: %s\n", fmt_name);
        return 1;
    }

    uft_convert_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.verify_after       = verify_after ? true : false;
    /* UFT-A05: map the CLI --accept-data-loss flag to its OWN option
     * field. preserve_errors is now strictly about carrying error-flags
     * forward in the output, not about consent. */
    opts.accept_data_loss   = accept_data_loss ? true : false;
    opts.preserve_errors    = true;   /* carry error-flags forward by default in CLI */
    opts.preserve_weak_bits = true;

    uft_convert_result_t result;
    memset(&result, 0, sizeof(result));

    uft_error_t err = uft_convert_file(src_path, dst_path, target, &opts, &result);

    for (int i = 0; i < result.warning_count; i++) {
        fprintf(stderr, "[uft-decode] %s\n", result.warnings[i]);
    }

    if (err != UFT_OK) {
        fprintf(stderr, "uft-decode: convert failed (err=%d)\n", (int)err);
        return (err == UFT_ERR_FILE_NOT_FOUND) ? 2
             : (err == UFT_ERR_NOT_SUPPORTED) ? 3
             : 5;
    }

    if (!result.success) {
        fprintf(stderr, "uft-decode: convert reported no success despite UFT_OK\n");
        return 5;
    }

    return 0;
}
