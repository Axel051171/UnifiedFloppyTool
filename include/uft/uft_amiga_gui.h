/**
 * @file uft_amiga_gui.h
 * @brief Qt GUI Widgets for Amiga Copy/Recovery Operations
 * 
 * This header defines Qt widget classes for the Amiga copy modes
 * panel, integrating XCopy-style operations with modern GUI.
 * 
 * @author UFT Development Team
 * @date 2025
 */

#ifndef UFT_AMIGA_GUI_H
#define UFT_AMIGA_GUI_H

#ifdef __cplusplus

#include <QWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QButtonGroup>

namespace UFT {
namespace Amiga {

// ============================================================================
// Copy Mode Selection Widget
// ============================================================================

/**
 * @brief XCopy-style mode selection panel
 * 
 * Provides radio buttons for selecting copy mode:
 * - DosCopy, DosCopy+, BamCopy+, Nibble
 * - Additional operations (Format, Check, etc.)
 */
class CopyModeWidget : public QGroupBox {
    Q_OBJECT
    
public:
    enum Mode {
        DosCopy = 0,
        BamCopy = 1,
        DosCopyPlus = 2,
        Nibble = 3,
        Optimize = 4,
        Format = 5,
        QuickFormat = 6,
        Erase = 7,
        SpeedCheck = 8,
        DiskName = 9,
        Directory = 10,
        Verify = 11,
        InstallBoot = 12
    };
    Q_ENUM(Mode)
    
    explicit CopyModeWidget(QWidget* parent = nullptr)
        : QGroupBox(tr("Copy Mode"), parent)
    {
        auto* layout = new QVBoxLayout(this);
        m_modeGroup = new QButtonGroup(this);
        
        // Main copy modes (XCopy-style)
        auto* copyGroup = new QGroupBox(tr("Copy"));
        auto* copyLayout = new QGridLayout(copyGroup);
        
        addModeButton(copyLayout, DosCopy, tr("DosCopy"), 
            tr("Standard DOS sector copy"), 0, 0);
        addModeButton(copyLayout, DosCopyPlus, tr("DosCopy+"), 
            tr("DOS copy with verification"), 0, 1);
        addModeButton(copyLayout, BamCopy, tr("BamCopy+"), 
            tr("BAM-aware copy (faster)"), 1, 0);
        addModeButton(copyLayout, Nibble, tr("Nibble"), 
            tr("Raw track copy (preserves protection)"), 1, 1);
        
        layout->addWidget(copyGroup);
        
        // Utility operations
        auto* utilGroup = new QGroupBox(tr("Utilities"));
        auto* utilLayout = new QGridLayout(utilGroup);
        
        addModeButton(utilLayout, Format, tr("Format"), 
            tr("Full format with verify"), 0, 0);
        addModeButton(utilLayout, QuickFormat, tr("Quick Format"), 
            tr("Quick format (root block only)"), 0, 1);
        addModeButton(utilLayout, Verify, tr("Verify"), 
            tr("Verify disk integrity"), 1, 0);
        addModeButton(utilLayout, Optimize, tr("Optimize"), 
            tr("Optimize file layout"), 1, 1);
        
        layout->addWidget(utilGroup);
        
        // Info operations
        auto* infoGroup = new QGroupBox(tr("Information"));
        auto* infoLayout = new QHBoxLayout(infoGroup);
        
        addModeButton(infoLayout, DiskName, tr("Name"), tr("View/set disk name"));
        addModeButton(infoLayout, Directory, tr("Dir"), tr("Show directory"));
        addModeButton(infoLayout, SpeedCheck, tr("Speed"), tr("Check rotation speed"));
        
        layout->addWidget(infoGroup);
        
        // Select default
        if (auto* btn = m_buttons.value(DosCopy))
            btn->setChecked(true);
        
        connect(m_modeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
                this, &CopyModeWidget::modeChanged);
    }
    
    Mode currentMode() const {
        return static_cast<Mode>(m_modeGroup->checkedId());
    }
    
    void setMode(Mode mode) {
        if (auto* btn = m_buttons.value(mode))
            btn->setChecked(true);
    }
    
signals:
    void modeChanged(int mode);
    
private:
    void addModeButton(QGridLayout* layout, Mode mode, 
                       const QString& text, const QString& tooltip,
                       int row, int col) {
        auto* btn = new QRadioButton(text, this);
        btn->setToolTip(tooltip);
        m_modeGroup->addButton(btn, mode);
        m_buttons[mode] = btn;
        layout->addWidget(btn, row, col);
    }
    
    void addModeButton(QHBoxLayout* layout, Mode mode,
                       const QString& text, const QString& tooltip) {
        auto* btn = new QRadioButton(text, this);
        btn->setToolTip(tooltip);
        m_modeGroup->addButton(btn, mode);
        m_buttons[mode] = btn;
        layout->addWidget(btn);
    }
    
    QButtonGroup* m_modeGroup;
    QMap<Mode, QRadioButton*> m_buttons;
};

// ============================================================================
// Drive Selection Widget
// ============================================================================

/**
 * @brief Drive selection panel (Source/Target/Verify)
 */
class DriveSelectWidget : public QGroupBox {
    Q_OBJECT
    
public:
    explicit DriveSelectWidget(const QString& title, QWidget* parent = nullptr)
        : QGroupBox(title, parent)
    {
        auto* layout = new QHBoxLayout(this);
        
        for (int i = 0; i < 4; i++) {
            m_drives[i] = new QCheckBox(QString("DF%1:").arg(i), this);
            layout->addWidget(m_drives[i]);
            connect(m_drives[i], &QCheckBox::toggled, 
                    this, &DriveSelectWidget::selectionChanged);
        }
        
        layout->addStretch();
    }
    
    uint8_t selectedDrives() const {
        uint8_t mask = 0;
        for (int i = 0; i < 4; i++) {
            if (m_drives[i]->isChecked())
                mask |= (1 << i);
        }
        return mask;
    }
    
    void setSelectedDrives(uint8_t mask) {
        for (int i = 0; i < 4; i++) {
            m_drives[i]->setChecked(mask & (1 << i));
        }
    }
    
    void setSingleSelection(bool single) {
        // For source, typically only one drive
        // For target/verify, multiple allowed
        m_singleSelect = single;
    }
    
signals:
    void selectionChanged();
    
private:
    QCheckBox* m_drives[4];
    bool m_singleSelect = false;
};

// ============================================================================
// Track Range Widget
// ============================================================================

/**
 * @brief Track range selection (Start/End track and head)
 */
class TrackRangeWidget : public QGroupBox {
    Q_OBJECT
    
public:
    explicit TrackRangeWidget(QWidget* parent = nullptr)
        : QGroupBox(tr("Track Range"), parent)
    {
        auto* layout = new QGridLayout(this);
        
        // Track range
        layout->addWidget(new QLabel(tr("Start Track:")), 0, 0);
        m_startTrack = new QSpinBox(this);
        m_startTrack->setRange(0, 83);
        m_startTrack->setValue(0);
        layout->addWidget(m_startTrack, 0, 1);
        
        layout->addWidget(new QLabel(tr("End Track:")), 0, 2);
        m_endTrack = new QSpinBox(this);
        m_endTrack->setRange(0, 83);
        m_endTrack->setValue(79);
        layout->addWidget(m_endTrack, 0, 3);
        
        // Side selection
        layout->addWidget(new QLabel(tr("Sides:")), 1, 0);
        m_sideCombo = new QComboBox(this);
        m_sideCombo->addItem(tr("Both"), 0);
        m_sideCombo->addItem(tr("Upper (0)"), 1);
        m_sideCombo->addItem(tr("Lower (1)"), 2);
        layout->addWidget(m_sideCombo, 1, 1);
        
        // Preset buttons
        auto* presetLayout = new QHBoxLayout;
        auto* fullDisk = new QPushButton(tr("Full Disk"), this);
        auto* bootOnly = new QPushButton(tr("Boot Only"), this);
        presetLayout->addWidget(fullDisk);
        presetLayout->addWidget(bootOnly);
        presetLayout->addStretch();
        layout->addLayout(presetLayout, 1, 2, 1, 2);
        
        connect(fullDisk, &QPushButton::clicked, [this]() {
            m_startTrack->setValue(0);
            m_endTrack->setValue(79);
            m_sideCombo->setCurrentIndex(0);
        });
        
        connect(bootOnly, &QPushButton::clicked, [this]() {
            m_startTrack->setValue(0);
            m_endTrack->setValue(1);
            m_sideCombo->setCurrentIndex(0);
        });
    }
    
    int startTrack() const { return m_startTrack->value(); }
    int endTrack() const { return m_endTrack->value(); }
    int side() const { return m_sideCombo->currentData().toInt(); }
    
private:
    QSpinBox* m_startTrack;
    QSpinBox* m_endTrack;
    QComboBox* m_sideCombo;
};

// ============================================================================
// Options Widget
// ============================================================================

/**
 * @brief Copy options panel
 */
class CopyOptionsWidget : public QGroupBox {
    Q_OBJECT
    
public:
    explicit CopyOptionsWidget(QWidget* parent = nullptr)
        : QGroupBox(tr("Options"), parent)
    {
        auto* layout = new QGridLayout(this);
        
        // Verify checkbox
        m_verify = new QCheckBox(tr("Verify after write"), this);
        m_verify->setChecked(true);
        layout->addWidget(m_verify, 0, 0, 1, 2);
        
        // Virus scan
        m_virusScan = new QCheckBox(tr("Scan for viruses"), this);
        layout->addWidget(m_virusScan, 0, 2, 1, 2);
        
        // Retries
        layout->addWidget(new QLabel(tr("Retries:")), 1, 0);
        m_retries = new QSpinBox(this);
        m_retries->setRange(1, 10);
        m_retries->setValue(3);
        layout->addWidget(m_retries, 1, 1);
        
        // Sync word (for advanced users)
        layout->addWidget(new QLabel(tr("Sync:")), 1, 2);
        m_syncCombo = new QComboBox(this);
        m_syncCombo->addItem(tr("Amiga MFM (4489)"), 0x4489);
        m_syncCombo->addItem(tr("Index (F8BC)"), 0xF8BC);
        m_syncCombo->addItem(tr("Custom..."), 0);
        layout->addWidget(m_syncCombo, 1, 3);
        
        // DOS type for format
        layout->addWidget(new QLabel(tr("DOS Type:")), 2, 0);
        m_dosType = new QComboBox(this);
        m_dosType->addItem("OFS (DOS\\0)", 0);
        m_dosType->addItem("FFS (DOS\\1)", 1);
        m_dosType->addItem("OFS Intl (DOS\\2)", 2);
        m_dosType->addItem("FFS Intl (DOS\\3)", 3);
        m_dosType->addItem("FFS+DC (DOS\\5)", 5);
        m_dosType->setCurrentIndex(1);  // FFS default
        layout->addWidget(m_dosType, 2, 1, 1, 3);
    }
    
    bool verifyEnabled() const { return m_verify->isChecked(); }
    bool virusScanEnabled() const { return m_virusScan->isChecked(); }
    int retries() const { return m_retries->value(); }
    uint16_t syncWord() const { return m_syncCombo->currentData().toUInt(); }
    int dosType() const { return m_dosType->currentData().toInt(); }
    
private:
    QCheckBox* m_verify;
    QCheckBox* m_virusScan;
    QSpinBox* m_retries;
    QComboBox* m_syncCombo;
    QComboBox* m_dosType;
};

// ============================================================================
// Main Amiga Panel Widget
// ============================================================================

/**
 * @brief Complete Amiga copy/recovery panel
 * 
 * Combines all Amiga-related widgets into a single panel
 * suitable for the UFT main window.
 */
class AmigaPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit AmigaPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* mainLayout = new QVBoxLayout(this);
        
        // Top row: Mode selection and drives
        auto* topLayout = new QHBoxLayout;
        
        m_modeWidget = new CopyModeWidget(this);
        topLayout->addWidget(m_modeWidget);
        
        auto* drivePanel = new QWidget(this);
        auto* driveLayout = new QVBoxLayout(drivePanel);
        driveLayout->setContentsMargins(0, 0, 0, 0);
        
        m_sourceWidget = new DriveSelectWidget(tr("Source"), this);
        m_targetWidget = new DriveSelectWidget(tr("Target"), this);
        m_verifyWidget = new DriveSelectWidget(tr("Verify"), this);
        
        driveLayout->addWidget(m_sourceWidget);
        driveLayout->addWidget(m_targetWidget);
        driveLayout->addWidget(m_verifyWidget);
        
        topLayout->addWidget(drivePanel);
        mainLayout->addLayout(topLayout);
        
        // Middle: Track range and options
        auto* midLayout = new QHBoxLayout;
        
        m_trackWidget = new TrackRangeWidget(this);
        midLayout->addWidget(m_trackWidget);
        
        m_optionsWidget = new CopyOptionsWidget(this);
        midLayout->addWidget(m_optionsWidget);
        
        mainLayout->addLayout(midLayout);
        
        // Bottom: Control buttons and progress
        auto* bottomLayout = new QHBoxLayout;
        
        m_startButton = new QPushButton(tr("Start"), this);
        m_startButton->setMinimumHeight(40);
        m_stopButton = new QPushButton(tr("Stop"), this);
        m_stopButton->setEnabled(false);
        
        bottomLayout->addWidget(m_startButton);
        bottomLayout->addWidget(m_stopButton);
        bottomLayout->addStretch();
        
        mainLayout->addLayout(bottomLayout);
        
        // Progress bar
        m_progress = new QProgressBar(this);
        m_progress->setRange(0, 160);  // 80 tracks * 2 sides
        m_progress->setValue(0);
        mainLayout->addWidget(m_progress);
        
        // Status label
        m_status = new QLabel(tr("Ready"), this);
        mainLayout->addWidget(m_status);
        
        // Connect mode changes to enable/disable options
        connect(m_modeWidget, &CopyModeWidget::modeChanged,
                this, &AmigaPanel::onModeChanged);
        connect(m_startButton, &QPushButton::clicked,
                this, &AmigaPanel::startOperation);
        connect(m_stopButton, &QPushButton::clicked,
                this, &AmigaPanel::stopOperation);
    }
    
    CopyModeWidget::Mode currentMode() const {
        return m_modeWidget->currentMode();
    }
    
signals:
    void startOperation();
    void stopOperation();
    void operationProgress(int track, int side, int percent);
    
public slots:
    void setStatus(const QString& status) {
        m_status->setText(status);
    }
    
    void setProgress(int track, int side) {
        m_progress->setValue(track * 2 + side);
    }
    
    void setRunning(bool running) {
        m_startButton->setEnabled(!running);
        m_stopButton->setEnabled(running);
        m_modeWidget->setEnabled(!running);
        m_sourceWidget->setEnabled(!running);
        m_targetWidget->setEnabled(!running);
        m_trackWidget->setEnabled(!running);
        m_optionsWidget->setEnabled(!running);
    }
    
private slots:
    void onModeChanged(int mode) {
        // Adjust UI based on selected mode
        bool needsTarget = (mode <= CopyModeWidget::Nibble) ||
                           (mode == CopyModeWidget::Format) ||
                           (mode == CopyModeWidget::QuickFormat);
        bool needsSource = (mode <= CopyModeWidget::Nibble) ||
                           (mode == CopyModeWidget::Verify) ||
                           (mode == CopyModeWidget::Directory) ||
                           (mode == CopyModeWidget::DiskName);
        
        m_targetWidget->setEnabled(needsTarget);
        m_sourceWidget->setEnabled(needsSource);
        m_verifyWidget->setEnabled(needsTarget);
    }
    
private:
    CopyModeWidget* m_modeWidget;
    DriveSelectWidget* m_sourceWidget;
    DriveSelectWidget* m_targetWidget;
    DriveSelectWidget* m_verifyWidget;
    TrackRangeWidget* m_trackWidget;
    CopyOptionsWidget* m_optionsWidget;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QProgressBar* m_progress;
    QLabel* m_status;
};

} // namespace Amiga
} // namespace UFT

#endif // __cplusplus

#endif // UFT_AMIGA_GUI_H
