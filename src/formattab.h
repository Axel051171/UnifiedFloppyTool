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
    /* MF-1293: der Arbeitsablauf braucht die Umdrehungen, fuehrt sie
     * aber nicht mehr selbst. Ein Leser statt eines zweiten Feldes. */
    int leseUmdrehungen() const;

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
    /* MF-1293: die Felder, die mit dem neuen Formular dazugekommen
     * sind — Abkuerzungen, gesperrte Haken ohne Kernparameter und
     * der Rest, der wenigstens eine Aenderung melden soll. */
    void verdrahteNeueFelder();
    /* MF-1293: ersetzt das gestapelte Layout jedes Traegers mit
     * `uft_flow = topdown` durch ein Spalten-Fliesslayout. */
    void fliessbereicheEinrichten();
    /* MF-1293: Zahlenfelder auf eine ablesbare Breite bringen. */
    void zahlenfelderLesbarMachen();
    /* MF-1293: den Erklaerungsblock auf die eine Zeile kuerzen,
     * die nirgends sonst steht. */
    void erklaerungKuerzen();
    /* MF-1299: Modus-Kasten in den Fliessbereich, engere
     * Raender - damit mehr Spalten nebeneinander passen. */
    void platzSparen();
    /* MF-1302: Zeilen sparen - Umdrehungen/Index auf eine
     * Zeile, Schutzsignale auf drei Spalten, Nachweis hinter
     * den Kopiermodus. */
    void zeilenSparen();
    /* MF-1306: ist dieses Feld - oder einer seiner Vorfahren -
     * ausdruecklich verborgen? Dann zaehlt sein Wert nicht. */
    bool istVerborgen(const QWidget *w) const;
    /* MF-1308: die gewaehlte Variante in die Geometriefelder
     * uebertragen - nur, was sie wirklich nennt. */
    void varianteAnwenden();
    /* MF-1298: die vier verborgenen Erklaerungsfelder als
     * Kurzhinweis am Kasten zusammenfassen. */
    void planHinweisNachziehen();
    /* MF-1293: die vier Sektorknoepfe neben dem Zahlenfeld,
     * das dasselbe kann. */
    void sektorknoepfeVerbergen();

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
    /* MF-1293: gemeinsamer Slot fuer jede Feldaenderung. */
    void onFeldGeaendert();
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

public:
    /* MF-1320 (E-15): „Ein Tor, das bei fehlendem Anker durchlaeuft, ist
     * keines."
     *
     * Beide Tore dieses Reiters haben bis hierher `if (!w) continue;`
     * gesagt und damit still alles durchgelassen, sobald ihre Anker
     * fehlten. Gemessen war das kein Randfall: der GUI-Umbau hat ALLE
     * VIER Gruppen entfernt, die `applyPluginCapabilities()` kennt
     * (`groupFlux`, `groupPLL`, `groupWrite`, `groupProtection` — je 1
     * Treffer in der HEAD-Fassung von `forms/tab_format.ui`, je 0 in der
     * neuen). Das Tor lief seither fuer JEDES Format durch und blendete
     * nichts aus, ohne ein Wort.
     *
     * Ein fehlender Anker ist damit ein FUND. Er wird gezaehlt und ist
     * abfragbar, damit ein Test darauf zeigen kann. 0 heisst „alle Anker
     * da"; -1 heisst „das Tor lief noch nicht". */
    int gruppenTorFehlendeAnker() const { return m_gruppenTorFehlendeAnker; }
    int parameterTorFehlendeAnker() const { return m_parameterTorFehlendeAnker; }

    /* MF-1320: sind die Faehigkeiten des gewaehlten Formats UEBERHAUPT
     * bekannt?
     *
     * `copyPlanCaps()` liefert `0` in zwei voellig verschiedenen Faellen:
     * „das Format kann nichts davon" und „das Format ist nicht
     * nachschlagbar". Wer die beiden zusammenwirft, blendet einem
     * Benutzer wegen eines NACHSCHLAGEFEHLERS Bedienelemente aus.
     *
     * Das alte Gruppentor kannte die Regel und schrieb sie hin: „Kein
     * Plugin gefunden: NICHTS ausblenden." Beim Umbau auf das
     * Parameter-Tor ist sie verlorengegangen. Dieselbe Dreiteilung wie
     * `caps_bekannt` im Kopierplan (MF-1311) und `probe_gemessen` an der
     * Scheibe (MF-1317). */
    bool copyPlanCapsBekannt() const;

private:
    int m_gruppenTorFehlendeAnker = -1;   /**< -1 = Tor lief noch nicht */
    int m_parameterTorFehlendeAnker = -1;

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
