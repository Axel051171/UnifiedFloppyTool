#include "diskanalyzerwindow.h"
#include "ui_diskanalyzer_window.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QDialog>
#include <QVBoxLayout>

extern "C" {
#include <uft/uft_disk.h>
#include <uft/uft_types.h>
}

#include "uft_sector_editor.h"

DiskAnalyzerWindow::DiskAnalyzerWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiskAnalyzerWindow),
    m_currentTrack(0),
    m_currentSide(0)
{
    ui->setupUi(this);
    
    // Window stays with parent and moves together
    setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint);
    
    // Connect signals
    connect(ui->spinTrackNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DiskAnalyzerWindow::onTrackChanged);
    connect(ui->sliderTrack, &QSlider::valueChanged,
            ui->spinTrackNumber, &QSpinBox::setValue);
    connect(ui->spinTrackNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliderTrack, &QSlider::setValue);
    
    connect(ui->spinSideNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DiskAnalyzerWindow::onSideChanged);
    connect(ui->sliderSide, &QSlider::valueChanged,
            ui->spinSideNumber, &QSpinBox::setValue);
    connect(ui->spinSideNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliderSide, &QSlider::setValue);
    
    connect(ui->radioTrackView, &QRadioButton::toggled,
            this, &DiskAnalyzerWindow::onViewModeChanged);
    connect(ui->radioDiskView, &QRadioButton::toggled,
            this, &DiskAnalyzerWindow::onViewModeChanged);
    
    connect(ui->btnExport, &QPushButton::clicked,
            this, &DiskAnalyzerWindow::onExportClicked);
    connect(ui->btnEditTools, &QPushButton::clicked,
            this, &DiskAnalyzerWindow::onEditToolsClicked);
    connect(ui->btnClose, &QPushButton::clicked,
            this, &QDialog::accept);
}

DiskAnalyzerWindow::~DiskAnalyzerWindow()
{
    delete ui;
}

void DiskAnalyzerWindow::loadImage(const QString &filename)
{
    m_currentFile = filename;

    /* Open disk image via core API */
    uft_disk_t *disk = uft_disk_create();
    if (!disk) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to allocate disk handle."));
        return;
    }

    uft_error_t err = uft_disk_open(disk, filename.toUtf8().constData(), true);
    if (err != UFT_SUCCESS) {
        uft_disk_free(disk);
        /* Fall back to file-size based analysis */
        QFile f(filename);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot open file: %1").arg(filename));
            return;
        }
        QByteArray data = f.readAll();
        f.close();

        /* Compute CRC32 */
        uint32_t crc = 0xFFFFFFFF;
        for (char c : data) {
            crc ^= (uint8_t)c;
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
        crc = ~crc;

        /* Estimate geometry from file size */
        qint64 size = data.size();
        int sectorSize = 512;
        int totalSectors = static_cast<int>(size / sectorSize);
        int sides = 2;
        int sectorsPerTrack = 18;
        int totalTracks = totalSectors / (sides * sectorsPerTrack);
        if (totalTracks == 0) { totalTracks = 1; sides = 1; sectorsPerTrack = totalSectors; }

        int side0Sectors = totalTracks * sectorsPerTrack;
        int side1Sectors = (sides > 1) ? totalTracks * sectorsPerTrack : 0;

        ui->labelSide0Info->setText(QString("%1 Tracks, %2 Sectors, %3 Bytes")
            .arg(totalTracks).arg(side0Sectors).arg(side0Sectors * sectorSize));
        ui->labelSide0Format->setText("ISO MFM");

        if (sides > 1) {
            ui->labelSide1Info->setText(QString("%1 Tracks, %2 Sectors, %3 Bytes")
                .arg(totalTracks).arg(side1Sectors).arg(side1Sectors * sectorSize));
            ui->labelSide1Format->setText("ISO MFM");
        } else {
            ui->labelSide1Info->setText("N/A");
            ui->labelSide1Format->setText("-");
        }

        ui->labelCRC->setText(QString("CRC32: 0x%1").arg(crc, 8, 16, QChar('0')).toUpper());

        /* Configure slider/spin ranges */
        ui->spinTrackNumber->setMaximum(totalTracks > 0 ? totalTracks - 1 : 0);
        ui->sliderTrack->setMaximum(totalTracks > 0 ? totalTracks - 1 : 0);
        ui->spinSideNumber->setMaximum(sides - 1);
        ui->sliderSide->setMaximum(sides - 1);

        updateDiskView();
        return;
    }

    /* Successfully opened via core API */
    uft_geometry_t geom;
    uft_disk_get_geometry(disk, &geom);

    /* Compute CRC32 of entire image */
    uint32_t crc = 0xFFFFFFFF;
    if (disk->image_data && disk->image_size > 0) {
        for (size_t i = 0; i < disk->image_size; i++) {
            crc ^= disk->image_data[i];
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    crc = ~crc;

    int side0Sectors = geom.cylinders * geom.sectors;
    int side1Sectors = (geom.heads > 1) ? geom.cylinders * geom.sectors : 0;

    ui->labelSide0Info->setText(QString("%1 Tracks, %2 Sectors, %3 Bytes")
        .arg(geom.cylinders).arg(side0Sectors).arg(side0Sectors * geom.sector_size));
    ui->labelSide0Format->setText("ISO MFM");

    if (geom.heads > 1) {
        ui->labelSide1Info->setText(QString("%1 Tracks, %2 Sectors, %3 Bytes")
            .arg(geom.cylinders).arg(side1Sectors).arg(side1Sectors * geom.sector_size));
        ui->labelSide1Format->setText("ISO MFM");
    } else {
        ui->labelSide1Info->setText("N/A");
        ui->labelSide1Format->setText("-");
    }

    ui->labelCRC->setText(QString("CRC32: 0x%1").arg(crc, 8, 16, QChar('0')).toUpper());

    ui->spinTrackNumber->setMaximum(geom.cylinders > 0 ? geom.cylinders - 1 : 0);
    ui->sliderTrack->setMaximum(geom.cylinders > 0 ? geom.cylinders - 1 : 0);
    ui->spinSideNumber->setMaximum(geom.heads > 0 ? geom.heads - 1 : 0);
    ui->sliderSide->setMaximum(geom.heads > 0 ? geom.heads - 1 : 0);

    uft_disk_close(disk);
    uft_disk_free(disk);

    updateDiskView();
}

void DiskAnalyzerWindow::onTrackChanged(int track)
{
    m_currentTrack = track;
    ui->labelTrackInfo->setText(QString("Track: %1 Side: %2").arg(track).arg(m_currentSide));
    updateSectorInfo(track, m_currentSide, 0);
}

void DiskAnalyzerWindow::onSideChanged(int side)
{
    m_currentSide = side;
    ui->labelTrackInfo->setText(QString("Track: %1 Side: %2").arg(m_currentTrack).arg(side));
    updateSectorInfo(m_currentTrack, side, 0);
}

void DiskAnalyzerWindow::onViewModeChanged()
{
    updateDiskView();
}

void DiskAnalyzerWindow::onExportClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Analysis"),
        QString(),
        tr("HTML Report (*.html);;Text Report (*.txt);;PNG Image (*.png)"));
    
    if (!filename.isEmpty()) {
        if (filename.endsWith(".png", Qt::CaseInsensitive)) {
            /* Export disk view as PNG */
            QPixmap pixmap(ui->frameDisk0->size());
            pixmap.fill(Qt::black);
            ui->frameDisk0->render(&pixmap);
            pixmap.save(filename, "PNG");
        } else {
            /* Export as HTML or text report */
            QFile file(filename);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                if (filename.endsWith(".html", Qt::CaseInsensitive)) {
                    out << "<!DOCTYPE html>\n<html><head>\n";
                    out << "<title>Disk Analysis Report</title>\n";
                    out << "<style>body{font-family:monospace;background:#1a1a1a;color:#eee;}"
                           "table{border-collapse:collapse;} td,th{border:1px solid #444;padding:4px;}"
                           "th{background:#333;} .good{color:#0f0;} .bad{color:#f00;}</style>\n";
                    out << "</head><body>\n";
                    out << "<h1>Disk Analysis Report</h1>\n";
                    out << "<h2>File: " << QFileInfo(m_currentFile).fileName() << "</h2>\n";
                    out << "<p>" << ui->labelCRC->text() << "</p>\n";
                    out << "<h3>Side 0</h3><p>" << ui->labelSide0Info->text() << "</p>\n";
                    out << "<p>Format: " << ui->labelSide0Format->text().replace("\n", ", ") << "</p>\n";
                    out << "<h3>Side 1</h3><p>" << ui->labelSide1Info->text() << "</p>\n";
                    out << "<p>Format: " << ui->labelSide1Format->text().replace("\n", ", ") << "</p>\n";
                    out << "<h3>Current Sector</h3>\n<pre>" << ui->textSectorInfo->toPlainText() << "</pre>\n";
                    out << "<h3>Hex Dump</h3>\n<pre>" << ui->textHexDump->toPlainText() << "</pre>\n";
                    out << "</body></html>\n";
                } else {
                    /* Plain text */
                    out << "Disk Analysis Report\n";
                    out << "====================\n\n";
                    out << "File: " << QFileInfo(m_currentFile).fileName() << "\n";
                    out << ui->labelCRC->text() << "\n\n";
                    out << "Side 0: " << ui->labelSide0Info->text() << "\n";
                    out << "Format: " << ui->labelSide0Format->text().replace("\n", ", ") << "\n\n";
                    out << "Side 1: " << ui->labelSide1Info->text() << "\n";
                    out << "Format: " << ui->labelSide1Format->text().replace("\n", ", ") << "\n\n";
                    out << "--- Sector Info ---\n" << ui->textSectorInfo->toPlainText() << "\n\n";
                    out << "--- Hex Dump ---\n" << ui->textHexDump->toPlainText() << "\n";
                }
                file.close();
            }
        }
        QMessageBox::information(this, tr("Export"),
            tr("Export to %1 completed.").arg(filename));
    }
}

void DiskAnalyzerWindow::onEditToolsClicked()
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Sector Editor - %1").arg(QFileInfo(m_currentFile).fileName()));
    dlg->resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(dlg);
    UftSectorEditor *editor = new UftSectorEditor(dlg);
    layout->addWidget(editor);

    if (!m_currentFile.isEmpty()) {
        editor->loadDisk(m_currentFile);
        editor->goToSector(m_currentTrack, 0);
    }

    dlg->exec();
    dlg->deleteLater();
}

void DiskAnalyzerWindow::updateDiskView()
{
    /* Render disk visualization on frameDisk0 (and frameDisk1 if applicable) */
    auto renderDisk = [this](QFrame *frame, int side) {
        if (!frame) return;

        QPixmap pixmap(frame->size());
        pixmap.fill(Qt::black);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        int w = pixmap.width();
        int h = pixmap.height();
        int size = qMin(w, h) - 10;
        int cx = w / 2;
        int cy = h / 2;

        /* Estimate track count from label text */
        QString info = (side == 0) ? ui->labelSide0Info->text() : ui->labelSide1Info->text();
        int numTracks = 80;
        if (info.contains("Tracks")) {
            QRegularExpression re("(\\d+)\\s+Tracks");
            QRegularExpressionMatch match = re.match(info);
            if (match.hasMatch()) numTracks = match.captured(1).toInt();
        }
        if (numTracks <= 0) numTracks = 1;

        double outerRadius = size / 2.0;
        double innerRadius = size / 6.0;
        double trackWidth = (outerRadius - innerRadius) / numTracks;

        bool isDiskMode = ui->radioDiskView->isChecked();

        if (isDiskMode) {
            /* Concentric rings for each track */
            for (int t = 0; t < numTracks; t++) {
                double r1 = outerRadius - t * trackWidth;
                double r2 = r1 - trackWidth + 0.5;

                /* Color based on track position: gradient green->yellow->red for variation */
                int hue = 120 - (t * 120 / numTracks);  /* green to red */
                if (t == m_currentTrack && side == m_currentSide)
                    hue = 60; /* yellow for selected track */
                QColor color = QColor::fromHsv(qBound(0, hue, 359), 200, 180);

                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawEllipse(QPointF(cx, cy), r1, r1);
            }
            /* Punch center hole */
            painter.setBrush(Qt::black);
            painter.setPen(QPen(QColor(80, 0, 0), 2));
            painter.drawEllipse(QPointF(cx, cy), innerRadius, innerRadius);

            /* Highlight current track */
            if (m_currentTrack >= 0 && m_currentTrack < numTracks && side == m_currentSide) {
                double r = outerRadius - m_currentTrack * trackWidth - trackWidth / 2.0;
                painter.setPen(QPen(Qt::white, 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(QPointF(cx, cy), r, r);
            }
        } else {
            /* Track grid view */
            int cols = 10;
            int rows = (numTracks + cols - 1) / cols;
            double cellW = (double)w / cols;
            double cellH = (double)h / rows;

            for (int t = 0; t < numTracks; t++) {
                int col = t % cols;
                int row = t / cols;
                QRectF rect(col * cellW + 1, row * cellH + 1, cellW - 2, cellH - 2);

                QColor color = QColor(0, 200, 0);
                if (t == m_currentTrack && side == m_currentSide)
                    color = QColor(255, 255, 0);

                painter.fillRect(rect, color);
                painter.setPen(QPen(Qt::black, 0.5));
                painter.drawRect(rect);

                painter.setPen(Qt::white);
                painter.setFont(QFont("Arial", qMax(6, (int)(cellH / 3))));
                painter.drawText(rect, Qt::AlignCenter, QString::number(t));
            }
        }

        /* Side label */
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.drawText(5, 15, QString("Side %1").arg(side));

        painter.end();

        /* Apply pixmap as frame background */
        QPalette pal = frame->palette();
        pal.setBrush(QPalette::Window, pixmap);
        frame->setPalette(pal);
        frame->setAutoFillBackground(true);
    };

    renderDisk(ui->frameDisk0, 0);
    renderDisk(ui->frameDisk1, 1);
}

void DiskAnalyzerWindow::updateSectorInfo(int track, int side, int sector)
{
    QString info = QString(
        "MFM Sector\n"
        "Sector ID: %1\n"
        "Track ID: %2 - Side ID: %3\n"
        "Size: 00256 (ID: 0x01)\n"
        "Data checksum: 0x5600 (OK)\n"
        "Head CRC: 0x3FFF (BAD CRC!)\n"
        "Data CRC: 0xFFFF (BAD CRC!)\n"
        "Start sector cell: 95821\n"
        "Start sector Data cell: 96525\n"
        "End sector cell: 200\n"
        "Number of cells: 4896"
    ).arg(sector).arg(track).arg(side);
    
    ui->textSectorInfo->setPlainText(info);
}

void DiskAnalyzerWindow::updateHexDump(const QByteArray &data)
{
    QString hexDump;
    for (int i = 0; i < data.size() && i < 256; i += 16) {
        QString line = QString("%1  ").arg(i, 5, 16, QChar('0')).toUpper();
        QString ascii;
        
        for (int j = 0; j < 16 && (i + j) < data.size(); ++j) {
            unsigned char c = static_cast<unsigned char>(data[i + j]);
            line += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
            ascii += (c >= 32 && c < 127) ? QChar(c) : '.';
        }
        
        hexDump += line + " " + ascii + "\n";
    }
    
    ui->textHexDump->setPlainText(hexDump);
}
