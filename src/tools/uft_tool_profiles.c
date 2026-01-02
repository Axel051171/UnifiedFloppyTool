/**
 * @file uft_tool_profiles.c
 * @brief Complete Tool Capability Profiles
 */

#include "uft/uft_tool_capabilities.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Format Bitmasks
// ============================================================================

#define FMT_SCP         (1 << UFT_FORMAT_SCP)
#define FMT_HFE         (1 << UFT_FORMAT_HFE)
#define FMT_KRYOFLUX    (1 << UFT_FORMAT_KRYOFLUX)
#define FMT_A2R         (1 << UFT_FORMAT_A2R)
#define FMT_WOZ         (1 << UFT_FORMAT_WOZ)
#define FMT_G64         (1 << UFT_FORMAT_G64)
#define FMT_NIB         (1 << UFT_FORMAT_NIB)
#define FMT_D64         (1 << UFT_FORMAT_D64)
#define FMT_ADF         (1 << UFT_FORMAT_ADF)
#define FMT_IMG         (1 << UFT_FORMAT_IMG)
#define FMT_DSK         (1 << UFT_FORMAT_DSK)
#define FMT_IPF         (1 << UFT_FORMAT_IPF)
#define FMT_STX         (1 << UFT_FORMAT_STX)
#define FMT_IMD         (1 << UFT_FORMAT_IMD)
#define FMT_TD0         (1 << UFT_FORMAT_TD0)

// ============================================================================
// Greaseweazle Options
// ============================================================================

static const uft_tool_option_t gw_options[] = {
    {
        .name = "--revs", .long_name = "revolutions",
        .description = "Number of revolutions to capture",
        .type = OPT_INT,
        .int_range = { .min = 1, .max = 20, .def = 3 },
        .category = "capture"
    },
    {
        .name = "--tracks", .long_name = "tracks",
        .description = "Track range (e.g., 0:79)",
        .type = OPT_STRING,
        .category = "capture"
    },
    {
        .name = "--retries", .long_name = "retries",
        .description = "Number of read retries",
        .type = OPT_INT,
        .int_range = { .min = 0, .max = 20, .def = 3 },
        .category = "capture"
    },
    {
        .name = "-d", .long_name = "device",
        .description = "Device path or serial port",
        .type = OPT_STRING,
        .required = true,
        .category = "device"
    },
    {
        .name = "--densel", .long_name = "densel",
        .description = "Force density select line",
        .type = OPT_FLAG,
        .category = "advanced"
    },
    {
        .name = "--rate", .long_name = "sample_rate",
        .description = "Sample rate in MHz",
        .type = OPT_INT,
        .int_range = { .min = 1, .max = 100, .def = 0 },
        .category = "advanced"
    },
};

const uft_tool_profile_t UFT_TOOL_GREASEWEAZLE = {
    .name = "Greaseweazle",
    .version = "1.x",
    .description = "Open-source USB floppy adapter with flux capture",
    .homepage = "https://github.com/keirf/greaseweazle",
    .executable = "gw",
    
    .io_caps = {
        .input = {
            .from_hardware = true,
            .from_flux = true,
            .from_bitstream = true,
            .from_sector = true,
            .flux_formats = FMT_SCP | FMT_HFE | FMT_KRYOFLUX,
            .bitstream_formats = FMT_HFE | FMT_G64,
            .sector_formats = FMT_IMG | FMT_ADF | FMT_D64,
        },
        .output = {
            .to_hardware = true,
            .to_flux = true,
            .to_bitstream = true,
            .to_sector = true,
            .flux_formats = FMT_SCP | FMT_HFE | FMT_KRYOFLUX,
            .bitstream_formats = FMT_HFE,
            .sector_formats = FMT_IMG,
        },
        .processing = {
            .can_convert = true,
            .can_analyze = true,
            .can_verify = true,
        },
    },
    
    .limits = {
        .max_cylinders = 85,
        .max_heads = 2,
        .max_revolutions = 20,
        .max_track_size = 200000,
        .requires_usb = true,
        .supported_hardware = { "Greaseweazle F7", "Greaseweazle F7 Plus", NULL },
    },
    
    .platforms = {
        { .os = "linux", .arch = "all" },
        { .os = "windows", .arch = "all" },
        { .os = "macos", .arch = "all" },
    },
    .platform_count = 3,
    
    .options = gw_options,
    .option_count = sizeof(gw_options) / sizeof(gw_options[0]),
    
    .read_pattern = "gw read --revs={revolutions} --tracks={tracks} -d {device} {output}",
    .write_pattern = "gw write --tracks={tracks} -d {device} {input}",
    .convert_pattern = "gw convert {input} {output}",
    .analyze_pattern = "gw info {input}",
};

// ============================================================================
// FluxEngine Options
// ============================================================================

static const uft_tool_option_t fe_options[] = {
    {
        .name = "--cylinders", .long_name = "cylinders",
        .description = "Cylinder range",
        .type = OPT_STRING,
        .category = "capture"
    },
    {
        .name = "--heads", .long_name = "heads",
        .description = "Head selection",
        .type = OPT_STRING,
        .category = "capture"
    },
    {
        .name = "-s", .long_name = "source",
        .description = "Source specification",
        .type = OPT_STRING,
        .category = "input"
    },
    {
        .name = "-d", .long_name = "dest",
        .description = "Destination specification",
        .type = OPT_STRING,
        .category = "output"
    },
};

const uft_tool_profile_t UFT_TOOL_FLUXENGINE = {
    .name = "FluxEngine",
    .version = "0.x",
    .description = "Profile-based floppy disk reader/writer",
    .homepage = "http://cowlark.com/fluxengine/",
    .executable = "fluxengine",
    
    .io_caps = {
        .input = {
            .from_hardware = true,
            .from_flux = true,
            .from_bitstream = true,
            .flux_formats = FMT_SCP | FMT_KRYOFLUX | FMT_A2R,
            .bitstream_formats = FMT_HFE,
        },
        .output = {
            .to_hardware = true,
            .to_flux = true,
            .to_bitstream = true,
            .to_sector = true,
            .flux_formats = FMT_SCP | FMT_KRYOFLUX,
            .bitstream_formats = FMT_HFE,
            .sector_formats = FMT_IMG | FMT_ADF | FMT_D64,
        },
        .processing = {
            .can_convert = true,
            .can_analyze = true,
        },
    },
    
    .limits = {
        .max_cylinders = 85,
        .max_heads = 2,
        .requires_usb = true,
        .supported_hardware = { "FluxEngine", "Greaseweazle", NULL },
        .format_notes = "Uses profile system for format-specific handling",
    },
    
    .platforms = {
        { .os = "linux", .arch = "all" },
        { .os = "windows", .arch = "all" },
        { .os = "macos", .arch = "all" },
    },
    .platform_count = 3,
    
    .options = fe_options,
    .option_count = sizeof(fe_options) / sizeof(fe_options[0]),
    
    .read_pattern = "fluxengine read {profile} -s {device} -d {output}",
    .write_pattern = "fluxengine write {profile} -s {input} -d {device}",
};

// ============================================================================
// Kryoflux Options
// ============================================================================

static const uft_tool_option_t kf_options[] = {
    {
        .name = "-i", .long_name = "image_type",
        .description = "Output image type (0=stream, 4=SCP)",
        .type = OPT_INT,
        .int_range = { .min = 0, .max = 20, .def = 0 },
        .category = "output"
    },
    {
        .name = "-s", .long_name = "start_track",
        .description = "Start track",
        .type = OPT_INT,
        .int_range = { .min = 0, .max = 85, .def = 0 },
        .category = "capture"
    },
    {
        .name = "-e", .long_name = "end_track",
        .description = "End track",
        .type = OPT_INT,
        .int_range = { .min = 0, .max = 85, .def = 83 },
        .category = "capture"
    },
    {
        .name = "-g", .long_name = "side_mode",
        .description = "Side mode (0=both, 1=side0, 2=side1)",
        .type = OPT_INT,
        .int_range = { .min = 0, .max = 2, .def = 0 },
        .category = "capture"
    },
};

const uft_tool_profile_t UFT_TOOL_KRYOFLUX = {
    .name = "Kryoflux DTC",
    .version = "3.x",
    .description = "Commercial high-quality flux capture",
    .homepage = "https://www.kryoflux.com/",
    .executable = "dtc",
    
    .io_caps = {
        .input = {
            .from_hardware = true,
            .from_flux = true,
            .flux_formats = FMT_KRYOFLUX,
        },
        .output = {
            .to_flux = true,
            .to_sector = true,
            .flux_formats = FMT_KRYOFLUX | FMT_SCP,
            .sector_formats = FMT_IMG | FMT_ADF | FMT_D64 | FMT_G64,
        },
        .processing = {
            .can_convert = true,
            .can_analyze = true,
        },
    },
    
    .limits = {
        .max_cylinders = 86,
        .max_heads = 2,
        .requires_usb = true,
        .supported_hardware = { "Kryoflux", NULL },
        .format_notes = "Proprietary hardware, excellent preservation quality",
    },
    
    .platforms = {
        { .os = "linux", .arch = "x86_64" },
        { .os = "windows", .arch = "all" },
        { .os = "macos", .arch = "x86_64" },
    },
    .platform_count = 3,
    
    .options = kf_options,
    .option_count = sizeof(kf_options) / sizeof(kf_options[0]),
    
    .read_pattern = "dtc -f{output} -i{image_type} -s{start} -e{end} -g{sides}",
};

// ============================================================================
// nibtools Options
// ============================================================================

static const uft_tool_option_t nib_options[] = {
    {
        .name = "-D", .long_name = "device",
        .description = "Drive number (8-11)",
        .type = OPT_INT,
        .int_range = { .min = 8, .max = 11, .def = 8 },
        .category = "device"
    },
    {
        .name = "-S", .long_name = "start_track",
        .description = "Start track",
        .type = OPT_INT,
        .int_range = { .min = 1, .max = 42, .def = 1 },
        .category = "capture"
    },
    {
        .name = "-E", .long_name = "end_track",
        .description = "End track",
        .type = OPT_INT,
        .int_range = { .min = 1, .max = 42, .def = 35 },
        .category = "capture"
    },
    {
        .name = "-h", .long_name = "halftracks",
        .description = "Use half-tracks",
        .type = OPT_FLAG,
        .category = "capture"
    },
    {
        .name = "-V", .long_name = "verify",
        .description = "Verify after write",
        .type = OPT_FLAG,
        .category = "write"
    },
    {
        .name = "-P", .long_name = "parallel",
        .description = "Use parallel transfer",
        .type = OPT_FLAG,
        .category = "advanced"
    },
};

const uft_tool_profile_t UFT_TOOL_NIBTOOLS = {
    .name = "nibtools",
    .version = "0.8.x",
    .description = "Commodore disk imaging via XUM1541/ZoomFloppy",
    .homepage = "https://c64preservation.com/dp.php?pg=nibtools",
    .executable = "nibread",
    
    .io_caps = {
        .input = {
            .from_hardware = true,      // Via XUM1541
            .from_bitstream = true,     // G64, NIB
            .from_sector = true,        // D64
            .bitstream_formats = FMT_G64 | FMT_NIB,
            .sector_formats = FMT_D64,
        },
        .output = {
            .to_hardware = true,
            .to_bitstream = true,
            .to_sector = true,
            .bitstream_formats = FMT_G64 | FMT_NIB,
            .sector_formats = FMT_D64,
        },
        .processing = {
            .can_convert = true,
            .can_verify = true,
            .can_format = true,
        },
    },
    
    .limits = {
        .max_cylinders = 42,        // Including extended tracks
        .max_heads = 1,             // Commodore is single-sided
        .max_track_size = 8192,
        .requires_usb = true,
        .supported_hardware = { "XUM1541", "ZoomFloppy", "xu1541", NULL },
        .format_notes = "CBM-specific: 1541, 1571, 1581. Half-track support.",
    },
    
    .platforms = {
        { .os = "linux", .arch = "all" },
        { .os = "windows", .arch = "all" },
    },
    .platform_count = 2,
    
    .options = nib_options,
    .option_count = sizeof(nib_options) / sizeof(nib_options[0]),
    
    .read_pattern = "nibread -D{device} -S{start} -E{end} {output}",
    .write_pattern = "nibwrite -D{device} -S{start} -E{end} {input}",
    .convert_pattern = "nibconv {input} {output}",
};

// ============================================================================
// HxCFE Options
// ============================================================================

static const uft_tool_option_t hxc_options[] = {
    {
        .name = "-finput:", .long_name = "input_file",
        .description = "Input file path",
        .type = OPT_STRING,
        .required = true,
        .category = "input"
    },
    {
        .name = "-foutput:", .long_name = "output_file",
        .description = "Output file path",
        .type = OPT_STRING,
        .category = "output"
    },
    {
        .name = "-conv:", .long_name = "output_format",
        .description = "Output format name",
        .type = OPT_STRING,
        .category = "output"
    },
    {
        .name = "-infos", .long_name = "info",
        .description = "Show disk information",
        .type = OPT_FLAG,
        .category = "analyze"
    },
};

const uft_tool_profile_t UFT_TOOL_HXCFE = {
    .name = "HxCFloppyEmulator",
    .version = "2.x",
    .description = "Universal floppy format converter",
    .homepage = "https://hxc2001.com/",
    .executable = "hxcfe",
    
    .io_caps = {
        .input = {
            .from_flux = true,
            .from_bitstream = true,
            .from_sector = true,
            .flux_formats = FMT_SCP | FMT_KRYOFLUX | FMT_A2R,
            .bitstream_formats = FMT_HFE | FMT_G64 | FMT_WOZ | FMT_NIB,
            .sector_formats = FMT_D64 | FMT_ADF | FMT_IMG | FMT_DSK | FMT_STX | FMT_IPF | FMT_IMD | FMT_TD0,
        },
        .output = {
            .to_bitstream = true,
            .to_sector = true,
            .bitstream_formats = FMT_HFE | FMT_G64,
            .sector_formats = FMT_D64 | FMT_ADF | FMT_IMG,
        },
        .processing = {
            .can_convert = true,
            .can_analyze = true,
        },
    },
    
    .limits = {
        .max_cylinders = 255,
        .max_heads = 2,
        .format_notes = "Supports 50+ formats. Best universal converter.",
    },
    
    .platforms = {
        { .os = "linux", .arch = "all" },
        { .os = "windows", .arch = "all" },
        { .os = "macos", .arch = "all" },
    },
    .platform_count = 3,
    
    .options = hxc_options,
    .option_count = sizeof(hxc_options) / sizeof(hxc_options[0]),
    
    .convert_pattern = "hxcfe -finput:{input} -foutput:{output} -conv:{format}",
    .analyze_pattern = "hxcfe -finput:{input} -infos",
};

// ============================================================================
// disk-analyse Options
// ============================================================================

static const uft_tool_option_t da_options[] = {
    {
        .name = "-f", .long_name = "format",
        .description = "Force format type",
        .type = OPT_STRING,
        .category = "format"
    },
    {
        .name = "-t", .long_name = "track",
        .description = "Single track (cyl.head)",
        .type = OPT_STRING,
        .category = "capture"
    },
    {
        .name = "-v", .long_name = "verbose",
        .description = "Verbose output",
        .type = OPT_FLAG,
        .category = "output"
    },
};

const uft_tool_profile_t UFT_TOOL_DISK_ANALYSE = {
    .name = "disk-analyse",
    .version = "Keir Fraser",
    .description = "Flux analysis and format detection tool",
    .homepage = "https://github.com/keirf/disk-utilities",
    .executable = "disk-analyse",
    
    .io_caps = {
        .input = {
            .from_flux = true,
            .from_bitstream = true,
            .from_sector = true,
            .flux_formats = FMT_SCP | FMT_KRYOFLUX,
            .bitstream_formats = FMT_HFE | FMT_G64,
            .sector_formats = FMT_ADF | FMT_IMG,
        },
        .output = {
            .to_flux = true,
            .to_sector = true,
            .flux_formats = FMT_SCP,
            .sector_formats = FMT_ADF | FMT_IMG,
        },
        .processing = {
            .can_convert = true,
            .can_analyze = true,
        },
    },
    
    .limits = {
        .max_cylinders = 85,
        .max_heads = 2,
        .format_notes = "Excellent for Amiga analysis. Auto-detection.",
    },
    
    .platforms = {
        { .os = "linux", .arch = "all" },
        { .os = "macos", .arch = "all" },
    },
    .platform_count = 2,
    
    .options = da_options,
    .option_count = sizeof(da_options) / sizeof(da_options[0]),
    
    .convert_pattern = "disk-analyse -f {format} {input} {output}",
    .analyze_pattern = "disk-analyse -v {input}",
};

// ============================================================================
// adftools/unadf Options
// ============================================================================

static const uft_tool_option_t adf_options[] = {
    {
        .name = "-l", .long_name = "list",
        .description = "List files",
        .type = OPT_FLAG,
        .category = "analyze"
    },
    {
        .name = "-d", .long_name = "directory",
        .description = "Extract to directory",
        .type = OPT_STRING,
        .category = "extract"
    },
    {
        .name = "-c", .long_name = "check",
        .description = "Check disk integrity",
        .type = OPT_FLAG,
        .category = "analyze"
    },
};

const uft_tool_profile_t UFT_TOOL_ADFTOOLS = {
    .name = "ADFlib/unadf",
    .version = "0.8.x",
    .description = "Amiga ADF file manipulation",
    .homepage = "https://github.com/lclevy/ADFlib",
    .executable = "unadf",
    
    .io_caps = {
        .input = {
            .from_sector = true,
            .from_filesystem = true,
            .sector_formats = FMT_ADF,
        },
        .output = {
            .to_sector = true,
            .to_filesystem = true,
            .sector_formats = FMT_ADF,
        },
        .processing = {
            .can_analyze = true,
        },
    },
    
    .limits = {
        .max_cylinders = 80,
        .max_heads = 2,
        .format_notes = "ADF only. Filesystem-level access.",
    },
    
    .platforms = {
        { .os = "linux", .arch = "all" },
        { .os = "windows", .arch = "all" },
        { .os = "macos", .arch = "all" },
    },
    .platform_count = 3,
    
    .options = adf_options,
    .option_count = sizeof(adf_options) / sizeof(adf_options[0]),
    
    .analyze_pattern = "unadf -l {input}",
};

// ============================================================================
// mtools Options
// ============================================================================

static const uft_tool_option_t mtools_options[] = {
    {
        .name = "-i", .long_name = "image",
        .description = "Image file path",
        .type = OPT_STRING,
        .required = true,
        .category = "input"
    },
};

const uft_tool_profile_t UFT_TOOL_MTOOLS = {
    .name = "mtools",
    .version = "4.x",
    .description = "FAT filesystem access without mounting",
    .homepage = "https://www.gnu.org/software/mtools/",
    .executable = "mdir",
    
    .io_caps = {
        .input = {
            .from_sector = true,
            .from_filesystem = true,
            .sector_formats = FMT_IMG,
        },
        .output = {
            .to_sector = true,
            .to_filesystem = true,
            .sector_formats = FMT_IMG,
        },
        .processing = {
            .can_analyze = true,
        },
    },
    
    .limits = {
        .format_notes = "FAT12/16/32. PC disk images only.",
    },
    
    .platforms = {
        { .os = "linux", .arch = "all" },
        { .os = "macos", .arch = "all" },
    },
    .platform_count = 2,
    
    .options = mtools_options,
    .option_count = sizeof(mtools_options) / sizeof(mtools_options[0]),
    
    .analyze_pattern = "mdir -i {input} ::",
};

// ============================================================================
// Tool Registry
// ============================================================================

static const uft_tool_profile_t* g_tool_profiles[] = {
    &UFT_TOOL_GREASEWEAZLE,
    &UFT_TOOL_FLUXENGINE,
    &UFT_TOOL_KRYOFLUX,
    &UFT_TOOL_NIBTOOLS,
    &UFT_TOOL_HXCFE,
    &UFT_TOOL_DISK_ANALYSE,
    &UFT_TOOL_ADFTOOLS,
    &UFT_TOOL_MTOOLS,
    NULL
};

#define NUM_TOOLS (sizeof(g_tool_profiles) / sizeof(g_tool_profiles[0]) - 1)

// ============================================================================
// API Implementation
// ============================================================================

int uft_tool_get_profiles(const uft_tool_profile_t** profiles, int max) {
    int count = 0;
    for (size_t i = 0; i < NUM_TOOLS && count < max; i++) {
        if (profiles) profiles[count] = g_tool_profiles[i];
        count++;
    }
    return count;
}

const uft_tool_profile_t* uft_tool_find_profile(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < NUM_TOOLS; i++) {
        if (strcasecmp(g_tool_profiles[i]->name, name) == 0 ||
            strcasecmp(g_tool_profiles[i]->executable, name) == 0) {
            return g_tool_profiles[i];
        }
    }
    return NULL;
}

bool uft_tool_supports_format(const uft_tool_profile_t* tool,
                               uft_format_t format,
                               bool as_input) {
    if (!tool) return false;
    
    uint32_t fmt_bit = (1 << format);
    
    if (as_input) {
        return (tool->io_caps.input.flux_formats & fmt_bit) ||
               (tool->io_caps.input.bitstream_formats & fmt_bit) ||
               (tool->io_caps.input.sector_formats & fmt_bit);
    } else {
        return (tool->io_caps.output.flux_formats & fmt_bit) ||
               (tool->io_caps.output.bitstream_formats & fmt_bit) ||
               (tool->io_caps.output.sector_formats & fmt_bit);
    }
}

const uft_tool_profile_t* uft_tool_find_for_io(
    uft_data_layer_t input_layer,
    uft_data_layer_t output_layer,
    uft_format_t input_format,
    uft_format_t output_format) {
    
    for (size_t i = 0; i < NUM_TOOLS; i++) {
        const uft_tool_profile_t* t = g_tool_profiles[i];
        
        // Check input capability
        bool input_ok = false;
        switch (input_layer) {
            case UFT_LAYER_FLUX:
                input_ok = t->io_caps.input.from_flux;
                break;
            case UFT_LAYER_BITSTREAM:
                input_ok = t->io_caps.input.from_bitstream;
                break;
            case UFT_LAYER_SECTOR:
                input_ok = t->io_caps.input.from_sector;
                break;
            case UFT_LAYER_FILESYSTEM:
                input_ok = t->io_caps.input.from_filesystem;
                break;
        }
        
        if (!input_ok) continue;
        
        // Check output capability
        bool output_ok = false;
        switch (output_layer) {
            case UFT_LAYER_FLUX:
                output_ok = t->io_caps.output.to_flux;
                break;
            case UFT_LAYER_BITSTREAM:
                output_ok = t->io_caps.output.to_bitstream;
                break;
            case UFT_LAYER_SECTOR:
                output_ok = t->io_caps.output.to_sector;
                break;
            case UFT_LAYER_FILESYSTEM:
                output_ok = t->io_caps.output.to_filesystem;
                break;
        }
        
        if (!output_ok) continue;
        
        // Check format support
        if (input_format != UFT_FORMAT_UNKNOWN &&
            !uft_tool_supports_format(t, input_format, true)) {
            continue;
        }
        
        if (output_format != UFT_FORMAT_UNKNOWN &&
            !uft_tool_supports_format(t, output_format, false)) {
            continue;
        }
        
        return t;
    }
    
    return NULL;
}

// ============================================================================
// Print Capability Matrix
// ============================================================================

void uft_tool_print_matrix(void) {
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                        TOOL CAPABILITY MATRIX\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("%-16s │ HW │ FLUX │ BIT │ SEC │ FS │ Notes\n", "TOOL");
    printf("─────────────────┼────┼──────┼─────┼─────┼────┼─────────────────────────────────\n");
    
    for (size_t i = 0; i < NUM_TOOLS; i++) {
        const uft_tool_profile_t* t = g_tool_profiles[i];
        
        printf("%-16s │ ", t->name);
        
        // Input capabilities
        printf("%c%c │ ", 
               t->io_caps.input.from_hardware ? 'R' : ' ',
               t->io_caps.output.to_hardware ? 'W' : ' ');
        printf("%c%c   │ ",
               t->io_caps.input.from_flux ? 'R' : ' ',
               t->io_caps.output.to_flux ? 'W' : ' ');
        printf("%c%c  │ ",
               t->io_caps.input.from_bitstream ? 'R' : ' ',
               t->io_caps.output.to_bitstream ? 'W' : ' ');
        printf("%c%c  │ ",
               t->io_caps.input.from_sector ? 'R' : ' ',
               t->io_caps.output.to_sector ? 'W' : ' ');
        printf("%c%c │ ",
               t->io_caps.input.from_filesystem ? 'R' : ' ',
               t->io_caps.output.to_filesystem ? 'W' : ' ');
        
        printf("%s\n", t->limits.format_notes ? t->limits.format_notes : "");
    }
    
    printf("\n");
    printf("Legend: R=Read, W=Write, HW=Hardware, FLUX=Flux files, BIT=Bitstream, SEC=Sector, FS=Filesystem\n");
}
