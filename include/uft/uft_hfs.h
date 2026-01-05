/**
 * @file uft_hfs.h
 * @brief Apple HFS, HFS+, HFSX und Apple Partition Map Support
 * @version 3.1.4.005
 *
 * Vollständige Unterstützung für:
 * - Apple Partition Map (APM) - Block 0 + DPME Entries
 * - HFS (Hierarchical File System) - Classic Mac OS
 * - HFS+ (Extended HFS) - Mac OS 8.1+
 * - HFSX (Case-sensitive HFS+) - Mac OS X
 * - Resource Forks und Extended Attributes
 * - HFS Compression (DEFLATE, LZVN, LZFSE)
 * - B-Tree Catalog/Extents/Attributes Files
 *
 * Quellen:
 * - libfshfs (Joachim Metz, LGPL)
 * - hfdisk (Eryk Vershen/Apple)
 * - Inside Macintosh: Devices & Files
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_HFS_H
#define UFT_HFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * APPLE PARTITION MAP (APM)
 *============================================================================*/

/** @brief Block 0 Signatur "ER" */
#define UFT_APM_BLOCK0_SIGNATURE    0x4552

/** @brief Partition Map Entry Signatur "PM" */
#define UFT_APM_DPME_SIGNATURE      0x504D

/** @brief A/UX BZB Magic */
#define UFT_APM_BZB_MAGIC           0xABADBABE

/** @brief Maximale Länge für Partition-Namen */
#define UFT_APM_NAME_LENGTH         32

/**
 * @brief Block 0 - Driver Descriptor Map
 *
 * Physischer Block 0 der Disk, enthält Disk-Geometrie und Driver-Map
 */
typedef struct  uft_apm_block0 {
    uint16_t signature;         /**< Muss 0x4552 ("ER") sein */
    uint16_t block_size;        /**< Blockgröße des Geräts (typ. 512) */
    uint32_t block_count;       /**< Anzahl Blöcke auf dem Gerät */
    uint16_t device_type;       /**< Gerätetyp */
    uint16_t device_id;         /**< Geräte-ID */
    uint32_t driver_data;       /**< Reserviert für Treiber */
    uint16_t driver_count;      /**< Anzahl Treiber-Einträge */
    /* DDMap entries folgen... */
    uint8_t  driver_map[494];   /**< Driver Descriptor Map */
} uft_apm_block0_t;

/**
 * @brief Driver Descriptor Map Entry
 */
typedef struct  uft_apm_ddmap {
    uint32_t block;             /**< Startblock des Treibers */
    uint16_t size;              /**< Größe in 512-Byte Blöcken */
    uint16_t type;              /**< Systemtyp (1 für Mac+) */
} uft_apm_ddmap_t;

/**
 * @brief Partition Map Entry Flags
 */
typedef enum {
    UFT_APM_FLAG_VALID       = 0x0001,  /**< Eintrag ist gültig */
    UFT_APM_FLAG_ALLOCATED   = 0x0002,  /**< Partition ist allokiert */
    UFT_APM_FLAG_IN_USE      = 0x0004,  /**< Partition wird verwendet */
    UFT_APM_FLAG_BOOTABLE    = 0x0008,  /**< Partition ist bootbar */
    UFT_APM_FLAG_READABLE    = 0x0010,  /**< Partition ist lesbar */
    UFT_APM_FLAG_WRITABLE    = 0x0020,  /**< Partition ist beschreibbar */
    UFT_APM_FLAG_PIC_CODE    = 0x0040,  /**< Boot-Code ist PIC */
    UFT_APM_FLAG_AUTOMOUNT   = 0x40000000, /**< Automount aktiviert */
} uft_apm_flags_t;

/**
 * @brief Disk Partition Map Entry (DPME)
 *
 * Jeder DPME ist 512 Bytes groß und beschreibt eine Partition
 */
typedef struct  uft_apm_dpme {
    uint16_t signature;                 /**< Muss 0x504D ("PM") sein */
    uint16_t reserved1;                 /**< Reserviert */
    uint32_t map_entries;               /**< Anzahl Partitionseinträge */
    uint32_t pblock_start;              /**< Start-Block (physisch) */
    uint32_t pblock_count;              /**< Anzahl Blöcke (physisch) */
    char     name[UFT_APM_NAME_LENGTH]; /**< Partitionsname */
    char     type[UFT_APM_NAME_LENGTH]; /**< Partitionstyp */
    uint32_t lblock_start;              /**< Start-Block (logisch) */
    uint32_t lblock_count;              /**< Anzahl Blöcke (logisch) */
    uint32_t flags;                     /**< Status-Flags */
    uint32_t boot_block;                /**< Boot-Code Startblock */
    uint32_t boot_size;                 /**< Boot-Code Größe */
    uint32_t load_addr;                 /**< Ladeadresse (low) */
    uint32_t load_addr2;                /**< Ladeadresse (high) */
    uint32_t entry_addr;                /**< Einsprungadresse (low) */
    uint32_t entry_addr2;               /**< Einsprungadresse (high) */
    uint32_t checksum;                  /**< Boot-Code Checksum */
    char     processor[16];             /**< Prozessor-ID */
    uint8_t  boot_args[128];            /**< Boot-Argumente oder BZB */
    uint8_t  reserved3[248];            /**< Reserviert */
} uft_apm_dpme_t;

/** @brief Bekannte Partitionstypen */
#define UFT_APM_TYPE_PARTITION_MAP  "Apple_partition_map"
#define UFT_APM_TYPE_DRIVER         "Apple_Driver"
#define UFT_APM_TYPE_DRIVER43       "Apple_Driver43"
#define UFT_APM_TYPE_HFS            "Apple_HFS"
#define UFT_APM_TYPE_FREE           "Apple_Free"
#define UFT_APM_TYPE_SCRATCH        "Apple_Scratch"
#define UFT_APM_TYPE_UNIX_SVR2      "Apple_UNIX_SVR2"
#define UFT_APM_TYPE_PRODOS         "Apple_PRODOS"

/*============================================================================
 * HFS CLASSIC - MASTER DIRECTORY BLOCK
 *============================================================================*/

/** @brief HFS Signatur */
#define UFT_HFS_SIGNATURE           0x4244  /* "BD" big-endian */

/** @brief HFS+ Signatur */
#define UFT_HFSPLUS_SIGNATURE       0x482B  /* "H+" big-endian */

/** @brief HFSX Signatur */
#define UFT_HFSX_SIGNATURE          0x4858  /* "HX" big-endian */

/**
 * @brief HFS Classic Master Directory Block (MDB)
 *
 * Liegt bei Block 2 (Offset 0x400) eines HFS Volumes
 */
typedef struct  uft_hfs_mdb {
    uint16_t signature;             /**< 0x4244 für HFS */
    uint32_t creation_date;         /**< Erstellungsdatum (Mac-Zeit) */
    uint32_t modification_date;     /**< Änderungsdatum */
    uint16_t attributes;            /**< Volume-Attribute */
    uint16_t root_file_count;       /**< Dateien im Root */
    uint16_t bitmap_block;          /**< Block der Allocation Bitmap */
    uint16_t alloc_ptr;             /**< Start für nächste Allokation */
    uint16_t alloc_blocks;          /**< Anzahl Allocation Blocks */
    uint32_t alloc_block_size;      /**< Größe eines Allocation Blocks */
    uint32_t clump_size;            /**< Standard Clump-Größe */
    uint16_t extents_start;         /**< Extents File Startblock */
    uint32_t next_cnid;             /**< Nächste verfügbare CNID */
    uint16_t free_blocks;           /**< Freie Allocation Blocks */
    uint8_t  volume_name_len;       /**< Länge des Volume-Namens */
    char     volume_name[27];       /**< Volume-Name (Pascal String) */
    uint32_t backup_date;           /**< Backup-Datum */
    uint16_t backup_seq;            /**< Backup-Sequenznummer */
    uint32_t write_count;           /**< Schreibzähler */
    uint32_t extents_clump;         /**< Extents File Clump-Größe */
    uint32_t catalog_clump;         /**< Catalog File Clump-Größe */
    uint16_t root_dir_count;        /**< Verzeichnisse im Root */
    uint32_t file_count;            /**< Gesamtzahl Dateien */
    uint32_t folder_count;          /**< Gesamtzahl Ordner */
    uint8_t  finder_info[32];       /**< Finder Information */
    uint16_t embedded_sig;          /**< Signatur für eingebettetes Volume */
    uint8_t  embedded_extent[4];    /**< Extent des eingebetteten Volumes */
    uint32_t extents_file_size;     /**< Extents File Größe */
    uint8_t  extents_record[12];    /**< Extents File Extent Record */
    uint32_t catalog_file_size;     /**< Catalog File Größe */
    uint8_t  catalog_record[12];    /**< Catalog File Extent Record */
} uft_hfs_mdb_t;

/*============================================================================
 * HFS+ VOLUME HEADER
 *============================================================================*/

/**
 * @brief HFS+/HFSX Volume Header
 *
 * Liegt bei Block 2 (Offset 0x400) eines HFS+ Volumes
 */
typedef struct  uft_hfsplus_header {
    uint16_t signature;             /**< 0x482B (H+) oder 0x4858 (HX) */
    uint16_t version;               /**< Version (4 für HFS+, 5 für HFSX) */
    uint32_t attributes;            /**< Volume-Attribute */
    uint32_t last_mounted_version;  /**< Letzte Mount-Version */
    uint32_t journal_info_block;    /**< Journal Info Block (0 wenn kein Journal) */
    
    uint32_t creation_date;         /**< Erstellungsdatum */
    uint32_t modification_date;     /**< Änderungsdatum */
    uint32_t backup_date;           /**< Backup-Datum */
    uint32_t checked_date;          /**< Letzter fsck */
    
    uint32_t file_count;            /**< Anzahl Dateien */
    uint32_t folder_count;          /**< Anzahl Ordner */
    
    uint32_t block_size;            /**< Allocation Block Größe */
    uint32_t total_blocks;          /**< Gesamtzahl Blöcke */
    uint32_t free_blocks;           /**< Freie Blöcke */
    
    uint32_t next_allocation;       /**< Nächster freier Block (Hint) */
    uint32_t rsrc_clump_size;       /**< Resource Fork Clump-Größe */
    uint32_t data_clump_size;       /**< Data Fork Clump-Größe */
    uint32_t next_cnid;             /**< Nächste Catalog Node ID */
    uint32_t write_count;           /**< Schreibzähler */
    
    uint64_t encodings_bitmap;      /**< Verwendete Encodings */
    
    uint8_t  finder_info[32];       /**< Finder Information */
    
    /* Fork Descriptors (je 80 Bytes) */
    uint8_t  allocation_file[80];   /**< Allocation File Fork Descriptor */
    uint8_t  extents_file[80];      /**< Extents B-Tree Fork Descriptor */
    uint8_t  catalog_file[80];      /**< Catalog B-Tree Fork Descriptor */
    uint8_t  attributes_file[80];   /**< Attributes B-Tree Fork Descriptor */
    uint8_t  startup_file[80];      /**< Startup File Fork Descriptor */
} uft_hfsplus_header_t;

/** @brief HFS+ Volume Attributes */
typedef enum {
    UFT_HFSPLUS_ATTR_UNMOUNTED       = 0x00000100, /**< Sauber unmounted */
    UFT_HFSPLUS_ATTR_SPARED_BLOCKS   = 0x00000200, /**< Spare Blocks verwendet */
    UFT_HFSPLUS_ATTR_NOCACHE         = 0x00000400, /**< Kein Block-Cache */
    UFT_HFSPLUS_ATTR_INCONSISTENT    = 0x00000800, /**< Inkonsistent (dirty) */
    UFT_HFSPLUS_ATTR_CNIDS_REUSED    = 0x00001000, /**< CNIDs wurden wiederverwendet */
    UFT_HFSPLUS_ATTR_JOURNALED       = 0x00002000, /**< Journal aktiviert */
    UFT_HFSPLUS_ATTR_SOFTWARE_LOCK   = 0x00004000, /**< Software Lock */
} uft_hfsplus_attr_t;

/*============================================================================
 * HFS+ FORK DESCRIPTOR
 *============================================================================*/

/**
 * @brief HFS+ Fork Descriptor (80 Bytes)
 */
typedef struct  uft_hfsplus_fork {
    uint64_t logical_size;          /**< Logische Größe in Bytes */
    uint32_t clump_size;            /**< Clump-Größe */
    uint32_t total_blocks;          /**< Anzahl Allocation Blocks */
    /* 8 Extents à 8 Bytes */
    struct {
        uint32_t start_block;       /**< Start Allocation Block */
        uint32_t block_count;       /**< Anzahl Blöcke */
    } extents[8];
} uft_hfsplus_fork_t;

/*============================================================================
 * HFS B-TREE STRUKTUREN
 *============================================================================*/

/** @brief B-Tree Node Types */
typedef enum {
    UFT_BTREE_NODE_LEAF     = -1,   /**< Leaf Node */
    UFT_BTREE_NODE_INDEX    = 0,    /**< Index Node */
    UFT_BTREE_NODE_HEADER   = 1,    /**< Header Node */
    UFT_BTREE_NODE_MAP      = 2,    /**< Map Node */
} uft_btree_node_type_t;

/**
 * @brief B-Tree Node Descriptor (14 Bytes)
 */
typedef struct  uft_btree_node_desc {
    uint32_t forward_link;          /**< Nächster Node (0 wenn keiner) */
    uint32_t backward_link;         /**< Vorheriger Node (0 wenn keiner) */
    int8_t   kind;                  /**< Node-Typ */
    uint8_t  height;                /**< Höhe im Baum (Leaves = 1) */
    uint16_t num_records;           /**< Anzahl Records */
    uint16_t reserved;              /**< Reserviert */
} uft_btree_node_desc_t;

/**
 * @brief B-Tree Header Record (106 Bytes für HFS+)
 */
typedef struct  uft_btree_header {
    uint16_t tree_depth;            /**< Tiefe des Baums */
    uint32_t root_node;             /**< Root Node Nummer */
    uint32_t leaf_records;          /**< Anzahl Leaf Records */
    uint32_t first_leaf;            /**< Erster Leaf Node */
    uint32_t last_leaf;             /**< Letzter Leaf Node */
    uint16_t node_size;             /**< Node-Größe in Bytes */
    uint16_t max_key_length;        /**< Maximale Schlüssellänge */
    uint32_t total_nodes;           /**< Gesamtzahl Nodes */
    uint32_t free_nodes;            /**< Freie Nodes */
    uint16_t reserved1;             /**< Reserviert */
    uint32_t clump_size;            /**< Clump-Größe */
    uint8_t  btree_type;            /**< B-Tree Typ */
    uint8_t  key_compare_type;      /**< Key-Vergleichstyp */
    uint32_t attributes;            /**< B-Tree Attribute */
    uint8_t  reserved2[64];         /**< Reserviert */
} uft_btree_header_t;

/*============================================================================
 * HFS CATALOG RECORDS
 *============================================================================*/

/** @brief Catalog Record Types */
typedef enum {
    UFT_HFS_FOLDER_RECORD       = 0x0001,   /**< HFS Folder Record */
    UFT_HFS_FILE_RECORD         = 0x0002,   /**< HFS File Record */
    UFT_HFS_FOLDER_THREAD       = 0x0003,   /**< HFS Folder Thread */
    UFT_HFS_FILE_THREAD         = 0x0004,   /**< HFS File Thread */
    UFT_HFSPLUS_FOLDER_RECORD   = 0x0001,   /**< HFS+ Folder Record */
    UFT_HFSPLUS_FILE_RECORD     = 0x0002,   /**< HFS+ File Record */
    UFT_HFSPLUS_FOLDER_THREAD   = 0x0003,   /**< HFS+ Folder Thread */
    UFT_HFSPLUS_FILE_THREAD     = 0x0004,   /**< HFS+ File Thread */
} uft_catalog_record_type_t;

/**
 * @brief HFS+ Catalog Key
 */
typedef struct  uft_hfsplus_catalog_key {
    uint16_t key_length;            /**< Schlüssellänge */
    uint32_t parent_cnid;           /**< Parent Directory CNID */
    uint16_t name_length;           /**< Länge des Namens (in UTF-16 chars) */
    /* uint16_t name[] folgt */
} uft_hfsplus_catalog_key_t;

/**
 * @brief HFS+ Folder Record (88 Bytes)
 */
typedef struct  uft_hfsplus_folder_record {
    uint16_t record_type;           /**< 0x0001 */
    uint16_t flags;                 /**< Flags */
    uint32_t valence;               /**< Anzahl Einträge */
    uint32_t cnid;                  /**< Catalog Node ID */
    uint32_t create_date;           /**< Erstellungsdatum */
    uint32_t content_mod_date;      /**< Änderungsdatum (Inhalt) */
    uint32_t attribute_mod_date;    /**< Änderungsdatum (Attribute) */
    uint32_t access_date;           /**< Zugriffsdatum */
    uint32_t backup_date;           /**< Backup-Datum */
    /* BSD Permissions (16 Bytes) */
    uint32_t owner_id;              /**< User ID */
    uint32_t group_id;              /**< Group ID */
    uint8_t  admin_flags;           /**< Admin-Flags */
    uint8_t  owner_flags;           /**< Owner-Flags */
    uint16_t file_mode;             /**< UNIX File Mode */
    uint32_t special;               /**< inode oder link count */
    /* Finder Info (16 Bytes) */
    uint8_t  finder_info[16];       /**< Finder Information */
    /* Extended Finder Info (16 Bytes) */
    uint8_t  ext_finder_info[16];   /**< Erweiterte Finder Info */
    uint32_t text_encoding;         /**< Text Encoding Hint */
    uint32_t reserved;              /**< Reserviert */
} uft_hfsplus_folder_record_t;

/**
 * @brief HFS+ File Record (248 Bytes)
 */
typedef struct  uft_hfsplus_file_record {
    uint16_t record_type;           /**< 0x0002 */
    uint16_t flags;                 /**< Flags */
    uint32_t reserved1;             /**< Reserviert */
    uint32_t cnid;                  /**< Catalog Node ID */
    uint32_t create_date;           /**< Erstellungsdatum */
    uint32_t content_mod_date;      /**< Änderungsdatum (Inhalt) */
    uint32_t attribute_mod_date;    /**< Änderungsdatum (Attribute) */
    uint32_t access_date;           /**< Zugriffsdatum */
    uint32_t backup_date;           /**< Backup-Datum */
    /* BSD Permissions (16 Bytes) */
    uint32_t owner_id;              /**< User ID */
    uint32_t group_id;              /**< Group ID */
    uint8_t  admin_flags;           /**< Admin-Flags */
    uint8_t  owner_flags;           /**< Owner-Flags */
    uint16_t file_mode;             /**< UNIX File Mode */
    uint32_t special;               /**< Device oder Link */
    /* Finder Info (16 Bytes) */
    uint8_t  finder_info[16];       /**< Finder Information */
    /* Extended Finder Info (16 Bytes) */
    uint8_t  ext_finder_info[16];   /**< Erweiterte Finder Info */
    uint32_t text_encoding;         /**< Text Encoding Hint */
    uint32_t reserved2;             /**< Reserviert */
    /* Fork Descriptors (je 80 Bytes) */
    uft_hfsplus_fork_t data_fork;   /**< Data Fork */
    uft_hfsplus_fork_t rsrc_fork;   /**< Resource Fork */
} uft_hfsplus_file_record_t;

/*============================================================================
 * HFS COMPRESSION
 *============================================================================*/

/** @brief HFS Compression Methods */
typedef enum {
    UFT_HFS_COMPRESS_NONE       = 0,    /**< Nicht komprimiert */
    UFT_HFS_COMPRESS_DEFLATE    = 3,    /**< DEFLATE (zlib) */
    UFT_HFS_COMPRESS_RESOURCE   = 4,    /**< Komprimiert in Resource Fork */
    UFT_HFS_COMPRESS_LZVN       = 7,    /**< LZVN (schnell) */
    UFT_HFS_COMPRESS_UNCOMPRESSED = 8,  /**< Unkomprimiert im Attribut */
    UFT_HFS_COMPRESS_LZFSE      = 11,   /**< LZFSE (schnell, modern) */
    UFT_HFS_COMPRESS_LZBITMAP   = 12,   /**< LZBITMAP */
} uft_hfs_compress_method_t;

/**
 * @brief HFS Compression Header
 */
typedef struct  uft_hfs_compress_header {
    uint32_t magic;                 /**< 'fpmc' (0x66706D63) */
    uint32_t compression_type;      /**< Kompressionstyp */
    uint64_t uncompressed_size;     /**< Unkomprimierte Größe */
} uft_hfs_compress_header_t;

#define UFT_HFS_COMPRESS_MAGIC  0x66706D63  /* 'fpmc' */

/*============================================================================
 * HFS TIMESTAMPS
 *============================================================================*/

/**
 * @brief HFS Timestamp zu Unix Timestamp konvertieren
 *
 * HFS verwendet Sekunden seit 1. Januar 1904 (Mac Epoch)
 * Unix verwendet Sekunden seit 1. Januar 1970
 */
static inline int64_t uft_hfs_to_unix_time(uint32_t hfs_time) {
    /* Differenz in Sekunden: 1904-01-01 bis 1970-01-01 */
    const int64_t HFS_EPOCH_DIFF = 2082844800LL;
    return (int64_t)hfs_time - HFS_EPOCH_DIFF;
}

/**
 * @brief Unix Timestamp zu HFS Timestamp konvertieren
 */
static inline uint32_t uft_unix_to_hfs_time(int64_t unix_time) {
    const int64_t HFS_EPOCH_DIFF = 2082844800LL;
    return (uint32_t)(unix_time + HFS_EPOCH_DIFF);
}

/*============================================================================
 * API FUNKTIONEN
 *============================================================================*/

/**
 * @brief Prüft ob Daten eine APM enthalten
 */
bool uft_apm_detect(const uint8_t* data, size_t size);

/**
 * @brief Liest Block 0 der APM
 */
bool uft_apm_read_block0(const uint8_t* data, size_t size,
                         uft_apm_block0_t* block0);

/**
 * @brief Liest einen Partition Map Entry
 */
bool uft_apm_read_entry(const uint8_t* data, size_t size,
                        int index, uft_apm_dpme_t* entry);

/**
 * @brief Findet eine Partition nach Typ
 */
int uft_apm_find_partition(const uint8_t* data, size_t size,
                           const char* type);

/**
 * @brief Prüft ob Daten ein HFS Volume sind
 */
bool uft_hfs_detect(const uint8_t* data, size_t size,
                    bool* is_hfs_classic, bool* is_hfsplus);

/**
 * @brief Liest HFS Classic MDB
 */
bool uft_hfs_read_mdb(const uint8_t* data, size_t size,
                      uft_hfs_mdb_t* mdb);

/**
 * @brief Liest HFS+ Volume Header
 */
bool uft_hfsplus_read_header(const uint8_t* data, size_t size,
                             uft_hfsplus_header_t* header);

/**
 * @brief Dekomprimiert HFS-komprimierte Daten
 */
int uft_hfs_decompress(const uint8_t* compressed, size_t compressed_size,
                       int method, uint8_t* output, size_t* output_size);

/**
 * @brief Validiert B-Tree Header
 */
bool uft_btree_validate_header(const uft_btree_header_t* header);

/**
 * @brief Byteswap für Big-Endian HFS Strukturen auf Little-Endian Host
 */
void uft_hfs_swap_mdb(uft_hfs_mdb_t* mdb);
void uft_hfsplus_swap_header(uft_hfsplus_header_t* header);
void uft_apm_swap_block0(uft_apm_block0_t* block0);
void uft_apm_swap_dpme(uft_apm_dpme_t* dpme);

/*============================================================================
 * SPEZIELLE CNIDs
 *============================================================================*/

#define UFT_HFS_CNID_ROOT_PARENT    1   /**< Parent des Root */
#define UFT_HFS_CNID_ROOT_FOLDER    2   /**< Root Folder */
#define UFT_HFS_CNID_EXTENTS_FILE   3   /**< Extents B-Tree */
#define UFT_HFS_CNID_CATALOG_FILE   4   /**< Catalog B-Tree */
#define UFT_HFS_CNID_BAD_BLOCKS     5   /**< Bad Blocks File */
#define UFT_HFS_CNID_ALLOC_FILE     6   /**< Allocation File */
#define UFT_HFS_CNID_STARTUP_FILE   7   /**< Startup File */
#define UFT_HFS_CNID_ATTRIBUTES     8   /**< Attributes B-Tree */
#define UFT_HFS_CNID_REPAIR_CATALOG 14  /**< Temp für Reparatur */
#define UFT_HFS_CNID_BOGUS_EXTENT   15  /**< Bogus Extent */
#define UFT_HFS_CNID_FIRST_USER     16  /**< Erste User CNID */

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFS_H */
