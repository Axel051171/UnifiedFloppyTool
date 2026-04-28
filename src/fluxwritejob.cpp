/**
 * @file fluxwritejob.cpp
 * @brief Implementation of FluxWriteJob — see header (MF-114).
 */

#include "fluxwritejob.h"

#include <QDebug>
#include <QFileInfo>
#include <QThread>

#include <cstdlib>
#include <cstring>
#include <vector>

#include "uft/hal/uft_greaseweazle_full.h"
#include "uft/flux/uft_scp_parser.h"

FluxWriteJob::FluxWriteJob(QObject *parent)
    : QObject(parent)
    , m_gwDevice(nullptr)
    , m_driveUnit(0)
    , m_verify(false)
    , m_cancel(false)
{
}

FluxWriteJob::~FluxWriteJob() = default;

void FluxWriteJob::setDevice(void *gwDevice)         { m_gwDevice = gwDevice; }
void FluxWriteJob::setInputPath(const QString &p)    { m_inputPath = p; }
void FluxWriteJob::setDriveUnit(int unit)            { m_driveUnit = unit; }
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

    if (m_gwDevice == nullptr) {
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

    auto *gw = static_cast<uft_gw_device_t *>(m_gwDevice);

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

    /* Hand the drive over to write mode. Motor on, drive selected,
     * track 0 settled before the first seek. */
    emit stageChanged(tr("Selecting drive…"));
    if (uft_gw_select_drive(gw, (uint8_t)m_driveUnit) != 0) {
        uft_scp_close(scp);
        uft_scp_destroy(scp);
        emit error(tr("Failed to select drive unit %1.").arg(m_driveUnit));
        return;
    }
    if (uft_gw_set_motor(gw, true) != 0) {
        uft_scp_close(scp);
        uft_scp_destroy(scp);
        emit error(tr("Failed to start drive motor."));
        return;
    }
    QThread::msleep(500);

    const uint32_t sample_freq = uft_gw_get_sample_freq(gw);
    if (sample_freq == 0) {
        uft_gw_set_motor(gw, false);
        uft_scp_close(scp);
        uft_scp_destroy(scp);
        emit error(tr("Greaseweazle reported sample_freq=0 — refusing to write."));
        return;
    }
    const double ticks_per_ns = (double)sample_freq / 1.0e9;

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
         * with `0` placeholders marking 16-bit overflow positions
         * (parser already accumulates those into the next non-zero
         * entry). The HAL expects ticks-between-transitions, so we
         * convert and drop the zero slots. */
        std::vector<uint32_t> ticks;
        ticks.reserve(rev.flux_count);
        for (uint32_t i = 0; i < rev.flux_count; ++i) {
            uint32_t ns = rev.flux_data[i];
            if (ns == 0) continue;
            uint32_t tk = (uint32_t)((double)ns * ticks_per_ns);
            if (tk == 0) tk = 1;  /* HAL refuses zero-tick gaps */
            ticks.push_back(tk);
        }

        if (ticks.empty()) {
            ++tracks_skipped;
            uft_scp_free_track(&track_data);
            qWarning() << "SCP track" << t << "produced no ticks after conversion — skipping";
            continue;
        }

        if (uft_gw_seek(gw, cyl) != 0) {
            ++hard_errors;
            uft_scp_free_track(&track_data);
            uft_gw_set_motor(gw, false);
            uft_scp_close(scp);
            uft_scp_destroy(scp);
            emit error(tr("Seek to cylinder %1 failed — aborting write to avoid partial disk.").arg(cyl));
            return;
        }
        uft_gw_select_head(gw, (uint8_t)side);

        uft_gw_write_params_t params = {};
        params.index_sync          = true;
        params.erase_empty         = false;
        params.verify              = false;
        params.pre_erase_ticks     = 0;
        params.terminate_at_index  = 1;  /* one revolution */

        int wrc = uft_gw_write_flux(gw, &params, ticks.data(), (uint32_t)ticks.size());
        uft_scp_free_track(&track_data);

        if (wrc != 0) {
            ++hard_errors;
            uft_gw_set_motor(gw, false);
            uft_scp_close(scp);
            uft_scp_destroy(scp);
            emit error(tr("Write failed at cyl=%1 side=%2 (rc=%3) — drive may have a partially written disk.")
                           .arg(cyl).arg(side).arg(wrc));
            return;
        }

        ++tracks_written;
        int pct = (tracks_written * 100) / track_count_in_scp;
        if (pct > 100) pct = 100;
        emit progress(pct);
    }

    uft_gw_set_motor(gw, false);
    uft_scp_close(scp);
    uft_scp_destroy(scp);

    if (aborted) {
        emit error(tr("Write cancelled by user — disk is partially written."));
        return;
    }

    if (m_verify) {
        /* Real read-back verify is a separate concern that needs the
         * capture path (FluxCaptureJob) plus a flux-level diff. Honest
         * stub for now: surface that we did NOT verify rather than
         * silently claim we did. */
        emit stageChanged(tr("Skipping verify — feature not implemented yet."));
    }

    emit progress(100);
    QString msg = tr("Wrote %1 tracks (skipped %2)")
                      .arg(tracks_written).arg(tracks_skipped);
    if (hard_errors) {
        msg += tr(" — %1 hard errors during write.").arg(hard_errors);
    }
    emit finished(msg);
}
