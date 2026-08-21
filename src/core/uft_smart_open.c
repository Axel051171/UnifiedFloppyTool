/**
 * @file uft_smart_open.c
 * @brief UFT Smart Pipeline Implementation
 * 
 * "Bei uns geht kein Bit verloren"
 */

#include "uft/uft_smart_open.h"
#include "uft/uft_v3_bridge.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/uft_error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format identity
 *
 * MF-444: this file used to carry its own probe table — sixteen formats, each
 * with a hand-written `extern` for its probe function and a wrapper to
 * normalise the signature. The registry already holds 88 plugins, every one
 * with the probe its own maintainer wrote and keeps in step with the parser.
 * Detection now asks the registry, and the table is gone.
 *
 * Three consequences, all wanted:
 *   - 72 formats this function could not name are now recognised;
 *   - the probe list cannot drift out of step with the registry, because
 *     there is no second list;
 *   - the link surface collapsed from "sixteen parsers plus three v3
 *     handlers" to the registry, which is why this function finally has a
 *     test (tests/test_smart_open_quality.c). ARCH-6 predicted exactly that:
 *     parallel implementations are what made these entry points untestable.
 *
 * The FMT_* ids survive only where something switches on them — the
 * protection path, which is bound to the three v3 parsers.
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define FMT_UNKNOWN  0
#define FMT_D64     10
#define FMT_G64     20
#define FMT_SCP     30

/* v3 parsers are OPTIONAL and are registered by whoever wants them.
 *
 * MF-444: this file used to reference uft_d64_v3_handler & co. directly, so
 * every consumer of uft_smart_open() had to link the whole v3 chain — bridge,
 * three parsers, several thousand lines — whether it used protection detection
 * or not. That coupling is why the function had no test.
 *
 * Turning it around costs one registration call and buys two things: core no
 * longer depends on a subsystem whose future is undecided (ARCH-8), and a
 * caller that wants protection detection says so explicitly. */
static struct {
    int format_id;
    uft_format_handler_t* handler;
    uft_smart_protection_fn detect_protection;
} g_v3_handlers[4];
static size_t g_v3_handler_count = 0;

void uft_smart_register_v3_handler(int format_id, uft_format_handler_t* handler,
                                   uft_smart_protection_fn detect_protection) {
    if (!handler || format_id == FMT_UNKNOWN) return;
    for (size_t i = 0; i < g_v3_handler_count; i++) {
        if (g_v3_handlers[i].format_id == format_id) {
            g_v3_handlers[i].handler = handler;             /* replace */
            g_v3_handlers[i].detect_protection = detect_protection;
            return;
        }
    }
    if (g_v3_handler_count >= sizeof(g_v3_handlers) / sizeof(g_v3_handlers[0]))
        return;
    g_v3_handlers[g_v3_handler_count].format_id = format_id;
    g_v3_handlers[g_v3_handler_count].handler = handler;
    g_v3_handlers[g_v3_handler_count].detect_protection = detect_protection;
    g_v3_handler_count++;
}

static uft_format_handler_t* v3_handler_for(int format_id) {
    for (size_t i = 0; i < g_v3_handler_count; i++)
        if (g_v3_handlers[i].format_id == format_id)
            return g_v3_handlers[i].handler;
    return NULL;
}

static uft_smart_protection_fn v3_protection_for(int format_id) {
    for (size_t i = 0; i < g_v3_handler_count; i++)
        if (g_v3_handlers[i].format_id == format_id)
            return g_v3_handlers[i].detect_protection;
    return NULL;
}

/** Map a registry plugin to the id the protection path switches on. */
static int format_id_for(const uft_format_plugin_t* plugin) {
    if (!plugin || !plugin->name) return FMT_UNKNOWN;
    if (strcasecmp(plugin->name, "D64") == 0) return FMT_D64;
    if (strcasecmp(plugin->name, "G64") == 0) return FMT_G64;
    if (strcasecmp(plugin->name, "SCP") == 0) return FMT_SCP;
    return FMT_UNKNOWN;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal State
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t* data;
    size_t size;
    char path[1024];
    void* parser_handle;
    int format_id;
    bool is_v3;

    /* MF-444: the registry plugin that recognised this file. This is what
     * analyze_quality() reads the disk with, instead of guessing about it. */
    const uft_format_plugin_t* plugin;
} smart_internal_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_smart_options_init(uft_smart_options_t* opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(uft_smart_options_t));
    opts->use_bayesian_detect = true;
    opts->prefer_v3_parsers = true;
    opts->auto_detect_protection = true;
    opts->enable_multi_rev_fusion = true;
    opts->enable_crc_correction = true;
    opts->strict_mode = false;
    opts->min_confidence = 70;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Ask the registry which plugin owns this file, and how sure it is.
 *
 * uft_probe_buffer_format() returns the best-scoring plugin but not its score,
 * so the winner is probed once more for the number. That costs one extra probe
 * call and keeps the confidence honest — it is the plugin's own answer, not a
 * value this file made up (MF-444). */
static const uft_format_plugin_t* detect_format(const uint8_t* data, size_t size,
                                                size_t file_size, int* confidence) {
    if (confidence) *confidence = 0;

    const uft_format_plugin_t* plugin =
        uft_probe_buffer_format(data, size, file_size);
    if (!plugin) return NULL;

    if (confidence && plugin->probe) {
        int conf = 0;
        if (plugin->probe(data, size, file_size, &conf)) *confidence = conf;
    }
    return plugin;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protection Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void detect_protection(smart_internal_t* internal,
                              uft_protection_result_t* prot) {
    memset(prot, 0, sizeof(uft_protection_result_t));

    if (!internal->is_v3 || !internal->parser_handle) return;

    uft_smart_protection_fn detect = v3_protection_for(internal->format_id);
    if (!detect) return;

    char name[64] = {0};
    float conf = 0.0f;
    bool detected = detect(internal->parser_handle, name, sizeof(name), &conf);

    if (detected) {
        switch (internal->format_id) {
            case FMT_D64:
            case FMT_G64:
                strncpy(prot->platform, "Commodore 64", sizeof(prot->platform) - 1);
                break;
            case FMT_SCP:
                strncpy(prot->platform, "Multi-Platform", sizeof(prot->platform) - 1);
                break;
            default: break;
        }
    }

    if (detected) {
        prot->detected = true;
        strncpy(prot->scheme_name, name, sizeof(prot->scheme_name) - 1);
        /* MF-442: the parser reports a per-scheme confidence (0.85 for C64
         * weak bits, 0.80 for Amiga long tracks, 0.75 generic). Those are
         * hand-assigned constants, NOT measurements — worth saying plainly,
         * because this file previously wrote `prot->confidence = 80`, one
         * global constant standing in for a value the parser had just produced
         * and a three-parameter call to a four-parameter function had thrown
         * away. Distinguishing schemes is an improvement over not doing so;
         * it is not yet a measured confidence. */
        int pct = (int)(conf * 100.0f + 0.5f);
        prot->confidence = (pct < 0) ? 0 : (pct > 100 ? 100 : pct);
        prot->indicator_count = 1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Quality Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void analyze_quality(smart_internal_t* internal,
                           uft_quality_result_t* quality,
                           const uft_smart_options_t* opts) {
    memset(quality, 0, sizeof(uft_quality_result_t));

    /* MF-443: nothing is claimed until something is measured.
     *
     * This used to open with `level = GOOD; readable_sectors = 100;
     * total_sectors = 100;` — hard-coded, before a single sector was read —
     * and uft_smart_report() printed it as "Sectors: 100 / 100 readable,
     * Quality: Good". For any disk. A report that looks like a measurement and
     * is not one is worse than no report: it goes into a preservation record,
     * where nobody can tell it apart from the real thing. */
    quality->level              = UFT_QUALITY_NOT_DETERMINED;
    quality->readable_sectors   = -1;
    quality->total_sectors      = -1;
    quality->crc_errors         = -1;
    quality->crc_corrected      = -1;
    quality->weak_bits_found    = -1;
    quality->weak_bits_resolved = -1;
    quality->bit_error_rate     = -1.0;

    /* MF-444: count what is actually there.
     *
     * Every sector of every track is read through the format plugin and
     * classified. That is the only honest source for these numbers — the
     * previous body asserted 100/100 without opening the disk.
     *
     * Sectors whose CRC did not validate are counted as errors and NOT as
     * readable: a sector that decoded to bytes but failed its checksum is a
     * damaged sector, and reporting it as readable is the same class of
     * mistake as inventing the count outright. */
    if (internal->plugin && internal->plugin->open && internal->plugin->read_track) {
        uft_disk_t disk;
        memset(&disk, 0, sizeof(disk));
        disk.read_only = true;

        if (internal->plugin->open(&disk, internal->path, true) == UFT_OK) {
            int cyls  = disk.geometry.cylinders;
            int heads = disk.geometry.heads > 0 ? disk.geometry.heads : 1;

            if (cyls > 0) {
                int total = 0, good = 0, bad = 0, weak = 0;

                for (int cyl = 0; cyl < cyls; cyl++) {
                    for (int hd = 0; hd < heads; hd++) {
                        uft_track_t track;
                        memset(&track, 0, sizeof(track));
                        if (internal->plugin->read_track(&disk, cyl, hd, &track) != UFT_OK) {
                            uft_track_release(&track);
                            continue;
                        }
                        for (size_t s = 0; s < track.sector_count; s++) {
                            const uft_sector_t* sec = &track.sectors[s];
                            total++;
                            if (sec->crc_ok) good++; else bad++;
                            if (sec->weak || sec->weak_mask) weak++;
                        }
                        uft_track_release(&track);
                    }
                }

                if (total > 0) {
                    quality->readable_sectors = good;
                    quality->total_sectors    = total;
                    quality->crc_errors       = bad;
                    quality->weak_bits_found  = weak;
                    quality->bit_error_rate   = (double)bad / (double)total;

                    /* The level follows from the ratio, so it cannot drift
                     * away from the numbers printed beside it. */
                    int pct = (good * 100) / total;
                    if      (pct == 100) quality->level = UFT_QUALITY_PERFECT;
                    else if (pct >=  95) quality->level = UFT_QUALITY_EXCELLENT;
                    else if (pct >=  80) quality->level = UFT_QUALITY_GOOD;
                    else if (pct >=  50) quality->level = UFT_QUALITY_FAIR;
                    else if (pct >    0) quality->level = UFT_QUALITY_POOR;
                    else                 quality->level = UFT_QUALITY_UNREADABLE;
                }
                /* total == 0: the plugin opened the file but yielded no
                 * sectors. Everything stays NOT_DETERMINED — "we read nothing"
                 * is not "the disk is empty". */
            }
            if (internal->plugin->close) internal->plugin->close(&disk);
        }
    }

    /* crc_corrected and weak_bits_resolved stay NOT_DETERMINED: neither CRC
     * correction nor multi-revolution fusion runs on this path yet. Reporting
     * 0 would claim that nothing needed correcting. */

    /* MF-444: there was a `if (opts->enable_god_mode)` branch here that called
     * uft_calculate_metrics() and then OVERWROTE the counted crc_errors, the
     * bit error rate and the quality level with what came back. What came back
     * was:
     *
     *     metrics->signal_quality = 0.9;   metrics->bit_error_rate = 0.001;
     *     metrics->sync_quality   = 0.95;  metrics->timing_jitter  = 50.0;
     *     (void)track_len; (void)encoding;
     *
     * — four constants, the disk never looked at. Enabling the option would
     * therefore have replaced a real measurement with an invented one, then set
     * god_mode_used so the report printed "God-Mode: Used". The fabricating
     * function is gone with it (uft_god_mode_api.c), so nobody can call it
     * again by accident. Real signal metrics live in the OTDR pipeline; when
     * this path grows them, they get counted here like the sectors are.
     */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main API
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_smart_open(const char* path, const uft_smart_options_t* opts,
                   uft_smart_result_t* result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(uft_smart_result_t));
    
    uft_smart_options_t default_opts;
    if (!opts) {
        uft_smart_options_init(&default_opts);
        opts = &default_opts;
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(0, "Opening file...", opts->user_data);
    }
    
    /* Open and read file */
    FILE* f = fopen(path, "rb");
    if (!f) {
        snprintf(result->error, sizeof(result->error), "Cannot open file: %s", path);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    size_t header_size = (file_size > 65536) ? 65536 : file_size;
    uint8_t* header = malloc(header_size);
    if (!header) {
        fclose(f);
        snprintf(result->error, sizeof(result->error), "Out of memory");
        return -1;
    }
    
    if (fread(header, 1, header_size, f) != header_size) {
        free(header);
        fclose(f);
        snprintf(result->error, sizeof(result->error), "Read error");
        return -1;
    }
    fclose(f);
    
    if (opts->progress_cb) {
        opts->progress_cb(20, "Detecting format...", opts->user_data);
    }
    
    /* Detect format.
     *
     * MF-444: one detection source, the registry. This used to run a private
     * table of 16 formats with hand-written probe externs, plus an extension
     * fallback that assigned a confidence of 50 to a guess made from the file
     * name. Both are gone: uft_probe_buffer_format() walks all 88 registered
     * plugins and each reports its own confidence.
     *
     * Dropping the extension fallback is deliberate. "The name ends in .d64,
     * so call it 50 % sure" is a claim about the content derived from
     * something that is not the content. If no plugin recognises the bytes,
     * saying so is the honest answer. */
    int confidence = 0;
    const uft_format_plugin_t* plugin =
        detect_format(header, header_size, file_size, &confidence);

    if (!plugin) {
        /* Two different states, and they must not share one message. An empty
         * registry means nobody registered plugins in this process — the file
         * was never examined. Reporting "Unknown format" for that would blame
         * the disk for a caller's omission, in the same voice used for a file
         * that really was examined and not recognised. */
        free(header);
        if (uft_registered_format_plugin_count() == 0) {
            snprintf(result->error, sizeof(result->error),
                     "No format plugins registered - call "
                     "uft_register_all_formats() before uft_smart_open(); "
                     "the file was not examined");
        } else {
            snprintf(result->error, sizeof(result->error),
                     "Unknown format: no registered plugin recognised the "
                     "contents of %s", path);
        }
        return -1;
    }

    int format_id = format_id_for(plugin);
    uft_format_handler_t* v3_handler = v3_handler_for(format_id);

    result->detection.format_id   = format_id;
    result->detection.format_name = plugin->name;
    result->detection.confidence  = confidence;

    if (opts->progress_cb) {
        opts->progress_cb(40, "Parsing disk image...", opts->user_data);
    }

    /* Create internal state */
    smart_internal_t* internal = calloc(1, sizeof(smart_internal_t));
    if (!internal) {
        free(header);
        snprintf(result->error, sizeof(result->error), "Out of memory");
        return -1;
    }

    internal->data = header;
    internal->size = header_size;
    internal->plugin = plugin;       /* MF-444: what analyze_quality() reads with */
    strncpy(internal->path, path, sizeof(internal->path) - 1);
    internal->format_id = format_id;

    /* Open with v3 parser if available */
    if (opts->prefer_v3_parsers && v3_handler && v3_handler->open) {
        if (v3_handler->open(path, &internal->parser_handle) == UFT_OK) {
            internal->is_v3 = true;
            result->detection.using_v3_parser = true;
        }
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(60, "Analyzing protection...", opts->user_data);
    }
    
    /* Protection detection */
    if (opts->auto_detect_protection) {
        detect_protection(internal, &result->protection);
        
        if (result->protection.detected) {
            snprintf(result->warnings + strlen(result->warnings),
                    sizeof(result->warnings) - strlen(result->warnings),
                    "Protection detected: %s (%s)\n",
                    result->protection.scheme_name,
                    result->protection.platform);
        }
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(80, "Analyzing quality...", opts->user_data);
    }
    
    /* Quality analysis */
    analyze_quality(internal, &result->quality, opts);
    
    if (result->quality.level < UFT_QUALITY_GOOD) {
        snprintf(result->warnings + strlen(result->warnings),
                sizeof(result->warnings) - strlen(result->warnings),
                "Quality: %s\n", uft_quality_level_name(result->quality.level));
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(100, "Done", opts->user_data);
    }
    
    result->handle = internal;
    return 0;
}

void uft_smart_close(uft_smart_result_t* result) {
    if (!result || !result->handle) return;
    
    smart_internal_t* internal = (smart_internal_t*)result->handle;
    
    if (internal->is_v3 && internal->parser_handle) {
        /* MF-444: the handler comes from the format id, not from a table walk. */
        uft_format_handler_t* h = v3_handler_for(internal->format_id);
        if (h && h->close) h->close(internal->parser_handle);
    }
    
    free(internal->data);
    free(internal);
    result->handle = NULL;
}

int uft_smart_reanalyze(uft_smart_result_t* result, const uft_smart_options_t* opts) {
    if (!result || !result->handle) return -1;
    
    smart_internal_t* internal = (smart_internal_t*)result->handle;
    
    /* MF-444: no table to look the format up in any more. Re-analysis works
     * from the plugin the first open already resolved. */
    if (!internal->plugin) return -1;

    if (opts->auto_detect_protection) {
        detect_protection(internal, &result->protection);
    }
    
    analyze_quality(internal, &result->quality, opts);
    
    return 0;
}

const char* uft_quality_level_name(uft_quality_level_t level) {
    switch (level) {
        case UFT_QUALITY_PERFECT:    return "Perfect";
        case UFT_QUALITY_EXCELLENT:  return "Excellent";
        case UFT_QUALITY_GOOD:       return "Good";
        case UFT_QUALITY_FAIR:       return "Fair";
        case UFT_QUALITY_POOR:       return "Poor";
        case UFT_QUALITY_UNREADABLE: return "Unreadable";
        case UFT_QUALITY_NOT_DETERMINED: return "not determined";
        default:                     return "not determined";
    }
}

/* Render one count, or say that it was not measured (MF-443). */
static const char* fmt_count(int value, char* buf, size_t n) {
    if (value < 0) return "not determined";
    snprintf(buf, n, "%d", value);
    return buf;
}

char* uft_smart_report(const uft_smart_result_t* result) {
    if (!result) return NULL;

    char* report = malloc(4096);
    if (!report) return NULL;

    char b_read[24], b_total[24], b_crc[24], b_corr[24], b_weak[24], b_wres[24];
    const char* s_read  = fmt_count(result->quality.readable_sectors, b_read,  sizeof(b_read));
    const char* s_total = fmt_count(result->quality.total_sectors,    b_total, sizeof(b_total));
    const char* s_crc   = fmt_count(result->quality.crc_errors,       b_crc,   sizeof(b_crc));
    const char* s_corr  = fmt_count(result->quality.crc_corrected,    b_corr,  sizeof(b_corr));
    const char* s_weak  = fmt_count(result->quality.weak_bits_found,  b_weak,  sizeof(b_weak));
    const char* s_wres  = fmt_count(result->quality.weak_bits_resolved, b_wres, sizeof(b_wres));

    snprintf(report, 4096,
        "═══════════════════════════════════════════════════════════════\n"
        "                    UFT Smart Open Report\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        "FORMAT DETECTION\n"
        "  Format:      %s\n"
        "  Confidence:  %d%%\n"
        "  v3 Parser:   %s\n\n"
        "PROTECTION ANALYSIS\n"
        "  Detected:    %s\n"
        "%s%s%s"
        "\nQUALITY ASSESSMENT\n"
        "  Level:       %s\n"
        "  Sectors:     %s / %s readable\n"
        "  CRC Errors:  %s (corrected: %s)\n"
        "  Weak Bits:   %s (resolved: %s)\n"
        "  God-Mode:    %s\n"
        "%s%s"
        "═══════════════════════════════════════════════════════════════\n",
        result->detection.format_name ? result->detection.format_name : "Unknown",
        result->detection.confidence,
        result->detection.using_v3_parser ? "Yes" : "No",
        result->protection.detected ? "Yes" : "No",
        result->protection.detected ? "  Scheme:      " : "",
        result->protection.detected ? result->protection.scheme_name : "",
        result->protection.detected ? "\n" : "",
        uft_quality_level_name(result->quality.level),
        s_read, s_total, s_crc, s_corr, s_weak, s_wres,
        result->warnings[0] ? "\nWARNINGS\n" : "",
        result->warnings[0] ? result->warnings : "");
    
    return report;
}

