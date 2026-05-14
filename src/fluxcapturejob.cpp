/**
 * @file fluxcapturejob.cpp
 * @brief Implementation of FluxCaptureJob — see header for context (MF-110).
 */

#include "fluxcapturejob.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QThread>

#include <cstdint>
#include <cstdlib>
#include <variant>
#include <vector>

#include "hardware_providers/greaseweazle_provider_v2.h"
#include "uft/formats/uft_scp_writer.h"

FluxCaptureJob::FluxCaptureJob(QObject *parent)
    : QObject(parent)
    , m_provider(nullptr)
    , m_cylinders(80)
    , m_sides(2)
    , m_revolutions(2)
    , m_scpDiskType(SCP_TYPE_PC_DD)
    , m_cancel(false)
{
}

FluxCaptureJob::~FluxCaptureJob() = default;

void FluxCaptureJob::setProvider(::uft::hal::GreaseweazleProviderV2 *provider)
{
    m_provider = provider;
}
void FluxCaptureJob::setOutputPath(const QString &p)  { m_outputPath = p; }
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

    if (m_provider == nullptr) {
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

    scp_writer_t *writer = scp_writer_create(m_scpDiskType, (uint8_t)m_revolutions);
    if (!writer) {
        emit error(tr("Could not allocate SCP writer."));
        return;
    }

    /* MF-200 (P1.20): migrated to the V2 outcome surface.
     *  - uft_gw_select_drive() is gone — GreaseweazleProviderV2 asserts
     *    the bus unit lazily on the first do_* (MF-199).
     *  - uft_gw_select_head() is gone — head is a field of ReadFluxParams.
     *  - uft_gw_get_sample_freq() / ns_per_tick are gone — FluxCaptured
     *    delivers transitions already in nanoseconds, and the per-rev
     *    boundaries in FluxCaptured::index_times_ns (MF-194), also in ns. */

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
            scp_writer_free(writer);
            emit error(tr("Failed to start drive motor: %1").arg(motor_err));
            return;
        }
    }
    QThread::msleep(500);  // Let the drive spin up.

    int total_steps = m_cylinders * m_sides;
    int step = 0;
    int errors = 0;

    emit stageChanged(tr("Capturing flux (%1 cyl × %2 sides × %3 rev)…")
                      .arg(m_cylinders).arg(m_sides).arg(m_revolutions));
    emit progress(0);

    bool aborted = false;

    for (int cyl = 0; cyl < m_cylinders && !aborted; ++cyl) {
        if (isCancelled()) { aborted = true; break; }

        bool seek_ok = false;
        QString seek_err;
        std::visit(::uft::hal::overloaded{
            [&](const ::uft::hal::SeekArrived&)               { seek_ok = true; },
            [&](const ::uft::hal::SeekOvershot& o)            { seek_err = tr("overshot (requested %1, actual %2)").arg(o.requested).arg(o.actual); },
            [&](const ::uft::hal::SeekTrack0Failed& t)        { seek_err = QString::fromStdString(t.reason); },
            [&](const ::uft::hal::CapabilityRequiresPolicy& p){ seek_err = QString::fromStdString(p.explain); },
            [&](const ::uft::hal::HardwareDisconnected& d)    { seek_err = tr("hardware disconnected (%1)").arg(QString::fromStdString(d.device_path)); aborted = true; },
            [&](const ::uft::hal::ProviderError& e)           { seek_err = QString::fromStdString(e.what); },
        }, m_provider->seek(cyl));
        if (!seek_ok) {
            ++errors;
            qWarning() << "Seek to cyl" << cyl << "failed:" << seek_err;
            // Honest path: skip but keep going. SCP writer simply has no
            // entry for this track — never silently zero-pads.
            step += m_sides;
            continue;
        }

        for (int side = 0; side < m_sides && !aborted; ++side) {
            if (isCancelled()) { aborted = true; break; }

            ::uft::hal::ReadFluxParams rp;
            rp.cylinder    = cyl;
            rp.head        = side;
            rp.revolutions = m_revolutions;

            std::visit(::uft::hal::overloaded{
                [&](const ::uft::hal::FluxCaptured& fc) {
                    /* Rule F-3: split the flux stream into per-revolution
                     * SCP track entries on the measured index boundaries.
                     * index_times_ns[r] is cumulative ns from capture
                     * start; transitions_ns is already ns — no ticks->ns
                     * conversion left. An EMPTY index_times_ns means the
                     * provider observed no index pulses: do NOT fabricate
                     * a single-revolution split, skip the track honestly. */
                    if (fc.index_times_ns.empty()) {
                        ++errors;
                        qWarning() << "cyl" << cyl << "side" << side
                                   << "- FluxCaptured carries no index"
                                      " boundaries; track omitted (no"
                                      " fabricated split)";
                        return;
                    }
                    const std::size_t n = fc.transitions_ns.size();
                    std::size_t   sample_start  = 0;
                    std::uint64_t cumulative_ns = 0;
                    std::uint32_t prev_index_ns = 0;
                    int           rev_in_writer = 0;
                    for (std::size_t r = 0;
                         r < fc.index_times_ns.size() && rev_in_writer < m_revolutions;
                         ++r) {
                        const std::uint32_t target_ns = fc.index_times_ns[r];
                        std::size_t sample_end = sample_start;
                        while (sample_end < n && cumulative_ns < target_ns) {
                            cumulative_ns += fc.transitions_ns[sample_end];
                            ++sample_end;
                        }
                        if (sample_end > sample_start) {
                            const std::uint32_t duration_ns = target_ns - prev_index_ns;
                            int rc2 = scp_writer_add_track(
                                writer, cyl, side,
                                fc.transitions_ns.data() + sample_start,
                                sample_end - sample_start,
                                duration_ns,
                                rev_in_writer);
                            if (rc2 != 0) {
                                ++errors;
                                qWarning() << "scp_writer_add_track rc=" << rc2;
                            }
                            ++rev_in_writer;
                        }
                        prev_index_ns = target_ns;
                        sample_start  = sample_end;
                    }
                },
                [&](const ::uft::hal::FluxMarginal& m) {
                    /* Marginal flux carries transitions but no index
                     * boundaries — cannot slice per revolution. Honest
                     * skip, log the anomaly (F-3: do not collapse it). */
                    ++errors;
                    qWarning() << "cyl" << cyl << "side" << side
                               << "- flux MARGINAL:"
                               << QString::fromStdString(m.anomaly_note);
                },
                [&](const ::uft::hal::FluxUnreadable& u) {
                    ++errors;
                    qWarning() << "cyl" << cyl << "side" << side
                               << "- flux unreadable:"
                               << QString::fromStdString(u.physical_reason);
                },
                [&](const ::uft::hal::CapabilityRequiresPolicy& p) {
                    ++errors;
                    qWarning() << "cyl" << cyl << "side" << side
                               << "- policy gate:"
                               << QString::fromStdString(p.explain);
                },
                [&](const ::uft::hal::HardwareDisconnected& d) {
                    /* The device is gone — aborting is the only honest
                     * option (V1 could not distinguish this case). */
                    aborted = true;
                    qWarning() << "cyl" << cyl << "side" << side
                               << "- hardware disconnected:"
                               << QString::fromStdString(d.device_path);
                },
                [&](const ::uft::hal::ProviderError& e) {
                    ++errors;
                    qWarning() << "cyl" << cyl << "side" << side
                               << "- read error:"
                               << QString::fromStdString(e.what);
                },
            }, m_provider->read_raw_flux(rp));

            ++step;
            int pct = (step * 100) / total_steps;
            emit progress(pct);
        }
    }

    /* Tearing down — the motor-stop outcome is intentionally discarded. */
    (void)m_provider->set_motor(false);

    if (aborted) {
        scp_writer_free(writer);
        emit error(isCancelled()
                   ? tr("Capture cancelled by user.")
                   : tr("Capture aborted — hardware disconnected mid-capture."));
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
