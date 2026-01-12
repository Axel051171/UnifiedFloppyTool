/**
 * @file forensictab.h
 * @brief Forensic Tab - Disk Image Analysis
 * 
 * P0-GUI-005 FIX: Real analysis instead of hardcoded results
 */

#ifndef FORENSICTAB_H
#define FORENSICTAB_H

#include <QWidget>
#include "disk_image_validator.h"

namespace Ui { class TabForensic; }

class ForensicTab : public QWidget
{
    Q_OBJECT

public:
    explicit ForensicTab(QWidget *parent = nullptr);
    ~ForensicTab();

public slots:
    void analyzeImage(const QString& imagePath);
    void analyzeImage(const QString& imagePath, const DiskImageInfo& info);

signals:
    void analysisComplete(const QString& summary);
    void statusMessage(const QString& message);

private slots:
    void onRunAnalysis();
    void onCompare();
    void onExportReport();
    void onBrowseImage();

private:
    void setupConnections();
    void calculateHashes(const QString& path);
    void analyzeStructure(const QString& path, const DiskImageInfo& info);
    void detectProtection(const QByteArray& data);
    void findHiddenData(const QByteArray& data);
    QString generateReport();
    
    Ui::TabForensic *ui;
    
    QString m_currentImage;
    DiskImageInfo m_currentInfo;
    QByteArray m_imageData;
};

#endif // FORENSICTAB_H
