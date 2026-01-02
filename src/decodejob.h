/**
 * @file decodejob.h
 * @brief Worker Thread for Disk Decoding Operations
 * 
 * FIX: Critical - UI Thread Freeze Issue
 * 
 * PROBLEM: Decoding operations were running in UI thread causing:
 * - "Not Responding" freezes
 * - No progress updates
 * - No cancel capability
 * 
 * SOLUTION: QThread-based worker with signals/slots
 * 
 * USAGE:
 *   auto* thread = new QThread(this);
 *   auto* job = new DecodeJob();
 *   job->moveToThread(thread);
 *   
 *   connect(thread, &QThread::started, job, &DecodeJob::run);
 *   connect(job, &DecodeJob::progress, this, &MainWindow::onProgress);
 *   connect(job, &DecodeJob::finished, thread, &QThread::quit);
 *   connect(job, &DecodeJob::finished, job, &QObject::deleteLater);
 *   connect(thread, &QThread::finished, thread, &QObject::deleteLater);
 *   
 *   thread->start();
 */

#ifndef DECODEJOB_H
#define DECODEJOB_H

#include <QObject>
#include <QString>
#include <atomic>

/**
 * @brief Worker class for disk decode operations
 * 
 * Runs in separate thread to prevent UI freeze.
 * Provides progress updates and cancel capability.
 */
class DecodeJob : public QObject
{
    Q_OBJECT
    
public:
    explicit DecodeJob(QObject *parent = nullptr);
    ~DecodeJob();
    
    /**
     * @brief Request cancellation of running job
     * 
     * Sets atomic flag that worker checks periodically.
     * Not immediate - worker must check cancel state.
     */
    void requestCancel();
    
    /**
     * @brief Check if job was cancelled
     * @return true if cancel was requested
     */
    bool isCancelled() const;
    
signals:
    /**
     * @brief Progress update (0-100%)
     * @param percentage Current progress
     */
    void progress(int percentage);
    
    /**
     * @brief Current operation stage changed
     * @param stage Description of current stage
     */
    void stageChanged(const QString& stage);
    
    /**
     * @brief Track/sector update for visualization
     * @param track Track number
     * @param sector Sector number
     * @param status Status string ("OK", "CRC_BAD", etc.)
     */
    void sectorUpdate(int track, int sector, const QString& status);
    
    /**
     * @brief Job completed successfully
     * @param resultMessage Summary of results
     */
    void finished(const QString& resultMessage);
    
    /**
     * @brief Job failed with error
     * @param errorMessage Error description
     */
    void error(const QString& errorMessage);
    
public slots:
    /**
     * @brief Main worker function
     * 
     * This runs in the worker thread.
     * Calls C core functions for decoding.
     * Emits progress signals.
     * Checks cancel flag periodically.
     */
    void run();
    
private:
    /**
     * @brief Atomic cancel flag
     * 
     * Set by UI thread via requestCancel().
     * Checked by worker thread in run().
     * Uses relaxed memory ordering (sufficient for boolean flag).
     */
    std::atomic_bool m_cancel{false};
    
    /**
     * @brief Demo: Simulate decode operation
     * 
     * Real implementation would call:
     * - uft_mfm_decode_flux_scalar()
     * - CRC checking
     * - Sector extraction
     * - etc.
     */
    void performDecode();
};

#endif // DECODEJOB_H
