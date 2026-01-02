#include "statustab.h"
#include "ui_tab_status.h"

#include <QScrollBar>

StatusTab::StatusTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TabStatus)
{
    ui->setupUi(this);
    
    // Connect hex scroll slider
    connect(ui->sliderHexScroll, &QSlider::valueChanged, [this](int value) {
        // Scroll hex dump view
        QScrollBar *vbar = ui->textHexDump->verticalScrollBar();
        if (vbar) {
            vbar->setValue(value * vbar->maximum() / 100);
        }
    });
}

StatusTab::~StatusTab()
{
    delete ui;
}

void StatusTab::setTrackSide(int track, int side)
{
    ui->labelTrackSide->setText(QString("Track : %1 Side : %2").arg(track).arg(side));
}

void StatusTab::setProgress(int trackProgress, int totalProgress)
{
    ui->progressTrack->setValue(trackProgress);
    ui->progressTotal->setValue(totalProgress);
}

void StatusTab::setSectorInfo(const QString &info)
{
    ui->textSectorInfo->setPlainText(info);
}

void StatusTab::setHexDump(const QByteArray &data, int offset)
{
    ui->textHexDump->setPlainText(formatHexDump(data, offset));
}

void StatusTab::appendHexLine(int address, const QByteArray &bytes, const QString &ascii)
{
    QString line = QString("%1  ").arg(address, 5, 10, QChar('0'));
    
    for (int i = 0; i < bytes.size(); ++i) {
        line += QString("%1 ").arg(static_cast<unsigned char>(bytes[i]), 2, 16, QChar('0')).toUpper();
    }
    
    // Pad if less than 8 bytes
    for (int i = bytes.size(); i < 8; ++i) {
        line += "   ";
    }
    
    line += "  " + ascii;
    
    ui->textHexDump->append(line);
}

void StatusTab::clear()
{
    ui->labelTrackSide->setText("Track : 0 Side : 0");
    ui->progressTrack->setValue(0);
    ui->progressTotal->setValue(0);
    ui->textSectorInfo->clear();
    ui->textHexDump->clear();
}

QString StatusTab::formatHexDump(const QByteArray &data, int startAddress)
{
    QString result;
    
    for (int i = 0; i < data.size(); i += 8) {
        QString line = QString("%1  ").arg(startAddress + i, 5, 10, QChar('0'));
        QString ascii;
        
        for (int j = 0; j < 8 && (i + j) < data.size(); ++j) {
            unsigned char c = static_cast<unsigned char>(data[i + j]);
            line += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
            ascii += (c >= 32 && c < 127) ? QChar(c) : '.';
        }
        
        // Pad if less than 8 bytes
        int remaining = 8 - qMin(8, data.size() - i);
        for (int j = 0; j < remaining; ++j) {
            line += "   ";
        }
        
        result += line + "  " + ascii + "\n";
    }
    
    return result;
}
