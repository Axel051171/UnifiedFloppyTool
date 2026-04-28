#pragma once
/**
 * @file fluxwritejob.h
 * @brief Worker thread that writes an SCP flux image to a real disk via
 *        the Greaseweazle HAL. Symmetric counterpart to FluxCaptureJob.
 *
 * Closes MF-114 (the destination half of MF-110): the WorkflowTab combo
 * "Source = Image File, Destination = Flux Device" used to fall through
 * the legacy DecodeJob path which has no idea how to talk to hardware,
 * so the operation either silently did nothing or aborted with a
 * misleading file-related error.
 *
 * Forensic-integrity rules: this job does NOT decode the SCP, does NOT
 * remap tracks, and does NOT re-time anything. It writes flux intervals
 * exactly as they came out of the SCP file, with the only conversion
 * being ns → device ticks against the live sample_freq. A short write,
 * a track read failure, or a HAL error stops the run; we never silently
 * skip to the next track and pretend the disk was written.
 */

#ifndef UFT_FLUXWRITEJOB_H
#define UFT_FLUXWRITEJOB_H

#include <QObject>
#include <QString>
#include <atomic>

class FluxWriteJob : public QObject
{
    Q_OBJECT

public:
    explicit FluxWriteJob(QObject *parent = nullptr);
    ~FluxWriteJob() override;

    void setDevice(void *gwDevice);             // uft_gw_device_t*
    void setInputPath(const QString &path);     // .scp source
    void setDriveUnit(int unit);                // 0 or 1
    void setVerify(bool verify);                // post-write read+compare (off by default)

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
    void *m_gwDevice;
    QString m_inputPath;
    int m_driveUnit;
    bool m_verify;
    std::atomic<bool> m_cancel;
};

#endif // UFT_FLUXWRITEJOB_H
