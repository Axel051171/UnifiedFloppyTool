#pragma once
/**
 * @file forensictab.h
 * @brief Forensic Tab - Disk Image Analysis
 * 
 * UI Dependencies:
 * - checkValidateStructure → sub-validation options
 * - comboReportFormat → report options enable/disable
 * - Hash checkboxes → hash result fields
 * 
 * @date 2026-01-12
 */

#ifndef FORENSICTAB_H
#define FORENSICTAB_H

#include <QWidget>
#include <QFutureWatcher>
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
    // File selection
    void onBrowseImage();
    void onRunAnalysis();
    void onCompare();
    void onExportReport();
    
    // Checkbox dependencies
    void onValidateStructureToggled(bool checked);
    void onReportFormatChanged(int index);
    void onHashCheckChanged();
    void onAnalyzeProtectionToggled(bool checked);

private:
    void setupConnections();
    void setupDependencies();
    void updateValidationSubOptions(bool enabled);
    void updateReportOptions(const QString& format);
    void updateHashFields();
    
    // Analysis functions
    void calculateHashes(const QString& path);
    void analyzeStructure(const QString& path, const DiskImageInfo& info);
    void detectProtection(const QByteArray& data);
    void findHiddenData(const QByteArray& data);
    QString generateReport();
    
    void addResultRow(const QString& check, const QString& status, 
                      const QString& details, bool isError = false);
    void clearResults();
    
    Ui::TabForensic *ui;
    
    QString m_currentImage;
    DiskImageInfo m_currentInfo;
    QByteArray m_imageData;
    
    // Hash results
    QString m_md5;
    QString m_sha1;
    QString m_sha256;
    QString m_crc32;
};

#endif // FORENSICTAB_H
