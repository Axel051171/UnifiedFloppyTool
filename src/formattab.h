/**
 * @file formattab.h
 * @brief Format/Settings Tab Widget - GUI↔Backend Integration
 * 
 * Implements cascading dependencies + Preset management
 * 
 * @date 2026-01-12
 */

#ifndef FORMATTAB_H
#define FORMATTAB_H

#include <QWidget>
#include "advanceddialogs.h"
#include <QSettings>
#include <QMap>
#include <QStringList>

extern "C" {
    #include "uft/uft_protection.h"
}

namespace Ui { class TabFormat; }


class FormatTab : public QWidget {
    Q_OBJECT

public:
    explicit FormatTab(QWidget *parent = nullptr);
    ~FormatTab();

    // ========================================================================
    // Format Data Structures
    // ========================================================================
    
    struct FormatInfo {
        QString name;
        QString description;
        QStringList versions;
        bool supportsHalfTracks;
        bool supportsWeakBits;
        bool supportsFlux;
        bool supportsGCR;
        bool supportsMFM;
        int defaultTracks;
        int defaultSectors;
        int defaultSectorSize;
    };
    
    /**
     * @brief Preset data structure
     */
    /**
     * @brief Read options configuration
     */
    struct ReadOptions {
        QString speed;           // Maximum, Auto, 1x, 2x
        bool ignoreErrors;       // Continue on bad sectors
        bool fastErrorSkip;      // Skip quickly
        bool advancedScanning;   // A.S.S mode
        int scanFactor;          // 1-1000
        bool readTimingData;     // Flux timing
        bool dpmAnalysis;        // Data Position Measurement
        QString dpmAccuracy;     // Normal, High, Maximum
        bool readSubChannel;     // Weak bits, extra revolutions
    };
    
    /**
     * @brief Write options configuration
     */
    struct WriteOptions {
        bool verifyAfterWrite;
        bool ignoreErrors;
        bool writeTimingData;
        bool correctSubChannel;
    };


    struct Preset {
        QString name;
        QString system;
        QString format;
        QString version;
        QString encoding;
        int tracks;
        int heads;
        QString density;
        bool halfTracks;
        bool preserveTiming;
        bool adaptivePLL;
        QString copyMode;
        QString gcrType;
        bool detectProtection;
    };

    // ========================================================================
    // Protection Settings API
    // ========================================================================
    
    uint32_t getProtectionFlags() const;
    uft_platform_t getPlatformHint() const;
    
    // Read/Write Options API
    ReadOptions getReadOptions() const;
    WriteOptions getWriteOptions() const;
    bool isPreserveProtection() const;
    void getProtectionConfig(uft_prot_config_t* config) const;
    
    // ========================================================================
    // Format Settings API
    // ========================================================================
    
    QString getSelectedSystem() const;
    QString getSelectedFormat() const;
    QString getSelectedVersion() const;
    
    // ========================================================================
    // Preset API
    // ========================================================================
    
    Preset getCurrentSettings() const;
    void applyPreset(const Preset& preset);
    QStringList getPresetNames() const;

signals:
    void protectionSettingsChanged();
    void formatSettingsChanged();
    void readOptionsChanged();
    void writeOptionsChanged();
    void systemChanged(const QString& system);
    void formatChanged(const QString& format);

public slots:
    void loadSettings();
    void saveSettings();

private slots:
    // Advanced dialog handlers
    void onFluxAdvanced();
    void onPLLAdvanced();
    void onNibbleAdvanced();
    
    // System/Format cascade
    void onSystemChanged(int index);
    void onFormatChanged(int index);
    void onVersionChanged(int index);
    void onEncodingChanged(int index);
    
    // Protection detection
    void onDetectAllToggled(bool checked);
    void onPlatformChanged(int index);
    void onProtectionCheckChanged();
    
    // PLL/Flux options
    void onAdaptivePLLToggled(bool checked);
    void onPreserveTimingToggled(bool checked);
    
    // XCopy dependencies
    void onCopyModeChanged(int index);
    void onAllTracksToggled(bool checked);
    
    // Nibble dependencies
    void onGCRTypeChanged(int index);
    
    // Write dependencies
    void onRetryErrorsToggled(bool checked);
    
    // Logging dependencies
    void onLogToFileToggled(bool checked);
    void onBrowseLogPath();
    
    // Forensic dependencies
    void onValidateStructureToggled(bool checked);
    void onReportFormatChanged(int index);
    
    // GW→DMK Direct
    void onGw2DmkOpenClicked();
    
    // Preset management
    void onLoadPreset();
    void onSavePreset();
    void onPresetChanged(int index);
    
    // Read/Write Options
    void onReadSpeedChanged(int index);
    void onIgnoreReadErrorsChanged(bool checked);
    void onFastErrorSkipChanged(bool checked);
    void onAdvancedScanningChanged(bool checked);
    void onScanFactorChanged(int value);
    void onReadTimingDataChanged(bool checked);
    void onDPMAnalysisChanged(bool checked);
    void onReadSubChannelChanged(bool checked);
    void onVerifyAfterWriteChanged(bool checked);
    void onIgnoreWriteErrorsChanged(bool checked);
    void onWriteTimingDataChanged(bool checked);
    void onCorrectSubChannelChanged(bool checked);

private:
    Ui::TabFormat *ui;
    
    // Advanced dialog parameters
    FluxAdvancedDialog::FluxAdvancedParams m_fluxAdvParams;
    PLLAdvancedDialog::PLLAdvancedParams m_pllAdvParams;
    NibbleAdvancedDialog::NibbleAdvancedParams m_nibbleAdvParams;
    
    // Format database
    QMap<QString, QStringList> m_systemFormats;
    QMap<QString, FormatInfo> m_formatInfo;
    
    // Preset storage
    QMap<QString, Preset> m_presets;
    QString m_presetsFilePath;
    
    void setupConnections();
    void setupFormatDatabase();
    void setupInitialState();
    void setupBuiltinPresets();
    
    void populateFormatsForSystem(const QString& system);
    void populateSystemCombo();
    void populateVersionsForFormat(const QString& format);
    void updateFormatSpecificOptions(const QString& format);
    void syncProtectionWidgets(bool detectAll);
    void updatePLLOptions(bool enabled);
    void updateFluxOptions(bool isFluxFormat);
    void updateGCROptions(bool isGCRFormat);
    
    // XCopy helpers
    void updateXCopyTrackRange(bool allTracks);
    void updateXCopyModeOptions(const QString& mode);
    
    // Nibble helpers
    void updateNibbleOptions(const QString& gcrType);
    
    // Write helpers
    void updateRetryOptions(bool enabled);
    
    // Logging helpers
    void updateLogFileOptions(bool enabled);
    
    // Forensic helpers
    void updateForensicValidation(bool enabled);
    void updateForensicReport(const QString& format);
    
    // Preset helpers
    void loadPresetsFromFile();
    void savePresetsToFile();
    void updatePresetCombo();
    
    static constexpr const char* SETTINGS_GROUP = "FormatTab";
};

#endif // FORMATTAB_H
