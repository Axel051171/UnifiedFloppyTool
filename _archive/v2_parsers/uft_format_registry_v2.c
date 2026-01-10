/**
 * @file uft_format_registry_v2.c
 * @brief Unified Format Registry - All 90+ Formats
 * 
 * AUTO-GENERATED FORMAT REGISTRY
 * Alle Formate aus dem integrierten ZIP-Paket
 */

#include "uft/formats/uft_format_adapter.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Format Includes - Commodore
// ============================================================================
#include "uft/formats/d64.h"
#include "uft/formats/d67.h"
#include "uft/formats/d71.h"
#include "uft/formats/d80.h"
#include "uft/formats/d81.h"
#include "uft/formats/d82.h"
#include "uft/formats/d90.h"
#include "uft/formats/d91.h"
#include "uft/formats/x64.h"
#include "uft/formats/x71.h"
#include "uft/formats/x81.h"
#include "uft/formats/g64.h"
#include "uft/formats/dnp.h"
#include "uft/formats/dnp2.h"
#include "uft/formats/p00.h"
#include "uft/formats/prg.h"
#include "uft/formats/t64.h"
#include "uft/formats/crt.h"

// ============================================================================
// Format Includes - Atari
// ============================================================================
#include "uft/formats/atr.h"
#include "uft/formats/atx.h"
#include "uft/formats/xdf.h"
#include "uft/formats/st.h"
#include "uft/formats/stx.h"
#include "uft/formats/stt.h"
#include "uft/formats/stz.h"
#include "uft/formats/st_msa.h"

// ============================================================================
// Format Includes - Apple
// ============================================================================
#include "uft/formats/2mg.h"
#include "uft/formats/nib.h"
#include "uft/formats/nib_nbz.h"
#include "uft/formats/woz.h"
#include "uft/formats/prodos_po_do.h"
#include "uft/formats/mac_dsk.h"

// ============================================================================
// Format Includes - PC-98/Japanese
// ============================================================================
#include "uft/formats/d88.h"
#include "uft/formats/nfd.h"
#include "uft/formats/fdd.h"
#include "uft/formats/fdx.h"
#include "uft/formats/hdm.h"
#include "uft/formats/dim.h"

// ============================================================================
// Format Includes - TRS-80
// ============================================================================
#include "uft/formats/dmk.h"
#include "uft/formats/jv3_jvc.h"
#include "uft/formats/jvc.h"
#include "uft/formats/vdk.h"

// ============================================================================
// Format Includes - BBC/Acorn
// ============================================================================
#include "uft/formats/ssd_dsd.h"
#include "uft/formats/adf_adl.h"

// ============================================================================
// Format Includes - Amstrad/Spectrum
// ============================================================================
#include "uft/formats/dsk.h"
#include "uft/formats/edsk_extdsk.h"
#include "uft/formats/trd_scl.h"
#include "uft/formats/mgt_sad_sdf.h"
#include "uft/formats/dsk_mfm.h"

// ============================================================================
// Format Includes - TI-99/4A
// ============================================================================
#include "uft/formats/v9t9_pc99.h"
#include "uft/formats/fiad.h"
#include "uft/formats/tifiles.h"

// ============================================================================
// Format Includes - Flux
// ============================================================================
#include "uft/formats/scp.h"
#include "uft/formats/hfe.h"
#include "uft/formats/ipf.h"
#include "uft/formats/gwraw.h"
#include "uft/formats/kfraw.h"
#include "uft/formats/pfi.h"
#include "uft/formats/pri.h"
#include "uft/formats/psi.h"
#include "uft/formats/mfi.h"
#include "uft/formats/dfi.h"
#include "uft/formats/f86.h"
#include "uft/formats/fluxdec.h"
#include "uft/formats/flux_profiles.h"

// ============================================================================
// Format Includes - Misc
// ============================================================================
#include "uft/formats/pc_img.h"
#include "uft/formats/adf.h"
#include "uft/formats/adz.h"
#include "uft/formats/imz.h"
#include "uft/formats/imd.h"
#include "uft/formats/td0.h"
#include "uft/formats/fdi.h"
#include "uft/formats/cqm.h"
#include "uft/formats/tap.h"
#include "uft/formats/ms_dmf.h"
#include "uft/formats/dcp_dcu.h"
#include "uft/formats/oric_dsk.h"
#include "uft/formats/osd.h"
#include "uft/formats/dhd.h"
#include "uft/formats/edd.h"
#include "uft/formats/lnx.h"
#include "uft/formats/fds.h"
#include "uft/formats/dmf_msx.h"

// ============================================================================
// Format Registry Table
// ============================================================================

typedef struct {
    const char* name;
    const char* extensions;
    const char* description;
    const char* platform;
    int data_layer;  // 0=flux, 1=bitstream, 2=sector, 3=file
} format_info_t;

static const format_info_t g_format_info[] = {
    // Commodore
    {"D64", "d64", "C64/1541 Disk Image", "Commodore", 2},
    {"D67", "d67", "2040/3040 Disk Image", "Commodore", 2},
    {"D71", "d71", "1571 Double-sided", "Commodore", 2},
    {"D80", "d80", "8050 Single-sided", "Commodore", 2},
    {"D81", "d81", "1581 3.5\" MFM", "Commodore", 2},
    {"D82", "d82", "8250 Double-sided", "Commodore", 2},
    {"D90", "d90", "CMD D9060 HD", "Commodore", 2},
    {"D91", "d91", "CMD D9090 HD", "Commodore", 2},
    {"X64", "x64", "Extended D64", "Commodore", 2},
    {"X71", "x71", "Extended D71", "Commodore", 2},
    {"X81", "x81", "Extended D81", "Commodore", 2},
    {"G64", "g64", "GCR Track Image", "Commodore", 1},
    {"DNP", "dnp", "CMD Native Partition", "Commodore", 2},
    {"DNP2", "dnp2", "CMD Native v2", "Commodore", 2},
    {"P00", "p00,p01,p02", "PC64 File", "Commodore", 3},
    {"PRG", "prg", "C64 Program", "Commodore", 3},
    {"T64", "t64", "Tape Archive", "Commodore", 3},
    {"CRT", "crt", "Cartridge Image", "Commodore", 3},
    
    // Atari
    {"ATR", "atr", "Atari 8-bit Disk", "Atari", 2},
    {"ATX", "atx", "Atari Extended (protection)", "Atari", 1},
    {"XDF", "xdf", "Extended Density", "Atari", 2},
    {"ST", "st", "Atari ST Raw", "Atari ST", 2},
    {"STX", "stx", "Pasti Extended", "Atari ST", 1},
    {"STT", "stt", "Pasti Track", "Atari ST", 1},
    {"STZ", "stz", "Zipped ST", "Atari ST", 2},
    {"MSA", "msa", "Magic Shadow Archiver", "Atari ST", 2},
    
    // Apple
    {"2MG", "2mg,2img", "Apple IIgs Universal", "Apple", 2},
    {"NIB", "nib", "Apple II Nibble", "Apple", 1},
    {"NBZ", "nbz", "Compressed NIB", "Apple", 1},
    {"WOZ", "woz", "WOZ Preservation", "Apple", 0},
    {"PO", "po", "ProDOS Order", "Apple", 2},
    {"DO", "do,dsk", "DOS Order", "Apple", 2},
    {"MAC_DSK", "image,img", "Macintosh Disk", "Apple", 2},
    
    // PC-98/Japanese
    {"D88", "d88,d77,d68", "PC-88/PC-98 Disk", "PC-98", 2},
    {"NFD", "nfd", "NFD Format", "PC-98", 2},
    {"FDD", "fdd", "FDD Format", "PC-98", 2},
    {"FDX", "fdx", "FDX Extended", "PC-98", 2},
    {"HDM", "hdm", "HDM Format", "PC-98", 2},
    {"DIM", "dim", "DIM Format", "PC-98", 2},
    
    // TRS-80
    {"DMK", "dmk", "TRS-80 Track Image", "TRS-80", 1},
    {"JV3", "jv3", "JV3 Format", "TRS-80", 2},
    {"JVC", "jvc,dsk", "JVC Format", "TRS-80", 2},
    {"VDK", "vdk", "Virtual Disk", "TRS-80", 2},
    
    // BBC/Acorn
    {"SSD", "ssd", "BBC Micro SS", "BBC", 2},
    {"DSD", "dsd", "BBC Micro DS", "BBC", 2},
    {"ADF_ADL", "adf,adl", "Acorn ADFS", "Acorn", 2},
    
    // Amstrad/Spectrum
    {"DSK", "dsk", "Amstrad CPC Disk", "Amstrad", 2},
    {"EDSK", "dsk", "Extended DSK", "Amstrad", 1},
    {"TRD", "trd", "TR-DOS Disk", "Spectrum", 2},
    {"SCL", "scl", "Sinclair Archive", "Spectrum", 3},
    {"MGT", "mgt", "MGT +D Image", "SAM", 2},
    {"SAD", "sad", "SAM Disk", "SAM", 2},
    {"SDF", "sdf", "SAM Disk Format", "SAM", 2},
    
    // TI-99/4A
    {"V9T9", "dsk", "V9T9 Disk", "TI-99", 2},
    {"PC99", "dsk", "PC99 Disk", "TI-99", 2},
    {"FIAD", "tfi", "TI Files", "TI-99", 3},
    {"TIFILES", "tifiles", "TIFILES Format", "TI-99", 3},
    
    // Flux
    {"SCP", "scp", "SuperCard Pro", "Flux", 0},
    {"HFE", "hfe", "UFT HFE Format", "Flux", 1},
    {"IPF", "ipf", "SPS Preservation", "Flux", 0},
    {"GWRAW", "raw", "Greaseweazle Raw", "Flux", 0},
    {"KFRAW", "raw", "Kryoflux Stream", "Flux", 0},
    {"PFI", "pfi", "PCE Flux Image", "Flux", 0},
    {"PRI", "pri", "PCE Raw Image", "Flux", 1},
    {"PSI", "psi", "PCE Sector Image", "Flux", 2},
    {"MFI", "mfi", "MAME Floppy Image", "Flux", 0},
    {"DFI", "dfi", "DiscFerret Image", "Flux", 0},
    {"86F", "86f", "86Box Floppy", "Flux", 1},
    
    // Misc
    {"IMG", "img,ima,flp", "PC Raw Sector", "PC", 2},
    {"ADF", "adf", "Amiga Disk File", "Amiga", 2},
    {"ADZ", "adz", "Gzipped ADF", "Amiga", 2},
    {"IMZ", "imz", "Gzipped IMG", "PC", 2},
    {"IMD", "imd", "ImageDisk", "PC", 2},
    {"TD0", "td0", "Teledisk", "PC", 2},
    {"FDI", "fdi", "Formatted Disk Image", "Multi", 2},
    {"CQM", "cqm", "CopyQM", "PC", 2},
    {"TAP", "tap", "Tape Image", "Multi", 3},
    {"MS_DMF", "dmf", "Microsoft DMF 1.68MB", "PC", 2},
    {"DCP", "dcp", "Disk Copy", "Mac", 2},
    {"DCU", "dcu", "Disk Copy Ultra", "Mac", 2},
    {"ORIC_DSK", "dsk", "Oric Disk", "Oric", 2},
    {"OSD", "osd", "OS-9 Disk", "OS-9", 2},
    {"DHD", "dhd", "Hard Disk Image", "Multi", 2},
    {"EDD", "edd", "Enhanced Density", "Preservation", 0},
    {"LNX", "lnx", "Atari Lynx Cart", "Lynx", 3},
    {"FDS", "fds", "Famicom Disk", "NES", 2},
    {"DMF_MSX", "dsk", "MSX Disk", "MSX", 2},
    
    // Sentinel
    {NULL, NULL, NULL, NULL, 0}
};

// ============================================================================
// Registry API
// ============================================================================

int uft_format_registry_count(void) {
    int count = 0;
    while (g_format_info[count].name) count++;
    return count;
}

const char* uft_format_registry_get_name(int index) {
    if (index < 0 || index >= uft_format_registry_count()) return NULL;
    return g_format_info[index].name;
}

const char* uft_format_registry_get_extensions(int index) {
    if (index < 0 || index >= uft_format_registry_count()) return NULL;
    return g_format_info[index].extensions;
}

const char* uft_format_registry_get_platform(int index) {
    if (index < 0 || index >= uft_format_registry_count()) return NULL;
    return g_format_info[index].platform;
}

int uft_format_registry_find_by_extension(const char* ext) {
    if (!ext) return -1;
    
    for (int i = 0; g_format_info[i].name; i++) {
        // Check if extension matches any in comma-separated list
        const char* exts = g_format_info[i].extensions;
        char buf[64];
        strncpy(buf, exts, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        
        char* token = strtok(buf, ",");
        while (token) {
            if (strcasecmp(token, ext) == 0) {
                return i;
            }
            token = strtok(NULL, ",");
        }
    }
    return -1;
}

void uft_format_registry_print(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                    UFT FORMAT REGISTRY (%d FORMATS)                                        ║\n", uft_format_registry_count());
    printf("╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ %-12s │ %-20s │ %-30s │ %-12s │ %-8s ║\n", "Name", "Extensions", "Description", "Platform", "Layer");
    printf("╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
    
    const char* layers[] = {"Flux", "Bitstream", "Sector", "File"};
    
    for (int i = 0; g_format_info[i].name; i++) {
        printf("║ %-12s │ %-20s │ %-30s │ %-12s │ %-8s ║\n",
               g_format_info[i].name,
               g_format_info[i].extensions,
               g_format_info[i].description,
               g_format_info[i].platform,
               layers[g_format_info[i].data_layer]);
    }
    
    printf("╚════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// ============================================================================

#include "uft/formats/brother.h"
#include "uft/formats/victor9k.h"
#include "uft/formats/micropolis.h"
#include "uft/formats/northstar.h"
#include "uft/formats/rolandd20.h"
#include "uft/formats/agat.h"
#include "uft/formats/zilogmcz.h"
#include "uft/formats/tids990.h"

#include "uft/formats/udi.h"
#include "uft/formats/lif.h"
#include "uft/formats/qdos.h"
#include "uft/formats/sap.h"
#include "uft/formats/opd.h"
#include "uft/formats/cpm.h"

// A8rawconv Formats
#include "uft/formats/vfd.h"
#include "uft/formats/xfd.h"

static const format_info_t g_extended_format_info[] = {
    {"Brother", "br", "Brother Word Processor", "Brother", 1},
    {"Victor9K", "v9k", "Victor 9000 / Sirius 1", "Victor", 1},
    {"Micropolis", "mpo", "Micropolis Vector Graphic", "Micropolis", 2},
    {"NorthStar", "nsi", "North Star Horizon", "NorthStar", 2},
    {"RolandD20", "d20", "Roland D-20 Synthesizer", "Roland", 2},
    {"Agat", "agat", "Agat (Soviet Apple II)", "Agat", 2},
    {"ZilogMCZ", "mcz", "Zilog MCZ Development", "Zilog", 2},
    {"TIDS990", "ti", "TI DS/990 Minicomputer", "TI", 2},
    {"Aeslanier", "aes", "Aeslanier Word Processor", "Aeslanier", 1},
    {"FB100", "fb", "FB-100", "FB", 2},
    {"Smaky6", "smk", "Smaky 6", "Smaky", 2},
    {"Tartu", "tar", "Tartu", "Tartu", 2},
    
    {"UDI", "udi", "Universal Disk Image", "Spectrum", 1},
    {"LIF", "lif", "HP LIF Format", "HP", 2},
    {"QDOS", "ql,mdv", "Sinclair QL / QDOS", "QL", 2},
    {"SAP", "sap", "Thomson SAP Archive", "Thomson", 2},
    {"OPD", "opd,opu", "Opus Discovery", "Spectrum", 2},
    {"CPM", "cpm", "CP/M Generic", "CP/M", 2},
    {"CFI", "cfi", "Catweasel Flux Image", "Flux", 0},
    {"DTI", "dti", "Disk Tool Image", "Multi", 1},
    {"PDI", "pdi", "PDI Format", "Multi", 2},
    {"MBD", "mbd", "MBD820/MBD1804", "Multi", 2},
    {"S24", "s24", "S24 Format", "Multi", 2},
    {"SBT", "sbt", "SBT Format", "Multi", 2},
    {"DS2", "ds2", "DS2 Format", "Multi", 2},
    {"DSC", "dsc", "DSC Format", "Multi", 2},
    {"CWTool", "cwt", "CWTool Format", "Flux", 0},
    {"Trinity", "trin", "Trinity Format", "Spectrum", 2},
    
    // A8rawconv Formats
    {"VFD", "vfd", "Virtual Floppy Disk", "PC", 2},
    {"XFD", "xfd", "Atari XFD (headerless)", "Atari", 2},
    
    // Sentinel
    {NULL, NULL, NULL, NULL, 0}
};

int uft_format_extended_count(void) {
    int count = 0;
    while (g_extended_format_info[count].name) count++;
    return count;
}

void uft_format_print_extended(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                           EXTENDED FORMAT REGISTRY (%d FORMATS)                                           ║\n", uft_format_extended_count());
    printf("╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
    
    const char* layers[] = {"Flux", "Bitstream", "Sector", "File"};
    
    for (int i = 0; g_extended_format_info[i].name; i++) {
        printf("║ %-12s │ %-12s │ %-28s │ %-12s │ %-8s ║\n",
               g_extended_format_info[i].name,
               g_extended_format_info[i].extensions,
               g_extended_format_info[i].description,
               g_extended_format_info[i].platform,
               layers[g_extended_format_info[i].data_layer]);
    }
    
    printf("╚════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n");
}
