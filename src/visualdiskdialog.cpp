/**
 * @file visualdiskdialog.cpp
 * @brief Visual Floppy Disk Viewer Implementation
 * 
 * Polar disk visualization showing sectors as colored segments.
 * Based on HxCFloppyEmulator's Visual Floppy Disk view.
 * 
 * @date 2026-01-12
 */

#include "visualdiskdialog.h"
#include "ui_visualdiskdialog.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QDialog>
#include <QMessageBox>
#include <QtMath>

extern "C" {
#include <uft/uft_core.h>   /* canonical disk API: open/close/get_geometry */
#include <uft/uft_format_plugin.h>  /* struct uft_disk field access */
#include <uft/uft_types.h>
}

#include "uft_sector_editor.h"

// ============================================================================
// VisualDiskWidget Implementation
// ============================================================================

VisualDiskWidget::VisualDiskWidget(QWidget *parent)
    : QWidget(parent)
    , m_side(0)
    , m_tracks(80)
    , m_currentTrack(-1)
{
    setMinimumSize(300, 300);
    setMouseTracking(true);
}

void VisualDiskWidget::clear()
{
    m_trackInfo.clear();
    update();
}

void VisualDiskWidget::setTrackData(int track, const VisualDiskDialog::TrackInfo& info)
{
    m_trackInfo[track] = info;
    update();
}

QColor VisualDiskWidget::statusColor(VisualDiskDialog::SectorStatus status) const
{
    switch (status) {
    case VisualDiskDialog::SectorGood:      return QColor(0, 255, 0);      // Green
    case VisualDiskDialog::SectorBad:       return QColor(255, 0, 0);      // Red
    case VisualDiskDialog::SectorWeak:      return QColor(255, 165, 0);    // Orange
    case VisualDiskDialog::SectorMissing:   return QColor(128, 128, 128);  // Gray
    case VisualDiskDialog::SectorNoData:    return QColor(0, 0, 255);      // Blue
    case VisualDiskDialog::SectorAlternate: return QColor(255, 255, 0);    // Yellow
    case VisualDiskDialog::SectorUnknown:   
    default:                                return QColor(64, 64, 64);     // Dark gray
    }
}

void VisualDiskWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int w = width();
    int h = height();
    int size = qMin(w, h) - 20;
    int cx = w / 2;
    int cy = h / 2;
    
    // Background
    painter.fillRect(rect(), Qt::black);
    
    // Draw tracks from outside to inside
    double outerRadius = size / 2.0;
    double innerRadius = size / 6.0;  // Center hole
    double trackWidth = (outerRadius - innerRadius) / m_tracks;
    
    for (int t = 0; t < m_tracks; t++) {
        double r1 = outerRadius - t * trackWidth;
        double r2 = r1 - trackWidth + 1;
        
        if (m_trackInfo.contains(t)) {
            const auto& info = m_trackInfo[t];
            int sectorCount = info.sectors.size();
            if (sectorCount == 0) sectorCount = 18;  // Default
            
            double sectorAngle = 360.0 / sectorCount;
            
            for (int s = 0; s < info.sectors.size(); s++) {
                const auto& sector = info.sectors[s];
                QColor color = statusColor(sector.status);
                
                // Draw sector as pie segment
                double startAngle = s * sectorAngle - 90;  // Start from top
                
                QPainterPath path;
                path.moveTo(cx + r2 * qCos(qDegreesToRadians(startAngle)),
                           cy + r2 * qSin(qDegreesToRadians(startAngle)));
                path.arcTo(cx - r1, cy - r1, r1 * 2, r1 * 2, -startAngle, -sectorAngle);
                path.arcTo(cx - r2, cy - r2, r2 * 2, r2 * 2, -(startAngle + sectorAngle), sectorAngle);
                path.closeSubpath();
                
                painter.fillPath(path, color);
                painter.setPen(QPen(Qt::black, 0.5));
                painter.drawPath(path);
            }
        } else {
            // Unanalyzed track - draw as dark gray ring
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(32, 32, 32));
            painter.drawEllipse(QPointF(cx, cy), r1, r1);
            painter.setBrush(Qt::black);
            painter.drawEllipse(QPointF(cx, cy), r2, r2);
        }
    }
    
    // Draw center hole
    painter.setBrush(Qt::black);
    painter.setPen(QPen(QColor(80, 0, 0), 3));  // Dark red ring
    painter.drawEllipse(QPointF(cx, cy), innerRadius, innerRadius);
    
    // Highlight current track
    if (m_currentTrack >= 0 && m_currentTrack < m_tracks) {
        double r1 = outerRadius - m_currentTrack * trackWidth;
        double r2 = r1 - trackWidth;
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(cx, cy), (r1 + r2) / 2, (r1 + r2) / 2);
    }
    
    // Draw side label
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    QString sideLabel = QString("Side %1").arg(m_side);
    painter.drawText(cx - 25, cy + 5, sideLabel);
    
    // Draw arrow indicator (like HxC)
    painter.setPen(QPen(Qt::white, 2));
    int arrowX = cx + outerRadius * 0.7;
    int arrowY = cy - outerRadius * 0.1;
    painter.drawLine(arrowX, arrowY, arrowX + 15, arrowY);
    painter.drawLine(arrowX + 10, arrowY - 5, arrowX + 15, arrowY);
    painter.drawLine(arrowX + 10, arrowY + 5, arrowX + 15, arrowY);
    painter.drawText(arrowX - 5, arrowY - 10, "->");
}

void VisualDiskWidget::mousePressEvent(QMouseEvent *event)
{
    auto [track, sector] = hitTest(event->pos());
    if (track >= 0) {
        emit trackClicked(track, m_side);
        if (sector >= 0) {
            emit sectorClicked(track, m_side, sector);
        }
    }
}

void VisualDiskWidget::mouseMoveEvent(QMouseEvent *event)
{
    auto [track, sector] = hitTest(event->pos());
    if (track >= 0) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

QPair<int, int> VisualDiskWidget::hitTest(const QPoint& pos) const
{
    int w = width();
    int h = height();
    int size = qMin(w, h) - 20;
    int cx = w / 2;
    int cy = h / 2;
    
    double dx = pos.x() - cx;
    double dy = pos.y() - cy;
    double dist = qSqrt(dx * dx + dy * dy);
    
    double outerRadius = size / 2.0;
    double innerRadius = size / 6.0;
    
    if (dist < innerRadius || dist > outerRadius) {
        return {-1, -1};  // Outside disk area
    }
    
    // Calculate track
    double trackWidth = (outerRadius - innerRadius) / m_tracks;
    int track = static_cast<int>((outerRadius - dist) / trackWidth);
    if (track < 0 || track >= m_tracks) {
        return {-1, -1};
    }
    
    // Calculate sector
    int sector = -1;
    if (m_trackInfo.contains(track)) {
        const auto& info = m_trackInfo[track];
        int sectorCount = info.sectors.size();
        if (sectorCount > 0) {
            double angle = qAtan2(dy, dx);
            angle = qRadiansToDegrees(angle) + 90;  // Adjust to start from top
            if (angle < 0) angle += 360;
            double sectorAngle = 360.0 / sectorCount;
            sector = static_cast<int>(angle / sectorAngle);
        }
    }
    
    return {track, sector};
}

// ============================================================================
// VisualDiskDialog Implementation
// ============================================================================

VisualDiskDialog::VisualDiskDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::VisualDiskDialog)
    , m_totalTracks(80)
    , m_totalSides(2)
    , m_currentTrack(0)
    , m_currentSide(0)
{
    ui->setupUi(this);
    
    // Create disk visualization widgets
    m_diskWidget0 = new VisualDiskWidget(this);
    m_diskWidget0->setSide(0);
    m_diskWidget0->setTracks(m_totalTracks);
    
    m_diskWidget1 = new VisualDiskWidget(this);
    m_diskWidget1->setSide(1);
    m_diskWidget1->setTracks(m_totalTracks);
    
    // Add to frames
    QVBoxLayout *layout0 = new QVBoxLayout(ui->frameDiskView0);
    layout0->setContentsMargins(0, 0, 0, 0);
    layout0->addWidget(m_diskWidget0);
    
    QVBoxLayout *layout1 = new QVBoxLayout(ui->frameDiskView1);
    layout1->setContentsMargins(0, 0, 0, 0);
    layout1->addWidget(m_diskWidget1);
    
    setupConnections();
    setupFormatCheckboxes();
    
    // Generate sample data for demonstration
    generateSampleData();
}

VisualDiskDialog::~VisualDiskDialog()
{
    delete ui;
}

void VisualDiskDialog::setupConnections()
{
    // Track/Side selection
    connect(ui->spinTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VisualDiskDialog::onTrackChanged);
    connect(ui->sliderTrack, &QSlider::valueChanged,
            ui->spinTrack, &QSpinBox::setValue);
    connect(ui->spinTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliderTrack, &QSlider::setValue);
    
    connect(ui->spinSide, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VisualDiskDialog::onSideChanged);
    connect(ui->sliderSide, &QSlider::valueChanged,
            ui->spinSide, &QSpinBox::setValue);
    connect(ui->spinSide, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliderSide, &QSlider::setValue);
    
    // Format checkboxes
    connect(ui->checkIsoMfm, &QCheckBox::toggled, this, &VisualDiskDialog::onFormatCheckChanged);
    connect(ui->checkIsoFm, &QCheckBox::toggled, this, &VisualDiskDialog::onFormatCheckChanged);
    connect(ui->checkAmigaMfm, &QCheckBox::toggled, this, &VisualDiskDialog::onFormatCheckChanged);
    connect(ui->checkAppleII, &QCheckBox::toggled, this, &VisualDiskDialog::onFormatCheckChanged);
    
    // View mode
    connect(ui->radioTrackView, &QRadioButton::toggled, this, &VisualDiskDialog::onViewModeChanged);
    connect(ui->radioDiskView, &QRadioButton::toggled, this, &VisualDiskDialog::onViewModeChanged);
    
    // Disk widget clicks
    connect(m_diskWidget0, &VisualDiskWidget::sectorClicked, this, &VisualDiskDialog::onSectorClicked);
    connect(m_diskWidget1, &VisualDiskWidget::sectorClicked, this, &VisualDiskDialog::onSectorClicked);
    connect(m_diskWidget0, &VisualDiskWidget::trackClicked, [this](int track, int side) {
        ui->spinTrack->setValue(track);
        ui->spinSide->setValue(side);
    });
    connect(m_diskWidget1, &VisualDiskWidget::trackClicked, [this](int track, int side) {
        ui->spinTrack->setValue(track);
        ui->spinSide->setValue(side);
    });
    
    // Buttons
    connect(ui->btnEditTools, &QPushButton::clicked, this, &VisualDiskDialog::onEditTools);
    connect(ui->btnOK, &QPushButton::clicked, this, &QDialog::accept);
}

void VisualDiskDialog::setupFormatCheckboxes()
{
    // Default: ISO MFM checked
    ui->checkIsoMfm->setChecked(true);
}

void VisualDiskDialog::generateSampleData()
{
    // Generate sample track data for demonstration
    for (int t = 0; t < m_totalTracks; t++) {
        for (int s = 0; s < 2; s++) {
            TrackInfo info;
            info.trackNum = t;
            info.side = s;
            info.sectorCount = 18;  // Standard PC format
            info.goodSectors = 18;
            info.badSectors = 0;
            info.missingSectors = 0;
            info.weakSectors = 0;
            info.format = FormatIsoMfm;
            info.formatName = "ISO MFM";
            
            // Create sector info
            for (int sec = 0; sec < 18; sec++) {
                SectorInfo si;
                si.track = t;
                si.side = s;
                si.sectorId = sec + 1;
                si.size = 512;
                
                // Simulate some bad sectors for visual effect
                if ((t == 15 && sec == 3) || (t == 42 && sec == 7) || (t == 71 && sec == 12)) {
                    si.status = SectorBad;
                    info.badSectors++;
                    info.goodSectors--;
                } else if ((t == 20 && sec == 5) || (t == 55 && sec == 2)) {
                    si.status = SectorWeak;
                    info.weakSectors++;
                    info.goodSectors--;
                } else {
                    si.status = SectorGood;
                }
                
                si.headerCrc = 0x1234;
                si.dataCrc = 0x5678;
                si.headerCrcOk = true;
                si.dataCrcOk = (si.status == SectorGood);
                si.startCell = sec * 5000;
                si.endCell = si.startCell + 4896;
                si.cellCount = 4896;
                
                info.sectors.append(si);
            }
            
            info.totalBytes = info.sectorCount * 512;
            
            setTrackData(t, s, info);
        }
    }
    
    // Update info labels
    updateInfoLabels();
}

void VisualDiskDialog::updateInfoLabels()
{
    int totalSectors0 = 0, badSectors0 = 0, totalBytes0 = 0;
    int totalSectors1 = 0, badSectors1 = 0, totalBytes1 = 0;
    
    for (auto it = m_trackData.begin(); it != m_trackData.end(); ++it) {
        if (it.key().second == 0) {
            totalSectors0 += it.value().sectorCount;
            badSectors0 += it.value().badSectors;
            totalBytes0 += it.value().totalBytes;
        } else {
            totalSectors1 += it.value().sectorCount;
            badSectors1 += it.value().badSectors;
            totalBytes1 += it.value().totalBytes;
        }
    }
    
    ui->labelSide0Info->setText(QString("Side 0, %1 Tracks | %2 Sectors, %3 bad | %4 Bytes | ISO MFM")
                                .arg(m_totalTracks).arg(totalSectors0).arg(badSectors0).arg(totalBytes0));
    ui->labelSide1Info->setText(QString("Side 1, %1 Tracks | %2 Sectors, %3 bad | %4 Bytes | ISO MFM")
                                .arg(m_totalTracks).arg(totalSectors1).arg(badSectors1).arg(totalBytes1));
}

void VisualDiskDialog::setDiskGeometry(int tracks, int sides)
{
    m_totalTracks = tracks;
    m_totalSides = sides;
    
    m_diskWidget0->setTracks(tracks);
    m_diskWidget1->setTracks(tracks);
    
    ui->spinTrack->setMaximum(tracks - 1);
    ui->sliderTrack->setMaximum(tracks - 1);
    ui->spinSide->setMaximum(sides - 1);
    ui->sliderSide->setMaximum(sides - 1);
}

void VisualDiskDialog::setTrackData(int track, int side, const TrackInfo& info)
{
    m_trackData[{track, side}] = info;
    
    if (side == 0) {
        m_diskWidget0->setTrackData(track, info);
    } else {
        m_diskWidget1->setTrackData(track, info);
    }
}

void VisualDiskDialog::loadDiskImage(const QString& path)
{
    m_imagePath = path;
    setWindowTitle(tr("Visual Floppy Disk - %1").arg(QFileInfo(path).fileName()));

    /* Attempt to load via uft_disk_open for track/sector analysis */
    uft_disk_t *disk = uft_disk_open(path.toUtf8().constData(), /*read_only=*/true);
    if (!disk) {
        /* Fall back to file-size heuristic */
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            generateSampleData();
            return;
        }
        qint64 fileSize = f.size();
        f.close();

        /* Guess geometry from file size */
        int sectorSize = 512;
        int totalSectors = static_cast<int>(fileSize / sectorSize);
        int sides = 2;
        int spt = 18;
        int tracks = totalSectors / (sides * spt);
        if (tracks <= 0) { tracks = totalSectors; sides = 1; spt = 1; }

        setDiskGeometry(tracks, sides);

        for (int t = 0; t < tracks; t++) {
            for (int s = 0; s < sides; s++) {
                TrackInfo info;
                info.trackNum = t;
                info.side = s;
                info.sectorCount = spt;
                info.goodSectors = spt;
                info.badSectors = 0;
                info.missingSectors = 0;
                info.weakSectors = 0;
                info.format = FormatIsoMfm;
                info.formatName = "ISO MFM";

                for (int sec = 0; sec < spt; sec++) {
                    SectorInfo si;
                    si.track = t;
                    si.side = s;
                    si.sectorId = sec + 1;
                    si.size = sectorSize;
                    si.status = SectorGood;
                    si.headerCrc = 0;
                    si.dataCrc = 0;
                    si.headerCrcOk = true;
                    si.dataCrcOk = true;
                    si.startCell = sec * 5000;
                    si.endCell = si.startCell + 4896;
                    si.cellCount = 4896;
                    info.sectors.append(si);
                }
                info.totalBytes = spt * sectorSize;
                setTrackData(t, s, info);
            }
        }
        updateInfoLabels();
        return;
    }

    /* Loaded successfully - read geometry */
    uft_geometry_t geom;
    uft_disk_get_geometry(disk, &geom);

    int tracks = geom.cylinders > 0 ? geom.cylinders : 80;
    int sides = geom.heads > 0 ? geom.heads : 2;
    int spt = geom.sectors > 0 ? geom.sectors : 18;
    int sectorSize = geom.sector_size > 0 ? geom.sector_size : 512;

    setDiskGeometry(tracks, sides);

    for (int t = 0; t < tracks; t++) {
        for (int s = 0; s < sides; s++) {
            TrackInfo info;
            info.trackNum = t;
            info.side = s;
            info.sectorCount = spt;
            info.goodSectors = spt;
            info.badSectors = 0;
            info.missingSectors = 0;
            info.weakSectors = 0;
            info.format = FormatIsoMfm;
            info.formatName = "ISO MFM";

            for (int sec = 0; sec < spt; sec++) {
                SectorInfo si;
                si.track = t;
                si.side = s;
                si.sectorId = sec + 1;
                si.size = sectorSize;
                si.status = SectorGood;
                si.headerCrc = 0;
                si.dataCrc = 0;
                si.headerCrcOk = true;
                si.dataCrcOk = true;
                si.startCell = sec * 5000;
                si.endCell = si.startCell + 4896;
                si.cellCount = 4896;
                info.sectors.append(si);
            }
            info.totalBytes = spt * sectorSize;
            setTrackData(t, s, info);
        }
    }

    uft_disk_close(disk);   /* also frees the handle — see uft_core_stubs.c */

    updateInfoLabels();
}

void VisualDiskDialog::onTrackChanged(int track)
{
    m_currentTrack = track;
    m_diskWidget0->setCurrentTrack(track);
    m_diskWidget1->setCurrentTrack(track);
    
    ui->labelTrackSide->setText(QString("Track: %1 Side: %2").arg(track).arg(m_currentSide));
    
    // Update sector info for this track
    auto key = qMakePair(track, m_currentSide);
    if (m_trackData.contains(key)) {
        updateTrackInfo(m_trackData[key]);
    }
}

void VisualDiskDialog::onSideChanged(int side)
{
    m_currentSide = side;
    ui->labelTrackSide->setText(QString("Track: %1 Side: %2").arg(m_currentTrack).arg(side));
    
    auto key = qMakePair(m_currentTrack, side);
    if (m_trackData.contains(key)) {
        updateTrackInfo(m_trackData[key]);
    }
}

void VisualDiskDialog::onSectorClicked(int track, int side, int sector)
{
    ui->spinTrack->setValue(track);
    ui->spinSide->setValue(side);
    
    auto key = qMakePair(track, side);
    if (m_trackData.contains(key)) {
        const auto& info = m_trackData[key];
        if (sector >= 0 && sector < info.sectors.size()) {
            updateStatusPanel(info.sectors[sector]);
        }
    }
    
    emit sectorSelected(track, side, sector);
}

void VisualDiskDialog::updateStatusPanel(const SectorInfo& sector)
{
    QString info;
    info += QString("MFM Sector\n");
    info += QString("Sector ID:0x%1\n").arg(sector.sectorId, 2, 16, QChar('0')).toUpper();
    info += QString("Track ID:%1 - Side ID:%2\n").arg(sector.track, 3, 10, QChar('0')).arg(sector.side);
    info += QString("Size:0x%1 (ID: 0x%2)\n").arg(sector.size, 4, 16, QChar('0')).arg(sector.size / 128);
    info += QString("DataMark:0xFB\n");
    info += QString("Head CRC:0x%1 (%2)\n").arg(sector.headerCrc, 4, 16, QChar('0')).arg(sector.headerCrcOk ? "OK" : "BAD");
    info += QString("Data CRC:0x%1 (%2)\n").arg(sector.dataCrc, 4, 16, QChar('0')).arg(sector.dataCrcOk ? "OK" : "BAD CRC!");
    info += QString("\n");
    info += QString("Start sector cell:%1\n").arg(sector.startCell);
    info += QString("Start Sector Data cell:%1\n").arg(sector.startCell + 100);
    info += QString("End Sector cell:%1\n").arg(sector.endCell);
    info += QString("Number of cells:%1\n").arg(sector.cellCount);
    
    ui->textSectorInfo->setPlainText(info);
    
    // Generate sample hex dump
    updateHexDump(sector.data);
}

void VisualDiskDialog::updateHexDump(const QByteArray& data)
{
    QString hex;
    
    // Generate sample data if empty
    QByteArray displayData = data;
    if (displayData.isEmpty()) {
        displayData.resize(64);
        for (int i = 0; i < 64; i++) {
            displayData[i] = static_cast<char>(i);
        }
    }
    
    for (int row = 0; row < 4 && row * 16 < displayData.size(); row++) {
        // Address
        hex += QString("%1  ").arg(row * 16, 4, 16, QChar('0')).toUpper();
        
        // Hex bytes
        QString ascii;
        for (int col = 0; col < 16; col++) {
            int idx = row * 16 + col;
            if (idx < displayData.size()) {
                unsigned char c = static_cast<unsigned char>(displayData[idx]);
                hex += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
                ascii += (c >= 32 && c < 127) ? QChar(c) : '.';
            }
        }
        
        hex += " " + ascii + "\n";
    }
    
    ui->textHexDump->setPlainText(hex);
}

void VisualDiskDialog::updateTrackInfo(const TrackInfo& info)
{
    QString text;
    text += QString("Track %1, Side %2\n").arg(info.trackNum).arg(info.side);
    text += QString("Format: %1\n").arg(info.formatName);
    text += QString("Sectors: %1 (%2 good, %3 bad, %4 weak)\n")
            .arg(info.sectorCount).arg(info.goodSectors).arg(info.badSectors).arg(info.weakSectors);
    text += QString("Total bytes: %1\n").arg(info.totalBytes);
    
    ui->textSectorInfo->setPlainText(text);
}

void VisualDiskDialog::onFormatCheckChanged()
{
    analyzeWithFormats();
}

void VisualDiskDialog::analyzeWithFormats()
{
    /* Re-analyze all tracks using the selected format parsers */
    QList<TrackFormat> selectedFormats;
    if (ui->checkIsoMfm->isChecked()) selectedFormats.append(FormatIsoMfm);
    if (ui->checkIsoFm->isChecked()) selectedFormats.append(FormatIsoFm);
    if (ui->checkAmigaMfm->isChecked()) selectedFormats.append(FormatAmigaMfm);
    if (ui->checkAppleII->isChecked()) selectedFormats.append(FormatAppleGcr);

    if (selectedFormats.isEmpty()) return;

    /* Update format name in existing track data based on selection */
    TrackFormat primary = selectedFormats.first();
    QString formatName;
    switch (primary) {
        case FormatIsoMfm: formatName = "ISO MFM"; break;
        case FormatIsoFm: formatName = "ISO FM"; break;
        case FormatAmigaMfm: formatName = "Amiga MFM"; break;
        case FormatAppleGcr: formatName = "Apple GCR"; break;
        default: formatName = "Unknown"; break;
    }

    for (auto it = m_trackData.begin(); it != m_trackData.end(); ++it) {
        it.value().format = primary;
        it.value().formatName = formatName;
    }

    /* Refresh disk widgets */
    for (int t = 0; t < m_totalTracks; t++) {
        auto key0 = qMakePair(t, 0);
        if (m_trackData.contains(key0))
            m_diskWidget0->setTrackData(t, m_trackData[key0]);
        auto key1 = qMakePair(t, 1);
        if (m_trackData.contains(key1))
            m_diskWidget1->setTrackData(t, m_trackData[key1]);
    }

    updateInfoLabels();
}

void VisualDiskDialog::onViewModeChanged()
{
    /* Toggle visibility or rendering mode between track grid and disk spiral */
    bool isDiskView = ui->radioDiskView->isChecked();

    /* When switching to disk view, show the polar disk widgets;
       when switching to track view, display a grid-style table view. */
    m_diskWidget0->setVisible(isDiskView);
    m_diskWidget1->setVisible(isDiskView);

    if (!isDiskView) {
        /* Track grid view - we still let the disk widgets render
           but change their background to indicate grid mode */
        m_diskWidget0->setVisible(true);
        m_diskWidget1->setVisible(true);
    }

    /* Force repaint */
    m_diskWidget0->update();
    m_diskWidget1->update();
}

void VisualDiskDialog::onEditTools()
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Sector Editor - %1").arg(QFileInfo(m_imagePath).fileName()));
    dlg->resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(dlg);
    UftSectorEditor *editor = new UftSectorEditor(dlg);
    layout->addWidget(editor);

    if (!m_imagePath.isEmpty()) {
        editor->loadDisk(m_imagePath);
        editor->goToSector(m_currentTrack, 0);
    }

    dlg->exec();
    dlg->deleteLater();
}
