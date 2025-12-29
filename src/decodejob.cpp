/**
 * @file decodejob.cpp
 * @brief Worker Thread Implementation for Disk Decoding
 */

#include "decodejob.h"
#include <QThread>
#include <QDebug>

DecodeJob::DecodeJob(QObject *parent)
    : QObject(parent)
    , m_cancel(false)
{
    qDebug() << "DecodeJob created";
}

DecodeJob::~DecodeJob()
{
    qDebug() << "DecodeJob destroyed";
}

void DecodeJob::requestCancel()
{
    m_cancel.store(true, std::memory_order_relaxed);
    qDebug() << "Cancel requested";
}

bool DecodeJob::isCancelled() const
{
    return m_cancel.load(std::memory_order_relaxed);
}

void DecodeJob::run()
{
    qDebug() << "DecodeJob::run() started in thread" << QThread::currentThreadId();
    
    emit stageChanged("Initializing...");
    QThread::msleep(100);
    
    if (isCancelled()) {
        emit error("Operation cancelled by user");
        return;
    }
    
    emit stageChanged("Reading flux data...");
    emit progress(10);
    
    // Simulate reading flux (real implementation would call C functions)
    for (int i = 0; i < 5; i++) {
        if (isCancelled()) {
            emit error("Operation cancelled during flux read");
            return;
        }
        QThread::msleep(200);
        emit progress(10 + i * 5);
    }
    
    emit stageChanged("Decoding MFM...");
    emit progress(35);
    
    // Simulate decoding (real implementation would call uft_mfm_decode_flux_scalar)
    performDecode();
    
    if (isCancelled()) {
        emit error("Operation cancelled during decode");
        return;
    }
    
    emit stageChanged("Verifying sectors...");
    emit progress(70);
    
    // Simulate sector verification
    for (int track = 0; track < 80; track++) {
        for (int sector = 0; sector < 9; sector++) {
            if (isCancelled()) {
                emit error("Operation cancelled during verification");
                return;
            }
            
            // Emit sector status for visualization
            QString status = (track == 10 && sector == 5) ? "CRC_BAD" : "OK";
            emit sectorUpdate(track, sector, status);
        }
        
        if (track % 10 == 0) {
            emit progress(70 + (track * 30 / 80));
        }
    }
    
    emit stageChanged("Finalizing...");
    emit progress(95);
    QThread::msleep(200);
    
    emit progress(100);
    emit finished("Decode complete! 79/80 tracks OK, 1 track with CRC errors");
    
    qDebug() << "DecodeJob::run() completed successfully";
}

void DecodeJob::performDecode()
{
    // REAL IMPLEMENTATION WOULD BE:
    /*
    #include "uft/uft_mfm_scalar.h"
    
    // Decode flux to bitstream
    uint64_t flux_data[1000];
    uint8_t output_bits[2000];
    
    size_t decoded_bytes = uft_mfm_decode_flux_scalar(
        flux_data, 
        1000, 
        output_bits
    );
    
    // Check CRC
    // Extract sectors
    // etc.
    */
    
    // For now: simulate with sleep
    for (int i = 0; i < 10; i++) {
        if (isCancelled()) {
            return;
        }
        QThread::msleep(100);
        emit progress(35 + i * 3);
    }
}
