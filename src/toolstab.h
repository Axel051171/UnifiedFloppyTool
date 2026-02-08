#pragma once
/**
 * @file toolstab.h
 * @brief Tools Tab - Disk Utilities
 * 
 * UI Dependencies:
 * - comboConvertFrom → comboConvertTo (compatible formats)
 * - comboBatchAction → batch-specific options
 * - checkRetryErrors → spinMaxRetries
 * 
 * @date 2026-01-12
 */

#ifndef TOOLSTAB_H
#define TOOLSTAB_H

#include <QWidget>
#include <QMap>
#include <QStringList>
#include "rawformatdialog.h"

namespace Ui { class TabTools; }

class ToolsTab : public QWidget
{
    Q_OBJECT

public:
    explicit ToolsTab(QWidget *parent = nullptr);
    ~ToolsTab();

signals:
    void statusMessage(const QString& message);

private slots:
    // Analysis tools
    void onDiskInfo();
    void onHexView();
    void onTrackView();
    void onFluxView();
    void onSectorEdit();
    void onAnalyze();
    
    // Conversion tools
    void onConvert();
    void onRepair();
    void onCompare();
    void onCreateBlank();
    
    // Format selection dependencies
    void onConvertFromChanged(int index);
    void onBatchActionChanged(int index);
    
    // Batch operations
    void onBatchStart();
    void onBatchStop();
    
    // Browse buttons
    void onBrowseConvertSource();
    void onBrowseConvertTarget();
    void onBrowseRepair();
    void onBrowseCompareA();
    void onBrowseCompareB();
    void onBrowseAnalyze();
    void onBrowseBatch();
    
    // Output
    void onClearOutput();
    void onSaveOutput();
    void onRawFormatConfig();
    void onVisualDisk();

private:
    void setupConnections();
    void setupFormatConversionMap();
    void populateConvertToFormats(const QString& fromFormat);
    void updateBatchOptions(const QString& action);
    void appendOutput(const QString& text);
    
    Ui::TabTools *ui;
    bool m_batchRunning;
    
    // Format conversion compatibility map
    // Key = source format, Value = list of valid target formats
    QMap<QString, QStringList> m_conversionMap;
};

#endif // TOOLSTAB_H
