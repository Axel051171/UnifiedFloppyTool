/**
 * @file qprocess_subprocess_runner.h
 * @brief QProcess-based runner factories for FluxEngine + KryoFlux DTC (MF-256).
 *
 * Both FluxEngineProviderV2 and KryoFluxProviderV2 take an injectable
 * subprocess runner of identical shape:
 *     (argv, stdin_data) -> { stdout_text, stderr_text, exit_code }
 * Pre-MF-256 the HardwareTab constructed these providers with nullptr
 * runners — every read/write surfaced a ProviderError. This file
 * provides Qt-based default runners that launch the real CLI tools
 * (`fluxengine` and `dtc`) so the providers exercise real hardware.
 *
 * The tools must be installed and on PATH (or an explicit path
 * supplied via the binary parameter). UFT does NOT bundle them —
 * KryoFlux DTC is closed-source and FluxEngine is a separate
 * project with its own release cadence.
 *
 * Hardware-verification status: same as MF-249..MF-255 — code path
 * is live and compiles, end-to-end behaviour against real hardware
 * has not yet been benchmarked. HardwareTab shows yellow "Disconnect
 * (Beta)" styling.
 */
#pragma once

#include "fluxengine_provider_v2.h"
#include "kryoflux_provider_v2.h"

#include <QString>
#include <string>

namespace uft::hal {

/* ─── Configuration ───────────────────────────────────────────────── */

struct SubprocessRunnerConfig {
    /** Tool name or absolute path. Empty → use the tool's default
     *  name as found on PATH ("fluxengine" / "dtc"). */
    QString binary;

    /** Per-invocation timeout (ms). 0 = wait forever. Default
     *  matches the bench-test default of 10 minutes (flux capture
     *  with multiple revolutions can take a while). */
    int     timeout_ms = 600000;
};

/* ─── FluxEngine runner ──────────────────────────────────────────── */

/** Build a FluxEngineRunner that launches the `fluxengine` CLI as a
 *  child process. Captures stdout (binary-safe), stderr, exit code.
 *  Pipes `stdin_data` to the child's stdin. */
FluxEngineProviderV2::FluxEngineRunner
make_fluxengine_qprocess_runner(SubprocessRunnerConfig cfg = {});

/* ─── KryoFlux runner ────────────────────────────────────────────── */

/** Build a DtcRunner that launches the KryoFlux `dtc` CLI as a child
 *  process. Same capture semantics as FluxEngine. */
KryoFluxProviderV2::DtcRunner
make_kryoflux_qprocess_runner(SubprocessRunnerConfig cfg = {});

} // namespace uft::hal
