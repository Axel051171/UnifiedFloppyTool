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
    #include "uft/core/uft_copy_plan.h"   /* MF-1233: der Kopierplan */
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
    // Kopierplan (MF-1233)
    // ========================================================================

    /** Der Plan, wie ihn die vier Auswahlfelder gerade beschreiben. */
    uft_copy_plan_t copyPlan() const;

    /* MF-1265 (`P3-509`, zweiter Halbsatz): hier stand eine statische
     * Auskunft `aktuellerPlan()`. Sie ist in den KERN gewandert —
     * `uft_copy_plan_current()` in `uft_copy_plan.h`. Der Grund kam
     * vom BINDER, nicht von mir: `toolstab.cpp` haette damit an
     * `formattab.cpp` gehangen, und zwei Qt-Tests binden das eine ohne
     * das andere. Der Reiter MELDET seinen Plan jetzt an
     * (`uft_copy_plan_set_quelle` im Konstruktor, NULL im Destruktor),
     * statt dass andere Reiter ihn befragen. */

    /** Die Faehigkeiten des gewaehlten Formats als Bitmaske fuer
     *  `uft_copy_plan_check()`. Was nicht gemessen werden kann, bleibt
     *  ungesetzt — dann meldet die Pruefung es, statt es anzunehmen. */
    uint32_t copyPlanCaps() const;

    /** Plan auf die Oberflaeche anwenden: Befunde und erzwungene Werte
     *  anzeigen, betroffene Bedienelemente sperren oder ausblenden. */
    void applyCopyPlan();

    /** MF-1237: das Profilfeld aus dem Kern fuellen, mit Grund bei den
     *  Profilen, die fuer dieses Format nicht taugen. */
    void fuelleProfile();

    /** Ein Profil anwenden; leere Kennung heisst „Benutzerdefiniert". */
    void wendeProfilAn(const QString &id);

    /** Die vier Achsen sperren (false) oder freigeben (true). */
    void setPlanFrei(bool frei);

    /** MF-1238: der Plan als JSON, wie ihn der KERN schreibt.
     *
     *  Zweistufig: erst die Laenge beim Kern erfragen, dann genau so
     *  viel bereitstellen. Ein fester Puffer waere eine stille Kuerzung,
     *  sobald ein Plan mehr erzwingt als hineinpasst. */
    QString planJson() const;

    /** MF-1238: die Zeile „was das Format traegt" nachziehen. */
    void zeigeCaps();
    
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

    // Kopierplan (MF-1233)
    void onCopyPlanChanged(int index);
    void onCopyPlanToggled(bool checked);   // MF-1235: Hashsatz
    void onCopyProfileChanged(int index);   // MF-1237: benannter Modus
    void onPlanAnpassen();                  // MF-1237: Achsen entsperren
    void onPlanJsonToggled(bool checked);   // MF-1238: JSON-Ansicht
    void onPlanJsonKopieren();              // MF-1238
    void onPlanJsonSichern();               // MF-1238
    
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

    /* MF-1237: der benannte Kopiermodus.
     *
     * `m_profil` ist leer, solange die vier Achsen frei bearbeitet
     * werden („Benutzerdefiniert"); `m_basisProfil` merkt sich, WOVON
     * ausgegangen wurde, damit der Knopf zurueckfuehren kann. */
    QString m_profil;
    QString m_basisProfil;
    bool    m_planFrei = false;

    // Advanced dialog parameters
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

    /**
     * @brief Bedienelemente nach dem Faehigkeits-Manifest des Plugins zeigen
     *        oder ausblenden (MF-661).
     *
     * Quelle ist `uft_plugin_control_visibility()` — also das, was das
     * Plugin ueber SICH SELBST sagt, nicht die handgepflegte Tabelle
     * `m_formatInfo`. Die beiden widersprechen sich an fuenf Stellen
     * gemessen (MF-660); fuer ein Bedienelement gilt das Plugin.
     *
     * Findet sich kein Plugin zum Namen, wird NICHTS ausgeblendet.
     * Auf Unwissen zu verstecken naehme dem Benutzer Funktion wegen
     * eines Nachschlagefehlers von uns.
     */
    void applyPluginCapabilities(const QString& format);
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
