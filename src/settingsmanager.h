/**
 * @file settingsmanager.h
 * @brief Application Settings Manager with Proper Lifecycle
 * 
 * CRITICAL FIX: Settings Lifecycle
 * 
 * PROBLEM:
 * - Settings "verpuffen" (get lost)
 * - UI shows one value, backend uses another
 * - No synchronization between UI and core
 * 
 * SOLUTION:
 * - Single Source of Truth (SSOT) pattern
 * - Load → Apply → Save cycle
 * - Signals for changes
 * - Automatic persistence
 * 
 * USAGE:
 *   // On startup:
 *   SettingsManager* settings = SettingsManager::instance();
 *   settings->load();
 *   applySettingsToUI();
 *   
 *   // On change:
 *   settings->setTracks(newValue);  // ← Automatically saved!
 *   
 *   // On shutdown:
 *   settings->save();  // ← Explicit save
 */

#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>

/**
 * @brief Global settings manager (Singleton)
 * 
 * Manages application settings with automatic persistence.
 * Emits signals when settings change.
 * Single source of truth for all configuration.
 */
class SettingsManager : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @brief Get singleton instance
     * @return Global settings manager
     */
    static SettingsManager* instance()
    {
        static SettingsManager s_instance;
        return &s_instance;
    }
    
    /**
     * @brief Load settings from disk
     * 
     * Call once at application startup.
     * Loads from platform-specific location:
     * - Windows: Registry or AppData/Local
     * - macOS: ~/Library/Preferences
     * - Linux: ~/.config
     */
    void load()
    {
        m_tracks = m_settings.value("decode/tracks", 80).toInt();
        m_sectors = m_settings.value("decode/sectors", 9).toInt();
        m_sectorSize = m_settings.value("decode/sectorSize", 512).toInt();
        m_sides = m_settings.value("decode/sides", 2).toInt();
        m_encoding = m_settings.value("decode/encoding", "MFM").toString();
        m_rpm = m_settings.value("hardware/rpm", 300).toInt();
        m_bitrate = m_settings.value("hardware/bitrate", 250).toInt();
        m_outputDir = m_settings.value("paths/outputDir", "").toString();
        m_autoSave = m_settings.value("ui/autoSave", true).toBool();
        m_showProgress = m_settings.value("ui/showProgress", true).toBool();
        
        emit settingsLoaded();
    }
    
    /**
     * @brief Save settings to disk
     * 
     * Call at application shutdown or after critical changes.
     * Auto-save also happens on each setter if enabled.
     */
    void save()
    {
        m_settings.setValue("decode/tracks", m_tracks);
        m_settings.setValue("decode/sectors", m_sectors);
        m_settings.setValue("decode/sectorSize", m_sectorSize);
        m_settings.setValue("decode/sides", m_sides);
        m_settings.setValue("decode/encoding", m_encoding);
        m_settings.setValue("hardware/rpm", m_rpm);
        m_settings.setValue("hardware/bitrate", m_bitrate);
        m_settings.setValue("paths/outputDir", m_outputDir);
        m_settings.setValue("ui/autoSave", m_autoSave);
        m_settings.setValue("ui/showProgress", m_showProgress);
        
        m_settings.sync();
        
        emit settingsSaved();
    }
    
    /* =========================================================================
     * DISK GEOMETRY SETTINGS
     * ========================================================================= */
    
    int tracks() const { return m_tracks; }
    void setTracks(int value)
    {
        if (m_tracks != value) {
            m_tracks = value;
            emit tracksChanged(value);
            autoSave();
        }
    }
    
    int sectors() const { return m_sectors; }
    void setSectors(int value)
    {
        if (m_sectors != value) {
            m_sectors = value;
            emit sectorsChanged(value);
            autoSave();
        }
    }
    
    int sectorSize() const { return m_sectorSize; }
    void setSectorSize(int value)
    {
        if (m_sectorSize != value) {
            m_sectorSize = value;
            emit sectorSizeChanged(value);
            autoSave();
        }
    }
    
    int sides() const { return m_sides; }
    void setSides(int value)
    {
        if (m_sides != value) {
            m_sides = value;
            emit sidesChanged(value);
            autoSave();
        }
    }
    
    /* =========================================================================
     * ENCODING SETTINGS
     * ========================================================================= */
    
    QString encoding() const { return m_encoding; }
    void setEncoding(const QString& value)
    {
        if (m_encoding != value) {
            m_encoding = value;
            emit encodingChanged(value);
            autoSave();
        }
    }
    
    /* =========================================================================
     * HARDWARE SETTINGS
     * ========================================================================= */
    
    int rpm() const { return m_rpm; }
    void setRPM(int value)
    {
        if (m_rpm != value) {
            m_rpm = value;
            emit rpmChanged(value);
            autoSave();
        }
    }
    
    int bitrate() const { return m_bitrate; }
    void setBitrate(int value)
    {
        if (m_bitrate != value) {
            m_bitrate = value;
            emit bitrateChanged(value);
            autoSave();
        }
    }
    
    /* =========================================================================
     * PATH SETTINGS
     * ========================================================================= */
    
    QString outputDir() const { return m_outputDir; }
    void setOutputDir(const QString& value)
    {
        if (m_outputDir != value) {
            m_outputDir = value;
            emit outputDirChanged(value);
            autoSave();
        }
    }
    
    /* =========================================================================
     * UI SETTINGS
     * ========================================================================= */
    
    bool autoSave() const { return m_autoSave; }
    void setAutoSave(bool value)
    {
        if (m_autoSave != value) {
            m_autoSave = value;
            emit autoSaveChanged(value);
            save();  // Always save this immediately
        }
    }
    
    bool showProgress() const { return m_showProgress; }
    void setShowProgress(bool value)
    {
        if (m_showProgress != value) {
            m_showProgress = value;
            emit showProgressChanged(value);
            autoSave();
        }
    }
    
signals:
    // Lifecycle signals
    void settingsLoaded();
    void settingsSaved();
    
    // Value change signals
    void tracksChanged(int value);
    void sectorsChanged(int value);
    void sectorSizeChanged(int value);
    void sidesChanged(int value);
    void encodingChanged(const QString& value);
    void rpmChanged(int value);
    void bitrateChanged(int value);
    void outputDirChanged(const QString& value);
    void autoSaveChanged(bool value);
    void showProgressChanged(bool value);
    
private:
    /**
     * @brief Private constructor (Singleton)
     */
    SettingsManager()
        : m_settings("UnifiedFloppyTool", "UFT")
        , m_tracks(80)
        , m_sectors(9)
        , m_sectorSize(512)
        , m_sides(2)
        , m_encoding("MFM")
        , m_rpm(300)
        , m_bitrate(250)
        , m_autoSave(true)
        , m_showProgress(true)
    {
    }
    
    ~SettingsManager()
    {
        save();  // Auto-save on exit
    }
    
    // Prevent copy/move
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    
    /**
     * @brief Auto-save if enabled
     */
    void autoSave()
    {
        if (m_autoSave) {
            save();
        }
    }
    
    QSettings m_settings;
    
    // Disk geometry
    int m_tracks;
    int m_sectors;
    int m_sectorSize;
    int m_sides;
    
    // Encoding
    QString m_encoding;
    
    // Hardware
    int m_rpm;
    int m_bitrate;
    
    // Paths
    QString m_outputDir;
    
    // UI
    bool m_autoSave;
    bool m_showProgress;
};

#endif // SETTINGSMANAGER_H
