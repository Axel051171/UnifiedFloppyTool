/**
 * @file uft_params.c
 * @brief Parameter schema implementation.
 */
#include "uft/uft_params.h"

static const char *const enum_bool[] = { "false", "true" };

/* Splice strategy for multi-pass reconstruction. */
static const char *const enum_splice_mode[] = { "vote", "best-crc", "hybrid" };

/* -------------------------------------------------------------------------- */
/* Recovery / decode parameter schema (GUI/CLI)                                */
/* -------------------------------------------------------------------------- */

static const uft_param_def_t params_recovery[] = {
    { "passes", "Multi-pass reads", UFT_PARAM_INT,
      "Number of read passes to perform (higher improves recovery, costs time).",
      "3", 1, 25, 1, NULL, 0 },
    { "offset_steps", "Offset steps", UFT_PARAM_INT,
      "Read-offset sweep steps per pass (helps with track misalignment / weak areas).",
      "3", 0, 20, 1, NULL, 0 },
    { "pll_bandwidth", "PLL bandwidth", UFT_PARAM_FLOAT,
      "Higher = more tolerant to RPM drift; lower = stricter timing (less jitter).",
      "0.25", 0.01, 2.0, 0.01, NULL, 0 },
    { "jitter_ns", "Jitter tolerance (ns)", UFT_PARAM_INT,
      "Expected flux jitter tolerance in nanoseconds. Used by bitcell classifier.",
      "150", 0, 2000, 10, NULL, 0 },
    { "vote_threshold", "Bit-vote threshold", UFT_PARAM_FLOAT,
      "Majority threshold for bit voting (0.5=majority, 0.67=stricter).",
      "0.55", 0.5, 0.95, 0.01, NULL, 0 },
    { "splice_mode", "Splice mode", UFT_PARAM_ENUM,
      "How to splice multiple passes: vote, best-crc, hybrid.",
      "vote", 0, 0, 0, enum_splice_mode, 3 },
    { "emit_map", "Emit detailed map", UFT_PARAM_BOOL,
      "If enabled, produce a detailed per-track/per-sector map for GUI visualization.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "emit_metrics", "Emit run metrics", UFT_PARAM_BOOL,
      "If enabled, produce a machine-readable metrics JSON (duration, recovered sectors, CRC stats).",
      "true", 0, 1, 1, enum_bool, 2 },

    { "emit_log", "Emit log file", UFT_PARAM_BOOL,
      "If enabled, write a human-readable log sidecar for troubleshooting and reproducibility.",
      "true", 0, 1, 1, enum_bool, 2 },
};



static const uft_param_def_t params_raw[] = {
    { "pad_missing", "Pad missing sectors", UFT_PARAM_BOOL,
      "If enabled, missing sectors are zero-filled to keep image geometry stable.",
      "true", 0, 1, 1, enum_bool, 2 },
    { "write_map", "Write map file", UFT_PARAM_BOOL,
      "If enabled, write a sidecar .map/.json with per-sector status for verification.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "write_profile", "Write profile JSON", UFT_PARAM_BOOL,
      "If enabled, write a profile JSON capturing all effective knobs (format/recovery/output) for reproducibility.",
      "true", 0, 1, 1, enum_bool, 2 },
};

static const uft_param_def_t params_adf[] = {
    { "validate_bootblock", "Validate bootblock", UFT_PARAM_BOOL,
      "If enabled, run extra sanity checks on Amiga bootblock structures.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "virus_scan", "Virus scan (bootblock)", UFT_PARAM_BOOL,
      "If enabled, write an extra Amiga virus scan sidecar (bootblock-focused).",
      "true", 0, 1, 1, enum_bool, 2 },

    { "virus_db", "Virus DB (optional)", UFT_PARAM_STRING,
      "Optional JSON signature DB for bootblock scanning. Supports entries with 'sha256' or 'pattern_hex' (with ?? wildcards) and optional 'mask_hex'. Leave empty to use heuristics only.",
      "", 0, 0, 0, NULL, 0 },
    { "write_map", "Write map file", UFT_PARAM_BOOL,
      "If enabled, write a sidecar status report.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "write_profile", "Write profile JSON", UFT_PARAM_BOOL,
      "If enabled, write a profile JSON capturing all effective knobs for reproducibility.",
      "true", 0, 1, 1, enum_bool, 2 },
};

static const char *const enum_g64_density[] = { "auto", "300rpm", "360rpm" };
static const uft_param_def_t params_g64[] = {
    { "density", "Drive speed", UFT_PARAM_ENUM,
      "Hint for timing normalization. 'auto' will derive speed from flux/bitstream statistics.",
      "auto", 0, 0, 0, enum_g64_density, 3 },
    { "write_map", "Write map file", UFT_PARAM_BOOL,
      "If enabled, write a sidecar status report.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "write_profile", "Write profile JSON", UFT_PARAM_BOOL,
      "If enabled, write a profile JSON capturing all effective knobs for reproducibility.",
      "true", 0, 1, 1, enum_bool, 2 },
};

static const uft_param_def_t params_woz[] = {
    { "version", "WOZ version", UFT_PARAM_ENUM,
      "WOZ container version.",
      "2", 0, 0, 0, (const char *const[]){ "1", "2" }, 2 },
    { "write_map", "Write map file", UFT_PARAM_BOOL,
      "If enabled, write a sidecar status report.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "write_profile", "Write profile JSON", UFT_PARAM_BOOL,
      "If enabled, write a profile JSON capturing all effective knobs for reproducibility.",
      "true", 0, 1, 1, enum_bool, 2 },
};

static const uft_param_def_t params_scp[] = {
    { "preserve_flux", "Preserve raw flux", UFT_PARAM_BOOL,
      "If enabled, export SCP with raw timing preserved as much as possible.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "write_profile", "Write profile JSON", UFT_PARAM_BOOL,
      "If enabled, write a profile JSON capturing all effective knobs for reproducibility.",
      "true", 0, 1, 1, enum_bool, 2 },
};

static const uft_param_def_t params_a2r[] = {
    { "write_map", "Write map file", UFT_PARAM_BOOL,
      "If enabled, write a sidecar status report.",
      "true", 0, 1, 1, enum_bool, 2 },

    { "write_profile", "Write profile JSON", UFT_PARAM_BOOL,
      "If enabled, write a profile JSON capturing all effective knobs for reproducibility.",
      "true", 0, 1, 1, enum_bool, 2 },
};

const uft_param_def_t *uft_output_param_defs(uft_output_format_t fmt, size_t *count)
{
    if (count) *count = 0;

    switch (fmt) {
    case UFT_OUTPUT_RAW_IMG:
    case UFT_OUTPUT_ATARI_ST:
        if (count) *count = sizeof(params_raw)/sizeof(params_raw[0]);
        return params_raw;
    case UFT_OUTPUT_AMIGA_ADF:
        if (count) *count = sizeof(params_adf)/sizeof(params_adf[0]);
        return params_adf;
    case UFT_OUTPUT_C64_G64:
        if (count) *count = sizeof(params_g64)/sizeof(params_g64[0]);
        return params_g64;
    case UFT_OUTPUT_APPLE_WOZ:
        if (count) *count = sizeof(params_woz)/sizeof(params_woz[0]);
        return params_woz;
    case UFT_OUTPUT_SCP:
        if (count) *count = sizeof(params_scp)/sizeof(params_scp[0]);
        return params_scp;
    case UFT_OUTPUT_A2R:
        if (count) *count = sizeof(params_a2r)/sizeof(params_a2r[0]);
        return params_a2r;
    default:
        return NULL;
    }
}


const uft_param_def_t *uft_recovery_param_defs(size_t *count)
{
    if (count) *count = sizeof(params_recovery)/sizeof(params_recovery[0]);
    return params_recovery;
}

/* For now, most classic formats are fixed-geometry. We expose a minimal schema
 * for formats where recovery often benefits from a knob or two. */
static const uft_param_def_t params_format_common[] = {
    { "rpm_hint", "RPM hint", UFT_PARAM_FLOAT,
      "Optional expected RPM for this disk (0=auto). Helps PLL in edge cases.",
      "0", 0, 400, 0.5, NULL, 0 },

    { "data_rate_hint", "Data rate hint (bps)", UFT_PARAM_INT,
      "Optional expected data rate in bits/s (0=auto/by-format). Helps decode on marginal media.",
      "0", 0, 2000000, 1000, NULL, 0 },

    { "sector_size_override", "Sector size override", UFT_PARAM_INT,
      "Force sector size in bytes (0=auto/by-format). Useful for damaged headers.",
      "0", 0, 8192, 128, NULL, 0 },

    { "track_skew_hint", "Track skew hint", UFT_PARAM_FLOAT,
      "Optional track-to-track phase skew hint (0=auto). Use small values to stabilize weak reads.",
      "0", -10, 10, 0.1, NULL, 0 },
};


/* -------------------------------------------------------------------------- */
/* Per-format parameter schemas                                                */
/* -------------------------------------------------------------------------- */

static const char *const enum_c64_drive[] = { "1541", "1571", "1581" };

static const uft_param_def_t params_format_pc_mfm[] = {
    { "rpm_hint", "RPM hint", UFT_PARAM_FLOAT,
      "Optional expected RPM for this disk (0=auto). Helps PLL in edge cases.",
      "0", 0, 400, 0.5, NULL, 0 },
    { "data_rate_hint", "Data rate hint (bps)", UFT_PARAM_INT,
      "Optional expected data rate in bits/s (0=auto/by-format). Helps decode on marginal media.",
      "0", 0, 2000000, 1000, NULL, 0 },
    { "sector_size_override", "Sector size override", UFT_PARAM_INT,
      "Force sector size in bytes (0=auto/by-format). Useful for damaged headers.",
      "0", 0, 8192, 128, NULL, 0 },
    { "track_skew_hint", "Track skew hint", UFT_PARAM_FLOAT,
      "Optional track-to-track phase skew hint (0=auto). Use small values to stabilize weak reads.",
      "0", -10, 10, 0.1, NULL, 0 },
    { "mfm_sync_tolerance", "MFM sync tolerance", UFT_PARAM_FLOAT,
      "Tolerance for MFM sync detection (higher tolerates more jitter, too high increases false positives).",
      "1.0", 0.5, 3.0, 0.1, NULL, 0 },
};

static const uft_param_def_t params_format_amiga[] = {
    { "rpm_hint", "RPM hint", UFT_PARAM_FLOAT,
      "Optional expected RPM for this disk (0=auto). Helps PLL in edge cases.",
      "0", 0, 400, 0.5, NULL, 0 },
    { "data_rate_hint", "Data rate hint (bps)", UFT_PARAM_INT,
      "Optional expected data rate in bits/s (0=auto/by-format). Helps decode on marginal media.",
      "0", 0, 2000000, 1000, NULL, 0 },
    { "track_skew_hint", "Track skew hint", UFT_PARAM_FLOAT,
      "Optional track-to-track phase skew hint (0=auto). Use small values to stabilize weak reads.",
      "0", -10, 10, 0.1, NULL, 0 },
    { "amiga_odd_even", "Amiga odd/even", UFT_PARAM_BOOL,
      "Expect Amiga odd/even longword interleaving when decoding raw track data.",
      "true", 0, 1, 1, enum_bool, 2 },
};

static const uft_param_def_t params_format_c64[] = {
    { "rpm_hint", "RPM hint", UFT_PARAM_FLOAT,
      "Optional expected RPM for this disk (0=auto). Helps PLL in edge cases.",
      "0", 0, 400, 0.5, NULL, 0 },
    { "track_skew_hint", "Track skew hint", UFT_PARAM_FLOAT,
      "Optional track-to-track phase skew hint (0=auto). Use small values to stabilize weak reads.",
      "0", -10, 10, 0.1, NULL, 0 },
    { "c64_drive", "Drive model", UFT_PARAM_ENUM,
      "Select the expected Commodore drive model to tune GCR timings.",
      "1541", 0, 0, 1, enum_c64_drive, 3 },
    { "gcr_tolerance", "GCR tolerance", UFT_PARAM_FLOAT,
      "Tolerance for GCR bitcell detection (higher tolerates jitter, too high increases false positives).",
      "1.0", 0.5, 3.0, 0.1, NULL, 0 },
};

static const uft_param_def_t params_format_apple2[] = {
    { "rpm_hint", "RPM hint", UFT_PARAM_FLOAT,
      "Optional expected RPM for this disk (0=auto). Helps PLL in edge cases.",
      "0", 0, 400, 0.5, NULL, 0 },
    { "track_skew_hint", "Track skew hint", UFT_PARAM_FLOAT,
      "Optional track-to-track phase skew hint (0=auto). Use small values to stabilize weak reads.",
      "0", -10, 10, 0.1, NULL, 0 },
    { "gcr_tolerance", "GCR tolerance", UFT_PARAM_FLOAT,
      "Tolerance for Apple II GCR bitcell detection (higher tolerates jitter).",
      "1.0", 0.5, 3.0, 0.1, NULL, 0 },
    { "apple2_phase_lock", "Phase lock", UFT_PARAM_BOOL,
      "Enable stricter phase locking during decode for weak media (may reduce false positives).",
      "true", 0, 1, 1, enum_bool, 2 },
};



const uft_param_def_t *uft_format_param_defs(uft_disk_format_id_t fmt, size_t *count)
{
    if (count) *count = 0;

    switch (fmt) {
    /* IBM PC / FAT12 (MFM) */
    case UFT_DISK_FORMAT_FAT12_160K:
    case UFT_DISK_FORMAT_FAT12_180K:
    case UFT_DISK_FORMAT_FAT12_320K:
    case UFT_DISK_FORMAT_PC_360K:
    case UFT_DISK_FORMAT_PC_720K:
    case UFT_DISK_FORMAT_PC_1200K:
    case UFT_DISK_FORMAT_PC_1440K:
    case UFT_DISK_FORMAT_PC_2880K:
    case UFT_DISK_FORMAT_ATARI_ST_720K:
    case UFT_DISK_FORMAT_ATARI_ST_1440K:
    case UFT_DISK_FORMAT_MAC_1440K:
        if (count) *count = sizeof(params_format_pc_mfm)/sizeof(params_format_pc_mfm[0]);
        return params_format_pc_mfm;

    /* Amiga (MFM but with odd/even layout) */
    case UFT_DISK_FORMAT_AMIGA_ADF_880K:
    case UFT_DISK_FORMAT_AMIGA_ADF_1760K:
        if (count) *count = sizeof(params_format_amiga)/sizeof(params_format_amiga[0]);
        return params_format_amiga;

    /* Commodore (GCR) */
    case UFT_DISK_FORMAT_C64_G64:
        if (count) *count = sizeof(params_format_c64)/sizeof(params_format_c64[0]);
        return params_format_c64;

    /* Apple II (GCR) */
    case UFT_DISK_FORMAT_APPLE2_DOS33:
        if (count) *count = sizeof(params_format_apple2)/sizeof(params_format_apple2[0]);
        return params_format_apple2;

    default:
        /* Safe fallback */
        if (count) *count = sizeof(params_format_common)/sizeof(params_format_common[0]);
        return params_format_common;
    }
}
