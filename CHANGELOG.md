# Changelog

## [4.1.0] - 2026-02-08

### Fixed – Hardware Protocol Audit
- **SuperCard Pro**: Complete rewrite from SDK v1.7 reference
  - Replaced fabricated ASCII protocol with correct binary commands (0x80-0xD2)
  - Fixed checksum (init 0x4A), endianness (big-endian), USB interface (FTDI FT240-X)
  - Implemented proper RAM model (512K SRAM, SENDRAM_USB/LOADRAM_USB)
  - Corrected flux read flow: READFLUX→GETFLUXINFO→SENDRAM_USB
  - Corrected flux write flow: LOADRAM_USB→WRITEFLUX
- **FC5025**: Complete rewrite from driver source v1309
  - Replaced fabricated opcodes with SCSI-like CBW/CSW protocol (sig 'CFBC')
  - Correct opcodes: SEEK=0xC0, FLAGS=0xC2, READ_FLEXIBLE=0xC6, READ_ID=0xC7
  - Device correctly marked as read-only (no write support in hardware)
  - Density control via FLAGS bit 2
- **KryoFlux**: Replaced invented USB commands with DTC subprocess wrapper
  - Proprietary USB protocol acknowledged, official tool used instead
  - Stream format parser (OOB 0x08-0x0D) verified and retained
- **Pauline**: Replaced fabricated binary protocol (0x01-0x90) with HTTP/SSH
  - Matches real architecture: embedded Linux web server on DE10-nano FPGA
  - SSH commands for drive control, HTTP for data transfer
- **Greaseweazle**: 16 protocol bugs fixed against gw_protocol.h v1.23
  - Fixed USB command codes, bandwidth negotiation, SET_BUS_TYPE
  - Corrected checksum and packet framing

### Added
- GitHub Actions CI/CD release workflow (macOS/Linux/Windows)
- Linux .deb packaging with desktop integration
- macOS .dmg with universal binary support
- CMakeLists.txt for cmake-based builds alongside qmake
- Linux .desktop entry and udev rules for floppy devices

### Changed
- Version bump: 4.0.0 → 4.1.0
- Cleaned up development-only files from distribution

## [4.1.6] - 2026-01-16

### Added
- **Track Alignment Module** (nibtools-compatible):
  - Essential for mastering protected C64/1541 disks
  - Based on nibtools by Pete Rittwage (c64preservation.com)
  - **V-MAX! alignment:**
    - `align_vmax()`: Standard V-MAX! marker detection (0x4B, 0x69, 0x49, 0x5A, 0xA5)
    - `align_vmax_new()`: Improved algorithm finding longest marker run
    - `align_vmax_cinemaware()`: Cinemaware variant (0x64 0xA5 0xA5 0xA5)
  - **Pirate Slayer alignment:**
    - `align_pirateslayer()`: Signature detection with bit-shifting (D7 D7 EB CC AD)
    - Supports versions 1 and 2
  - **RapidLok alignment:**
    - `align_rapidlok()`: Full track header detection (0x75 sector IDs, 0x7B extra sectors)
    - Version detection (RL1-RL7)
    - PAL/NTSC detection
  - **Generic alignment:**
    - `align_long_sync()`: Align to longest sync mark
    - `align_auto_gap()`: Align to longest gap run
    - `align_bad_gcr()`: Align to bad GCR (mastering artifacts)
    - `align_sector0()`: Align to sector 0 header
  - **Bitshift repair** (Kryoflux/SCP support):
    - `is_track_bitshifted()`: Detect bitshifted tracks
    - `sync_align_track()`: Sync-align bitshifted tracks
    - `align_bitshifted_track()`: Full Kryoflux G64 repair
    - `shift_buffer_left/right()`: Bit-level buffer manipulation
  - **Fat track detection:**
    - `search_fat_tracks()`: Auto-detect wide tracks spanning halftracks
    - `check_fat_track()`: Compare adjacent tracks for similarity
  - **Track comparison:**
    - `compare_tracks()`: Byte-level track comparison
    - `compare_sectors()`: Sector-level comparison with disk ID
  - **GCR helpers:**
    - `find_sync()`: Find sync mark in GCR data
    - `is_bad_gcr()`: Bad GCR byte detection
    - `fix_first_gcr()/fix_last_gcr()`: GCR repair
  - **Utilities:**
    - `get_track_capacity()`: Capacity for density zones
    - `get_sectors_per_track()`: 1541 sector counts
    - `get_track_density()`: Track-to-density mapping
  - Location: `include/uft/protection/uft_track_align.h`, `src/protection/c64/uft_track_align.c`
  - Tests: 27/27 passing

- **NIB/NB2/NBZ Format Support** (nibtools-compatible):
  - Complete implementation of nibtools disk image formats
  - **NIB Format** (MNIB-1541-RAW):
    - Version 3 format with 256-byte header
    - Track/density entries at offset 0x10
    - 8KB per track data storage
    - Halftrack support flag
    - Load, save, create, and modify operations
  - **NB2 Format** (Multi-Pass NIB):
    - 16 passes per track (4 densities × 4 passes)
    - Automatic best-pass selection based on error count
    - Access to all pass data for analysis
    - Track info with error statistics
  - **NBZ Format** (Compressed NIB):
    - LZ77 compression (Marcus Geelnard algorithm)
    - Fast compression with jump table optimization
    - Typical 2-4x compression ratio
    - Transparent load/save operations
  - **LZ77 Compression Engine:**
    - `lz77_compress()`: Standard compression
    - `lz77_compress_fast()`: Jump table optimized
    - `lz77_decompress()`: Decompression
    - Variable-length encoding for offsets/lengths
    - Marker byte optimization for best compression
  - **Format Detection:**
    - `nib_detect_format()`: File-based detection
    - `nib_detect_format_buffer()`: Buffer-based detection
    - Supports NIB, NB2, NBZ, and G64 identification
  - **Analysis:**
    - `nib_analyze()`: Complete format analysis
    - Track count, halftrack presence, disk ID extraction
    - Compression ratio for NBZ files
    - Report generation
  - Location: `include/uft/formats/c64/uft_nib_format.h`, `src/formats/c64/uft_nib_format.c`
  - Tests: 18/18 passing

- **GCR Operations Module** (nibtools-compatible):
  - Complete GCR encoding/decoding and track manipulation
  - **GCR Encode/Decode:**
    - `gcr_encode()`: Nibble-to-GCR encoding (4 bytes → 5 GCR bytes)
    - `gcr_decode()`: GCR-to-nibble decoding with error detection
    - `gcr_check_errors()`: Count bad GCR bytes in track
    - Standard encode/decode tables for C64/1541
  - **Sync Operations:**
    - `gcr_find_sync()`: Locate sync marks in track data
    - `gcr_find_sync_end()`: Find end of sync run
    - `gcr_count_syncs()`: Count total syncs in track
    - `gcr_longest_sync()`: Find longest sync mark
    - `gcr_lengthen_syncs()`: Extend syncs to minimum length
    - `gcr_kill_partial_syncs()`: Remove short/invalid syncs
  - **Gap Operations:**
    - `gcr_find_gap()`: Locate gap marks
    - `gcr_longest_gap()`: Find longest gap run
    - `gcr_strip_runs()`: Strip sync/gap runs to minimum
    - `gcr_reduce_runs()`: Reduce oversized runs
    - `gcr_reduce_gaps()`: Minimize inter-sector gaps
  - **Track Cycle Detection:**
    - `gcr_detect_cycle()`: Find repeating track pattern
    - `gcr_extract_cycle()`: Extract single rotation data
    - Quality scoring for cycle matches
  - **Track Comparison:**
    - `gcr_compare_tracks()`: Byte-level track comparison
    - `gcr_compare_sectors()`: Sector-level comparison
    - Similarity percentage calculation
    - Format compatibility checking
  - **Track Verification:**
    - `gcr_verify_track()`: Full track verification
    - `gcr_extract_sector()`: Extract decoded sector data
    - Header/data checksum validation
    - Per-sector status reporting
  - **Track Utilities:**
    - `gcr_is_empty_track()`: Detect unformatted tracks
    - `gcr_is_killer_track()`: Detect all-sync tracks
    - `gcr_detect_density()`: Auto-detect track density
    - `gcr_expected_capacity()`: Get expected track size
    - `gcr_sectors_per_track()`: Get sector count for track
  - **Checksums:**
    - `gcr_calc_data_checksum()`: XOR checksum for sector data
    - `gcr_calc_header_checksum()`: Header checksum calculation
    - `gcr_crc_track()`: CRC32 for track verification
    - `gcr_crc_directory()`: CRC32 for directory track
  - Location: `include/uft/formats/c64/uft_gcr_ops.h`, `src/formats/c64/uft_gcr_ops.c`
  - Tests: 27/27 passing

- **D64/G64 Format Conversion** (nibtools-compatible):
  - Complete D64 ↔ G64 bidirectional conversion
  - **D64 Format:**
    - Load/save 35-track and 40-track images
    - Error info support (683/768 byte extension)
    - BAM parsing for disk ID, name, DOS type
    - Sector read/write with error codes
    - Block offset calculation
  - **G64 Format:**
    - GCR-1541 format with track offsets
    - Extended SPS G64 detection
    - Track data with length and speed zone
    - Halftrack support
  - **GCR Sector Conversion:**
    - `sector_to_gcr()`: Raw sector → GCR encoding
    - `gcr_to_sector()`: GCR decoding with error detection
    - `build_gcr_track()`: Build complete GCR track
    - `extract_gcr_track()`: Extract sectors from GCR
    - Header/data checksum calculation
  - **Conversion Functions:**
    - `d64_to_g64()`: D64 → G64 conversion
    - `g64_to_d64()`: G64 → D64 conversion
    - Configurable options (halftracks, errors, gaps)
    - Conversion result reporting
  - **Utilities:**
    - `d64_speed_zone()`: Track → speed zone
    - `d64_gap_length()`: Track → gap length
    - `d64_track_capacity()`: Track → capacity
    - `d64_error_name()`: Error code names
  - Location: `include/uft/formats/c64/uft_d64_g64.h`, `src/formats/c64/uft_d64_g64.c`
  - Tests: 19/19 passing

- **Index Hole Sensor (IHS) Support** (SuperCard+ compatible):
  - Hardware abstraction for precise track alignment
  - **IHS Control:**
    - `ihs_check_presence()`: Detect IHS hardware
    - `ihs_enable()` / `ihs_disable()`: IHS on/off
    - Session management
  - **Track Reading:**
    - `ihs_read_track()`: Read with IHS alignment
    - `ihs_scan_density()`: Auto-detect density
  - **Track Analysis:**
    - `ihs_analyze_track()`: Single track analysis
    - `ihs_alignment_report()`: Full disk report
    - `ihs_deep_bitrate_analysis()`: Multi-pass density
  - **Software Emulation:**
    - `ihs_analyze_buffer()`: Analyze existing track data
    - `ihs_find_index_position()`: Estimate index hole
    - `ihs_rotate_to_index()`: Rotate track data
  - **Interface Backends:**
    - Abstract interface for hardware drivers
    - Null interface for testing
    - Simulation interface for offline analysis
  - Location: `include/uft/hardware/uft_ihs.h`, `src/hardware/uft_ihs.c`

- **Track Writer Module** (nibtools-compatible Disk Mastering):
  - Complete disk mastering support for 1541/1571 drives
  - **Session Management:**
    - `writer_create_session()`: Create writer session
    - `writer_close_session()`: Close session
    - `writer_get_defaults()`: Get default options
    - `writer_create_null_session()`: Create test session
  - **Motor Calibration:**
    - `writer_calibrate()`: Calibrate motor speed
    - `writer_get_capacity()`: Get track capacity at density
    - `writer_speed_valid()`: Validate RPM range (280-320)
    - `writer_calc_rpm()`: Calculate RPM from capacity
  - **Track Writing:**
    - `writer_write_track()`: Write single track
    - `writer_fill_track()`: Fill track with pattern
    - `writer_kill_track()`: Create killer track (all sync)
    - `writer_erase_track()`: Erase track
  - **Track Processing:**
    - `writer_check_sync_flags()`: Detect sync/killer flags
    - `writer_check_formatted()`: Check if track formatted
    - `writer_compress_track()`: Compress track for writing
    - `writer_lengthen_sync()`: Extend short sync marks
    - `writer_replace_bytes()`: Replace bytes in buffer
    - `writer_prepare_track()`: Full track preparation
  - **Disk Mastering:**
    - `writer_master_disk()`: Master entire disk with progress
    - `writer_unformat_disk()`: Wipe disk
    - `writer_init_aligned()`: Initialize aligned disk
    - `writer_verify_track()`: Verify written track
  - **Image Management:**
    - `writer_create_image()`: Create master image
    - `writer_free_image()`: Free image
    - Support for NIB and G64 input files
  - **Write Options:**
    - Verify after write
    - Backwards writing
    - IHS support
    - Track alignment/skew
    - Fat track handling
    - Custom fill bytes
  - Location: `include/uft/hardware/uft_track_writer.h`, `src/hardware/uft_track_writer.c`
  - Tests: 27/27 passing

- **Extended C64 Protection Detection**:
  - **TimeWarp Detection:** Versions 1-3 signature detection
  - **Densitron Detection:** Density gradient pattern detection
  - **Kracker Jax Detection:** Signature and volume/issue extraction
  - **Generic Detection API:** Unified protection scanner
  - **Track Analysis:** Fat track, custom sync, gap analysis
  - **Publisher Patterns:** Ocean, US Gold, Mastertronic, Codemasters, Activision, Epyx
  - Location: `include/uft/protection/uft_c64_protection_ext.h`, `src/protection/c64/uft_c64_protection_ext.c`
  - Tests: 19/19 passing

- **Flux Timing Analysis Module**:
  - **Transition Management:** Create, add, load flux data
  - **Sample Rates:** Kryoflux (24MHz), SCP (40MHz), Greaseweazle (80MHz)
  - **Histogram Analysis:** Generate timing histogram, find peaks
  - **Encoding Detection:** Automatic FM/MFM/GCR detection
  - **Revolution Analysis:** Index mark detection, RPM calculation
  - **Speed Analysis:** Mean RPM, variation percentage
  - **Track Analysis:** Cell statistics, jitter, signal quality
  - **Weak Bit Detection:** Find weak/fuzzy regions
  - **No-Flux Detection:** Find gaps in flux data
  - **Protection Detection:** Long track, weak bits, density changes
  - **Disk Analysis:** Full disk report generation
  - Location: `include/uft/flux/uft_flux_analysis.h`, `src/flux/uft_flux_analysis.c`
  - Tests: 25/25 passing

- **BAM Editor for D64 Images**:
  - **D64 Creation:** Create empty 35/40 track images
  - **Disk Info:** Get/set disk name, ID, DOS type
  - **Block Allocation:** Check/allocate/free blocks
  - **Track Management:** Free sector counting, next free allocation
  - **Directory Operations:** File count, info, find, delete, rename, lock
  - **Validation:** BAM integrity check, cross-link detection
  - **Repair:** Automatic BAM reconstruction from directory
  - **Sector I/O:** Direct sector read/write
  - **PETSCII Conversion:** ASCII↔PETSCII utilities
  - **Formatting:** Initialize new disk images
  - Location: `include/uft/formats/c64/uft_bam_editor.h`, `src/formats/c64/uft_bam_editor.c`
  - Tests: 21/21 passing

- **D64 File Operations**:
  - **File Extraction:** Extract PRG/SEQ/USR/REL files from D64
  - **File Insertion:** Insert files with automatic BAM allocation
  - **PRG Support:** Load address handling, header extraction
  - **File Chain:** Follow/validate sector chains
  - **Multi-file Support:** Extract all, insert multiple files
  - **Utilities:** Filename conversion, block calculation
  - Location: `include/uft/formats/c64/uft_d64_file.h`, `src/formats/c64/uft_d64_file.c`
  - Tests: 15/15 passing

- **T64 Tape Image Format**:
  - **Format Detection:** Validate T64 magic and structure
  - **Image Management:** Open, create, save, close T64 files
  - **Directory Operations:** File listing, search, info
  - **File Extraction:** Extract single, by index, or all files
  - **T64 Creation:** Create new images, add files
  - **File Management:** Add PRG files, remove files
  - **PETSCII Support:** ASCII/PETSCII conversion
  - Location: `include/uft/formats/c64/uft_t64.h`, `src/formats/c64/uft_t64.c`
  - Tests: 18/18 passing

- **TAP Raw Tape Format**:
  - **Format Detection:** TAP magic validation, version detection
  - **Image Management:** Open, create, save, close TAP files
  - **Pulse Operations:** Read cycles, classify (short/medium/long/pause)
  - **Overflow Support:** TAP v1+ long pulse encoding
  - **Analysis:** Statistics, pilot detection, block analysis
  - **TAP Creation:** Add pulses, pilot tone, sync, data bytes
  - **Timing Utilities:** CPU cycles ↔ microseconds conversion
  - Location: `include/uft/formats/c64/uft_tap.h`, `src/formats/c64/uft_tap.c`
  - Tests: 19/19 passing

- **1571/1581 Extended Drive Formats (D71/D81)**:
  - **D71 (1571 Double-Sided):** 70 tracks, 349KB, dual BAM
  - **D81 (1581 3.5" HD):** 80 tracks, 800KB, 40 sectors/track
  - **Image Creation:** Create formatted D71/D81 images
  - **BAM Management:** Block allocation/free, free block counting
  - **Sector I/O:** Read/write sectors on all tracks
  - **Disk Info:** Name, ID, DOS version, block counts
  - **Conversion:** D64↔D71 conversion utilities
  - **Detection:** Auto-detect D64/D71/D81 format
  - Location: `include/uft/formats/c64/uft_d71_d81.h`, `src/formats/c64/uft_d71_d81.c`
  - Tests: 18/18 passing

- **CRT Cartridge Format**:
  - **Format Detection:** CRT magic, version validation
  - **Image Management:** Open, create, save, close CRT files
  - **50+ Cartridge Types:** Normal, Action Replay, Ocean, EasyFlash, etc.
  - **CHIP Packets:** Parse, extract, add ROM data
  - **ROM Extraction:** Extract all ROM data from cartridge
  - **CRT Creation:** Create 8K/16K cartridges from ROM
  - Location: `include/uft/formats/c64/uft_crt.h`, `src/formats/c64/uft_crt.c`
  - Tests: 15/15 passing

- **REU/GeoRAM Support**:
  - **REU Types:** 1700 (128K), 1764 (256K), 1750 (512K), up to 16MB
  - **GeoRAM:** 512K-4MB with block/page emulation
  - **Memory Access:** Read/write byte, block, page operations
  - **GeoRAM Emulation:** Block/page register handling
  - **Utilities:** Fill, clear, compare, dump
  - Location: `include/uft/formats/c64/uft_reu.h`, `src/formats/c64/uft_reu.c`
  - Tests: 17/17 passing

- **SID Music Format (PSID/RSID)**:
  - **Format Detection:** PSID/RSID magic, version validation
  - **Metadata Parsing:** Name, author, release, addresses
  - **Song Info:** Multi-song support, speed flags (CIA/VBI)
  - **Data Extraction:** C64 program data, PRG export
  - **SID Creation:** Create from scratch, from PRG
  - **Clock/Model:** PAL/NTSC, 6581/8580 flags
  - **Multi-SID:** Second/third SID address decoding
  - Location: `include/uft/formats/c64/uft_sid.h`, `src/formats/c64/uft_sid.c`
  - Tests: 18/18 passing

- **GEOS Filesystem Support**:
  - **Info Block:** Parse/write GEOS file metadata
  - **Icon Handling:** 24x21 pixel icons, default icons
  - **VLIR Support:** Variable Length Index Record parsing
  - **File Operations:** Create SEQ/VLIR files
  - **CVT Format:** Convert File format support
  - **Timestamps:** GEOS date/time formatting
  - Location: `include/uft/formats/c64/uft_geos.h`, `src/formats/c64/uft_geos.c`
  - Tests: 16/16 passing

- **P00/S00/U00/R00 PC64 Format**:
  - **Detection:** Magic validation, type from extension
  - **File Operations:** Open, create, save P00 files
  - **PRG Support:** Load address extraction, PRG creation
  - **PETSCII Conversion:** PETSCII ↔ ASCII filename conversion
  - **All Types:** PRG (P00), SEQ (S00), USR (U00), REL (R00), DEL (D00)
  - Location: `include/uft/formats/c64/uft_p00.h`, `src/formats/c64/uft_p00.c`
  - Tests: 17/17 passing

- **VICE Snapshot Format (VSF)**:
  - **Format Detection:** VICE magic, version validation
  - **Machine Support:** C64, C128, VIC-20, Plus/4, PET, CBM-II
  - **Module Parsing:** MAINCPU, VIC-II, SID, CIA1/2, etc.
  - **State Extraction:** CPU registers, memory contents
  - **Module Search:** Find modules by name
  - Location: `include/uft/formats/c64/uft_vsf.h`, `src/formats/c64/uft_vsf.c`
  - Tests: 13/13 passing

- **CMD FD2000/FD4000 Disk Format (D1M/D2M/D4M)**:
  - **Image Types:** D1M (1581 emulation), D2M (FD2000), D4M (FD4000)
  - **Sector Operations:** Read/write 256-byte sectors
  - **BAM Management:** Block allocation, free count tracking
  - **Directory Support:** List/find files, formatted output
  - **Disk Formatting:** Create new formatted CMD disk images
  - Location: `include/uft/formats/c64/uft_cmd.h`, `src/formats/c64/uft_cmd.c`
  - Tests: 17/17 passing

- **Nintendo Switch Container Formats (XCI/NSP)**:
  - **XCI Support:** Game Card Image parsing, partition detection
  - **NSP/PFS0 Support:** Nintendo Submission Package file listing
  - **NCA Detection:** Nintendo Content Archive identification
  - **File Extraction:** List and extract files from containers
  - **Cartridge Info:** Size detection (1GB-32GB), partition enumeration
  - Based on hactool by SciresM
  - Location: `include/uft/formats/nintendo/uft_switch.h`, `src/formats/nintendo/uft_switch.c`
  - Tests: 12/12 passing

- **C64 Freezer Snapshot Format (.FRZ, .AR)**:
  - **Freezer Types:** Action Replay MK4/5/6, Final Cartridge III, Super Snapshot, Retro Replay
  - **State Extraction:** CPU (A/X/Y/SP/PC/Status), VIC-II, SID, CIA1/2
  - **Memory Access:** 64KB RAM, 1KB Color RAM, peek function
  - **PRG Export:** Extract memory range as PRG file
  - **VSF Conversion:** Convert to VICE snapshot format
  - Location: `include/uft/formats/c64/uft_frz.h`, `src/formats/c64/uft_frz.c`
  - Tests: 10/10 passing

- **C64 ROM Image Format (BASIC/KERNAL/CHAR)**:
  - **ROM Types:** BASIC (8KB), KERNAL (8KB), Character (4KB), Combined, Full
  - **Version Detection:** Original, JiffyDOS, Dolphin DOS, SpeedDOS, EXOS
  - **KERNAL Analysis:** Hardware vectors (NMI/RESET/IRQ), I/O routines
  - **ROM Operations:** Extract, combine, patch, CRC32 calculation
  - Location: `include/uft/formats/c64/uft_c64rom.h`, `src/formats/c64/uft_c64rom.c`
  - Tests: 15/15 passing

- **PlayStation 1 Disc Image Format (BIN/CUE, ISO)**:
  - **Image Types:** BIN (2352-byte raw), ISO (2048-byte), IMG, MDF
  - **Sector Access:** Mode 1, Mode 2 Form 1/2, CD-DA Audio
  - **Game Detection:** SYSTEM.CNF parsing, game ID extraction
  - **Region Detection:** NTSC-J (Japan), NTSC-U (USA), PAL (Europe)
  - **CUE Sheet:** Multi-track parsing, MSF/LBA conversion
  - Location: `include/uft/formats/sony/uft_ps1.h`, `src/formats/sony/uft_ps1.c`
  - Tests: 16/16 passing

- **Nintendo 3DS Container Formats (CCI/CIA/NCCH)**:
  - **CCI/3DS:** CTR Cart Image (physical cartridge dump)
  - **CIA:** CTR Importable Archive (installable content)
  - **NCCH:** Nintendo Content Container Header parsing
  - **ExeFS:** Executable filesystem file listing and extraction
  - **RomFS:** Read-Only filesystem detection
  - **Encryption:** Detection of encrypted vs. decrypted content
  - Location: `include/uft/formats/nintendo/uft_3ds.h`, `src/formats/nintendo/uft_3ds.c`
  - Tests: 11/11 passing

- **Sega Genesis/Mega Drive ROM Format**:
  - **Formats:** BIN (raw), SMD (Super Magic Drive interleaved), MD
  - **Header Parsing:** Console name, titles, serial, region codes
  - **Checksum:** Calculate, verify, and fix ROM checksums
  - **Region Detection:** Japan, USA, Europe, World
  - **SMD Conversion:** Deinterleave SMD to binary, interleave binary to SMD
  - **Save Detection:** SRAM, EEPROM detection from header
  - Location: `include/uft/formats/sega/uft_genesis.h`, `src/formats/sega/uft_genesis.c`
  - Tests: 16/16 passing

- **Nintendo Game Boy / Game Boy Advance ROM Format**:
  - **GB/GBC:** DMG, Game Boy Color, Super Game Boy support
  - **GBA:** Game Boy Advance ROM detection and parsing
  - **MBC Detection:** MBC1-7, MMM01, HuC1/3, Pocket Camera, etc.
  - **Checksum:** Header checksum verification and fix
  - **Save Detection:** Battery backup, RTC, Rumble detection
  - **GBA Save Types:** EEPROM, SRAM, Flash detection from ROM
  - Location: `include/uft/formats/nintendo/uft_gameboy.h`, `src/formats/nintendo/uft_gameboy.c`
  - Tests: 14/14 passing

- **Nintendo 64 ROM Format (z64/v64/n64)**:
  - **Format Conversion:** Auto-convert v64/n64 to z64 (big-endian)
  - **Header Parsing:** Title, game code, region, version
  - **CRC:** Calculate, verify, and fix CRC1/CRC2 checksums
  - **CIC Detection:** 6101, 6102, 6103, 6105, 6106 chips
  - **Region Support:** NTSC-U, NTSC-J, PAL, Gateway
  - Location: `include/uft/formats/nintendo/uft_n64.h`, `src/formats/nintendo/uft_n64.c`
  - Tests: 11/11 passing

- **Atari 2600/7800/Lynx ROM Format**:
  - **Atari 2600:** Bankswitching detection (F8, F6, F4, E0, E7, 3F, etc.)
  - **Atari 7800:** A78 header parsing, POKEY detection, controller types
  - **Atari Lynx:** LNX header parsing, rotation support
  - **Features:** ROM size detection, save type detection
  - Location: `include/uft/formats/atari/uft_atari.h`, `src/formats/atari/uft_atari.c`
  - Tests: 14/14 passing

- **NEC TurboGrafx-16 / PC Engine ROM Format**:
  - **Consoles:** PC Engine, TurboGrafx-16, SuperGrafx
  - **Header Detection:** 512-byte header detection and stripping
  - **Mapper Detection:** Standard, SF2, Populous, Tennokoe Bank
  - **Region Detection:** Japan (PCE) vs USA (TG16)
  - Location: `include/uft/formats/nec/uft_pce.h`, `src/formats/nec/uft_pce.c`
  - Tests: 11/11 passing

- **SNK Neo Geo ROM Format**:
  - **Systems:** AES (home), MVS (arcade)
  - **ROM Types:** P-ROM (program), C-ROM (character), S-ROM (fix), M-ROM (Z80), V-ROM (samples)
  - **Header Parsing:** NGH number, name, year, genre
  - **Interleaving:** Automatic C-ROM interleave detection
  - Location: `include/uft/formats/snk/uft_neogeo.h`, `src/formats/snk/uft_neogeo.c`
  - Tests: 9/9 passing

- **Sega Master System / Game Gear ROM Format**:
  - **Consoles:** SMS, Game Gear, SG-1000, SC-3000
  - **Header:** TMR SEGA signature detection at multiple offsets
  - **Mapper Detection:** Sega, Codemasters, Korean, MSX, Nemesis, Janggun
  - **Checksum:** Calculate, verify, and fix ROM checksums
  - **Region Detection:** SMS Japan/Export, GG Japan/Export/International
  - Location: `include/uft/formats/sega/uft_sms.h`, `src/formats/sega/uft_sms.c`
  - Tests: 14/14 passing

- **Amiga ADF Disk Image Format**:
  - **Format:** Amiga Disk File (880KB standard floppy)
  - **Filesystem:** Amiga OFS/FFS support
  - **Operations:** Read/write sectors, file operations
  - Tests: 17/17 passing

- **WOZ Disk Image Format** (Apple II):
  - WOZ 1.0/2.0/2.1 support for bit-accurate Apple II disk preservation
  - Full chunk parsing: INFO, TMAP, TRKS, META, WRIT, FLUX
  - Quarter-track mapping for copy protection support
  - Flux timing data support (125ns resolution, WOZ 2.1)
  - MC3470 fake bit simulation for weak/random bits
  - Spiradisc protection detection
  - Cross-track synchronization analysis
  - Metadata parsing (title, publisher, developer, etc.)
  - CRC32 integrity verification
  - API: `woz_load()`, `woz_get_track_525()`, `woz_get_flux()`, `woz_detect_spiradisc()`
  - References: applesaucefdc.com/woz/reference2/

- **Apple Disk Copy / NDIF Format** (Macintosh):
  - Disk Copy 4.2 format (.img, .image) - 84-byte header, raw sectors
  - NDIF (Disk Copy 6.x) format - resource fork metadata, ADC compression
  - Self-Mounting Image (SMI) detection and extraction
  - MacBinary II/III wrapper detection and unwrapping
  - Supported disk formats:
    - Mac 400K GCR (single-sided)
    - Mac 800K GCR (double-sided)
    - PC/Mac 720K MFM
    - PC/Mac 1.44MB HD MFM
  - Checksum calculation and verification (rotate-right-1 algorithm)
  - ADC (Apple Data Compression) decompression
  - Image creation with automatic checksum
  - Detailed analysis report generation
  - API: `dc_analyze()`, `dc_detect_format()`, `dc_extract_disk_data()`, 
         `dc42_create_image()`, `macbinary_unwrap()`, `smi_extract_image()`
  - Reference: Disk Copy 6.3.3 SMI analysis

- **FAT Boot Sector Analysis Module** (based on OpenGate.at article):
  - Complete BPB (BIOS Parameter Block) parsing and validation
  - Media Descriptor Byte identification (0xF0-0xFF)
  - Boot signature verification (0x55AA at offset 510)
  - Extended BPB (EBPB) support with volume label/serial
  - Standard disk geometry definitions:
    - 5.25" 160KB, 180KB, 320KB, 360KB, 1.2MB
    - 3.5" 720KB, 1.44MB, 2.88MB
    - 8" formats (250KB, 500KB, 1.2MB)
  - FAT type determination (FAT12/FAT16/FAT32)
  - Boot sector creation for standard formats
  - Detailed analysis report generation
  - API: `fat_analyze_boot_sector()`, `fat_create_boot_sector()`, `fat_generate_report()`
  - Reference: https://www.opengate.at/blog/2024/01/bootsector/

- **C64 Protection Database Expansion** (1041 titles, +101 from 940):
  - **New protection schemes with detection functions:**
    - `C64_PROT_DATASOFT`: Datasoft long track (6680 bytes vs 6500 normal)
      - Detector: `c64_detect_datasoft()` - analyzes G64 track sizes
      - Titles: Bruce Lee, Mr. Do!, Dig Dug, Pac-Man, Conan, etc. (20 titles)
    - `C64_PROT_SSI_RDOS`: SSI RapidDOS with track 36 key
      - Detector: `c64_detect_ssi_rdos()` - checks 10 sectors/track structure
      - Titles: Pool of Radiance, Curse of Azure Bonds, Champions of Krynn (32 titles)
    - `C64_PROT_EA_INTERLOCK`: Electronic Arts Interlock
      - Detector: `c64_detect_ea_interlock()` - boot sector and interleave analysis
      - Titles: Bard's Tale II/III, Archon, Seven Cities of Gold (18 titles)
    - `C64_PROT_ABACUS`: Abacus software protection (9 titles)
    - `C64_PROT_RAINBIRD`: Rainbird/Firebird protection (10 titles)
  - **Enhanced existing detectors:**
    - `c64_detect_novaload()`: Ocean/Imagine fast loader with anti-tampering
    - `c64_detect_speedlock()`: Encrypted loader with timing checks
  - **New unified detection API:**
    - `c64_detect_all_protections()`: Runs all detectors on an image
  - Sources: Parameter Cross Reference Vol. 8/9 (E.A. Mallang III, 1987-1990)
  - **SSI RapidDOS Protection** - 32 SSI war/RPG games:
    - Gold Box Series: Pool of Radiance, Curse of Azure Bonds, Champions of Krynn
    - War Games: Panzer Strike!, Kampfgruppe, Carrier Force, Gettysburg
  - **Datasoft Long Track Protection** - 20 Datasoft titles:
    - Bruce Lee, Mr. Do!, Dig Dug, Pole Position, Pac-Man, Aztec Challenge
  - **EA Interlock Protection** - 18 Electronic Arts titles:
    - Bard's Tale II/III, Chuck Yeager AFT, Earl Weaver Baseball
  - **Abacus Protection** - 9 Abacus software titles
  - **Rainbird/Firebird Protection** - 10 titles (Elite, Sentinel, Carrier Command)
  - Additional Infocom, MicroProse titles

- **New C64 Protection Schemes**:
  - `C64_PROT_DATASOFT` - Datasoft long track (6680 bytes vs normal 6500)
  - `C64_PROT_SSI_RDOS` - SSI RapidDOS with track 36 key
  - `C64_PROT_EA_INTERLOCK` - Electronic Arts protection
  - `C64_PROT_ABACUS` - Abacus software protection
  - `C64_PROT_RAINBIRD` - Rainbird/Firebird protection

### Technical Details
- WOZ implementation based on John K. Morris specification (applesaucefdc.com)
- SSI RapidDOS uses track 36 key with 10 sectors per track
- Datasoft protection allows 6680 bytes per track (34 bytes more than normal)
- Source: Parameter Cross Reference Vol. 8/9 (10000+ listings)

## [4.1.5] - 2026-01-16

### Added
- **C64 Copy Protection Detection Module** (based on Super-Kit 1541 V2.0):
  - Complete 1541 drive error code analysis (Error 20-29)
  - **Known title database with 940 protected games** (expanded from Maverick V5 800+ title list)
  - Protection scheme detection:
    - Vorpal (Epyx)
    - V-Max! (Cinemaware/Activision/Taito)
    - RapidLok (Dane Final Agency/MicroProse)
    - Fat Track (Access)
    - Speedlock (Ocean/US Gold)
    - Novaload
    - EA Interlock
    - And many more...
  - Track 36-40 extended analysis
  - Half-track detection
  - GCR anomaly detection (sync marks, density, timing, bad GCR)
  - BAM validation and anomaly detection
  - Killer track detection
  - Detailed analysis report generation

- **V-MAX! Version Detection** (Pete Rittwage / Lord Crass documentation):
  - VMAX_VERSION_0: Star Rank Boxing (first title, CBM DOS, checksums)
  - VMAX_VERSION_1: Activision games (CBM DOS, byte counting)
  - VMAX_VERSION_2A: Cinemaware v2a (single EOR track 20, CBM DOS)
  - VMAX_VERSION_2B: Cinemaware v2b (dual EOR track 20, custom sectors)
  - VMAX_VERSION_3A: Taito v3a (variable sectors, normal syncs)
  - VMAX_VERSION_3B: Taito v3b (variable sectors, short syncs)
  - VMAX_VERSION_4: Later variation (4 marker bytes vs 7)
  - Technical constants: sector sizes, GCR ratios, marker bytes

- **RapidLok Version Detection** (Dane Final Agency / Pete Rittwage / Banguibob):
  - RAPIDLOK_VERSION_1-4: Patch keycheck works
  - RAPIDLOK_VERSION_5-6: Intermittent failures in VICE emulator
  - RAPIDLOK_VERSION_7: Requires additional crack work
  - Track 36 key sector extraction
  - $7B byte counting per track
  - Sync length measurement (320/480/40 bits)
  - Technical constants: sector counts, bit rates, marker bytes

- **New Publishers**: Cinemaware, Taito, Sega, Thunder Mountain, Ocean, US Gold, Dane Final Agency

- **New Protection Flags**: 
  - C64_PROT_GCR_BAD_GCR (invalid GCR patterns)
  - C64_PROT_SPEEDLOCK (Ocean/US Gold)
  - C64_PROT_NOVALOAD (Novaload fast loader)

- **API Functions**:
  - `c64_analyze_d64()` - Analyze D64 images
  - `c64_analyze_g64()` - Analyze G64 (GCR-level) images
  - `c64_analyze_d64_errors()` - Analyze D64 with error bytes
  - `c64_lookup_title()` - Search known titles database
  - `c64_generate_report()` - Generate detailed analysis report
  - `c64_detect_vmax_version()` - Detect V-MAX! version from G64
  - `c64_detect_rapidlok_version()` - Detect RapidLok version from G64
  - `c64_extract_rapidlok_key()` - Extract Track 36 key table
  - `c64_check_vmax_directory()` - Check for V-MAX "!" directory

### Technical Details
- Based on Super-Kit 1541 V2.0 Errata Sheet documentation
- V-MAX! research from Pete Rittwage and Lord Crass (c64preservation.com)
- RapidLok research from Banguibob, Kracker Jax Revealed Trilogy
- Supports D64 (35/40 tracks), G64 (GCR), and error-extended formats
- Confidence scoring for detection accuracy

## [4.1.4] - 2026-01-16

### Added
- **East Block/DDR Formats** (Option 1):
  - Robotron KC 85/87: DDR home computer with MicroDOS (Z80)
  - Pravetz 82/8M: Bulgarian Apple II clone
  - Meritum/TNS: Polish/Czech TRS-80 clone

- **Minicomputer Formats** (Option 2):
  - Data General Nova/Eclipse: 16-bit minicomputer (RDOS/AOS)
  - Prime Computer: 32-bit minicomputer (PRIMOS)

- **Industrial/Embedded Formats** (Option 3):
  - Heathkit H8/H89: Kit computer with hard-sectored disks (HDOS)
  - Cromemco CDOS: S-100 bus system

- **Extended Japanese Formats** (Option 4):
  - Sharp X1/X1 Turbo: Z80 home computer
  - Sanyo MBC-55x: MS-DOS compatible (sold as MBC-550/555 in USA)
  - Hitachi S1: Business computer

### Changed
- Format registry expanded to 163 formats
- Added platform categories: Robotron, Pravetz, Meritum, DG, Prime, Heathkit, Cromemco, SharpX1, Sanyo, Hitachi

## [4.1.3] - 2026-01-16

### Added
- **FC5025-Compatible Formats**:
  - Motorola VersaDOS: 68000 real-time OS (EXORmacs development systems)
  - PMC MicroMate: CP/M workstation, TRS-80 Model 100 accessory
  - Calcomp Vistagraphics 4500: Professional plotter/CAD workstation

- **Niche/Regional Formats**:
  - Pyldin 601 (Bulgaria): Z80-based home computer with UniDOS
  - RC759 Piccoline (Denmark): Regnecentralen 8088-based PC
  - Applix 1616 (Australia): 68000-based multi-user workstation

### Changed
- Format registry expanded to 156 formats
- Added platform categories: Motorola, PMC, Calcomp, Pyldin, RC759, Applix

## [4.1.2] - 2026-01-16

### Added
- **FLEX/UniFLEX Format Support** (6809 Systems):
  - TSC FLEX disk format (.dsk) for 6800/6809 systems
  - Used by: SWTPC, CoCo FLEX, Gimix, SMOKE SIGNAL
  - Directory listing and file extraction
  - Image creation with custom geometry
  - Supported geometries: 35/40/77/80 tracks, SS/DS, SD/DD/HD

- **UniFLEX Format**: Multi-user FLEX variant support

- **DEC RX50 Format** (DEC Rainbow/Professional):
  - 5.25" floppy format for DEC systems
  - 80 tracks, 10 sectors, 512 bytes/sector
  - Single-sided (400 KB) and double-sided (800 KB)
  - Filesystem detection: RT-11, CP/M-86, MS-DOS
  - Sector interleave handling
  - Image creation with optional MS-DOS BPB

- **Luxor ABC 80/800 Format** (Nordic):
  - Swedish home/office computers (1978-1985)
  - UFD-DOS and ABC-DOS filesystem support
  - Geometries: 160KB, 320KB, 640KB
  - 256-byte sectors, 16 sectors/track

- **Elektronika BK-0010/0011 Format** (Soviet):
  - Soviet PDP-11 compatible home computers (1985-1990s)
  - RT-11 compatible disk format
  - ANDOS, MK-DOS, CSI-DOS detection
  - 800KB DS/DD format (80x10x2, 512-byte sectors)

### Changed
- Format registry expanded to 150 formats
- Added platform categories: 6809, DEC, Nordic, Soviet

## [4.1.1] - 2026-01-16

### Added
- **CMD FD2000/FD4000 Formats**:
  - D1M: 720KB DD disk image
  - D2M: 1.44MB HD disk image  
  - D4M: 2.88MB ED disk image (FD4000 only)

- **G71 Format**: GCR track image for 1571 (double-sided G64)
  - Full read/write support
  - 70 tracks (35 per side)
  - GCR encoding with speed zones

- **IEEE-488 HAL**: Support for vintage CBM IEEE drives
  - CBM 2031, 4040, 8050, 8250, SFD-1001

- **XUM1541-II Support**: 
  - Hardware profiles for all XUM1541 variants
  - ZoomFloppy, XUM1541-II, AT90USBKEY, Bumble-B
  - Detailed documentation and build guide

- **M2I Tape Format**: Tape image format support
  - Read/write/extract files
  - Directory listing
  - Convert to/from T64

- **ZoomTape**: C2N/VIC-1530 tape device support
  - TAP file read/write
  - PRG extraction
  - Motor control and status

- **GEOS Copy Protection Detection**:
  - V1/V2 key disk detection
  - BAM signature analysis
  - Half-track protection detection
  - Detailed protection reports
  - Copy recommendations

- **XA1541/X1541 Legacy Support (GUI)**:
  - Full GUI integration in Hardware tab
  - X1541, XE1541, XM1541, XA1541, XAP1541 cable types
  - Parallel port detection (Linux/Windows)
  - Commodore device selection (8-15)
  - ZoomFloppy/XUM1541 USB detection

### Changed
- Hardware tab now shows categorized controller list
- Commodore format count increased to 25+ formats
- Total format count: 142 registered formats

## [4.1.0] - 2026-01-16

### Added
- CP/M Disk Definitions: 55 total
- LDBS Format support
- Plugin System architecture
- Context Menu in Explorer tab
- Translations: German, French, English

## [4.0.0] - 2026-01-16

### Added
- DMK Analyzer, Flux Histogram
- MAME MFI & 86Box 86F formats
- Documentation suite
- CI/CD pipeline

## [3.9.0] - 2026-01-15

### Added
- TI-99/4A format support
- FAT filesystem extensions
- 43 CP/M disk definitions
