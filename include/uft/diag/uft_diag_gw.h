/**
 * @file uft_diag_gw.h
 * @brief Surface-scan read adapter over a Greaseweazle (door stage 4).
 *
 * uft_diag_surface_scan() takes a uft_diag_read_fn and retries a sector
 * on the SAME side up to config.retries times. That retry is only worth
 * something when each attempt is a new physical measurement — over an
 * image file every attempt returns the same bytes. This adapter is that
 * measurement: every call that needs a new read performs
 *
 *     uft_gw_read_track(dev, track, side, revolutions)   (flux capture)
 *  -> flux_decode_track()                                (MFM/FM decode)
 *  -> look up the requested sector NUMBER in the decoded track
 *
 * Read cache, and why a retry is never served from it: one read of a
 * track answers every sector of that track once. A sector number that
 * was already asked from the current read — i.e. a retry — forces a NEW
 * read. So each attempt for one sector comes from its own capture, and
 * WEAK means "failed in one capture, read in a later one".
 *
 * What counts as "read": a decoded sector whose ID field carries the
 * requested sector number AND cylinder == track AND head == side, with
 * good ID CRC, good data CRC and exactly config.sector_size data bytes.
 * Anything else answers -1 (not read). The C/H comparison is deliberate:
 * a sector whose ID names another place is not this sector (a mispositioned
 * head reads a neighbour track with plausible data). Limit, stated: formats
 * that do not store the physical cylinder/head in the ID field scan as
 * BAD through this adapter.
 *
 * Geometry, sector numbers, encoding and cell time come from the CALLER
 * (uft_diag_config_t and uft_diag_gw_init()), never from a read.
 *
 * Two more limits, stated (neither loses or invents data; both make the
 * verdict coarser than the disk):
 *
 *  - revolutions > 1: one capture then holds each sector once per
 *    revolution, and the lookup takes the FIRST decoded copy that passes
 *    all checks. A sector that fails in revolution 1 and reads in
 *    revolution 2 of the same capture therefore counts as GOOD, not
 *    WEAK. WEAK only means "failed in one capture, read in a later
 *    capture". Tested is revolutions = 1 only; how many copies the
 *    decoder keeps per capture is not measured here.
 *  - No double-step: track t is always read at head position t. A
 *    40-track medium in an 80-track drive carries its track t/2 there
 *    (even t) or nothing readable (odd t); for t > 0 the IDs do not name
 *    cylinder t, so the C check above scans those sectors as BAD (track 0
 *    carries cylinder 0 and reads GOOD). Follows from the code; not
 *    exercised by a test.
 *
 * There is no write path: the adapter has no write parameter, stores no
 * write function, and calls no uft_gw_write_* / uft_gw_erase_* function.
 * uft_diag_write_verify() (destructive) is not reachable through it.
 *
 * Verified only against the Greaseweazle firmware emulator under
 * tests/emulators/greaseweazle (no physical hardware, MF-310):
 * tests/test_diag_gw_emulator.c.
 */
#ifndef UFT_DIAG_GW_H
#define UFT_DIAG_GW_H

#include "uft/diag/uft_disc_diagnostics.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uft_gw_device;   /* uft_greaseweazle_full.h: uft_gw_device_t */

typedef struct {
    /* Set by uft_diag_gw_init(), read-only afterwards. */
    struct uft_gw_device *dev;       /**< opened by the caller; borrowed */
    flux_encoding_t       encoding;  /**< FLUX_ENC_MFM or FLUX_ENC_FM */
    uint32_t              bitcell_ns;/**< 0 = decoder default for the encoding */
    uint8_t               revolutions; /**< per capture, >= 1 */
    uint32_t              sample_freq; /**< the device's own clock (GET_INFO) */

    /* Measured by the adapter. */
    unsigned              reads;       /**< captures performed */
    int                   last_gw_error; /**< last uft_gw_* error, 0 = none */

    /* Internal: the last decoded capture. */
    flux_decoded_track_t *track_cache;
    int                   cache_track, cache_side;
    bool                  cache_valid;
    uint8_t               asked[32];   /**< sector numbers served from it */
} uft_diag_gw_t;

/**
 * Prepares @p a for @p dev. @p encoding must be FLUX_ENC_MFM or
 * FLUX_ENC_FM (the caller knows the disk; AUTO would let a read decide),
 * @p revolutions >= 1. Sends one GET_INFO to learn the sample clock — it
 * asks the device itself rather than trusting the handle (until P3-551,
 * MF-1356, a device opened via uft_gw_open_stream() never had it).
 * Returns 0, or -1 on invalid arguments, a failed GET_INFO, or no memory.
 */
int  uft_diag_gw_init(uft_diag_gw_t *a, struct uft_gw_device *dev,
                      flux_encoding_t encoding, uint32_t bitcell_ns,
                      uint8_t revolutions);

/** Releases the read cache. Safe on a zeroed or already freed adapter. */
void uft_diag_gw_free(uft_diag_gw_t *a);

/**
 * uft_diag_read_fn for uft_diag_surface_scan(); @p user_data is the
 * uft_diag_gw_t. Returns 0 and fills @p buffer when the sector was read
 * as described above, -1 otherwise (never partial data).
 */
int  uft_diag_gw_read(int track, int side, int sector,
                      uint8_t *buffer, size_t sector_size, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DIAG_GW_H */
