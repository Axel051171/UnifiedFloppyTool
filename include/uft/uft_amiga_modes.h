/**
 * @file uft_amiga_modes.h
 * @brief Amiga Disk Copy/Recovery/Virus Scan Modes for UFT GUI
 * 
 * This header defines copy modes, recovery options, and virus scanning
 * - XCopy Pro (Copy modes: DosCopy, DosCopy+, BamCopy+, Nibble)
 * - DiskSalv 4 (Recovery and stream archive format)
 * - XVS Library (Virus scanning for bootblocks, sectors, files)
 * 
 * These definitions are designed to integrate with UFT's Qt GUI
 * for a comprehensive Amiga disk imaging/recovery workflow.
 * 
 * @author UFT Development Team
 * @date 2025
 * @license MIT
 */

#ifndef UFT_AMIGA_MODES_H
#define UFT_AMIGA_MODES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// XCopy Copy Modes (from xcopy.i)
// ============================================================================

/**
 * @brief XCopy-style copy modes
 * 
 * These modes determine how tracks are copied:
 * - DOSCOPY: Standard DOS copy, only used tracks
 * - BAMCOPY: Block Allocation Map aware copy (faster)
 * - DOSPLUS: DOS copy with verify
 * - NIBBLE:  Raw track copy (for copy protection)
 */
typedef enum uft_amiga_copy_mode {
    UFT_AMIGA_MODE_DOSCOPY    = 0,  ///< Standard DOS sector copy
    UFT_AMIGA_MODE_BAMCOPY    = 1,  ///< BAM-aware copy (skip empty tracks)
    UFT_AMIGA_MODE_DOSPLUS    = 2,  ///< DOS copy with extra verification
    UFT_AMIGA_MODE_NIBBLE     = 3,  ///< Raw nibble copy (preserves protection)
    
    // Additional operations from XCopy
    UFT_AMIGA_MODE_OPTIMIZE   = 4,  ///< Disk optimizer
    UFT_AMIGA_MODE_FORMAT     = 5,  ///< Full format
    UFT_AMIGA_MODE_QFORMAT    = 6,  ///< Quick format
    UFT_AMIGA_MODE_ERASE      = 7,  ///< Erase disk
    UFT_AMIGA_MODE_MESLEN     = 8,  ///< Measure disk length/speed
    
    // Utility operations
    UFT_AMIGA_MODE_NAME       = 9,  ///< Read/set disk name
    UFT_AMIGA_MODE_DIR        = 10, ///< Directory listing
    UFT_AMIGA_MODE_CHECK      = 11, ///< Disk check/verify
    UFT_AMIGA_MODE_INSTALL    = 12, ///< Install bootblock
    
    UFT_AMIGA_MODE_MAX
} uft_amiga_copy_mode_t;

/**
 * @brief Human-readable mode names for GUI
 */
static const char* const UFT_AMIGA_MODE_NAMES[] = {
    "DosCopy",      // 0
    "BamCopy+",     // 1
    "DosCopy+",     // 2
    "Nibble",       // 3
    "Optimize",     // 4
    "Format",       // 5
    "Quick Format", // 6
    "Erase",        // 7
    "Speed Check",  // 8
    "Disk Name",    // 9
    "Directory",    // 10
    "Verify",       // 11
    "Install Boot"  // 12
};

/**
 * @brief Mode descriptions for GUI tooltips
 */
static const char* const UFT_AMIGA_MODE_DESCRIPTIONS[] = {
    "Standard DOS copy - copies all used sectors",
    "BAM-aware copy - skips empty tracks for speed",
    "DOS copy with verification pass",
    "Raw track copy - preserves copy protection",
    "Optimize disk file layout",
    "Full format with verify",
    "Quick format (root block only)",
    "Erase all data on disk",
    "Measure disk rotation speed",
    "View or set disk name",
    "Show directory listing",
    "Verify disk integrity",
    "Install bootblock"
};

// ============================================================================
// XCopy Sync Word Options
// ============================================================================

/**
 * @brief Sync word for track reading
 */
typedef enum uft_amiga_sync_type {
    UFT_SYNC_AMIGA_MFM   = 0x4489,  ///< Standard Amiga MFM sync
    UFT_SYNC_INDEX       = 0xF8BC,  ///< Index sync (raw mode)
    UFT_SYNC_CUSTOM      = 0       ///< Custom sync word
} uft_amiga_sync_type_t;

// ============================================================================
// XCopy Drive Selection
// ============================================================================

/**
 * @brief Drive selection bitmask
 */
typedef enum uft_amiga_drive {
    UFT_DRIVE_NONE = 0x00,
    UFT_DRIVE_DF0  = 0x01,  ///< DF0:
    UFT_DRIVE_DF1  = 0x02,  ///< DF1:
    UFT_DRIVE_DF2  = 0x04,  ///< DF2:
    UFT_DRIVE_DF3  = 0x08,  ///< DF3:
    UFT_DRIVE_ALL  = 0x0F
} uft_amiga_drive_t;

/**
 * @brief Side selection
 */
typedef enum uft_amiga_side {
    UFT_SIDE_BOTH  = 0,  ///< Both sides
    UFT_SIDE_UPPER = 1,  ///< Side 0 only
    UFT_SIDE_LOWER = 2   ///< Side 1 only
} uft_amiga_side_t;

// ============================================================================
// XCopy Error Codes
// ============================================================================

/**
 * @brief XCopy operation result codes
 */
typedef enum uft_amiga_result {
    UFT_AMIGA_OK           = 0,  ///< Success
    UFT_AMIGA_SPECIAL_ERR  = 1,  ///< Special error
    UFT_AMIGA_USER_BREAK   = 2,  ///< User cancelled
    UFT_AMIGA_NO_INDEX     = 3,  ///< No index signal
    UFT_AMIGA_VERIFY_ERR   = 4,  ///< Verify error
    UFT_AMIGA_WRITE_PROT   = 5,  ///< Write protected
    UFT_AMIGA_NO_DRIVE     = 6,  ///< Drive not found
    UFT_AMIGA_OPT_ERR      = 7,  ///< Optimize error
    UFT_AMIGA_NO_MEMORY    = 8   ///< Out of memory
} uft_amiga_result_t;

// ============================================================================
// XCopy Copy Parameters (for GUI)
// ============================================================================

/**
 * @brief Complete XCopy-style parameters for GUI binding
 */
typedef struct uft_amiga_copy_params {
    // Track range
    uint16_t start_track;      ///< First track (0-83)
    uint16_t end_track;        ///< Last track (0-83)
    uint16_t start_head;       ///< Start side (0-1)
    uint16_t end_head;         ///< End side (0-1)
    
    // Mode and options
    uft_amiga_copy_mode_t mode;     ///< Copy mode
    uft_amiga_side_t      side;     ///< Side selection
    uint16_t              sync;     ///< Sync word (0x4489 or 0xF8BC)
    
    // Drive selection
    uft_amiga_drive_t source;       ///< Source drive(s)
    uft_amiga_drive_t target;       ///< Target drive(s)
    uft_amiga_drive_t verify;       ///< Verify on these drives
    
    // Options
    bool use_ram_buffer;            ///< Use RAM for multi-disk copy
    bool kill_system;               ///< Disable system during copy
    uint8_t retries;                ///< Number of retries (default: 3)
} uft_amiga_copy_params_t;

/**
 * @brief Default Amiga copy parameters
 */
static inline uft_amiga_copy_params_t uft_amiga_default_params(void) {
    return (uft_amiga_copy_params_t){
        .start_track = 0,
        .end_track = 79,
        .start_head = 0,
        .end_head = 1,
        .mode = UFT_AMIGA_MODE_DOSCOPY,
        .side = UFT_SIDE_BOTH,
        .sync = UFT_SYNC_AMIGA_MFM,
        .source = UFT_DRIVE_DF0,
        .target = UFT_DRIVE_DF1,
        .verify = UFT_DRIVE_DF1,
        .use_ram_buffer = false,
        .kill_system = false,
        .retries = 3
    };
}

// ============================================================================
// DiskSalv Recovery Types (from ds_stream.h)
// ============================================================================

/**
 * @brief DiskSalv stream block types
 */
typedef enum uft_disksalv_block_type {
    UFT_DS_ROOT = 0x524F4F54,  ///< 'ROOT' - Archive root
    UFT_DS_UDIR = 0x55444952,  ///< 'UDIR' - User directory
    UFT_DS_FILE = 0x46494C45,  ///< 'FILE' - File header
    UFT_DS_DATA = 0x44415441,  ///< 'DATA' - File data
    UFT_DS_DLNK = 0x444C4E4B,  ///< 'DLNK' - Directory hard link
    UFT_DS_FLNK = 0x464C4E4B,  ///< 'FLNK' - File hard link
    UFT_DS_SLNK = 0x534C4E4B,  ///< 'SLNK' - Soft link
    UFT_DS_ERRS = 0x45525253,  ///< 'ERRS' - Error record
    UFT_DS_ENDA = 0x454E4441,  ///< 'ENDA' - End of archive
    UFT_DS_DELB = 0x44454C42   ///< 'DELB' - Delete marker
} uft_disksalv_block_type_t;

/**
 * @brief DiskSalv stream header
 */
typedef struct uft_disksalv_header {
    uint32_t type;       ///< Block type (uft_disksalv_block_type_t)
    uint32_t size;       ///< Block size
    uint32_t count;      ///< Various counts
    uint32_t parent;     ///< Parent object ID
    uint32_t id;         ///< Object ID
    uint32_t checksum;   ///< Block checksum
} uft_disksalv_header_t;

/**
 * @brief DiskSalv file basics
 */
typedef struct uft_disksalv_basics {
    char     filename[32];   ///< File name
    uint32_t protect;        ///< Protection bits
    uint32_t days;           ///< DateStamp.ds_Days
    uint32_t mins;           ///< DateStamp.ds_Minute
    uint32_t ticks;          ///< DateStamp.ds_Tick
    char     filenote[92];   ///< File comment
} uft_disksalv_basics_t;

/**
 * @brief DiskSalv recovery statistics
 */
typedef struct uft_disksalv_stats {
    uint32_t file_count;     ///< Files recovered
    uint32_t dir_count;      ///< Directories recovered
    uint32_t link_count;     ///< Links recovered
    uint32_t error_count;    ///< Errors encountered
    uint32_t total_objects;  ///< Total objects processed
} uft_disksalv_stats_t;

// ============================================================================
// XVS Virus Scanner Types (from xvs.h)
// ============================================================================

/**
 * @brief XVS virus list types
 */
typedef enum uft_xvs_list_type {
    UFT_XVS_LIST_BOOT  = 0x42,  ///< 'B' - Boot viruses
    UFT_XVS_LIST_DATA  = 0x44,  ///< 'D' - Data viruses
    UFT_XVS_LIST_FILE  = 0x46,  ///< 'F' - File viruses
    UFT_XVS_LIST_LINK  = 0x4C   ///< 'L' - Link viruses
} uft_xvs_list_type_t;

/**
 * @brief XVS bootblock types
 */
typedef enum uft_xvs_boot_type {
    UFT_XVS_BT_UNKNOWN     = 0,  ///< Unknown bootblock
    UFT_XVS_BT_NOTDOS      = 1,  ///< Not a DOS bootblock
    UFT_XVS_BT_STANDARD13  = 2,  ///< Standard 1.3 bootblock
    UFT_XVS_BT_STANDARD20  = 3,  ///< Standard 2.0+ bootblock
    UFT_XVS_BT_VIRUS       = 4,  ///< Virus detected!
    UFT_XVS_BT_UNINSTALLED = 5   ///< No bootblock installed
} uft_xvs_boot_type_t;

/**
 * @brief XVS sector status
 */
typedef enum uft_xvs_sector_type {
    UFT_XVS_ST_UNKNOWN   = 0,  ///< Unknown sector
    UFT_XVS_ST_DESTROYED = 1,  ///< Destroyed by virus
    UFT_XVS_ST_INFECTED  = 2   ///< Infected by virus
} uft_xvs_sector_type_t;

/**
 * @brief XVS file types
 */
typedef enum uft_xvs_file_type {
    UFT_XVS_FT_EMPTY     = 1,  ///< Empty file
    UFT_XVS_FT_DATA      = 2,  ///< Data file
    UFT_XVS_FT_EXE       = 3,  ///< Executable file
    UFT_XVS_FT_DATAVIRUS = 4,  ///< Data virus (must delete)
    UFT_XVS_FT_FILEVIRUS = 5,  ///< File virus (must delete)
    UFT_XVS_FT_LINKVIRUS = 6   ///< Link virus (can repair)
} uft_xvs_file_type_t;

/**
 * @brief XVS repair error codes
 */
typedef enum uft_xvs_error {
    UFT_XVS_ERR_NONE           = 0,
    UFT_XVS_ERR_WRONG_TYPE     = 1,  ///< Wrong file type
    UFT_XVS_ERR_TRUNCATED      = 2,  ///< File truncated
    UFT_XVS_ERR_BAD_HUNK       = 3,  ///< Unsupported hunk
    UFT_XVS_ERR_UNEXPECTED     = 4,  ///< Unexpected data
    UFT_XVS_ERR_NO_MEMORY      = 5,  ///< Out of memory
    UFT_XVS_ERR_NOT_IMPL       = 6   ///< Not implemented
} uft_xvs_error_t;

/**
 * @brief Bootblock scan result
 */
typedef struct uft_xvs_boot_info {
    uint8_t*           bootblock;     ///< 1024 byte bootblock
    const char*        name;          ///< Description/virus name
    uft_xvs_boot_type_t boot_type;    ///< Boot type
    uint8_t            dos_type;      ///< DOS type (0-7)
    bool               checksum_ok;   ///< Checksum valid?
} uft_xvs_boot_info_t;

/**
 * @brief Sector scan result
 */
typedef struct uft_xvs_sector_info {
    uint8_t*             sector;       ///< 512 byte sector
    uint32_t             key;          ///< Sector number
    const char*          name;         ///< Description/virus name
    uft_xvs_sector_type_t sector_type; ///< Sector status
} uft_xvs_sector_info_t;

/**
 * @brief File scan result
 */
typedef struct uft_xvs_file_info {
    uint8_t*           file;          ///< File buffer
    uint32_t           file_len;      ///< File length
    const char*        name;          ///< Description/virus name
    uft_xvs_file_type_t file_type;    ///< File type
    bool               modified;      ///< Buffer was modified?
    uft_xvs_error_t    error_code;    ///< Error if repair failed
    uint8_t*           fixed;         ///< Fixed file (if repaired)
    uint32_t           fixed_len;     ///< Fixed file length
} uft_xvs_file_info_t;

// ============================================================================
// Amiga Filesystem Constants
// ============================================================================

/**
 * @brief Amiga DOS types
 */
typedef enum uft_amiga_dos_type {
    UFT_DOS_OFS      = 0,  ///< DOS\0 - Original File System
    UFT_DOS_FFS      = 1,  ///< DOS\1 - Fast File System
    UFT_DOS_OFS_INTL = 2,  ///< DOS\2 - OFS International
    UFT_DOS_FFS_INTL = 3,  ///< DOS\3 - FFS International
    UFT_DOS_OFS_DC   = 4,  ///< DOS\4 - OFS Dir Cache
    UFT_DOS_FFS_DC   = 5,  ///< DOS\5 - FFS Dir Cache
    UFT_DOS_LNFS     = 6,  ///< DOS\6 - Long Filename
    UFT_DOS_LNFS_DC  = 7   ///< DOS\7 - Long Filename + DC
} uft_amiga_dos_type_t;

/**
 * @brief DOS type names for GUI
 */
static const char* const UFT_AMIGA_DOS_NAMES[] = {
    "OFS (Original)",
    "FFS (Fast)",
    "OFS International",
    "FFS International",
    "OFS + DirCache",
    "FFS + DirCache",
    "Long Names",
    "Long Names + DC"
};

/**
 * @brief Amiga disk geometry
 */
typedef struct uft_amiga_geometry {
    uint16_t cylinders;      ///< Number of cylinders (80/83)
    uint16_t heads;          ///< Number of heads (2)
    uint16_t sectors;        ///< Sectors per track (11 DD, 22 HD)
    uint16_t block_size;     ///< Block size (512)
    uint32_t total_blocks;   ///< Total blocks
    bool     is_hd;          ///< High density?
} uft_amiga_geometry_t;

/**
 * @brief Standard DD geometry
 */
static inline uft_amiga_geometry_t uft_amiga_dd_geometry(void) {
    return (uft_amiga_geometry_t){
        .cylinders = 80,
        .heads = 2,
        .sectors = 11,
        .block_size = 512,
        .total_blocks = 80 * 2 * 11,  // 1760 = 880KB
        .is_hd = false
    };
}

/**
 * @brief Standard HD geometry
 */
static inline uft_amiga_geometry_t uft_amiga_hd_geometry(void) {
    return (uft_amiga_geometry_t){
        .cylinders = 80,
        .heads = 2,
        .sectors = 22,
        .block_size = 512,
        .total_blocks = 80 * 2 * 22,  // 3520 = 1.76MB
        .is_hd = true
    };
}

// ============================================================================
// GUI Integration Structures
// ============================================================================

/**
 * @brief Combined Amiga operation for GUI
 * 
 * This structure combines all Amiga-related parameters into
 * a single structure suitable for Qt GUI binding.
 */
typedef struct uft_amiga_operation {
    // Basic operation
    uft_amiga_copy_mode_t mode;
    
    // Track range
    struct {
        uint16_t start;
        uint16_t end;
    } tracks;
    
    // Side selection
    uft_amiga_side_t side;
    
    // Source/Target
    struct {
        int source_drive;    ///< -1 = file, 0-3 = DF0:-DF3:
        int target_drive;    ///< -1 = file, 0-3 = DF0:-DF3:
        const char* source_file;
        const char* target_file;
    } io;
    
    // Options
    struct {
        bool verify;
        bool virus_scan;
        bool recover_mode;       ///< DiskSalv-style recovery
        uint8_t retries;
        uint16_t sync_word;
    } options;
    
    // Results
    struct {
        uft_amiga_result_t result;
        uint32_t tracks_ok;
        uint32_t tracks_bad;
        uint32_t sectors_recovered;
        const char* virus_name;  ///< If virus found
    } results;
    
} uft_amiga_operation_t;

/**
 * @brief Progress callback for GUI
 */
typedef void (*uft_amiga_progress_fn)(
    int track,
    int head,
    int percent,
    const char* status,
    void* user_data
);

/**
 * @brief Track status for GUI grid
 */
typedef enum uft_amiga_track_status {
    UFT_TRACK_UNKNOWN   = 0,  ///< Not yet processed
    UFT_TRACK_READING   = 1,  ///< Currently reading
    UFT_TRACK_OK        = 2,  ///< Read/written OK
    UFT_TRACK_BAD       = 3,  ///< Has errors
    UFT_TRACK_EMPTY     = 4,  ///< Empty (BAM says unused)
    UFT_TRACK_PROTECTED = 5,  ///< Copy protection detected
    UFT_TRACK_RECOVERED = 6   ///< Recovered with errors
} uft_amiga_track_status_t;

/**
 */
static inline uint32_t uft_amiga_track_color(uft_amiga_track_status_t status) {
    switch (status) {
        case UFT_TRACK_UNKNOWN:   return 0x808080;  // Gray
        case UFT_TRACK_READING:   return 0x0080FF;  // Blue (animated)
        case UFT_TRACK_OK:        return 0x00C000;  // Green
        case UFT_TRACK_BAD:       return 0xFF0000;  // Red
        case UFT_TRACK_EMPTY:     return 0x404040;  // Dark gray
        case UFT_TRACK_PROTECTED: return 0xFFFF00;  // Yellow
        case UFT_TRACK_RECOVERED: return 0xFFA500;  // Orange
        default:                  return 0x000000;  // Black
    }
}

// ============================================================================
// Standard Amiga Bootblocks
// ============================================================================

/**
 * @brief Install a standard bootblock
 * 
 * @param buffer 1024-byte bootblock buffer
 * @param dos_type DOS type (0-7)
 * @param kickstart 1 = 1.3 style, 2 = 2.0+ style
 */
void uft_amiga_install_bootblock(uint8_t* buffer, 
                                  uft_amiga_dos_type_t dos_type,
                                  int kickstart);

/**
 * @brief Calculate Amiga bootblock checksum
 * 
 * @param buffer 1024-byte bootblock buffer
 * @return Checksum value
 */
uint32_t uft_amiga_bootblock_checksum(const uint8_t* buffer);

/**
 * @brief Verify bootblock checksum
 * 
 * @param buffer 1024-byte bootblock buffer
 * @return true if checksum is valid
 */
static inline bool uft_amiga_verify_bootblock(const uint8_t* buffer) {
    uint32_t sum = 0;
    const uint32_t* p = (const uint32_t*)buffer;
    for (int i = 0; i < 256; i++) {
        uint32_t old = sum;
        sum += __builtin_bswap32(p[i]);  // Big-endian!
        if (sum < old) sum++;  // Add carry
    }
    return sum == 0xFFFFFFFF;
}

#ifdef __cplusplus
}
#endif

#endif // UFT_AMIGA_MODES_H
