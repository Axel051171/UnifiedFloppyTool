/**
 * @file uft_gui_kryoflux_style.h
 * @brief UFT Qt GUI Components - KryoFlux-Inspired Layout
 * 
 * This header defines Qt widget interfaces inspired by the KryoFlux UI
 * layout structure while using Qt/C++ conventions.
 * 
 * Layout concept:
 * ┌────────────────────────────────────────────────────────────────┐
 * │ [Menu Bar]                                                      │
 * ├────────────────────────────┬───────────────────────────────────┤
 * │                            │                                   │
 * │     TRACK GRID             │      CONTROL PANEL               │
 * │  (84 tracks × 2 sides)     │  - Start/Stop                    │
 * │                            │  - LED Status (Motor/Stream/Err) │
 * │                            │  - Output Selection              │
 * │                            │  - Name Input                    │
 * ├────────────────────────────┴───────────────────────────────────┤
 * │ [Track | Advanced | Histogram | Scatter | Density]             │
 * │                                                                │
 * │              INFORMATION PANEL (Tabbed)                       │
 * │                                                                │
 * ├────────────────────────────────────────────────────────────────┤
 * │ [Status Bar]                                                   │
 * └────────────────────────────────────────────────────────────────┘
 * 
 * @author UFT Development Team
 * @date 2025
 * @license MIT
 */

#ifndef UFT_GUI_KRYOFLUX_STYLE_H
#define UFT_GUI_KRYOFLUX_STYLE_H

#include <QMainWindow>
#include <QWidget>
#include <QTabWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QTableWidget>
#include <QDialog>
#include <QListWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPainter>
#include <QMouseEvent>
#include <QVector>
#include <QColor>
#include <QTimer>
#include <QChart>
#include <QChartView>
#include <QScatterSeries>
#include <QBarSeries>
#include <QLineSeries>

#include <stdbool.h>

namespace UFT {
namespace GUI {

// ============================================================================
// Track Grid Cell State
// ============================================================================

/**
 * @brief Track cell state enumeration (matches KryoFlux concept)
 */
enum class CellState {
    Unknown,    ///< Not yet read (gray)
    Good,       ///< All sectors OK (green)
    Bad,        ///< CRC/read errors (red)
    Modified,   ///< Changed since last save (yellow)
    Reading,    ///< Currently being read (blue pulse)
    Selected    ///< User selected (highlighted)
};

/**
 * @brief Track info structure for display
 */
struct TrackInfo {
    int trackNumber;
    int logicalTrack;
    QString format;         // "Amiga DD", "IBM MFM", etc.
    QString result;         // "OK", "Bad", "Missing"
    int sectorsFound;
    int sectorsExpected;
    double rpm;
    int transferRate;       // Bytes/second
    
    // Advanced info
    int fluxReversals;
    double driftUs;
    double baseUs;
    QString bandsUs;        // "4.0, 6.0, 8.0"
    
    // Flags (from KryoFlux)
    QString statusFlags;    // "P", "N", "X", etc.
};

// ============================================================================
// LED Status Widget
// ============================================================================

/**
 * @brief LED indicator widget
 * 
 * Displays a colored LED with optional label.
 * Colors: Off (dark gray), Green, Yellow, Red
 */
class LEDWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool on READ isOn WRITE setOn NOTIFY stateChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor)
    
public:
    explicit LEDWidget(const QString& label = QString(), QWidget* parent = nullptr);
    
    bool isOn() const { return m_on; }
    QColor color() const { return m_color; }
    
public slots:
    void setOn(bool on);
    void setColor(const QColor& color);
    void pulse();  // Brief flash animation
    
signals:
    void stateChanged(bool on);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    bool m_on = false;
    QColor m_color = Qt::green;
    QString m_label;
    QTimer* m_pulseTimer = nullptr;
    int m_pulseAlpha = 255;
};

/**
 * @brief LED group with multiple indicators
 */
class LEDGroupWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit LEDGroupWidget(QWidget* parent = nullptr);
    
    void addLED(const QString& name, const QColor& color);
    LEDWidget* led(const QString& name) const;
    
    // Convenience for standard UFT LEDs
    void setMotor(bool on);
    void setStream(bool on);
    void setError(bool on);
    
private:
    QMap<QString, LEDWidget*> m_leds;
    QVBoxLayout* m_layout;
};

// ============================================================================
// Track Grid Widget
// ============================================================================

/**
 * @brief Interactive track grid component
 * 
 * Displays a grid of track cells (84 tracks × 2 sides by default).
 * Supports:
 * - Cell state coloring
 * - Single cell selection
 * - Range selection (drag)
 * - Cell hover highlighting
 * - Animation for reading state
 */
class TrackGridWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int maxTracks READ maxTracks WRITE setMaxTracks)
    Q_PROPERTY(int sides READ sides WRITE setSides)
    
public:
    explicit TrackGridWidget(QWidget* parent = nullptr);
    
    int maxTracks() const { return m_maxTracks; }
    int sides() const { return m_sides; }
    
    void setMaxTracks(int tracks);
    void setSides(int sides);
    
    CellState cellState(int track, int side) const;
    void setCellState(int track, int side, CellState state);
    void setAllCells(CellState state);
    
    // Selection
    void selectCell(int track, int side);
    void selectRange(int startTrack, int endTrack, int side = -1);
    void clearSelection();
    QList<QPair<int, int>> selectedCells() const;
    
    // Current reading position
    void setReadingPosition(int track, int side);
    void clearReadingPosition();
    
signals:
    void cellClicked(int track, int side);
    void cellDoubleClicked(int track, int side);
    void rangeSelected(int startTrack, int endTrack, int startSide, int endSide);
    void cellHovered(int track, int side);
    void contextMenuRequested(int track, int side, const QPoint& globalPos);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private:
    QPair<int, int> cellAt(const QPoint& pos) const;
    QRect cellRect(int track, int side) const;
    QColor stateColor(CellState state) const;
    void updateCellSizes();
    
    int m_maxTracks = 84;
    int m_sides = 2;
    QVector<QVector<CellState>> m_cells;
    QSet<QPair<int, int>> m_selection;
    int m_hoverTrack = -1;
    int m_hoverSide = -1;
    int m_readingTrack = -1;
    int m_readingSide = -1;
    
    // Drag selection
    bool m_dragging = false;
    int m_dragStartTrack = -1;
    int m_dragStartSide = -1;
    
    // Cell dimensions
    int m_cellWidth = 20;
    int m_cellHeight = 16;
    int m_headerHeight = 20;
    int m_sideHeaderWidth = 40;
    
    // Animation
    QTimer* m_animTimer = nullptr;
    int m_animPhase = 0;
};

// ============================================================================
// Control Panel Widget
// ============================================================================

/**
 * @brief Control panel with start/stop, LEDs, output selection
 */
class ControlPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit ControlPanel(QWidget* parent = nullptr);
    
    // LED access
    LEDGroupWidget* leds() { return m_leds; }
    
    // State
    bool isRunning() const { return m_running; }
    QString selectedOutput() const;
    QString imageName() const;
    
    // Output format list
    void setOutputFormats(const QStringList& formats);
    void setSelectedOutput(const QString& format);
    
public slots:
    void setRunning(bool running);
    void setImageName(const QString& name);
    
signals:
    void startClicked();
    void stopClicked();
    void outputChanged(const QString& format);
    void imageNameChanged(const QString& name);
    void calibrateClicked();
    
private:
    void setupUI();
    
    bool m_running = false;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_calibrateBtn;
    QComboBox* m_outputCombo;
    QLineEdit* m_nameEdit;
    LEDGroupWidget* m_leds;
};

// ============================================================================
// Track Info Panel
// ============================================================================

/**
 * @brief Basic track information display
 */
class TrackInfoBasic : public QWidget {
    Q_OBJECT
    
public:
    explicit TrackInfoBasic(QWidget* parent = nullptr);
    void update(const TrackInfo& info);
    void clear();
    
private:
    QLabel* m_trackLabel;
    QLabel* m_logicalTrackLabel;
    QLabel* m_formatLabel;
    QLabel* m_resultLabel;
    QLabel* m_sectorsLabel;
    QLabel* m_rpmLabel;
    QLabel* m_transferLabel;
};

/**
 * @brief Advanced track information display
 */
class TrackInfoAdvanced : public QWidget {
    Q_OBJECT
    
public:
    explicit TrackInfoAdvanced(QWidget* parent = nullptr);
    void update(const TrackInfo& info);
    void clear();
    
private:
    QLabel* m_fluxReversalsLabel;
    QLabel* m_driftLabel;
    QLabel* m_baseLabel;
    QLabel* m_bandsLabel;
    QLabel* m_flagsLabel;
};

/**
 * @brief Histogram plot for timing distribution
 */
class HistogramPlot : public QWidget {
    Q_OBJECT
    
public:
    explicit HistogramPlot(QWidget* parent = nullptr);
    void setData(const QVector<double>& values, int binCount = 100);
    void clear();
    
    void setXLabel(const QString& label);
    void setYLabel(const QString& label);
    void setTitle(const QString& title);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    QVector<int> m_bins;
    double m_minVal = 0;
    double m_maxVal = 100;
    QString m_xLabel = "Timing (µs)";
    QString m_yLabel = "Count";
    QString m_title = "Histogram";
};

/**
 * @brief Scatter plot for timing analysis
 */
class ScatterPlot : public QWidget {
    Q_OBJECT
    
public:
    explicit ScatterPlot(QWidget* parent = nullptr);
    void setData(const QVector<QPointF>& points);
    void clear();
    
    void setXLabel(const QString& label);
    void setYLabel(const QString& label);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    QVector<QPointF> m_points;
    QString m_xLabel = "Position";
    QString m_yLabel = "Timing (µs)";
};

/**
 * @brief Density plot for flux visualization
 */
class DensityPlot : public QWidget {
    Q_OBJECT
    
public:
    explicit DensityPlot(QWidget* parent = nullptr);
    void setData(const QVector<double>& densities, int width, int height);
    void clear();
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    QImage m_densityImage;
};

/**
 * @brief Tabbed track information panel
 */
class TrackInfoPanel : public QTabWidget {
    Q_OBJECT
    
public:
    explicit TrackInfoPanel(QWidget* parent = nullptr);
    
    void updateTrackInfo(const TrackInfo& info);
    void updateHistogram(const QVector<double>& timings);
    void updateScatter(const QVector<QPointF>& points);
    void updateDensity(const QVector<double>& densities, int w, int h);
    void clear();
    
private:
    TrackInfoBasic* m_basicTab;
    TrackInfoAdvanced* m_advancedTab;
    HistogramPlot* m_histogramTab;
    ScatterPlot* m_scatterTab;
    DensityPlot* m_densityTab;
};

// ============================================================================
// Settings Dialog
// ============================================================================

/**
 * @brief Image profile data structure
 */
struct ImageProfile {
    QString name;
    QString imageType;
    QString extension;
    int trackStart = 0;
    int trackEnd = 79;
    int sideMode = 2;  // 0=Side0, 1=Side1, 2=Both
    int sectorSize = 512;
    int sectorCountMode = 0;  // 0=Any, 1=Exactly, 2=AtMost
    int sectorCount = 0;
    int trackDistance = 1;
    double targetRPM = 300.0;
    bool flippyMode = false;
    QString otherParams;
};

/**
 * @brief Profile selection widget with list and edit buttons
 */
class ProfileSelectionWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ProfileSelectionWidget(QWidget* parent = nullptr);
    
    void setProfiles(const QList<ImageProfile>& profiles);
    ImageProfile currentProfile() const;
    int currentIndex() const;
    
public slots:
    void selectProfile(int index);
    
signals:
    void profileSelected(int index);
    void addRequested();
    void removeRequested(int index);
    void copyRequested(int index);
    void updateRequested();
    
private:
    QListWidget* m_list;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_copyBtn;
    QPushButton* m_updateBtn;
    QList<ImageProfile> m_profiles;
};

/**
 * @brief Profile editor widget
 */
class ProfileEditorWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ProfileEditorWidget(QWidget* parent = nullptr);
    
    void setProfile(const ImageProfile& profile);
    ImageProfile profile() const;
    
signals:
    void profileChanged(const ImageProfile& profile);
    
private:
    void setupUI();
    void updateFromUI();
    
    QLineEdit* m_nameEdit;
    QComboBox* m_typeCombo;
    QLineEdit* m_extensionEdit;
    QSpinBox* m_trackStartSpin;
    QSpinBox* m_trackEndSpin;
    QComboBox* m_sideModeCombo;
    QSpinBox* m_sectorSizeSpin;
    QComboBox* m_sectorCountModeCombo;
    QSpinBox* m_sectorCountSpin;
    QSpinBox* m_trackDistanceSpin;
    QDoubleSpinBox* m_rpmSpin;
    QCheckBox* m_flippyCheck;
    QLineEdit* m_otherParamsEdit;
    
    ImageProfile m_profile;
};

/**
 * @brief Advanced settings widget
 */
class AdvancedSettingsWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit AdvancedSettingsWidget(QWidget* parent = nullptr);
    
    // Getters
    int retries() const;
    int revolutions() const;
    int driveSelection() const;
    int sideSelection() const;
    int maxTrackDrive0() const;
    int maxTrackDrive1() const;
    QString globalParams() const;
    
    // Setters
    void setRetries(int value);
    void setRevolutions(int value);
    void setDriveSelection(int drive);
    void setSideSelection(int side);
    void setMaxTrackDrive0(int track);
    void setMaxTrackDrive1(int track);
    void setGlobalParams(const QString& params);
    void setCalibrated(bool calibrated, int maxTrack = 83);
    
signals:
    void settingsChanged();
    
private:
    void setupUI();
    
    QComboBox* m_driveCombo;
    QComboBox* m_sideCombo;
    QSpinBox* m_retriesSpin;
    QSpinBox* m_revolutionsSpin;
    QSpinBox* m_maxTrack0Spin;
    QSpinBox* m_maxTrack1Spin;
    QLineEdit* m_globalParamsEdit;
    QLabel* m_calibrationLabel;
};

/**
 * @brief Main settings dialog
 */
class SettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    
    // Profile management
    void setProfiles(const QList<ImageProfile>& profiles);
    QList<ImageProfile> profiles() const;
    
    // Advanced settings
    AdvancedSettingsWidget* advancedSettings() { return m_advancedTab; }
    
signals:
    void profilesChanged(const QList<ImageProfile>& profiles);
    void settingsChanged();
    
private:
    void setupUI();
    
    QTabWidget* m_tabs;
    ProfileSelectionWidget* m_profileSelection;
    ProfileEditorWidget* m_profileEditor;
    AdvancedSettingsWidget* m_advancedTab;
};

// ============================================================================
// Main Window
// ============================================================================

/**
 * @brief UFT Main Window (KryoFlux-style layout)
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    
    // Access to main components
    TrackGridWidget* trackGrid() { return m_trackGrid; }
    ControlPanel* controlPanel() { return m_controlPanel; }
    TrackInfoPanel* infoPanel() { return m_infoPanel; }
    
public slots:
    void showSettings();
    void showAbout();
    void showCalibration();
    
    // Status updates
    void setStatusMessage(const QString& message);
    void setDensityStatus(const QString& density);
    
signals:
    void settingsChanged();
    void calibrationRequested();
    
protected:
    void closeEvent(QCloseEvent* event) override;
    
private:
    void setupUI();
    void setupMenus();
    void setupStatusBar();
    void loadSettings();
    void saveSettings();
    
    TrackGridWidget* m_trackGrid;
    ControlPanel* m_controlPanel;
    TrackInfoPanel* m_infoPanel;
    
    QLabel* m_statusLabel;
    QLabel* m_densityLabel;
};

// ============================================================================
// Localization Support
// ============================================================================

/**
 * @brief UI strings for localization (matching KryoFlux keys)
 */
struct UIStrings {
    // Sections
    static constexpr const char* SECTION_TRACKS = "Tracks";
    static constexpr const char* SECTION_INFO = "Information";
    static constexpr const char* SECTION_CONTROL = "Control";
    
    // LEDs
    static constexpr const char* LED_MOTOR = "Motor";
    static constexpr const char* LED_STREAM = "Stream";
    static constexpr const char* LED_ERROR = "Error";
    
    // Control
    static constexpr const char* CONTROL_START = "Start";
    static constexpr const char* CONTROL_STOP = "Stop";
    static constexpr const char* CONTROL_ENTER_NAME = "Enter name...";
    static constexpr const char* CONTROL_SELECT_OUTPUTS = "Select outputs...";
    
    // Info tabs
    static constexpr const char* TAB_TRACK = "Track";
    static constexpr const char* TAB_ADVANCED = "Advanced";
    static constexpr const char* TAB_HISTOGRAM = "Histogram";
    static constexpr const char* TAB_SCATTER = "Scatter";
    static constexpr const char* TAB_DENSITY = "Density";
    
    // Track fields
    static constexpr const char* FIELD_TRACK = "Track";
    static constexpr const char* FIELD_LOGICAL_TRACK = "Logical Track";
    static constexpr const char* FIELD_FORMAT = "Format";
    static constexpr const char* FIELD_RESULT = "Result";
    static constexpr const char* FIELD_SECTORS = "Sectors";
    static constexpr const char* FIELD_RPM = "RPM";
    static constexpr const char* FIELD_TRANSFER = "Transfer (Bytes/s)";
    
    // Advanced fields
    static constexpr const char* FIELD_FLUX_REVERSALS = "Flux Reversals";
    static constexpr const char* FIELD_DRIFT = "Drift (µs)";
    static constexpr const char* FIELD_BASE = "Base (µs)";
    static constexpr const char* FIELD_BANDS = "Bands (µs)";
    
    // Status
    static constexpr const char* STATUS_READY = "Ready";
    static constexpr const char* STATUS_READING = "Reading...";
    static constexpr const char* STATUS_ERROR = "Error";
    
    // Settings
    static constexpr const char* SETTINGS_PROFILES = "Image Profiles";
    static constexpr const char* SETTINGS_ADVANCED = "Advanced";
    static constexpr const char* SETTINGS_OUTPUT = "Output";
    
    // Profile fields
    static constexpr const char* PROFILE_NAME = "Profile Name";
    static constexpr const char* PROFILE_IMAGE_TYPE = "Image Type";
    static constexpr const char* PROFILE_EXTENSION = "Extension";
    static constexpr const char* PROFILE_TRACK_START = "Start Track";
    static constexpr const char* PROFILE_TRACK_END = "End Track";
    static constexpr const char* PROFILE_SIDE_MODE = "Side Mode";
    static constexpr const char* PROFILE_SECTOR_SIZE = "Sector Size";
    static constexpr const char* PROFILE_SECTOR_COUNT = "Sector Count";
    static constexpr const char* PROFILE_TRACK_DISTANCE = "Track Distance";
    static constexpr const char* PROFILE_TARGET_RPM = "Target RPM";
    static constexpr const char* PROFILE_FLIPPY_MODE = "Flippy Mode";
    
    // Error flags (KryoFlux style)
    static constexpr const char* FLAG_P = "Generic protection present";
    static constexpr const char* FLAG_N = "Sector not in image";
    static constexpr const char* FLAG_X = "Decoding stopped (protection)";
    static constexpr const char* FLAG_H = "Hidden data in header";
    static constexpr const char* FLAG_I = "Non-standard format/ID";
    static constexpr const char* FLAG_T = "Wrong track number";
    static constexpr const char* FLAG_S = "Wrong side number";
    static constexpr const char* FLAG_B = "Sector out of range";
    static constexpr const char* FLAG_L = "Non-standard sector length";
    static constexpr const char* FLAG_Z = "Illegal offset";
    static constexpr const char* FLAG_C = "Unchecked checksum";
};

} // namespace GUI
} // namespace UFT

#endif // UFT_GUI_KRYOFLUX_STYLE_H
