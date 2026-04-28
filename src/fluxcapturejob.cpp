/**
 * @file fluxcapturejob.cpp
 * @brief Implementation of FluxCaptureJob — see header for context (MF-110).
 */

#include "fluxcapturejob.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QThread>

#include <cstdlib>
#include <vector>

#include "uft/hal/uft_greaseweazle_full.h"
#include "uft/formats/uft_scp_writer.h"

FluxCaptureJob::FluxCaptureJob(QObject *parent)
    : QObject(parent)
    , m_gwDevice(nullptr)
    , m_cylinders(80)
    , m_sides(2)
    , m_revolutions(2)
    , m_driveUnit(0)
    , m_scpDiskType(SCP_TYPE_PC_DD)
    , m_cancel(false)
{
}

FluxCaptureJob::~FluxCaptureJob() = default;

void FluxCaptureJob::setDevice(void *gwDevice)        { m_gwDevice = gwDevice; }
void FluxCaptureJob::setOutputPath(const QString &p)  { m_outputPath = p; }
void FluxCaptureJob::setDriveUnit(int unit)           { m_driveUnit = unit; }
void FluxCaptureJob::setScpDiskType(uint8_t type)     { m_scpDiskType = type; }

void FluxCaptureJob::setGeometry(int cylinders, int sides)
{
    m_cylinders = cylinders;
    m_sides = sides;
}

void FluxCaptureJob::setRevolutions(int revs)
{
    if (revs < 1) revs = 1;
    if (revs > 5) revs = 5;
    m_revolutions = revs;
}

void FluxCaptureJob::requestCancel()
{
    m_cancel.store(true, std::memory_order_relaxed);
}

bool FluxCaptureJob::isCancelled() const
{
    return m_cancel.load(std::memory_order_relaxed);
}

void FluxCaptureJob::run()
{
    qDebug() << "FluxCaptureJob::run() in thread" << QThread::currentThreadId();

    if (m_gwDevice == nullptr) {
        emit error(tr("No hardware device — connect a Greaseweazle in the Hardware tab first."));
        return;
    }
    if (m_outputPath.isEmpty()) {
        emit error(tr("No destination file specified."));
        return;
    }
    if (m_cylinders <= 0 || m_sides <= 0 || m_sides > 2) {
        emit error(tr("Invalid geometry (cylinders=%1 sides=%2).").arg(m_cylinders).arg(m_sides));
        return;
    }

    QFileInfo outInfo(m_outputPath);
    QDir outDir = outInfo.absoluteDir();
    if (!outDir.exists()) {
        emit error(tr("Destination directory does not exist: %1").arg(outDir.absolutePath()));
        return;
    }

    auto *gw = static_cast<uft_gw_device_t *>(m_gwDevice);

    scp_writer_t *writer = scp_writer_create(m_scpDiskType, (uint8_t)m_revolutions);
    if (!writer) {
        emit error(tr("Could not allocate SCP writer."));
        return;
    }

    emit stageChanged(tr("Selecting drive…"));
    if (uft_gw_select_drive(gw, (uint8_t)m_driveUnit) != 0) {
        scp_writer_free(writer);
        emit error(tr("Failed to select drive unit %1.").arg(m_driveUnit));
        return;
    }
    if (uft_gw_set_motor(gw, true) != 0) {
        scp_writer_free(writer);
        emit error(tr("Failed to start drive motor."));
        return;
    }
    QThread::msleep(500);  // Let the drive spin up.

    const uint32_t sample_freq = uft_gw_get_sample_freq(gw);
    if (sample_freq == 0) {
        uft_gw_set_motor(gw, false);
        scp_writer_free(writer);
        emit error(tr("Greaseweazle reported sample_freq=0 — refusing to capture."));
        return;
    }
    const double ns_per_tick = 1.0e9 / static_cast<double>(sample_freq);

    int total_steps = m_cylinders * m_sides;
    int step = 0;
    int errors = 0;

    emit stageChanged(tr("Capturing flux (%1 cyl × %2 sides × %3 rev)…")
                      .arg(m_cylinders).arg(m_sides).arg(m_revolutions));
    emit progress(0);

    bool aborted = false;

    for (int cyl = 0; cyl < m_cylinders && !aborted; ++cyl) {
        if (isCancelled()) { aborted = true; break; }

        if (uft_gw_seek(gw, cyl) != 0) {
            ++errors;
            qWarning() << "Seek to cyl" << cyl << "failed";
            // Honest path: skip but keep going. SCP writer simply has no
            // entry for this track — never silently zero-pads.
            step += m_sides;
            continue;
        }

        for (int side = 0; side < m_sides && !aborted; ++side) {
            if (isCancelled()) { aborted = true; break; }

            uft_gw_select_head(gw, (uint8_t)side);

            uft_gw_read_params_t params = {};
            params.revolutions = (uint8_t)m_revolutions;
            params.index_sync  = true;

            uft_gw_flux_data_t *flux = nullptr;
            int rc = uft_gw_read_flux(gw, &params, &flux);
            if (rc != 0 || flux == nullptr || flux->sample_count == 0) {
                ++errors;
                qWarning() << "read_flux failed cyl" << cyl << "side" << side
                           << "rc" << rc;
                if (flux) uft_gw_flux_free(flux);
                ++step;
                continue;
            }

            // SCP wants intervals in nanoseconds and a per-revolution
            // duration. The HAL gives us a single intervals-in-ticks
            // stream that already spans `revolutions` revolutions; we
            // split on flux->index_times[].
            std::vector<uint32_t> ns_buf;
            ns_buf.reserve(flux->sample_count);

            uint32_t cumulative_ticks = 0;
            uint32_t prev_index_ticks = 0;
            uint32_t rev_idx_in_writer = 0;

            // Pre-convert intervals to ns into a single buffer; we'll
            // slice it per revolution boundary below.
            std::vector<uint32_t> all_ns(flux->sample_count);
            for (uint32_t i = 0; i < flux->sample_count; ++i) {
                all_ns[i] = (uint32_t)((double)flux->samples[i] * ns_per_tick);
            }

            // Walk samples, emit each revolution as its own
            // scp_writer_add_track call. index_times[] are cumulative
            // ticks from start of capture.
            uint32_t sample_start = 0;
            for (uint8_t r = 0; r < flux->index_count && rev_idx_in_writer < (uint32_t)m_revolutions; ++r) {
                uint32_t target_ticks = flux->index_times[r];

                // Walk samples until cumulative_ticks reaches target_ticks.
                uint32_t sample_end = sample_start;
                while (sample_end < flux->sample_count
                       && cumulative_ticks < target_ticks) {
                    cumulative_ticks += flux->samples[sample_end];
                    ++sample_end;
                }

                if (sample_end > sample_start) {
                    uint32_t rev_ticks = target_ticks - prev_index_ticks;
                    uint32_t duration_ns = (uint32_t)((double)rev_ticks * ns_per_tick);
                    int rc2 = scp_writer_add_track(
                        writer, cyl, side,
                        all_ns.data() + sample_start,
                        (size_t)(sample_end - sample_start),
                        duration_ns,
                        (int)rev_idx_in_writer);
                    if (rc2 != 0) {
                        ++errors;
                        qWarning() << "scp_writer_add_track rc=" << rc2;
                    }
                    ++rev_idx_in_writer;
                }

                prev_index_ticks = target_ticks;
                sample_start = sample_end;
            }

            uft_gw_flux_free(flux);

            ++step;
            int pct = (step * 100) / total_steps;
            emit progress(pct);
        }
    }

    uft_gw_set_motor(gw, false);

    if (aborted) {
        scp_writer_free(writer);
        emit error(tr("Capture cancelled by user."));
        return;
    }

    emit stageChanged(tr("Writing SCP file…"));
    int save_rc = scp_writer_save(writer, m_outputPath.toLocal8Bit().constData());
    scp_writer_free(writer);

    if (save_rc != 0) {
        emit error(tr("Failed to write SCP file (rc=%1).").arg(save_rc));
        return;
    }

    emit progress(100);
    QString msg = tr("Captured %1×%2 tracks to %3")
                      .arg(m_cylinders).arg(m_sides).arg(QFileInfo(m_outputPath).fileName());
    if (errors > 0) {
        msg += tr(" — %1 read errors logged (track entries omitted, no fabricated samples).").arg(errors);
    }
    emit finished(msg);
}
