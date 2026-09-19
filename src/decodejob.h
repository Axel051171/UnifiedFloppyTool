#pragma once
/**
 * @file decodejob.h
 * @brief Worker Thread for Disk Decoding Operations
 * 
 * P0-GUI-001 FIX: Real Backend Integration
 * 
 * Uses UFT Unified API for actual disk image processing:
 * - uft_load() for image loading
 * - uft_verify() for sector verification  
 * - uft_get_track_info() for track analysis
 * 
 * USAGE:
 *   auto* thread = new QThread(this);
 *   auto* job = new DecodeJob();
 *   job->setSourcePath("/path/to/disk.adf");
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
/* MF-1265: der Kopierplan gilt auch hier (`P3-509`, zweiter Halbsatz
 * der Vorgabe: „FluxCopy + DeepCopy + Protected + Evidence ist EIN
 * Vorgang"). */
#include "uft/core/uft_copy_plan.h"

/**
 * @brief Image information returned after decode
 */
struct DecodeResult {
    QString formatName;
    QString platformName;
    QString volumeName;
    int tracks = 0;
    int heads = 0;
    int sectorsPerTrack = 0;
    int sectorSize = 0;
    qint64 totalSize = 0;
    int goodSectors = 0;
    int badSectors = 0;
    int totalSectors = 0;
    bool hasErrors = false;
};

/**
 * @brief Worker class for disk decode operations
 * 
 * Runs in separate thread to prevent UI freeze.
 * Uses UFT C backend for actual decoding.
 * Provides progress updates and cancel capability.
 */
class DecodeJob : public QObject
{
    Q_OBJECT
    
public:
    explicit DecodeJob(QObject *parent = nullptr);
    ~DecodeJob();
    
    /**
     * @brief Set source file path
     * @param path Path to disk image (ADF, D64, SCP, etc.)
     */
    void setSourcePath(const QString& path);
    
    /**
     * @brief Set destination path for conversion (optional)
     * @param path Output path
     * @param format Output format (e.g., "ADF", "D64")
     */
    void setDestination(const QString& path, const QString& format = QString());

    /**
     * @brief Den Kopierplan fuer diesen Auftrag setzen (MF-1265).
     *
     * Gespeichert wird eine **Kopie**, und das ist kein Zufall: dieser
     * Auftrag laeuft nach `moveToThread()` in einem eigenen Faden. Ein
     * Zeiger auf einen Reiter waere ein Zugriff auf die Oberflaeche aus
     * einem fremden Faden; eine Rueckruffunktion ebenso. `uft_copy_plan_t`
     * ist ein reiner Wertetyp, also ist die Kopie billig und sicher.
     *
     * Wer ihn nicht setzt, bekommt die Vorgaben wie bisher — `run()`
     * fragt `m_planGesetzt`, nicht den Inhalt. Ein genullter Plan ist
     * naemlich ein GUELTIGER Plan (FILE/FAST/LOGICAL/NORMAL), und ihn von
     * „kein Plan" zu unterscheiden ist genau der Punkt.
     */
    void setCopyPlan(const uft_copy_plan_t& plan);


    /**
     * @brief Get decode results after completion
     */
    DecodeResult result() const { return m_result; }
    
    /**
     * @brief Request cancellation of running job
     */
    void requestCancel();
    
    /**
     * @brief Check if job was cancelled
     */
    bool isCancelled() const;
    
signals:
    /**
     * @brief Progress update (0-100%)
     */
    void progress(int percentage);
    
    /**
     * @brief Current operation stage changed
     */
    void stageChanged(const QString& stage);
    
    /**
     * @brief Track/sector update for visualization
     * @param track Track number
     * @param sector Sector number  
     * @param status Status string ("OK", "CRC_BAD", "MISSING", etc.)
     */
    void sectorUpdate(int track, int sector, const QString& status);
    
    /**
     * @brief Image information available
     */
    void imageInfo(const DecodeResult& info);
    
    /**
     * @brief Job completed successfully
     */
    void finished(const QString& resultMessage);
    
    /**
     * @brief Job failed with error
     */
    void error(const QString& errorMessage);
    
public slots:
    /**
     * @brief Main worker function - runs in worker thread
     */
    void run();
    
private:
    std::atomic_bool m_cancel{false};
    QString m_sourcePath;
    QString m_destPath;
    QString m_destFormat;
    /* MF-1265: eine KOPIE des Plans, kein Zeiger — siehe setCopyPlan(). */
    uft_copy_plan_t m_plan{};
    bool m_planGesetzt = false;
    DecodeResult m_result;
    
    /**
     * @brief Load and analyze image using UFT backend
     */
    bool loadImage();
    
    /**
     * @brief Verify all sectors
     */
    bool verifySectors();
    
    /**
     * @brief Convert to destination format (if set)
     */
    bool convertImage();
};

#endif // DECODEJOB_H
