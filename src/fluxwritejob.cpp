/**
 * @file fluxwritejob.cpp
 * @brief Implementation of FluxWriteJob — see header (MF-114).
 */

#include "fluxwritejob.h"

#include <QDebug>
#include <QFileInfo>
#include <QThread>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <variant>
#include <vector>

#include "hardware_providers/greaseweazle_provider_v2.h"
#include "uft/flux/uft_scp_parser.h"

FluxWriteJob::FluxWriteJob(QObject *parent)
    : QObject(parent)
    , m_provider(nullptr)
    , m_verify(false)
    , m_cancel(false)
{
}

FluxWriteJob::~FluxWriteJob() = default;

void FluxWriteJob::setProvider(::uft::hal::GreaseweazleProviderV2 *provider)
{
    m_provider = provider;
}
void FluxWriteJob::setInputPath(const QString &p)    { m_inputPath = p; }
void FluxWriteJob::setVerify(bool verify)            { m_verify = verify; }

void FluxWriteJob::requestCancel()
{
    m_cancel.store(true, std::memory_order_relaxed);
}

bool FluxWriteJob::isCancelled() const
{
    return m_cancel.load(std::memory_order_relaxed);
}

void FluxWriteJob::run()
{
    qDebug() << "FluxWriteJob::run() in thread" << QThread::currentThreadId();

    if (m_provider == nullptr) {
        emit error(tr("No hardware device — connect a Greaseweazle in the Hardware tab first."));
        return;
    }
    if (m_inputPath.isEmpty()) {
        emit error(tr("No source file specified."));
        return;
    }
    QFileInfo srcInfo(m_inputPath);
    if (!srcInfo.exists() || !srcInfo.isReadable()) {
        emit error(tr("Source file not readable: %1").arg(m_inputPath));
        return;
    }

    /* This worker is intentionally SCP-only. Other flux-image formats
     * (HFE, IPF, A2R, …) reach the HAL through their own conversion
     * paths; bundling the matrix into one worker would force every
     * format-specific decision into a single function. */
    if (srcInfo.suffix().toLower() != "scp") {
        emit error(tr("Only SCP source images are supported by the flux-write "
                      "path right now (got .%1). Convert to SCP first or use the "
                      "Format Converter tab.").arg(srcInfo.suffix()));
        return;
    }

    /* Open the SCP via the real ctx-based parser. The legacy stub
     * uft_scp_read(uint8_t*, size_t, uft_scp_file_t*) returns -1 on
     * purpose — see src/core/uft_core_stubs.c. We use the live API. */
    uft_scp_ctx_t *scp = uft_scp_create();
    if (!scp) {
        emit error(tr("Out of memory creating SCP parser context."));
        return;
    }

    emit stageChanged(tr("Opening SCP image…"));
    int rc = uft_scp_open(scp, m_inputPath.toLocal8Bit().constData());
    if (rc != UFT_SCP_OK) {
        uft_scp_destroy(scp);
        emit error(tr("Failed to open SCP file (%1, rc=%2).").arg(m_inputPath).arg(rc));
        return;
    }

    int track_count_in_scp = uft_scp_get_track_count(scp);
    if (track_count_in_scp <= 0) {
        uft_scp_close(scp);
        uft_scp_destroy(scp);
        emit error(tr("SCP image contains no tracks."));
        return;
    }

    /* MF-201 (P1.21): migrated to the V2 outcome surface — mirror of the
     * MF-200 FluxCaptureJob migration.
     *  - uft_gw_select_drive() is gone — GreaseweazleProviderV2 asserts
     *    the bus unit lazily (MF-199).
     *  - uft_gw_select_head() is gone — head is a field of WriteFluxParams.
     *  - uft_gw_get_sample_freq() / ticks_per_ns are gone — FluxStream
     *    carries transitions in nanoseconds and do_write_raw_flux does
     *    the ns->tick conversion internally. */

    emit stageChanged(tr("Starting drive motor…"));
    {
        bool motor_ok = false;
        QString motor_err;
        std::visit(::uft::hal::overloaded{
            [&](const ::uft::hal::MotorRunning&)             { motor_ok = true; },
            [&](const ::uft::hal::MotorStopped&)             { motor_ok = true; },
            [&](const ::uft::hal::MotorStalled& s)           { motor_err = QString::fromStdString(s.reason); },
            [&](const ::uft::hal::CapabilityRequiresPolicy& p){ motor_err = QString::fromStdString(p.explain); },
            [&](const ::uft::hal::HardwareDisconnected& d)   { motor_err = tr("hardware disconnected (%1)").arg(QString::fromStdString(d.device_path)); },
            [&](const ::uft::hal::ProviderError& e)          { motor_err = QString::fromStdString(e.what); },
        }, m_provider->set_motor(true));
        if (!motor_ok) {
            uft_scp_close(scp);
            uft_scp_destroy(scp);
            emit error(tr("Failed to start drive motor: %1").arg(motor_err));
            return;
        }
    }
    QThread::msleep(500);

    emit stageChanged(tr("Writing %1 tracks…").arg(track_count_in_scp));
    emit progress(0);

    int tracks_written = 0;
    int tracks_skipped = 0;
    int hard_errors    = 0;
    bool aborted = false;

    /* SCP track numbering is interleaved: cylinder*2 + side. Walk the
     * 0..167 slot space and write each populated slot. */
    for (int t = 0; t < UFT_SCP_MAX_TRACKS && !aborted; ++t) {
        if (isCancelled()) { aborted = true; break; }

        if (!uft_scp_has_track(scp, t)) {
            continue;
        }

        int cyl  = t / 2;
        int side = t % 2;

        uft_scp_track_data_t track_data;
        memset(&track_data, 0, sizeof(track_data));
        int rrc = uft_scp_read_track(scp, t, &track_data);
        if (rrc != UFT_SCP_OK || !track_data.valid
            || track_data.revolution_count == 0
            || track_data.revolutions[0].flux_count == 0
            || track_data.revolutions[0].flux_data == nullptr) {
            ++tracks_skipped;
            uft_scp_free_track(&track_data);
            qWarning() << "SCP track" << t << "unreadable — skipping";
            continue;
        }

        const uft_scp_rev_data_t &rev = track_data.revolutions[0];

        /* SCP flux entries are nanosecond intervals between transitions,
         * with `0` placeholders marking 16-bit overflow positions the
         * parser already folded into the next non-zero entry.
         * FluxStream::transitions_ns is itself in nanoseconds, so the
         * intervals pass straight through — only the zero placeholders
         * are dropped. The ns->tick conversion now lives in
         * GreaseweazleProviderV2::do_write_raw_flux. */
        ::uft::hal::FluxStream flux;
        flux.transitions_ns.reserve(rev.flux_count);
        for (uint32_t i = 0; i < rev.flux_count; ++i) {
            uint32_t ns = rev.flux_data[i];
            if (ns == 0) continue;
            flux.transitions_ns.push_back(ns);
        }

        if (flux.transitions_ns.empty()) {
            ++tracks_skipped;
            uft_scp_free_track(&track_data);
            qWarning() << "SCP track" << t
                       << "produced no flux after dropping zero placeholders — skipping";
            continue;
        }

        bool seek_ok = false;
        QString seek_err;
        std::visit(::uft::hal::overloaded{
            [&](const ::uft::hal::SeekArrived&)               { seek_ok = true; },
            [&](const ::uft::hal::SeekOvershot& o)            { seek_err = tr("overshot (requested %1, actual %2)").arg(o.requested).arg(o.actual); },
            [&](const ::uft::hal::SeekTrack0Failed& tk)       { seek_err = QString::fromStdString(tk.reason); },
            [&](const ::uft::hal::CapabilityRequiresPolicy& p){ seek_err = QString::fromStdString(p.explain); },
            [&](const ::uft::hal::HardwareDisconnected& d)    { seek_err = tr("hardware disconnected (%1)").arg(QString::fromStdString(d.device_path)); },
            [&](const ::uft::hal::ProviderError& e)           { seek_err = QString::fromStdString(e.what); },
        }, m_provider->seek(cyl));
        if (!seek_ok) {
            ++hard_errors;
            uft_scp_free_track(&track_data);
            (void)m_provider->set_motor(false);
            uft_scp_close(scp);
            uft_scp_destroy(scp);
            emit error(tr("Seek to cylinder %1 failed (%2) — aborting write to avoid partial disk.")
                           .arg(cyl).arg(seek_err));
            return;
        }

        /* WriteFluxParams::verify defaults to true; honour the job's own
         * m_verify (off by default, mirroring the V1 params.verify=false).
         * When verify is on, do_write_raw_flux does a read-back pass and
         * a mismatch surfaces here as WriteVerifyFailed. */
        ::uft::hal::WriteFluxParams wp;
        wp.cylinder = cyl;
        wp.head     = side;
        wp.verify   = m_verify;

        bool write_ok = false;
        QString write_err;
        std::visit(::uft::hal::overloaded{
            [&](const ::uft::hal::WriteCompleted&)            { write_ok = true; },
            [&](const ::uft::hal::WriteVerifyFailed&)         { write_err = tr("post-write verify mismatch"); },
            [&](const ::uft::hal::WriteRefused& r)            { write_err = tr("write refused: %1").arg(QString::fromStdString(r.physical_reason)); },
            [&](const ::uft::hal::CapabilityRequiresPolicy& p){ write_err = QString::fromStdString(p.explain); },
            [&](const ::uft::hal::HardwareDisconnected& d)    { write_err = tr("hardware disconnected (%1)").arg(QString::fromStdString(d.device_path)); },
            [&](const ::uft::hal::ProviderError& e)           { write_err = QString::fromStdString(e.what); },
        }, m_provider->write_raw_flux(wp, flux));

        uft_scp_free_track(&track_data);

        if (!write_ok) {
            ++hard_errors;
            (void)m_provider->set_motor(false);
            uft_scp_close(scp);
            uft_scp_destroy(scp);
            emit error(tr("Write failed at cyl=%1 side=%2 (%3) — drive may have a partially written disk.")
                           .arg(cyl).arg(side).arg(write_err));
            return;
        }

        ++tracks_written;
        int pct = (tracks_written * 100) / track_count_in_scp;
        if (pct > 100) pct = 100;
        emit progress(pct);
    }

    /* Tearing down — the motor-stop outcome is intentionally discarded. */
    (void)m_provider->set_motor(false);
    uft_scp_close(scp);
    uft_scp_destroy(scp);

    if (aborted) {
        emit error(tr("Write cancelled by user — disk is partially written."));
        return;
    }

    emit progress(100);
    QString msg = tr("Wrote %1 tracks (skipped %2)")
                      .arg(tracks_written).arg(tracks_skipped);
    if (m_verify) {
        msg += tr(" — each track verified inline (read-back compare).");
    }
    if (hard_errors) {
        msg += tr(" — %1 hard errors during write.").arg(hard_errors);
    }
    emit finished(msg);
}
