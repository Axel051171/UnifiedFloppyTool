/**
 * @file inputvalidation.h
 * @brief Input Validation Framework
 * 
 * CRITICAL FIX: Input Validation
 * 
 * PROBLEM:
 * - No validation before passing to C core
 * - Trust UI = crashes
 * - Invalid values cause undefined behavior
 * 
 * SOLUTION:
 * - Validate ALL inputs before C core calls
 * - Defensive programming
 * - Clear error messages
 * 
 * USAGE:
 *   int tracks = ui->spinTracks->value();
 *   
 *   if (!InputValidation::validateTracks(tracks)) {
 *       showError(InputValidation::lastError());
 *       return;
 *   }
 *   
 *   // Safe to proceed
 *   decode_disk(tracks);
 */

#ifndef INPUTVALIDATION_H
#define INPUTVALIDATION_H

#include <QString>
#include <QFileInfo>

/**
 * @brief Input validation utilities
 * 
 * Validates all user inputs before passing to C core.
 * Provides clear error messages for invalid inputs.
 */
class InputValidation
{
public:
    /* =========================================================================
     * DISK GEOMETRY VALIDATION
     * ========================================================================= */
    
    /**
     * @brief Validate track count
     * @param tracks Number of tracks
     * @return true if valid
     * 
     * Valid range: 1-200 tracks
     * Common values: 35, 40, 77, 80, 82, 83, 84
     */
    static bool validateTracks(int tracks)
    {
        if (tracks < 1 || tracks > 200) {
            setError(QString("Invalid track count: %1 (must be 1-200)").arg(tracks));
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate sector count
     * @param sectors Number of sectors per track
     * @return true if valid
     * 
     * Valid range: 1-64 sectors
     * Common values: 8, 9, 10, 16, 18, 21
     */
    static bool validateSectors(int sectors)
    {
        if (sectors < 1 || sectors > 64) {
            setError(QString("Invalid sector count: %1 (must be 1-64)").arg(sectors));
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate sector size
     * @param size Sector size in bytes
     * @return true if valid
     * 
     * Valid values: 128, 256, 512, 1024, 2048
     */
    static bool validateSectorSize(int size)
    {
        if (size != 128 && size != 256 && size != 512 && 
            size != 1024 && size != 2048) {
            setError(QString("Invalid sector size: %1 (must be 128/256/512/1024/2048)").arg(size));
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate side/head count
     * @param sides Number of sides
     * @return true if valid
     * 
     * Valid values: 1 or 2
     */
    static bool validateSides(int sides)
    {
        if (sides != 1 && sides != 2) {
            setError(QString("Invalid side count: %1 (must be 1 or 2)").arg(sides));
            return false;
        }
        return true;
    }
    
    /* =========================================================================
     * FILE VALIDATION
     * ========================================================================= */
    
    /**
     * @brief Validate input file exists
     * @param filepath Path to check
     * @return true if file exists and is readable
     */
    static bool validateInputFile(const QString& filepath)
    {
        QFileInfo info(filepath);
        
        if (!info.exists()) {
            setError(QString("File not found: %1").arg(filepath));
            return false;
        }
        
        if (!info.isFile()) {
            setError(QString("Not a file: %1").arg(filepath));
            return false;
        }
        
        if (!info.isReadable()) {
            setError(QString("File not readable: %1").arg(filepath));
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate output file path
     * @param filepath Path to check
     * @return true if path is writable
     */
    static bool validateOutputFile(const QString& filepath)
    {
        QFileInfo info(filepath);
        QFileInfo dirInfo(info.absolutePath());
        
        if (!dirInfo.exists()) {
            setError(QString("Directory does not exist: %1").arg(dirInfo.absolutePath()));
            return false;
        }
        
        if (!dirInfo.isWritable()) {
            setError(QString("Directory not writable: %1").arg(dirInfo.absolutePath()));
            return false;
        }
        
        if (info.exists() && !info.isWritable()) {
            setError(QString("File exists and is not writable: %1").arg(filepath));
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate file extension
     * @param filepath File path
     * @param extensions List of valid extensions (e.g., {"scp", "hfe", "img"})
     * @return true if extension is valid
     */
    static bool validateExtension(const QString& filepath, const QStringList& extensions)
    {
        QFileInfo info(filepath);
        QString ext = info.suffix().toLower();
        
        if (!extensions.contains(ext)) {
            setError(QString("Invalid file extension: .%1 (expected: %2)")
                    .arg(ext)
                    .arg(extensions.join(", ")));
            return false;
        }
        
        return true;
    }
    
    /* =========================================================================
     * TIMING/HARDWARE VALIDATION
     * ========================================================================= */
    
    /**
     * @brief Validate RPM value
     * @param rpm Rotations per minute
     * @return true if valid
     * 
     * Valid range: 200-400 RPM
     * Common values: 300 (standard), 360 (Commodore)
     */
    static bool validateRPM(int rpm)
    {
        if (rpm < 200 || rpm > 400) {
            setError(QString("Invalid RPM: %1 (must be 200-400)").arg(rpm));
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate bitrate/data rate
     * @param bitrate Bitrate in kbps
     * @return true if valid
     * 
     * Valid range: 125-1000 kbps
     * Common values: 250 (DD), 500 (HD)
     */
    static bool validateBitrate(int bitrate)
    {
        if (bitrate < 125 || bitrate > 1000) {
            setError(QString("Invalid bitrate: %1 kbps (must be 125-1000)").arg(bitrate));
            return false;
        }
        return true;
    }
    
    /* =========================================================================
     * ENCODING VALIDATION
     * ========================================================================= */
    
    /**
     * @brief Validate encoding type
     * @param encoding Encoding name
     * @return true if valid
     * 
     * Valid values: "MFM", "FM", "GCR"
     */
    static bool validateEncoding(const QString& encoding)
    {
        QString enc = encoding.toUpper();
        
        if (enc != "MFM" && enc != "FM" && enc != "GCR") {
            setError(QString("Invalid encoding: %1 (must be MFM, FM, or GCR)").arg(encoding));
            return false;
        }
        
        return true;
    }
    
    /* =========================================================================
     * ERROR HANDLING
     * ========================================================================= */
    
    /**
     * @brief Get last validation error
     * @return Error message
     */
    static QString lastError()
    {
        return s_lastError;
    }
    
    /**
     * @brief Clear error state
     */
    static void clearError()
    {
        s_lastError.clear();
    }
    
private:
    /**
     * @brief Set error message
     * @param error Error description
     */
    static void setError(const QString& error)
    {
        s_lastError = error;
    }
    
    static QString s_lastError;
};

// Initialize static member
QString InputValidation::s_lastError;

#endif // INPUTVALIDATION_H
