#pragma once
/**
 * @file fluxcapturejob.h
 * @brief Worker thread that drives a Greaseweazle through a full disk
 *        capture and writes the result as an SCP flux image.
 *
 * Closes MF-110 — the WorkflowTab "Source = Flux Device, Destination =
 * Image File" path used to abort with "No source file specified" because
 * DecodeJob was the only worker and it expected a file. FluxCaptureJob
 * is the missing first stage: it speaks to the C HAL (uft_gw_*) and
 * produces the .scp file that DecodeJob would otherwise have wanted.
 *
 * Honesty rule: this job touches forensic raw data. It must NEVER
 * fabricate a sample on a USB read failure — partial captures are
 * reported as errors and the SCP writer is freed without saving.
 */

#ifndef UFT_FLUXCAPTUREJOB_H
#define UFT_FLUXCAPTUREJOB_H

#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>

/* MF-200 (P1.20): FluxCaptureJob now drives the V2 outcome surface via a
 * non-owning GreaseweazleProviderV2 pointer instead of a raw
 * uft_gw_device_t* — the provider is owned by HardwareTab. */
namespace uft::hal { class GreaseweazleProviderV2; }

class FluxCaptureJob : public QObject
{
    Q_OBJECT

public:
    explicit FluxCaptureJob(QObject *parent = nullptr);
    ~FluxCaptureJob() override;

    // Configuration — all setters must be called before run() unless the
    // default is acceptable. Setters are no-ops once run() has started.
    /** Non-owning. The provider is owned by HardwareTab; the drive unit
     *  is already bound on the provider (ctor / set_drive_unit). */
    void setProvider(::uft::hal::GreaseweazleProviderV2 *provider);
    void setOutputPath(const QString &path);
    void setGeometry(int cylinders, int sides);  // typical 80x2
    void setRevolutions(int revs);               // 1..5, clamped
    void setScpDiskType(uint8_t type);           // SCP_TYPE_*

    void requestCancel();
    bool isCancelled() const;

public slots:
    void run();

signals:
    void progress(int percent);
    void stageChanged(const QString &stage);
    void error(const QString &msg);
    void finished(const QString &resultMsg);

private:
    ::uft::hal::GreaseweazleProviderV2 *m_provider;  // non-owning (HardwareTab owns)
    QString m_outputPath;
    int m_cylinders;
    int m_sides;
    int m_revolutions;
    uint8_t m_scpDiskType;
    std::atomic<bool> m_cancel;
};

#endif // UFT_FLUXCAPTUREJOB_H
