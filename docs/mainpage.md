/**
 * @mainpage UnifiedFloppyTool API Documentation
 * 
 * @section intro Introduction
 * 
 * **UnifiedFloppyTool (UFT)** is a comprehensive floppy disk preservation and
 * forensic analysis library. It provides professional-grade tools for reading,
 * writing, analyzing, and recovering floppy disk images across multiple platforms
 * and formats.
 * 
 * @section features Key Features
 * 
 * - **Multi-Format Support**: 115+ formats including Commodore (D64, G64), 
 *   Amiga (ADF), Apple II, Atari, PC (IMG, TD0, IMD), and flux formats
 * - **Hardware Support**: Greaseweazle, FluxEngine, KryoFlux, FC5025, SuperCard Pro
 * - **Advanced Recovery**: Multi-pass reading, majority voting, weak bit detection
 * - **Copy Protection Analysis**: V-MAX, CopyLock, RapidLok, and 50+ protection schemes
 * - **GUI and CLI**: Professional Qt6 interface with command-line equivalence
 * 
 * @section modules Core Modules
 * 
 * @subsection core Core Library
 * - @ref uft_core.h "uft_core.h" - Core types and error handling
 * - @ref uft_error.h "uft_error.h" - Error codes and messages
 * - @ref uft_params.h "uft_params.h" - Parameter management
 * 
 * @subsection formats Format Parsers
 * - @ref uft_adf.h "uft_adf.h" - Amiga ADF format
 * - @ref uft_d64.h "uft_d64.h" - Commodore D64/D71/D81
 * - @ref uft_td0.h "uft_td0.h" - Teledisk format
 * - @ref uft_imd.h "uft_imd.h" - ImageDisk format
 * - @ref uft_dmk.h "uft_dmk.h" - TRS-80 DMK format
 * - @ref uft_scp.h "uft_scp.h" - SuperCard Pro flux format
 * - @ref uft_hfe.h "uft_hfe.h" - HxC floppy emulator format
 * 
 * @subsection encoding Encoding/Decoding
 * - @ref uft_mfm.h "uft_mfm.h" - MFM encoding/decoding
 * - @ref uft_fm.h "uft_fm.h" - FM (single density) encoding
 * - @ref uft_gcr.h "uft_gcr.h" - GCR encoding (Apple, Commodore)
 * - @ref uft_pll.h "uft_pll.h" - Phase-locked loop for flux decoding
 * 
 * @subsection recovery Recovery & Analysis
 * - @ref uft_recovery.h "uft_recovery.h" - Multi-pass recovery pipeline
 * - @ref uft_protection.h "uft_protection.h" - Copy protection detection
 * - @ref uft_weak_bits.h "uft_weak_bits.h" - Weak/fuzzy bit analysis
 * 
 * @subsection hal Hardware Abstraction
 * - @ref uft_hal.h "uft_hal.h" - Hardware abstraction layer
 * - @ref uft_greaseweazle.h "uft_greaseweazle.h" - Greaseweazle interface
 * - @ref uft_fluxengine.h "uft_fluxengine.h" - FluxEngine interface
 * 
 * @section quickstart Quick Start
 * 
 * @subsection reading Reading a Disk Image
 * 
 * @code{.c}
 * #include <uft/uft_core.h>
 * #include <uft/uft_adf.h>
 * 
 * int main(void) {
 *     uft_adf_image_t* img = NULL;
 *     int err = uft_adf_read("disk.adf", &img);
 *     if (err != UFT_ERR_OK) {
 *         fprintf(stderr, "Error: %s\n", uft_error_string(err));
 *         return 1;
 *     }
 *     
 *     printf("Tracks: %d, Heads: %d\n", img->tracks, img->heads);
 *     
 *     uft_adf_free(img);
 *     return 0;
 * }
 * @endcode
 * 
 * @subsection flux Working with Flux Data
 * 
 * @code{.c}
 * #include <uft/uft_scp.h>
 * #include <uft/uft_pll.h>
 * 
 * // Read SCP flux file
 * uft_scp_image_t* scp = NULL;
 * uft_scp_read("capture.scp", &scp);
 * 
 * // Decode using PLL
 * uft_pll_config_t pll_cfg = UFT_PLL_CONFIG_MFM_HD;
 * uft_decoded_track_t* track = NULL;
 * uft_pll_decode(scp->tracks[0], &pll_cfg, &track);
 * 
 * // Extract sectors
 * uft_sector_t* sectors = NULL;
 * size_t count = 0;
 * uft_mfm_extract_sectors(track, &sectors, &count);
 * @endcode
 * 
 * @section building Building UFT
 * 
 * @subsection requirements Requirements
 * - CMake 3.16+
 * - C11/C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
 * - Qt6.2+ (for GUI, optional)
 * 
 * @subsection linux Linux Build
 * @code{.bash}
 * mkdir build && cd build
 * cmake .. -DCMAKE_BUILD_TYPE=Release
 * cmake --build . --parallel
 * @endcode
 * 
 * @subsection windows Windows Build (MSVC)
 * @code{.bat}
 * scripts\build_msvc.bat --release
 * @endcode
 * 
 * @subsection macos macOS Universal Binary
 * @code{.bash}
 * ./scripts/build_macos_universal.sh
 * @endcode
 * 
 * @section license License
 * 
 * UFT is released under a permissive open-source license. See LICENSE file
 * for details.
 * 
 * @section links Links
 * 
 * - GitHub: https://github.com/yourusername/UnifiedFloppyTool
 * - Documentation: https://yourusername.github.io/UnifiedFloppyTool
 * 
 * @version 4.1.0
 * @date 2026-01-03
 */

/**
 * @defgroup core Core Library
 * @brief Core types, error handling, and utilities
 */

/**
 * @defgroup formats Format Parsers
 * @brief Disk image format parsers and writers
 */

/**
 * @defgroup encoding Encoding/Decoding
 * @brief Low-level encoding (MFM, FM, GCR) routines
 */

/**
 * @defgroup flux Flux Processing
 * @brief Flux data handling and PLL decoding
 */

/**
 * @defgroup recovery Recovery & Analysis
 * @brief Disk recovery and forensic analysis
 */

/**
 * @defgroup protection Copy Protection
 * @brief Copy protection detection and analysis
 */

/**
 * @defgroup hal Hardware Abstraction
 * @brief Hardware controller interfaces
 */

/**
 * @defgroup gui GUI Components
 * @brief Qt6 GUI widgets and dialogs
 */
