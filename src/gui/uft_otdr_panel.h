/**
 * @file uft_otdr_panel.h
 * @brief Signal Analysis Panel — OTDR-Style Flux Quality Visualization
 *
 * Integrates the floppy_otdr analysis engine with a Qt6 GUI panel.
 * Provides:
 *   - Per-track quality trace (OTDR-style dB view)
 *   - Disk-wide heatmap (quality across all tracks)
 *   - Timing histogram with peak detection
 *   - Event list (jitter spikes, weak bits, protection, CRC errors)
 *   - Multi-revolution overlay for weak-bit analysis
 *
 * Data flow:
 *   SCP/HFE file → uft_scp_parser → uint32_t flux_ns[]
 *     → otdr_track_load_flux() → otdr_track_analyze()
 *       → FloppyOtdrWidget (visualization)
 *       → Event table (QTreeWidget)
 *       → Statistics panel
 *
 * @version 4.1.0
 */

#ifndef UFT_OTDR_PANEL_H
#define UFT_OTDR_PANEL_H

#include <QWidget>

#include <cstdint>
#include <vector>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QTimer>
#include <QString>

/* FloppyOtdrWidget is a header-only Qt widget */
#include "FloppyOtdrWidget.h"

/* C API headers */
#ifdef __cplusplus
extern "C" {
#endif
#include "uft/analysis/floppy_otdr.h"
#include "uft/flux/uft_scp_parser.h"
#include "uft/analysis/uft_anomaly_detect.h"
#include "uft/analysis/uft_ml_protection.h"
#include "uft/forensic/uft_provenance.h"
/* uft_otdr_adaptive_decode.h and uft_otdr_encoding_boost.h
 * included in .cpp only — C headers clash with C++ keywords */
#ifdef __cplusplus
}
#endif

#include <QSettings>
#include <QSlider>
#include <QDoubleSpinBox>

class UftOtdrPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftOtdrPanel(QWidget *parent = nullptr);
    ~UftOtdrPanel();

    /**
     * @brief Load and analyze a flux image file (SCP, HFE, G64, etc.)
     * @param path  Path to flux image
     * @return true on success
     */
    bool loadFluxImage(const QString &path);

    /**
     * @brief Analyze a single track from already-loaded data
     * @param cylinder  Cylinder number (0-83)
     * @param head      Head/side (0 or 1)
     */
    void analyzeTrack(int cylinder, int head);

    /**
     * @brief Run full-disk analysis (all tracks)
     */
    void analyzeFullDisk();

    /** @brief Check if flux data is loaded */
    bool hasFluxData() const { return m_scpCtx != nullptr; }

    /**
     * @brief Rohe Flusszeiten einer Spur, in Nanosekunden (MF-632).
     *
     * Das Panel haelt die Zeiten ohnehin — `otdr_track_t::flux_ns` und
     * `flux_multi[]`. Bisher gab es keinen Weg, sie von aussen zu sehen;
     * damit war der zweite Blick auf dieselben Daten (die
     * Fluss-Visualisierung) nicht anschliessbar, obwohl beide Seiten im
     * Baum lagen.
     *
     * @param cylinder   Zylinder
     * @param head       Kopf (0/1)
     * @param revolution -1 fuer die Hauptaufnahme, sonst der Index in
     *                   `flux_multi[]`
     * @return leerer Vektor, wenn nichts geladen ist oder die Spur nicht
     *         existiert — nie ein Teilergebnis ohne Hinweis
     */
    std::vector<uint32_t> trackFlux(int cylinder, int head,
                                    int revolution = -1) const;

    /** @brief Wie viele Umdrehungen diese Spur fuehrt (0 = keine Daten). */
    int trackRevolutions(int cylinder, int head) const;

signals:
    void analysisStarted();
    void analysisProgress(int percent, const QString &status);
    void analysisComplete(float overallQuality);
    void trackSelected(int cylinder, int head);

    /**
     * @brief Fuer diese Spur liegen rohe Flusszeiten bereit (MF-632).
     *
     * Gesendet am Ende von analyzeTrack(). Absichtlich ohne Nutzdaten im
     * Signal: der Empfaenger holt sie mit trackFlux(), damit keine Kopie
     * durch die Signalkette laeuft und keine veraltete Kopie entstehen
     * kann.
     */
    void fluxTrackReady(int cylinder, int head);

public slots:
    void onTrackChanged(int trackIndex);
    void onEncodingChanged(int index);
    void onAnalyzeClicked();
    void onAnalyzeAllClicked();
    void onExportReport();
    void onDeepReadToggled(bool enabled);
    void onDeepReadModeChanged(int index);
    void onExportProvenance();

private:
    void setupUi();
    void setupControlPanel(QHBoxLayout *layout);
    void setupDeepReadPanel(QVBoxLayout *layout);
    void setupVisualization(QSplitter *splitter);
    void setupEventTable(QSplitter *splitter);
    void setupStatsPanel(QVBoxLayout *layout);
    void setupForensicPanel(QVBoxLayout *layout);
    void updateTrackDisplay();
    void updateEventTable();
    void updateStatsDisplay();
    void runMLAnalysis();
    void updateProvenanceDisplay();
    void updateDeepReadStats(uint32_t sectors_improved, uint32_t sectors_attempted,
                             bool used_otdr, float avg_quality);
    /* Traegt ALLE von der Oberflaeche gesteuerten Werte in m_config ein.
     *
     * Vor MF-671 stand diese Zuweisung viermal im Quelltext, je einmal vor
     * einem otdr_track_analyze()-Aufruf — und eine der vier war bereits
     * unvollstaendig: die erste Auswertung nach dem Laden setzte
     * `smooth_window`, aber nicht `encoding`. Wer eine Kodierung waehlte
     * und DANN lud, bekam die erste Spur mit "Auto" ausgewertet, waehrend
     * der Kasten etwas anderes zeigte.
     *
     * Das ist dieselbe Aufzaehlungs-Falle, die in diesem Baum achtmal
     * zugeschnappt ist (CLAUDE.md §Dateimengen; MF-668 zuletzt). Ein
     * Anwender, vier Aufrufer: der fuenfte Auswertungsweg, den jemand
     * anlegt, kann es nicht mehr vergessen, ohne dass es auffaellt —
     * `scripts/audit_otdr_apply.py` zaehlt beide Seiten gegeneinander. */
    void applyConfigFromControls();

    /* Der EINZIGE Weg von diesem Panel in die Spuranalyse.
     *
     * Vor MF-671 riefen vier Stellen `otdr_track_analyze()` selbst auf und
     * trugen je eine Kopie der Konfigurations-Zuweisung davor — eine der
     * vier bereits unvollstaendig. Ein Trichter kann nicht vergessen
     * werden; nachzuzaehlen, ob jemand ihn vergessen hat, faellt erst auf,
     * NACHDEM es passiert ist.
     *
     * `scripts/audit_setting_wiring.py` haelt fest, dass es bei einem Weg
     * bleibt. */
    void analyzeWithCurrentConfig(otdr_track_t *track);
    void populateTrackCombo();
    void freeCurrentAnalysis();
    void loadDeepReadSettings();
    void saveDeepReadSettings();

    /* ── Visualization ── */
    FloppyOtdrWidget    *m_otdrWidget;      /**< OTDR trace/heatmap/histogram */
    QTabWidget          *m_viewTabs;         /**< Trace | Heatmap | Histogram */

    /* ── Controls ── */
    QComboBox           *m_trackCombo;       /**< Track selector */
    QComboBox           *m_encodingCombo;    /**< MFM DD/HD, FM, GCR, Auto */
    QCheckBox           *m_showSmoothed;     /**< Show smoothed trace */
    QCheckBox           *m_showEvents;       /**< Show event markers */
    QCheckBox           *m_showSectors;      /**< Show sector boundaries */
    QCheckBox           *m_showRaw;          /**< Show raw trace */
    QCheckBox           *m_multiRevOverlay;  /**< Multi-revolution overlay */
    QPushButton         *m_analyzeBtn;       /**< Analyze current track */
    QPushButton         *m_analyzeAllBtn;    /**< Analyze full disk */
    QPushButton         *m_exportBtn;        /**< Export report */
    QSpinBox            *m_smoothWindow;     /**< Smoothing window size */

    /* Die drei Zahlen-Schwellen der OTDR-Auswertung (MF-671).
     *
     * Sie standen bis dahin nur in `otdr_config_defaults()` und waren von
     * aussen nicht erreichbar, obwohl alle drei im Kern eine Lesestelle
     * haben. Die Einheiten der Regler sind NICHT die des Traegers —
     * umgerechnet wird in applyConfigFromControls(), an der einen Stelle,
     * die den Traeger setzt. Siehe docs/SETTINGS_ROADMAP.md
     * §Einheiten-Falle. */
    QDoubleSpinBox      *m_pllLockThreshold; /**< Lock-Schwelle in %    */
    QDoubleSpinBox      *m_weakBitCv;        /**< Weak-Bit-Streuung, CV */
    QDoubleSpinBox      *m_noFluxThreshold;  /**< No-Flux, x Nennperiode */

    /* ── DeepRead Controls ── */
    QGroupBox           *m_deepReadGroup;    /**< DeepRead collapsible group */
    QCheckBox           *m_deepReadEnabled;  /**< Master toggle */
    QComboBox           *m_deepReadMode;     /**< Lite / Full */
    QDoubleSpinBox      *m_confThreshold;    /**< Confidence threshold */
    QDoubleSpinBox      *m_pllTolerance;     /**< PLL tolerance % */
    QLabel              *m_lblDeepReadStatus;/**< "3 sectors improved" etc. */
    QLabel              *m_lblDeepReadConf;  /**< Average confidence */

    /* ── Event Table ── */
    QTreeWidget         *m_eventTree;        /**< Event list */

    /* ── Statistics ── */
    QLabel              *m_lblQuality;       /**< Overall quality */
    QLabel              *m_lblJitter;        /**< Mean jitter % */
    QLabel              *m_lblEvents;        /**< Event count */
    QLabel              *m_lblEncoding;      /**< Detected encoding */
    QLabel              *m_lblRPM;           /**< Measured RPM */
    QLabel              *m_lblFluxCount;     /**< Flux transition count */
    QLabel              *m_lblWeakBits;      /**< Weak bit count */
    QLabel              *m_lblProtection;    /**< Protection detected */
    QLabel              *m_lblAnomaly;       /**< ML anomaly detection result */
    QLabel              *m_lblMLProtection;  /**< ML protection classifier result */
    QProgressBar        *m_progressBar;      /**< Analysis progress */
    QLabel              *m_statusLabel;      /**< Status text */

    /* ── Provenance Chain ── */
    QGroupBox           *m_provGroup;        /**< Provenance chain group box */
    QLabel              *m_lblProvStatus;    /**< Chain entry count */
    QLabel              *m_lblProvHash;      /**< Last chain hash (truncated) */
    QPushButton         *m_provExportBtn;    /**< Export chain button */

    /* ── Analysis State ── */
    uft_scp_ctx_t       *m_scpCtx;          /**< SCP parser context */
    otdr_disk_t         *m_disk;             /**< Full disk analysis */
    otdr_config_t        m_config;           /**< Analysis configuration */
    int                  m_currentTrack;     /**< Currently displayed track */
    QString              m_currentFile;      /**< Currently loaded file */

    /* ── DeepRead State ── */
    bool                 m_deepReadActive;   /**< DeepRead currently active */
    uint32_t             m_deepReadImproved; /**< Total sectors improved */
    uint32_t             m_deepReadAttempted;/**< Total sectors attempted */

    /* ── ML / Provenance State ── */
    uft_provenance_chain_t *m_provChain;    /**< Forensic provenance chain */
};

#endif /* UFT_OTDR_PANEL_H */
