#include "diskanalyzerwindow.h"
#include "ui_diskanalyzer_window.h"

#include <QFileDialog>
#include <QMessageBox>

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
    
    // TODO: Load actual image data
    // For now, just update UI with placeholder info
    
    ui->labelSide0Info->setText("42 Tracks, 691 Sectors, 176000 Bytes");
    ui->labelSide0Format->setText("ISO FM\nISO MFM");
    
    ui->labelSide1Info->setText("42 Tracks, 702 Sectors, 181376 Bytes");
    ui->labelSide1Format->setText("ISO FM\nISO MFM");
    
    ui->labelCRC->setText(QString("CRC32: 0x37C3257F"));
    
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
        // TODO: Implement export
        QMessageBox::information(this, tr("Export"), 
            tr("Export to %1 completed.").arg(filename));
    }
}

void DiskAnalyzerWindow::onEditToolsClicked()
{
    // TODO: Open edit tools dialog
    QMessageBox::information(this, tr("Edit Tools"), 
        tr("Sector editor and other tools will be available here."));
}

void DiskAnalyzerWindow::updateDiskView()
{
    // TODO: Render actual disk visualization
    // This would involve custom painting on frameDisk0 and frameDisk1
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
