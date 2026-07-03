/**
 * @file tests/emulators/fluxengine/firmware_state_machine.h
 * @brief fluxengine-CLI subprocess + device model for the FluxEngine controller.
 *
 * FluxEngine has no C HAL and no wire protocol in UFT — the C++ provider
 * (src/hardware_providers/fluxengine_provider_v2.cpp) runs the
 * `fluxengine` command-line tool via an injectable runner. So this
 * emulator models the fluxengine + device pair: a read request produces
 * a real SCP file (from the synthetic generator) the way fluxengine
 * would, a write is refused (forensic read-only policy), and `rpm`
 * returns a stdout line the provider's regex must parse.
 *
 * SPEC_STATUS: CommunityConsensus — FluxEngine is David Given's
 * open-source tool (github.com/davidgiven/fluxengine); the CLI is
 * documented by its README/source, not a vendor SDK. Modelled on the
 * provider's own argv construction:
 *     read  -c ibm -s drive:0 --tracks=cNhM --drive.revolutions=R -o out.scp
 *     write -c ibm -d drive:0 --tracks=cNhM -i in.scp
 *     rpm
 * and the RPM stdout regex (\d+\.?\d*)\s*rpm (icase). Every inferred
 * behaviour is recorded in DIVERGENCES.md.
 *
 * Forensic invariants:
 *   - No fabricated flux: a successful read returns the generator's SCP
 *     bytes verbatim; a failed read returns a non-zero exit AND no file.
 *   - Write is ALWAYS refused (read-only policy; the V1 provider's
 *     motor/seek are silent stubs, and UFT does not write via FluxEngine
 *     in the forensic path). Visible via a distinct exit code.
 */
#ifndef UFT_TESTS_FE_STATE_MACHINE_H
#define UFT_TESTS_FE_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── fluxengine exit codes (modelled; DIVERGENCES.md FE-1) ─────────── */
typedef enum {
    FE_EXIT_OK          = 0,   /* command succeeded                     */
    FE_EXIT_NO_DEVICE   = 1,   /* no FluxEngine board / greaseweazle    */
    FE_EXIT_NO_DISK     = 2,   /* drive empty                           */
    FE_EXIT_BAD_ARGS    = 3,   /* malformed argv                        */
    FE_EXIT_WRITE_DENY  = 4,   /* write refused (read-only policy)      */
    FE_EXIT_IO          = 5,   /* read/transfer failure                 */
} fe_exit_t;

typedef enum {
    FE_STATE_DISCONNECTED = 0,
    FE_STATE_READY        = 1,
    FE_STATE_BUSY         = 2,   /* transient during a command */
    FE_STATE_ERROR        = 3,
} fe_state_t;

/* ─── request (mirrors the provider argv knobs) ─────────────────────── */
typedef struct {
    const char *subcommand;   /* "read" / "write" / "rpm" */
    int   cylinder;           /* --tracks=c<cyl> */
    int   head;               /* h<head> */
    int   revolutions;        /* --drive.revolutions=R (read) */
    const char *profile;      /* -c <profile>, e.g. "ibm" */
} fe_request_t;

/* ─── device model ──────────────────────────────────────────────────── */
typedef struct {
    fe_state_t state;

    bool   device_present;
    bool   disk_present;
    double rpm;               /* what `rpm` reports */
    int    rpm_variant;       /* which stdout shape `rpm` emits (0..2) */

    int    fail_next_n;       /* next N reads fail with IO */
    uint64_t stream_seed;     /* seed for the SCP generator */
    uint32_t inject_defects;  /* uft_fe_defect_flags_t for produced SCP */

    fe_exit_t last_exit;
    char      last_stdout[128];
    uint64_t  reads_ok;
    uint64_t  reads_failed;
} fe_dev_t;

/* ─── lifecycle ─────────────────────────────────────────────────────── */
void fe_reset(fe_dev_t *dev);
void fe_power_on_defaults(fe_dev_t *dev);   /* board+disk present, 300 rpm */
void fe_set_device_present(fe_dev_t *dev, bool present);
void fe_set_disk_present(fe_dev_t *dev, bool present);
void fe_set_rpm(fe_dev_t *dev, double rpm);
void fe_set_rpm_variant(fe_dev_t *dev, int variant);
void fe_set_fail_next(fe_dev_t *dev, int n);
void fe_set_stream_seed(fe_dev_t *dev, uint64_t seed);
void fe_set_inject_defects(fe_dev_t *dev, uint32_t defects);

/* ─── operations ────────────────────────────────────────────────────── */

/** Model one `fluxengine` invocation. For "read", on success allocates
 *  *out_scp / *out_len with the synthetic SCP file (caller frees via
 *  free()). For "rpm", fills dev->last_stdout with the RPM line. Returns
 *  the exit code; on any failure *out_scp is NULL. */
fe_exit_t fe_run(fe_dev_t *dev, const fe_request_t *req, int max_retries,
                 uint8_t **out_scp, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_FE_STATE_MACHINE_H */
