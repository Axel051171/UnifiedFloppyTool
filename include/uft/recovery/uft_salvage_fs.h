/**
 * @file uft_salvage_fs.h
 * @brief DiskSalv-inspired recovery strategies for AmigaDOS images.
 *
 * Three recovery strategies from DiskSalv.guide:
 *
 *   (1) Repair-in-place        — Prinzip-1-FORBIDDEN (writes to source)
 *   (2) Recover-by-copy        — bit-exact copy, then scan the copy
 *   (3) File-by-file extract   — walk header blocks, extract valid chains
 *
 * Strategy (1) is explicitly rejected with UFT_ERR_UNSUPPORTED per
 * XCOPY_INTEGRATION_TODO § Anti-Features (repair must not modify
 * source). Strategies (2) and (3) both produce a .loss.json sidecar
 * documenting what could not be recovered.
 *
 * Part of MASTER_PLAN.md M2 T7.
 */
#ifndef UFT_SALVAGE_FS_H
#define UFT_SALVAGE_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Recovery strategy selector. */
typedef enum {
    UFT_SALVAGE_REPAIR_IN_PLACE  = 1,   /**< FORBIDDEN — always errors */
    UFT_SALVAGE_RECOVER_BY_COPY  = 2,   /**< bit-exact copy + scan copy */
    UFT_SALVAGE_FILE_BY_FILE     = 3    /**< walk headers, extract chains */
} uft_salvage_strategy_t;

/** Statistics from one salvage run. */
typedef struct {
    uft_salvage_strategy_t   strategy;
    uint32_t                 blocks_total;
    uint32_t                 blocks_valid_checksum;  /**< std Amiga sum == 0 */
    uint32_t                 blocks_unreadable;
    uint32_t                 header_candidates;      /**< type=2 blocks */
    uint32_t                 headers_with_valid_chain;
    uint32_t                 files_extracted;        /**< 0 until walker lands */
    uint32_t                 files_skipped_corrupt;
    bool                     loss_json_written;
    char                     output_dir[260];        /**< path used */
} uft_salvage_result_t;


/**
 * Run a salvage operation on an AmigaDOS image buffer.
 *
 * Behaviour per strategy:
 *   REPAIR_IN_PLACE → returns UFT_ERR_UNSUPPORTED immediately
 *                     (Prinzip 1: never modify source)
 *   RECOVER_BY_COPY → writes <output_dir>/image.adf as a byte-identical
 *                     copy, then scans that copy and populates the stats.
 *   FILE_BY_FILE    → walks potential header blocks (type=2), counts
 *                     chain-consistent ones, populates header_candidates
 *                     and headers_with_valid_chain. Actual file
 *                     extraction is deferred (see files_extracted = 0).
 *
 * @param image        source AmigaDOS image (typically a full ADF)
 * @param image_size   bytes; must be a multiple of 512
 * @param strategy     which strategy to run
 * @param output_dir   directory for output artefacts
 *                     (RECOVER_BY_COPY: image.adf + loss.json;
 *                      FILE_BY_FILE: manifest.json + per-file data)
 * @param result       out: populated statistics
 *
 * @return UFT_OK on success,
 *         UFT_ERR_UNSUPPORTED for REPAIR_IN_PLACE,
 *         UFT_ERR_INVALID_ARG on NULL inputs or invalid size,
 *         UFT_ERR_IO on write errors.
 *
 * Note: file-by-file extraction (populating files_extracted > 0) is
 * currently a documented TODO in the implementation — it requires a
 * complete AmigaDOS header-block walker that has not yet been ported
 * from DiskSalv. The scaffolding (candidate count, chain validation)
 * is wired so a follow-up commit can add extraction without touching
 * the API surface.
 */
uft_error_t uft_salvage_run(const uint8_t *image,
                             size_t image_size,
                             uft_salvage_strategy_t strategy,
                             const char *output_dir,
                             uft_salvage_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SALVAGE_FS_H */
