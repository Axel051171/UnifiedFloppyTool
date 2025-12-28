/**
 * @file diskvisualizationwindow.cpp
 * @brief Implementation of Disk Visualization Window
 */

#include "diskvisualizationwindow.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <cmath>

// ============================================================================
// TrackInfo Implementation
// ============================================================================

QColor TrackInfo::getColor() const
{
    if (errorCount > 0) {
        if (quality < 50) return QColor(255, 0, 0);      // Red
        else return QColor(255, 165, 0);                  // Orange
    } else if (quality >= 95) {
        return QColor(0, 255, 0);                         // Bright Green
    } else if (quality >= 80) {
        return QColor(50, 200, 50);                       // Green
    } else {
        return QColor(100, 100, 255);                     // Blue (unknown)
    }
}

// ============================================================================
// DualDiskWidget Implementation
// ============================================================================

DualDiskWidget::DualDiskWidget(QWidget *parent)
    : QWidget(parent)
    , m_maxTracks(84)
    , m_selectedTrack(-1)
    , m_selectedSide(0)
{
    setMinimumSize(1000, 520);
    setMouseTracking(true);
}

void DualDiskWidget::setTrackData(const std::vector<TrackInfo>& tracks)
{
    m_tracks = tracks;
    update();
}

void DualDiskWidget::setSelectedTrack(int track, int side)
{
    m_selectedTrack = track;
    m_selectedSide = side;
    update();
}

void DualDiskWidget::setMaxTracks(int maxTracks)
{
    m_maxTracks = maxTracks;
    update();
}

void DualDiskWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), Qt::black);
    
    // Draw two disks side-by-side
    int diskRadius = std::min(width() / 2, height()) / 2 - 20;
    
    // Side 0 (left)
    drawDisk(painter, width() / 4, height() / 2, diskRadius, 0);
    
    // Side 1 (right)
    drawDisk(painter, 3 * width() / 4, height() / 2, diskRadius, 1);
    
    // Labels
    painter.setPen(Qt::white);
    QFont labelFont = painter.font();
    labelFont.setPointSize(12);
    painter.setFont(labelFont);
    
    painter.drawText(width() / 4 - 30, 20, "Side 0");
    painter.drawText(3 * width() / 4 - 30, 20, "Side 1");
    
    // Disk info
    painter.drawText(10, height() - 10, 
                    QString("Side 0: %1 Tracks").arg(m_maxTracks));
    painter.drawText(width() / 2 + 10, height() - 10, 
                    QString("Side 1: %1 Tracks").arg(m_maxTracks));
}

void DualDiskWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    
    int diskRadius = std::min(width() / 2, height()) / 2 - 20;
    
    // Check Side 0
    int dx0 = event->pos().x() - width() / 4;
    int dy0 = event->pos().y() - height() / 2;
    double dist0 = std::sqrt(dx0*dx0 + dy0*dy0);
    
    if (dist0 < diskRadius && dist0 > 30) {
        // Calculate track from distance
        int track = static_cast<int>((dist0 - 30) / ((diskRadius - 30) / m_maxTracks));
        if (track >= 0 && track < m_maxTracks) {
            emit trackClicked(track, 0);
            return;
        }
    }
    
    // Check Side 1
    int dx1 = event->pos().x() - (3 * width() / 4);
    int dy1 = event->pos().y() - height() / 2;
    double dist1 = std::sqrt(dx1*dx1 + dy1*dy1);
    
    if (dist1 < diskRadius && dist1 > 30) {
        int track = static_cast<int>((dist1 - 30) / ((diskRadius - 30) / m_maxTracks));
        if (track >= 0 && track < m_maxTracks) {
            emit trackClicked(track, 1);
            return;
        }
    }
}

void DualDiskWidget::drawDisk(QPainter& painter, int centerX, int centerY, 
                               int radius, int side)
{
    // Draw each track as concentric circles
    for (int track = 0; track < m_maxTracks; ++track) {
        // Find track info
        QColor color = Qt::gray;
        
        for (const auto& info : m_tracks) {
            if (info.cylinder == track && info.head == side) {
                color = info.getColor();
                break;
            }
        }
        
        // Calculate radii
        double innerRadius = 30 + (track * (radius - 30) / m_maxTracks);
        double outerRadius = 30 + ((track + 1) * (radius - 30) / m_maxTracks);
        
        // Highlight if selected
        if (track == m_selectedTrack && side == m_selectedSide) {
            color = color.lighter(150);
        }
        
        // Draw track as circle segments (simulating sectors)
        int numSegments = 18;
        for (int seg = 0; seg < numSegments; seg++) {
            double startAngle = seg * 360.0 / numSegments;
            double spanAngle = 360.0 / numSegments - 1;
            
            QColor segColor = color;
            if (seg % 2 == 0) {
                segColor = segColor.darker(110);
            }
            
            painter.setBrush(segColor);
            painter.setPen(Qt::NoPen);
            painter.drawPie(
                centerX - outerRadius, 
                centerY - outerRadius,
                outerRadius * 2, 
                outerRadius * 2,
                startAngle * 16, 
                spanAngle * 16
            );
            
            // Draw inner circle to create ring
            painter.setBrush(Qt::black);
            painter.drawPie(
                centerX - innerRadius, 
                centerY - innerRadius,
                innerRadius * 2, 
                innerRadius * 2,
                startAngle * 16, 
                spanAngle * 16
            );
        }
    }
    
    // Draw center hole
    painter.setBrush(Qt::black);
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.drawEllipse(centerX - 30, centerY - 30, 60, 60);
    
    // Draw label in center
    painter.setPen(Qt::white);
    painter.drawText(centerX - 25, centerY + 5, 
                    QString("Side %1").arg(side));
}

// ============================================================================
// DiskVisualizationWindow Implementation
// ============================================================================

DiskVisualizationWindow::DiskVisualizationWindow(QWidget *parent)
    : QDialog(parent)
    , m_currentTrack(0)
    , m_currentSide(0)
{
    setWindowTitle("Visual Floppy Disk - Forensic Analysis");
    resize(1280, 600);
    
    createUI();
    setupConnections();
}

void DiskVisualizationWindow::createUI()
{
    auto *mainLayout = new QHBoxLayout(this);
    
    // LEFT: Dual Disk Visualization
    m_diskWidget = new DualDiskWidget(this);
    mainLayout->addWidget(m_diskWidget, 2);
    
    // RIGHT: Info Panel
    m_infoPanel = new QWidget(this);
    auto *infoPanelLayout = new QVBoxLayout(m_infoPanel);
    
    // Status Group
    m_statusGroup = new QGroupBox("Status", m_infoPanel);
    auto *statusLayout = new QVBoxLayout(m_statusGroup);
    
    m_diskInfoLabel = new QLabel("No disk loaded", m_statusGroup);
    m_diskInfoLabel->setStyleSheet("QLabel { color: #00FF00; background: black; "
                                   "padding: 5px; font-family: monospace; }");
    statusLayout->addWidget(m_diskInfoLabel);
    
    m_trackInfoLabel = new QLabel("Track: - Side: -", m_statusGroup);
    m_trackInfoLabel->setStyleSheet("QLabel { color: #00FF00; background: black; "
                                    "padding: 5px; font-family: monospace; }");
    statusLayout->addWidget(m_trackInfoLabel);
    
    // Hex Dump
    m_hexDumpEdit = new QTextEdit(m_statusGroup);
    m_hexDumpEdit->setReadOnly(true);
    m_hexDumpEdit->setStyleSheet("QTextEdit { background: black; color: #00FF00; "
                                 "font-family: 'Courier New'; font-size: 9pt; }");
    m_hexDumpEdit->setMaximumHeight(150);
    statusLayout->addWidget(m_hexDumpEdit);
    
    infoPanelLayout->addWidget(m_statusGroup);
    
    // Format Group
    m_formatGroup = new QGroupBox("Track analysis format", m_infoPanel);
    auto *formatLayout = new QVBoxLayout(m_formatGroup);
    
    m_isoMFMCheck = new QCheckBox("ISO MFM", m_formatGroup);
    m_isoMFMCheck->setChecked(true);
    formatLayout->addWidget(m_isoMFMCheck);
    
    m_isoFMCheck = new QCheckBox("ISO FM", m_formatGroup);
    formatLayout->addWidget(m_isoFMCheck);
    
    m_amigaMFMCheck = new QCheckBox("AMIGA MFM", m_formatGroup);
    formatLayout->addWidget(m_amigaMFMCheck);
    
    m_eEmuCheck = new QCheckBox("E-Emu", m_formatGroup);
    formatLayout->addWidget(m_eEmuCheck);
    
    m_aed6200pCheck = new QCheckBox("AED 6200P", m_formatGroup);
    formatLayout->addWidget(m_aed6200pCheck);
    
    m_membrainCheck = new QCheckBox("MEMBRAIN", m_formatGroup);
    formatLayout->addWidget(m_membrainCheck);
    
    m_appleIICheck = new QCheckBox("Apple II", m_formatGroup);
    formatLayout->addWidget(m_appleIICheck);
    
    infoPanelLayout->addWidget(m_formatGroup);
    
    // Track/Side Selection
    m_selectionGroup = new QGroupBox("Track / Side selection", m_infoPanel);
    auto *selectionLayout = new QFormLayout(m_selectionGroup);
    
    m_trackSpinBox = new QSpinBox(m_selectionGroup);
    m_trackSpinBox->setRange(0, 83);
    selectionLayout->addRow("Track number:", m_trackSpinBox);
    
    m_sideSpinBox = new QSpinBox(m_selectionGroup);
    m_sideSpinBox->setRange(0, 1);
    selectionLayout->addRow("Side number:", m_sideSpinBox);
    
    infoPanelLayout->addWidget(m_selectionGroup);
    
    // View Mode
    auto *viewModeLayout = new QHBoxLayout();
    m_trackViewRadio = new QRadioButton("Track view mode", m_infoPanel);
    m_diskViewRadio = new QRadioButton("Disk view mode", m_infoPanel);
    m_diskViewRadio->setChecked(true);
    viewModeLayout->addWidget(m_trackViewRadio);
    viewModeLayout->addWidget(m_diskViewRadio);
    infoPanelLayout->addLayout(viewModeLayout);
    
    // Edit tools button
    auto *editToolsButton = new QPushButton("Edit tools", m_infoPanel);
    infoPanelLayout->addWidget(editToolsButton);
    
    // Spacer
    infoPanelLayout->addStretch();
    
    // OK Button
    auto *okButton = new QPushButton("OK", m_infoPanel);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    infoPanelLayout->addWidget(okButton);
    
    mainLayout->addWidget(m_infoPanel, 1);
}

void DiskVisualizationWindow::setupConnections()
{
    connect(m_diskWidget, &DualDiskWidget::trackClicked,
            this, [this](int cylinder, int head) {
        setSelectedTrack(cylinder, head);
        emit trackSelected(cylinder, head);
    });
    
    connect(m_trackSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DiskVisualizationWindow::onTrackSpinBoxChanged);
    
    connect(m_sideSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DiskVisualizationWindow::onSideSpinBoxChanged);
}

void DiskVisualizationWindow::loadDisk(const QString& diskPath)
{
    m_currentDiskPath = diskPath;
    m_diskInfoLabel->setText(QString("Loaded: %1").arg(diskPath));
}

void DiskVisualizationWindow::updateTrackQuality(int cylinder, int head, 
                                                  int quality, int errorCount)
{
    // Find or create track info
    for (auto& track : m_trackData) {
        if (track.cylinder == cylinder && track.head == head) {
            track.quality = quality;
            track.errorCount = errorCount;
            m_diskWidget->setTrackData(m_trackData);
            return;
        }
    }
    
    // Create new track info
    TrackInfo info;
    info.cylinder = cylinder;
    info.head = head;
    info.quality = quality;
    info.errorCount = errorCount;
    info.goodSectors = 18 - errorCount;
    info.totalSectors = 18;
    info.format = TrackInfo::Format::ISO_MFM;
    
    m_trackData.push_back(info);
    m_diskWidget->setTrackData(m_trackData);
}

void DiskVisualizationWindow::setSelectedTrack(int cylinder, int head)
{
    m_currentTrack = cylinder;
    m_currentSide = head;
    
    m_trackSpinBox->setValue(cylinder);
    m_sideSpinBox->setValue(head);
    
    m_diskWidget->setSelectedTrack(cylinder, head);
    
    // Update info label
    m_trackInfoLabel->setText(
        QString("Track: %1  Side: %2").arg(cylinder).arg(head));
    
    // Update hex dump (example data)
    QString hexDump;
    for (int i = 0; i < 16; i++) {
        hexDump += QString("%1: ").arg(i*16, 5, 16, QChar('0')).toUpper();
        for (int j = 0; j < 16; j++) {
            hexDump += QString("%1 ").arg((i*16+j) % 256, 2, 16, QChar('0')).toUpper();
        }
        hexDump += "\n";
    }
    m_hexDumpEdit->setPlainText(hexDump);
}

void DiskVisualizationWindow::onTrackSpinBoxChanged(int value)
{
    setSelectedTrack(value, m_currentSide);
}

void DiskVisualizationWindow::onSideSpinBoxChanged(int value)
{
    setSelectedTrack(m_currentTrack, value);
}

void DiskVisualizationWindow::onFormatComboChanged(int index)
{
    Q_UNUSED(index);
    // Format selection logic here
}

void DiskVisualizationWindow::onDiskViewModeToggled(bool checked)
{
    Q_UNUSED(checked);
    // View mode toggle logic here
}
